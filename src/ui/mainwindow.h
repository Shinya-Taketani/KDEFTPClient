#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QString>

#include <optional>
#include <vector>

#include "app/SiteProfileService.h"
#include "app/TransferQueueService.h"
#include "domain/SiteProfile.h"
#include "infra/settings/JsonSiteProfileRepository.h"

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
    void loadRemotePlaceholder(const QString &path);
    void openSiteManager();
    void connectToSelectedSite();
    void enqueueUpload();
    void enqueueDownload();
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
    app::TransferQueueService m_transferQueueService;
    domain::Protocol m_currentProtocol;
    bool m_connected;
};
#endif // MAINWINDOW_H
