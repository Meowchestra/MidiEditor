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
#include <QFileDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QSlider>
#include <QVBoxLayout>
#include "../midi/FluidSynthEngine.h"
#include "../midi/MidiOutput.h"
#include "DownloadSoundFontDialog.h"
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

MidiSettingsWidget::MidiSettingsWidget(QWidget *parent)
    : SettingsWidget("Midi I/O", parent) {
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
    QLabel *sfLabel = new QLabel(tr("SoundFonts (top = highest priority):"), _fluidSynthSettingsGroup);
    fsLayout->addWidget(sfLabel);

    QHBoxLayout *sfRow = new QHBoxLayout();
    _soundFontList = new QListWidget(_fluidSynthSettingsGroup);
    _soundFontList->setMinimumHeight(100);
    sfRow->addWidget(_soundFontList, 1);

    QVBoxLayout *sfBtnCol = new QVBoxLayout();
    _addSoundFontBtn = new QPushButton(tr("Add..."), _fluidSynthSettingsGroup);
    _removeSoundFontBtn = new QPushButton(tr("Remove"), _fluidSynthSettingsGroup);
    _moveSoundFontUpBtn = new QPushButton(tr("Up"), _fluidSynthSettingsGroup);
    _moveSoundFontDownBtn = new QPushButton(tr("Down"), _fluidSynthSettingsGroup);
    _downloadDefaultSoundFontBtn = new QPushButton(tr("Download Default..."), _fluidSynthSettingsGroup);
    sfBtnCol->addWidget(_addSoundFontBtn);
    sfBtnCol->addWidget(_removeSoundFontBtn);
    sfBtnCol->addWidget(_moveSoundFontUpBtn);
    sfBtnCol->addWidget(_moveSoundFontDownBtn);
    sfBtnCol->addWidget(_downloadDefaultSoundFontBtn);
    sfBtnCol->addStretch();
    sfRow->addLayout(sfBtnCol);
    fsLayout->addLayout(sfRow);

    connect(_addSoundFontBtn, SIGNAL(clicked()), this, SLOT(addSoundFont()));
    connect(_removeSoundFontBtn, SIGNAL(clicked()), this, SLOT(removeSoundFont()));
    connect(_moveSoundFontUpBtn, SIGNAL(clicked()), this, SLOT(moveSoundFontUp()));
    connect(_moveSoundFontDownBtn, SIGNAL(clicked()), this, SLOT(moveSoundFontDown()));
    connect(_downloadDefaultSoundFontBtn, SIGNAL(clicked()), this, SLOT(showDownloadSoundFontDialog()));

    // Audio Driver, Sample Rate, and Reverb Engine on one row
    QHBoxLayout *topSettingsRow = new QHBoxLayout();
    topSettingsRow->addWidget(new QLabel(tr("Driver:"), _fluidSynthSettingsGroup));
    _audioDriverCombo = new QComboBox(_fluidSynthSettingsGroup);
    FluidSynthEngine *engine = FluidSynthEngine::instance();
    for (const QString &driver : engine->availableAudioDrivers()) {
        _audioDriverCombo->addItem(FluidSynthEngine::audioDriverDisplayName(driver), driver);
    }
    if (!engine->audioDriver().isEmpty()) {
        int driverIdx = _audioDriverCombo->findData(engine->audioDriver());
        if (driverIdx != -1) {
            _audioDriverCombo->setCurrentIndex(driverIdx);
        }
    }
    topSettingsRow->addWidget(_audioDriverCombo);
    connect(_audioDriverCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(onAudioDriverChanged(int)));

    topSettingsRow->addWidget(new QLabel(tr("Rate:"), _fluidSynthSettingsGroup));
    _sampleRateCombo = new QComboBox(_fluidSynthSettingsGroup);
    _sampleRateCombo->addItems({"22050 Hz", "44100 Hz", "48000 Hz", "96000 Hz"});

    // Set fallback to 44100 Hz if engine returned something not in the list
    QString engineRate = QString::number(static_cast<int>(engine->sampleRate())) + " Hz";
    if (_sampleRateCombo->findText(engineRate) != -1) {
        _sampleRateCombo->setCurrentText(engineRate);
    } else {
        _sampleRateCombo->setCurrentText("44100 Hz");
    }
    topSettingsRow->addWidget(_sampleRateCombo);
    connect(_sampleRateCombo, SIGNAL(currentTextChanged(QString)), this, SLOT(onSampleRateChanged(QString)));

    topSettingsRow->addWidget(new QLabel(tr("Reverb Engine:"), _fluidSynthSettingsGroup));
    _reverbEngineCombo = new QComboBox(_fluidSynthSettingsGroup);
    _reverbEngineCombo->addItem(tr("FDN Reverb (Default)"), "fdn");
    _reverbEngineCombo->addItem(tr("Freeverb (Pre-2.1.0)"), "free");
    _reverbEngineCombo->addItem(tr("LEXverb"), "lex");
    _reverbEngineCombo->addItem(tr("Dattorro Reverb"), "dat");

    int engineIdx = _reverbEngineCombo->findData(engine->reverbEngine());
    if (engineIdx != -1) {
        _reverbEngineCombo->setCurrentIndex(engineIdx);
    }
    topSettingsRow->addWidget(_reverbEngineCombo);
    connect(_reverbEngineCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(onReverbEngineChanged(int)));

    fsLayout->addLayout(topSettingsRow);

    // Gain slider with Reverb & Chorus toggles on the same row
    QHBoxLayout *gainRow = new QHBoxLayout();
    gainRow->addWidget(new QLabel(tr("Gain:"), _fluidSynthSettingsGroup));
    _gainSlider = new QSlider(Qt::Horizontal, _fluidSynthSettingsGroup);
    _gainSlider->setMinimum(0);
    _gainSlider->setMaximum(300); // 0.0 - 3.0 in steps of 0.01
    _gainSlider->setValue(static_cast<int>(engine->gain() * 100.0));
    gainRow->addWidget(_gainSlider, 1);
    _gainValueLabel = new QLabel(QString::number(engine->gain(), 'f', 2), _fluidSynthSettingsGroup);
    _gainValueLabel->setFixedWidth(40);
    gainRow->addWidget(_gainValueLabel);
    _gainResetBtn = new QPushButton(tr("Reset"), _fluidSynthSettingsGroup);
    _gainResetBtn->setFixedWidth(50);
    _gainResetBtn->setToolTip(tr("Reset gain to default (0.50)"));
    gainRow->addWidget(_gainResetBtn);
    connect(_gainSlider, SIGNAL(valueChanged(int)), this, SLOT(onGainChanged(int)));
    connect(_gainResetBtn, SIGNAL(clicked()), this, SLOT(onGainReset()));

    // Reverb and Chorus checkboxes to the right of gain
    _reverbCheckBox = new QCheckBox(tr("Reverb"), _fluidSynthSettingsGroup);
    _reverbCheckBox->setChecked(engine->reverbEnabled());
    gainRow->addWidget(_reverbCheckBox);
    _chorusCheckBox = new QCheckBox(tr("Chorus"), _fluidSynthSettingsGroup);
    _chorusCheckBox->setChecked(engine->chorusEnabled());
    gainRow->addWidget(_chorusCheckBox);
    connect(_reverbCheckBox, SIGNAL(toggled(bool)), this, SLOT(onReverbToggled(bool)));
    connect(_chorusCheckBox, SIGNAL(toggled(bool)), this, SLOT(onChorusToggled(bool)));

    fsLayout->addLayout(gainRow);

    layout->addWidget(_fluidSynthSettingsGroup, 3, 0, 1, 6);

    // Populate SoundFont list from engine
    refreshSoundFontList();

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
    bool enabled = MidiOutput::isFluidSynthOutput();
    _fluidSynthSettingsGroup->setEnabled(enabled);
}

