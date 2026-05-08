#include "infra/settings/InMemorySiteProfileRepository.h"

#include <algorithm>
#include <utility>

namespace infra::settings {

InMemorySiteProfileRepository::InMemorySiteProfileRepository(std::vector<domain::SiteProfile> initialProfiles)
    : m_profiles(std::move(initialProfiles))
{
}

SiteProfileRepositoryResult InMemorySiteProfileRepository::save(const domain::SiteProfile &siteProfile)
{
    if (siteProfile.connectionName.empty()) {
        return {
            .succeeded = false,
            .error = {
                .code = "empty_connection_name",
                .message = "Connection name is required.",
            },
        };
    }

    auto existingProfile = std::find_if(m_profiles.begin(), m_profiles.end(), [&siteProfile](const auto &candidate) {
        return candidate.connectionName == siteProfile.connectionName;
    });

    if (existingProfile == m_profiles.end()) {
        m_profiles.push_back(siteProfile);
    } else {
        *existingProfile = siteProfile;
    }

    return {.succeeded = true};
}

FindSiteProfileResult InMemorySiteProfileRepository::findByName(const std::string &connectionName)
{
    auto existingProfile = std::find_if(m_profiles.begin(), m_profiles.end(), [&connectionName](const auto &candidate) {
        return candidate.connectionName == connectionName;
    });

    if (existingProfile == m_profiles.end()) {
        return {
            .operation = {.succeeded = true},
            .found = false,
        };
    }

    return {
        .operation = {.succeeded = true},
        .siteProfile = *existingProfile,
        .found = true,
    };
}

ListSiteProfilesResult InMemorySiteProfileRepository::findAll()
{
    return {
        .operation = {.succeeded = true},
        .siteProfiles = m_profiles,
    };
}

SiteProfileRepositoryResult InMemorySiteProfileRepository::remove(const std::string &connectionName)
{
    const auto oldSize = m_profiles.size();
    m_profiles.erase(
        std::remove_if(m_profiles.begin(), m_profiles.end(), [&connectionName](const auto &candidate) {
            return candidate.connectionName == connectionName;
        }),
        m_profiles.end());

    return {
        .succeeded = oldSize != m_profiles.size(),
        .error = oldSize == m_profiles.size()
            ? SiteProfileRepositoryError {
                .code = "not_found",
                .message = "Connection profile was not found.",
            }
            : SiteProfileRepositoryError {},
    };
}

} // namespace infra::settings
