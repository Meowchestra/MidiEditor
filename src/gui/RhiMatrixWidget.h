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

#ifndef RHIMATRIXWIDGET_H
#define RHIMATRIXWIDGET_H

#include <QRhiWidget>
#include "IMatrixWidget.h"
#include <QSettings>
#include <QTimer>
#include <QElapsedTimer>
#include <QMap>
#include <QRectF>

// Qt RHI includes - using official Qt 6.10 API only
#include <rhi/qrhi.h>

// Forward declarations
class MidiFile;
class MidiEvent;
class NoteOnEvent;
class GraphicObject;

// Include Appearance for stripStyle enum
#include "Appearance.h"

// RHI rendering constants matching MatrixWidget for exact compatibility
#define RHI_PIXEL_PER_LINE 11
#define RHI_PIXEL_PER_S 100
#define RHI_NUM_LINES 139
#define RHI_PIXEL_PER_EVENT 15
#define RHI_MAX_INSTANCES 65536
#define RHI_UNIFORM_BUFFER_SIZE 1024
#define RHI_VERTEX_BUFFER_SIZE 4096

/**
 * \class RhiMatrixWidget
 *
 * \brief Hardware-accelerated Qt RHI implementation of MatrixWidget.
 *
 * RhiMatrixWidget provides GPU-accelerated rendering of MIDI events using Qt's
 * Rendering Hardware Interface (RHI). It maintains exact functionality and
 * behavior compatibility with the software MatrixWidget while offering
 * significantly improved performance for large numbers of MIDI events.
 *
 * **Hardware Acceleration Features:**
 * - **Automatic API Selection**: D3D12 -> D3D11 -> Vulkan -> OpenGL -> Software fallback
 * - **Instanced Rendering**: Efficient batch rendering of MIDI events
 * - **GPU Vertex Buffers**: All geometry processed on GPU
 * - **Shader-based Rendering**: Custom shaders for each visual element
 * - **Dynamic Uniform Buffers**: Real-time color and setting updates
 *
 * **Rendering Pipeline:**
 * - Background rendering with solid colors
 * - Piano key rendering with instanced quads
 * - MIDI event rendering with instanced rounded rectangles
 * - Line rendering for grid, measures, and cursors
 * - Text rendering with font atlas (fallback to software for complex text)
 *
 * **Performance Optimizations:**
 * - Batch updates to minimize GPU state changes
 * - Frustum culling for off-screen events
 * - Dynamic LOD for distant/small events
 * - Efficient memory management with buffer pools
 */
class RhiMatrixWidget : public QRhiWidget, public virtual IMatrixWidget {
    Q_OBJECT

public:
    /**
     * \brief Creates a new RhiMatrixWidget.
     * \param settings Application settings for configuration
     * \param parent Parent widget
     */
    explicit RhiMatrixWidget(QSettings *settings, QWidget *parent = nullptr);

    /**
     * \brief Destructor - cleans up RHI resources.
     */
    ~RhiMatrixWidget() override;

    // === Core Interface (matches MatrixWidget) ===

    /**
     * \brief Sets the MIDI file to display.
     * \param file The MidiFile to display and edit
     */
    void setFile(MidiFile *file);

    /**
     * \brief Gets the current MIDI file.
     * \return Pointer to the current MidiFile, or nullptr if none
     */
    MidiFile *midiFile();

    /**
     * \brief Gets the list of active events for rendering.
     * \return List of MidiEvent pointers currently visible
     */
    QList<MidiEvent *> *activeEvents();

    /**
     * \brief Gets the list of events for velocity editing.
     * \return List of MidiEvent pointers for velocity display
     */
    QList<MidiEvent *> *velocityEvents();

    /**
     * \brief Gets the list of graphic objects for rendering.
     * \return List of GraphicObject pointers (recast from events)
     */
    QList<GraphicObject *> *getObjects() { return reinterpret_cast<QList<GraphicObject *> *>(objects); }

    // === Coordinate Conversion ===

