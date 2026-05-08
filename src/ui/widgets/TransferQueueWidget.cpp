#include "ui/widgets/TransferQueueWidget.h"

#include <QAbstractItemView>
#include <QHeaderView>
#include <QLabel>
#include <QProgressBar>
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

void TransferQueueWidget::appendItem(const QueueItem &item)
{
    m_items.append(item);
    updateTable();
}

void TransferQueueWidget::clearItems()
{
    m_items.clear();
    updateTable();
}

void TransferQueueWidget::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    m_descriptionLabel = new QLabel(tr("現在、転送キューに項目はありません。"), this);
    layout->addWidget(m_descriptionLabel);

    m_transferTable = new QTableWidget(0, 5, this);
    m_transferTable->setHorizontalHeaderLabels(
        {tr("方向"), tr("送信元"), tr("送信先"), tr("状態"), tr("進捗")});
    m_transferTable->horizontalHeader()->setStretchLastSection(true);
    m_transferTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_transferTable->verticalHeader()->setVisible(false);
    m_transferTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_transferTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    layout->addWidget(m_transferTable);
    updateTable();
}

void TransferQueueWidget::setItems(const QList<QueueItem> &items)
{
    m_items = items;
    updateTable();
}

void TransferQueueWidget::updateTable()
{
    updateEmptyState(m_items.isEmpty());
    m_transferTable->setRowCount(m_items.size());

    for (int row = 0; row < m_items.size(); ++row) {
        const auto &item = m_items.at(row);
        m_transferTable->setItem(row, 0, new QTableWidgetItem(item.direction));
        m_transferTable->setItem(row, 1, new QTableWidgetItem(item.source));
        m_transferTable->setItem(row, 2, new QTableWidgetItem(item.destination));
        m_transferTable->setItem(row, 3, new QTableWidgetItem(item.status));

        auto *progressBar = new QProgressBar(m_transferTable);
        progressBar->setRange(0, 100);
        progressBar->setValue(item.progressPercent);
        progressBar->setTextVisible(true);
        m_transferTable->setCellWidget(row, 4, progressBar);
    }

    if (!m_items.isEmpty()) {
        m_transferTable->selectRow(0);
    } else {
        m_transferTable->clearSelection();
    }
}

void TransferQueueWidget::updateEmptyState(bool isEmpty)
{
    if (isEmpty) {
        m_descriptionLabel->setText(tr("現在、転送キューに項目はありません。"));
    } else {
        m_descriptionLabel->setText(tr("バックグラウンド転送キュー"));
    }

    m_transferTable->setVisible(!isEmpty);
}
