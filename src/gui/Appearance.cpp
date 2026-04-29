#include "Appearance.h"
#include <QStyleFactory>
#include <QStyleHints>
#include <QTimer>
#include <QDateTime>
#include <QFile>
#include "../tool/ToolButton.h"
#include "ProtocolWidget.h"
#include "MatrixWidget.h"
#include "AppearanceSettingsWidget.h"
#include "MidiSettingsWidget.h"
#include "InstrumentSettingsWidget.h"
#include "PerformanceSettingsWidget.h"
#include "TrackListWidget.h"
#include "ChannelListWidget.h"
#include "ControlChangeSettingsWidget.h"
#include "ThemePalettes.h"

#ifdef Q_OS_WIN
#include <dwmapi.h>

class AppearanceEventFilter : public QObject {
public:
    AppearanceEventFilter(QObject *parent = nullptr) : QObject(parent) {}
    bool eventFilter(QObject *watched, QEvent *event) override {
        if (event->type() == QEvent::Show) {
            QWidget *widget = qobject_cast<QWidget *>(watched);
            if (widget && widget->isWindow()) {
                Appearance::applyTitleBarColor(widget);
            }
        }
        return QObject::eventFilter(watched, event);
    }
};

static AppearanceEventFilter *g_appearanceEventFilter = nullptr;

#endif

// Prevent rapid successive theme changes
static QDateTime lastThemeChange;

// Simple cache to prevent infinite loops in shouldUseDarkMode
static bool darkModeResult = false;
static QString cachedStyle = "";
static QDateTime cacheTime;

// Queue-based icon processing to prevent crashes
static QTimer *iconUpdateTimer = nullptr;
static QList<QPair<QAction *, QString> > iconUpdateQueue;

// PERFORMANCE: Use QHash instead of QMap for O(1) color lookups instead of O(log n)
QHash<int, QColor *> Appearance::channelColors = QHash<int, QColor *>();
QHash<int, QColor *> Appearance::trackColors = QHash<int, QColor *>();
QSet<int> Appearance::customChannelColors = QSet<int>();
QSet<int> Appearance::customTrackColors = QSet<int>();
QMap<QAction *, QString> Appearance::registeredIconActions = QMap<QAction *, QString>();

// Icon caching for performance on theme switch
static QHash<QString, QIcon> s_iconCache;
static Appearance::ApplicationTheme s_lastIconTheme = Appearance::ThemeAuto;
static bool s_lastDarkMode = false;

int Appearance::_opacity = 100;
Appearance::stripStyle Appearance::_strip = Appearance::onSharp;
bool Appearance::_showRangeLines = false;
bool Appearance::_showProgramChangeMarkers = false;
bool Appearance::_showControlChangeMarkers = false;
bool Appearance::_showTextEventMarkers = false;
Appearance::MarkerColorMode Appearance::_markerColorMode = Appearance::ColorByTrack;
bool Appearance::_showMarkerGuideLines = true;
Appearance::ColorPreset Appearance::_colorPreset = Appearance::PresetDefault;
Appearance::ApplicationTheme Appearance::_applicationTheme = Appearance::ThemeAuto;
bool Appearance::_smoothPlaybackScrolling = false;

QString Appearance::_applicationStyle = "windowsvista";
int Appearance::_toolbarIconSize = 20;
bool Appearance::_ignoreSystemScaling = false;
bool Appearance::_ignoreFontScaling = false;
bool Appearance::_useRoundedScaling = false;
int Appearance::_msaaSamples = 2;
bool Appearance::_enableVSync = false;
bool Appearance::_useHardwareAcceleration = false;
bool Appearance::_toolbarTwoRowMode = false;
bool Appearance::_toolbarCustomizeEnabled = false;
QStringList Appearance::_toolbarActionOrder = QStringList();
QStringList Appearance::_toolbarEnabledActions = QStringList();
bool Appearance::_toolbarVisible = true;
bool Appearance::_shuttingDown = false;

void Appearance::init(QSettings *settings) {
    // CRITICAL: Load application style FIRST before creating any colors
    _opacity = settings->value("appearance_opacity", 100).toInt();
    _strip = static_cast<Appearance::stripStyle>(settings->value("strip_style", Appearance::onSharp).toInt());
    _showRangeLines = settings->value("show_range_lines", false).toBool();
    _showProgramChangeMarkers = settings->value("show_program_change_markers", false).toBool();
    _showControlChangeMarkers = settings->value("show_control_change_markers", false).toBool();
    _showTextEventMarkers = settings->value("show_text_event_markers", false).toBool();
    _markerColorMode = static_cast<Appearance::MarkerColorMode>(settings->value("marker_color_mode", Appearance::ColorByTrack).toInt());
    _showMarkerGuideLines = settings->value("show_marker_guide_lines", true).toBool();
    _colorPreset = static_cast<Appearance::ColorPreset>(settings->value("color_preset", Appearance::PresetDefault).toInt());
    _applicationTheme = static_cast<Appearance::ApplicationTheme>(settings->value("application_theme", Appearance::ThemeAuto).toInt());
    _smoothPlaybackScrolling = settings->value("rendering/smooth_playback_scroll", false).toBool();

    // Set default style with fallback
    QString defaultStyle = "windowsvista";
    QStringList availableStyles = QStyleFactory::keys();
    if (!availableStyles.contains(defaultStyle, Qt::CaseInsensitive)) {
        // Fallback order: windows -> fusion -> first available
        if (availableStyles.contains("windows", Qt::CaseInsensitive)) {
            defaultStyle = "windows";
        } else if (availableStyles.contains("fusion", Qt::CaseInsensitive)) {
            defaultStyle = "fusion";
        } else if (!availableStyles.isEmpty()) {
            defaultStyle = availableStyles.first();
        }
    }
    _applicationStyle = settings->value("application_style", defaultStyle).toString();
    _toolbarIconSize = settings->value("toolbar_icon_size", 20).toInt();
    _ignoreSystemScaling = settings->value("ignore_system_scaling", false).toBool();
    _ignoreFontScaling = settings->value("ignore_font_scaling", false).toBool();
    _useRoundedScaling = settings->value("use_rounded_scaling", false).toBool();
    _toolbarTwoRowMode = settings->value("toolbar_two_row_mode", false).toBool();
    _toolbarCustomizeEnabled = settings->value("toolbar_customize_enabled", false).toBool();
    _toolbarActionOrder = settings->value("toolbar_action_order", QStringList()).toStringList();
    _toolbarEnabledActions = settings->value("toolbar_enabled_actions", QStringList()).toStringList();
    _toolbarVisible = settings->value("toolbar_visible", true).toBool();

    // Load custom color tracking FIRST
    QList<QVariant> customChannels = settings->value("custom_channel_colors", QList<QVariant>()).toList();
    foreach(const QVariant& var, customChannels) {
        int channel = var.toInt();
        customChannelColors.insert(channel);
    }

    QList<QVariant> customTracks = settings->value("custom_track_colors", QList<QVariant>()).toList();
    foreach(const QVariant& var, customTracks) {
        int track = var.toInt();
        customTrackColors.insert(track);
    }

    // Load colors: Custom colors from settings, default colors for current theme
    for (int channel = 0; channel < 17; channel++) {
        if (customChannelColors.contains(channel)) {
            // User has customized this channel - load their custom color from settings
            QColor *customColor = decode("channel_color_" + QString::number(channel),
                                         settings, defaultColor(channel));
            channelColors.insert(channel, customColor);
        } else {
            // User has NOT customized this channel - use current theme's default
            QColor *themeDefaultColor = defaultColor(channel);
            channelColors.insert(channel, themeDefaultColor);
        }
    }

    for (int track = 0; track < 17; track++) {
        if (customTrackColors.contains(track)) {
            // User has customized this track - load their custom color from settings
            QColor *customColor = decode("track_color_" + QString::number(track),
                                         settings, defaultColor(track));
            trackColors.insert(track, customColor);
        } else {
            // User has NOT customized this track - use current theme's default
            QColor *themeDefaultColor = defaultColor(track);
            trackColors.insert(track, themeDefaultColor);
        }
    }

    // Validate custom color markings against actual colors
    QSet<int> invalidCustomChannels;
    foreach(int channel, customChannelColors) {
        QColor *currentColor = channelColors.value(channel, nullptr);
        QColor *defaultColor = Appearance::defaultColor(channel);
        if (currentColor && defaultColor) {
            bool actuallyCustom = (currentColor->red() != defaultColor->red() ||
                                   currentColor->green() != defaultColor->green() ||
                                   currentColor->blue() != defaultColor->blue());
            if (!actuallyCustom) {
                invalidCustomChannels.insert(channel);
            }
        }
        if (defaultColor) delete defaultColor;
    }

    QSet<int> invalidCustomTracks;
    foreach(int track, customTrackColors) {
        QColor *currentColor = trackColors.value(track, nullptr);
        QColor *defaultColor = Appearance::defaultColor(track);
        if (currentColor && defaultColor) {
            bool actuallyCustom = (currentColor->red() != defaultColor->red() ||
                                   currentColor->green() != defaultColor->green() ||
                                   currentColor->blue() != defaultColor->blue());
            if (!actuallyCustom) {
                invalidCustomTracks.insert(track);
            }
        }
        if (defaultColor) delete defaultColor;
    }

    // Remove invalid custom markings
    foreach(int channel, invalidCustomChannels) {
        customChannelColors.remove(channel);
    }
    foreach(int track, invalidCustomTracks) {
        customTrackColors.remove(track);
    }

    // Apply the style after loading settings
    applyStyle();

    // Force initial color refresh to ensure colors match current theme
    autoResetDefaultColors();

    // Connect to system theme changes
    connectToSystemThemeChanges();
}

