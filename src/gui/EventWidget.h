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

#ifndef EVENTWIDGET_H_
#define EVENTWIDGET_H_

// Qt includes
#include <QStyledItemDelegate>
#include <QTableWidget>

// Forward declarations
class MidiEvent;
class EventWidget;
class MidiFile;

/**
 * \class EventWidgetDelegate
 *
 * \brief Custom delegate for editing MIDI events in the EventWidget table.
 *
 * EventWidgetDelegate provides custom editing capabilities for different types
 * of MIDI event properties in the table view. It creates appropriate editors
 * for different data types and handles the data transfer between the model
 * and the editing widgets.
 */
class EventWidgetDelegate : public QStyledItemDelegate {
    Q_OBJECT

public:
    /**
     * \brief Creates a new EventWidgetDelegate.
     * \param w The EventWidget this delegate belongs to
     * \param parent The parent widget
     */
    EventWidgetDelegate(EventWidget *w, QWidget *parent = 0)
        : QStyledItemDelegate(parent) {
        eventWidget = w;
    }

    /**
     * \brief Returns the size hint for the given item.
     * \param option Style options for the item
     * \param index Model index of the item
     * \return Recommended size for the item
     */
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const;

    /**
     * \brief Creates an editor widget for the given item.
     * \param parent Parent widget for the editor
     * \param option Style options for the item
     * \param index Model index of the item
     * \return Pointer to the created editor widget
     */
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const;

    /**
     * \brief Sets the editor's data from the model.
     * \param editor The editor widget
     * \param index Model index of the item being edited
     */
    void setEditorData(QWidget *editor, const QModelIndex &index) const;

    /**
     * \brief Updates the model with data from the editor.
     * \param editor The editor widget
     * \param model The data model
     * \param index Model index of the item being edited
     */
    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const;

private:
    /** \brief Reference to the parent EventWidget */
    EventWidget *eventWidget;
};

/**
 * \class EventWidget
 *
 * \brief Table widget for displaying and editing MIDI events in detail.
 *
 * EventWidget provides a tabular view of MIDI events with detailed information
 * about each event's properties. It allows users to:
 *
 * - **View event details**: See all properties of MIDI events in a table
 * - **Edit event properties**: Modify event values directly in the table
 * - **Filter by type**: Show only specific types of MIDI events
 * - **Sort events**: Order events by different criteria
 * - **Add/remove events**: Create new events or delete existing ones
 *
 * The widget uses a custom delegate (EventWidgetDelegate) to provide
 * appropriate editors for different types of event properties.
 */
class EventWidget : public QTableWidget {
    Q_OBJECT

public:
    /**
     * \brief Creates a new EventWidget.
     * \param parent The parent widget
     */
    EventWidget(QWidget *parent = 0);

    /**
     * \brief Sets the list of events to display.
     * \param events List of MIDI events to show in the table
     */
    void setEvents(QList<MidiEvent *> events);

    /**
     * \brief Gets the current list of events.
     * \return List of MIDI events currently displayed
     */
    QList<MidiEvent *> events();

    /**
     * \brief Removes an event from the widget.
     * \param event The MIDI event to remove
     */
    void removeEvent(MidiEvent *event);

    /**
     * \brief Sets the MIDI file context.
     * \param file The MIDI file containing the events
     */
    void setFile(MidiFile *file);

    /**
     * \brief Gets the current MIDI file context.
     * \return The MIDI file containing the events
     */
    MidiFile *file();

    /**
     * \brief Event type enumeration for filtering and categorization.
     */
    enum EventType {
        MidiEventType,              ///< Generic MIDI event
        ChannelPressureEventType,   ///< Channel pressure/aftertouch
        ControlChangeEventType,     ///< Control change message
        KeyPressureEventType,       ///< Key pressure/polyphonic aftertouch
        KeySignatureEventType,      ///< Key signature meta-event
        NoteEventType,              ///< Note on/off events
        PitchBendEventType,         ///< Pitch bend message
        ProgramChangeEventType,     ///< Program change message
        SystemExclusiveEventType,   ///< System exclusive message
        TempoChangeEventType,       ///< Tempo change meta-event
        TextEventType,              ///< Text meta-event
        TimeSignatureEventType,     ///< Time signature meta-event
        UnknownEventType            ///< Unknown or unsupported event type
    };

    /**
     * \brief Editor field enumeration for table columns.
     */
    enum EditorField {
        MidiEventTick,          ///< Event timing tick
        MidiEventTrack,         ///< Track number
        MidiEventChannel,       ///< MIDI channel
        MidiEventNote,          ///< Note number
        NoteEventOffTick,       ///< Note off timing
        NoteEventVelocity,      ///< Note velocity
        NoteEventDuration,      ///< Note duration
        MidiEventValue,         ///< Generic event value
        ControlChangeControl,   ///< Control change number
        ProgramChangeProgram,   ///< Program number
        KeySignatureKey,        ///< Key signature key
        TimeSignatureDenom,     ///< Time signature denominator
        TimeSignatureNum,       ///< Time signature numerator
        TextType,               ///< Text event type
        TextText,               ///< Text content
        UnknownType,            ///< Unknown event type
        MidiEventData           ///< Raw event data
    };

    /**
     * \brief Gets the content for a specific editor field.
     * \param field The field to get content for
     * \return QVariant containing the field content
     */
    QVariant fieldContent(EditorField field);

    /**
     * \brief Gets the current event type.
     * \return The EventType of currently selected events
     */
    EventType type() { return _currentType; }

    /**
     * \brief Gets the list of key signature strings.
     * \return QStringList containing key signature names
     */
    QStringList keyStrings();

    /**
     * \brief Gets the index for a key signature.
     * \param tonality The tonality (sharps/flats)
     * \param minor True for minor keys, false for major
     * \return Index in the key strings list
     */
    int keyIndex(int tonality, bool minor);

    /**
     * \brief Gets key signature information from an index.
     * \param index The index in the key strings list
     * \param tonality Pointer to receive tonality value
     * \param minor Pointer to receive minor key flag
     */
    void getKey(int index, int *tonality, bool *minor);

    /**
     * \brief Converts binary data to a readable string.
     * \param data The binary data to convert
     * \return String representation of the data
     */
    static QString dataToString(QByteArray data);

    /**
     * \brief Reports that selection was changed by a tool.
     */
    void reportSelectionChangedByTool();

public slots:
    /**
     * \brief Reloads the event data and updates the display.
     */
    void reload();

signals:
    /**
     * \brief Emitted when the selection changes.
     * \param hasSelection True if events are selected
     */
    void selectionChanged(bool hasSelection);

    /**
     * \brief Emitted when selection is changed by a tool.
     * \param hasSelection True if events are selected
     */
    void selectionChangedByTool(bool hasSelection);

private:
    /** \brief List of currently selected events */
    QList<MidiEvent *> _events;

    /** \brief Current event type being displayed */
    EventType _currentType;

    /**
     * \brief Computes the event type from selected events.
     * \return The computed EventType
     */
    EventType computeType();

    /**
     * \brief Gets the event type as a string.
     * \return String representation of the event type
     */
    QString eventType();

    /**
     * \brief Gets the list of fields for the current event type.
     * \return List of field name and EditorField pairs
     */
    QList<QPair<QString, EditorField> > getFields();

    /** \brief The associated MIDI file */
    MidiFile *_file;
};

#endif // EVENTWIDGET_H_
