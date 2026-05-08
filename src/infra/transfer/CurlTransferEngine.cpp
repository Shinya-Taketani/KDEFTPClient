#include "infra/transfer/CurlTransferEngine.h"

#include <curl/curl.h>

#include <QByteArray>
#include <QDir>
#include <QRegularExpression>
#include <QString>
#include <QStringConverter>
#include <QStringList>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
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
    if (path == QStringLiteral("~") || path.startsWith(QStringLiteral("~/"))) {
        return QDir::cleanPath(path).toStdString();
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
    if (directory == "~") {
        return "~/" + name;
    }
    if (directory.back() == '/') {
        return directory + name;
    }

    return directory + "/" + name;
}

bool containsReplacementCharacter(const QString &text)
{
    return text.contains(QChar(0xfffd));
}

std::optional<QString> decodeWithNamedEncoding(const std::string &listing, const char *encodingName)
{
    QStringDecoder decoder(encodingName);
    if (!decoder.isValid()) {
        return std::nullopt;
    }

    const QByteArray bytes(listing.data(), static_cast<qsizetype>(listing.size()));
    const QString decoded = decoder.decode(bytes);
    if (decoder.hasError()) {
        return std::nullopt;
    }

    return decoded;
}

std::optional<QByteArray> encodeWithNamedEncoding(const QString &text, const char *encodingName)
{
    QStringEncoder encoder(encodingName);
    if (!encoder.isValid()) {
        return std::nullopt;
    }

    const QByteArray encoded = encoder.encode(text);
    if (encoder.hasError()) {
        return std::nullopt;
    }

    return encoded;
}

QString decodeAsUtf8(const std::string &listing)
{
    const auto *data = listing.data();
    const auto size = static_cast<qsizetype>(listing.size());
    return QString::fromUtf8(data, size);
}

QString decodeAsLocal8Bit(const std::string &listing)
{
    const auto *data = listing.data();
    const auto size = static_cast<qsizetype>(listing.size());
    return QString::fromLocal8Bit(data, size);
}

QByteArray encodeRemotePath(
    const std::string &remotePath,
    domain::FilenameEncoding filenameEncoding,
    bool directoryPath)
{
    std::string normalizedPath = normalizedRemotePath(remotePath);
    if (directoryPath && normalizedPath != "/" && normalizedPath.back() != '/') {
        normalizedPath.push_back('/');
    }

    const auto path = QString::fromStdString(normalizedPath);
    if (filenameEncoding == domain::FilenameEncoding::Local8Bit) {
        return path.toLocal8Bit();
    }
    if (filenameEncoding == domain::FilenameEncoding::ShiftJis) {
        if (const auto encoded = encodeWithNamedEncoding(path, "Shift-JIS"); encoded.has_value()) {
            return *encoded;
        }
        if (const auto encoded = encodeWithNamedEncoding(path, "CP932"); encoded.has_value()) {
            return *encoded;
        }
    }

    return path.toUtf8();
}

bool isUnreservedUrlByte(unsigned char byte)
{
    return (byte >= 'A' && byte <= 'Z')
        || (byte >= 'a' && byte <= 'z')
        || (byte >= '0' && byte <= '9')
        || byte == '-'
        || byte == '.'
        || byte == '_'
        || byte == '~';
}

std::string percentEncodePathBytes(const QByteArray &pathBytes)
{
    constexpr char kHexDigits[] = "0123456789ABCDEF";

    std::string encoded;
    encoded.reserve(static_cast<std::size_t>(pathBytes.size()));
    for (const auto byteValue : pathBytes) {
        const auto byte = static_cast<unsigned char>(byteValue);
        if (byte == '/') {
            encoded.push_back('/');
            continue;
        }
        if (isUnreservedUrlByte(byte)) {
            encoded.push_back(static_cast<char>(byte));
            continue;
        }

        encoded.push_back('%');
        encoded.push_back(kHexDigits[(byte >> 4U) & 0x0FU]);
        encoded.push_back(kHexDigits[byte & 0x0FU]);
    }

    return encoded;
}

