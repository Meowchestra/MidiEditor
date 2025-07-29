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

#include "HybridMatrixWidget.h"
#include "MatrixWidget.h"
#include "AcceleratedMatrixWidget.h"
#include "../midi/MidiFile.h"
#include "../midi/MidiPlayer.h"
#include "../midi/MidiOutput.h"
#include "../midi/PlayerThread.h"
#include "../midi/MidiChannel.h"
#include "../gui/GraphicObject.h"
#include "../MidiEvent/NoteOnEvent.h"
#include "../MidiEvent/OffEvent.h"
#include "../MidiEvent/TempoChangeEvent.h"
#include "../MidiEvent/TimeSignatureEvent.h"
#include "../tool/Tool.h"
#include "../tool/EditorTool.h"

#include <QStackedWidget>
#include <QVBoxLayout>

#include <QSettings>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QApplication>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QEnterEvent>
#include <QDebug>
#include <QtMath>

// Business logic constants (shared by both renderers) - from old MatrixWidget
#define NUM_LINES 139
#define PIXEL_PER_S 100
#define PIXEL_PER_LINE 11
#define PIXEL_PER_EVENT 15

HybridMatrixWidget::HybridMatrixWidget(QWidget* parent)
    : QWidget(parent)
    , _stackedWidget(nullptr)
    , _softwareWidget(nullptr)
    , _hardwareWidget(nullptr)
    , _settings(nullptr)
    , _hardwareAccelerationEnabled(false)
    , _hardwareAccelerationAvailable(false)
    , _currentlyUsingHardware(false)
    , _currentFile(nullptr)
    , _startTick(0)
    , _endTick(1000)
    , _startLine(0)
    , _endLine(127)
    , _startTimeX(0)
    , _endTimeX(0)
    , _startLineY(50)
    , _endLineY(0)
    , _timeHeight(50)
    , _msOfFirstEventInList(0)
    , _lineHeight(10.0)
    , _lineNameWidth(110)
    , _scaleX(1)
    , _scaleY(1)
    , _screenLocked(false)
    , _colorsByChannels(false)
    , _div(2)
    , _measure(0)
    , _tool(0)
    , _pianoEmulationEnabled(false)
    , _mouseX(0)
    , _mouseY(0)
    , _mouseOver(false)
    , _objects(new QList<MidiEvent*>())
    , _velocityObjects(new QList<MidiEvent*>())
    , _currentTempoEvents(new QList<MidiEvent*>())
    , _currentTimeSignatureEvents(new QList<TimeSignatureEvent*>())
    , _pianoEvent(new NoteOnEvent(0, 100, 0, 0))
    , _currentDivs()  // Initialize empty list
{
    // Initialize settings
    _settings = new QSettings(QString("MidiEditor"), QString("NONE"));

    // Configure widget properties (same as MatrixWidget)
    setMouseTracking(true);
    setFocusPolicy(Qt::ClickFocus);

    // CRITICAL: Set this widget as the matrix widget for tools
    EditorTool::setMatrixWidget(this);

    // Initialize business logic (now just calculates viewport since data is already allocated)
    calculateViewportBounds();

    setupWidgets();
    refreshAccelerationSettings();

    // CRITICAL: Sync widgets with initial data
    syncActiveWidget();

    // Connect to player thread for playback scrolling (critical for playback!)
    connect(MidiPlayer::playerThread(), SIGNAL(timeMsChanged(int)),
            this, SLOT(timeMsChanged(int)));

    // Set this HybridMatrixWidget as the matrix widget for editor tools
    // Tools now work directly with the business logic hub
    EditorTool::setMatrixWidget(this);
}

HybridMatrixWidget::~HybridMatrixWidget() {
    // Clear tool reference to avoid dangling pointer
    EditorTool::setMatrixWidget(nullptr);

    // Disconnect from player thread
    disconnect(MidiPlayer::playerThread(), nullptr, this, nullptr);

    delete _settings;
    delete _objects;
    delete _velocityObjects;
    delete _currentTempoEvents;
    delete _currentTimeSignatureEvents;
    delete _pianoEvent;

    // Note: _stackedWidget and child widgets are automatically cleaned up by Qt parent-child system
}

void HybridMatrixWidget::setupWidgets() {
    // Create stacked widget to hold both rendering widgets
    _stackedWidget = new QStackedWidget(this);
    
    // Create software rendering widget (always available)
    _softwareWidget = new MatrixWidget(this);
    _stackedWidget->addWidget(_softwareWidget);
    
    // Try to create hardware rendering widget
    _hardwareWidget = new AcceleratedMatrixWidget(this);
    _stackedWidget->addWidget(_hardwareWidget);

    // Check if hardware acceleration actually initialized
    if (_hardwareWidget->isHardwareAccelerated()) {
        _hardwareAccelerationAvailable = true;
        qDebug() << "HybridMatrixWidget: Hardware acceleration available";

        // Connect hardware widget signals
        connect(_hardwareWidget, &AcceleratedMatrixWidget::fileChanged,
                this, &HybridMatrixWidget::updateView);
        connect(_hardwareWidget, &AcceleratedMatrixWidget::viewportChanged,
                this, &HybridMatrixWidget::onViewportChanged);
        connect(_hardwareWidget, &AcceleratedMatrixWidget::hardwareAccelerationFailed,
                this, &HybridMatrixWidget::onAcceleratedWidgetFailed);
    } else {
        qWarning() << "HybridMatrixWidget: Hardware acceleration failed to initialize";
        _hardwareAccelerationAvailable = false;
        // Keep the widget but it will show empty/clear background
    }
    
    // Connect software widget signals (using available MatrixWidget signals)
    connect(_softwareWidget, &MatrixWidget::objectListChanged, this, &HybridMatrixWidget::updateView);
    connect(_softwareWidget, &MatrixWidget::objectListChanged, this, &HybridMatrixWidget::objectListChanged);
    connect(_softwareWidget, &MatrixWidget::sizeChanged, this, [this]() { updateView(); });
    connect(_softwareWidget, &MatrixWidget::sizeChanged, this, &HybridMatrixWidget::sizeChanged);
    connect(_softwareWidget, &MatrixWidget::scrollChanged, this, &HybridMatrixWidget::scrollChanged);
    
    // Setup layout
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(_stackedWidget);
    setLayout(layout);
    
    // Try to start with hardware acceleration (default enabled)
    refreshAccelerationSettings();
}

