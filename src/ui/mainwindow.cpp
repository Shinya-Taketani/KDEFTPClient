#include "mainwindow.h"
#include "ui/widgets/TransferQueueWidget.h"
#include "ui_mainwindow.h"

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

    ui->statusbar->showMessage(tr("準備完了 - 現在はUI骨格のみです"));
}
