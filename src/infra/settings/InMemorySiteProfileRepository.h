#ifndef INFRA_SETTINGS_INMEMORYSITEPROFILEREPOSITORY_H
#define INFRA_SETTINGS_INMEMORYSITEPROFILEREPOSITORY_H

#include "infra/settings/ISiteProfileRepository.h"

#include <vector>

namespace infra::settings {

class InMemorySiteProfileRepository final : public ISiteProfileRepository
{
public:
    InMemorySiteProfileRepository() = default;
    explicit InMemorySiteProfileRepository(std::vector<domain::SiteProfile> initialProfiles);

    SiteProfileRepositoryResult save(const domain::SiteProfile &siteProfile) override;
    FindSiteProfileResult findByName(const std::string &connectionName) override;
    ListSiteProfilesResult findAll() override;
    SiteProfileRepositoryResult remove(const std::string &connectionName) override;

private:
    std::vector<domain::SiteProfile> m_profiles;
};

} // namespace infra::settings

#endif // INFRA_SETTINGS_INMEMORYSITEPROFILEREPOSITORY_H
