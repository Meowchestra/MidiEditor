/*
 * MidiEditor
 * Copyright (C) 2010  Markus Schwenk
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "MidiSettingsWidget.h"
#include "MainWindow.h"
#include "Appearance.h"

#include "../Terminal.h"
#include "../midi/MidiFile.h"
#include "../midi/MidiInput.h"
#include "../midi/MidiOutput.h"
#include "../midi/Metronome.h"
#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidgetItem>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QTextEdit>

#ifdef FLUIDSYNTH_SUPPORT
#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QSlider>
#include <QVBoxLayout>
#include <QThreadPool>
#include <QTableWidget>
#include <QHeaderView>
#include <QCheckBox>
#include <QPainter>
#include <QMouseEvent>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QStyledItemDelegate>
#include <QtConcurrent/QtConcurrent>
#include "../midi/FluidSynthEngine.h"
#include "../midi/MidiOutput.h"
#include "AudioExportDialog.h"
#include "DownloadSoundFontDialog.h"

#include <QStandardPaths>
#include <QProcess>
#include <QDesktopServices>
#include <QUrl>
#include <QTemporaryFile>
#include <QFileInfo>
#include <QProgressBar>
#include "MainWindow.h"

// Custom role to mark separator rows
static const int SeparatorRole = Qt::UserRole + 100;

// ============================================================================
// SoundFontSeparatorDelegate — draws separator as dotted line with centered text
// ============================================================================
class SoundFontSeparatorDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override {
        QVariant isSep = index.data(SeparatorRole);
        if (isSep.isValid() && isSep.toBool()) {
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing, false);

            int centerY = option.rect.center().y();

            // Draw dotted line across full width
            QPen pen(QColor(140, 140, 140), 1, Qt::DotLine);
            painter->setPen(pen);
            painter->drawLine(option.rect.left() + 4, centerY,
                              option.rect.right() - 4, centerY);

            // Draw centered label with background fill
            QString text = QObject::tr("Disabled SoundFonts");
            QFont font = painter->font();
            font.setPointSizeF(font.pointSizeF() - 0.5);
            font.setItalic(true);
            painter->setFont(font);
            QFontMetrics fm(font);
            int textW = fm.horizontalAdvance(text) + 16;
            int textH = fm.height() + 2;
            QRect textBg(option.rect.center().x() - textW / 2,
                         centerY - textH / 2, textW, textH);

            QColor bgColor = option.widget
                ? option.widget->palette().color(QPalette::Base)
                : QColor(255, 255, 255);
            painter->fillRect(textBg, bgColor);
            painter->setPen(QColor(140, 140, 140));
            painter->drawText(textBg, Qt::AlignCenter, text);

            painter->restore();
            return;
        }
        QStyledItemDelegate::paint(painter, option, index);
    }

    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override {
        QVariant isSep = index.data(SeparatorRole);
        if (isSep.isValid() && isSep.toBool()) {
            return QSize(0, 22);
        }
        return QStyledItemDelegate::sizeHint(option, index);
    }
};

// ============================================================================
// SoundFontDragController — manual drag reordering via event filter
// ============================================================================
class SoundFontDragController : public QObject {
public:
    SoundFontDragController(QTableWidget *table, MidiSettingsWidget *widget)
        : QObject(table), _table(table), _widget(widget) {
        _table->viewport()->installEventFilter(this);
        _table->viewport()->setMouseTracking(true);

        _dropLine = new QFrame(_table->viewport());
        _dropLine->setFrameShape(QFrame::HLine);
        _dropLine->setStyleSheet("background: #3daee9; border: none;");
        _dropLine->setFixedHeight(2);
        _dropLine->hide();

        _preview = new QLabel(_table->viewport());
        _preview->setAttribute(Qt::WA_TransparentForMouseEvents);
        auto *fx = new QGraphicsOpacityEffect(_preview);
        fx->setOpacity(0.80);
        _preview->setGraphicsEffect(fx);
        _preview->hide();
    }

    void setEnabledCount(int n) { _enabledCount = n; }
    int enabledCount() const { return _enabledCount; }
    int separatorRow() const { return _enabledCount; }

protected:
    bool eventFilter(QObject *obj, QEvent *event) override {
        if (obj != _table->viewport()) return false;

        switch (event->type()) {
        case QEvent::MouseButtonPress:
            return handlePress(static_cast<QMouseEvent*>(event));
        case QEvent::MouseMove:
            return handleMove(static_cast<QMouseEvent*>(event));
        case QEvent::MouseButtonRelease:
            return handleRelease(static_cast<QMouseEvent*>(event));
        default:
            break;
        }
        return false;
    }

private:
    QTableWidget *_table;
    MidiSettingsWidget *_widget;
    QFrame *_dropLine;
    QLabel *_preview;

    bool _dragging = false;
    bool _dragStarted = false;
    int _srcRow = -1;
    int _dropRow = -1;
    QPoint _startPos;
    int _yOffset = 0;
    int _enabledCount = 0;

    static constexpr int THRESHOLD = 5;

    bool handlePress(QMouseEvent *e) {
        if (e->button() != Qt::LeftButton) return false;
        int row = _table->rowAt(e->pos().y());
        int col = _table->columnAt(e->pos().x());
        if (row < 0 || col != 0 || row == separatorRow()) return false;

        // Check that this isn't the separator
        QTableWidgetItem *item = _table->item(row, 0);
        if (!item || item->data(SeparatorRole).toBool()) return false;

        _srcRow = row;
        _startPos = e->pos();
        _dragging = true;
        _dragStarted = false;
        _yOffset = e->pos().y() - _table->rowViewportPosition(row);
        _table->viewport()->setCursor(Qt::ClosedHandCursor);
        return true;
    }

    bool handleMove(QMouseEvent *e) {
        if (!_dragging) {
            // Cursor management on hover
            int col = _table->columnAt(e->pos().x());
            int row = _table->rowAt(e->pos().y());
            if (col == 0 && row >= 0 && row != separatorRow()) {
                _table->viewport()->setCursor(Qt::OpenHandCursor);
            } else {
                _table->viewport()->setCursor(Qt::ArrowCursor);
            }
            return false;
        }

        if (!_dragStarted) {
            if ((e->pos() - _startPos).manhattanLength() < THRESHOLD)
                return true;
            _dragStarted = true;

            // Render row pixmap
            int y = _table->rowViewportPosition(_srcRow);
            int h = _table->rowHeight(_srcRow);
            int w = _table->viewport()->width();
            QPixmap pm = _table->viewport()->grab(QRect(0, y, w, h));
            _preview->setPixmap(pm);
            _preview->setFixedSize(pm.size());
            _preview->show();
            _preview->raise();

            // Deselect to avoid blue highlight
            _table->clearSelection();
        }

        // Position preview
        int previewY = e->pos().y() - _yOffset;
        _preview->move(0, previewY);

        // Compute drop target
        _dropRow = computeDropRow(e->pos());
        showDropIndicator(_dropRow);

        _table->viewport()->setCursor(Qt::ClosedHandCursor);
        return true;
    }

    bool handleRelease(QMouseEvent *e) {
        Q_UNUSED(e);
        if (!_dragging) return false;

        _dropLine->hide();
        _preview->hide();
        _table->viewport()->setCursor(Qt::ArrowCursor);

        if (_dragStarted && _dropRow >= 0 && _dropRow != _srcRow) {
            _widget->reorderSoundFont(_srcRow, _dropRow);
        }

        _dragging = false;
        _dragStarted = false;
        _srcRow = -1;
        _dropRow = -1;
        return true;
    }

    int computeDropRow(const QPoint &pos) {
        int sepRow = separatorRow();
        int totalRows = _table->rowCount();
        int y = pos.y();

        // If below all rows, target is "append to disabled"
        int lastRowBottom = 0;
        if (totalRows > 0) {
            int lr = totalRows - 1;
            lastRowBottom = _table->rowViewportPosition(lr) + _table->rowHeight(lr);
        }
        if (y >= lastRowBottom && totalRows > 0) {
            return totalRows; // past-the-end = disabled section
        }

        // Find closest row boundary
        for (int r = 0; r <= totalRows; ++r) {
            int rowTop;
            if (r < totalRows) {
                rowTop = _table->rowViewportPosition(r);
            } else {
                rowTop = lastRowBottom;
            }
            if (y < rowTop + (_table->rowHeight(qMin(r, totalRows - 1)) / 2)) {
                // Drop before row r
                if (r == sepRow) {
                    // On separator boundary — if source is enabled, keep in enabled
                    return (_srcRow < sepRow) ? sepRow : sepRow + 1;
                }
                return r;
            }
        }
        return totalRows; // fallback: append to disabled
    }

    void showDropIndicator(int atRow) {
        int totalRows = _table->rowCount();
        int indicatorY;

        if (atRow <= 0) {
            indicatorY = _table->rowViewportPosition(0);
        } else if (atRow >= totalRows) {
            int lr = totalRows - 1;
            indicatorY = _table->rowViewportPosition(lr) + _table->rowHeight(lr);
        } else {
            indicatorY = _table->rowViewportPosition(atRow);
        }

        _dropLine->setGeometry(0, indicatorY - 1, _table->viewport()->width(), 2);
        _dropLine->show();
        _dropLine->raise();
    }
};

#endif

AdditionalMidiSettingsWidget::AdditionalMidiSettingsWidget(QSettings *settings, QWidget *parent)
    : SettingsWidget(tr("Additional MIDI Settings"), parent) {
    _settings = settings;

    // Load metronome volume (default 127)
    int metronomeVelocity = _settings->value("metronome_velocity", 127).toInt();
    Metronome::setLoudness(metronomeVelocity);

    QGridLayout *layout = new QGridLayout(this);
    setLayout(layout);

    layout->addWidget(new QLabel(tr("Default ticks per quarter note:"), this), 0, 0, 1, 2);
    _tpqBox = new QSpinBox(this);
    _tpqBox->setMinimum(1);
    _tpqBox->setMaximum(1024);
    _tpqBox->setValue(MidiFile::defaultTimePerQuarter);
    connect(_tpqBox, SIGNAL(valueChanged(int)), this, SLOT(setDefaultTimePerQuarter(int)));
    layout->addWidget(_tpqBox, 0, 2, 1, 4);

    _tpqInfoBox = createInfoBox(tr("Note: There aren't many reasons to change this. MIDI files have a resolution for how many ticks can fit in a quarter note. Higher values = more detail. Lower values may be required for compatibility. Only affects new files."));
    layout->addWidget(_tpqInfoBox, 1, 0, 1, 6);

    layout->addWidget(separator(), 2, 0, 1, 6);

    _alternativePlayerModeBox = new QCheckBox(tr("Manually Stop Notes"), this);
    _alternativePlayerModeBox->setChecked(MidiOutput::isAlternativePlayer);

    connect(_alternativePlayerModeBox, SIGNAL(toggled(bool)), this, SLOT(manualModeToggled(bool)));
    layout->addWidget(_alternativePlayerModeBox, 3, 0, 1, 6);

    _playerModeInfoBox = createInfoBox(tr("Note: the above option should not be enabled in general. It is only required if the stop button does not stop playback as expected (e.g. when some notes are not stopped correctly)."));
    layout->addWidget(_playerModeInfoBox, 4, 0, 1, 6);

    layout->addWidget(separator(), 5, 0, 1, 6);

    _trackBasedProgramChangesBox = new QCheckBox(tr("Track-Based Program Changes (Compatibility Mode)"), this);
    _trackBasedProgramChangesBox->setChecked(_settings->value("track_based_program_changes", false).toBool());
    layout->addWidget(_trackBasedProgramChangesBox, 6, 0, 1, 6);

    _trackModeInfoBox = createInfoBox(tr("When enabled, notes will use the Program Change from their Track, even if it is on a different Channel. This forces standard MIDI synthesizers to simulate track-based instruments. Warning: If multiple tracks share the same channel, their instruments may overwrite each other during playback."));
    layout->addWidget(_trackModeInfoBox, 7, 0, 1, 6);

    layout->addWidget(separator(), 8, 0, 1, 6);

    layout->addWidget(new QLabel(tr("Text Encoding Fallback:"), this), 9, 0, 1, 2);
    _textEncodingCombo = new QComboBox(this);
    _textEncodingCombo->addItem(tr("Auto-Detect (uchardet)"), "Auto-Detect");
    _textEncodingCombo->addItem(tr("System Default"), "System");
    _textEncodingCombo->addItem(tr("UTF-8"), "UTF-8");
    _textEncodingCombo->addItem(tr("Latin-1 (ISO-8859-1)"), "ISO-8859-1");
    _textEncodingCombo->addItem(tr("Shift-JIS (Japanese)"), "Shift-JIS");
    _textEncodingCombo->addItem(tr("GBK (Chinese Simplified)"), "GBK");
    _textEncodingCombo->addItem(tr("Big5 (Chinese Traditional)"), "Big5");
    _textEncodingCombo->addItem(tr("EUC-KR (Korean)"), "EUC-KR");
    _textEncodingCombo->addItem(tr("Windows-1251 (Cyrillic)"), "windows-1251");
    
    QString currentEncoding = _settings->value("text_encoding_fallback", "Auto-Detect").toString();
    int encodingIdx = _textEncodingCombo->findData(currentEncoding);
    if (encodingIdx != -1) _textEncodingCombo->setCurrentIndex(encodingIdx);
    
    layout->addWidget(_textEncodingCombo, 9, 2, 1, 4);

    _textEncodingInfoBox = createInfoBox(tr("When reading MIDI text events, the application will try UTF-8 first. If the text is not valid UTF-8, it will use this fallback encoding. \"Auto-Detect\" will use uchardet to guess the encoding based on character frequency."));
    layout->addWidget(_textEncodingInfoBox, 10, 0, 1, 6);
    
    layout->addWidget(separator(), 11, 0, 1, 6);

    layout->addWidget(new QLabel(tr("Metronome Volume (Velocity):"), this), 12, 0, 1, 2, Qt::AlignVCenter);
    
    QHBoxLayout *metronomeLayout = new QHBoxLayout();
    _metronomeLoudnessSlider = new QSlider(Qt::Horizontal, this);
    _metronomeLoudnessSlider->setMinimum(0);
    _metronomeLoudnessSlider->setMaximum(127);
    _metronomeLoudnessSlider->setValue(Metronome::loudness());
    connect(_metronomeLoudnessSlider, SIGNAL(valueChanged(int)), this, SLOT(setMetronomeVelocity(int)));
    metronomeLayout->addWidget(_metronomeLoudnessSlider, 1);
    _metronomeLoudnessLabel = new QLabel(QString::number(Metronome::loudness()), this);
    _metronomeLoudnessLabel->setFixedWidth(25);
    metronomeLayout->addWidget(_metronomeLoudnessLabel, 0);
    layout->addLayout(metronomeLayout, 12, 2, 1, 4);

    layout->addWidget(new QLabel(tr("Start Command:"), this), 13, 0, 1, 2);
    startCmd = new QLineEdit(this);
    layout->addWidget(startCmd, 13, 2, 1, 4);

    _startCmdInfoBox = createInfoBox(tr("The start command can be used to start additional software components (e.g. MIDI synthesizers) each time, MidiEditor is started. You can see the output of the started software / script in the field below."));
    layout->addWidget(_startCmdInfoBox, 14, 0, 1, 6);

    layout->addWidget(Terminal::terminal()->console(), 15, 0, 1, 6);

    startCmd->setText(_settings->value("start_cmd", "").toString());
    layout->setRowStretch(15, 1);
}

bool AdditionalMidiSettingsWidget::trackBasedProgramChanges() {
    QSettings settings(QString("MidiEditor"), QString("NONE"));
    return settings.value("track_based_program_changes", false).toBool();
}

void AdditionalMidiSettingsWidget::manualModeToggled(bool enable) {
    MidiOutput::isAlternativePlayer = enable;
}

void AdditionalMidiSettingsWidget::setDefaultTimePerQuarter(int value) {
    MidiFile::defaultTimePerQuarter = value;
}

void AdditionalMidiSettingsWidget::setMetronomeVelocity(int value) {
    Metronome::setLoudness(value);
    _settings->setValue("metronome_velocity", value);
    if (_metronomeLoudnessLabel) {
        _metronomeLoudnessLabel->setText(QString::number(value));
    }
}

void AdditionalMidiSettingsWidget::refreshColors() {
    // Update info box colors to match current theme
    auto updateInfoBox = [](QWidget *box) {
        if (box) {
            QLabel *label = qobject_cast<QLabel *>(box);
            if (label) {
                QColor bgColor = Appearance::infoBoxBackgroundColor();
                QColor textColor = Appearance::infoBoxTextColor();
                QString styleSheet = QString("color: rgb(%1, %2, %3); background-color: rgb(%4, %5, %6); padding: 5px")
                        .arg(textColor.red()).arg(textColor.green()).arg(textColor.blue())
                        .arg(bgColor.red()).arg(bgColor.green()).arg(bgColor.blue());
                label->setStyleSheet(styleSheet);
            }
        }
    };

    updateInfoBox(_tpqInfoBox);
    updateInfoBox(_startCmdInfoBox);
    updateInfoBox(_playerModeInfoBox);
    updateInfoBox(_trackModeInfoBox);
    updateInfoBox(_textEncodingInfoBox);

    update();
}

bool AdditionalMidiSettingsWidget::accept() {
    QString text = startCmd->text();
    if (!text.isEmpty()) {
        _settings->setValue("start_cmd", text);
    }
    _settings->setValue("track_based_program_changes", _trackBasedProgramChangesBox->isChecked());
    _settings->setValue("text_encoding_fallback", _textEncodingCombo->currentData().toString());
    return true;
}

MidiSettingsWidget::MidiSettingsWidget(MainWindow *mainWindow, QWidget *parent)
    : SettingsWidget("MIDI I/O", parent), _mainWindow(mainWindow) {
    QGridLayout *layout = new QGridLayout(this);
    setLayout(layout);

    _playerModeInfoBox = createInfoBox(tr("Choose the MIDI ports on your machine to which MidiEditor connects in order to play and record MIDI data."));
    layout->addWidget(_playerModeInfoBox, 0, 0, 1, 6);

    // output
    layout->addWidget(new QLabel(tr("MIDI Output: "), this), 1, 0, 1, 2);
    _outList = new QListWidget(this);
    connect(_outList, SIGNAL(itemChanged(QListWidgetItem*)), this,
            SLOT(outputChanged(QListWidgetItem*)));

    layout->addWidget(_outList, 2, 0, 1, 3);
    QPushButton *reloadOutputList = new QPushButton();
    reloadOutputList->setToolTip(tr("Refresh port list"));
    reloadOutputList->setFlat(true);
    reloadOutputList->setIcon(QIcon(":/run_environment/graphics/tool/refresh.png"));
    reloadOutputList->setFixedSize(30, 30);
    layout->addWidget(reloadOutputList, 1, 2, 1, 1);
    connect(reloadOutputList, SIGNAL(clicked()), this,
            SLOT(reloadOutputPorts()));
    reloadOutputPorts();

    // input
    layout->addWidget(new QLabel(tr("MIDI Input: "), this), 1, 3, 1, 2);
    _inList = new QListWidget(this);
    connect(_inList, SIGNAL(itemChanged(QListWidgetItem*)), this,
            SLOT(inputChanged(QListWidgetItem*)));

    layout->addWidget(_inList, 2, 3, 1, 3);
    QPushButton *reloadInputList = new QPushButton();
    reloadInputList->setFlat(true);
    layout->addWidget(reloadInputList, 1, 5, 1, 1);
    reloadInputList->setToolTip(tr("Refresh port list"));
    reloadInputList->setIcon(QIcon(":/run_environment/graphics/tool/refresh.png"));
    reloadInputList->setFixedSize(30, 30);
    connect(reloadInputList, SIGNAL(clicked()), this,
            SLOT(reloadInputPorts()));
    reloadInputPorts();

#ifdef FLUIDSYNTH_SUPPORT
    // === FluidSynth Settings Section ===
    _fluidSynthSettingsGroup = new QGroupBox(tr("FluidSynth Settings"), this);
    QVBoxLayout *fsLayout = new QVBoxLayout(_fluidSynthSettingsGroup);

    // SoundFont list with management buttons
    QHBoxLayout *sfRow = new QHBoxLayout();
    _soundFontList = new QTableWidget(_fluidSynthSettingsGroup);
    _soundFontList->setMinimumHeight(160);
    _soundFontList->setColumnCount(4);
    _soundFontList->setHorizontalHeaderLabels({"", tr("Enabled"), tr("Priority"), tr("SoundFont")});
    _soundFontList->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    _soundFontList->setColumnWidth(0, 30);
    _soundFontList->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    _soundFontList->setColumnWidth(1, 50);
    _soundFontList->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    _soundFontList->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    _soundFontList->horizontalHeader()->setHighlightSections(false);
    _soundFontList->setSelectionBehavior(QAbstractItemView::SelectRows);
    _soundFontList->setSelectionMode(QAbstractItemView::SingleSelection);
    _soundFontList->verticalHeader()->setVisible(false);
    // No built-in drag-drop — we use SoundFontDragController for manual reordering
    _soundFontList->setDragEnabled(false);
    _soundFontList->setAcceptDrops(false);
    _soundFontList->setDragDropMode(QAbstractItemView::NoDragDrop);

    // Custom delegate for separator row painting
    _separatorDelegate = new SoundFontSeparatorDelegate(_soundFontList);
    _soundFontList->setItemDelegate(_separatorDelegate);

    // Manual drag controller (event filter on viewport)
    _separatorRow = 0;
    _dragController = new SoundFontDragController(_soundFontList, this);

    sfRow->addWidget(_soundFontList, 1);

    QVBoxLayout *sfBtnCol = new QVBoxLayout();
    _addSoundFontBtn = new QPushButton(tr("Add..."), _fluidSynthSettingsGroup);
    _removeSoundFontBtn = new QPushButton(tr("Remove"), _fluidSynthSettingsGroup);
    _moveSoundFontUpBtn = new QPushButton(tr("Up"), _fluidSynthSettingsGroup);
    _moveSoundFontDownBtn = new QPushButton(tr("Down"), _fluidSynthSettingsGroup);
    _downloadDefaultSoundFontBtn = new QPushButton(tr("Download..."), _fluidSynthSettingsGroup);
    sfBtnCol->addWidget(_addSoundFontBtn);
    sfBtnCol->addWidget(_removeSoundFontBtn);
    sfBtnCol->addWidget(_moveSoundFontUpBtn);
    sfBtnCol->addWidget(_moveSoundFontDownBtn);
    sfBtnCol->addWidget(_downloadDefaultSoundFontBtn);
    
    _exportWavBtn = new QPushButton(tr("Export MIDI"), _fluidSynthSettingsGroup);
    _exportWavBtn->setToolTip(tr("Export current workspace to WAV"));

    QFontMetrics fm(_exportWavBtn->font());
    int minWidth = qMax(fm.horizontalAdvance(tr("Export MIDI")), fm.horizontalAdvance(tr("Exporting..."))) + 30;
    _exportWavBtn->setMinimumWidth(minWidth);
    
    sfBtnCol->addWidget(_exportWavBtn);
    connect(_exportWavBtn, SIGNAL(clicked()), this, SLOT(onExportToWav()));

    sfBtnCol->addStretch();
    sfRow->addLayout(sfBtnCol);
    fsLayout->addLayout(sfRow);

    connect(_addSoundFontBtn, SIGNAL(clicked()), this, SLOT(addSoundFont()));
    connect(_removeSoundFontBtn, SIGNAL(clicked()), this, SLOT(removeSoundFont()));
    connect(_moveSoundFontUpBtn, SIGNAL(clicked()), this, SLOT(moveSoundFontUp()));
    connect(_moveSoundFontDownBtn, SIGNAL(clicked()), this, SLOT(moveSoundFontDown()));
    connect(_downloadDefaultSoundFontBtn, SIGNAL(clicked()), this, SLOT(showDownloadSoundFontDialog()));

    // Settings grid: 3 rows x 5 columns
    FluidSynthEngine *engine = FluidSynthEngine::instance();
    QGridLayout *settingsGrid = new QGridLayout();
    settingsGrid->setSpacing(0);
    settingsGrid->setVerticalSpacing(8);
    settingsGrid->setHorizontalSpacing(10);
    settingsGrid->setContentsMargins(0, 0, 0, 0);

    // Column Stretch: Labels (0, 3) fixed, Widgets (1, 4) stretch
    settingsGrid->setColumnStretch(0, 0);
    settingsGrid->setColumnStretch(1, 1);
    settingsGrid->setColumnStretch(2, 0); // Spacer column / Reset button
    settingsGrid->setColumnMinimumWidth(2, 20); 
    settingsGrid->setColumnStretch(3, 0);
    settingsGrid->setColumnStretch(4, 1);

    // ========================================================================
    // ROW 0
    // ========================================================================

    // (0,0): Audio Driver Label
    settingsGrid->addWidget(new QLabel(tr("Audio Driver:"), _fluidSynthSettingsGroup), 0, 0);

    // (0,1): Audio Driver Combo (Stretches)
    _audioDriverCombo = new QComboBox(_fluidSynthSettingsGroup);
    for (const QString &driver : engine->availableAudioDrivers()) {
        _audioDriverCombo->addItem(FluidSynthEngine::audioDriverDisplayName(driver), driver);
    }
    if (!engine->audioDriver().isEmpty()) {
        int driverIdx = _audioDriverCombo->findData(engine->audioDriver());
        if (driverIdx != -1) _audioDriverCombo->setCurrentIndex(driverIdx);
    }
    _audioDriverCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    settingsGrid->addWidget(_audioDriverCombo, 0, 1);

    // (0,3): Reverb Engine Label
    settingsGrid->addWidget(new QLabel(tr("Reverb Engine:"), _fluidSynthSettingsGroup), 0, 3);

    // (0,4): Reverb Engine Combo (Stretch 1)
    _reverbEngineCombo = new QComboBox(_fluidSynthSettingsGroup);
    _reverbEngineCombo->addItem(tr("FDN Reverb"), "fdn");
    _reverbEngineCombo->addItem(tr("Freeverb"), "free");
    _reverbEngineCombo->addItem(tr("LEXverb"), "lex");
    _reverbEngineCombo->addItem(tr("Dattorro Reverb"), "dat");
    int rEngIdx = _reverbEngineCombo->findData(engine->reverbEngine());
    if (rEngIdx != -1) _reverbEngineCombo->setCurrentIndex(rEngIdx);
    _reverbEngineCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    settingsGrid->addWidget(_reverbEngineCombo, 0, 4);

    // ========================================================================
    // ROW 1
    // ========================================================================

    // (1,0): Sample Rate Label
    settingsGrid->addWidget(new QLabel(tr("Sample Rate:"), _fluidSynthSettingsGroup), 1, 0);

    // (1,1): Sample Rate (Stretch 1) + Format (Stretch 0)
    QHBoxLayout *row1LeftLayout = new QHBoxLayout();
    row1LeftLayout->setContentsMargins(0, 0, 0, 0);
    row1LeftLayout->setSpacing(8);
    _sampleRateCombo = new QComboBox(_fluidSynthSettingsGroup);
    _sampleRateCombo->addItems({"44100 Hz", "48000 Hz", "88200 Hz", "96000 Hz", "176400 Hz", "192000 Hz"});
    QString rateStr = QString::number(static_cast<int>(engine->sampleRate())) + " Hz";
    if (_sampleRateCombo->findText(rateStr) != -1) _sampleRateCombo->setCurrentText(rateStr);
    else _sampleRateCombo->setCurrentText("48000 Hz");
    row1LeftLayout->addWidget(_sampleRateCombo, 1);

    _sampleFormatCombo = new QComboBox(_fluidSynthSettingsGroup);
    _sampleFormatCombo->addItem("16 bit", "16bits");
    _sampleFormatCombo->addItem("Float", "float");
    int formatIdx = _sampleFormatCombo->findData(engine->sampleFormat());
    if (formatIdx != -1) _sampleFormatCombo->setCurrentIndex(formatIdx);
    row1LeftLayout->addWidget(new QLabel(tr("Format:"), _fluidSynthSettingsGroup));
    row1LeftLayout->addWidget(_sampleFormatCombo, 0);
    settingsGrid->addLayout(row1LeftLayout, 1, 1);

    // (1,4) [ ] Reverb [ ] Chorus
    QHBoxLayout *row1RightLayout = new QHBoxLayout();
    row1RightLayout->setContentsMargins(0, 0, 0, 0);
    row1RightLayout->setSpacing(8);
    _reverbCheckBox = new QCheckBox(tr("Reverb"), _fluidSynthSettingsGroup);
    _reverbCheckBox->setChecked(engine->reverbEnabled());
    row1RightLayout->addWidget(_reverbCheckBox, 0);
    
    _chorusCheckBox = new QCheckBox(tr("Chorus"), _fluidSynthSettingsGroup);
    _chorusCheckBox->setChecked(engine->chorusEnabled());
    row1RightLayout->addWidget(_chorusCheckBox, 0);
    
    row1RightLayout->addStretch(1);
    settingsGrid->addLayout(row1RightLayout, 1, 4);
    
    // (2,4): Polyphony: [_poly_]
    QHBoxLayout *row2RightLayout = new QHBoxLayout();
    row2RightLayout->setContentsMargins(0, 0, 0, 0);
    row2RightLayout->setSpacing(8);
    
    QLabel *polyLabel = new QLabel(tr("Polyphony:"), _fluidSynthSettingsGroup);
    row2RightLayout->addWidget(polyLabel, 0);
    
    _polyphonyCombo = new QComboBox(_fluidSynthSettingsGroup);
    QStringList polyPresets = {"8", "16", "32", "64", "128", "256"};
    _polyphonyCombo->addItems(polyPresets);
    _polyphonyCombo->addItem(tr("Custom"));
    
    int currentPoly = engine->polyphony();
    int polyIdx = _polyphonyCombo->findText(QString::number(currentPoly));
    if (polyIdx != -1) {
        _polyphonyCombo->setCurrentIndex(polyIdx);
    } else {
        // It's a custom value: rename the 'Custom' item and select it
        int lastIdx = _polyphonyCombo->count() - 1;
        _polyphonyCombo->setItemText(lastIdx, QString::number(currentPoly));
        _polyphonyCombo->setCurrentIndex(lastIdx);
    }
    _polyphonyCombo->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    _polyphonyCombo->setMinimumWidth(80);
    row2RightLayout->addWidget(_polyphonyCombo, 0);
    
    row2RightLayout->addStretch(1);
    settingsGrid->addLayout(row2RightLayout, 2, 4);

    // ========================================================================
    // ROW 2
    // ========================================================================

    // (2,0): Gain Label (Right Aligned)
    settingsGrid->addWidget(new QLabel(tr("Gain:"), _fluidSynthSettingsGroup), 2, 0, Qt::AlignRight | Qt::AlignVCenter);

    // (2,1): Gain Slider + Value label
    QHBoxLayout *gainLayout = new QHBoxLayout();
    gainLayout->setContentsMargins(0, 0, 0, 0);
    gainLayout->setSpacing(10);
    _gainSlider = new QSlider(Qt::Horizontal, _fluidSynthSettingsGroup);
    _gainSlider->setMinimum(0);
    _gainSlider->setMaximum(300);
    _gainSlider->setSingleStep(5);
    _gainSlider->setPageStep(5);
    _gainSlider->setValue(static_cast<int>(engine->gain() * 100.0));
    gainLayout->addWidget(_gainSlider, 1);

    _gainValueLabel = new QLabel(QString::number(engine->gain(), 'f', 2), _fluidSynthSettingsGroup);
    _gainValueLabel->setFixedWidth(35);
    gainLayout->addWidget(_gainValueLabel, 0);
    settingsGrid->addLayout(gainLayout, 2, 1);

    // (2,2): Gain Reset (Center column)
    _gainResetBtn = new QPushButton(tr("Reset"), _fluidSynthSettingsGroup);
    _gainResetBtn->setFixedWidth(45);
    settingsGrid->addWidget(_gainResetBtn, 2, 2, Qt::AlignCenter);

    // ========================================================================
    // Connections
    // ========================================================================
    connect(_audioDriverCombo, SIGNAL(activated(int)), this, SLOT(onAudioDriverChanged(int)));
    connect(_sampleRateCombo, SIGNAL(currentTextChanged(QString)), this, SLOT(onSampleRateChanged(QString)));
    connect(_sampleFormatCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(onSampleFormatChanged(int)));
    connect(_reverbEngineCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(onReverbEngineChanged(int)));
    connect(_polyphonyCombo, SIGNAL(activated(int)), this, SLOT(onPolyphonyChanged(int)));
    connect(_gainSlider, SIGNAL(valueChanged(int)), this, SLOT(onGainChanged(int)));
    connect(_gainResetBtn, SIGNAL(clicked()), this, SLOT(onGainReset()));
    connect(_reverbCheckBox, SIGNAL(toggled(bool)), this, SLOT(onReverbToggled(bool)));
    connect(_chorusCheckBox, SIGNAL(toggled(bool)), this, SLOT(onChorusToggled(bool)));

    fsLayout->addLayout(settingsGrid);

    layout->addWidget(_fluidSynthSettingsGroup, 3, 0, 1, 6);

    // Populate SoundFont list from engine
    refreshSoundFontList();
    connect(engine, SIGNAL(soundFontsChanged()), this, SLOT(refreshSoundFontList()));
    connect(engine, SIGNAL(engineRestarted()), this, SLOT(refreshSoundFontList()));

    // Set initial enabled state
    updateFluidSynthSettingsEnabled();
#endif
}

void MidiSettingsWidget::reloadInputPorts() {
    disconnect(_inList, SIGNAL(itemChanged(QListWidgetItem*)), this,
               SLOT(inputChanged(QListWidgetItem*)));

    // clear the list
    _inList->clear();

    foreach(QString name, MidiInput::inputPorts()) {
        QListWidgetItem *item = new QListWidgetItem(name, _inList,
                                                    QListWidgetItem::UserType);
        item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);

        if (name == MidiInput::inputPort()) {
            item->setCheckState(Qt::Checked);
        } else {
            item->setCheckState(Qt::Unchecked);
        }
        _inList->addItem(item);
    }
    connect(_inList, SIGNAL(itemChanged(QListWidgetItem*)), this,
            SLOT(inputChanged(QListWidgetItem*)));
}

void MidiSettingsWidget::reloadOutputPorts() {
    disconnect(_outList, SIGNAL(itemChanged(QListWidgetItem*)), this,
               SLOT(outputChanged(QListWidgetItem*)));

    // clear the list
    _outList->clear();

    foreach(QString name, MidiOutput::outputPorts()) {
        QListWidgetItem *item = new QListWidgetItem(name, _outList,
                                                    QListWidgetItem::UserType);
        item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);

        if (name == MidiOutput::outputPort()) {
            item->setCheckState(Qt::Checked);
        } else {
            item->setCheckState(Qt::Unchecked);
        }
        _outList->addItem(item);
    }
    connect(_outList, SIGNAL(itemChanged(QListWidgetItem*)), this,
            SLOT(outputChanged(QListWidgetItem*)));
}

void MidiSettingsWidget::inputChanged(QListWidgetItem *item) {
    if (item->checkState() == Qt::Checked) {
        MidiInput::setInputPort(item->text());

        reloadInputPorts();
    }
}

void MidiSettingsWidget::outputChanged(QListWidgetItem *item) {
    if (item->checkState() == Qt::Checked) {
        MidiOutput::setOutputPort(item->text());

        reloadOutputPorts();

#ifdef FLUIDSYNTH_SUPPORT
        updateFluidSynthSettingsEnabled();
#endif
        _mainWindow->updateAll();
    }
}

void MidiSettingsWidget::refreshColors() {
    // Update info box colors to match current theme
    if (_playerModeInfoBox) {
        QLabel *label = qobject_cast<QLabel *>(_playerModeInfoBox);
        if (label) {
            QColor bgColor = Appearance::infoBoxBackgroundColor();
            QColor textColor = Appearance::infoBoxTextColor();
            QString styleSheet = QString("color: rgb(%1, %2, %3); background-color: rgb(%4, %5, %6); padding: 5px")
                    .arg(textColor.red()).arg(textColor.green()).arg(textColor.blue())
                    .arg(bgColor.red()).arg(bgColor.green()).arg(bgColor.blue());
            label->setStyleSheet(styleSheet);
        }
    }

    update();
}

#ifdef FLUIDSYNTH_SUPPORT
void MidiSettingsWidget::updateFluidSynthSettingsEnabled() {
    bool fsOutput = MidiOutput::isFluidSynthOutput();
    _fluidSynthSettingsGroup->setEnabled(fsOutput);
}

void MidiSettingsWidget::addSoundFont() {
    // Default to the soundfonts directory next to the application
    QString sfDir = QCoreApplication::applicationDirPath();
    QDir dir(sfDir);
    if (!dir.exists("soundfonts")) {
        dir.mkpath("soundfonts");
    }
    dir.cd("soundfonts");
    sfDir = dir.absolutePath();

    QStringList files = QFileDialog::getOpenFileNames(
        this,
        tr("Select SoundFont Files"),
        sfDir,
        tr("SoundFont Files (*.sf2 *.sf3 *.dls);;All Files (*)")
    );

    FluidSynthEngine *engine = FluidSynthEngine::instance();
    engine->addSoundFonts(files);
}

void MidiSettingsWidget::removeSoundFont() {
    int row = _soundFontList->currentRow();
    if (row < 0 || row == _separatorRow) return;
    QTableWidgetItem *nameItem = _soundFontList->item(row, 3);
    if (!nameItem) return;
    
    QString path = nameItem->data(Qt::UserRole).toString();
    FluidSynthEngine *engine = FluidSynthEngine::instance();
    QList<QPair<QString, bool>> collection = engine->soundFontCollection();
    
    // Remove matching entry from collection
    for (int i = 0; i < collection.size(); ++i) {
        if (collection[i].first == path) {
            collection.removeAt(i);
            break;
        }
    }
    engine->setSoundFontCollection(collection);
    refreshSoundFontList();
}

void MidiSettingsWidget::moveSoundFontUp() {
    int row = _soundFontList->currentRow();
    if (row <= 0 || row >= _separatorRow) return; // only enabled rows, not first

    // Work through the collection directly
    FluidSynthEngine *engine = FluidSynthEngine::instance();
    QList<QPair<QString, bool>> collection = engine->soundFontCollection();

    // Build enabled sub-list
    QList<QPair<QString, bool>> enabled, disabled;
    for (const auto &pair : collection) {
        if (pair.second) enabled.append(pair);
        else disabled.append(pair);
    }

    if (row <= 0 || row >= enabled.size()) return;
    enabled.swapItemsAt(row, row - 1);

    QList<QPair<QString, bool>> newCollection;
    newCollection.append(enabled);
    newCollection.append(disabled);
    engine->setSoundFontCollection(newCollection);
    refreshSoundFontList();

    _soundFontList->selectRow(row - 1);
    _soundFontList->setCurrentCell(row - 1, 0);
}

void MidiSettingsWidget::moveSoundFontDown() {
    int row = _soundFontList->currentRow();
    if (row < 0 || row >= _separatorRow - 1) return; // only enabled rows, not last

    FluidSynthEngine *engine = FluidSynthEngine::instance();
    QList<QPair<QString, bool>> collection = engine->soundFontCollection();

    QList<QPair<QString, bool>> enabled, disabled;
    for (const auto &pair : collection) {
        if (pair.second) enabled.append(pair);
        else disabled.append(pair);
    }

    if (row < 0 || row >= enabled.size() - 1) return;
    enabled.swapItemsAt(row, row + 1);

    QList<QPair<QString, bool>> newCollection;
    newCollection.append(enabled);
    newCollection.append(disabled);
    engine->setSoundFontCollection(newCollection);
    refreshSoundFontList();
    _soundFontList->selectRow(row + 1);
    _soundFontList->setCurrentCell(row + 1, 0);
}

void MidiSettingsWidget::onExportToWav() {
    if (!_mainWindow) return;
    MidiFile *file = _mainWindow->getFile();
    if (!file) return;

    // Delegate to the comprehensive AudioExportDialog
    AudioExportDialog *dialog = new AudioExportDialog(file, _mainWindow);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setModal(true);
    dialog->exec();
}

void MidiSettingsWidget::onExportProgress(int percent) {
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    
    double progress = percent / 100.0;
    // 0.001 offset creates a sharp edge for the progress bar look
    double stop2 = qMin(1.0, progress + 0.001);

    // Provide proper start (0) and end (1) anchors for the gradient to work
    QString style = QString(
        "QPushButton:disabled {"
        "  border: 1px solid #999; border-radius: 4px; padding: 4px;"
        "  color: #111;"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:0, "
        "  stop:0 #4CAF50, stop:%1 #4CAF50, "
        "  stop:%2 #E0E0E0, stop:1 #E0E0E0);"
        "}"
    ).arg(progress).arg(stop2);
    
    _exportWavBtn->setStyleSheet(style);
}

void MidiSettingsWidget::onExportFinished(bool success, const QString &path) {
    _exportWavBtn->setEnabled(true);
    _exportWavBtn->setText(tr("Export MIDI"));
    
    // Clearing the stylesheet completely returns the button to its native state
    _exportWavBtn->setStyleSheet("");

    QFile::remove(QDir::tempPath() + "/MidiEditor_fastrender_" + QString::number(QCoreApplication::applicationPid()) + ".mid");
}

void MidiSettingsWidget::onSoundFontToggled() {
    QCheckBox *cbTriggered = qobject_cast<QCheckBox*>(sender());
    if (!cbTriggered) return;
    
    QList<QPair<QString, bool>> newCollection;
    QString newlyEnabledPath;
    
    for (int r = 0; r < _soundFontList->rowCount(); ++r) {
        if (r == _separatorRow) continue; // skip separator
        
        QTableWidgetItem *nameItem = _soundFontList->item(r, 3);
        if (!nameItem) continue;
        
        QString path = nameItem->data(Qt::UserRole).toString();
        bool enabled = (r < _separatorRow);
        
        QWidget* cbWidget = _soundFontList->cellWidget(r, 1);
        if (cbWidget) {
            QCheckBox* cb = cbWidget->findChild<QCheckBox*>();
            if (cb == cbTriggered) {
                enabled = cb->isChecked(); 
                if (enabled && r > _separatorRow) {
                    newlyEnabledPath = path;
                }
            }
        }
        
        if (path == newlyEnabledPath) {
            newCollection.prepend(qMakePair(path, true));
        } else {
            newCollection.append(qMakePair(path, enabled));
        }
    }
    FluidSynthEngine::instance()->setSoundFontCollection(newCollection);
    refreshSoundFontList();
}

void MidiSettingsWidget::onSoundFontTableDropped() {
    // Rebuild collection from current table state (used by removeSoundFont)
    QList<QPair<QString, bool>> newCollection;
    
    for (int r = 0; r < _soundFontList->rowCount(); ++r) {
        if (r == _separatorRow) continue;
        
        QTableWidgetItem *nameItem = _soundFontList->item(r, 3);
        if (!nameItem) continue;
        
        QString path = nameItem->data(Qt::UserRole).toString();
        bool enabled = (r < _separatorRow);
        newCollection.append(qMakePair(path, enabled));
    }
    FluidSynthEngine::instance()->setSoundFontCollection(newCollection);
    refreshSoundFontList();
}

void MidiSettingsWidget::reorderSoundFont(int fromRow, int toRow) {
    int sepRow = _separatorRow;
    
    FluidSynthEngine *engine = FluidSynthEngine::instance();
    QList<QPair<QString, bool>> collection = engine->soundFontCollection();
    
    // Split into enabled and disabled
    QList<QPair<QString, bool>> enabled, disabled;
    for (const auto &pair : collection) {
        if (pair.second) enabled.append(pair);
        else disabled.append(pair);
    }
    
    bool fromEnabled = (fromRow < sepRow);
    bool toEnabled = (toRow < sepRow);
    
    if (fromEnabled && toEnabled) {
        // Reorder within enabled section
        if (fromRow < 0 || fromRow >= enabled.size()) return;
        auto item = enabled.takeAt(fromRow);
        int insertAt = (toRow > fromRow) ? toRow - 1 : toRow;
        if (insertAt < 0) insertAt = 0;
        if (insertAt > enabled.size()) insertAt = enabled.size();
        enabled.insert(insertAt, item);
    } else if (fromEnabled && !toEnabled) {
        // Move enabled → disabled
        if (fromRow < 0 || fromRow >= enabled.size()) return;
        auto item = enabled.takeAt(fromRow);
        item.second = false;
        disabled.append(item);
    } else if (!fromEnabled && toEnabled) {
        // Move disabled → enabled
        int disIdx = fromRow - sepRow - 1;
        if (disIdx < 0 || disIdx >= disabled.size()) return;
        auto item = disabled.takeAt(disIdx);
        item.second = true;
        int insertAt = toRow;
        if (insertAt < 0) insertAt = 0;
        if (insertAt > enabled.size()) insertAt = enabled.size();
        enabled.insert(insertAt, item);
    }
    // disabled→disabled is a no-op (always sorted alphabetically)
    
    QList<QPair<QString, bool>> newCollection;
    newCollection.append(enabled);
    newCollection.append(disabled);
    engine->setSoundFontCollection(newCollection);
    refreshSoundFontList();
}

void MidiSettingsWidget::onAudioDriverChanged(int index) {
    QString driver = _audioDriverCombo->itemData(index).toString();
    FluidSynthEngine::instance()->setAudioDriver(driver);
    updateFluidSynthSettingsEnabled();
}

void MidiSettingsWidget::onSampleFormatChanged(int index) {
    QString format = _sampleFormatCombo->itemData(index).toString();
    FluidSynthEngine::instance()->setSampleFormat(format);
}

void MidiSettingsWidget::onPolyphonyChanged(int index) {
    FluidSynthEngine *engine = FluidSynthEngine::instance();
    int lastIdx = _polyphonyCombo->count() - 1;
    QStringList presets = {"8", "16", "32", "64", "128", "256"};
    
    if (index == lastIdx) {
        // Custom trigger or current custom value selected
        bool ok;
        int val = QInputDialog::getInt(this, tr("Polyphony"),
                                      tr("Enter custom polyphony (Limit: 1-65535).\nWarning: High polyphony may cause CPU overload."),
                                      engine->polyphony(), 1, 65535, 1, &ok);
        if (ok) {
            engine->setPolyphony(val);
            // If the new value happens to be a preset, switch to that preset and reset "Custom" text
            int presetIdx = _polyphonyCombo->findText(QString::number(val));
            if (presetIdx != -1 && presetIdx < lastIdx) {
                _polyphonyCombo->setItemText(lastIdx, tr("Custom"));
                _polyphonyCombo->setCurrentIndex(presetIdx);
            } else {
                // Keep it as the custom item and show the number
                _polyphonyCombo->setItemText(lastIdx, QString::number(val));
            }
        } else {
            // Cancelled: Ensure combo shows current engine state faithfully
            int currentIdx = _polyphonyCombo->findText(QString::number(engine->polyphony()));
            if (currentIdx != -1) {
                _polyphonyCombo->setCurrentIndex(currentIdx);
            } else {
                // If it was already a custom value, the item at lastIdx already has the right text
                _polyphonyCombo->setCurrentIndex(lastIdx);
            }
        }
    } else {
        // Standard preset selected: Reset the custom item text for next time
        QString text = _polyphonyCombo->itemText(index);
        engine->setPolyphony(text.toInt());
        _polyphonyCombo->setItemText(lastIdx, tr("Custom"));
    }
}

void MidiSettingsWidget::onGainChanged(int value) {
    // Snap to nearest 5 (0.05 step units) to ensure consistent behavior during drag
    int snappedValue = (value + 2) / 5 * 5;
    if (snappedValue != value) {
        _gainSlider->blockSignals(true);
        _gainSlider->setValue(snappedValue);
        _gainSlider->blockSignals(false);
        value = snappedValue;
    }
    
    double gain = value / 100.0;
    _gainValueLabel->setText(QString::number(gain, 'f', 2));
    FluidSynthEngine::instance()->setGain(gain);
}

void MidiSettingsWidget::onGainReset() {
    _gainSlider->setValue(50); // 0.50 default
}

void MidiSettingsWidget::onSampleRateChanged(const QString &rate) {
    QString rateStr = rate;
    rateStr.remove(" Hz");
    FluidSynthEngine::instance()->setSampleRate(rateStr.toDouble());
}

void MidiSettingsWidget::onReverbEngineChanged(int index) {
    QString engine = _reverbEngineCombo->itemData(index).toString();
    FluidSynthEngine::instance()->setReverbEngine(engine);
}

void MidiSettingsWidget::onReverbToggled(bool enabled) {
    FluidSynthEngine::instance()->setReverbEnabled(enabled);
}

void MidiSettingsWidget::onChorusToggled(bool enabled) {
    FluidSynthEngine::instance()->setChorusEnabled(enabled);
}

void MidiSettingsWidget::refreshSoundFontList() {
    // Remember selection
    QString selectedPath = "";
    int currentRow = _soundFontList->currentRow();
    if (currentRow >= 0 && currentRow != _separatorRow) {
        QTableWidgetItem *nameItem = _soundFontList->item(currentRow, 3);
        if (nameItem) {
            selectedPath = nameItem->data(Qt::UserRole).toString();
        }
    }

    _soundFontList->setRowCount(0);
    FluidSynthEngine *engine = FluidSynthEngine::instance();
    QList<QPair<QString, bool>> collection = engine->soundFontCollection();

    QList<QPair<QString, bool>> enabledList;
    QList<QPair<QString, bool>> disabledList;
    for (int i = 0; i < collection.size(); ++i) {
        if (collection[i].second) {
            enabledList.append(collection[i]);
        } else {
            disabledList.append(collection[i]);
        }
    }

    // Sort disabled by filename (case-insensitive), not by full path
    std::sort(disabledList.begin(), disabledList.end(),
              [](const QPair<QString, bool> &a, const QPair<QString, bool> &b) {
        return QFileInfo(a.first).fileName().compare(
                   QFileInfo(b.first).fileName(), Qt::CaseInsensitive) < 0;
    });

    // Always show separator: enabled + 1 separator + disabled
    int totalRows = enabledList.size() + 1 + disabledList.size();
    _soundFontList->setRowCount(totalRows);

    auto addRow = [&](int r, const QString& path, bool enabled, int priority) {
        // Grip: braille pattern ⠿ (2×3 dot grid) for a thicker drag handle
        QTableWidgetItem *gripItem = new QTableWidgetItem(QString::fromUtf8("\xE2\xA0\xBF"));
        gripItem->setTextAlignment(Qt::AlignCenter);
        gripItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);

        QWidget *cbWidget = new QWidget();
        cbWidget->setCursor(Qt::ArrowCursor);
        QHBoxLayout *cbLayout = new QHBoxLayout(cbWidget);
        QCheckBox *cb = new QCheckBox();
        cb->setCursor(Qt::ArrowCursor);
        cb->setChecked(enabled);
        cbLayout->addWidget(cb);
        cbLayout->setAlignment(Qt::AlignCenter);
        cbLayout->setContentsMargins(0,0,0,0);
        connect(cb, &QCheckBox::clicked, this, &MidiSettingsWidget::onSoundFontToggled);

        QTableWidgetItem *cbHiddenItem = new QTableWidgetItem();

        QTableWidgetItem *prioItem = new QTableWidgetItem(enabled ? QString::number(priority) : "-");
        prioItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        prioItem->setTextAlignment(Qt::AlignCenter);

        QFileInfo fi(path);
        bool exists = fi.exists();

        QTableWidgetItem *nameItem = new QTableWidgetItem(fi.fileName());
        nameItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        nameItem->setData(Qt::UserRole, path);

        if (!exists) {
            nameItem->setForeground(QBrush(Qt::red));
            nameItem->setToolTip(tr("File not found: ") + path);
            prioItem->setForeground(QBrush(Qt::red));
        } else {
            nameItem->setToolTip(path);
        }

        _soundFontList->setItem(r, 0, gripItem);
        _soundFontList->setCellWidget(r, 1, cbWidget);
        _soundFontList->setItem(r, 1, cbHiddenItem);
        _soundFontList->setItem(r, 2, prioItem);
        _soundFontList->setItem(r, 3, nameItem);

        if (path == selectedPath) {
            _soundFontList->selectRow(r);
            _soundFontList->setCurrentCell(r, 0);
        }
    };

    int rowIdx = 0;
    int priority = 1;

    // Enabled rows
    for (int i = 0; i < enabledList.size(); ++i) {
        addRow(rowIdx++, enabledList[i].first, true, priority++);
    }

    // Separator row — always present, painted by SoundFontSeparatorDelegate
    _separatorRow = rowIdx;
    {
        QTableWidgetItem *sepItem = new QTableWidgetItem();
        sepItem->setFlags(Qt::NoItemFlags);
        sepItem->setData(SeparatorRole, true);
        _soundFontList->setItem(rowIdx, 0, sepItem);
        for (int col = 1; col < 4; ++col) {
            QTableWidgetItem *emptySep = new QTableWidgetItem();
            emptySep->setFlags(Qt::NoItemFlags);
            _soundFontList->setItem(rowIdx, col, emptySep);
        }
        _soundFontList->setSpan(rowIdx, 0, 1, 4);
        _soundFontList->setRowHeight(rowIdx, 22);
        rowIdx++;
    }

    // Disabled rows (sorted by filename)
    for (int i = 0; i < disabledList.size(); ++i) {
        addRow(rowIdx++, disabledList[i].first, false, 0);
    }

    // Update drag controller
    _dragController->setEnabledCount(enabledList.size());
}

void MidiSettingsWidget::showDownloadSoundFontDialog() {
    DownloadSoundFontDialog *dialog = new DownloadSoundFontDialog(this);
    connect(dialog, &DownloadSoundFontDialog::soundFontDownloaded, this, [this](const QString &path) {
        FluidSynthEngine::instance()->addSoundFonts(QStringList() << path);
    });
    dialog->exec();
    dialog->deleteLater();
}

#endif

