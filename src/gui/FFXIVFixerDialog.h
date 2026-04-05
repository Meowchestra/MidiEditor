#ifndef FFXIVFIXERDIALOG_H
#define FFXIVFIXERDIALOG_H

#include <QDialog>
#include <QJsonObject>

class QPushButton;
class QRadioButton;
class QLabel;

/**
 * \brief Dialog to display FFXIV channel analysis and let the user select a Tier.
 */
class FFXIVFixerDialog : public QDialog {
    Q_OBJECT

public:
    explicit FFXIVFixerDialog(const QJsonObject &analysis, QWidget *parent = nullptr);
    int selectedTier() const;

private:
    QJsonObject _analysis;
    QRadioButton *_rebuildRadio;
    QRadioButton *_preserveRadio;
};

#endif // FFXIVFIXERDIALOG_H