void HybridMatrixWidget::refreshAccelerationSettings() {
    // Default to software rendering for stability
    bool enabled = _settings->value("rendering/hardware_acceleration", false).toBool();
    setHardwareAcceleration(enabled);
}

void HybridMatrixWidget::setHardwareAcceleration(bool enabled) {
    _hardwareAccelerationEnabled = enabled;
    
    if (enabled && canUseHardwareAcceleration()) {
        switchToHardwareRendering();
    } else {
        switchToSoftwareRendering();
    }
}

bool HybridMatrixWidget::canUseHardwareAcceleration() const {
    if (!_hardwareAccelerationAvailable || !_hardwareWidget) {
        return false;
    }

    // Check if hardware acceleration is available and working
    if (!_hardwareWidget->isHardwareAccelerated()) {
        return false;
    }

    return true;
}

void HybridMatrixWidget::switchToSoftwareRendering() {
    if (!_softwareWidget) {
        qWarning() << "HybridMatrixWidget: Software widget not available";
        return;
    }

    if (_currentlyUsingHardware) {
        qDebug() << "HybridMatrixWidget: Switching to software rendering";
        _currentlyUsingHardware = false;
        emit accelerationStatusChanged(false);

        // CRITICAL: Clear hardware cache when switching away from it
        if (_hardwareWidget) {
            _hardwareWidget->clearGPUCache();
        }
    }

    _stackedWidget->setCurrentWidget(_softwareWidget);

    // CRITICAL: Clear software cache to ensure fresh rendering
    _softwareWidget->clearSoftwareCache();

    // Only sync the active widget (software)
    syncActiveWidget();

    _lastPerformanceInfo = "Software rendering (QPainter)";
}

void HybridMatrixWidget::switchToHardwareRendering() {
    if (!_hardwareWidget || !canUseHardwareAcceleration()) {
        switchToSoftwareRendering();
        return;
    }

    if (!_currentlyUsingHardware) {
        qDebug() << "HybridMatrixWidget: Switching to hardware rendering";
        _currentlyUsingHardware = true;
        emit accelerationStatusChanged(true);

        // CRITICAL: Clear software cache when switching away from it
        if (_softwareWidget) {
            _softwareWidget->clearSoftwareCache();
        }
    }

    _stackedWidget->setCurrentWidget(_hardwareWidget);

    // CRITICAL: Clear GPU cache to ensure fresh rendering
    _hardwareWidget->clearGPUCache();

    // Only sync the active widget (hardware)
    syncActiveWidget();

    // Get actual backend name from AcceleratedMatrixWidget
    QString backendName = _hardwareWidget->backendName();
    if (!backendName.isEmpty()) {
        _lastPerformanceInfo = QString("Hardware rendering (%1)").arg(backendName);
    } else {
        #ifdef Q_OS_WIN
            _lastPerformanceInfo = "Hardware rendering (D3D12/D3D11/Vulkan/OpenGL)";
        #else
            _lastPerformanceInfo = "Hardware rendering (Vulkan/OpenGL)";
        #endif
    }
}

void HybridMatrixWidget::syncActiveWidget() {
    // Update cached render data structure with all necessary information
    _cachedRenderData.startTick = _startTick;
    _cachedRenderData.endTick = _endTick;
    _cachedRenderData.startLine = _startLine;
    _cachedRenderData.endLine = _endLine;
    _cachedRenderData.startTimeX = _startTimeX;
    _cachedRenderData.endTimeX = _endTimeX;
    _cachedRenderData.startLineY = _startLineY;
    _cachedRenderData.endLineY = _endLineY;
    _cachedRenderData.timeHeight = _timeHeight;
    _cachedRenderData.lineNameWidth = _lineNameWidth;
    _cachedRenderData.scaleX = _scaleX;
    _cachedRenderData.scaleY = _scaleY;
    _cachedRenderData.lineHeight = _lineHeight;  // Use stored value for render data
    _cachedRenderData.objects = _objects;
    _cachedRenderData.velocityObjects = _velocityObjects;
    _cachedRenderData.tempoEvents = _currentTempoEvents;
    _cachedRenderData.timeSignatureEvents = _currentTimeSignatureEvents;
    _cachedRenderData.divs = _currentDivs;
    _cachedRenderData.msOfFirstEventInList = _msOfFirstEventInList;
    _cachedRenderData.colorsByChannels = _colorsByChannels;
    _cachedRenderData.screenLocked = _screenLocked;
    _cachedRenderData.pianoEmulationEnabled = _pianoEmulationEnabled;
    _cachedRenderData.div = _div;
    _cachedRenderData.measure = _measure;
    _cachedRenderData.tool = _tool;
    _cachedRenderData.toolArea = _toolArea;
    _cachedRenderData.pianoArea = _pianoArea;
    _cachedRenderData.timeLineArea = _timeLineArea;

    // Populate shared constants
    _cachedRenderData.numLines = NUM_LINES;
    _cachedRenderData.pixelPerS = PIXEL_PER_S;
    _cachedRenderData.pixelPerLine = PIXEL_PER_LINE;
    _cachedRenderData.pixelPerEvent = PIXEL_PER_EVENT;
    _cachedRenderData.pianoKeys = _pianoKeys;

    // Populate mouse state for rendering
    _cachedRenderData.mouseX = _mouseX;
    _cachedRenderData.mouseY = _mouseY;
    _cachedRenderData.mouseOver = _mouseOver;

    // Populate file reference
    _cachedRenderData.file = _currentFile;

    // Only sync the currently active widget
    if (_currentlyUsingHardware && _hardwareWidget) {
        _hardwareWidget->setRenderData(_cachedRenderData);
        if (_currentFile) {
            _hardwareWidget->setFile(_currentFile);
        }
    } else if (_softwareWidget) {
        _softwareWidget->setRenderData(_cachedRenderData);
        if (_currentFile) {
            _softwareWidget->setFile(_currentFile);
        }
    }
}

