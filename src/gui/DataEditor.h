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

#ifndef DATAEDITOR_H_
#define DATAEDITOR_H_

// Qt includes
#include <QScrollArea>

// Forward declarations
class QPushButton;
class QLineEdit;

/**
 * \class DataLineEditor
 *
 * \brief Editor for a single line of binary data with add/remove controls.
 *
 * DataLineEditor manages the editing of a single byte in a binary data array.
 * It provides:
 *
 * - **Byte editing**: Text field for entering hexadecimal byte values
 * - **Add/remove controls**: Plus and minus buttons for array manipulation
 * - **Validation**: Ensures entered values are valid bytes (0-255)
 * - **Change notification**: Signals when data is modified
 *
 * This class is used internally by DataEditor to manage individual data bytes.
 */
class DataLineEditor : public QObject {
    Q_OBJECT

public:
    /**
     * \brief Creates a new DataLineEditor.
     * \param line The line number in the data array
     * \param plus The plus button for adding data
     * \param minus Optional minus button for removing data
     * \param edit Optional line edit for data input
     */
    DataLineEditor(int line, QPushButton *plus, QPushButton *minus = 0, QLineEdit *edit = 0);

public slots:
    /**
     * \brief Handles plus button clicks to add data.
     */
    void plus();

    /**
     * \brief Handles minus button clicks to remove data.
     */
    void minus();

    /**
     * \brief Handles text changes in the edit field.
     * \param text The new text value
     */
    void changed(QString text);

signals:
    /**
     * \brief Emitted when data changes.
     * \param line The line number that changed
     * \param data The new data value
     */
    void dataChanged(int line, unsigned char data);

    /**
     * \brief Emitted when plus button is clicked.
     * \param line The line number where plus was clicked
     */
    void plusClicked(int line);

    /**
     * \brief Emitted when minus button is clicked.
     * \param line The line number where minus was clicked
     */
    void minusClicked(int line);

private:
    /** \brief The line number in the data array */
    int _line;
};

/**
 * \class DataEditor
 *
 * \brief Scrollable editor for binary data arrays with hexadecimal display.
 *
 * DataEditor provides a comprehensive interface for editing binary data,
 * commonly used for SysEx messages and other raw MIDI data. It features:
 *
 * - **Hexadecimal display**: Shows data bytes in hex format for easy editing
 * - **Scrollable interface**: Handles large data arrays with scroll support
 * - **Dynamic editing**: Add and remove bytes from the data array
 * - **Validation**: Ensures all entered values are valid bytes
 * - **Real-time updates**: Immediate feedback for data changes
 *
 * The editor displays each byte on a separate line with controls for
 * modification, making it suitable for editing complex binary data structures.
 */
class DataEditor : public QScrollArea {
    Q_OBJECT

public:
    /**
     * \brief Creates a new DataEditor.
     * \param parent The parent widget
     */
    DataEditor(QWidget *parent = 0);

    /**
     * \brief Sets the data to be edited.
     * \param data The binary data to edit
     */
    void setData(QByteArray data);

    /**
     * \brief Gets the current data.
     * \return QByteArray containing the edited data
     */
    QByteArray data();

    /**
     * \brief Rebuilds the editor interface.
     */
    void rebuild();

public slots:
    /**
     * \brief Handles data changes from line editors.
     * \param line The line number that changed
     * \param data The new data value
     */
    void dataChanged(int line, unsigned char data);

    /**
     * \brief Handles plus button clicks to add data.
     * \param line The line number where data should be added
     */
    void plusClicked(int line);

    /**
     * \brief Handles minus button clicks to remove data.
     * \param line The line number where data should be removed
     */
    void minusClicked(int line);

private:
    /** \brief The binary data being edited */
    QByteArray _data;

    /** \brief The central widget containing the editor interface */
    QWidget *_central;
};

#endif // DATAEDITOR_H_