    /**
     * \brief Gets the height of each piano key line.
     * \return Height in pixels per MIDI note line
     */
    double lineHeight();

    /**
     * \brief Converts Y coordinate to MIDI note line number.
     * \param y Y coordinate in pixels
     * \return MIDI note line number (0-127)
     */
    int lineAtY(int y);

    /**
     * \brief Converts MIDI note line to Y coordinate.
     * \param line MIDI note line number (0-127)
     * \return Y coordinate in pixels
     */
    int yPosOfLine(int line);

    /**
     * \brief Converts X coordinate to time in milliseconds.
     * \param x X coordinate in pixels
     * \return Time in milliseconds
     */
    int msOfXPos(int x);

    /**
     * \brief Converts time in milliseconds to X coordinate.
     * \param ms Time in milliseconds
     * \return X coordinate in pixels
     */
    int xPosOfMs(int ms);

    /**
     * \brief Converts MIDI tick to time in milliseconds.
     * \param tick MIDI tick value
     * \return Time in milliseconds
     */
    int msOfTick(int tick);

    /**
     * \brief Converts time in milliseconds to MIDI tick.
     * \param ms Time in milliseconds
     * \return MIDI tick value
     */
    int tickOfMs(int ms);

    /**
     * \brief Plays a note when piano key is clicked.
     * \param note MIDI note number (0-127)
     */
    void playNote(int note);

    /**
     * \brief Converts width in pixels to time duration.
     * \param w Width in pixels
     * \return Time duration in milliseconds
     */
    int timeMsOfWidth(int w);

    /**
     * \brief Gets the minimum visible MIDI time.
     * \return Minimum time in milliseconds currently visible
     */
    int minVisibleMidiTime();

    /**
     * \brief Gets the maximum visible MIDI time.
     * \return Maximum time in milliseconds currently visible
     */
    int maxVisibleMidiTime();

    // === Event Testing ===

    /**
     * \brief Tests if an event is currently visible in the widget.
     * \param event The MidiEvent to test
     * \return True if the event is within the visible area
     */
    bool eventInWidget(MidiEvent *event);

    // === View Control ===

    /**
     * \brief Sets the viewport to display a specific time and pitch range.
     * \param startTick Starting MIDI tick
     * \param endTick Ending MIDI tick
     * \param startLine Starting MIDI note line
     * \param endLine Ending MIDI note line
     */
    void setViewport(int startTick, int endTick, int startLine, int endLine);

    /**
     * \brief Sets the width of the line name area (piano keys).
     * \param width Width in pixels for the piano key area
     */
    void setLineNameWidth(int width) {
        lineNameWidth = width;
        update();
    }

    /**
     * \brief Gets the width of the line name area.
     * \return Width in pixels of the piano key area
     */
    int getLineNameWidth() { return lineNameWidth; }

    /**
     * \brief Sets the height of the timeline area.
     * \param height Height in pixels for the timeline area
     */
    void setTimeHeight(int height) {
        timeHeight = height;
        update();
    }

    /**
     * \brief Gets the height of the timeline area.
     * \return Height in pixels of the timeline area
     */
    int getTimeHeight() { return timeHeight; }

    // === Scaling and Zoom ===

    /**
     * \brief Sets the horizontal scaling factor.
     * \param scale Scaling factor for time axis
     */
    void setScaleX(double scale);

    /**
     * \brief Sets the vertical scaling factor.
     * \param scale Scaling factor for pitch axis
     */
    void setScaleY(double scale);

    /**
     * \brief Gets the horizontal scaling factor.
     * \return Current time axis scaling factor
     */
    double getScaleX() { return scaleX; }

    /**
     * \brief Gets the vertical scaling factor.
     * \return Current pitch axis scaling factor
     */
    double getScaleY() { return scaleY; }

    // === Configuration ===

    /**
     * \brief Sets the time division for grid display.
     * \param div Time division value
     */
    void setDiv(int div);

    /**
     * \brief Gets the current time division.
     * \return Current time division value
     */
    int getDiv() { return _div; }

