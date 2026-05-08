#include "infra/credentials/EncryptedFileCredentialStore.h"

#include <openssl/evp.h>
#include <openssl/rand.h>

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace {

constexpr int kCredentialVersion = 1;
constexpr int kSaltSize = 16;
constexpr int kIvSize = 12;
constexpr int kTagSize = 16;
constexpr int kKeySize = 32;
constexpr int kPbkdf2Iterations = 210000;

using EvpCipherContextPtr = std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)>;

infra::credentials::CredentialStoreResult succeeded()
{
    return {.succeeded = true};
}

infra::credentials::CredentialStoreResult failed(std::string code, std::string message)
{
    return {
        .succeeded = false,
        .error = {
            .code = std::move(code),
            .message = std::move(message),
        },
    };
}

QByteArray randomBytes(int size)
{
    QByteArray bytes(size, Qt::Uninitialized);
    if (RAND_bytes(reinterpret_cast<unsigned char *>(bytes.data()), size) != 1) {
        return {};
    }

    return bytes;
}

QByteArray deriveKey(const std::string &masterPassword, const QByteArray &salt)
{
    QByteArray key(kKeySize, Qt::Uninitialized);
    const auto ok = PKCS5_PBKDF2_HMAC(
        masterPassword.data(),
        static_cast<int>(masterPassword.size()),
        reinterpret_cast<const unsigned char *>(salt.constData()),
        salt.size(),
        kPbkdf2Iterations,
        EVP_sha256(),
        kKeySize,
        reinterpret_cast<unsigned char *>(key.data()));
    if (ok != 1) {
        return {};
    }

    return key;
}

