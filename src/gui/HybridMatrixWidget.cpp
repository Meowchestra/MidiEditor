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
#include "../gui/GraphicObject.h"

#include <QStackedWidget>
#include <QVBoxLayout>
#include <QSettings>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QApplication>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QDebug>

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
    , _lineHeight(10.0)
    , _lineNameWidth(100)
    , _colorsByChannels(false)
    , _div(1)
    , _measure(0)
    , _tool(0)
{
    // Initialize settings
    _settings = new QSettings(QString("MidiEditor"), QString("NONE"));
    
    setupWidgets();
    refreshAccelerationSettings();
}

HybridMatrixWidget::~HybridMatrixWidget() {
    delete _settings;
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
    bool enabled = _settings->value("rendering/hardware_acceleration", true).toBool();
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
    if (_currentlyUsingHardware) {
        qDebug() << "HybridMatrixWidget: Switching to software rendering";
        _currentlyUsingHardware = false;
        emit accelerationStatusChanged(false);
    }
    
    _stackedWidget->setCurrentWidget(_softwareWidget);
    syncWidgetStates();
    
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
    }
    
    _stackedWidget->setCurrentWidget(_hardwareWidget);
    syncWidgetStates();

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

void HybridMatrixWidget::syncWidgetStates() {
    // Sync file
    if (_currentFile) {
        _softwareWidget->setFile(_currentFile);
        if (_hardwareWidget) {
            _hardwareWidget->setFile(_currentFile);
        }
    }

    // Sync viewport (both widgets now have setViewport)
    _softwareWidget->setViewport(_startTick, _endTick, _startLine, _endLine);
    if (_hardwareWidget) {
        _hardwareWidget->setViewport(_startTick, _endTick, _startLine, _endLine);
        _hardwareWidget->setLineHeight(_lineHeight);
    }
}

void HybridMatrixWidget::setFile(MidiFile* file) {
    _currentFile = file;
    
    _softwareWidget->setFile(file);
    if (_hardwareWidget) {
        _hardwareWidget->setFile(file);
    }
}

MidiFile* HybridMatrixWidget::midiFile() const {
    return _currentFile;
}

void HybridMatrixWidget::setViewport(int startTick, int endTick, int startLine, int endLine) {
    _startTick = startTick;
    _endTick = endTick;
    _startLine = startLine;
    _endLine = endLine;
    
    if (_hardwareWidget) {
        _hardwareWidget->setViewport(startTick, endTick, startLine, endLine);
    }
    
    // Note: Need to add viewport control to MatrixWidget
    emit viewportChanged(startTick, endTick);
}

void HybridMatrixWidget::setLineHeight(double height) {
    _lineHeight = height;
    
    if (_hardwareWidget) {
        _hardwareWidget->setLineHeight(height);
    }
    
    // Note: MatrixWidget already has lineHeight control
}

double HybridMatrixWidget::lineHeight() const {
    return _lineHeight;
}

bool HybridMatrixWidget::isHardwareAccelerated() const {
    return _currentlyUsingHardware;
}

QString HybridMatrixWidget::getPerformanceInfo() const {
    return _lastPerformanceInfo;
}

