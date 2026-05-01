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

#include "DrumKitPreset.h"
#include <QSettings>
#include <QVariant>
#include <QStringList>

DrumKitPresetManager &DrumKitPresetManager::instance() {
    static DrumKitPresetManager inst;
    return inst;
}

DrumKitPresetManager::DrumKitPresetManager() {
    initDefaults();
    
    // Load any user edits from QSettings
    QSettings settings(QStringLiteral("MidiEditor"), QStringLiteral("NONE"));
    settings.beginGroup("DrumKitPresets");
    QStringList savedPresets = settings.childGroups();
    
    for (const QString &presetName : savedPresets) {
        settings.beginGroup(presetName);
        
        // Start with the default template if it exists
        DrumKitPreset preset;
        for (const DrumKitPreset &defPreset : _defaults) {
            if (defPreset.name == presetName) {
                preset = defPreset;
                break;
            }
        }
        
        if (preset.name.isEmpty()) continue; // Not a known default
        
        // Find and overwrite user edited mapping target notes & groups
        int savedMappingsCount = settings.beginReadArray("UserMappings");
        for (int i = 0; i < savedMappingsCount; ++i) {
            settings.setArrayIndex(i);
            int sourceNote = settings.value("sourceNote").toInt();
            int customTargetNote = settings.value("targetNote").toInt();
            QString customGroupName = settings.value("group").toString();
            
            // First locate and remove the mapping from its current group
            DrumNoteMapping mappingToMove;
            bool foundMapping = false;
            
            for (int g = 0; g < preset.groups.size(); ++g) {
                for (int m = 0; m < preset.groups[g].mappings.size(); ++m) {
                    if (preset.groups[g].mappings[m].sourceNote == sourceNote) {
                        mappingToMove = preset.groups[g].mappings[m];
                        preset.groups[g].mappings.removeAt(m);
                        foundMapping = true;
                        break;
                    }
                }
                if (foundMapping) break;
            }
            
            if (foundMapping) {
                // Apply the target note edit
                mappingToMove.targetNote = customTargetNote;
                
                // Add it to the correct group
                bool groupFound = false;
                for (int g = 0; g < preset.groups.size(); ++g) {
                    if (preset.groups[g].trackName == customGroupName) {
                        preset.groups[g].mappings.append(mappingToMove);
                        groupFound = true;
                        break;
                    }
                }
                
                if (!groupFound) {
                    // Safety fallback: if group isn't found, put it in Drumkit Rest
                    for (int g = 0; g < preset.groups.size(); ++g) {
                        if (preset.groups[g].trackName == "Drumkit Rest") {
                            preset.groups[g].mappings.append(mappingToMove);
                            break;
                        }
                    }
                }
            }
        }
        settings.endArray();
        settings.endGroup(); // presetName
        
        _userEdits[presetName] = preset;
    }
    settings.endGroup(); // DrumKitPresets
}

QStringList DrumKitPresetManager::presetNames() const {
    QStringList names;
    for (const DrumKitPreset &p : _defaults) {
        names.append(p.name);
    }
    return names;
}

bool DrumKitPresetManager::isCustomPreset(const QString &name) const {
    return _userEdits.contains(name);
}

DrumKitPreset DrumKitPresetManager::preset(const QString &name) const {
    if (_userEdits.contains(name)) {
        return _userEdits.value(name);
    }
    for (const DrumKitPreset &p : _defaults) {
        if (p.name == name) {
            return p;
        }
    }
    return DrumKitPreset();
}

