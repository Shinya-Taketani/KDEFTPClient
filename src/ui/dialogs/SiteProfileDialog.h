#ifndef SITEPROFILEDIALOG_H
#define SITEPROFILEDIALOG_H

#include "domain/SiteProfile.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QPushButton;
class QSpinBox;

class SiteProfileDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SiteProfileDialog(QWidget *parent = nullptr);

    void setSiteProfile(const domain::SiteProfile &siteProfile);
    void setHasSavedPassword(bool hasSavedPassword);
    [[nodiscard]] domain::SiteProfile siteProfile() const;
    [[nodiscard]] bool shouldSavePassword() const;
    [[nodiscard]] QString passwordForSaving() const;

private:
    void setupUi();
    void updatePortForProtocol(int protocolIndex);
    void updateAuthenticationControls();
    void browseInitialLocalPath();
    void browsePrivateKeyFile();
    [[nodiscard]] domain::Protocol selectedProtocol() const;
    [[nodiscard]] domain::AuthenticationMethod selectedAuthenticationMethod() const;
    [[nodiscard]] domain::FilenameEncoding selectedFilenameEncoding() const;

    QLineEdit *m_connectionNameLineEdit;
    QLineEdit *m_hostLineEdit;
    QSpinBox *m_portSpinBox;
    QLineEdit *m_userNameLineEdit;
    QComboBox *m_protocolComboBox;
    QComboBox *m_authenticationComboBox;
    QLineEdit *m_initialLocalPathLineEdit;
    QPushButton *m_browseInitialLocalPathButton;
    QLineEdit *m_initialRemotePathLineEdit;
    QComboBox *m_filenameEncodingComboBox;
    QLineEdit *m_passwordLineEdit;
    QCheckBox *m_savePasswordCheckBox;
    QLineEdit *m_privateKeyPathLineEdit;
    QPushButton *m_browsePrivateKeyButton;
    QCheckBox *m_passiveModeCheckBox;
    QCheckBox *m_anonymousLoginCheckBox;
};

#endif // SITEPROFILEDIALOG_H
