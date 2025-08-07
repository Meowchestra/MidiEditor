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

#include "ToolButton.h"
#include "Tool.h"
#include "../gui/Appearance.h"
#include <QApplication>

ToolButton::ToolButton(Tool *tool, QKeySequence sequence, QWidget *parent)
    : QAction(reinterpret_cast<QObject *>(parent)) {
    button_tool = tool;
    tool->setButton(this);
    setText(button_tool->toolTip());
    QImage image = *(button_tool->image());
    QPixmap pixmap = QPixmap::fromImage(image);
    // Apply dark mode adjustment if needed
    pixmap = Appearance::adjustIconForDarkMode(pixmap, button_tool->toolTip());
    setIcon(QIcon(pixmap));
    connect(this, SIGNAL(triggered()), this, SLOT(buttonClick()));
    setCheckable(true);
    setShortcut(sequence);
}

void ToolButton::buttonClick() {
    button_tool->buttonClick();
}

void ToolButton::releaseButton() {
    button_tool->buttonClick();
}

void ToolButton::refreshIcon() {
    // Check if application is shutting down to prevent QPixmap creation
    QApplication *app = qobject_cast<QApplication *>(QApplication::instance());
    if (!app || app->closingDown()) {
        return; // Don't create QPixmap during shutdown
    }

    QImage image = *(button_tool->image());
    QPixmap pixmap = QPixmap::fromImage(image);
    // Apply dark mode adjustment if needed
    pixmap = Appearance::adjustIconForDarkMode(pixmap, button_tool->toolTip());
    setIcon(QIcon(pixmap));
}