void DrumKitPresetManager::savePreset(const DrumKitPreset &preset) {
    if (preset.name.isEmpty()) return;
    
    // Update active memory
    _userEdits[preset.name] = preset;
    
    // Save differences compared to default
    DrumKitPreset defaultPreset;
    for (const DrumKitPreset &p : _defaults) {
        if (p.name == preset.name) {
            defaultPreset = p;
            break;
        }
    }
    
    if (defaultPreset.name.isEmpty()) return;
    
    // Find all mapping discrepancies (either target note differs, or group differs)
    struct CustomMapping {
        int sourceNote;
        int targetNote;
        QString group;
    };
    QList<CustomMapping> customMappings;
    
    // Map default layout for quick lookup
    QMap<int, QPair<QString, int>> defaultMap; // sourceNote -> <groupName, targetNote>
    for (const auto &g : defaultPreset.groups) {
        for (const auto &m : g.mappings) {
            defaultMap.insert(m.sourceNote, qMakePair(g.trackName, m.targetNote));
        }
    }
    
    // Check current layout against default
    for (const auto &g : preset.groups) {
        for (const auto &m : g.mappings) {
            if (defaultMap.contains(m.sourceNote)) {
                QPair<QString, int> def = defaultMap.value(m.sourceNote);
                if (m.targetNote != def.second || g.trackName != def.first) {
                    customMappings.append({m.sourceNote, m.targetNote, g.trackName});
                }
            }
        }
    }
    
    QSettings settings(QStringLiteral("MidiEditor"), QStringLiteral("NONE"));
    settings.beginGroup("DrumKitPresets");
    settings.beginGroup(preset.name);
    settings.remove(""); // clear old edits
    
    if (!customMappings.isEmpty()) {
        settings.beginWriteArray("UserMappings");
        for (int i = 0; i < customMappings.size(); ++i) {
            settings.setArrayIndex(i);
            settings.setValue("sourceNote", customMappings[i].sourceNote);
            settings.setValue("targetNote", customMappings[i].targetNote);
            settings.setValue("group", customMappings[i].group);
        }
        settings.endArray();
    }
    
    settings.endGroup(); // preset.name
    settings.endGroup(); // DrumKitPresets
}

void DrumKitPresetManager::resetPreset(const QString &name) {
    _userEdits.remove(name);
    
    QSettings settings(QStringLiteral("MidiEditor"), QStringLiteral("NONE"));
    settings.beginGroup("DrumKitPresets");
    settings.remove(name);
    settings.endGroup();
}

QString DrumKitPresetManager::gmDrumName(int note) {
    static const QMap<int, QString> names = {
        {27, "High Q"}, {28, "Slap"}, {29, "Scratch Push"}, {30, "Scratch Pull"},
        {31, "Sticks"}, {32, "Square Click"}, {33, "Metronome Click"}, {34, "Metronome Bell"},
        {35, "Kick Drum 2"}, {36, "Kick Drum 1"}, {37, "Side Stick"}, {38, "Snare Drum 1"},
        {39, "Hand Clap"}, {40, "Snare Drum 2"}, {41, "Low Tom 2"}, {42, "Closed Hi-Hat"},
        {43, "Low Tom 1"}, {44, "Pedal Hi-Hat"}, {45, "Mid Tom 2"}, {46, "Open Hi-Hat"},
        {47, "Mid Tom 1"}, {48, "High Tom 2"}, {49, "Crash Cymbal 1"}, {50, "High Tom 1"},
        {51, "Ride Cymbal 1"}, {52, "Chinese Cymbal"}, {53, "Ride Bell"}, {54, "Tambourine"},
        {55, "Splash Cymbal"}, {56, "Cowbell"}, {57, "Crash Cymbal 2"}, {58, "Vibra-Slap"},
        {59, "Ride Cymbal 2"}, {60, "High Bongo"}, {61, "Low Bongo"}, {62, "Mute Hi Conga"},
        {63, "Open Hi Conga"}, {64, "Low Conga"}, {65, "High Timbale"}, {66, "Low Timbale"},
        {67, "High Agogo"}, {68, "Low Agogo"}, {69, "Cabasa"}, {70, "Maracas"},
        {71, "Short Hi Whistle"}, {72, "Long Lo Whistle"}, {73, "Short Guiro"}, {74, "Long Guiro"},
        {75, "Claves"}, {76, "High Woodblock"}, {77, "Low Woodblock"}, {78, "Mute Cuica"},
        {79, "Open Cuica"}, {80, "Mute Triangle"}, {81, "Open Triangle"}, {82, "Shaker"},
        {83, "Jingle Bell"}, {84, "Belltree"}, {85, "Castanets"}, {86, "Closed Surdo"},
        {87, "Open Surdo"}
    };
    return names.value(note, QString("Drum %1").arg(note));
}

QList<int> DrumKitPresetManager::allGmDrumNotes() {
    QList<int> notes;
    for (int i = 27; i <= 87; ++i) {
        notes.append(i);
    }
    return notes;
}

