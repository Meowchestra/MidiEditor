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

#ifndef PERFORMANCESETTINGSWIDGET_H_
#define PERFORMANCESETTINGSWIDGET_H_

// Project includes
#include "SettingsWidget.h"

// Qt includes
#include <QWidget>
#include <QSettings>

// Forward declarations
class QCheckBox;
class QComboBox;
class QSpinBox;
class QSlider;
class QLabel;
class QGroupBox;

/**
 * \class PerformanceSettingsWidget
 *
 * \brief Settings widget for performance and rendering optimizations.
 *
 * PerformanceSettingsWidget allows users to configure various performance
 * and rendering optimizations to improve the application's responsiveness
 * and visual quality:
 *
 * - **Rendering quality**: Smooth pixmap transforms, lossless image rendering
 * - **Hardware acceleration**: GPU-accelerated rendering when available
 * - **DPI scaling**: High-DPI display optimization settings
 *
 * The widget provides detailed information about available backends and
 * their capabilities, helping users make informed performance choices.
 */
class PerformanceSettingsWidget : public SettingsWidget {
    Q_OBJECT

public:
    /**
     * \brief Creates a new PerformanceSettingsWidget.
     * \param settings QSettings instance for configuration storage
     * \param parent The parent widget
     */
    explicit PerformanceSettingsWidget(QSettings *settings, QWidget *parent = nullptr);

    /**
     * \brief Validates and applies the performance settings.
     * \return True if settings are valid and applied successfully
     */
    bool accept() override;

    /**
     * \brief Gets the icon for this settings panel.
     * \return QIcon for the performance settings
     */
    QIcon icon() override;

signals:
    /**
     * \brief Emitted when hardware acceleration setting changes.
     * \param enabled True if hardware acceleration was enabled
     */
    void hardwareAccelerationChanged(bool enabled);

public slots:
    /**
     * \brief Refreshes colors when theme changes.
     */
    void refreshColors();

private slots:
    // === Rendering Quality Settings ===

    /**
     * \brief Handles antialiasing setting changes.
     * \param enabled True to enable antialiasing
     */
    void enableAntialiasingChanged(bool enabled);

    /**
     * \brief Handles smooth pixmap transform setting changes.
     * \param enabled True to enable smooth pixmap transforms
     */
    void enableSmoothPixmapTransformChanged(bool enabled);

    // === Hardware Acceleration Settings ===

    /**
     * \brief Handles hardware acceleration setting changes.
     * \param enabled True to enable hardware acceleration
     */
    void enableHardwareAccelerationChanged(bool enabled);

    // === DPI Scaling Settings ===

    /**
     * \brief Handles DPI scaling ignore setting changes.
     * \param enabled True to ignore system DPI scaling
     */
    void ignoreScalingChanged(bool enabled);

    /**
     * \brief Handles rounded scaling setting changes.
     * \param enabled True to use rounded scaling factors
     */
    void roundedScalingChanged(bool enabled);

    // === Reset to Defaults ===

    /**
     * \brief Resets all performance settings to default values.
     */
    void resetToDefaults();

private:
    // === Setup and Management Methods ===

    /**
     * \brief Sets up the user interface.
     */
    void setupUI();

    /**
     * \brief Loads settings from configuration.
     */
    void loadSettings();

    /**
     * \brief Updates information labels with current backend info.
     */
    void updateInfoLabels();

    /**
     * \brief Gets a description for a rendering backend.
     * \param backend The backend name to get description for
     * \return String description of the backend
     */
    QString getBackendDescription(const QString &backend) const;

    // === Member Variables ===

    /** \brief Settings storage */
    QSettings *_settings;

    // === Rendering Quality Controls ===

    /** \brief Group box for rendering quality settings */
    QGroupBox *_renderingQualityGroup;

    /** \brief Checkboxes for rendering quality options */
    QCheckBox *_enableAntialiasing;
    QCheckBox *_enableSmoothPixmapTransform;

    // === Hardware Acceleration Controls ===

    /** \brief Group box for hardware acceleration settings */
    QGroupBox *_hardwareAccelerationGroup;

    /** \brief Checkbox for hardware acceleration options */
    QCheckBox *_enableHardwareAcceleration;

    /** \brief Label showing backend information */
    QLabel *_backendInfoLabel;

    /** \brief Info box widget for theme color updates */
    QWidget *_infoBox;
};

#endif // PERFORMANCESETTINGSWIDGET_H_
