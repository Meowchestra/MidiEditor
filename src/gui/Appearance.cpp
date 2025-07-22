#include "Appearance.h"
#include <QApplication>
#include <QStyleFactory>
#include <QPalette>
#include <QStyle>
#include <QToolBar>
#include <QWidget>

QMap<int, QColor*> Appearance::channelColors = QMap<int, QColor*>();
QMap<int, QColor*> Appearance::trackColors = QMap<int, QColor*>();
int Appearance::_opacity = 100;
Appearance::stripStyle Appearance::_strip = Appearance::onSharp;
bool Appearance::_showRangeLines = false;
QString Appearance::_applicationStyle = "windowsvista";
int Appearance::_toolbarIconSize = 20;

void Appearance::init(QSettings *settings){
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

    // Apply the style after loading settings
    applyStyle();
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
}

QColor *Appearance::defaultColor(int n) {
    QColor* color;

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
    trackColors[trackToColorIndex(track)] = new QColor(color);
}

void Appearance::setChannelColor(int channel, QColor color){
    channelColors[channelToColorIndex(channel)] = new QColor(color);
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
    for (int channel = 0; channel < 17; channel++) {
        channelColors[channel] = defaultColor(channel);
    }
    for (int track = 0; track < 17; track++) {
        trackColors[track] = defaultColor(track);
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

    // Apply QWidget style
    if (QStyleFactory::keys().contains(_applicationStyle)) {
        app->setStyle(_applicationStyle);
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
