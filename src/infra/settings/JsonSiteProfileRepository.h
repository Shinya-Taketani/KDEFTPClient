#ifndef INFRA_SETTINGS_JSONSITEPROFILEREPOSITORY_H
#define INFRA_SETTINGS_JSONSITEPROFILEREPOSITORY_H

#include "infra/settings/ISiteProfileRepository.h"

#include <QString>

#include <vector>

namespace infra::settings {

class JsonSiteProfileRepository final : public ISiteProfileRepository
{
public:
    explicit JsonSiteProfileRepository(std::vector<domain::SiteProfile> defaultProfiles = {});
    JsonSiteProfileRepository(QString filePath, std::vector<domain::SiteProfile> defaultProfiles = {});

    [[nodiscard]] static QString defaultFilePath();

    SiteProfileRepositoryResult save(const domain::SiteProfile &siteProfile) override;
    FindSiteProfileResult findByName(const std::string &connectionName) override;
    ListSiteProfilesResult findAll() override;
    SiteProfileRepositoryResult remove(const std::string &connectionName) override;

private:
    [[nodiscard]] ListSiteProfilesResult loadProfiles() const;
    [[nodiscard]] SiteProfileRepositoryResult writeProfiles(const std::vector<domain::SiteProfile> &profiles) const;

    QString m_filePath;
    std::vector<domain::SiteProfile> m_defaultProfiles;
};

} // namespace infra::settings

#endif // INFRA_SETTINGS_JSONSITEPROFILEREPOSITORY_H
