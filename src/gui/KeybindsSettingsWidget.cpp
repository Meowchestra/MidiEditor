/*
 * MidiEditor - Keybinds Settings implementation
 */
#include "KeybindsSettingsWidget.h"
#include "SettingsDialog.h"
#include "MainWindow.h"
#include "MatrixWidget.h"

#include <QTimer>
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
#include <QFrame>
#include <QComboBox>
#include <QSet>

// PianoKeyButton implementation
PianoKeyButton::PianoKeyButton(int offset, bool isBlack, QWidget* parent) 
    : QPushButton(parent), noteOffset(offset), currentKey(0), isBlackKey(isBlack) {
    if (isBlackKey) {
        setStyleSheet("QPushButton { background-color: #333; color: white; border: 1px solid #111; border-radius: 2px; } "
                      "QPushButton:focus { border: 2px solid #0078D7; background-color: #444; }");
    } else {
        setStyleSheet("QPushButton { background-color: #f9f9f9; color: black; border: 1px solid #ccc; border-radius: 2px; padding-top: 45px; } "
                      "QPushButton:focus { border: 2px solid #0078D7; background-color: #e5f1fb; }");
    }
    
    // Make text bold and legible
    QFont f = font();
    f.setPixelSize(10);
    f.setBold(true);
    setFont(f);
}

void PianoKeyButton::setKey(int key) {
    currentKey = key;
    if (key == 0) {
        setText("");
    } else {
        setText(QKeySequence(key).toString());
    }
}

void PianoKeyButton::flashError() {
    setStyleSheet("QPushButton { background-color: #ffcccc; color: black; border: 2px solid red; border-radius: 2px; } ");
    
    QTimer::singleShot(500, this, [this]() {
        if (isBlackKey) {
            setStyleSheet("QPushButton { background-color: #333; color: white; border: 1px solid #111; border-radius: 2px; } "
                          "QPushButton:focus { border: 2px solid #0078D7; background-color: #444; }");
        } else {
            setStyleSheet("QPushButton { background-color: #f9f9f9; color: black; border: 1px solid #ccc; border-radius: 2px; padding-top: 45px; } "
                          "QPushButton:focus { border: 2px solid #0078D7; background-color: #e5f1fb; }");
        }
    });
}

void PianoKeyButton::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape || event->key() == Qt::Key_Backspace) {
        setKey(0);
        emit keyChanged(this, noteOffset, 0);
        clearFocus();
        return;
    }
    
    // Ignore modifiers
    if (event->key() == Qt::Key_Control || event->key() == Qt::Key_Shift ||
        event->key() == Qt::Key_Alt || event->key() == Qt::Key_Meta) {
        return;
    }
    
    int newKey = event->key();
    setKey(newKey);
    emit keyChanged(this, noteOffset, newKey);
    clearFocus();
}

// CaptureKeyButton implementation
CaptureKeyButton::CaptureKeyButton(QWidget* parent) : QPushButton(parent), currentKey(0) {
    setFocusPolicy(Qt::StrongFocus);
    setCheckable(true);
    setText(tr("Press shortcut"));
    setMinimumWidth(120);
    setStyleSheet(
        "QPushButton { padding: 4px 12px; border: 2px solid palette(midlight); border-radius: 3px; background-color: palette(base); color: palette(mid); }"
        "QPushButton:checked { border: 2px solid palette(highlight); background-color: palette(base); color: palette(highlight); }"
    );
    
    // Auto-timeout: uncheck after 3 seconds if no key is pressed
    _captureTimer = new QTimer(this);
    _captureTimer->setSingleShot(true);
    _captureTimer->setInterval(3000);
    connect(_captureTimer, &QTimer::timeout, this, [this]() {
        setChecked(false);
    });
    
    _errorTimer = new QTimer(this);
    _errorTimer->setSingleShot(true);
    _errorTimer->setInterval(400);
    connect(_errorTimer, &QTimer::timeout, this, [this]() {
        setKey(currentKey); // This will restore the correct stylesheet
    });

    connect(this, &QPushButton::toggled, this, [this](bool checked) {
        if (checked) {
            setText(tr("Press a key..."));
            _captureTimer->start();
        } else {
            _captureTimer->stop();
            // Restore display text
            setKey(currentKey);
        }
    });
}

