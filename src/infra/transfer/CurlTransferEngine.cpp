#include "infra/transfer/CurlTransferEngine.h"

#include <curl/curl.h>

#include <QDir>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QUrl>

#include <algorithm>
#include <array>
#include <cstdint>
#include <sstream>
#include <utility>

namespace {

bool ensureCurlInitialized()
{
    static const bool initialized = curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK;
    return initialized;
}

infra::transfer::OperationResult succeeded()
{
    return {.succeeded = true};
}

infra::transfer::OperationResult failed(std::string code, std::string message)
{
    return {
        .succeeded = false,
        .error = {
            .code = std::move(code),
            .message = std::move(message),
        },
    };
}

QString expandUserPath(const QString &path)
{
    if (path == QStringLiteral("~")) {
        return QDir::homePath();
    }
    if (path.startsWith(QStringLiteral("~/"))) {
        return QDir::homePath() + path.mid(1);
    }

    return path;
}

std::string protocolScheme(domain::Protocol protocol)
{
    switch (protocol) {
    case domain::Protocol::Ftp:
        return "ftp";
    case domain::Protocol::Ftps:
        return "ftps";
    case domain::Protocol::Sftp:
        return "sftp";
    }

    return "ftp";
}

std::string normalizedRemotePath(const std::string &remotePath)
{
    QString path = QString::fromStdString(remotePath).trimmed();
    if (path.isEmpty()) {
        return "/";
    }
    if (!path.startsWith(QLatin1Char('/'))) {
        path.prepend(QLatin1Char('/'));
    }

    return QDir::cleanPath(path).toStdString();
}

std::string joinRemotePath(const std::string &directory, const std::string &name)
{
    if (directory.empty() || directory == "/") {
        return "/" + name;
    }
    if (directory.back() == '/') {
        return directory + name;
    }

    return directory + "/" + name;
}

size_t writeToString(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    auto *output = static_cast<std::string *>(userdata);
    const auto bytes = size * nmemb;
    output->append(ptr, bytes);
    return bytes;
}

std::string curlErrorMessage(CURLcode code, const std::array<char, CURL_ERROR_SIZE> &errorBuffer)
{
    if (errorBuffer.front() != '\0') {
        return errorBuffer.data();
    }

    return curl_easy_strerror(code);
}

bool configureAuthentication(CURL *curl, const domain::SiteProfile &siteProfile, const std::string &password)
{
    if (siteProfile.allowAnonymousLogin) {
        curl_easy_setopt(curl, CURLOPT_USERNAME, "anonymous");
        curl_easy_setopt(curl, CURLOPT_PASSWORD, "anonymous@");
        return true;
    }

    curl_easy_setopt(curl, CURLOPT_USERNAME, siteProfile.userName.c_str());
    curl_easy_setopt(curl, CURLOPT_PASSWORD, password.c_str());

    if (siteProfile.protocol == domain::Protocol::Sftp
        && siteProfile.authenticationMethod == domain::AuthenticationMethod::PrivateKey) {
        const auto privateKeyPath = expandUserPath(QString::fromStdString(siteProfile.privateKeyPath).trimmed());
        curl_easy_setopt(curl, CURLOPT_SSH_PRIVATE_KEYFILE, privateKeyPath.toUtf8().constData());
    }

    return true;
}

void configureCommonOptions(
    CURL *curl,
    const domain::SiteProfile &siteProfile,
    const std::string &password,
    const std::string &url,
    std::array<char, CURL_ERROR_SIZE> &errorBuffer)
{
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer.data());
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);

    if (siteProfile.protocol == domain::Protocol::Ftps) {
        curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);
    }

    if (!siteProfile.passiveMode && siteProfile.protocol != domain::Protocol::Sftp) {
        curl_easy_setopt(curl, CURLOPT_FTPPORT, "-");
    }

    configureAuthentication(curl, siteProfile, password);
}

