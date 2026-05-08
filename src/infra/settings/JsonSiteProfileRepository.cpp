#include "infra/settings/JsonSiteProfileRepository.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

#include <algorithm>
#include <utility>

namespace {

constexpr auto kSitesFileName = "sites.json";

QString protocolName(domain::Protocol protocol)
{
    switch (protocol) {
    case domain::Protocol::Ftp:
        return QStringLiteral("ftp");
    case domain::Protocol::Ftps:
        return QStringLiteral("ftps");
    case domain::Protocol::Sftp:
        return QStringLiteral("sftp");
    }

    return QStringLiteral("ftp");
}

domain::Protocol protocolFromName(const QString &protocolName)
{
    if (protocolName == QStringLiteral("ftps")) {
        return domain::Protocol::Ftps;
    }
    if (protocolName == QStringLiteral("sftp")) {
        return domain::Protocol::Sftp;
    }

    return domain::Protocol::Ftp;
}

QString authenticationMethodName(domain::AuthenticationMethod authenticationMethod)
{
    switch (authenticationMethod) {
    case domain::AuthenticationMethod::Password:
        return QStringLiteral("password");
    case domain::AuthenticationMethod::PrivateKey:
        return QStringLiteral("private_key");
    }

    return QStringLiteral("password");
}

domain::AuthenticationMethod authenticationMethodFromName(const QString &authenticationMethodName)
{
    if (authenticationMethodName == QStringLiteral("private_key")) {
        return domain::AuthenticationMethod::PrivateKey;
    }

    return domain::AuthenticationMethod::Password;
}

QJsonObject toJson(const domain::SiteProfile &siteProfile)
{
    QJsonObject object;
    object.insert(QStringLiteral("connectionName"), QString::fromStdString(siteProfile.connectionName));
    object.insert(QStringLiteral("host"), QString::fromStdString(siteProfile.host));
    object.insert(QStringLiteral("port"), static_cast<int>(siteProfile.port));
    object.insert(QStringLiteral("userName"), QString::fromStdString(siteProfile.userName));
    object.insert(QStringLiteral("protocol"), protocolName(siteProfile.protocol));
    object.insert(QStringLiteral("authenticationMethod"), authenticationMethodName(siteProfile.authenticationMethod));
    object.insert(QStringLiteral("privateKeyPath"), QString::fromStdString(siteProfile.privateKeyPath));
    object.insert(QStringLiteral("passiveMode"), siteProfile.passiveMode);
    object.insert(QStringLiteral("allowAnonymousLogin"), siteProfile.allowAnonymousLogin);
    object.insert(QStringLiteral("useSshTunnel"), siteProfile.useSshTunnel);
    return object;
}

domain::SiteProfile fromJson(const QJsonObject &object)
{
    domain::SiteProfile siteProfile;
    siteProfile.connectionName = object.value(QStringLiteral("connectionName")).toString().toStdString();
    siteProfile.host = object.value(QStringLiteral("host")).toString().toStdString();
    siteProfile.protocol = protocolFromName(object.value(QStringLiteral("protocol")).toString());
    siteProfile.port = static_cast<std::uint16_t>(
        object.value(QStringLiteral("port")).toInt(domain::defaultPortForProtocol(siteProfile.protocol)));
    siteProfile.userName = object.value(QStringLiteral("userName")).toString().toStdString();
    siteProfile.authenticationMethod =
        authenticationMethodFromName(object.value(QStringLiteral("authenticationMethod")).toString());
    siteProfile.privateKeyPath = object.value(QStringLiteral("privateKeyPath")).toString().toStdString();
    siteProfile.passiveMode = object.value(QStringLiteral("passiveMode")).toBool(true);
    siteProfile.allowAnonymousLogin = object.value(QStringLiteral("allowAnonymousLogin")).toBool(false);
    siteProfile.useSshTunnel = object.value(QStringLiteral("useSshTunnel")).toBool(false);
    return siteProfile;
}

infra::settings::SiteProfileRepositoryResult failedResult(std::string code, std::string message)
{
    return {
        .succeeded = false,
        .error = {
            .code = std::move(code),
            .message = std::move(message),
        },
    };
}

} // namespace

