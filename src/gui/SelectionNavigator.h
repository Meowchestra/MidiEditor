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

#ifndef SELECTIONNAVIGATOR_H_
#define SELECTIONNAVIGATOR_H_

// Forward declarations
class MainWindow;

/**
 * \class SelectionNavigator
 *
 * \brief Utility class for navigating between selected MIDI events.
 *
 * SelectionNavigator provides intelligent navigation functionality that allows
 * users to move between MIDI events using directional commands. It features:
 *
 * - **Directional navigation**: Move up, down, left, right between events
 * - **Smart selection**: Finds the nearest appropriate event in each direction
 * - **Type awareness**: Considers event types when navigating
 * - **Viewport integration**: Works with the visible time range
 * - **Distance weighting**: Uses geometric distance calculations for optimal selection
 *
 * The navigator uses angular search algorithms to find the most appropriate
 * next event based on the current selection and the requested direction.
 * This provides intuitive keyboard-based navigation through complex MIDI data.
 */
class SelectionNavigator {
public:
    /**
     * \brief Creates a new SelectionNavigator.
     * \param mainWindow The MainWindow instance to navigate within
     */
    SelectionNavigator(MainWindow *mainWindow);

    /**
     * \brief Navigates to the event above the current selection.
     */
    void up();

    /**
     * \brief Navigates to the event below the current selection.
     */
    void down();

    /**
     * \brief Navigates to the event to the left of the current selection.
     */
    void left();

    /**
     * \brief Navigates to the event to the right of the current selection.
     */
    void right();

protected:
    /** \brief Reference to the main window */
    MainWindow *mainWindow;

    /**
     * \brief Performs navigation in a specific angular direction.
     * \param searchAngle The angle in radians to search in
     */
    void navigate(qreal searchAngle);

    /**
     * \brief Gets the first selected event as navigation origin.
     * \return Pointer to the first selected MidiEvent, or nullptr if none
     */
    MidiEvent *getFirstSelectedEvent();

    /**
     * \brief Checks if an event is within the visible time range.
     * \param event The MidiEvent to check
     * \return True if the event is visible
     */
    bool eventIsInVisibleTimeRange(MidiEvent *event);

    /**
     * \brief Checks if two events are of the same type.
     * \param event1 The first MidiEvent
     * \param event2 The second MidiEvent
     * \return True if both events are the same type
     */
    bool eventsAreSameType(MidiEvent *event1, MidiEvent *event2);

    /**
     * \brief Calculates weighted distance between events for navigation.
     * \param originEvent The starting event
     * \param targetEvent The potential target event
     * \param searchAngle The search direction angle
     * \return Weighted distance value (lower is better)
     */
    qreal getDisplayDistanceWeightedByDirection(MidiEvent *originEvent, MidiEvent *targetEvent, qreal searchAngle);
};

#endif // SELECTIONNAVIGATOR_H_