std::optional<domain::RemoteEntry> parseMlsdLine(const QString &line, const std::string &directory)
{
    const auto separatorIndex = line.indexOf(QLatin1Char(' '));
    if (separatorIndex < 0) {
        return std::nullopt;
    }

    const auto facts = line.left(separatorIndex);
    const auto name = line.mid(separatorIndex + 1).trimmed();
    if (name.isEmpty() || name == QStringLiteral(".") || name == QStringLiteral("..")) {
        return std::nullopt;
    }

    const bool isDirectory = facts.contains(QStringLiteral("type=dir"), Qt::CaseInsensitive)
        || facts.contains(QStringLiteral("type=cdir"), Qt::CaseInsensitive)
        || facts.contains(QStringLiteral("type=pdir"), Qt::CaseInsensitive);
    if (facts.contains(QStringLiteral("type=cdir"), Qt::CaseInsensitive)
        || facts.contains(QStringLiteral("type=pdir"), Qt::CaseInsensitive)) {
        return std::nullopt;
    }

    std::uint64_t size = 0;
    const QRegularExpression sizeExpression(QStringLiteral("size=(\\d+)"), QRegularExpression::CaseInsensitiveOption);
    const auto match = sizeExpression.match(facts);
    if (match.hasMatch()) {
        size = match.captured(1).toULongLong();
    }

    return domain::RemoteEntry {
        .name = name.toStdString(),
        .path = joinRemotePath(directory, name.toStdString()),
        .isDirectory = isDirectory,
        .size = size,
    };
}

std::optional<domain::RemoteEntry> parseUnixListLine(const QString &line, const std::string &directory)
{
    if (line.isEmpty() || line.startsWith(QStringLiteral("total "))) {
        return std::nullopt;
    }

    const auto columns = line.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    if (columns.size() < 9) {
        return std::nullopt;
    }

    const auto permissions = columns.at(0);
    if (permissions.isEmpty()) {
        return std::nullopt;
    }

    const auto type = permissions.at(0);
    if (type != QLatin1Char('d') && type != QLatin1Char('-') && type != QLatin1Char('l')) {
        return std::nullopt;
    }

    const auto name = QStringList(columns.mid(8)).join(QLatin1Char(' '));
    if (name.isEmpty() || name == QStringLiteral(".") || name == QStringLiteral("..")) {
        return std::nullopt;
    }

    return domain::RemoteEntry {
        .name = name.toStdString(),
        .path = joinRemotePath(directory, name.toStdString()),
        .isDirectory = type == QLatin1Char('d'),
        .size = columns.at(4).toULongLong(),
    };
}

std::vector<domain::RemoteEntry> parseDirectoryListing(const std::string &listing, const std::string &directory)
{
    std::vector<domain::RemoteEntry> entries;
    const auto lines = QString::fromUtf8(listing.data(), static_cast<qsizetype>(listing.size()))
                           .split(QRegularExpression(QStringLiteral("\\r?\\n")), Qt::SkipEmptyParts);

    for (const auto &line : lines) {
        if (auto entry = parseMlsdLine(line.trimmed(), directory); entry.has_value()) {
            entries.push_back(std::move(*entry));
            continue;
        }
        if (auto entry = parseUnixListLine(line.trimmed(), directory); entry.has_value()) {
            entries.push_back(std::move(*entry));
        }
    }

    std::sort(entries.begin(), entries.end(), [](const auto &left, const auto &right) {
        if (left.isDirectory != right.isDirectory) {
            return left.isDirectory && !right.isDirectory;
        }
        return left.name < right.name;
    });

    return entries;
}

} // namespace

