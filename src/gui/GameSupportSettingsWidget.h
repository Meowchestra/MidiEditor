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

#ifndef GAMESUPPORTSETTINGSWIDGET_H_
#define GAMESUPPORTSETTINGSWIDGET_H_

// Project includes
#include "SettingsWidget.h"

// Forward declarations
class QSettings;
class QCheckBox;
class QGroupBox;

/**
 * \class GameSupportSettingsWidget
 *
 * \brief Settings widget for game-specific MIDI support features.
 *
 * GameSupportSettingsWidget provides configuration for game-specific
 * features that extend the editor with specialised tools for creating
 * MIDI files targeting specific games:
 *
 * - **Final Fantasy XIV**: Bard Performance MIDI support including
 *   automatic channel/program assignment based on instrument names,
 *   and a "Fix XIV Channels" button in the Tracks and Channels widgets.
 *
 * Additional game profiles can be added in the future by adding new
 * sections to this settings panel.
 */
class GameSupportSettingsWidget : public SettingsWidget {
    Q_OBJECT

public:
    /**
     * \brief Creates a new GameSupportSettingsWidget.
     * \param settings QSettings instance for configuration storage
     * \param parent The parent widget
     */
    explicit GameSupportSettingsWidget(QSettings *settings, QWidget *parent = nullptr);

    /**
     * \brief Validates and applies the settings changes.
     * \return True if settings are valid and applied successfully
     */
    bool accept() override;

    /**
     * \brief Gets the icon for this settings panel.
     * \return QIcon for the game support settings
     */
    QIcon icon() override;

    /**
     * \brief Returns whether FFXIV support is currently enabled.
     * \param settings QSettings instance to read from
     * \return True if FFXIV support is enabled
     */
    static bool isFFXIVEnabled(QSettings *settings);

signals:
    /**
     * \brief Emitted when any settings in this widget are changed
     */
    void settingsChanged();

    /**
     * \brief Emitted immediately when a setting is toggled, before accept() is called
     */
    void immediateSettingsChanged(bool ffxivEnabled);

private slots:
    void onFFXIVBoxToggled(bool checked);

private:
    /** \brief Settings storage */
    QSettings *_settings;

    /** \brief FFXIV section group box */
    QGroupBox *_ffxivGroup;

    /** \brief FFXIV enable checkbox */
    QCheckBox *_ffxivEnabledBox;
};

#endif // GAMESUPPORTSETTINGSWIDGET_H_