QString decodeDirectoryListing(const std::string &listing, domain::FilenameEncoding filenameEncoding)
{
    if (filenameEncoding == domain::FilenameEncoding::Utf8) {
        return decodeAsUtf8(listing);
    }
    if (filenameEncoding == domain::FilenameEncoding::Local8Bit) {
        return decodeAsLocal8Bit(listing);
    }
    if (filenameEncoding == domain::FilenameEncoding::ShiftJis) {
        if (const auto decoded = decodeWithNamedEncoding(listing, "Shift-JIS"); decoded.has_value()) {
            return *decoded;
        }
        if (const auto decoded = decodeWithNamedEncoding(listing, "CP932"); decoded.has_value()) {
            return *decoded;
        }
        return decodeAsUtf8(listing);
    }

    const auto utf8Text = decodeAsUtf8(listing);
    if (!containsReplacementCharacter(utf8Text)) {
        return utf8Text;
    }

    if (const auto decoded = decodeWithNamedEncoding(listing, "Shift-JIS"); decoded.has_value()
        && !containsReplacementCharacter(*decoded)) {
        return *decoded;
    }
    if (const auto decoded = decodeWithNamedEncoding(listing, "CP932"); decoded.has_value()
        && !containsReplacementCharacter(*decoded)) {
        return *decoded;
    }

    const auto localText = decodeAsLocal8Bit(listing);
    if (!containsReplacementCharacter(localText)) {
        return localText;
    }

    return utf8Text;
}

size_t writeToString(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    auto *output = static_cast<std::string *>(userdata);
    const auto bytes = size * nmemb;
    output->append(ptr, bytes);
    return bytes;
}