QColor *Appearance::channelColor(int channel) {
    int index = channelToColorIndex(channel);

    // Get existing color or create default for current theme
    QColor *color = channelColors.value(index, nullptr);
    if (!color) {
        color = defaultColor(index);
        channelColors[index] = color;
    }

    // PERFORMANCE: Apply opacity setting to the color only if it has changed
    // This avoids expensive setAlpha() calls during rendering
    int targetAlpha = _opacity * 255 / 100;
    if (color->alpha() != targetAlpha) {
        color->setAlpha(targetAlpha);
    }

    return color;
}

QColor *Appearance::trackColor(int track) {
    int index = trackToColorIndex(track);

    // Get existing color or create default for current theme
    QColor *color = trackColors.value(index, nullptr);
    if (!color) {
        color = defaultColor(index);
        trackColors[index] = color;
    }

    // PERFORMANCE: Apply opacity setting to the color only if it has changed
    // This avoids expensive setAlpha() calls during rendering
    int targetAlpha = _opacity * 255 / 100;
    if (color->alpha() != targetAlpha) {
        color->setAlpha(targetAlpha);
    }

    return color;
}

void Appearance::writeSettings(QSettings *settings) {
    for (int channel = 0; channel < 17; channel++) {
        QColor *color = channelColors.value(channel, nullptr);
        if (color) {
            write("channel_color_" + QString::number(channel), settings, color);
        }
    }
    for (int track = 0; track < 17; track++) {
        QColor *color = trackColors.value(track, nullptr);
        if (color) {
            write("track_color_" + QString::number(track), settings, color);
        }
    }
    settings->setValue("appearance_opacity", _opacity);
    settings->setValue("strip_style", _strip);
    settings->setValue("show_range_lines", _showRangeLines);
    settings->setValue("show_program_change_markers", _showProgramChangeMarkers);
    settings->setValue("show_control_change_markers", _showControlChangeMarkers);
    settings->setValue("show_text_event_markers", _showTextEventMarkers);
    settings->setValue("marker_color_mode", static_cast<int>(_markerColorMode));
    settings->setValue("show_marker_guide_lines", _showMarkerGuideLines);
    settings->setValue("color_preset", static_cast<int>(_colorPreset));
    settings->setValue("application_theme", static_cast<int>(_applicationTheme));
    settings->setValue("rendering/smooth_playback_scroll", _smoothPlaybackScrolling);
    settings->setValue("application_style", _applicationStyle);
    settings->setValue("toolbar_icon_size", _toolbarIconSize);
    settings->setValue("ignore_system_scaling", _ignoreSystemScaling);
    settings->setValue("ignore_font_scaling", _ignoreFontScaling);
    settings->setValue("use_rounded_scaling", _useRoundedScaling);
    settings->setValue("toolbar_two_row_mode", _toolbarTwoRowMode);
    settings->setValue("toolbar_customize_enabled", _toolbarCustomizeEnabled);
    settings->setValue("toolbar_action_order", _toolbarActionOrder);
    settings->setValue("toolbar_enabled_actions", _toolbarEnabledActions);
    settings->setValue("toolbar_visible", _toolbarVisible);

    // Save custom color tracking
    QList<QVariant> customChannels;
    foreach(int channel, customChannelColors) {
        customChannels.append(channel);
    }
    settings->setValue("custom_channel_colors", customChannels);

    QList<QVariant> customTracks;
    foreach(int track, customTrackColors) {
        customTracks.append(track);
    }
    settings->setValue("custom_track_colors", customTracks);
}

QColor *Appearance::defaultColor(int n) {
    if (_colorPreset == PresetPastel) {
        switch (n) {
            case 0: return new QColor(255, 179, 186, 255);
            case 1: return new QColor(255, 223, 186, 255);
            case 2: return new QColor(255, 255, 186, 255);
            case 3: return new QColor(186, 255, 201, 255);
            case 4: return new QColor(186, 225, 255, 255);
            case 5: return new QColor(210, 186, 255, 255);
            case 6: return new QColor(255, 186, 225, 255);
            case 7: return new QColor(186, 255, 255, 255);
            case 8: return new QColor(220, 235, 186, 255);
            case 9: return new QColor(180, 180, 180, 255);
            case 10: return new QColor(255, 204, 229, 255);
            case 11: return new QColor(204, 229, 255, 255);
            case 12: return new QColor(204, 255, 204, 255);
            case 13: return new QColor(255, 229, 204, 255);
            case 14: return new QColor(229, 204, 255, 255);
            case 15: return new QColor(220, 240, 240, 255);
            default: return new QColor(200, 200, 200, 255);
        }
    } else if (_colorPreset == PresetVibrant) {
        switch (n) {
            case 0: return new QColor(255, 0, 0, 255);
            case 1: return new QColor(255, 128, 0, 255);
            case 2: return new QColor(255, 255, 0, 255);
            case 3: return new QColor(0, 255, 0, 255);
            case 4: return new QColor(0, 255, 255, 255);
            case 5: return new QColor(0, 0, 255, 255);
            case 6: return new QColor(255, 0, 255, 255);
            case 7: return new QColor(128, 0, 255, 255);
            case 8: return new QColor(0, 255, 128, 255);
            case 9: return new QColor(128, 128, 128, 255);
            case 10: return new QColor(255, 0, 128, 255);
            case 11: return new QColor(0, 128, 255, 255);
            case 12: return new QColor(128, 255, 0, 255);
            case 13: return new QColor(255, 200, 0, 255);
            case 14: return new QColor(128, 0, 128, 255);
            case 15: return new QColor(0, 128, 128, 255);
            default: return new QColor(100, 100, 100, 255);
        }
    } else if (_colorPreset == PresetAccessible) {
        // High contrast / Colorblind safe scheme (Okabe-Ito / categorical)
        switch (n) {
            case 0: return new QColor(213, 94, 0, 255);    // Vermilion
            case 1: return new QColor(230, 159, 0, 255);   // Orange
            case 2: return new QColor(240, 228, 66, 255);  // Yellow
            case 3: return new QColor(0, 158, 115, 255);   // Bluish Green
            case 4: return new QColor(86, 180, 233, 255);  // Sky Blue
            case 5: return new QColor(0, 114, 178, 255);   // Blue
            case 6: return new QColor(204, 121, 167, 255); // Reddish Purple
            case 7: return new QColor(176, 224, 230, 255); // Powder Blue
            case 8: return new QColor(0, 0, 0, 255);       // Black
            case 9: return new QColor(128, 128, 128, 255); // Gray
            case 10: return new QColor(255, 165, 0, 255);  // Vivid Orange
            case 11: return new QColor(144, 238, 144, 255);// Light Green
            case 12: return new QColor(255, 69, 0, 255);   // Red Orange
            case 13: return new QColor(186, 85, 211, 255); // Orchid
            case 14: return new QColor(46, 139, 87, 255);  // Sea Green
            case 15: return new QColor(160, 82, 45, 255);  // Sienna
            default: return new QColor(100, 100, 100, 255);
        }
    } else if (_colorPreset == PresetFLStudio) {
        // FL Studio inspired (Fruity/punchy colors)
        switch (n) {
            case 0: return new QColor(135, 222, 104, 255); // Apple Green
            case 1: return new QColor(255, 148, 77, 255);  // Mango
            case 2: return new QColor(255, 102, 102, 255); // Strawberry
            case 3: return new QColor(102, 204, 255, 255); // Frost
            case 4: return new QColor(204, 153, 255, 255); // Grape
            case 5: return new QColor(255, 236, 102, 255); // Banana
            case 6: return new QColor(102, 255, 178, 255); // Mint
            case 7: return new QColor(255, 102, 178, 255); // Raspberry
            case 8: return new QColor(224, 224, 224, 255); // Silver
            case 9: return new QColor(153, 153, 153, 255); // Iron
            case 10: return new QColor(255, 178, 102, 255);// Tangerine
            case 11: return new QColor(153, 204, 255, 255);// Sky
            case 12: return new QColor(204, 255, 153, 255);// Lime
            case 13: return new QColor(255, 153, 204, 255);// Bubblegum
            case 14: return new QColor(153, 255, 204, 255);// Seafoam
            case 15: return new QColor(204, 153, 102, 255);// Bronze
            default: return new QColor(150, 150, 150, 255);
        }
    } else if (_colorPreset == PresetAbleton) {
        // Ableton Live inspired (Muted, utilitarian, flat)
        switch (n) {
            case 0: return new QColor(219, 112, 147, 255); // Palevioletred
            case 1: return new QColor(143, 188, 143, 255); // Darkseagreen
            case 2: return new QColor(244, 164, 96, 255);  // Sandybrown
            case 3: return new QColor(100, 149, 237, 255); // Cornflower
            case 4: return new QColor(205, 92, 92, 255);   // Indianred
            case 5: return new QColor(218, 165, 32, 255);  // Goldenrod
            case 6: return new QColor(95, 158, 160, 255);  // Cadetblue
            case 7: return new QColor(138, 43, 226, 255);  // Blueviolet
            case 8: return new QColor(112, 128, 144, 255); // Slate
            case 9: return new QColor(188, 143, 143, 255); // Rosybrown
            case 10: return new QColor(240, 128, 128, 255);// Lightcoral
            case 11: return new QColor(32, 178, 170, 255); // Lightseagreen
            case 12: return new QColor(222, 184, 135, 255);// Burlywood
            case 13: return new QColor(176, 196, 222, 255);// Lightsteelblue
            case 14: return new QColor(255, 182, 193, 255);// Lightpink
            case 15: return new QColor(107, 142, 35, 255); // Olive
            default: return new QColor(128, 128, 128, 255);
        }
    } else if (_colorPreset == PresetLogic) {
        // Logic Pro inspired (Clean, distinct distinct Apple hues)
        switch (n) {
            case 0: return new QColor(93, 172, 228, 255);  // Aqua Blue
            case 1: return new QColor(114, 204, 102, 255); // Clean Green
            case 2: return new QColor(230, 199, 58, 255);  // Gold
            case 3: return new QColor(213, 99, 93, 255);   // Crimson
            case 4: return new QColor(160, 116, 196, 255); // Purple
            case 5: return new QColor(234, 147, 88, 255);  // Orange
            case 6: return new QColor(87, 196, 186, 255);  // Teal
            case 7: return new QColor(228, 128, 170, 255); // Pink
            case 8: return new QColor(200, 200, 205, 255); // Gray
            case 9: return new QColor(140, 148, 158, 255); // Steel
            case 10: return new QColor(58, 125, 222, 255); // Royal Blue
            case 11: return new QColor(175, 218, 102, 255);// Yellow-Green
            case 12: return new QColor(250, 115, 150, 255);// Hot Pink
            case 13: return new QColor(128, 100, 162, 255);// Deep Purple
            case 14: return new QColor(96, 180, 150, 255); // Sea Green
            case 15: return new QColor(200, 160, 100, 255);// Sand
            default: return new QColor(160, 160, 160, 255);
        }
    }

    bool useDarkMode = shouldUseDarkMode();

    QColor *color;

    if (useDarkMode) {
        // Darker, more muted colors for dark mode (slightly darker shade)
        switch (n) {
            case 0: {
                color = new QColor(180, 80, 60, 255);
                break;
            }
            case 1: {
                color = new QColor(130, 160, 0, 255);
                break;
            }
            case 2: {
                color = new QColor(25, 130, 5, 255);
                break;
            }
            case 3: {
                color = new QColor(60, 160, 150, 255);
                break;
            }
            case 4: {
                color = new QColor(110, 85, 140, 255);
                break;
            }
            case 5: {
                color = new QColor(160, 80, 130, 255);
                break;
            }
            case 6: {
                color = new QColor(110, 140, 110, 255);
                break;
            }
            case 7: {
                color = new QColor(150, 130, 110, 255);
                break;
            }
            case 8: {
                color = new QColor(160, 130, 5, 255);
                break;
            }
            case 9: {
                color = new QColor(120, 120, 120, 255);
                break;
            }
            case 10: {
                color = new QColor(130, 25, 80, 255);
                break;
            }
            case 11: {
                color = new QColor(70, 120, 200, 255);
                break;
            }
            case 12: {
                color = new QColor(90, 140, 70, 255);
                break;
            }
            case 13: {
                color = new QColor(160, 100, 40, 255);
                break;
            }
            case 14: {
                color = new QColor(60, 15, 60, 255);
                break;
            }
            case 15: {
                color = new QColor(25, 80, 80, 255);
                break;
            }
            default: {
                color = new QColor(80, 120, 200, 255);
                break;
            }
        }
    } else {
        // Original bright colors for light mode
        switch (n) {
            case 0: {
                color = new QColor(241, 70, 57, 255);
                break;
            }
            case 1: {
                color = new QColor(205, 241, 0, 255);
                break;
            }
            case 2: {
                color = new QColor(50, 201, 20, 255);
                break;
            }
            case 3: {
                color = new QColor(107, 241, 231, 255);
                break;
            }
            case 4: {
                color = new QColor(127, 67, 255, 255);
                break;
            }
            case 5: {
                color = new QColor(241, 127, 200, 255);
                break;
            }
            case 6: {
                color = new QColor(170, 212, 170, 255);
                break;
            }
            case 7: {
                color = new QColor(222, 202, 170, 255);
                break;
            }
            case 8: {
                color = new QColor(241, 201, 20, 255);
                break;
            }
            case 9: {
                color = new QColor(80, 80, 80, 255);
                break;
            }
            case 10: {
                color = new QColor(202, 50, 127, 255);
                break;
            }
            case 11: {
                color = new QColor(0, 132, 255, 255);
                break;
            }
            case 12: {
                color = new QColor(102, 127, 37, 255);
                break;
            }
            case 13: {
                color = new QColor(241, 164, 80, 255);
                break;
            }
            case 14: {
                color = new QColor(107, 30, 107, 255);
                break;
            }
            case 15: {
                color = new QColor(50, 127, 127, 255);
                break;
            }
            default: {
                color = new QColor(50, 50, 255, 255);
                break;
            }
        }
    }

    return color;
}

