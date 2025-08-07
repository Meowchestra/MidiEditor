#ifndef APPEARANCE_H_
#define APPEARANCE_H_

// Qt includes
#include <QSettings>
#include <QColor>
#include <QStringList>
#include <QMap>
#include <QString>
#include <QSet>

// Forward declarations
class QAction;

/**
 * \class Appearance
 *
 * \brief Static class managing visual appearance and theming for the MIDI editor.
 *
 * Appearance provides centralized management of all visual styling options
 * in the MIDI editor, including colors, themes, and UI preferences:
 *
 * - **Color management**: Channel and track color customization
 * - **Theme support**: Light/dark theme switching
 * - **UI styling**: Application style and icon size configuration
 * - **Visual effects**: Opacity, strips, and range line settings
 * - **Settings persistence**: Save/load appearance preferences
 * - **Dynamic updates**: Real-time appearance changes
 *
 * Key features:
 * - Static interface for global access
 * - Comprehensive color management for channels and tracks
 * - Theme-aware color schemes with automatic adaptation
 * - UI style customization (toolbar icons, application style)
 * - Settings integration for persistent preferences
 * - Color reset and auto-reset functionality
 *
 * The class serves as the central point for all appearance-related
 * configuration and provides consistent theming across the application.
 */
class Appearance {
public:
    // === Initialization and Settings ===

    /**
     * \brief Initializes the appearance system with settings.
     * \param settings QSettings instance for loading preferences
     */
    static void init(QSettings *settings);

    /**
     * \brief Writes current appearance settings to storage.
     * \param settings QSettings instance for saving preferences
     */
    static void writeSettings(QSettings *settings);

    /**
     * \brief Cleans up static resources to prevent shutdown issues.
     */
    static void cleanup();

    /**
     * \brief Sets the application shutdown flag to prevent QPixmap creation during shutdown.
     */
    static void setShuttingDown(bool shuttingDown);

    // === Color Management ===

    /**
     * \brief Gets the color for a specific MIDI channel.
     * \param channel MIDI channel number (0-15)
     * \return Pointer to the channel's color
     */
    static QColor *channelColor(int channel);

    /**
     * \brief Gets the color for a specific MIDI track.
     * \param track Track number
     * \return Pointer to the track's color
     */
    static QColor *trackColor(int track);

    /**
     * \brief Sets the color for a specific MIDI track.
     * \param track Track number
     * \param color New color for the track
     */
    static void setTrackColor(int track, QColor color);

    /**
     * \brief Sets the color for a specific MIDI channel.
     * \param channel MIDI channel number (0-15)
     * \param color New color for the channel
     */
    static void setChannelColor(int channel, QColor color);

    /**
     * \brief Resets all appearance settings to defaults.
     */
    static void reset();

    /**
     * \brief Auto-resets colors if they're still at default values.
     */
    static void autoResetDefaultColors();

    /**
     * \brief Forces reset of all colors to current theme defaults.
     */
    static void forceResetAllColors();

    // === Visual Effects ===

    /**
     * \brief Gets the current opacity setting.
     * \return Opacity value (0-100)
     */
    static int opacity();

    /**
     * \brief Sets the opacity for visual elements.
     * \param opacity Opacity value (0-100)
     */
    static void setOpacity(int opacity);

    /**
     * \brief Strip style enumeration for piano roll background.
     */
    enum stripStyle {
        onOctave = 0,  ///< Strips on octave boundaries
        onSharp = 1,   ///< Strips on sharp/flat keys
        onEven = 2,    ///< Strips on even-numbered keys
    };

    /**
     * \brief Gets the current strip style.
     * \return Current stripStyle setting
     */
    static stripStyle strip();

    /**
     * \brief Sets the strip style for the piano roll.
     * \param style The stripStyle to use
     */
    static void setStrip(stripStyle style);

    /**
     * \brief Gets whether range lines are shown.
     * \return True if range lines are displayed
     */
    static bool showRangeLines();

    /**
     * \brief Sets whether to show range lines.
     * \param enabled True to show range lines, false to hide them
     */
    static void setShowRangeLines(bool enabled);

    // === UI Styling Options ===

    /**
     * \brief Gets the current application style name.
     * \return String name of the current application style
     */
    static QString applicationStyle();

    /**
     * \brief Sets the application style.
     * \param style The name of the style to apply
     */
    static void setApplicationStyle(const QString &style);

    /**
     * \brief Gets the current toolbar icon size.
     * \return Icon size in pixels
     */
    static int toolbarIconSize();

    /**
     * \brief Sets the toolbar icon size.
     * \param size Icon size in pixels
     */
    static void setToolbarIconSize(int size);

