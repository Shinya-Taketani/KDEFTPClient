#include "ui/dialogs/SiteManagerDialog.h"

#include <QAbstractItemView>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

SiteManagerDialog::SiteManagerDialog(QWidget *parent)
    : QDialog(parent)
    , m_siteTable(nullptr)
{
    setupUi();
    populatePlaceholderData();
}

void SiteManagerDialog::setSiteProfiles(const std::vector<domain::SiteProfile> &siteProfiles)
{
    updateTable(siteProfiles);
}

void SiteManagerDialog::setupUi()
{
    setWindowTitle(tr("接続先管理"));
    resize(760, 420);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    auto *descriptionLabel = new QLabel(
        tr("保存済み接続先の一覧を表示するためのダミー画面です。"), this);
    layout->addWidget(descriptionLabel);

    m_siteTable = new QTableWidget(this);
    m_siteTable->setColumnCount(5);
    m_siteTable->setHorizontalHeaderLabels(
        {tr("接続名"), tr("ホスト"), tr("ポート"), tr("ユーザー"), tr("プロトコル")});
    m_siteTable->horizontalHeader()->setStretchLastSection(true);
    m_siteTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_siteTable->verticalHeader()->setVisible(false);
    m_siteTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_siteTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_siteTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(m_siteTable);

    auto *buttonBox = new QDialogButtonBox(this);
    auto *newButton = buttonBox->addButton(tr("新規"), QDialogButtonBox::ActionRole);
    auto *editButton = buttonBox->addButton(tr("編集"), QDialogButtonBox::ActionRole);
    auto *removeButton = buttonBox->addButton(tr("削除"), QDialogButtonBox::ActionRole);
    auto *connectButton = buttonBox->addButton(tr("接続"), QDialogButtonBox::ActionRole);
    auto *closeButton = buttonBox->addButton(tr("閉じる"), QDialogButtonBox::RejectRole);

    newButton->setEnabled(false);
    editButton->setEnabled(false);
    removeButton->setEnabled(false);
    connectButton->setEnabled(false);

    connect(closeButton, &QPushButton::clicked, this, &QDialog::reject);

    layout->addWidget(buttonBox);
}

void SiteManagerDialog::populatePlaceholderData()
{
    std::vector<domain::SiteProfile> profiles;

    domain::SiteProfile ftpProfile;
    ftpProfile.connectionName = "Example FTP";
    ftpProfile.host = "ftp.example.com";
    ftpProfile.port = 21;
    ftpProfile.userName = "demo";
    ftpProfile.protocol = domain::Protocol::Ftp;
    profiles.push_back(ftpProfile);

    domain::SiteProfile sftpProfile;
    sftpProfile.connectionName = "Example SFTP";
    sftpProfile.host = "sftp.example.net";
    sftpProfile.port = 22;
    sftpProfile.userName = "deploy";
    sftpProfile.protocol = domain::Protocol::Sftp;
    sftpProfile.useSshTunnel = true;
    profiles.push_back(sftpProfile);

    updateTable(profiles);
}

void SiteManagerDialog::updateTable(const std::vector<domain::SiteProfile> &siteProfiles)
{
    m_siteTable->setRowCount(static_cast<int>(siteProfiles.size()));

    for (int row = 0; row < static_cast<int>(siteProfiles.size()); ++row) {
        const auto &siteProfile = siteProfiles[static_cast<std::size_t>(row)];

        QString protocolLabel;
        switch (siteProfile.protocol) {
        case domain::Protocol::Ftp:
            protocolLabel = QStringLiteral("FTP");
            break;
        case domain::Protocol::Ftps:
            protocolLabel = QStringLiteral("FTPS");
            break;
        case domain::Protocol::Sftp:
            protocolLabel = QStringLiteral("SFTP");
            break;
        }

        m_siteTable->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(siteProfile.connectionName)));
        m_siteTable->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(siteProfile.host)));
        m_siteTable->setItem(row, 2, new QTableWidgetItem(QString::number(siteProfile.port)));
        m_siteTable->setItem(row, 3, new QTableWidgetItem(QString::fromStdString(siteProfile.userName)));
        m_siteTable->setItem(row, 4, new QTableWidgetItem(protocolLabel));
    }

    if (!siteProfiles.empty()) {
        m_siteTable->selectRow(0);
    }
}
