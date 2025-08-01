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

#ifndef TWEAKTARGET_H_
#define TWEAKTARGET_H_

// Forward declarations
class MainWindow;

/**
 * \class TweakTarget
 *
 * \brief Abstract base class for tweaking different properties of MIDI events.
 *
 * TweakTarget defines the interface for adjusting various properties of
 * selected MIDI events by small, medium, or large amounts. It provides:
 *
 * - **Incremental adjustments**: Small, medium, and large step sizes
 * - **Bidirectional changes**: Both increase and decrease operations
 * - **Property abstraction**: Common interface for different event properties
 * - **Keyboard shortcuts**: Integration with keyboard-based tweaking
 *
 * Concrete implementations handle specific properties like timing, pitch,
 * velocity, and other MIDI event parameters.
 */
class TweakTarget {
public:
    /**
     * \brief Virtual destructor for proper cleanup.
     */
    virtual ~TweakTarget() {};

    /**
     * \brief Decreases the target property by a small amount.
     */
    virtual void smallDecrease() = 0;

    /**
     * \brief Increases the target property by a small amount.
     */
    virtual void smallIncrease() = 0;

    /**
     * \brief Decreases the target property by a medium amount.
     */
    virtual void mediumDecrease() = 0;

    /**
     * \brief Increases the target property by a medium amount.
     */
    virtual void mediumIncrease() = 0;

    /**
     * \brief Decreases the target property by a large amount.
     */
    virtual void largeDecrease() = 0;

    /**
     * \brief Increases the target property by a large amount.
     */
    virtual void largeIncrease() = 0;
};

/**
 * \class TimeTweakTarget
 *
 * \brief Tweak target for adjusting event timing (start time).
 *
 * TimeTweakTarget allows fine-tuning of MIDI event start times by
 * moving events earlier or later in time by specified amounts.
 */
class TimeTweakTarget : public TweakTarget {
public:
    /**
     * \brief Creates a new TimeTweakTarget.
     * \param mainWindow The MainWindow instance to operate on
     */
    TimeTweakTarget(MainWindow *mainWindow);

    void smallDecrease();
    void smallIncrease();
    void mediumDecrease();
    void mediumIncrease();
    void largeDecrease();
    void largeIncrease();

protected:
    /** \brief Reference to the main window */
    MainWindow *mainWindow;

    /**
     * \brief Applies a time offset to selected events.
     * \param amount The time offset in MIDI ticks
     */
    void offset(int amount);
};

/**
 * \class StartTimeTweakTarget
 *
 * \brief Tweak target for adjusting note start times specifically.
 *
 * StartTimeTweakTarget provides precise control over when notes begin,
 * allowing for timing adjustments without affecting note duration.
 */
class StartTimeTweakTarget : public TweakTarget {
public:
    /**
     * \brief Creates a new StartTimeTweakTarget.
     * \param mainWindow The MainWindow instance to operate on
     */
    StartTimeTweakTarget(MainWindow *mainWindow);

    void smallDecrease();
    void smallIncrease();
    void mediumDecrease();
    void mediumIncrease();
    void largeDecrease();
    void largeIncrease();

protected:
    /** \brief Reference to the main window */
    MainWindow *mainWindow;

    /**
     * \brief Applies a start time offset to selected events.
     * \param amount The time offset in MIDI ticks
     */
    void offset(int amount);
};

/**
 * \class EndTimeTweakTarget
 *
 * \brief Tweak target for adjusting note end times (duration).
 *
 * EndTimeTweakTarget allows modification of note durations by adjusting
 * when notes end, effectively lengthening or shortening them.
 */
class EndTimeTweakTarget : public TweakTarget {
public:
    /**
     * \brief Creates a new EndTimeTweakTarget.
     * \param mainWindow The MainWindow instance to operate on
     */
    EndTimeTweakTarget(MainWindow *mainWindow);

    void smallDecrease();
    void smallIncrease();
    void mediumDecrease();
    void mediumIncrease();
    void largeDecrease();
    void largeIncrease();

protected:
    /** \brief Reference to the main window */
    MainWindow *mainWindow;

    /**
     * \brief Applies an end time offset to selected events.
     * \param amount The time offset in MIDI ticks
     */
    void offset(int amount);
};

/**
 * \class NoteTweakTarget
 *
 * \brief Tweak target for adjusting note pitch (MIDI note numbers).
 *
 * NoteTweakTarget enables transposition of selected notes by semitones,
 * allowing for quick pitch adjustments without opening the transpose dialog.
 */
class NoteTweakTarget : public TweakTarget {
public:
    /**
     * \brief Creates a new NoteTweakTarget.
     * \param mainWindow The MainWindow instance to operate on
     */
    NoteTweakTarget(MainWindow *mainWindow);

    void smallDecrease();
    void smallIncrease();
    void mediumDecrease();
    void mediumIncrease();
    void largeDecrease();
    void largeIncrease();

protected:
    /** \brief Reference to the main window */
    MainWindow *mainWindow;

    /**
     * \brief Applies a pitch offset to selected notes.
     * \param amount The pitch offset in semitones
     */
    void offset(int amount);
};

/**
 * \class ValueTweakTarget
 *
 * \brief Tweak target for adjusting various MIDI event values.
 *
 * ValueTweakTarget handles different types of MIDI event values including
 * velocity, controller values, pitch bend amounts, and tempo changes.
 * The specific behavior depends on the type of selected events.
 */
class ValueTweakTarget : public TweakTarget {
public:
    /**
     * \brief Creates a new ValueTweakTarget.
     * \param mainWindow The MainWindow instance to operate on
     */
    ValueTweakTarget(MainWindow *mainWindow);

    void smallDecrease();
    void smallIncrease();
    void mediumDecrease();
    void mediumIncrease();
    void largeDecrease();
    void largeIncrease();

protected:
    /** \brief Reference to the main window */
    MainWindow *mainWindow;

    /**
     * \brief Applies value offsets to selected events.
     * \param amount General value offset (velocity, controller, etc.)
     * \param pitchBendAmount Specific offset for pitch bend events
     * \param tempoAmount Specific offset for tempo events
     */
    void offset(int amount, int pitchBendAmount, int tempoAmount);
};

#endif // TWEAKTARGET_H_
