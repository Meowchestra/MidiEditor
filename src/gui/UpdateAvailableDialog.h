#ifndef UPDATEAVAILABLEDIALOG_H
#define UPDATEAVAILABLEDIALOG_H

#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>

class UpdateAvailableDialog : public QDialog
{
    Q_OBJECT
public:
    enum Result {
        Close = QDialog::Rejected,     // 0
        UpdateNow = QDialog::Accepted, // 1
        AfterExit = 2,
        Manual = 3
    };

    explicit UpdateAvailableDialog(const QString &latestVersion, const QString &currentVersion, const QString &changelogBody, QWidget *parent = nullptr);

private slots:
    void onUpdateNow();
    void onAfterExit();
    void onManual();
    void onClose();

private:
    void setupUi(const QString &latestVersion, const QString &currentVersion);
};

#endif // UPDATEAVAILABLEDIALOG_H
