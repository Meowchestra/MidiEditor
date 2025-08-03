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

#ifndef MATRIXWIDGETMANAGER_H
#define MATRIXWIDGETMANAGER_H

#include <QObject>
#include <QWidget>
#include <QSettings>
#include <QStackedWidget>
#include "IMatrixWidget.h"

class MatrixWidget;
class RhiMatrixWidget;
class MidiFile;

/**
 * \class MatrixWidgetManager
 *
 * \brief Manages switching between software and hardware-accelerated MatrixWidget implementations.
 *
 * MatrixWidgetManager provides seamless switching between the traditional software-based
 * MatrixWidget and the new hardware-accelerated RhiMatrixWidget. It maintains state
 * synchronization between the two implementations and handles fallback scenarios.
 *
 * **Key Features:**
 * - **Transparent Switching**: Automatically switches based on settings and availability
 * - **State Synchronization**: Maintains viewport, zoom, and other settings across switches
 * - **Fallback Support**: Gracefully falls back to software rendering if hardware fails
 * - **Performance Monitoring**: Tracks rendering performance and suggests optimal mode
 * - **Hot Swapping**: Can switch rendering modes without losing current state
 *
 * **Usage Pattern:**
 * ```cpp
 * MatrixWidgetManager *manager = new MatrixWidgetManager(settings, parent);
 * manager->setFile(midiFile);
 * 
 * // The manager automatically chooses the best widget implementation
 * QWidget *activeWidget = manager->currentWidget();
 * 
 * // Force a specific mode
 * manager->setHardwareAcceleration(true);
 * ```
 *
 * **Fallback Scenarios:**
 * - RHI initialization failure
 * - Graphics driver issues
 * - Insufficient GPU memory
 * - User preference for software rendering
 * - Compatibility mode for older systems
 */
class MatrixWidgetManager : public QObject {
    Q_OBJECT

public:
    /**
     * \brief Rendering mode enumeration.
     */
    enum RenderingMode {
        SoftwareRendering,    ///< Traditional QPainter-based rendering
        HardwareRendering,    ///< Qt RHI hardware-accelerated rendering
        AutomaticSelection    ///< Automatically choose best available mode
    };

    /**
     * \brief Creates a new MatrixWidgetManager.
     * \param settings Application settings for configuration
     * \param parent Parent widget that will contain the matrix widget
     */
    explicit MatrixWidgetManager(QSettings *settings, QWidget *parent = nullptr);

    /**
     * \brief Destructor.
     */
    ~MatrixWidgetManager();

    /**
     * \brief Gets the currently active widget.
     * \return Pointer to the active MatrixWidget (software or hardware)
     */
    QWidget *currentWidget();

    /**
     * \brief Gets the actual active matrix widget for signal connections.
     * \return Pointer to the active widget (software or hardware) as QWidget
     */
    QWidget *activeMatrixWidget();

    /**
     * \brief Gets the currently active widget as a MatrixWidget interface.
     * \return Pointer to the active widget cast to MatrixWidget base interface
     */
    MatrixWidget *matrixWidget();

    /**
     * \brief Gets the software widget directly (always available).
     * \return Pointer to the software MatrixWidget (for components that need direct access)
     */
    MatrixWidget *softwareWidget();

    /**
     * \brief Gets the currently active widget as an IMatrixWidget interface.
     * \return Pointer to the active widget implementing IMatrixWidget
     */
    IMatrixWidget *iMatrixWidget();

    /**
     * \brief Gets the currently active widget interface (unified access).
     * \return Pointer to the active widget implementing IMatrixWidget (hardware or software)
     */
    IMatrixWidget *matrixWidgetInterface();

    /**
     * \brief Gets the current rendering mode.
     * \return Current rendering mode being used
     */
    RenderingMode currentMode() const { return _currentMode; }

    /**
     * \brief Checks if hardware acceleration is currently active.
     * \return True if using hardware acceleration, false for software
     */
    bool isUsingHardwareAcceleration() const;

    /**
     * \brief Checks if hardware acceleration is available on this system.
     * \return True if RHI hardware acceleration is supported
     */
    bool isHardwareAccelerationAvailable() const;

    /**
     * \brief Gets performance statistics for the current rendering mode.
     * \return String describing current performance metrics
     */
    QString getPerformanceInfo() const;

    // === Configuration ===

    /**
     * \brief Sets the rendering mode.
     * \param mode Desired rendering mode
     * \return True if the mode was successfully set
     */
    bool setRenderingMode(RenderingMode mode);

