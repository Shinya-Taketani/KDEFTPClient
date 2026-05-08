#include "mainwindow.h"
#include "ui/dialogs/SiteManagerDialog.h"
#include "ui/dialogs/SiteProfileDialog.h"
#include "ui/widgets/TransferQueueWidget.h"
#include "ui_mainwindow.h"

#include <QAction>
#include <QDir>
#include <QFileInfo>
#include <QHeaderView>
#include <QInputDialog>
#include <QKeySequence>
#include <QLineEdit>
#include <QMessageBox>
#include <QStyle>
#include <QTime>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include <algorithm>
#include <cstdint>
#include <optional>

namespace {

QString formattedSize(qint64 size)
{
    if (size < 0) {
        return QStringLiteral("-");
    }

    constexpr double kKiB = 1024.0;
    constexpr double kMiB = kKiB * 1024.0;
    constexpr double kGiB = kMiB * 1024.0;

    if (size >= kGiB) {
        return QStringLiteral("%1 GB").arg(size / kGiB, 0, 'f', 1);
    }
    if (size >= kMiB) {
        return QStringLiteral("%1 MB").arg(size / kMiB, 0, 'f', 1);
    }
    if (size >= kKiB) {
        return QStringLiteral("%1 KB").arg(size / kKiB, 0, 'f', 1);
    }

    return QStringLiteral("%1 B").arg(size);
}

QString remoteFileNameFromPath(const QString &remotePath)
{
    const QString normalizedPath = QDir::cleanPath(remotePath);
    const QString fileName = QFileInfo(normalizedPath).fileName();
    return fileName.isEmpty() ? normalizedPath : fileName;
}

QString normalizedRemotePathForUi(const QString &path, const QString &fallbackPath = QStringLiteral("/"))
{
    QString normalizedPath = path.trimmed();
    if (normalizedPath.isEmpty()) {
        normalizedPath = fallbackPath;
    }

    if (normalizedPath == QStringLiteral("~") || normalizedPath.startsWith(QStringLiteral("~/"))) {
        return QDir::cleanPath(normalizedPath);
    }

    if (!normalizedPath.startsWith(QLatin1Char('/'))) {
        normalizedPath.prepend(QLatin1Char('/'));
    }

    return QDir::cleanPath(normalizedPath);
}

QString defaultRemotePathForSite(const domain::SiteProfile &siteProfile)
{
    return siteProfile.protocol == domain::Protocol::Sftp
        ? QStringLiteral("~")
        : QStringLiteral("/");
}

std::optional<QString> parentRemotePath(const QString &path)
{
    const QString normalizedPath = normalizedRemotePathForUi(path);
    if (normalizedPath == QStringLiteral("/") || normalizedPath == QStringLiteral("~")) {
        return std::nullopt;
    }

    if (normalizedPath.startsWith(QStringLiteral("~/"))) {
        const QString relativePath = normalizedPath.mid(2);
        const auto separatorIndex = relativePath.lastIndexOf(QLatin1Char('/'));
        if (separatorIndex < 0) {
            return QStringLiteral("~");
        }

        return QStringLiteral("~/") + relativePath.left(separatorIndex);
    }

    const QString parentPath = QFileInfo(normalizedPath).path();
    if (parentPath.isEmpty() || parentPath == QStringLiteral(".")) {
        return QStringLiteral("/");
    }

    return parentPath;
}

QString protocolLabel(domain::Protocol protocol)
{
    switch (protocol) {
    case domain::Protocol::Ftp:
        return QStringLiteral("FTP");
    case domain::Protocol::Ftps:
        return QStringLiteral("FTPS");
    case domain::Protocol::Sftp:
        return QStringLiteral("SFTP");
    }

    return QStringLiteral("Unknown");
}

QString authenticationLabel(const domain::SiteProfile &siteProfile)
{
    if (siteProfile.protocol != domain::Protocol::Sftp) {
        return QObject::tr("standard authentication");
    }

    switch (siteProfile.authenticationMethod) {
    case domain::AuthenticationMethod::Password:
        return QObject::tr("password authentication");
    case domain::AuthenticationMethod::PrivateKey:
        return QObject::tr("key file authentication");
    }

    return QObject::tr("unknown authentication");
}

int progressPercent(const domain::TransferProgress &progress)
{
    if (progress.state == domain::TransferState::Completed) {
        return 100;
    }
    if (progress.totalBytes == 0) {
        return 0;
    }

    const auto percent = (progress.transferredBytes * 100) / progress.totalBytes;
    return static_cast<int>(std::min<std::uint64_t>(percent, 100));
}

QString stateLabel(domain::TransferState state, bool connected)
{
    switch (state) {
    case domain::TransferState::Pending:
        return connected ? QObject::tr("待機中") : QObject::tr("未接続で保留");
    case domain::TransferState::Running:
        return QObject::tr("転送中");
    case domain::TransferState::Completed:
        return QObject::tr("完了");
    case domain::TransferState::Failed:
        return QObject::tr("失敗");
    case domain::TransferState::Cancelled:
        return QObject::tr("キャンセル済み");
    }

    return QObject::tr("不明");
}

TransferQueueWidget::QueueItem queueItemFromJob(const domain::TransferJob &job, bool connected)
{
    const bool isUpload = job.request.direction == domain::TransferDirection::Upload;
    return {
        isUpload ? QObject::tr("アップロード") : QObject::tr("ダウンロード"),
        QString::fromStdString(isUpload ? job.request.localPath : job.request.remotePath),
        QString::fromStdString(isUpload ? job.request.remotePath : job.request.localPath),
        stateLabel(job.progress.state, connected),
        progressPercent(job.progress),
    };
}

bool isTerminalTransferState(domain::TransferState state)
{
    return state == domain::TransferState::Completed
        || state == domain::TransferState::Failed
        || state == domain::TransferState::Cancelled;
}

}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_transferQueueWidget(nullptr)
    , m_transferProgressTimer(nullptr)
    , m_localPath(QDir::homePath())
    , m_remotePath(QStringLiteral("/"))
    , m_siteProfileRepository(sampleSites())
    , m_siteProfileService(m_siteProfileRepository)
    , m_credentialService(m_credentialStore)
    , m_remoteSessionService(m_transferEngine)
    , m_currentProtocol(domain::Protocol::Ftp)
    , m_connected(false)
{
    ui->setupUi(this);
    setupInitialState();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupInitialState()
{
    auto *layout = qobject_cast<QVBoxLayout *>(ui->transferQueueTab->layout());
    m_transferQueueWidget = new TransferQueueWidget(ui->transferQueueTab);
    layout->addWidget(m_transferQueueWidget);

    m_transferProgressTimer = new QTimer(this);
    m_transferProgressTimer->setInterval(500);
    connect(m_transferProgressTimer, &QTimer::timeout, this, &MainWindow::pollTransferProgress);

    ui->localPaneTitleLabel->setText(tr("ローカル"));
    ui->remotePaneTitleLabel->setText(tr("リモート"));
    ui->localPathLineEdit->setReadOnly(false);
    ui->remotePathLineEdit->setReadOnly(false);
    ui->bottomTabWidget->setTabText(0, tr("転送キュー"));
    ui->bottomTabWidget->setTabText(1, tr("ログ"));
    ui->mainSplitter->setSizes({560, 220});
    ui->paneSplitter->setSizes({640, 640});

    ui->localFileTreeWidget->setRootIsDecorated(false);
    ui->localFileTreeWidget->setAlternatingRowColors(true);
    ui->localFileTreeWidget->header()->setSectionResizeMode(QHeaderView::Stretch);
    ui->remoteFileTreeWidget->setRootIsDecorated(false);
    ui->remoteFileTreeWidget->setAlternatingRowColors(true);
    ui->remoteFileTreeWidget->header()->setSectionResizeMode(QHeaderView::Stretch);

    setupActions();
    loadLocalDirectory(m_localPath);
    clearRemotePanel(m_remotePath);
    ui->logPlainTextEdit->clear();
    appendLogMessage(tr("kdeftpclient started"));
    appendLogMessage(tr("Commander-style shell ready"));

    ui->statusbar->showMessage(tr("準備完了"));
}

void MainWindow::setupActions()
{
    ui->actionNewSite->setIcon(style()->standardIcon(QStyle::SP_FileDialogDetailedView));
    ui->actionConnect->setIcon(style()->standardIcon(QStyle::SP_DialogApplyButton));
    ui->actionDisconnect->setIcon(style()->standardIcon(QStyle::SP_DialogCloseButton));
    ui->actionUpload->setIcon(style()->standardIcon(QStyle::SP_ArrowRight));
    ui->actionDownload->setIcon(style()->standardIcon(QStyle::SP_ArrowLeft));
    ui->actionStop->setIcon(style()->standardIcon(QStyle::SP_BrowserStop));
    ui->actionRefresh->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));

    ui->actionUpload->setShortcut(QKeySequence(Qt::Key_F5));
    ui->actionDownload->setShortcut(QKeySequence(Qt::Key_F6));
    ui->actionRefresh->setShortcut(QKeySequence::Refresh);
    ui->actionToggleQueue->setCheckable(true);
    ui->actionToggleQueue->setChecked(true);

    connect(ui->actionNewSite, &QAction::triggered, this, &MainWindow::openSiteManager);
    connect(ui->actionConnect, &QAction::triggered, this, &MainWindow::connectToSelectedSite);
    connect(ui->actionDisconnect, &QAction::triggered, this, [this]() {
        for (const auto &[queueJobId, backendJobId] : m_backendJobIds) {
            const auto cancelResult = m_remoteSessionService.cancel(backendJobId);
            if (!cancelResult.succeeded) {
                appendLogMessage(tr("Transfer cancel failed: %1").arg(QString::fromStdString(cancelResult.error.message)));
            }
            m_transferQueueService.updateState(queueJobId, domain::TransferState::Cancelled);
        }
        m_backendJobIds.clear();
        const auto result = m_remoteSessionService.disconnect();
        if (!result.succeeded) {
            appendLogMessage(tr("Disconnect failed: %1").arg(QString::fromStdString(result.error.message)));
        }
        m_connected = false;
        clearRemotePanel(QStringLiteral("/"));
        renderTransferQueue();
        appendLogMessage(tr("Disconnected"));
        ui->statusbar->showMessage(tr("切断しました"));
    });
    connect(ui->actionExit, &QAction::triggered, this, &QWidget::close);
    connect(ui->actionUpload, &QAction::triggered, this, &MainWindow::enqueueUpload);
    connect(ui->actionDownload, &QAction::triggered, this, &MainWindow::enqueueDownload);
    connect(ui->actionStop, &QAction::triggered, this, [this]() {
        for (const auto &[queueJobId, backendJobId] : m_backendJobIds) {
            (void)queueJobId;
            const auto cancelResult = m_remoteSessionService.cancel(backendJobId);
            if (!cancelResult.succeeded) {
                appendLogMessage(tr("Transfer cancel failed: %1").arg(QString::fromStdString(cancelResult.error.message)));
            }
        }
        m_backendJobIds.clear();
        m_transferProgressTimer->stop();
        m_transferQueueService.clear();
        renderTransferQueue();
        appendLogMessage(tr("Transfer queue cleared"));
        ui->statusbar->showMessage(tr("転送キューをクリアしました"));
    });
    connect(ui->actionRefresh, &QAction::triggered, this, [this]() {
        loadLocalDirectory(m_localPath);
        if (m_connected) {
            loadRemoteDirectory(m_remotePath);
        } else {
            clearRemotePanel(m_remotePath);
        }
        appendLogMessage(tr("Panels refreshed"));
    });
    connect(ui->actionToggleQueue, &QAction::triggered, this, [this](bool checked) {
        ui->bottomTabWidget->setVisible(checked);
    });
    connect(ui->actionAbout, &QAction::triggered, this, [this]() {
        QMessageBox::about(
            this,
            tr("kdeftpclientについて"),
            tr("kdeftpclient は KDE Plasma 向けのQt Widgets FTPクライアントです。\n"
               "現在はWinSCP Commander風のUIシェルと転送キューの基礎を実装しています。"));
    });

    connect(ui->localPathLineEdit, &QLineEdit::returnPressed, this, [this]() {
        loadLocalDirectory(ui->localPathLineEdit->text());
    });
    connect(ui->remotePathLineEdit, &QLineEdit::returnPressed, this, [this]() {
        loadRemoteDirectory(ui->remotePathLineEdit->text());
    });
    connect(ui->localFileTreeWidget, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *item) {
        if (item != nullptr && item->data(0, Qt::UserRole + 1).toBool()) {
            loadLocalDirectory(item->data(0, Qt::UserRole).toString());
        }
    });
    connect(ui->remoteFileTreeWidget, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *item) {
        if (item != nullptr && item->data(0, Qt::UserRole + 1).toBool()) {
            loadRemoteDirectory(item->data(0, Qt::UserRole).toString());
        }
    });
}

