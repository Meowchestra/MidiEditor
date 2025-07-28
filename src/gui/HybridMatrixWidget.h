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

class MatrixWidget;
class AcceleratedMatrixWidget;
class MidiFile;
class MidiEvent;
class GraphicObject;

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
    double lineHeight() const;
    
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

    // Additional MatrixWidget methods
    void setColorsByChannels(bool enabled);
    bool colorsByChannels() const;
    void setColorsByChannel();
    void setColorsByTracks();
    bool colorsByChannel() const;
    void setDiv(int div);
    int div() const;
    void setMeasure(int measure);
    int measure() const;
    void setTool(int tool);
    int tool() const;

    // Piano emulation
    bool getPianoEmulation() const;
    void setPianoEmulation(bool enabled);

    // View control methods
    void resetView();
    void timeMsChanged(int ms, bool ignoreLocked = false);

    // Additional MatrixWidget interface methods needed by MiscWidget and SelectionNavigator
    QList<MidiEvent*>* velocityEvents();
    QList<QPair<int, int>> divs();
    int msOfXPos(int x);
    int xPosOfMs(int ms);
    int msOfTick(int tick);
    int yPosOfLine(int line);

    // Compatibility method for widgets that need MatrixWidget interface
    MatrixWidget* getMatrixWidget() const;

public slots:
    void updateView();
    void settingsChanged();
    void calcSizes();
    void registerRelayout();
    void scrollXChanged(int scrollPositionX);
    void scrollYChanged(int scrollPositionY);
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

protected:
    void resizeEvent(QResizeEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private slots:
    void onAcceleratedWidgetFailed();
    void onViewportChanged(int startTick, int endTick);

private:
    void setupWidgets();
    void switchToSoftwareRendering();
    void switchToHardwareRendering();
    bool canUseHardwareAcceleration() const;
    void syncWidgetStates();
    void forwardSignals();
    
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
    
    // View state synchronization
    int _startTick, _endTick, _startLine, _endLine;
    double _lineHeight;
    int _lineNameWidth;

    // Additional state for MatrixWidget compatibility
    bool _colorsByChannels;
    int _div;
    int _measure;
    int _tool;

    // Performance monitoring
    QString _lastPerformanceInfo;
};

#endif // HYBRIDMATRIXWIDGET_H
