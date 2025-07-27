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

#include <QApplication>
#include <QColor>
#include <QElapsedTimer>
#include <QFuture>
#include <QMap>
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

class MatrixWidget : public PaintWidget {

    Q_OBJECT

  public:
    MatrixWidget(QWidget* parent = 0);
    void setFile(MidiFile* file);
    MidiFile* midiFile();
    QList<MidiEvent*>* activeEvents();
    QList<MidiEvent*>* velocityEvents();

    double lineHeight();
    int lineAtY(int y);
    int msOfXPos(int x);
    int timeMsOfWidth(int w);
    bool eventInWidget(MidiEvent* event);
    int yPosOfLine(int line);
    void setScreenLocked(bool b);
    bool screenLocked();
    int minVisibleMidiTime();
    int maxVisibleMidiTime();

    void setColorsByChannel();
    void setColorsByTracks();
    bool colorsByChannel();

    bool getPianoEmulation();
    void setPianoEmulation(bool);

    void playNote(int);

    int msOfTick(int tick);
    int xPosOfMs(int ms);
    QList<QPair<int, int> > divs();

  public slots:
    void scrollXChanged(int scrollPositionX);
    void scrollYChanged(int scrollPositionY);
    void zoomHorIn();
    void zoomHorOut();
    void zoomVerIn();
    void zoomVerOut();
    void zoomStd();
    void resetView();
    void timeMsChanged(int ms, bool ignoreLocked = false);
    void registerRelayout();
    void calcSizes();
    void takeKeyPressEvent(QKeyEvent* event);
    void takeKeyReleaseEvent(QKeyEvent* event);
    void setDiv(int div);
    int div();
    void forceCompleteRedraw(); // Force complete redraw for theme changes

  signals:
    void sizeChanged(int maxScrollTime, int maxScrollLine, int valueX,
                     int valueY);
    void objectListChanged();
    void scrollChanged(int startMs, int maxMs, int startLine, int maxLine);

  protected:
    void paintEvent(QPaintEvent* event);
    void mouseMoveEvent(QMouseEvent* event);
    void resizeEvent(QResizeEvent* event);
    void enterEvent(QEvent* event);
    void leaveEvent(QEvent* event);
    void mousePressEvent(QMouseEvent* event);
    void mouseDoubleClickEvent(QMouseEvent* event);
    void mouseReleaseEvent(QMouseEvent* event);
    void keyPressEvent(QKeyEvent* e);
    void keyReleaseEvent(QKeyEvent* event);
    void wheelEvent(QWheelEvent* event);

  private:
    void pianoEmulator(QKeyEvent*);
    void paintChannel(QPainter* painter, int channel);
    void paintPianoKey(QPainter* painter, int number, int x, int y,
                       int width, int height);

    // Performance optimization methods
    enum RenderTier {
        RENDER_FULL_DETAIL = 0,    // Full detailed rendering
        RENDER_MEDIUM_DETAIL = 1,  // Simplified rendering for medium zoom
        RENDER_LOW_DETAIL = 2      // LOD rendering for far zoom
    };

    RenderTier getRenderTier() const;
    bool shouldUseLevelOfDetail() const;
    void paintChannelLOD(QPainter* painter, int channel);
    void paintChannelMediumDetail(QPainter* painter, int channel);
    void paintEventLOD(QPainter* painter, MidiEvent* event, const QColor& color);
    void paintEventMediumDetail(QPainter* painter, MidiEvent* event, const QColor& color);
    void batchDrawEvents(QPainter* painter, const QList<MidiEvent*>& events, const QColor& color);

    // Constants for performance tuning (Qt6 optimized - higher thresholds due to better performance)
    static const int LOD_PIXEL_THRESHOLD = 20; // Use LOD when less than this many pixels per tick
    static const int LOD_TIME_THRESHOLD = 100000; // Use LOD when showing more than this many ticks (doubled for Qt6)
    static const int MEDIUM_DETAIL_TIME_THRESHOLD = 50000; // Use medium detail threshold (doubled for Qt6)
    static const int MAX_EVENTS_PER_FRAME = 10000; // Adaptive optimization trigger (doubled for Qt6)

    // Performance monitoring (debug builds only)
    #ifdef QT_DEBUG
    mutable int _lastRenderEventCount;
    mutable bool _lastUsedLOD;
    #endif

    // Viewport caching for performance
    int _lastStartTick, _lastEndTick, _lastStartLineY, _lastEndLineY;
    double _lastScaleX, _lastScaleY;

    // Performance monitoring
    mutable QElapsedTimer _renderTimer;
    mutable bool _showingPerformanceWarning;

    // Qt6 threading optimizations
    bool _useAsyncRendering;
    QFuture<void> _renderFuture;
    QMutex _renderMutex;

    bool _isPianoEmulationEnabled = false;

    int startTick, endTick, startTimeX, endTimeX, startLineY, endLineY,
        lineNameWidth, timeHeight, msOfFirstEventInList;
    double scaleX, scaleY;
    MidiFile* file;

    QRectF ToolArea, PianoArea, TimeLineArea;
    bool screen_locked;
    // pixmap is the painted widget (without tools and cursorLines).
    // it will be zero if it needs to be repainted
    QPixmap* pixmap;

    // saves all TempoEvents from one before the first shown tick to the
    // last in the window
    QList<MidiEvent*>* currentTempoEvents;
    QList<TimeSignatureEvent*>* currentTimeSignatureEvents;

    // All Events to show in the velocityWidget are saved in velocityObjects
    QList<MidiEvent *> *objects, *velocityObjects;
    QList<QPair<int, int> > currentDivs;

    // To play the pianokeys, there is one NoteOnEvent
    NoteOnEvent* pianoEvent;

    bool _colorsByChannels;
    int _div;

    QMap<int, QRect> pianoKeys;

    //here num 0 is key E.
    static const unsigned sharp_strip_mask = (1 << 4) | (1 << 6) | (1 << 9) | (1 << 11) | (1 << 1);

};

#endif
