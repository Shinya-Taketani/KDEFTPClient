#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QString>

#include <optional>
#include <vector>

#include "app/RemoteSessionService.h"
#include "app/SiteProfileService.h"
#include "app/TransferQueueService.h"
#include "domain/RemoteEntry.h"
#include "domain/SiteProfile.h"
#include "infra/settings/JsonSiteProfileRepository.h"
#include "infra/transfer/CurlTransferEngine.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class TransferQueueWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    void setupInitialState();
    void setupActions();
    void loadLocalDirectory(const QString &path);
    void clearRemotePanel(const QString &path);
    void loadRemoteDirectory(const QString &path);
    void renderRemoteEntries(const QString &path, const std::vector<domain::RemoteEntry> &entries);
    void openSiteManager();
    void connectToSelectedSite();
    [[nodiscard]] std::optional<QString> promptPasswordForSite(const domain::SiteProfile &siteProfile);
    void enqueueUpload();
    void enqueueDownload();
    void startQueuedTransfer(domain::TransferJobId jobId);
    void renderTransferQueue();
    bool editSiteProfile(const std::optional<QString> &connectionName);
    void refreshSiteManagerDialog(class SiteManagerDialog &dialog);
    void appendLogMessage(const QString &message);
    [[nodiscard]] QString selectedLocalPath() const;
    [[nodiscard]] QString selectedRemotePath() const;
    [[nodiscard]] std::vector<domain::SiteProfile> sampleSites() const;

    Ui::MainWindow *ui;
    TransferQueueWidget *m_transferQueueWidget;
    QString m_localPath;
    QString m_remotePath;
    infra::settings::JsonSiteProfileRepository m_siteProfileRepository;
    app::SiteProfileService m_siteProfileService;
    infra::transfer::CurlTransferEngine m_transferEngine;
    app::RemoteSessionService m_remoteSessionService;
    app::TransferQueueService m_transferQueueService;
    domain::Protocol m_currentProtocol;
    bool m_connected;
};
#endif // MAINWINDOW_H
