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

#include "SizeChangeTool.h"

#include "../MidiEvent/MidiEvent.h"
#include "../MidiEvent/OffEvent.h"
#include "../MidiEvent/OnEvent.h"
#include "../gui/MatrixWidget.h"
#include "../midi/MidiFile.h"
#include "../protocol/Protocol.h"
#include "StandardTool.h"

#include "Selection.h"

SizeChangeTool::SizeChangeTool()
    : EventTool() {
    inDrag = false;
    xPos = 0;
    dragsOnEvent = false;
    setImage(":/run_environment/graphics/tool/change_size.png");
    setToolTipText(QObject::tr("Resize Events"));
}

SizeChangeTool::SizeChangeTool(SizeChangeTool &other)
    : EventTool(other) {
    return;
}

ProtocolEntry *SizeChangeTool::copy() {
    return new SizeChangeTool(*this);
}

void SizeChangeTool::reloadState(ProtocolEntry *entry) {
    SizeChangeTool *other = dynamic_cast<SizeChangeTool *>(entry);
    if (!other) {
        return;
    }
    EventTool::reloadState(entry);
}

void SizeChangeTool::draw(QPainter *painter) {
    int currentX = rasteredX(mouseX);

    // Set cursor on OpenGL container if available, otherwise on matrix widget
    if (_openglContainer) {
        _openglContainer->setCursor(Qt::ArrowCursor);
    } else {
        matrixWidget->setCursor(Qt::ArrowCursor);
    }

    paintSelectedEvents(painter);

    if (!inDrag) {
        foreach(MidiEvent* event, Selection::instance()->selectedEvents()) {
            bool show = event->shown();
            if (!show) {
                OnEvent *ev = dynamic_cast<OnEvent *>(event);
                if (ev) {
                    show = ev->offEvent() && ev->offEvent()->shown();
                }
            }
            if (show) {
                if (pointInRect(mouseX, mouseY, event->x() + event->width() - 2, event->y(), event->x() + event->width() + 2, event->y() + event->height())) {
                    if (_openglContainer) _openglContainer->setCursor(Qt::SplitHCursor); else matrixWidget->setCursor(Qt::SplitHCursor);
                }
                if (pointInRect(mouseX, mouseY, event->x() - 2, event->y(), event->x() + 2, event->y() + event->height())) {
                    if (_openglContainer) _openglContainer->setCursor(Qt::SplitHCursor); else matrixWidget->setCursor(Qt::SplitHCursor);
                }
            }
        }
        return;
    }

    // Force the resize cursor during drawing if we are dragging
    if (_openglContainer) {
        _openglContainer->setCursor(Qt::SplitHCursor);
    } else {
        matrixWidget->setCursor(Qt::SplitHCursor);
    }

    painter->setPen(Qt::gray);
    painter->drawLine(currentX, 0, currentX, matrixWidget->height());

    // Ghost Note Colors (DAW Standards)
    bool darkMode = Appearance::shouldUseDarkMode();
    QColor ghostFill = darkMode ? QColor(255, 255, 255, 60) : QColor(0, 0, 0, 40);
    QColor ghostBorder = darkMode ? QColor(255, 255, 255, 120) : QColor(0, 0, 0, 80);

    int endEventShift = dragsOnEvent ? 0 : currentX - xPos;
    int startEventShift = dragsOnEvent ? currentX - xPos : 0;

    foreach(MidiEvent* event, Selection::instance()->selectedEvents()) {
        bool show = event->shown();
        if (!show) {
            OnEvent *ev = dynamic_cast<OnEvent *>(event);
            if (ev) {
                show = ev->offEvent() && ev->offEvent()->shown();
            }
        }
        if (show) {
            int dragX, dragWidth;
            OnEvent *onEvent = dynamic_cast<OnEvent *>(event);
            if (onEvent && onEvent->offEvent()) {
                int rawX = matrixWidget->xPosOfMs(matrixWidget->msOfTick(onEvent->midiTime()));
                int rawEndX = matrixWidget->xPosOfMs(matrixWidget->msOfTick(onEvent->offEvent()->midiTime()));
                dragX = rawX;
                dragWidth = rawEndX - rawX;
            } else {
                dragX = event->x();
                dragWidth = event->width();
            }

            QRectF ghostRect(dragX + startEventShift, event->y(),
                             dragWidth - startEventShift + endEventShift,
                             event->height());
            
            if (ghostRect.width() < 1) {
                ghostRect.setWidth(1);
            }
            
            painter->setBrush(ghostFill);
            painter->setPen(QPen(ghostBorder, 1, Qt::SolidLine));
            painter->drawRoundedRect(ghostRect, 1, 1);
        }
    }
}

