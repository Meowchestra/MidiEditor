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

#include "SelectTool.h"
#include "Selection.h"
#include "../MidiEvent/MidiEvent.h"
#include "../MidiEvent/OnEvent.h"
#include "../MidiEvent/OffEvent.h"
#include "../gui/MatrixWidget.h"
#include "../gui/Appearance.h"
#include "../midi/MidiFile.h"
#include "../midi/MidiChannel.h"
#include "../protocol/Protocol.h"
#include "StandardTool.h"

SelectTool::SelectTool(int type)
    : EventTool() {
    stool_type = type;
    x_rect = 0;
    y_rect = 0;
    switch (stool_type) {
        case SELECTION_TYPE_BOX: {
            setImage(":/run_environment/graphics/tool/select_box.png");
            setToolTipText(QObject::tr("Select Events (Box)"));
            break;
        }
        case SELECTION_TYPE_SINGLE: {
            setImage(":/run_environment/graphics/tool/select_single.png");
            setToolTipText(QObject::tr("Select Events (Single)"));
            break;
        }
        case SELECTION_TYPE_LEFT: {
            setImage(":/run_environment/graphics/tool/select_left.png");
            setToolTipText(QObject::tr("Select All Events (Left Side)"));
            break;
        }
        case SELECTION_TYPE_RIGHT: {
            setImage(":/run_environment/graphics/tool/select_right.png");
            setToolTipText(QObject::tr("Select All Events (Right Side)"));
            break;
        }
        case SELECTION_TYPE_ROW: {
            setImage(":/run_environment/graphics/tool/select_cursor.png");
            setToolTipText(QObject::tr("Select All Events (Row)"));
            break;
        }
        case SELECTION_TYPE_MEASURE: {
            setImage(":/run_environment/graphics/tool/select_box3.png");
            setToolTipText(QObject::tr("Select All Events (Measure)"));
            break;
        }
    }
}

SelectTool::SelectTool(SelectTool &other)
    : EventTool(other) {
    stool_type = other.stool_type;
    x_rect = 0;
    y_rect = 0;
    _isActive = false;
    _dragStartTick = -1;
    _dragLastTick = -1;
}

void SelectTool::draw(QPainter *painter) {
    paintSelectedEvents(painter);
    if (stool_type == SELECTION_TYPE_BOX && (x_rect || y_rect)) {
        painter->setPen(Appearance::borderColor());
        QColor selectionColor = Appearance::foregroundColor();
        selectionColor.setAlpha(100);
        painter->setBrush(selectionColor);
        painter->drawRect(x_rect, y_rect, mouseX - x_rect, mouseY - y_rect);
    } else if (stool_type == SELECTION_TYPE_RIGHT || stool_type == SELECTION_TYPE_LEFT) {
        if (mouseIn) {
            painter->setPen(Appearance::borderColor());
            QColor selectionColor = Appearance::foregroundColor();
            selectionColor.setAlpha(100);
            painter->setBrush(selectionColor);
            if (stool_type == SELECTION_TYPE_LEFT) {
                painter->drawRect(0, 0, mouseX, matrixWidget->height() - 1);
            } else {
                painter->drawRect(mouseX, 0, matrixWidget->width() - 1, matrixWidget->height() - 1);
            }
        }
    } else if (stool_type == SELECTION_TYPE_ROW && mouseIn) {
        int line = matrixWidget->lineAtY(mouseY);
        // Allow row selection in both note area (0-127) and misc area (>127)
        if (line >= 0) {
            int rowY = matrixWidget->yPosOfLine(line);
            int nextRowY = matrixWidget->yPosOfLine(line + 1);
            painter->setPen(Appearance::borderColor());
            QColor selectionColor = Appearance::foregroundColor();
            selectionColor.setAlpha(100);
            painter->setBrush(selectionColor);
            painter->drawRect(0, rowY, matrixWidget->width() - 1, nextRowY - rowY);
        }
    } else if (stool_type == SELECTION_TYPE_MEASURE && file()) {
        if (_isActive && _dragStartTick != -1) {
            // Highlight all measures from start to current drag pos
            int startMeasureTick, startMeasureEnd;
            int currentMeasureTick, currentMeasureEnd;
            file()->measure(_dragStartTick, &startMeasureTick, &startMeasureEnd);
            file()->measure(file()->tick(matrixWidget->msOfXPos(mouseX)), &currentMeasureTick, &currentMeasureEnd);
            
            int minTick = qMin(startMeasureTick, currentMeasureTick);
            int maxTick = qMax(startMeasureEnd, currentMeasureEnd);
            
            int startX = matrixWidget->xPosOfMs(file()->msOfTick(minTick));
            int endX = matrixWidget->xPosOfMs(file()->msOfTick(maxTick));
            
            painter->setPen(Appearance::borderColor());
            QColor selectionColor = Appearance::foregroundColor();
            selectionColor.setAlpha(100);
            painter->setBrush(selectionColor);
            painter->drawRect(startX, 0, endX - startX, matrixWidget->height() - 1);
        } else if (mouseIn) {
            int tick = file()->tick(matrixWidget->msOfXPos(mouseX));
            int measureStartTick, measureEndTick;
            file()->measure(tick, &measureStartTick, &measureEndTick);
            int startX = matrixWidget->xPosOfMs(file()->msOfTick(measureStartTick));
            int endX = matrixWidget->xPosOfMs(file()->msOfTick(measureEndTick));
            painter->setPen(Appearance::borderColor());
            QColor selectionColor = Appearance::foregroundColor();
            selectionColor.setAlpha(100);
            painter->setBrush(selectionColor);
            painter->drawRect(startX, 0, endX - startX, matrixWidget->height() - 1);
        }
    }
}

