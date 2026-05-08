#ifndef INFRA_TRANSFER_CURLTRANSFERENGINE_H
#define INFRA_TRANSFER_CURLTRANSFERENGINE_H

#include "infra/transfer/ITransferEngine.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>

namespace infra::transfer {

struct CurlTransferTask;

class CurlTransferEngine final : public ITransferEngine
{
public:
    CurlTransferEngine();
    ~CurlTransferEngine() override;

    ConnectionResult connect(const ConnectionRequest &request) override;
    OperationResult disconnect() override;
    ListDirectoryResult listDirectory(const std::string &remotePath) override;
    StartTransferResult upload(const TransferRequest &request) override;
    StartTransferResult download(const TransferRequest &request) override;
    TransferProgressResult progress(TransferJobId jobId) override;
    OperationResult cancel(TransferJobId jobId) override;

private:
    [[nodiscard]] OperationResult ensureConnected() const;
    [[nodiscard]] std::string buildUrl(const std::string &remotePath) const;
    [[nodiscard]] StartTransferResult startTransfer(const TransferRequest &request);
    [[nodiscard]] std::shared_ptr<CurlTransferTask> findTask(TransferJobId jobId) const;

    std::optional<domain::SiteProfile> m_connectedSite;
    std::string m_sessionPassword;
    TransferJobId m_nextJobId { 1 };
    mutable std::mutex m_taskMutex;
    std::unordered_map<TransferJobId, std::shared_ptr<CurlTransferTask>> m_transferTasks;
};

} // namespace infra::transfer

#endif // INFRA_TRANSFER_CURLTRANSFERENGINE_H
