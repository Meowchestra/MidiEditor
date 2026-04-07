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

#ifndef FFXIVCHANNELFIXER_H
#define FFXIVCHANNELFIXER_H

#include <QJsonObject>
#include <QJsonArray>
#include <QString>
#include <QHash>
#include <QList>
#include <functional>
#include <QCoreApplication>

class MidiFile;
class MidiTrack;

/**
 * \class FFXIVChannelFixer
 *
 * \brief Fixes channel assignments and program changes for FFXIV Bard
 *        Performance MIDI files.
 *
 * Resolves track instrument names (including aliases and various naming
 * conventions) to their canonical FFXIV instrument names, then assigns
 * each track to the correct MIDI channel with the proper General MIDI
 * program number for FFXIV soundfont playback.
 *
 * Channel assignment rules:
 *  1. Track N → Channel N  (T0→CH0, T1→CH1, …)
 *  2. Percussion tracks (Bass Drum, Snare Drum, Cymbal, Bongo) → CH9.
 *     Timpani follows Rule 1 (it is tonal).
 *  3. Guitar tracks follow Rule 1; extra variants get free channels.
 *  4. A program_change is inserted at tick 0 for every used channel.
 *  5. All events are migrated to the correct channel.
 */
class FFXIVChannelFixer {
    Q_DECLARE_TR_FUNCTIONS(FFXIVChannelFixer)
public:
    /**
     * \brief Resolve a track name (with aliases, spaces, suffixes) to
     *        the canonical FFXIV instrument name.
     *
     * Examples:
     *   "Snare Drum+1"       → "SnareDrum"
     *   "OrchestralHarp"     → "Harp"
     *   "Electric Guitar: Clean" → "ElectricGuitarClean"
     *   "PowerChords"        → "ElectricGuitarPowerChords"
     *
     * \param trackName  The raw track name from the MIDI file
     * \return Canonical instrument name, or empty string if unrecognised
     */
    static QString resolveInstrument(const QString &trackName);

    /**
     * \brief Get the FFXIV program number for a canonical instrument name.
     * \param canonicalName  A name returned by resolveInstrument()
     * \return GM program number (0-127), or -1 if unknown
     */
    static int programForInstrument(const QString &canonicalName);

    /**
     * \brief Check whether a canonical instrument is a percussion instrument.
     *
     * Percussion instruments are routed to channel 9.
     * Note: Timpani is tonal and is NOT percussion.
     */
    static bool isPercussion(const QString &canonicalName);

    /**
     * \brief Check whether a canonical instrument is an electric guitar variant.
     */
    static bool isGuitar(const QString &canonicalName);

    /**
     * \brief Return a list of all known canonical instrument names.
     */
    static QStringList allInstrumentNames();

    /**
     * \brief Analyse the file without modifying it.
     * \param file  The loaded MidiFile
     * \return JSON with trackCount, ffxivTrackCount, instrument list, etc.
     */
    static QJsonObject analyzeFile(MidiFile *file);

    /// Progress callback: (percent 0-100, phase description)
    using ProgressCallback = std::function<void(int, const QString &)>;

    /// Options for the fix operation
    struct FixOptions {
        bool cleanupControlChanges = true;
        bool cleanupKeyPressure = true;
        bool cleanupChannelPressure = true;
        bool cleanupPitchBend = true;
        bool normalizeVelocity = true;
    };

    /**
     * \brief Fix all channel assignments and program_change events.
     * \param file       The loaded MidiFile
     * \param forcedTier 0=Auto (Detect), 2=Rebuild (Full Reassignment), 3=Preserve (Minimal Changes)
     * \param options    Cleanup and normalization options
     * \param progress   Optional callback for progress updates
     * \return JSON result with success, tier, channelMap, summary
     */
    static QJsonObject fixChannels(MidiFile *file,
                                   int forcedTier = 0,
                                   FixOptions options = FixOptions(),
                                   ProgressCallback progress = nullptr);

private:
    /// Strip trailing [+-]\d+$ suffix from a track name.
    static QString stripSuffix(const QString &name);

    /// Build and return the alias → canonical name map (lazy-initialised).
    static const QHash<QString, QString> &aliasMap();
};

#endif // FFXIVCHANNELFIXER_H
