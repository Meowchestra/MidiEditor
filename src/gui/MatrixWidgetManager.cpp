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

#include "MatrixWidgetManager.h"
#include "MatrixWidget.h"
#include "RhiMatrixWidget.h"
#include "../midi/MidiFile.h"
#include "../tool/EditorTool.h"

#include <QStackedWidget>
#include <QVBoxLayout>
#include <QApplication>
#include <QDebug>
#include <QTimer>
#include <QDateTime>

MatrixWidgetManager::MatrixWidgetManager(QSettings *settings, QWidget *parent)
    : QObject(parent), _settings(settings), _parent(parent) {
    
    // Initialize state
    _stackedWidget = nullptr;
    _softwareWidget = nullptr;
    _hardwareWidget = nullptr;
    _currentMode = SoftwareRendering;
    _hardwareAvailable = false;
    _hardwareFallbackActive = false;
    _currentFile = nullptr;
    _lastPerformanceUpdate = 0;
    
    // Create the stacked widget container
    _stackedWidget = new QStackedWidget(parent);
    
    // Initialize widgets
    initializeWidgets();
    
    // Determine initial rendering mode
    bool hardwareEnabled = _settings->value("rendering/hardware_acceleration", false).toBool();
    if (hardwareEnabled && _hardwareAvailable) {
        setRenderingMode(HardwareRendering);
    } else {
        setRenderingMode(SoftwareRendering);
    }
    
    qDebug() << "MatrixWidgetManager: Initialized with mode:" << 
                (_currentMode == HardwareRendering ? "Hardware" : "Software");
}

MatrixWidgetManager::~MatrixWidgetManager() {
    // Widgets will be cleaned up by Qt's parent-child system
    qDebug() << "MatrixWidgetManager: Destroyed";
}

QWidget *MatrixWidgetManager::currentWidget() {
    return _stackedWidget;
}

QWidget *MatrixWidgetManager::activeMatrixWidget() {
    if (_currentMode == HardwareRendering && _hardwareWidget) {
        return _hardwareWidget;
    } else if (_softwareWidget) {
        return _softwareWidget;
    }
    return nullptr;
}

MatrixWidget *MatrixWidgetManager::matrixWidget() {
    // Only return the software widget for MatrixWidget* requests
    // Hardware widget should be accessed through IMatrixWidget interface
    if (_currentMode == SoftwareRendering && _softwareWidget) {
        return _softwareWidget;
    }
    return nullptr;
}

MatrixWidget *MatrixWidgetManager::softwareWidget() {
    // Always return the software widget, regardless of current mode
    return _softwareWidget;
}

IMatrixWidget *MatrixWidgetManager::matrixWidgetInterface() {
    if (_currentMode == HardwareRendering && _hardwareWidget) {
        return _hardwareWidget;
    } else if (_softwareWidget) {
        return _softwareWidget;
    }
    return nullptr;
}

IMatrixWidget *MatrixWidgetManager::iMatrixWidget() {
    if (_currentMode == HardwareRendering && _hardwareWidget) {
        return _hardwareWidget;
    } else if (_softwareWidget) {
        return _softwareWidget;
    }
    return nullptr;
}

bool MatrixWidgetManager::isUsingHardwareAcceleration() const {
    return _currentMode == HardwareRendering && _hardwareWidget && !_hardwareFallbackActive;
}

bool MatrixWidgetManager::isHardwareAccelerationAvailable() const {
    return _hardwareAvailable;
}

QString MatrixWidgetManager::getPerformanceInfo() const {
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    
    // Update performance info every 5 seconds
    if (now - _lastPerformanceUpdate > 5000) {
        _lastPerformanceUpdate = now;
        
        if (_currentMode == HardwareRendering && _hardwareWidget) {
            _lastPerformanceInfo = QString("Hardware Acceleration: Active (RHI Backend: %1)")
                                   .arg("Auto-detected");  // Would get actual backend info
        } else {
            _lastPerformanceInfo = QString("Software Rendering: Active (QPainter)");
        }
        
        if (_hardwareFallbackActive) {
            _lastPerformanceInfo += " - Fallback mode due to hardware issues";
        }
    }
    
    return _lastPerformanceInfo;
}