QColor *Appearance::decode(QString name, QSettings *settings, QColor *defaultColor) {
    bool ok;
    int r = settings->value(name + "_r").toInt(&ok);
    if (!ok) {
        return new QColor(*defaultColor);
    }
    int g = settings->value(name + "_g").toInt(&ok);
    if (!ok) {
        return defaultColor;
    }
    int b = settings->value(name + "_b").toInt(&ok);
    if (!ok) {
        return defaultColor;
    }
    return new QColor(r, g, b);
}

void Appearance::write(QString name, QSettings *settings, QColor *color) {
    settings->setValue(name + "_r", QVariant(color->red()));
    settings->setValue(name + "_g", QVariant(color->green()));
    settings->setValue(name + "_b", QVariant(color->blue()));
}

void Appearance::setTrackColor(int track, QColor color) {
    int index = trackToColorIndex(track);
    QColor *oldColor = trackColors.value(index, nullptr);

    // Apply current opacity setting to the new color
    color.setAlpha(_opacity * 255 / 100);
    trackColors[index] = new QColor(color);

    // Only mark as custom if the color is different from current theme default
    QColor *currentDefault = defaultColor(index);
    bool isCustomColor = (currentDefault->red() != color.red() ||
                          currentDefault->green() != color.green() ||
                          currentDefault->blue() != color.blue());

    if (isCustomColor) {
        customTrackColors.insert(index);
    } else {
        customTrackColors.remove(index);
    }

    delete currentDefault; // Clean up temporary color
    if (oldColor) {
        delete oldColor;
    }
}

void Appearance::setChannelColor(int channel, QColor color) {
    int index = channelToColorIndex(channel);
    QColor *oldColor = channelColors.value(index, nullptr);

    // Apply current opacity setting to the new color
    color.setAlpha(_opacity * 255 / 100);
    channelColors[index] = new QColor(color);

    // Only mark as custom if the color is different from current theme default
    QColor *currentDefault = defaultColor(index);
    bool isCustomColor = (currentDefault->red() != color.red() ||
                          currentDefault->green() != color.green() ||
                          currentDefault->blue() != color.blue());

    if (isCustomColor) {
        customChannelColors.insert(index);
    } else {
        customChannelColors.remove(index);
    }

    delete currentDefault; // Clean up temporary color
    if (oldColor) {
        delete oldColor;
    }
}

int Appearance::trackToColorIndex(int track) {
    int mod = (track - 1) % 17;
    if (mod < 0) {
        mod += 17;
    }
    return mod;
}

int Appearance::channelToColorIndex(int channel) {
    if (channel > 16) {
        channel = 16;
    }
    return channel;
}

void Appearance::reset() {
    // Reset to appropriate colors for current mode (light/dark)
    forceResetAllColors();
    customChannelColors.clear(); // Clear custom color tracking - all colors are now "default"
    customTrackColors.clear(); // Clear custom color tracking - all colors are now "default"
}

void Appearance::autoResetDefaultColors() {
    // Update ONLY non-custom colors to match current theme
    // Custom colors are NEVER changed - they stay exactly as the user set them
    int channelsUpdated = 0;
    for (int channel = 0; channel < 17; channel++) {
        if (!customChannelColors.contains(channel)) {
            // This is a DEFAULT color - update it to match current theme
            QColor *existingColor = channelColors.value(channel, nullptr);
            if (existingColor) {
                QColor oldColor = *existingColor; // Save for logging
                QColor *newThemeDefault = defaultColor(channel);

                // Copy the new theme default values to existing color object
                existingColor->setRgb(newThemeDefault->red(),
                                      newThemeDefault->green(),
                                      newThemeDefault->blue(),
                                      newThemeDefault->alpha());

                // Clean up the temporary color
                delete newThemeDefault;

                channelsUpdated++;
            }
        }
    }

    int tracksUpdated = 0;
    for (int track = 0; track < 17; track++) {
        if (!customTrackColors.contains(track)) {
            // This is a DEFAULT color - update it to match current theme
            QColor *existingColor = trackColors.value(track, nullptr);
            if (existingColor) {
                QColor oldColor = *existingColor; // Save for logging
                QColor *newThemeDefault = defaultColor(track);

                // Copy the new theme default values to existing color object
                existingColor->setRgb(newThemeDefault->red(),
                                      newThemeDefault->green(),
                                      newThemeDefault->blue(),
                                      newThemeDefault->alpha());

                // Clean up the temporary color
                delete newThemeDefault;

                tracksUpdated++;
            }
        }
    }
}

