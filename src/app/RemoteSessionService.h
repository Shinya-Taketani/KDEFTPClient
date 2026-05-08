#ifndef APP_REMOTESESSIONSERVICE_H
#define APP_REMOTESESSIONSERVICE_H

#include "domain/SiteProfile.h"
#include "infra/transfer/ITransferEngine.h"

namespace app {

class RemoteSessionService
{
public:
    explicit RemoteSessionService(infra::transfer::ITransferEngine &transferEngine);

    [[nodiscard]] infra::transfer::ConnectionResult connect(const infra::transfer::ConnectionRequest &request);
    [[nodiscard]] infra::transfer::OperationResult disconnect();
    [[nodiscard]] infra::transfer::ListDirectoryResult listDirectory(const std::string &remotePath);
    [[nodiscard]] infra::transfer::StartTransferResult upload(const domain::TransferRequest &request);
    [[nodiscard]] infra::transfer::StartTransferResult download(const domain::TransferRequest &request);
    [[nodiscard]] infra::transfer::OperationResult cancel(domain::TransferJobId jobId);

private:
    infra::transfer::ITransferEngine &m_transferEngine;
};

} // namespace app

#endif // APP_REMOTESESSIONSERVICE_H
