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

#include "PerformanceSettingsWidget.h"
#include "Appearance.h"

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QGroupBox>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QApplication>

PerformanceSettingsWidget::PerformanceSettingsWidget(QSettings *settings, QWidget *parent)
    : SettingsWidget(tr("System & Performance"), parent)
      , _settings(settings)
      , _isLoading(false)
      , _infoBox(nullptr) {
    setupUI();
    loadSettings();
}

void PerformanceSettingsWidget::setupUI() {
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    setLayout(mainLayout);

    // High DPI Scaling Group
    QGroupBox *scalingGroup = new QGroupBox(tr("High DPI Scaling"), this);
    QGridLayout *scalingLayout = new QGridLayout(scalingGroup);

    _ignoreSystemUIScaling = new QCheckBox(tr("Ignore system UI scaling"), this);
    _ignoreSystemUIScaling->setChecked(Appearance::ignoreSystemScaling());
    _ignoreSystemUIScaling->setToolTip(tr("Disable high DPI scaling for UI elements"));
    connect(_ignoreSystemUIScaling, &QCheckBox::toggled, this, &PerformanceSettingsWidget::ignoreScalingChanged);
    scalingLayout->addWidget(_ignoreSystemUIScaling, 0, 0, 1, 2);

    QLabel *ignoreDesc = new QLabel(tr("Provides smallest UI but may be hard to read on high DPI displays.\nChanges apply on restart."), this);
    ignoreDesc->setWordWrap(true);
    ignoreDesc->setStyleSheet("color: gray; font-size: 11px; margin-left: 10px;");
    scalingLayout->addWidget(ignoreDesc, 1, 0, 1, 2);

    _ignoreSystemFontScaling = new QCheckBox(tr("Ignore system font scaling"), this);
    _ignoreSystemFontScaling->setChecked(Appearance::ignoreFontScaling());
    _ignoreSystemFontScaling->setToolTip(tr("Keep fonts at their original sizes regardless of system DPI."));
    connect(_ignoreSystemFontScaling, &QCheckBox::toggled, this, &PerformanceSettingsWidget::ignoreFontScalingChanged);
    scalingLayout->addWidget(_ignoreSystemFontScaling, 2, 0, 1, 2);

    QLabel *fontDesc = new QLabel(tr("Prevents fonts from scaling with system DPI.\nChanges apply on restart."), this);
    fontDesc->setWordWrap(true);
    fontDesc->setStyleSheet("color: gray; font-size: 11px; margin-left: 10px;");
    scalingLayout->addWidget(fontDesc, 3, 0, 1, 2);

    _useRoundedScaling = new QCheckBox(tr("Use rounded scaling behavior"), this);
    _useRoundedScaling->setChecked(Appearance::useRoundedScaling());
    _useRoundedScaling->setToolTip(tr("Use integer scaling instead of fractional."));
    connect(_useRoundedScaling, &QCheckBox::toggled, this, &PerformanceSettingsWidget::roundedScalingChanged);
    scalingLayout->addWidget(_useRoundedScaling, 4, 0, 1, 2);

    QLabel *roundedDesc = new QLabel(tr("Integer scaling (100%, 200%) may provide sharper text than fractional scaling (125%, 150%) but result in a larger ui.\nChanges apply on restart."), this);
    roundedDesc->setWordWrap(true);
    roundedDesc->setStyleSheet("color: gray; font-size: 11px; margin-left: 10px;");
    scalingLayout->addWidget(roundedDesc, 5, 0, 1, 2);

    mainLayout->addWidget(scalingGroup);

    // Rendering Quality Group
    _renderingQualityGroup = new QGroupBox(tr("Rendering Quality"), this);
    QGridLayout *qualityLayout = new QGridLayout(_renderingQualityGroup);

    _enableAntialiasing = new QCheckBox(tr("Enable software anti-aliasing"), this);
    _enableAntialiasing->setToolTip(tr("CPU-based anti-aliasing. Smoother edges but reduces performance."));
    connect(_enableAntialiasing, &QCheckBox::toggled, this, &PerformanceSettingsWidget::enableAntialiasingChanged);
    qualityLayout->addWidget(_enableAntialiasing, 0, 0, 1, 2);

    _enableSmoothPixmapTransform = new QCheckBox(tr("Enable software smooth pixmap transforms"), this);
    _enableSmoothPixmapTransform->setToolTip(tr("CPU-based smooth pixmap transforms. Smoother scaling but reduces performance."));
    connect(_enableSmoothPixmapTransform, &QCheckBox::toggled, this, &PerformanceSettingsWidget::enableSmoothPixmapTransformChanged);
    qualityLayout->addWidget(_enableSmoothPixmapTransform, 1, 0, 1, 2);

    // Add note about software rendering tearing
    QLabel *tearingNote = new QLabel(tr("Note: Software rendering tearing is controlled by graphics drivers / compositor settings, not by this application."), this);
    tearingNote->setWordWrap(true);
    tearingNote->setStyleSheet("color: gray; font-size: 11px; margin-left: 10px;");
    qualityLayout->addWidget(tearingNote, 2, 0, 1, 2);

    mainLayout->addWidget(_renderingQualityGroup);

    // Hardware Acceleration Group
    _hardwareAccelerationGroup = new QGroupBox(tr("Hardware Acceleration"), this);
    QGridLayout *accelLayout = new QGridLayout(_hardwareAccelerationGroup);

    _enableHardwareAcceleration = new QCheckBox(tr("Enable GPU acceleration for MIDI events"), this);
    _enableHardwareAcceleration->setToolTip(tr("Use OpenGL for GPU-accelerated MIDI rendering."));
    connect(_enableHardwareAcceleration, &QCheckBox::toggled, this, &PerformanceSettingsWidget::enableHardwareAccelerationChanged);
    accelLayout->addWidget(_enableHardwareAcceleration, 0, 0, 1, 2);

    // Description right below the hardware acceleration checkbox
    QLabel *accelDesc = new QLabel(tr("GPU acceleration uses direct OpenGL widgets for maximum performance.\nChanges apply on restart."), this);
    accelDesc->setWordWrap(true);
    accelDesc->setStyleSheet("color: gray; font-size: 11px; margin-left: 10px;");
    accelLayout->addWidget(accelDesc, 1, 0, 1, 2);

    // Multisampling option
    QLabel *multisamplingLabel = new QLabel(tr("Hardware anti-aliasing (MSAA):"), this);
    accelLayout->addWidget(multisamplingLabel, 2, 0);

    _multisamplingCombo = new QComboBox(this);
    _multisamplingCombo->addItem(tr("Disabled"), 0);
    _multisamplingCombo->addItem(tr("2x MSAA"), 2);
    _multisamplingCombo->addItem(tr("4x MSAA"), 4);
    _multisamplingCombo->addItem(tr("8x MSAA"), 8);
    _multisamplingCombo->setToolTip(tr("GPU-based anti-aliasing. Higher values provide smoother edges but reduces performance."));
    connect(_multisamplingCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PerformanceSettingsWidget::multisamplingChanged);
    accelLayout->addWidget(_multisamplingCombo, 2, 1);

    // Hardware smooth transforms option
    _enableHardwareSmoothTransforms = new QCheckBox(tr("Enable hardware smooth transforms"), this);
    _enableHardwareSmoothTransforms->setToolTip(tr("GPU-based texture filtering for smoother scaling but reduces performance."));
    connect(_enableHardwareSmoothTransforms, &QCheckBox::toggled, this, &PerformanceSettingsWidget::enableHardwareSmoothTransformsChanged);
    accelLayout->addWidget(_enableHardwareSmoothTransforms, 3, 0, 1, 2);

    // VSync option (only affects OpenGL hardware acceleration)
    _enableVSync = new QCheckBox(tr("Enable VSync"), this);
    connect(_enableVSync, &QCheckBox::toggled, this, &PerformanceSettingsWidget::enableVSyncChanged);
    accelLayout->addWidget(_enableVSync, 4, 0, 1, 2);

    // VSync description
    QLabel *vsyncDescription = new QLabel(tr("Synchronizes OpenGL rendering with display refresh rate. Prevents tearing but may reduce responsiveness.\nChanges apply on restart."), this);
    vsyncDescription->setWordWrap(true);
    vsyncDescription->setStyleSheet("color: gray; font-size: 11px; margin-left: 10px;");
    accelLayout->addWidget(vsyncDescription, 5, 0, 1, 2);

    _backendInfoLabel = new QLabel(this);
    _backendInfoLabel->setWordWrap(true);
    accelLayout->addWidget(_backendInfoLabel, 6, 0, 1, 2);

    mainLayout->addWidget(_hardwareAccelerationGroup);

    // Reset button
    QPushButton *resetButton = new QPushButton(tr("Reset to Default"), this);
    connect(resetButton, &QPushButton::clicked, this, &PerformanceSettingsWidget::resetToDefaults);
    mainLayout->addWidget(resetButton);

    mainLayout->addStretch();
}