void MainWindow::loadLocalDirectory(const QString &path)
{
    QDir directory(path);
    if (!directory.exists()) {
        QMessageBox::warning(this, tr("ローカルパス"), tr("指定されたフォルダーを開けません: %1").arg(path));
        ui->localPathLineEdit->setText(m_localPath);
        return;
    }

    m_localPath = directory.absolutePath();
    ui->localPathLineEdit->setText(m_localPath);
    ui->localFileTreeWidget->clear();

    if (!directory.isRoot()) {
        auto *parentItem = new QTreeWidgetItem({QStringLiteral(".."), QStringLiteral("<DIR>"), QString()});
        parentItem->setData(0, Qt::UserRole, directory.absoluteFilePath(QStringLiteral("..")));
        parentItem->setData(0, Qt::UserRole + 1, true);
        parentItem->setData(0, Qt::UserRole + 2, 0);
        ui->localFileTreeWidget->addTopLevelItem(parentItem);
    }

    const auto entries = directory.entryInfoList(
        QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Readable,
        QDir::DirsFirst | QDir::Name | QDir::IgnoreCase);
    for (const QFileInfo &entry : entries) {
        auto *item = new QTreeWidgetItem({
            entry.fileName(),
            entry.isDir() ? QStringLiteral("<DIR>") : formattedSize(entry.size()),
            entry.lastModified().toString(QStringLiteral("yyyy-MM-dd HH:mm")),
        });
        item->setData(0, Qt::UserRole, entry.absoluteFilePath());
        item->setData(0, Qt::UserRole + 1, entry.isDir());
        item->setData(0, Qt::UserRole + 2, entry.isFile() ? entry.size() : 0);
        ui->localFileTreeWidget->addTopLevelItem(item);
    }
}

