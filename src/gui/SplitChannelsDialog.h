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

#ifndef SPLITCHANNELSDIALOG_H_
#define SPLITCHANNELSDIALOG_H_

#include <QDialog>
#include <QCheckBox>
#include <QRadioButton>
#include <QTableWidget>
#include <QScrollArea>
#include <QComboBox>
#include <QPushButton>
#include <QGroupBox>
#include "DrumKitPreset.h"

class MidiFile;
class MidiTrack;

class SplitChannelsDialog : public QDialog {
    Q_OBJECT

public:
    struct ChannelInfo {
        int channel;
        int eventCount;
        int noteCount;
        int programNumber;
        QString instrumentName;
    };

    SplitChannelsDialog(MidiFile *file, MidiTrack *sourceTrack, QWidget *parent = nullptr);

    bool keepDrumsOnSource() const;
    bool removeEmptySource() const;
    bool insertAtEnd() const;
    bool forceDrumSplit() const;

    bool useDrumKitPreset() const;
    DrumKitPreset selectedDrumKitPreset() const;
    MidiTrack *sourceTrack() const;

private slots:
    void onSourceTrackChanged(int index);
    void onPresetSelected(const QString &presetName);
    void onTargetNoteChanged(int sourceNote, int val);
    void onGroupChanged(int sourceNote, const QString &group);
    void onResetPresetClicked();
    void onSavePresetClicked();

private:
    void populatePreviewTable(const QList<ChannelInfo> &channels);
    void populateMappingTable();
    void analyzeTrack(MidiTrack *track);

    MidiFile *_file;
    MidiTrack *_sourceTrack;

    QComboBox *_sourceTrackCombo;

    QCheckBox *_keepDrumsCheck;
    QCheckBox *_removeSourceCheck;
    QCheckBox *_forceDrumSplitCheck;
    QRadioButton *_insertAfterRadio;
    QRadioButton *_insertAtEndRadio;
    QTableWidget *_channelTable;

    QGroupBox *_drumsGroup;
    QCheckBox *_usePresetCheck;
    QComboBox *_presetCombo;
    QPushButton *_editMapBtn;
    QWidget *_mappingContainer;
    QScrollArea *_mappingScrollArea;
    QPushButton *_resetPresetBtn;
    QPushButton *_savePresetBtn;

    DrumKitPreset _currentPreset;
    bool _updatingTable = false;
};

#endif // SPLITCHANNELSDIALOG_H_