    /**
     * \brief Gets the list of available application styles.
     * \return QStringList containing available style names
     */
    static QStringList availableStyles();

    // === Toolbar Layout Options ===

    /**
     * \brief Gets whether toolbar is in two-row mode.
     * \return True if toolbar uses two rows
     */
    static bool toolbarTwoRowMode();

    /**
     * \brief Sets the toolbar row mode.
     * \param twoRows True for two-row mode, false for single row
     */
    static void setToolbarTwoRowMode(bool twoRows);

    /**
     * \brief Gets whether toolbar customization is enabled.
     * \return True if toolbar can be customized
     */
    static bool toolbarCustomizeEnabled();

    /**
     * \brief Sets whether toolbar customization is enabled.
     * \param enabled True to enable customization, false to disable
     */
    static void setToolbarCustomizeEnabled(bool enabled);

    /**
     * \brief Gets the current toolbar action order.
     * \return QStringList containing action names in display order
     */
    static QStringList toolbarActionOrder();

    /**
     * \brief Sets the toolbar action order.
     * \param order QStringList containing action names in desired order
     */
    static void setToolbarActionOrder(const QStringList &order);

    /**
     * \brief Gets the list of enabled toolbar actions.
     * \return QStringList containing names of enabled actions
     */
    static QStringList toolbarEnabledActions();

    /**
     * \brief Sets which toolbar actions are enabled.
     * \param enabled QStringList containing names of actions to enable
     */
    static void setToolbarEnabledActions(const QStringList &enabled);

    // === High DPI Scaling Control ===

    /**
     * \brief Sets whether to ignore system DPI scaling.
     * \param ignore True to ignore system scaling, false to use it
     */
    static void setIgnoreSystemScaling(bool ignore);

    /**
     * \brief Gets whether system DPI scaling is ignored.
     * \return True if ignoring system scaling
     */
    static bool ignoreSystemScaling();

    /**
     * \brief Sets whether to ignore system font scaling.
     * \param ignore True to ignore font scaling, false to use it
     */
    static void setIgnoreFontScaling(bool ignore);

    /**
     * \brief Gets whether system font scaling is ignored.
     * \return True if ignoring font scaling
     */
    static bool ignoreFontScaling();

    /**
     * \brief Sets whether to use rounded scaling factors.
     * \param useRounded True to use rounded scaling, false for precise scaling
     */
    static void setUseRoundedScaling(bool useRounded);

    /**
     * \brief Gets whether rounded scaling is used.
     * \return True if using rounded scaling factors
     */
    static bool useRoundedScaling();

    /**
     * \brief Loads settings needed before QApplication creation.
     */
    static void loadEarlySettings();

    /**
     * \brief Gets the MSAA samples setting loaded early.
     * \return Number of MSAA samples (0, 2, 4, or 8)
     */
    static int msaaSamples();

    /**
     * \brief Gets the VSync setting loaded early.
     * \return True if VSync should be enabled
     */
    static bool enableVSync();

    /**
     * \brief Gets the hardware acceleration setting loaded early.
     * \return True if hardware acceleration should be enabled
     */
    static bool useHardwareAcceleration();

    // === Font and Style Management ===

    /**
     * \brief Improves font rendering for better display.
     * \param font The font to improve
     * \return Improved QFont with better rendering settings
     */
    static QFont improveFont(const QFont &font);

    /**
     * \brief Applies the current style to the application.
     */
    static void applyStyle();

    /**
     * \brief Notifies components that icon size has changed.
     */
    static void notifyIconSizeChanged();

    /**
     * \brief Forces a refresh of all colors.
     */
    static void forceColorRefresh();

    // === Dark Mode Support ===

    /**
     * \brief Checks if dark mode is enabled.
     * \return True if dark mode is active
     */
    static bool isDarkModeEnabled();

    /**
     * \brief Determines if dark mode should be used.
     * \return True if dark mode should be applied
     */
    static bool shouldUseDarkMode();

    /**
     * \brief Refreshes all UI colors when theme changes.
     */
    static void refreshColors();

    /**
     * \brief Connects to system theme change signals.
     */
    static void connectToSystemThemeChanges();

    // === Color Scheme Methods ===

    /**
     * \brief Gets the background color for the current theme.
     * \return QColor for the background
     */
    static QColor backgroundColor();

    /**
     * \brief Gets the background shade color for the current theme.
     * \return QColor for the background shade
     */
    static QColor backgroundShade();

    /**
     * \brief Gets the foreground color for the current theme.
     * \return QColor for the foreground
     */
    static QColor foregroundColor();

    /**
     * \brief Gets the light gray color for the current theme.
     * \return QColor for light gray elements
     */
    static QColor lightGrayColor();