void MainWindow::clearRemotePanel(const QString &path)
{
    m_remotePath = normalizedRemotePathForUi(path);
    ui->remotePathLineEdit->setText(m_remotePath);
    ui->remoteFileTreeWidget->clear();
    ui->remotePaneTitleLabel->setText(tr("リモート - 未接続"));
}

void MainWindow::loadRemoteDirectory(const QString &path)
{
    if (!m_connected) {
        clearRemotePanel(path);
        ui->statusbar->showMessage(tr("リモートに接続していません"));
        return;
    }

    const QString normalizedPath = normalizedRemotePathForUi(path, m_remotePath);

    const auto result = m_remoteSessionService.listDirectory(normalizedPath.toStdString());
    if (!result.operation.succeeded) {
        QMessageBox::warning(
            this,
            tr("リモート一覧"),
            tr("リモートディレクトリを取得できませんでした: %1")
                .arg(QString::fromStdString(result.operation.error.message)));
        return;
    }

    renderRemoteEntries(normalizedPath, result.entries);
}

void MainWindow::renderRemoteEntries(const QString &path, const std::vector<domain::RemoteEntry> &entries)
{
    m_remotePath = path;
    ui->remotePathLineEdit->setText(m_remotePath);
    ui->remoteFileTreeWidget->clear();

    if (const auto parentPath = parentRemotePath(m_remotePath); parentPath.has_value()) {
        auto *parentItem = new QTreeWidgetItem({QStringLiteral(".."), QStringLiteral("<DIR>"), QString()});
        parentItem->setData(0, Qt::UserRole, *parentPath);
        parentItem->setData(0, Qt::UserRole + 1, true);
        parentItem->setData(0, Qt::UserRole + 2, 0);
        ui->remoteFileTreeWidget->addTopLevelItem(parentItem);
    }

    for (const auto &entry : entries) {
        auto *item = new QTreeWidgetItem({
            QString::fromStdString(entry.name),
            entry.isDirectory ? QStringLiteral("<DIR>") : formattedSize(static_cast<qint64>(entry.size)),
            QStringLiteral("-"),
        });
        item->setData(0, Qt::UserRole, QString::fromStdString(entry.path));
        item->setData(0, Qt::UserRole + 1, entry.isDirectory);
        item->setData(0, Qt::UserRole + 2, QVariant::fromValue(static_cast<qulonglong>(entry.size)));
        ui->remoteFileTreeWidget->addTopLevelItem(item);
    }
}

