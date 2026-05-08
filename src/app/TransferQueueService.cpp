#include "app/TransferQueueService.h"

#include <filesystem>
#include <string_view>
#include <utility>

namespace {

std::string fileNameFromRemotePath(std::string_view remotePath)
{
    const auto lastSeparator = remotePath.find_last_of('/');
    if (lastSeparator == std::string_view::npos) {
        return std::string(remotePath);
    }

    return std::string(remotePath.substr(lastSeparator + 1));
}

std::string joinRemotePath(std::string_view directory, std::string_view fileName)
{
    if (directory.empty() || directory == "/") {
        return "/" + std::string(fileName);
    }

    std::string result(directory);
    if (result.back() != '/') {
        result.push_back('/');
    }
    result.append(fileName);
    return result;
}

std::string joinLocalPath(const std::string &directory, const std::string &fileName)
{
    return (std::filesystem::path(directory) / fileName).string();
}

} // namespace

namespace app {

const std::vector<domain::TransferJob> &TransferQueueService::jobs() const noexcept
{
    return m_jobs;
}

domain::TransferJob TransferQueueService::enqueueUpload(
    const std::string &localPath,
    const std::string &remoteDirectory,
    std::uint64_t expectedSize,
    domain::Protocol protocol)
{
    const auto fileName = std::filesystem::path(localPath).filename().string();
    return enqueue(domain::TransferRequest {
        .direction = domain::TransferDirection::Upload,
        .localPath = localPath,
        .remotePath = joinRemotePath(remoteDirectory, fileName),
        .expectedSize = expectedSize,
        .protocol = protocol,
    });
}

domain::TransferJob TransferQueueService::enqueueDownload(
    const std::string &remotePath,
    const std::string &localDirectory,
    std::uint64_t expectedSize,
    domain::Protocol protocol)
{
    return enqueue(domain::TransferRequest {
        .direction = domain::TransferDirection::Download,
        .localPath = joinLocalPath(localDirectory, fileNameFromRemotePath(remotePath)),
        .remotePath = remotePath,
        .expectedSize = expectedSize,
        .protocol = protocol,
    });
}

void TransferQueueService::clear()
{
    m_jobs.clear();
}

domain::TransferJob TransferQueueService::enqueue(domain::TransferRequest request)
{
    domain::TransferJob job;
    job.id = m_nextJobId++;
    job.request = std::move(request);
    job.progress = domain::TransferProgress {
        .jobId = job.id,
        .state = domain::TransferState::Pending,
        .transferredBytes = 0,
        .totalBytes = job.request.expectedSize,
    };

    m_jobs.push_back(job);
    return job;
}

} // namespace app
