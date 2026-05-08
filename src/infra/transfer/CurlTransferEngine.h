#ifndef INFRA_TRANSFER_CURLTRANSFERENGINE_H
#define INFRA_TRANSFER_CURLTRANSFERENGINE_H

#include "infra/transfer/ITransferEngine.h"

#include <optional>
#include <string>

namespace infra::transfer {

class CurlTransferEngine final : public ITransferEngine
{
public:
    CurlTransferEngine();

    ConnectionResult connect(const ConnectionRequest &request) override;
    OperationResult disconnect() override;
    ListDirectoryResult listDirectory(const std::string &remotePath) override;
    StartTransferResult upload(const TransferRequest &request) override;
    StartTransferResult download(const TransferRequest &request) override;
    OperationResult cancel(TransferJobId jobId) override;

private:
    [[nodiscard]] OperationResult ensureConnected() const;
    [[nodiscard]] std::string buildUrl(const std::string &remotePath) const;

    std::optional<domain::SiteProfile> m_connectedSite;
    std::string m_sessionPassword;
    TransferJobId m_nextJobId { 1 };
};

} // namespace infra::transfer

#endif // INFRA_TRANSFER_CURLTRANSFERENGINE_H