    /**
     * \brief Enables or disables hardware acceleration.
     * \param enabled True to enable hardware acceleration, false for software
     * \return True if the setting was successfully applied
     */
    bool setHardwareAcceleration(bool enabled);

    /**
     * \brief Forces a fallback to software rendering.
     * Used when hardware rendering encounters issues.
     */
    void forceSoftwareFallback();

    // === File and State Management ===

    /**
     * \brief Sets the MIDI file for both widget implementations.
     * \param file The MidiFile to display and edit
     */
    void setFile(MidiFile *file);

    /**
     * \brief Synchronizes state between software and hardware widgets.
     * Copies viewport, zoom, and other settings from active to inactive widget.
     */
    void synchronizeState();

    /**
     * \brief Updates rendering settings for both widgets.
     * Called when settings change in the preferences dialog.
     */
    void updateRenderingSettings();

signals:
    /**
     * \brief Emitted when the rendering mode changes.
     * \param mode New rendering mode
     * \param reason Human-readable reason for the change
     */
    void renderingModeChanged(RenderingMode mode, const QString &reason);

    /**
     * \brief Emitted when hardware acceleration availability changes.
     * \param available True if hardware acceleration became available
     */
    void hardwareAccelerationAvailabilityChanged(bool available);

    /**
     * \brief Emitted when performance metrics are updated.
     * \param info Performance information string
     */
    void performanceInfoUpdated(const QString &info);

    // Forward signals from active widget
    void sizeChanged(int maxScrollTime, int maxScrollLine, int vX, int vY);
    void scrollChanged(int maxScrollTime, int maxScrollLine, int vX, int vY);
    void objectListChanged();

public slots:
    /**
     * \brief Handles settings changes that might affect rendering mode.
     */
    void onSettingsChanged();

    /**
     * \brief Attempts to recover from hardware rendering failures.
     */
    void recoverFromHardwareFailure();

private slots:
    /**
     * \brief Handles signals from the software widget.
     */
    void onSoftwareWidgetSignal();

    /**
     * \brief Handles signals from the hardware widget.
     */
    void onHardwareWidgetSignal();

private:
    /**
     * \brief Initializes both widget implementations.
     */
    void initializeWidgets();

    /**
     * \brief Creates the software MatrixWidget.
     */
    void createSoftwareWidget();

    /**
     * \brief Creates the hardware RhiMatrixWidget.
     * \return True if creation was successful
     */
    bool createHardwareWidget();

    /**
     * \brief Switches to the specified widget.
     * \param useSoftware True to switch to software widget, false for hardware
     * \return True if switch was successful
     */
    bool switchToWidget(bool useSoftware);

    /**
     * \brief Copies state from one widget to another.
     * \param from Source widget
     * \param to Destination widget
     */
    void copyWidgetState(QWidget *from, QWidget *to);

    /**
     * \brief Connects signals from a widget to this manager.
     * \param widget Widget to connect signals from
     */
    void connectWidgetSignals(QWidget *widget);

    /**
     * \brief Disconnects signals from a widget.
     * \param widget Widget to disconnect signals from
     */
    void disconnectWidgetSignals(QWidget *widget);

    /**
     * \brief Tests hardware acceleration performance.
     * \return True if hardware acceleration provides better performance
     */
    bool testHardwarePerformance();

    /**
     * \brief Logs rendering mode change.
     * \param mode New mode
     * \param reason Reason for change
     */
    void logModeChange(RenderingMode mode, const QString &reason);

    // === Member Variables ===

    /** \brief Application settings */
    QSettings *_settings;

    /** \brief Parent widget container */
    QWidget *_parent;

    /** \brief Stacked widget to hold both implementations */
    QStackedWidget *_stackedWidget;

    /** \brief Software MatrixWidget implementation */
    MatrixWidget *_softwareWidget;

    /** \brief Hardware RhiMatrixWidget implementation */
    RhiMatrixWidget *_hardwareWidget;

    /** \brief Current rendering mode */
    RenderingMode _currentMode;

    /** \brief Whether hardware acceleration is available */
    bool _hardwareAvailable;

    /** \brief Whether we've fallen back due to hardware issues */
    bool _hardwareFallbackActive;

    /** \brief Current MIDI file */
    MidiFile *_currentFile;

    /** \brief Performance monitoring */
    mutable QString _lastPerformanceInfo;
    mutable qint64 _lastPerformanceUpdate;
};

#endif // MATRIXWIDGETMANAGER_H
