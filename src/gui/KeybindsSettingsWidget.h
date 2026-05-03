/*
 * MidiEditor - Keybindings Settings
 *
 * Provides a settings tab to view and edit all action shortcuts.
 */
#ifndef KEYBINDSSETTINGSWIDGET_H_
#define KEYBINDSSETTINGSWIDGET_H_

#include "SettingsWidget.h"

#include <QMap>
#include <QList>
#include <QKeySequence>
#include <QKeySequenceEdit>
#include <QPushButton>

class QTableWidget;
class SettingsDialog;
class QLabel;
class QVBoxLayout;
class QComboBox;
class QTimer;

class PianoKeyButton : public QPushButton {
    Q_OBJECT
public:
    PianoKeyButton(int offset, bool isBlack, QWidget* parent = nullptr);
    void setKey(int key);
    int noteOffset;
    int currentKey;
    bool isBlackKey;
    
    void flashError();

signals:
    void keyChanged(PianoKeyButton *btn, int offset, int key);

protected:
    void keyPressEvent(QKeyEvent *event) override;
};

// Simple button that captures a single key press (supports Windows VK codes for L/R Shift)
class CaptureKeyButton : public QPushButton {
    Q_OBJECT
public:
    explicit CaptureKeyButton(QWidget* parent = nullptr);
    void setKey(int key);
    int currentKey;
    
    void flashError();

signals:
    void keyChanged(CaptureKeyButton* btn, int key);

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    QTimer *_captureTimer;
    QTimer *_errorTimer;
};

// Custom KeySequenceEdit that captures actual key combinations
// instead of interpreting symbols (e.g., Shift+4 as "$")
class CustomKeySequenceEdit : public QKeySequenceEdit {
    Q_OBJECT
public:
    explicit CustomKeySequenceEdit(QWidget *parent = nullptr);

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    int _keyPressCount;
};

class KeybindsSettingsWidget : public SettingsWidget {
    Q_OBJECT
public:
    // dialog provides access to MainWindow and QSettings
    explicit KeybindsSettingsWidget(SettingsDialog *dialog, QWidget *parent = nullptr);

    bool accept() override;
    QIcon icon() override { return QIcon(); }

private slots:
    void resetRowToDefault(int row);
    void resetAllToDefaults();
    void checkDuplicates();
    void onPianoKeyChanged(PianoKeyButton *btn, int offset, int key);

private:
    void buildUI();
    void buildPianoUI(QVBoxLayout *layout);
    void loadActions();
    QStringList getActionOrder() const;

    SettingsDialog *_dialog;
    QTableWidget   *_table;
    // Map table row -> action id
    QMap<int, QString> _rowToActionId;
    // Default shortcuts cache (id -> sequences)
    QMap<QString, QList<QKeySequence>> _defaults;
    
    // Custom piano mapping: offset -> Qt::Key
    QMap<int, int> _pianoKeyMap;
    QLabel *_pianoStatusLabel;
    QTimer *_statusTimer;
    
    CaptureKeyButton *_octaveUpButton;
    CaptureKeyButton *_octaveDownButton;
};

#endif // KEYBINDSSETTINGSWIDGET_H_
