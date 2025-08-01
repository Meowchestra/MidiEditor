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

#ifndef PLAYERTHREAD_H_
#define PLAYERTHREAD_H_

// Qt includes
#include <QObject>
#include <QThread>
#include <QTimer>
#include <QElapsedTimer>

// Forward declarations
class MidiFile;
class MidiEvent;

/**
 * \class PlayerThread
 *
 * \brief Thread-based MIDI file playback engine.
 *
 * PlayerThread handles the real-time playback of MIDI files in a separate
 * thread to ensure smooth performance without blocking the user interface.
 * It provides:
 *
 * - **Threaded playback**: Non-blocking MIDI file playback
 * - **Timing precision**: Accurate MIDI event timing and scheduling
 * - **Real-time updates**: Position and measure tracking during playback
 * - **Event processing**: Sequential MIDI event transmission
 * - **Playback control**: Start, stop, and position management
 * - **Status signals**: Real-time feedback to the UI
 *
 * Key features:
 * - High-precision timing using QElapsedTimer
 * - Automatic tempo and time signature tracking
 * - Measure and beat position calculation
 * - Thread-safe playback control
 * - Integration with MIDI output system
 *
 * The thread processes MIDI events in chronological order and sends
 * them to the MIDI output system at the correct times.
 */
class PlayerThread : public QThread {
    Q_OBJECT

public:
    /**
     * \brief Creates a new PlayerThread.
     */
    PlayerThread();

    /**
     * \brief Sets the MIDI file to play.
     * \param f The MidiFile to play
     */
    void setFile(MidiFile *f);

    /**
     * \brief Stops playback.
     */
    void stop();

    /**
     * \brief Main thread execution function.
     */
    void run();

    /**
     * \brief Sets the playback timing interval.
     * \param i Interval in milliseconds
     */
    void setInterval(int i);

    /**
     * \brief Gets the current playback time.
     * \return Current time in milliseconds
     */
    int timeMs();

public slots:
    /**
     * \brief Handles timer timeout events for playback processing.
     */
    void timeout();

signals:
    /**
     * \brief Emitted when playback time changes.
     * \param ms Current time in milliseconds
     */
    void timeMsChanged(int ms);

    /**
     * \brief Emitted when playback stops.
     */
    void playerStopped();

    /**
     * \brief Emitted when playback starts.
     */
    void playerStarted();

    /**
     * \brief Emitted when key signature changes.
     * \param tonality New tonality (sharps/flats)
     */
    void tonalityChanged(int tonality);

    /**
     * \brief Emitted when measure changes.
     * \param measure Current measure number
     * \param tickInMeasure Current tick within the measure
     */
    void measureChanged(int measure, int tickInMeasure);

    /**
     * \brief Emitted when time signature changes.
     * \param num Numerator of time signature
     * \param denum Denominator of time signature
     */
    void meterChanged(int num, int denum);

    /**
     * \brief Emitted for measure position updates.
     * \param measure Current measure number
     * \param tickInMeasure Current tick within the measure
     */
    void measureUpdate(int measure, int tickInMeasure);

private:
    /** \brief The MIDI file being played */
    MidiFile *file;

    /** \brief Map of events organized by time */
    QMultiMap<int, MidiEvent *> *events;

    /** \brief Timing and position variables */
    int interval, position, timeoutSinceLastSignal;

    /** \brief Playback control flag */
    volatile bool stopped;

    /** \brief Timer for playback timing */
    QTimer *timer;

    /** \brief Elapsed time tracker */
    QElapsedTimer *time;

    /** \brief Current measure and position tracking */
    int measure, posInMeasure;
};

#endif // PLAYERTHREAD_H_