void CaptureKeyButton::flashError() {
    setStyleSheet(
        "QPushButton { padding: 4px 12px; border: 2px solid #ff4444; border-radius: 3px; background-color: #ffcccc; color: #ff0000; font-weight: bold; }"
    );
    _errorTimer->start();
}

static QString vkCodeToDisplayName(int vkCode) {
    // Map Windows virtual key codes to human-readable names
    switch (vkCode) {
        case 0xA0: return QObject::tr("Left Shift");
        case 0xA1: return QObject::tr("Right Shift");
        case 0xA2: return QObject::tr("Left Ctrl");
        case 0xA3: return QObject::tr("Right Ctrl");
        case 0xA4: return QObject::tr("Left Alt");
        case 0xA5: return QObject::tr("Right Alt");
        case 0x10: return QObject::tr("Shift");
        case 0x11: return QObject::tr("Ctrl");
        case 0x12: return QObject::tr("Alt");
        case 0x20: return QObject::tr("Space");
        case 0x09: return QObject::tr("Tab");
        default: {
            // Try Qt key sequence for normal keys
            QString name = QKeySequence(vkCode).toString();
            if (!name.isEmpty()) return name;
            return QString("0x%1").arg(vkCode, 0, 16);
        }
    }
}

void CaptureKeyButton::setKey(int key) {
    currentKey = key;
    if (key == 0) {
        setText(tr("Press shortcut"));
        setStyleSheet(
            "QPushButton { padding: 4px 12px; border: 2px solid palette(midlight); border-radius: 3px; background-color: palette(base); color: palette(mid); }"
            "QPushButton:checked { border: 2px solid palette(highlight); background-color: palette(base); color: palette(highlight); }"
        );
    } else {
        setText(vkCodeToDisplayName(key));
        setStyleSheet(
            "QPushButton { padding: 4px 12px; border: 2px solid palette(midlight); border-radius: 3px; background-color: palette(base); color: palette(text); }"
            "QPushButton:checked { border: 2px solid palette(highlight); background-color: palette(base); color: palette(highlight); }"
        );
    }
    setChecked(false);
}

void CaptureKeyButton::keyPressEvent(QKeyEvent *event) {
    if (!isChecked()) {
        QPushButton::keyPressEvent(event);
        return;
    }
    
    if (event->key() == Qt::Key_Escape) {
        emit keyChanged(this, 0);
        return;
    }
    
    // Detect Left/Right Shift via native virtual key
    quint32 nativeVK = event->nativeVirtualKey();
    if (nativeVK == 0xA0 || nativeVK == 0xA1) {
        // Store as Windows VK code directly
        emit keyChanged(this, (int)nativeVK);
        clearFocus();
        return;
    }
    
    // For generic Shift (couldn't differentiate), ignore
    if (event->key() == Qt::Key_Shift || event->key() == Qt::Key_Control ||
        event->key() == Qt::Key_Alt || event->key() == Qt::Key_Meta) {
        return;
    }

    emit keyChanged(this, event->key());
    clearFocus();
}

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
    , _table(nullptr)
    , _pianoStatusLabel(nullptr)
    , _statusTimer(new QTimer(this))
    , _octaveUpButton(nullptr)
    , _octaveDownButton(nullptr) {
    _statusTimer->setSingleShot(true);
    connect(_statusTimer, &QTimer::timeout, this, [this]() {
        if (_pianoStatusLabel) _pianoStatusLabel->setText("");
    });
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
    _table->setColumnWidth(1, 180);  // Set width for shortcut column
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

    // Add Piano Emulation UI
    QFrame *line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    mainLayout->addWidget(line);

    QLabel *pianoInfo = new QLabel(tr("Piano Emulation Keybinds"), this);
    pianoInfo->setStyleSheet("font-weight: bold;");
    mainLayout->addWidget(pianoInfo);

    QLabel *pianoHelp = new QLabel(tr("Click any key on the piano below, then press a key on your keyboard to map it."), this);
    pianoHelp->setStyleSheet("color: gray; font-size: 11px;");
    pianoHelp->setWordWrap(true);
    mainLayout->addWidget(pianoHelp);

    buildPianoUI(mainLayout);
}