void Appearance::forceResetAllColors() {
    // Force reset all colors to current theme defaults
    for (int channel = 0; channel < 17; channel++) {
        QColor *existingColor = channelColors.value(channel, nullptr);
        if (existingColor) {
            QColor *newDefaultColor = defaultColor(channel);
            if (newDefaultColor) {
                // Copy the color values safely
                int r = newDefaultColor->red();
                int g = newDefaultColor->green();
                int b = newDefaultColor->blue();
                int a = newDefaultColor->alpha();

                // Delete the temporary color BEFORE modifying existing one
                delete newDefaultColor;

                // Now safely update the existing color
                existingColor->setRgb(r, g, b, a);
            }
        }
    }

    for (int track = 0; track < 17; track++) {
        QColor *existingColor = trackColors.value(track, nullptr);
        if (existingColor) {
            QColor *newDefaultColor = defaultColor(track);
            if (newDefaultColor) {
                // Copy the color values safely
                int r = newDefaultColor->red();
                int g = newDefaultColor->green();
                int b = newDefaultColor->blue();
                int a = newDefaultColor->alpha();

                // Delete the temporary color BEFORE modifying existing one
                delete newDefaultColor;

                // Now safely update the existing color
                existingColor->setRgb(r, g, b, a);
            }
        }
    }
}

int Appearance::opacity() {
    return _opacity;
}

void Appearance::setOpacity(int opacity) {
    _opacity = opacity;

    // Update alpha values for all existing colors
    int alphaValue = _opacity * 255 / 100;

    // Update all channel colors
    for (auto it = channelColors.begin(); it != channelColors.end(); ++it) {
        if (it.value()) {
            it.value()->setAlpha(alphaValue);
        }
    }

    // Update all track colors
    for (auto it = trackColors.begin(); it != trackColors.end(); ++it) {
        if (it.value()) {
            it.value()->setAlpha(alphaValue);
        }
    }
}

Appearance::stripStyle Appearance::strip() {
    return _strip;
}

void Appearance::setStrip(Appearance::stripStyle render) {
    _strip = render;
}

bool Appearance::showRangeLines() {
    return _showRangeLines;
}

void Appearance::setShowRangeLines(bool enabled) {
    _showRangeLines = enabled;
}

bool Appearance::showProgramChangeMarkers() {
    return _showProgramChangeMarkers;
}

void Appearance::setShowProgramChangeMarkers(bool enabled) {
    _showProgramChangeMarkers = enabled;
}

bool Appearance::showControlChangeMarkers() {
    return _showControlChangeMarkers;
}

void Appearance::setShowControlChangeMarkers(bool enabled) {
    _showControlChangeMarkers = enabled;
}

bool Appearance::showTextEventMarkers() {
    return _showTextEventMarkers;
}

void Appearance::setShowTextEventMarkers(bool enabled) {
    _showTextEventMarkers = enabled;
}

Appearance::MarkerColorMode Appearance::markerColorMode() {
    return _markerColorMode;
}

void Appearance::setMarkerColorMode(MarkerColorMode mode) {
    _markerColorMode = mode;
}

bool Appearance::showMarkerGuideLines() {
    return _showMarkerGuideLines;
}

void Appearance::setShowMarkerGuideLines(bool enabled) {
    _showMarkerGuideLines = enabled;
}

Appearance::ColorPreset Appearance::colorPreset() {
    return _colorPreset;
}

void Appearance::setColorPreset(Appearance::ColorPreset preset) {
    _colorPreset = preset;
}

Appearance::ApplicationTheme Appearance::applicationTheme() {
    return _applicationTheme;
}

void Appearance::setApplicationTheme(Appearance::ApplicationTheme theme) {
    _applicationTheme = theme;
}

bool Appearance::smoothPlaybackScrolling() {
    return _smoothPlaybackScrolling;
}

void Appearance::setSmoothPlaybackScrolling(bool enabled) {
    _smoothPlaybackScrolling = enabled;
}

QString Appearance::applicationStyle() {
    return _applicationStyle;
}

void Appearance::setApplicationStyle(const QString &style) {
    // Prevent rapid successive theme changes that might cause crashes
    QDateTime now = QDateTime::currentDateTime();
    if (lastThemeChange.isValid() && lastThemeChange.msecsTo(now) < 500) {
        return;
    }
    lastThemeChange = now;

    _applicationStyle = style;

    // Invalidate cache when style changes
    cachedStyle = "";

    applyStyle();

    // Coalesce refreshes through a single shared debounce timer
    // Implemented as a static in this translation unit
    auto queueRefreshColors = [](int delayMs) {
        static QTimer *s_refreshDebounceTimer = nullptr;
        if (!s_refreshDebounceTimer) {
            s_refreshDebounceTimer = new QTimer();
            s_refreshDebounceTimer->setSingleShot(true);
            QObject::connect(s_refreshDebounceTimer, &QTimer::timeout, []() {
                refreshColors();
            });
        }
        s_refreshDebounceTimer->stop();
        s_refreshDebounceTimer->start(delayMs);
    };

    // Request a single refresh soon (no stacked timers)
    queueRefreshColors(120);
}

int Appearance::toolbarIconSize() {
    return _toolbarIconSize;
}

void Appearance::setToolbarIconSize(int size) {
    _toolbarIconSize = size;
    notifyIconSizeChanged();
}

bool Appearance::ignoreSystemScaling() {
    return _ignoreSystemScaling;
}

void Appearance::setIgnoreSystemScaling(bool ignore) {
    qDebug() << "Appearance: Setting ignore system scaling to" << ignore;
    _ignoreSystemScaling = ignore;
    // Note: This setting requires application restart to take effect
}

bool Appearance::ignoreFontScaling() {
    return _ignoreFontScaling;
}

void Appearance::setIgnoreFontScaling(bool ignore) {
    qDebug() << "Appearance: Setting ignore font scaling to" << ignore;
    _ignoreFontScaling = ignore;
    // Note: This setting requires application restart to take effect
}

bool Appearance::useRoundedScaling() {
    return _useRoundedScaling;
}

void Appearance::setUseRoundedScaling(bool useRounded) {
    qDebug() << "Appearance: Setting use rounded scaling to" << useRounded;
    _useRoundedScaling = useRounded;
    // Note: This setting requires application restart to take effect
}

int Appearance::msaaSamples() {
    return _msaaSamples;
}

bool Appearance::enableVSync() {
    return _enableVSync;
}

bool Appearance::useHardwareAcceleration() {
    return _useHardwareAcceleration;
}

void Appearance::loadEarlySettings() {
    // Load only the settings needed before QApplication is created
    QSettings settings(QString("MidiEditor"), QString("NONE"));
    _ignoreSystemScaling = settings.value("ignore_system_scaling", false).toBool();
    _ignoreFontScaling = settings.value("ignore_font_scaling", false).toBool();
    _useRoundedScaling = settings.value("use_rounded_scaling", false).toBool();
    _msaaSamples = settings.value("rendering/msaa_samples", 2).toInt();
    _enableVSync = settings.value("rendering/enable_vsync", false).toBool();
    _useHardwareAcceleration = settings.value("rendering/hardware_acceleration", false).toBool();

    qDebug() << "Appearance::loadEarlySettings() - Loaded values:";
    qDebug() << "  ignore_system_scaling:" << _ignoreSystemScaling;
    qDebug() << "  ignore_font_scaling:" << _ignoreFontScaling;
    qDebug() << "  use_rounded_scaling:" << _useRoundedScaling;
    qDebug() << "  msaa_samples:" << _msaaSamples;
    qDebug() << "  enable_vsync:" << _enableVSync;
    qDebug() << "  use_hardware_acceleration:" << _useHardwareAcceleration;
}

QFont Appearance::improveFont(const QFont &font) {
    QFont improvedFont = font;
    improvedFont.setHintingPreference(QFont::PreferFullHinting);
    improvedFont.setStyleStrategy(QFont::PreferAntialias);
    return improvedFont;
}

bool Appearance::toolbarTwoRowMode() {
    return _toolbarTwoRowMode;
}

void Appearance::setToolbarTwoRowMode(bool twoRows) {
    _toolbarTwoRowMode = twoRows;
}

bool Appearance::toolbarVisible() {
    return _toolbarVisible;
}

void Appearance::setToolbarVisible(bool visible) {
    _toolbarVisible = visible;
}

bool Appearance::toolbarCustomizeEnabled() {
    return _toolbarCustomizeEnabled;
}

void Appearance::setToolbarCustomizeEnabled(bool enabled) {
    _toolbarCustomizeEnabled = enabled;
}

QStringList Appearance::toolbarActionOrder() {
    return _toolbarActionOrder;
}

void Appearance::setToolbarActionOrder(const QStringList &order) {
    _toolbarActionOrder = order;
}

QStringList Appearance::toolbarEnabledActions() {
    return _toolbarEnabledActions;
}

void Appearance::setToolbarEnabledActions(const QStringList &enabled) {
    _toolbarEnabledActions = enabled;
}

QStringList Appearance::availableStyles() {
    // Only return QWidget styles that actually work with QApplication::setStyle()
    // Qt Quick Controls styles (Material, Universal, FluentWinUI3, etc.) don't work with QWidget applications
    QStringList styles = QStyleFactory::keys();
    styles.sort();
    
    return styles;
}

