#ifndef APP_SITEPROFILESERVICE_H
#define APP_SITEPROFILESERVICE_H

#include "infra/settings/ISiteProfileRepository.h"

namespace app {

class SiteProfileService
{
public:
    explicit SiteProfileService(infra::settings::ISiteProfileRepository &repository);

    [[nodiscard]] infra::settings::ListSiteProfilesResult listProfiles();
    [[nodiscard]] infra::settings::FindSiteProfileResult findProfileByName(const std::string &connectionName);

private:
    infra::settings::ISiteProfileRepository &m_repository;
};

} // namespace app

#endif // APP_SITEPROFILESERVICE_H
