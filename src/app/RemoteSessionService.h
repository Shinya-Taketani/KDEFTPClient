#ifndef APP_REMOTESESSIONSERVICE_H
#define APP_REMOTESESSIONSERVICE_H

#include "domain/SiteProfile.h"
#include "infra/transfer/ITransferEngine.h"

namespace app {

class RemoteSessionService
{
public:
    explicit RemoteSessionService(infra::transfer::ITransferEngine &transferEngine);

    [[nodiscard]] infra::transfer::ConnectionResult connect(const domain::SiteProfile &siteProfile);
    [[nodiscard]] infra::transfer::OperationResult disconnect();
    [[nodiscard]] infra::transfer::ListDirectoryResult listDirectory(const std::string &remotePath);

private:
    infra::transfer::ITransferEngine &m_transferEngine;
};

} // namespace app

#endif // APP_REMOTESESSIONSERVICE_H
