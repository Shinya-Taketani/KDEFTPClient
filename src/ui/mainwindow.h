#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QString>

#include <vector>

#include "domain/SiteProfile.h"

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
    void appendLogMessage(const QString &message);
    [[nodiscard]] QString selectedLocalPath() const;
    [[nodiscard]] QString selectedRemotePath() const;
    [[nodiscard]] std::vector<domain::SiteProfile> sampleSites() const;

    Ui::MainWindow *ui;
    TransferQueueWidget *m_transferQueueWidget;
    QString m_localPath;
    QString m_remotePath;
    std::vector<domain::SiteProfile> m_sites;
    bool m_connected;
};
#endif // MAINWINDOW_H
