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

#ifndef SETTINGSDIALOG_H_
#define SETTINGSDIALOG_H_

// Qt includes
#include <QDialog>
#include <QSettings>

// Forward declarations
class QListWidget;
class QWidget;
class QString;
class QStackedWidget;
class SettingsWidget;
class MainWindow;

/**
 * \class SettingsDialog
 *
 * \brief Main settings dialog with tabbed interface for configuration options.
 *
 * SettingsDialog provides a centralized interface for all application settings.
 * It uses a list-based navigation system with multiple settings panels:
 *
 * - **Tabbed interface**: List widget for navigation between setting categories
 * - **Modular design**: Each settings category is a separate SettingsWidget
 * - **Settings persistence**: Integration with QSettings for configuration storage
 * - **Apply/Cancel**: Standard dialog behavior with settings validation
 * - **Change notification**: Signals when settings are modified
 *
 * The dialog supports multiple settings categories such as:
 * - MIDI settings (input/output ports)
 * - Appearance settings (colors, themes)
 * - Performance settings (rendering options)
 * - Layout settings (UI arrangement)
 *
 * Each category is implemented as a separate SettingsWidget that can be
 * added to the dialog dynamically.
 */
class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    /**
     * \brief Creates a new SettingsDialog.
     * \param title The dialog window title
     * \param settings The QSettings instance for configuration storage
     * \param parent The parent widget (expected to be MainWindow)
     */
    SettingsDialog(QString title, QSettings *settings, QWidget *parent);

    /**
     * \brief Adds a settings widget to the dialog.
     * \param settingsWidget The SettingsWidget to add as a new category
     */
    void addSetting(SettingsWidget *settingsWidget);

    /**
     * \brief Returns the main window associated with this dialog.
     */
    MainWindow *mainWindow() const { return _mainWindow; }

    /**
     * \brief Returns the shared QSettings instance.
     */
    QSettings *settings() const { return _settings; }

public slots:
    /**
     * \brief Handles navigation list selection changes.
     * \param row The selected row in the navigation list
     */
    void rowChanged(int row);

    /**
     * \brief Applies and saves all settings changes.
     */
    void submit();

    /**
     * \brief Refreshes toolbar icons when theme changes.
     */
    void refreshToolbarIcons();

signals:
    /**
     * \brief Emitted when settings have been changed and applied.
     */
    void settingsChanged();

protected:
    /** \brief Navigation list widget for settings categories */
    QListWidget *_listWidget;

    /** \brief List of all settings widgets */
    QList<SettingsWidget *> *_settingsWidgets;

    /** \brief Stacked widget container for settings panels */
    QStackedWidget *_container;

    /** \brief Pointer back to the main window */
    MainWindow *_mainWindow;

    /** \brief Shared settings */
    QSettings *_settings;
};

#endif // SETTINGSDIALOG_H_
