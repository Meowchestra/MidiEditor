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
// analyzeFile — read-only scan
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

    result["valid"]            = (ffxivTrackCount > 0);
    result["trackCount"]       = trackCount;
    result["ffxivTrackCount"]  = ffxivTrackCount;
    result["hasGuitar"]        = hasGuitar;
    result["guitarVariants"]   = QJsonArray::fromStringList(guitarVariants);
    result["percussionTracks"] = QJsonArray::fromStringList(percussionTracks);
    result["melodicTracks"]    = QJsonArray::fromStringList(melodicTracks);
    return result;
}

// ---------------------------------------------------------------------------
// fixChannels — full channel and program-change rebuild
// ---------------------------------------------------------------------------

QJsonObject FFXIVChannelFixer::fixChannels(MidiFile *file, ProgressCallback progress) {
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
    // 1. Resolve every track name to its canonical FFXIV instrument
    // -----------------------------------------------------------------
    report(5, QStringLiteral("Resolving track names..."));

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
        result["error"]   = QStringLiteral(
            "No FFXIV instrument names detected. "
            "Track names must match FFXIV instruments. ");
        return result;
    }

    // -----------------------------------------------------------------
    // 2. Build channel assignment map
    //    - Preconfigured guitars -> keep channels, don't move
    //    - Guitar duplicate variants share the first occurrence's channel
    //    - Everything else → track index (clamped 0-15)
    // -----------------------------------------------------------------
    report(15, QStringLiteral("Building channel map..."));

    QVector<int> channelFor(trackCount, -1);
    QSet<int>    usedChannels;
    QSet<int>    protectedChannels;
    QHash<QString, int> guitarChannelMap;

    auto hasValidGuitarPCs = [&](MidiTrack *track) -> bool {
        for (int ch = 0; ch < 16; ch++) {
            MidiChannel *channel = file->channel(ch);
            if (!channel) continue;
            auto *eventMap = channel->eventMap();
            for (auto it = eventMap->begin(); it != eventMap->end(); ++it) {
                if (auto *pc = dynamic_cast<ProgChangeEvent *>(it.value())) {
                    if (pc->track() == track) {
                        int p = pc->program();
                        if (p >= 27 && p <= 31) return true;
                    }
                }
            }
        }
        return false;
    };

    for (int t = 0; t < trackCount; t++) {
        const QString &name = canonicalNames[t];
        MidiTrack *track = file->track(t);
        bool isPreconfigGuitar = (name == QStringLiteral("ProgramElectricGuitar") ||
                                 (isGuitar(name) && hasValidGuitarPCs(track)));

        if (name.isEmpty()) {
            // Not an FFXIV track — keep at track index
            channelFor[t] = qMin(t, 15);
        } else if (isPreconfigGuitar) {
            channelFor[t] = -2; // Preconfigured, do not move events or strip PCs
            for (int ch = 0; ch < 16; ch++) {
                MidiChannel *channel = file->channel(ch);
                if (!channel) continue;
                auto *eventMap = channel->eventMap();
                for (auto it = eventMap->begin(); it != eventMap->end(); ++it) {
                    if (it.value()->track() == track) {
                        protectedChannels.insert(ch);
                        usedChannels.insert(ch);
                        break;
                    }
                }
            }
        } else if (isGuitar(name) && guitarChannelMap.contains(name)) {
            // Duplicate guitar variant: reuse first occurrence's channel
            channelFor[t] = guitarChannelMap[name];
        } else {
            int ch = t;
            if (ch > 15) ch = 15;
            channelFor[t] = ch;
            if (isGuitar(name))
                guitarChannelMap[name] = ch;
        }
        if (channelFor[t] >= 0) {
            usedChannels.insert(channelFor[t]);
        }
    }

    // Reserve free channels for guitar variants not present as tracks
    if (hasGuitar) {
        auto nextFreeChannel = [&]() -> int {
            for (int ch = 0; ch <= 15; ch++) {
                if (!usedChannels.contains(ch))
                    return ch;
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

    // Collect all guitar channels
    QSet<int> allGuitarChs;
    if (hasGuitar) {
        for (auto it = guitarChannelMap.begin(); it != guitarChannelMap.end(); ++it)
            allGuitarChs.insert(it.value());
    }

    // -----------------------------------------------------------------
    // 3. Remove all existing program-change events
    // -----------------------------------------------------------------
    report(30, QStringLiteral("Removing old program changes..."));

    int removedPcCount = 0;
    for (int ch = 0; ch < 16; ch++) {
        if (protectedChannels.contains(ch)) continue; // Preserve PCs for preconfigured channels
        MidiChannel *channel = file->channel(ch);
        if (!channel) continue;
        QMultiMap<int, MidiEvent *> *map = channel->eventMap();
        QList<MidiEvent *> toRemove;
        for (auto it = map->begin(); it != map->end(); ++it) {
            if (dynamic_cast<ProgChangeEvent *>(it.value()))
                toRemove.append(it.value());
        }
        for (MidiEvent *ev : toRemove)
            channel->removeEvent(ev);
        removedPcCount += toRemove.size();
    }

    // -----------------------------------------------------------------
    // 4. Migrate events to correct channels
    // -----------------------------------------------------------------
    report(50, QStringLiteral("Migrating events..."));

    for (int t = 0; t < trackCount; t++) {
        int targetCh = channelFor[t];
        if (targetCh < 0) continue; // Skip preconfigured tracks (-2)
        MidiTrack *track = file->track(t);

        struct EventInfo { MidiEvent *ev; int currentCh; };
        QList<EventInfo> trackEvents;

        for (int ch = 0; ch < 16; ch++) {
            MidiChannel *channel = file->channel(ch);
            if (!channel) continue;
            QMultiMap<int, MidiEvent *> *map = channel->eventMap();
            for (auto it = map->begin(); it != map->end(); ++it) {
                MidiEvent *ev = it.value();
                if (ev->track() != track) continue;
                if (dynamic_cast<ProgChangeEvent *>(ev)) continue;
                if (dynamic_cast<OffEvent *>(ev)) continue;
                trackEvents.append({ev, ch});
            }
        }

        for (const auto &info : trackEvents) {
            if (info.currentCh == targetCh) continue;
            info.ev->moveToChannel(targetCh);
        }

        track->assignChannel(targetCh);
    }

    // -----------------------------------------------------------------
    // 5. Insert program-change at tick 0 for every FFXIV channel
    // -----------------------------------------------------------------
    report(70, QStringLiteral("Inserting program changes..."));

    struct ChannelProgram { int channel; int program; };
    QList<ChannelProgram> channelPrograms;

    // Non-guitar tracks
    for (int t = 0; t < trackCount; t++) {
        if (channelFor[t] < 0) continue; // Skip preconfigured tracks
        if (isGuitar(canonicalNames[t])) continue;
        int prog = programForInstrument(canonicalNames[t]);
        if (prog >= 0)
            channelPrograms.append({channelFor[t], prog});
    }

    // All guitar channels (from the full variant map)
    if (hasGuitar) {
        for (auto it = guitarChannelMap.begin(); it != guitarChannelMap.end(); ++it) {
            int prog = programForInstrument(it.key());
            if (prog >= 0)
                channelPrograms.append({it.value(), prog});
        }
    }

    // Deduplicate: only one PC per channel
    QSet<int> insertedChannels;
    for (int t = 0; t < trackCount; t++) {
        MidiTrack *track = file->track(t);
        for (const auto &cp : channelPrograms) {
            if (insertedChannels.contains(cp.channel))
                continue;
            auto *pc = new ProgChangeEvent(cp.channel, cp.program, track);
            file->channel(cp.channel)->insertEvent(pc, 0);
            insertedChannels.insert(cp.channel);
        }
        if (insertedChannels.size() == channelPrograms.size())
            break; // All PCs inserted
    }

    // -----------------------------------------------------------------
    // 6. Insert dynamic program-changes for multi-track guitar sequences
    // -----------------------------------------------------------------
    report(85, QStringLiteral("Processing guitar switches..."));

    int switchCount = 0;
    if (hasGuitar) {
        struct GuitarNote { int tick; MidiTrack *track; int channel; int program; };
        QList<GuitarNote> gNotes;

        for (int t = 0; t < trackCount; t++) {
            if (!isGuitar(canonicalNames[t])) continue;
            if (channelFor[t] < 0) continue; // Skip preconfigured tracks (-2)
            
            int prog = programForInstrument(canonicalNames[t]);
            if (prog < 0) continue;
            
            MidiTrack *track = file->track(t);
            int trackCh = channelFor[t];
            MidiChannel *channel = file->channel(trackCh);
            if (!channel) continue;
            
            auto *eventMap = channel->eventMap();
            for (auto it = eventMap->begin(); it != eventMap->end(); ++it) {
                if (it.value()->track() == track && dynamic_cast<NoteOnEvent*>(it.value()) != nullptr) {
                    gNotes.append({it.key(), track, trackCh, prog});
                }
            }
        }

        // Sort all notes globally through time
        std::sort(gNotes.begin(), gNotes.end(), [](const GuitarNote &a, const GuitarNote &b) {
            return a.tick < b.tick;
        });

        int lastProg = -1;
        for (const auto &n : gNotes) {
            if (n.program != lastProg) {
                // Loop 5 already places initialization PCs at tick 0 for all variants.
                // We only need to inject mid-track switches chronologically for the sequencer.
                if (n.tick > 0 || lastProg != -1) {
                    auto *pc = new ProgChangeEvent(n.channel, n.program, n.track);
                    file->channel(n.channel)->insertEvent(pc, n.tick);
                    switchCount++;
                }
                lastProg = n.program;
            }
        }
    }

    // -----------------------------------------------------------------
    // 7. Normalise all NoteOn velocities to 100
    //    FFXIV performance ignores velocity but 100 is a safe default.
    // -----------------------------------------------------------------
    report(95, QStringLiteral("Normalizing velocity..."));

    int velocityChangedCount = 0;
    for (int ch = 0; ch < 16; ch++) {
        MidiChannel *channel = file->channel(ch);
        if (!channel) continue;
        QMultiMap<int, MidiEvent *> *map = channel->eventMap();
        for (auto it = map->begin(); it != map->end(); ++it) {
            NoteOnEvent *noteOn = dynamic_cast<NoteOnEvent *>(it.value());
            if (noteOn && noteOn->velocity() > 0 && noteOn->velocity() != 100) {
                noteOn->setVelocity(100);
                velocityChangedCount++;
            }
        }
    }

    // -----------------------------------------------------------------
    // 8. Build result report
    // -----------------------------------------------------------------
    report(100, QStringLiteral("Done!"));

    QJsonArray channelMapArr;
    for (int t = 0; t < trackCount; t++) {
        QJsonObject entry;
        entry["track"]   = t;
        entry["name"]    = file->track(t)->name();
        entry["channel"] = channelFor[t];
        entry["program"] = programForInstrument(canonicalNames[t]);
        if (!canonicalNames[t].isEmpty())
            entry["instrument"] = canonicalNames[t];
        channelMapArr.append(entry);
    }

    if (hasGuitar) {
        QJsonArray extraGuitarArr;
        for (const QString &variant : allGuitarVariants) {
            if (!guitarVariantsPresent.contains(variant)
                && guitarChannelMap.contains(variant)) {
                QJsonObject entry;
                entry["variant"]  = variant;
                entry["channel"]  = guitarChannelMap[variant];
                entry["program"]  = programForInstrument(variant);
                entry["reserved"] = true;
                extraGuitarArr.append(entry);
            }
        }
        if (!extraGuitarArr.isEmpty())
            result["reservedGuitarChannels"] = extraGuitarArr;
    }

    result["success"]                   = true;
    result["channelMap"]                = channelMapArr;
    result["guitarSwitchProgramChanges"]= switchCount;
    result["removedProgramChanges"]     = removedPcCount;
    result["velocityNormalized"]        = velocityChangedCount;
    result["trackCount"]                = trackCount;
    result["ffxivTrackCount"]           = ffxivTrackCount;
    
    QString summary = QStringLiteral("Successfully processed %1 FFXIV tracks (out of %2 total).\n"
                         "Removed %3 existing program change events.\n"
                         "%4")
                         .arg(ffxivTrackCount)
                         .arg(trackCount)
                         .arg(removedPcCount)
                         .arg(switchCount > 0 ? QStringLiteral("Added %1 guitar variant program changes.").arg(switchCount) : QStringLiteral(""));
    result["summary"] = summary;
    
    return result;
}
