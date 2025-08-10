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

public slots:
    /**
     * \brief Refreshes colors when theme changes.
     */
    void refreshColors();

signals:
    /**
     * \brief Emitted when rendering mode changes and should be applied immediately.
     */
    void renderingModeChanged();

private slots:
    // === Software Rendering Settings ===

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

    /**
     * \brief Handles multisampling setting changes.
     * \param index Selected multisampling index
     */
    void multisamplingChanged(int index);

    /**
     * \brief Handles hardware smooth transforms setting changes.
     * \param enabled True to enable hardware smooth transforms
     */
    void enableHardwareSmoothTransformsChanged(bool enabled);

    /**
     * \brief Handles VSync setting changes.
     * \param enabled True to enable VSync
     */
    void enableVSyncChanged(bool enabled);

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

    /**
     * \brief Handles widget size unlock setting changes.
     * \param enabled True to unlock widget minimum sizes
     */
    void widgetSizeUnlockChanged(bool enabled);

    /**
     * \brief Handles font scaling ignore setting changes.
     * \param enabled True to ignore system font scaling
     */
    void ignoreFontScalingChanged(bool enabled);

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
     * \brief Gets a description for a rendering backend.
     * \param backend The backend name to get description for
     * \return String description of the backend
     */
    QString getBackendDescription(const QString &backend) const;

    // === Member Variables ===

    /** \brief Settings storage */
    QSettings *_settings;

    /** \brief Flag to prevent change events during loading */
    bool _isLoading;

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

    /** \brief Checkbox for hardware smooth transforms */
    QCheckBox *_enableHardwareSmoothTransforms;

    /** \brief Checkbox for VSync */
    QCheckBox *_enableVSync;

    /** \brief Combo box for multisampling options */
    QComboBox *_multisamplingCombo;

    /** \brief Label showing backend information */
    QLabel *_backendInfoLabel;

    /** \brief Info box widget for theme color updates */
    QWidget *_infoBox;

    // === DPI Scaling Controls ===

    /** \brief Checkbox for ignoring system UI scaling */
    QCheckBox *_ignoreSystemUIScaling;

    /** \brief Checkbox for ignoring system font scaling */
    QCheckBox *_ignoreSystemFontScaling;

    /** \brief Checkbox for using rounded scaling behavior */
    QCheckBox *_useRoundedScaling;

    /** \brief Checkbox for unlocking widget minimum sizes */
    QCheckBox *_unlockWidgetSizes;
};

#endif // PERFORMANCESETTINGSWIDGET_H_
