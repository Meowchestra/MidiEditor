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

#include "SplitChannelsDialog.h"
#include "../midi/MidiFile.h"
#include "../MidiEvent/MidiEvent.h"
#include "../MidiEvent/NoteOnEvent.h"
#include "../MidiEvent/ProgChangeEvent.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QLabel>
#include <QSpinBox>
#include <QMessageBox>
#include <QVariant>

SplitChannelsDialog::SplitChannelsDialog(MidiFile *file, MidiTrack *sourceTrack, QWidget *parent)
    : QDialog(parent), _file(file), _sourceTrack(sourceTrack),
      _sourceTrackCombo(nullptr), _keepDrumsCheck(nullptr), _removeSourceCheck(nullptr),
      _insertAfterRadio(nullptr), _insertAtEndRadio(nullptr),
      _channelTable(nullptr), _drumsGroup(nullptr),
      _usePresetCheck(nullptr), _presetCombo(nullptr),
      _mappingTable(nullptr), _resetPresetBtn(nullptr),
      _savePresetBtn(nullptr), _updatingTable(false) {

    setWindowTitle(tr("Split Channels to Tracks"));
    setModal(true);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // Track Selection
    QHBoxLayout *trackLayout = new QHBoxLayout();
    trackLayout->addWidget(new QLabel(tr("Source Track:"), this));
    _sourceTrackCombo = new QComboBox(this);
    for (int i = 0; i < file->tracks()->size(); ++i) {
        MidiTrack *t = file->track(i);
        _sourceTrackCombo->addItem(t->name().isEmpty() ? tr("Track %1").arg(i) : t->name(), QVariant::fromValue((void*)t));
        if (t == sourceTrack) {
            _sourceTrackCombo->setCurrentIndex(i);
        }
    }
    trackLayout->addWidget(_sourceTrackCombo);
    trackLayout->addStretch();
    mainLayout->addLayout(trackLayout);
    
    connect(_sourceTrackCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SplitChannelsDialog::onSourceTrackChanged);

    // Channel preview table
    QGroupBox *previewGroup = new QGroupBox(tr("Channels found"), this);
    QVBoxLayout *previewLayout = new QVBoxLayout(previewGroup);

    _channelTable = new QTableWidget(0, 4, this);
    _channelTable->setHorizontalHeaderLabels({tr("Channel"), tr("Program"), tr("Track Name"), tr("Notes")});
    _channelTable->horizontalHeader()->setStretchLastSection(true);
    _channelTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    _channelTable->verticalHeader()->setVisible(false);
    _channelTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    _channelTable->setSelectionMode(QAbstractItemView::NoSelection);

    previewLayout->addWidget(_channelTable);
    mainLayout->addWidget(previewGroup);

    // Drum Kit Splitting UI (Hidden by default, shown if track has drums)
    _drumsGroup = new QGroupBox(tr("Drum Kit Splitting (Channel 9)"), this);
    QVBoxLayout *drumsLayout = new QVBoxLayout(_drumsGroup);
    
    QHBoxLayout *presetLayout = new QHBoxLayout();
    _usePresetCheck = new QCheckBox(tr("Split drums using preset"), this);
    _usePresetCheck->setChecked(true);
    
    QLabel *presetLabel = new QLabel(tr("Preset:"), this);
    _presetCombo = new QComboBox(this);
    _presetCombo->addItems(DrumKitPresetManager::instance().presetNames());
    
    presetLayout->addWidget(_usePresetCheck);
    presetLayout->addStretch();
    presetLayout->addWidget(presetLabel);
    presetLayout->addWidget(_presetCombo);
    drumsLayout->addLayout(presetLayout);

    _mappingTable = new QTableWidget(0, 4, this);
    _mappingTable->setHorizontalHeaderLabels({tr("Source Note"), tr("Instrument"), tr("Group"), tr("Target Note")});
    _mappingTable->horizontalHeader()->setStretchLastSection(true);
    _mappingTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    _mappingTable->verticalHeader()->setVisible(false);
    drumsLayout->addWidget(_mappingTable);
    
    QHBoxLayout *btnLayout = new QHBoxLayout();
    _resetPresetBtn = new QPushButton(tr("Reset Default"), this);
    _savePresetBtn = new QPushButton(tr("Save Preset"), this);
    btnLayout->addStretch();
    btnLayout->addWidget(_resetPresetBtn);
    btnLayout->addWidget(_savePresetBtn);
    drumsLayout->addLayout(btnLayout);
    
    mainLayout->addWidget(_drumsGroup);
    _drumsGroup->hide();
    
    connect(_usePresetCheck, &QCheckBox::toggled, this, [this](bool checked) {
        _presetCombo->setEnabled(checked);
        _mappingTable->setEnabled(checked);
        _resetPresetBtn->setEnabled(checked);
        _savePresetBtn->setEnabled(checked);
        if (_keepDrumsCheck) {
            _keepDrumsCheck->setEnabled(!checked);
            if (checked) _keepDrumsCheck->setChecked(false);
        }
    });
    
    connect(_presetCombo, &QComboBox::currentTextChanged, this, &SplitChannelsDialog::onPresetSelected);
    connect(_resetPresetBtn, &QPushButton::clicked, this, &SplitChannelsDialog::onResetPresetClicked);
    connect(_savePresetBtn, &QPushButton::clicked, this, &SplitChannelsDialog::onSavePresetClicked);
    
    _currentPreset = DrumKitPresetManager::instance().preset(_presetCombo->currentText());

    // Options
    QGroupBox *optionsGroup = new QGroupBox(tr("Options"), this);
    QVBoxLayout *optionsLayout = new QVBoxLayout(optionsGroup);

    _keepDrumsCheck = new QCheckBox(tr("Keep Channel 9 (Drums) on original track"), this);
    _keepDrumsCheck->setToolTip(tr("When checked, drum events stay on the source track instead of getting their own track"));
    optionsLayout->addWidget(_keepDrumsCheck);

    _removeSourceCheck = new QCheckBox(tr("Remove empty source track after split"), this);
    _removeSourceCheck->setChecked(true);
    _removeSourceCheck->setToolTip(tr("Deletes the original track if no events remain on it"));
    optionsLayout->addWidget(_removeSourceCheck);

    mainLayout->addWidget(optionsGroup);

    // Track position
    QGroupBox *positionGroup = new QGroupBox(tr("New track position"), this);
    QVBoxLayout *positionLayout = new QVBoxLayout(positionGroup);

    _insertAfterRadio = new QRadioButton(tr("Insert after source track"), this);
    _insertAfterRadio->setChecked(true);
    positionLayout->addWidget(_insertAfterRadio);

    _insertAtEndRadio = new QRadioButton(tr("Insert at end of track list"), this);
    positionLayout->addWidget(_insertAtEndRadio);

    mainLayout->addWidget(positionGroup);

    // Buttons
    QDialogButtonBox *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttonBox->button(QDialogButtonBox::Ok)->setText(tr("Split"));
    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
    mainLayout->addWidget(buttonBox);

    setLayout(mainLayout);
    
    // Initial analysis
    analyzeTrack(_sourceTrack);
}

