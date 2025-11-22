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

#ifndef MIDIINPUT_H_
#define MIDIINPUT_H_

// Qt includes
#include <QMultiMap>
#include <QProcess>

// Standard includes
#include <vector>

// Forward declarations
class MidiEvent;
inline namespace rt { inline namespace midi { class RtMidiIn; class RtMidiOut; } }

class MidiTrack;

/**
 * \class MidiInput
 *
 * \brief Static interface for MIDI input and recording functionality.
 *
 * MidiInput provides centralized management of MIDI input operations,
 * including real-time recording and MIDI thru functionality. It handles:
 *
 * - **MIDI input ports**: Detection and connection to MIDI input devices
 * - **Real-time recording**: Capture MIDI events during playback
 * - **MIDI thru**: Pass-through of input to output for monitoring
 * - **Message processing**: Convert raw MIDI data to MidiEvent objects
 * - **Timing synchronization**: Align recorded events with playback time
 * - **Port management**: Dynamic switching between input devices
 *
 * Key features:
 * - Static interface for global access
 * - Real-time MIDI message capture
 * - Automatic event creation and timing
 * - MIDI thru for live monitoring
 * - Multi-port support with dynamic switching
 * - Integration with the recording system
 *
 * The class uses RtMidi for cross-platform MIDI input handling
 * and provides a simplified interface for the rest of the application.
 */
class MidiInput : public QObject {
public:
    // === Initialization and Setup ===

    /**
     * \brief Initializes the MIDI input system.
     */
    static void init();

    // === MIDI Output (for thru functionality) ===

    /**
     * \brief Sends a raw MIDI command.
     * \param array Raw MIDI data bytes to send
     */
    static void sendCommand(QByteArray array);

    /**
     * \brief Sends a MIDI event as output.
     * \param e The MidiEvent to send
     */
    static void sendCommand(MidiEvent *e);

    // === Port Management ===

    /**
     * \brief Gets a list of available MIDI input ports.
     * \return QStringList containing names of available input ports
     */
    static QStringList inputPorts();

    /**
     * \brief Sets the active MIDI input port.
     * \param name Name of the input port to connect to
     * \return True if connection was successful
     */
    static bool setInputPort(QString name);

    /**
     * \brief Gets the name of the current input port.
     * \return Name of the currently connected input port
     */
    static QString inputPort();

    /**
     * \brief Checks if MIDI input is connected.
     * \return True if connected to an input port
     */
    static bool isConnected();

    // === Recording Control ===

    /**
     * \brief Starts MIDI input recording.
     */
    static void startInput();

    /**
     * \brief Ends MIDI input recording and returns captured events.
     * \param track The MidiTrack to associate with recorded events
     * \return QMultiMap of recorded events organized by time
     */
    static QMultiMap<int, MidiEvent *> endInput(MidiTrack *track);

    /**
     * \brief Callback function for receiving MIDI messages.
     * \param deltatime Time since last message
     * \param message Raw MIDI message bytes
     * \param userData Optional user data pointer
     */
    static void receiveMessage(double deltatime, std::vector<unsigned char> *message, void *userData = 0);

    /**
     * \brief Sets the current recording time reference.
     * \param ms Time in milliseconds
     */
    static void setTime(int ms);

    /**
     * \brief Checks if currently recording MIDI input.
     * \return True if recording is active
     */
    static bool recording();

    // === MIDI Thru Control ===

    /**
     * \brief Enables or disables MIDI thru functionality.
     * \param b True to enable thru, false to disable
     */
    static void setThruEnabled(bool b);

    /**
     * \brief Checks if MIDI thru is enabled.
     * \return True if thru is enabled
     */
    static bool thru();

private:
    /** \brief Name of the current input port */
    static QString _inPort;

    /** \brief RtMidi input interface */
    static rt::midi::RtMidiIn *_midiIn;

    /** \brief Map of recorded MIDI messages by time */
    static QMultiMap<int, std::vector<unsigned char> > *_messages;

    /** \brief Current recording time reference */
    static int _currentTime;

    /** \brief Recording state flag */
    static bool _recording;

    /** \brief MIDI thru enabled flag */
    static bool _thru;

    /**
     * \brief Removes duplicate values from a list.
     * \param in Input list with potential duplicates
     * \return List with unique values only
     */
    static QList<int> toUnique(QList<int> in);
};

#endif // MIDIINPUT_H_
