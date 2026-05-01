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

#ifdef FLUIDSYNTH_SUPPORT

#include "AudioExportDialog.h"
#include "../midi/FluidSynthEngine.h"
#include "../midi/MidiFile.h"
#include "../tool/Selection.h"
#include "../MidiEvent/MidiEvent.h"
#include "../MidiEvent/NoteOnEvent.h"
#include "../MidiEvent/OffEvent.h"

#include <QBoxLayout>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QGroupBox>
#include <QLabel>
#include <QMessageBox>
#include <QPalette>
#include "../midi/MidiChannel.h"
#include "../MidiEvent/OnEvent.h"
#include "../MidiEvent/NoteOnEvent.h"
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QStackedLayout>
#include <QStandardPaths>
#include <QThreadPool>
#include <QUrl>

// ============================================================================
// Constructor
// ============================================================================

AudioExportDialog::AudioExportDialog(MidiFile *file, QWidget *parent)
    : QDialog(parent), _file(file)
{
    setWindowTitle(tr("Export Audio"));
    setFixedWidth(440);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Build both pages
    buildConfigPage();
    buildCompletionPage();

    mainLayout->addWidget(_configPage);
    mainLayout->addWidget(_completionPage);

    // Auto-select "Selection" if events are highlighted
    if (!Selection::instance()->selectedEvents().isEmpty()) {
        _rangeSelection->setChecked(true);
        onRangeChanged();
    }
    showConfigPage();
    centerOnParent();
}

// ============================================================================
// Page Builders
// ============================================================================

