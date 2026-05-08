#include "app/RemoteSessionService.h"

namespace app {

RemoteSessionService::RemoteSessionService(infra::transfer::ITransferEngine &transferEngine)
    : m_transferEngine(transferEngine)
{
}

infra::transfer::ConnectionResult RemoteSessionService::connect(const infra::transfer::ConnectionRequest &request)
{
    return m_transferEngine.connect(request);
}

infra::transfer::OperationResult RemoteSessionService::disconnect()
{
    return m_transferEngine.disconnect();
}

infra::transfer::ListDirectoryResult RemoteSessionService::listDirectory(const std::string &remotePath)
{
    return m_transferEngine.listDirectory(remotePath);
}

infra::transfer::StartTransferResult RemoteSessionService::upload(const domain::TransferRequest &request)
{
    return m_transferEngine.upload(request);
}

infra::transfer::StartTransferResult RemoteSessionService::download(const domain::TransferRequest &request)
{
    return m_transferEngine.download(request);
}

infra::transfer::TransferProgressResult RemoteSessionService::progress(domain::TransferJobId jobId)
{
    return m_transferEngine.progress(jobId);
}

infra::transfer::OperationResult RemoteSessionService::cancel(domain::TransferJobId jobId)
{
    return m_transferEngine.cancel(jobId);
}

} // namespace app
