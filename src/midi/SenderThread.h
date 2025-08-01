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

#ifndef SENDERTHREAD_H_
#define SENDERTHREAD_H_

// Qt includes
#include <QMutex>
#include <QQueue>
#include <QThread>
#include <QWaitCondition>

// Project includes
#include "MidiOutput.h"

/**
 * \class SenderThread
 *
 * \brief Thread for queued MIDI message transmission.
 *
 * SenderThread handles the queued transmission of MIDI messages in a separate
 * thread to ensure smooth MIDI output without blocking the main application.
 * It provides:
 *
 * - **Queued transmission**: Buffered MIDI message sending
 * - **Thread safety**: Mutex-protected queue operations
 * - **Event prioritization**: Separate queues for different event types
 * - **Non-blocking**: Doesn't interfere with UI responsiveness
 * - **Automatic processing**: Continuous processing of queued events
 *
 * The thread maintains separate queues for different types of MIDI events
 * and processes them in order, ensuring proper timing and delivery.
 */
class SenderThread : public QThread {
public:
    /**
     * \brief Creates a new SenderThread.
     */
    SenderThread();

    /**
     * \brief Main thread execution function.
     */
    void run();

    /**
     * \brief Enqueues a MIDI event for transmission.
     * \param event The MidiEvent to queue for sending
     */
    void enqueue(MidiEvent *event);

private:
    /** \brief Queue for general MIDI events */
    QQueue<MidiEvent *> *_eventQueue;

    /** \brief Queue for note events */
    QQueue<MidiEvent *> *_noteQueue;

    /** \brief Mutex for thread-safe queue access */
    QMutex *_mutex;

    /** \brief Wait condition for thread synchronization */
    QWaitCondition *_waitCondition;
};

#endif // SENDERTHREAD_H_