void Appearance::applyStyle() {
    QApplication *app = qobject_cast<QApplication *>(QApplication::instance());
    if (!app) return;

    // Clear stylesheet before setting style to prevent QStyleSheetStyle from interfering with native styles
    app->setStyleSheet("");

    // Apply QWidget style first
    if (QStyleFactory::keys().contains(_applicationStyle, Qt::CaseInsensitive)) {
        app->setStyle(_applicationStyle);
    }

    bool isCustomTheme = (_applicationTheme >= ThemeSakura);

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    // Guard against redundant setColorScheme() calls that trigger cascading signals
    static Qt::ColorScheme s_lastSetScheme = Qt::ColorScheme::Unknown;
    Qt::ColorScheme desiredScheme = Qt::ColorScheme::Unknown;
    if (_applicationTheme == ThemeLight || _applicationTheme == ThemeSakura) {
        desiredScheme = Qt::ColorScheme::Light;
    } else if (_applicationTheme == ThemeDark || _applicationTheme == ThemeAMOLED || 
               _applicationTheme == ThemeMaterialDark || _applicationTheme == ThemeNord) {
        desiredScheme = Qt::ColorScheme::Dark;
    }
    if (desiredScheme != s_lastSetScheme) {
        s_lastSetScheme = desiredScheme;
        QApplication::styleHints()->setColorScheme(desiredScheme);
    }
#endif

    // Apply specific palettes and stylesheets for custom themes
    if (isCustomTheme) {
        if (_applicationTheme == ThemeSakura) {
            app->setPalette(ThemePalettes::getSakuraPalette());
            app->setStyleSheet(ThemePalettes::getSakuraStyleSheet());
        } else if (_applicationTheme == ThemeAMOLED) {
            app->setPalette(ThemePalettes::getAmoledPalette());
            app->setStyleSheet(ThemePalettes::getAmoledStyleSheet());
        } else if (_applicationTheme == ThemeMaterialDark) {
            app->setPalette(ThemePalettes::getMaterialDarkPalette());
            app->setStyleSheet(ThemePalettes::getMaterialDarkStyleSheet());
        } else if (_applicationTheme == ThemeNord) {
            app->setPalette(ThemePalettes::getNordPalette());
            app->setStyleSheet(ThemePalettes::getNordStyleSheet());
        }
    } else {
        // Passing a default-constructed QPalette clears the application's custom palette flag,
        // which completely restores native OS drawing routines (e.g. Windows UxTheme).
        app->setPalette(QPalette());

        // Apply button accent override ONLY for Windows 11 style in non-custom themes.
        // This keeps other styles (Fusion, etc.) native while giving Windows 11 a neutral look.
        QString buttonStyleSheet;
        if (_applicationStyle.compare("windows11", Qt::CaseInsensitive) == 0) {
            if (shouldUseDarkMode()) {
                buttonStyleSheet =
                        "QToolButton:checked { "
                        "    background-color: rgba(88, 96, 110, 120); "
                        "} ";
            } else {
                buttonStyleSheet =
                        "QToolButton:checked { "
                        "    background-color: rgba(180, 188, 196, 140); "
                        "} ";
            }
        }
        app->setStyleSheet(buttonStyleSheet);
    }

#ifdef Q_OS_WIN
    // Install event filter to catch new dialogs/windows if not already installed
    if (!g_appearanceEventFilter) {
        g_appearanceEventFilter = new AppearanceEventFilter(app);
        app->installEventFilter(g_appearanceEventFilter);
    }

    // Apply DWM Dark Mode title bar for existing top-level widgets
    foreach(QWidget* widget, app->topLevelWidgets()) {
        applyTitleBarColor(widget);
    }
#endif
}

void Appearance::applyTitleBarColor(QWidget* widget) {
#ifdef Q_OS_WIN
    if (!widget || !widget->winId()) return;

    BOOL dark = shouldUseDarkMode() ? TRUE : FALSE;
    DwmSetWindowAttribute((HWND)widget->winId(), 20 /*DWMWA_USE_IMMERSIVE_DARK_MODE*/, &dark, sizeof(dark));
    DwmSetWindowAttribute((HWND)widget->winId(), 19 /*older win10 versions*/, &dark, sizeof(dark));
    
    // Apply custom caption colors for Windows 11 (build 22000+)
    COLORREF captionColor = 0xFFFFFFFF; // DWM_COLOR_DEFAULT
    if (_applicationTheme == ThemeSakura) {
        captionColor = RGB(255, 240, 245); // Match Sakura main background (LavenderBlush)
    } else if (_applicationTheme == ThemeAMOLED) {
        captionColor = RGB(0, 0, 0); // Pure black
    } else if (_applicationTheme == ThemeMaterialDark) {
        captionColor = RGB(30, 29, 35); // Material dark background
    } else if (_applicationTheme == ThemeNord) {
        captionColor = RGB(46, 52, 64); // Nord0
    }
    DwmSetWindowAttribute((HWND)widget->winId(), 35 /*DWMWA_CAPTION_COLOR*/, &captionColor, sizeof(captionColor));
#endif
}

void Appearance::notifyIconSizeChanged() {
    // Find the main window and trigger a toolbar rebuild to apply new icon size
    QApplication *app = qobject_cast<QApplication *>(QApplication::instance());
    if (!app) return;

    // Find all top-level widgets and look for the main window
    foreach(QWidget* widget, app->topLevelWidgets()) {
        if (widget->objectName() == "MainWindow" || widget->inherits("MainWindow")) {
            // Call the MainWindow's method to rebuild toolbar with new icon size
            QMetaObject::invokeMethod(widget, "rebuildToolbarFromSettings", Qt::QueuedConnection);
            break;
        }
    }
}

// Dark mode detection and color scheme methods
bool Appearance::isDarkModeEnabled() {
    // Use Qt's built-in dark mode detection
    QStyleHints *hints = QApplication::styleHints();
    Qt::ColorScheme scheme = hints->colorScheme();
    bool isDark = (scheme == Qt::ColorScheme::Dark);
    return isDark;
}

bool Appearance::shouldUseDarkMode() {
    // If user has overridden the theme via ApplicationTheme setting, use that
    if (_applicationTheme == ThemeLight || _applicationTheme == ThemeSakura) {
        darkModeResult = false;
        return false;
    } else if (_applicationTheme == ThemeDark || _applicationTheme == ThemeAMOLED || 
               _applicationTheme == ThemeMaterialDark || _applicationTheme == ThemeNord) {
        darkModeResult = true;
        return true;
    }

    QString style = _applicationStyle.toLower();

    // Simple cache check - no expensive DateTime operations
    if (style == cachedStyle && !cachedStyle.isEmpty()) {
        return darkModeResult;
    }

    // Update cache
    cachedStyle = style;

    if (style == "windowsvista") {
        darkModeResult = false;
        return false;
    }

    if (style == "windows11" || style == "windows" || style == "fusion") {
        bool systemDarkMode = isDarkModeEnabled();
        darkModeResult = systemDarkMode;
        return systemDarkMode;
    }

    darkModeResult = false;
    return false;
}

// Color scheme methods
QColor Appearance::backgroundColor() {
    if (shouldUseDarkMode()) {
        return QColor(45, 45, 45); // Dark gray
    }
    return Qt::darkGray; // Original Qt color for light mode
}

QColor Appearance::backgroundShade() {
    if (shouldUseDarkMode()) {
        return QColor(60, 60, 60); // Dark gray shade
    }
    return Qt::lightGray; // Original Qt color for light mode
}

QColor Appearance::foregroundColor() {
    if (shouldUseDarkMode()) {
        return QColor(255, 255, 255); // White
    }
    return Qt::black; // Original Qt color for light mode
}

QColor Appearance::lightGrayColor() {
    if (shouldUseDarkMode()) {
        return QColor(150, 150, 150); // Light gray for dark mode
    }
    return Qt::lightGray; // Original Qt color for light mode
}

QColor Appearance::darkGrayColor() {
    if (shouldUseDarkMode()) {
        return QColor(100, 100, 100); // Dark gray for dark mode
    }
    return Qt::darkGray; // Original Qt color for light mode
}

QColor Appearance::grayColor() {
    if (shouldUseDarkMode()) {
        return QColor(128, 128, 128); // Medium gray for dark mode
    }
    return Qt::gray; // Original Qt color for light mode
}

QColor Appearance::pianoWhiteKeyColor() {
    if (_applicationTheme == ThemeMaterialDark) {
        return QColor(55, 55, 58);
    }
    if (_applicationTheme == ThemeAMOLED) {
        return QColor(35, 35, 35);
    }
    if (_applicationTheme == ThemeNord) {
        return QColor(76, 86, 106); // nord3
    }
    if (shouldUseDarkMode()) {
        return QColor(120, 120, 120); // Darker gray for dark mode
    }
    return Qt::white;
}

QColor Appearance::pianoBlackKeyColor() {
    if (_applicationTheme == ThemeMaterialDark) {
        return QColor(22, 22, 25);
    }
    if (_applicationTheme == ThemeAMOLED) {
        return QColor(0, 0, 0);
    }
    if (_applicationTheme == ThemeNord) {
        return QColor(46, 52, 64); // nord0
    }
    if (shouldUseDarkMode()) {
        return Qt::black; // Keep black keys black in dark mode
    }
    return Qt::black;
}

QColor Appearance::pianoWhiteKeyHoverColor() {
    if (_applicationTheme == ThemeMaterialDark) {
        return QColor(70, 70, 75);
    }
    if (_applicationTheme == ThemeAMOLED) {
        return QColor(50, 50, 50);
    }
    if (_applicationTheme == ThemeNord) {
        return QColor(94, 105, 122);
    }
    if (shouldUseDarkMode()) {
        return QColor(100, 100, 100); // Dark gray for hover in dark mode
    }
    return Qt::darkGray; // Original Qt color for light mode
}

QColor Appearance::pianoBlackKeyHoverColor() {
    if (_applicationTheme == ThemeMaterialDark) {
        return QColor(45, 45, 48);
    }
    if (_applicationTheme == ThemeAMOLED) {
        return QColor(30, 30, 30);
    }
    if (_applicationTheme == ThemeNord) {
        return QColor(67, 76, 94); // nord2
    }
    if (shouldUseDarkMode()) {
        return QColor(150, 150, 150); // Light gray for hover in dark mode
    }
    return Qt::lightGray; // Original Qt color for light mode
}