void HybridMatrixWidget::setFile(MidiFile* file) {
    // Disconnect from previous file's protocol if any
    if (_currentFile && _currentFile->protocol()) {
        disconnect(_currentFile->protocol(), nullptr, this, nullptr);
    }

    _currentFile = file;

    if (file) {
        // Initialize viewport for new file
        _startTimeX = 0;
        _endTimeX = qMin(10000, file->maxTime()); // Show first 10 seconds or entire file
        _startLineY = 50; // Center around middle C
        _endLineY = 77;

        // CRITICAL: Connect to protocol signals like old MatrixWidget
        if (file->protocol()) {
            connect(file->protocol(), SIGNAL(actionFinished()), this, SLOT(registerRelayout()));
            connect(file->protocol(), SIGNAL(actionFinished()), this, SLOT(updateView()));
        }

        calculateViewportBounds();
        calcSizes();
    }

    // File is set through syncActiveWidget() - no need for duplicate calls
    syncActiveWidget();
    updateView();
}

MidiFile* HybridMatrixWidget::midiFile() const {
    return _currentFile;
}

void HybridMatrixWidget::setViewport(int startTick, int endTick, int startLine, int endLine) {
    _startTick = startTick;
    _endTick = endTick;
    _startLine = startLine;
    _endLine = endLine;

    // Viewport data is passed through syncActiveWidget() -> setRenderData()
    // No need to call setViewport() directly on widgets
    syncActiveWidget();
    emit viewportChanged(startTick, endTick);
}

void HybridMatrixWidget::setLineHeight(double height) {
    _lineHeight = height;
    // Line height is passed through render data - no need to call widgets directly
    syncActiveWidget();
}

double HybridMatrixWidget::getStoredLineHeight() const {
    return _lineHeight;
}

bool HybridMatrixWidget::isHardwareAccelerated() const {
    return _currentlyUsingHardware;
}

QString HybridMatrixWidget::getPerformanceInfo() const {
    return _lastPerformanceInfo;
}

void HybridMatrixWidget::updateView() {
    // CRITICAL: Always sync active widget before updating view
    syncActiveWidget();

    if (_currentlyUsingHardware && _hardwareWidget) {
        _hardwareWidget->updateView();
    } else if (_softwareWidget) {
        _softwareWidget->update();
    }
}

void HybridMatrixWidget::settingsChanged() {
    refreshAccelerationSettings();
}

void HybridMatrixWidget::onAcceleratedWidgetFailed() {
    qWarning() << "HybridMatrixWidget: Hardware acceleration failed, falling back to software rendering";
    switchToSoftwareRendering();
}

void HybridMatrixWidget::onViewportChanged(int startTick, int endTick) {
    emit viewportChanged(startTick, endTick);
}



void HybridMatrixWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    calcSizes();
}

// Event handling optimized for business logic

// Forward all MatrixWidget methods for complete interface compatibility
void HybridMatrixWidget::setScreenLocked(bool locked) {
    _screenLocked = locked;
    // Screen lock state is passed through render data - no need to call widgets directly
    syncActiveWidget();
}

bool HybridMatrixWidget::screenLocked() {
    return _screenLocked;
}

int HybridMatrixWidget::minVisibleMidiTime() {
    return _startTick;
}

int HybridMatrixWidget::maxVisibleMidiTime() {
    return _endTick;
}

void HybridMatrixWidget::setLineNameWidth(int width) {
    _lineNameWidth = width;
    // Line name width is passed through render data - no need to call widgets directly
    syncActiveWidget();
}

int HybridMatrixWidget::getLineNameWidth() const {
    return _lineNameWidth;
}

QList<GraphicObject*>* HybridMatrixWidget::getObjects() {
    // Always return from software widget as it handles selection properly
    return _softwareWidget->getObjects();
}

void HybridMatrixWidget::setColorsByChannels(bool enabled) {
    _colorsByChannels = enabled;
    // Color information is passed through render data - no need to call widgets directly
    syncActiveWidget();
}

bool HybridMatrixWidget::colorsByChannels() const {
    return _colorsByChannels;
}



int HybridMatrixWidget::div() const {
    return _div;
}

void HybridMatrixWidget::setMeasure(int measure) {
    _measure = measure;
    // MatrixWidget doesn't have setMeasure method - store locally
}

int HybridMatrixWidget::measure() const {
    return _measure;
}

void HybridMatrixWidget::setTool(int tool) {
    _tool = tool;
    // MatrixWidget doesn't have setTool method - store locally
}

int HybridMatrixWidget::tool() const {
    return _tool;
}

void HybridMatrixWidget::setColorsByChannel() {
    setColorsByChannels(true);
}

void HybridMatrixWidget::setColorsByTracks() {
    setColorsByChannels(false);
}

bool HybridMatrixWidget::colorsByChannel() const {
    return _colorsByChannels;
}

bool HybridMatrixWidget::getPianoEmulation() const {
    return _pianoEmulationEnabled;
}

void HybridMatrixWidget::setPianoEmulation(bool enabled) {
    _pianoEmulationEnabled = enabled;
    // Piano emulation is handled by HybridMatrixWidget business logic
    // State is passed through render data - no need to call widgets directly
    syncActiveWidget();
}

void HybridMatrixWidget::resetView() {
    if (!_currentFile) return;

    // Reset zoom to default
    _scaleX = 1.0;
    _scaleY = 1.0;

    // Reset horizontal scroll to beginning
    _startTimeX = 0;

    // Reset vertical scroll to roughly center on Middle C (line 60)
    _startLineY = 50;

    // Reset cursor and pause positions to beginning
    _currentFile->setCursorTick(0);
    _currentFile->setPauseTick(-1);

    // Recalculate sizes and update display
    calcSizes();
    updateView();
}

void HybridMatrixWidget::timeMsChanged(int ms) {
    // Player thread slot - always respect screen lock
    timeMsChangedInternal(ms, false);
}

void HybridMatrixWidget::timeMsChanged(int ms, bool ignoreLocked) {
    // Public interface for manual calls
    timeMsChangedInternal(ms, ignoreLocked);
}

