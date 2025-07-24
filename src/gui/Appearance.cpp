#include "Appearance.h"
#include <QApplication>
#include <QStyleFactory>
#include <QPalette>
#include <QStyle>
#include <QToolBar>
#include <QWidget>
#include <QStyleHints>
#include <QTimer>
#include <QPainter>
#include <QPixmap>
#include <QAction>
#include "../tool/ToolButton.h"
#include "ProtocolWidget.h"
#include "MatrixWidget.h"
#include "AppearanceSettingsWidget.h"

QMap<int, QColor*> Appearance::channelColors = QMap<int, QColor*>();
QMap<int, QColor*> Appearance::trackColors = QMap<int, QColor*>();
QSet<int> Appearance::customChannelColors = QSet<int>();
QSet<int> Appearance::customTrackColors = QSet<int>();
QMap<QAction*, QString> Appearance::registeredIconActions = QMap<QAction*, QString>();
int Appearance::_opacity = 100;
Appearance::stripStyle Appearance::_strip = Appearance::onSharp;
bool Appearance::_showRangeLines = false;
QString Appearance::_applicationStyle = "windowsvista";
int Appearance::_toolbarIconSize = 20;

void Appearance::init(QSettings *settings){
    // CRITICAL: Load application style FIRST before creating any colors
    _opacity = settings->value("appearance_opacity", 100).toInt();
    _strip = static_cast<Appearance::stripStyle>(settings->value("strip_style",Appearance::onSharp).toInt());
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

    // NOW load colors with correct theme context
    for (int channel = 0; channel < 17; channel++) {
        channelColors.insert(channel,
                             decode("channel_color_" + QString::number(channel),
                                    settings, defaultColor(channel)));
    }
    for (int track = 0; track < 17; track++) {
        trackColors.insert(track,
                           decode("track_color_" + QString::number(track),
                                  settings, defaultColor(track)));
    }

    // Load custom color tracking
    QList<QVariant> customChannels = settings->value("custom_channel_colors", QList<QVariant>()).toList();
    foreach (const QVariant& var, customChannels) {
        customChannelColors.insert(var.toInt());
    }

    QList<QVariant> customTracks = settings->value("custom_track_colors", QList<QVariant>()).toList();
    foreach (const QVariant& var, customTracks) {
        customTrackColors.insert(var.toInt());
    }

    // Apply the style after loading settings
    applyStyle();

    // Connect to system theme changes
    connectToSystemThemeChanges();
}

QColor *Appearance::channelColor(int channel){
    QColor *color = channelColors[channelToColorIndex(channel)];
    color->setAlpha(_opacity * 255 / 100);
    return color;
}

QColor *Appearance::trackColor(int track) {
    QColor *color = trackColors[trackToColorIndex(track)];
    color->setAlpha(_opacity * 255 / 100);
    return color;
}

void Appearance::writeSettings(QSettings *settings) {
    for (int channel = 0; channel < 17; channel++) {
        write("channel_color_" + QString::number(channel), settings, channelColors[channel]);
    }
    for (int track = 0; track < 17; track++) {
        write("track_color_" + QString::number(track), settings, trackColors[track]);
    }
    settings->setValue("appearance_opacity", _opacity);
    settings->setValue("strip_style",_strip);
    settings->setValue("show_range_lines", _showRangeLines);
    settings->setValue("application_style", _applicationStyle);
    settings->setValue("toolbar_icon_size", _toolbarIconSize);

    // Save custom color tracking
    QList<QVariant> customChannels;
    foreach (int channel, customChannelColors) {
        customChannels.append(channel);
    }
    settings->setValue("custom_channel_colors", customChannels);

    QList<QVariant> customTracks;
    foreach (int track, customTrackColors) {
        customTracks.append(track);
    }
    settings->setValue("custom_track_colors", customTracks);
}