void MainWindow::openSiteManager()
{
    SiteManagerDialog dialog(this);
    refreshSiteManagerDialog(dialog);

    connect(&dialog, &SiteManagerDialog::createSiteRequested, this, [&dialog, this]() {
        if (const auto savedName = editSiteProfile(std::nullopt, &dialog); savedName.has_value()) {
            refreshSiteManagerDialog(dialog);
            dialog.selectSiteByName(*savedName);
        }
    });
    connect(&dialog, &SiteManagerDialog::editSiteRequested, this, [&dialog, this](const QString &connectionName) {
        if (const auto savedName = editSiteProfile(connectionName, &dialog); savedName.has_value()) {
            refreshSiteManagerDialog(dialog);
            dialog.selectSiteByName(*savedName);
        }
    });
    connect(&dialog, &SiteManagerDialog::removeSiteRequested, this, [&dialog, this](const QString &connectionName) {
        const auto reply = QMessageBox::question(
            this,
            tr("接続先の削除"),
            tr("接続先「%1」を削除しますか？").arg(connectionName));
        if (reply != QMessageBox::Yes) {
            return;
        }

        const auto result = m_siteProfileService.removeProfile(connectionName.toStdString());
        if (!result.succeeded) {
            QMessageBox::warning(this, tr("接続先の削除"), tr("接続先を削除できませんでした。"));
            return;
        }
        const auto removeCredentialResult = m_credentialService.removePassword(connectionName.toStdString());
        if (!removeCredentialResult.succeeded) {
            appendLogMessage(tr("Saved credential was not removed for site: %1").arg(connectionName));
        }

        appendLogMessage(tr("Site profile removed: %1").arg(connectionName));
        refreshSiteManagerDialog(dialog);
    });

    if (dialog.exec() == QDialog::Accepted) {
        const auto siteName = dialog.selectedSiteName();
        if (siteName.has_value()) {
            connectToSite(*siteName);
        }
    }
}