void HybridMatrixWidget::timeMsChangedInternal(int ms, bool ignoreLocked) {
    if (!_currentFile) return;

    int x = xPosOfMs(ms);

    if ((!_screenLocked || ignoreLocked) && (x < _lineNameWidth || ms < _startTimeX || ms > _endTimeX || x > width() - 100)) {
        // Return if the last tick is already shown
        if (_currentFile->maxTime() <= _endTimeX && ms >= _startTimeX) {
            updateView();
            return;
        }

        // Sets the new position and updates
        emit scrollChanged(ms, (_currentFile->maxTime() - _endTimeX + _startTimeX), _startLineY,
                          128 - (_endLineY - _startLineY)); // NUM_LINES = 128
    } else {
        updateView();
    }
}

void HybridMatrixWidget::calcSizes() {
    if (!_currentFile || _scaleX <= 0 || _scaleY <= 0) return;

    // Calculate end times based on current scale and widget size
    _endTimeX = _startTimeX + ((width() - _lineNameWidth) * 1000) / (PIXEL_PER_S * _scaleX);
    _endLineY = _startLineY + (height() - _timeHeight) / (_scaleY * PIXEL_PER_LINE);

    // Clamp to valid ranges
    if (_endTimeX > _currentFile->maxTime()) {
        _endTimeX = _currentFile->maxTime();
    }
    if (_endLineY > 128) {
        _endLineY = 128;
    }

    // Calculate UI areas (moved from MatrixWidget)
    _toolArea = QRectF(_lineNameWidth, _timeHeight, width() - _lineNameWidth, height() - _timeHeight);
    _pianoArea = QRectF(0, _timeHeight, _lineNameWidth, height() - _timeHeight);
    _timeLineArea = QRectF(_lineNameWidth, 0, width() - _lineNameWidth, _timeHeight);

    // Calculate piano keys (moved from MatrixWidget)
    calculatePianoKeys();

    calculateViewportBounds();
    syncActiveWidget();
    emitSizeChanged();
}

void HybridMatrixWidget::registerRelayout() {
    // Tell active widget to invalidate its cached rendering
    if (_currentlyUsingHardware && _hardwareWidget) {
        // Hardware widget doesn't need explicit relayout - it updates on next render
    } else if (_softwareWidget) {
        _softwareWidget->registerRelayout();
    }
    updateView();
}

void HybridMatrixWidget::takeKeyPressEvent(QKeyEvent* event) {
    // Handle tool key events
    if (Tool::currentTool()) {
        if (Tool::currentTool()->pressKey(event->key())) {
            updateView();
        }
    }

    // Handle piano emulation
    pianoEmulator(event);
}

void HybridMatrixWidget::takeKeyReleaseEvent(QKeyEvent* event) {
    // Handle tool key events
    if (Tool::currentTool()) {
        if (Tool::currentTool()->releaseKey(event->key())) {
            updateView();
        }
    }
}

// Tool interface methods (moved from MatrixWidget)
// Note: These methods are thread-safe by using atomic reads of viewport state
int HybridMatrixWidget::msOfXPos(int x) {
    // Make atomic copies to avoid race conditions
    int startTimeX = _startTimeX;
    int endTimeX = _endTimeX;
    int lineNameWidth = _lineNameWidth;
    int widgetWidth = width();

    if (widgetWidth <= lineNameWidth) return startTimeX;
    return startTimeX + ((x - lineNameWidth) * (endTimeX - startTimeX)) / (widgetWidth - lineNameWidth);
}

int HybridMatrixWidget::xPosOfMs(int ms) {
    // Make atomic copies to avoid race conditions
    int startTimeX = _startTimeX;
    int endTimeX = _endTimeX;
    int lineNameWidth = _lineNameWidth;
    int widgetWidth = width();

    if (endTimeX <= startTimeX || widgetWidth <= lineNameWidth) return lineNameWidth;
    return lineNameWidth + (ms - startTimeX) * (widgetWidth - lineNameWidth) / (endTimeX - startTimeX);
}

// Scroll methods (business logic implementation)
void HybridMatrixWidget::scrollXChanged(int scrollPositionX) {
    if (!_currentFile || _scaleX <= 0) return;

    _startTimeX = scrollPositionX;
    _endTimeX = _startTimeX + ((width() - _lineNameWidth) * 1000) / (PIXEL_PER_S * _scaleX);

    // more space than needed: scale x
    if (_endTimeX - _startTimeX > _currentFile->maxTime()) {
        _endTimeX = _currentFile->maxTime();
        _startTimeX = 0;
    } else if (_startTimeX < 0) {
        _endTimeX -= _startTimeX;
        _startTimeX = 0;
    } else if (_endTimeX > _currentFile->maxTime()) {
        _startTimeX += _currentFile->maxTime() - _endTimeX;
        _endTimeX = _currentFile->maxTime();
    }

    calculateViewportBounds();
    syncActiveWidget();
    updateView();

    // Emit scroll changed signal like old MatrixWidget
    if (_currentFile) {
        int maxScrollTime = _currentFile->maxTime() - (_endTimeX - _startTimeX);
        int maxScrollLine = 128 - (_endLineY - _startLineY);
        emit scrollChanged(_startTimeX, maxScrollTime, _startLineY, maxScrollLine);
    }
}

void HybridMatrixWidget::scrollYChanged(int scrollPositionY) {
    if (!_currentFile || _scaleY <= 0) return;

    _startLineY = scrollPositionY;

    double space = height() - _timeHeight;
    double lineSpace = _scaleY * PIXEL_PER_LINE;
    if (lineSpace <= 0) return; // Prevent division by zero
    double linesInWidget = space / lineSpace;
    _endLineY = _startLineY + linesInWidget;

    if (_endLineY > 128) { // MIDI has 128 lines (0-127)
        int d = _endLineY - 128;
        _endLineY = 128;
        _startLineY -= d;
        if (_startLineY < 0) {
            _startLineY = 0;
        }
    }

    calculateViewportBounds();
    syncActiveWidget();
    updateView();

    // Emit scroll changed signal like old MatrixWidget
    if (_currentFile) {
        int maxScrollTime = _currentFile->maxTime() - (_endTimeX - _startTimeX);
        int maxScrollLine = 128 - (_endLineY - _startLineY);
        emit scrollChanged(_startTimeX, maxScrollTime, _startLineY, maxScrollLine);
    }
}

