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

#ifndef MAINWINDOW_H_
#define MAINWINDOW_H_

// Qt includes
#include <QCloseEvent>
#include <QMainWindow>
#include <QScrollBar>
#include <QSettings>

// Project includes
#include "ToolbarActionInfo.h"

// Forward declarations
class MatrixWidget;
class MidiEvent;
class MidiFile;
class ChannelListWidget;
class ProtocolWidget;
class EventWidget;
class ClickButton;
class QTabWidget;
class QMenu;
class TrackListWidget;
class QComboBox;
class MiscWidget;
class QGridLayout;
class MidiTrack;
class QShowEvent;
class Update;
class SelectionNavigator;
class TweakTarget;

/**
 * \class MainWindow
 *
 * \brief The main application window for the MIDI Editor.
 *
 * MainWindow is the central hub of the MIDI Editor application. It provides:
 *
 * - The main user interface with menus, toolbars, and panels
 * - File management (open, save, new, recent files)
 * - Integration of all major components (matrix widget, event widget, etc.)
 * - MIDI playback controls and transport functions
 * - Tool selection and editing operations
 * - Settings and preferences management
 * - Drag-and-drop file support
 *
 * The window is organized into several main areas:
 * - Matrix widget for visual note editing
 * - Event widget for detailed event editing
 * - Channel and track management panels
 * - Protocol widget for undo/redo history
 * - Various control panels and toolbars
 */
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    /**
     * \brief Creates a new MainWindow.
     * \param initFile Optional initial file to load on startup
     */
    MainWindow(QString initFile = "");

    /**
     * \brief Sets the current MIDI file.
     * \param f The MidiFile to set as current
     */
    void setFile(MidiFile *f);

    /**
     * \brief Gets the current MIDI file.
     * \return The currently loaded MidiFile, or nullptr if none
     */
    MidiFile *getFile();

    /**
     * \brief Gets the matrix widget for note editing.
     * \return Pointer to the MatrixWidget
     */
    MatrixWidget *matrixWidget();

    /**
     * \brief Gets the event widget for detailed event editing.
     * \return Pointer to the EventWidget
     */
    EventWidget *eventWidget();

    /**
     * \brief Sets the starting directory for file dialogs.
     * \param dir The directory path to use as default
     */
    void setStartDir(QString dir);

    /**
     * \brief Sets the initial file to load on startup.
     * \param file Path to the file to load initially
     */
    void setInitFile(const char *file);

    /**
     * \brief Prompts user to save before closing if needed.
     * \return True if it's safe to close, false if user cancelled
     */
    bool saveBeforeClose();

protected:
    /**
     * \brief Handles file drop events for drag-and-drop file loading.
     * \param ev The drop event containing file information
     */
    void dropEvent(QDropEvent *ev);

    /**
     * \brief Handles drag enter events to accept file drops.
     * \param ev The drag enter event
     */
    void dragEnterEvent(QDragEnterEvent *ev);