void MainWindow::connectToSelectedSite()
{
    const auto profilesResult = m_siteProfileService.listProfiles();
    if (!profilesResult.operation.succeeded) {
        QMessageBox::warning(
            this,
            tr("接続"),
            tr("接続先一覧を読み込めませんでした。"));
        return;
    }

    SiteManagerDialog dialog(this);
    dialog.setSites(profilesResult.siteProfiles);
    connect(&dialog, &SiteManagerDialog::createSiteRequested, this, [&dialog, this]() {
        if (const auto savedName = editSiteProfile(std::nullopt, &dialog); savedName.has_value()) {
            refreshSiteManagerDialog(dialog);
            dialog.selectSiteByName(*savedName);
        }
    });
    connect(&dialog, &SiteManagerDialog::editSiteRequested, this, [&dialog, this](const QString &connectionName) {
        if (const auto savedName = editSiteProfile(connectionName, &dialog); savedName.has_value()) {
            refreshSiteManagerDialog(dialog);
            dialog.selectSiteByName(*savedName);
        }
    });
    connect(&dialog, &SiteManagerDialog::removeSiteRequested, this, [&dialog, this](const QString &connectionName) {
        const auto reply = QMessageBox::question(
            this,
            tr("接続先の削除"),
            tr("接続先「%1」を削除しますか？").arg(connectionName));
        if (reply != QMessageBox::Yes) {
            return;
        }

        const auto result = m_siteProfileService.removeProfile(connectionName.toStdString());
        if (!result.succeeded) {
            QMessageBox::warning(this, tr("接続先の削除"), tr("接続先を削除できませんでした。"));
            return;
        }
        const auto removeCredentialResult = m_credentialService.removePassword(connectionName.toStdString());
        if (!removeCredentialResult.succeeded) {
            appendLogMessage(tr("Saved credential was not removed for site: %1").arg(connectionName));
        }
        refreshSiteManagerDialog(dialog);
    });
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const auto siteName = dialog.selectedSiteName();
    if (!siteName.has_value()) {
        return;
    }

    connectToSite(*siteName);
}

void MainWindow::connectToSite(const QString &connectionName)
{
    const auto selectedSite = m_siteProfileService.findProfileByName(connectionName.toStdString());
    if (!selectedSite.operation.succeeded || !selectedSite.found) {
        QMessageBox::warning(
            this,
            tr("接続"),
            tr("選択した接続先が見つかりませんでした。"));
        return;
    }

    const auto password = promptPasswordForSite(selectedSite.siteProfile);
    if (!password.has_value()) {
        return;
    }

    ui->statusbar->showMessage(tr("接続中: %1").arg(QString::fromStdString(selectedSite.siteProfile.connectionName)));
    appendLogMessage(tr("Connecting to site: %1").arg(QString::fromStdString(selectedSite.siteProfile.connectionName)));

    const auto connectionResult = m_remoteSessionService.connect({
        .siteProfile = selectedSite.siteProfile,
        .password = password->toStdString(),
    });
    if (!connectionResult.operation.succeeded) {
        QMessageBox::warning(
            this,
            tr("接続"),
            tr("接続できませんでした: %1").arg(QString::fromStdString(connectionResult.operation.error.message)));
        return;
    }

    m_connected = true;
    m_currentProtocol = selectedSite.siteProfile.protocol;
    m_remotePath = defaultRemotePathForSite(selectedSite.siteProfile);
    ui->remotePaneTitleLabel->setText(
        tr("リモート - %1 (%2)")
            .arg(QString::fromStdString(selectedSite.siteProfile.connectionName), protocolLabel(selectedSite.siteProfile.protocol)));
    loadRemoteDirectory(m_remotePath);
    startPendingTransfers();
    appendLogMessage(
        tr("Connected to %1 via %2 using %3")
            .arg(
                QString::fromStdString(selectedSite.siteProfile.host),
                protocolLabel(selectedSite.siteProfile.protocol),
                authenticationLabel(selectedSite.siteProfile)));
    renderTransferQueue();
    ui->statusbar->showMessage(tr("接続しました: %1").arg(QString::fromStdString(selectedSite.siteProfile.connectionName)));
}

std::optional<QString> MainWindow::promptPasswordForSite(const domain::SiteProfile &siteProfile)
{
    if (siteProfile.allowAnonymousLogin) {
        return QString();
    }

    if (siteProfile.authenticationMethod == domain::AuthenticationMethod::PrivateKey) {
        bool accepted = false;
        const auto passphrase = QInputDialog::getText(
            this,
            tr("秘密鍵パスフレーズ"),
            tr("秘密鍵にパスフレーズが設定されている場合は入力してください。空欄でも続行できます。"),
            QLineEdit::Password,
            QString(),
            &accepted);
        if (!accepted) {
            return std::nullopt;
        }

        return passphrase;
    }

    if (m_credentialService.hasPassword(siteProfile.connectionName)) {
        const auto masterPassword = promptMasterPassword(
            tr("保存済みパスワード"),
            tr("保存済みパスワードを復号するマスターパスワード"));
        if (!masterPassword.has_value()) {
            return std::nullopt;
        }

        const auto readResult = m_credentialService.readPassword(
            siteProfile.connectionName,
            masterPassword->toStdString());
        if (!readResult.operation.succeeded) {
            QMessageBox::warning(
                this,
                tr("パスワード"),
                tr("保存済みパスワードを復号できませんでした。マスターパスワードを確認してください。"));
            return std::nullopt;
        }
        if (readResult.found) {
            return QString::fromStdString(readResult.password);
        }
    }

    bool accepted = false;
    const auto password = QInputDialog::getText(
        this,
        tr("パスワード入力"),
        tr("%1@%2 のパスワード")
            .arg(QString::fromStdString(siteProfile.userName), QString::fromStdString(siteProfile.host)),
        QLineEdit::Password,
        QString(),
        &accepted);
    if (!accepted) {
        return std::nullopt;
    }

    return password;
}