namespace infra::settings {

JsonSiteProfileRepository::JsonSiteProfileRepository(std::vector<domain::SiteProfile> defaultProfiles)
    : JsonSiteProfileRepository(defaultFilePath(), std::move(defaultProfiles))
{
}

JsonSiteProfileRepository::JsonSiteProfileRepository(QString filePath, std::vector<domain::SiteProfile> defaultProfiles)
    : m_filePath(std::move(filePath))
    , m_defaultProfiles(std::move(defaultProfiles))
{
}

QString JsonSiteProfileRepository::defaultFilePath()
{
    auto configDirectory = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (configDirectory.isEmpty()) {
        configDirectory = QDir::homePath() + QStringLiteral("/.config/kdeftpclient");
    }

    return QDir(configDirectory).filePath(QString::fromLatin1(kSitesFileName));
}

SiteProfileRepositoryResult JsonSiteProfileRepository::save(const domain::SiteProfile &siteProfile)
{
    if (siteProfile.connectionName.empty()) {
        return failedResult("empty_connection_name", "Connection name is required.");
    }

    auto profilesResult = loadProfiles();
    if (!profilesResult.operation.succeeded) {
        return profilesResult.operation;
    }

    auto &profiles = profilesResult.siteProfiles;
    auto existingProfile = std::find_if(profiles.begin(), profiles.end(), [&siteProfile](const auto &candidate) {
        return candidate.connectionName == siteProfile.connectionName;
    });

    if (existingProfile == profiles.end()) {
        profiles.push_back(siteProfile);
    } else {
        *existingProfile = siteProfile;
    }

    return writeProfiles(profiles);
}

FindSiteProfileResult JsonSiteProfileRepository::findByName(const std::string &connectionName)
{
    auto profilesResult = loadProfiles();
    if (!profilesResult.operation.succeeded) {
        return {
            .operation = profilesResult.operation,
            .found = false,
        };
    }

    auto existingProfile = std::find_if(profilesResult.siteProfiles.begin(), profilesResult.siteProfiles.end(), [&connectionName](const auto &candidate) {
        return candidate.connectionName == connectionName;
    });

    if (existingProfile == profilesResult.siteProfiles.end()) {
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

ListSiteProfilesResult JsonSiteProfileRepository::findAll()
{
    return loadProfiles();
}

SiteProfileRepositoryResult JsonSiteProfileRepository::remove(const std::string &connectionName)
{
    auto profilesResult = loadProfiles();
    if (!profilesResult.operation.succeeded) {
        return profilesResult.operation;
    }

    auto &profiles = profilesResult.siteProfiles;
    const auto oldSize = profiles.size();
    profiles.erase(
        std::remove_if(profiles.begin(), profiles.end(), [&connectionName](const auto &candidate) {
            return candidate.connectionName == connectionName;
        }),
        profiles.end());

    if (oldSize == profiles.size()) {
        return failedResult("not_found", "Connection profile was not found.");
    }

    return writeProfiles(profiles);
}

ListSiteProfilesResult JsonSiteProfileRepository::loadProfiles() const
{
    QFile file(m_filePath);
    if (!file.exists()) {
        return {
            .operation = {.succeeded = true},
            .siteProfiles = m_defaultProfiles,
        };
    }

    if (!file.open(QIODevice::ReadOnly)) {
        return {
            .operation = failedResult("open_failed", file.errorString().toStdString()),
        };
    }

    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return {
            .operation = failedResult("parse_failed", parseError.errorString().toStdString()),
        };
    }

    const auto sites = document.object().value(QStringLiteral("sites")).toArray();
    std::vector<domain::SiteProfile> profiles;
    profiles.reserve(static_cast<std::size_t>(sites.size()));

    for (const auto &site : sites) {
        if (!site.isObject()) {
            continue;
        }

        auto profile = fromJson(site.toObject());
        if (!profile.connectionName.empty()) {
            profiles.push_back(std::move(profile));
        }
    }

    return {
        .operation = {.succeeded = true},
        .siteProfiles = std::move(profiles),
    };
}

SiteProfileRepositoryResult JsonSiteProfileRepository::writeProfiles(const std::vector<domain::SiteProfile> &profiles) const
{
    const QFileInfo fileInfo(m_filePath);
    QDir directory(fileInfo.absolutePath());
    if (!directory.exists() && !directory.mkpath(QStringLiteral("."))) {
        return failedResult("create_directory_failed", "Could not create settings directory.");
    }

    QJsonArray sites;
    for (const auto &profile : profiles) {
        sites.append(toJson(profile));
    }

    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("sites"), sites);

    QSaveFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return failedResult("open_failed", file.errorString().toStdString());
    }

    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        return failedResult("write_failed", file.errorString().toStdString());
    }

    return {.succeeded = true};
}

} // namespace infra::settings