bool MatrixWidgetManager::setRenderingMode(RenderingMode mode) {
    if (mode == _currentMode) {
        return true;  // Already in the requested mode
    }
    
    RenderingMode oldMode = _currentMode;
    QString reason;
    
    switch (mode) {
        case SoftwareRendering:
            if (switchToWidget(true)) {
                _currentMode = SoftwareRendering;
                reason = "User requested software rendering";
            } else {
                qWarning() << "MatrixWidgetManager: Failed to switch to software rendering";
                return false;
            }
            break;
            
        case HardwareRendering:
            if (_hardwareAvailable && switchToWidget(false)) {
                _currentMode = HardwareRendering;
                _hardwareFallbackActive = false;
                reason = "User requested hardware acceleration";
            } else {
                qWarning() << "MatrixWidgetManager: Hardware acceleration not available, staying in software mode";
                reason = "Hardware acceleration unavailable";
                return false;
            }
            break;
            
        case AutomaticSelection:
            // Test performance and choose best mode
            if (_hardwareAvailable && testHardwarePerformance()) {
                if (switchToWidget(false)) {
                    _currentMode = HardwareRendering;
                    reason = "Automatic selection: hardware provides better performance";
                } else {
                    _currentMode = SoftwareRendering;
                    reason = "Automatic selection: hardware failed, using software";
                }
            } else {
                if (switchToWidget(true)) {
                    _currentMode = SoftwareRendering;
                    reason = "Automatic selection: software rendering optimal";
                }
            }
            break;
    }
    
    if (_currentMode != oldMode) {
        logModeChange(_currentMode, reason);
        emit renderingModeChanged(_currentMode, reason);
        return true;
    }
    
    return false;
}

bool MatrixWidgetManager::setHardwareAcceleration(bool enabled) {
    // DISABLED: Always use software rendering mode
    // Hardware acceleration is now handled directly within MatrixWidget for MIDI events only
    Q_UNUSED(enabled)
    return setRenderingMode(SoftwareRendering);
}

void MatrixWidgetManager::forceSoftwareFallback() {
    if (_currentMode == HardwareRendering) {
        _hardwareFallbackActive = true;
        switchToWidget(true);
        _currentMode = SoftwareRendering;
        
        QString reason = "Hardware rendering failure - forced fallback to software";
        logModeChange(_currentMode, reason);
        emit renderingModeChanged(_currentMode, reason);
    }
}

void MatrixWidgetManager::setFile(MidiFile *file) {
    _currentFile = file;
    
    // Set file on both widgets to keep them synchronized
    if (_softwareWidget) {
        _softwareWidget->setFile(file);
    }
    if (_hardwareWidget) {
        _hardwareWidget->setFile(file);
    }
}

void MatrixWidgetManager::synchronizeState() {
    if (!_softwareWidget || !_hardwareWidget) return;
    
    // Copy state from active widget to inactive widget
    if (_currentMode == HardwareRendering) {
        copyWidgetState(_hardwareWidget, _softwareWidget);
    } else {
        copyWidgetState(_softwareWidget, _hardwareWidget);
    }
}

void MatrixWidgetManager::updateRenderingSettings() {
    // Update settings for both widgets
    if (_softwareWidget) {
        _softwareWidget->updateRenderingSettings();
    }
    if (_hardwareWidget) {
        _hardwareWidget->updateRenderingSettings();
    }
    
    // Check if hardware acceleration setting changed
    bool hardwareEnabled = _settings->value("rendering/hardware_acceleration", false).toBool();
    if (hardwareEnabled != isUsingHardwareAcceleration()) {
        setHardwareAcceleration(hardwareEnabled);
    }
}

void MatrixWidgetManager::onSettingsChanged() {
    updateRenderingSettings();
}

void MatrixWidgetManager::recoverFromHardwareFailure() {
    qWarning() << "MatrixWidgetManager: Attempting to recover from hardware failure";
    
    // Try to recreate the hardware widget
    if (_hardwareWidget) {
        disconnectWidgetSignals(_hardwareWidget);
        _stackedWidget->removeWidget(_hardwareWidget);
        delete _hardwareWidget;
        _hardwareWidget = nullptr;
    }
    
    // Attempt to recreate hardware widget
    if (createHardwareWidget()) {
        _hardwareAvailable = true;
        _hardwareFallbackActive = false;
        qDebug() << "MatrixWidgetManager: Hardware recovery successful";
    } else {
        _hardwareAvailable = false;
        forceSoftwareFallback();
        qWarning() << "MatrixWidgetManager: Hardware recovery failed, staying in software mode";
    }
}

void MatrixWidgetManager::onSoftwareWidgetSignal() {
    // Forward signals from software widget
    if (_currentMode == SoftwareRendering && _softwareWidget) {
        // Signals would be forwarded here
        emit objectListChanged();
    }
}

void MatrixWidgetManager::onHardwareWidgetSignal() {
    // Forward signals from hardware widget
    if (_currentMode == HardwareRendering && _hardwareWidget) {
        // Signals would be forwarded here
        emit objectListChanged();
    }
}

void MatrixWidgetManager::initializeWidgets() {
    // Always create software widget (fallback)
    createSoftwareWidget();
    
    // Try to create hardware widget
    if (createHardwareWidget()) {
        _hardwareAvailable = true;
        qDebug() << "MatrixWidgetManager: Hardware acceleration available";
    } else {
        _hardwareAvailable = false;
        qDebug() << "MatrixWidgetManager: Hardware acceleration not available";
    }
}

void MatrixWidgetManager::createSoftwareWidget() {
    if (_softwareWidget) return;
    
    _softwareWidget = new MatrixWidget(_settings, _stackedWidget);
    _stackedWidget->addWidget(_softwareWidget);
    connectWidgetSignals(_softwareWidget);
    
    qDebug() << "MatrixWidgetManager: Software widget created";
}

