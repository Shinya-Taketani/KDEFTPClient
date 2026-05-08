#ifndef APP_CREDENTIALSERVICE_H
#define APP_CREDENTIALSERVICE_H

#include "infra/credentials/ICredentialStore.h"

namespace app {

class CredentialService
{
public:
    explicit CredentialService(infra::credentials::ICredentialStore &credentialStore);

    [[nodiscard]] bool hasPassword(const std::string &credentialId);
    [[nodiscard]] infra::credentials::CredentialStoreResult savePassword(
        const std::string &credentialId,
        const std::string &password,
        const std::string &masterPassword);
    [[nodiscard]] infra::credentials::ReadCredentialResult readPassword(
        const std::string &credentialId,
        const std::string &masterPassword);
    [[nodiscard]] infra::credentials::CredentialStoreResult removePassword(const std::string &credentialId);

private:
    infra::credentials::ICredentialStore &m_credentialStore;
};

} // namespace app

#endif // APP_CREDENTIALSERVICE_H
