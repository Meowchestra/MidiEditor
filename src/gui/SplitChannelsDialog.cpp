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
#include "../midi/MidiTrack.h"
#include "../midi/MidiChannel.h"
#include "../MidiEvent/MidiEvent.h"
#include "../MidiEvent/NoteOnEvent.h"
#include "../MidiEvent/ProgChangeEvent.h"
#include "../midi/MidiOutput.h"
#include "../midi/InstrumentDefinitions.h"
#include <QSettings>
#include <QToolButton>
#include <QTimer>
#include <QSpinBox>

static QString getNoteName(int note) {
    if (note < 0 || note > 127) return "";
    static const QString pitchClass[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    return QString("%1%2").arg(pitchClass[note % 12]).arg((note / 12) - 1);
}

class NoteSpinBox : public QSpinBox {
public:
    NoteSpinBox(QWidget *parent = nullptr) : QSpinBox(parent) {}
    QString textFromValue(int val) const override {
        return QString("%1 (%2)").arg(val).arg(getNoteName(val));
    }
};

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
#include <QScrollArea>
#include <QScrollBar>
#include <QGridLayout>
#include <QFrame>
#include <QSplitter>
#include <algorithm>
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
      _mappingScrollArea(nullptr), _resetPresetBtn(nullptr),
      _savePresetBtn(nullptr), _updatingTable(false) {

    setWindowTitle(tr("Split Channels to Tracks"));
    setModal(true);
    setMinimumSize(850, 750);
    resize(850, 750);

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

    QSplitter *splitter = new QSplitter(Qt::Vertical, this);
    splitter->setChildrenCollapsible(false);

    // Channels Found preview table
    QGroupBox *previewGroup = new QGroupBox(tr("Channels Found"), this);
    QVBoxLayout *previewLayout = new QVBoxLayout(previewGroup);

    _channelTable = new QTableWidget(0, 4, this);
    _channelTable->setHorizontalHeaderLabels({tr("Channel"), tr("Program"), tr("Notes"), tr("Track Name")});
    _channelTable->horizontalHeader()->setStretchLastSection(false);
    _channelTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    _channelTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    _channelTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    _channelTable->setColumnWidth(2, 120);
    _channelTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    _channelTable->verticalHeader()->setVisible(false);
    _channelTable->verticalHeader()->setDefaultSectionSize(28);
    _channelTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    _channelTable->setSelectionMode(QAbstractItemView::NoSelection);

    previewLayout->addWidget(_channelTable);
    
    splitter->addWidget(previewGroup);

    // Drum Kit Splitting UI (Hidden by default, shown if track has drums)
    _drumsGroup = new QGroupBox(tr("Drum Kit Splitting (Channel 9)"), this);
    QVBoxLayout *drumsLayout = new QVBoxLayout(_drumsGroup);
    
    QHBoxLayout *presetLayout = new QHBoxLayout();
    _usePresetCheck = new QCheckBox(tr("Split drums using preset"), this);
    _usePresetCheck->setChecked(true);
    
    QLabel *presetLabel = new QLabel(tr("Preset:"), this);
    _presetCombo = new QComboBox(this);
    
    QSettings settings(QStringLiteral("MidiEditor"), QStringLiteral("NONE"));
    QString lastPreset = settings.value("split_channels_last_preset", QString("Bard Forge 1")).toString();

    QStringList presets = DrumKitPresetManager::instance().presetNames();
    presets.sort(Qt::CaseInsensitive);

    for (const QString &p : presets) {
        QString displayName = DrumKitPresetManager::instance().isCustomPreset(p) ? p + " *" : p;
        _presetCombo->addItem(displayName, p);
    }
    
    int idx = _presetCombo->findData(lastPreset);
    if (idx >= 0) _presetCombo->setCurrentIndex(idx);
    else _presetCombo->setCurrentIndex(0);
    
    presetLayout->addWidget(_usePresetCheck);
    presetLayout->addStretch();
    presetLayout->addWidget(presetLabel);
    _presetCombo->setMinimumWidth(160);
    presetLayout->addWidget(_presetCombo);
    
    _editMapBtn = new QPushButton(tr("Edit Preset Map..."), this);
    _editMapBtn->setCheckable(true);
    presetLayout->addWidget(_editMapBtn);
    
    drumsLayout->addLayout(presetLayout);

    _mappingContainer = new QWidget(this);
    QVBoxLayout *mapLayout = new QVBoxLayout(_mappingContainer);
    mapLayout->setContentsMargins(0, 0, 0, 0);

    _mappingScrollArea = new QScrollArea(this);
    _mappingScrollArea->setWidgetResizable(true);
    _mappingScrollArea->setMinimumHeight(200);
    _mappingScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    _mappingScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    mapLayout->addWidget(_mappingScrollArea);

    // Reset/Save buttons fixed at bottom of mapping container
    QHBoxLayout *presetRow2 = new QHBoxLayout();
    presetRow2->addStretch();
    _resetPresetBtn = new QPushButton(tr("Reset Default"), this);
    _savePresetBtn = new QPushButton(tr("Save Preset"), this);
    presetRow2->addWidget(_resetPresetBtn);
    presetRow2->addWidget(_savePresetBtn);
    mapLayout->addLayout(presetRow2);
    
    drumsLayout->addWidget(_mappingContainer);
    _mappingContainer->hide();
    
    splitter->addWidget(_drumsGroup);
    _drumsGroup->hide();
    
    mainLayout->addWidget(splitter, 1);
    
    connect(_editMapBtn, &QPushButton::toggled, this, [this](bool checked) {
        _mappingContainer->setVisible(checked);
    });
    
    connect(_usePresetCheck, &QCheckBox::toggled, this, [this](bool checked) {
        _presetCombo->setEnabled(checked);
        _editMapBtn->setEnabled(checked);
        _mappingContainer->setEnabled(checked);
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
    
    _currentPreset = DrumKitPresetManager::instance().preset(_presetCombo->currentData().toString());

    // Options
    QGroupBox *optionsGroup = new QGroupBox(tr("Options"), this);
    QVBoxLayout *optionsLayout = new QVBoxLayout(optionsGroup);

    _keepDrumsCheck = new QCheckBox(tr("Keep Channel 9 (Percussion) on source track"), this);
    _keepDrumsCheck->setToolTip(tr("When checked, drum events stay on the source track instead of getting their own track"));
    optionsLayout->addWidget(_keepDrumsCheck);

    _forceDrumSplitCheck = new QCheckBox(tr("Force drum kit splitting on source track"), this);
    _forceDrumSplitCheck->setToolTip(tr("Allows splitting drum events to tracks even if they are not on channel 9"));
    optionsLayout->addWidget(_forceDrumSplitCheck);
    
    connect(_forceDrumSplitCheck, &QCheckBox::toggled, this, [this](bool) {
        analyzeTrack(_sourceTrack);
    });

    _removeSourceCheck = new QCheckBox(tr("Remove empty source track after split"), this);
    _removeSourceCheck->setChecked(true);
    _removeSourceCheck->setToolTip(tr("Deletes the original track if no events remain on it"));
    optionsLayout->addWidget(_removeSourceCheck);

    mainLayout->addWidget(optionsGroup);

    // Track position
    QGroupBox *positionGroup = new QGroupBox(tr("New Track Position"), this);
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

bool SplitChannelsDialog::forceDrumSplit() const {
    return _forceDrumSplitCheck && _forceDrumSplitCheck->isChecked();
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
            if (prog < 0) {
                prog = _file->channel(ch)->progAtTick(0);
            }

            QString name;
            if (ch == 9) {
                name = tr("Drumkit");
            } else if (prog >= 0) {
                name = InstrumentDefinitions::instance()->instrumentName(prog);
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
    
    if (_forceDrumSplitCheck && _forceDrumSplitCheck->isChecked()) {
        hasDrums = true;
    }

    if (hasDrums) {
        _drumsGroup->show();
        populateMappingTable();
        _keepDrumsCheck->setEnabled(!_usePresetCheck->isChecked());
    } else {
        _drumsGroup->hide();
        _keepDrumsCheck->setEnabled(true);
    }
}

void SplitChannelsDialog::populatePreviewTable(const QList<ChannelInfo> &channels) {
    _channelTable->setRowCount(channels.size());
    for (int row = 0; row < channels.size(); ++row) {
        const ChannelInfo &info = channels[row];

        auto *chItem = new QTableWidgetItem(QString::number(info.channel));
        chItem->setTextAlignment(Qt::AlignCenter);
        _channelTable->setItem(row, 0, chItem);

        QString progStr = (info.channel == 9) ? tr("Drumkit") :
                          (info.programNumber >= 0) ? QString::number(info.programNumber) : QStringLiteral("-");
        auto *progItem = new QTableWidgetItem(progStr);
        progItem->setTextAlignment(Qt::AlignCenter);
        _channelTable->setItem(row, 1, progItem);

        auto *noteItem = new QTableWidgetItem(QString::number(info.noteCount));
        noteItem->setTextAlignment(Qt::AlignCenter);
        _channelTable->setItem(row, 2, noteItem);

        _channelTable->setItem(row, 3, new QTableWidgetItem(info.instrumentName));
    }
    _channelTable->resizeColumnsToContents();
    _channelTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    _channelTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    _channelTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    _channelTable->setColumnWidth(2, 120);
    _channelTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    
    // Ensure rows are consistently sized
    for (int i = 0; i < _channelTable->rowCount(); ++i) {
        _channelTable->setRowHeight(i, 28);
    }
    
    _channelTable->setMinimumHeight(qMin(channels.size() * 30 + 40, 300));
}

void SplitChannelsDialog::populateMappingTable() {
    if (!_mappingScrollArea) return;
    
    _updatingTable = true;
    
    QWidget *scrollWidget = new QWidget(this);
    QVBoxLayout *scrollLayout = new QVBoxLayout(scrollWidget);
    scrollLayout->setSpacing(10);
    
    for (int g = 0; g < _currentPreset.groups.size(); ++g) {
        DrumTrackGroup group = _currentPreset.groups[g];
        if (group.mappings.isEmpty()) continue;
        
        QFrame *groupFrame = new QFrame(scrollWidget);
        groupFrame->setFrameShape(QFrame::StyledPanel);
        
        QVBoxLayout *groupLayout = new QVBoxLayout(groupFrame);
        groupLayout->setContentsMargins(8, 8, 8, 8);
        
        QLabel *titleLabel = new QLabel(QString("<b>%1</b> %2").arg(tr("Track:")).arg(group.trackName), groupFrame);
        titleLabel->setStyleSheet("font-size: 13px;");
        groupLayout->addWidget(titleLabel);
        
        QGridLayout *gridLayout = new QGridLayout();
        gridLayout->setSpacing(6);
        groupLayout->addLayout(gridLayout);
        
        std::sort(group.mappings.begin(), group.mappings.end(), [](const DrumNoteMapping &a, const DrumNoteMapping &b) {
            return a.sourceNote < b.sourceNote;
        });
        
        int col = 0;
        int row = 0;
        for (int m = 0; m < group.mappings.size(); ++m) {
            const auto &mapping = group.mappings[m];
            
            QFrame *card = new QFrame(groupFrame);
            card->setFrameShape(QFrame::StyledPanel);
            card->setFrameShadow(QFrame::Raised);
            QVBoxLayout *cardLayout = new QVBoxLayout(card);
            cardLayout->setContentsMargins(4, 4, 4, 4);
            cardLayout->setSpacing(4);
            
            QHBoxLayout *topLayout = new QHBoxLayout;
            topLayout->setSpacing(4);
            
            QGroupBox *sourceBox = new QGroupBox(mapping.drumName, card);
            QVBoxLayout *srcL = new QVBoxLayout(sourceBox);
            srcL->setContentsMargins(4, 12, 4, 4);
            QLabel *srcLabel = new QLabel(QString("%1 (%2)").arg(mapping.sourceNote).arg(getNoteName(mapping.sourceNote)), sourceBox);
            srcLabel->setAlignment(Qt::AlignCenter);
            srcL->addWidget(srcLabel);
            
            QGroupBox *targetBox = new QGroupBox(tr("Transpose to Note"), card);
            QHBoxLayout *tgtL = new QHBoxLayout(targetBox);
            tgtL->setContentsMargins(4, 12, 4, 4);
            
            NoteSpinBox *targetNoteSpin = new NoteSpinBox(targetBox);
            targetNoteSpin->setRange(0, 127);
            targetNoteSpin->setValue(mapping.targetNote);
            
            QToolButton *playBtn = new QToolButton(targetBox);
            playBtn->setIcon(QIcon(":/run_environment/graphics/tool/play.png"));
            playBtn->setToolTip(tr("Preview Note"));
            
            tgtL->addWidget(targetNoteSpin);
            tgtL->addWidget(playBtn);
            
            topLayout->addWidget(sourceBox, 1);
            topLayout->addWidget(targetBox, 1);
            cardLayout->addLayout(topLayout);
            
            QComboBox *groupCombo = new QComboBox(card);
            for (const auto &grp : _currentPreset.groups) {
                groupCombo->addItem(grp.trackName);
            }
            groupCombo->setCurrentText(group.trackName);
            cardLayout->addWidget(groupCombo);
            
            gridLayout->addWidget(card, row, col);
            
            col++;
            if (col >= 3) {
                col = 0;
                row++;
            }
            
            int sNote = mapping.sourceNote;
            bool isPerc = group.staysOnPercussionChannel;
            int prog = group.programNumber;
            
            connect(groupCombo, &QComboBox::currentTextChanged, this, [this, sNote](const QString &txt) {
                if (!_updatingTable) {
                    int scrollV = _mappingScrollArea->verticalScrollBar()->value();
                    onGroupChanged(sNote, txt);
                    populateMappingTable();
                    _mappingScrollArea->verticalScrollBar()->setValue(scrollV);
                }
            });
            connect(targetNoteSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this, sNote](int val) {
                if (!_updatingTable) onTargetNoteChanged(sNote, val);
            });
            connect(playBtn, &QToolButton::clicked, this, [playBtn, targetNoteSpin, isPerc, prog]() {
                QTimer *tmr = playBtn->findChild<QTimer*>("PlayTimer");
                if (!tmr) {
                    tmr = new QTimer(playBtn);
                    tmr->setObjectName("PlayTimer");
                    tmr->setSingleShot(true);
                }
                tmr->disconnect();
                
                int ch = isPerc ? 9 : 0;
                int note = targetNoteSpin->value();
                
                // Ensure any previous preview is properly silenced
                QByteArray off; off.append((char)(0x80 | ch)); off.append((char)note); off.append((char)0);
                MidiOutput::sendCommand(off);
                
                if (!isPerc && prog >= 0) {
                    MidiOutput::sendProgram(ch, prog);
                }
                QByteArray onMsg; onMsg.append((char)(0x90 | ch)); onMsg.append((char)note); onMsg.append((char)100);
                MidiOutput::sendCommand(onMsg);
                
                QObject::connect(tmr, &QTimer::timeout, playBtn, [ch, note]() {
                    QByteArray offMsg; offMsg.append((char)(0x80 | ch)); offMsg.append((char)note); offMsg.append((char)0);
                    MidiOutput::sendCommand(offMsg);
                });
                tmr->start(500);
            });
        }
        gridLayout->setColumnStretch(0, 1);
        gridLayout->setColumnStretch(1, 1);
        gridLayout->setColumnStretch(2, 1);
        scrollLayout->addWidget(groupFrame);
    }
    
    scrollLayout->addStretch();
    _mappingScrollArea->setWidget(scrollWidget);
    
    _updatingTable = false;
}

void SplitChannelsDialog::onPresetSelected(const QString &presetName) {
    if (_usePresetCheck && !_usePresetCheck->isChecked()) return;
    int idx = _presetCombo->findText(presetName);
    if (idx < 0) return;
    QString realName = _presetCombo->itemData(idx).toString();
    _currentPreset = DrumKitPresetManager::instance().preset(realName);
    
    QSettings settings(QStringLiteral("MidiEditor"), QStringLiteral("NONE"));
    settings.setValue("split_channels_last_preset", realName);
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
    int res = QMessageBox::warning(this, tr("Reset Preset"), tr("Are you sure you want to discard your custom edits to this preset and restore the defaults?"), QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if(res != QMessageBox::Yes) return;
    
    QString realName = _presetCombo->currentData().toString();
    DrumKitPresetManager::instance().resetPreset(realName);
    _currentPreset = DrumKitPresetManager::instance().preset(realName);
    int idx = _presetCombo->currentIndex();
    _presetCombo->setItemText(idx, realName); // Remove *
    populateMappingTable();
}

void SplitChannelsDialog::onSavePresetClicked() {
    DrumKitPresetManager::instance().savePreset(_currentPreset);
    int idx = _presetCombo->currentIndex();
    QString realName = _presetCombo->itemData(idx).toString();
    if(DrumKitPresetManager::instance().isCustomPreset(realName)) {
        _presetCombo->setItemText(idx, realName + " *");
    }
}
