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

#ifndef SETTINGSWIDGET_H_
#define SETTINGSWIDGET_H_

// Qt includes
#include <QWidget>

// Forward declarations
class QString;

/**
 * \class SettingsWidget
 *
 * \brief Base class for individual settings panels in the SettingsDialog.
 *
 * SettingsWidget provides the foundation for creating modular settings panels
 * that can be added to the main SettingsDialog. Each settings category
 * (MIDI, Appearance, etc.) inherits from this class to provide:
 *
 * - **Modular design**: Self-contained settings panels
 * - **Consistent interface**: Standard methods for all settings widgets
 * - **UI helpers**: Common UI elements like info boxes and separators
 * - **Validation**: Accept/reject settings changes
 * - **Visual identity**: Icons and titles for navigation
 *
 * Key features:
 * - Virtual accept() method for settings validation
 * - Helper methods for creating common UI elements
 * - Title and icon support for navigation display
 * - Integration with the main SettingsDialog
 *
 * Subclasses implement specific settings categories and override
 * the accept() method to validate and apply their settings.
 */
class SettingsWidget : public QWidget {
public:
    /**
     * \brief Creates a new SettingsWidget.
     * \param title The title for this settings panel
     * \param parent The parent widget
     */
    SettingsWidget(QString title, QWidget *parent = 0);

    /**
     * \brief Gets the title of this settings panel.
     * \return The panel title string
     */
    QString title();

    /**
     * \brief Validates and applies the settings changes.
     * \return True if settings are valid and applied successfully
     */
    virtual bool accept();

    /**
     * \brief Creates an information box widget.
     * \param info The information text to display
     * \return Pointer to the created info box widget
     */
    QWidget *createInfoBox(QString info);

    /**
     * \brief Creates a visual separator widget.
     * \return Pointer to the created separator widget
     */
    QWidget *separator();

    /**
     * \brief Gets the icon for this settings panel.
     * \return QIcon for display in the navigation list
     */
    virtual QIcon icon();

private:
    /** \brief The title of this settings panel */
    QString _title;
};

#endif // SETTINGSWIDGET_H_