void MidiSettingsWidget::addSoundFont() {
    QStringList files = QFileDialog::getOpenFileNames(
        this,
        tr("Select SoundFont Files"),
        QString(),
        tr("SoundFont Files (*.sf2 *.sf3 *.SF2 *.SF3);;All Files (*)")
    );

    FluidSynthEngine *engine = FluidSynthEngine::instance();
    for (const QString &file : files) {
        engine->loadSoundFont(file);
    }
    refreshSoundFontList();
}

void MidiSettingsWidget::removeSoundFont() {
    int row = _soundFontList->currentRow();
    if (row < 0) {
        return;
    }

    FluidSynthEngine *engine = FluidSynthEngine::instance();
    QList<QPair<int, QString>> fonts = engine->loadedSoundFonts();

    // The list is displayed in priority order (highest first = last loaded first)
    // So row 0 maps to the last item in _loadedFonts
    int reversedIndex = fonts.size() - 1 - row;
    if (reversedIndex >= 0 && reversedIndex < fonts.size()) {
        engine->unloadSoundFont(fonts[reversedIndex].first);
    }
    refreshSoundFontList();
}

void MidiSettingsWidget::moveSoundFontUp() {
    int row = _soundFontList->currentRow();
    if (row <= 0) {
        return;
    }

    // Rebuild the entire stack with swapped items
    FluidSynthEngine *engine = FluidSynthEngine::instance();
    QList<QPair<int, QString>> fonts = engine->loadedSoundFonts();

    // Build path list in priority order (highest first = reversed load order)
    QStringList paths;
    for (int i = fonts.size() - 1; i >= 0; --i) {
        paths.append(fonts[i].second);
    }

    // Swap the item up
    paths.swapItemsAt(row, row - 1);

    engine->setSoundFontStack(paths);
    refreshSoundFontList();
    _soundFontList->setCurrentRow(row - 1);
}