namespace infra::transfer {

CurlTransferEngine::CurlTransferEngine()
{
    ensureCurlInitialized();
}

ConnectionResult CurlTransferEngine::connect(const ConnectionRequest &request)
{
    const auto &siteProfile = request.siteProfile;

    if (!ensureCurlInitialized()) {
        return {
            .operation = failed("curl_init_failed", "Could not initialize libcurl."),
            .siteProfile = siteProfile,
        };
    }
    if (siteProfile.host.empty()) {
        return {
            .operation = failed("empty_host", "Host is required."),
            .siteProfile = siteProfile,
        };
    }
    if (siteProfile.protocol == domain::Protocol::Sftp
        && siteProfile.authenticationMethod == domain::AuthenticationMethod::PrivateKey
        && siteProfile.privateKeyPath.empty()) {
        return {
            .operation = failed("empty_private_key_path", "Private key path is required."),
            .siteProfile = siteProfile,
        };
    }

    m_connectedSite = siteProfile;
    m_sessionPassword = request.password;
    const auto listResult = listDirectory("/");
    if (!listResult.operation.succeeded) {
        m_connectedSite.reset();
        m_sessionPassword.clear();
        return {
            .operation = listResult.operation,
            .siteProfile = siteProfile,
        };
    }

    return {
        .operation = succeeded(),
        .siteProfile = siteProfile,
    };
}

OperationResult CurlTransferEngine::disconnect()
{
    m_connectedSite.reset();
    m_sessionPassword.clear();
    return succeeded();
}

ListDirectoryResult CurlTransferEngine::listDirectory(const std::string &remotePath)
{
    auto connected = ensureConnected();
    if (!connected.succeeded) {
        return {.operation = connected};
    }

    auto curl = curl_easy_init();
    if (curl == nullptr) {
        return {.operation = failed("curl_easy_init_failed", "Could not create libcurl handle.")};
    }

    std::array<char, CURL_ERROR_SIZE> errorBuffer {};
    std::string listing;
    const auto normalizedPath = normalizedRemotePath(remotePath);
    configureCommonOptions(curl, *m_connectedSite, m_sessionPassword, buildUrl(normalizedPath), errorBuffer);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeToString);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &listing);

    const auto code = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    if (code != CURLE_OK) {
        return {
            .operation = failed("list_directory_failed", curlErrorMessage(code, errorBuffer)),
        };
    }

    return {
        .operation = succeeded(),
        .entries = parseDirectoryListing(listing, normalizedPath),
    };
}

StartTransferResult CurlTransferEngine::upload(const TransferRequest &request)
{
    (void)request;

    auto connected = ensureConnected();
    if (!connected.succeeded) {
        return {.operation = connected};
    }

    return {
        .operation = failed("transfer_worker_not_implemented", "CURL transfer worker is not implemented yet."),
    };
}

StartTransferResult CurlTransferEngine::download(const TransferRequest &request)
{
    (void)request;

    auto connected = ensureConnected();
    if (!connected.succeeded) {
        return {.operation = connected};
    }

    return {
        .operation = failed("transfer_worker_not_implemented", "CURL transfer worker is not implemented yet."),
    };
}

OperationResult CurlTransferEngine::cancel(TransferJobId jobId)
{
    (void)jobId;

    auto connected = ensureConnected();
    if (!connected.succeeded) {
        return connected;
    }

    return failed("transfer_worker_not_implemented", "CURL transfer worker is not implemented yet.");
}

OperationResult CurlTransferEngine::ensureConnected() const
{
    if (!m_connectedSite.has_value()) {
        return failed("not_connected", "No remote session is connected.");
    }

    return succeeded();
}

std::string CurlTransferEngine::buildUrl(const std::string &remotePath) const
{
    if (!m_connectedSite.has_value()) {
        return {};
    }

    const auto path = QString::fromStdString(normalizedRemotePath(remotePath));
    const auto encodedPath = QString::fromLatin1(QUrl::toPercentEncoding(path, "/"));
    std::ostringstream url;
    url << protocolScheme(m_connectedSite->protocol)
        << "://"
        << m_connectedSite->host
        << ':'
        << m_connectedSite->port
        << '/'
        << encodedPath.toStdString();
    return url.str();
}

} // namespace infra::transfer
