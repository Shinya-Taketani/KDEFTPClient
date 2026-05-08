#include "app/SiteProfileService.h"

namespace app {

SiteProfileService::SiteProfileService(infra::settings::ISiteProfileRepository &repository)
    : m_repository(repository)
{
}

infra::settings::ListSiteProfilesResult SiteProfileService::listProfiles()
{
    return m_repository.findAll();
}

infra::settings::FindSiteProfileResult SiteProfileService::findProfileByName(const std::string &connectionName)
{
    return m_repository.findByName(connectionName);
}

} // namespace app
