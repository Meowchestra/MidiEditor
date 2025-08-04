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
#include "PerformanceSettingsWidget.h"
#include "TrackListWidget.h"
#include "ChannelListWidget.h"

// Prevent rapid successive theme changes
static QDateTime lastThemeChange;

// Simple cache to prevent infinite loops in shouldUseDarkMode
static bool darkModeResult = false;
static QString cachedStyle = "";
static QDateTime cacheTime;

// Queue-based icon processing to prevent crashes
static QTimer *iconUpdateTimer = nullptr;
static QList<QPair<QAction *, QString> > iconUpdateQueue;

QMap<int, QColor *> Appearance::channelColors = QMap<int, QColor *>();
QMap<int, QColor *> Appearance::trackColors = QMap<int, QColor *>();
QSet<int> Appearance::customChannelColors = QSet<int>();
QSet<int> Appearance::customTrackColors = QSet<int>();
QMap<QAction *, QString> Appearance::registeredIconActions = QMap<QAction *, QString>();
int Appearance::_opacity = 100;
Appearance::stripStyle Appearance::_strip = Appearance::onSharp;
bool Appearance::_showRangeLines = false;
QString Appearance::_applicationStyle = "windowsvista";
int Appearance::_toolbarIconSize = 20;
bool Appearance::_ignoreSystemScaling = false;
bool Appearance::_ignoreFontScaling = false;
bool Appearance::_useRoundedScaling = false;
int Appearance::_msaaSamples = 2;
bool Appearance::_enableVSync = false;
bool Appearance::_toolbarTwoRowMode = false;
bool Appearance::_toolbarCustomizeEnabled = false;
QStringList Appearance::_toolbarActionOrder = QStringList();
QStringList Appearance::_toolbarEnabledActions = QStringList();

