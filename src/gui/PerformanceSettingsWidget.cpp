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
      , _settings(settings) {
    setupUI();
    loadSettings();
    updateInfoLabels();
}

void PerformanceSettingsWidget::setupUI() {
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    setLayout(mainLayout);

    // System & Performance info
    QString infoText = tr("Configure system display settings and rendering performance for optimal MIDI editing experience.");
    _infoBox = createInfoBox(infoText);
    mainLayout->addWidget(_infoBox);

    // High DPI Scaling Group
    QGroupBox *scalingGroup = new QGroupBox(tr("High DPI Scaling"), this);
    QGridLayout *scalingLayout = new QGridLayout(scalingGroup);

    QCheckBox *ignoreScaling = new QCheckBox(tr("Ignore system scaling"), this);
    ignoreScaling->setChecked(Appearance::ignoreSystemScaling());
    ignoreScaling->setToolTip(tr("Disable high DPI scaling (requires restart)"));
    connect(ignoreScaling, &QCheckBox::toggled, this, &PerformanceSettingsWidget::ignoreScalingChanged);
    scalingLayout->addWidget(ignoreScaling, 0, 0, 1, 2);

    QLabel *ignoreDesc = new QLabel(tr("Provides smallest UI but may be hard to read on high DPI displays. (requires restart)"), this);
    ignoreDesc->setWordWrap(true);
    ignoreDesc->setStyleSheet("color: gray; font-size: 10px; margin-left: 10px;");
    scalingLayout->addWidget(ignoreDesc, 1, 0, 1, 2);

    QCheckBox *roundedScaling = new QCheckBox(tr("Use rounded scaling behavior"), this);
    roundedScaling->setChecked(Appearance::useRoundedScaling());
    roundedScaling->setToolTip(tr("Use integer scaling instead of fractional (requires restart)"));
    connect(roundedScaling, &QCheckBox::toggled, this, &PerformanceSettingsWidget::roundedScalingChanged);
    scalingLayout->addWidget(roundedScaling, 2, 0, 1, 2);

    QLabel *roundedDesc = new QLabel(tr("Integer scaling (100%, 200%) provides sharper text than fractional scaling (125%, 150%). (requires restart)"), this);
    roundedDesc->setWordWrap(true);
    roundedDesc->setStyleSheet("color: gray; font-size: 10px; margin-left: 10px;");
    scalingLayout->addWidget(roundedDesc, 3, 0, 1, 2);

    mainLayout->addWidget(scalingGroup);

    // Rendering Quality Group
    _renderingQualityGroup = new QGroupBox(tr("Rendering Quality"), this);
    QGridLayout *qualityLayout = new QGridLayout(_renderingQualityGroup);

    _enableAntialiasing = new QCheckBox(tr("Enable anti-aliasing"), this);
    _enableAntialiasing->setToolTip(tr("Provides smoother edges but reduces performance"));
    connect(_enableAntialiasing, &QCheckBox::toggled, this, &PerformanceSettingsWidget::enableAntialiasingChanged);
    qualityLayout->addWidget(_enableAntialiasing, 0, 0, 1, 2);

    _enableSmoothPixmapTransform = new QCheckBox(tr("Enable smooth pixmap transforms"), this);
    _enableSmoothPixmapTransform->setToolTip(tr("Provides smoother scaling but reduces performance"));
    connect(_enableSmoothPixmapTransform, &QCheckBox::toggled, this, &PerformanceSettingsWidget::enableSmoothPixmapTransformChanged);
    qualityLayout->addWidget(_enableSmoothPixmapTransform, 1, 0, 1, 2);

    mainLayout->addWidget(_renderingQualityGroup);

    // Hardware Acceleration Group
    _hardwareAccelerationGroup = new QGroupBox(tr("Hardware Acceleration"), this);
    QGridLayout *accelLayout = new QGridLayout(_hardwareAccelerationGroup);

    _enableHardwareAcceleration = new QCheckBox(tr("Enable GPU acceleration for MIDI events"), this);
    _enableHardwareAcceleration->setToolTip(tr("Use Qt RHI to automatically select the best graphics API (D3D12/D3D11/Vulkan/OpenGL) for GPU-accelerated MIDI rendering"));
    connect(_enableHardwareAcceleration, &QCheckBox::toggled, this, &PerformanceSettingsWidget::enableHardwareAccelerationChanged);
    accelLayout->addWidget(_enableHardwareAcceleration, 0, 0, 1, 2);

    QLabel *accelDesc = new QLabel(tr("GPU acceleration uses Qt RHI (Rendering Hardware Interface) to automatically select the best graphics API: D3D12/D3D11 on Windows, Vulkan/OpenGL on other platforms, with fallback to software rendering."), this);
    accelDesc->setWordWrap(true);
    accelDesc->setStyleSheet("color: gray; font-size: 10px; margin-left: 10px;");
    accelLayout->addWidget(accelDesc, 1, 0, 1, 2);

    _backendInfoLabel = new QLabel(this);
    _backendInfoLabel->setWordWrap(true);
    accelLayout->addWidget(_backendInfoLabel, 3, 0, 1, 2);

    mainLayout->addWidget(_hardwareAccelerationGroup);

    // Reset button
    QPushButton *resetButton = new QPushButton(tr("Reset to Defaults"), this);
    connect(resetButton, &QPushButton::clicked, this, &PerformanceSettingsWidget::resetToDefaults);
    mainLayout->addWidget(resetButton);

    mainLayout->addStretch();
}