void KeybindsSettingsWidget::buildPianoUI(QVBoxLayout *layout) {
    QWidget *pianoContainer = new QWidget(this);
    pianoContainer->setMinimumHeight(120);
    
    int whiteKeyWidth = 26;
    int whiteKeyHeight = 100;
    int blackKeyWidth = 16;
    int blackKeyHeight = 60;
    
    pianoContainer->setFixedSize(22 * whiteKeyWidth, whiteKeyHeight);
    
    const int C3_OFFSET = 48;
    const int C4_OFFSET = 60;
    
    // Default mapping only to C4-C5 using QWERTY row
    QMap<int, int> defaults;
    defaults.insert(C4_OFFSET + 0, Qt::Key_Q);   // C4
    defaults.insert(C4_OFFSET + 1, Qt::Key_2);   // C#4
    defaults.insert(C4_OFFSET + 2, Qt::Key_W);   // D4
    defaults.insert(C4_OFFSET + 3, Qt::Key_3);   // D#4
    defaults.insert(C4_OFFSET + 4, Qt::Key_E);   // E4
    defaults.insert(C4_OFFSET + 5, Qt::Key_R);   // F4
    defaults.insert(C4_OFFSET + 6, Qt::Key_5);   // F#4
    defaults.insert(C4_OFFSET + 7, Qt::Key_T);   // G4
    defaults.insert(C4_OFFSET + 8, Qt::Key_6);   // G#4
    defaults.insert(C4_OFFSET + 9, Qt::Key_Y);   // A4
    defaults.insert(C4_OFFSET + 10, Qt::Key_7);  // A#4
    defaults.insert(C4_OFFSET + 11, Qt::Key_U);  // B4
    defaults.insert(C4_OFFSET + 12, Qt::Key_I);  // C5

    // Load from settings if exists
    _pianoKeyMap.clear();
    QSettings *settings = _dialog->settings();
    if (settings->contains("piano_emulation/keys")) {
        QVariantMap map = settings->value("piano_emulation/keys").toMap();
        for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
            _pianoKeyMap.insert(it.key().toInt(), it.value().toInt());
        }
    } else {
        _pianoKeyMap = defaults;
    }

    // Add white keys first (background)
    int whiteKeyIndex = 0;
    for (int i = 0; i < 37; i++) {
        int noteInOctave = i % 12;
        bool isBlack = (noteInOctave == 1 || noteInOctave == 3 || noteInOctave == 6 || noteInOctave == 8 || noteInOctave == 10);
        if (!isBlack) {
            int noteOffset = C3_OFFSET + i;
            PianoKeyButton *btn = new PianoKeyButton(noteOffset, false, pianoContainer);
            btn->setGeometry(whiteKeyIndex * whiteKeyWidth, 0, whiteKeyWidth, whiteKeyHeight);
            btn->setKey(_pianoKeyMap.value(noteOffset, 0));
            
            // Add C label
            if (noteInOctave == 0) {
                QLabel *l = new QLabel(QString("C%1").arg(noteOffset / 12 - 1), btn);
                l->setGeometry(0, whiteKeyHeight - 20, whiteKeyWidth, 20);
                l->setAlignment(Qt::AlignCenter);
                l->setStyleSheet("color: gray; font-size: 9px;");
            }
            connect(btn, &PianoKeyButton::keyChanged, this, &KeybindsSettingsWidget::onPianoKeyChanged);
            whiteKeyIndex++;
        }
    }
    
    // Add black keys (foreground)
    whiteKeyIndex = 0;
    for (int i = 0; i < 37; i++) {
        int noteInOctave = i % 12;
        bool isBlack = (noteInOctave == 1 || noteInOctave == 3 || noteInOctave == 6 || noteInOctave == 8 || noteInOctave == 10);
        if (!isBlack) {
            whiteKeyIndex++;
        } else {
            int noteOffset = C3_OFFSET + i;
            PianoKeyButton *btn = new PianoKeyButton(noteOffset, true, pianoContainer);
            btn->setGeometry(whiteKeyIndex * whiteKeyWidth - (blackKeyWidth / 2), 0, blackKeyWidth, blackKeyHeight);
            btn->setKey(_pianoKeyMap.value(noteOffset, 0));
            connect(btn, &PianoKeyButton::keyChanged, this, &KeybindsSettingsWidget::onPianoKeyChanged);
        }
    }
    
    // Wrap the piano in a layout that centers it
    QHBoxLayout *pianoLayout = new QHBoxLayout();
    pianoLayout->addStretch(1);
    pianoLayout->addWidget(pianoContainer);
    pianoLayout->addStretch(1);
    
    layout->addLayout(pianoLayout);
    
    QHBoxLayout *optionsLayout = new QHBoxLayout();
    optionsLayout->addStretch(1);
    
    optionsLayout->addWidget(new QLabel(tr("Octave Down:"), this));
    _octaveDownButton = new CaptureKeyButton(this);
    optionsLayout->addWidget(_octaveDownButton);
    
    optionsLayout->addSpacing(30);
    
    QPushButton *btnRestore = new QPushButton(tr("Reset"), this);
    connect(btnRestore, &QPushButton::clicked, this, [this, defaults]() {
        _pianoKeyMap = defaults;
        if (_octaveDownButton) _octaveDownButton->setKey(Qt::Key_Comma);
        if (_octaveUpButton) _octaveUpButton->setKey(Qt::Key_Period);
        
        QList<PianoKeyButton*> btns = findChildren<PianoKeyButton*>();
        for (PianoKeyButton* btn : btns) {
            btn->setKey(_pianoKeyMap.value(btn->noteOffset, 0));
        }
        
        // Save and update matrix immediately
        QSettings *settings = _dialog->settings();
        QVariantMap pianoMap;
        for (auto it = _pianoKeyMap.constBegin(); it != _pianoKeyMap.constEnd(); ++it) {
            pianoMap.insert(QString::number(it.key()), it.value());
        }
        settings->setValue("piano_emulation/keys", pianoMap);
        settings->setValue("piano_emulation/octave_down", Qt::Key_Comma);
        settings->setValue("piano_emulation/octave_up", Qt::Key_Period);
        settings->sync();
        if (_dialog && _dialog->mainWindow() && _dialog->mainWindow()->matrixWidget()) {
            _dialog->mainWindow()->matrixWidget()->updateRenderingSettings();
        }
        checkDuplicates();
    });
    optionsLayout->addWidget(btnRestore);
    
    optionsLayout->addSpacing(30);
    
    optionsLayout->addWidget(new QLabel(tr("Octave Up:"), this));
    _octaveUpButton = new CaptureKeyButton(this);
    optionsLayout->addWidget(_octaveUpButton);
    
    // Load values
    int downKey = settings->value("piano_emulation/octave_down", Qt::Key_Comma).toInt();
    int upKey = settings->value("piano_emulation/octave_up", Qt::Key_Period).toInt();
    
    _octaveDownButton->setKey(downKey);
    _octaveUpButton->setKey(upKey);
    
    auto saveSettings = [this](CaptureKeyButton* btn, int key) {
        if (key == 0) {
            btn->setKey(0);
            QSettings *settings = _dialog->settings();
            settings->setValue(btn == _octaveDownButton ? "piano_emulation/octave_down" : "piano_emulation/octave_up", 0);
            settings->sync();
            if (_dialog && _dialog->mainWindow() && _dialog->mainWindow()->matrixWidget()) {
                _dialog->mainWindow()->matrixWidget()->updateRenderingSettings();
            }
            checkDuplicates();
            return;
        }

        // Check duplicates
        bool isDuplicate = false;
        QKeySequence pianoSeq(key);
        
        // 1. Check against normal keybinds
        for (int i = 0; i < _table->rowCount(); i++) {
            QKeySequenceEdit *edit = qobject_cast<QKeySequenceEdit*>(_table->cellWidget(i, 1));
            if (edit && !edit->keySequence().isEmpty()) {
                if (edit->keySequence()[0] == pianoSeq[0]) {
                    isDuplicate = true;
                    break;
                }
            }
        }
        
        // 2. Check against piano keys
        if (!isDuplicate) {
            for (auto it = _pianoKeyMap.constBegin(); it != _pianoKeyMap.constEnd(); ++it) {
                if (it.value() == key) {
                    isDuplicate = true;
                    break;
                }
            }
        }
        
        // 3. Check against the other octave button
        if (!isDuplicate) {
            CaptureKeyButton* otherBtn = (btn == _octaveDownButton) ? _octaveUpButton : _octaveDownButton;
            if (otherBtn && otherBtn->currentKey == key) {
                isDuplicate = true;
            }
        }
        
        if (isDuplicate) {
            btn->setKey(btn->currentKey); // Restore visual display to old key
            btn->flashError();
            if (_pianoStatusLabel) {
                _pianoStatusLabel->setText(tr("Warning: %1 is already in use!").arg(pianoSeq.toString()));
                _pianoStatusLabel->setStyleSheet("color: red; font-weight: bold; font-size: 12px;");
                _statusTimer->start(8000);
            }
            return; // Reject the change
        }

        // Accept the change
        btn->setKey(key);
        QSettings *settings = _dialog->settings();
        settings->setValue(btn == _octaveDownButton ? "piano_emulation/octave_down" : "piano_emulation/octave_up", key);
        settings->sync();
        if (_dialog && _dialog->mainWindow() && _dialog->mainWindow()->matrixWidget()) {
            _dialog->mainWindow()->matrixWidget()->updateRenderingSettings();
        }
        
        if (_pianoStatusLabel) {
            _pianoStatusLabel->setText(tr("Mapped %1 to: %2")
                .arg(btn == _octaveDownButton ? "Octave Down" : "Octave Up")
                .arg(pianoSeq.toString()));
            _pianoStatusLabel->setStyleSheet("color: #0078D7; font-weight: bold; font-size: 12px;");
            _statusTimer->start(8000);
        }
        
        checkDuplicates();
    };
    
    connect(_octaveDownButton, &CaptureKeyButton::keyChanged, saveSettings);
    connect(_octaveUpButton, &CaptureKeyButton::keyChanged, saveSettings);
    
    optionsLayout->addStretch(1);
    layout->addLayout(optionsLayout);
    
    _pianoStatusLabel = new QLabel(this);
    _pianoStatusLabel->setStyleSheet("color: #0078D7; font-weight: bold; font-size: 12px;");
    _pianoStatusLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(_pianoStatusLabel);
}