std::optional<QString> MainWindow::promptMasterPassword(const QString &title, const QString &label)
{
    bool accepted = false;
    const auto masterPassword = QInputDialog::getText(
        this,
        title,
        label,
        QLineEdit::Password,
        QString(),
        &accepted);
    if (!accepted) {
        return std::nullopt;
    }

    if (masterPassword.isEmpty()) {
        QMessageBox::warning(this, title, tr("マスターパスワードを入力してください。"));
        return std::nullopt;
    }

    return masterPassword;
}

bool MainWindow::savePasswordIfRequested(const domain::SiteProfile &siteProfile, const QString &password)
{
    if (password.isEmpty()) {
        return true;
    }

    const auto masterPassword = promptMasterPassword(
        tr("パスワード保存"),
        tr("保存するパスワードを暗号化するためのマスターパスワード"));
    if (!masterPassword.has_value()) {
        return false;
    }

    const auto result = m_credentialService.savePassword(
        siteProfile.connectionName,
        password.toStdString(),
        masterPassword->toStdString());
    if (!result.succeeded) {
        QMessageBox::warning(this, tr("パスワード保存"), tr("パスワードを保存できませんでした。"));
        return false;
    }

    appendLogMessage(tr("Encrypted password saved for site: %1").arg(QString::fromStdString(siteProfile.connectionName)));
    return true;
}

void MainWindow::enqueueUpload()
{
    const QString source = selectedLocalPath();
    if (source.isEmpty()) {
        ui->statusbar->showMessage(tr("アップロードするローカル項目を選択してください"));
        return;
    }
    if (selectedLocalIsDirectory()) {
        QMessageBox::information(this, tr("アップロード"), tr("現在はフォルダーのアップロードには対応していません。ファイルを選択してください。"));
        return;
    }

    const QFileInfo sourceInfo(source);
    if (!sourceInfo.isFile()) {
        QMessageBox::warning(this, tr("アップロード"), tr("選択したローカル項目は転送できるファイルではありません。"));
        return;
    }
    if (m_connected && remoteChildExists(sourceInfo.fileName())) {
        const auto reply = QMessageBox::question(
            this,
            tr("上書き確認"),
            tr("リモートに同名項目があります。\n%1\n上書きしますか？").arg(sourceInfo.fileName()));
        if (reply != QMessageBox::Yes) {
            ui->statusbar->showMessage(tr("アップロードをキャンセルしました"));
            return;
        }
    }

    const auto job = m_transferQueueService.enqueueUpload(
        source.toStdString(),
        m_remotePath.toStdString(),
        static_cast<std::uint64_t>(sourceInfo.size()),
        m_currentProtocol);
    startQueuedTransfer(job.id);
    renderTransferQueue();
    appendLogMessage(
        tr("Queued upload #%1: %2 -> %3")
            .arg(job.id)
            .arg(QString::fromStdString(job.request.localPath), QString::fromStdString(job.request.remotePath)));
    ui->bottomTabWidget->setCurrentWidget(ui->transferQueueTab);
    ui->statusbar->showMessage(tr("アップロードをキューに追加しました"));
}

void MainWindow::enqueueDownload()
{
    const QString source = selectedRemotePath();
    if (source.isEmpty()) {
        ui->statusbar->showMessage(tr("ダウンロードするリモート項目を選択してください"));
        return;
    }
    if (selectedRemoteIsDirectory()) {
        QMessageBox::information(this, tr("ダウンロード"), tr("現在はフォルダーのダウンロードには対応していません。ファイルを選択してください。"));
        return;
    }

    const QString destination = QDir(m_localPath).filePath(remoteFileNameFromPath(source));
    if (QFileInfo::exists(destination)) {
        const auto reply = QMessageBox::question(
            this,
            tr("上書き確認"),
            tr("ローカルに同名ファイルがあります。\n%1\n上書きしますか？").arg(destination));
        if (reply != QMessageBox::Yes) {
            ui->statusbar->showMessage(tr("ダウンロードをキャンセルしました"));
            return;
        }
    }

    const auto job = m_transferQueueService.enqueueDownload(
        source.toStdString(),
        m_localPath.toStdString(),
        selectedRemoteSize(),
        m_currentProtocol);
    startQueuedTransfer(job.id);
    renderTransferQueue();
    appendLogMessage(
        tr("Queued download #%1: %2 -> %3")
            .arg(job.id)
            .arg(QString::fromStdString(job.request.remotePath), QString::fromStdString(job.request.localPath)));
    ui->bottomTabWidget->setCurrentWidget(ui->transferQueueTab);
    ui->statusbar->showMessage(tr("ダウンロードをキューに追加しました"));
}

