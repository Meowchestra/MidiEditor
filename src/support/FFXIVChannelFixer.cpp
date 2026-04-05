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

#include "FFXIVChannelFixer.h"

#include "../midi/MidiFile.h"
#include "../midi/MidiTrack.h"
#include "../midi/MidiChannel.h"
#include "../MidiEvent/MidiEvent.h"
#include "../MidiEvent/NoteOnEvent.h"
#include "../MidiEvent/OffEvent.h"
#include "../MidiEvent/ProgChangeEvent.h"

#include <QRegularExpression>
#include <QSet>
#include <algorithm>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

QString FFXIVChannelFixer::stripSuffix(const QString &name) {
    QString base = name;
    static const QRegularExpression suffixRe(QStringLiteral("[+-]\\d+$"));
    base.remove(suffixRe);
    return base.trimmed();
}

// ---------------------------------------------------------------------------
// Alias map  — built once, maps every known alias (lower-cased, no spaces)
//              to the canonical instrument name.
// ---------------------------------------------------------------------------

const QHash<QString, QString> &FFXIVChannelFixer::aliasMap() {
    static QHash<QString, QString> map;
    if (!map.isEmpty())
        return map;

    auto reg = [&](const QString &canonical, const QStringList &aliases) {
        map[canonical.toLower()] = canonical;
        for (const QString &a : aliases)
            map[a.toLower()] = canonical;
    };

    // --- Chordophones (Strummed) ---
    reg(QStringLiteral("Harp"), {"harps", "orchestralharp", "orchestralharps"});
    reg(QStringLiteral("Piano"), {"pianos", "acousticgrandpiano", "acousticgrandpianos"});
    reg(QStringLiteral("Lute"), {"lutes"});
    reg(QStringLiteral("Fiddle"), {"fiddles", "pizzicato", "pizzicatostrings"});

    // --- Woodwinds ---
    reg(QStringLiteral("Flute"), {"flutes"});
    reg(QStringLiteral("Oboe"), {"oboes"});
    reg(QStringLiteral("Clarinet"), {"clarinets"});
    reg(QStringLiteral("Fife"), {"fifes", "piccolo", "piccolos", "ocarina", "ocarinas"});
    reg(QStringLiteral("Panpipes"), {"panpipe", "panflute", "panflutes"});

    // --- Percussion ---
    reg(QStringLiteral("Timpani"), {"timpanis"});
    reg(QStringLiteral("Bongo"), {"bongos"});
    reg(QStringLiteral("BassDrum"), {"bassdrums", "kick"});
    reg(QStringLiteral("SnareDrum"), {"snaredrums", "snare", "ricedrum"});
    reg(QStringLiteral("Cymbal"), {"cymbals"});

    // --- Brass ---
    reg(QStringLiteral("Trumpet"), {"trumpets"});
    reg(QStringLiteral("Trombone"), {"trombones"});
    reg(QStringLiteral("Tuba"), {"tubas"});
    reg(QStringLiteral("Horn"), {"horns", "frenchhorn", "frenchhorns"});
    reg(QStringLiteral("Saxophone"), {"saxophones", "sax", "altosaxophone", "altosax", "saxaphone"});

    // --- Strings ---
    reg(QStringLiteral("Violin"), {"violins"});
    reg(QStringLiteral("Viola"), {"violas"});
    reg(QStringLiteral("Cello"), {"cellos", "violoncello", "violoncellos"});
    reg(QStringLiteral("DoubleBass"), {"contrabass", "bass"});

    return map;
}

// ---------------------------------------------------------------------------
// Public helpers
// ---------------------------------------------------------------------------

