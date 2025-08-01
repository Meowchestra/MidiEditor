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

#ifndef TOOLBARACTIONINFO_H_
#define TOOLBARACTIONINFO_H_

// Qt includes
#include <QString>

// Forward declarations
class QAction;

/**
 * \struct ToolbarActionInfo
 *
 * \brief Information structure for toolbar action configuration.
 *
 * ToolbarActionInfo contains all the metadata needed to manage toolbar
 * actions in the customizable toolbar system. It includes:
 *
 * - **Identification**: Unique ID and display name for the action
 * - **Visual elements**: Icon path and display properties
 * - **State management**: Enabled/disabled state and essential flag
 * - **Organization**: Category grouping for logical arrangement
 * - **Action reference**: Direct link to the QAction object
 *
 * This structure is used by the LayoutSettingsWidget and toolbar
 * management system to provide flexible toolbar customization while
 * maintaining essential functionality.
 */
struct ToolbarActionInfo {
    QString id;          ///< Unique identifier for the action
    QString name;        ///< Display name for the action
    QString iconPath;    ///< Path to the action's icon file
    QAction *action;     ///< Pointer to the actual QAction object
    bool enabled;        ///< Whether the action is enabled in the toolbar
    bool essential;      ///< Cannot be disabled (New, Open, Save, Undo, Redo)
    QString category;    ///< Category for grouping related actions
};

#endif // TOOLBARACTIONINFO_H