bool SelectTool::press(bool leftClick) {
    if (!leftClick && QApplication::keyboardModifiers().testFlag(Qt::ControlModifier)) {
        _isActive = false;
        return false;
    }
    _isActive = true;
    if (stool_type == SELECTION_TYPE_BOX) {
        y_rect = mouseY;
        x_rect = mouseX;
    } else if (stool_type == SELECTION_TYPE_MEASURE && file()) {
        _dragStartTick = file()->tick(matrixWidget->msOfXPos(mouseX));
        _dragLastTick = _dragStartTick;
    }
    return true;
}

bool SelectTool::release() {
    if (!file() || !_isActive) {
        _isActive = false;
        _dragStartTick = -1;
        return false;
    }
    _isActive = false;

    file()->protocol()->startNewAction(QObject::tr("Selection Changed"), image());
    ProtocolEntry *toCopy = copy();

    Qt::KeyboardModifiers mods = QApplication::keyboardModifiers();
    bool modifierHeld = (mods & Qt::ShiftModifier) || (mods & Qt::ControlModifier);

    if (!modifierHeld) {
        clearSelection();
    }

    if (stool_type == SELECTION_TYPE_BOX || stool_type == SELECTION_TYPE_SINGLE) {
        int x_start, y_start, x_end, y_end;
        if (stool_type == SELECTION_TYPE_BOX) {
            x_start = x_rect;
            y_start = y_rect;
            x_end = mouseX;
            y_end = mouseY;
            if (x_start > x_end) {
                int tmp = x_start;
                x_start = x_end;
                x_end = tmp;
            }
            if (y_start > y_end) {
                int tmp = y_start;
                y_start = y_end;
                y_end = tmp;
            }
        } else if (stool_type == SELECTION_TYPE_SINGLE) {
            x_start = mouseX;
            y_start = mouseY;
            x_end = mouseX + 1;
            y_end = mouseY + 1;
        }

        bool isShift = (mods & Qt::ShiftModifier);
        bool isCtrl = (mods & Qt::ControlModifier);

        foreach(MidiEvent* event, *(matrixWidget->activeEvents())) {
            if (inRect(event, x_start, y_start, x_end, y_end)) {
                if (isShift) {
                    // Additive: select if not already selected
                    if (!Selection::instance()->selectedEvents().contains(event)) {
                        selectEvent(event, false, true, false);
                    }
                } else if (isCtrl) {
                    // Subtractive: unselect if already selected
                    if (Selection::instance()->selectedEvents().contains(event)) {
                        deselectEvent(event);
                    }
                } else {
                    // Normal: select
                    selectEvent(event, false, false, false);
                }
            }
        }
        Selection::instance()->setSelection(Selection::instance()->selectedEvents());
    } else if (stool_type == SELECTION_TYPE_RIGHT || stool_type == SELECTION_TYPE_LEFT) {
        int tick = file()->tick(matrixWidget->msOfXPos(mouseX));
        int start, end;
        if (stool_type == SELECTION_TYPE_LEFT) {
            start = 0;
            end = tick;
        } else if (stool_type == SELECTION_TYPE_RIGHT) {
            end = file()->endTick();
            start = tick;
        }
        foreach(MidiEvent* event, *(file()->eventsBetween(start, end))) {
            selectEvent(event, false, false, false);
        }
        Selection::instance()->setSelection(Selection::instance()->selectedEvents());
    } else if (stool_type == SELECTION_TYPE_ROW) {
        selectRow(matrixWidget->lineAtY(mouseY), mods);
    } else if (stool_type == SELECTION_TYPE_MEASURE) {
        // Handle potentially multi-measure drag selection
        int currentTick = file()->tick(matrixWidget->msOfXPos(mouseX));
        int measureStart, measureEnd;
        int dragStart, dragEnd;
        file()->measure(_dragStartTick, &dragStart, &measureEnd);
        file()->measure(currentTick, &measureStart, &dragEnd);
        
        int finalStart = qMin(dragStart, measureStart);
        int finalEnd = qMax(dragEnd, measureEnd);
        
        bool isShift = (mods & Qt::ShiftModifier);
        bool isCtrl = (mods & Qt::ControlModifier);
        bool isAlt = (mods & Qt::AltModifier);

        if (!isShift && !isCtrl && !isAlt) {
            clearSelection();
        }

        // Collect all events in the measure range first
        QList<MidiEvent *> allMeasureEvents;
        for (int i = 0; i < 16; ++i) {
            MidiChannel *channel = file()->channel(i);
            if (!channel) continue;
            foreach (MidiEvent *event, *(channel->eventMap())) {
                int eventStart = event->midiTime();
                int eventEnd = eventStart;
                if (OnEvent *on = dynamic_cast<OnEvent*>(event)) {
                    if (on->offEvent()) eventEnd = on->offEvent()->midiTime();
                }
                if (!(eventEnd <= finalStart || eventStart >= finalEnd)) {
                    allMeasureEvents.append(event);
                }
            }
        }

        foreach(MidiEvent* event, allMeasureEvents) {
            if (isShift || isAlt) { // Alt on measures is additive only
                if (!Selection::instance()->selectedEvents().contains(event)) {
                    selectEvent(event, false, true, false);
                }
            } else if (isCtrl) {
                if (Selection::instance()->selectedEvents().contains(event)) {
                    deselectEvent(event);
                }
            } else {
                selectEvent(event, false, true, false);
            }
        }
    }

    x_rect = 0;
    y_rect = 0;
    _dragStartTick = -1;

    protocol(toCopy, this);
    file()->protocol()->endAction();
    if (_standardTool) {
        Tool::setCurrentTool(_standardTool);
        _standardTool->move(mouseX, mouseY);
        _standardTool->release();
    }
    return true;
}