QString FFXIVChannelFixer::resolveInstrument(const QString &trackName) {
    QString cleanName = stripSuffix(trackName).toLower().remove(' ').remove(':').remove('-').remove('_');

    if (cleanName.contains(QStringLiteral("guitar")) || cleanName.contains(QStringLiteral("powerchord"))) {
        if (cleanName.contains(QStringLiteral("clean"))) return QStringLiteral("ElectricGuitarClean");
        if (cleanName.contains(QStringLiteral("muted")) || cleanName.contains(QStringLiteral("mute"))) return QStringLiteral("ElectricGuitarMuted");
        if (cleanName.contains(QStringLiteral("overdriven")) || cleanName.contains(QStringLiteral("overdrive"))) return QStringLiteral("ElectricGuitarOverdriven");
        if (cleanName.contains(QStringLiteral("powerchord")) || cleanName.contains(QStringLiteral("power"))) return QStringLiteral("ElectricGuitarPowerChords");
        if (cleanName.contains(QStringLiteral("special"))) return QStringLiteral("ElectricGuitarSpecial");
        if (cleanName.contains(QStringLiteral("program"))) return QStringLiteral("ProgramElectricGuitar");
        return QStringLiteral("Lute"); // Generic guitar maps to Lute
    }

    const auto &map = aliasMap();
    auto it = map.find(cleanName);
    if (it != map.end())
        return it.value();

    return {};
}

int FFXIVChannelFixer::programForInstrument(const QString &canonicalName) {
    static const QHash<QString, int> map = {
        // Chordophones
        {"Harp", 46}, {"Piano", 0},  {"Lute", 25}, {"Fiddle", 45},
        // Woodwinds
        {"Flute", 73}, {"Oboe", 68}, {"Clarinet", 71}, {"Fife", 72}, {"Panpipes", 75},
        // Percussion
        {"Timpani", 47}, {"Bongo", 96}, {"BassDrum", 97}, {"SnareDrum", 98}, {"Cymbal", 99},
        // Brass
        {"Trumpet", 56}, {"Trombone", 57}, {"Tuba", 58}, {"Horn", 60}, {"Saxophone", 65},
        // Strings
        {"Violin", 40}, {"Viola", 41}, {"Cello", 42}, {"DoubleBass", 43},
        // Electric Guitar
        {"ElectricGuitarOverdriven", 29}, {"ElectricGuitarClean", 27},
		{"ElectricGuitarMuted", 28}, {"ElectricGuitarPowerChords", 30},
		{"ElectricGuitarSpecial", 31},
    };
    return map.value(canonicalName, -1);
}

bool FFXIVChannelFixer::isPercussion(const QString &canonicalName) {
    // Note: FFXIV Bard soundfonts map drums to normal melodic program slots (96-99)
    // rather than relying on GM channel 9 routing. So drums are treated as
    // standard tracks and won't be forced to channel 9.
    Q_UNUSED(canonicalName);
    return false;
}

bool FFXIVChannelFixer::isGuitar(const QString &canonicalName) {
    // return true for specific variants so we reserve free channels for them
    return canonicalName.startsWith(QStringLiteral("ElectricGuitar"));
}

QStringList FFXIVChannelFixer::allInstrumentNames() {
    return {
        // Chordophones
        "Harp", "Piano", "Lute", "Fiddle",
        // Woodwinds
        "Flute", "Oboe", "Clarinet", "Fife", "Panpipes",
        // Percussion
        "Timpani", "Bongo", "BassDrum", "SnareDrum", "Cymbal",
        // Brass
        "Trumpet", "Trombone", "Tuba", "Horn", "Saxophone",
        // Strings
        "Violin", "Viola", "Cello", "DoubleBass",
        // Electric Guitar
        "ElectricGuitarOverdriven", "ElectricGuitarClean",
        "ElectricGuitarMuted", "ElectricGuitarPowerChords",
        "ElectricGuitarSpecial",
    };
}

// ---------------------------------------------------------------------------
// analyzeFile — read-only scan for Tier detection
// ---------------------------------------------------------------------------

