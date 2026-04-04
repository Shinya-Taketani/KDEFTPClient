#ifndef TRANSFERQUEUEWIDGET_H
#define TRANSFERQUEUEWIDGET_H

#include <QWidget>

class QTableWidget;

class TransferQueueWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TransferQueueWidget(QWidget *parent = nullptr);

private:
    void setupUi();

    QTableWidget *m_transferTable;
};

#endif // TRANSFERQUEUEWIDGET_H
