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

#include "CompleteMidiSetupDialog.h"
#include "Appearance.h"
#include "MainWindow.h"

#include <QGridLayout>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>

CompleteMidiSetupDialog::CompleteMidiSetupDialog(QWidget* parent, bool alertAboutInput, bool alertAboutOutput)
    : QDialog(parent) {

    setMinimumWidth(550);
    setMaximumHeight(450);
    setWindowTitle(tr("No Sound?"));
    // Note: setWindowIcon doesn't use QAction, so we keep the direct approach
    setWindowIcon(Appearance::adjustIconForDarkMode(":/run_environment/graphics/icon.png"));
    QGridLayout* layout = new QGridLayout(this);

    QLabel* icon = new QLabel();
    QPixmap iconPixmap = Appearance::adjustIconForDarkMode(QPixmap(":/run_environment/graphics/midieditor.png"), "midieditor");
    icon->setPixmap(iconPixmap.scaledToWidth(80, Qt::SmoothTransformation));
    icon->setFixedSize(80, 80);
    layout->addWidget(icon, 0, 0, 3, 1);

    QLabel* title = new QLabel(tr("<h1>Complete MIDI Setup</h1>"), this);
    layout->addWidget(title, 0, 1, 1, 2);
    QColor textColor = Appearance::foregroundColor();
    QString textColorStyle = QString("color: rgb(%1, %2, %3)")
                            .arg(textColor.red()).arg(textColor.green()).arg(textColor.blue());
    title->setStyleSheet(textColorStyle);

    QLabel* version = new QLabel(tr("It appears that you did not complete your midi setup!"), this);
    layout->addWidget(version, 1, 1, 1, 2);
    version->setStyleSheet(textColorStyle);

    QScrollArea* a = new QScrollArea(this);
    QString connectOutput = "";
    if (alertAboutOutput) {
        connectOutput = tr("<h3>Output is not connected</h3>"
                           "<p>"
                           "In order to play your music, you have to connect MidiEditor to a "
                           "midi device (on your computer or externally) which can play your sounds.</br>"
                           "</p>");
    }
    QString connectInput = "";
    if (alertAboutInput) {
        connectInput = tr("<h3>Input is not connected</h3>"
                          "<p>"
                          "In order to record music, MidiEditor must be connected to a midi device that you will record music on.</br>"
                          "</p>");
    }
    QLabel* content = new QLabel(QString("<html>"
                                         "<body>")
                                 + connectInput + connectOutput + QString(tr("<p>Please refer to the manual for further instructions.<p/>") +
                                         "</body>"
                                         "</html>"));
    a->setWidgetResizable(true);
    a->setWidget(content);
    a->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    a->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    layout->addWidget(a, 2, 1, 2, 2);

    if (Appearance::shouldUseDarkMode()) {
        content->setStyleSheet("color: white; background-color: #404040; padding: 5px");
        // Don't override scroll area styling - let it inherit from application
        a->setStyleSheet("QScrollArea { background-color: #404040; }");
    } else {
        content->setStyleSheet("color: black; background-color: white; padding: 5px");
        // Don't override scroll area styling - let it inherit from application
        a->setStyleSheet("QScrollArea { background-color: white; }");
    }

    content->setTextInteractionFlags(Qt::LinksAccessibleByMouse);
    content->setOpenExternalLinks(true);
    content->setWordWrap(true);

    layout->setRowStretch(3, 1);
    layout->setColumnStretch(1, 1);

    QFrame* f = new QFrame(this);
    f->setFrameStyle(QFrame::HLine | QFrame::Sunken);
    layout->addWidget(f, 4, 0, 1, 3);

    QPushButton* settings = new QPushButton(tr("Settings"));
    layout->addWidget(settings, 5, 1, 1, 1);
    connect(settings, SIGNAL(clicked()), this, SLOT(openSettings()));

    QPushButton* close = new QPushButton(tr("Close"));
    layout->addWidget(close, 5, 2, 1, 1);
    connect(close, SIGNAL(clicked()), this, SLOT(hide()));
}

void CompleteMidiSetupDialog::openSettings() {
    // Find the MainWindow parent and open its settings dialog
    MainWindow* mainWindow = qobject_cast<MainWindow*>(parent());
    if (mainWindow) {
        mainWindow->openConfig();
    }
}