QJsonObject FFXIVChannelFixer::analyzeFile(MidiFile *file) {
    QJsonObject result;
    if (!file || file->numTracks() == 0) {
        result["valid"] = false;
        return result;
    }

    const int trackCount = file->numTracks();
    int ffxivTrackCount = 0;
    bool hasGuitar = false;
    QStringList guitarVariants, percussionTracks, melodicTracks;

    for (int t = 0; t < trackCount; t++) {
        QString canonical = resolveInstrument(file->track(t)->name());
        if (canonical.isEmpty())
            continue;
        ffxivTrackCount++;

        if (isGuitar(canonical)) {
            hasGuitar = true;
            if (!guitarVariants.contains(canonical))
                guitarVariants.append(canonical);
        } else if (isPercussion(canonical)) {
            if (!percussionTracks.contains(canonical))
                percussionTracks.append(canonical);
        } else {
            if (!melodicTracks.contains(canonical))
                melodicTracks.append(canonical);
        }
    }

    // Count existing program changes for Tier detection
    int totalProgramChanges = 0;
    bool hasGuitarPCs = false;
    for (int ch = 0; ch < 16; ch++) {
        MidiChannel *channel = file->channel(ch);
        if (!channel) continue;
        auto *eventMap = channel->eventMap();
        for (auto it = eventMap->begin(); it != eventMap->end(); ++it) {
            auto *pc = dynamic_cast<ProgChangeEvent *>(it.value());
            if (!pc) continue;
            totalProgramChanges++;
            int p = pc->program();
            if (p >= 27 && p <= 31) hasGuitarPCs = true;
        }
    }

    // Auto-detect tier
    int autoTier = (ffxivTrackCount > 0) ? 2 : 1;
    if (autoTier == 2 && hasGuitar && hasGuitarPCs)
        autoTier = 3;

    result["valid"]               = (ffxivTrackCount > 0);
    result["trackCount"]          = trackCount;
    result["ffxivTrackCount"]    = ffxivTrackCount;
    result["hasGuitar"]           = hasGuitar;
    result["totalProgramChanges"] = totalProgramChanges;
    result["autoDetectedTier"]    = autoTier;
    result["guitarVariants"]      = QJsonArray::fromStringList(guitarVariants);
    result["percussionTracks"]    = QJsonArray::fromStringList(percussionTracks);
    result["melodicTracks"]       = QJsonArray::fromStringList(melodicTracks);
    return result;
}

// ---------------------------------------------------------------------------
// fixChannels — tiered reassignment and preservation
// ---------------------------------------------------------------------------

