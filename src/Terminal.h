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

#ifndef TERMINAL_H_
#define TERMINAL_H_

#include <QObject>

// Forward declarations
class QProcess;
class QTextEdit;

/**
 * \class Terminal
 *
 * \brief Terminal interface for external MIDI processes and debugging.
 *
 * The Terminal class provides a console interface for running external MIDI
 * processes and displaying their output. It's primarily used for:
 *
 * - Running external MIDI tools and utilities
 * - Debugging MIDI connections and port configurations
 * - Displaying process output and error messages
 * - Managing MIDI port connections
 *
 * The terminal uses a singleton pattern and integrates with Qt's process
 * management system to provide a unified interface for external tool execution.
 */
class Terminal : public QObject {
    Q_OBJECT

public:
    /**
     * \brief Creates a new Terminal instance.
     */
    Terminal();

    /**
     * \brief Initializes the terminal with a process and MIDI ports.
     * \param startString Command string to start the external process
     * \param inPort MIDI input port identifier
     * \param outPort MIDI output port identifier
     *
     * Tries to start a process with the given command string. On success,
     * it will attempt to open the MIDI ports with the specified identifiers.
     */
    static void initTerminal(QString startString, QString inPort,
                             QString outPort);

    /**
     * \brief Gets the singleton terminal instance.
     * \return Pointer to the global Terminal instance
     */
    static Terminal *terminal();

    /**
     * \brief Writes a message to the terminal console.
     * \param message The message to display in the terminal
     */
    void writeString(QString message);

    /**
     * \brief Executes a command string with MIDI port configuration.
     * \param startString Command string to execute
     * \param inPort MIDI input port identifier
     * \param outPort MIDI output port identifier
     *
     * Will connect to the specified MIDI ports if they are not empty.
     * Will stop any currently running process before starting the new one.
     */
    void execute(QString startString, QString inPort,
                 QString outPort);

    /**
     * \brief Gets the terminal console widget.
     * \return Pointer to the QTextEdit widget used as the console
     */
    QTextEdit *console();

public slots:
    /**
     * \brief Called when a process has been started.
     */
    void processStarted();

    /**
     * \brief Prints standard output from the process to the terminal.
     */
    void printToTerminal();

    /**
     * \brief Prints error output from the process to the terminal.
     */
    void printErrorToTerminal();

private:
    /** \brief Singleton instance of the terminal */
    static Terminal *_terminal;

    /** \brief The external process being managed */
    QProcess *_process;

    /** \brief The text widget used as the console display */
    QTextEdit *_textEdit;

    /** \brief MIDI input and output port identifiers */
    QString _inPort, _outPort;
};

#endif // TERMINAL_H_
