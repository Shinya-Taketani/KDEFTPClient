#include "infra/transfer/MockTransferEngine.h"

#include <QDir>
#include <QFileInfo>
#include <QString>

#include <cstdint>
#include <string>
#include <utility>

namespace {

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

domain::RemoteEntry directoryEntry(const std::string &directory, const std::string &name)
{
    return {
        .name = name,
        .path = joinRemotePath(directory, name),
        .isDirectory = true,
        .size = 0,
    };
}

domain::RemoteEntry fileEntry(const std::string &directory, const std::string &name, std::uint64_t size)
{
    return {
        .name = name,
        .path = joinRemotePath(directory, name),
        .isDirectory = false,
        .size = size,
    };
}

std::string normalizedPath(const std::string &path)
{
    QString qtPath = QString::fromStdString(path).trimmed();
    if (qtPath.isEmpty()) {
        return "/";
    }
    if (!qtPath.startsWith(QLatin1Char('/'))) {
        qtPath.prepend(QLatin1Char('/'));
    }

    return QDir::cleanPath(qtPath).toStdString();
}

} // namespace

namespace infra::transfer {

ConnectionResult MockTransferEngine::connect(const ConnectionRequest &request)
{
    const auto &siteProfile = request.siteProfile;

    if (siteProfile.host.empty()) {
        return {
            .operation = failed("empty_host", "Host is required."),
            .siteProfile = siteProfile,
        };
    }

    if (siteProfile.authenticationMethod == domain::AuthenticationMethod::PrivateKey) {
        const auto keyPath = expandUserPath(QString::fromStdString(siteProfile.privateKeyPath).trimmed());
        if (keyPath.isEmpty()) {
            return {
                .operation = failed("empty_private_key_path", "Private key path is required."),
                .siteProfile = siteProfile,
            };
        }
        if (!QFileInfo::exists(keyPath)) {
            return {
                .operation = failed("private_key_not_found", "Private key file was not found."),
                .siteProfile = siteProfile,
            };
        }
    }

    m_connectedSite = siteProfile;
    return {
        .operation = succeeded(),
        .siteProfile = siteProfile,
    };
}

OperationResult MockTransferEngine::disconnect()
{
    m_connectedSite.reset();
    return succeeded();
}

ListDirectoryResult MockTransferEngine::listDirectory(const std::string &remotePath)
{
    auto connected = ensureConnected();
    if (!connected.succeeded) {
        return {.operation = connected};
    }

    const auto path = normalizedPath(remotePath);
    return {
        .operation = succeeded(),
        .entries = {
            directoryEntry(path, "incoming"),
            directoryEntry(path, "releases"),
            directoryEntry(path, "logs"),
            fileEntry(path, "README.txt", 12 * 1024),
            fileEntry(path, "release.tar.gz", 48ULL * 1024ULL * 1024ULL),
        },
    };
}

StartTransferResult MockTransferEngine::upload(const TransferRequest &request)
{
    (void)request;

    auto connected = ensureConnected();
    if (!connected.succeeded) {
        return {.operation = connected};
    }

    return {
        .operation = succeeded(),
        .jobId = m_nextJobId++,
    };
}

StartTransferResult MockTransferEngine::download(const TransferRequest &request)
{
    (void)request;

    auto connected = ensureConnected();
    if (!connected.succeeded) {
        return {.operation = connected};
    }

    return {
        .operation = succeeded(),
        .jobId = m_nextJobId++,
    };
}

TransferProgressResult MockTransferEngine::progress(TransferJobId jobId)
{
    return {
        .operation = succeeded(),
        .progress = {
            .jobId = jobId,
            .state = domain::TransferState::Completed,
            .transferredBytes = 1,
            .totalBytes = 1,
        },
        .found = true,
    };
}

OperationResult MockTransferEngine::cancel(TransferJobId jobId)
{
    (void)jobId;

    auto connected = ensureConnected();
    if (!connected.succeeded) {
        return connected;
    }

    return succeeded();
}

OperationResult MockTransferEngine::ensureConnected() const
{
    if (!m_connectedSite.has_value()) {
        return failed("not_connected", "No remote session is connected.");
    }

    return succeeded();
}

} // namespace infra::transfer