QColor *Appearance::defaultColor(int n) {
    QColor* color;

    if (shouldUseDarkMode()) {
        // Darker, more muted colors for dark mode (slightly darker shade)
        switch (n) {
        case 0: {
            color = new QColor(160, 35, 25, 255);
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
            color = new QColor(80, 35, 180, 255);
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
            color = new QColor(80, 80, 80, 255);
            break;
        }
        case 10: {
            color = new QColor(130, 25, 80, 255);
            break;
        }
        case 11: {
            color = new QColor(0, 80, 180, 255);
            break;
        }
        case 12: {
            color = new QColor(60, 80, 15, 255);
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
            color = new QColor(25, 25, 180, 255);
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

QColor *Appearance::decode(QString name, QSettings *settings, QColor *defaultColor){
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
    QColor* oldColor = trackColors[index];
    trackColors[index] = new QColor(color);
    customTrackColors.insert(index); // Mark this track color as custom
    if (oldColor) {
        delete oldColor;
    }
}

void Appearance::setChannelColor(int channel, QColor color){
    int index = channelToColorIndex(channel);
    QColor* oldColor = channelColors[index];
    channelColors[index] = new QColor(color);
    customChannelColors.insert(index); // Mark this channel color as custom
    if (oldColor) {
        delete oldColor;
    }
}

int Appearance::trackToColorIndex(int track){
    int mod = (track-1) %17;
    if (mod < 0) {
        mod+=17;
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
    // Always auto-reset non-custom colors to current theme defaults
    // This ensures default colors always match the current theme

    for (int channel = 0; channel < 17; channel++) {
        if (!customChannelColors.contains(channel)) {
            // This is a default color, update it to match current theme
            QColor* existingColor = channelColors[channel];
            if (existingColor) {
                QColor* newDefaultColor = defaultColor(channel);
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

    for (int track = 0; track < 17; track++) {
        if (!customTrackColors.contains(track)) {
            // This is a default color, update it to match current theme
            QColor* existingColor = trackColors[track];
            if (existingColor) {
                QColor* newDefaultColor = defaultColor(track);
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
}

void Appearance::forceResetAllColors() {
    // Force reset all colors to current theme defaults
    for (int channel = 0; channel < 17; channel++) {
        QColor* existingColor = channelColors[channel];
        if (existingColor) {
            QColor* newDefaultColor = defaultColor(channel);
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
        QColor* existingColor = trackColors[track];
        if (existingColor) {
            QColor* newDefaultColor = defaultColor(track);
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

int Appearance::opacity(){
    return _opacity;
}

void Appearance::setOpacity(int opacity){
    _opacity = opacity;
}

Appearance::stripStyle Appearance::strip(){
    return _strip;
}

void Appearance::setStrip(Appearance::stripStyle render){
    _strip = render;
}

bool Appearance::showRangeLines(){
    return _showRangeLines;
}

void Appearance::setShowRangeLines(bool enabled){
    _showRangeLines = enabled;
}

QString Appearance::applicationStyle(){
    return _applicationStyle;
}

void Appearance::setApplicationStyle(const QString& style){
    _applicationStyle = style;
    applyStyle();
    // Refresh colors when style changes since dark mode behavior depends on style
    refreshColors();
}

int Appearance::toolbarIconSize(){
    return _toolbarIconSize;
}

void Appearance::setToolbarIconSize(int size){
    _toolbarIconSize = size;
    notifyIconSizeChanged();
}

QStringList Appearance::availableStyles(){
    // Only return QWidget styles that actually work with QApplication::setStyle()
    // Qt Quick Controls styles (Material, Universal, FluentWinUI3, etc.) don't work with QWidget applications
    QStringList styles = QStyleFactory::keys();
    styles.sort();
    return styles;
}



void Appearance::applyStyle(){
    QApplication* app = qobject_cast<QApplication*>(QApplication::instance());
    if (!app) return;

    // Apply QWidget style first
    if (QStyleFactory::keys().contains(_applicationStyle)) {
        app->setStyle(_applicationStyle);
    }

    // Note: autoResetDefaultColors() is called in refreshColors() after this method

    // Apply dark mode specific styling if needed
    if (shouldUseDarkMode()) {
        QString darkStyleSheet =
            "QToolButton:checked { "
            "    background-color: rgba(80, 80, 80, 150); "
            "    border: 1px solid rgba(120, 120, 120, 150); "
            "}";
        app->setStyleSheet(darkStyleSheet);
    } else {
        app->setStyleSheet(""); // Clear custom styling for light mode
    }
}

void Appearance::notifyIconSizeChanged(){
    // Find the main window and update only the main toolbar icon size
    QApplication* app = qobject_cast<QApplication*>(QApplication::instance());
    if (!app) return;

    // Find all top-level widgets and look for the main window
    foreach (QWidget* widget, app->topLevelWidgets()) {
        if (widget->objectName() == "MainWindow" || widget->inherits("MainWindow")) {
            // Find only the main toolbar (not channel/track toolbars)
            // Look for toolbar with name "File" which is the main toolbar
            QList<QToolBar*> toolbars = widget->findChildren<QToolBar*>();
            foreach (QToolBar* toolbar, toolbars) {
                if (toolbar->windowTitle() == "File") {  // This is the main toolbar
                    toolbar->setIconSize(QSize(_toolbarIconSize, _toolbarIconSize));
                    break;  // Only update the main toolbar
                }
            }
            break;
        }
    }
}

// Dark mode detection and color scheme methods
bool Appearance::isDarkModeEnabled() {
    // Use Qt's built-in dark mode detection
    QStyleHints* hints = QApplication::styleHints();
    return hints->colorScheme() == Qt::ColorScheme::Dark;
}

bool Appearance::shouldUseDarkMode() {
    // Only use dark mode for specific styles that support it
    QString style = _applicationStyle.toLower();
    if (style == "windowsvista") {
        return false; // Always use light mode for WindowsVista
    }

    // For Windows11, Windows, and Fusion styles, respect system dark mode
    if (style == "windows11" || style == "windows" || style == "fusion") {
        return isDarkModeEnabled();
    }

    return false; // Default to light mode for other styles
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

QPixmap Appearance::adjustIconForDarkMode(const QPixmap& original, const QString& iconName) {
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

QIcon Appearance::adjustIconForDarkMode(const QString& iconPath) {
    QPixmap original(iconPath);

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
    // Refresh all registered icon actions with their updated icons
    // Also clean up any destroyed actions
    auto it = registeredIconActions.begin();
    while (it != registeredIconActions.end()) {
        QAction* action = it.key();
        const QString& iconPath = it.value();

        if (action) {
            // Update the action's icon with the current theme-appropriate version
            action->setIcon(adjustIconForDarkMode(iconPath));
            ++it;
        } else {
            // Remove destroyed actions
            it = registeredIconActions.erase(it);
        }
    }
}

void Appearance::registerIconAction(QAction* action, const QString& iconPath) {
    if (action) {
        registeredIconActions[action] = iconPath;
    }
}

void Appearance::setActionIcon(QAction* action, const QString& iconPath) {
    if (action) {
        action->setIcon(adjustIconForDarkMode(iconPath));
        registerIconAction(action, iconPath);
    }
}

void Appearance::refreshColors() {
    // Auto-reset default colors for the new theme
    autoResetDefaultColors();

    // Force all widgets to update their colors by triggering a repaint
    QApplication* app = qobject_cast<QApplication*>(QApplication::instance());
    if (!app) return;

    // Update all top-level widgets
    foreach (QWidget* widget, app->topLevelWidgets()) {
        if (widget->isVisible()) {
            // Force a complete repaint with style refresh
            widget->style()->unpolish(widget);
            widget->style()->polish(widget);
            widget->update();

            // Also update all child widgets recursively
            QList<QWidget*> children = widget->findChildren<QWidget*>();
            foreach (QWidget* child, children) {
                child->style()->unpolish(child);
                child->style()->polish(child);
                child->update();
            }

            // Refresh all ToolButton icons for theme changes
            QList<ToolButton*> toolButtons = widget->findChildren<ToolButton*>();
            foreach (ToolButton* toolButton, toolButtons) {
                toolButton->refreshIcon();
            }

            // Refresh all ProtocolWidget colors for theme changes
            QList<ProtocolWidget*> protocolWidgets = widget->findChildren<ProtocolWidget*>();
            foreach (ProtocolWidget* protocolWidget, protocolWidgets) {
                protocolWidget->refreshColors();
            }

            // Refresh all MatrixWidget colors for theme changes
            QList<MatrixWidget*> matrixWidgets = widget->findChildren<MatrixWidget*>();
            foreach (MatrixWidget* matrixWidget, matrixWidgets) {
                matrixWidget->forceCompleteRedraw();
            }

            // Refresh all AppearanceSettingsWidget colors for theme changes
            QList<AppearanceSettingsWidget*> appearanceWidgets = widget->findChildren<AppearanceSettingsWidget*>();
            foreach (AppearanceSettingsWidget* appearanceWidget, appearanceWidgets) {
                appearanceWidget->refreshColors();
            }

            // Force immediate processing of paint events
            app->processEvents();
        }
    }

    // Refresh all icons after widget updates
    refreshAllIcons();

    // Reapply styling for theme changes
    applyStyle();
}

void Appearance::forceColorRefresh() {
    // Public method that can be called from settings dialogs or other places
    refreshColors();
}

void Appearance::connectToSystemThemeChanges() {
    QApplication* app = qobject_cast<QApplication*>(QApplication::instance());
    if (!app) return;

    // Connect to system theme change detection
    QStyleHints* hints = app->styleHints();

    // Connect to colorSchemeChanged signal
    QObject::connect(hints, &QStyleHints::colorSchemeChanged, [](Qt::ColorScheme colorScheme) {
        Q_UNUSED(colorScheme)
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