void MainWindow::startQueuedTransfer(domain::TransferJobId jobId)
{
    if (!m_connected) {
        return;
    }

    const auto *job = m_transferQueueService.findJob(jobId);
    if (job == nullptr) {
        return;
    }

    const auto startResult = job->request.direction == domain::TransferDirection::Upload
        ? m_remoteSessionService.upload(job->request)
        : m_remoteSessionService.download(job->request);
    if (!startResult.operation.succeeded) {
        m_transferQueueService.updateState(jobId, domain::TransferState::Failed);
        appendLogMessage(
            tr("Transfer #%1 failed to start: %2")
                .arg(jobId)
                .arg(QString::fromStdString(startResult.operation.error.message)));
        return;
    }

    m_transferQueueService.updateState(jobId, domain::TransferState::Running);
    m_backendJobIds[jobId] = startResult.jobId;
    if (!m_transferProgressTimer->isActive()) {
        m_transferProgressTimer->start();
    }
    appendLogMessage(
        tr("Transfer #%1 started as backend job #%2")
            .arg(jobId)
            .arg(startResult.jobId));
}

void MainWindow::startPendingTransfers()
{
    if (!m_connected) {
        return;
    }

    for (const auto &job : m_transferQueueService.jobs()) {
        if (job.progress.state == domain::TransferState::Pending && !m_backendJobIds.contains(job.id)) {
            startQueuedTransfer(job.id);
        }
    }

    renderTransferQueue();
}

void MainWindow::pollTransferProgress()
{
    bool updated = false;
    bool refreshLocalPanel = false;
    bool refreshRemotePanel = false;
    for (auto job = m_backendJobIds.begin(); job != m_backendJobIds.end();) {
        const auto queueJobId = job->first;
        const auto backendJobId = job->second;
        const auto progressResult = m_remoteSessionService.progress(backendJobId);
        if (!progressResult.operation.succeeded || !progressResult.found) {
            ++job;
            continue;
        }

        m_transferQueueService.updateProgress(
            queueJobId,
            progressResult.progress.transferredBytes,
            progressResult.progress.totalBytes,
            progressResult.progress.state);
        updated = true;

        if (isTerminalTransferState(progressResult.progress.state)) {
            const auto *queueJob = m_transferQueueService.findJob(queueJobId);
            if (queueJob != nullptr) {
                if (progressResult.progress.state == domain::TransferState::Completed) {
                    appendLogMessage(tr("Transfer #%1 completed").arg(queueJobId));
                    refreshLocalPanel = refreshLocalPanel
                        || queueJob->request.direction == domain::TransferDirection::Download;
                    refreshRemotePanel = refreshRemotePanel
                        || queueJob->request.direction == domain::TransferDirection::Upload;
                } else if (progressResult.progress.state == domain::TransferState::Failed) {
                    appendLogMessage(tr("Transfer #%1 failed").arg(queueJobId));
                } else if (progressResult.progress.state == domain::TransferState::Cancelled) {
                    appendLogMessage(tr("Transfer #%1 cancelled").arg(queueJobId));
                }
            }
            job = m_backendJobIds.erase(job);
        } else {
            ++job;
        }
    }

    if (refreshLocalPanel) {
        loadLocalDirectory(m_localPath);
    }
    if (refreshRemotePanel && m_connected) {
        loadRemoteDirectory(m_remotePath);
    }
    if (updated) {
        renderTransferQueue();
    }
    if (m_backendJobIds.empty()) {
        m_transferProgressTimer->stop();
    }
}

void MainWindow::renderTransferQueue()
{
    QList<TransferQueueWidget::QueueItem> items;
    for (const auto &job : m_transferQueueService.jobs()) {
        items.append(queueItemFromJob(job, m_connected));
    }

    m_transferQueueWidget->setItems(items);
}