    /**
     * \brief Sets whether to color notes by channels or tracks.
     * \param byChannels True to color by channels, false by tracks
     */
    void setColorsByChannels(bool byChannels);

    /**
     * \brief Gets the current coloring mode.
     * \return True if coloring by channels, false if by tracks
     */
    bool colorsByChannels() { return _colorsByChannels; }

    /**
     * \brief Sets coloring by channels (legacy method for compatibility).
     */
    void setColorsByChannel();

    /**
     * \brief Sets coloring by tracks (legacy method for compatibility).
     */
    void setColorsByTracks() { setColorsByChannels(false); }

    /**
     * \brief Gets the current coloring mode (legacy method for compatibility).
     * \return True if coloring by channels
     */
    bool colorsByChannel() { return _colorsByChannels; }

    /**
     * \brief Sets piano emulation mode.
     * \param enabled True to enable piano emulation
     */
    void setPianoEmulation(bool enabled);

    /**
     * \brief Gets the current piano emulation mode.
     * \return True if piano emulation is enabled
     */
    bool getPianoEmulation() { return _isPianoEmulationEnabled; }

    /**
     * \brief Gets the current time division (legacy method for compatibility).
     * \return Current time division value
     */
    int div() { return _div; }

    /**
     * \brief Gets the current division lines for grid display.
     * \return List of division line positions and tick values
     */
    QList<QPair<int, int> > divs() { return currentDivs; }



    /**
     * \brief Handles key press events for tools and piano emulation.
     * \param event The key event
     */
    void takeKeyPressEvent(QKeyEvent *event);

    /**
     * \brief Handles key release events for tools.
     * \param event The key event
     */
    void takeKeyReleaseEvent(QKeyEvent *event);

    /**
     * \brief Sets the screen lock state.
     * \param locked True to lock screen position during playback
     */
    void setScreenLocked(bool locked);

    /**
     * \brief Gets the screen lock state.
     * \return True if screen is locked
     */
    bool screenLocked();

    /**
     * \brief Enables or disables the widget.
     * \param enabled True to enable, false to disable
     */
    void setEnabled(bool enabled) override;

    /**
     * \brief Gets the enabled state.
     * \return True if widget is enabled
     */
    bool isEnabled() { return enabled; }

protected:
    // === QRhiWidget Implementation ===

    /**
     * \brief Initializes the RHI rendering context.
     * \param cb The command buffer for the current frame
     */
    void initialize(QRhiCommandBuffer *cb) override;

    /**
     * \brief Renders a frame using RHI.
     * \param cb The command buffer for the current frame
     */
    void render(QRhiCommandBuffer *cb) override;

    /**
     * \brief Releases RHI resources when context is lost.
     */
    void releaseResources();

    // === Event Handling ===

    /**
     * \brief Handles mouse press events.
     * \param event The mouse event
     */
    void mousePressEvent(QMouseEvent *event) override;

    /**
     * \brief Handles mouse release events.
     * \param event The mouse event
     */
    void mouseReleaseEvent(QMouseEvent *event) override;

    /**
     * \brief Handles mouse move events.
     * \param event The mouse event
     */
    void mouseMoveEvent(QMouseEvent *event) override;

    /**
     * \brief Handles mouse double click events.
     * \param event The mouse event
     */
    void mouseDoubleClickEvent(QMouseEvent *event) override;

    /**
     * \brief Handles wheel events for zooming and scrolling.
     * \param event The wheel event
     */
    void wheelEvent(QWheelEvent *event) override;

    /**
     * \brief Handles key press events.
     * \param event The key event
     */
    void keyPressEvent(QKeyEvent *event) override;

    /**
     * \brief Handles key release events.
     * \param event The key event
     */
    void keyReleaseEvent(QKeyEvent *event) override;

    /**
     * \brief Handles resize events.
     * \param event The resize event
     */
    void resizeEvent(QResizeEvent *event) override;