    /**
     * \brief Gets the dark gray color for the current theme.
     * \return QColor for dark gray elements
     */
    static QColor darkGrayColor();

    /**
     * \brief Gets the standard gray color for the current theme.
     * \return QColor for gray elements
     */
    static QColor grayColor();

    /**
     * \brief Gets the piano white key color for the current theme.
     * \return QColor for piano white keys
     */
    static QColor pianoWhiteKeyColor();

    /**
     * \brief Gets the piano black key color for the current theme.
     * \return QColor for piano black keys
     */
    static QColor pianoBlackKeyColor();

    /**
     * \brief Gets the piano white key hover color for the current theme.
     * \return QColor for hovered piano white keys
     */
    static QColor pianoWhiteKeyHoverColor();

    /**
     * \brief Gets the piano black key hover color for the current theme.
     * \return QColor for hovered piano black keys
     */
    static QColor pianoBlackKeyHoverColor();

    /**
     * \brief Gets the piano white key selected color for the current theme.
     * \return QColor for selected piano white keys
     */
    static QColor pianoWhiteKeySelectedColor();

    /**
     * \brief Gets the piano black key selected color for the current theme.
     * \return QColor for selected piano black keys
     */
    static QColor pianoBlackKeySelectedColor();

    /**
     * \brief Gets the strip highlight color for the current theme.
     * \return QColor for highlighted strips
     */
    static QColor stripHighlightColor();

    /**
     * \brief Gets the normal strip color for the current theme.
     * \return QColor for normal strips
     */
    static QColor stripNormalColor();

    /**
     * \brief Gets the range line color for the current theme.
     * \return QColor for range lines
     */
    static QColor rangeLineColor();

    /**
     * \brief Gets the velocity editor background color for the current theme.
     * \return QColor for velocity editor background
     */
    static QColor velocityBackgroundColor();

    /**
     * \brief Gets the velocity editor grid color for the current theme.
     * \return QColor for velocity editor grid lines
     */
    static QColor velocityGridColor();

    /**
     * \brief Gets the system window color from the application palette.
     * \return QColor from QApplication::palette().window()
     */
    static QColor systemWindowColor();

    /**
     * \brief Gets the system text color from the application palette.
     * \return QColor from QApplication::palette().windowText()
     */
    static QColor systemTextColor();

    /**
     * \brief Gets the info box background color for the current theme.
     * \return QColor for info box backgrounds
     */
    static QColor infoBoxBackgroundColor();

    /**
     * \brief Gets the info box text color for the current theme.
     * \return QColor for info box text
     */
    static QColor infoBoxTextColor();

    /**
     * \brief Gets the toolbar background color for the current theme.
     * \return QColor for toolbar backgrounds
     */
    static QColor toolbarBackgroundColor();

    /**
     * \brief Gets the border color for the current theme.
     * \return QColor for borders
     */
    static QColor borderColor();

    /**
     * \brief Gets the alternative border color for the current theme.
     * \return QColor for alternative borders
     */
    static QColor borderColorAlt();

    /**
     * \brief Gets the selection border color for the current theme.
     * \return QColor for selected notes and velocity bars
     */
    static QColor selectionBorderColor();

    /**
     * \brief Gets the error color for the current theme.
     * \return QColor for error indicators
     */
    static QColor errorColor();

    /**
     * \brief Gets the cursor line color for the current theme.
     * \return QColor for cursor lines
     */
    static QColor cursorLineColor();

    /**
     * \brief Gets the cursor triangle color for the current theme.
     * \return QColor for cursor triangles
     */
    static QColor cursorTriangleColor();

    /**
     * \brief Gets the tempo tool highlight color for the current theme.
     * \return QColor for tempo tool highlights
     */
    static QColor tempoToolHighlightColor();

    /**
     * \brief Gets the measure tool highlight color for the current theme.
     * \return QColor for measure tool highlights
     */
    static QColor measureToolHighlightColor();

    /**
     * \brief Gets the time signature tool highlight color for the current theme.
     * \return QColor for time signature tool highlights
     */
    static QColor timeSignatureToolHighlightColor();

    /**
     * \brief Gets the piano key line highlight color for the current theme.
     * \return QColor for piano key line highlights
     */
    static QColor pianoKeyLineHighlightColor();

    /**
     * \brief Gets the measure text color for the current theme.
     * \return QColor for measure text
     */
    static QColor measureTextColor();

    /**
     * \brief Gets the measure bar color for the current theme.
     * \return QColor for measure bars
     */
    static QColor measureBarColor();

    /**
     * \brief Gets the measure line color for the current theme.
     * \return QColor for measure lines
     */
    static QColor measureLineColor();

