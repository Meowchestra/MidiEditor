#ifndef FFXIVFIXERDIALOG_H
#define FFXIVFIXERDIALOG_H

#include <QDialog>
#include <QJsonObject>

class QPushButton;
class QRadioButton;
class QCheckBox;
class QLabel;

/**
 * \brief Dialog to display FFXIV channel analysis and let the user select a Tier.
 */
class FFXIVFixerDialog : public QDialog {
    Q_OBJECT

public:
    explicit FFXIVFixerDialog(const QJsonObject &analysis, QWidget *parent = nullptr);
    int selectedTier() const;
    bool cleanupControlChanges() const;
    bool cleanupKeyPressure() const;
    bool cleanupChannelPressure() const;
    bool cleanupPitchBend() const;
    bool normalizeVelocity() const;

private:
    QJsonObject _analysis;
    QRadioButton *_rebuildRadio;
    QRadioButton *_preserveRadio;
    QCheckBox *_cleanupCCCheck;
    QCheckBox *_cleanupKeyPressureCheck;
    QCheckBox *_cleanupChannelPressureCheck;
    QCheckBox *_cleanupPitchBendCheck;
    QCheckBox *_normalizeVelocityCheck;
};

#endif // FFXIVFIXERDIALOG_H
