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

#ifndef DRUMKITPRESET_H_
#define DRUMKITPRESET_H_

#include <QString>
#include <QList>
#include <QMap>

/**
 * \brief A single note mapping: source GM drum note → target melodic note.
 */
struct DrumNoteMapping {
    int sourceNote;      ///< GM drum note number (e.g., 35 = Kick Drum 2)
    int targetNote;      ///< Target note after transposition
    QString drumName;    ///< GM drum instrument name (e.g., "Kick Drum 2")
};

/**
 * \brief A group of drum notes assigned to one output track.
 *
 * Each group corresponds to an instrument in the target system.
 * For example, "BassDrum" receives kick drums and toms.
 */
struct DrumTrackGroup {
    QString trackName;                ///< Display name (e.g., "BassDrum")
    int programNumber;                ///< MIDI program for the target channel (-1 = none)
    bool staysOnPercussionChannel;    ///< If true, stays on channel 9 (e.g., Drumkit Rest)
    QList<DrumNoteMapping> mappings;  ///< Note mappings for this group
};

/**
 * \brief A complete drum kit preset defining how to split and transpose
 *        percussion notes into grouped melodic tracks.
 */
struct DrumKitPreset {
    QString name;                     ///< Preset display name (e.g., "Mog Amp")
    QList<DrumTrackGroup> groups;     ///< Track groups with their mappings
};

/**
 * \class DrumKitPresetManager
 *
 * \brief Manages drum kit presets: provides defaults, loads/saves user edits.
 *
 * Three default presets are provided (Mog Amp, Bard Forge 1, Bard Forge 2).
 * Users can edit target notes and save changes via QSettings. Resetting
 * restores the original default values.
 */
class DrumKitPresetManager {
public:
    /// Returns the singleton instance.
    static DrumKitPresetManager &instance();

    /// Returns the list of all available preset names.
    QStringList presetNames() const;

    /// Returns a preset by name. If user has saved edits, those are returned.
    DrumKitPreset preset(const QString &name) const;

    /// Saves user edits for a preset (persists to QSettings).
    void savePreset(const DrumKitPreset &preset);

    /// Resets a preset to its default values (removes user edits).
    void resetPreset(const QString &name);

    /// Returns the GM drum instrument name for a given note number (27-87).
    static QString gmDrumName(int note);

    /// Returns all GM drum note numbers (27-87).
    static QList<int> allGmDrumNotes();

private:
    DrumKitPresetManager();
    void initDefaults();

    QList<DrumKitPreset> _defaults;
    QMap<QString, DrumKitPreset> _userEdits;
};

#endif // DRUMKITPRESET_H_
