#ifndef SITEMANAGERDIALOG_H
#define SITEMANAGERDIALOG_H

#include "domain/SiteProfile.h"

#include <QDialog>
#include <QString>

#include <optional>
#include <vector>

class QLabel;
class QPushButton;
class QTableWidget;

class SiteManagerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SiteManagerDialog(QWidget *parent = nullptr);

    void setSites(const std::vector<domain::SiteProfile> &sites);
    [[nodiscard]] std::optional<QString> selectedSiteName() const;

private:
    void setupUi();
    void updateActionButtonState();
    void updateEmptyState(bool isEmpty);
    void updateTable(const std::vector<domain::SiteProfile> &siteProfiles);

    QLabel *m_descriptionLabel;
    QTableWidget *m_siteTable;
    QPushButton *m_editButton;
    QPushButton *m_removeButton;
    QPushButton *m_connectButton;
};

#endif // SITEMANAGERDIALOG_H
