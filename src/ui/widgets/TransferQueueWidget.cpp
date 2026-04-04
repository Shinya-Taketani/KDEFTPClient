#include "ui/widgets/TransferQueueWidget.h"

#include <QHeaderView>
#include <QLabel>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

TransferQueueWidget::TransferQueueWidget(QWidget *parent)
    : QWidget(parent)
    , m_transferTable(nullptr)
{
    setupUi();
}

void TransferQueueWidget::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    auto *descriptionLabel = new QLabel(tr("転送キューのダミー表示です。"), this);
    layout->addWidget(descriptionLabel);

    m_transferTable = new QTableWidget(1, 4, this);
    m_transferTable->setHorizontalHeaderLabels(
        {tr("方向"), tr("送信元"), tr("送信先"), tr("状態")});
    m_transferTable->horizontalHeader()->setStretchLastSection(true);
    m_transferTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_transferTable->verticalHeader()->setVisible(false);
    m_transferTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_transferTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    m_transferTable->setItem(0, 0, new QTableWidgetItem(tr("アップロード")));
    m_transferTable->setItem(0, 1, new QTableWidgetItem(QStringLiteral("README.txt")));
    m_transferTable->setItem(0, 2, new QTableWidgetItem(QStringLiteral("/remote/home/README.txt")));
    m_transferTable->setItem(0, 3, new QTableWidgetItem(tr("待機中")));

    layout->addWidget(m_transferTable);
}
