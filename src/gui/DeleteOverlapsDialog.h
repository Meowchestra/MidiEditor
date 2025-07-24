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

#ifndef DELETEOVERLAPSDIALOG_H_
#define DELETEOVERLAPSDIALOG_H_

#include <QDialog>
#include <QRadioButton>
#include <QCheckBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>

#include "../tool/DeleteOverlapsTool.h"

/**
 * \class DeleteOverlapsDialog
 *
 * \brief Dialog for selecting delete overlaps options.
 *
 * This dialog allows users to choose between different overlap deletion modes
 * and whether to respect channel boundaries.
 */
class DeleteOverlapsDialog : public QDialog {
    Q_OBJECT

public:
    /**
     * \brief Creates a new DeleteOverlapsDialog.
     * \param parent Parent widget
     */
    explicit DeleteOverlapsDialog(QWidget* parent = nullptr);

    /**
     * \brief Gets the selected overlap mode.
     * \return The selected DeleteOverlapsTool::OverlapMode
     */
    DeleteOverlapsTool::OverlapMode getSelectedMode() const;

    /**
     * \brief Gets whether channels should be respected.
     * \return True if channels should be respected, false otherwise
     */
    bool getRespectChannels() const;

    /**
     * \brief Gets whether tracks should be respected.
     * \return True if tracks should be respected, false otherwise
     */
    bool getRespectTracks() const;

private slots:
    /**
     * \brief Handles OK button click.
     */
    void onOkClicked();

    /**
     * \brief Handles Cancel button click.
     */
    void onCancelClicked();

    /**
     * \brief Updates the description when mode selection changes.
     */
    void onModeChanged();

private:
    QRadioButton* _monoModeRadio;
    QRadioButton* _polyModeRadio;
    QRadioButton* _doublesModeRadio;
    QCheckBox* _respectChannelsCheckBox;
    QCheckBox* _respectTracksCheckBox;
    QLabel* _modeDescriptionLabel;
    QPushButton* _okButton;
    QPushButton* _cancelButton;

    void setupUI();
    void setupConnections();
};

#endif
