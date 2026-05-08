#ifndef TRANSFERQUEUEWIDGET_H
#define TRANSFERQUEUEWIDGET_H

#include <QWidget>

#include <QString>

class QLabel;
class QTableWidget;

class TransferQueueWidget : public QWidget
{
    Q_OBJECT

public:
    struct QueueItem {
        QString direction;
        QString source;
        QString destination;
        QString status;
        int progressPercent { 0 };
    };

    explicit TransferQueueWidget(QWidget *parent = nullptr);
    void setItems(const QList<QueueItem> &items);
    void appendItem(const QueueItem &item);
    void clearItems();

private:
    void setupUi();
    void updateEmptyState(bool isEmpty);
    void updateTable();

    QList<QueueItem> m_items;
    QLabel *m_descriptionLabel;
    QTableWidget *m_transferTable;
};

#endif // TRANSFERQUEUEWIDGET_H