void AudioExportDialog::buildConfigPage() {
    _configPage = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(_configPage);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(10);

    // --- Info ---
    QString path = _file->path();
    QString fileName = QFileInfo(path).fileName();
    if (fileName.isEmpty()) fileName = tr("Untitled");
    _sourceLabel = new QLabel(tr("<b>Source:</b> %1").arg(fileName), _configPage);
    _sourceLabel->setToolTip(path);
    layout->addWidget(_sourceLabel);

    _durationLabel = new QLabel(_configPage);
    layout->addWidget(_durationLabel);
    layout->addSpacing(4);

    // --- Song Range ---
    QGroupBox *rangeGroup = new QGroupBox(tr("Song Range"), _configPage);
    QVBoxLayout *rangeLayout = new QVBoxLayout(rangeGroup);
    rangeLayout->setSpacing(8);

    _rangeFullSong = new QRadioButton(tr("Full Song"), rangeGroup);
    _rangeFullSong->setChecked(true);
    _rangeSelection = new QRadioButton(tr("Selection"), rangeGroup);
    _rangeSelection->setEnabled(!Selection::instance()->selectedEvents().isEmpty());
    _rangeCustom = new QRadioButton(tr("Custom Range"), rangeGroup);

    QHBoxLayout *radioLayout = new QHBoxLayout();
    radioLayout->addWidget(_rangeFullSong, 0, Qt::AlignLeft);
    radioLayout->addStretch();
    radioLayout->addWidget(_rangeSelection, 0, Qt::AlignCenter);
    radioLayout->addStretch();
    radioLayout->addWidget(_rangeCustom, 0, Qt::AlignRight);
    rangeLayout->addLayout(radioLayout);

    QHBoxLayout *customBoxesLayout = new QHBoxLayout();
    customBoxesLayout->setContentsMargins(0, 0, 0, 0);
    customBoxesLayout->setSpacing(12);

    int lastMeasure = 1;
    {
        int d1 = 0, d2 = 0;
        lastMeasure = _file->measure(_file->endTick(), &d1, &d2);
        if (lastMeasure < 1) lastMeasure = 1;
    }

    auto setupMeasureBox = [&](QSpinBox** sb, const QString& title, int val) {
        QGroupBox* box = new QGroupBox(title, rangeGroup);
        QVBoxLayout* l = new QVBoxLayout(box);
        l->setContentsMargins(8, 8, 8, 8);
        *sb = new QSpinBox(box);
        (*sb)->setMinimum(1);
        (*sb)->setMaximum(lastMeasure);
        (*sb)->setValue(val);
        (*sb)->setPrefix(tr("Measure "));
        (*sb)->setMinimumWidth(140);
        (*sb)->setAlignment(Qt::AlignCenter);
        l->addWidget(*sb);
        return box;
    };

    QGroupBox* startBox = setupMeasureBox(&_rangeFrom, tr("Starting"), 1);
    QGroupBox* endBox = setupMeasureBox(&_rangeTo, tr("Ending"), lastMeasure);

    customBoxesLayout->addStretch();
    customBoxesLayout->addWidget(startBox);
    QLabel* dashLabel = new QLabel(tr("-"), rangeGroup);
    dashLabel->setStyleSheet("font-weight: bold; font-size: 16px;");
    customBoxesLayout->addWidget(dashLabel);
    customBoxesLayout->addWidget(endBox);
    customBoxesLayout->addStretch();

    _rangeFrom->setEnabled(false);
    _rangeTo->setEnabled(false);
    rangeLayout->addLayout(customBoxesLayout);
    layout->addWidget(rangeGroup);

    connect(_rangeFullSong, &QRadioButton::toggled, this, &AudioExportDialog::onRangeChanged);
    connect(_rangeSelection, &QRadioButton::toggled, this, &AudioExportDialog::onRangeChanged);
    connect(_rangeCustom, &QRadioButton::toggled, this, &AudioExportDialog::onRangeChanged);
    connect(_rangeFrom, QOverload<int>::of(&QSpinBox::valueChanged), this, &AudioExportDialog::updateEstimatedSize);
    connect(_rangeTo, QOverload<int>::of(&QSpinBox::valueChanged), this, &AudioExportDialog::updateEstimatedSize);

    // --- Audio Configuration ---
    QGroupBox *configGroup = new QGroupBox(tr("Audio Configuration"), _configPage);
    QGridLayout *configLayout = new QGridLayout(configGroup);
    configLayout->setSpacing(10);

    configLayout->addWidget(new QLabel(tr("File Type:"), configGroup), 0, 0);
    _formatCombo = new QComboBox(configGroup);
    _formatCombo->addItem(AudioExportSettings::formatName(AudioExportSettings::WAV), static_cast<int>(AudioExportSettings::WAV));
    _formatCombo->addItem(AudioExportSettings::formatName(AudioExportSettings::FLAC), static_cast<int>(AudioExportSettings::FLAC));
    _formatCombo->addItem(AudioExportSettings::formatName(AudioExportSettings::OPUS), static_cast<int>(AudioExportSettings::OPUS));
    _formatCombo->addItem(AudioExportSettings::formatName(AudioExportSettings::OGG_VORBIS), static_cast<int>(AudioExportSettings::OGG_VORBIS));
    _formatCombo->addItem(AudioExportSettings::formatName(AudioExportSettings::MP3), static_cast<int>(AudioExportSettings::MP3));
    _formatCombo->setCurrentIndex(1); // FLAC default
    configLayout->addWidget(_formatCombo, 0, 1, 1, 3);

    configLayout->addWidget(new QLabel(tr("Sample Rate:"), configGroup), 1, 0);
    _rateCombo = new QComboBox(configGroup);
    _rateCombo->addItems({"22050 Hz", "44100 Hz", "48000 Hz", "88200 Hz", "96000 Hz", "176400 Hz", "192000 Hz"});
    // Set data values for easy retrieval
    _rateCombo->setItemData(0, 22050); _rateCombo->setItemData(1, 44100); _rateCombo->setItemData(2, 48000);
    _rateCombo->setItemData(3, 88200); _rateCombo->setItemData(4, 96000); _rateCombo->setItemData(5, 176400);
    _rateCombo->setItemData(6, 192000);
    _rateCombo->setCurrentIndex(2); // 48000 default
    configLayout->addWidget(_rateCombo, 1, 1);

    configLayout->addWidget(new QLabel(tr("Format:"), configGroup), 1, 2);
    _bitsCombo = new QComboBox(configGroup);
    _bitsCombo->addItem(tr("16 bit"), "16bits");
    _bitsCombo->addItem(tr("Float"), "float");
    _bitsCombo->setCurrentIndex(0);
    configLayout->addWidget(_bitsCombo, 1, 3);

    configLayout->setColumnStretch(1, 1);
    configLayout->setColumnStretch(3, 1);

    layout->addWidget(configGroup);

    connect(_formatCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AudioExportDialog::onFormatChanged);
    connect(_rateCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AudioExportDialog::onAudioConfigChanged);
    connect(_bitsCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AudioExportDialog::onAudioConfigChanged);

    // --- Options ---
    QGroupBox *optionsGroup = new QGroupBox(tr("Options"), _configPage);
    QVBoxLayout *optionsLayout = new QVBoxLayout(optionsGroup);
    optionsLayout->setSpacing(6);

    _reverbTailCheck = new QCheckBox(tr("Reverb Tail"), optionsGroup);
    _reverbTailCheck->setChecked(true);
    optionsLayout->addWidget(_reverbTailCheck);

    QLabel *reverbDesc = new QLabel(tr("Adds 2s after the last note so reverb and sustain can fade naturally."), optionsGroup);
    reverbDesc->setStyleSheet("color: #777; font-size: 11px; margin-left: 20px;");
    reverbDesc->setWordWrap(true);
    optionsLayout->addWidget(reverbDesc);

    _trimSilenceCheck = new QCheckBox(tr("Trim Silence"), optionsGroup);
    _trimSilenceCheck->setChecked(false);
    optionsLayout->addWidget(_trimSilenceCheck);

    QLabel *trimDesc = new QLabel(tr("Automatically trims dead air before the first note and after the last note."), optionsGroup);
    trimDesc->setStyleSheet("color: #777; font-size: 11px; margin-left: 20px;");
    trimDesc->setWordWrap(true);
    optionsLayout->addWidget(trimDesc);

    connect(_reverbTailCheck, &QCheckBox::toggled, this, &AudioExportDialog::updateEstimatedSize);
    connect(_trimSilenceCheck, &QCheckBox::toggled, this, &AudioExportDialog::updateEstimatedSize);

    // SoundFont list (read-only display)
    FluidSynthEngine *engine = FluidSynthEngine::instance();
    QList<QPair<QString, bool>> collection = engine->soundFontCollection();
    QStringList enabledFonts;
    for (const auto &pair : collection) {
        if (pair.second) {
            enabledFonts.append(QFileInfo(pair.first).fileName());
        }
    }

    if (!enabledFonts.isEmpty()) {
        QLabel *sfLabel = new QLabel(tr("SoundFonts:"), optionsGroup);
        QFont sfFont = sfLabel->font();
        sfFont.setBold(true);
        sfLabel->setFont(sfFont);
        optionsLayout->addSpacing(4);
        optionsLayout->addWidget(sfLabel);

        _soundFontListLabel = new QLabel(optionsGroup);
        QString sfText;
        for (const QString &name : enabledFonts) {
            sfText += QString("  \u2022 %1\n").arg(name);
        }
        sfText.chop(1); // remove trailing newline
        _soundFontListLabel->setText(sfText);
        _soundFontListLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        optionsLayout->addWidget(_soundFontListLabel);
    }

    layout->addWidget(optionsGroup);

    // --- Buttons ---
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    _exportBtn = new QPushButton(tr("Export..."), _configPage);
    _exportBtn->setDefault(true);
    _cancelConfigBtn = new QPushButton(tr("Cancel"), _configPage);
    btnLayout->addWidget(_exportBtn);
    btnLayout->addWidget(_cancelConfigBtn);
    layout->addLayout(btnLayout);

    connect(_cancelConfigBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(_exportBtn, &QPushButton::clicked, this, &AudioExportDialog::onExportClicked);

    updateEstimatedSize();
}

void AudioExportDialog::buildCompletionPage() {
    _completionPage = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(_completionPage);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(12);

    layout->addStretch();

    _completionIcon = new QLabel(_completionPage);
    _completionIcon->setAlignment(Qt::AlignCenter);
    layout->addWidget(_completionIcon);

    _completionFileLabel = new QLabel(_completionPage);
    _completionFileLabel->setAlignment(Qt::AlignCenter);
    QFont fileFont = _completionFileLabel->font();
    fileFont.setPointSize(fileFont.pointSize() + 1);
    fileFont.setBold(true);
    _completionFileLabel->setFont(fileFont);
    layout->addWidget(_completionFileLabel);

    _completionDetailsLabel = new QLabel(_completionPage);
    _completionDetailsLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(_completionDetailsLabel);

    layout->addStretch();

    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    _openFileBtn = new QPushButton(tr("Open"), _completionPage);
    _openFolderBtn = new QPushButton(tr("Open Folder"), _completionPage);
    _closeBtn = new QPushButton(tr("Close"), _completionPage);
    btnLayout->addWidget(_openFileBtn);
    btnLayout->addWidget(_openFolderBtn);
    btnLayout->addWidget(_closeBtn);
    btnLayout->addStretch();
    layout->addLayout(btnLayout);

    connect(_openFileBtn, &QPushButton::clicked, this, &AudioExportDialog::openExportedFile);
    connect(_openFolderBtn, &QPushButton::clicked, this, &AudioExportDialog::openExportedFolder);
    connect(_closeBtn, &QPushButton::clicked, this, &QDialog::accept);
}

// ============================================================================
// Page Switching
// ============================================================================

void AudioExportDialog::showConfigPage() {
    _configPage->setVisible(true);
    _completionPage->setVisible(false);
    setFixedSize(sizeHint());
    centerOnParent();
}

void AudioExportDialog::showCompletionPage(bool success) {
    _configPage->setVisible(false);
    _completionPage->setVisible(true);

    if (success) {
        _completionIcon->setVisible(false);
        _completionFileLabel->setText(tr("Export Successful"));
        _completionFileLabel->setStyleSheet("color: #4CAF50; font-size: 18px; font-weight: bold;");

        double fileSizeBytes = QFile(_exportedPath).size();
        double durationSec = estimateDurationSeconds();
        AudioExportSettings s = currentSettings();

        QColor textColor = palette().color(QPalette::WindowText);
        QString details = QString("<center style='color: %1;'>"
                                  "Size: %2        Duration: %3<br>"
                                  "Format: %4 Hz, %5, %6"
                                  "</center>")
            .arg(textColor.name())
            .arg(formatFileSize(fileSizeBytes))
            .arg(formatDuration(durationSec))
            .arg(s.rate)
            .arg(s.sampleFormat == "float" ? tr("Float") : tr("16 bit"))
            .arg(AudioExportSettings::formatShortName(s.format));

        _completionDetailsLabel->setText(details);
        _completionDetailsLabel->setStyleSheet("font-size: 13px;");
        _completionDetailsLabel->setAlignment(Qt::AlignCenter);

        _openFileBtn->setVisible(true);
        _openFolderBtn->setVisible(true);
    } else {
        _completionIcon->setVisible(false);
        _completionFileLabel->setText(tr("Export Failed"));
        _completionFileLabel->setStyleSheet("color: #F44336; font-size: 18px; font-weight: bold;");

        QColor textColor = palette().color(QPalette::WindowText);
        _completionDetailsLabel->setText(QString("<center style='color: %1;'>%2</center>")
            .arg(textColor.name())
            .arg(tr("An error occurred during audio export.")));
        _completionDetailsLabel->setStyleSheet("font-size: 13px;");
        _completionDetailsLabel->setAlignment(Qt::AlignCenter);

        _openFileBtn->setVisible(false);
        _openFolderBtn->setVisible(false);
    }

    setFixedSize(sizeHint());
    centerOnParent();
}

void AudioExportDialog::centerOnParent() {
    if (parentWidget()) {
        QRect parentRect = parentWidget()->geometry();
        QRect dialogRect = geometry();
        int x = parentRect.x() + (parentRect.width() - dialogRect.width()) / 2;
        int y = parentRect.y() + (parentRect.height() - dialogRect.height()) / 2;
        move(x, y);
    }
}

int AudioExportDialog::lastActualEventTick() const {
    if (!_file) return 0;
    int lastTick = 0;
    for (int i = 0; i < 19; i++) {
        if (_file->channel(i)->eventMap()->isEmpty()) continue;
        int last = _file->channel(i)->eventMap()->lastKey();
        if (last > lastTick) lastTick = last;
    }
    return lastTick;
}

// ============================================================================
// Settings Helpers
// ============================================================================

AudioExportSettings AudioExportDialog::currentSettings() const {
    AudioExportSettings s;
    s.format = static_cast<AudioExportSettings::Format>(_formatCombo->currentData().toInt());
    s.rate = _rateCombo->currentData().toInt();
    s.sampleFormat = _bitsCombo->currentData().toString();
    s.includeReverbTail = _reverbTailCheck->isChecked();
    return s;
}



double AudioExportDialog::estimateFileSize() const {
    AudioExportSettings s = currentSettings();
    double durationSec = estimateDurationSeconds();
    int channels = 2;       // stereo
    int bytesPerSample = (s.sampleFormat == "float") ? 4 : 2;
    double rawBytes = durationSec * s.sampleRate() * channels * bytesPerSample;

    switch (s.format) {
        case AudioExportSettings::WAV:        return rawBytes * 1.0;
        case AudioExportSettings::FLAC:       return rawBytes * 0.55;
        case AudioExportSettings::OPUS:       return rawBytes * 0.06;
        case AudioExportSettings::OGG_VORBIS: return rawBytes * 0.08;
        case AudioExportSettings::MP3:        return rawBytes * 0.10;
    }
    return rawBytes;
}

QString AudioExportDialog::formatFileSize(double bytes) const {
    if (bytes < 1024.0) return QString::number(bytes, 'f', 0) + " B";
    if (bytes < 1024.0 * 1024.0) return QString::number(bytes / 1024.0, 'f', 1) + " KB";
    if (bytes < 1024.0 * 1024.0 * 1024.0) return QString::number(bytes / (1024.0 * 1024.0), 'f', 1) + " MB";
    return QString::number(bytes / (1024.0 * 1024.0 * 1024.0), 'f', 2) + " GB";
}

QString AudioExportDialog::formatDuration(double seconds) const {
    if (seconds < 0) seconds = 0;
    int totalSec = static_cast<int>(seconds);
    int min = totalSec / 60;
    int sec = totalSec % 60;
    return QString("%1:%2").arg(min).arg(sec, 2, 10, QChar('0'));
}

// ============================================================================
// Slots
// ============================================================================

void AudioExportDialog::onRangeChanged() {
    bool custom = _rangeCustom->isChecked();
    _rangeFrom->setEnabled(custom);
    _rangeTo->setEnabled(custom);
    updateEstimatedSize();
}

void AudioExportDialog::onFormatChanged(int /*index*/) {
    updateEstimatedSize();
}

void AudioExportDialog::onAudioConfigChanged(int /*index*/) {
    updateEstimatedSize();
}

void AudioExportDialog::updateEstimatedSize() {
    double durationSec = estimateDurationSeconds();
    _durationLabel->setText(tr("<b>Duration:</b> %1").arg(formatDuration(durationSec)));
}

double AudioExportDialog::estimateDurationSeconds() const {
    double totalMs = 0;
    int maxTick = 0;

    int fromTick = 0;
    int toTick = lastActualEventTick();

    if (_rangeCustom->isChecked()) {
        int fromMeasure = _rangeFrom->value();
        int toMeasure = _rangeTo->value();

        if (fromMeasure > toMeasure) {
            std::swap(fromMeasure, toMeasure);
        }

        fromTick = _file->startTickOfMeasure(fromMeasure);
        toTick = _file->startTickOfMeasure(toMeasure + 1);
        if (toTick > _file->endTick()) toTick = _file->endTick();
        if (fromTick < 0) fromTick = 0;
    } else if (_rangeSelection->isChecked()) {
        QList<MidiEvent *> selected = Selection::instance()->selectedEvents();
        if (!selected.isEmpty()) {
            fromTick = -1;
            toTick = -1;
            for (MidiEvent *ev : selected) {
                if (fromTick == -1 || ev->midiTime() < fromTick) fromTick = ev->midiTime();
                if (toTick == -1 || ev->midiTime() > toTick) toTick = ev->midiTime();
                if (NoteOnEvent *on = dynamic_cast<NoteOnEvent *>(ev)) {
                    if (on->offEvent() && on->offEvent()->midiTime() > toTick) {
                        toTick = on->offEvent()->midiTime();
                    }
                }
            }
        }
    }

    if (_trimSilenceCheck->isChecked()) {
        int firstNoteTick = -1;
        int lastNoteTick = -1;
        for (int i = 0; i < 16; i++) {
            if (_file->channel(i)->eventMap()->isEmpty()) continue;
            QMultiMap<int, MidiEvent*>::iterator it = _file->channel(i)->eventMap()->lowerBound(fromTick);
            while (it != _file->channel(i)->eventMap()->end()) {
                if (toTick != -1 && it.key() > toTick) break;
                if (dynamic_cast<NoteOnEvent*>(it.value())) {
                    if (firstNoteTick == -1 || it.key() < firstNoteTick) firstNoteTick = it.key();
                }
                if (OffEvent* off = dynamic_cast<OffEvent*>(it.value())) {
                    if (lastNoteTick == -1 || it.key() > lastNoteTick) lastNoteTick = it.key();
                }
                it++;
            }
        }
        if (firstNoteTick != -1) fromTick = firstNoteTick;
        if (lastNoteTick != -1) toTick = lastNoteTick;
    }

    if (toTick < fromTick) toTick = fromTick;
    totalMs = _file->msOfTick(toTick) - _file->msOfTick(fromTick);

    double durationSec = totalMs / 1000.0;
    if (_reverbTailCheck->isChecked()) {
        durationSec += 2.0;
    }
    return durationSec;
}

void AudioExportDialog::onExportClicked() {
    if (!_file) return;

    AudioExportSettings settings = currentSettings();

    // Build default filename
    QString baseName = QFileInfo(_file->path()).baseName();
    if (baseName.isEmpty()) baseName = "Exported";
    QString defaultPath = QDir(QStandardPaths::writableLocation(QStandardPaths::DownloadLocation))
                              .filePath(baseName + settings.extension());

    QString outputPath = QFileDialog::getSaveFileName(
        this,
        tr("Export Audio"),
        defaultPath,
        settings.filterString()
    );

    if (outputPath.isEmpty()) return;

    _exportedPath = outputPath;

    // Save a filtered temp MIDI (respects mute/solo)
    QString tempMidi = QDir::tempPath() + "/MidiEditor_export_" +
                       QString::number(QCoreApplication::applicationPid()) + ".mid";
    
    // Handle Range
    int startTick = 0;
    int endTick = lastActualEventTick();
    if (_rangeCustom->isChecked()) {
        int fromMeasure = _rangeFrom->value();
        int toMeasure = _rangeTo->value();
        if (fromMeasure > toMeasure) std::swap(fromMeasure, toMeasure);
        
        startTick = _file->startTickOfMeasure(fromMeasure);
        endTick = _file->startTickOfMeasure(toMeasure + 1);
    } else if (_rangeSelection->isChecked()) {
        QList<MidiEvent *> selected = Selection::instance()->selectedEvents();
        if (!selected.isEmpty()) {
            int minT = -1, maxT = -1;
            for (MidiEvent *ev : selected) {
                if (minT == -1 || ev->midiTime() < minT) minT = ev->midiTime();
                if (maxT == -1 || ev->midiTime() > maxT) maxT = ev->midiTime();
                if (NoteOnEvent *on = dynamic_cast<NoteOnEvent *>(ev)) {
                    if (on->offEvent() && on->offEvent()->midiTime() > maxT) maxT = on->offEvent()->midiTime();
                }
            }
            startTick = minT;
            endTick = maxT;
        }
    }

    if (endTick > _file->endTick()) endTick = _file->endTick();
    if (startTick < 0) startTick = 0;

    if (_trimSilenceCheck->isChecked()) {
        int firstNoteTick = -1;
        int lastNoteTick = -1;
        for (int i = 0; i < 16; i++) {
            if (_file->channel(i)->eventMap()->isEmpty()) continue;
            QMultiMap<int, MidiEvent*>::iterator it = _file->channel(i)->eventMap()->lowerBound(startTick);
            while (it != _file->channel(i)->eventMap()->end()) {
                if (endTick != -1 && it.key() > endTick) break;
                if (dynamic_cast<NoteOnEvent*>(it.value())) {
                    if (firstNoteTick == -1 || it.key() < firstNoteTick) firstNoteTick = it.key();
                }
                if (OffEvent* off = dynamic_cast<OffEvent*>(it.value())) {
                    if (lastNoteTick == -1 || it.key() > lastNoteTick) lastNoteTick = it.key();
                }
                it++;
            }
        }
        if (firstNoteTick != -1) startTick = firstNoteTick;
        if (lastNoteTick != -1) endTick = lastNoteTick;
    }

    if (endTick < startTick) endTick = startTick;

    if (!_file->saveForExport(tempMidi, startTick, endTick)) {
        QMessageBox::critical(this, tr("Export Error"), tr("Failed to prepare MIDI data for export."));
        return;
    }

    _exportBtn->setEnabled(false);
    _exportBtn->setText(tr("Exporting..."));

    // Connect signals
    FluidSynthEngine *engine = FluidSynthEngine::instance();
    connect(engine, &FluidSynthEngine::exportProgress,
            this, &AudioExportDialog::onExportProgress, Qt::QueuedConnection);
    connect(engine, &FluidSynthEngine::exportFinished,
            this, &AudioExportDialog::onExportFinished, Qt::QueuedConnection);
    connect(engine, &FluidSynthEngine::exportCancelled,
            this, &AudioExportDialog::onExportCancelled, Qt::QueuedConnection);

    // Run export in background thread
    QThreadPool::globalInstance()->start([tempMidi, outputPath, settings]() {
        FluidSynthEngine::instance()->exportAudio(tempMidi, outputPath, settings);
    });
}

void AudioExportDialog::onExportProgress(int /*percent*/) {
    _exportBtn->setText(tr("Exporting..."));
}

void AudioExportDialog::onExportFinished(bool success, const QString &path) {
    _exportBtn->setEnabled(true);
    _exportBtn->setText(tr("Export..."));

    // Disconnect signals
    FluidSynthEngine *engine = FluidSynthEngine::instance();
    disconnect(engine, &FluidSynthEngine::exportProgress,
               this, &AudioExportDialog::onExportProgress);
    disconnect(engine, &FluidSynthEngine::exportFinished,
               this, &AudioExportDialog::onExportFinished);
    disconnect(engine, &FluidSynthEngine::exportCancelled,
               this, &AudioExportDialog::onExportCancelled);

    // Clean up temp MIDI
    QString tempMidi = QDir::tempPath() + "/MidiEditor_export_" +
                       QString::number(QCoreApplication::applicationPid()) + ".mid";
    QFile::remove(tempMidi);

    _exportedPath = path;
    showCompletionPage(success);
}

void AudioExportDialog::onExportCancelled() {
    _exportBtn->setEnabled(true);
    _exportBtn->setText(tr("Export..."));

    // Disconnect signals
    FluidSynthEngine *engine = FluidSynthEngine::instance();
    disconnect(engine, &FluidSynthEngine::exportProgress,
               this, &AudioExportDialog::onExportProgress);
    disconnect(engine, &FluidSynthEngine::exportFinished,
               this, &AudioExportDialog::onExportFinished);
    disconnect(engine, &FluidSynthEngine::exportCancelled,
               this, &AudioExportDialog::onExportCancelled);

    // Clean up temp MIDI
    QString tempMidi = QDir::tempPath() + "/MidiEditor_export_" +
                       QString::number(QCoreApplication::applicationPid()) + ".mid";
    QFile::remove(tempMidi);
}

void AudioExportDialog::onCancelExport() {
    FluidSynthEngine::instance()->cancelExport();
}

void AudioExportDialog::openExportedFile() {
    QDesktopServices::openUrl(QUrl::fromLocalFile(_exportedPath));
}

void AudioExportDialog::openExportedFolder() {
#ifdef Q_OS_WIN
    QProcess::startDetached("explorer.exe", {"/select,", QDir::toNativeSeparators(_exportedPath)});
#else
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(_exportedPath).absolutePath()));
#endif
}

#endif // FLUIDSYNTH_SUPPORT
