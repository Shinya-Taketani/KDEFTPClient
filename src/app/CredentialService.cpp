#include "app/CredentialService.h"

namespace app {

CredentialService::CredentialService(infra::credentials::ICredentialStore &credentialStore)
    : m_credentialStore(credentialStore)
{
}

bool CredentialService::hasPassword(const std::string &credentialId)
{
    return m_credentialStore.hasPassword(credentialId);
}

infra::credentials::CredentialStoreResult CredentialService::savePassword(
    const std::string &credentialId,
    const std::string &password,
    const std::string &masterPassword)
{
    return m_credentialStore.savePassword(credentialId, password, masterPassword);
}

infra::credentials::ReadCredentialResult CredentialService::readPassword(
    const std::string &credentialId,
    const std::string &masterPassword)
{
    return m_credentialStore.readPassword(credentialId, masterPassword);
}

infra::credentials::CredentialStoreResult CredentialService::removePassword(const std::string &credentialId)
{
    return m_credentialStore.removePassword(credentialId);
}

} // namespace app