bool MatrixWidgetManager::createHardwareWidget() {
    // DISABLED: RhiMatrixWidget is disabled in favor of simple RHI acceleration in MatrixWidget
    qDebug() << "MatrixWidgetManager: Hardware widget creation disabled - using software widget with optional RHI acceleration";
    return false;
}

bool MatrixWidgetManager::switchToWidget(bool useSoftware) {
    QWidget *targetWidget = useSoftware ?
        static_cast<QWidget*>(_softwareWidget) :
        static_cast<QWidget*>(_hardwareWidget);

    if (!targetWidget) {
        qWarning() << "MatrixWidgetManager: Target widget not available for switch";
        return false;
    }

    // Synchronize state before switching
    synchronizeState();

    // Switch to the target widget
    _stackedWidget->setCurrentWidget(targetWidget);

    // Debug: Check if the widget switch was successful
    QWidget *currentWidget = _stackedWidget->currentWidget();
    qDebug() << "MatrixWidgetManager: Switched to widget:" << targetWidget;
    qDebug() << "MatrixWidgetManager: Current widget is:" << currentWidget;
    qDebug() << "MatrixWidgetManager: Switch successful:" << (currentWidget == targetWidget);

    // For RhiMatrixWidget, give it time to initialize
    if (!useSoftware && currentWidget == targetWidget) {
        qDebug() << "MatrixWidgetManager: RhiMatrixWidget is now current, forcing update";
        targetWidget->update();

        // Process events to allow Qt to handle the widget becoming visible
        QApplication::processEvents();
    }

    // Update EditorTool to use the new widget interface
    IMatrixWidget *newInterface = useSoftware ?
        static_cast<IMatrixWidget*>(_softwareWidget) :
        static_cast<IMatrixWidget*>(_hardwareWidget);
    EditorTool::setIMatrixWidget(newInterface);

    qDebug() << "MatrixWidgetManager: Switched to" << (useSoftware ? "software" : "hardware") << "widget";
    return true;
}

void MatrixWidgetManager::copyWidgetState(QWidget *from, QWidget *to) {
    // This would copy viewport, zoom, and other state between widgets
    // Implementation depends on the specific interface provided by both widgets
    
    // For now, just ensure both have the same file
    if (_currentFile) {
        MatrixWidget *fromMatrix = dynamic_cast<MatrixWidget*>(from);
        MatrixWidget *toMatrix = dynamic_cast<MatrixWidget*>(to);
        RhiMatrixWidget *fromRhi = dynamic_cast<RhiMatrixWidget*>(from);
        RhiMatrixWidget *toRhi = dynamic_cast<RhiMatrixWidget*>(to);
        
        if (fromMatrix && toMatrix) {
            // Copy state between software widgets
            // toMatrix->setViewport(...);
            // toMatrix->setScaleX(fromMatrix->getScaleX());
            // etc.
        } else if (fromRhi && toRhi) {
            // Copy state between hardware widgets
        } else if (fromMatrix && toRhi) {
            // Copy from software to hardware
        } else if (fromRhi && toMatrix) {
            // Copy from hardware to software
        }
    }
}

void MatrixWidgetManager::connectWidgetSignals(QWidget *widget) {
    MatrixWidget *matrixWidget = dynamic_cast<MatrixWidget*>(widget);
    RhiMatrixWidget *rhiWidget = dynamic_cast<RhiMatrixWidget*>(widget);
    
    if (matrixWidget) {
        connect(matrixWidget, SIGNAL(objectListChanged()),
                this, SLOT(onSoftwareWidgetSignal()));
        connect(matrixWidget, SIGNAL(sizeChanged(int,int,int,int)),
                this, SIGNAL(sizeChanged(int,int,int,int)));
        connect(matrixWidget, SIGNAL(scrollChanged(int,int,int,int)),
                this, SIGNAL(scrollChanged(int,int,int,int)));
    } else if (rhiWidget) {
        connect(rhiWidget, SIGNAL(objectListChanged()),
                this, SLOT(onHardwareWidgetSignal()));
        connect(rhiWidget, SIGNAL(sizeChanged(int,int,int,int)),
                this, SIGNAL(sizeChanged(int,int,int,int)));
        connect(rhiWidget, SIGNAL(scrollChanged(int,int,int,int)),
                this, SIGNAL(scrollChanged(int,int,int,int)));
    }
}

void MatrixWidgetManager::disconnectWidgetSignals(QWidget *widget) {
    // Disconnect all signals from the widget
    disconnect(widget, nullptr, this, nullptr);
}

bool MatrixWidgetManager::testHardwarePerformance() {
    // Simple performance test - in a full implementation this would
    // render a test scene and measure performance
    return _hardwareAvailable;
}

void MatrixWidgetManager::logModeChange(RenderingMode mode, const QString &reason) {
    QString modeStr = (mode == HardwareRendering) ? "Hardware" : "Software";
    qDebug() << "MatrixWidgetManager: Rendering mode changed to" << modeStr << "-" << reason;
}
