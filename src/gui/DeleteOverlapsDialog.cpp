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

#include "DeleteOverlapsDialog.h"
#include <QTimer>

DeleteOverlapsDialog::DeleteOverlapsDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Delete Overlaps"));
    setModal(true);

    setupUI();
    setupConnections();

    // Set default selection
    _monoModeRadio->setChecked(true);
    _respectChannelsCheckBox->setChecked(true);
    _respectTracksCheckBox->setChecked(true);

    // Update initial description
    onModeChanged();

    // Set fixed size after UI is set up to ensure proper sizing
    setFixedSize(480, 420);
}

void DeleteOverlapsDialog::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSizeConstraint(QLayout::SetFixedSize);  // Prevent layout from resizing
    
    // Mode selection group
    QGroupBox* modeGroup = new QGroupBox(tr("Delete Mode"), this);
    modeGroup->setFixedHeight(200);  // Increased height for mode group
    QVBoxLayout* modeLayout = new QVBoxLayout(modeGroup);
    
    _monoModeRadio = new QRadioButton(tr("Delete Overlaps (Mono)"), modeGroup);
    _polyModeRadio = new QRadioButton(tr("Delete Overlaps (Poly)"), modeGroup);
    _doublesModeRadio = new QRadioButton(tr("Delete Doubles"), modeGroup);
    
    modeLayout->addWidget(_monoModeRadio);
    modeLayout->addWidget(_polyModeRadio);
    modeLayout->addWidget(_doublesModeRadio);

    // Add dynamic description label with fixed size
    _modeDescriptionLabel = new QLabel(modeGroup);
    _modeDescriptionLabel->setWordWrap(true);
    _modeDescriptionLabel->setFixedSize(400, 90);  // Fixed width AND height
    _modeDescriptionLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    _modeDescriptionLabel->setStyleSheet("QLabel { color: gray; font-size: 10px; margin-left: 20px; margin-bottom: 10px; padding: 5px; }");
    _modeDescriptionLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    modeLayout->addWidget(_modeDescriptionLabel);
    
    mainLayout->addWidget(modeGroup);
    
    // Boundary options group
    QGroupBox* boundaryGroup = new QGroupBox(tr("Boundary Options"), this);
    boundaryGroup->setFixedHeight(160);  // Increased height for boundary group
    QVBoxLayout* boundaryLayout = new QVBoxLayout(boundaryGroup);

    _respectTracksCheckBox = new QCheckBox(tr("Respect track boundaries"), boundaryGroup);
    boundaryLayout->addWidget(_respectTracksCheckBox);

    QLabel* trackDesc = new QLabel(tr("When checked: Only process overlaps within the same track.\nWhen unchecked: Process overlaps across all selected tracks."), boundaryGroup);
    trackDesc->setWordWrap(true);
    trackDesc->setStyleSheet("QLabel { color: gray; font-size: 10px; margin-left: 20px; margin-bottom: 5px; }");
    boundaryLayout->addWidget(trackDesc);

    _respectChannelsCheckBox = new QCheckBox(tr("Respect channel boundaries"), boundaryGroup);
    boundaryLayout->addWidget(_respectChannelsCheckBox);

    QLabel* channelDesc = new QLabel(tr("When checked: Only process overlaps within the same MIDI channel.\nWhen unchecked: Process overlaps across all channels."), boundaryGroup);
    channelDesc->setWordWrap(true);
    channelDesc->setStyleSheet("QLabel { color: gray; font-size: 10px; margin-left: 20px; }");
    boundaryLayout->addWidget(channelDesc);

    mainLayout->addWidget(boundaryGroup);
    
    // Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    
    _cancelButton = new QPushButton(tr("Cancel"), this);
    _okButton = new QPushButton(tr("OK"), this);
    _okButton->setDefault(true);
    
    buttonLayout->addWidget(_okButton);
    buttonLayout->addWidget(_cancelButton);
    
    mainLayout->addLayout(buttonLayout);
}

