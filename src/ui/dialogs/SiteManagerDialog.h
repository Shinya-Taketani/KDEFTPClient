#ifndef SITEMANAGERDIALOG_H
#define SITEMANAGERDIALOG_H

#include "domain/SiteProfile.h"

#include <QDialog>

#include <vector>

class QLabel;
class QTableWidget;

class SiteManagerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SiteManagerDialog(QWidget *parent = nullptr);

    void setSites(const std::vector<domain::SiteProfile> &sites);

private:
    void setupUi();
    void updateEmptyState(bool isEmpty);
    void updateTable(const std::vector<domain::SiteProfile> &siteProfiles);

    QLabel *m_descriptionLabel;
    QTableWidget *m_siteTable;
};

#endif // SITEMANAGERDIALOG_H
