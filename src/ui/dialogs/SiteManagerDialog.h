#ifndef SITEMANAGERDIALOG_H
#define SITEMANAGERDIALOG_H

#include "domain/SiteProfile.h"

#include <QDialog>

#include <vector>

class QTableWidget;

class SiteManagerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SiteManagerDialog(QWidget *parent = nullptr);

    void setSiteProfiles(const std::vector<domain::SiteProfile> &siteProfiles);

private:
    void setupUi();
    void populatePlaceholderData();
    void updateTable(const std::vector<domain::SiteProfile> &siteProfiles);

    QTableWidget *m_siteTable;
};

#endif // SITEMANAGERDIALOG_H