// Business logic implementation

int HybridMatrixWidget::yPosOfLine(int line) {
    // Make atomic copies to avoid race conditions
    int timeHeight = _timeHeight;
    int startLineY = _startLineY;
    double lh = lineHeight();

    return timeHeight + (line - startLineY) * lh;
}

int HybridMatrixWidget::lineAtY(int y) {
    // Make atomic copies to avoid race conditions
    int timeHeight = _timeHeight;
    int startLineY = _startLineY;
    double lh = lineHeight();

    if (lh == 0) return startLineY;
    return startLineY + (y - timeHeight) / lh;
}

double HybridMatrixWidget::lineHeight() const {
    if (_endLineY - _startLineY == 0) return 0;
    return (double)(height() - _timeHeight) / (double)(_endLineY - _startLineY);
}

int HybridMatrixWidget::msOfTick(int tick) {
    if (!_currentFile) return 0;
    return _currentFile->msOfTick(tick, _currentTempoEvents, _msOfFirstEventInList);
}

int HybridMatrixWidget::timeMsOfWidth(int w) {
    if (width() <= _lineNameWidth) return 0;
    return (w * (_endTimeX - _startTimeX)) / (width() - _lineNameWidth);
}



// Missing visibility methods
int HybridMatrixWidget::minVisibleMidiTime() {
    return _startTick;
}

int HybridMatrixWidget::maxVisibleMidiTime() {
    return _endTick;
}

// Missing file method
MidiFile* HybridMatrixWidget::midiFile() {
    return _currentFile;
}

// Missing piano method
void HybridMatrixWidget::playNote(int note) {
    if (_pianoEvent) {
        _pianoEvent->setNote(note);
        _pianoEvent->setChannel(MidiOutput::standardChannel(), false);
        MidiPlayer::play(_pianoEvent);
    }
}

bool HybridMatrixWidget::eventInWidget(MidiEvent* event) {
    if (!event || !_currentFile) return false;

    NoteOnEvent* on = dynamic_cast<NoteOnEvent*>(event);
    OffEvent* off = dynamic_cast<OffEvent*>(event);
    if (on) {
        off = on->offEvent();
    } else if (off) {
        on = dynamic_cast<NoteOnEvent*>(off->onEvent());
    }

    if (on && off) {
        int offLine = off->line();
        int offTick = off->midiTime();
        bool offIn = offLine >= _startLineY && offLine <= _endLineY && offTick >= _startTick && offTick <= _endTick;

        int onLine = on->line();
        int onTick = on->midiTime();
        bool onIn = onLine >= _startLineY && onLine <= _endLineY && onTick >= _startTick && onTick <= _endTick;

        // Check if note line is visible (same line for both on and off events)
        bool lineVisible = (onLine >= _startLineY && onLine <= _endLineY);

        // Check all possible time overlap scenarios:
        // 1. Note starts before viewport and ends after viewport (spans completely)
        // 2. Note starts before viewport and ends inside viewport
        // 3. Note starts inside viewport and ends after viewport
        // 4. Note starts and ends inside viewport
        // All of these can be captured by: note starts before viewport ends AND note ends after viewport starts
        bool timeOverlaps = (onTick < _endTick && offTick > _startTick);

        // Show note if:
        // 1. Either start or end is fully visible (both time and line), OR
        // 2. Note line is visible AND note overlaps viewport in time
        bool shouldShow = offIn || onIn || (lineVisible && timeOverlaps);

        off->setShown(shouldShow);
        on->setShown(shouldShow);

        return shouldShow;

    } else {
        int line = event->line();
        int tick = event->midiTime();
        bool shown = line >= _startLineY && line <= _endLineY && tick >= _startTick && tick <= _endTick;
        event->setShown(shown);

        return shown;
    }
}

void HybridMatrixWidget::calculateViewportBounds() {
    if (!_currentFile) return;

    // Calculate line bounds
    _startLine = _startLineY;
    _endLine = _endLineY;

    // Calculate time bounds using the same method as old MatrixWidget
    // This also populates tempo events and sets _msOfFirstEventInList
    _currentTempoEvents->clear();
    _startTick = _currentFile->tick(_startTimeX, _endTimeX, &_currentTempoEvents,
                                   &_endTick, &_msOfFirstEventInList);

    // Update other event lists
    updateEventLists();
    updateDivisions();
    updateTimeSignatureEvents();
}

void HybridMatrixWidget::updateEventLists() {
    if (!_currentFile || !_objects || !_velocityObjects) return;

    _objects->clear();
    _velocityObjects->clear();

    // Process all visible channels
    for (int channel = 0; channel < 16; channel++) {
        if (!_currentFile->channel(channel)->visible()) continue;
        updateEventListsForChannel(channel);
    }

    // Emit signal when object lists are updated (like old MatrixWidget)
    emit objectListChanged();
}

void HybridMatrixWidget::updateEventListsForChannel(int channel) {
    QMultiMap<int, MidiEvent*>* map = _currentFile->channel(channel)->eventMap();

    // Early exit for empty channels
    if (!map || map->isEmpty()) {
        return;
    }

    // Two-pass approach for performance while maintaining correctness:
    // Pass 1: Process events that start within an extended viewport (for performance)
    // Pass 2: For note events, check for long notes that start before extended viewport

    // Calculate extended search range - look back/forward based on typical note lengths
    int maxNoteLengthEstimate = qMin(_endTick - _startTick, _currentFile->ticksPerQuarter() * 16); // Max 16 beats
    int searchStartTick = _startTick - maxNoteLengthEstimate;
    int searchEndTick = _endTick + maxNoteLengthEstimate;
    if (searchStartTick < 0) searchStartTick = 0;

    QMultiMap<int, MidiEvent*>::iterator it = map->lowerBound(searchStartTick);
    QMultiMap<int, MidiEvent*>::iterator endIt = map->upperBound(searchEndTick);

    while (it != endIt && it != map->end()) {
        MidiEvent* event = it.value();
        // Quick Y-axis (line) culling before expensive eventInWidget() call
        int line = event->line();
        if (line < _startLineY - 1 || line > _endLineY + 1) {
            it++;
            continue;
        }

        if (eventInWidget(event)) {
            processEventForDisplay(event);
        }
        it++;
    }
}