public slots:
    // === General Update Methods ===

    /**
     * \brief Updates all widgets and components.
     */
    void updateAll();

    /**
     * \brief Loads the initial file specified at startup.
     */
    void loadInitFile();

    /**
     * \brief Initializes the shared clipboard system.
     */
    void initializeSharedClipboard();

    /**
     * \brief Updates the state of paste-related actions.
     */
    void updatePasteActionState();

    /**
     * \brief Called when the matrix widget size changes.
     * \param maxScrollTime Maximum scroll time in milliseconds
     * \param maxScrollLine Maximum scroll line
     * \param vX Viewport X position
     * \param vY Viewport Y position
     */
    void matrixSizeChanged(int maxScrollTime, int maxScrollLine, int vX, int vY);

    // === Playback Control Methods ===

    /**
     * \brief Starts MIDI playback.
     */
    void play();

    /**
     * \brief Toggles between play and stop.
     */
    void playStop();

    /**
     * \brief Stops MIDI playback.
     * \param autoConfirmRecord Auto-confirm recording if active
     * \param addEvents Add recorded events to the file
     * \param resetPause Reset pause state
     */
    void stop(bool autoConfirmRecord = false, bool addEvents = true, bool resetPause = true);

    /**
     * \brief Pauses MIDI playback.
     */
    void pause();

    /**
     * \brief Moves playback position forward.
     */
    void forward();

    /**
     * \brief Moves playback position backward.
     */
    void back();

    /**
     * \brief Moves playback position to the beginning.
     */
    void backToBegin();

    /**
     * \brief Moves playback position to the next marker.
     */
    void forwardMarker();

    /**
     * \brief Moves playback position to the previous marker.
     */
    void backMarker();

    // === File Operations ===

    /**
     * \brief Opens a file dialog to load a MIDI file.
     */
    void load();

    /**
     * \brief Loads a specific MIDI file.
     * \param file Path to the MIDI file to load
     */
    void loadFile(QString file);

    /**
     * \brief Opens a MIDI file at the specified path.
     * \param filePath Path to the MIDI file to open
     */
    void openFile(QString filePath);

    /**
     * \brief Saves the current MIDI file.
     */
    void save();

    /**
     * \brief Opens a save-as dialog to save the file with a new name.
     */
    void saveas();

    /**
     * \brief Undoes the last action.
     */
    void undo();

    /**
     * \brief Redoes the last undone action.
     */
    void redo();

    // === Channel Management ===

    /**
     * \brief Mutes all MIDI channels.
     */
    void muteAllChannels();

    /**
     * \brief Unmutes all MIDI channels.
     */
    void unmuteAllChannels();

    /**
     * \brief Makes all channels visible in the editor.
     */
    void allChannelsVisible();

    /**
     * \brief Hides all channels in the editor.
     */
    void allChannelsInvisible();

    // === Track Management ===

    /**
     * \brief Mutes all MIDI tracks.
     */
    void muteAllTracks();

    /**
     * \brief Unmutes all MIDI tracks.
     */
    void unmuteAllTracks();

    /**
     * \brief Makes all tracks visible in the editor.
     */
    void allTracksVisible();

    /**
     * \brief Hides all tracks in the editor.
     */
    void allTracksInvisible();

    // === Dialogs and Settings ===

    /**
     * \brief Shows the about dialog.
     */
    void about();

    /**
     * \brief Opens dialog to set the file length in milliseconds.
     */
    void setFileLengthMs();

    /**
     * \brief Called when scroll positions change in the editor.
     * \param startMs Start time in milliseconds
     * \param maxMs Maximum time in milliseconds
     * \param startLine Start line number
     * \param maxLine Maximum line number
     */
    void scrollPositionsChanged(int startMs, int maxMs, int startLine, int maxLine);

    /**
     * \brief Starts MIDI recording.
     */
    void record();

    /**
     * \brief Creates a new MIDI file.
     */
    void newFile();

    /**
     * \brief Sends panic (all notes off) to all MIDI channels.
     */
    void panic();

    /**
     * \brief Handles screen lock button press.
     * \param enable True to enable screen lock, false to disable
     */
    void screenLockPressed(bool enable);

    /**
     * \brief Opens the scale selection dialog.
     */
    void scaleSelection();

    /**
     * \brief Aligns selected events to the left.
     */
    void alignLeft();

    /**
     * \brief Aligns selected events to the right.
     */
    void alignRight();

    /**
     * \brief Equalizes selected events.
     */
    void equalize();

    /**
     * \brief Glues selected events together.
     */
    void glueSelection();

    /**
     * \brief Glues selected events together across all channels.
     */
    void glueSelectionAllChannels();

    /**
     * \brief Deletes overlapping notes.
     */
    void deleteOverlaps();

    /**
     * \brief Resets the view to default settings.
     */
    void resetView();

    /**
     * \brief Deletes all selected events.
     */
    void deleteSelectedEvents();

    /**
     * \brief Deletes all events from a specific channel.
     * \param action The action containing channel information
     */
    void deleteChannel(QAction *action);

    /**
     * \brief Moves selected events to a specific channel.
     * \param action The action containing channel information
     */
    void moveSelectedEventsToChannel(QAction *action);

    /**
     * \brief Moves selected events to a specific track.
     * \param action The action containing track information
     */
    void moveSelectedEventsToTrack(QAction *action);

    /**
     * \brief Updates the recent files list.
     */
    void updateRecentPathsList();

    /**
     * \brief Opens a recent file.
     * \param action The action containing file path information
     */
    void openRecent(QAction *action);

    /**
     * \brief Updates the channel menu with current channels.
     */
    void updateChannelMenu();

    /**
     * \brief Updates the track menu with current tracks.
     */
    void updateTrackMenu();

    /**
     * \brief Mutes or unmutes a channel.
     * \param action The action containing channel information
     */
    void muteChannel(QAction *action);

    /**
     * \brief Solos or unsolos a channel.
     * \param action The action containing channel information
     */
    void soloChannel(QAction *action);

    /**
     * \brief Shows or hides a channel.
     * \param action The action containing channel information
     */
    void viewChannel(QAction *action);

    /**
     * \brief Opens instrument selection for a channel.
     * \param action The action containing channel information
     */
    void instrumentChannel(QAction *action);

    void renameTrackMenuClicked(QAction *action);

    void removeTrackMenuClicked(QAction *action);

    void showEventWidget(bool show);

    void showTrackMenuClicked(QAction *action);

    void muteTrackMenuClicked(QAction *action);

    void renameTrack(int tracknumber);

    void removeTrack(int tracknumber);

    void setInstrumentForChannel(int i);

    void spreadSelection();

    void copy();

    void paste();

    void addTrack();

    void selectAll();

    void transposeNSemitones();

    /**
     * \brief Marks the file as edited (unsaved changes).
     */
    void markEdited();

    /**
     * \brief Sets coloring mode to color by MIDI channels.
     */
    void colorsByChannel();

    /**
     * \brief Sets coloring mode to color by MIDI tracks.
     */
    void colorsByTrack();

    /**
     * \brief Edits a specific MIDI channel.
     * \param i The channel number to edit
     * \param assign Whether to assign the channel
     */
    void editChannel(int i, bool assign = true);

    /**
     * \brief Edits a specific MIDI track.
     * \param i The track number to edit
     * \param assign Whether to assign the track
     */
    void editTrack(int i, bool assign = true);

    /**
     * \brief Edits both track and channel for a MIDI track.
     * \param track The MidiTrack to edit
     */
    void editTrackAndChannel(MidiTrack *track);

    /**
     * \brief Opens the manual/help documentation.
     */
    void manual();

    /**
     * \brief Changes the miscellaneous editor mode.
     * \param mode The new editor mode
     */
    void changeMiscMode(int mode);

    /**
     * \brief Handles selection mode changes.
     * \param action The action that triggered the change
     */
    void selectModeChanged(QAction *action);

    /**
     * \brief Pastes clipboard content to a specific channel.
     * \param action The action containing channel information
     */
    void pasteToChannel(QAction *action);

    /**
     * \brief Pastes clipboard content to a specific track.
     * \param action The action containing track information
     */
    void pasteToTrack(QAction *action);

    /**
     * \brief Selects all events from a specific channel.
     * \param action The action containing channel information
     */
    void selectAllFromChannel(QAction *action);

    /**
     * \brief Selects all events from a specific track.
     * \param action The action containing track information
     */
    void selectAllFromTrack(QAction *action);

    /**
     * \brief Handles division setting changes.
     * \param action The action containing division information
     */
    void divChanged(QAction *action);

    /**
     * \brief Handles quantization setting changes.
     * \param action The action containing quantization information
     */
    void quantizationChanged(QAction *action);

    /**
     * \brief Enables or disables the magnet (snap to grid) feature.
     * \param enable True to enable magnet, false to disable
     */
    void enableMagnet(bool enable);

    /**
     * \brief Opens the configuration/settings dialog.
     */
    void openConfig();

    /**
     * \brief Enables or disables the metronome.
     * \param enable True to enable metronome, false to disable
     */
    void enableMetronome(bool enable);

    /**
     * \brief Enables or disables MIDI thru functionality.
     * \param enable True to enable thru, false to disable
     */
    void enableThru(bool enable);

    /**
     * \brief Toggles piano emulation mode.
     * \param enable True to enable piano emulation, false to disable
     */
    void togglePianoEmulation(bool enable);

    /**
     * \brief Rebuilds the toolbar from current settings.
     */
    void rebuildToolbarFromSettings();

    /**
     * \brief Quantizes the selected events to the current grid.
     */
    void quantizeSelection();

    /**
     * \brief Opens the N-tole quantization dialog.
     */
    void quantizeNtoleDialog();

    /**
     * \brief Applies N-tole quantization to selected events.
     */
    void quantizeNtole();

    /**
     * \brief Sets the playback speed from an action.
     * \param action The action containing speed information
     */
    void setSpeed(QAction *action);

    /**
     * \brief Checks and enables/disables actions based on current selection.
     */
    void checkEnableActionsForSelection();

    /**
     * \brief Handles tool change events.
     */
    void toolChanged();

    /**
     * \brief Handles changes to copied events.
     */
    void copiedEventsChanged();

    /**
     * \brief Opens the time tweaking dialog.
     */
    void tweakTime();

    /**
     * \brief Opens the start time tweaking dialog.
     */
    void tweakStartTime();

    /**
     * \brief Opens the end time tweaking dialog.
     */
    void tweakEndTime();

    /**
     * \brief Opens the note tweaking dialog.
     */
    void tweakNote();

    /**
     * \brief Opens the value tweaking dialog.
     */
    void tweakValue();

    /**
     * \brief Applies small decrease tweaking to selected events.
     */
    void tweakSmallDecrease();

    /**
     * \brief Applies small increase tweaking to selected events.
     */
    void tweakSmallIncrease();

    /**
     * \brief Applies medium decrease tweaking to selected events.
     */
    void tweakMediumDecrease();

    /**
     * \brief Applies medium increase tweaking to selected events.
     */
    void tweakMediumIncrease();

    /**
     * \brief Applies large decrease tweaking to selected events.
     */
    void tweakLargeDecrease();

    /**
     * \brief Applies large increase tweaking to selected events.
     */
    void tweakLargeIncrease();

    /**
     * \brief Navigates selection up (higher pitch).
     */
    void navigateSelectionUp();

    /**
     * \brief Navigates selection down (lower pitch).
     */
    void navigateSelectionDown();

    /**
     * \brief Navigates selection left (earlier time).
     */
    void navigateSelectionLeft();

    /**
     * \brief Navigates selection right (later time).
     */
    void navigateSelectionRight();

    /**
     * \brief Transposes selected notes up by one octave.
     */
    void transposeSelectedNotesOctaveUp();

    /**
     * \brief Transposes selected notes down by one octave.
     */
    void transposeSelectedNotesOctaveDown();

