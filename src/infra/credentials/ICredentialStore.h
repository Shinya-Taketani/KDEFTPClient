#ifndef INFRA_CREDENTIALS_ICREDENTIALSTORE_H
#define INFRA_CREDENTIALS_ICREDENTIALSTORE_H

#include <string>

namespace infra::credentials {

struct CredentialStoreError {
    std::string code;
    std::string message;

    [[nodiscard]] bool hasError() const noexcept
    {
        return !code.empty() || !message.empty();
    }
};

struct CredentialStoreResult {
    bool succeeded { false };
    CredentialStoreError error;
};

struct ReadCredentialResult {
    CredentialStoreResult operation;
    std::string password;
    bool found { false };
};

class ICredentialStore
{
public:
    virtual ~ICredentialStore() = default;

    [[nodiscard]] virtual bool hasPassword(const std::string &credentialId) = 0;
    virtual CredentialStoreResult savePassword(
        const std::string &credentialId,
        const std::string &password,
        const std::string &masterPassword) = 0;
    virtual ReadCredentialResult readPassword(
        const std::string &credentialId,
        const std::string &masterPassword) = 0;
    virtual CredentialStoreResult removePassword(const std::string &credentialId) = 0;
};

} // namespace infra::credentials

#endif // INFRA_CREDENTIALS_ICREDENTIALSTORE_H
