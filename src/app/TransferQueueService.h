#ifndef APP_TRANSFERQUEUESERVICE_H
#define APP_TRANSFERQUEUESERVICE_H

#include "domain/Transfer.h"

#include <cstdint>
#include <string>
#include <vector>

namespace app {

class TransferQueueService
{
public:
    [[nodiscard]] const std::vector<domain::TransferJob> &jobs() const noexcept;

    domain::TransferJob enqueueUpload(
        const std::string &localPath,
        const std::string &remoteDirectory,
        std::uint64_t expectedSize,
        domain::Protocol protocol);
    domain::TransferJob enqueueDownload(
        const std::string &remotePath,
        const std::string &localDirectory,
        std::uint64_t expectedSize,
        domain::Protocol protocol);
    [[nodiscard]] const domain::TransferJob *findJob(domain::TransferJobId jobId) const;
    bool updateState(domain::TransferJobId jobId, domain::TransferState state);
    bool updateProgress(
        domain::TransferJobId jobId,
        std::uint64_t transferredBytes,
        std::uint64_t totalBytes,
        domain::TransferState state);
    void clear();

private:
    [[nodiscard]] domain::TransferJob *findMutableJob(domain::TransferJobId jobId);
    [[nodiscard]] domain::TransferJob enqueue(domain::TransferRequest request);

    std::vector<domain::TransferJob> m_jobs;
    domain::TransferJobId m_nextJobId { 1 };
};

} // namespace app

#endif // APP_TRANSFERQUEUESERVICE_H