void MidiSettingsWidget::moveSoundFontDown() {
    int row = _soundFontList->currentRow();
    if (row < 0 || row >= _soundFontList->count() - 1) {
        return;
    }

    // Rebuild the entire stack with swapped items
    FluidSynthEngine *engine = FluidSynthEngine::instance();
    QList<QPair<int, QString>> fonts = engine->loadedSoundFonts();

    // Build path list in priority order (highest first = reversed load order)
    QStringList paths;
    for (int i = fonts.size() - 1; i >= 0; --i) {
        paths.append(fonts[i].second);
    }

    // Swap the item down
    paths.swapItemsAt(row, row + 1);

    engine->setSoundFontStack(paths);
    refreshSoundFontList();
    _soundFontList->setCurrentRow(row + 1);
}

void MidiSettingsWidget::onAudioDriverChanged(int index) {
    QString driver = _audioDriverCombo->itemData(index).toString();
    FluidSynthEngine::instance()->setAudioDriver(driver);
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
    _soundFontList->clear();
    FluidSynthEngine *engine = FluidSynthEngine::instance();
    QList<QPair<int, QString>> fonts = engine->loadedSoundFonts();

    // Display in priority order: highest priority first (last loaded = top)
    for (int i = fonts.size() - 1; i >= 0; --i) {
        QFileInfo fi(fonts[i].second);
        _soundFontList->addItem(fi.fileName());
        // Store full path in the item's tooltip
        _soundFontList->item(_soundFontList->count() - 1)->setToolTip(fonts[i].second);
    }
}

void MidiSettingsWidget::showDownloadSoundFontDialog() {
    DownloadSoundFontDialog *dialog = new DownloadSoundFontDialog(this);
    connect(dialog, &DownloadSoundFontDialog::soundFontDownloaded, this, [this](const QString &path) {
        FluidSynthEngine::instance()->loadSoundFont(path);
        refreshSoundFontList();
    });
    dialog->exec();
    dialog->deleteLater();
}

#endif