void HybridMatrixWidget::updateView() {
    if (_currentlyUsingHardware && _hardwareWidget) {
        _hardwareWidget->updateView();
    } else {
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
    updateView();
}

void HybridMatrixWidget::paintEvent(QPaintEvent* event) {
    // Let the stacked widget handle painting
    QWidget::paintEvent(event);
}

// Forward mouse events to the active widget for proper interaction
void HybridMatrixWidget::mousePressEvent(QMouseEvent* event) {
    QWidget* activeWidget = _stackedWidget->currentWidget();
    if (activeWidget) {
        // Forward the event to the active widget
        QMouseEvent forwardedEvent(event->type(), event->pos(), event->button(), event->buttons(), event->modifiers());
        QApplication::sendEvent(activeWidget, &forwardedEvent);
    }
    QWidget::mousePressEvent(event);
}

void HybridMatrixWidget::mouseMoveEvent(QMouseEvent* event) {
    QWidget* activeWidget = _stackedWidget->currentWidget();
    if (activeWidget) {
        QMouseEvent forwardedEvent(event->type(), event->pos(), event->button(), event->buttons(), event->modifiers());
        QApplication::sendEvent(activeWidget, &forwardedEvent);
    }
    QWidget::mouseMoveEvent(event);
}

void HybridMatrixWidget::mouseReleaseEvent(QMouseEvent* event) {
    QWidget* activeWidget = _stackedWidget->currentWidget();
    if (activeWidget) {
        QMouseEvent forwardedEvent(event->type(), event->pos(), event->button(), event->buttons(), event->modifiers());
        QApplication::sendEvent(activeWidget, &forwardedEvent);
    }
    QWidget::mouseReleaseEvent(event);
}

void HybridMatrixWidget::wheelEvent(QWheelEvent* event) {
    QWidget* activeWidget = _stackedWidget->currentWidget();
    if (activeWidget) {
        // Qt 6.10 QWheelEvent constructor - use position() and globalPosition() instead of pos() and globalPos()
        QWheelEvent forwardedEvent(event->position(), event->globalPosition(), event->pixelDelta(), event->angleDelta(),
                                  event->buttons(), event->modifiers(), event->phase(), event->inverted());
        QApplication::sendEvent(activeWidget, &forwardedEvent);
    }
    QWidget::wheelEvent(event);
}

// Forward all MatrixWidget methods for complete interface compatibility
void HybridMatrixWidget::setScreenLocked(bool locked) {
    _softwareWidget->setScreenLocked(locked);
    // Hardware widget doesn't need screen locking
}

bool HybridMatrixWidget::screenLocked() {
    return _softwareWidget->screenLocked();
}

int HybridMatrixWidget::minVisibleMidiTime() {
    return _softwareWidget->minVisibleMidiTime();
}

int HybridMatrixWidget::maxVisibleMidiTime() {
    return _softwareWidget->maxVisibleMidiTime();
}

void HybridMatrixWidget::setLineNameWidth(int width) {
    _lineNameWidth = width;
    _softwareWidget->setLineNameWidth(width);
    if (_hardwareWidget) {
        _hardwareWidget->setLineNameWidth(width);
    }
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
    if (enabled) {
        _softwareWidget->setColorsByChannel();
    } else {
        _softwareWidget->setColorsByTracks();
    }
    if (_hardwareWidget) {
        _hardwareWidget->setColorsByChannels(enabled);
    }
}

bool HybridMatrixWidget::colorsByChannels() const {
    return _colorsByChannels;
}

void HybridMatrixWidget::setDiv(int div) {
    _div = div;
    _softwareWidget->setDiv(div);
    // Hardware widget doesn't need div setting
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
    return _softwareWidget->getPianoEmulation();
}

void HybridMatrixWidget::setPianoEmulation(bool enabled) {
    _softwareWidget->setPianoEmulation(enabled);
    // Hardware widget doesn't need piano emulation
}

void HybridMatrixWidget::resetView() {
    _softwareWidget->resetView();
    // Hardware widget doesn't have resetView - just update
    if (_hardwareWidget) {
        _hardwareWidget->updateView();
    }
}

void HybridMatrixWidget::timeMsChanged(int ms, bool ignoreLocked) {
    _softwareWidget->timeMsChanged(ms, ignoreLocked);
    // Hardware widget doesn't have timeMsChanged - just update
    if (_hardwareWidget) {
        _hardwareWidget->updateView();
    }
}

void HybridMatrixWidget::calcSizes() {
    _softwareWidget->calcSizes();
    // Hardware widget doesn't need calcSizes - just update
    if (_hardwareWidget) {
        _hardwareWidget->updateView();
    }
}

void HybridMatrixWidget::registerRelayout() {
    _softwareWidget->registerRelayout();
    // Hardware widget doesn't need registerRelayout - just update
    if (_hardwareWidget) {
        _hardwareWidget->updateView();
    }
}

void HybridMatrixWidget::scrollXChanged(int scrollPositionX) {
    _softwareWidget->scrollXChanged(scrollPositionX);
    // Hardware widget doesn't have scroll methods - just update
    if (_hardwareWidget) {
        _hardwareWidget->updateView();
    }
}

void HybridMatrixWidget::scrollYChanged(int scrollPositionY) {
    _softwareWidget->scrollYChanged(scrollPositionY);
    // Hardware widget doesn't have scroll methods - just update
    if (_hardwareWidget) {
        _hardwareWidget->updateView();
    }
}

void HybridMatrixWidget::takeKeyPressEvent(QKeyEvent* event) {
    _softwareWidget->takeKeyPressEvent(event);
    // Hardware widget doesn't have key event methods - just forward to software
}

void HybridMatrixWidget::takeKeyReleaseEvent(QKeyEvent* event) {
    _softwareWidget->takeKeyReleaseEvent(event);
    // Hardware widget doesn't have key event methods - just forward to software
}

QList<MidiEvent*>* HybridMatrixWidget::velocityEvents() {
    return _softwareWidget->velocityEvents();
}

QList<QPair<int, int>> HybridMatrixWidget::divs() {
    return _softwareWidget->divs();
}

int HybridMatrixWidget::msOfXPos(int x) {
    return _softwareWidget->msOfXPos(x);
}

int HybridMatrixWidget::xPosOfMs(int ms) {
    return _softwareWidget->xPosOfMs(ms);
}

int HybridMatrixWidget::msOfTick(int tick) {
    return _softwareWidget->msOfTick(tick);
}

int HybridMatrixWidget::yPosOfLine(int line) {
    return _softwareWidget->yPosOfLine(line);
}

MatrixWidget* HybridMatrixWidget::getMatrixWidget() const {
    return _softwareWidget;
}
