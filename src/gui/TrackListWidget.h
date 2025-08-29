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

#ifndef TRACKLISTWIDGET_H_
#define TRACKLISTWIDGET_H_

// Qt includes
#include <QListWidget>
#include <QPaintEvent>
#include <QWidget>

// Forward declarations
class QAction;
class MidiFile;
class QLabel;
class MidiTrack;
class TrackListWidget;
class ColoredWidget;

/**
 * \class TrackListItem
 *
 * \brief Individual track item widget for the track list.
 *
 * TrackListItem represents a single MIDI track in the track list,
 * providing controls for track visibility, audibility, renaming,
 * and removal operations.
 */
class TrackListItem : public QWidget {
    Q_OBJECT

public:
    TrackListItem(MidiTrack *track, TrackListWidget *parent);

    void onBeforeUpdate();

signals:
    void trackRenameClicked(int tracknumber);

    void trackRemoveClicked(int tracknumber);

public slots:
    /**
     * \brief Toggles the visibility of the track.
     * \param visible True to show the track, false to hide it
     */
    void toggleVisibility(bool visible);

    /**
     * \brief Toggles the audibility (mute state) of the track.
     * \param audible True to unmute the track, false to mute it
     */
    void toggleAudibility(bool audible);

    /**
     * \brief Removes the track from the MIDI file.
     */
    void removeTrack();

    /**
     * \brief Initiates track renaming.
     */
    void renameTrack();

private:
    /** \brief Label displaying the track name */
    QLabel *trackNameLabel;

    /** \brief Reference to the parent track list widget */
    TrackListWidget *trackList;

    /** \brief The MIDI track this item represents */
    MidiTrack *track;

    /** \brief Color widget showing the track color */
    ColoredWidget *colored;

    /** \brief Actions for track control */
    QAction *visibleAction, *loudAction;
};

class TrackListWidget : public QListWidget {
    Q_OBJECT

public:
    /**
     * \brief Creates a new TrackListWidget.
     * \param parent The parent widget
     */
    TrackListWidget(QWidget *parent = 0);

    /**
     * \brief Sets the MIDI file to display tracks for.
     * \param f The MidiFile to associate with this widget
     */
    void setFile(MidiFile *f);

    /**
     * \brief Gets the current MIDI file.
     * \return Pointer to the current MidiFile
     */
    MidiFile *midiFile();

signals:
    /**
     * \brief Emitted when track rename is requested.
     * \param tracknumber The number of the track to rename
     */
    void trackRenameClicked(int tracknumber);

    /**
     * \brief Emitted when track removal is requested.
     * \param tracknumber The number of the track to remove
     */
    void trackRemoveClicked(int tracknumber);

    /**
     * \brief Emitted when a track is clicked.
     * \param track The MidiTrack that was clicked
     */
    void trackClicked(MidiTrack *track);
    
    /**
     * \brief Emitted when tracks are reordered.
     */
    void trackOrderChanged();

public slots:
    /**
     * \brief Updates the track list display.
     */
    void update();

    /**
     * \brief Handles track selection from list items.
     * \param item The selected list widget item
     */
    void chooseTrack(QListWidgetItem *item);

    /**
     * \brief Reorders tracks after drag-and-drop operation.
     * \param fromIndex Original position of the track
     * \param toIndex New position of the track
     */
    void reorderTracks(int fromIndex, int toIndex);

protected:
    /**
     * \brief Handles drop events for track reordering.
     * \param event The drop event
     */
    void dropEvent(QDropEvent *event) override;

    /**
     * \brief Handles MIME data drops for internal moves.
     * \param index Target index for the drop
     * \param data MIME data being dropped
     * \param action Drop action type
     * \return True if drop was handled successfully
     */
    bool dropMimeData(int index, const QMimeData *data, Qt::DropAction action) override;

private:
    /** \brief The associated MIDI file */
    MidiFile *file;

    /** \brief Map of tracks to their list items */
    QMap<MidiTrack *, TrackListItem *> items;

    /** \brief Ordered list of tracks */
    QList<MidiTrack *> trackorder;
};

#endif // TRACKLISTWIDGET_H_
