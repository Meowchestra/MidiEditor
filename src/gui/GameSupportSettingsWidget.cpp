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

#include "GameSupportSettingsWidget.h"
#include "Appearance.h"

#include <QCheckBox>
#include <QGroupBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QSettings>
#include <QIcon>

#include "../midi/InstrumentDefinitions.h"

GameSupportSettingsWidget::GameSupportSettingsWidget(QSettings *settings, QWidget *parent)
    : SettingsWidget(tr("Game Support"), parent)
      , _settings(settings) {

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    setLayout(mainLayout);

    // --- Final Fantasy XIV Section ---
    _ffxivGroup = new QGroupBox(tr("Final Fantasy XIV"), this);
    QGridLayout *ffxivLayout = new QGridLayout(_ffxivGroup);

    _ffxivEnabledBox = new QCheckBox(tr("Enable Channel Fixer"), _ffxivGroup);
    _ffxivEnabledBox->setChecked(_settings->value("game_support/ffxiv_enabled", false).toBool());
    _ffxivEnabledBox->setToolTip(tr("Adds FFXIV-specific tools to the Tracks and Channels panels."));
    connect(_ffxivEnabledBox, SIGNAL(toggled(bool)), this, SLOT(onFFXIVBoxToggled(bool)));
    ffxivLayout->addWidget(_ffxivEnabledBox, 0, 0, 1, 2);

    QLabel *ffxivDesc = new QLabel(
        tr("When enabled, a <b>Fix XIV Channels</b> button appears in the "
           "Tracks and Channels widgets. This automatically assigns each track "
           "to the correct MIDI channel and sets program-change events based on "
           "the track's instrument name so FFXIV soundfonts play back correctly.<br><br>"
           "Supported instruments (including common aliases): Harp, Piano, Lute, "
           "Fiddle, Flute, Oboe, Clarinet, Fife, Panpipes, Timpani, Bongo, "
           "BassDrum, SnareDrum, Cymbal, Trumpet, Trombone, Tuba, Horn, "
           "Saxophone, Violin, Viola, Cello, DoubleBass, "
           "ElectricGuitarOverdriven, ElectricGuitarClean, "
           "ElectricGuitarMuted, ElectricGuitarPowerChords, "
           "& ElectricGuitarSpecial."),
        _ffxivGroup);

    ffxivDesc->setTextFormat(Qt::RichText);
    ffxivDesc->setWordWrap(true);

    ffxivDesc->setStyleSheet("color: gray; font-size: 11px; margin-left: 10px;");
    ffxivLayout->addWidget(ffxivDesc, 1, 0, 1, 2);

    _ffxivInstrumentNamesBox = new QCheckBox(tr("Use FFXIV Instrument Names"), _ffxivGroup);
    _ffxivInstrumentNamesBox->setChecked(_settings->value("game_support/ffxiv_instrument_names", false).toBool());
    connect(_ffxivInstrumentNamesBox, SIGNAL(toggled(bool)), this, SLOT(onFFXIVInstrumentNamesToggled(bool)));
    ffxivLayout->addWidget(_ffxivInstrumentNamesBox, 2, 0, 1, 2);

    QLabel *instrNamesDesc = new QLabel(
        tr("Sets custom instrument names in the <b>Instruments</b> settings "
           "to match FFXIV instrument names (e.g. Harp, Lute, Electric Guitar: Clean). "
           "You can further customize individual names after enabling this option."),
        _ffxivGroup);
    instrNamesDesc->setTextFormat(Qt::RichText);
    instrNamesDesc->setWordWrap(true);
    instrNamesDesc->setStyleSheet("color: gray; font-size: 11px; margin-left: 10px;");
    ffxivLayout->addWidget(instrNamesDesc, 3, 0, 1, 2);

    mainLayout->addWidget(_ffxivGroup);
    mainLayout->addStretch();
}

void GameSupportSettingsWidget::onFFXIVBoxToggled(bool checked) {
    // Write directly to settings so MainWindow can read it via isFFXIVEnabled
    // This allows immediate UI updates without waiting for "OK" to be clicked
    _settings->setValue("game_support/ffxiv_enabled", checked);
    emit immediateSettingsChanged(checked);
}

void GameSupportSettingsWidget::onFFXIVInstrumentNamesToggled(bool checked) {
    _settings->setValue("game_support/ffxiv_instrument_names", checked);
    applyFFXIVInstrumentNames(checked);
    emit instrumentNamesChanged();
}

void GameSupportSettingsWidget::applyFFXIVInstrumentNames(bool enable) {
    // FFXIV canonical instrument names and their GM program numbers
    struct FFXIVInstrument {
        int program;
        const char *displayName;
    };

    static const FFXIVInstrument ffxivInstruments[] = {
        // Chordophones
        { 46, "Harp" },
        {  0, "Piano" },
        { 25, "Lute" },
        { 45, "Fiddle" },
        // Woodwinds
        { 73, "Flute" },
        { 68, "Oboe" },
        { 71, "Clarinet" },
        { 72, "Fife" },
        { 75, "Panpipes" },
        // Percussion
        { 47, "Timpani" },
        { 96, "Bongo" },
        { 97, "Bass Drum" },
        { 98, "Snare Drum" },
        { 99, "Cymbal" },
        // Brass
        { 56, "Trumpet" },
        { 57, "Trombone" },
        { 58, "Tuba" },
        { 60, "Horn" },
        { 65, "Saxophone" },
        // Strings
        { 40, "Violin" },
        { 41, "Viola" },
        { 42, "Cello" },
        { 43, "Double Bass" },
        // Guitars
        { 29, "Electric Guitar: Overdriven" },
        { 27, "Electric Guitar: Clean" },
        { 28, "Electric Guitar: Muted" },
        { 30, "Electric Guitar: Power Chords" },
        { 31, "Electric Guitar: Special" },
    };

    InstrumentDefinitions *defs = InstrumentDefinitions::instance();

    for (const auto &inst : ffxivInstruments) {
        if (enable) {
            defs->setInstrumentName(inst.program, QString::fromLatin1(inst.displayName));
        } else {
            // Clear the override (empty string removes the override, reverting to GM default)
            defs->setInstrumentName(inst.program, QString());
        }
    }

    // Save immediately to registry
    defs->saveOverrides(_settings);
}

bool GameSupportSettingsWidget::accept() {
    bool oldValue = _settings->value("game_support/ffxiv_enabled", false).toBool();
    bool newValue = _ffxivEnabledBox->isChecked();
    if (oldValue != newValue) {
        _settings->setValue("game_support/ffxiv_enabled", newValue);
        emit settingsChanged();
    }

    return true;
}

QIcon GameSupportSettingsWidget::icon() {
    return QIcon();
}

bool GameSupportSettingsWidget::isFFXIVEnabled(QSettings *settings) {
    return settings->value("game_support/ffxiv_enabled", false).toBool();
}