bool SplitChannelsDialog::keepDrumsOnSource() const {
    return _keepDrumsCheck && _keepDrumsCheck->isChecked();
}

bool SplitChannelsDialog::removeEmptySource() const {
    return _removeSourceCheck && _removeSourceCheck->isChecked();
}

bool SplitChannelsDialog::insertAtEnd() const {
    return _insertAtEndRadio && _insertAtEndRadio->isChecked();
}

bool SplitChannelsDialog::useDrumKitPreset() const {
    return _usePresetCheck && _usePresetCheck->isChecked() && !_drumsGroup->isHidden();
}

DrumKitPreset SplitChannelsDialog::selectedDrumKitPreset() const {
    return _currentPreset;
}

MidiTrack *SplitChannelsDialog::sourceTrack() const {
    return _sourceTrack;
}

void SplitChannelsDialog::onSourceTrackChanged(int index) {
    _sourceTrack = (MidiTrack*)_sourceTrackCombo->itemData(index).value<void*>();
    analyzeTrack(_sourceTrack);
}

void SplitChannelsDialog::analyzeTrack(MidiTrack *track) {
    if (!track || !_file) return;

    QList<ChannelInfo> activeChannels;
    for (int ch = 0; ch < 16; ++ch) {
        QMultiMap<int, MidiEvent *> *emap = _file->channel(ch)->eventMap();
        int eventCount = 0;
        int noteCount = 0;
        int prog = -1;

        for (auto it = emap->begin(); it != emap->end(); ++it) {
            MidiEvent *ev = it.value();
            if (ev->track() != track) continue;
            
            eventCount++;
            if (dynamic_cast<NoteOnEvent *>(ev)) {
                noteCount++;
            }
            if (prog < 0) {
                if (ProgChangeEvent *pc = dynamic_cast<ProgChangeEvent *>(ev)) {
                    prog = pc->program();
                }
            }
        }

        if (eventCount > 0) {
            QString name;
            if (ch == 9) {
                name = tr("Drums");
            } else if (prog >= 0) {
                name = MidiFile::gmInstrumentName(prog);
            } else {
                name = tr("Channel %1").arg(ch);
            }
            activeChannels.append({ch, eventCount, noteCount, prog, name});
        }
    }

    populatePreviewTable(activeChannels);

    bool hasDrums = false;
    for (const auto &info : activeChannels) {
        if (info.channel == 9) {
            hasDrums = true;
            break;
        }
    }

    if (hasDrums) {
        _drumsGroup->show();
        populateMappingTable();
        _keepDrumsCheck->setEnabled(!_usePresetCheck->isChecked());
    } else {
        _drumsGroup->hide();
        _keepDrumsCheck->setEnabled(true);
    }
    
    // Adjust size appropriately
    resize(520, qMin(activeChannels.size() * 30 + (hasDrums ? 680 : 380), 800));
}

