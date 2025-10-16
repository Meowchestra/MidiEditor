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

#include "ExplodeChordsDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QDialogButtonBox>

#include "../midi/MidiFile.h"
#include "../midi/MidiTrack.h"
#include "../midi/MidiChannel.h"
#include "../MidiEvent/NoteOnEvent.h"
#include "../MidiEvent/OffEvent.h"
#include "../tool/Selection.h"

ExplodeChordsDialog::ExplodeChordsDialog(MidiFile *file, MidiTrack *sourceTrack, QWidget *parent)
    : QDialog(parent), _file(file), _sourceTrack(sourceTrack) {
    
    setWindowTitle(tr("Explode Chords to Tracks"));
    setModal(true);
    
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    // Strategy group
    QGroupBox *strategyGroup = new QGroupBox(tr("Chord Detection Strategy"), this);
    QVBoxLayout *strategyLayout = new QVBoxLayout(strategyGroup);
    
    _strategyCombo = new QComboBox(this);
    _strategyCombo->addItem(tr("Same start tick"));
    _strategyCombo->addItem(tr("Same start tick && note length"));
    strategyLayout->addWidget(_strategyCombo);
    
    QLabel *strategyHelp = new QLabel(tr("How to identify which notes belong to the same chord"), this);
    strategyHelp->setWordWrap(true);
    strategyHelp->setStyleSheet("color: gray; font-size: 9pt;");
    strategyLayout->addWidget(strategyHelp);
    
    mainLayout->addWidget(strategyGroup);
    
    // Group mode
    QGroupBox *groupModeGroup = new QGroupBox(tr("Grouping Mode"), this);
    QVBoxLayout *groupModeLayout = new QVBoxLayout(groupModeGroup);
    
    _groupModeCombo = new QComboBox(this);
    _groupModeCombo->addItem(tr("Group equal parts (voices) into tracks"));
    _groupModeCombo->addItem(tr("Each chord into separate track"));
    _groupModeCombo->addItem(tr("All chords into single track"));
    groupModeLayout->addWidget(_groupModeCombo);
    
    QLabel *groupModeHelp = new QLabel(tr("How to organize the separated chord notes into new tracks"), this);
    groupModeHelp->setWordWrap(true);
    groupModeHelp->setStyleSheet("color: gray; font-size: 9pt;");
    groupModeLayout->addWidget(groupModeHelp);
    
    mainLayout->addWidget(groupModeGroup);
    
    // Settings group
    QGroupBox *settingsGroup = new QGroupBox(tr("Settings"), this);
    QGridLayout *settingsLayout = new QGridLayout(settingsGroup);
    
    QLabel *minNotesLabel = new QLabel(tr("Minimum simultaneous notes:"), this);
    _minNotesCombo = new QComboBox(this);
    for (int i = 2; i <= 10; ++i) {
        _minNotesCombo->addItem(QString::number(i));
    }
    settingsLayout->addWidget(minNotesLabel, 0, 0);
    settingsLayout->addWidget(_minNotesCombo, 0, 1);
    
    mainLayout->addWidget(settingsGroup);
    
    // Options group
    QGroupBox *optionsGroup = new QGroupBox(tr("Options"), this);
    QVBoxLayout *optionsLayout = new QVBoxLayout(optionsGroup);
    
    _insertAtEndCheck = new QCheckBox(tr("Insert new tracks at end of track list"), this);
    _insertAtEndCheck->setToolTip(tr("When unchecked, new tracks are inserted directly below the source track"));
    optionsLayout->addWidget(_insertAtEndCheck);
    
    _keepOriginalCheck = new QCheckBox(tr("Keep original notes on source track"), this);
    _keepOriginalCheck->setToolTip(tr("When checked, chord notes are copied instead of moved"));
    optionsLayout->addWidget(_keepOriginalCheck);
    
    mainLayout->addWidget(optionsGroup);
    
    // Preview
    QGroupBox *previewGroup = new QGroupBox(tr("Preview"), this);
    QVBoxLayout *previewLayout = new QVBoxLayout(previewGroup);
    
    _previewLabel = new QLabel(tr("Analysis: ..."), this);
    _previewLabel->setWordWrap(true);
    previewLayout->addWidget(_previewLabel);
    
    mainLayout->addWidget(previewGroup);
    
    // Buttons
    QDialogButtonBox *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
    
    mainLayout->addWidget(buttonBox);
    
    // Connect signals for live preview updates
    connect(_strategyCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(updatePreview()));
    connect(_groupModeCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(updatePreview()));
    connect(_minNotesCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(updatePreview()));
    
    // Initial preview
    updatePreview();
    
    setLayout(mainLayout);
    resize(500, 400);
}

