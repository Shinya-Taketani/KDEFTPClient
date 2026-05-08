#include "app/RemoteSessionService.h"

namespace app {

RemoteSessionService::RemoteSessionService(infra::transfer::ITransferEngine &transferEngine)
    : m_transferEngine(transferEngine)
{
}

infra::transfer::ConnectionResult RemoteSessionService::connect(const domain::SiteProfile &siteProfile)
{
    return m_transferEngine.connect(siteProfile);
}

infra::transfer::OperationResult RemoteSessionService::disconnect()
{
    return m_transferEngine.disconnect();
}

infra::transfer::ListDirectoryResult RemoteSessionService::listDirectory(const std::string &remotePath)
{
    return m_transferEngine.listDirectory(remotePath);
}

} // namespace app