void SplitChannelsDialog::populatePreviewTable(const QList<ChannelInfo> &channels) {
    _channelTable->setRowCount(channels.size());
    for (int row = 0; row < channels.size(); ++row) {
        const ChannelInfo &info = channels[row];

        auto *chItem = new QTableWidgetItem(QString::number(info.channel));
        chItem->setTextAlignment(Qt::AlignCenter);
        _channelTable->setItem(row, 0, chItem);

        QString progStr = (info.channel == 9) ? tr("Drums") :
                          (info.programNumber >= 0) ? QString::number(info.programNumber) : QStringLiteral("-");
        auto *progItem = new QTableWidgetItem(progStr);
        progItem->setTextAlignment(Qt::AlignCenter);
        _channelTable->setItem(row, 1, progItem);

        _channelTable->setItem(row, 2, new QTableWidgetItem(info.instrumentName));

        auto *noteItem = new QTableWidgetItem(QString::number(info.noteCount));
        noteItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        _channelTable->setItem(row, 3, noteItem);
    }
    _channelTable->resizeColumnsToContents();
    _channelTable->setMinimumHeight(qMin(channels.size() * 30 + 30, 200));
}

void SplitChannelsDialog::populateMappingTable() {
    if (!_mappingTable) return;
    
    _updatingTable = true;
    
    int rowCount = 0;
    for (const auto &g : _currentPreset.groups) {
        rowCount += g.mappings.size();
    }
    
    _mappingTable->setRowCount(rowCount);
    _mappingTable->clearContents();
    
    int row = 0;
    for (int g = 0; g < _currentPreset.groups.size(); ++g) {
        const auto &group = _currentPreset.groups[g];
        for (int m = 0; m < group.mappings.size(); ++m) {
            const auto &mapping = group.mappings[m];
            
            auto *srcNoteItem = new QTableWidgetItem(QString::number(mapping.sourceNote));
            srcNoteItem->setFlags(srcNoteItem->flags() & ~Qt::ItemIsEditable);
            srcNoteItem->setTextAlignment(Qt::AlignCenter);
            _mappingTable->setItem(row, 0, srcNoteItem);
            
            auto *nameItem = new QTableWidgetItem(mapping.drumName);
            nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
            _mappingTable->setItem(row, 1, nameItem);
            
            QComboBox *groupCombo = new QComboBox(this);
            for (const auto &grp : _currentPreset.groups) {
                groupCombo->addItem(grp.trackName);
            }
            groupCombo->setCurrentText(group.trackName);
            
            QSpinBox *targetNoteSpin = new QSpinBox(this);
            targetNoteSpin->setRange(0, 127);
            targetNoteSpin->setValue(mapping.targetNote);
            
            _mappingTable->setCellWidget(row, 2, groupCombo);
            _mappingTable->setCellWidget(row, 3, targetNoteSpin);
            
            int sNote = mapping.sourceNote;
            connect(groupCombo, &QComboBox::currentTextChanged, this, [this, sNote](const QString &txt) {
                if (!_updatingTable) onGroupChanged(sNote, txt);
            });
            connect(targetNoteSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this, sNote](int val) {
                if (!_updatingTable) onTargetNoteChanged(sNote, val);
            });
            
            ++row;
        }
    }
    
    _mappingTable->resizeColumnsToContents();
    _mappingTable->setMinimumHeight(200);
    _updatingTable = false;
}

void SplitChannelsDialog::onPresetSelected(const QString &presetName) {
    if (_usePresetCheck && !_usePresetCheck->isChecked()) return;
    _currentPreset = DrumKitPresetManager::instance().preset(presetName);
    populateMappingTable();
}

void SplitChannelsDialog::onTargetNoteChanged(int sourceNote, int val) {
    for (int g = 0; g < _currentPreset.groups.size(); ++g) {
        for (int m = 0; m < _currentPreset.groups[g].mappings.size(); ++m) {
            if (_currentPreset.groups[g].mappings[m].sourceNote == sourceNote) {
                _currentPreset.groups[g].mappings[m].targetNote = val;
                return;
            }
        }
    }
}

void SplitChannelsDialog::onGroupChanged(int sourceNote, const QString &newGroupName) {
    DrumNoteMapping mappingToMove;
    bool found = false;
    
    for (int g = 0; g < _currentPreset.groups.size(); ++g) {
        for (int m = 0; m < _currentPreset.groups[g].mappings.size(); ++m) {
            if (_currentPreset.groups[g].mappings[m].sourceNote == sourceNote) {
                mappingToMove = _currentPreset.groups[g].mappings[m];
                _currentPreset.groups[g].mappings.removeAt(m);
                found = true;
                break;
            }
        }
        if (found) break;
    }
    
    if (found) {
        for (int g = 0; g < _currentPreset.groups.size(); ++g) {
            if (_currentPreset.groups[g].trackName == newGroupName) {
                _currentPreset.groups[g].mappings.append(mappingToMove);
                break;
            }
        }
    }
}

void SplitChannelsDialog::onResetPresetClicked() {
    DrumKitPresetManager::instance().resetPreset(_currentPreset.name);
    _currentPreset = DrumKitPresetManager::instance().preset(_currentPreset.name);
    populateMappingTable();
}

void SplitChannelsDialog::onSavePresetClicked() {
    DrumKitPresetManager::instance().savePreset(_currentPreset);
}