QColor Appearance::pianoWhiteKeySelectedColor() {
    if (_applicationTheme == ThemeMaterialDark) {
        return QColor(80, 85, 90);
    }
    if (_applicationTheme == ThemeAMOLED) {
        return QColor(60, 60, 60);
    }
    if (_applicationTheme == ThemeNord) {
        return QColor(110, 125, 145);
    }
    if (shouldUseDarkMode()) {
        return QColor(150, 150, 150); // Light gray for selected in dark mode
    }
    return Qt::lightGray; // Original Qt color for light mode
}

QColor Appearance::pianoBlackKeySelectedColor() {
    if (_applicationTheme == ThemeMaterialDark) {
        return QColor(50, 50, 55);
    }
    if (_applicationTheme == ThemeAMOLED) {
        return QColor(35, 35, 35);
    }
    if (_applicationTheme == ThemeNord) {
        return QColor(76, 86, 106); // nord3
    }
    if (shouldUseDarkMode()) {
        return QColor(100, 100, 100); // Dark gray for selected in dark mode
    }
    return Qt::darkGray; // Original Qt color for light mode
}

QColor Appearance::stripHighlightColor() {
    if (_applicationTheme == ThemeSakura) {
        return QColor(255, 245, 250); // Alternating subtle pink (Lighter)
    }
    if (_applicationTheme == ThemeNord) {
        return QColor(67, 76, 94); // nord2
    }
    if (_applicationTheme == ThemeMaterialDark) {
        return QColor(40, 40, 40); // Material dark alternate
    }
    if (_applicationTheme == ThemeAMOLED) {
        return QColor(24, 24, 24); // Slightly brighter for visible alternation
    }
    if (shouldUseDarkMode()) {
        return QColor(70, 70, 70); // Dark gray
    }
    return QColor(234, 246, 255); // Light blue
}

QColor Appearance::stripNormalColor() {
    if (_applicationTheme == ThemeSakura) {
        return QColor(255, 235, 245); // Alternating subtle pink (Darker)
    }
    if (_applicationTheme == ThemeNord) {
        return QColor(59, 66, 82); // nord1
    }
    if (_applicationTheme == ThemeMaterialDark) {
        return QColor(30, 29, 35); // Material dark background
    }
    if (_applicationTheme == ThemeAMOLED) {
        return QColor(0, 0, 0); // Pure black
    }
    if (shouldUseDarkMode()) {
        return QColor(55, 55, 55); // Darker gray
    }
    return QColor(194, 230, 255); // Light blue (original color)
}

QColor Appearance::rangeLineColor() {
    if (_applicationTheme == ThemeSakura) {
        return QColor(230, 210, 255); // Pastel purple for C3/C6
    }
    if (_applicationTheme == ThemeNord) {
        return QColor(129, 161, 193); // nord9 (highlighted C3/C6)
    }
    if (shouldUseDarkMode()) {
        return QColor(120, 105, 85); // Brighter cream for dark mode
    }
    return QColor(255, 239, 194); // Light cream
}

QColor Appearance::velocityBackgroundColor() {
    if (_applicationTheme == ThemeSakura) {
        return QColor(255, 245, 250); // Very light pink
    }
    if (_applicationTheme == ThemeNord) {
        return QColor(46, 52, 64); // nord0
    }
    if (_applicationTheme == ThemeMaterialDark) {
        return QColor(34, 34, 34); // Slightly lighter than background
    }
    if (_applicationTheme == ThemeAMOLED) {
        return QColor(10, 10, 10);
    }
    if (shouldUseDarkMode()) {
        return QColor(60, 75, 85); // Dark blue-gray
    }
    return QColor(234, 246, 255); // Light blue
}

QColor Appearance::velocityGridColor() {
    if (_applicationTheme == ThemeSakura) {
        return QColor(255, 220, 230); // Pink grid
    }
    if (_applicationTheme == ThemeNord) {
        return QColor(67, 76, 94); // nord2
    }
    if (_applicationTheme == ThemeMaterialDark) {
        return QColor(50, 50, 50); // Gray grid
    }
    if (_applicationTheme == ThemeAMOLED) {
        return QColor(40, 40, 40); // Dark gray grid
    }
    if (shouldUseDarkMode()) {
        return QColor(80, 95, 105); // Lighter blue-gray
    }
    return QColor(194, 230, 255); // Light blue
}

QColor Appearance::systemTextColor() {
    if (shouldUseDarkMode()) {
        return foregroundColor(); // Use main foreground color
    }
    return QApplication::palette().windowText().color();
}

QColor Appearance::systemWindowColor() {
    if (_applicationTheme == ThemeSakura) {
        return QColor(255, 235, 245); // Muted pink background for measure timeline
    }
    if (_applicationTheme == ThemeNord) {
        return QColor(46, 52, 64); // nord0
    }
    if (_applicationTheme == ThemeMaterialDark) {
        return QColor(30, 29, 35); // Material dark background
    }
    if (_applicationTheme == ThemeAMOLED) {
        return QColor(0, 0, 0); // Pure black
    }
    if (shouldUseDarkMode()) {
        return QColor(35, 35, 35); // Darker background for time area in dark mode
    }
    return QApplication::palette().window().color(); // Original system window color
}

QColor Appearance::infoBoxBackgroundColor() {
    if (shouldUseDarkMode()) {
        return QColor(60, 60, 60); // Dark gray
    }
    return Qt::white;
}

QColor Appearance::infoBoxTextColor() {
    if (shouldUseDarkMode()) {
        return QColor(200, 200, 200); // Light gray
    }
    return Qt::gray;
}

QColor Appearance::toolbarBackgroundColor() {
    if (shouldUseDarkMode()) {
        return QColor(70, 70, 70); // Dark gray
    }
    return Qt::white;
}

QColor Appearance::borderColor() {
    if (shouldUseDarkMode()) {
        return QColor(80, 80, 80); // Darker gray for dark mode (matches darker track colors)
    }
    return Qt::gray;
}

QColor Appearance::borderColorAlt() {
    if (shouldUseDarkMode()) {
        return QColor(100, 100, 100); // Medium gray for dark mode
    }
    return Qt::lightGray;
}

QColor Appearance::selectionBorderColor() {
    if (shouldUseDarkMode()) {
        return QColor(80, 80, 80); // Darker gray for dark mode (matches main border)
    }
    return Qt::lightGray; // Original light gray for light mode
}

QColor Appearance::errorColor() {
    if (shouldUseDarkMode()) {
        return QColor(200, 80, 80); // Lighter red for dark mode
    }
    return Qt::red;
}

QColor Appearance::cursorLineColor() {
    if (shouldUseDarkMode()) {
        return QColor(150, 150, 150); // Light gray for dark mode
    }
    return Qt::darkGray; // Original Qt color for light mode
}

QColor Appearance::cursorTriangleColor() {
    if (_applicationTheme == ThemeMaterialDark) {
        return QColor(4, 185, 127, 80); // Material green accent
    }
    if (_applicationTheme == ThemeAMOLED) {
        return QColor(230, 126, 34, 80); // AMOLED orange accent
    }
    if (_applicationTheme == ThemeNord) {
        return QColor(129, 161, 193, 80); // nord9 accent
    }
    if (shouldUseDarkMode()) {
        return QColor(80, 95, 105); // Dark blue-gray for dark mode
    }
    return QColor(194, 230, 255); // Light blue for light mode
}

QColor Appearance::tempoToolHighlightColor() {
    if (shouldUseDarkMode()) {
        return QColor(100, 100, 100); // Medium gray for dark mode
    }
    return Qt::lightGray; // Original Qt color for light mode
}

QColor Appearance::measureToolHighlightColor() {
    if (shouldUseDarkMode()) {
        return QColor(100, 100, 100); // Medium gray for dark mode
    }
    return Qt::lightGray; // Original Qt color for light mode
}

QColor Appearance::timeSignatureToolHighlightColor() {
    if (shouldUseDarkMode()) {
        return QColor(100, 100, 100); // Medium gray for dark mode
    }
    return Qt::lightGray; // Original Qt color for light mode
}

QColor Appearance::pianoKeyLineHighlightColor() {
    if (shouldUseDarkMode()) {
        return QColor(80, 120, 160, 80); // Much brighter blue with higher opacity for dark mode
    }
    return QColor(0, 0, 100, 40); // Blue with transparency for light mode (original)
}

QColor Appearance::measureTextColor() {
    if (_applicationTheme == ThemeMaterialDark) {
        return QColor(180, 190, 200);
    }
    if (_applicationTheme == ThemeAMOLED) {
        return QColor(160, 165, 170);
    }
    if (_applicationTheme == ThemeNord) {
        return QColor(216, 222, 233); // nord4
    }
    if (shouldUseDarkMode()) {
        return QColor(200, 200, 200); // Light gray for dark mode
    }
    return Qt::white; // White text on measure bars (original)
}

QColor Appearance::measureBarColor() {
    if (_applicationTheme == ThemeSakura) {
        return QColor(255, 182, 193); // Light pink
    }
    if (_applicationTheme == ThemeNord) {
        return QColor(76, 86, 106); // nord3
    }
    if (_applicationTheme == ThemeMaterialDark) {
        return QColor(55, 55, 60);
    }
    if (_applicationTheme == ThemeAMOLED) {
        return QColor(35, 35, 35);
    }
    if (shouldUseDarkMode()) {
        return QColor(100, 100, 100); // Medium gray for dark mode
    }
    return Qt::lightGray; // Original measure bar color
}

