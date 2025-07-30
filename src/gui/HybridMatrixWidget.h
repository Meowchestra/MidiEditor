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
#include <QApplication>
#include <functional>
#include "MatrixRenderData.h" // For widget switching data transfer

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
    bool canUseHardwareAcceleration() const;
    bool getHardwareAccelerationEnabled() const { return _hardwareAccelerationEnabled; }
    void setHardwareAcceleration(bool enabled);
    // REMOVED: refreshAccelerationSettings() - replaced with direct setting management
    
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

    // Missing file method
    MidiFile* midiFile();

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

protected:
    // CRITICAL: Mouse and keyboard event handling (moved from MatrixWidget)
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void enterEvent(QEnterEvent* event) override;  // Qt6 uses QEnterEvent
    void leaveEvent(QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

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

    // REMOVED: updateView() method that caused infinite loops
    // REMOVED: settingsChanged() - replaced with direct setting management
    void calcSizes();
    void takeKeyPressEvent(QKeyEvent* event);
    void takeKeyReleaseEvent(QKeyEvent* event);

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
    void connectWidgetSignals();
    void switchToSoftwareRendering();
    void switchToHardwareRendering();
    void syncViewportFromActiveWidget(); // Sync viewport from active child widget
    MatrixRenderData createRenderData(); // Create render data for widget switching
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

    // Helper methods for mouse interaction
    bool mouseInRect(const QRectF& rect);

    // Smart update methods that handle both software and hardware rendering
    void smartUpdate();      // Async update - for non-critical updates
    void smartRepaint();     // Immediate update - for real-time updates
    
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

    // Initialization state
    bool _initializing;
    bool _constructorComplete;

    // REMOVED: _cachedRenderData - using direct access pattern instead
};

#endif // HYBRIDMATRIXWIDGET_H
