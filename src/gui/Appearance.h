#ifndef APPEARANCE_H
#define APPEARANCE_H

#include <QSettings>
#include <QColor>
#include <QStringList>
#include <QMap>
#include <QString>
#include <QSet>

class QAction;

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
    static void autoResetDefaultColors(); // Auto-reset colors if they're still default
    static void forceResetAllColors(); // Force reset all colors to current theme defaults
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

    // High DPI scaling control
    static void setIgnoreSystemScaling(bool ignore);
    static bool ignoreSystemScaling();
    static void setUseRoundedScaling(bool useRounded);
    static bool useRoundedScaling();
    static void loadEarlySettings(); // Load settings needed before QApplication creation

    // Font rendering improvements
    static QFont improveFont(const QFont& font);
    static void applyStyle();
    static void notifyIconSizeChanged();
    static void forceColorRefresh(); // Public method to manually trigger color refresh

    // Dark mode support
    static bool isDarkModeEnabled();
    static bool shouldUseDarkMode();
    static void refreshColors(); // Refresh all UI colors when theme changes
    static void connectToSystemThemeChanges(); // Connect to system theme change signals

    // Color scheme methods
    static QColor backgroundColor();
    static QColor backgroundShade();
    static QColor foregroundColor();
    static QColor lightGrayColor();
    static QColor darkGrayColor();
    static QColor grayColor();
    static QColor pianoWhiteKeyColor();
    static QColor pianoBlackKeyColor();
    static QColor pianoWhiteKeyHoverColor();
    static QColor pianoBlackKeyHoverColor();
    static QColor pianoWhiteKeySelectedColor();
    static QColor pianoBlackKeySelectedColor();
    static QColor stripHighlightColor();
    static QColor stripNormalColor();
    static QColor rangeLineColor();
    static QColor velocityBackgroundColor();
    static QColor velocityGridColor();
    static QColor systemWindowColor(); // QApplication::palette().window()
    static QColor systemTextColor(); // QApplication::palette().windowText()
    static QColor infoBoxBackgroundColor();
    static QColor infoBoxTextColor();
    static QColor toolbarBackgroundColor();
    static QColor borderColor();
    static QColor borderColorAlt();
    static QColor selectionBorderColor(); // For selected notes and velocity bars
    static QColor errorColor();
    static QColor cursorLineColor();
    static QColor cursorTriangleColor();
    static QColor tempoToolHighlightColor();
    static QColor measureToolHighlightColor();
    static QColor timeSignatureToolHighlightColor();
    static QColor pianoKeyLineHighlightColor();
    static QColor measureTextColor();
    static QColor measureBarColor();
    static QColor measureLineColor();
    static QColor timelineGridColor();
    static QColor playbackCursorColor();
    static QColor recordingIndicatorColor();
    static QColor programEventHighlightColor();
    static QColor programEventNormalColor();
    static QColor noteSelectionColor(); // Consistent color for selected/dragged notes

    // Icon adjustment for dark mode
    static QPixmap adjustIconForDarkMode(const QPixmap& original, const QString& iconName = "");
    static QIcon adjustIconForDarkMode(const QString& iconPath);
    static void refreshAllIcons(); // Refresh all icons in the application
    static void registerIconAction(QAction* action, const QString& iconPath); // Register action for icon refresh
    static void setActionIcon(QAction* action, const QString& iconPath); // Set icon and register for refresh

private:
    // Internal icon processing functions
    static void startQueuedIconProcessing(); // Start processing icons from queue
    static void processNextQueuedIcon(); // Process next icon in queue
    static void cleanupIconRegistry(); // Remove invalid actions from registry (automatic)

private:
    static int trackToColorIndex(int track);
    static int channelToColorIndex(int channel);
    static QMap<int, QColor*> channelColors;
    static QMap<int, QColor*> trackColors;
    static QSet<int> customChannelColors; // Track which channel colors are custom
    static QSet<int> customTrackColors; // Track which track colors are custom
    static QMap<QAction*, QString> registeredIconActions; // Track actions with their icon paths
    static QColor *defaultColor(int n);
    static QColor *decode(QString name, QSettings *settings, QColor *defaultColor);
    static void write(QString name, QSettings *settings, QColor *color);
    static int _opacity;
    static stripStyle _strip;
    static bool _showRangeLines;
    static QString _applicationStyle;
    static int _toolbarIconSize;
    static bool _ignoreSystemScaling;
    static bool _useRoundedScaling;
};

#endif // APPEARANCE_H