void PerformanceSettingsWidget::loadSettings() {
    // Set loading flag to prevent change events during initialization
    _isLoading = true;

    // Load rendering quality settings (default to high quality)
    _enableAntialiasing->setChecked(_settings->value("rendering/antialiasing", true).toBool());
    _enableSmoothPixmapTransform->setChecked(_settings->value("rendering/smooth_pixmap_transform", true).toBool());

    // Load hardware acceleration settings (default to disabled to avoid shutdown crashes)
    _enableHardwareAcceleration->setChecked(_settings->value("rendering/hardware_acceleration", false).toBool());

    // Load hardware smooth transforms setting
    _enableHardwareSmoothTransforms->setChecked(_settings->value("rendering/hardware_smooth_transforms", true).toBool());

    // Load VSync setting (default to false for responsiveness)
    _enableVSync->setChecked(_settings->value("rendering/enable_vsync", false).toBool());

    // Load multisampling setting
    int msaaSamples = _settings->value("rendering/msaa_samples", 2).toInt();
    int comboIndex = 0;
    switch (msaaSamples) {
        case 0: comboIndex = 0; break;
        case 2: comboIndex = 1; break;
        case 4: comboIndex = 2; break;
        case 8: comboIndex = 3; break;
        default: comboIndex = 1; break; // Default to 2x MSAA
    }
    _multisamplingCombo->setCurrentIndex(comboIndex);

    // Apply the enable/disable logic for all options
    enableHardwareAccelerationChanged(_enableHardwareAcceleration->isChecked());

    // Clear loading flag - change events can now be processed normally
    _isLoading = false;
}

