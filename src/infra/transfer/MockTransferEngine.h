#ifndef INFRA_TRANSFER_MOCKTRANSFERENGINE_H
#define INFRA_TRANSFER_MOCKTRANSFERENGINE_H

#include "infra/transfer/ITransferEngine.h"

#include <optional>

namespace infra::transfer {

class MockTransferEngine final : public ITransferEngine
{
public:
    ConnectionResult connect(const ConnectionRequest &request) override;
    OperationResult disconnect() override;
    ListDirectoryResult listDirectory(const std::string &remotePath) override;
    StartTransferResult upload(const TransferRequest &request) override;
    StartTransferResult download(const TransferRequest &request) override;
    TransferProgressResult progress(TransferJobId jobId) override;
    OperationResult cancel(TransferJobId jobId) override;

private:
    [[nodiscard]] OperationResult ensureConnected() const;

    std::optional<domain::SiteProfile> m_connectedSite;
    TransferJobId m_nextJobId { 1 };
};

} // namespace infra::transfer

#endif // INFRA_TRANSFER_MOCKTRANSFERENGINE_H
