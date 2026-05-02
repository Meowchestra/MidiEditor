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

#ifndef TRANSPOSEDIALOG_H_
#define TRANSPOSEDIALOG_H_

// Qt includes
#include <QDialog>
#include <QList>
#include <QRadioButton>
#include <QSpinBox>

// Forward declarations
class NoteOnEvent;
class MidiFile;
class QComboBox;
class QCheckBox;

/**
 * \class TransposeDialog
 *
 * \brief Dialog for transposing selected notes by a specified interval.
 *
 * TransposeDialog provides a user interface for transposing MIDI notes
 * up or down by a specified number of semitones. It features:
 *
 * - **Direction selection**: Radio buttons for up or down transposition
 * - **Interval specification**: Spin box for entering semitone intervals
 * - **Range validation**: Ensures transposed notes stay within MIDI range (0-127)
 * - **Batch processing**: Transposes multiple selected notes simultaneously
 * - **Undo support**: Integrates with the protocol system for undo/redo
 *
 * The dialog validates that all transposed notes will remain within the
 * valid MIDI note range and provides feedback if any notes would be
 * out of range after transposition.
 */
class QComboBox;
class QStackedWidget;

/**
 * \class TransposeDialog
 *
 * \brief Advanced dialog for transposing MIDI notes.
 */
class TransposeDialog : public QDialog {
    Q_OBJECT

public:
    /**
     * \brief Creates a new TransposeDialog.
     * \param toTranspose List of NoteOnEvent objects to transpose
     * \param file The MidiFile containing the notes
     * \param parent The parent widget
     */
    TransposeDialog(QList<NoteOnEvent *> toTranspose, MidiFile *file, QWidget *parent = 0);

public slots:
    /**
     * \brief Accepts the dialog and applies the transposition.
     */
    void accept();

private slots:
    void updateIntervalLabel();
    void onIntervalComboChanged(int index);

private:
    /** \brief List of notes to transpose */
    QList<NoteOnEvent *> _toTranspose;

    /** \brief The MIDI file containing the notes */
    MidiFile *_file;

    QComboBox *_modeSelector;
    QStackedWidget *_stack;

    // Mode 1: Standard (Interval)
    QSpinBox *_chromaticOctBox;
    QSpinBox *_chromaticMsBox;
    QRadioButton *_chromaticUp, *_chromaticDown;
    QComboBox *_chromaticIntervalCombo;
    QLabel *_chromaticIntervalInfo;

    // Mode 2: Key Conversion
    QComboBox *_fromKeyRoot, *_fromKeyMode;
    QComboBox *_toKeyRoot, *_toKeyMode;
    QComboBox *_keyDirection;

    // Mode 3: Scale-Aware Shift
    QComboBox *_diatonicScaleRoot, *_diatonicScaleType;
    QSpinBox *_diatonicSteps;
    QRadioButton *_diatonicUp, *_diatonicDown;

    QWidget* createStandardTab();
    QWidget* createKeyTab();
    QWidget* createScaleTab();

    QString identifyChord();
    QString identifyRange();
    int calculateChromaticShift();
    int performDiatonicTranspose(int note, int steps, int root, int scaleType);
    int performModalInterchange(int note, int srcRoot, int srcType, int tgtRoot, int tgtType);
    void applyRangeProtection(int &shift);
    void detectScale(int &outRoot, int &outType);
    bool _protectRange;
    QCheckBox *_excludeDrums;
};

#endif // TRANSPOSEDIALOG_H_
