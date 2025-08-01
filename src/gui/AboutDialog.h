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

#ifndef ABOUTDIALOG_H_
#define ABOUTDIALOG_H_

// Qt includes
#include <QDialog>
#include <QList>
#include <QObject>
#include <QString>
#include <QWidget>

/**
 * \class AboutDialog
 *
 * \brief Dialog displaying application information and credits.
 *
 * AboutDialog shows information about the MIDI Editor application including:
 *
 * - **Application details**: Version, copyright, and license information
 * - **Contributors**: List of people who contributed to the project
 * - **Credits**: Acknowledgments for libraries and resources used
 * - **Contact information**: Links to project resources and support
 *
 * The dialog provides a standard "About" interface that users can access
 * from the Help menu to learn more about the application and its development.
 */
class AboutDialog : public QDialog {
    Q_OBJECT

public:
    /**
     * \brief Creates a new AboutDialog.
     * \param parent The parent widget
     */
    AboutDialog(QWidget *parent = 0);

private:
    /**
     * \brief Loads the list of contributors from resources.
     * \return Pointer to QList containing contributor names
     */
    QList<QString> *loadContributors();
};

#endif // ABOUTDIALOG_H_
