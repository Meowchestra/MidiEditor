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

#ifndef CHANNELLISTWIDGET_H_
#define CHANNELLISTWIDGET_H_

// Qt includes
#include <QListWidget>
#include <QResizeEvent>
#include <QWidget>

// Forward declarations
class QAction;
class MidiFile;
class QLabel;
class ChannelListWidget;
class ColoredWidget;

/**
 * \class ChannelListItem
 *
 * \brief Individual channel item widget for the channel list.
 *
 * ChannelListItem represents a single MIDI channel in the channel list,
 * providing controls for channel visibility, audibility, solo state,
 * and instrument selection.
 */
class ChannelListItem : public QWidget {
    Q_OBJECT

public:
    /**
     * \brief Creates a new ChannelListItem.
     * \param channel The MIDI channel number (0-15)
     * \param parent The parent ChannelListWidget
     */
    ChannelListItem(int channel, ChannelListWidget *parent);

    /**
     * \brief Called before the item is updated.
     */
    void onBeforeUpdate();

signals:
    /**
     * \brief Emitted when instrument selection is requested.
     * \param channel The channel number for instrument selection
     */
    void selectInstrumentClicked(int channel);

    /**
     * \brief Emitted when the channel state changes.
     */
    void channelStateChanged();

public slots:
    /**
     * \brief Toggles the visibility of the channel.
     * \param visible True to show the channel, false to hide it
     */
    void toggleVisibility(bool visible);

    /**
     * \brief Toggles the audibility (mute state) of the channel.
     * \param audible True to unmute the channel, false to mute it
     */
    void toggleAudibility(bool audible);

    /**
     * \brief Toggles the solo state of the channel.
     * \param solo True to enable solo mode, false to disable it
     */
    void toggleSolo(bool solo);

    /**
     * \brief Opens the instrument selection dialog.
     */
    void instrument();

private:
    /** \brief Label displaying the instrument name */
    QLabel *instrumentLabel;

    /** \brief Reference to the parent channel list widget */
    ChannelListWidget *channelList;

    /** \brief The MIDI channel number */
    int channel;

    /** \brief Color widget showing the channel color */
    ColoredWidget *colored;

    /** \brief Actions for channel control */
    QAction *visibleAction, *loudAction, *soloAction;
};

/**
 * \class ChannelListWidget
 *
 * \brief Widget displaying a list of MIDI channels with individual controls.
 *
 * ChannelListWidget provides a comprehensive interface for managing MIDI channels
 * in the editor. Each channel is represented by a ChannelListItem that shows:
 *
 * - **Channel identification**: Channel number and color coding
 * - **Instrument display**: Current instrument name for each channel
 * - **Visibility control**: Show/hide channels in the matrix view
 * - **Audio control**: Mute/unmute individual channels
 * - **Solo functionality**: Play only selected channels
 * - **Instrument selection**: Choose instruments for each channel
 *
 * The widget automatically updates when the MIDI file changes and provides
 * intuitive controls for channel management during editing and playback.
 */
class ChannelListWidget : public QListWidget {
    Q_OBJECT

public:
    /**
     * \brief Creates a new ChannelListWidget.
     * \param parent The parent widget
     */
    ChannelListWidget(QWidget *parent = 0);

    /**
     * \brief Sets the MIDI file to display channels for.
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
     * \brief Emitted when any channel state changes.
     */
    void channelStateChanged();

    /**
     * \brief Emitted when instrument selection is requested.
     * \param channel The channel number for instrument selection
     */
    void selectInstrumentClicked(int channel);

public slots:
    /**
     * \brief Updates the channel list display.
     */
    void update();

private:
    /** \brief The associated MIDI file */
    MidiFile *file;

    /** \brief List of channel item widgets */
    QList<ChannelListItem *> items;
};

#endif // CHANNELLISTWIDGET_H_
