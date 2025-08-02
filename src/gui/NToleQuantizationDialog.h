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

#ifndef NTOLEQUANTIZATIONDIALOG_H_
#define NTOLEQUANTIZATIONDIALOG_H_

// Qt includes
#include <QDialog>

// Forward declarations
class QComboBox;

/**
 * \class NToleQuantizationDialog
 *
 * \brief Dialog for configuring N-tole (tuplet) quantization settings.
 *
 * NToleQuantizationDialog allows users to configure advanced quantization
 * settings for creating tuplets (triplets, quintuplets, etc.) and other
 * complex rhythmic patterns. It provides:
 *
 * - **N-tole configuration**: Set the number of notes in the tuplet
 * - **Beat subdivision**: Choose the beat value for the tuplet
 * - **Replacement ratios**: Configure how existing notes are quantized
 * - **Flexible timing**: Support for complex rhythmic patterns
 *
 * The dialog is used for advanced quantization operations that go beyond
 * simple grid-based quantization, allowing for musical tuplets and
 * irregular rhythmic divisions.
 */
class NToleQuantizationDialog : public QDialog {
    Q_OBJECT

public:
    /**
     * \brief Creates a new NToleQuantizationDialog.
     * \param parent The parent widget
     */
    NToleQuantizationDialog(QWidget *parent = 0);

    /** \brief Static variables storing quantization parameters */
    static int ntoleNNum;        ///< Number of notes in the N-tole
    static int ntoleBeatNum;     ///< Beat value for the N-tole
    static int replaceNumNum;    ///< Numerator for replacement ratio
    static int replaceDenomNum;  ///< Denominator for replacement ratio

public slots:
    /**
     * \brief Takes the results from the dialog and stores them in static variables.
     */
    void takeResults();

private:
    /** \brief Combo boxes for N-tole configuration */
    QComboBox *ntoleN;          ///< N-tole number selection
    QComboBox *ntoleBeat;       ///< Beat value selection
    QComboBox *replaceNum;      ///< Replacement numerator selection
    QComboBox *replaceDenom;    ///< Replacement denominator selection
};

#endif // NTOLEQUANTIZATIONDIALOG_H_