std::optional<QString> MainWindow::editSiteProfile(const std::optional<QString> &connectionName, QWidget *dialogParent)
{
    SiteProfileDialog dialog(dialogParent != nullptr ? dialogParent : this);
    dialog.setWindowTitle(connectionName.has_value() ? tr("接続先の編集") : tr("新規接続先"));

    if (connectionName.has_value()) {
        const auto existingProfile = m_siteProfileService.findProfileByName(connectionName->toStdString());
        if (!existingProfile.operation.succeeded || !existingProfile.found) {
            QMessageBox::warning(this, tr("接続先"), tr("選択した接続先が見つかりませんでした。"));
            return std::nullopt;
        }

        dialog.setSiteProfile(existingProfile.siteProfile);
    }

    if (dialog.exec() != QDialog::Accepted) {
        return std::nullopt;
    }

    const auto profile = dialog.siteProfile();
    const auto saveResult = m_siteProfileService.saveProfile(profile);
    if (!saveResult.succeeded) {
        QMessageBox::warning(this, tr("接続先"), tr("接続先を保存できませんでした。"));
        return std::nullopt;
    }
    if (dialog.shouldSavePassword() && !savePasswordIfRequested(profile, dialog.passwordForSaving())) {
        return std::nullopt;
    }
    if (connectionName.has_value() && *connectionName != QString::fromStdString(profile.connectionName)) {
        const auto removeOldResult = m_siteProfileService.removeProfile(connectionName->toStdString());
        if (!removeOldResult.succeeded) {
            appendLogMessage(tr("Old site profile name was not removed: %1").arg(*connectionName));
        }
        const auto removeCredentialResult = m_credentialService.removePassword(connectionName->toStdString());
        if (!removeCredentialResult.succeeded) {
            appendLogMessage(tr("Saved credential was not removed for old site name: %1").arg(*connectionName));
        }
    }

    appendLogMessage(tr("Site profile saved: %1").arg(QString::fromStdString(profile.connectionName)));
    return QString::fromStdString(profile.connectionName);
}

void MainWindow::refreshSiteManagerDialog(SiteManagerDialog &dialog)
{
    const auto result = m_siteProfileService.listProfiles();
    if (!result.operation.succeeded) {
        QMessageBox::warning(
            this,
            tr("接続先管理"),
            tr("接続先一覧を読み込めませんでした。"));
        return;
    }

    dialog.setSites(result.siteProfiles);
}

void MainWindow::appendLogMessage(const QString &message)
{
    ui->logPlainTextEdit->appendPlainText(
        QStringLiteral("[%1] %2").arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss")), message));
}

QString MainWindow::selectedLocalPath() const
{
    const auto selectedItems = ui->localFileTreeWidget->selectedItems();
    if (selectedItems.isEmpty()) {
        return {};
    }

    return selectedItems.first()->data(0, Qt::UserRole).toString();
}

bool MainWindow::selectedLocalIsDirectory() const
{
    const auto selectedItems = ui->localFileTreeWidget->selectedItems();
    if (selectedItems.isEmpty()) {
        return false;
    }

    return selectedItems.first()->data(0, Qt::UserRole + 1).toBool();
}

QString MainWindow::selectedRemotePath() const
{
    const auto selectedItems = ui->remoteFileTreeWidget->selectedItems();
    if (selectedItems.isEmpty()) {
        return {};
    }

    return selectedItems.first()->data(0, Qt::UserRole).toString();
}

bool MainWindow::selectedRemoteIsDirectory() const
{
    const auto selectedItems = ui->remoteFileTreeWidget->selectedItems();
    if (selectedItems.isEmpty()) {
        return false;
    }

    return selectedItems.first()->data(0, Qt::UserRole + 1).toBool();
}

std::uint64_t MainWindow::selectedRemoteSize() const
{
    const auto selectedItems = ui->remoteFileTreeWidget->selectedItems();
    if (selectedItems.isEmpty()) {
        return 0;
    }

    return selectedItems.first()->data(0, Qt::UserRole + 2).toULongLong();
}

bool MainWindow::remoteChildExists(const QString &fileName) const
{
    if (fileName.isEmpty()) {
        return false;
    }

    for (int row = 0; row < ui->remoteFileTreeWidget->topLevelItemCount(); ++row) {
        const auto *item = ui->remoteFileTreeWidget->topLevelItem(row);
        if (item != nullptr && item->text(0) == fileName) {
            return true;
        }
    }

    return false;
}

std::vector<domain::SiteProfile> MainWindow::sampleSites() const
{
    std::vector<domain::SiteProfile> sites;

    domain::SiteProfile ftpSite;
    ftpSite.connectionName = "Example FTP";
    ftpSite.host = "ftp.example.com";
    ftpSite.port = 21;
    ftpSite.userName = "demo";
    ftpSite.protocol = domain::Protocol::Ftp;
    sites.push_back(ftpSite);

    domain::SiteProfile ftpsSite;
    ftpsSite.connectionName = "Example FTPS";
    ftpsSite.host = "ftps.example.net";
    ftpsSite.port = 990;
    ftpsSite.userName = "deploy";
    ftpsSite.protocol = domain::Protocol::Ftps;
    sites.push_back(ftpsSite);

    domain::SiteProfile sftpKeySite;
    sftpKeySite.connectionName = "Example SFTP Key";
    sftpKeySite.host = "sftp.example.net";
    sftpKeySite.port = 22;
    sftpKeySite.userName = "deploy";
    sftpKeySite.protocol = domain::Protocol::Sftp;
    sftpKeySite.authenticationMethod = domain::AuthenticationMethod::PrivateKey;
    sftpKeySite.privateKeyPath = (QDir::homePath() + QStringLiteral("/.ssh/id_ed25519")).toStdString();
    sites.push_back(sftpKeySite);

    return sites;
}
