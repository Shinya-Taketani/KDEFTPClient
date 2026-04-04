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
    struct PlaceholderItem {
        QString direction;
        QString source;
        QString destination;
        QString status;
    };

    explicit TransferQueueWidget(QWidget *parent = nullptr);
    void setPlaceholderItems(const QList<PlaceholderItem> &items);

private:
    void setupUi();
    void updateEmptyState(bool isEmpty);
    void updateTable(const QList<PlaceholderItem> &items);

    QLabel *m_descriptionLabel;
    QTableWidget *m_transferTable;
};

#endif // TRANSFERQUEUEWIDGET_H
