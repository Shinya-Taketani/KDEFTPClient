#include "ui/dialogs/SiteProfileDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace {

int protocolIndex(domain::Protocol protocol)
{
    switch (protocol) {
    case domain::Protocol::Ftp:
        return 0;
    case domain::Protocol::Ftps:
        return 1;
    case domain::Protocol::Sftp:
        return 2;
    }

    return 0;
}

int authenticationIndex(domain::AuthenticationMethod authenticationMethod)
{
    switch (authenticationMethod) {
    case domain::AuthenticationMethod::Password:
        return 0;
    case domain::AuthenticationMethod::PrivateKey:
        return 1;
    }

    return 0;
}

} // namespace

SiteProfileDialog::SiteProfileDialog(QWidget *parent)
    : QDialog(parent)
    , m_connectionNameLineEdit(nullptr)
    , m_hostLineEdit(nullptr)
    , m_portSpinBox(nullptr)
    , m_userNameLineEdit(nullptr)
    , m_protocolComboBox(nullptr)
    , m_authenticationComboBox(nullptr)
    , m_passwordLineEdit(nullptr)
    , m_savePasswordCheckBox(nullptr)
    , m_privateKeyPathLineEdit(nullptr)
    , m_browsePrivateKeyButton(nullptr)
    , m_passiveModeCheckBox(nullptr)
    , m_anonymousLoginCheckBox(nullptr)
{
    setupUi();
}

void SiteProfileDialog::setSiteProfile(const domain::SiteProfile &siteProfile)
{
    m_connectionNameLineEdit->setText(QString::fromStdString(siteProfile.connectionName));
    m_hostLineEdit->setText(QString::fromStdString(siteProfile.host));
    m_portSpinBox->setValue(siteProfile.port);
    m_userNameLineEdit->setText(QString::fromStdString(siteProfile.userName));
    m_protocolComboBox->setCurrentIndex(protocolIndex(siteProfile.protocol));
    m_authenticationComboBox->setCurrentIndex(authenticationIndex(siteProfile.authenticationMethod));
    m_privateKeyPathLineEdit->setText(QString::fromStdString(siteProfile.privateKeyPath));
    m_passiveModeCheckBox->setChecked(siteProfile.passiveMode);
    m_anonymousLoginCheckBox->setChecked(siteProfile.allowAnonymousLogin);
    updateAuthenticationControls();
}

domain::SiteProfile SiteProfileDialog::siteProfile() const
{
    domain::SiteProfile siteProfile;
    siteProfile.connectionName = m_connectionNameLineEdit->text().trimmed().toStdString();
    siteProfile.host = m_hostLineEdit->text().trimmed().toStdString();
    siteProfile.port = static_cast<std::uint16_t>(m_portSpinBox->value());
    siteProfile.userName = m_userNameLineEdit->text().trimmed().toStdString();
    siteProfile.protocol = selectedProtocol();
    siteProfile.authenticationMethod = selectedAuthenticationMethod();
    siteProfile.privateKeyPath = m_privateKeyPathLineEdit->text().trimmed().toStdString();
    siteProfile.passiveMode = m_passiveModeCheckBox->isChecked();
    siteProfile.allowAnonymousLogin = m_anonymousLoginCheckBox->isChecked();
    return siteProfile;
}

bool SiteProfileDialog::shouldSavePassword() const
{
    return m_savePasswordCheckBox->isChecked();
}

QString SiteProfileDialog::passwordForSaving() const
{
    return m_passwordLineEdit->text();
}