void DeleteOverlapsDialog::setupConnections()
{
    connect(_okButton, SIGNAL(clicked()), this, SLOT(onOkClicked()));
    connect(_cancelButton, SIGNAL(clicked()), this, SLOT(onCancelClicked()));

    // Connect radio buttons to update description
    connect(_monoModeRadio, SIGNAL(toggled(bool)), this, SLOT(onModeChanged()));
    connect(_polyModeRadio, SIGNAL(toggled(bool)), this, SLOT(onModeChanged()));
    connect(_doublesModeRadio, SIGNAL(toggled(bool)), this, SLOT(onModeChanged()));
}

DeleteOverlapsTool::OverlapMode DeleteOverlapsDialog::getSelectedMode() const
{
    if (_monoModeRadio->isChecked()) {
        return DeleteOverlapsTool::MONO_MODE;
    } else if (_polyModeRadio->isChecked()) {
        return DeleteOverlapsTool::POLY_MODE;
    } else {
        return DeleteOverlapsTool::DOUBLES_MODE;
    }
}

bool DeleteOverlapsDialog::getRespectChannels() const
{
    return _respectChannelsCheckBox->isChecked();
}

bool DeleteOverlapsDialog::getRespectTracks() const
{
    return _respectTracksCheckBox->isChecked();
}

void DeleteOverlapsDialog::onOkClicked()
{
    accept();
}

void DeleteOverlapsDialog::onCancelClicked()
{
    reject();
}

void DeleteOverlapsDialog::onModeChanged()
{
    if (!_modeDescriptionLabel) {
        return; // Safety check
    }

    QString description;

    if (_monoModeRadio && _monoModeRadio->isChecked()) {
        description = tr("Removes overlapping notes of the same pitch. When notes of the same pitch overlap, "
                        "longer notes are preserved and shorter overlapping notes are removed or shortened. "
                        "This is useful for cleaning up duplicate notes or notes hidden under long sustains.");
    } else if (_polyModeRadio && _polyModeRadio->isChecked()) {
        description = tr("Makes the part monophonic by shortening all overlapping notes regardless of pitch. "
                        "No two notes will overlap after this operation. This is useful for preparing parts "
                        "for monophonic synthesizers or removing unwanted overlaps in solo lines.");
    } else if (_doublesModeRadio && _doublesModeRadio->isChecked()) {
        description = tr("Removes notes that are exact duplicates (same pitch, start time, end time, and duration). "
                        "This is useful for cleaning up accidentally duplicated notes or MIDI recording artifacts.");
    }

    // Ensure the text is set and the widget is updated
    _modeDescriptionLabel->setText(description);
    _modeDescriptionLabel->update();
    _modeDescriptionLabel->repaint();
}

void DeleteOverlapsDialog::paintEvent(QPaintEvent* event)
{
    // Call the base class paint event first
    QDialog::paintEvent(event);

    // Ensure the description label is always properly painted
    if (_modeDescriptionLabel && !_modeDescriptionLabel->text().isEmpty()) {
        _modeDescriptionLabel->update();
    }
}

void DeleteOverlapsDialog::moveEvent(QMoveEvent* event)
{
    // Call the base class move event first
    QDialog::moveEvent(event);

    // Force refresh of the description label after window move
    if (_modeDescriptionLabel) {
        // Use a single-shot timer to refresh after the move is complete
        QTimer::singleShot(0, this, [this]() {
            if (_modeDescriptionLabel) {
                _modeDescriptionLabel->update();
                _modeDescriptionLabel->repaint();
            }
        });
    }
}

void DeleteOverlapsDialog::resizeEvent(QResizeEvent* event)
{
    // Ignore resize events and maintain fixed size
    Q_UNUSED(event);

    // Force the dialog to stay at the fixed size
    if (size() != QSize(480, 420)) {
        resize(480, 420);
    }
}