bool PerformanceSettingsWidget::accept() {
    // Save rendering quality settings
    _settings->setValue("rendering/antialiasing", _enableAntialiasing->isChecked());
    _settings->setValue("rendering/smooth_pixmap_transform", _enableSmoothPixmapTransform->isChecked());

    // Save hardware acceleration settings
    _settings->setValue("rendering/hardware_acceleration", _enableHardwareAcceleration->isChecked());

    // Save hardware smooth transforms setting
    _settings->setValue("rendering/hardware_smooth_transforms", _enableHardwareSmoothTransforms->isChecked());

    // Save VSync setting
    _settings->setValue("rendering/enable_vsync", _enableVSync->isChecked());

    // Save multisampling setting
    int msaaSamples = _multisamplingCombo->currentData().toInt();
    _settings->setValue("rendering/msaa_samples", msaaSamples);

    return true;
}

QIcon PerformanceSettingsWidget::icon() {
    return QIcon(); // No icon for performance tab
}

void PerformanceSettingsWidget::refreshColors() {
    // Update info box colors to match current theme
    if (_infoBox) {
        QLabel *label = qobject_cast<QLabel *>(_infoBox);
        if (label) {
            QColor bgColor = Appearance::infoBoxBackgroundColor();
            QColor textColor = Appearance::infoBoxTextColor();
            QString styleSheet = QString("color: rgb(%1, %2, %3); background-color: rgb(%4, %5, %6); padding: 5px")
                    .arg(textColor.red()).arg(textColor.green()).arg(textColor.blue())
                    .arg(bgColor.red()).arg(bgColor.green()).arg(bgColor.blue());
            label->setStyleSheet(styleSheet);
        }
    }

    update();
}

void PerformanceSettingsWidget::enableHardwareAccelerationChanged(bool enabled) {
    // Always update UI state, but skip save/signal during loading
    if (!_isLoading) {
        qDebug() << "PerformanceSettingsWidget: Hardware acceleration changed to" << enabled;
    }

    // Enable/disable OpenGL-specific options based on hardware acceleration setting
    _multisamplingCombo->setEnabled(enabled);
    // Note: _enableHardwareSmoothTransforms should always be enabled when hardware acceleration is on
    // so users can choose whether they want smooth transforms or pixelated rendering
    _enableHardwareSmoothTransforms->setEnabled(enabled);
    _enableVSync->setEnabled(enabled);

    // Software rendering options are only available when hardware acceleration is OFF
    if (enabled) {
        // Hardware acceleration is ON - disable software rendering options (but preserve user's preferences)
        if (!_isLoading) {
            qDebug() << "PerformanceSettingsWidget: Hardware acceleration ON, disabling software rendering options";
        }
        _enableAntialiasing->setEnabled(false);
        _enableSmoothPixmapTransform->setEnabled(false);
        // Don't uncheck them - preserve the user's preferences for when they turn hardware acceleration off
        _enableAntialiasing->setToolTip(tr("Disabled when hardware acceleration is enabled. Use Hardware anti-aliasing (MSAA) instead."));
        _enableSmoothPixmapTransform->setToolTip(tr("Disabled when hardware acceleration is enabled. Use Hardware Smooth Transforms instead."));
    } else {
        // Hardware acceleration is OFF - enable software rendering options
        if (!_isLoading) {
            qDebug() << "PerformanceSettingsWidget: Hardware acceleration OFF, enabling software rendering options";
        }
        _enableAntialiasing->setEnabled(true);
        _enableSmoothPixmapTransform->setEnabled(true);
        _enableAntialiasing->setToolTip(tr("CPU-based anti-aliasing. Provides smoother edges but reduces performance."));
        _enableSmoothPixmapTransform->setToolTip(tr("CPU-based smooth pixmap transforms. Smoother scaling but reduces performance."));
    }
}