    /**
     * \brief Handles show events.
     * \param event The show event
     */
    void showEvent(QShowEvent *event) override;

    /**
     * \brief Handles mouse enter events.
     * \param event The enter event
     */
    void enterEvent(QEnterEvent *event) override;

    /**
     * \brief Handles mouse leave events.
     * \param event The leave event
     */
    void leaveEvent(QEvent *event) override;

    /**
     * \brief Handles paint events (fallback when RHI is not available).
     * \param event The paint event
     */
    void paintEvent(QPaintEvent *event) override;

signals:
    /**
     * \brief Emitted when the widget size changes.
     * \param maxScrollTime Maximum scroll time
     * \param maxScrollLine Maximum scroll line
     * \param vX Horizontal scroll position
     * \param vY Vertical scroll position
     */
    void sizeChanged(int maxScrollTime, int maxScrollLine, int vX, int vY);

    /**
     * \brief Emitted when scroll positions change.
     * \param maxScrollTime Maximum scroll time
     * \param maxScrollLine Maximum scroll line
     * \param vX Horizontal scroll position
     * \param vY Vertical scroll position
     */
    void scrollChanged(int maxScrollTime, int maxScrollLine, int vX, int vY);

    /**
     * \brief Emitted when the object list changes.
     */
    void objectListChanged();

public slots:
    /**
     * \brief Updates the current playback time position.
     * \param ms Time in milliseconds
     * \param ignoreLocked If true, updates even when screen is locked
     */
    void timeMsChanged(int ms, bool ignoreLocked = false) override;

    /**
     * \brief Registers that a layout recalculation is needed.
     */
    void registerRelayout() override;

    /**
     * \brief Recalculates widget sizes and layout.
     */
    void calcSizes();

    /**
     * \brief Updates cached rendering settings from QSettings.
     * Call this when rendering settings have changed to refresh the cache.
     */
    void updateRenderingSettings();

    /**
     * \brief Resets the view to show the entire file.
     */
    void resetView();

    // === Scroll Control ===

    /**
     * \brief Handles horizontal scroll position changes.
     * \param scrollPositionX New horizontal scroll position
     */
    void scrollXChanged(int scrollPositionX);

    /**
     * \brief Handles vertical scroll position changes.
     * \param scrollPositionY New vertical scroll position
     */
    void scrollYChanged(int scrollPositionY);

    // === Zoom Control ===

    /**
     * \brief Zooms in horizontally (time axis).
     */
    void zoomHorIn();

    /**
     * \brief Zooms out horizontally (time axis).
     */
    void zoomHorOut();

    /**
     * \brief Zooms in vertically (pitch axis).
     */
    void zoomVerIn();

    /**
     * \brief Zooms out vertically (pitch axis).
     */
    void zoomVerOut();

    /**
     * \brief Resets zoom to standard/default levels.
     */
    void zoomStd();

    // === IMatrixWidget Interface ===

    /**
     * \brief Gets the widget width.
     * \return Widget width in pixels
     */
    int width() const override { return QRhiWidget::width(); }

    /**
     * \brief Gets the widget height.
     * \return Widget height in pixels
     */
    int height() const override { return QRhiWidget::height(); }

    /**
     * \brief Updates the widget display.
     */
    void update() override { QRhiWidget::update(); }





private:
    // === RHI Implementation Methods ===
    void initializeBuffers();
    void initializeShaders();
    QShader loadShaderFromResource(const QString &path);
    void createBackgroundPipeline(const QShader &vertShader, const QShader &fragShader);
    void createMidiEventPipeline(const QShader &vertShader, const QShader &fragShader);
    void createPianoPipeline(const QShader &vertShader, const QShader &fragShader);
    void createLinePipeline(const QShader &vertShader, const QShader &fragShader);
    void createPianoComplexPipeline(const QShader &vertShader, const QShader &fragShader);
    void createTextPipeline(const QShader &vertShader, const QShader &fragShader);
    void createCirclePipeline(const QShader &vertShader, const QShader &fragShader);
    void createTrianglePipeline(const QShader &vertShader, const QShader &fragShader);

