/*
 * MidiEditor - Keybinds Settings implementation
 */
#include "KeybindsSettingsWidget.h"
#include "SettingsDialog.h"
#include "MainWindow.h"

#include <QTableWidget>
#include <QHeaderView>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QSettings>
#include <QAction>
#include <QKeyEvent>
#include <QMessageBox>

// CustomKeySequenceEdit implementation
CustomKeySequenceEdit::CustomKeySequenceEdit(QWidget *parent)
    : QKeySequenceEdit(parent), _keyPressCount(0) {
}

void CustomKeySequenceEdit::keyPressEvent(QKeyEvent *event) {
    // Ignore modifier-only key presses
    if (event->key() == Qt::Key_Control ||
        event->key() == Qt::Key_Shift ||
        event->key() == Qt::Key_Alt ||
        event->key() == Qt::Key_Meta) {
        event->ignore();
        return;
    }

    // Construct key combination from actual modifiers and key
    int keyInt = event->key();
    Qt::KeyboardModifiers modifiers = event->modifiers();
    
    // Remove modifiers that Qt automatically applies to the key
    if (modifiers & Qt::ShiftModifier) {
        // For number keys and other keys, we want to preserve the base key
        // not the shifted character
        if (keyInt >= Qt::Key_Exclam && keyInt <= Qt::Key_At) {
            // These are shifted characters, map back to base keys
            const QMap<int, int> shiftedToBase = {
                {Qt::Key_Exclam, Qt::Key_1},
                {Qt::Key_At, Qt::Key_2},
                {Qt::Key_NumberSign, Qt::Key_3},
                {Qt::Key_Dollar, Qt::Key_4},
                {Qt::Key_Percent, Qt::Key_5},
                {Qt::Key_AsciiCircum, Qt::Key_6},
                {Qt::Key_Ampersand, Qt::Key_7},
                {Qt::Key_Asterisk, Qt::Key_8},
                {Qt::Key_ParenLeft, Qt::Key_9},
                {Qt::Key_ParenRight, Qt::Key_0}
            };
            if (shiftedToBase.contains(keyInt)) {
                keyInt = shiftedToBase[keyInt];
            }
        }
    }
    
    // Build key combination
    QKeyCombination combination(modifiers, static_cast<Qt::Key>(keyInt));
    QKeySequence sequence(combination);
    
    setKeySequence(sequence);
    _keyPressCount++;
    
    event->accept();
}

KeybindsSettingsWidget::KeybindsSettingsWidget(SettingsDialog *dialog, QWidget *parent)
    : SettingsWidget(QObject::tr("Keybinds"), parent)
    , _dialog(dialog)
    , _table(nullptr) {
    buildUI();
    loadActions();
}

void KeybindsSettingsWidget::buildUI() {
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    setLayout(mainLayout);

    QLabel *info = new QLabel(tr("Customize keyboard shortcuts for actions."), this);
    info->setWordWrap(true);
    info->setStyleSheet("color: gray; font-size: 11px;");
    mainLayout->addWidget(info);

    _table = new QTableWidget(this);
    _table->setColumnCount(3);
    QStringList headers;
    headers << tr("Action") << tr("Shortcut") << tr("Reset");
    _table->setHorizontalHeaderLabels(headers);
    _table->horizontalHeader()->setStretchLastSection(false);
    _table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    _table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);
    _table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    _table->setColumnWidth(1, 250);  // Set width for shortcut column
    _table->verticalHeader()->setVisible(false);
    _table->setShowGrid(true);
    _table->setAlternatingRowColors(true);
    _table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    _table->setFocusPolicy(Qt::NoFocus);

    mainLayout->addWidget(_table);

    QHBoxLayout *btns = new QHBoxLayout();
    QPushButton *resetAll = new QPushButton(tr("Restore All Defaults"), this);
    connect(resetAll, &QPushButton::clicked, this, &KeybindsSettingsWidget::resetAllToDefaults);
    btns->addStretch(1);
    btns->addWidget(resetAll);
    mainLayout->addLayout(btns);
}

static QString keySeqToString(const QKeySequence &seq) {
    // Use PortableText to ensure consistency across platforms
    return seq.toString(QKeySequence::PortableText);
}

QStringList KeybindsSettingsWidget::getActionOrder() const {
    // Define menu order: File, Edit, Tools (subdivided), View, Playback, MIDI
    QStringList order;
    
    // File menu
    order << "new" << "open" << "save" << "save_as" << "quit";
    
    // Edit menu
    order << "undo" << "redo" << "select_all"
          << "navigate_up" << "navigate_down" << "navigate_left" << "navigate_right"
          << "copy" << "paste";
    
    // Tools menu - Tools submenu
    order << "standard_tool" << "new_note" << "remove_notes"
          << "select_single" << "select_box" << "select_left" << "select_right"
          << "move_all" << "move_lr" << "move_ud" << "size_change"
          << "measure" << "time_signature" << "tempo";
    
    // Tools menu - Tweak submenu
    order << "tweak_time" << "tweak_start_time" << "tweak_end_time" << "tweak_note" << "tweak_value"
          << "tweak_small_decrease" << "tweak_small_increase"
          << "tweak_medium_decrease" << "tweak_medium_increase"
          << "tweak_large_decrease" << "tweak_large_increase";
    
    // Tools menu - Editing
    order << "delete" << "align_left" << "align_right" << "equalize"
          << "glue" << "glue_all_channels" << "scissors" << "delete_overlaps"
          << "convert_pitch_bend_to_notes" << "explode_chords_to_tracks";
    
    // Tools menu - Quantization & Transform
    order << "quantize" << "quantize_ntuplet_dialog" << "quantize_ntuplet_repeat"
          << "transpose" << "transpose_up" << "transpose_down"
          << "scale_selection" << "magnet";
    
    // View menu
    order << "zoom_hor_out" << "zoom_hor_in" << "zoom_ver_out" << "zoom_ver_in"
          << "zoom_std" << "reset_view";
    
    // Playback menu
    order << "play_stop" << "play" << "pause" << "record" << "stop"
          << "back_to_begin" << "back" << "forward" << "back_marker" << "forward_marker"
          << "lock" << "metronome";
    
    // MIDI menu
    order << "thru" << "panic";
    
    return order;
}