void KeybindsSettingsWidget::onPianoKeyChanged(PianoKeyButton *btn, int offset, int key) {
    if (key == 0) {
        _pianoKeyMap.remove(offset);
        // Save immediately
        QSettings *settings = _dialog->settings();
        QVariantMap pianoMap;
        for (auto it = _pianoKeyMap.constBegin(); it != _pianoKeyMap.constEnd(); ++it) {
            pianoMap.insert(QString::number(it.key()), it.value());
        }
        settings->setValue("piano_emulation/keys", pianoMap);
        settings->sync();
        if (_dialog && _dialog->mainWindow() && _dialog->mainWindow()->matrixWidget()) {
            _dialog->mainWindow()->matrixWidget()->updateRenderingSettings();
        }
        checkDuplicates();
        return;
    }
    
    // Check duplicates immediately
    bool isDuplicate = false;
    QKeySequence pianoSeq(key);
    
    // 1. Check against normal keybinds (this is simplistic but catches direct overlap)
    for (int i = 0; i < _table->rowCount(); i++) {
        QKeySequenceEdit *edit = qobject_cast<QKeySequenceEdit*>(_table->cellWidget(i, 1));
        if (edit && !edit->keySequence().isEmpty()) {
            if (edit->keySequence()[0] == pianoSeq[0]) {
                isDuplicate = true;
                break;
            }
        }
    }
    
    // 2. Check against other piano keys
    if (!isDuplicate) {
        for (auto it = _pianoKeyMap.constBegin(); it != _pianoKeyMap.constEnd(); ++it) {
            if (it.key() != offset && it.value() == key) {
                isDuplicate = true;
                break;
            }
        }
    }
    
    if (isDuplicate) {
        btn->setKey(_pianoKeyMap.value(offset, 0)); // Restore old key visually
        btn->flashError();
        if (_pianoStatusLabel) {
            _pianoStatusLabel->setText(tr("Warning: %1 is already in use!").arg(pianoSeq.toString()));
            _pianoStatusLabel->setStyleSheet("color: red; font-weight: bold; font-size: 12px;");
            _statusTimer->start(8000);
        }
        return;
    }
    
    _pianoKeyMap.insert(offset, key);
    
    // Save to settings immediately
    QSettings *settings = _dialog->settings();
    QVariantMap pianoMap;
    for (auto it = _pianoKeyMap.constBegin(); it != _pianoKeyMap.constEnd(); ++it) {
        pianoMap.insert(QString::number(it.key()), it.value());
    }
    settings->setValue("piano_emulation/keys", pianoMap);
    settings->sync();
    
    // Tell MatrixWidget to update
    if (_dialog && _dialog->mainWindow() && _dialog->mainWindow()->matrixWidget()) {
        _dialog->mainWindow()->matrixWidget()->updateRenderingSettings();
    }
    
    if (_pianoStatusLabel) {
        int noteInOctave = offset % 12;
        int octave = (offset / 12) - 1;
        const char* noteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
        QString noteName = QString("%1%2").arg(noteNames[noteInOctave]).arg(octave);
        
        _pianoStatusLabel->setText(tr("Mapped %1 (%2) to: %3").arg(noteName).arg(offset).arg(pianoSeq.toString()));
        _pianoStatusLabel->setStyleSheet("color: #0078D7; font-weight: bold; font-size: 12px;");
        _statusTimer->start(8000);
    }
    
    checkDuplicates();
}

