#ifndef APPEARANCE_H
#define APPEARANCE_H

#include <QSettings>
#include <QColor>
#include <QStringList>

class Appearance
{
public:
    static void init(QSettings *settings);
    static QColor *channelColor(int channel);
    static QColor *trackColor(int track);
    static void writeSettings(QSettings *settings);
    static void setTrackColor(int track, QColor color);
    static void setChannelColor(int channel, QColor color);
    static void reset();
    static int opacity();
    static void setOpacity(int opacity);

    enum stripStyle{
        onOctave = 0,
        onSharp = 1,
        onEven = 2,
    };
    static stripStyle strip();
    static void setStrip(stripStyle opacity);
    static bool showRangeLines();
    static void setShowRangeLines(bool enabled);

    // UI Styling options
    static QString applicationStyle();
    static void setApplicationStyle(const QString& style);
    static int toolbarIconSize();
    static void setToolbarIconSize(int size);
    static QStringList availableStyles();
    static void applyStyle();
    static void notifyIconSizeChanged();

private:
    static int trackToColorIndex(int track);
    static int channelToColorIndex(int channel);
    static QMap<int, QColor*> channelColors;
    static QMap<int, QColor*> trackColors;
    static QColor *defaultColor(int n);
    static QColor *decode(QString name, QSettings *settings, QColor *defaultColor);
    static void write(QString name, QSettings *settings, QColor *color);
    static int _opacity;
    static stripStyle _strip;
    static bool _showRangeLines;
    static QString _applicationStyle;
    static int _toolbarIconSize;
};

#endif // APPEARANCE_H
