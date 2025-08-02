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

#ifndef EVENTLISTWIDGET_H_
#define EVENTLISTWIDGET_H_

/**
 * \class EventListWidget
 *
 * \brief Widget for displaying MIDI events in a list format.
 *
 * EventListWidget provides a list-based view of MIDI events, offering
 * an alternative to the matrix view for event management. It features:
 *
 * - **List display**: Shows events in chronological order
 * - **Event details**: Displays event type, time, and parameters
 * - **Selection support**: Allows selecting and editing individual events
 * - **Filtering**: Can filter events by type, channel, or other criteria
 * - **Sorting**: Supports sorting by various event properties
 *
 * This widget is useful for detailed event inspection and editing,
 * particularly for non-note events that may not be clearly visible
 * in the matrix view.
 *
 * \note This class is currently a stub and may be implemented in future versions.
 */
class EventListWidget {
public:
    /**
     * \brief Creates a new EventListWidget.
     */
    EventListWidget();

    /**
     * \brief Destroys the EventListWidget.
     */
    virtual ~EventListWidget();
};

#endif // EVENTLISTWIDGET_H_
