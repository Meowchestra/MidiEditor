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

class QTableWidget;
class QPushButton;
class SettingsDialog;

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

private:
    void buildUI();
    void loadActions();
    QStringList getActionOrder() const;

    SettingsDialog *_dialog;
    QTableWidget   *_table;
    // Map table row -> action id
    QMap<int, QString> _rowToActionId;
    // Default shortcuts cache (id -> sequences)
    QMap<QString, QList<QKeySequence>> _defaults;
};

#endif // KEYBINDSSETTINGSWIDGET_H_
