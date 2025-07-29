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

#ifndef PERFORMANCESETTINGSWIDGET_H
#define PERFORMANCESETTINGSWIDGET_H

#include "SettingsWidget.h"
#include <QWidget>
#include <QSettings>

class QCheckBox;
class QComboBox;
class QSpinBox;
class QSlider;
class QLabel;
class QGroupBox;

/**
 * @brief Settings widget for performance and rendering optimizations
 * 
 * This widget allows users to configure Qt6 rendering optimizations,
 * and hardware acceleration preferences.
 */
class PerformanceSettingsWidget : public SettingsWidget {
    Q_OBJECT

public:
    explicit PerformanceSettingsWidget(QSettings* settings, QWidget* parent = nullptr);
    bool accept() override;
    QIcon icon() override;

public slots:
    void refreshColors();

private slots:
    // Rendering quality settings
    void enableSmoothPixmapTransformChanged(bool enabled);
    void enableLosslessImageRenderingChanged(bool enabled);
    void enableOptimizedCompositionChanged(bool enabled);
    
    // Hardware acceleration settings
    void enableHardwareAccelerationChanged(bool enabled);
    void enableAsyncRenderingChanged(bool enabled);

    // DPI scaling settings
    void ignoreScalingChanged(bool enabled);
    void roundedScalingChanged(bool enabled);

    // Reset to defaults
    void resetToDefaults();

private:
    void setupUI();
    void loadSettings();
    void updateInfoLabels();
    QString getBackendDescription(const QString& backend) const;
    
    QSettings* _settings;
    
    // Rendering quality controls
    QGroupBox* _renderingQualityGroup;
    QCheckBox* _enableSmoothPixmapTransform;
    QCheckBox* _enableLosslessImageRendering;
    QCheckBox* _enableOptimizedComposition;
    
    // Hardware acceleration controls
    QGroupBox* _hardwareAccelerationGroup;
    QCheckBox* _enableHardwareAcceleration;
    QCheckBox* _enableAsyncRendering;
    QLabel* _backendInfoLabel;
};

#endif // PERFORMANCESETTINGSWIDGET_H
