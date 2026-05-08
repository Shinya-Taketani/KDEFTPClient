#include "ui/dialogs/SiteManagerDialog.h"

#include <QAbstractItemView>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace {

constexpr int kConnectionNameColumn = 0;

QString authenticationLabel(const domain::SiteProfile &siteProfile)
{
    if (siteProfile.protocol != domain::Protocol::Sftp) {
        return QStringLiteral("-");
    }

    switch (siteProfile.authenticationMethod) {
    case domain::AuthenticationMethod::Password:
        return QObject::tr("パスワード");
    case domain::AuthenticationMethod::PrivateKey:
        return QObject::tr("キーファイル");
    }

    return QStringLiteral("-");
}

}

SiteManagerDialog::SiteManagerDialog(QWidget *parent)
    : QDialog(parent)
    , m_descriptionLabel(nullptr)
    , m_siteTable(nullptr)
    , m_editButton(nullptr)
    , m_removeButton(nullptr)
    , m_connectButton(nullptr)
{
    setupUi();
}

void SiteManagerDialog::setSites(const std::vector<domain::SiteProfile> &sites)
{
    updateTable(sites);
}

void SiteManagerDialog::selectSiteByName(const QString &connectionName)
{
    if (m_siteTable == nullptr || connectionName.isEmpty()) {
        return;
    }

    for (int row = 0; row < m_siteTable->rowCount(); ++row) {
        const auto *nameItem = m_siteTable->item(row, kConnectionNameColumn);
        if (nameItem != nullptr && nameItem->text() == connectionName) {
            m_siteTable->selectRow(row);
            updateActionButtonState();
            return;
        }
    }
}

std::optional<int> SiteManagerDialog::selectedRow() const
{
    if (m_siteTable == nullptr) {
        return std::nullopt;
    }

    const auto selectedItems = m_siteTable->selectedItems();
    if (selectedItems.isEmpty()) {
        return std::nullopt;
    }

    return selectedItems.first()->row();
}

std::optional<QString> SiteManagerDialog::selectedSiteName() const
{
    const auto row = selectedRow();
    if (!row.has_value() || m_siteTable == nullptr) {
        return std::nullopt;
    }

    auto *nameItem = m_siteTable->item(*row, kConnectionNameColumn);
    if (nameItem == nullptr) {
        return std::nullopt;
    }

    return nameItem->text();
}

void SiteManagerDialog::setupUi()
{
    setWindowTitle(tr("接続先管理"));
    resize(760, 420);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    m_descriptionLabel = new QLabel(
        tr("保存済み接続先を選択して接続します。"), this);
    layout->addWidget(m_descriptionLabel);

    m_siteTable = new QTableWidget(this);
    m_siteTable->setColumnCount(6);
    m_siteTable->setHorizontalHeaderLabels(
        {tr("接続名"), tr("ホスト"), tr("ポート"), tr("ユーザー"), tr("プロトコル"), tr("認証")});
    m_siteTable->horizontalHeader()->setStretchLastSection(true);
    m_siteTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_siteTable->verticalHeader()->setVisible(false);
    m_siteTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_siteTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_siteTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(m_siteTable);

    auto *buttonBox = new QDialogButtonBox(this);
    auto *newButton = buttonBox->addButton(tr("新規"), QDialogButtonBox::ActionRole);
    m_editButton = buttonBox->addButton(tr("編集"), QDialogButtonBox::ActionRole);
    m_removeButton = buttonBox->addButton(tr("削除"), QDialogButtonBox::ActionRole);
    m_connectButton = buttonBox->addButton(tr("接続"), QDialogButtonBox::ActionRole);
    auto *closeButton = buttonBox->addButton(tr("閉じる"), QDialogButtonBox::RejectRole);

    newButton->setEnabled(true);
    updateActionButtonState();

    connect(newButton, &QPushButton::clicked, this, [this]() {
        emit createSiteRequested();
    });
    connect(m_editButton, &QPushButton::clicked, this, [this]() {
        const auto siteName = selectedSiteName();
        if (siteName.has_value()) {
            emit editSiteRequested(*siteName);
        }
    });
    connect(m_removeButton, &QPushButton::clicked, this, [this]() {
        const auto siteName = selectedSiteName();
        if (siteName.has_value()) {
            emit removeSiteRequested(*siteName);
        }
    });
    connect(closeButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_connectButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_siteTable, &QTableWidget::itemSelectionChanged, this, &SiteManagerDialog::updateActionButtonState);
    connect(m_siteTable, &QTableWidget::itemDoubleClicked, this, [this]() {
        if (selectedSiteName().has_value()) {
            accept();
        }
    });

    layout->addWidget(buttonBox);
}

void SiteManagerDialog::updateActionButtonState()
{
    const bool hasSelection = m_siteTable != nullptr && !m_siteTable->selectedItems().isEmpty();

    if (m_editButton != nullptr) {
        m_editButton->setEnabled(hasSelection);
    }

    if (m_removeButton != nullptr) {
        m_removeButton->setEnabled(hasSelection);
    }

    if (m_connectButton != nullptr) {
        m_connectButton->setEnabled(hasSelection);
    }
}

void SiteManagerDialog::updateTable(const std::vector<domain::SiteProfile> &siteProfiles)
{
    updateEmptyState(siteProfiles.empty());
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

        m_siteTable->setItem(row, kConnectionNameColumn, new QTableWidgetItem(QString::fromStdString(siteProfile.connectionName)));
        m_siteTable->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(siteProfile.host)));
        m_siteTable->setItem(row, 2, new QTableWidgetItem(QString::number(siteProfile.port)));
        m_siteTable->setItem(row, 3, new QTableWidgetItem(QString::fromStdString(siteProfile.userName)));
        m_siteTable->setItem(row, 4, new QTableWidgetItem(protocolLabel));
        m_siteTable->setItem(row, 5, new QTableWidgetItem(authenticationLabel(siteProfile)));
    }

    m_siteTable->clearSelection();
    updateActionButtonState();
}

void SiteManagerDialog::updateEmptyState(bool isEmpty)
{
    if (isEmpty) {
        m_descriptionLabel->setText(
            tr("接続先はまだ登録されていません。まずは「新規」から接続先を追加してください。"));
    } else {
        m_descriptionLabel->setText(tr("保存済み接続先を選択して接続します。"));
    }

    m_siteTable->setVisible(!isEmpty);
}