void DrumKitPresetManager::initDefaults() {
    struct RawMapping { int note; int target; };
    
    auto createPreset = [](const QString &name, 
                           const QList<RawMapping> &bassDrum,
                           const QList<RawMapping> &snareDrum,
                           const QList<RawMapping> &cymbal,
                           const QList<RawMapping> &bongo) {
        DrumKitPreset preset;
        preset.name = name;
        
        // Define groups according to FFXIV mappings
        QList<DrumTrackGroup> groups;
        
        DrumTrackGroup bDrum;
        bDrum.trackName = "Bass Drum";
        bDrum.programNumber = 97; // FFXIV BassDrum
        bDrum.staysOnPercussionChannel = false;
        
        DrumTrackGroup sDrum;
        sDrum.trackName = "Snare Drum";
        sDrum.programNumber = 98; // FFXIV SnareDrum
        sDrum.staysOnPercussionChannel = false;
        
        DrumTrackGroup cym;
        cym.trackName = "Cymbal";
        cym.programNumber = 99; // FFXIV Cymbal
        cym.staysOnPercussionChannel = false;
        
        DrumTrackGroup bgo;
        bgo.trackName = "Bongo";
        bgo.programNumber = 96; // FFXIV Bongo
        bgo.staysOnPercussionChannel = false;
        
        DrumTrackGroup rest;
        rest.trackName = "Drumkit Rest";
        rest.programNumber = -1;
        rest.staysOnPercussionChannel = true;
        
        // Helper to add mappings and track assigned notes
        QList<int> assignedNotes;
        auto addMappings = [&](DrumTrackGroup &grp, const QList<RawMapping> &raw) {
            for (const auto &r : raw) {
                grp.mappings.append({r.note, r.target, gmDrumName(r.note)});
                assignedNotes.append(r.note);
            }
        };
        
        addMappings(bDrum, bassDrum);
        addMappings(sDrum, snareDrum);
        addMappings(cym, cymbal);
        addMappings(bgo, bongo);
        
        // Add all unassigned notes to Rest group (untransposed)
        for (int note : allGmDrumNotes()) {
            if (!assignedNotes.contains(note)) {
                rest.mappings.append({note, note, gmDrumName(note)});
            }
        }
        
        DrumTrackGroup timp;
        timp.trackName = "Timpani";
        timp.programNumber = 47;
        timp.staysOnPercussionChannel = false;
        
        preset.groups.append(bDrum);
        preset.groups.append(sDrum);
        preset.groups.append(cym);
        preset.groups.append(bgo);
        preset.groups.append(timp);
        preset.groups.append(rest);
        
        return preset;
    };
    
    // Mog Amp
    _defaults.append(createPreset("Mog Amp",
        {{35, 55}, {36, 57}, {41, 63}, {43, 66}, {45, 70}, {47, 73}, {48, 77}, {50, 80}}, // BassDrum
        {{38, 67}, {40, 69}}, // Snare
        {{49, 71}, {52, 69}, {55, 77}, {57, 71}}, // Cymbal
        {{60, 70}, {61, 67}}  // Bongo
    ));
    
    // Bard Forge 1
    _defaults.append(createPreset("Bard Forge 1",
        {{35, 48}, {36, 51}, {41, 56}, {43, 58}, {45, 60}, {47, 62}, {48, 51}, {50, 53}},
        {{38, 62}, {40, 64}},
        {{49, 73}, {52, 76}, {55, 79}, {57, 81}},
        {{60, 60}, {61, 61}}
    ));
    
    // Bard Forge 2
    _defaults.append(createPreset("Bard Forge 2",
        {{35, 53}, {36, 55}, {41, 58}, {43, 61}, {45, 65}, {47, 68}, {48, 71}, {50, 74}},
        {{38, 64}, {40, 66}},
        {{49, 71}, {52, 69}, {55, 77}, {57, 71}},
        {{60, 70}, {61, 67}}
    ));
    
    // Bard Metal (Eltana)
    _defaults.append(createPreset("Bard Metal (Eltana)",
        {{35, 59}, {36, 60}, {41, 62}, {43, 64}, {45, 66}, {47, 68}, {48, 69}, {50, 71}},
        {{38, 68}, {39, 84}, {40, 68}},
        {{49, 78}, {57, 81}},
        {{60, 70}, {61, 67}, {62, 72}, {63, 65}, {64, 74}}
    ));
}