ExplodeChordsDialog::SplitStrategy ExplodeChordsDialog::splitStrategy() const {
    return static_cast<SplitStrategy>(_strategyCombo->currentIndex());
}

ExplodeChordsDialog::GroupMode ExplodeChordsDialog::groupMode() const {
    return static_cast<GroupMode>(_groupModeCombo->currentIndex());
}

int ExplodeChordsDialog::minimumNotes() const {
    return _minNotesCombo->currentText().toInt();
}

bool ExplodeChordsDialog::insertAtEnd() const {
    return _insertAtEndCheck->isChecked();
}

bool ExplodeChordsDialog::keepOriginalNotes() const {
    return _keepOriginalCheck->isChecked();
}

void ExplodeChordsDialog::analyzeChords(int &chordCount, int &maxVoices) {
    chordCount = 0;
    maxVoices = 0;
    
    if (!_file || !_sourceTrack) {
        return;
    }
    
    SplitStrategy strategy = splitStrategy();
    int minNotes = minimumNotes();
    
    // Collect candidate notes
    QList<NoteOnEvent *> notes;
    
    // Check if there are selected notes
    bool useSelection = false;
    foreach (MidiEvent *ev, Selection::instance()->selectedEvents()) {
        NoteOnEvent *on = dynamic_cast<NoteOnEvent *>(ev);
        if (on && on->offEvent() && on->track() == _sourceTrack) {
            notes.append(on);
            useSelection = true;
        }
    }
    
    // If no selection, use all notes on source track
    if (!useSelection) {
        for (int ch = 0; ch < 16; ++ch) {
            QMultiMap<int, MidiEvent *> *emap = _file->channel(ch)->eventMap();
            for (auto it = emap->begin(); it != emap->end(); ++it) {
                NoteOnEvent *on = dynamic_cast<NoteOnEvent *>(it.value());
                if (on && on->offEvent() && on->track() == _sourceTrack) {
                    notes.append(on);
                }
            }
        }
    }
    
    // Group notes by key (tick or tick+length)
    QMap<qint64, int> groups;
    
    for (NoteOnEvent *on : notes) {
        int t = on->midiTime();
        int l = on->offEvent() ? (on->offEvent()->midiTime() - on->midiTime()) : 0;
        if (l < 0) l = 0;
        
        qint64 key = (strategy == SAME_START) ? qint64(t) : ((qint64(t) << 32) | qint64(l & 0xFFFFFFFF));
        groups[key]++;
    }
    
    // Count groups that meet minimum threshold
    for (auto it = groups.begin(); it != groups.end(); ++it) {
        int count = it.value();
        if (count >= minNotes) {
            chordCount++;
            if (count > maxVoices) {
                maxVoices = count;
            }
        }
    }
}

void ExplodeChordsDialog::updatePreview() {
    int chordCount = 0;
    int maxVoices = 0;
    
    analyzeChords(chordCount, maxVoices);
    
    QString previewText;
    
    if (chordCount == 0) {
        previewText = tr("No chords found matching the criteria.");
    } else {
        GroupMode mode = groupMode();
        int trackCount = 0;
        
        if (mode == ALL_CHORDS_ONE_TRACK) {
            trackCount = 1;
        } else if (mode == EACH_CHORD_OWN_TRACK) {
            trackCount = chordCount;
        } else { // VOICES_ACROSS_CHORDS
            trackCount = maxVoices;
        }
        
        previewText = tr("Found %1 chord group(s) with up to %2 voice(s).\n"
                        "Will create %3 new track(s).")
                        .arg(chordCount)
                        .arg(maxVoices)
                        .arg(trackCount);
    }
    
    _previewLabel->setText(previewText);
}
