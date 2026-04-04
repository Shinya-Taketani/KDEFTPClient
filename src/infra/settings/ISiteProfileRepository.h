#ifndef INFRA_SETTINGS_ISITEPROFILEREPOSITORY_H
#define INFRA_SETTINGS_ISITEPROFILEREPOSITORY_H

#include "domain/SiteProfile.h"

#include <string>
#include <vector>

namespace infra::settings {

struct SiteProfileRepositoryError {
    std::string code;
    std::string message;

    [[nodiscard]] bool hasError() const noexcept
    {
        return !code.empty() || !message.empty();
    }
};

struct SiteProfileRepositoryResult {
    bool succeeded { false };
    SiteProfileRepositoryError error;
};

struct FindSiteProfileResult {
    SiteProfileRepositoryResult operation;
    domain::SiteProfile siteProfile;
    bool found { false };
};

struct ListSiteProfilesResult {
    SiteProfileRepositoryResult operation;
    std::vector<domain::SiteProfile> siteProfiles;
};

class ISiteProfileRepository
{
public:
    virtual ~ISiteProfileRepository() = default;

    virtual SiteProfileRepositoryResult save(const domain::SiteProfile &siteProfile) = 0;
    virtual FindSiteProfileResult findByName(const std::string &connectionName) = 0;
    virtual ListSiteProfilesResult findAll() = 0;
    virtual SiteProfileRepositoryResult remove(const std::string &connectionName) = 0;
};

} // namespace infra::settings

#endif // INFRA_SETTINGS_ISITEPROFILEREPOSITORY_H
