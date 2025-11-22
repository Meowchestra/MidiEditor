/*
 * MidiEditor
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef STRUMMERTOOL_H_
#define STRUMMERTOOL_H_

#include "EventTool.h"

// Forward declarations
class NoteOnEvent;

/**
 * \class StrummerTool
 *
 * \brief Tool for strumming selected notes (staggering start/end times).
 */
class StrummerTool : public EventTool {
public:
    /**
     * \brief Creates a new StrummerTool.
     */
    StrummerTool();

    /**
     * \brief Creates a new StrummerTool copying another instance.
     * \param other The StrummerTool instance to copy
     */
    StrummerTool(StrummerTool &other);

    /**
     * \brief Handles mouse release events.
     */
    bool release();

    /**
     * \brief Creates a copy of this tool for the protocol system.
     */
    ProtocolEntry *copy();

    /**
     * \brief Reloads the tool's state from a protocol entry.
     */
    void reloadState(ProtocolEntry *entry);

    /**
     * \brief Returns whether this tool shows selection.
     */
    bool showsSelection();

    /**
     * \brief Performs the strum operation on selected events.
     * \param startStrengthMs Amount to stagger start times (milliseconds). Positive = Up (low->high), Negative = Down.
     * \param startTension Curve of the start stagger (-1.0 to 1.0, 0 is linear).
     * \param endStrengthMs Amount to stagger end times (milliseconds).
     * \param endTension Curve of the end stagger.
     * \param velocityStrength Amount to change velocity.
     * \param velocityTension Curve of the velocity change.
     * \\param preserveEnd If true, note ends are not moved (duration changes). If false, duration is preserved (ends move).
     * \\param alternateDirection If true, direction alternates for each chord.
     * \\param useStepStrength If true, strength is applied per note (step). If false, strength is total range.
     * \\param ignoreTrack If true, notes are grouped across tracks.
     */
    void performStrum(int startStrengthMs, double startTension, int endStrengthMs, double endTension, int velocityStrength, double velocityTension, bool preserveEnd, bool alternateDirection, bool useStepStrength, bool ignoreTrack);

private:
    /**
     * \\brief Applies strumming to a group of notes (chord).
     */
    void strumChord(QList<NoteOnEvent *> &chordNotes, int startStrengthMs, double startTension, int endStrengthMs, double endTension, int velocityStrength, double velocityTension, bool preserveEnd, bool directionUp, bool useStepStrength);

    /**
     * \\brief Calculates the offset based on index, total count, strength and tension.
     */
    int calculateOffset(int index, int count, int strength, double tension, bool useStepStrength);
};

#endif // STRUMMERTOOL_H_
