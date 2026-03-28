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
#include "Appearance.h"

#include "../Terminal.h"
#include "../midi/MidiFile.h"
#include "../midi/MidiInput.h"
#include "../midi/MidiOutput.h"
#include "../midi/Metronome.h"
#include <QCheckBox>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidgetItem>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QTextEdit>

#ifdef FLUIDSYNTH_SUPPORT
#include <QComboBox>
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
    : SettingsWidget(tr("Additional Midi Settings"), parent) {
    _settings = settings;

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

    _alternativePlayerModeBox = new QCheckBox(tr("Manually stop notes"), this);
    _alternativePlayerModeBox->setChecked(MidiOutput::isAlternativePlayer);

    connect(_alternativePlayerModeBox, SIGNAL(toggled(bool)), this, SLOT(manualModeToggled(bool)));
    layout->addWidget(_alternativePlayerModeBox, 3, 0, 1, 6);

    _playerModeInfoBox = createInfoBox(tr("Note: the above option should not be enabled in general. It is only required if the stop button does not stop playback as expected (e.g. when some notes are not stopped correctly)."));
    layout->addWidget(_playerModeInfoBox, 4, 0, 1, 6);

    layout->addWidget(separator(), 5, 0, 1, 6);

    layout->addWidget(new QLabel(tr("Metronome loudness:"), this), 6, 0, 1, 2);
    _metronomeLoudnessBox = new QSpinBox(this);
    _metronomeLoudnessBox->setMinimum(10);
    _metronomeLoudnessBox->setMaximum(100);
    _metronomeLoudnessBox->setValue(Metronome::loudness());
    connect(_metronomeLoudnessBox, SIGNAL(valueChanged(int)), this, SLOT(setMetronomeLoudness(int)));
    layout->addWidget(_metronomeLoudnessBox, 6, 2, 1, 4);

    layout->addWidget(separator(), 7, 0, 1, 6);

    layout->addWidget(new QLabel(tr("Start command:"), this), 8, 0, 1, 2);
    startCmd = new QLineEdit(this);
    layout->addWidget(startCmd, 8, 2, 1, 4);

    _startCmdInfoBox = createInfoBox(tr("The start command can be used to start additional software components (e.g. Midi synthesizers) each time, MidiEditor is started. You can see the output of the started software / script in the field below."));
    layout->addWidget(_startCmdInfoBox, 9, 0, 1, 6);

    layout->addWidget(Terminal::terminal()->console(), 10, 0, 1, 6);

    startCmd->setText(_settings->value("start_cmd", "").toString());
    layout->setRowStretch(3, 1);
}

void AdditionalMidiSettingsWidget::manualModeToggled(bool enable) {
    MidiOutput::isAlternativePlayer = enable;
}

void AdditionalMidiSettingsWidget::setDefaultTimePerQuarter(int value) {
    MidiFile::defaultTimePerQuarter = value;
}

void AdditionalMidiSettingsWidget::setMetronomeLoudness(int value) {
    Metronome::setLoudness(value);
}