void PerformanceSettingsWidget::loadSettings() {
    // Load rendering quality settings (default to high quality)
    _enableAntialiasing->setChecked(_settings->value("rendering/antialiasing", true).toBool());
    _enableSmoothPixmapTransform->setChecked(_settings->value("rendering/smooth_pixmap_transform", true).toBool());

    // Load hardware acceleration settings (default to software rendering for stability)
    _enableHardwareAcceleration->setChecked(_settings->value("rendering/hardware_acceleration", false).toBool());

    // Apply the enable/disable logic for software-only options
    enableHardwareAccelerationChanged(_enableHardwareAcceleration->isChecked());
}

bool PerformanceSettingsWidget::accept() {
    // Save rendering quality settings
    _settings->setValue("rendering/antialiasing", _enableAntialiasing->isChecked());
    _settings->setValue("rendering/smooth_pixmap_transform", _enableSmoothPixmapTransform->isChecked());

    // Save hardware acceleration settings
    _settings->setValue("rendering/hardware_acceleration", _enableHardwareAcceleration->isChecked());

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

// Slot implementations
void PerformanceSettingsWidget::enableAntialiasingChanged(bool enabled) {
    Q_UNUSED(enabled)
    updateInfoLabels();
}

void PerformanceSettingsWidget::enableSmoothPixmapTransformChanged(bool enabled) {
    Q_UNUSED(enabled)
    updateInfoLabels();
}

void PerformanceSettingsWidget::enableHardwareAccelerationChanged(bool enabled) {
    // Enable/disable software-only options based on hardware acceleration setting
    // (Currently no software-only options remaining)

    updateInfoLabels();
}

void PerformanceSettingsWidget::ignoreScalingChanged(bool enabled) {
    Appearance::setIgnoreSystemScaling(enabled);
}

void PerformanceSettingsWidget::roundedScalingChanged(bool enabled) {
    Appearance::setUseRoundedScaling(enabled);
}

void PerformanceSettingsWidget::resetToDefaults() {
    _enableAntialiasing->setChecked(true);
    _enableSmoothPixmapTransform->setChecked(true);

    // Default to software rendering for stability
    _enableHardwareAcceleration->setChecked(false);

    updateInfoLabels();
}

void PerformanceSettingsWidget::updateInfoLabels() {
    // Update hardware acceleration info
    if (_enableHardwareAcceleration->isChecked()) {
#ifdef Q_OS_WIN
        _backendInfoLabel->setText(tr("Qt RHI will automatically select D3D12 (best), D3D11, Vulkan, or OpenGL, with fallback to software MatrixWidget if needed."));
#else
        _backendInfoLabel->setText(tr("Qt RHI will automatically select Vulkan (best) or OpenGL, with fallback to software MatrixWidget if needed."));
#endif
    } else {
        _backendInfoLabel->setText(tr("Will use the original software MatrixWidget. All drawing is done by the CPU using QPainter."));
    }
}
