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

#ifndef HYBRIDMATRIXWIDGET_H
#define HYBRIDMATRIXWIDGET_H

#include <QWidget>
#include <QStackedWidget>
#include <QSettings>
#include <QEnterEvent>
#include "MatrixRenderData.h"
#include <QApplication>
#include <functional>
#include "MatrixRenderData.h"

class MatrixWidget;
class AcceleratedMatrixWidget;
class MidiFile;
class MidiEvent;
class GraphicObject;
class NoteOnEvent;
class TempoChangeEvent;
class TimeSignatureEvent;

/**
 * @brief Hybrid widget that manages both software and hardware-accelerated matrix rendering
 * 
 * This widget automatically switches between:
 * - MatrixWidget (QPainter-based, software rendering)
 * - AcceleratedMatrixWidget (OpenGL-based, hardware rendering)
 * 
 * Based on user settings and hardware capabilities.
 */
class HybridMatrixWidget : public QWidget
{
    Q_OBJECT

public:
    explicit HybridMatrixWidget(QWidget* parent = nullptr);
    ~HybridMatrixWidget();

    // MatrixWidget interface forwarding
    void setFile(MidiFile* file);
    MidiFile* midiFile() const;
    
    // View control
    void setViewport(int startTick, int endTick, int startLine, int endLine);
    void setLineHeight(double height);
    // Note: lineHeight() declared below in coordinate methods section
    
    // Hardware acceleration control
    bool isHardwareAccelerated() const;
    void setHardwareAcceleration(bool enabled);
    void refreshAccelerationSettings();
    
    // Performance information
    QString getPerformanceInfo() const;

    // Complete MatrixWidget interface forwarding
    void setScreenLocked(bool locked);
    bool screenLocked();
    int minVisibleMidiTime();
    int maxVisibleMidiTime();
    void setLineNameWidth(int width);
    int getLineNameWidth() const;
    QList<GraphicObject*>* getObjects();

    // Missing coordinate methods
    int lineAtY(int y);
    int yPosOfLine(int line);

    // Missing file method
    MidiFile* midiFile();

    // Missing piano method
    void playNote(int line);

    // Additional MatrixWidget methods
    void setColorsByChannels(bool enabled);
    bool colorsByChannels() const;
    void setColorsByChannel();
    void setColorsByTracks();
    bool colorsByChannel() const;
    int div() const;
    void setMeasure(int measure);
    int measure() const;
    void setTool(int tool);
    int tool() const;

    // Piano emulation
    bool getPianoEmulation() const;
    void setPianoEmulation(bool enabled);



    // Core business logic methods (moved from MatrixWidget)
    QList<MidiEvent*>* velocityEvents() { return _velocityObjects; }
    QList<MidiEvent*>* activeEvents() { return _objects; }
    QList<QPair<int, int>> divs() { return _currentDivs; }

    // UI Areas (moved from MatrixWidget)
    QRectF toolArea() const { return _toolArea; }
    QRectF pianoArea() const { return _pianoArea; }
    QRectF timeLineArea() const { return _timeLineArea; }
    QMap<int, QRect> pianoKeys() const { return _pianoKeys; }

    // Coordinate conversion methods (business logic)
    int msOfXPos(int x);
    int xPosOfMs(int ms);
    int msOfTick(int tick);
    int yPosOfLine(int line);
    int lineAtY(int y);
    double lineHeight() const;  // Calculated line height based on viewport
    double getStoredLineHeight() const;  // Stored line height value
    int timeMsOfWidth(int w);
    bool eventInWidget(MidiEvent* event);

    // UI methods for tools
    void setCursor(const QCursor& cursor) { QWidget::setCursor(cursor); }



    // Piano emulation
    void playNote(int note);

    // Tool interaction (business logic for both renderers)
    void handleToolPress(QMouseEvent* event);
    void handleToolRelease(QMouseEvent* event);
    void handleToolMove(QMouseEvent* event);
    void handleToolEnter();
    void handleToolExit();

    // Additional methods from MatrixWidget
    void forceCompleteRedraw();

public slots:
    // Scroll and zoom methods (must be slots for signal connections)
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
    void setDiv(int div);

    void updateView();
    void settingsChanged();
    void calcSizes();
    void scrollXChanged(int scrollPositionX);
    void scrollYChanged(int scrollPositionY);
    void takeKeyPressEvent(QKeyEvent* event);
    void takeKeyReleaseEvent(QKeyEvent* event);
    void timeMsChanged(int ms); // Player thread connection for playback scrolling

protected:
    // Event handling (business logic, then forward to renderers)
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

signals:
    void eventClicked(MidiEvent* event);
    void viewportChanged(int startTick, int endTick);
    void accelerationStatusChanged(bool isAccelerated);

    // MatrixWidget signals forwarding
    void sizeChanged(int maxScrollTime, int maxScrollLine, int valueX, int valueY);
    void objectListChanged();
    void scrollChanged(int startMs, int maxMs, int startLine, int maxLine);

// Duplicate protected section removed

private slots:
    void onAcceleratedWidgetFailed();
    void onViewportChanged(int startTick, int endTick);

private:
    void setupWidgets();
    void switchToSoftwareRendering();
    void switchToHardwareRendering();
    bool canUseHardwareAcceleration() const;
    void syncActiveWidget();
    void forwardSignals();

    // Business logic helper methods (moved from MatrixWidget)
    void updateEventLists();
    void updateEventListsForChannel(int channel);
    void processEventForDisplay(MidiEvent* event);
    void updateDivisions();
    void updateTempoEvents();
    void updateTimeSignatureEvents();
    void pianoEmulator(QKeyEvent* event);
    void calculateViewportBounds();
    void calculatePianoKeys();
    void emitSizeChanged();
    void timeMsChangedInternal(int ms, bool ignoreLocked); // Internal implementation
    
    // Widget management
    QStackedWidget* _stackedWidget;
    MatrixWidget* _softwareWidget;
    AcceleratedMatrixWidget* _hardwareWidget;


    
    // State
    QSettings* _settings;
    bool _hardwareAccelerationEnabled;
    bool _hardwareAccelerationAvailable;
    bool _currentlyUsingHardware;
    MidiFile* _currentFile;
    
    // Core business logic state (moved from MatrixWidget)
    int _startTick, _endTick, _startLine, _endLine;
    int _startTimeX, _endTimeX, _startLineY, _endLineY;
    int _timeHeight, _msOfFirstEventInList;
    double _lineHeight;
    int _lineNameWidth;
    double _scaleX, _scaleY;
    bool _screenLocked;

    // Additional state for MatrixWidget compatibility
    bool _colorsByChannels;
    int _div;
    int _measure;
    int _tool;
    bool _pianoEmulationEnabled;

    // Mouse state for rendering
    int _mouseX, _mouseY;
    bool _mouseOver;

    // Event management
    QList<MidiEvent*>* _objects;
    QList<MidiEvent*>* _velocityObjects;
    QList<MidiEvent*>* _currentTempoEvents;
    QList<class TimeSignatureEvent*>* _currentTimeSignatureEvents;
    QList<QPair<int, int>> _currentDivs;

    // Piano emulation
    class NoteOnEvent* _pianoEvent;
    QMap<int, QRect> _pianoKeys;

    // UI Areas (moved from MatrixWidget)
    QRectF _toolArea, _pianoArea, _timeLineArea;

    // Performance monitoring
    QString _lastPerformanceInfo;

    // Cached render data to avoid recreation
    MatrixRenderData _cachedRenderData;
};

#endif // HYBRIDMATRIXWIDGET_H
