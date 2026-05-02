#include "MeasureTool.h"

#include <QInputDialog>

#include "EventTool.h"
#include "../gui/MatrixWidget.h"
#include "../gui/Appearance.h"
#include "../midi/MidiFile.h"
#include "../protocol/Protocol.h"

MeasureTool::MeasureTool()
    : EventTool() {
    setImage(":/run_environment/graphics/tool/measure.png");
    setToolTipText(QObject::tr("Insert or Delete Measures"));
    _dragAnchorMeasure = -1;
    _isDragging = false;
}

MeasureTool::MeasureTool(MeasureTool &other) : MeasureTool() {
    _selectedMeasures = other._selectedMeasures;
    _dragAnchorMeasure = other._dragAnchorMeasure;
    _isDragging = other._isDragging;
}

void MeasureTool::clearSelection() {
    _selectedMeasures.clear();
    _dragAnchorMeasure = -1;
    _isDragging = false;
}

void MeasureTool::enter() {
    // Only reset dragging state to prevent stuck drags when mouse leaves
    _isDragging = false;
}

void MeasureTool::deselect() {
    EditorTool::deselect();
    clearSelection();
}

void MeasureTool::select() {
    EditorTool::select();
    clearSelection();
}

void MeasureTool::draw(QPainter *painter) {
    int ms = matrixWidget->msOfXPos(mouseX);
    int tick = file()->tick(ms);
    int measureStartTick, measureEndTick;
    int currentMeasure = file()->measure(tick, &measureStartTick, &measureEndTick);

    // 1. Draw active selection (for deletion)
    bool isShiftHeld = QApplication::keyboardModifiers().testFlag(Qt::ShiftModifier);
    bool showPreview = _isDragging || (!_selectedMeasures.isEmpty() && isShiftHeld);
    int anchor = _dragAnchorMeasure > 0 ? _dragAnchorMeasure : (_selectedMeasures.isEmpty() ? -1 : _selectedMeasures.last());

    if (!_selectedMeasures.isEmpty() || showPreview) {
        QList<int> measuresToDraw = _selectedMeasures;
        
        if (showPreview && anchor > 0) {
            int m1 = anchor;
            int m2 = currentMeasure;
            if (m1 > m2) std::swap(m1, m2);
            for (int i = m1; i <= m2; ++i) {
                if (!measuresToDraw.contains(i)) {
                    measuresToDraw.append(i);
                }
            }
        }

        painter->setOpacity(0.5);
        foreach(int m, measuresToDraw) {
            fillMeasures(painter, m, m);
        }
    }

    int dist, measureX;
    int closestMeasure = this->closestMeasureStart(&dist, &measureX);

    // 2. Determine if we are in "Insert" mode or "Select" mode
    if (dist < 20) {
        // Insertion Mode: Wide shaded column with side borders (classic look)
        QColor col = Appearance::timeSignatureToolHighlightColor();
        painter->setOpacity(0.4); // Slightly stronger for visibility
        painter->fillRect(measureX - 10, 0, 20, matrixWidget->height(), col);
        
        painter->setOpacity(0.9);
        QPen pen(col);
        pen.setWidth(1);
        painter->setPen(pen);
        painter->drawLine(measureX - 10, 0, measureX - 10, matrixWidget->height());
        painter->drawLine(measureX + 10, 0, measureX + 10, matrixWidget->height());
        
        // Label the insertion point (simplified text)
        painter->drawText(QPoint(measureX + 12, 20), QObject::tr("Insert Measures"));
    } else {
        // Selection Mode: Highlight the measure we are hovering over
        // Only if it's not already part of the active selection (to avoid darkening)
        bool isAlreadySelected = _selectedMeasures.contains(currentMeasure);
        
        if (showPreview && anchor > 0) {
            int m1 = anchor;
            int m2 = currentMeasure;
            if (m1 > m2) std::swap(m1, m2);
            if (currentMeasure >= m1 && currentMeasure <= m2) isAlreadySelected = true;
        }

        if (!isAlreadySelected) {
            int x1 = matrixWidget->xPosOfMs(file()->msOfTick(measureStartTick));
            int x2 = matrixWidget->xPosOfMs(file()->msOfTick(measureEndTick));
            
            painter->setOpacity(1.0); // Reset opacity
            painter->setPen(Appearance::borderColor());
            QColor selectionColor = Appearance::foregroundColor();
            selectionColor.setAlpha(100);
            painter->setBrush(selectionColor);
            painter->drawRect(x1, 0, x2 - x1, matrixWidget->height() - 1);
        }
    }
    painter->setOpacity(1.0); // Final reset
}

bool MeasureTool::press(bool leftClick) {
    int ms = matrixWidget->msOfXPos(mouseX);
    int tick = file()->tick(ms);
    int measureStartTick, measureEndTick;
    int currentMeasure = file()->measure(tick, &measureStartTick, &measureEndTick);

    if (leftClick) {
        int dist, measureX;
        closestMeasureStart(&dist, &measureX);
        if (dist >= 20) {
            if (QApplication::keyboardModifiers().testFlag(Qt::ControlModifier)) {
                if (_selectedMeasures.contains(currentMeasure)) {
                    _selectedMeasures.removeAll(currentMeasure);
                } else {
                    _selectedMeasures.append(currentMeasure);
                }
                _dragAnchorMeasure = currentMeasure;
            } else if (QApplication::keyboardModifiers().testFlag(Qt::ShiftModifier)) {
                if (_dragAnchorMeasure == -1) {
                    _dragAnchorMeasure = currentMeasure;
                }
                _isDragging = true; // allow move() to show preview
            } else {
                _selectedMeasures.clear();
                _selectedMeasures.append(currentMeasure);
                _dragAnchorMeasure = currentMeasure;
                _isDragging = true;
            }
        }
    } else {
        // Middle click to delete (only if not near a divider)
        int dist, measureX;
        closestMeasureStart(&dist, &measureX);
        if (dist >= 20) {
            if (!_selectedMeasures.contains(currentMeasure)) {
                _selectedMeasures.clear();
                _selectedMeasures.append(currentMeasure);
            }
            deleteSelectedMeasures();
        }
    }
    return true;
}