void PerformanceSettingsWidget::multisamplingChanged(int index) {
    Q_UNUSED(index)

    // Skip processing during loading to avoid unnecessary events
    if (_isLoading) return;

    // MSAA setting changed - apply immediately
    int msaaSamples = _multisamplingCombo->currentData().toInt();
    qDebug() << "PerformanceSettingsWidget: MSAA changed to" << msaaSamples << "samples - applying immediately";

    // Save the setting immediately
    _settings->setValue("rendering/msaa_samples", msaaSamples);

    // Notify the main window to update OpenGL widgets
    emit renderingModeChanged();
}

void PerformanceSettingsWidget::enableHardwareSmoothTransformsChanged(bool enabled) {
    // Skip processing during loading to avoid unnecessary events
    if (_isLoading) return;

    qDebug() << "PerformanceSettingsWidget: Hardware smooth transforms changed to" << enabled;

    // Save the setting immediately
    _settings->setValue("rendering/hardware_smooth_transforms", enabled);

    // Notify the main window to update OpenGL widgets
    emit renderingModeChanged();
}

void PerformanceSettingsWidget::enableAntialiasingChanged(bool enabled) {
    // Skip processing during loading to avoid unnecessary events
    if (_isLoading) return;

    qDebug() << "PerformanceSettingsWidget: Antialiasing changed to" << enabled;

    // Save the setting immediately
    _settings->setValue("rendering/antialiasing", enabled);

    // Notify the main window to update rendering
    emit renderingModeChanged();
}

void PerformanceSettingsWidget::enableSmoothPixmapTransformChanged(bool enabled) {
    // Skip processing during loading to avoid unnecessary events
    if (_isLoading) return;

    qDebug() << "PerformanceSettingsWidget: Smooth pixmap transform changed to" << enabled;

    // Save the setting immediately
    _settings->setValue("rendering/smooth_pixmap_transform", enabled);

    // Notify the main window to update rendering
    emit renderingModeChanged();
}

void PerformanceSettingsWidget::enableVSyncChanged(bool enabled) {
    // Skip processing during loading to avoid unnecessary events
    if (_isLoading) return;

    qDebug() << "PerformanceSettingsWidget: VSync changed to" << enabled;

    // Save the setting immediately
    _settings->setValue("rendering/enable_vsync", enabled);

    // Note: VSync changes require application restart to take effect
    // since it's configured at OpenGL context creation time
}

void PerformanceSettingsWidget::ignoreScalingChanged(bool enabled) {
    qDebug() << "PerformanceSettingsWidget: Ignore system scaling changed to" << enabled;
    Appearance::setIgnoreSystemScaling(enabled);
}

void PerformanceSettingsWidget::ignoreFontScalingChanged(bool enabled) {
    qDebug() << "PerformanceSettingsWidget: Ignore font scaling changed to" << enabled;
    Appearance::setIgnoreFontScaling(enabled);
}

void PerformanceSettingsWidget::roundedScalingChanged(bool enabled) {
    qDebug() << "PerformanceSettingsWidget: Rounded scaling changed to" << enabled;
    Appearance::setUseRoundedScaling(enabled);
}

void PerformanceSettingsWidget::resetToDefaults() {
    // Rendering quality defaults
    _enableAntialiasing->setChecked(true);
    _enableSmoothPixmapTransform->setChecked(true);

    // Hardware acceleration defaults
    _enableHardwareAcceleration->setChecked(false); // Default to software to avoid crashes
    _enableHardwareSmoothTransforms->setChecked(true); // Default to enabled for better visual quality
    _multisamplingCombo->setCurrentIndex(1); // 2x MSAA
    _enableVSync->setChecked(false); // Default to OFF for maximum responsiveness

    // DPI scaling defaults (all off)
    _ignoreSystemUIScaling->setChecked(false);
    _ignoreSystemFontScaling->setChecked(false);
    _useRoundedScaling->setChecked(false);
}