    // === Graphics API Fallback Methods ===
    bool initializeGraphicsAPI();
    bool tryInitializeAPI(QRhiWidget::Api api);
    QString getApiName(QRhiWidget::Api api);

    void renderBackground(QRhiCommandBuffer *cb, QRhiRenderTarget *rt);
    void renderPianoAreaBackground(QRhiCommandBuffer *cb, QRhiRenderTarget *rt);
    void renderTimelineAreaBackground(QRhiCommandBuffer *cb, QRhiRenderTarget *rt);
    void renderRowStrips(QRhiCommandBuffer *cb, QRhiRenderTarget *rt);
    void renderMidiEvents(QRhiCommandBuffer *cb, QRhiRenderTarget *rt);
    void renderPianoKeys(QRhiCommandBuffer *cb, QRhiRenderTarget *rt);
    void renderLines(QRhiCommandBuffer *cb, QRhiRenderTarget *rt);
    void renderTools(QRhiCommandBuffer *cb, QRhiRenderTarget *rt);
    void renderCursors(QRhiCommandBuffer *cb, QRhiRenderTarget *rt);
    void renderBorderLines(QRhiCommandBuffer *cb, QRhiRenderTarget *rt);
    void renderRecordingIndicator(QRhiCommandBuffer *cb, QRhiRenderTarget *rt);
    void renderText(QRhiCommandBuffer *cb, QRhiRenderTarget *rt);

    void updateUniformBuffer();
    void updateMidiEventInstances();
    void updatePianoKeyInstances();
    void updateRowStripInstances();
    void updateLineVertices();
    void updateMeasureLineVertices();
    void updateTextInstances();
    void updatePianoComplexInstances();
    void renderComplexPianoKeys(QRhiCommandBuffer *cb, QRhiRenderTarget *rt);
    void renderCursorTriangle(QRhiCommandBuffer *cb, int x, int y, const QColor &color);
    void renderSelectionLines(const QList<QPair<int, int>> &selectionLines);
    void renderErrorBackground();
    void createFontAtlas();

    QColor getEventColor(MidiEvent *event);
    void cleanupRhiResources();

    // === RHI Resources ===
    struct RhiResources {
        QRhi *rhi = nullptr;

        // Pipelines and bindings
        QRhiShaderResourceBindings *backgroundSrb = nullptr;
        QRhiGraphicsPipeline *backgroundPipeline = nullptr;
        QRhiShaderResourceBindings *midiEventSrb = nullptr;
        QRhiGraphicsPipeline *midiEventPipeline = nullptr;
        QRhiShaderResourceBindings *pianoSrb = nullptr;
        QRhiGraphicsPipeline *pianoPipeline = nullptr;
        QRhiShaderResourceBindings *lineSrb = nullptr;
        QRhiGraphicsPipeline *linePipeline = nullptr;
        QRhiShaderResourceBindings *textSrb = nullptr;
        QRhiGraphicsPipeline *textPipeline = nullptr;
        QRhiShaderResourceBindings *pianoComplexSrb = nullptr;
        QRhiGraphicsPipeline *pianoComplexPipeline = nullptr;
        QRhiShaderResourceBindings *circleSrb = nullptr;
        QRhiGraphicsPipeline *circlePipeline = nullptr;
        QRhiShaderResourceBindings *triangleSrb = nullptr;
        QRhiGraphicsPipeline *trianglePipeline = nullptr;

        // Buffers
        QRhiBuffer *uniformBuffer = nullptr;
        QRhiBuffer *quadVertexBuffer = nullptr;
        QRhiBuffer *backgroundInstanceBuffer = nullptr;
        QRhiBuffer *midiEventInstanceBuffer = nullptr;
        QRhiBuffer *pianoInstanceBuffer = nullptr;
        QRhiBuffer *rowStripInstanceBuffer = nullptr;
        QRhiBuffer *lineVertexBuffer = nullptr;
        QRhiBuffer *measureLineVertexBuffer = nullptr;
        QRhiBuffer *textVertexBuffer = nullptr;
        QRhiBuffer *textInstanceBuffer = nullptr;
        QRhiBuffer *pianoComplexInstanceBuffer = nullptr;
        QRhiBuffer *circleInstanceBuffer = nullptr;
        QRhiBuffer *triangleInstanceBuffer = nullptr;