bool SizeChangeTool::press(bool leftClick) {
    if (Selection::instance()->selectedEvents().isEmpty()) {
        return false;
    }

    inDrag = true;
    xPos = rasteredX(mouseX);

    if (_standardTool != 0) {
        // Edge drag delegated from Standard Tool
        bool foundEdge = false;
        foreach(MidiEvent* event, Selection::instance()->selectedEvents()) {
            bool onLeftEdge = pointInRect(mouseX, mouseY, event->x() - 2, event->y(), event->x() + 2, event->y() + event->height());
            if (onLeftEdge) { 
                dragsOnEvent = true; 
                foundEdge = true; 
                break; 
            }
            bool onRightEdge = pointInRect(mouseX, mouseY, event->x() + event->width() - 2, event->y(), event->x() + event->width() + 2, event->y() + event->height());
            if (onRightEdge) { 
                dragsOnEvent = false; 
                foundEdge = true; 
                break; 
            }
        }
        if (!foundEdge) {
            dragsOnEvent = false;
        }
    } else {
        // Standalone Resize Events Tool
        // Left click grabs Note On (start), Right click grabs Note Off (end)
        dragsOnEvent = leftClick;
    }

    return true;
}

bool SizeChangeTool::release() {
    int currentX = rasteredX(mouseX);

    inDrag = false;
    int endEventShift = 0;
    int startEventShift = 0;
    if (dragsOnEvent) {
        startEventShift = currentX - xPos;
    } else {
        endEventShift = currentX - xPos;
    }
    xPos = 0;
    if (Selection::instance()->selectedEvents().count() > 0) {
        currentProtocol()->startNewAction(QObject::tr("Change Event Duration"), image());
        foreach(MidiEvent* event, Selection::instance()->selectedEvents()) {
            OnEvent *on = dynamic_cast<OnEvent *>(event);
            OffEvent *off = dynamic_cast<OffEvent *>(event);
            if (on) {
                int onTick = file()->tick(file()->msOfTick(on->midiTime()) - matrixWidget->timeMsOfWidth(-startEventShift));
                int offTick = file()->tick(file()->msOfTick(on->offEvent()->midiTime()) - matrixWidget->timeMsOfWidth(-endEventShift));
                if (onTick < offTick) {
                    if (dragsOnEvent) {
                        changeTick(on, -startEventShift);
                    } else {
                        changeTick(on->offEvent(), -endEventShift);
                    }
                }
            } else if (off) {
                // do nothing; endEvents are shifted when touching their OnEvent
                continue;
            } else if (dragsOnEvent) {
                // normal events will be moved as normal
                changeTick(event, -startEventShift);
            }
        }
        currentProtocol()->endAction();
    }
    // Set cursor on OpenGL container if available, otherwise on matrix widget
    if (_openglContainer) {
        _openglContainer->setCursor(Qt::ArrowCursor);
    } else {
        matrixWidget->setCursor(Qt::ArrowCursor);
    }
    if (_standardTool) {
        Tool::setCurrentTool(_standardTool);
        _standardTool->move(mouseX, mouseY);
        _standardTool->release();
    }
    return true;
}

bool SizeChangeTool::move(int mouseX, int mouseY) {
    // Update member variables so draw() uses current mouse position
    EditorTool::move(mouseX, mouseY);

    if (inDrag) {
        // Set cursor on OpenGL container if available, otherwise on matrix widget
        if (_openglContainer) {
            _openglContainer->setCursor(Qt::SplitHCursor);
        } else {
            matrixWidget->setCursor(Qt::SplitHCursor);
        }
        return true;
    }

    foreach(MidiEvent* event, Selection::instance()->selectedEvents()) {
        if (pointInRect(mouseX, mouseY, event->x() - 2, event->y(), event->x() + 2, event->y() + event->height())) {
            // Set cursor on OpenGL container if available, otherwise on matrix widget
            if (_openglContainer) {
                _openglContainer->setCursor(Qt::SplitHCursor);
            } else {
                matrixWidget->setCursor(Qt::SplitHCursor);
            }
            return false;
        }
        if (pointInRect(mouseX, mouseY, event->x() + event->width() - 2, event->y(), event->x() + event->width() + 2, event->y() + event->height())) {
            // Set cursor on OpenGL container if available, otherwise on matrix widget
            if (_openglContainer) {
                _openglContainer->setCursor(Qt::SplitHCursor);
            } else {
                matrixWidget->setCursor(Qt::SplitHCursor);
            }
            return false;
        }
    }
    // Set cursor on OpenGL container if available, otherwise on matrix widget
    if (_openglContainer) {
        _openglContainer->setCursor(Qt::ArrowCursor);
    } else {
        matrixWidget->setCursor(Qt::ArrowCursor);
    }
    return false;
}

bool SizeChangeTool::releaseOnly() {
    inDrag = false;
    xPos = 0;
    return true;
}

bool SizeChangeTool::showsSelection() {
    return true;
}