void SiteProfileDialog::setupUi()
{
    setWindowTitle(tr("接続先"));
    resize(460, 280);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    auto *formLayout = new QFormLayout;

    m_connectionNameLineEdit = new QLineEdit(this);
    m_connectionNameLineEdit->setPlaceholderText(tr("例: Production FTP"));
    formLayout->addRow(tr("接続名"), m_connectionNameLineEdit);

    m_hostLineEdit = new QLineEdit(this);
    m_hostLineEdit->setPlaceholderText(tr("ftp.example.com"));
    formLayout->addRow(tr("ホスト"), m_hostLineEdit);

    m_protocolComboBox = new QComboBox(this);
    m_protocolComboBox->addItem(QStringLiteral("FTP"));
    m_protocolComboBox->addItem(QStringLiteral("FTPS"));
    m_protocolComboBox->addItem(QStringLiteral("SFTP"));
    formLayout->addRow(tr("プロトコル"), m_protocolComboBox);

    m_portSpinBox = new QSpinBox(this);
    m_portSpinBox->setRange(1, 65535);
    m_portSpinBox->setValue(domain::defaultPortForProtocol(domain::Protocol::Ftp));
    formLayout->addRow(tr("ポート"), m_portSpinBox);

    m_userNameLineEdit = new QLineEdit(this);
    formLayout->addRow(tr("ユーザー名"), m_userNameLineEdit);

    m_authenticationComboBox = new QComboBox(this);
    m_authenticationComboBox->addItem(tr("パスワード"));
    m_authenticationComboBox->addItem(tr("キーファイル"));
    formLayout->addRow(tr("認証方式"), m_authenticationComboBox);

    m_passwordLineEdit = new QLineEdit(this);
    m_passwordLineEdit->setEchoMode(QLineEdit::Password);
    formLayout->addRow(tr("パスワード"), m_passwordLineEdit);

    m_savePasswordCheckBox = new QCheckBox(tr("パスワードを暗号化して保存"), this);
    formLayout->addRow(QString(), m_savePasswordCheckBox);

    auto *privateKeyLayout = new QHBoxLayout;
    privateKeyLayout->setContentsMargins(0, 0, 0, 0);
    privateKeyLayout->setSpacing(6);

    m_privateKeyPathLineEdit = new QLineEdit(this);
    m_privateKeyPathLineEdit->setPlaceholderText(tr("~/.ssh/id_ed25519"));
    privateKeyLayout->addWidget(m_privateKeyPathLineEdit);

    m_browsePrivateKeyButton = new QPushButton(tr("参照"), this);
    privateKeyLayout->addWidget(m_browsePrivateKeyButton);
    formLayout->addRow(tr("キーファイル"), privateKeyLayout);

    m_passiveModeCheckBox = new QCheckBox(tr("FTPでパッシブモードを使う"), this);
    m_passiveModeCheckBox->setChecked(true);
    formLayout->addRow(QString(), m_passiveModeCheckBox);

    m_anonymousLoginCheckBox = new QCheckBox(tr("匿名ログイン"), this);
    formLayout->addRow(QString(), m_anonymousLoginCheckBox);

    layout->addLayout(formLayout);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, [this]() {
        if (m_connectionNameLineEdit->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, tr("接続先"), tr("接続名を入力してください。"));
            return;
        }
        if (m_hostLineEdit->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, tr("接続先"), tr("ホストを入力してください。"));
            return;
        }
        if (selectedAuthenticationMethod() == domain::AuthenticationMethod::PrivateKey
            && m_privateKeyPathLineEdit->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, tr("接続先"), tr("キーファイルを指定してください。"));
            return;
        }
        if (m_savePasswordCheckBox->isChecked() && m_passwordLineEdit->text().isEmpty()) {
            QMessageBox::warning(this, tr("接続先"), tr("保存するパスワードを入力してください。"));
            return;
        }

        accept();
    });
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttonBox);

    connect(m_protocolComboBox, &QComboBox::currentIndexChanged, this, &SiteProfileDialog::updatePortForProtocol);
    connect(m_protocolComboBox, &QComboBox::currentIndexChanged, this, &SiteProfileDialog::updateAuthenticationControls);
    connect(m_authenticationComboBox, &QComboBox::currentIndexChanged, this, &SiteProfileDialog::updateAuthenticationControls);
    connect(m_anonymousLoginCheckBox, &QCheckBox::toggled, this, &SiteProfileDialog::updateAuthenticationControls);
    connect(m_browsePrivateKeyButton, &QPushButton::clicked, this, &SiteProfileDialog::browsePrivateKeyFile);
}

void SiteProfileDialog::updatePortForProtocol(int protocolIndex)
{
    const auto currentPort = static_cast<std::uint16_t>(m_portSpinBox->value());
    const bool isDefaultPort =
        currentPort == domain::defaultPortForProtocol(domain::Protocol::Ftp)
        || currentPort == domain::defaultPortForProtocol(domain::Protocol::Ftps)
        || currentPort == domain::defaultPortForProtocol(domain::Protocol::Sftp);

    if (!isDefaultPort) {
        return;
    }

    domain::Protocol protocol = domain::Protocol::Ftp;
    if (protocolIndex == 1) {
        protocol = domain::Protocol::Ftps;
    } else if (protocolIndex == 2) {
        protocol = domain::Protocol::Sftp;
    }

    m_portSpinBox->setValue(domain::defaultPortForProtocol(protocol));
    updateAuthenticationControls();
}

void SiteProfileDialog::updateAuthenticationControls()
{
    const bool isSftp = selectedProtocol() == domain::Protocol::Sftp;
    const bool usesPrivateKey = selectedAuthenticationMethod() == domain::AuthenticationMethod::PrivateKey;
    const bool passwordAvailable = !usesPrivateKey && !m_anonymousLoginCheckBox->isChecked();

    m_authenticationComboBox->setEnabled(isSftp);
    m_passwordLineEdit->setEnabled(passwordAvailable);
    m_savePasswordCheckBox->setEnabled(passwordAvailable);
    m_privateKeyPathLineEdit->setEnabled(isSftp && usesPrivateKey);
    m_browsePrivateKeyButton->setEnabled(isSftp && usesPrivateKey);

    if (!passwordAvailable) {
        m_savePasswordCheckBox->setChecked(false);
        m_passwordLineEdit->clear();
    }

    if (!isSftp) {
        m_authenticationComboBox->setCurrentIndex(authenticationIndex(domain::AuthenticationMethod::Password));
    }
}

void SiteProfileDialog::browsePrivateKeyFile()
{
    const auto selectedPath = QFileDialog::getOpenFileName(
        this,
        tr("キーファイルを選択"),
        m_privateKeyPathLineEdit->text().trimmed().isEmpty()
            ? QDir::homePath()
            : m_privateKeyPathLineEdit->text().trimmed(),
        tr("秘密鍵 (*.pem *.key id_*);;すべてのファイル (*)"));
    if (!selectedPath.isEmpty()) {
        m_privateKeyPathLineEdit->setText(selectedPath);
    }
}

domain::Protocol SiteProfileDialog::selectedProtocol() const
{
    switch (m_protocolComboBox->currentIndex()) {
    case 1:
        return domain::Protocol::Ftps;
    case 2:
        return domain::Protocol::Sftp;
    default:
        return domain::Protocol::Ftp;
    }
}

domain::AuthenticationMethod SiteProfileDialog::selectedAuthenticationMethod() const
{
    switch (m_authenticationComboBox->currentIndex()) {
    case 1:
        return domain::AuthenticationMethod::PrivateKey;
    default:
        return domain::AuthenticationMethod::Password;
    }
}