QJsonObject FFXIVChannelFixer::fixChannels(MidiFile *file, int forcedTier, ProgressCallback progress) {
    auto report = [&](int pct, const QString &msg) {
        if (progress) progress(pct, msg);
    };

    QJsonObject result;
    if (!file) {
        result["success"] = false;
        result["error"]   = QStringLiteral("No file loaded.");
        return result;
    }

    const int trackCount = file->numTracks();
    if (trackCount == 0) {
        result["success"] = false;
        result["error"]   = QStringLiteral("No tracks in file.");
        return result;
    }

    // All 5 guitar variant names
    static const QStringList allGuitarVariants = {
        QStringLiteral("ElectricGuitarOverdriven"),
        QStringLiteral("ElectricGuitarClean"),
        QStringLiteral("ElectricGuitarMuted"),
        QStringLiteral("ElectricGuitarPowerChords"),
        QStringLiteral("ElectricGuitarSpecial"),
    };

    // -----------------------------------------------------------------
    // 1. Resolve Instrument Names and Analyze Guitar Progs
    // -----------------------------------------------------------------
    report(5, QStringLiteral("Analyzing tracks..."));

    QVector<QString> canonicalNames(trackCount);
    int ffxivTrackCount = 0;
    bool hasGuitar = false;
    QSet<QString> guitarVariantsPresent;

    for (int t = 0; t < trackCount; t++) {
        QString canonical = resolveInstrument(file->track(t)->name());
        canonicalNames[t] = canonical;
        if (!canonical.isEmpty())
            ffxivTrackCount++;
        if (isGuitar(canonical)) {
            hasGuitar = true;
            guitarVariantsPresent.insert(canonical);
        }
    }

    if (ffxivTrackCount == 0) {
        result["success"] = false;
        result["error"]   = QStringLiteral("No FFXIV instrument names detected. Track names must match FFXIV instruments.");
        result["tier"]    = 1;
        return result;
    }

    // Scan guitar program changes on all channels
    QHash<int, int> guitarChToProgram;
    if (hasGuitar) {
        for (int ch = 0; ch < 16; ch++) {
            MidiChannel *channel = file->channel(ch);
            if (!channel) continue;
            auto *eventMap = channel->eventMap();
            for (auto it = eventMap->begin(); it != eventMap->end(); ++it) {
                if (auto *pc = dynamic_cast<ProgChangeEvent *>(it.value())) {
                    int p = pc->program();
                    if (p >= 27 && p <= 31) {
                        if (!guitarChToProgram.contains(ch))
                            guitarChToProgram[ch] = p;
                    }
                }
            }
        }
    }

    // -----------------------------------------------------------------
    // 2. TIER DETECTION: Rebuild (2) vs Preserve (3)
    // -----------------------------------------------------------------
    report(15, QStringLiteral("Detecting mode..."));

    bool isPreserveMode = false;
    // Default detection: if guitar PCs exist, it's likely configured (Tier 3)
    if (hasGuitar && !guitarChToProgram.isEmpty()) isPreserveMode = true;

    // Manual override if forced
    if (forcedTier == 2) isPreserveMode = false;
    else if (forcedTier == 3) isPreserveMode = true;

    int activeTier = isPreserveMode ? 3 : 2;

    // -----------------------------------------------------------------
    // 3. Build channel assignment map
    // -----------------------------------------------------------------
    report(20, QStringLiteral("Building channel map..."));

    QVector<int> channelFor(trackCount, -1);
    QSet<int> usedChannels;
    QHash<QString, int> guitarChannelMap;
    QSet<int> dupGuitarTracks;

    if (isPreserveMode) {
        // TIER 3: Notes are ground truth for channel mapping
        if (hasGuitar) {
            for (int t = 0; t < trackCount; t++) {
                if (!isGuitar(canonicalNames[t])) continue;

                // Duplicate variant: reuse first occurrence's channel
                if (guitarChannelMap.contains(canonicalNames[t])) {
                    channelFor[t] = guitarChannelMap[canonicalNames[t]];
                    usedChannels.insert(channelFor[t]);
                    dupGuitarTracks.insert(t);
                    continue;
                }

                MidiTrack *track = file->track(t);
                QHash<int, int> chCount;
                for (int ch = 0; ch < 16; ch++) {
                    MidiChannel *channel = file->channel(ch);
                    if (!channel) continue;
                    auto *eventMap = channel->eventMap();
                    for (auto it = eventMap->begin(); it != eventMap->end(); ++it) {
                        if (it.value()->track() == track && dynamic_cast<NoteOnEvent*>(it.value()))
                            chCount[ch]++;
                    }
                }
                int bestCh = t, bestCount = 0;
                for (auto it = chCount.begin(); it != chCount.end(); ++it) {
                    if (it.value() > bestCount) { bestCount = it.value(); bestCh = it.key(); }
                }
                channelFor[t] = bestCh;
                guitarChannelMap[canonicalNames[t]] = bestCh;
                usedChannels.insert(bestCh);
            }

            // Fill missing variants from existing PCs
            for (auto it = guitarChToProgram.begin(); it != guitarChToProgram.end(); ++it) {
                int ch = it.key();
                int prog = it.value();
                for (const QString &v : allGuitarVariants) {
                    if (programForInstrument(v) == prog && !guitarChannelMap.contains(v)) {
                        guitarChannelMap[v] = ch;
                        usedChannels.insert(ch);
                        break;
                    }
                }
            }
        }

        // Channel for non-guitar tracks: also used note distribution
        for (int t = 0; t < trackCount; t++) {
            if (isGuitar(canonicalNames[t])) continue;
            if (isPercussion(canonicalNames[t])) {
                channelFor[t] = 9;
                usedChannels.insert(9);
                continue;
            }
            MidiTrack *track = file->track(t);
            QHash<int, int> chCount;
            for (int ch = 0; ch < 16; ch++) {
                MidiChannel *channel = file->channel(ch);
                if (!channel) continue;
                auto *eventMap = channel->eventMap();
                for (auto it = eventMap->begin(); it != eventMap->end(); ++it) {
                    if (it.value()->track() == track && dynamic_cast<NoteOnEvent*>(it.value()))
                        chCount[ch]++;
                }
            }
            int bestCh = t, bestCount = 0;
            for (auto it = chCount.begin(); it != chCount.end(); ++it) {
                if (it.value() > bestCount) { bestCount = it.value(); bestCh = it.key(); }
            }
            channelFor[t] = bestCh;
            usedChannels.insert(bestCh);
        }
    } else {
        // TIER 2: Fresh reassignment by track index
        for (int t = 0; t < trackCount; t++) {
            if (isPercussion(canonicalNames[t])) {
                channelFor[t] = 9;
            } else if (hasGuitar && isGuitar(canonicalNames[t]) && guitarChannelMap.contains(canonicalNames[t])) {
                channelFor[t] = guitarChannelMap[canonicalNames[t]];
            } else {
                int ch = t;
                if (ch > 15) ch = 15;
                channelFor[t] = ch;
                if (hasGuitar && isGuitar(canonicalNames[t]))
                    guitarChannelMap[canonicalNames[t]] = ch;
            }
            usedChannels.insert(channelFor[t]);
        }
    }

    // Reserve free channels for missing guitar variants
    if (hasGuitar) {
        auto nextFreeChannel = [&]() -> int {
            for (int ch = 0; ch <= 15; ch++) {
                if (!usedChannels.contains(ch)) return ch;
            }
            return -1;
        };
        for (const QString &variant : allGuitarVariants) {
            if (!guitarChannelMap.contains(variant)) {
                int ch = nextFreeChannel();
                if (ch >= 0) {
                    guitarChannelMap[variant] = ch;
                    usedChannels.insert(ch);
                }
            }
        }
    }

    QSet<int> allGuitarChs;
    if (hasGuitar) {
        for (auto it = guitarChannelMap.begin(); it != guitarChannelMap.end(); ++it)
            allGuitarChs.insert(it.value());
    }

    // -----------------------------------------------------------------
    // 4. CLEANUP: Remove Program Changes
    // -----------------------------------------------------------------
    report(35, QStringLiteral("Removing old program changes..."));

    int removedPcCount = 0;
    for (int ch = 0; ch < 16; ch++) {
        // Tier 3: only remove PCs on guitar channels (preserve non-guitar)
        if (isPreserveMode && !allGuitarChs.contains(ch)) continue;
        
        MidiChannel *channel = file->channel(ch);
        if (!channel) continue;
        QList<MidiEvent *> toRemove;
        auto *eventMap = channel->eventMap();
        for (auto it = eventMap->begin(); it != eventMap->end(); ++it) {
            if (dynamic_cast<ProgChangeEvent *>(it.value()))
                toRemove.append(it.value());
        }
        for (MidiEvent *ev : toRemove)
            channel->removeEvent(ev);
        removedPcCount += toRemove.size();
    }

    // -----------------------------------------------------------------
    // 5. MIGRATE: Move events to channels
    // -----------------------------------------------------------------
    report(50, QStringLiteral("Migrating events..."));

    if (!isPreserveMode) {
        // Tier 2: full migration
        for (int t = 0; t < trackCount; t++) {
            int targetCh = channelFor[t];
            MidiTrack *track = file->track(t);

            struct EventInfo { MidiEvent *ev; int currentCh; };
            QList<EventInfo> trackEvents;

            for (int ch = 0; ch < 16; ch++) {
                if (ch == targetCh) continue;
                MidiChannel *channel = file->channel(ch);
                if (!channel) continue;
                QMultiMap<int, MidiEvent *> *eventMap = channel->eventMap();
                for (auto it = eventMap->begin(); it != eventMap->end(); ++it) {
                    MidiEvent *ev = it.value();
                    if (ev->track() != track) continue;
                    if (dynamic_cast<ProgChangeEvent *>(ev)) continue;
                    if (dynamic_cast<OffEvent *>(ev)) continue;
                    trackEvents.append({ev, ch});
                }
            }

            for (const auto &info : trackEvents) {
                info.ev->moveToChannel(targetCh);
            }
            track->assignChannel(targetCh);
        }
    } else {
        // Tier 3: migrate only duplicate guitar tracks
        for (int t : dupGuitarTracks) {
            int targetCh = channelFor[t];
            MidiTrack *track = file->track(t);

            struct EventInfo { MidiEvent *ev; int currentCh; };
            QList<EventInfo> trackEvents;

            for (int ch = 0; ch < 16; ch++) {
                if (ch == targetCh) continue;
                MidiChannel *channel = file->channel(ch);
                if (!channel) continue;
                QMultiMap<int, MidiEvent *> *eventMap = channel->eventMap();
                for (auto it = eventMap->begin(); it != eventMap->end(); ++it) {
                    MidiEvent *ev = it.value();
                    if (ev->track() != track) continue;
                    if (dynamic_cast<ProgChangeEvent *>(ev)) continue;
                    if (dynamic_cast<OffEvent *>(ev)) continue;
                    trackEvents.append({ev, ch});
                }
            }

            for (const auto &info : trackEvents) {
                info.ev->moveToChannel(targetCh);
            }
            track->assignChannel(targetCh);
        }
        
        // Ensure other tracks reflect their assignments in the track object too
        for (int t = 0; t < trackCount; t++) {
            if (!dupGuitarTracks.contains(t))
                file->track(t)->assignChannel(channelFor[t]);
        }
    }

    // -----------------------------------------------------------------
    // 6. PROGRAM: Insert PCs at Tick 0
    // -----------------------------------------------------------------
    report(75, QStringLiteral("Inserting program changes..."));

    struct ChannelProgram { int channel; int program; };
    QList<ChannelProgram> channelPrograms;

    // Non-guitar tracks: only in Tier 2
    if (!isPreserveMode) {
        for (int t = 0; t < trackCount; t++) {
            if (isGuitar(canonicalNames[t])) continue;
            int prog = programForInstrument(canonicalNames[t]);
            if (prog >= 0) channelPrograms.append({channelFor[t], prog});
        }
    }

    if (hasGuitar) {
        for (auto it = guitarChannelMap.begin(); it != guitarChannelMap.end(); ++it) {
            int prog = programForInstrument(it.key());
            if (prog >= 0) channelPrograms.append({it.value(), prog});
        }
    }

    // Insert on one track (Rule: program changes are global to the channel)
    for (const auto &cp : channelPrograms) {
        auto *pc = new ProgChangeEvent(cp.channel, cp.program, file->track(0));
        file->channel(cp.channel)->insertEvent(pc, 0);
    }

    // -----------------------------------------------------------------
    // 7. SWITCH: Process guitar variant switches mid-sequence
    // -----------------------------------------------------------------
    report(90, QStringLiteral("Processing guitar switches..."));

    int switchCount = 0;
    if (hasGuitar) {
        QHash<int, QString> chToVariant;
        for (auto it = guitarChannelMap.begin(); it != guitarChannelMap.end(); ++it)
            chToVariant[it.value()] = it.key();

        for (int t = 0; t < trackCount; t++) {
            if (!isGuitar(canonicalNames[t])) continue;
            MidiTrack *track = file->track(t);

            struct NoteInfo { int tick; int channel; };
            QList<NoteInfo> notes;

            for (int ch : allGuitarChs) {
                MidiChannel *channel = file->channel(ch);
                if (!channel) continue;
                auto *eventMap = channel->eventMap();
                for (auto it = eventMap->begin(); it != eventMap->end(); ++it) {
                    if (it.value()->track() == track && dynamic_cast<NoteOnEvent*>(it.value()))
                        notes.append({it.key(), ch});
                }
            }

            std::sort(notes.begin(), notes.end(), [](const NoteInfo &a, const NoteInfo &b) {
                return a.tick < b.tick;
            });

            int lastCh = -1;
            for (const auto &n : notes) {
                if (n.channel != lastCh) {
                    if (lastCh != -1 && n.tick > 0) {
                        int prog = programForInstrument(chToVariant[n.channel]);
                        if (prog >= 0) {
                            auto *pc = new ProgChangeEvent(n.channel, prog, track);
                            file->channel(n.channel)->insertEvent(pc, n.tick);
                            switchCount++;
                        }
                    }
                    lastCh = n.channel;
                }
            }
        }
    }

    // -----------------------------------------------------------------
    // 8. VELOCITY: Normalize velocities (Main repo default is 100)
    // -----------------------------------------------------------------
    report(95, QStringLiteral("Normalizing velocity..."));

    int velocityChangedCount = 0;
    for (int ch = 0; ch < 16; ch++) {
        MidiChannel *channel = file->channel(ch);
        if (!channel) continue;
        auto *eventMap = channel->eventMap();
        for (auto it = eventMap->begin(); it != eventMap->end(); ++it) {
            if (auto *noteOn = dynamic_cast<NoteOnEvent *>(it.value())) {
                if (noteOn->velocity() > 0 && noteOn->velocity() != 100) {
                    noteOn->setVelocity(100);
                    velocityChangedCount++;
                }
            }
        }
    }

    report(100, QStringLiteral("Done!"));

    result["success"] = true;
    result["mode"] = (activeTier == 2) ? tr("Rebuild") : tr("Preserve");
    
    QString summaryText = tr("FFXIV Channel Fixer complete.\n\n"
                             "Mode: %1\n"
                             "Tracks processed: %2 (%3 FFXIV tracks matched)\n\n"
                             "Details:\n"
                             "- Reassigned channels: %4\n"
                             "- Fixed program changes: %5\n"
                             "- Velocity normalized: %6\n")
                             .arg((activeTier == 2) ? tr("Rebuild (Full Reassignment)") : tr("Preserve (Minimal Changes)"))
                             .arg(trackCount)
                             .arg(ffxivTrackCount)
                             .arg((activeTier == 2) ? tr("%1 channels").arg(ffxivTrackCount) : tr("Configuration preserved"))
                             .arg(removedPcCount + switchCount)
                             .arg(velocityChangedCount);
    
    result["summary"] = summaryText;

    result["trackCount"] = trackCount;
    result["ffxivTrackCount"] = ffxivTrackCount;
    result["removedProgramChanges"] = removedPcCount;
    result["guitarSwitchProgramChanges"] = switchCount;
    result["velocityNormalized"] = velocityChangedCount;
    
    QJsonArray channelMapArr;
    for (int t = 0; t < trackCount; t++) {
        QJsonObject entry;
        entry["track"]   = t;
        entry["name"]    = file->track(t)->name();
        entry["channel"] = channelFor[t];
        if (!canonicalNames[t].isEmpty()) {
            entry["instrument"] = canonicalNames[t];
            entry["program"]    = programForInstrument(canonicalNames[t]);
        }
        channelMapArr.append(entry);
    }
    result["channelMap"] = channelMapArr;

    return result;
}