size_t writeToFile(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    auto *file = static_cast<std::FILE *>(userdata);
    return std::fwrite(ptr, size, nmemb, file);
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

    if (siteProfile.protocol == domain::Protocol::Sftp
        && siteProfile.authenticationMethod == domain::AuthenticationMethod::PrivateKey) {
        const auto privateKeyPath = expandUserPath(QString::fromStdString(siteProfile.privateKeyPath).trimmed());
        curl_easy_setopt(curl, CURLOPT_SSH_PRIVATE_KEYFILE, privateKeyPath.toUtf8().constData());
        if (!password.empty()) {
            curl_easy_setopt(curl, CURLOPT_KEYPASSWD, password.c_str());
        }
        return true;
    }

    curl_easy_setopt(curl, CURLOPT_PASSWORD, password.c_str());
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

    const auto facts = line.left(separatorIndex).trimmed();
    if (!facts.endsWith(QLatin1Char(';'))
        || !facts.contains(QRegularExpression(QStringLiteral("(^|;)type="), QRegularExpression::CaseInsensitiveOption))) {
        return std::nullopt;
    }

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

    auto name = QStringList(columns.mid(8)).join(QLatin1Char(' '));
    if (type == QLatin1Char('l')) {
        const auto linkTargetSeparator = name.indexOf(QStringLiteral(" -> "));
        if (linkTargetSeparator > 0) {
            name = name.left(linkTargetSeparator);
        }
    }
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

std::optional<domain::RemoteEntry> parseDosListLine(const QString &line, const std::string &directory)
{
    static const QRegularExpression dosListExpression(
        QStringLiteral(R"(^\d{2}-\d{2}-\d{2,4}\s+\d{1,2}:\d{2}\s*[AP]M\s+(<DIR>|\d+)\s+(.+)$)"),
        QRegularExpression::CaseInsensitiveOption);
    const auto match = dosListExpression.match(line);
    if (!match.hasMatch()) {
        return std::nullopt;
    }

    const auto sizeOrDirectory = match.captured(1);
    const auto name = match.captured(2).trimmed();
    if (name.isEmpty() || name == QStringLiteral(".") || name == QStringLiteral("..")) {
        return std::nullopt;
    }

    const bool isDirectory = sizeOrDirectory.compare(QStringLiteral("<DIR>"), Qt::CaseInsensitive) == 0;
    return domain::RemoteEntry {
        .name = name.toStdString(),
        .path = joinRemotePath(directory, name.toStdString()),
        .isDirectory = isDirectory,
        .size = isDirectory ? 0 : sizeOrDirectory.toULongLong(),
    };
}

std::vector<domain::RemoteEntry> parseDirectoryListing(
    const std::string &listing,
    const std::string &directory,
    domain::FilenameEncoding filenameEncoding)
{
    std::vector<domain::RemoteEntry> entries;
    const auto lines = decodeDirectoryListing(listing, filenameEncoding)
                           .split(QRegularExpression(QStringLiteral("\\r?\\n")), Qt::SkipEmptyParts);

    for (const auto &line : lines) {
        if (auto entry = parseMlsdLine(line.trimmed(), directory); entry.has_value()) {
            entries.push_back(std::move(*entry));
            continue;
        }
        if (auto entry = parseUnixListLine(line.trimmed(), directory); entry.has_value()) {
            entries.push_back(std::move(*entry));
            continue;
        }
        if (auto entry = parseDosListLine(line.trimmed(), directory); entry.has_value()) {
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

struct CurlTransferTask {
    explicit CurlTransferTask(TransferJobId taskJobId, const TransferRequest &taskRequest)
        : request(taskRequest)
        , progress {
            .jobId = taskJobId,
            .state = domain::TransferState::Running,
            .transferredBytes = 0,
            .totalBytes = taskRequest.expectedSize,
        }
    {
    }

    TransferRequest request;
    mutable std::mutex mutex;
    TransferProgress progress;
    std::atomic_bool cancelRequested { false };
    std::jthread worker;
};

namespace {

struct CurlProgressContext {
    std::shared_ptr<CurlTransferTask> task;
    domain::TransferDirection direction { domain::TransferDirection::Upload };
};

int updateTransferProgress(
    void *clientp,
    curl_off_t downloadTotal,
    curl_off_t downloadNow,
    curl_off_t uploadTotal,
    curl_off_t uploadNow)
{
    auto *context = static_cast<CurlProgressContext *>(clientp);
    if (context == nullptr || context->task == nullptr) {
        return 0;
    }
    if (context->task->cancelRequested.load()) {
        return 1;
    }

    const auto total = context->direction == domain::TransferDirection::Upload ? uploadTotal : downloadTotal;
    const auto now = context->direction == domain::TransferDirection::Upload ? uploadNow : downloadNow;

    std::scoped_lock lock(context->task->mutex);
    context->task->progress.state = domain::TransferState::Running;
    context->task->progress.transferredBytes = now > 0 ? static_cast<std::uint64_t>(now) : 0;
    if (total > 0) {
        context->task->progress.totalBytes = static_cast<std::uint64_t>(total);
    }
    return 0;
}

void updateTaskState(
    const std::shared_ptr<CurlTransferTask> &task,
    domain::TransferState state,
    std::uint64_t transferredBytes = 0,
    std::uint64_t totalBytes = 0)
{
    std::scoped_lock lock(task->mutex);
    task->progress.state = state;
    if (transferredBytes > 0 || state == domain::TransferState::Completed) {
        task->progress.transferredBytes = transferredBytes;
    }
    if (totalBytes > 0) {
        task->progress.totalBytes = totalBytes;
    }
    if (state == domain::TransferState::Completed && task->progress.totalBytes > 0) {
        task->progress.transferredBytes = task->progress.totalBytes;
    }
    if (state == domain::TransferState::Completed
        && task->progress.totalBytes == 0
        && task->progress.transferredBytes > 0) {
        task->progress.totalBytes = task->progress.transferredBytes;
    }
}

void finishTaskFromCurlCode(const std::shared_ptr<CurlTransferTask> &task, CURLcode code)
{
    if (code == CURLE_OK) {
        updateTaskState(task, domain::TransferState::Completed);
        return;
    }
    if (task->cancelRequested.load() || code == CURLE_ABORTED_BY_CALLBACK) {
        updateTaskState(task, domain::TransferState::Cancelled);
        return;
    }

    updateTaskState(task, domain::TransferState::Failed);
}

void performDownload(
    const std::shared_ptr<CurlTransferTask> &task,
    const domain::SiteProfile &siteProfile,
    const std::string &password,
    const std::string &url)
{
    auto *file = std::fopen(task->request.localPath.c_str(), "wb");
    if (file == nullptr) {
        updateTaskState(task, domain::TransferState::Failed);
        return;
    }

    auto curl = curl_easy_init();
    if (curl == nullptr) {
        std::fclose(file);
        updateTaskState(task, domain::TransferState::Failed);
        return;
    }

    std::array<char, CURL_ERROR_SIZE> errorBuffer {};
    CurlProgressContext progressContext {
        .task = task,
        .direction = domain::TransferDirection::Download,
    };
    configureCommonOptions(curl, siteProfile, password, url, errorBuffer);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 0L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeToFile);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, updateTransferProgress);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &progressContext);

    const auto code = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    std::fclose(file);
    finishTaskFromCurlCode(task, code);
}

void performUpload(
    const std::shared_ptr<CurlTransferTask> &task,
    const domain::SiteProfile &siteProfile,
    const std::string &password,
    const std::string &url)
{
    auto *file = std::fopen(task->request.localPath.c_str(), "rb");
    if (file == nullptr) {
        updateTaskState(task, domain::TransferState::Failed);
        return;
    }

    auto curl = curl_easy_init();
    if (curl == nullptr) {
        std::fclose(file);
        updateTaskState(task, domain::TransferState::Failed);
        return;
    }

    std::uint64_t uploadSize = task->request.expectedSize;
    if (uploadSize == 0) {
        std::error_code error;
        uploadSize = std::filesystem::file_size(task->request.localPath, error);
        if (error) {
            uploadSize = 0;
        }
    }

    std::array<char, CURL_ERROR_SIZE> errorBuffer {};
    CurlProgressContext progressContext {
        .task = task,
        .direction = domain::TransferDirection::Upload,
    };
    configureCommonOptions(curl, siteProfile, password, url, errorBuffer);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 0L);
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(curl, CURLOPT_READDATA, file);
    if (uploadSize > 0) {
        curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, static_cast<curl_off_t>(uploadSize));
    }
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, updateTransferProgress);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &progressContext);

    const auto code = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    std::fclose(file);
    finishTaskFromCurlCode(task, code);
}

} // namespace