        // Font atlas system
        QRhiTexture *fontAtlasTexture = nullptr;
        QRhiSampler *fontAtlasSampler = nullptr;
        QRhiBuffer *lineIndexBuffer = nullptr;

        // Textures and samplers
        QRhiTexture *fontAtlas = nullptr;
        QRhiSampler *fontSampler = nullptr;

        // Resource update batches
        QRhiResourceUpdateBatch *resourceUpdates = nullptr;

        // Current instance counts
        int midiEventInstanceCount = 0;
        int pianoInstanceCount = 0;
        int rowStripInstanceCount = 0;
        int lineVertexCount = 0;
        int measureLineVertexCount = 0;
        int textInstanceCount = 0;
        int pianoComplexInstanceCount = 0;
        int circleInstanceCount = 0;
        int triangleInstanceCount = 0;

        // Initialization state
        bool initialized = false;
        bool shadersLoaded = false;
        bool buffersCreated = false;
    } m_rhi;

    // === Uniform Buffer Data ===
    struct UniformData {
        float mvpMatrix[16];
        float screenSize[2];
        float time;
        float padding;

        // Background and basic colors
        float backgroundColor[4];
        float foregroundColor[4];
        float borderColor[4];

        // Strip and range colors
        float stripHighlightColor[4];
        float stripNormalColor[4];
        float rangeLineColor[4];

        // Program event colors
        float programEventHighlightColor[4];
        float programEventNormalColor[4];

        // System and UI colors
        float systemWindowColor[4];
        float darkGrayColor[4];
        float grayColor[4];
        float errorColor[4];

        // Timeline and measure colors
        float measureBarColor[4];
        float measureLineColor[4];
        float measureTextColor[4];
        float timelineGridColor[4];

        // Playback and recording colors
        float playbackCursorColor[4];
        float cursorTriangleColor[4];
        float recordingIndicatorColor[4];

        // Piano key colors
        float pianoBlackKeyColor[4];
        float pianoBlackKeyHoverColor[4];
        float pianoBlackKeySelectedColor[4];
        float pianoWhiteKeyColor[4];
        float pianoWhiteKeyHoverColor[4];
        float pianoWhiteKeySelectedColor[4];
        float pianoKeyLineHighlightColor[4];

        // Flags and settings
        float showRangeLines;
        float stripStyle;
        float shouldUseDarkMode;
        float padding2;
    };

    // === Instance Data Structures ===
    struct MidiEventInstance {
        float posSize[4];  // x, y, width, height
        float color[4];    // r, g, b, a
    };

    struct PianoKeyInstance {
        float posSize[4];  // x, y, width, height
        float color[4];    // r, g, b, keyType (packed in alpha)
    };

    struct LineVertex {
        float position[2]; // x, y
        float color[4];    // r, g, b, a
    };

    // === Helper Methods ===
    void initializeFontAtlas();
    void loadShader(const QString &name, QRhiShaderStage::Type type, QByteArray &shaderData);
    void updateCachedAppearanceColors();
    bool mouseInRect(const QRectF &rect);
    bool mouseInRect(int x, int y, int width, int height);
    void pianoEmulator(QKeyEvent *event);
    void calculateDivs();
    bool isBlackPianoKey(int midiNote);
    void renderMidiEventsForChannel(int channel);
    QColor getChannelColor(int channel);
    void renderGridLines(QRhiCommandBuffer *cb);
    void renderMeasureLines(QRhiCommandBuffer *cb);

    // === Piano Key Structure ===
    struct PianoKeyInfo {
        int midiNote;
        int x, y, width, height;
        QColor color;
        bool isBlackKey;
    };

