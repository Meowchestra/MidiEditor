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

#ifndef MIDIOUTPUT_H_
#define MIDIOUTPUT_H_

// Qt includes
#include <QMap>
#include <QObject>

// Forward declarations
class MidiEvent;
class RtMidiIn;
class RtMidiOut;
class SenderThread;

/**
 * \class MidiOutput
 *
 * \brief Static interface for MIDI output functionality.
 *
 * MidiOutput provides centralized management of MIDI output operations,
 * handling all MIDI message transmission to external devices or software.
 * It manages:
 *
 * - **MIDI output ports**: Detection and connection to MIDI output devices
 * - **Message transmission**: Send MIDI events and raw data
 * - **Program changes**: Instrument selection on MIDI channels
 * - **Note tracking**: Monitor currently playing notes
 * - **Queued commands**: Buffered message transmission
 * - **Channel management**: Default channel assignment
 *
 * Key features:
 * - Static interface for global access
 * - Real-time MIDI message transmission
 * - Program change management
 * - Note-on tracking for proper note-off handling
 * - Multi-port support with dynamic switching
 * - Integration with playback and input systems
 *
 * The class uses RtMidi for cross-platform MIDI output handling
 * and provides a simplified interface for the rest of the application.
 */
class MidiOutput : public QObject {
public:
    // === Initialization ===

    /**
     * \brief Initializes the MIDI output system.
     */
    static void init();

    // === Message Transmission ===

    /**
     * \brief Sends a raw MIDI command immediately.
     * \param array Raw MIDI data bytes to send
     */
    static void sendCommand(QByteArray array);

    /**
     * \brief Sends a MIDI event immediately.
     * \param e The MidiEvent to send
     */
    static void sendCommand(MidiEvent *e);

    /**
     * \brief Sends a raw MIDI command through the queue.
     * \param array Raw MIDI data bytes to enqueue
     */
    static void sendEnqueuedCommand(QByteArray array);

    // === Port Management ===

    /**
     * \brief Gets a list of available MIDI output ports.
     * \return QStringList containing names of available output ports
     */
    static QStringList outputPorts();

    /**
     * \brief Sets the active MIDI output port.
     * \param name Name of the output port to connect to
     * \return True if connection was successful
     */
    static bool setOutputPort(QString name);

    /**
     * \brief Gets the name of the current output port.
     * \return Name of the currently connected output port
     */
    static QString outputPort();

    /**
     * \brief Checks if MIDI output is connected.
     * \return True if connected to an output port
     */
    static bool isConnected();

    // === Channel and Program Management ===

    /**
     * \brief Sets the standard/default MIDI channel.
     * \param channel The MIDI channel (0-15) to use as default
     */
    static void setStandardChannel(int channel);

    /**
     * \brief Gets the standard/default MIDI channel.
     * \return The default MIDI channel (0-15)
     */
    static int standardChannel();

    /**
     * \brief Sends a program change message.
     * \param channel The MIDI channel (0-15)
     * \param prog The program number (0-127)
     */
    static void sendProgram(int channel, int prog);

    // === Public State Variables ===

    /** \brief Flag indicating if using alternative player */
    static bool isAlternativePlayer;

    /** \brief Map tracking currently playing notes by channel */
    static QMap<int, QList<int> > playedNotes;

private:
    /** \brief Name of the current output port */
    static QString _outPort;

    /** \brief RtMidi output interface */
    static RtMidiOut *_midiOut;

    /** \brief Thread for queued message transmission */
    static SenderThread *_sender;

    /** \brief Standard/default MIDI channel */
    static int _stdChannel;
};

#endif // MIDIOUTPUT_H_