CurlTransferEngine::CurlTransferEngine()
{
    ensureCurlInitialized();
}

CurlTransferEngine::~CurlTransferEngine() = default;

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
    const auto listResult = listDirectory(siteProfile.protocol == domain::Protocol::Sftp ? "~" : "/");
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
    {
        std::scoped_lock lock(m_taskMutex);
        for (const auto &[jobId, task] : m_transferTasks) {
            (void)jobId;
            task->cancelRequested.store(true);
        }
    }
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

    const auto normalizedPath = normalizedRemotePath(remotePath);
    const auto fetchListing = [this, &normalizedPath](const char *customRequest) -> ListDirectoryResult {
        auto curl = curl_easy_init();
        if (curl == nullptr) {
            return {.operation = failed("curl_easy_init_failed", "Could not create libcurl handle.")};
        }

        std::array<char, CURL_ERROR_SIZE> errorBuffer {};
        std::string listing;
        configureCommonOptions(curl, *m_connectedSite, m_sessionPassword, buildUrl(normalizedPath, true), errorBuffer);
        if (customRequest != nullptr) {
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, customRequest);
        }
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
            .entries = parseDirectoryListing(listing, normalizedPath, m_connectedSite->filenameEncoding),
        };
    };

    if (m_connectedSite->protocol != domain::Protocol::Sftp) {
        const auto mlsdResult = fetchListing("MLSD");
        if (mlsdResult.operation.succeeded) {
            return mlsdResult;
        }
    }

    return fetchListing(nullptr);
}

