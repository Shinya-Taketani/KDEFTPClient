#include "mainwindow.h"
#include "ui/dialogs/SiteManagerDialog.h"
#include "ui/widgets/TransferQueueWidget.h"
#include "ui_mainwindow.h"

#include <QAction>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_transferQueueWidget(nullptr)
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
    m_transferQueueWidget->setPlaceholderItems({
        {tr("アップロード"), QStringLiteral("README.txt"), QStringLiteral("/remote/home/README.txt"), tr("待機中")},
        {tr("ダウンロード"), QStringLiteral("/remote/releases/app.tar.gz"), QStringLiteral("/home/user/Downloads/app.tar.gz"), tr("転送準備完了")},
    });

    connect(ui->actionNewSite, &QAction::triggered, this, [this]() {
        SiteManagerDialog dialog(this);
        dialog.exec();
    });

    ui->statusbar->showMessage(tr("準備完了 - 現在はUI骨格のみです"));
}
