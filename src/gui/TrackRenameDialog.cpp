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

#include "TrackRenameDialog.h"
#include "Appearance.h"
#include "GameSupportSettingsWidget.h"
#include "../support/FFXIVChannelFixer.h"
#include "../midi/InstrumentDefinitions.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QToolButton>
#include <QMenu>
#include <QAction>
#include <QSettings>

TrackRenameDialog::TrackRenameDialog(const QString &currentName, int trackNumber, QSettings *settings, QWidget *parent)
    : QDialog(parent), _settings(settings), _trackNumber(trackNumber) {
    
    setWindowTitle(tr("Set Track Name"));
    setMinimumWidth(350);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 10, 20, 20);
    mainLayout->setSpacing(5);

    QHBoxLayout *topLayout = new QHBoxLayout();
    QLabel *label = new QLabel(tr("Track Name (Track %1)").arg(trackNumber), this);
    topLayout->addWidget(label);
    topLayout->addStretch();

    if (GameSupportSettingsWidget::isFFXIVRenamePresetsEnabled(_settings)) {
        _presetsButton = new QToolButton(this);
        _presetsButton->setIcon(QIcon(":/run_environment/graphics/channelwidget/instrument.png"));
        _presetsButton->setIconSize(QSize(20, 20));
        _presetsButton->setFixedSize(QSize(30, 30));
        _presetsButton->setToolTip(tr("FFXIV Instrument Presets"));
        _presetsButton->setPopupMode(QToolButton::InstantPopup);
        
        QMenu *menu = new QMenu(_presetsButton);
        menu->setStyleSheet("QMenu { menu-scrollable: 1; }"); // Ensure scrolling if too many items

        QList<FFXIVChannelFixer::InstrumentGroup> groups = FFXIVChannelFixer::ffxivInstrumentGroups();
        InstrumentDefinitions *defs = InstrumentDefinitions::instance();

        for (const auto &group : groups) {
            QMenu *groupMenu = menu->addMenu(group.name);
            for (int program : group.programs) {
                QString name = defs->instrumentName(program);
                name.remove(" ");
                if (group.name == tr("Guitars")) {
                    name.remove(":");
                }

                QAction *action = groupMenu->addAction(name);
                action->setData(name);
                connect(action, SIGNAL(triggered()), this, SLOT(setPresetName()));
            }

            if (group.name == tr("Guitars")) {
                QString special = "Program:ElectricGuitar";
                QAction *action = groupMenu->addAction(special);
                action->setData(special);
                connect(action, SIGNAL(triggered()), this, SLOT(setPresetName()));
            }
        }

        _presetsButton->setMenu(menu);
        topLayout->addWidget(_presetsButton);
    }

    mainLayout->addLayout(topLayout);

    _nameEdit = new QLineEdit(currentName, this);
    _nameEdit->setPlaceholderText(tr("Enter track name..."));
    _nameEdit->setFixedHeight(30);
    _nameEdit->selectAll();
    mainLayout->addWidget(_nameEdit);

    mainLayout->addSpacing(10);
    QHBoxLayout *buttonLayout = new QHBoxLayout();

    QPushButton *cancelButton = new QPushButton(tr("Cancel"), this);
    connect(cancelButton, SIGNAL(clicked()), this, SLOT(reject()));
    buttonLayout->addWidget(cancelButton);

    buttonLayout->addStretch();

    QPushButton *acceptButton = new QPushButton(tr("Accept"), this);
    acceptButton->setDefault(true);
    connect(acceptButton, SIGNAL(clicked()), this, SLOT(accept()));
    buttonLayout->addWidget(acceptButton);

    mainLayout->addLayout(buttonLayout);
}

QString TrackRenameDialog::name() const {
    return _nameEdit->text();
}

void TrackRenameDialog::showPresets() {
    // Handled by QToolButton popup mode
}

void TrackRenameDialog::setPresetName() {
    QAction *action = qobject_cast<QAction *>(sender());
    if (action) {
        _nameEdit->setText(action->data().toString());
        _nameEdit->setFocus();
    }
}