    /**
     * \brief Gets the timeline grid color for the current theme.
     * \return QColor for timeline grid lines
     */
    static QColor timelineGridColor();

    /**
     * \brief Gets the playback cursor color for the current theme.
     * \return QColor for the playback cursor
     */
    static QColor playbackCursorColor();

    /**
     * \brief Gets the recording indicator color for the current theme.
     * \return QColor for recording indicators
     */
    static QColor recordingIndicatorColor();

    /**
     * \brief Gets the program event highlight color for the current theme.
     * \return QColor for highlighted program events
     */
    static QColor programEventHighlightColor();

    /**
     * \brief Gets the program event normal color for the current theme.
     * \return QColor for normal program events
     */
    static QColor programEventNormalColor();

    /**
     * \brief Gets the note selection color for the current theme.
     * \return QColor for selected/dragged notes
     */
    static QColor noteSelectionColor();

    // === Icon Adjustment ===

    /**
     * \brief Adjusts an icon for dark mode compatibility.
     * \param original The original pixmap to adjust
     * \param iconName Optional icon name for caching
     * \return QPixmap adjusted for the current theme
     */
    static QPixmap adjustIconForDarkMode(const QPixmap &original, const QString &iconName = "");

    static QIcon adjustIconForDarkMode(const QString &iconPath);

    /** \brief Refresh all icons in the application */
    static void refreshAllIcons();

    /** \brief Register action for icon refresh */
    static void registerIconAction(QAction *action, const QString &iconPath);

    /** \brief Set icon and register for refresh */
    static void setActionIcon(QAction *action, const QString &iconPath);

private:
    // === Internal icon processing functions ===
    /** \brief Start processing icons from queue */
    static void startQueuedIconProcessing();

    /** \brief Process next icon in queue */
    static void processNextQueuedIcon();

private:
    // === Color Index Conversion ===

    /**
     * \brief Converts track number to color index.
     * \param track The track number
     * \return Color index for the track
     */
    static int trackToColorIndex(int track);

    /**
     * \brief Converts channel number to color index.
     * \param channel The channel number
     * \return Color index for the channel
     */
    static int channelToColorIndex(int channel);

    // === Static Data Members ===

    /** \brief Maps for storing colors - using QHash for O(1) lookup instead of O(log n) */
    static QHash<int, QColor *> channelColors;
    static QHash<int, QColor *> trackColors;
    static QSet<int> customChannelColors;                  ///< Track which channel colors are custom
    static QSet<int> customTrackColors;                    ///< Track which track colors are custom
    static QMap<QAction *, QString> registeredIconActions; ///< Track actions with their icon paths

    /**
     * \brief Gets the default color for an index.
     * \param n The color index
     * \return Pointer to the default QColor
     */
    static QColor *defaultColor(int n);

    // === Settings I/O ===

    /**
     * \brief Decodes a color from settings.
     * \param name The setting name
     * \param settings The QSettings instance
     * \param defaultColor The default color if not found
     * \return Pointer to the decoded QColor
     */
    static QColor *decode(QString name, QSettings *settings, QColor *defaultColor);

    /**
     * \brief Writes a color to settings.
     * \param name The setting name
     * \param settings The QSettings instance
     * \param color The color to write
     */
    static void write(QString name, QSettings *settings, QColor *color);

    // === Configuration Variables ===

    /** \brief Current opacity setting */
    static int _opacity;

    /** \brief Current strip style */
    static stripStyle _strip;

    /** \brief Whether to show range lines */
    static bool _showRangeLines;

    /** \brief Current application style name */
    static QString _applicationStyle;

    /** \brief Current toolbar icon size */
    static int _toolbarIconSize;

    /** \brief Whether toolbar is in two-row mode */
    static bool _toolbarTwoRowMode;

    /** \brief Whether toolbar customization is enabled */
    static bool _toolbarCustomizeEnabled;

    /** \brief Order of toolbar actions */
    static QStringList _toolbarActionOrder;

    /** \brief List of enabled toolbar actions */
    static QStringList _toolbarEnabledActions;

    /** \brief Whether to ignore system DPI scaling */
    static bool _ignoreSystemScaling;

    /** \brief Whether to ignore system font scaling */
    static bool _ignoreFontScaling;

    /** \brief Whether to use rounded scaling factors */
    static bool _useRoundedScaling;

    /** \brief MSAA samples setting loaded early */
    static int _msaaSamples;

    /** \brief VSync setting loaded early */
    static bool _enableVSync;

    /** \brief Hardware acceleration setting loaded early */
    static bool _useHardwareAcceleration;

    /** \brief Flag to prevent QPixmap creation during application shutdown */
    static bool _shuttingDown;
};

#endif // APPEARANCE_H_
