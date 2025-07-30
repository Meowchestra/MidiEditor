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

#ifndef MATRIXWIDGET_H_
#define MATRIXWIDGET_H_

#include "PaintWidget.h"
#include "MatrixRenderData.h"

#include <QApplication>
#include <QColor>
#include <QMap>
#include <QAtomicInt>
#include <QMouseEvent>
#include <QMutex>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QResizeEvent>
#include <QWidget>

class MidiFile;
class TempoChangeEvent;
class TimeSignatureEvent;
class MidiEvent;
class GraphicObject;
class NoteOnEvent;
class QSettings;
class MatrixRenderTask; // Forward declaration for friend class

class MatrixWidget : public PaintWidget {

    Q_OBJECT
    friend class HybridMatrixWidget; // Allow access to private methods
    friend class MatrixRenderTask; // Allow access to async rendering members

  public:
    MatrixWidget(QWidget* parent = 0);
    ~MatrixWidget();

    // NEW: Pure renderer interface - receives data from HybridMatrixWidget
    void setRenderData(const MatrixRenderData& data);

    // Legacy interface - will be simplified to just pass data to HybridMatrixWidget
    void setFile(MidiFile* file);
    MidiFile* midiFile();
    // Tool interface methods handled by HybridMatrixWidget

    // Coordinate methods - now use cached render data
    double lineHeight();
    int lineAtY(int y);
    int timeMsOfWidth(int w);
    bool eventInWidget(MidiEvent* event);
    int yPosOfLine(int line);
    void setScreenLocked(bool b);
    bool screenLocked();
    // Coordinate methods for tools handled by HybridMatrixWidget

    // Pure renderer - viewport managed through render data only
    int getLineNameWidth() const { return lineNameWidth; }
    QList<GraphicObject*>* getObjects() { return reinterpret_cast<QList<GraphicObject*>*>(objects); }

    // Note: color and piano emulation methods removed - handled by HybridMatrixWidget

    // Note: playNote removed - handled by HybridMatrixWidget

    int msOfTick(int tick);
    int xPosOfMs(int ms);  // Needed for rendering calculations
    int msOfXPos(int x);   // Needed for rendering calculations
    // Business logic methods handled by HybridMatrixWidget

  public slots:
    // Rendering-specific methods
    void registerRelayout();
    void scrollXChanged(int scrollPositionX);
    void scrollYChanged(int scrollPositionY);

  private:
    void calcSizes();  // Internal method for updating UI areas from render data
    void forceCompleteRedraw();

    // Async rendering methods
    void startAsyncRendering();
    void renderPixmapAsync();

  signals:
    void sizeChanged(int maxScrollTime, int maxScrollLine, int valueX,
                     int valueY);
    void objectListChanged();
    void scrollChanged(int startMs, int maxMs, int startLine, int maxLine);

  protected:
    void paintEvent(QPaintEvent* event);
    void resizeEvent(QResizeEvent* event);
    void enterEvent(QEvent* event);
    void leaveEvent(QEvent* event);
    void mouseDoubleClickEvent(QMouseEvent* event);

  private:
    // PURE RENDERER: Rendering methods that stay
    void paintChannel(QPainter* painter, int channel);
    void paintPianoKey(QPainter* painter, int number, int x, int y,
                       int width, int height);

    // Performance optimization methods
    void batchDrawEvents(QPainter* painter, const QList<MidiEvent*>& events, const QColor& color);
    void clearSoftwareCache();

    // Viewport caching for performance
    int _lastStartTick, _lastEndTick, _lastStartLineY, _lastEndLineY;
    double _lastScaleX, _lastScaleY;

    // Qt6 threading optimizations using QThreadPool
    bool _useAsyncRendering;
    QMutex _renderMutex;
    QAtomicInt _renderingInProgress; // Thread-safe flag for rendering state

    // Performance settings
    QSettings* _settings;

    // Cached render data from HybridMatrixWidget
    MatrixRenderData* _renderData;

    // Rendering-specific state only
    MidiFile* file; // Keep for signal connections
    QPixmap* pixmap; // Pixmap cache for software rendering
    QRectF ToolArea, PianoArea, TimeLineArea; // UI areas from render data
    QMap<int, QRect> pianoKeys; // Piano key areas from render data

    // Legacy state for compatibility (from render data)
    int startTick, endTick, startTimeX, endTimeX, startLineY, endLineY,
        lineNameWidth, timeHeight, msOfFirstEventInList;
    double scaleX, scaleY;
    bool screen_locked;
    bool _colorsByChannels;
    int _div;

    // Event lists from render data
    QList<MidiEvent *> *objects, *velocityObjects;
    QList<MidiEvent*>* currentTempoEvents;
    QList<TimeSignatureEvent*>* currentTimeSignatureEvents;
    QList<QPair<int, int> > currentDivs;

    // Piano emulation (kept for compatibility)
    NoteOnEvent* pianoEvent;

    //here num 0 is key E.
    static const unsigned sharp_strip_mask = (1 << 4) | (1 << 6) | (1 << 9) | (1 << 11) | (1 << 1);

};

#endif