std::optional<QJsonObject> encryptPassword(const std::string &password, const std::string &masterPassword)
{
    const auto salt = randomBytes(kSaltSize);
    const auto iv = randomBytes(kIvSize);
    if (salt.isEmpty() || iv.isEmpty()) {
        return std::nullopt;
    }

    const auto key = deriveKey(masterPassword, salt);
    if (key.isEmpty()) {
        return std::nullopt;
    }

    EvpCipherContextPtr context(EVP_CIPHER_CTX_new(), &EVP_CIPHER_CTX_free);
    if (context == nullptr) {
        return std::nullopt;
    }

    if (EVP_EncryptInit_ex(context.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1
        || EVP_CIPHER_CTX_ctrl(context.get(), EVP_CTRL_GCM_SET_IVLEN, iv.size(), nullptr) != 1
        || EVP_EncryptInit_ex(
            context.get(),
            nullptr,
            nullptr,
            reinterpret_cast<const unsigned char *>(key.constData()),
            reinterpret_cast<const unsigned char *>(iv.constData())) != 1) {
        return std::nullopt;
    }

    QByteArray cipherText(static_cast<qsizetype>(password.size()) + EVP_CIPHER_get_block_size(EVP_aes_256_gcm()), Qt::Uninitialized);
    int written = 0;
    if (EVP_EncryptUpdate(
            context.get(),
            reinterpret_cast<unsigned char *>(cipherText.data()),
            &written,
            reinterpret_cast<const unsigned char *>(password.data()),
            static_cast<int>(password.size())) != 1) {
        return std::nullopt;
    }

    int finalWritten = 0;
    if (EVP_EncryptFinal_ex(
            context.get(),
            reinterpret_cast<unsigned char *>(cipherText.data() + written),
            &finalWritten) != 1) {
        return std::nullopt;
    }
    cipherText.resize(written + finalWritten);

    QByteArray tag(kTagSize, Qt::Uninitialized);
    if (EVP_CIPHER_CTX_ctrl(context.get(), EVP_CTRL_GCM_GET_TAG, tag.size(), tag.data()) != 1) {
        return std::nullopt;
    }

    QJsonObject object;
    object.insert(QStringLiteral("version"), kCredentialVersion);
    object.insert(QStringLiteral("kdf"), QStringLiteral("pbkdf2-sha256"));
    object.insert(QStringLiteral("iterations"), kPbkdf2Iterations);
    object.insert(QStringLiteral("cipher"), QStringLiteral("aes-256-gcm"));
    object.insert(QStringLiteral("salt"), QString::fromLatin1(salt.toBase64()));
    object.insert(QStringLiteral("iv"), QString::fromLatin1(iv.toBase64()));
    object.insert(QStringLiteral("tag"), QString::fromLatin1(tag.toBase64()));
    object.insert(QStringLiteral("ciphertext"), QString::fromLatin1(cipherText.toBase64()));
    return object;
}

std::optional<std::string> decryptPassword(const QJsonObject &object, const std::string &masterPassword)
{
    const auto salt = QByteArray::fromBase64(object.value(QStringLiteral("salt")).toString().toLatin1());
    const auto iv = QByteArray::fromBase64(object.value(QStringLiteral("iv")).toString().toLatin1());
    const auto tag = QByteArray::fromBase64(object.value(QStringLiteral("tag")).toString().toLatin1());
    const auto cipherText = QByteArray::fromBase64(object.value(QStringLiteral("ciphertext")).toString().toLatin1());
    if (salt.size() != kSaltSize || iv.size() != kIvSize || tag.size() != kTagSize || cipherText.isEmpty()) {
        return std::nullopt;
    }

    const auto key = deriveKey(masterPassword, salt);
    if (key.isEmpty()) {
        return std::nullopt;
    }

    EvpCipherContextPtr context(EVP_CIPHER_CTX_new(), &EVP_CIPHER_CTX_free);
    if (context == nullptr) {
        return std::nullopt;
    }

    if (EVP_DecryptInit_ex(context.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1
        || EVP_CIPHER_CTX_ctrl(context.get(), EVP_CTRL_GCM_SET_IVLEN, iv.size(), nullptr) != 1
        || EVP_DecryptInit_ex(
            context.get(),
            nullptr,
            nullptr,
            reinterpret_cast<const unsigned char *>(key.constData()),
            reinterpret_cast<const unsigned char *>(iv.constData())) != 1) {
        return std::nullopt;
    }

    QByteArray plainText(cipherText.size(), Qt::Uninitialized);
    int written = 0;
    if (EVP_DecryptUpdate(
            context.get(),
            reinterpret_cast<unsigned char *>(plainText.data()),
            &written,
            reinterpret_cast<const unsigned char *>(cipherText.constData()),
            cipherText.size()) != 1) {
        return std::nullopt;
    }

    if (EVP_CIPHER_CTX_ctrl(context.get(), EVP_CTRL_GCM_SET_TAG, tag.size(), const_cast<char *>(tag.constData())) != 1) {
        return std::nullopt;
    }

    int finalWritten = 0;
    if (EVP_DecryptFinal_ex(
            context.get(),
            reinterpret_cast<unsigned char *>(plainText.data() + written),
            &finalWritten) != 1) {
        return std::nullopt;
    }
    plainText.resize(written + finalWritten);

    return std::string(plainText.constData(), static_cast<std::size_t>(plainText.size()));
}

} // namespace

namespace infra::credentials {

EncryptedFileCredentialStore::EncryptedFileCredentialStore()
    : EncryptedFileCredentialStore(defaultFilePath())
{
}

EncryptedFileCredentialStore::EncryptedFileCredentialStore(QString filePath)
    : m_filePath(std::move(filePath))
{
}

QString EncryptedFileCredentialStore::defaultFilePath()
{
    auto configDirectory = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (configDirectory.isEmpty()) {
        configDirectory = QDir::homePath() + QStringLiteral("/.config/kdeftpclient");
    }

    return QDir(configDirectory).filePath(QStringLiteral("credentials.json"));
}

bool EncryptedFileCredentialStore::hasPassword(const std::string &credentialId)
{
    const auto credentials = loadRootObject().value(QStringLiteral("credentials")).toObject();
    return credentials.contains(QString::fromStdString(credentialId));
}

CredentialStoreResult EncryptedFileCredentialStore::savePassword(
    const std::string &credentialId,
    const std::string &password,
    const std::string &masterPassword)
{
    if (credentialId.empty()) {
        return failed("empty_credential_id", "Credential id is required.");
    }
    if (masterPassword.empty()) {
        return failed("empty_master_password", "Master password is required.");
    }

    auto encrypted = encryptPassword(password, masterPassword);
    if (!encrypted.has_value()) {
        return failed("encrypt_failed", "Could not encrypt password.");
    }

    auto rootObject = loadRootObject();
    auto credentials = rootObject.value(QStringLiteral("credentials")).toObject();
    credentials.insert(QString::fromStdString(credentialId), *encrypted);
    rootObject.insert(QStringLiteral("version"), kCredentialVersion);
    rootObject.insert(QStringLiteral("credentials"), credentials);
    return writeRootObject(rootObject);
}

ReadCredentialResult EncryptedFileCredentialStore::readPassword(
    const std::string &credentialId,
    const std::string &masterPassword)
{
    if (masterPassword.empty()) {
        return {
            .operation = failed("empty_master_password", "Master password is required."),
            .found = false,
        };
    }

    const auto rootObject = loadRootObject();
    const auto credentials = rootObject.value(QStringLiteral("credentials")).toObject();
    const auto encrypted = credentials.value(QString::fromStdString(credentialId));
    if (!encrypted.isObject()) {
        return {
            .operation = succeeded(),
            .found = false,
        };
    }

    const auto password = decryptPassword(encrypted.toObject(), masterPassword);
    if (!password.has_value()) {
        return {
            .operation = failed("decrypt_failed", "Could not decrypt password. The master password may be incorrect."),
            .found = true,
        };
    }

    return {
        .operation = succeeded(),
        .password = *password,
        .found = true,
    };
}

CredentialStoreResult EncryptedFileCredentialStore::removePassword(const std::string &credentialId)
{
    auto rootObject = loadRootObject();
    auto credentials = rootObject.value(QStringLiteral("credentials")).toObject();
    credentials.remove(QString::fromStdString(credentialId));
    rootObject.insert(QStringLiteral("version"), kCredentialVersion);
    rootObject.insert(QStringLiteral("credentials"), credentials);
    return writeRootObject(rootObject);
}

QJsonObject EncryptedFileCredentialStore::loadRootObject() const
{
    QFile file(m_filePath);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        return {
            {QStringLiteral("version"), kCredentialVersion},
            {QStringLiteral("credentials"), QJsonObject {}},
        };
    }

    const auto document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) {
        return {
            {QStringLiteral("version"), kCredentialVersion},
            {QStringLiteral("credentials"), QJsonObject {}},
        };
    }

    return document.object();
}

CredentialStoreResult EncryptedFileCredentialStore::writeRootObject(const QJsonObject &rootObject) const
{
    const QFileInfo fileInfo(m_filePath);
    QDir directory(fileInfo.absolutePath());
    if (!directory.exists() && !directory.mkpath(QStringLiteral("."))) {
        return failed("create_directory_failed", "Could not create credentials directory.");
    }

    QSaveFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return failed("open_failed", file.errorString().toStdString());
    }

    file.write(QJsonDocument(rootObject).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        return failed("write_failed", file.errorString().toStdString());
    }

    return succeeded();
}

} // namespace infra::credentials