static QString keySeqToString(const QKeySequence &seq) {
    // Use PortableText to ensure consistency across platforms
    return seq.toString(QKeySequence::PortableText);
}

QStringList KeybindsSettingsWidget::getActionOrder() const {
    // Define menu order: File, Edit, Tools (subdivided), View, Playback, MIDI
    QStringList order;
    
    // File menu
    order << "new" << "open" << "save" << "save_as" << "export_audio" << "quit";
    
    // Edit menu
    order << "undo" << "redo" << "select_all"
          << "navigate_up" << "navigate_down" << "navigate_left" << "navigate_right"
          << "copy" << "paste";
    
    // Tools menu - Tools submenu
    order << "standard_tool" << "new_note" << "remove_notes"
          << "select_single" << "select_box" << "select_row" << "select_measure" << "select_left" << "select_right"
          << "move_all" << "move_lr" << "move_ud" << "size_change"
          << "measure" << "time_signature" << "tempo"
          << "duration_drag" << "duration_1" << "duration_2" << "duration_4" 
          << "duration_8" << "duration_16" << "duration_32" << "duration_64"
          << "duration_3" << "duration_5" << "duration_6" << "duration_7" 
          << "duration_9" << "duration_12" << "duration_24" << "duration_48";
    
    // Tools menu - Tweak submenu
    order << "tweak_time" << "tweak_start_time" << "tweak_end_time" << "tweak_note" << "tweak_value"
          << "tweak_small_decrease" << "tweak_small_increase"
          << "tweak_medium_decrease" << "tweak_medium_increase"
          << "tweak_large_decrease" << "tweak_large_increase";
    
    // Tools menu - Editing
    order << "delete" << "align_left" << "align_right" << "equalize"
          << "glue" << "glue_all" << "scissors" << "delete_overlaps"
          << "convert_pitch_bend_to_notes" << "explode_chords_to_tracks" << "split_channels_to_tracks"<< "strum";
    
    // Tools menu - Quantization & Transform
    order << "quantize" << "quantize_ntuplet_dialog" << "quantize_ntuplet_repeat"
          << "transpose" << "transpose_up" << "transpose_down"
          << "set_file_duration" << "trim_blank_start" << "set_velocity"
          << "scale_selection" << "magnet";
    
    // View menu
    order << "zoom_hor_out" << "zoom_hor_in" << "zoom_ver_out" << "zoom_ver_in"
          << "zoom_std" << "reset_view";
    
    // Playback menu
    order << "play_stop" << "play" << "pause" << "record" << "stop"
          << "back_to_begin" << "back" << "forward" << "back_marker" << "forward_marker"
          << "lock" << "smooth_playback_scroll" << "metronome";
    
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

        // Connect change signal for duplicate checking (scans all rows)
        connect(edit, &QKeySequenceEdit::keySequenceChanged, this, &KeybindsSettingsWidget::checkDuplicates);

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

    // Don't call resizeColumnsToContents() as it overrides the custom width for the shortcut column
    // and shrinks it to the smallest entry. Column 0 is Stretch, Col 2 is ResizeToContents mode.
    // _table->resizeColumnsToContents();
    
    // Perform initial check
    checkDuplicates();
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
            // Trigger validation after programmatic change
            checkDuplicates();

            // Persist immediately
            QSettings *settings = _dialog->settings();
            settings->beginGroup("shortcuts");
            if (def.isEmpty()) {
                settings->remove(id);
            } else {
                QStringList seqStrings;
                for (const QKeySequence &s : def) {
                    seqStrings << s.toString(QKeySequence::PortableText);
                }
                settings->setValue(id, seqStrings);
            }
            settings->endGroup();
            settings->sync();

            // Apply to live action
            _dialog->mainWindow()->setActionShortcuts(id, def);
        }
    }
}