void HybridMatrixWidget::processEventForDisplay(MidiEvent* event) {
    // Insert all Events in objects, set their coordinates
    // Only onEvents are inserted. When there is an On
    // and an OffEvent, the OnEvent will hold the coordinates

    OffEvent* offEvent = dynamic_cast<OffEvent*>(event);
    OnEvent* onEvent = dynamic_cast<OnEvent*>(event);

    int line = event->line();
    int x, width;
    int y = yPosOfLine(line);
    int height = lineHeight();

    if (onEvent || offEvent) {
        if (onEvent) {
            offEvent = onEvent->offEvent();
        } else if (offEvent) {
            onEvent = dynamic_cast<OnEvent*>(offEvent->onEvent());
        }

        if (onEvent && offEvent) {
            width = xPosOfMs(msOfTick(offEvent->midiTime())) - xPosOfMs(msOfTick(onEvent->midiTime()));
            x = xPosOfMs(msOfTick(onEvent->midiTime()));
            event = onEvent;

            if (!_objects->contains(event)) {
                onEvent->setX(x);
                onEvent->setY(y);
                onEvent->setWidth(width);
                onEvent->setHeight(height);
                onEvent->setShown(true);
                _objects->append(event);
                _velocityObjects->append(event);
            }
        }
    } else {
        // Non-note events (controllers, program changes, etc.)
        x = xPosOfMs(msOfTick(event->midiTime()));
        width = PIXEL_PER_EVENT;

        event->setX(x);
        event->setY(y);
        event->setWidth(width);
        event->setHeight(height);
        event->setShown(true);
        _objects->append(event);
    }
}

void HybridMatrixWidget::updateDivisions() {
    _currentDivs.clear();
    if (!_currentFile || !_currentTimeSignatureEvents || _currentTimeSignatureEvents->isEmpty()) return;

    // Calculate divisions like the old MatrixWidget did, but in business logic layer
    TimeSignatureEvent* currentEvent = _currentTimeSignatureEvents->at(0);
    int i = 0;
    if (!currentEvent) return;

    int tick = currentEvent->midiTime();
    while (tick + currentEvent->ticksPerMeasure() <= _startTick) {
        tick += currentEvent->ticksPerMeasure();
    }

    while (tick < _endTick) {
        // Add measure divisions
        int xfrom = xPosOfMs(msOfTick(tick));
        _currentDivs.append(QPair<int, int>(xfrom, tick));

        int measureStartTick = tick;
        tick += currentEvent->ticksPerMeasure();

        // Add subdivision divisions within the measure
        if (_div >= 0 || _div <= -100) {
            int ticksPerDiv;

            if (_div >= 0) {
                // Regular divisions: _div=0 (whole), _div=1 (half), _div=2 (quarter), etc.
                // Formula: 4 / 2^_div quarters per division
                double metronomeDiv = 4 / (double)qPow(2, _div);
                ticksPerDiv = metronomeDiv * _currentFile->ticksPerQuarter();
            } else if (_div <= -100) {
                // Extended subdivision system:
                // -100 to -199: Triplets (÷3)
                // -200 to -299: Quintuplets (÷5)
                // -300 to -399: Sextuplets (÷6)
                // -400 to -499: Septuplets (÷7)
                // -500 to -599: Dotted notes (×1.5)
                // -600 to -699: Double dotted notes (×1.75)

                int subdivisionType = (-_div) / 100;  // 1=triplets, 2=quintuplets, etc.
                int baseDivision = (-_div) % 100;     // Extract base division

                double baseDiv = 4 / (double)qPow(2, baseDivision);

                if (subdivisionType == 1) {
                    // Triplets: divide by 3
                    ticksPerDiv = (baseDiv * _currentFile->ticksPerQuarter()) / 3;
                } else if (subdivisionType == 2) {
                    // Quintuplets: divide by 5
                    ticksPerDiv = (baseDiv * _currentFile->ticksPerQuarter()) / 5;
                } else if (subdivisionType == 3) {
                    // Sextuplets: divide by 6
                    ticksPerDiv = (baseDiv * _currentFile->ticksPerQuarter()) / 6;
                } else if (subdivisionType == 4) {
                    // Septuplets: divide by 7
                    ticksPerDiv = (baseDiv * _currentFile->ticksPerQuarter()) / 7;
                } else if (subdivisionType == 5) {
                    // Dotted notes: multiply by 1.5
                    ticksPerDiv = (baseDiv * _currentFile->ticksPerQuarter()) * 1.5;
                } else if (subdivisionType == 6) {
                    // Double dotted notes: multiply by 1.75
                    ticksPerDiv = (baseDiv * _currentFile->ticksPerQuarter()) * 1.75;
                } else {
                    // Fallback to triplets for unknown types
                    ticksPerDiv = (baseDiv * _currentFile->ticksPerQuarter()) / 3;
                }
            }

            int startTickDiv = ticksPerDiv;
            while (startTickDiv < currentEvent->ticksPerMeasure()) {
                int divTick = startTickDiv + measureStartTick;
                int xDiv = xPosOfMs(msOfTick(divTick));
                _currentDivs.append(QPair<int, int>(xDiv, divTick));
                startTickDiv += ticksPerDiv;
            }
        }

        // Move to next time signature if needed
        if (i < _currentTimeSignatureEvents->length() - 1) {
            if (_currentTimeSignatureEvents->at(i + 1)->midiTime() <= tick) {
                currentEvent = _currentTimeSignatureEvents->at(i + 1);
                tick = currentEvent->midiTime();
                i++;
            }
        }
    }
}

void HybridMatrixWidget::updateTempoEvents() {
    // Tempo events are now updated in calculateViewportBounds() using file->tick()
    // This method is kept for interface compatibility but does nothing
    // since the tempo events are already populated correctly
}