bool MeasureTool::move(int mouseX, int mouseY) {
    EventTool::move(mouseX, mouseY);
    return true;
}

bool MeasureTool::release() {
    int ms = matrixWidget->msOfXPos(mouseX);
    int tick = file()->tick(ms);
    int measureStartTick, measureEndTick;
    int currentMeasure = file()->measure(tick, &measureStartTick, &measureEndTick);

    bool wasDragging = _isDragging;
    if (_isDragging && _dragAnchorMeasure > 0) {
        int m1 = _dragAnchorMeasure;
        int m2 = currentMeasure;
        if (m1 > m2) std::swap(m1, m2);
        for (int i = m1; i <= m2; ++i) {
            if (!_selectedMeasures.contains(i)) {
                _selectedMeasures.append(i);
            }
        }
    }
    _isDragging = false;

    int dist, measureX;
    int closestMeasure = this->closestMeasureStart(&dist, &measureX);

    // If near a divider, we insert
    // We shouldn't insert if we were dragging to select a range.
    if (dist < 20 && !wasDragging) {
        bool ok;
        int num = QInputDialog::getInt(matrixWidget, QObject::tr("Insert Measures"),
                                       QObject::tr("Number of measures to insert:"), 1, 1, 100000, 1, &ok);
        if (ok) {
            file()->protocol()->startNewAction(QObject::tr("Insert Measures"), image());
            file()->insertMeasures(closestMeasure - 1, num);
            clearSelection();
            file()->protocol()->endAction();
        }
        return true;
    }

    return true;
}

void MeasureTool::deleteSelectedMeasures() {
    if (_selectedMeasures.isEmpty()) return;
    
    file()->protocol()->startNewAction(QObject::tr("Remove Measures"), image());
    
    // Sort measures in descending order so we can delete them safely
    // without shifting the indices of subsequent measures we want to delete.
    QList<int> sortedMeasures = _selectedMeasures;
    std::sort(sortedMeasures.begin(), sortedMeasures.end(), std::greater<int>());
    
    int currentEnd = sortedMeasures.first();
    int currentStart = currentEnd;
    
    for (int i = 1; i < sortedMeasures.size(); ++i) {
        if (sortedMeasures[i] == currentStart - 1) {
            // Part of the contiguous block
            currentStart = sortedMeasures[i];
        } else {
            // Break in contiguity, delete the block we found
            file()->deleteMeasures(currentStart, currentEnd);
            currentEnd = sortedMeasures[i];
            currentStart = currentEnd;
        }
    }
    // Delete the final block
    file()->deleteMeasures(currentStart, currentEnd);
    
    clearSelection();
    file()->protocol()->endAction();
}

bool MeasureTool::releaseOnly() {
    _isDragging = false;
    return false;
}

bool MeasureTool::releaseKey(int key) {
    if (key == Qt::Key_Escape) {
        clearSelection();
        return true;
    }
    if (key == Qt::Key_Delete && !_selectedMeasures.isEmpty()) {
        deleteSelectedMeasures();
        return true;
    }
    return false;
}

ProtocolEntry *MeasureTool::copy() {
    return new MeasureTool(*this);
}

void MeasureTool::reloadState(ProtocolEntry *entry) {
    MeasureTool *other = dynamic_cast<MeasureTool *>(entry);
    if (!other) {
        return;
    }
    EventTool::reloadState(entry);
}

int MeasureTool::closestMeasureStart(int *distX, int *measureX) {
    int ms = matrixWidget->msOfXPos(mouseX);
    int tick = file()->tick(ms);
    int measureStartTick, measureEndTick;
    int measure = file()->measure(tick, &measureStartTick, &measureEndTick);

    int startX = matrixWidget->xPosOfMs(file()->msOfTick(measureStartTick));
    int endX = matrixWidget->xPosOfMs(file()->msOfTick(measureEndTick));

    int distStart = abs(mouseX - startX);
    int distEnd = abs(mouseX - endX);
    if (distEnd < distStart) {
        measure++;
        *distX = distEnd;
        *measureX = endX;
    } else {
        *distX = distStart;
        *measureX = startX;
    }
    return measure;
}

void MeasureTool::fillMeasures(QPainter *painter, int measureFrom, int measureTo) {
    // Draw selection
    if (measureFrom > measureTo) {
        int tmp = measureFrom;
        measureFrom = measureTo;
        measureTo = tmp;
    }
    int x1 = matrixWidget->xPosOfMs(file()->msOfTick(file()->startTickOfMeasure(measureFrom)));
    int x2 = matrixWidget->xPosOfMs(file()->msOfTick(file()->startTickOfMeasure(measureTo + 1)));
    
    painter->setPen(Appearance::borderColor());
    QColor selectionColor = Appearance::foregroundColor();
    selectionColor.setAlpha(100);
    painter->setBrush(selectionColor);
    painter->drawRect(x1, 0, x2 - x1, matrixWidget->height() - 1);
}