QColor Appearance::measureLineColor() {
    if (_applicationTheme == ThemeSakura) {
        return QColor(255, 105, 180); // Hot pink
    }
    if (_applicationTheme == ThemeNord) {
        return QColor(136, 192, 208); // nord8
    }
    if (_applicationTheme == ThemeMaterialDark) {
        return QColor(80, 85, 90);
    }
    if (_applicationTheme == ThemeAMOLED) {
        return QColor(65, 65, 65);
    }
    if (shouldUseDarkMode()) {
        return QColor(120, 120, 120); // Medium gray for dark mode
    }
    return Qt::gray; // Original measure line color
}

QColor Appearance::timelineGridColor() {
    if (_applicationTheme == ThemeSakura) {
        return QColor(255, 215, 230); // Light pink grid
    }
    if (_applicationTheme == ThemeNord) {
        return QColor(100, 112, 128); // Brighter than nord2 for visible grid lines
    }
    if (_applicationTheme == ThemeMaterialDark) {
        return QColor(70, 70, 70); // Brighter grid against dark strips
    }
    if (_applicationTheme == ThemeAMOLED) {
        return QColor(50, 50, 50); // Visible grid against black strips
    }
    if (shouldUseDarkMode()) {
        return QColor(100, 100, 100); // Medium gray for dark mode
    }
    return Qt::lightGray; // Original timeline grid color
}

QColor Appearance::playbackCursorColor() {
    if (shouldUseDarkMode()) {
        return QColor(200, 80, 80); // More muted red for dark mode
    }
    return Qt::red; // Original red playback cursor
}

QColor Appearance::recordingIndicatorColor() {
    if (shouldUseDarkMode()) {
        return QColor(255, 100, 100); // Lighter red for dark mode
    }
    return Qt::red; // Original red recording indicator
}

QColor Appearance::programEventHighlightColor() {
    if (_applicationTheme == ThemeSakura) {
        return QColor(240, 228, 245); // Lilac — clearly distinct from pink note strips
    }
    if (_applicationTheme == ThemeNord) {
        return QColor(59, 66, 82); // nord1 (creates distinct separation from matrix)
    }
    if (_applicationTheme == ThemeMaterialDark) {
        return QColor(28, 28, 28);
    }
    if (_applicationTheme == ThemeAMOLED) {
        return QColor(12, 16, 22); // Dark blue-tinted — distinct from pure gray/black strips
    }
    if (shouldUseDarkMode()) {
        return QColor(60, 70, 90); // Darker blue for dark mode (different from strip)
    }
    return QColor(234, 246, 255); // Light blue (original)
}

QColor Appearance::programEventNormalColor() {
    if (_applicationTheme == ThemeSakura) {
        return QColor(232, 218, 238); // Deeper lilac — clearly distinct from pink note strips
    }
    if (_applicationTheme == ThemeNord) {
        return QColor(46, 52, 64); // nord0 (creates distinct separation from matrix)
    }
    if (_applicationTheme == ThemeMaterialDark) {
        return QColor(22, 22, 22);
    }
    if (_applicationTheme == ThemeAMOLED) {
        return QColor(6, 10, 16); // Darker blue-tinted — distinct from pure gray/black strips
    }
    if (shouldUseDarkMode()) {
        return QColor(45, 55, 70); // Darker blue-gray for dark mode (different from strip)
    }
    return QColor(194, 194, 194); // Light gray (original)
}

QColor Appearance::noteSelectionColor() {
    // Use the same selection color in both light and dark mode
    return Qt::darkBlue;
}

QPixmap Appearance::adjustIconForDarkMode(const QPixmap &original, const QString &iconName) {
    // Prevent QPixmap operations during application shutdown
    if (_shuttingDown) {
        return QPixmap(); // Return empty pixmap to avoid crashes
    }

    if (!shouldUseDarkMode()) {
        return original;
    }

    // List of icons that don't need color adjustment (they're not black)
    QStringList skipIcons = {"load", "new", "redo", "undo", "save", "saveas", "stop_record", "icon", "midieditor"};

    // Extract just the filename from the path for comparison
    QString fileName = iconName;
    if (fileName.contains("/")) {
        fileName = fileName.split("/").last();
    }
    if (fileName.contains(".")) {
        fileName = fileName.split(".").first();
    }

    // Skip adjustment for non-black icons
    if (skipIcons.contains(fileName)) {
        return original;
    }

    // Create adjusted icon for dark mode
    QPixmap adjusted = original;
    QPainter painter(&adjusted);
    painter.setCompositionMode(QPainter::CompositionMode_SourceAtop);
    
    QColor iconColor = QColor(180, 180, 180); // Default light gray
    if (_applicationTheme == ThemeAMOLED || _applicationTheme == ThemeMaterialDark || _applicationTheme == ThemeNord) {
        iconColor = Qt::white; // White icons for very dark custom themes
    }
    
    painter.fillRect(adjusted.rect(), iconColor);
    painter.end();
    return adjusted;
}

QIcon Appearance::adjustIconForDarkMode(const QString &iconPath) {
    // Prevent QPixmap operations during application shutdown
    if (_shuttingDown) {
        return QIcon(); // Return empty icon to avoid crashes
    }

    bool dark = shouldUseDarkMode();
    if (s_lastIconTheme != _applicationTheme || s_lastDarkMode != dark) {
        s_iconCache.clear();
        s_lastIconTheme = _applicationTheme;
        s_lastDarkMode = dark;
    }

    if (s_iconCache.contains(iconPath)) {
        return s_iconCache[iconPath];
    }

    QPixmap original(iconPath);

    // Check if the pixmap loaded successfully
    if (original.isNull()) {
        return QIcon(iconPath);
    }

    QString fileName = iconPath;
    if (fileName.contains("/")) {
        fileName = fileName.split("/").last();
    }
    if (fileName.contains(".")) {
        fileName = fileName.split(".").first();
    }

    QIcon result(adjustIconForDarkMode(original, fileName));
    s_iconCache[iconPath] = result;
    return result;
}

void Appearance::refreshAllIcons() {
    // Check if registeredIconActions is in a valid state
    if (registeredIconActions.isEmpty()) {
        return;
    }

    // Update all icons immediately - no cleanup needed since actions auto-unregister - destroyed() signal handles removal
    for (auto it = registeredIconActions.begin(); it != registeredIconActions.end(); ++it) {
        QAction *action = it.key();
        const QString &iconPath = it.value();

        if (action) {
            // Update the icon immediately
            QIcon newIcon = adjustIconForDarkMode(iconPath);
            action->setIcon(newIcon);
        }
    }
}

void Appearance::startQueuedIconProcessing() {
    // Create timer for processing one icon per tick
    iconUpdateTimer = new QTimer();
    iconUpdateTimer->setSingleShot(false);
    iconUpdateTimer->setInterval(10); // Very slow timer - 10ms per icon for maximum safety

    QObject::connect(iconUpdateTimer, &QTimer::timeout, []() {
        processNextQueuedIcon();
    });

    iconUpdateTimer->start();
}

void Appearance::processNextQueuedIcon() {
    if (iconUpdateQueue.isEmpty()) {
        // All icons processed
        if (iconUpdateTimer) {
            iconUpdateTimer->stop();
            iconUpdateTimer->deleteLater();
            iconUpdateTimer = nullptr;
        }
        return;
    }

    // Process only ONE icon per timer tick to prevent crashes
    QPair<QAction *, QString> iconPair = iconUpdateQueue.takeFirst();
    QAction *action = iconPair.first;
    QString iconPath = iconPair.second;

    if (!action) {
        return; // Skip null actions
    }

    // Verify action is still valid and update icon
    try {
        // Test if action is still valid
        action->objectName();

        // Update the icon
        QIcon newIcon = adjustIconForDarkMode(iconPath);
        action->setIcon(newIcon);
    } catch (...) {
        // Action is invalid or update failed, just skip it
        // Don't crash the entire process
    }
}

void Appearance::registerIconAction(QAction *action, const QString &iconPath) {
    if (!action) return;

    // Check if already registered
    auto existing = registeredIconActions.find(action);
    if (existing != registeredIconActions.end()) {
        // Already registered - just update icon path if different
        if (existing.value() != iconPath) {
            existing.value() = iconPath;
        }
        return;
    }

    // New registration - add to map and connect destruction signal
    registeredIconActions[action] = iconPath;

    // Auto-unregister when action is destroyed
    QObject::connect(action, &QObject::destroyed, [action]() {
        registeredIconActions.remove(action);
    });
}

void Appearance::setActionIcon(QAction *action, const QString &iconPath) {
    if (action) {
        action->setIcon(adjustIconForDarkMode(iconPath));
        registerIconAction(action, iconPath);
    }
}