bool SelectTool::inRect(MidiEvent *event, int x_start, int y_start, int x_end, int y_end) {
    return pointInRect(event->x(), event->y(), x_start, y_start, x_end, y_end) ||
           pointInRect(event->x(), event->y() + event->height(), x_start, y_start, x_end, y_end) ||
           pointInRect(event->x() + event->width(), event->y(), x_start, y_start, x_end, y_end) ||
           pointInRect(event->x() + event->width(), event->y() + event->height(), x_start, y_start, x_end, y_end) ||
           pointInRect(x_start, y_start, event->x(), event->y(), event->x() + event->width(), event->y() + event->height());
}

bool SelectTool::move(int mouseX, int mouseY) {
    EditorTool::move(mouseX, mouseY);
    if (_isActive && stool_type == SELECTION_TYPE_MEASURE && file()) {
        _dragLastTick = file()->tick(matrixWidget->msOfXPos(mouseX));
        return true; // Trigger update during drag
    }
    return true;
}

ProtocolEntry *SelectTool::copy() {
    return new SelectTool(*this);
}

void SelectTool::reloadState(ProtocolEntry *entry) {
    SelectTool *other = dynamic_cast<SelectTool *>(entry);
    if (!other) {
        return;
    }
    EventTool::reloadState(entry);
    x_rect = 0;
    y_rect = 0;
    stool_type = other->stool_type;
    _isActive = false;
    _dragStartTick = -1;
    _dragLastTick = -1;
}

