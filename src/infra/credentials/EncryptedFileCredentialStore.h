#ifndef INFRA_CREDENTIALS_ENCRYPTEDFILECREDENTIALSTORE_H
#define INFRA_CREDENTIALS_ENCRYPTEDFILECREDENTIALSTORE_H

#include "infra/credentials/ICredentialStore.h"

#include <QJsonObject>
#include <QString>

namespace infra::credentials {

class EncryptedFileCredentialStore final : public ICredentialStore
{
public:
    EncryptedFileCredentialStore();
    explicit EncryptedFileCredentialStore(QString filePath);

    [[nodiscard]] static QString defaultFilePath();

    [[nodiscard]] bool hasPassword(const std::string &credentialId) override;
    CredentialStoreResult savePassword(
        const std::string &credentialId,
        const std::string &password,
        const std::string &masterPassword) override;
    ReadCredentialResult readPassword(
        const std::string &credentialId,
        const std::string &masterPassword) override;
    CredentialStoreResult removePassword(const std::string &credentialId) override;

private:
    [[nodiscard]] QJsonObject loadRootObject() const;
    [[nodiscard]] CredentialStoreResult writeRootObject(const QJsonObject &rootObject) const;

    QString m_filePath;
};

} // namespace infra::credentials

#endif // INFRA_CREDENTIALS_ENCRYPTEDFILECREDENTIALSTORE_H
