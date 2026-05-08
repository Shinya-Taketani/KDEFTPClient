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
    [[nodiscard]] domain::SiteProfile siteProfile() const;

private:
    void setupUi();
    void updatePortForProtocol(int protocolIndex);
    void updateAuthenticationControls();
    void browsePrivateKeyFile();
    [[nodiscard]] domain::Protocol selectedProtocol() const;
    [[nodiscard]] domain::AuthenticationMethod selectedAuthenticationMethod() const;

    QLineEdit *m_connectionNameLineEdit;
    QLineEdit *m_hostLineEdit;
    QSpinBox *m_portSpinBox;
    QLineEdit *m_userNameLineEdit;
    QComboBox *m_protocolComboBox;
    QComboBox *m_authenticationComboBox;
    QLineEdit *m_privateKeyPathLineEdit;
    QPushButton *m_browsePrivateKeyButton;
    QCheckBox *m_passiveModeCheckBox;
    QCheckBox *m_anonymousLoginCheckBox;
};

#endif // SITEPROFILEDIALOG_H