void KeybindsSettingsWidget::resetAllToDefaults() {
    for (auto it = _rowToActionId.constBegin(); it != _rowToActionId.constEnd(); ++it) {
        resetRowToDefault(it.key());
    }
}

void KeybindsSettingsWidget::checkDuplicates() {
    if (!_table) return;

    QMap<QString, int> counts;
    
    // Collect piano key sequences for cross-checking
    QSet<QString> pianoKeyStrings;
    for (auto it = _pianoKeyMap.constBegin(); it != _pianoKeyMap.constEnd(); ++it) {
        if (it.value() != 0) {
            pianoKeyStrings.insert(QKeySequence(it.value()).toString(QKeySequence::PortableText));
        }
    }
    // Add Octave keys to conflicts
    if (_octaveUpButton && _octaveUpButton->currentKey != 0) {
        pianoKeyStrings.insert(QKeySequence(_octaveUpButton->currentKey).toString(QKeySequence::PortableText));
    }
    if (_octaveDownButton && _octaveDownButton->currentKey != 0) {
        pianoKeyStrings.insert(QKeySequence(_octaveDownButton->currentKey).toString(QKeySequence::PortableText));
    }
    
    // First pass: Count occurrences
    for (int r = 0; r < _table->rowCount(); r++) {
        QWidget *w = _table->cellWidget(r, 1);
        if (CustomKeySequenceEdit *e = qobject_cast<CustomKeySequenceEdit*>(w)) {
            QKeySequence seq = e->keySequence();
            if (!seq.isEmpty()) {
                // Use PortableText for consistent comparison
                counts[seq.toString(QKeySequence::PortableText)]++;
            }
        }
    }

    // Second pass: Update UI
    for (int r = 0; r < _table->rowCount(); r++) {
        QWidget *w = _table->cellWidget(r, 1);
        if (CustomKeySequenceEdit *e = qobject_cast<CustomKeySequenceEdit*>(w)) {
            QKeySequence seq = e->keySequence();
            QString seqStr = seq.toString(QKeySequence::PortableText);
            
            bool isDup = (!seq.isEmpty() && counts.value(seqStr) > 1);
            bool isPianoConflict = (!seq.isEmpty() && pianoKeyStrings.contains(seqStr));
            
            if (isDup || isPianoConflict) {
                e->setStyleSheet("background-color: #ffcccc;");
                e->setToolTip(isPianoConflict ? tr("Conflicts with piano emulation key") : tr("Duplicate shortcut"));
            } else {
                e->setStyleSheet("");
                e->setToolTip("");
            }
        }
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

    // Check for duplicates in main actions
    QStringList duplicates;
    for (auto it = shortcutToActions.constBegin(); it != shortcutToActions.constEnd(); ++it) {
        const QStringList &actionIds = it.value();
        if (actionIds.size() > 1) {
            QString keyStr = it.key().toString(QKeySequence::NativeText);
            QString actionsStr = actionIds.join(", ");
            duplicates.append(tr("Shortcut '%1' assigned to: %2").arg(keyStr, actionsStr));
        }
    }

    // Check for duplicates in Piano Keys
    QMap<int, QStringList> pianoDuplicates;
    for (auto it = _pianoKeyMap.constBegin(); it != _pianoKeyMap.constEnd(); ++it) {
        if (it.value() != 0) {
            pianoDuplicates[it.value()].append(QString("Piano Note %1").arg(it.key()));
            
            // Cross check with main actions
            QKeySequence pianoSeq(it.value());
            if (shortcutToActions.contains(pianoSeq)) {
                QString keyStr = pianoSeq.toString(QKeySequence::NativeText);
                QString actionsStr = shortcutToActions[pianoSeq].join(", ");
                duplicates.append(tr("Shortcut '%1' assigned to: %2 AND Piano Note %3").arg(keyStr, actionsStr).arg(it.key()));
            }
        }
    }
    
    for (auto it = pianoDuplicates.constBegin(); it != pianoDuplicates.constEnd(); ++it) {
        if (it.value().size() > 1) {
            QString keyStr = QKeySequence(it.key()).toString(QKeySequence::NativeText);
            QString actionsStr = it.value().join(", ");
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
            
            bool isDefault = (def == seqs) || (def.isEmpty() && seqs.isEmpty());
            
            // Special handling for actions with multiple default shortcuts (like play_stop)
            // The UI only supports editing a single shortcut. If the user hasn't changed the
            // primary shortcut, we should treat it as "default" to preserve secondary shortcuts
            // and avoid writing partial settings to registry.
            if (!isDefault && seqs.size() == 1 && def.size() > 1) {
                if (seqs.first() == def.first()) {
                    isDefault = true;
                    // Restore the full list of default shortcuts to the action
                    // so that secondary shortcuts continue to work
                    _dialog->mainWindow()->setActionShortcuts(id, def);
                }
            }

            if (isDefault) {
                settings->remove(id);
            } else {
                QStringList list;
                for (const QKeySequence &s: seqs) list << keySeqToString(s);
                settings->setValue(id, list);
            }
        }
    }

    settings->endGroup();
    
    // Save custom piano key mappings
    QVariantMap pianoMap;
    for (auto it = _pianoKeyMap.constBegin(); it != _pianoKeyMap.constEnd(); ++it) {
        pianoMap.insert(QString::number(it.key()), it.value());
    }
    settings->setValue("piano_emulation/keys", pianoMap);
    
    if (_octaveUpButton && _octaveDownButton) {
        settings->setValue("piano_emulation/octave_up", _octaveUpButton->currentKey);
        settings->setValue("piano_emulation/octave_down", _octaveDownButton->currentKey);
    }
    
    if (_dialog && _dialog->mainWindow() && _dialog->mainWindow()->matrixWidget()) {
        _dialog->mainWindow()->matrixWidget()->updateRenderingSettings();
    }
    
    settings->sync();
    
    return true;
}