void AdditionalMidiSettingsWidget::refreshColors() {
    // Update info box colors to match current theme
    if (_tpqInfoBox) {
        QLabel *label = qobject_cast<QLabel *>(_tpqInfoBox);
        if (label) {
            QColor bgColor = Appearance::infoBoxBackgroundColor();
            QColor textColor = Appearance::infoBoxTextColor();
            QString styleSheet = QString("color: rgb(%1, %2, %3); background-color: rgb(%4, %5, %6); padding: 5px")
                    .arg(textColor.red()).arg(textColor.green()).arg(textColor.blue())
                    .arg(bgColor.red()).arg(bgColor.green()).arg(bgColor.blue());
            label->setStyleSheet(styleSheet);
        }
    }

    if (_startCmdInfoBox) {
        QLabel *label = qobject_cast<QLabel *>(_startCmdInfoBox);
        if (label) {
            QColor bgColor = Appearance::infoBoxBackgroundColor();
            QColor textColor = Appearance::infoBoxTextColor();
            QString styleSheet = QString("color: rgb(%1, %2, %3); background-color: rgb(%4, %5, %6); padding: 5px")
                    .arg(textColor.red()).arg(textColor.green()).arg(textColor.blue())
                    .arg(bgColor.red()).arg(bgColor.green()).arg(bgColor.blue());
            label->setStyleSheet(styleSheet);
        }
    }

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

bool AdditionalMidiSettingsWidget::accept() {
    QString text = startCmd->text();
    if (!text.isEmpty()) {
        _settings->setValue("start_cmd", text);
    }
    return true;
}

MidiSettingsWidget::MidiSettingsWidget(MainWindow *mainWindow, QWidget *parent)
    : SettingsWidget("Midi I/O", parent), _mainWindow(mainWindow) {
    QGridLayout *layout = new QGridLayout(this);
    setLayout(layout);

    _playerModeInfoBox = createInfoBox(tr("Choose the Midi ports on your machine to which MidiEditor connects in order to play and record Midi data."));
    layout->addWidget(_playerModeInfoBox, 0, 0, 1, 6);

    // output
    layout->addWidget(new QLabel(tr("Midi output: "), this), 1, 0, 1, 2);
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
    layout->addWidget(new QLabel(tr("Midi input: "), this), 1, 3, 1, 2);
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
    _soundFontList->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    _soundFontList->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
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
    sfBtnCol->addWidget(_exportWavBtn);
    connect(_exportWavBtn, SIGNAL(clicked()), this, SLOT(onExportToWav()));

    _exportProgressBar = new QProgressBar(_fluidSynthSettingsGroup);
    _exportProgressBar->setRange(0, 100);
    _exportProgressBar->setValue(0);
    _exportProgressBar->setAlignment(Qt::AlignCenter);
    _exportProgressBar->setFormat(tr("Exporting... %p%"));
    _exportProgressBar->hide();
    sfBtnCol->addWidget(_exportProgressBar);

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
    settingsGrid->setContentsMargins(0, 0, 0, 0);

    // Column Stretch: Labels (0, 3) fixed, Widgets (1, 4) stretch
    settingsGrid->setColumnStretch(0, 0);
    settingsGrid->setColumnStretch(1, 1);
    settingsGrid->setColumnStretch(2, 0); // Spacer column
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

    // (0,4): Reverb Engine (Stretch 1) + Polyphony (Stretch 0)
    QHBoxLayout *row0RightLayout = new QHBoxLayout();
    row0RightLayout->setContentsMargins(0, 0, 0, 0);
    row0RightLayout->setSpacing(8);
    _reverbEngineCombo = new QComboBox(_fluidSynthSettingsGroup);
    _reverbEngineCombo->addItem(tr("FDN Reverb"), "fdn");
    _reverbEngineCombo->addItem(tr("Freeverb"), "free");
    _reverbEngineCombo->addItem(tr("LEXverb"), "lex");
    _reverbEngineCombo->addItem(tr("Dattorro Reverb"), "dat");
    int rEngIdx = _reverbEngineCombo->findData(engine->reverbEngine());
    if (rEngIdx != -1) _reverbEngineCombo->setCurrentIndex(rEngIdx);
    row0RightLayout->addWidget(_reverbEngineCombo, 1);

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
    
    row0RightLayout->addWidget(new QLabel(tr("Polyphony:"), _fluidSynthSettingsGroup));
    row0RightLayout->addWidget(_polyphonyCombo, 0);
    settingsGrid->addLayout(row0RightLayout, 0, 4);

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
    else _sampleRateCombo->setCurrentText("44100 Hz");
    row1LeftLayout->addWidget(_sampleRateCombo, 1);

    _sampleFormatCombo = new QComboBox(_fluidSynthSettingsGroup);
    _sampleFormatCombo->addItem("16 bit", "16bits");
    _sampleFormatCombo->addItem("Float", "float");
    int formatIdx = _sampleFormatCombo->findData(engine->sampleFormat());
    if (formatIdx != -1) _sampleFormatCombo->setCurrentIndex(formatIdx);
    row1LeftLayout->addWidget(new QLabel(tr("Format:"), _fluidSynthSettingsGroup));
    row1LeftLayout->addWidget(_sampleFormatCombo, 0);
    settingsGrid->addLayout(row1LeftLayout, 1, 1);

    // (1,4): Checkboxes (Aligned under Reverb Engine dropdown)
    QHBoxLayout *fxLayout = new QHBoxLayout();
    fxLayout->setContentsMargins(0, 0, 0, 0);
    _reverbCheckBox = new QCheckBox(tr("Reverb"), _fluidSynthSettingsGroup);
    _reverbCheckBox->setChecked(engine->reverbEnabled());
    fxLayout->addWidget(_reverbCheckBox);
    _chorusCheckBox = new QCheckBox(tr("Chorus"), _fluidSynthSettingsGroup);
    _chorusCheckBox->setChecked(engine->chorusEnabled());
    fxLayout->addWidget(_chorusCheckBox);
    fxLayout->addStretch();
    settingsGrid->addLayout(fxLayout, 1, 4);

    // ========================================================================
    // ROW 2
    // ========================================================================

    // (2,0): Volume Gain Label
    settingsGrid->addWidget(new QLabel(tr("Volume Gain:"), _fluidSynthSettingsGroup), 2, 0);

    // (2,1): Volume Gain Area
    QHBoxLayout *gainLayout = new QHBoxLayout();
    gainLayout->setContentsMargins(0, 0, 0, 0);
    _gainSlider = new QSlider(Qt::Horizontal, _fluidSynthSettingsGroup);
    _gainSlider->setMinimum(0);
    _gainSlider->setMaximum(300);
    _gainSlider->setSingleStep(5);
    _gainSlider->setPageStep(5);
    _gainSlider->setValue(static_cast<int>(engine->gain() * 100.0));
    gainLayout->addWidget(_gainSlider, 1);
    
    _gainValueLabel = new QLabel(QString::number(engine->gain(), 'f', 2), _fluidSynthSettingsGroup);
    _gainValueLabel->setFixedWidth(35);
    gainLayout->addWidget(_gainValueLabel);
    _gainResetBtn = new QPushButton(tr("Reset"), _fluidSynthSettingsGroup);
    _gainResetBtn->setFixedWidth(45);
    gainLayout->addWidget(_gainResetBtn);
    settingsGrid->addLayout(gainLayout, 2, 1);

    // (2,4): Reserved or empty space 
    settingsGrid->addItem(new QSpacerItem(1, 1, QSizePolicy::Minimum, QSizePolicy::Minimum), 2, 4);

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
        tr("SoundFont Files (*.sf2 *.sf3);;All Files (*)")
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

    QString exportName = QFileInfo(file->path()).baseName();
    if (exportName.isEmpty()) {
        exportName = "Exported Midi";
    }
    
    QString defaultPath = QDir(QStandardPaths::writableLocation(QStandardPaths::DownloadLocation)).filePath(exportName + ".wav");
    QString wavPath = QFileDialog::getSaveFileName(
        this,
        tr("Export MIDI to WAV"),
        defaultPath,
        tr("WAV Audio Files (*.wav)")
    );

    if (wavPath.isEmpty()) {
        return;
    }

    // Save current memory state to a temporary fast-render file
    QString tempMidi = QDir::tempPath() + "/MidiEditor_fastrender_" + QString::number(QCoreApplication::applicationPid()) + ".mid";
    if (!file->save(tempMidi)) {
        return;
    }

    _exportWavBtn->hide();
    _exportProgressBar->setValue(0);
    _exportProgressBar->show();
    
    // Disconnect any old connections to avoid double calls
    disconnect(FluidSynthEngine::instance(), SIGNAL(exportProgress(int)), this, SLOT(onExportProgress(int)));
    disconnect(FluidSynthEngine::instance(), SIGNAL(exportFinished(bool,QString)), this, SLOT(onExportFinished(bool,QString)));
    
    connect(FluidSynthEngine::instance(), SIGNAL(exportProgress(int)), this, SLOT(onExportProgress(int)));
    connect(FluidSynthEngine::instance(), SIGNAL(exportFinished(bool,QString)), this, SLOT(onExportFinished(bool,QString)));

    QThreadPool::globalInstance()->start([tempMidi, wavPath]() {
        FluidSynthEngine::instance()->exportToWav(tempMidi, wavPath);
    });
}

void MidiSettingsWidget::onExportProgress(int percent) {
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    _exportProgressBar->setValue(percent);
}

void MidiSettingsWidget::onExportFinished(bool success, const QString &path) {
    _exportProgressBar->hide();
    _exportWavBtn->show();

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
        QHBoxLayout *cbLayout = new QHBoxLayout(cbWidget);
        QCheckBox *cb = new QCheckBox();
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