void Appearance::init(QSettings *settings) {
    // CRITICAL: Load application style FIRST before creating any colors
    _opacity = settings->value("appearance_opacity", 100).toInt();
    _strip = static_cast<Appearance::stripStyle>(settings->value("strip_style", Appearance::onSharp).toInt());
    _showRangeLines = settings->value("show_range_lines", false).toBool();

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

    // Apply opacity setting to the color
    color->setAlpha(_opacity * 255 / 100);

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

    // Apply opacity setting to the color
    color->setAlpha(_opacity * 255 / 100);

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
    settings->setValue("application_style", _applicationStyle);
    settings->setValue("toolbar_icon_size", _toolbarIconSize);
    settings->setValue("ignore_system_scaling", _ignoreSystemScaling);
    settings->setValue("ignore_font_scaling", _ignoreFontScaling);
    settings->setValue("use_rounded_scaling", _useRoundedScaling);
    settings->setValue("toolbar_two_row_mode", _toolbarTwoRowMode);
    settings->setValue("toolbar_customize_enabled", _toolbarCustomizeEnabled);
    settings->setValue("toolbar_action_order", _toolbarActionOrder);
    settings->setValue("toolbar_enabled_actions", _toolbarEnabledActions);

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

    // Use a timer to debounce the expensive refresh operations
    static QTimer *refreshTimer = nullptr;
    if (!refreshTimer) {
        refreshTimer = new QTimer();
        refreshTimer->setSingleShot(true);
        QObject::connect(refreshTimer, &QTimer::timeout, []() {
            refreshColors();
        });
    }

    // Debounce refresh to prevent multiple rapid updates
    refreshTimer->stop();
    refreshTimer->start(100); // Wait 100ms before refreshing
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

void Appearance::loadEarlySettings() {
    // Load only the settings needed before QApplication is created
    QSettings settings(QString("MidiEditor"), QString("NONE"));
    _ignoreSystemScaling = settings.value("ignore_system_scaling", false).toBool();
    _ignoreFontScaling = settings.value("ignore_font_scaling", false).toBool();
    _useRoundedScaling = settings.value("use_rounded_scaling", false).toBool();
    _msaaSamples = settings.value("rendering/msaa_samples", 2).toInt();
    _enableVSync = settings.value("rendering/enable_vsync", false).toBool();

    qDebug() << "Appearance::loadEarlySettings() - Loaded values:";
    qDebug() << "  ignore_system_scaling:" << _ignoreSystemScaling;
    qDebug() << "  ignore_font_scaling:" << _ignoreFontScaling;
    qDebug() << "  use_rounded_scaling:" << _useRoundedScaling;
    qDebug() << "  msaa_samples:" << _msaaSamples;
    qDebug() << "  enable_vsync:" << _enableVSync;
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

    // Apply QWidget style first
    if (QStyleFactory::keys().contains(_applicationStyle)) {
        app->setStyle(_applicationStyle);
    }

    // Apply dark mode specific styling if needed
    if (shouldUseDarkMode()) {
        QString darkStyleSheet =
                "QToolButton:checked { "
                "    background-color: rgba(80, 80, 80, 150); "
                "    border: 1px solid rgba(120, 120, 120, 150); "
                "} "
                "QToolBar::separator { "
                "    background-color: rgba(120, 120, 120, 150); "
                "    width: 1px; "
                "    height: 1px; "
                "    margin: 2px; "
                "}";
        app->setStyleSheet(darkStyleSheet);
    } else {
        app->setStyleSheet(""); // Clear custom styling for light mode
    }
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
    if (shouldUseDarkMode()) {
        return QColor(120, 120, 120); // Darker gray for dark mode
    }
    return Qt::white;
}

QColor Appearance::pianoBlackKeyColor() {
    if (shouldUseDarkMode()) {
        return Qt::black; // Keep black keys black in dark mode
    }
    return Qt::black;
}

QColor Appearance::pianoWhiteKeyHoverColor() {
    if (shouldUseDarkMode()) {
        return QColor(100, 100, 100); // Dark gray for hover in dark mode
    }
    return Qt::darkGray; // Original Qt color for light mode
}

QColor Appearance::pianoBlackKeyHoverColor() {
    if (shouldUseDarkMode()) {
        return QColor(150, 150, 150); // Light gray for hover in dark mode
    }
    return Qt::lightGray; // Original Qt color for light mode
}

QColor Appearance::pianoWhiteKeySelectedColor() {
    if (shouldUseDarkMode()) {
        return QColor(150, 150, 150); // Light gray for selected in dark mode
    }
    return Qt::lightGray; // Original Qt color for light mode
}

QColor Appearance::pianoBlackKeySelectedColor() {
    if (shouldUseDarkMode()) {
        return QColor(100, 100, 100); // Dark gray for selected in dark mode
    }
    return Qt::darkGray; // Original Qt color for light mode
}

QColor Appearance::stripHighlightColor() {
    if (shouldUseDarkMode()) {
        return QColor(70, 70, 70); // Dark gray
    }
    return QColor(234, 246, 255); // Light blue
}

QColor Appearance::stripNormalColor() {
    if (shouldUseDarkMode()) {
        return QColor(55, 55, 55); // Darker gray
    }
    return QColor(194, 230, 255); // Light blue (original color)
}

QColor Appearance::rangeLineColor() {
    if (shouldUseDarkMode()) {
        return QColor(120, 105, 85); // Brighter cream for dark mode
    }
    return QColor(255, 239, 194); // Light cream
}

QColor Appearance::velocityBackgroundColor() {
    if (shouldUseDarkMode()) {
        return QColor(60, 75, 85); // Dark blue-gray
    }
    return QColor(234, 246, 255); // Light blue
}

QColor Appearance::velocityGridColor() {
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
    if (shouldUseDarkMode()) {
        return QColor(200, 200, 200); // Light gray for dark mode
    }
    return Qt::white; // White text on measure bars (original)
}

QColor Appearance::measureBarColor() {
    if (shouldUseDarkMode()) {
        return QColor(100, 100, 100); // Medium gray for dark mode
    }
    return Qt::lightGray; // Original measure bar color
}

QColor Appearance::measureLineColor() {
    if (shouldUseDarkMode()) {
        return QColor(120, 120, 120); // Medium gray for dark mode
    }
    return Qt::gray; // Original measure line color
}

QColor Appearance::timelineGridColor() {
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
    if (shouldUseDarkMode()) {
        return QColor(60, 70, 90); // Darker blue for dark mode (different from strip)
    }
    return QColor(234, 246, 255); // Light blue (original)
}

QColor Appearance::programEventNormalColor() {
    if (shouldUseDarkMode()) {
        return QColor(45, 55, 70); // Darker blue-gray for dark mode (different from strip)
    }
    return QColor(194, 194, 194); // Light gray (original)
}

QColor Appearance::noteSelectionColor() {
    if (shouldUseDarkMode()) {
        // In dark mode, use a darker version of the track color with some transparency
        // This allows the track color to show through while indicating selection
        return QColor(60, 80, 120, 150); // Dark blue with transparency
    }
    return Qt::darkBlue; // Original selection color for light mode
}

QPixmap Appearance::adjustIconForDarkMode(const QPixmap &original, const QString &iconName) {
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
    painter.fillRect(adjusted.rect(), QColor(180, 180, 180)); // Light gray for dark mode
    painter.end();
    return adjusted;
}

QIcon Appearance::adjustIconForDarkMode(const QString &iconPath) {
    QPixmap original(iconPath);

    // Check if the pixmap loaded successfully
    if (original.isNull()) {
        return QIcon(); // Return empty icon instead of crashing
    }

    // Extract filename from path for icon name detection
    QString fileName = iconPath;
    if (fileName.contains("/")) {
        fileName = fileName.split("/").last();
    }
    if (fileName.contains(".")) {
        fileName = fileName.split(".").first();
    }

    QPixmap adjusted = adjustIconForDarkMode(original, fileName);
    return QIcon(adjusted);
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
    // Prevent cascading toolbar updates during style changes
    static bool isRefreshingColors = false;
    static QDateTime lastRefresh;
    QDateTime now = QDateTime::currentDateTime();

    if (isRefreshingColors) {
        return; // Already refreshing, prevent recursion
    }

    // Debounce rapid refresh calls
    if (lastRefresh.isValid() && lastRefresh.msecsTo(now) < 200) {
        return; // Too soon since last refresh
    }

    isRefreshingColors = true;
    lastRefresh = now;

    try {
        // Auto-reset default colors for the new theme
        autoResetDefaultColors();
    } catch (...) {
        // Color reset failed - this might be the crash source
        // Continue with other updates but skip color reset
    }

    // Update all widgets
    QApplication *app = qobject_cast<QApplication *>(QApplication::instance());
    if (!app) {
        return;
    }

    // Update all top-level widgets with crash protection
    foreach(QWidget* widget, app->topLevelWidgets()) {
        if (widget->isVisible()) {
            try {
                // Force a complete repaint with style refresh
                widget->style()->unpolish(widget);
                widget->style()->polish(widget);
                widget->update();
            } catch (...) {
                // Widget style update failed - skip this widget but continue
                continue;
            }

            // Refresh all ToolButton icons for theme changes
            QList<ToolButton *> toolButtons = widget->findChildren<ToolButton *>();
            foreach(ToolButton* toolButton, toolButtons) {
                toolButton->refreshIcon();
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
            QList<QWidget *> layoutSettingsWidgets = widget->findChildren<QWidget *>("LayoutSettingsWidget");
            foreach(QWidget* layoutWidget, layoutSettingsWidgets) {
                // Call refreshIcons method if it exists
                QMetaObject::invokeMethod(layoutWidget, "refreshIcons", Qt::DirectConnection);
            }

            // Refresh toolbar icons for widgets that support it
            if (widget->objectName() == "MainWindow" || widget->inherits("MainWindow")) {
                // MainWindow has refreshToolbarIcons method
                QMetaObject::invokeMethod(widget, "refreshToolbarIcons", Qt::DirectConnection);
            } else if (widget->objectName() == "SettingsDialog" || widget->inherits("SettingsDialog")) {
                // SettingsDialog has its own refreshToolbarIcons method
                QMetaObject::invokeMethod(widget, "refreshToolbarIcons", Qt::DirectConnection);
            }

            // Force immediate processing of paint events
            app->processEvents();
        }
    }

    // Refresh all icons after widget updates
    refreshAllIcons();

    // Reapply styling for theme changes
    applyStyle();

    // Update MatrixWidgets with crash protection
    foreach(QWidget* widget, app->topLevelWidgets()) {
        if (widget->isVisible()) {
            QList<MatrixWidget *> matrixWidgets = widget->findChildren<MatrixWidget *>();
            foreach(MatrixWidget* matrixWidget, matrixWidgets) {
                if (matrixWidget && matrixWidget->isVisible()) {
                    try {
                        // Simple update first
                        matrixWidget->update();

                        // Process events to prevent Qt event queue overload
                        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

                        // Force complete redraw - this might be the crash source
                        matrixWidget->updateRenderingSettings();
                    } catch (...) {
                        // MatrixWidget update failed - skip this widget but continue with others
                        // This prevents one bad widget from crashing the entire theme change
                        continue;
                    }
                }
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

        // Refresh colors when system theme changes - use a timer to ensure it happens after the system has fully switched
        QTimer::singleShot(100, []() {
            refreshColors();
            // Force another refresh after a short delay to catch any delayed updates
            QTimer::singleShot(500, []() {
                refreshColors();
            });
        });
    });
}