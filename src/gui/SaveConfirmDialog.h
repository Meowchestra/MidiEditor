#ifndef SAVECONFIRMDIALOG_H
#define SAVECONFIRMDIALOG_H

#include <QDialog>

/**
 * @brief A modern, themed dialog to confirm saving before closing.
 */
class SaveConfirmDialog : public QDialog {
    Q_OBJECT
public:
    enum Result {
        Cancel = QDialog::Rejected, // 0
        Save = QDialog::Accepted,   // 1
        Discard = 2
    };
    SaveConfirmDialog(const QString &fileName, const QString &fullPath, QWidget *parent = nullptr);
};

#endif // SAVECONFIRMDIALOG_H