StartTransferResult CurlTransferEngine::upload(const TransferRequest &request)
{
    return startTransfer(request);
}

StartTransferResult CurlTransferEngine::download(const TransferRequest &request)
{
    return startTransfer(request);
}

TransferProgressResult CurlTransferEngine::progress(TransferJobId jobId)
{
    const auto task = findTask(jobId);
    if (task == nullptr) {
        return {
            .operation = succeeded(),
            .found = false,
        };
    }

    std::scoped_lock lock(task->mutex);
    return {
        .operation = succeeded(),
        .progress = task->progress,
        .found = true,
    };
}

OperationResult CurlTransferEngine::cancel(TransferJobId jobId)
{
    auto connected = ensureConnected();
    if (!connected.succeeded) {
        return connected;
    }

    const auto task = findTask(jobId);
    if (task == nullptr) {
        return failed("not_found", "Transfer job was not found.");
    }

    task->cancelRequested.store(true);
    updateTaskState(task, domain::TransferState::Cancelled);
    return succeeded();
}

OperationResult CurlTransferEngine::ensureConnected() const
{
    if (!m_connectedSite.has_value()) {
        return failed("not_connected", "No remote session is connected.");
    }

    return succeeded();
}

std::string CurlTransferEngine::buildUrl(const std::string &remotePath, bool directoryPath) const
{
    if (!m_connectedSite.has_value()) {
        return {};
    }

    const auto encodedPath = percentEncodePathBytes(
        encodeRemotePath(remotePath, m_connectedSite->filenameEncoding, directoryPath));
    std::ostringstream url;
    url << protocolScheme(m_connectedSite->protocol)
        << "://"
        << m_connectedSite->host
        << ':'
        << m_connectedSite->port
        << '/'
        << encodedPath;
    return url.str();
}

StartTransferResult CurlTransferEngine::startTransfer(const TransferRequest &request)
{
    auto connected = ensureConnected();
    if (!connected.succeeded) {
        return {.operation = connected};
    }
    if (!m_connectedSite.has_value()) {
        return {.operation = failed("not_connected", "No remote session is connected.")};
    }

    if (request.direction == domain::TransferDirection::Upload && !std::filesystem::exists(request.localPath)) {
        return {.operation = failed("local_file_not_found", "Local file was not found.")};
    }
    if (request.direction == domain::TransferDirection::Download) {
        const auto parentPath = std::filesystem::path(request.localPath).parent_path();
        if (!parentPath.empty()) {
            std::error_code error;
            std::filesystem::create_directories(parentPath, error);
            if (error) {
                return {.operation = failed("create_directory_failed", error.message())};
            }
        }
    }

    const auto siteProfile = *m_connectedSite;
    const auto password = m_sessionPassword;
    const auto url = buildUrl(request.remotePath, false);

    std::shared_ptr<CurlTransferTask> task;
    TransferJobId jobId = 0;
    {
        std::scoped_lock lock(m_taskMutex);
        jobId = m_nextJobId++;
        task = std::make_shared<CurlTransferTask>(jobId, request);
        m_transferTasks.emplace(jobId, task);
    }

    task->worker = std::jthread([task, siteProfile, password, url]() {
        if (task->request.direction == domain::TransferDirection::Upload) {
            performUpload(task, siteProfile, password, url);
        } else {
            performDownload(task, siteProfile, password, url);
        }
    });

    return {
        .operation = succeeded(),
        .jobId = jobId,
    };
}

std::shared_ptr<CurlTransferTask> CurlTransferEngine::findTask(TransferJobId jobId) const
{
    std::scoped_lock lock(m_taskMutex);
    const auto task = m_transferTasks.find(jobId);
    if (task == m_transferTasks.end()) {
        return {};
    }

    return task->second;
}

} // namespace infra::transfer
