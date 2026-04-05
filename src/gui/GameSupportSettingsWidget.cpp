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

GameSupportSettingsWidget::GameSupportSettingsWidget(QSettings *settings, QWidget *parent)
    : SettingsWidget(tr("Game Support"), parent)
      , _settings(settings) {

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    setLayout(mainLayout);

    // --- Final Fantasy XIV Section ---
    _ffxivGroup = new QGroupBox(tr("Final Fantasy XIV"), this);
    QGridLayout *ffxivLayout = new QGridLayout(_ffxivGroup);

    _ffxivEnabledBox = new QCheckBox(tr("Enable FFXIV Bard Performance Support"), _ffxivGroup);
    _ffxivEnabledBox->setChecked(_settings->value("game_support/ffxiv_enabled", false).toBool());
    _ffxivEnabledBox->setToolTip(tr("Adds FFXIV-specific tools to the Tracks and Channels panels."));
    ffxivLayout->addWidget(_ffxivEnabledBox, 0, 0, 1, 2);

    QLabel *ffxivDesc = new QLabel(
        tr("When enabled, a <b>Fix XIV Channels</b> button appears in the "
           "Tracks and Channels widgets. This automatically assigns each track "
           "to the correct MIDI channel and sets program-change events based on "
           "the track's instrument name so FFXIV soundfonts play back correctly.\n\n"
           "Supported instruments: Harp, Piano, Lute, Fiddle, Flute, Oboe, "
           "Clarinet, Fife, Panpipes, Timpani, Bongo, Bass Drum, Snare Drum, "
           "Cymbal, Trumpet, Trombone, Tuba, Horn, Saxophone, Violin, Viola, "
           "Cello, Double Bass, and all Electric Guitar variants.\n\n"
           "Track names also resolve with common aliases."),
        _ffxivGroup);
    ffxivDesc->setWordWrap(true);
    ffxivDesc->setStyleSheet("color: gray; font-size: 11px; margin-left: 10px;");
    ffxivLayout->addWidget(ffxivDesc, 1, 0, 1, 2);

    mainLayout->addWidget(_ffxivGroup);
    mainLayout->addStretch();
}

bool GameSupportSettingsWidget::accept() {
    _settings->setValue("game_support/ffxiv_enabled", _ffxivEnabledBox->isChecked());
    return true;
}

QIcon GameSupportSettingsWidget::icon() {
    return QIcon();
}

bool GameSupportSettingsWidget::isFFXIVEnabled(QSettings *settings) {
    return settings->value("game_support/ffxiv_enabled", false).toBool();
}
