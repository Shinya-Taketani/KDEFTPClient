#include "ui/widgets/TransferQueueWidget.h"

#include <QHeaderView>
#include <QLabel>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

TransferQueueWidget::TransferQueueWidget(QWidget *parent)
    : QWidget(parent)
    , m_descriptionLabel(nullptr)
    , m_transferTable(nullptr)
{
    setupUi();
}

void TransferQueueWidget::setPlaceholderItems(const QList<PlaceholderItem> &items)
{
    updateTable(items);
}

void TransferQueueWidget::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    m_descriptionLabel = new QLabel(tr("転送キューのダミー表示です。"), this);
    layout->addWidget(m_descriptionLabel);

    m_transferTable = new QTableWidget(0, 4, this);
    m_transferTable->setHorizontalHeaderLabels(
        {tr("方向"), tr("送信元"), tr("送信先"), tr("状態")});
    m_transferTable->horizontalHeader()->setStretchLastSection(true);
    m_transferTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_transferTable->verticalHeader()->setVisible(false);
    m_transferTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_transferTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    layout->addWidget(m_transferTable);

    setPlaceholderItems({
        {tr("アップロード"), QStringLiteral("README.txt"), QStringLiteral("/remote/home/README.txt"), tr("待機中")},
    });
}

void TransferQueueWidget::updateTable(const QList<PlaceholderItem> &items)
{
    m_transferTable->setRowCount(items.size());

    for (int row = 0; row < items.size(); ++row) {
        const auto &item = items.at(row);
        m_transferTable->setItem(row, 0, new QTableWidgetItem(item.direction));
        m_transferTable->setItem(row, 1, new QTableWidgetItem(item.source));
        m_transferTable->setItem(row, 2, new QTableWidgetItem(item.destination));
        m_transferTable->setItem(row, 3, new QTableWidgetItem(item.status));
    }

    if (!items.isEmpty()) {
        m_transferTable->selectRow(0);
    }
}