void KeybindsSettingsWidget::loadActions() {
    _table->setRowCount(0);
    _rowToActionId.clear();

    if (!_dialog || !_dialog->mainWindow()) return;

    // Cache defaults once
    _defaults = _dialog->mainWindow()->getDefaultShortcuts();

    QMap<QString, QAction*> map = _dialog->mainWindow()->getActionMap();
    QStringList order = getActionOrder();

    int row = 0;
    // Iterate in defined order
    for (const QString &id : order) {
        QAction *action = map.value(id, nullptr);
        if (!action) continue;

        _table->insertRow(row);
        _rowToActionId[row] = id;

        // Column 0: Action text
        QLabel *label = new QLabel(action->text(), _table);
        label->setContentsMargins(5, 2, 5, 2);
        _table->setCellWidget(row, 0, label);

        // Column 1: Interactive shortcut editor
        CustomKeySequenceEdit *edit = new CustomKeySequenceEdit(_table);
        QList<QKeySequence> curr = action->shortcuts();
        if (curr.isEmpty()) {
            QKeySequence single = action->shortcut();
            if (!single.isEmpty()) curr << single;
        }
        // QKeySequenceEdit only supports single shortcut, use first one
        if (!curr.isEmpty()) {
            edit->setKeySequence(curr.first());
        }
        edit->setClearButtonEnabled(true);
        _table->setCellWidget(row, 1, edit);

        // Column 2: Reset button
        QPushButton *resetBtn = new QPushButton(tr("Reset"), _table);
        resetBtn->setProperty("row", row);
        connect(resetBtn, &QPushButton::clicked, this, [this, resetBtn]() {
            bool ok = false;
            int r = resetBtn->property("row").toInt(&ok);
            if (ok) resetRowToDefault(r);
        });
        _table->setCellWidget(row, 2, resetBtn);

        row++;
    }

    _table->resizeColumnsToContents();
}

void KeybindsSettingsWidget::resetRowToDefault(int row) {
    if (!_rowToActionId.contains(row)) return;
    const QString id = _rowToActionId.value(row);
    QList<QKeySequence> def = _defaults.value(id);
    if (QWidget *w = _table->cellWidget(row, 1)) {
        if (CustomKeySequenceEdit *e = qobject_cast<CustomKeySequenceEdit*>(w)) {
            if (!def.isEmpty()) {
                e->setKeySequence(def.first());
            } else {
                e->clear();
            }
        }
    }
}

void KeybindsSettingsWidget::resetAllToDefaults() {
    for (auto it = _rowToActionId.constBegin(); it != _rowToActionId.constEnd(); ++it) {
        resetRowToDefault(it.key());
    }
}

bool KeybindsSettingsWidget::accept() {
    if (!_dialog || !_dialog->mainWindow()) return true;

    // First pass: collect all shortcuts and check for duplicates
    QMap<QKeySequence, QStringList> shortcutToActions;
    for (auto it = _rowToActionId.constBegin(); it != _rowToActionId.constEnd(); ++it) {
        int row = it.key();
        const QString id = it.value();
        QWidget *w = _table->cellWidget(row, 1);
        if (QKeySequenceEdit *e = qobject_cast<QKeySequenceEdit*>(w)) {
            QKeySequence seq = e->keySequence();
            if (!seq.isEmpty()) {
                shortcutToActions[seq].append(id);
            }
        }
    }

    // Check for duplicates
    QStringList duplicates;
    for (auto it = shortcutToActions.constBegin(); it != shortcutToActions.constEnd(); ++it) {
        const QStringList &actionIds = it.value();
        if (actionIds.size() > 1) {
            QString keyStr = it.key().toString(QKeySequence::NativeText);
            QString actionsStr = actionIds.join(", ");
            duplicates.append(tr("Shortcut '%1' assigned to: %2").arg(keyStr, actionsStr));
        }
    }

    if (!duplicates.isEmpty()) {
        QMessageBox::warning(this, tr("Duplicate Shortcuts"),
                             tr("The following shortcuts are assigned to multiple actions:\n\n%1\n\n"
                                "Please resolve these conflicts before saving.")
                             .arg(duplicates.join("\n")));
        return false;
    }

    QSettings *settings = _dialog->settings();
    settings->beginGroup("shortcuts");

    // Apply each row
    for (auto it = _rowToActionId.constBegin(); it != _rowToActionId.constEnd(); ++it) {
        int row = it.key();
        const QString id = it.value();
        QWidget *w = _table->cellWidget(row, 1);
        if (CustomKeySequenceEdit *e = qobject_cast<CustomKeySequenceEdit*>(w)) {
            QKeySequence seq = e->keySequence();
            QList<QKeySequence> seqs;
            if (!seq.isEmpty()) seqs << seq;

            // Apply to action
            _dialog->mainWindow()->setActionShortcuts(id, seqs);

            // Store only if different from defaults; otherwise remove key
            QList<QKeySequence> def = _defaults.value(id);
            if (def == seqs || (def.isEmpty() && seqs.isEmpty())) {
                settings->remove(id);
            } else {
                QStringList list;
                for (const QKeySequence &s: seqs) list << keySeqToString(s);
                settings->setValue(id, list);
            }
        }
    }

    settings->endGroup();
    return true;
}