    // === MIDI Event Structure ===
    struct MidiEventInfo {
        MidiEvent *event;
        int x, y, width, height;
        QColor color;
        int channel;
    };

    // === Data Members (matching MatrixWidget) ===
    QSettings *_settings;
    MidiFile *file;
    bool enabled;
    bool screen_locked;
    
    // View state (exact copy from MatrixWidget)
    int startTick, endTick;
    int startTimeX, endTimeX, startLineY, endLineY;
    int lineNameWidth;
    int timeHeight;
    int msOfFirstEventInList;
    double scaleX, scaleY;
    
    // Configuration
    bool _colorsByChannels;
    bool _isPianoEmulationEnabled;
    int _div;
    
    // Event lists
    QList<MidiEvent *> *objects;
    QList<MidiEvent *> *velocityObjects;
    QList<MidiEvent *> *currentTempoEvents;
    QList<class TimeSignatureEvent *> *currentTimeSignatureEvents;

    // Current time divisions for grid line display
    QList<QPair<int, int> > currentDivs;
    
    // Piano emulation
    NoteOnEvent *pianoEvent;
    QMap<int, QRect> pianoKeys;

    // RHI piano key storage
    QMap<int, PianoKeyInfo> m_pianoKeys;

    // RHI MIDI event storage
    QList<MidiEventInfo> m_midiEvents;
    
    // Display areas
    QRectF ToolArea;
    QRectF PianoArea;
    QRectF TimeLineArea;
    
    // Mouse state
    int mouseX, mouseY;
    bool mouseReleased;

    // Configuration flags (EXACT COPY from MatrixWidget)

    // === Cached Appearance Colors (complete copy from MatrixWidget) ===

    /** \brief Cached appearance colors to avoid expensive theme checks during paint */
    bool _cachedShowRangeLines;
    Appearance::stripStyle _cachedStripStyle;
    QColor _cachedBackgroundColor;
    QColor _cachedForegroundColor;
    QColor _cachedBorderColor;
    QColor _cachedRangeLineColor;
    QColor _cachedStripHighlightColor;
    QColor _cachedStripNormalColor;
    QColor _cachedProgramEventHighlightColor;
    QColor _cachedProgramEventNormalColor;
    QColor _cachedSystemWindowColor;
    QColor _cachedMeasureBarColor;
    QColor _cachedMeasureLineColor;
    QColor _cachedMeasureTextColor;
    QColor _cachedTimelineGridColor;
    QColor _cachedDarkGrayColor;
    QColor _cachedGrayColor;
    QColor _cachedErrorColor;
    QColor _cachedPlaybackCursorColor;
    QColor _cachedPianoWhiteKeyColor;
    QColor _cachedPianoBlackKeyColor;
    QColor _cachedPianoWhiteKeyHoverColor;
    QColor _cachedPianoBlackKeyHoverColor;
    QColor _cachedPianoWhiteKeySelectedColor;
    QColor _cachedPianoBlackKeySelectedColor;
    QColor _cachedPianoKeyLineHighlightColor;
    QColor _cachedCursorTriangleColor;
    QColor _cachedRecordingIndicatorColor;

    // Cached theme state to avoid expensive shouldUseDarkMode() calls
    bool _cachedShouldUseDarkMode;
    
    // Performance tracking
    QElapsedTimer _frameTimer;
    int _frameCount;
    double _averageFrameTime;

    // Scroll repaint suppression (exact copy from MatrixWidget)
    bool _suppressScrollRepaints;

    // Rendering settings cache (exact copy from MatrixWidget lines 81-82, 88)
    bool _antialiasing;
    bool _smoothPixmapTransform;
    bool _usingHardwareAcceleration;

    // Font atlas system
    struct FontCharacter {
        float u1, v1, u2, v2; // UV coordinates in atlas
        int width, height;     // Character dimensions
    };
    QMap<QChar, FontCharacter> fontCharacterMap;
};

#endif // RHIMATRIXWIDGET_H