protected:
    /**
     * \brief Handles close events to save work before closing.
     * \param event The close event
     */
    void closeEvent(QCloseEvent *event);

    /**
     * \brief Handles key press events for shortcuts and piano emulation.
     * \param e The key press event
     */
    void keyPressEvent(QKeyEvent *e);

    /**
     * \brief Handles key release events for piano emulation.
     * \param event The key release event
     */
    void keyReleaseEvent(QKeyEvent *event);

private:
    /**
     * \brief Removes trailing separators from a toolbar.
     * \param toolbar The toolbar to clean up
     */
    void removeTrailingSeparators(QToolBar *toolbar);

    // === Core Widgets ===

    /** \brief Main matrix widget for MIDI editing */
    MatrixWidget *mw_matrixWidget;

    /** \brief Vertical and horizontal scroll bars */
    QScrollBar *vert, *hori;

    /** \brief Widget for managing MIDI channels */
    ChannelListWidget *channelWidget;

    /** \brief Widget for undo/redo protocol management */
    ProtocolWidget *protocolWidget;

    /** \brief Widget for managing MIDI tracks */
    TrackListWidget *_trackWidget;

    /** \brief Current MIDI file */
    MidiFile *file;

    /** \brief Start directory and initialization file */
    QString startDirectory, _initFile;

    /** \brief Widget for editing event properties */
    EventWidget *_eventWidget;

    /** \brief Application settings */
    QSettings *_settings;

    /** \brief List of recent file paths */
    QStringList _recentFilePaths;

    // === Menus ===

    /** \brief Various context and action menus */
    QMenu *_recentPathsMenu, *_deleteChannelMenu, *_moveSelectedEventsToTrackMenu, *_moveSelectedEventsToChannelMenu,
    *_pasteToTrackMenu, *_pasteToChannelMenu, *_selectAllFromTrackMenu, *_selectAllFromChannelMenu, *_pasteOptionsMenu;

    /** \brief Lower tab widget for additional panels */
    QTabWidget *lowerTabWidget;

    /** \brief Actions for color mode selection */
    QAction *_colorsByChannel, *_colorsByTracks;

    /** \brief Combo boxes for track and channel selection */
    QComboBox *_chooseEditTrack, *_chooseEditChannel;

    // === Misc Widget Controls ===

    /** \brief Control widget for misc editor */
    QWidget *_miscWidgetControl;

    /** \brief Layout for misc control widgets */
    QGridLayout *_miscControlLayout;

    /** \brief Combo boxes for misc editor configuration */
    QComboBox *_miscMode, *_miscController, *_miscChannel;

    // === Actions ===

    /** \brief Various UI actions for tools and operations */
    QAction *setSingleMode, *setLineMode, *setFreehandMode, *_allChannelsVisible, *_allChannelsInvisible,
    *_allTracksAudible, *_allTracksMute, *_allChannelsAudible, *_allChannelsMute, *_allTracksVisible,
    *_allTracksInvisible, *stdToolAction, *undoAction, *redoAction, *_pasteAction;

    /** \brief Misc editor widget */
    MiscWidget *_miscWidget;

    // === Setup and Toolbar Methods ===

    /**
     * \brief Sets up actions and returns the action widget.
     * \param parent The parent widget
     * \return Pointer to the created action widget
     */
    QWidget *setupActions(QWidget *parent);

    /**
     * \brief Rebuilds the toolbar from current settings.
     */
    void rebuildToolbar();

    /**
     * \brief Gets an action by its identifier.
     * \param actionId The action identifier string
     * \return Pointer to the QAction, or nullptr if not found
     */
    QAction *getActionById(const QString &actionId);

    /**
     * \brief Creates a custom toolbar widget.
     * \param parent The parent widget
     * \return Pointer to the created toolbar widget
     */
    QWidget *createCustomToolbar(QWidget *parent);

    /**
     * \brief Creates a simple custom toolbar widget.
     * \param parent The parent widget
     * \return Pointer to the created simple toolbar widget
     */
    QWidget *createSimpleCustomToolbar(QWidget *parent);

    /**
     * \brief Updates toolbar contents with current actions.
     * \param toolbarWidget The toolbar widget to update
     * \param layout The layout to update
     */
    void updateToolbarContents(QWidget *toolbarWidget, QGridLayout *layout);

    /**
     * \brief Gets default actions for toolbar placeholder.
     * \return List of ToolbarActionInfo for default actions
     */
    QList<ToolbarActionInfo> getDefaultActionsForPlaceholder();

    /**
     * \brief Refreshes toolbar icons when theme changes.
     */
    void refreshToolbarIcons();

    int _quantizationGrid;

    int quantize(int t, QList<int> ticks);

    QList<QAction *> _activateWithSelections;

    QWidget *_toolbarWidget;

    QMap<QString, QAction *> _actionMap;

    TweakTarget *currentTweakTarget;

    SelectionNavigator *selectionNavigator;
};

#endif // MAINWINDOW_H_
