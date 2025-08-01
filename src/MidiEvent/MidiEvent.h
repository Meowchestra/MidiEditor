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

#ifndef MIDIEVENT_H_
#define MIDIEVENT_H_

// Project includes
#include "../gui/EventWidget.h"
#include "../gui/GraphicObject.h"
#include "../protocol/ProtocolEntry.h"

// Forward declarations
class MidiFile;
class QSpinBox;
class QLabel;
class QWidget;
class EventWidget;
class MidiTrack;

/**
 * \class MidiEvent
 *
 * \brief Base class for all MIDI events in the editor.
 *
 * MidiEvent is the fundamental base class for all types of MIDI events that can
 * be stored and manipulated in the MIDI editor. It provides common functionality
 * for all events including:
 *
 * - Channel and track association
 * - Timing information (MIDI time)
 * - Serialization and deserialization
 * - Protocol system integration for undo/redo
 * - Graphical representation capabilities
 * - Event widget integration for editing
 *
 * All specific MIDI event types (NoteOnEvent, ControlChangeEvent, etc.) inherit
 * from this base class and implement their specific behavior.
 */
class MidiEvent : public ProtocolEntry, public GraphicObject {
public:
    /**
     * \brief Creates a new MidiEvent.
     * \param channel The MIDI channel (0-15)
     * \param track The MIDI track this event belongs to
     */
    MidiEvent(int channel, MidiTrack *track);

    /**
     * \brief Creates a new MidiEvent copying another instance.
     * \param other The MidiEvent instance to copy
     */
    MidiEvent(MidiEvent &other);

    /**
     * \brief Loads a MIDI event from a data stream.
     * \param content The data stream to read from
     * \param ok Pointer to bool indicating success/failure
     * \param endEvent Pointer to bool indicating if this is an end event
     * \param track The MIDI track this event belongs to
     * \param startByte Optional start byte for parsing
     * \param secondByte Optional second byte for parsing
     * \return Pointer to the loaded MidiEvent, or nullptr on failure
     */
    static MidiEvent *loadMidiEvent(QDataStream *content,
                                    bool *ok, bool *endEvent, MidiTrack *track, quint8 startByte = 0,
                                    quint8 secondByte = 0);

    /**
     * \brief Gets the global event widget instance.
     * \return Pointer to the EventWidget used for editing events
     */
    static EventWidget *eventWidget();

    /**
     * \brief Sets the global event widget instance.
     * \param widget The EventWidget to use for editing events
     */
    static void setEventWidget(EventWidget *widget);

    /**
     * \brief Display line constants for different event types.
     */
    enum {
        TEMPO_CHANGE_EVENT_LINE = 128,  ///< Line for tempo change events
        TIME_SIGNATURE_EVENT_LINE,      ///< Line for time signature events
        KEY_SIGNATURE_EVENT_LINE,       ///< Line for key signature events
        PROG_CHANGE_LINE,               ///< Line for program change events
        CONTROLLER_LINE,                ///< Line for controller events
        KEY_PRESSURE_LINE,              ///< Line for key pressure events
        CHANNEL_PRESSURE_LINE,          ///< Line for channel pressure events
        TEXT_EVENT_LINE,                ///< Line for text events
        PITCH_BEND_LINE,                ///< Line for pitch bend events
        SYSEX_LINE,                     ///< Line for system exclusive events
        UNKNOWN_LINE                    ///< Line for unknown events
    };

    void setTrack(MidiTrack *track, bool toProtocol = true);

    MidiTrack *track();

    void setChannel(int channel, bool toProtocol = true);

    int channel();

    virtual void setMidiTime(int t, bool toProtocol = true);

    int midiTime();

    void setFile(MidiFile *f);

    MidiFile *file();

    bool shownInEventWidget();

    virtual int line();

    virtual QString toMessage();

    virtual QByteArray save();

    virtual void draw(QPainter *p, QColor c);

    virtual ProtocolEntry *copy();

    virtual void reloadState(ProtocolEntry *entry);

    virtual QString typeString();

    virtual bool isOnEvent();

    static QMap<int, QString> knownMetaTypes();

    void setTemporaryRecordID(int id);

    int temporaryRecordID();

    virtual void moveToChannel(int channel);

protected:
    int numChannel, timePos;
    MidiFile *midiFile;
    static quint8 _startByte;
    static EventWidget *_eventWidget;
    MidiTrack *_track;
    int _tempID;
};

#endif // MIDIEVENT_H_
