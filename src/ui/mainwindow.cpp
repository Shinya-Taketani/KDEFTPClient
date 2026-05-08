#include "mainwindow.h"
#include "ui/dialogs/SiteManagerDialog.h"
#include "ui/widgets/TransferQueueWidget.h"
#include "ui_mainwindow.h"

#include <QAction>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QHeaderView>
#include <QKeySequence>
#include <QLineEdit>
#include <QMessageBox>
#include <QStyle>
#include <QTime>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include <algorithm>

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

QString remoteFileName(const QString &remotePath)
{
    const auto parts = remotePath.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    return parts.isEmpty() ? remotePath : parts.last();
}

}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_transferQueueWidget(nullptr)
    , m_localPath(QDir::homePath())
    , m_remotePath(QStringLiteral("/"))
    , m_sites(sampleSites())
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
    loadRemotePlaceholder(m_remotePath);
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
        m_connected = false;
        loadRemotePlaceholder(QStringLiteral("/"));
        appendLogMessage(tr("Disconnected"));
        ui->statusbar->showMessage(tr("切断しました"));
    });
    connect(ui->actionExit, &QAction::triggered, this, &QWidget::close);
    connect(ui->actionUpload, &QAction::triggered, this, &MainWindow::enqueueUpload);
    connect(ui->actionDownload, &QAction::triggered, this, &MainWindow::enqueueDownload);
    connect(ui->actionStop, &QAction::triggered, this, [this]() {
        m_transferQueueWidget->clearItems();
        appendLogMessage(tr("Transfer queue cleared"));
        ui->statusbar->showMessage(tr("転送キューをクリアしました"));
    });
    connect(ui->actionRefresh, &QAction::triggered, this, [this]() {
        loadLocalDirectory(m_localPath);
        loadRemotePlaceholder(m_remotePath);
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
        loadRemotePlaceholder(ui->remotePathLineEdit->text());
    });
    connect(ui->localFileTreeWidget, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *item) {
        if (item != nullptr && item->data(0, Qt::UserRole + 1).toBool()) {
            loadLocalDirectory(item->data(0, Qt::UserRole).toString());
        }
    });
    connect(ui->remoteFileTreeWidget, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *item) {
        if (item != nullptr && item->data(0, Qt::UserRole + 1).toBool()) {
            loadRemotePlaceholder(item->data(0, Qt::UserRole).toString());
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
        ui->localFileTreeWidget->addTopLevelItem(item);
    }
}

void MainWindow::loadRemotePlaceholder(const QString &path)
{
    QString normalizedPath = path.trimmed();
    if (normalizedPath.isEmpty()) {
        normalizedPath = QStringLiteral("/");
    }
    if (!normalizedPath.startsWith(QLatin1Char('/'))) {
        normalizedPath.prepend(QLatin1Char('/'));
    }

    m_remotePath = QDir::cleanPath(normalizedPath);
    ui->remotePathLineEdit->setText(m_remotePath);
    ui->remoteFileTreeWidget->clear();

    if (m_remotePath != QStringLiteral("/")) {
        auto *parentItem = new QTreeWidgetItem({QStringLiteral(".."), QStringLiteral("<DIR>"), QString()});
        parentItem->setData(0, Qt::UserRole, QFileInfo(m_remotePath).path());
        parentItem->setData(0, Qt::UserRole + 1, true);
        ui->remoteFileTreeWidget->addTopLevelItem(parentItem);
    }

    const QList<QTreeWidgetItem *> items {
        new QTreeWidgetItem({QStringLiteral("incoming"), QStringLiteral("<DIR>"), QStringLiteral("2026-05-09 09:10")}),
        new QTreeWidgetItem({QStringLiteral("releases"), QStringLiteral("<DIR>"), QStringLiteral("2026-05-09 09:11")}),
        new QTreeWidgetItem({QStringLiteral("README.txt"), QStringLiteral("12 KB"), QStringLiteral("2026-05-08 18:20")}),
        new QTreeWidgetItem({QStringLiteral("release.tar.gz"), QStringLiteral("48 MB"), QStringLiteral("2026-05-07 14:10")}),
    };

    for (QTreeWidgetItem *item : items) {
        const bool isDirectory = item->text(1) == QStringLiteral("<DIR>");
        const QString fullPath = QDir::cleanPath(m_remotePath + QLatin1Char('/') + item->text(0));
        item->setData(0, Qt::UserRole, fullPath);
        item->setData(0, Qt::UserRole + 1, isDirectory);
        ui->remoteFileTreeWidget->addTopLevelItem(item);
    }

    if (!m_connected) {
        ui->remotePaneTitleLabel->setText(tr("リモート - 未接続プレビュー"));
    }
}

void MainWindow::openSiteManager()
{
    SiteManagerDialog dialog(this);
    dialog.setSites(m_sites);
    dialog.exec();
}

void MainWindow::connectToSelectedSite()
{
    SiteManagerDialog dialog(this);
    dialog.setSites(m_sites);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const auto siteName = dialog.selectedSiteName();
    if (!siteName.has_value()) {
        return;
    }

    const auto selectedName = siteName->toStdString();
    auto selectedSite = std::find_if(m_sites.begin(), m_sites.end(), [&selectedName](const domain::SiteProfile &site) {
        return site.connectionName == selectedName;
    });
    if (selectedSite == m_sites.end()) {
        return;
    }

    m_connected = true;
    m_remotePath = QStringLiteral("/home/%1").arg(QString::fromStdString(selectedSite->userName));
    ui->remotePaneTitleLabel->setText(
        tr("リモート - %1 (%2)")
            .arg(QString::fromStdString(selectedSite->connectionName), protocolLabel(selectedSite->protocol)));
    loadRemotePlaceholder(m_remotePath);
    appendLogMessage(
        tr("Connected to %1 via %2")
            .arg(QString::fromStdString(selectedSite->host), protocolLabel(selectedSite->protocol)));
    ui->statusbar->showMessage(tr("接続しました: %1").arg(QString::fromStdString(selectedSite->connectionName)));
}

void MainWindow::enqueueUpload()
{
    const QString source = selectedLocalPath();
    if (source.isEmpty()) {
        ui->statusbar->showMessage(tr("アップロードするローカル項目を選択してください"));
        return;
    }

    const QFileInfo sourceInfo(source);
    const QString destination = QDir::cleanPath(m_remotePath + QLatin1Char('/') + sourceInfo.fileName());
    m_transferQueueWidget->appendItem({
        tr("アップロード"),
        source,
        destination,
        m_connected ? tr("待機中") : tr("未接続"),
        0,
    });
    appendLogMessage(tr("Queued upload: %1 -> %2").arg(source, destination));
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

    const QString destination = QDir::cleanPath(m_localPath + QLatin1Char('/') + remoteFileName(source));
    m_transferQueueWidget->appendItem({
        tr("ダウンロード"),
        source,
        destination,
        m_connected ? tr("待機中") : tr("未接続"),
        0,
    });
    appendLogMessage(tr("Queued download: %1 -> %2").arg(source, destination));
    ui->bottomTabWidget->setCurrentWidget(ui->transferQueueTab);
    ui->statusbar->showMessage(tr("ダウンロードをキューに追加しました"));
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

QString MainWindow::selectedRemotePath() const
{
    const auto selectedItems = ui->remoteFileTreeWidget->selectedItems();
    if (selectedItems.isEmpty()) {
        return {};
    }

    return selectedItems.first()->data(0, Qt::UserRole).toString();
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

    return sites;
}