void HybridMatrixWidget::updateTimeSignatureEvents() {
    if (!_currentFile || !_currentTimeSignatureEvents) return;

    _currentTimeSignatureEvents->clear();

    // Use the same method as old MatrixWidget - let file->measure() populate the time signature events
    // This ensures we get the default time signature if none exists
    _measure = _currentFile->measure(_startTick, _endTick, &_currentTimeSignatureEvents);
}

void HybridMatrixWidget::zoomHorIn() {
    _scaleX += 0.1;
    calcSizes();
}

void HybridMatrixWidget::zoomHorOut() {
    if (_scaleX >= 0.2) {
        _scaleX -= 0.1;
        calcSizes();
    }
}

void HybridMatrixWidget::zoomVerIn() {
    _scaleY += 0.1;
    calcSizes();
}

void HybridMatrixWidget::zoomVerOut() {
    if (_scaleY >= 0.2) {
        _scaleY -= 0.1;
        calcSizes();
    }
}

void HybridMatrixWidget::zoomStd() {
    _scaleX = 1.0;
    _scaleY = 1.0;
    calcSizes();
    syncActiveWidget();
    updateView();
}



void HybridMatrixWidget::playNote(int note) {
    if (!_pianoEvent) return;
    _pianoEvent->setNote(note);
    _pianoEvent->setChannel(MidiOutput::standardChannel(), false);
    MidiPlayer::play(_pianoEvent);
}

void HybridMatrixWidget::pianoEmulator(QKeyEvent* event) {
    if (!_pianoEmulationEnabled) return;

    int key = event->key();
    const int C4_OFFSET = 48;

    // z, s, x, d, c, v -> C, C#, D, D#, E, F
    int keys[] = {
        90, 83, 88, 68, 67, 86, 71, 66, 72, 78, 74, 77, // C3 - H3
        81, 50, 87, 51, 69, 82, 53, 84, 54, 89, 55, 85, // C4 - H4
        73, 57, 79, 48, 80, 91, 61, 93 // C5 - G5
    };
    for (uint8_t idx = 0; idx < sizeof(keys) / sizeof(*keys); idx++) {
        if (key == keys[idx]) {
            playNote(idx + C4_OFFSET);
        }
    }

    int dupkeys[] = {
        44, 76, 46, 59, 47 // C4 - E4 (,l.;/)
    };
    for (uint8_t idx = 0; idx < sizeof(dupkeys) / sizeof(*dupkeys); idx++) {
        if (key == dupkeys[idx]) {
            playNote(idx + C4_OFFSET + 12);
        }
    }
}

void HybridMatrixWidget::calculatePianoKeys() {
    _pianoKeys.clear();

    // Calculate piano key rectangles for mouse interaction
    // This needs to match the complex piano key shapes from MatrixWidget::paintPianoKey()
    for (int line = _startLineY; line <= _endLineY; line++) {
        if (line < 0 || line > 127) continue;

        int midiNote = 127 - line;
        int y = yPosOfLine(line);
        int height = (int)lineHeight();
        int x = 0;
        int width = _lineNameWidth - 10; // Border

        // For now, use simple rectangles - the complex shapes are handled in paintPianoKey()
        // The mouse interaction will work with these simplified rectangles
        _pianoKeys.insert(midiNote, QRect(x, y, width, height));
    }
}

void HybridMatrixWidget::emitSizeChanged() {
    if (!_currentFile) return;

    int maxScrollTime = _currentFile->maxTime() - (_endTimeX - _startTimeX);
    int maxScrollLine = 128 - (_endLineY - _startLineY);

    emit sizeChanged(maxScrollTime, maxScrollLine, _startTimeX, _startLineY);
}

void HybridMatrixWidget::forceCompleteRedraw() {
    // Force complete redraw on active widget only
    if (_currentlyUsingHardware && _hardwareWidget) {
        _hardwareWidget->updateView(); // Hardware widget doesn't need pixmap invalidation
    } else if (_softwareWidget) {
        _softwareWidget->forceCompleteRedraw();
    }
    updateView();
}

void HybridMatrixWidget::setDiv(int div) {
    _div = div;
    updateDivisions();
    syncActiveWidget();
    updateView();
}



// Tool interaction (business logic for both renderers)
void HybridMatrixWidget::handleToolPress(QMouseEvent* event) {
    if (!MidiPlayer::isPlaying() && Tool::currentTool()) {
        if (Tool::currentTool()->press(event->buttons() == Qt::LeftButton)) {
            updateView();
        }
    }
}

void HybridMatrixWidget::handleToolRelease(QMouseEvent*) {
    // Event parameter not needed - tools use EditorTool::mouseX/mouseY for position
    if (!MidiPlayer::isPlaying() && Tool::currentTool()) {
        if (Tool::currentTool()->release()) {
            updateView();
        }
    } else if (Tool::currentTool()) {
        if (Tool::currentTool()->releaseOnly()) {
            updateView();
        }
    }
}

void HybridMatrixWidget::handleToolMove(QMouseEvent* event) {
    if (!MidiPlayer::isPlaying() && Tool::currentTool()) {
        Tool::currentTool()->move(event->x(), event->y());
        updateView();
    }
}

void HybridMatrixWidget::handleToolEnter() {
    if (Tool::currentTool()) {
        Tool::currentTool()->enter();
        updateView();
    }
}

void HybridMatrixWidget::mouseMoveEvent(QMouseEvent* event) {
    // Update mouse coordinates for rendering
    _mouseX = event->x();
    _mouseY = event->y();

    // Handle tool move for ALL mouse movements (like old MatrixWidget)
    // Tools need to track mouse movement even outside the tool area
    handleToolMove(event);

    // Update rendering to show hover effects (only if not playing for performance)
    if (!MidiPlayer::isPlaying()) {
        syncActiveWidget();
        updateView();
    }
}

void HybridMatrixWidget::enterEvent(QEnterEvent* event) {
    Q_UNUSED(event);
    _mouseOver = true;
    handleToolEnter();
    syncActiveWidget();
    updateView();
}

