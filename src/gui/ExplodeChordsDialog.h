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

#ifndef EXPLODECHORDSDIALOG_H_
#define EXPLODECHORDSDIALOG_H_

#include <QDialog>
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>

class MidiFile;
class MidiTrack;

/**
 * \class ExplodeChordsDialog
 *
 * \brief Dialog for configuring chord explosion to separate tracks.
 *
 * Provides a unified interface for all chord splitting options including
 * split strategy, grouping mode, minimum notes threshold, and track placement.
 */
class ExplodeChordsDialog : public QDialog {
    Q_OBJECT

public:
    enum SplitStrategy {
        SAME_START = 0,
        SAME_START_AND_LENGTH = 1
    };

    enum GroupMode {
        VOICES_ACROSS_CHORDS = 0,
        EACH_CHORD_OWN_TRACK = 1,
        ALL_CHORDS_ONE_TRACK = 2
    };

    /**
     * \brief Creates a new ExplodeChordsDialog.
     * \param file The MIDI file containing the chords
     * \param sourceTrack The track to analyze for chords
     * \param parent Parent widget
     */
    ExplodeChordsDialog(MidiFile *file, MidiTrack *sourceTrack, QWidget *parent = nullptr);

    /**
     * \brief Gets the selected split strategy.
     */
    SplitStrategy splitStrategy() const;

    /**
     * \brief Gets the selected group mode.
     */
    GroupMode groupMode() const;

    /**
     * \brief Gets the minimum number of simultaneous notes.
     */
    int minimumNotes() const;

    /**
     * \brief Gets whether to insert tracks at end.
     */
    bool insertAtEnd() const;

    /**
     * \brief Gets whether to keep original notes on source track.
     */
    bool keepOriginalNotes() const;

private slots:
    /**
     * \brief Updates the preview text when settings change.
     */
    void updatePreview();

private:
    MidiFile *_file;
    MidiTrack *_sourceTrack;

    QComboBox *_strategyCombo;
    QComboBox *_groupModeCombo;
    QComboBox *_minNotesCombo;
    QCheckBox *_insertAtEndCheck;
    QCheckBox *_keepOriginalCheck;
    QLabel *_previewLabel;

    /**
     * \brief Analyzes chords based on current settings.
     * \param chordCount Output: number of chord groups found
     * \param maxVoices Output: maximum number of voices in any chord
     */
    void analyzeChords(int &chordCount, int &maxVoices);
};

#endif // EXPLODECHORDSDIALOG_H_