bool SelectTool::releaseOnly() {
    return release();
}

bool SelectTool::showsSelection() {
    return true;
}

void SelectTool::selectRow(int line, Qt::KeyboardModifiers modifiers) {
    if (line < 0 || !matrixWidget || !file()) return;

    bool isShift = (modifiers & Qt::ShiftModifier);
    bool isCtrl = (modifiers & Qt::ControlModifier);
    bool isAlt = (modifiers & Qt::AltModifier);

    if (!isShift && !isCtrl && !isAlt) {
        clearSelection();
    }
    
    QList<MidiEvent *> eventsToSelect;
    for (int i = 0; i < 19; ++i) {
        MidiChannel *channel = file()->channel(i);
        if (!channel) continue;
        foreach (MidiEvent *event, *(channel->eventMap())) {
            if (event->line() == line) {
                eventsToSelect.append(event);
            }
        }
    }

    foreach(MidiEvent* event, eventsToSelect) {
        if (isShift) {
            if (!Selection::instance()->selectedEvents().contains(event)) {
                selectEvent(event, false, true, false);
            }
        } else if (isCtrl) {
            if (Selection::instance()->selectedEvents().contains(event)) {
                deselectEvent(event);
            }
        } else if (isAlt) {
            // Alt on row: Toggle
            if (Selection::instance()->selectedEvents().contains(event)) {
                deselectEvent(event);
            } else {
                selectEvent(event, false, true, false);
            }
        } else {
            selectEvent(event, false, false, false);
        }
    }
    Selection::instance()->setSelection(Selection::instance()->selectedEvents());
}

void SelectTool::selectMeasure(int tick, Qt::KeyboardModifiers modifiers) {
    if (!file() || !matrixWidget) return;

    int measureStartTick, measureEndTick;
    file()->measure(tick, &measureStartTick, &measureEndTick);

    bool isShift = (modifiers & Qt::ShiftModifier);
    bool isCtrl = (modifiers & Qt::ControlModifier);
    bool isAlt = (modifiers & Qt::AltModifier);

    if (!isShift && !isCtrl && !isAlt) {
        clearSelection();
    }

    // Select all note events that overlap this measure
    QList<MidiEvent *> eventsToSelect;
    for (int i = 0; i < 16; ++i) { // Only scan instrument channels for measure selection
        MidiChannel *channel = file()->channel(i);
        if (!channel) continue;
        foreach (MidiEvent *event, *(channel->eventMap())) {
            int eventStart = event->midiTime();
            int eventEnd = eventStart;
            
            if (OnEvent *on = dynamic_cast<OnEvent*>(event)) {
                if (on->offEvent()) {
                    eventEnd = on->offEvent()->midiTime();
                }
            }
            
            // Overlap check: selects if any part of the note is inside the measure
            bool overlaps = !(eventEnd <= measureStartTick || eventStart >= measureEndTick);
            
            if (overlaps) {
                eventsToSelect.append(event);
            }
        }
    }

    foreach(MidiEvent* event, eventsToSelect) {
        if (isShift || isAlt) { // Alt on measures is additive only
            if (!Selection::instance()->selectedEvents().contains(event)) {
                selectEvent(event, false, true, false);
            }
        } else if (isCtrl) {
            if (Selection::instance()->selectedEvents().contains(event)) {
                deselectEvent(event);
            }
        } else {
            selectEvent(event, false, true, false);
        }
    }
    Selection::instance()->setSelection(Selection::instance()->selectedEvents());
}