void HybridMatrixWidget::leaveEvent(QEvent* event) {
    Q_UNUSED(event);
    _mouseOver = false;
    handleToolExit();
    syncActiveWidget();
    updateView();
}

void HybridMatrixWidget::handleToolExit() {
    if (Tool::currentTool()) {
        Tool::currentTool()->exit();
        updateView();
    }
}

// Event handling methods (business logic only - no forwarding to renderers)
void HybridMatrixWidget::mousePressEvent(QMouseEvent* event) {
    // Handle business logic first
    if (event->x() >= _toolArea.x() && event->y() >= _toolArea.y() &&
        event->x() <= _toolArea.x() + _toolArea.width() &&
        event->y() <= _toolArea.y() + _toolArea.height()) {

        // Tool interaction - handle business logic
        handleToolPress(event);

    } else if (event->x() >= _pianoArea.x() && event->y() >= _pianoArea.y() &&
               event->x() <= _pianoArea.x() + _pianoArea.width() &&
               event->y() <= _pianoArea.y() + _pianoArea.height()) {

        // Piano key interaction - handle business logic
        for (auto it = _pianoKeys.begin(); it != _pianoKeys.end(); ++it) {
            QRect keyRect = it.value();
            if (event->x() >= keyRect.x() && event->y() >= keyRect.y() &&
                event->x() <= keyRect.x() + keyRect.width() &&
                event->y() <= keyRect.y() + keyRect.height()) {
                playNote(it.key());
                break;
            }
        }
    }
}

void HybridMatrixWidget::mouseReleaseEvent(QMouseEvent* event) {
    // Handle tool interaction business logic
    if (event->x() >= _toolArea.x() && event->y() >= _toolArea.y() &&
        event->x() <= _toolArea.x() + _toolArea.width() &&
        event->y() <= _toolArea.y() + _toolArea.height()) {
        handleToolRelease(event);
    } else if (Tool::currentTool()) {
        // Handle global tool release (like old MatrixWidget)
        if (!MidiPlayer::isPlaying() && Tool::currentTool()->releaseOnly()) {
            updateView();
        }
    }
}

// mouseMoveEvent method moved above to avoid duplication

void HybridMatrixWidget::mouseDoubleClickEvent(QMouseEvent* event) {
    // Handle timeline double-click for cursor positioning
    if (event->x() >= _timeLineArea.x() && event->y() >= _timeLineArea.y() &&
        event->x() <= _timeLineArea.x() + _timeLineArea.width() &&
        event->y() <= _timeLineArea.y() + _timeLineArea.height()) {

        int ms = msOfXPos(event->x());
        if (_currentFile) {
            _currentFile->setCursorTick(_currentFile->tick(ms));
            updateView();
        }
    }
}

void HybridMatrixWidget::wheelEvent(QWheelEvent* event) {
    /*
     * Qt has some underdocumented behaviors for reporting wheel events, so the
     * following were determined empirically:
     *
     * 1.  Some platforms use pixelDelta and some use angleDelta; you need to
     *     handle both.
     *
     * 2.  The documentation for angleDelta is very convoluted, but it boils
     *     down to a scaling factor of 8 to convert to pixels.  Note that
     *     some mouse wheels scroll very coarsely, but this should result in an
     *     equivalent amount of movement as seen in other programs, even when
     *     that means scrolling by multiple lines at a time.
     *
     * 3.  When a modifier key is held, the X and Y may be swapped in how
     *     they're reported, but which modifiers these are differ by platform.
     *     If you want to reserve the modifiers for your own use, you have to
     *     counteract this explicitly.
     *
     * 4.  A single-dimensional scrolling device (mouse wheel) seems to be
     *     reported in the Y dimension of the pixelDelta or angleDelta, but is
     *     subject to the same X/Y swapping when modifiers are pressed.
     */

    if (!_currentFile) return;

    Qt::KeyboardModifiers km = event->modifiers();
    QPoint pixelDelta = event->pixelDelta();
    int pixelDeltaX = pixelDelta.x();
    int pixelDeltaY = pixelDelta.y();

    if ((pixelDeltaX == 0) && (pixelDeltaY == 0)) {
        QPoint angleDelta = event->angleDelta();
        pixelDeltaX = angleDelta.x() / 8;
        pixelDeltaY = angleDelta.y() / 8;
    }

    int horScrollAmount = 0;
    int verScrollAmount = 0;

    if (km) {
        int pixelDeltaLinear = pixelDeltaY;
        if (pixelDeltaLinear == 0) pixelDeltaLinear = pixelDeltaX;

        if (km == Qt::ShiftModifier) {
            if (pixelDeltaLinear > 0) {
                zoomVerIn();
            } else if (pixelDeltaLinear < 0) {
                zoomVerOut();
            }
        } else if (km == Qt::ControlModifier) {
            if (pixelDeltaLinear > 0) {
                zoomHorIn();
            } else if (pixelDeltaLinear < 0) {
                zoomHorOut();
            }
        } else if (km == Qt::AltModifier) {
            horScrollAmount = pixelDeltaLinear;
        }
    } else {
        horScrollAmount = pixelDeltaX;
        verScrollAmount = pixelDeltaY;
    }

    if (_currentFile) {
        int maxTimeInFile = _currentFile->maxTime();
        int widgetRange = _endTimeX - _startTimeX;

        if (horScrollAmount != 0) {
            int scroll = -1 * horScrollAmount * widgetRange / 1000;
            int newStartTime = _startTimeX + scroll;
            scrollXChanged(newStartTime);
        }

        if (verScrollAmount != 0 && _scaleY > 0) {
            double lineSpace = _scaleY * PIXEL_PER_LINE;
            if (lineSpace > 0) {
                int newStartLineY = _startLineY - (verScrollAmount / lineSpace);
                if (newStartLineY < 0) newStartLineY = 0;
                scrollYChanged(newStartLineY);
            }
        }
    }
}

void HybridMatrixWidget::keyPressEvent(QKeyEvent* event) {
    takeKeyPressEvent(event);
}

void HybridMatrixWidget::keyReleaseEvent(QKeyEvent* event) {
    takeKeyReleaseEvent(event);
}

// enterEvent and leaveEvent methods moved above to avoid duplication