void Appearance::refreshColors() {
    // Update all widgets
    QApplication *app = qobject_cast<QApplication *>(QApplication::instance());
    if (!app) {
        return;
    }

    // Prevent cascading toolbar updates during style changes
    static bool isRefreshingColors = false;
    static QDateTime lastRefresh;
    QDateTime now = QDateTime::currentDateTime();

    if (isRefreshingColors) {
        return; // Already refreshing, prevent recursion
    }

    static QTimer* s_pendingRefreshTimer = nullptr;
    if (lastRefresh.isValid() && lastRefresh.msecsTo(now) < 200) {
        // If a refresh is requested too soon, schedule it for the near future
        // to ensure the final state is always applied.
        if (!s_pendingRefreshTimer) {
            s_pendingRefreshTimer = new QTimer(app);
            s_pendingRefreshTimer->setSingleShot(true);
            QObject::connect(s_pendingRefreshTimer, &QTimer::timeout, []() {
                refreshColors();
            });
        }
        if (!s_pendingRefreshTimer->isActive()) {
            s_pendingRefreshTimer->start(210);
        }
        return; // Too soon since last refresh
    }

    if (s_pendingRefreshTimer && s_pendingRefreshTimer->isActive()) {
        s_pendingRefreshTimer->stop();
    }

    isRefreshingColors = true;
    lastRefresh = now;

    // Determine if effective color scheme (light/dark) actually changed
    bool newScheme = shouldUseDarkMode();
    static int s_lastScheme = -1; // -1 unknown, 0 light, 1 dark
    bool schemeChanged = (s_lastScheme == -1) || (s_lastScheme != (newScheme ? 1 : 0));
    s_lastScheme = newScheme ? 1 : 0;

    // Detect actual theme change for icon recoloring
    static ApplicationTheme s_lastTheme = ThemeAuto;
    bool themeChanged = (s_lastTheme != _applicationTheme);
    s_lastTheme = _applicationTheme;

    // Detect application style name change (e.g., windowsvista -> windows11)
    static QString s_lastStyleName = QString();
    bool styleNameChanged = (s_lastStyleName.isEmpty() || s_lastStyleName != _applicationStyle);
    s_lastStyleName = _applicationStyle;

    try {
        // Auto-reset default colors for the new theme
        autoResetDefaultColors();
    } catch (...) {
        // Color reset failed - continue with other updates
    }

    // Update all top-level widgets with crash protection
    foreach(QWidget* widget, app->topLevelWidgets()) {
        if (widget->isVisible()) {
            // Full style unpolish/polish is needed when the style name, scheme, or theme changes.
            // This ensures native styles (like Windows 11) update their internal caches/brushes
            // and pick up the new palette correctly. Without this, switching between light
            // and dark themes can result in 'inverted' or stale backgrounds.
            if (styleNameChanged || schemeChanged || themeChanged) {
                try {
                    widget->style()->unpolish(widget);
                    widget->style()->polish(widget);
                } catch (...) {
                    // If style repolish fails for some widget, skip it
                }
            }
            // Always update to schedule a repaint
            try { widget->update(); } catch (...) {}

            // Refresh ToolButton icons only when theme/scheme actually changed
            // This is a perf bottleneck — each refreshIcon() does
            // QImage->QPixmap->QPainter compositing
            if (schemeChanged || themeChanged) {
                QList<ToolButton *> toolButtons = widget->findChildren<ToolButton *>();
                foreach(ToolButton* toolButton, toolButtons) {
                    toolButton->refreshIcon();
                }
            }

            // Refresh all ProtocolWidget colors for theme changes
            QList<ProtocolWidget *> protocolWidgets = widget->findChildren<ProtocolWidget *>();
            foreach(ProtocolWidget* protocolWidget, protocolWidgets) {
                protocolWidget->refreshColors();
            }

            // Refresh all AppearanceSettingsWidget colors for theme changes
            QList<AppearanceSettingsWidget *> appearanceWidgets = widget->findChildren<AppearanceSettingsWidget *>();
            foreach(AppearanceSettingsWidget* appearanceWidget, appearanceWidgets) {
                appearanceWidget->refreshColors();
            }

            // Refresh all MidiSettingsWidget colors for theme changes
            QList<MidiSettingsWidget *> midiWidgets = widget->findChildren<MidiSettingsWidget *>();
            foreach(MidiSettingsWidget* midiWidget, midiWidgets) {
                midiWidget->refreshColors();
            }

            // Refresh all AdditionalMidiSettingsWidget colors for theme changes
            QList<AdditionalMidiSettingsWidget *> additionalMidiWidgets = widget->findChildren<AdditionalMidiSettingsWidget *>();
            foreach(AdditionalMidiSettingsWidget* additionalMidiWidget, additionalMidiWidgets) {
                additionalMidiWidget->refreshColors();
            }

            // Refresh all InstrumentSettingsWidget colors for theme changes
            QList<InstrumentSettingsWidget *> instrumentWidgets = widget->findChildren<InstrumentSettingsWidget *>();
            foreach(InstrumentSettingsWidget* instrumentWidget, instrumentWidgets) {
                instrumentWidget->refreshColors();
            }

            // Refresh all ControlChangeSettingsWidget colors for theme changes
            QList<ControlChangeSettingsWidget *> ccWidgets = widget->findChildren<ControlChangeSettingsWidget *>();
            foreach(ControlChangeSettingsWidget* ccWidget, ccWidgets) {
                ccWidget->refreshColors();
            }

            // Refresh all PerformanceSettingsWidget colors for theme changes
            QList<PerformanceSettingsWidget *> performanceWidgets = widget->findChildren<PerformanceSettingsWidget *>();
            foreach(PerformanceSettingsWidget* performanceWidget, performanceWidgets) {
                performanceWidget->refreshColors();
            }

            // Refresh all TrackListWidget colors for theme changes
            QList<TrackListWidget *> trackListWidgets = widget->findChildren<TrackListWidget *>();
            foreach(TrackListWidget* trackListWidget, trackListWidgets) {
                trackListWidget->update();
            }

            // Refresh all ChannelListWidget colors for theme changes
            QList<ChannelListWidget *> channelListWidgets = widget->findChildren<ChannelListWidget *>();
            foreach(ChannelListWidget* channelListWidget, channelListWidgets) {
                channelListWidget->update();
            }

            // Refresh all LayoutSettingsWidget icons for theme changes
            if (schemeChanged || themeChanged) {
                QList<QWidget *> layoutSettingsWidgets = widget->findChildren<QWidget *>("LayoutSettingsWidget");
                foreach(QWidget* layoutWidget, layoutSettingsWidgets) {
                    QMetaObject::invokeMethod(layoutWidget, "refreshIcons", Qt::DirectConnection);
                }
            }

            // Refresh toolbar icons for widgets that support it
            if (widget->objectName() == "MainWindow" || widget->inherits("MainWindow")) {
                QMetaObject::invokeMethod(widget, "refreshToolbarIcons", Qt::DirectConnection);
            } else if (widget->objectName() == "SettingsDialog" || widget->inherits("SettingsDialog")) {
                QMetaObject::invokeMethod(widget, "refreshToolbarIcons", Qt::DirectConnection);
            }
        }
    }

    // Refresh registered action icons (uses cache — fast when theme unchanged)
    if (schemeChanged || themeChanged) {
        refreshAllIcons();
    }

    // Reapply styling for theme changes (title bars, stylesheets)
    // The static guard in applyStyle() prevents infinite recursion.
    applyStyle();

    // Update MatrixWidgets efficiently
    foreach(QWidget* widget, app->topLevelWidgets()) {
        QList<MatrixWidget *> matrixWidgets = widget->findChildren<MatrixWidget *>();
        foreach (MatrixWidget* matrixWidget, matrixWidgets) {
            if (!matrixWidget) continue;

            // Always refresh cached appearance colors (cheap)
            try { matrixWidget->updateCachedAppearanceColors(); } catch (...) {}

            // Only drop pixmaps and invalidate track color cache if scheme actually changed
            if (schemeChanged || themeChanged) {
                try { matrixWidget->updateRenderingSettings(); } catch (...) {}
            } else {
                // No effective scheme change; a simple update is enough
                try { matrixWidget->update(); } catch (...) {}
            }
        }
    }

    // Reset the flag to allow future refreshes
    isRefreshingColors = false;
}

void Appearance::forceColorRefresh() {
    // Public method that can be called from settings dialogs or other places
    refreshColors();
}

void Appearance::connectToSystemThemeChanges() {
    QApplication *app = qobject_cast<QApplication *>(QApplication::instance());
    if (!app) return;

    // Connect to system theme change detection
    QStyleHints *hints = app->styleHints();

    // Connect to colorSchemeChanged signal
    QObject::connect(hints, &QStyleHints::colorSchemeChanged, [](Qt::ColorScheme colorScheme) {
        Q_UNUSED(colorScheme)

        // Invalidate cache when system theme changes
        cachedStyle = "";

        // Refresh colors when system theme changes using the same shared debounce
        // We defer slightly to allow the platform to finalize its palette/style
        static QTimer *s_refreshDebounceTimer = nullptr;
        if (!s_refreshDebounceTimer) {
            s_refreshDebounceTimer = new QTimer();
            s_refreshDebounceTimer->setSingleShot(true);
            QObject::connect(s_refreshDebounceTimer, &QTimer::timeout, []() {
                refreshColors();
            });
        }
        s_refreshDebounceTimer->stop();
        s_refreshDebounceTimer->start(150);
    });
}

void Appearance::cleanup() {
    qDebug() << "Appearance: Starting cleanup of static resources";

    // Set shutdown flag to prevent any new QPixmap creation
    _shuttingDown = true;

    // Clean up color maps to prevent QColor destructor issues after QApplication shutdown
    qDeleteAll(channelColors);
    channelColors.clear();

    qDeleteAll(trackColors);
    trackColors.clear();

    // Clear custom color sets
    customChannelColors.clear();
    customTrackColors.clear();

    // Clear registered icon actions map
    // Note: We don't delete the QAction objects as they're owned by other components
    registeredIconActions.clear();

    qDebug() << "Appearance: Static resource cleanup completed";
}

void Appearance::setShuttingDown(bool shuttingDown) {
    _shuttingDown = shuttingDown;
}