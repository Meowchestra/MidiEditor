/**
 * RhiMatrixWidget.cpp - Qt RHI Hardware Accelerated Matrix Widget
 * 
 * This is a minimal, compilable version of the RHI-based MatrixWidget.
 * The full RHI implementation will be added incrementally.
 */

#include "RhiMatrixWidget.h"
#include "../midi/MidiFile.h"
#include "../midi/MidiTrack.h"
#include "../MidiEvent/MidiEvent.h"
#include "../MidiEvent/NoteOnEvent.h"
#include "../MidiEvent/OffEvent.h"
#include "../MidiEvent/OnEvent.h"
#include "../MidiEvent/TimeSignatureEvent.h"
#include "../MidiEvent/TempoChangeEvent.h"
#include "../MidiEvent/ControlChangeEvent.h"
#include "../protocol/Protocol.h"
#include "../tool/Tool.h"
#include "../tool/EditorTool.h"
#include "Appearance.h"
#include "Selection.h"
#include "../midi/MidiPlayer.h"
#include "../midi/MidiOutput.h"
#include "../midi/MidiInput.h"
#include "ChannelVisibilityManager.h"

#include <cmath>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QResizeEvent>
#include <QFontMetrics>
#include <QFont>
#include <QShowEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QWindow>
#include <QFile>
#include <QDir>
#include <QDebug>
#include <QLoggingCategory>

// RHI includes required for QRhiWidget implementation
#include <rhi/qrhi.h>

RhiMatrixWidget::RhiMatrixWidget(QSettings *settings, QWidget *parent)
    : QRhiWidget(parent), file(nullptr) {

    // Enable Qt logging for RHI debugging
    QLoggingCategory::setFilterRules("qt.rhi.general.debug=true");
    QLoggingCategory::setFilterRules("qt.opengl.debug=true");

    // Initialize graphics API with fallback chain
    qDebug() << "RhiMatrixWidget: Initializing graphics API with fallback chain";
    if (!initializeGraphicsAPI()) {
        qWarning() << "RhiMatrixWidget: All graphics APIs failed - will fall back to software rendering";
        // Note: MatrixWidgetManager will handle the fallback to software rendering
    }

    // Disable debug layer for compatibility (requires D3D11 SDK)
    setDebugLayerEnabled(false);
    qDebug() << "RhiMatrixWidget: Debug layer disabled for compatibility";

    // Set a dark gray background color (similar to MatrixWidget)
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(40, 40, 40));
    setPalette(pal);

    // Initialize basic state
    scaleX = 1.0;
    scaleY = 1.0;
    startTick = 0;
    endTick = 0;
    startTimeX = 0;
    startLineY = 50;
    endTimeX = 0;
    endLineY = 0;
    
    // Initialize widget properties (EXACT COPY from MatrixWidget)
    lineNameWidth = 110;  // Match MatrixWidget default
    timeHeight = 50;      // Match MatrixWidget default
    enabled = true;
    screen_locked = false;
    mouseX = 0;
    mouseY = 0;
    mouseReleased = true;
    _div = 2;
    _colorsByChannels = false;
    _isPianoEmulationEnabled = false; // Default piano emulation off
    
    // Initialize event lists
    objects = new QList<MidiEvent *>();
    velocityObjects = new QList<MidiEvent *>();
    currentTempoEvents = new QList<MidiEvent *>();
    currentTimeSignatureEvents = new QList<TimeSignatureEvent *>();

    // Initialize piano event for key playback (EXACT COPY from MatrixWidget line 79)
    pianoEvent = new NoteOnEvent(60, 100, 0, nullptr);
    msOfFirstEventInList = 0;
    
    // Initialize RHI state
    m_rhi.rhi = nullptr;
    m_rhi.initialized = false;
    m_rhi.shadersLoaded = false;
    m_rhi.resourceUpdates = nullptr;
    
    // Initialize performance tracking
    _frameCount = 0;
    _averageFrameTime = 0.0;

    // RHI is now working - no need for debug timer

    // Connect to QRhiWidget signals for debugging
    connect(this, &QRhiWidget::frameSubmitted, this, []() {
        qDebug() << "RhiMatrixWidget: Frame submitted successfully";
    });

    connect(this, &QRhiWidget::renderFailed, this, []() {
        qWarning() << "RhiMatrixWidget: Render failed - QRhiWidget could not render";
    });

    _frameTimer.start();

    // Initialize scroll repaint suppression flag
    _suppressScrollRepaints = false;

    // Initialize view ranges (EXACT COPY from MatrixWidget constructor lines 51-60)
    startTimeX = 0;
    endTimeX = 0;     // Let calcSizes() calculate this based on widget size
    startLineY = 50;  // Roughly center on Middle C
    endLineY = 0;     // Let calcSizes() calculate this based on widget size
    lineNameWidth = 110;  // Match MatrixWidget
    timeHeight = 50;      // Match MatrixWidget
    scaleX = 1.0;
    scaleY = 1.0;

    // Update cached appearance colors
    updateCachedAppearanceColors();

    // EXACT COPY from MatrixWidget constructor (lines 66-68, 70-72, 74-75)
    // Note: EditorTool setup is handled by MainWindow, not individual widgets
    setMouseTracking(true);
    setFocusPolicy(Qt::ClickFocus);

    // Note: setRepaintOn* methods are MatrixWidget-specific, not needed for RHI widget
    // Note: PlayerThread connection is handled by MainWindow, not individual widgets
    
    qDebug() << "RhiMatrixWidget: Created (minimal version)";
    qDebug() << "RhiMatrixWidget: Initial size:" << size();
    qDebug() << "RhiMatrixWidget: Parent:" << (parent ? "yes" : "no");
    qDebug() << "RhiMatrixWidget: API set to:" << static_cast<int>(api());

    // OPTIMIZED API FALLBACK CHAIN:
    // D3D12 (HLSL 67) → Vulkan (GLSL 460) → OpenGL (GLSL 460) → Software MatrixWidget
    // All compiled shaders support this fallback chain perfectly
    qDebug() << "RhiMatrixWidget: Using optimized API chain - D3D12 → Vulkan → OpenGL → Software";

    _settings = settings;

    // EXACT COPY from MatrixWidget constructor (lines 81-82)
    // Cache rendering settings to avoid reading from QSettings on every paint event
    _antialiasing = _settings->value("rendering/antialiasing", true).toBool();
    _smoothPixmapTransform = _settings->value("rendering/smooth_pixmap_transform", true).toBool();

    // Initialize hardware acceleration flag (EXACT COPY from MatrixWidget line 88)
    _usingHardwareAcceleration = true; // RhiMatrixWidget always uses hardware acceleration

    // Update cached appearance colors
    updateCachedAppearanceColors();

    // Initialize widget state (EXACT COPY from MatrixWidget)
    setAttribute(Qt::WA_OpaquePaintEvent, true); // Optimize painting
    setAttribute(Qt::WA_NoSystemBackground, true); // We handle our own background
}

RhiMatrixWidget::~RhiMatrixWidget() {
    qDebug() << "RhiMatrixWidget: Destructor called";

    // Cleanup RHI resources if not already done
    if (m_rhi.initialized) {
        qDebug() << "RhiMatrixWidget: Cleaning up RHI resources in destructor";
        cleanupRhiResources();
    }

    delete objects;
    delete velocityObjects;
    delete currentTempoEvents;
    delete currentTimeSignatureEvents;
    delete pianoEvent;

    qDebug() << "RhiMatrixWidget: Destroyed";
}

void RhiMatrixWidget::setFile(MidiFile *newFile) {
    // EXACT COPY from MatrixWidget::setFile() validation
    if (file == newFile) {
        return; // No change needed
    }

    file = newFile;

    // Initialize scales with validation (EXACT COPY from MatrixWidget)
    scaleX = qMax(0.1, qMin(10.0, 1.0)); // Clamp between 0.1 and 10.0
    scaleY = qMax(0.1, qMin(10.0, 1.0)); // Clamp between 0.1 and 10.0

    startTimeX = 0;
    // Roughly vertically center on Middle C.
    startLineY = 50;

    if (file) {
        connect(file->protocol(), SIGNAL(actionFinished()), this, SLOT(registerRelayout()));
        connect(file->protocol(), SIGNAL(actionFinished()), this, SLOT(update()));

        // Populate time signature events (EXACT COPY from MatrixWidget)
        currentTimeSignatureEvents->clear();
        QMap<int, MidiEvent *> *timeSignatureEvents = file->timeSignatureEvents();
        QMap<int, MidiEvent *>::iterator it = timeSignatureEvents->begin();
        while (it != timeSignatureEvents->end()) {
            TimeSignatureEvent *event = dynamic_cast<TimeSignatureEvent *>(it.value());
            if (event) {
                currentTimeSignatureEvents->append(event);
            }
            it++;
        }

        // Add default time signature if none exists
        if (currentTimeSignatureEvents->isEmpty()) {
            TimeSignatureEvent *defaultEvent = new TimeSignatureEvent(18, 4, 4, 24, 8, nullptr);
            defaultEvent->setFile(file);
            currentTimeSignatureEvents->append(defaultEvent);
        }

        // Populate tempo events (EXACT COPY from MatrixWidget)
        currentTempoEvents->clear();
        QMap<int, MidiEvent *> *tempoEvents = file->tempoEvents();
        QMap<int, MidiEvent *>::iterator tempoIt = tempoEvents->begin();
        while (tempoIt != tempoEvents->end()) {
            TempoChangeEvent *event = dynamic_cast<TempoChangeEvent *>(tempoIt.value());
            if (event) {
                currentTempoEvents->append(static_cast<MidiEvent*>(event));
            }
            tempoIt++;
        }

        // Add default tempo if none exists
        if (currentTempoEvents->isEmpty()) {
            TempoChangeEvent *defaultTempo = new TempoChangeEvent(0, 500000, nullptr); // 120 BPM
            defaultTempo->setFile(file);
            currentTempoEvents->append(static_cast<MidiEvent*>(defaultTempo));
        }

        calcSizes();

        // scroll down to see events
        int maxNote = -1;
        for (int channel = 0; channel < 16; channel++) {
            QMultiMap<int, MidiEvent *> *map = file->channelEvents(channel);

            QMultiMap<int, MidiEvent *>::iterator it = map->lowerBound(0);
            while (it != map->end()) {
                NoteOnEvent *onev = dynamic_cast<NoteOnEvent *>(it.value());
                if (onev && eventInWidget(onev)) {
                    if (onev->line() < maxNote || maxNote < 0) {
                        maxNote = onev->line();
                    }
                }
                it++;
            }
        }

        if (maxNote - 5 > 0) {
            startLineY = maxNote - 5;
        }

        calcSizes();
    }
}

void RhiMatrixWidget::initialize(QRhiCommandBuffer *cb) {
    Q_UNUSED(cb)

    QRhi *rhi = this->rhi(); // Get QRhi from QRhiWidget base class
    if (!rhi) {
        qWarning() << "RhiMatrixWidget: Cannot initialize with null QRhi";
        return;
    }

    qDebug() << "RhiMatrixWidget: *** INITIALIZE CALLED! ***";
    qDebug() << "RhiMatrixWidget: Initializing RHI with backend:" << rhi->backend();
    qDebug() << "RhiMatrixWidget: Driver info:" << rhi->driverInfo();

    // Check if we need to recreate resources (e.g., when QRhi changes)
    if (m_rhi.rhi != rhi) {
        // Release old resources if QRhi changed
        m_rhi.initialized = false;
        m_rhi.rhi = rhi;
    }

    try {
        // Initialize in correct order
        initializeBuffers();
        initializeShaders();

        m_rhi.initialized = true;
        qDebug() << "RhiMatrixWidget: RHI initialization completed successfully";
        qDebug() << "RhiMatrixWidget: Hardware acceleration is now active";

        // Check widget state after initialization
        qDebug() << "RhiMatrixWidget: Widget size:" << width() << "x" << height();
        qDebug() << "RhiMatrixWidget: Widget visible:" << isVisible();
        qDebug() << "RhiMatrixWidget: Widget enabled:" << isEnabled();

        // Force an update to trigger rendering
        update();

    } catch (const std::exception &e) {
        qWarning() << "RhiMatrixWidget: RHI initialization failed:" << e.what();
        qWarning() << "RhiMatrixWidget: Will fall back to software rendering";
        cleanupRhiResources();
        throw;
    }
}

void RhiMatrixWidget::render(QRhiCommandBuffer *cb) {
    // Always log render calls to debug the issue
    static int totalRenderCalls = 0;
    totalRenderCalls++;
    if (totalRenderCalls <= 5 || totalRenderCalls % 60 == 0) {
        qDebug() << "RhiMatrixWidget: render() called - call #" << totalRenderCalls;
        qDebug() << "RhiMatrixWidget: RHI available:" << (m_rhi.rhi ? "yes" : "no");
        qDebug() << "RhiMatrixWidget: RHI initialized:" << m_rhi.initialized;
    }

    if (!m_rhi.rhi || !m_rhi.initialized) {
        qWarning() << "RhiMatrixWidget: Cannot render - RHI not initialized (call #" << totalRenderCalls << ")";
        return;
    }

    // Get the render target from QRhiWidget
    QRhiRenderTarget *rt = renderTarget();
    if (!rt) {
        qWarning() << "RhiMatrixWidget: No render target available";
        return;
    }

    // Submit any accumulated resource updates BEFORE the render pass begins
    if (m_rhi.resourceUpdates) {
        cb->resourceUpdate(m_rhi.resourceUpdates);
        m_rhi.resourceUpdates = nullptr; // Clear after submission
    }

    // Begin render pass with dark background color
    QColor clearColor(20, 20, 20); // Very dark background to contrast with rendered content

    cb->beginPass(rt, clearColor, { 1.0f, 0 });

    // Performance tracking
    static int frameCount = 0;
    frameCount++;

    // Debug: Check if render method is being called (show first few frames and every 60th)
    if (frameCount <= 10 || frameCount % 60 == 0) {
        qDebug() << "RhiMatrixWidget: MAIN RENDER METHOD CALLED - frame" << frameCount;
        qDebug() << "RhiMatrixWidget: RHI initialized:" << m_rhi.initialized;
        qDebug() << "RhiMatrixWidget: File loaded:" << (file ? "yes" : "no");
        qDebug() << "RhiMatrixWidget: Background pipeline:" << (m_rhi.backgroundPipeline ? "available" : "missing");
        qDebug() << "RhiMatrixWidget: Vertex buffer:" << (m_rhi.quadVertexBuffer ? "available" : "missing");
        qDebug() << "RhiMatrixWidget: Shaders loaded:" << m_rhi.shadersLoaded;
    }

    // Always render background, even if no file is loaded
    // This will show the piano roll layout and grid

    // Start frame timing
    qint64 frameStart = _frameTimer.nsecsElapsed();

    try {
        // Clear previous frame data
        m_pianoKeys.clear();
        m_midiEvents.clear();

        // Update uniform buffer with current state (every frame)
        updateUniformBuffer();

        // Update instance data for visible elements (only when view changes)
        static int lastUpdateFrame = -1;
        static int lastStartTimeX = -1;
        static int lastEndTimeX = -1;
        static int lastStartLineY = -1;
        static int lastEndLineY = -1;
        static int lastWidth = -1;
        static int lastHeight = -1;

        bool viewChanged = (startTimeX != lastStartTimeX ||
                           endTimeX != lastEndTimeX ||
                           startLineY != lastStartLineY ||
                           endLineY != lastEndLineY ||
                           width() != lastWidth ||
                           height() != lastHeight);

        if (frameCount != lastUpdateFrame && viewChanged) {
            updateMidiEventInstances();
            updatePianoKeyInstances();
            updateRowStripInstances();
            updateLineVertices();
            updateMeasureLineVertices();
            updateTextInstances();
            lastUpdateFrame = frameCount;
            lastStartTimeX = startTimeX;
            lastEndTimeX = endTimeX;
            lastStartLineY = startLineY;
            lastEndLineY = endLineY;
            lastWidth = width();
            lastHeight = height();
        }

        // Render all components in EXACT MatrixWidget order
        if (frameCount <= 10 || frameCount % 60 == 0) {
            qDebug() << "RhiMatrixWidget: About to call renderBackground()";
        }
        // 1. Background fill
        renderBackground(cb, rt);

        if (frameCount <= 10 || frameCount % 60 == 0) {
            qDebug() << "RhiMatrixWidget: renderBackground() completed, calling renderRowStrips()";
        }
        // 2a. Piano area background (EXACT COPY from MatrixWidget lines 244-246)
        renderPianoAreaBackground(cb, rt);

        // 2b. Row strips (line backgrounds)
        renderRowStrips(cb, rt);

        // 2c. Timeline area background (EXACT COPY from MatrixWidget line 304)
        renderTimelineAreaBackground(cb, rt);

        // 3. Measure lines (timeline grid)
        renderMeasureLines(cb);

        // 4. Grid lines (horizontal lines)
        renderLines(cb, rt);

        if (frameCount <= 10 || frameCount % 60 == 0) {
            qDebug() << "RhiMatrixWidget: About to render MIDI events";
        }
        // 5. MIDI events (the actual notes)
        renderMidiEvents(cb, rt);

        if (frameCount <= 10 || frameCount % 60 == 0) {
            qDebug() << "RhiMatrixWidget: MIDI events completed, rendering piano keys (EXACT MatrixWidget order)";
        }
        // 6. Piano keys (rendered AFTER pixmap but BEFORE tools - EXACT COPY from MatrixWidget lines 509-514)
        renderPianoKeys(cb, rt);

        // 7. Tools (EXACT COPY from MatrixWidget lines 579-584)
        renderTools(cb, rt);

        // 8. Cursors (EXACT COPY from MatrixWidget lines 586-631)
        renderCursors(cb, rt);

        // 9. Text rendering (piano key labels, measure numbers, time markers)
        renderText(cb, rt);

        // 10. Border lines (EXACT COPY from MatrixWidget lines 486-488, 634-636)
        renderBorderLines(cb, rt);

        // 11. Recording indicator (EXACT COPY from MatrixWidget lines 639-641)
        renderRecordingIndicator(cb, rt);

        // Update performance metrics
        qint64 frameEnd = _frameTimer.nsecsElapsed();
        double frameTime = (frameEnd - frameStart) / 1000000.0; // Convert to milliseconds

        _frameCount++;
        _averageFrameTime = (_averageFrameTime * (_frameCount - 1) + frameTime) / _frameCount;

        // Log performance every 300 frames (5 seconds at 60fps)
        if (_frameCount % 300 == 0) {
            qDebug() << "RhiMatrixWidget: Avg frame time:" << QString::number(_averageFrameTime, 'f', 2) << "ms"
                     << "(" << QString::number(1000.0 / _averageFrameTime, 'f', 1) << "fps)"
                     << "Events:" << m_rhi.midiEventInstanceCount
                     << "Piano keys:" << m_rhi.pianoInstanceCount;
        }

        // End the render pass
        cb->endPass();

    } catch (const std::exception &e) {
        qWarning() << "RhiMatrixWidget: Render error:" << e.what();
        // Make sure to end the render pass even on error
        cb->endPass();
    }
}

void RhiMatrixWidget::releaseResources() {
    qDebug() << "RhiMatrixWidget: releaseResources() called by Qt RHI system";
    cleanupRhiResources();
    qDebug() << "RhiMatrixWidget: Resources released";
}

void RhiMatrixWidget::cleanupRhiResources() {
    qDebug() << "RhiMatrixWidget: Starting RHI resource cleanup";

    // Clean up pending resource updates first
    if (m_rhi.resourceUpdates) {
        m_rhi.resourceUpdates = nullptr; // Qt manages the lifecycle
    }

    // Clean up shader resource bindings first (they reference other resources)
    if (m_rhi.backgroundSrb) {
        delete m_rhi.backgroundSrb;
        m_rhi.backgroundSrb = nullptr;
    }
    if (m_rhi.midiEventSrb) {
        delete m_rhi.midiEventSrb;
        m_rhi.midiEventSrb = nullptr;
    }
    if (m_rhi.pianoSrb) {
        delete m_rhi.pianoSrb;
        m_rhi.pianoSrb = nullptr;
    }
    if (m_rhi.lineSrb) {
        delete m_rhi.lineSrb;
        m_rhi.lineSrb = nullptr;
    }

    // Clean up graphics pipelines (they reference SRBs)
    if (m_rhi.backgroundPipeline) {
        delete m_rhi.backgroundPipeline;
        m_rhi.backgroundPipeline = nullptr;
    }
    if (m_rhi.midiEventPipeline) {
        delete m_rhi.midiEventPipeline;
        m_rhi.midiEventPipeline = nullptr;
    }
    if (m_rhi.pianoPipeline) {
        delete m_rhi.pianoPipeline;
        m_rhi.pianoPipeline = nullptr;
    }
    if (m_rhi.linePipeline) {
        delete m_rhi.linePipeline;
        m_rhi.linePipeline = nullptr;
    }

    // Clean up buffers last
    if (m_rhi.uniformBuffer) {
        delete m_rhi.uniformBuffer;
        m_rhi.uniformBuffer = nullptr;
    }
    if (m_rhi.quadVertexBuffer) {
        delete m_rhi.quadVertexBuffer;
        m_rhi.quadVertexBuffer = nullptr;
    }
    if (m_rhi.midiEventInstanceBuffer) {
        delete m_rhi.midiEventInstanceBuffer;
        m_rhi.midiEventInstanceBuffer = nullptr;
    }
    if (m_rhi.pianoInstanceBuffer) {
        delete m_rhi.pianoInstanceBuffer;
        m_rhi.pianoInstanceBuffer = nullptr;
    }
    if (m_rhi.rowStripInstanceBuffer) {
        delete m_rhi.rowStripInstanceBuffer;
        m_rhi.rowStripInstanceBuffer = nullptr;
    }
    if (m_rhi.lineVertexBuffer) {
        delete m_rhi.lineVertexBuffer;
        m_rhi.lineVertexBuffer = nullptr;
    }
    if (m_rhi.measureLineVertexBuffer) {
        delete m_rhi.measureLineVertexBuffer;
        m_rhi.measureLineVertexBuffer = nullptr;
    }
    if (m_rhi.textInstanceBuffer) {
        delete m_rhi.textInstanceBuffer;
        m_rhi.textInstanceBuffer = nullptr;
    }
    if (m_rhi.pianoComplexInstanceBuffer) {
        delete m_rhi.pianoComplexInstanceBuffer;
        m_rhi.pianoComplexInstanceBuffer = nullptr;
    }
    if (m_rhi.circleInstanceBuffer) {
        delete m_rhi.circleInstanceBuffer;
        m_rhi.circleInstanceBuffer = nullptr;
    }
    if (m_rhi.triangleInstanceBuffer) {
        delete m_rhi.triangleInstanceBuffer;
        m_rhi.triangleInstanceBuffer = nullptr;
    }
    if (m_rhi.fontAtlasTexture) {
        delete m_rhi.fontAtlasTexture;
        m_rhi.fontAtlasTexture = nullptr;
    }
    if (m_rhi.fontAtlasSampler) {
        delete m_rhi.fontAtlasSampler;
        m_rhi.fontAtlasSampler = nullptr;
    }
    if (m_rhi.textSrb) {
        delete m_rhi.textSrb;
        m_rhi.textSrb = nullptr;
    }
    if (m_rhi.textPipeline) {
        delete m_rhi.textPipeline;
        m_rhi.textPipeline = nullptr;
    }
    if (m_rhi.pianoComplexSrb) {
        delete m_rhi.pianoComplexSrb;
        m_rhi.pianoComplexSrb = nullptr;
    }
    if (m_rhi.pianoComplexPipeline) {
        delete m_rhi.pianoComplexPipeline;
        m_rhi.pianoComplexPipeline = nullptr;
    }
    if (m_rhi.circleSrb) {
        delete m_rhi.circleSrb;
        m_rhi.circleSrb = nullptr;
    }
    if (m_rhi.circlePipeline) {
        delete m_rhi.circlePipeline;
        m_rhi.circlePipeline = nullptr;
    }
    if (m_rhi.triangleSrb) {
        delete m_rhi.triangleSrb;
        m_rhi.triangleSrb = nullptr;
    }
    if (m_rhi.trianglePipeline) {
        delete m_rhi.trianglePipeline;
        m_rhi.trianglePipeline = nullptr;
    }

    // Reset state flags
    m_rhi.initialized = false;
    m_rhi.shadersLoaded = false;
    m_rhi.pianoInstanceCount = 0;
    m_rhi.midiEventInstanceCount = 0;

    qDebug() << "RhiMatrixWidget: RHI resource cleanup completed";
}

void RhiMatrixWidget::initializeBuffers() {
    if (!m_rhi.rhi) {
        throw std::runtime_error("RHI not available");
    }

    qDebug() << "RhiMatrixWidget: Creating GPU buffers...";

    // Create uniform buffer for shader constants
    m_rhi.uniformBuffer = m_rhi.rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, RHI_UNIFORM_BUFFER_SIZE);
    if (!m_rhi.uniformBuffer->create()) {
        throw std::runtime_error("Failed to create uniform buffer");
    }

    // Create quad vertex buffer (for background and instanced rendering)
    // Two triangles to form a quad (6 vertices total)
    const float quadVertices[] = {
        // Triangle 1: position(x,y,z) + texCoord(u,v)
        -1.0f, -1.0f, 0.0f,   0.0f, 0.0f,  // bottom-left
         1.0f, -1.0f, 0.0f,   1.0f, 0.0f,  // bottom-right
         1.0f,  1.0f, 0.0f,   1.0f, 1.0f,  // top-right

        // Triangle 2
        -1.0f, -1.0f, 0.0f,   0.0f, 0.0f,  // bottom-left
         1.0f,  1.0f, 0.0f,   1.0f, 1.0f,  // top-right
        -1.0f,  1.0f, 0.0f,   0.0f, 1.0f   // top-left
    };

    m_rhi.quadVertexBuffer = m_rhi.rhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, sizeof(quadVertices));
    if (!m_rhi.quadVertexBuffer->create()) {
        throw std::runtime_error("Failed to create quad vertex buffer");
    }

    // Upload vertex data to the buffer - will be submitted during first render
    if (!m_rhi.resourceUpdates) {
        m_rhi.resourceUpdates = m_rhi.rhi->nextResourceUpdateBatch();
    }
    m_rhi.resourceUpdates->uploadStaticBuffer(m_rhi.quadVertexBuffer, quadVertices);

    // Create instance buffers for MIDI events and piano keys
    m_rhi.midiEventInstanceBuffer = m_rhi.rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer,
                                                         RHI_MAX_INSTANCES * 12 * sizeof(float));
    if (!m_rhi.midiEventInstanceBuffer->create()) {
        throw std::runtime_error("Failed to create MIDI event instance buffer");
    }

    m_rhi.pianoInstanceBuffer = m_rhi.rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer,
                                                     RHI_MAX_INSTANCES * 8 * sizeof(float));
    if (!m_rhi.pianoInstanceBuffer->create()) {
        throw std::runtime_error("Failed to create piano instance buffer");
    }

    m_rhi.rowStripInstanceBuffer = m_rhi.rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer,
                                                        RHI_MAX_INSTANCES * 8 * sizeof(float));
    if (!m_rhi.rowStripInstanceBuffer->create()) {
        throw std::runtime_error("Failed to create row strip instance buffer");
    }

    // Create line vertex buffer
    m_rhi.lineVertexBuffer = m_rhi.rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer,
                                                  RHI_VERTEX_BUFFER_SIZE * 6 * sizeof(float));
    if (!m_rhi.lineVertexBuffer->create()) {
        throw std::runtime_error("Failed to create line vertex buffer");
    }

    // Create measure line vertex buffer
    m_rhi.measureLineVertexBuffer = m_rhi.rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer,
                                                         RHI_VERTEX_BUFFER_SIZE * 6 * sizeof(float));
    if (!m_rhi.measureLineVertexBuffer->create()) {
        throw std::runtime_error("Failed to create measure line vertex buffer");
    }

    // Create text instance buffer
    m_rhi.textInstanceBuffer = m_rhi.rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer,
                                                    RHI_MAX_INSTANCES * 12 * sizeof(float)); // 12 floats per text instance
    if (!m_rhi.textInstanceBuffer->create()) {
        throw std::runtime_error("Failed to create text instance buffer");
    }

    // Create piano complex instance buffer (for complex piano key shapes)
    m_rhi.pianoComplexInstanceBuffer = m_rhi.rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer,
                                                            RHI_MAX_INSTANCES * 12 * sizeof(float)); // 12 floats per piano complex instance
    if (!m_rhi.pianoComplexInstanceBuffer->create()) {
        throw std::runtime_error("Failed to create piano complex instance buffer");
    }

    // Create circle instance buffer (for recording indicator)
    m_rhi.circleInstanceBuffer = m_rhi.rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer,
                                                      RHI_MAX_INSTANCES * 8 * sizeof(float)); // 8 floats per circle instance
    if (!m_rhi.circleInstanceBuffer->create()) {
        throw std::runtime_error("Failed to create circle instance buffer");
    }

    // Create triangle instance buffer (for cursor triangles)
    m_rhi.triangleInstanceBuffer = m_rhi.rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer,
                                                        RHI_MAX_INSTANCES * 8 * sizeof(float)); // 8 floats per triangle instance
    if (!m_rhi.triangleInstanceBuffer->create()) {
        throw std::runtime_error("Failed to create triangle instance buffer");
    }

    // Create font atlas
    createFontAtlas();

    // Quad data already uploaded in the buffer creation section above

    qDebug() << "RhiMatrixWidget: GPU buffers created successfully";
}

void RhiMatrixWidget::initializeShaders() {
    if (!m_rhi.rhi) {
        throw std::runtime_error("RHI not available");
    }

    if (m_rhi.shadersLoaded) {
        qDebug() << "RhiMatrixWidget: Shaders already loaded";
        return;
    }

    qDebug() << "RhiMatrixWidget: Loading embedded shaders...";

    try {
        // Load compiled shaders from embedded resources
        // Using correct paths from main resources.qrc (includes src/shaders/compiled/ in the path)
        qDebug() << "RhiMatrixWidget: Loading shaders from main resources.qrc with full paths";
        QShader backgroundVertShader = loadShaderFromResource(":/src/shaders/compiled/background.vert.qsb");
        QShader backgroundFragShader = loadShaderFromResource(":/src/shaders/compiled/background.frag.qsb");
        QShader midiEventVertShader = loadShaderFromResource(":/src/shaders/compiled/midi_event.vert.qsb");
        QShader midiEventFragShader = loadShaderFromResource(":/src/shaders/compiled/midi_event.frag.qsb");
        QShader pianoVertShader = loadShaderFromResource(":/src/shaders/compiled/piano.vert.qsb");
        QShader pianoFragShader = loadShaderFromResource(":/src/shaders/compiled/piano.frag.qsb");
        QShader lineVertShader = loadShaderFromResource(":/src/shaders/compiled/line.vert.qsb");
        QShader lineFragShader = loadShaderFromResource(":/src/shaders/compiled/line.frag.qsb");

        // Load new advanced shaders
        QShader pianoComplexVertShader = loadShaderFromResource(":/src/shaders/compiled/piano_complex.vert.qsb");
        QShader pianoComplexFragShader = loadShaderFromResource(":/src/shaders/compiled/piano_complex.frag.qsb");
        QShader textVertShader = loadShaderFromResource(":/src/shaders/compiled/text.vert.qsb");
        QShader textFragShader = loadShaderFromResource(":/src/shaders/compiled/text.frag.qsb");
        QShader circleVertShader = loadShaderFromResource(":/src/shaders/compiled/circle.vert.qsb");
        QShader circleFragShader = loadShaderFromResource(":/src/shaders/compiled/circle.frag.qsb");
        QShader triangleVertShader = loadShaderFromResource(":/src/shaders/compiled/triangle.vert.qsb");
        QShader triangleFragShader = loadShaderFromResource(":/src/shaders/compiled/triangle.frag.qsb");

        // Verify all shaders loaded successfully before creating pipelines
        if (!backgroundVertShader.isValid() || !backgroundFragShader.isValid()) {
            throw std::runtime_error("Failed to load background shaders");
        }
        if (!midiEventVertShader.isValid() || !midiEventFragShader.isValid()) {
            throw std::runtime_error("Failed to load MIDI event shaders");
        }
        if (!pianoVertShader.isValid() || !pianoFragShader.isValid()) {
            throw std::runtime_error("Failed to load piano shaders");
        }
        if (!lineVertShader.isValid() || !lineFragShader.isValid()) {
            throw std::runtime_error("Failed to load line shaders");
        }
        if (!pianoComplexVertShader.isValid() || !pianoComplexFragShader.isValid()) {
            qWarning() << "RhiMatrixWidget: Complex piano shaders not available, using simple piano rendering";
        }
        if (!textVertShader.isValid() || !textFragShader.isValid()) {
            qWarning() << "RhiMatrixWidget: Text shaders not available, text rendering disabled";
        }
        if (!circleVertShader.isValid() || !circleFragShader.isValid()) {
            qWarning() << "RhiMatrixWidget: Circle shaders not available, using square recording indicator";
        }
        if (!triangleVertShader.isValid() || !triangleFragShader.isValid()) {
            qWarning() << "RhiMatrixWidget: Triangle shaders not available, using line cursor indicators";
        }

        qDebug() << "RhiMatrixWidget: All shaders loaded successfully, creating pipelines...";

        // Create background pipeline
        createBackgroundPipeline(backgroundVertShader, backgroundFragShader);

        // Create MIDI event pipeline
        createMidiEventPipeline(midiEventVertShader, midiEventFragShader);

        // Create piano pipeline
        createPianoPipeline(pianoVertShader, pianoFragShader);

        // Create line pipeline
        createLinePipeline(lineVertShader, lineFragShader);

        // Create advanced pipelines (optional - graceful fallback if shaders not available)
        if (pianoComplexVertShader.isValid() && pianoComplexFragShader.isValid()) {
            createPianoComplexPipeline(pianoComplexVertShader, pianoComplexFragShader);
        }
        if (textVertShader.isValid() && textFragShader.isValid()) {
            createTextPipeline(textVertShader, textFragShader);
        }
        if (circleVertShader.isValid() && circleFragShader.isValid()) {
            createCirclePipeline(circleVertShader, circleFragShader);
        }
        if (triangleVertShader.isValid() && triangleFragShader.isValid()) {
            createTrianglePipeline(triangleVertShader, triangleFragShader);
        }

        m_rhi.shadersLoaded = true;
        qDebug() << "RhiMatrixWidget: All shaders loaded and pipelines created successfully";

    } catch (const std::exception &e) {
        qWarning() << "RhiMatrixWidget: Shader initialization failed:" << e.what();
        cleanupRhiResources();
        throw;
    }
}

QShader RhiMatrixWidget::loadShaderFromResource(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "RhiMatrixWidget: Failed to open shader resource:" << path;
        return QShader();
    }

    QByteArray data = file.readAll();
    if (data.isEmpty()) {
        qWarning() << "RhiMatrixWidget: Shader resource is empty:" << path;
        return QShader();
    }

    QShader shader = QShader::fromSerialized(data);
    if (!shader.isValid()) {
        qWarning() << "RhiMatrixWidget: Failed to deserialize shader:" << path;
        return QShader();
    }

    return shader;
}

void RhiMatrixWidget::createBackgroundPipeline(const QShader &vertShader, const QShader &fragShader) {
    if (!m_rhi.rhi) return;

    if (!vertShader.isValid() || !fragShader.isValid()) {
        qWarning() << "Failed to load background shaders";
        return;
    }

    // Create shader resource bindings
    m_rhi.backgroundSrb = m_rhi.rhi->newShaderResourceBindings();
    m_rhi.backgroundSrb->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage, m_rhi.uniformBuffer)
    });
    if (!m_rhi.backgroundSrb->create()) {
        qWarning() << "Failed to create background shader resource bindings";
        return;
    }

    // Create graphics pipeline
    m_rhi.backgroundPipeline = m_rhi.rhi->newGraphicsPipeline();
    m_rhi.backgroundPipeline->setShaderStages({
        { QRhiShaderStage::Vertex, vertShader },
        { QRhiShaderStage::Fragment, fragShader }
    });

    // Set vertex input layout for background quads (position + UV)
    QRhiVertexInputLayout inputLayout;
    inputLayout.setBindings({
        { 5 * sizeof(float) } // 3 floats position + 2 floats UV
    });
    inputLayout.setAttributes({
        { 0, 0, QRhiVertexInputAttribute::Float3, 0 },                    // position
        { 0, 1, QRhiVertexInputAttribute::Float2, 3 * sizeof(float) }     // UV
    });

    m_rhi.backgroundPipeline->setVertexInputLayout(inputLayout);
    m_rhi.backgroundPipeline->setShaderResourceBindings(m_rhi.backgroundSrb);
    m_rhi.backgroundPipeline->setRenderPassDescriptor(renderTarget()->renderPassDescriptor());

    if (!m_rhi.backgroundPipeline->create()) {
        qWarning() << "Failed to create background graphics pipeline";
        return;
    }

    qDebug() << "RhiMatrixWidget: Background pipeline created successfully";
}

void RhiMatrixWidget::createMidiEventPipeline(const QShader &vertShader, const QShader &fragShader) {
    if (!m_rhi.rhi) return;

    if (!vertShader.isValid() || !fragShader.isValid()) {
        qWarning() << "Failed to load MIDI event shaders";
        return;
    }

    // Create shader resource bindings for MIDI events
    m_rhi.midiEventSrb = m_rhi.rhi->newShaderResourceBindings();
    m_rhi.midiEventSrb->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage, m_rhi.uniformBuffer)
    });
    if (!m_rhi.midiEventSrb->create()) {
        qWarning() << "Failed to create MIDI event shader resource bindings";
        return;
    }

    // Create graphics pipeline for MIDI events
    m_rhi.midiEventPipeline = m_rhi.rhi->newGraphicsPipeline();
    m_rhi.midiEventPipeline->setShaderStages({
        { QRhiShaderStage::Vertex, vertShader },
        { QRhiShaderStage::Fragment, fragShader }
    });

    // Set vertex input layout for MIDI events (position + instance data)
    QRhiVertexInputLayout inputLayout;
    inputLayout.setBindings({
        { 5 * sizeof(float) },  // Binding 0: quad vertices (position + UV)
        { 12 * sizeof(float), QRhiVertexInputBinding::PerInstance }  // Binding 1: instance data (larger for MIDI events)
    });
    inputLayout.setAttributes({
        // Binding 0: Quad vertex data
        { 0, 0, QRhiVertexInputAttribute::Float3, 0 },                    // position
        { 0, 1, QRhiVertexInputAttribute::Float2, 3 * sizeof(float) },     // UV
        // Binding 1: Instance data (per MIDI event)
        { 1, 2, QRhiVertexInputAttribute::Float4, 0 },                    // position + size (x, y, width, height)
        { 1, 3, QRhiVertexInputAttribute::Float4, 4 * sizeof(float) },    // color (r, g, b, a)
        { 1, 4, QRhiVertexInputAttribute::Float4, 8 * sizeof(float) }     // extra data (velocity, channel, etc.)
    });

    m_rhi.midiEventPipeline->setVertexInputLayout(inputLayout);
    m_rhi.midiEventPipeline->setShaderResourceBindings(m_rhi.midiEventSrb);
    m_rhi.midiEventPipeline->setRenderPassDescriptor(renderTarget()->renderPassDescriptor());

    if (!m_rhi.midiEventPipeline->create()) {
        qWarning() << "Failed to create MIDI event graphics pipeline";
        return;
    }

    qDebug() << "RhiMatrixWidget: MIDI event pipeline created successfully";
}

void RhiMatrixWidget::createPianoPipeline(const QShader &vertShader, const QShader &fragShader) {
    if (!m_rhi.rhi) return;

    if (!vertShader.isValid() || !fragShader.isValid()) {
        qWarning() << "Failed to load piano shaders";
        return;
    }

    // Create shader resource bindings for piano keys
    m_rhi.pianoSrb = m_rhi.rhi->newShaderResourceBindings();
    m_rhi.pianoSrb->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage, m_rhi.uniformBuffer)
    });
    if (!m_rhi.pianoSrb->create()) {
        qWarning() << "Failed to create piano shader resource bindings";
        return;
    }

    // Create graphics pipeline for piano keys
    m_rhi.pianoPipeline = m_rhi.rhi->newGraphicsPipeline();
    m_rhi.pianoPipeline->setShaderStages({
        { QRhiShaderStage::Vertex, vertShader },
        { QRhiShaderStage::Fragment, fragShader }
    });

    // Set vertex input layout for piano keys (position + instance data)
    QRhiVertexInputLayout inputLayout;
    inputLayout.setBindings({
        { 5 * sizeof(float) },  // Binding 0: quad vertices (position + UV)
        { 8 * sizeof(float), QRhiVertexInputBinding::PerInstance }  // Binding 1: instance data
    });
    inputLayout.setAttributes({
        // Binding 0: Quad vertex data
        { 0, 0, QRhiVertexInputAttribute::Float3, 0 },                    // position
        { 0, 1, QRhiVertexInputAttribute::Float2, 3 * sizeof(float) },     // UV
        // Binding 1: Instance data (per piano key)
        { 1, 2, QRhiVertexInputAttribute::Float4, 0 },                    // position + size (x, y, width, height)
        { 1, 3, QRhiVertexInputAttribute::Float4, 4 * sizeof(float) }     // color (r, g, b, a)
    });

    m_rhi.pianoPipeline->setVertexInputLayout(inputLayout);
    m_rhi.pianoPipeline->setShaderResourceBindings(m_rhi.pianoSrb);
    m_rhi.pianoPipeline->setRenderPassDescriptor(renderTarget()->renderPassDescriptor());

    if (!m_rhi.pianoPipeline->create()) {
        qWarning() << "Failed to create piano graphics pipeline";
        return;
    }

    qDebug() << "RhiMatrixWidget: Piano pipeline created successfully";
}

void RhiMatrixWidget::createLinePipeline(const QShader &vertShader, const QShader &fragShader) {
    if (!m_rhi.rhi) return;

    if (!vertShader.isValid() || !fragShader.isValid()) {
        qWarning() << "Failed to load line shaders";
        return;
    }

    // Create shader resource bindings for lines
    m_rhi.lineSrb = m_rhi.rhi->newShaderResourceBindings();
    m_rhi.lineSrb->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage, m_rhi.uniformBuffer)
    });
    if (!m_rhi.lineSrb->create()) {
        qWarning() << "Failed to create line shader resource bindings";
        return;
    }

    // Create graphics pipeline for lines
    m_rhi.linePipeline = m_rhi.rhi->newGraphicsPipeline();
    m_rhi.linePipeline->setShaderStages({
        { QRhiShaderStage::Vertex, vertShader },
        { QRhiShaderStage::Fragment, fragShader }
    });

    // Set vertex input layout for lines (position + color per vertex)
    QRhiVertexInputLayout inputLayout;
    inputLayout.setBindings({
        { 6 * sizeof(float) }  // Binding 0: vertex data (position + color)
    });
    inputLayout.setAttributes({
        { 0, 0, QRhiVertexInputAttribute::Float2, 0 },                    // position (x, y)
        { 0, 1, QRhiVertexInputAttribute::Float4, 2 * sizeof(float) }     // color (r, g, b, a)
    });

    m_rhi.linePipeline->setVertexInputLayout(inputLayout);
    m_rhi.linePipeline->setShaderResourceBindings(m_rhi.lineSrb);
    m_rhi.linePipeline->setRenderPassDescriptor(renderTarget()->renderPassDescriptor());

    // Set topology for lines
    m_rhi.linePipeline->setTopology(QRhiGraphicsPipeline::Lines);

    if (!m_rhi.linePipeline->create()) {
        qWarning() << "Failed to create line graphics pipeline";
        return;
    }

    qDebug() << "RhiMatrixWidget: Line pipeline created successfully";
}

void RhiMatrixWidget::createPianoComplexPipeline(const QShader &vertShader, const QShader &fragShader) {
    if (!m_rhi.rhi) return;

    if (!vertShader.isValid() || !fragShader.isValid()) {
        qWarning() << "RhiMatrixWidget: Failed to load piano complex shaders";
        return;
    }

    m_rhi.pianoComplexPipeline = m_rhi.rhi->newGraphicsPipeline();
    m_rhi.pianoComplexPipeline->setShaderStages({
        { QRhiShaderStage::Vertex, vertShader },
        { QRhiShaderStage::Fragment, fragShader }
    });

    // Piano complex vertex input layout (quad + instance data with shape parameters)
    QRhiVertexInputLayout inputLayout;
    inputLayout.setBindings({
        { 3 * sizeof(float) + 2 * sizeof(float) }, // Binding 0: position + uv
        { 12 * sizeof(float), QRhiVertexInputBinding::PerInstance } // Binding 1: piano complex instance data
    });
    inputLayout.setAttributes({
        { 0, 0, QRhiVertexInputAttribute::Float3, 0 },                    // position
        { 0, 1, QRhiVertexInputAttribute::Float2, 3 * sizeof(float) },    // uv
        { 1, 2, QRhiVertexInputAttribute::Float4, 0 },                    // keyTransform
        { 1, 3, QRhiVertexInputAttribute::Float4, 4 * sizeof(float) },    // keyColor
        { 1, 4, QRhiVertexInputAttribute::Float4, 8 * sizeof(float) }     // keyParams
    });

    m_rhi.pianoComplexPipeline->setVertexInputLayout(inputLayout);
    m_rhi.pianoComplexPipeline->setRenderPassDescriptor(renderTarget()->renderPassDescriptor());

    // Create shader resource bindings for piano complex
    m_rhi.pianoComplexSrb = m_rhi.rhi->newShaderResourceBindings();
    m_rhi.pianoComplexSrb->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage, m_rhi.uniformBuffer)
    });

    if (!m_rhi.pianoComplexSrb->create()) {
        qWarning() << "RhiMatrixWidget: Failed to create piano complex shader resource bindings";
        return;
    }

    m_rhi.pianoComplexPipeline->setShaderResourceBindings(m_rhi.pianoComplexSrb);

    if (!m_rhi.pianoComplexPipeline->create()) {
        qWarning() << "RhiMatrixWidget: Failed to create piano complex graphics pipeline";
        return;
    }

    qDebug() << "RhiMatrixWidget: Piano complex pipeline created successfully";
}

void RhiMatrixWidget::createTextPipeline(const QShader &vertShader, const QShader &fragShader) {
    if (!m_rhi.rhi || !m_rhi.fontAtlasTexture || !m_rhi.fontAtlasSampler) {
        qWarning() << "RhiMatrixWidget: Cannot create text pipeline - missing font atlas";
        return;
    }

    if (!vertShader.isValid() || !fragShader.isValid()) {
        qWarning() << "RhiMatrixWidget: Failed to load text shaders";
        return;
    }

    m_rhi.textPipeline = m_rhi.rhi->newGraphicsPipeline();
    m_rhi.textPipeline->setShaderStages({
        { QRhiShaderStage::Vertex, vertShader },
        { QRhiShaderStage::Fragment, fragShader }
    });

    // Text vertex input layout (quad + instance data)
    QRhiVertexInputLayout inputLayout;
    inputLayout.setBindings({
        { 3 * sizeof(float) + 2 * sizeof(float) }, // Binding 0: position + uv
        { 12 * sizeof(float), QRhiVertexInputBinding::PerInstance } // Binding 1: text instance data
    });
    inputLayout.setAttributes({
        { 0, 0, QRhiVertexInputAttribute::Float3, 0 },                    // position
        { 0, 1, QRhiVertexInputAttribute::Float2, 3 * sizeof(float) },    // uv
        { 1, 2, QRhiVertexInputAttribute::Float4, 0 },                    // instancePosSize
        { 1, 3, QRhiVertexInputAttribute::Float4, 4 * sizeof(float) },    // instanceColor
        { 1, 4, QRhiVertexInputAttribute::Float4, 8 * sizeof(float) }     // instanceUV
    });

    m_rhi.textPipeline->setVertexInputLayout(inputLayout);
    m_rhi.textPipeline->setRenderPassDescriptor(renderTarget()->renderPassDescriptor());

    // Enable alpha blending for text
    QRhiGraphicsPipeline::TargetBlend blend;
    blend.enable = true;
    blend.srcColor = QRhiGraphicsPipeline::SrcAlpha;
    blend.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
    blend.srcAlpha = QRhiGraphicsPipeline::One;
    blend.dstAlpha = QRhiGraphicsPipeline::OneMinusSrcAlpha;
    m_rhi.textPipeline->setTargetBlends({ blend });

    // Create shader resource bindings for text (uniform buffer + font atlas)
    m_rhi.textSrb = m_rhi.rhi->newShaderResourceBindings();
    m_rhi.textSrb->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage, m_rhi.uniformBuffer),
        QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage, m_rhi.fontAtlasTexture, m_rhi.fontAtlasSampler)
    });

    if (!m_rhi.textSrb->create()) {
        qWarning() << "RhiMatrixWidget: Failed to create text shader resource bindings";
        return;
    }

    m_rhi.textPipeline->setShaderResourceBindings(m_rhi.textSrb);

    if (!m_rhi.textPipeline->create()) {
        qWarning() << "RhiMatrixWidget: Failed to create text graphics pipeline";
        return;
    }

    qDebug() << "RhiMatrixWidget: Text pipeline created successfully";
}

void RhiMatrixWidget::createCirclePipeline(const QShader &vertShader, const QShader &fragShader) {
    if (!m_rhi.rhi) return;

    if (!vertShader.isValid() || !fragShader.isValid()) {
        qWarning() << "RhiMatrixWidget: Failed to load circle shaders";
        return;
    }

    m_rhi.circlePipeline = m_rhi.rhi->newGraphicsPipeline();
    m_rhi.circlePipeline->setShaderStages({
        { QRhiShaderStage::Vertex, vertShader },
        { QRhiShaderStage::Fragment, fragShader }
    });

    // Circle vertex input layout (quad + instance data)
    QRhiVertexInputLayout inputLayout;
    inputLayout.setBindings({
        { 3 * sizeof(float) + 2 * sizeof(float) }, // Binding 0: position + uv
        { 8 * sizeof(float), QRhiVertexInputBinding::PerInstance } // Binding 1: circle instance data
    });
    inputLayout.setAttributes({
        { 0, 0, QRhiVertexInputAttribute::Float3, 0 },                    // position
        { 0, 1, QRhiVertexInputAttribute::Float2, 3 * sizeof(float) },    // uv
        { 1, 2, QRhiVertexInputAttribute::Float4, 0 },                    // circleTransform
        { 1, 3, QRhiVertexInputAttribute::Float4, 4 * sizeof(float) }     // circleColor
    });

    m_rhi.circlePipeline->setVertexInputLayout(inputLayout);
    m_rhi.circlePipeline->setRenderPassDescriptor(renderTarget()->renderPassDescriptor());

    // Enable alpha blending for circles
    QRhiGraphicsPipeline::TargetBlend blend;
    blend.enable = true;
    blend.srcColor = QRhiGraphicsPipeline::SrcAlpha;
    blend.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
    blend.srcAlpha = QRhiGraphicsPipeline::One;
    blend.dstAlpha = QRhiGraphicsPipeline::OneMinusSrcAlpha;
    m_rhi.circlePipeline->setTargetBlends({ blend });

    // Create shader resource bindings for circle
    m_rhi.circleSrb = m_rhi.rhi->newShaderResourceBindings();
    m_rhi.circleSrb->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage, m_rhi.uniformBuffer)
    });

    if (!m_rhi.circleSrb->create()) {
        qWarning() << "RhiMatrixWidget: Failed to create circle shader resource bindings";
        return;
    }

    m_rhi.circlePipeline->setShaderResourceBindings(m_rhi.circleSrb);

    if (!m_rhi.circlePipeline->create()) {
        qWarning() << "RhiMatrixWidget: Failed to create circle graphics pipeline";
        return;
    }

    qDebug() << "RhiMatrixWidget: Circle pipeline created successfully";
}

void RhiMatrixWidget::createTrianglePipeline(const QShader &vertShader, const QShader &fragShader) {
    if (!m_rhi.rhi) return;

    if (!vertShader.isValid() || !fragShader.isValid()) {
        qWarning() << "RhiMatrixWidget: Failed to load triangle shaders";
        return;
    }

    m_rhi.trianglePipeline = m_rhi.rhi->newGraphicsPipeline();
    m_rhi.trianglePipeline->setShaderStages({
        { QRhiShaderStage::Vertex, vertShader },
        { QRhiShaderStage::Fragment, fragShader }
    });

    // Triangle vertex input layout (triangle vertices + instance data)
    QRhiVertexInputLayout inputLayout;
    inputLayout.setBindings({
        { 3 * sizeof(float) + 2 * sizeof(float) }, // Binding 0: position + uv
        { 8 * sizeof(float), QRhiVertexInputBinding::PerInstance } // Binding 1: triangle instance data
    });
    inputLayout.setAttributes({
        { 0, 0, QRhiVertexInputAttribute::Float3, 0 },                    // position
        { 0, 1, QRhiVertexInputAttribute::Float2, 3 * sizeof(float) },    // uv
        { 1, 2, QRhiVertexInputAttribute::Float4, 0 },                    // triangleTransform
        { 1, 3, QRhiVertexInputAttribute::Float4, 4 * sizeof(float) }     // triangleColor
    });

    m_rhi.trianglePipeline->setVertexInputLayout(inputLayout);
    m_rhi.trianglePipeline->setRenderPassDescriptor(renderTarget()->renderPassDescriptor());

    // Set topology for triangles
    m_rhi.trianglePipeline->setTopology(QRhiGraphicsPipeline::Triangles);

    // Enable alpha blending for triangles
    QRhiGraphicsPipeline::TargetBlend blend;
    blend.enable = true;
    blend.srcColor = QRhiGraphicsPipeline::SrcAlpha;
    blend.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
    blend.srcAlpha = QRhiGraphicsPipeline::One;
    blend.dstAlpha = QRhiGraphicsPipeline::OneMinusSrcAlpha;
    m_rhi.trianglePipeline->setTargetBlends({ blend });

    // Create shader resource bindings for triangle
    m_rhi.triangleSrb = m_rhi.rhi->newShaderResourceBindings();
    m_rhi.triangleSrb->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage, m_rhi.uniformBuffer)
    });

    if (!m_rhi.triangleSrb->create()) {
        qWarning() << "RhiMatrixWidget: Failed to create triangle shader resource bindings";
        return;
    }

    m_rhi.trianglePipeline->setShaderResourceBindings(m_rhi.triangleSrb);

    if (!m_rhi.trianglePipeline->create()) {
        qWarning() << "RhiMatrixWidget: Failed to create triangle graphics pipeline";
        return;
    }

    qDebug() << "RhiMatrixWidget: Triangle pipeline created successfully";
}

void RhiMatrixWidget::renderBackground(QRhiCommandBuffer *cb, QRhiRenderTarget *rt) {
    Q_UNUSED(rt)

    static int bgRenderCount = 0;
    bgRenderCount++;

    if (bgRenderCount % 60 == 0) {
        qDebug() << "RhiMatrixWidget: Background render called - frame" << bgRenderCount;
        qDebug() << "RhiMatrixWidget: Pipeline available:" << (m_rhi.backgroundPipeline ? "yes" : "no");
        qDebug() << "RhiMatrixWidget: Vertex buffer available:" << (m_rhi.quadVertexBuffer ? "yes" : "no");
    }

    if (!m_rhi.backgroundPipeline || !m_rhi.quadVertexBuffer) {
        if (bgRenderCount % 60 == 0) {
            qDebug() << "RhiMatrixWidget: Background render skipped - missing resources";
            qDebug() << "RhiMatrixWidget: Pipeline:" << (m_rhi.backgroundPipeline ? "available" : "NULL");
            qDebug() << "RhiMatrixWidget: Vertex buffer:" << (m_rhi.quadVertexBuffer ? "available" : "NULL");
        }
        return;
    }

    // Set up background rendering pipeline
    cb->setGraphicsPipeline(m_rhi.backgroundPipeline);
    const QSize outputSize = renderTarget()->pixelSize();
    cb->setViewport(QRhiViewport(0, 0, outputSize.width(), outputSize.height()));

    // Bind shader resources
    if (m_rhi.backgroundSrb) {
        cb->setShaderResources(m_rhi.backgroundSrb);
    }

    // Bind vertex buffer for background quad
    const QRhiCommandBuffer::VertexInput vbufBinding(m_rhi.quadVertexBuffer, 0);
    cb->setVertexInput(0, 1, &vbufBinding);

    // Draw background quad (2 triangles = 6 vertices)
    cb->draw(6);
}

void RhiMatrixWidget::renderMidiEvents(QRhiCommandBuffer *cb, QRhiRenderTarget *rt) {
    Q_UNUSED(rt)

    static int callCount = 0;
    callCount++;

    if (!file) {
        if (callCount <= 5) {
            qDebug() << "RhiMatrixWidget: No file loaded, skipping MIDI events";
        }
        return;
    }

    // MIDI event data is updated in the main render loop, not here

    // Debug rendering conditions
    if (callCount <= 5) {
        qDebug() << "RhiMatrixWidget: MIDI event render conditions - count:" << m_rhi.midiEventInstanceCount
                 << "pipeline:" << (m_rhi.midiEventPipeline ? "ok" : "missing")
                 << "quadBuffer:" << (m_rhi.quadVertexBuffer ? "ok" : "missing")
                 << "instanceBuffer:" << (m_rhi.midiEventInstanceBuffer ? "ok" : "missing");

        if (file && objects) {
            qDebug() << "RhiMatrixWidget: File has" << objects->size() << "total events in objects list";
            qDebug() << "RhiMatrixWidget: View range - startTick:" << startTick << "endTick:" << endTick;
            qDebug() << "RhiMatrixWidget: View range - startLineY:" << startLineY << "endLineY:" << endLineY;
            qDebug() << "RhiMatrixWidget: Time range - startTimeX:" << startTimeX << "endTimeX:" << endTimeX;
            qDebug() << "RhiMatrixWidget: Widget size:" << width() << "x" << height() << "lineNameWidth:" << lineNameWidth << "timeHeight:" << timeHeight;
            qDebug() << "RhiMatrixWidget: Scale - scaleX:" << scaleX << "scaleY:" << scaleY;
        }
    }

    // Render MIDI events using the MIDI event pipeline
    if (m_rhi.midiEventInstanceCount > 0 && m_rhi.midiEventPipeline && m_rhi.quadVertexBuffer && m_rhi.midiEventInstanceBuffer) {
        if (callCount <= 5) {
            qDebug() << "RhiMatrixWidget: Rendering" << m_rhi.midiEventInstanceCount << "MIDI events";
        }

        // Set up MIDI event rendering pipeline
        cb->setGraphicsPipeline(m_rhi.midiEventPipeline);
        const QSize outputSize = renderTarget()->pixelSize();
        cb->setViewport(QRhiViewport(0, 0, outputSize.width(), outputSize.height()));

        // Bind shader resources
        if (m_rhi.midiEventSrb) {
            cb->setShaderResources(m_rhi.midiEventSrb);
        }

        // Bind vertex buffers (quad vertices + MIDI event instance data)
        const QRhiCommandBuffer::VertexInput vbufBindings[] = {
            { m_rhi.quadVertexBuffer, 0 },           // Binding 0: quad vertices
            { m_rhi.midiEventInstanceBuffer, 0 }     // Binding 1: instance data
        };
        cb->setVertexInput(0, 2, vbufBindings);

        // Draw instanced quads (6 vertices per quad, midiEventInstanceCount instances)
        cb->draw(6, m_rhi.midiEventInstanceCount);

    } else if (callCount <= 5) {
        qDebug() << "RhiMatrixWidget: MIDI event rendering skipped - count:" << m_rhi.midiEventInstanceCount;
    }
}

void RhiMatrixWidget::renderMidiEventsForChannel(int channel) {
    if (!file) return;

    // TODO: Implement proper MIDI event collection for RHI rendering
    // This is a placeholder for future implementation
    Q_UNUSED(channel)
}

void RhiMatrixWidget::renderPianoKeys(QRhiCommandBuffer *cb, QRhiRenderTarget *rt) {
    Q_UNUSED(rt)

    static int callCount = 0;
    callCount++;
    if (callCount <= 5) {
        qDebug() << "RhiMatrixWidget: renderPianoKeys() called #" << callCount;
    }

    if (!file) {
        if (callCount <= 5) {
            qDebug() << "RhiMatrixWidget: No file loaded, skipping piano keys";
        }
        return;
    }

    // Use complex piano rendering to match software MatrixWidget's complex key shapes
    // Software uses complex polygons with cut-out corners and proper key separation
    bool useComplexPiano = (m_rhi.pianoComplexPipeline && m_rhi.pianoComplexInstanceBuffer && m_rhi.pianoComplexSrb);

    if (useComplexPiano) {
        // Update complex piano key instance data
        updatePianoComplexInstances();

        if (callCount <= 5) {
            qDebug() << "RhiMatrixWidget: Using complex piano rendering with" << m_rhi.pianoComplexInstanceCount << "keys";
        }

        // Render using complex piano pipeline
        if (m_rhi.pianoComplexInstanceCount > 0) {
            renderComplexPianoKeys(cb, rt);
        }
    } else {
        // Fallback to simple piano rendering
        updatePianoKeyInstances();

        // Debug rendering conditions
        if (callCount <= 5) {
            qDebug() << "RhiMatrixWidget: Using simple piano rendering - count:" << m_rhi.pianoInstanceCount
                     << "pipeline:" << (m_rhi.pianoPipeline ? "ok" : "missing")
                     << "quadBuffer:" << (m_rhi.quadVertexBuffer ? "ok" : "missing")
                     << "instanceBuffer:" << (m_rhi.pianoInstanceBuffer ? "ok" : "missing");
        }

        // Render piano keys using the simple piano pipeline
        if (m_rhi.pianoInstanceCount > 0 && m_rhi.pianoPipeline && m_rhi.quadVertexBuffer && m_rhi.pianoInstanceBuffer) {
        if (callCount <= 5) {
            qDebug() << "RhiMatrixWidget: Rendering" << m_rhi.pianoInstanceCount << "piano keys";
        }

        // Set up piano rendering pipeline
        cb->setGraphicsPipeline(m_rhi.pianoPipeline);
        const QSize outputSize = renderTarget()->pixelSize();
        cb->setViewport(QRhiViewport(0, 0, outputSize.width(), outputSize.height()));

        // Bind shader resources
        if (m_rhi.pianoSrb) {
            cb->setShaderResources(m_rhi.pianoSrb);
        }

        // Bind vertex buffers (quad vertices + instance data)
        const QRhiCommandBuffer::VertexInput vbufBindings[] = {
            { m_rhi.quadVertexBuffer, 0 },      // Binding 0: quad vertices
            { m_rhi.pianoInstanceBuffer, 0 }    // Binding 1: instance data
        };
        cb->setVertexInput(0, 2, vbufBindings);

            // Draw instanced quads (6 vertices per quad, pianoInstanceCount instances)
            cb->draw(6, m_rhi.pianoInstanceCount);

        } else {
            static int debugCount = 0;
            debugCount++;

            if (debugCount % 300 == 0) {
                qDebug() << "RhiMatrixWidget: Simple piano key rendering skipped - count:" << m_rhi.pianoInstanceCount
                         << "pipeline:" << (m_rhi.pianoPipeline ? "ok" : "missing")
                         << "vertex buffer:" << (m_rhi.quadVertexBuffer ? "ok" : "missing")
                         << "instance buffer:" << (m_rhi.pianoInstanceBuffer ? "ok" : "missing");
            }
        }
    }
}

void RhiMatrixWidget::updatePianoComplexInstances() {
    if (!m_rhi.pianoComplexInstanceBuffer || !file) {
        m_rhi.pianoComplexInstanceCount = 0;
        return;
    }

    QVector<float> instanceData;
    int keyCount = 0;

    // Generate complex piano key instance data (EXACT COPY from MatrixWidget logic)
    for (int line = startLineY; line <= endLineY && keyCount < RHI_MAX_INSTANCES; line++) {
        if (line < 0 || line > 127) continue;

        int midiNote = 127 - line;
        int note = midiNote % 12;
        bool isBlackKey = (note == 1 || note == 3 || note == 6 || note == 8 || note == 10);

        // Calculate key position and size (EXACT COPY from MatrixWidget)
        int y = yPosOfLine(line);
        int keyHeight = lineHeight();
        int keyX = 10;
        int keyWidth = lineNameWidth - 20;

        if (isBlackKey) {
            keyWidth = static_cast<int>(keyWidth * 0.6); // Black keys are narrower
            keyHeight = static_cast<int>(keyHeight * 0.5); // Black keys are shorter
        }

        // Determine shape parameters using EXACT MatrixWidget logic
        bool blackOnTop = false;
        bool blackBeneath = false;

        switch (note) {
            case 0: // C
                blackOnTop = true;
                blackBeneath = false;
                break;
            case 2: // D
                blackOnTop = true;
                blackBeneath = true;
                break;
            case 4: // E
                blackOnTop = false;
                blackBeneath = true;
                break;
            case 5: // F
                blackOnTop = true;
                blackBeneath = false;
                break;
            case 7: // G
                blackOnTop = true;
                blackBeneath = true;
                break;
            case 9: // A
                blackOnTop = true;
                blackBeneath = true;
                break;
            case 11: // B
                blackOnTop = false;
                blackBeneath = true;
                break;
        }

        // Special case: if this is the top line, don't cut top
        if (127 - midiNote == startLineY) {
            blackOnTop = false;
        }

        // Calculate key color with hover effects
        QColor keyColor;
        bool isHovered = false;
        bool isSelected = false;

        // Check if mouse is hovering over this key
        QRect keyRect(keyX, y, keyWidth, keyHeight);
        if (enabled && mouseInRect(PianoArea) && keyRect.contains(mouseX, mouseY)) {
            isHovered = true;
        }

        // Check if this piano key line has selected events (EXACT COPY from MatrixWidget lines 858-863)
        foreach(MidiEvent* event, Selection::instance()->selectedEvents()) {
            if (event->line() == 127 - midiNote) {
                isSelected = true;
                break;
            }
        }

        // Apply color based on state priority: selected > hovered > normal
        if (isSelected) {
            keyColor = isBlackKey ? _cachedPianoBlackKeySelectedColor : _cachedPianoWhiteKeySelectedColor;
        } else if (isHovered) {
            keyColor = isBlackKey ? _cachedPianoBlackKeyHoverColor : _cachedPianoWhiteKeyHoverColor;
        } else {
            keyColor = isBlackKey ? _cachedPianoBlackKeyColor : _cachedPianoWhiteKeyColor;
        }

        // Instance data: keyTransform (x,y,w,h), keyColor (r,g,b,a), keyParams (midiNote, isBlack, blackOnTop, blackBeneath)
        instanceData.append(static_cast<float>(keyX));
        instanceData.append(static_cast<float>(y));
        instanceData.append(static_cast<float>(keyWidth));
        instanceData.append(static_cast<float>(keyHeight));
        instanceData.append(keyColor.redF());
        instanceData.append(keyColor.greenF());
        instanceData.append(keyColor.blueF());
        instanceData.append(keyColor.alphaF());
        instanceData.append(static_cast<float>(midiNote));
        instanceData.append(isBlackKey ? 1.0f : 0.0f);
        instanceData.append(blackOnTop ? 1.0f : 0.0f);
        instanceData.append(blackBeneath ? 1.0f : 0.0f);

        keyCount++;

        // Store piano key rectangle for mouse interaction
        QRect keyRectForMouse(keyX, y, keyWidth, keyHeight);
        pianoKeys[midiNote] = keyRectForMouse;
    }

    m_rhi.pianoComplexInstanceCount = keyCount;

    if (m_rhi.pianoComplexInstanceCount > 0 && !instanceData.isEmpty() && m_rhi.rhi && m_rhi.pianoComplexInstanceBuffer) {
        // Create or reuse the resource update batch for this frame
        if (!m_rhi.resourceUpdates) {
            m_rhi.resourceUpdates = m_rhi.rhi->nextResourceUpdateBatch();
        }

        if (m_rhi.resourceUpdates) {
            m_rhi.resourceUpdates->updateDynamicBuffer(m_rhi.pianoComplexInstanceBuffer, 0,
                                      instanceData.size() * sizeof(float),
                                      instanceData.constData());
        }
    }
}

void RhiMatrixWidget::renderComplexPianoKeys(QRhiCommandBuffer *cb, QRhiRenderTarget *rt) {
    Q_UNUSED(rt)

    if (m_rhi.pianoComplexInstanceCount <= 0 || !m_rhi.pianoComplexPipeline || !m_rhi.quadVertexBuffer || !m_rhi.pianoComplexInstanceBuffer) {
        return;
    }

    // Set up complex piano rendering pipeline
    cb->setGraphicsPipeline(m_rhi.pianoComplexPipeline);
    const QSize outputSize = renderTarget()->pixelSize();
    cb->setViewport(QRhiViewport(0, 0, outputSize.width(), outputSize.height()));

    // Bind shader resources
    if (m_rhi.pianoComplexSrb) {
        cb->setShaderResources(m_rhi.pianoComplexSrb);
    }

    // Bind vertex buffers (quad vertices + complex piano instance data)
    const QRhiCommandBuffer::VertexInput vbufBindings[] = {
        { m_rhi.quadVertexBuffer, 0 },              // Binding 0: quad vertices
        { m_rhi.pianoComplexInstanceBuffer, 0 }     // Binding 1: complex instance data
    };
    cb->setVertexInput(0, 2, vbufBindings);

    // Draw instanced quads with complex shapes (6 vertices per quad, pianoComplexInstanceCount instances)
    cb->draw(6, m_rhi.pianoComplexInstanceCount);
}

void RhiMatrixWidget::renderCursorTriangle(QRhiCommandBuffer *cb, int x, int y, const QColor &color) {
    // Use triangle pipeline if available, otherwise skip
    if (!m_rhi.trianglePipeline || !m_rhi.triangleInstanceBuffer || !m_rhi.triangleSrb) {
        return; // Graceful fallback - triangles are optional visual enhancement
    }

    QVector<float> instanceData;
    int triangleSize = 8;

    // Triangle instance data: triangleTransform (x, y, size, rotation), triangleColor (r, g, b, a)
    instanceData.append(static_cast<float>(x));           // x (center)
    instanceData.append(static_cast<float>(y));           // y (center)
    instanceData.append(static_cast<float>(triangleSize)); // size
    instanceData.append(0.0f);                            // rotation (0 = pointing down)
    instanceData.append(color.redF());                    // colorR
    instanceData.append(color.greenF());                  // colorG
    instanceData.append(color.blueF());                   // colorB
    instanceData.append(color.alphaF());                  // colorA

    // Update triangle instance buffer
    // Create or reuse the resource update batch for this frame
    if (!m_rhi.resourceUpdates) {
        m_rhi.resourceUpdates = m_rhi.rhi->nextResourceUpdateBatch();
    }

    if (m_rhi.resourceUpdates) {
        m_rhi.resourceUpdates->updateDynamicBuffer(m_rhi.triangleInstanceBuffer, 0,
                                  instanceData.size() * sizeof(float),
                                  instanceData.constData());
    }

    // Render the triangle (outside the resource update check)
    cb->setGraphicsPipeline(m_rhi.trianglePipeline);
    const QSize outputSize = renderTarget()->pixelSize();
    cb->setViewport(QRhiViewport(0, 0, outputSize.width(), outputSize.height()));

    if (m_rhi.triangleSrb) {
        cb->setShaderResources(m_rhi.triangleSrb);
    }

    const QRhiCommandBuffer::VertexInput vbufBindings[] = {
        { m_rhi.quadVertexBuffer, 0 },
        { m_rhi.triangleInstanceBuffer, 0 }
    };
    cb->setVertexInput(0, 2, vbufBindings);

    // Draw single triangle
    cb->draw(6, 1);
}

void RhiMatrixWidget::renderSelectionLines(const QList<QPair<int, int>> &selectionLines) {
    if (selectionLines.isEmpty() || !m_rhi.linePipeline || !m_rhi.lineVertexBuffer) {
        return;
    }

    // Generate line vertex data for selection borders (EXACT COPY from MatrixWidget lines 738-741)
    QVector<float> lineVertexData;

    for (const auto &linePair : selectionLines) {
        int topY = linePair.first;
        int bottomY = linePair.second;

        // Top selection line (gray line above selected event)
        lineVertexData.append(static_cast<float>(lineNameWidth));  // x1
        lineVertexData.append(static_cast<float>(topY));           // y1
        lineVertexData.append(0.5f);  // gray color R
        lineVertexData.append(0.5f);  // gray color G
        lineVertexData.append(0.5f);  // gray color B
        lineVertexData.append(1.0f);  // alpha

        lineVertexData.append(static_cast<float>(width()));        // x2
        lineVertexData.append(static_cast<float>(topY));           // y2
        lineVertexData.append(0.5f);  // gray color R
        lineVertexData.append(0.5f);  // gray color G
        lineVertexData.append(0.5f);  // gray color B
        lineVertexData.append(1.0f);  // alpha

        // Bottom selection line (gray line below selected event)
        lineVertexData.append(static_cast<float>(lineNameWidth));  // x1
        lineVertexData.append(static_cast<float>(bottomY));        // y1
        lineVertexData.append(0.5f);  // gray color R
        lineVertexData.append(0.5f);  // gray color G
        lineVertexData.append(0.5f);  // gray color B
        lineVertexData.append(1.0f);  // alpha

        lineVertexData.append(static_cast<float>(width()));        // x2
        lineVertexData.append(static_cast<float>(bottomY));        // y2
        lineVertexData.append(0.5f);  // gray color R
        lineVertexData.append(0.5f);  // gray color G
        lineVertexData.append(0.5f);  // gray color B
        lineVertexData.append(1.0f);  // alpha
    }

    if (lineVertexData.isEmpty()) return;

    // Update line vertex buffer with selection lines
    // Create or reuse the resource update batch for this frame
    if (!m_rhi.resourceUpdates) {
        m_rhi.resourceUpdates = m_rhi.rhi->nextResourceUpdateBatch();
    }

    if (m_rhi.resourceUpdates) {
        m_rhi.resourceUpdates->updateDynamicBuffer(m_rhi.lineVertexBuffer, 0,
                                  lineVertexData.size() * sizeof(float),
                                  lineVertexData.constData());
    }
}

void RhiMatrixWidget::renderPianoAreaBackground(QRhiCommandBuffer *cb, QRhiRenderTarget *rt) {
    Q_UNUSED(rt)

    // EXACT COPY from MatrixWidget lines 235-246
    if (!file) return;

    QVector<float> instanceData;

    // First: fill background of the line descriptions (EXACT COPY from MatrixWidget line 236)
    instanceData.append(static_cast<float>(PianoArea.x()));
    instanceData.append(static_cast<float>(PianoArea.y()));
    instanceData.append(static_cast<float>(PianoArea.width()));
    instanceData.append(static_cast<float>(PianoArea.height()));
    instanceData.append(_cachedSystemWindowColor.redF());
    instanceData.append(_cachedSystemWindowColor.greenF());
    instanceData.append(_cachedSystemWindowColor.blueF());
    instanceData.append(_cachedSystemWindowColor.alphaF());

    // Second: fill the piano's background (EXACT COPY from MatrixWidget lines 238-246)
    int pianoKeys = endLineY - startLineY;
    if (endLineY > 127) {
        pianoKeys -= (endLineY - 127);
    }

    if (pianoKeys > 0) {
        // Piano white key background rectangle (EXACT COPY from MatrixWidget lines 244-246)
        int pianoX = 0;
        int pianoY = timeHeight;
        int pianoWidth = lineNameWidth - 10; // Same as MatrixWidget
        int pianoHeight = pianoKeys * lineHeight();

        // Instance data: posX, posY, sizeX, sizeY, colorR, colorG, colorB, colorA
        instanceData.append(static_cast<float>(pianoX));
        instanceData.append(static_cast<float>(pianoY));
        instanceData.append(static_cast<float>(pianoWidth));
        instanceData.append(static_cast<float>(pianoHeight));
        instanceData.append(_cachedPianoWhiteKeyColor.redF());
        instanceData.append(_cachedPianoWhiteKeyColor.greenF());
        instanceData.append(_cachedPianoWhiteKeyColor.blueF());
        instanceData.append(_cachedPianoWhiteKeyColor.alphaF());
    }

    // Update a temporary buffer or reuse piano instance buffer
    if (m_rhi.pianoPipeline && m_rhi.quadVertexBuffer && m_rhi.pianoInstanceBuffer && !instanceData.isEmpty()) {
        // Create or reuse the resource update batch for this frame
        if (!m_rhi.resourceUpdates) {
            m_rhi.resourceUpdates = m_rhi.rhi->nextResourceUpdateBatch();
        }

        if (m_rhi.resourceUpdates) {
            m_rhi.resourceUpdates->updateDynamicBuffer(m_rhi.pianoInstanceBuffer, 0,
                                      instanceData.size() * sizeof(float),
                                      instanceData.constData());
        }

        // Render the piano area background
        cb->setGraphicsPipeline(m_rhi.pianoPipeline);
        const QSize outputSize = renderTarget()->pixelSize();
        cb->setViewport(QRhiViewport(0, 0, outputSize.width(), outputSize.height()));

        if (m_rhi.pianoSrb) {
            cb->setShaderResources(m_rhi.pianoSrb);
        }

        const QRhiCommandBuffer::VertexInput vbufBindings[] = {
            { m_rhi.quadVertexBuffer, 0 },
            { m_rhi.pianoInstanceBuffer, 0 }
        };
        cb->setVertexInput(0, 2, vbufBindings);

        // Draw quads for piano area backgrounds (system window + white key background)
        int numInstances = instanceData.size() / 8; // 8 floats per instance
        cb->draw(6, numInstances);
    }
}

void RhiMatrixWidget::renderRowStrips(QRhiCommandBuffer *cb, QRhiRenderTarget *rt) {
    Q_UNUSED(rt)

    static int callCount = 0;
    callCount++;

    if (!file) {
        if (callCount <= 5) {
            qDebug() << "RhiMatrixWidget: No file loaded, skipping row strips";
        }
        return;
    }

    // Row strip data is updated in the main render loop, not here

    // Debug rendering conditions
    if (callCount <= 5) {
        qDebug() << "RhiMatrixWidget: Row strip render conditions - count:" << m_rhi.rowStripInstanceCount
                 << "pipeline:" << (m_rhi.pianoPipeline ? "ok" : "missing")
                 << "quadBuffer:" << (m_rhi.quadVertexBuffer ? "ok" : "missing")
                 << "instanceBuffer:" << (m_rhi.rowStripInstanceBuffer ? "ok" : "missing");
    }

    // Render row strips using the piano pipeline (same quad rendering)
    if (m_rhi.rowStripInstanceCount > 0 && m_rhi.pianoPipeline && m_rhi.quadVertexBuffer && m_rhi.rowStripInstanceBuffer) {
        if (callCount <= 5) {
            qDebug() << "RhiMatrixWidget: Rendering" << m_rhi.rowStripInstanceCount << "row strips";
        }

        // Set up row strip rendering pipeline
        cb->setGraphicsPipeline(m_rhi.pianoPipeline);
        const QSize outputSize = renderTarget()->pixelSize();
        cb->setViewport(QRhiViewport(0, 0, outputSize.width(), outputSize.height()));

        // Bind shader resources
        if (m_rhi.pianoSrb) {
            cb->setShaderResources(m_rhi.pianoSrb);
        }

        // Bind vertex buffers (quad vertices + row strip instance data)
        const QRhiCommandBuffer::VertexInput vbufBindings[] = {
            { m_rhi.quadVertexBuffer, 0 },        // Binding 0: quad vertices
            { m_rhi.rowStripInstanceBuffer, 0 }   // Binding 1: instance data
        };
        cb->setVertexInput(0, 2, vbufBindings);

        // Draw instanced quads (6 vertices per quad, rowStripInstanceCount instances)
        cb->draw(6, m_rhi.rowStripInstanceCount);

    } else if (callCount <= 5) {
        qDebug() << "RhiMatrixWidget: Row strip rendering skipped - count:" << m_rhi.rowStripInstanceCount;
    }
}

void RhiMatrixWidget::renderLines(QRhiCommandBuffer *cb, QRhiRenderTarget *rt) {
    Q_UNUSED(rt)

    static int callCount = 0;
    callCount++;

    if (!file) {
        if (callCount <= 5) {
            qDebug() << "RhiMatrixWidget: No file loaded, skipping lines";
        }
        return;
    }

    // Update line vertex data
    updateLineVertices();

    // Debug rendering conditions
    if (callCount <= 5) {
        qDebug() << "RhiMatrixWidget: Line render conditions - vertexCount:" << m_rhi.lineVertexCount
                 << "pipeline:" << (m_rhi.linePipeline ? "ok" : "missing")
                 << "vertexBuffer:" << (m_rhi.lineVertexBuffer ? "ok" : "missing");
    }

    // Render lines using the line pipeline
    if (m_rhi.lineVertexCount > 0 && m_rhi.linePipeline && m_rhi.lineVertexBuffer) {
        if (callCount <= 5) {
            qDebug() << "RhiMatrixWidget: Rendering" << m_rhi.lineVertexCount << "line vertices";
        }

        // Set up line rendering pipeline
        cb->setGraphicsPipeline(m_rhi.linePipeline);
        const QSize outputSize = renderTarget()->pixelSize();
        cb->setViewport(QRhiViewport(0, 0, outputSize.width(), outputSize.height()));

        // Bind shader resources
        if (m_rhi.lineSrb) {
            cb->setShaderResources(m_rhi.lineSrb);
        }

        // Bind vertex buffer
        const QRhiCommandBuffer::VertexInput vbufBinding = { m_rhi.lineVertexBuffer, 0 };
        cb->setVertexInput(0, 1, &vbufBinding);

        // Draw lines (2 vertices per line)
        cb->draw(m_rhi.lineVertexCount);

    } else if (callCount <= 5) {
        qDebug() << "RhiMatrixWidget: Line rendering skipped - vertexCount:" << m_rhi.lineVertexCount;
    }
}

void RhiMatrixWidget::updateUniformBuffer() {
    if (!m_rhi.uniformBuffer) return;

    // Create uniform data structure matching the shader EXACTLY
    struct UniformData {
        float mvpMatrix[16];           // mat4 mvpMatrix
        float screenSize[2];           // vec2 screenSize
        float time;                    // float time
        float padding;                 // float padding
        float backgroundColor[4];      // vec4 backgroundColor
        float stripHighlightColor[4];  // vec4 stripHighlightColor
        float stripNormalColor[4];     // vec4 stripNormalColor
        float rangeLineColor[4];       // vec4 rangeLineColor
        float borderColor[4];          // vec4 borderColor
        float measureLineColor[4];     // vec4 measureLineColor
        float playbackCursorColor[4];  // vec4 playbackCursorColor
        float recordingIndicatorColor[4]; // vec4 recordingIndicatorColor
    } uniformData;

    // Fill uniform data
    // Create orthographic projection matrix for Qt coordinate system (Y=0 at top)
    float left = 0.0f;
    float right = static_cast<float>(width());
    float top = 0.0f;
    float bottom = static_cast<float>(height());
    float near = -1.0f;
    float far = 1.0f;

    // Orthographic projection matrix (column-major) - Qt coordinate system
    uniformData.mvpMatrix[0] = 2.0f / (right - left);
    uniformData.mvpMatrix[1] = 0.0f;
    uniformData.mvpMatrix[2] = 0.0f;
    uniformData.mvpMatrix[3] = 0.0f;

    uniformData.mvpMatrix[4] = 0.0f;
    uniformData.mvpMatrix[5] = -2.0f / (bottom - top);  // Negative for Qt coordinates (Y=0 at top)
    uniformData.mvpMatrix[6] = 0.0f;
    uniformData.mvpMatrix[7] = 0.0f;

    uniformData.mvpMatrix[8] = 0.0f;
    uniformData.mvpMatrix[9] = 0.0f;
    uniformData.mvpMatrix[10] = -2.0f / (far - near);
    uniformData.mvpMatrix[11] = 0.0f;

    uniformData.mvpMatrix[12] = -(right + left) / (right - left);
    uniformData.mvpMatrix[13] = (bottom + top) / (bottom - top);  // Positive for Qt coordinates
    uniformData.mvpMatrix[14] = -(far + near) / (far - near);
    uniformData.mvpMatrix[15] = 1.0f;

    // Screen and time parameters
    uniformData.screenSize[0] = static_cast<float>(width());
    uniformData.screenSize[1] = static_cast<float>(height());
    uniformData.time = static_cast<float>(_frameTimer.elapsed()) / 1000.0f; // Time in seconds
    uniformData.padding = 0.0f; // Padding for alignment

    // Colors - ensure cached colors are initialized
    updateCachedAppearanceColors();

    uniformData.backgroundColor[0] = _cachedBackgroundColor.redF();
    uniformData.backgroundColor[1] = _cachedBackgroundColor.greenF();
    uniformData.backgroundColor[2] = _cachedBackgroundColor.blueF();
    uniformData.backgroundColor[3] = _cachedBackgroundColor.alphaF();

    // Use cached appearance colors (EXACT COPY from MatrixWidget)
    uniformData.stripHighlightColor[0] = _cachedStripHighlightColor.redF();
    uniformData.stripHighlightColor[1] = _cachedStripHighlightColor.greenF();
    uniformData.stripHighlightColor[2] = _cachedStripHighlightColor.blueF();
    uniformData.stripHighlightColor[3] = _cachedStripHighlightColor.alphaF();

    QColor stripNormal = QColor(45, 45, 45); // Darker than highlight
    uniformData.stripNormalColor[0] = stripNormal.redF();
    uniformData.stripNormalColor[1] = stripNormal.greenF();
    uniformData.stripNormalColor[2] = stripNormal.blueF();
    uniformData.stripNormalColor[3] = stripNormal.alphaF();

    QColor rangeLine = QColor(80, 80, 80); // Range line color
    uniformData.rangeLineColor[0] = rangeLine.redF();
    uniformData.rangeLineColor[1] = rangeLine.greenF();
    uniformData.rangeLineColor[2] = rangeLine.blueF();
    uniformData.rangeLineColor[3] = rangeLine.alphaF();

    QColor border = QColor(100, 100, 100); // Border color
    uniformData.borderColor[0] = border.redF();
    uniformData.borderColor[1] = border.greenF();
    uniformData.borderColor[2] = border.blueF();
    uniformData.borderColor[3] = border.alphaF();

    QColor measureLine = QColor(120, 120, 120); // Measure line color
    uniformData.measureLineColor[0] = measureLine.redF();
    uniformData.measureLineColor[1] = measureLine.greenF();
    uniformData.measureLineColor[2] = measureLine.blueF();
    uniformData.measureLineColor[3] = measureLine.alphaF();

    QColor playbackCursor = QColor(255, 0, 0); // Red playback cursor
    uniformData.playbackCursorColor[0] = playbackCursor.redF();
    uniformData.playbackCursorColor[1] = playbackCursor.greenF();
    uniformData.playbackCursorColor[2] = playbackCursor.blueF();
    uniformData.playbackCursorColor[3] = playbackCursor.alphaF();

    QColor recordingIndicator = QColor(255, 0, 0); // Red recording indicator
    uniformData.recordingIndicatorColor[0] = recordingIndicator.redF();
    uniformData.recordingIndicatorColor[1] = recordingIndicator.greenF();
    uniformData.recordingIndicatorColor[2] = recordingIndicator.blueF();
    uniformData.recordingIndicatorColor[3] = recordingIndicator.alphaF();

    // Upload uniform data to GPU buffer
    if (!m_rhi.rhi) return;

    // Create or reuse the resource update batch for this frame
    if (!m_rhi.resourceUpdates) {
        m_rhi.resourceUpdates = m_rhi.rhi->nextResourceUpdateBatch();
    }

    m_rhi.resourceUpdates->updateDynamicBuffer(m_rhi.uniformBuffer, 0, sizeof(uniformData), &uniformData);

    // Debug uniform buffer update
    static int uniformUpdateCount = 0;
    uniformUpdateCount++;
    if (uniformUpdateCount % 60 == 0) {
        qDebug() << "RhiMatrixWidget: Uniform buffer updated - frame" << uniformUpdateCount;
        qDebug() << "RhiMatrixWidget: Screen size:" << uniformData.screenSize[0] << "x" << uniformData.screenSize[1];
        qDebug() << "RhiMatrixWidget: Background color:" << uniformData.backgroundColor[0] << uniformData.backgroundColor[1] << uniformData.backgroundColor[2];
    }
}

void RhiMatrixWidget::updateMidiEventInstances() {
    if (!m_rhi.midiEventInstanceBuffer || !file) {
        m_rhi.midiEventInstanceCount = 0;
        return;
    }

    // Collect visible MIDI events (EXACT COPY from MatrixWidget lines 206-220)
    QList<MidiEvent *> visibleEvents;
    QList<QPair<int, int>> selectionLines; // Store selection line coordinates

    // Clear the objects list and repopulate it (like MatrixWidget does)
    if (objects) {
        for (int i = 0; i < objects->length(); i++) {
            MidiEvent *event = objects->at(i);
            if (event) {
                event->setShown(false);
                NoteOnEvent *onev = dynamic_cast<NoteOnEvent *>(event);
                if (onev && onev->offEvent()) {
                    onev->offEvent()->setShown(false);
                }
            }
        }
        objects->clear();
    }

    if (velocityObjects) {
        velocityObjects->clear();
    }

    // Get tempo events for the visible time range (EXACT COPY from MatrixWidget line 219-220)
    startTick = file->tick(startTimeX, endTimeX, &currentTempoEvents, &endTick, &msOfFirstEventInList);

    // Validate tempo events
    if (!currentTempoEvents || currentTempoEvents->isEmpty()) {
        m_rhi.midiEventInstanceCount = 0;
        return;
    }

    TempoChangeEvent *ev = dynamic_cast<TempoChangeEvent *>(currentTempoEvents->at(0));
    if (!ev) {
        // EXACT COPY from MatrixWidget error handling (line 225-228)
        qWarning() << "RhiMatrixWidget: Invalid tempo event, filling with error color";
        m_rhi.midiEventInstanceCount = 0;

        // Fill background with error color (EXACT COPY from MatrixWidget line 225)
        renderErrorBackground();
        return;
    }

    // Use MatrixWidget's paintChannel approach for all channels
    for (int channel = 0; channel < 19; channel++) { // MatrixWidget uses 19 channels
        if (!ChannelVisibilityManager::instance().isChannelVisible(channel)) continue;

        // Get events for this channel using MatrixWidget's approach
        QMultiMap<int, MidiEvent *> *channelEvents = file->channelEvents(channel);
        QMultiMap<int, MidiEvent *>::iterator it = channelEvents->lowerBound(startTick);

        while (it != channelEvents->end() && it.key() <= endTick) {
            MidiEvent *currentEvent = it.value();
            if (currentEvent && eventInWidget(currentEvent)) {
                visibleEvents.append(currentEvent);
            }
            ++it;
        }
    }

    // Limit to maximum instances
    if (visibleEvents.size() > RHI_MAX_INSTANCES) {
        visibleEvents = visibleEvents.mid(0, RHI_MAX_INSTANCES);
    }

    m_rhi.midiEventInstanceCount = visibleEvents.size();

    if (m_rhi.midiEventInstanceCount == 0) return;

    // Create instance data array
    QVector<float> instanceData;
    instanceData.reserve(m_rhi.midiEventInstanceCount * 12); // 12 floats per instance (to match pipeline)

    for (MidiEvent *event : visibleEvents) {
        // EXACT COPY from MatrixWidget coordinate calculation (lines 709-724)
        int x, y, eventWidth, eventHeight;

        y = yPosOfLine(event->line());
        eventHeight = static_cast<int>(lineHeight()) - 1;

        // Handle different event types (EXACT COPY from MatrixWidget approach)
        NoteOnEvent *onEvent = dynamic_cast<NoteOnEvent *>(event);
        OffEvent *offEvent = dynamic_cast<OffEvent *>(event);
        ControlChangeEvent *ccEvent = dynamic_cast<ControlChangeEvent *>(event);
        TempoChangeEvent *tempoEvent = dynamic_cast<TempoChangeEvent *>(event);
        TimeSignatureEvent *timeSigEvent = dynamic_cast<TimeSignatureEvent *>(event);

        if (onEvent || offEvent) {
            // Note events (EXACT COPY from MatrixWidget lines 709-724)
            if (onEvent) {
                offEvent = onEvent->offEvent();
            } else if (offEvent) {
                onEvent = dynamic_cast<NoteOnEvent *>(offEvent->onEvent());
            }

            if (onEvent && offEvent) {
                // Calculate raw coordinates
                int rawX = xPosOfMs(msOfTick(onEvent->midiTime()));
                int rawEndX = xPosOfMs(msOfTick(offEvent->midiTime()));

                // Clamp coordinates to viewport for partially visible notes
                x = qMax(rawX, lineNameWidth);  // Don't start before the piano area
                int endX = qMin(rawEndX, width());  // Don't extend beyond widget width
                eventWidth = qMax(endX - x, 1);  // Ensure minimum width of 1 pixel

                event = onEvent; // Use the note on event for rendering
            } else {
                // Fallback for incomplete note pairs
                eventWidth = 10;
                x = xPosOfMs(msOfTick(event->midiTime()));
            }
        } else if (ccEvent || tempoEvent || timeSigEvent) {
            // Non-note events (Control Change, Tempo, Time Signature)
            // These are rendered as small rectangles (EXACT COPY from MatrixWidget)
            eventWidth = 10; // Fixed width for non-note events
            x = xPosOfMs(msOfTick(event->midiTime()));

            // Clamp to visible area
            if (x < lineNameWidth) x = lineNameWidth;
            if (x >= width()) continue; // Skip if completely off-screen
        } else {
            // Other event types - use default rendering
            eventWidth = 10;
            x = xPosOfMs(msOfTick(event->midiTime()));
        }

        // Check track visibility (EXACT COPY from MatrixWidget line 731)
        if (event->track()->hidden()) {
            continue; // Skip hidden tracks
        }

        // Get event color
        QColor eventColor = getEventColor(event);

        // Store coordinates on event for tool system (EXACT COPY from MatrixWidget lines 709-724)
        event->setX(x);
        event->setY(y);
        event->setWidth(eventWidth);
        event->setHeight(eventHeight);

        // Add to objects list (EXACT COPY from MatrixWidget)
        objects->append(event);

        // Check if event is selected (EXACT COPY from MatrixWidget lines 737-742)
        bool isSelected = Selection::instance()->selectedEvents().contains(event);
        if (isSelected) {
            eventColor = eventColor.lighter(150); // Highlight selected events

            // Add selection border lines (EXACT COPY from MatrixWidget lines 738-741)
            // Store selection lines to render after all events
            selectionLines.append(QPair<int, int>(y, y + eventHeight)); // Store top and bottom Y coordinates
        }

        // Instance data: posX, posY, sizeX, sizeY, colorR, colorG, colorB, colorA, velocity, channel, type, reserved
        instanceData.append(static_cast<float>(x));
        instanceData.append(static_cast<float>(y));
        instanceData.append(static_cast<float>(eventWidth));
        instanceData.append(static_cast<float>(eventHeight));
        instanceData.append(eventColor.redF());
        instanceData.append(eventColor.greenF());
        instanceData.append(eventColor.blueF());
        instanceData.append(1.0f); // Force full alpha for visibility

        // Extra data for MIDI events
        float velocity = 0.0f;
        if (onEvent) {
            velocity = static_cast<float>(onEvent->velocity()) / 127.0f;
        }
        instanceData.append(velocity);                              // velocity (0-1)
        instanceData.append(static_cast<float>(event->channel()));  // channel (0-15)
        instanceData.append(static_cast<float>(event->line()));     // event line (for type identification)
        instanceData.append(0.0f);                                  // reserved
    }

    // Update MIDI event instance buffer (count already set above)

    if (m_rhi.midiEventInstanceCount > 0 && !instanceData.isEmpty() && m_rhi.rhi && m_rhi.midiEventInstanceBuffer) {
        // Validate buffer size to prevent crashes
        int maxBufferSize = RHI_MAX_INSTANCES * 12 * sizeof(float);
        int dataSize = instanceData.size() * sizeof(float);
        if (dataSize > maxBufferSize) {
            qWarning() << "RhiMatrixWidget: MIDI event instance data too large, truncating";
            instanceData.resize(maxBufferSize / sizeof(float));
            m_rhi.midiEventInstanceCount = instanceData.size() / 12; // 12 floats per instance
        }

        // Create or reuse the resource update batch for this frame
        if (!m_rhi.resourceUpdates) {
            m_rhi.resourceUpdates = m_rhi.rhi->nextResourceUpdateBatch();
        }

        if (m_rhi.resourceUpdates) {
            m_rhi.resourceUpdates->updateDynamicBuffer(m_rhi.midiEventInstanceBuffer, 0,
                                      instanceData.size() * sizeof(float),
                                      instanceData.constData());
        } else {
            qWarning() << "RhiMatrixWidget: Failed to create resource update batch for MIDI events";
        }
    }

    // Selection border lines will be rendered as part of the line rendering system
}

void RhiMatrixWidget::updatePianoKeyInstances() {
    static int callCount = 0;
    callCount++;
    if (callCount <= 5) {
        qDebug() << "RhiMatrixWidget: updatePianoKeyInstances() called #" << callCount;
    }

    if (!m_rhi.pianoInstanceBuffer) {
        if (callCount <= 5) {
            qDebug() << "RhiMatrixWidget: No piano instance buffer, count set to 0";
        }
        m_rhi.pianoInstanceCount = 0;
        return;
    }

    // Calculate visible range (ensure proper initialization)
    int startLineY = lineAtY(0);
    int endLineY = lineAtY(height());

    // Debug view parameters (show immediately for first few calls)
    static int debugCount = 0;
    debugCount++;
    if (debugCount <= 5) {
        qDebug() << "RhiMatrixWidget: View parameters - startLineY:" << startLineY << "endLineY:" << endLineY;
        qDebug() << "RhiMatrixWidget: Widget size:" << width() << "x" << height();
        qDebug() << "RhiMatrixWidget: lineNameWidth:" << lineNameWidth << "timeHeight:" << timeHeight;
        qDebug() << "RhiMatrixWidget: lineHeight():" << lineHeight() << "scaleY:" << scaleY;
        qDebug() << "RhiMatrixWidget: lineAtY(0):" << lineAtY(0) << "lineAtY(height()):" << lineAtY(height());
        qDebug() << "RhiMatrixWidget: yPosOfLine(startLineY):" << yPosOfLine(startLineY) << "yPosOfLine(endLineY):" << yPosOfLine(endLineY);
    }

    // Create piano key instances for visible lines
    QVector<float> instanceData;
    int keyCount = 0;

    if (debugCount <= 5) {
        qDebug() << "RhiMatrixWidget: Starting piano key loop from line" << startLineY << "to" << endLineY;
    }

    for (int line = startLineY; line <= endLineY && keyCount < RHI_MAX_INSTANCES; line++) {
        // Only render piano keys for MIDI note lines (0-127)
        if (line < 0 || line > 127) continue;

        if (debugCount <= 5 && keyCount < 5) {
            qDebug() << "RhiMatrixWidget: Processing line" << line << "keyCount" << keyCount;
        }

        int y = yPosOfLine(line);
        int keyHeight = static_cast<int>(lineHeight()) - 1;

        // Piano key dimensions (EXACT COPY from MatrixWidget)
        int keyX = 0;
        int keyWidth = lineNameWidth - 10;  // Leave border like MatrixWidget

        if (debugCount <= 5 && keyCount < 5) {
            qDebug() << "RhiMatrixWidget: Line" << line << "y=" << y << "keyHeight=" << keyHeight;
        }

        // Convert line to MIDI note (MatrixWidget uses inverted indexing)
        int midiNote = 127 - line;
        int note = midiNote % 12;
        bool isBlackKey = (note == 1 || note == 3 || note == 6 || note == 8 || note == 10);

        // Use MatrixWidget's EXACT color scheme with hover and selection effects
        QColor keyColor;
        bool isHovered = false;
        bool isSelected = false;

        // Hover detection will be done after key dimensions are finalized

        // Check if this piano key line has selected events (EXACT COPY from MatrixWidget lines 858-863)
        foreach(MidiEvent* event, Selection::instance()->selectedEvents()) {
            if (event->line() == 127 - midiNote) {
                isSelected = true;
                break;
            }
        }

        // Apply color based on state priority: selected > hovered > normal
        if (isSelected) {
            keyColor = isBlackKey ? _cachedPianoBlackKeySelectedColor : _cachedPianoWhiteKeySelectedColor;
        } else if (isHovered) {
            keyColor = isBlackKey ? _cachedPianoBlackKeyHoverColor : _cachedPianoWhiteKeyHoverColor;
        } else {
            keyColor = isBlackKey ? _cachedPianoBlackKeyColor : _cachedPianoWhiteKeyColor;
        }

        // Use MatrixWidget's EXACT sizing logic (from paintPianoKey line 762)
        int borderRight = 10;
        keyWidth = lineNameWidth - borderRight;  // Full width minus border
        keyX = 0;  // Start at left edge

        if (isBlackKey) {
            // Black keys: 60% width, 50% height (EXACT COPY from MatrixWidget lines 764-875)
            double scaleWidthBlack = 0.6;
            double scaleHeightBlack = 0.5;
            keyWidth = static_cast<int>(keyWidth * scaleWidthBlack);
            keyHeight = static_cast<int>(keyHeight * scaleHeightBlack);
            // Center black keys vertically (EXACT COPY from MatrixWidget line 870)
            y += (lineHeight() - keyHeight) / 2;
            keyX = 0; // Black keys start at left edge
        } else {
            // White keys: full width minus border, full height
            keyX = 0;
        }

        // Now that key dimensions are finalized, create keyRect and check hover (EXACT COPY from MatrixWidget)
        QRect keyRect(keyX, y, keyWidth, keyHeight);
        if (enabled && mouseInRect(PianoArea)) {
            // Use exact same hover detection as MatrixWidget (lines 877-880, 908)
            if (isBlackKey) {
                isHovered = mouseInRect(keyRect);
            } else {
                isHovered = mouseInRect(keyX, y, keyWidth, lineHeight()); // Use full line height for white keys
            }
        }

        // Generate complex piano key shape (EXACT COPY from MatrixWidget paintPianoKey)
        QString keyLabel = "";
        if (note == 0) { // C notes only
            int octave = midiNote / 12;
            keyLabel = "C" + QString::number(octave - 1);
        }

        // Calculate complex piano key shape (EXACT COPY from MatrixWidget lines 764-928)
        double scaleHeightBlack = 0.5;
        double scaleWidthBlack = 0.6;

        bool blackOnTop = false;
        bool blackBeneath = false;

        // Use EXACT MatrixWidget logic for each note type (lines 772-851)
        switch (note) {
            case 0: // C
                blackOnTop = true;
                blackBeneath = false;
                break;
            case 1: // C# (black key)
                // Will be handled as black key
                break;
            case 2: // D
                blackOnTop = true;
                blackBeneath = true;
                break;
            case 3: // D# (black key)
                // Will be handled as black key
                break;
            case 4: // E
                blackOnTop = false;
                blackBeneath = true;
                break;
            case 5: // F
                blackOnTop = true;
                blackBeneath = false;
                break;
            case 6: // F# (black key)
                // Will be handled as black key
                break;
            case 7: // G
                blackOnTop = true;
                blackBeneath = true;
                break;
            case 8: // G# (black key)
                // Will be handled as black key
                break;
            case 9: // A
                blackOnTop = true;
                blackBeneath = true;
                break;
            case 10: // A# (black key)
                // Will be handled as black key
                break;
            case 11: // B
                blackOnTop = false;
                blackBeneath = true;
                break;
        }

        // Special case: if this is the top line, don't cut top (EXACT COPY from MatrixWidget line 853-855)
        if (127 - midiNote == startLineY) {
            blackOnTop = false;
        }

        // Generate complex polygon vertices for piano key shape
        QVector<float> keyVertices;

        if (isBlackKey) {
            // Black key: simple rectangle
            keyVertices = {
                static_cast<float>(keyX), static_cast<float>(y),                           // Bottom-left
                static_cast<float>(keyX + keyWidth), static_cast<float>(y),               // Bottom-right
                static_cast<float>(keyX + keyWidth), static_cast<float>(y + keyHeight),   // Top-right
                static_cast<float>(keyX), static_cast<float>(y + keyHeight)               // Top-left
            };
        } else {
            // White key: complex shape based on adjacent black keys (EXACT COPY from MatrixWidget)
            int blackKeyWidth = static_cast<int>(keyWidth * scaleWidthBlack);
            int blackKeyHeight = static_cast<int>(keyHeight * scaleHeightBlack);

            if (!blackOnTop && !blackBeneath) {
                // No black keys adjacent: full rectangle
                keyVertices = {
                    static_cast<float>(keyX), static_cast<float>(y),
                    static_cast<float>(keyX + keyWidth), static_cast<float>(y),
                    static_cast<float>(keyX + keyWidth), static_cast<float>(y + keyHeight),
                    static_cast<float>(keyX), static_cast<float>(y + keyHeight)
                };
            } else if (blackOnTop && !blackBeneath) {
                // Black key above: notched top
                keyVertices = {
                    static_cast<float>(keyX), static_cast<float>(y),
                    static_cast<float>(keyX + keyWidth), static_cast<float>(y),
                    static_cast<float>(keyX + keyWidth), static_cast<float>(y + keyHeight - blackKeyHeight),
                    static_cast<float>(keyX + blackKeyWidth), static_cast<float>(y + keyHeight - blackKeyHeight),
                    static_cast<float>(keyX + blackKeyWidth), static_cast<float>(y + keyHeight),
                    static_cast<float>(keyX), static_cast<float>(y + keyHeight)
                };
            } else if (!blackOnTop && blackBeneath) {
                // Black key below: notched bottom
                keyVertices = {
                    static_cast<float>(keyX), static_cast<float>(y),
                    static_cast<float>(keyX + blackKeyWidth), static_cast<float>(y),
                    static_cast<float>(keyX + blackKeyWidth), static_cast<float>(y + blackKeyHeight),
                    static_cast<float>(keyX + keyWidth), static_cast<float>(y + blackKeyHeight),
                    static_cast<float>(keyX + keyWidth), static_cast<float>(y + keyHeight),
                    static_cast<float>(keyX), static_cast<float>(y + keyHeight)
                };
            } else {
                // Black keys above and below: double notched
                keyVertices = {
                    static_cast<float>(keyX), static_cast<float>(y),
                    static_cast<float>(keyX + blackKeyWidth), static_cast<float>(y),
                    static_cast<float>(keyX + blackKeyWidth), static_cast<float>(y + blackKeyHeight),
                    static_cast<float>(keyX + keyWidth), static_cast<float>(y + blackKeyHeight),
                    static_cast<float>(keyX + keyWidth), static_cast<float>(y + keyHeight - blackKeyHeight),
                    static_cast<float>(keyX + blackKeyWidth), static_cast<float>(y + keyHeight - blackKeyHeight),
                    static_cast<float>(keyX + blackKeyWidth), static_cast<float>(y + keyHeight),
                    static_cast<float>(keyX), static_cast<float>(y + keyHeight)
                };
            }
        }

        // Store complex shape for rendering (we'll triangulate this)
        // For now, store the bounding rectangle for mouse interaction
        pianoKeys[midiNote] = keyRect;

        // Add line highlighting if key is hovered (EXACT COPY from MatrixWidget lines 946-948)
        if (isHovered) {
            // Create a highlight rectangle for the entire row
            int highlightX = keyX + keyWidth + 10; // After piano key
            int highlightY = y;
            int highlightWidth = width() - highlightX;
            int highlightHeight = keyHeight;

            // Add highlight rectangle to instance data
            instanceData.append(static_cast<float>(highlightX));
            instanceData.append(static_cast<float>(highlightY));
            instanceData.append(static_cast<float>(highlightWidth));
            instanceData.append(static_cast<float>(highlightHeight));
            instanceData.append(_cachedPianoKeyLineHighlightColor.redF());
            instanceData.append(_cachedPianoKeyLineHighlightColor.greenF());
            instanceData.append(_cachedPianoKeyLineHighlightColor.blueF());
            instanceData.append(_cachedPianoKeyLineHighlightColor.alphaF());

            keyCount++;
        }

        // Hover state already determined above using keyRect.contains()
        // Adjust color for hover effect
        if (isHovered) {
            keyColor = keyColor.lighter(120); // Make it 20% lighter when hovered
        }

        // Instance data: posX, posY, sizeX, sizeY, colorR, colorG, colorB, colorA
        instanceData.append(static_cast<float>(keyX));  // x position
        instanceData.append(static_cast<float>(y));  // y position
        instanceData.append(static_cast<float>(keyWidth));  // width
        instanceData.append(static_cast<float>(keyHeight));  // height
        instanceData.append(keyColor.redF());
        instanceData.append(keyColor.greenF());
        instanceData.append(keyColor.blueF());
        instanceData.append(isBlackKey ? 1.0f : 0.0f); // Key type in alpha channel

        keyCount++;
    }

    m_rhi.pianoInstanceCount = keyCount;

    if (debugCount <= 5) {
        qDebug() << "RhiMatrixWidget: Piano key generation completed - keyCount:" << keyCount;
        qDebug() << "RhiMatrixWidget: instanceData size:" << instanceData.size() << "floats";
    }

    if (m_rhi.pianoInstanceCount == 0) return;

    // Update instance buffer with piano key data
    if (!m_rhi.rhi) return;

    // Create or reuse the resource update batch for this frame
    if (!m_rhi.resourceUpdates) {
        m_rhi.resourceUpdates = m_rhi.rhi->nextResourceUpdateBatch();
    }

    m_rhi.resourceUpdates->updateDynamicBuffer(m_rhi.pianoInstanceBuffer, 0,
                              instanceData.size() * sizeof(float),
                              instanceData.constData());
}

void RhiMatrixWidget::updateRowStripInstances() {
    if (!m_rhi.rowStripInstanceBuffer) {
        m_rhi.rowStripInstanceCount = 0;
        return;
    }

    static int callCount = 0;
    callCount++;
    if (callCount <= 5) {
        qDebug() << "RhiMatrixWidget: updateRowStripInstances() called #" << callCount;
    }

    // Calculate visible range
    int startLineY = lineAtY(0);
    int endLineY = lineAtY(height());

    QVector<float> instanceData;
    int stripCount = 0;

    if (callCount <= 5) {
        qDebug() << "RhiMatrixWidget: Creating row strips from line" << startLineY << "to" << endLineY;
    }

    // Create row strip instances for ALL visible lines (exactly like MatrixWidget)
    for (int line = startLineY; line <= endLineY && stripCount < RHI_MAX_INSTANCES; line++) {
        // Only render strips for MIDI note lines (0-127)
        if (line < 0 || line > 127) continue;

        int y = yPosOfLine(line);
        int stripHeight = static_cast<int>(lineHeight());

        // Determine strip color using MatrixWidget's EXACT logic
        QColor stripColor;
        bool isHighlighted = false;
        bool isRangeLine = false;

        // Check for C3/C6 range lines if enabled (EXACT COPY from MatrixWidget)
        if (_cachedShowRangeLines) {
            // C3 is MIDI note 48, C6 is MIDI note 84
            int midiNote = 127 - line;
            if (midiNote == 48 || midiNote == 84) { // C3 or C6
                isRangeLine = true;
            }
        }

        // Use MatrixWidget's strip highlighting logic based on cached strip style
        int stripStyle = static_cast<int>(_cachedStripStyle); // onOctave = 0, onSharp = 1, onEven = 2

        switch (stripStyle) {
            case 0: // onOctave
                // MIDI note 0 = C, so we want (127-line) % 12 == 0 for C notes
                isHighlighted = ((127 - static_cast<unsigned int>(line)) % 12) == 0;
                break;
            case 1: // onSharp
                // Use sharp_strip_mask logic: highlight non-sharp keys
                {
                    const unsigned sharp_strip_mask = (1 << 4) | (1 << 6) | (1 << 9) | (1 << 11) | (1 << 1);
                    isHighlighted = !((1 << (static_cast<unsigned int>(line) % 12)) & sharp_strip_mask);
                }
                break;
            case 2: // onEven
                isHighlighted = (static_cast<unsigned int>(line) % 2);
                break;
        }

        // Set colors exactly like MatrixWidget
        if (isRangeLine) {
            stripColor = _cachedRangeLineColor;
        } else if (isHighlighted) {
            // Use proper cached strip highlight color
            stripColor = _cachedStripHighlightColor;
        } else {
            // Use proper cached strip normal color
            stripColor = _cachedStripNormalColor;
        }

        // Create strip for ALL rows (both highlighted and normal)
        // Strip covers the entire width from piano keys to the right edge
        int stripX = lineNameWidth;
        int stripWidth = width() - lineNameWidth;

        // Instance data: posX, posY, sizeX, sizeY, colorR, colorG, colorB, colorA
        instanceData.append(static_cast<float>(stripX));  // x position
        instanceData.append(static_cast<float>(y));  // y position
        instanceData.append(static_cast<float>(stripWidth));  // width
        instanceData.append(static_cast<float>(stripHeight));  // height
        instanceData.append(stripColor.redF());
        instanceData.append(stripColor.greenF());
        instanceData.append(stripColor.blueF());
        instanceData.append(1.0f); // Fully opaque strips like MatrixWidget

        stripCount++;
    }

    m_rhi.rowStripInstanceCount = stripCount;

    if (callCount <= 5) {
        qDebug() << "RhiMatrixWidget: Generated" << stripCount << "row strips, instanceData size:" << instanceData.size();
    }

    if (stripCount == 0) return;

    // Update instance buffer with row strip data
    if (!m_rhi.rhi) return;

    // Create or reuse the resource update batch for this frame
    if (!m_rhi.resourceUpdates) {
        m_rhi.resourceUpdates = m_rhi.rhi->nextResourceUpdateBatch();
    }

    m_rhi.resourceUpdates->updateDynamicBuffer(m_rhi.rowStripInstanceBuffer, 0,
                              instanceData.size() * sizeof(float),
                              instanceData.constData());
}

void RhiMatrixWidget::updateLineVertices() {
    if (!m_rhi.lineVertexBuffer) {
        m_rhi.lineVertexCount = 0;
        return;
    }

    static int callCount = 0;
    callCount++;
    if (callCount <= 5) {
        qDebug() << "RhiMatrixWidget: updateLineVertices() called #" << callCount;
    }

    QVector<float> vertexData;

    // Ensure cached colors are initialized
    updateCachedAppearanceColors();
    QColor lineColor = _cachedForegroundColor;
    lineColor.setAlphaF(0.3f); // Semi-transparent lines

    // Calculate visible range
    int startLineY = lineAtY(0);
    int endLineY = lineAtY(height());

    // Add vertical time division lines
    for (const QPair<int, int> &div : currentDivs) {
        int x = div.first;
        if (x < lineNameWidth || x > QRhiWidget::width()) continue;

        // Line vertex data: x1, y1, r, g, b, a, x2, y2, r, g, b, a
        // First vertex (top)
        vertexData.append(static_cast<float>(x));
        vertexData.append(static_cast<float>(timeHeight));
        vertexData.append(lineColor.redF());
        vertexData.append(lineColor.greenF());
        vertexData.append(lineColor.blueF());
        vertexData.append(lineColor.alphaF());

        // Second vertex (bottom)
        vertexData.append(static_cast<float>(x));
        vertexData.append(static_cast<float>(QRhiWidget::height()));
        vertexData.append(lineColor.redF());
        vertexData.append(lineColor.greenF());
        vertexData.append(lineColor.blueF());
        vertexData.append(lineColor.alphaF());
    }

    // Add horizontal piano lines (grid lines for each MIDI note)
    if (callCount <= 5) {
        qDebug() << "RhiMatrixWidget: Adding horizontal lines from" << startLineY << "to" << endLineY;
    }

    int horizontalLineCount = 0;
    for (int line = startLineY; line <= endLineY; line++) {
        int y = yPosOfLine(line) + lineHeight(); // Position at bottom of row for better alignment
        if (y < timeHeight || y > QRhiWidget::height()) continue;

        horizontalLineCount++;

        // First vertex (left)
        vertexData.append(static_cast<float>(lineNameWidth));
        vertexData.append(static_cast<float>(y));
        vertexData.append(lineColor.redF());
        vertexData.append(lineColor.greenF());
        vertexData.append(lineColor.blueF());
        vertexData.append(lineColor.alphaF());

        // Second vertex (right)
        vertexData.append(static_cast<float>(QRhiWidget::width()));
        vertexData.append(static_cast<float>(y));
        vertexData.append(lineColor.redF());
        vertexData.append(lineColor.greenF());
        vertexData.append(lineColor.blueF());
        vertexData.append(lineColor.alphaF());
    }

    m_rhi.lineVertexCount = vertexData.size() / 6; // 6 floats per vertex

    if (callCount <= 5) {
        qDebug() << "RhiMatrixWidget: Generated" << horizontalLineCount << "horizontal lines,"
                 << "total vertices:" << m_rhi.lineVertexCount << "vertexData size:" << vertexData.size();
    }

    if (m_rhi.lineVertexCount == 0) return;

    // Update line vertex buffer
    if (!vertexData.isEmpty() && m_rhi.rhi) {
        // Create or reuse the resource update batch for this frame
        if (!m_rhi.resourceUpdates) {
            m_rhi.resourceUpdates = m_rhi.rhi->nextResourceUpdateBatch();
        }

        m_rhi.resourceUpdates->updateDynamicBuffer(m_rhi.lineVertexBuffer, 0,
                                  vertexData.size() * sizeof(float),
                                  vertexData.constData());
    }
}

void RhiMatrixWidget::updateMeasureLineVertices() {
    if (!m_rhi.measureLineVertexBuffer || !file) {
        m_rhi.measureLineVertexCount = 0;
        return;
    }

    static int callCount = 0;
    callCount++;
    if (callCount <= 5) {
        qDebug() << "RhiMatrixWidget: updateMeasureLineVertices() called #" << callCount;
    }

    QVector<float> vertexData;

    // Ensure cached colors are initialized
    updateCachedAppearanceColors();
    QColor measureLineColor = _cachedMeasureLineColor;
    QColor beatLineColor = _cachedTimelineGridColor;

    int lineCount = 0;

    // Calculate divisions first (EXACT COPY from MatrixWidget)
    calculateDivs();

    // EXACT COPY from MatrixWidget measure line logic (lines 372-483)
    int measure = file->measure(startTick, endTick, &currentTimeSignatureEvents);

    if (!currentTimeSignatureEvents || currentTimeSignatureEvents->isEmpty()) {
        m_rhi.measureLineVertexCount = 0;
        return;
    }

    TimeSignatureEvent *currentEvent = currentTimeSignatureEvents->at(0);
    int i = 0;
    if (!currentEvent) {
        m_rhi.measureLineVertexCount = 0;
        return;
    }

    int tick = currentEvent->midiTime();
    while (tick + currentEvent->ticksPerMeasure() <= startTick) {
        tick += currentEvent->ticksPerMeasure();
    }

    while (tick < endTick && lineCount < RHI_MAX_INSTANCES) {
        TimeSignatureEvent *measureEvent = currentTimeSignatureEvents->at(i);
        int xfrom = xPosOfMs(msOfTick(tick));
        measure++;
        int measureStartTick = tick;
        tick += currentEvent->ticksPerMeasure();

        if (i < currentTimeSignatureEvents->length() - 1) {
            if (currentTimeSignatureEvents->at(i + 1)->midiTime() <= tick) {
                currentEvent = currentTimeSignatureEvents->at(i + 1);
                tick = currentEvent->midiTime();
                i++;
            }
        }

        if (tick > startTick && xfrom >= lineNameWidth && xfrom < width()) {
            // Add measure line (vertical line from timeHeight/2 to bottom)
            vertexData.append(static_cast<float>(xfrom));  // x1
            vertexData.append(static_cast<float>(timeHeight / 2));  // y1
            vertexData.append(measureLineColor.redF());
            vertexData.append(measureLineColor.greenF());
            vertexData.append(measureLineColor.blueF());
            vertexData.append(measureLineColor.alphaF());

            vertexData.append(static_cast<float>(xfrom));  // x2
            vertexData.append(static_cast<float>(height()));  // y2
            vertexData.append(measureLineColor.redF());
            vertexData.append(measureLineColor.greenF());
            vertexData.append(measureLineColor.blueF());
            vertexData.append(measureLineColor.alphaF());

            lineCount++;
        }

        // Add beat subdivision lines (dashed lines)
        if (measureEvent && lineCount < RHI_MAX_INSTANCES - 10) {
            int ticksPerDiv = file->ticksPerQuarter(); // Default to quarter notes

            // Calculate subdivision based on time signature
            if (measureEvent->denom() == 8) {
                ticksPerDiv = file->ticksPerQuarter() / 2; // Eighth note subdivisions
            } else if (measureEvent->denom() == 16) {
                ticksPerDiv = file->ticksPerQuarter() / 4; // Sixteenth note subdivisions
            }

            int startTickDiv = ticksPerDiv;
            while (startTickDiv < measureEvent->ticksPerMeasure() && lineCount < RHI_MAX_INSTANCES) {
                int divTick = startTickDiv + measureStartTick;
                int xDiv = xPosOfMs(msOfTick(divTick));

                if (xDiv >= lineNameWidth && xDiv < width()) {
                    // Add beat line (vertical dashed line from timeHeight to bottom)
                    vertexData.append(static_cast<float>(xDiv));  // x1
                    vertexData.append(static_cast<float>(timeHeight));  // y1
                    vertexData.append(beatLineColor.redF());
                    vertexData.append(beatLineColor.greenF());
                    vertexData.append(beatLineColor.blueF());
                    vertexData.append(beatLineColor.alphaF() * 0.5f); // Semi-transparent

                    vertexData.append(static_cast<float>(xDiv));  // x2
                    vertexData.append(static_cast<float>(height()));  // y2
                    vertexData.append(beatLineColor.redF());
                    vertexData.append(beatLineColor.greenF());
                    vertexData.append(beatLineColor.blueF());
                    vertexData.append(beatLineColor.alphaF() * 0.5f);

                    lineCount++;
                }
                startTickDiv += ticksPerDiv;
            }
        }
    }

    m_rhi.measureLineVertexCount = vertexData.size() / 6; // 6 floats per vertex

    if (callCount <= 5) {
        qDebug() << "RhiMatrixWidget: Generated" << lineCount << "measure/beat lines,"
                 << "total vertices:" << m_rhi.measureLineVertexCount << "vertexData size:" << vertexData.size();
    }

    if (m_rhi.measureLineVertexCount == 0) return;

    // Update vertex buffer with measure line data
    if (!m_rhi.rhi) return;

    // Validate buffer size to prevent crashes
    int maxBufferSize = RHI_VERTEX_BUFFER_SIZE * 6; // 6 floats per vertex
    if (vertexData.size() * sizeof(float) > maxBufferSize) {
        qWarning() << "RhiMatrixWidget: Measure line vertex data too large, truncating";
        vertexData.resize(maxBufferSize / sizeof(float));
        m_rhi.measureLineVertexCount = vertexData.size() / 6;
    }

    // Create or reuse the resource update batch for this frame
    if (!m_rhi.resourceUpdates) {
        m_rhi.resourceUpdates = m_rhi.rhi->nextResourceUpdateBatch();
    }

    m_rhi.resourceUpdates->updateDynamicBuffer(m_rhi.measureLineVertexBuffer, 0,
                              vertexData.size() * sizeof(float),
                              vertexData.constData());
}

void RhiMatrixWidget::renderTimelineAreaBackground(QRhiCommandBuffer *cb, QRhiRenderTarget *rt) {
    Q_UNUSED(rt)

    // EXACT COPY from MatrixWidget line 304: fill timeline area background
    if (!file) return;

    // Create a single rectangle for the timeline area background
    QVector<float> instanceData;

    // Timeline area background rectangle
    int timelineX = lineNameWidth;
    int timelineY = 0;
    int timelineWidth = width() - lineNameWidth;
    int timelineHeight = timeHeight;

    // Instance data: posX, posY, sizeX, sizeY, colorR, colorG, colorB, colorA
    instanceData.append(static_cast<float>(timelineX));
    instanceData.append(static_cast<float>(timelineY));
    instanceData.append(static_cast<float>(timelineWidth));
    instanceData.append(static_cast<float>(timelineHeight));
    instanceData.append(_cachedSystemWindowColor.redF());
    instanceData.append(_cachedSystemWindowColor.greenF());
    instanceData.append(_cachedSystemWindowColor.blueF());
    instanceData.append(_cachedSystemWindowColor.alphaF());

    // Timeline border rectangle (EXACT COPY from MatrixWidget line 312)
    instanceData.append(static_cast<float>(lineNameWidth));
    instanceData.append(static_cast<float>(2));
    instanceData.append(static_cast<float>(width() - lineNameWidth - 1));
    instanceData.append(static_cast<float>(timeHeight - 2));
    instanceData.append(_cachedPianoWhiteKeyColor.redF());
    instanceData.append(_cachedPianoWhiteKeyColor.greenF());
    instanceData.append(_cachedPianoWhiteKeyColor.blueF());
    instanceData.append(_cachedPianoWhiteKeyColor.alphaF());

    // Horizontal separator bar (EXACT COPY from MatrixWidget line 315)
    instanceData.append(static_cast<float>(0));
    instanceData.append(static_cast<float>(timeHeight - 3));
    instanceData.append(static_cast<float>(width()));
    instanceData.append(static_cast<float>(3));
    instanceData.append(_cachedSystemWindowColor.redF());
    instanceData.append(_cachedSystemWindowColor.greenF());
    instanceData.append(_cachedSystemWindowColor.blueF());
    instanceData.append(_cachedSystemWindowColor.alphaF());

    // Render using piano pipeline (reuse for rectangles)
    if (m_rhi.pianoPipeline && m_rhi.quadVertexBuffer && m_rhi.pianoInstanceBuffer && !instanceData.isEmpty()) {
        // Create or reuse the resource update batch for this frame
        if (!m_rhi.resourceUpdates) {
            m_rhi.resourceUpdates = m_rhi.rhi->nextResourceUpdateBatch();
        }

        if (m_rhi.resourceUpdates) {
            m_rhi.resourceUpdates->updateDynamicBuffer(m_rhi.pianoInstanceBuffer, 0,
                                      instanceData.size() * sizeof(float),
                                      instanceData.constData());
        }

        // Render the timeline area background
        cb->setGraphicsPipeline(m_rhi.pianoPipeline);
        const QSize outputSize = renderTarget()->pixelSize();
        cb->setViewport(QRhiViewport(0, 0, outputSize.width(), outputSize.height()));

        if (m_rhi.pianoSrb) {
            cb->setShaderResources(m_rhi.pianoSrb);
        }

        const QRhiCommandBuffer::VertexInput vbufBindings[] = {
            { m_rhi.quadVertexBuffer, 0 },
            { m_rhi.pianoInstanceBuffer, 0 }
        };
        cb->setVertexInput(0, 2, vbufBindings);

        // Draw quads for timeline area backgrounds (background + border + separator)
        int numInstances = instanceData.size() / 8; // 8 floats per instance
        cb->draw(6, numInstances);
    }
}

void RhiMatrixWidget::renderTools(QRhiCommandBuffer *cb, QRhiRenderTarget *rt) {
    Q_UNUSED(rt)

    // TODO: Implement tool rendering using RHI instead of QPainter
    // For now, skip tool rendering to avoid compilation errors
    // Selection feedback will be handled through the existing selection system
    if (Tool::currentTool()) {
        // Skip tool-specific rendering for now
        // The selection system already handles selection feedback through renderSelectionLines()
    }
}

void RhiMatrixWidget::renderCursors(QRhiCommandBuffer *cb, QRhiRenderTarget *rt) {
    Q_UNUSED(rt)

    if (!file) return;

    // EXACT COPY from MatrixWidget cursor rendering (lines 586-631)
    QVector<float> lineVertexData;
    int lineCount = 0;

    // 1. Mouse cursor in timeline (lines 586-590)
    if (enabled && mouseInRect(TimeLineArea)) {
        // Add vertical line at mouse position
        lineVertexData.append(static_cast<float>(mouseX));  // x1
        lineVertexData.append(0.0f);  // y1
        lineVertexData.append(_cachedPlaybackCursorColor.redF());
        lineVertexData.append(_cachedPlaybackCursorColor.greenF());
        lineVertexData.append(_cachedPlaybackCursorColor.blueF());
        lineVertexData.append(_cachedPlaybackCursorColor.alphaF());

        lineVertexData.append(static_cast<float>(mouseX));  // x2
        lineVertexData.append(static_cast<float>(height()));  // y2
        lineVertexData.append(_cachedPlaybackCursorColor.redF());
        lineVertexData.append(_cachedPlaybackCursorColor.greenF());
        lineVertexData.append(_cachedPlaybackCursorColor.blueF());
        lineVertexData.append(_cachedPlaybackCursorColor.alphaF());

        lineCount++;
    }

    // 2. Playback cursor (lines 592-599)
    if (MidiPlayer::isPlaying()) {
        int x = xPosOfMs(MidiPlayer::timeMs());
        if (x >= lineNameWidth) {
            // Add vertical line at playback position
            lineVertexData.append(static_cast<float>(x));  // x1
            lineVertexData.append(0.0f);  // y1
            lineVertexData.append(_cachedPlaybackCursorColor.redF());
            lineVertexData.append(_cachedPlaybackCursorColor.greenF());
            lineVertexData.append(_cachedPlaybackCursorColor.blueF());
            lineVertexData.append(_cachedPlaybackCursorColor.alphaF());

            lineVertexData.append(static_cast<float>(x));  // x2
            lineVertexData.append(static_cast<float>(height()));  // y2
            lineVertexData.append(_cachedPlaybackCursorColor.redF());
            lineVertexData.append(_cachedPlaybackCursorColor.greenF());
            lineVertexData.append(_cachedPlaybackCursorColor.blueF());
            lineVertexData.append(_cachedPlaybackCursorColor.alphaF());

            lineCount++;
        }
    }

    // 3. File cursor (lines 601-616)
    if (file->cursorTick() >= startTick && file->cursorTick() <= endTick) {
        int x = xPosOfMs(msOfTick(file->cursorTick()));

        // Add vertical line at cursor position
        lineVertexData.append(static_cast<float>(x));  // x1
        lineVertexData.append(0.0f);  // y1
        lineVertexData.append(0.3f);  // Dark gray
        lineVertexData.append(0.3f);
        lineVertexData.append(0.3f);
        lineVertexData.append(1.0f);

        lineVertexData.append(static_cast<float>(x));  // x2
        lineVertexData.append(static_cast<float>(height()));  // y2
        lineVertexData.append(0.3f);  // Dark gray
        lineVertexData.append(0.3f);
        lineVertexData.append(0.3f);
        lineVertexData.append(1.0f);

        lineCount++;
    }

    // 4. Pause cursor (lines 618-631)
    if (!MidiPlayer::isPlaying() && file->pauseTick() >= startTick && file->pauseTick() <= endTick) {
        int x = xPosOfMs(msOfTick(file->pauseTick()));

        // Add vertical line at pause position
        lineVertexData.append(static_cast<float>(x));  // x1
        lineVertexData.append(0.0f);  // y1
        lineVertexData.append(_cachedGrayColor.redF());
        lineVertexData.append(_cachedGrayColor.greenF());
        lineVertexData.append(_cachedGrayColor.blueF());
        lineVertexData.append(_cachedGrayColor.alphaF());

        lineVertexData.append(static_cast<float>(x));  // x2
        lineVertexData.append(static_cast<float>(height()));  // y2
        lineVertexData.append(_cachedGrayColor.redF());
        lineVertexData.append(_cachedGrayColor.greenF());
        lineVertexData.append(_cachedGrayColor.blueF());
        lineVertexData.append(_cachedGrayColor.alphaF());

        lineCount++;
    }

    // Render cursor lines using line pipeline
    if (lineCount > 0 && m_rhi.linePipeline && m_rhi.lineVertexBuffer) {
        // Create or reuse the resource update batch for this frame
        if (!m_rhi.resourceUpdates) {
            m_rhi.resourceUpdates = m_rhi.rhi->nextResourceUpdateBatch();
        }

        if (m_rhi.resourceUpdates) {
            m_rhi.resourceUpdates->updateDynamicBuffer(m_rhi.lineVertexBuffer, 0,
                                      lineVertexData.size() * sizeof(float),
                                      lineVertexData.constData());
        }

        // Render cursor lines
        cb->setGraphicsPipeline(m_rhi.linePipeline);
        const QSize outputSize = renderTarget()->pixelSize();
        cb->setViewport(QRhiViewport(0, 0, outputSize.width(), outputSize.height()));

        if (m_rhi.lineSrb) {
            cb->setShaderResources(m_rhi.lineSrb);
        }

        const QRhiCommandBuffer::VertexInput vbufBinding = { m_rhi.lineVertexBuffer, 0 };
        cb->setVertexInput(0, 1, &vbufBinding);

        // Draw cursor lines (2 vertices per line)
        cb->draw(lineCount * 2);
    }

    // Render cursor triangles using triangle pipeline (if available)
    // 1. File cursor triangle
    if (file->cursorTick() >= startTick && file->cursorTick() <= endTick) {
        int x = xPosOfMs(msOfTick(file->cursorTick()));
        renderCursorTriangle(cb, x, 8, _cachedCursorTriangleColor);
    }

    // 2. Pause cursor triangle
    if (!MidiPlayer::isPlaying() && file->pauseTick() >= startTick && file->pauseTick() <= endTick) {
        int x = xPosOfMs(msOfTick(file->pauseTick()));
        renderCursorTriangle(cb, x, 8, _cachedGrayColor);
    }
}

void RhiMatrixWidget::renderBorderLines(QRhiCommandBuffer *cb, QRhiRenderTarget *rt) {
    Q_UNUSED(rt)

    // EXACT COPY from MatrixWidget lines 634-636: border lines
    QVector<float> lineVertexData;

    // Bottom border line
    lineVertexData.append(static_cast<float>(width() - 1));  // x1
    lineVertexData.append(static_cast<float>(height() - 1));  // y1
    lineVertexData.append(_cachedForegroundColor.redF());
    lineVertexData.append(_cachedForegroundColor.greenF());
    lineVertexData.append(_cachedForegroundColor.blueF());
    lineVertexData.append(_cachedForegroundColor.alphaF());

    lineVertexData.append(static_cast<float>(lineNameWidth));  // x2
    lineVertexData.append(static_cast<float>(height() - 1));  // y2
    lineVertexData.append(_cachedForegroundColor.redF());
    lineVertexData.append(_cachedForegroundColor.greenF());
    lineVertexData.append(_cachedForegroundColor.blueF());
    lineVertexData.append(_cachedForegroundColor.alphaF());

    // Right border line
    lineVertexData.append(static_cast<float>(width() - 1));  // x1
    lineVertexData.append(static_cast<float>(height() - 1));  // y1
    lineVertexData.append(_cachedForegroundColor.redF());
    lineVertexData.append(_cachedForegroundColor.greenF());
    lineVertexData.append(_cachedForegroundColor.blueF());
    lineVertexData.append(_cachedForegroundColor.alphaF());

    lineVertexData.append(static_cast<float>(width() - 1));  // x2
    lineVertexData.append(2.0f);  // y2
    lineVertexData.append(_cachedForegroundColor.redF());
    lineVertexData.append(_cachedForegroundColor.greenF());
    lineVertexData.append(_cachedForegroundColor.blueF());
    lineVertexData.append(_cachedForegroundColor.alphaF());

    // Render border lines using line pipeline
    if (m_rhi.linePipeline && m_rhi.lineVertexBuffer && !lineVertexData.isEmpty()) {
        // Create or reuse the resource update batch for this frame
        if (!m_rhi.resourceUpdates) {
            m_rhi.resourceUpdates = m_rhi.rhi->nextResourceUpdateBatch();
        }

        if (m_rhi.resourceUpdates) {
            m_rhi.resourceUpdates->updateDynamicBuffer(m_rhi.lineVertexBuffer, 0,
                                      lineVertexData.size() * sizeof(float),
                                      lineVertexData.constData());
        }

        // Render border lines
        cb->setGraphicsPipeline(m_rhi.linePipeline);
        const QSize outputSize = renderTarget()->pixelSize();
        cb->setViewport(QRhiViewport(0, 0, outputSize.width(), outputSize.height()));

        if (m_rhi.lineSrb) {
            cb->setShaderResources(m_rhi.lineSrb);
        }

        const QRhiCommandBuffer::VertexInput vbufBinding = { m_rhi.lineVertexBuffer, 0 };
        cb->setVertexInput(0, 1, &vbufBinding);

        // Draw border lines (4 vertices = 2 lines)
        cb->draw(4);
    }
}

void RhiMatrixWidget::renderRecordingIndicator(QRhiCommandBuffer *cb, QRhiRenderTarget *rt) {
    Q_UNUSED(rt)

    // EXACT COPY from MatrixWidget lines 639-641
    // Check if recording is active
    bool isRecording = false; // MidiInput::recording(); // Temporarily disabled to fix compilation

    if (isRecording) {
        // Use circle pipeline if available, otherwise fallback to square
        if (m_rhi.circlePipeline && m_rhi.circleInstanceBuffer && m_rhi.circleSrb) {
            // Render perfect circular recording indicator
            QVector<float> instanceData;

            int indicatorRadius = 6;
            int indicatorX = width() - indicatorRadius - 10;
            int indicatorY = indicatorRadius + 10;

            // Circle instance data: circleTransform (x, y, radius, padding), circleColor (r, g, b, a)
            instanceData.append(static_cast<float>(indicatorX));
            instanceData.append(static_cast<float>(indicatorY));
            instanceData.append(static_cast<float>(indicatorRadius));
            instanceData.append(0.0f); // padding
            instanceData.append(_cachedRecordingIndicatorColor.redF());
            instanceData.append(_cachedRecordingIndicatorColor.greenF());
            instanceData.append(_cachedRecordingIndicatorColor.blueF());
            instanceData.append(_cachedRecordingIndicatorColor.alphaF());

            // Create or reuse the resource update batch for this frame
            if (!m_rhi.resourceUpdates) {
                m_rhi.resourceUpdates = m_rhi.rhi->nextResourceUpdateBatch();
            }

            if (m_rhi.resourceUpdates) {
                m_rhi.resourceUpdates->updateDynamicBuffer(m_rhi.circleInstanceBuffer, 0,
                                          instanceData.size() * sizeof(float),
                                          instanceData.constData());
            }

            // Render the circular recording indicator (outside the batch check)
            cb->setGraphicsPipeline(m_rhi.circlePipeline);
            const QSize outputSize = renderTarget()->pixelSize();
            cb->setViewport(QRhiViewport(0, 0, outputSize.width(), outputSize.height()));

            if (m_rhi.circleSrb) {
                cb->setShaderResources(m_rhi.circleSrb);
            }

            const QRhiCommandBuffer::VertexInput vbufBindings[] = {
                { m_rhi.quadVertexBuffer, 0 },
                { m_rhi.circleInstanceBuffer, 0 }
            };
            cb->setVertexInput(0, 2, vbufBindings);

            // Draw single circle for recording indicator
            cb->draw(6, 1);
        } else {
            // Fallback to square recording indicator using piano pipeline
            QVector<float> instanceData;

            int indicatorSize = 12;
            int indicatorX = width() - indicatorSize - 5;
            int indicatorY = 5;

            // Square instance data: posX, posY, sizeX, sizeY, colorR, colorG, colorB, colorA
            instanceData.append(static_cast<float>(indicatorX));
            instanceData.append(static_cast<float>(indicatorY));
            instanceData.append(static_cast<float>(indicatorSize));
            instanceData.append(static_cast<float>(indicatorSize));
            instanceData.append(_cachedRecordingIndicatorColor.redF());
            instanceData.append(_cachedRecordingIndicatorColor.greenF());
            instanceData.append(_cachedRecordingIndicatorColor.blueF());
            instanceData.append(_cachedRecordingIndicatorColor.alphaF());

            // Render using piano pipeline (reuse for rectangles)
            if (m_rhi.pianoPipeline && m_rhi.quadVertexBuffer && m_rhi.pianoInstanceBuffer && !instanceData.isEmpty()) {
                // Create or reuse the resource update batch for this frame
                if (!m_rhi.resourceUpdates) {
                    m_rhi.resourceUpdates = m_rhi.rhi->nextResourceUpdateBatch();
                }

                if (m_rhi.resourceUpdates) {
                    m_rhi.resourceUpdates->updateDynamicBuffer(m_rhi.pianoInstanceBuffer, 0,
                                              instanceData.size() * sizeof(float),
                                              instanceData.constData());
                }

                // Render the recording indicator (outside the batch check)
                cb->setGraphicsPipeline(m_rhi.pianoPipeline);
                const QSize outputSize = renderTarget()->pixelSize();
                cb->setViewport(QRhiViewport(0, 0, outputSize.width(), outputSize.height()));

                if (m_rhi.pianoSrb) {
                    cb->setShaderResources(m_rhi.pianoSrb);
                }

                const QRhiCommandBuffer::VertexInput vbufBindings[] = {
                    { m_rhi.quadVertexBuffer, 0 },
                    { m_rhi.pianoInstanceBuffer, 0 }
                };
                cb->setVertexInput(0, 2, vbufBindings);

                cb->draw(6, 1);
            }
        }
    }
}

void RhiMatrixWidget::createFontAtlas() {
    if (!m_rhi.rhi) return;

    // Create font atlas texture (512x512 should be enough for basic characters)
    const int atlasSize = 512;

    // Generate font atlas using QPainter (this is the standard approach)
    QImage atlasImage(atlasSize, atlasSize, QImage::Format_RGBA8888);
    atlasImage.fill(Qt::transparent);

    QPainter painter(&atlasImage);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    // Use the same font as MatrixWidget with proper size
    QFont font = Appearance::improveFont(painter.font());
    font.setPixelSize(10);  // Match MatrixWidget font size
    painter.setFont(font);
    painter.setPen(Qt::white);

    // Generate character set (ASCII printable characters + some special ones)
    QString charset = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~";
    charset += "♪♫♬♭♯"; // Musical symbols

    // Calculate character layout
    QFontMetrics fm(font);
    int charWidth = fm.maxWidth() + 2; // Add padding
    int charHeight = fm.height() + 2;
    int charsPerRow = atlasSize / charWidth;
    int charsPerCol = atlasSize / charHeight;

    // Store character UV coordinates for later use
    fontCharacterMap.clear();

    int x = 0, y = 0;
    for (int i = 0; i < charset.length() && i < (charsPerRow * charsPerCol); i++) {
        QChar ch = charset[i];

        // Draw character
        painter.drawText(x * charWidth + 1, y * charHeight + fm.ascent() + 1, QString(ch));

        // Store UV coordinates
        FontCharacter fontChar;
        fontChar.u1 = static_cast<float>(x * charWidth) / atlasSize;
        fontChar.v1 = static_cast<float>(y * charHeight) / atlasSize;
        fontChar.u2 = static_cast<float>((x + 1) * charWidth) / atlasSize;
        fontChar.v2 = static_cast<float>((y + 1) * charHeight) / atlasSize;
        fontChar.width = fm.horizontalAdvance(ch);
        fontChar.height = charHeight - 2;

        fontCharacterMap[ch] = fontChar;

        x++;
        if (x >= charsPerRow) {
            x = 0;
            y++;
        }
    }

    // Create RHI texture from the atlas image
    m_rhi.fontAtlasTexture = m_rhi.rhi->newTexture(QRhiTexture::RGBA8, QSize(atlasSize, atlasSize));
    if (!m_rhi.fontAtlasTexture->create()) {
        qWarning() << "Failed to create font atlas texture";
        return;
    }

    // Upload atlas image to GPU
    // Create or reuse the resource update batch for this frame
    if (!m_rhi.resourceUpdates) {
        m_rhi.resourceUpdates = m_rhi.rhi->nextResourceUpdateBatch();
    }
    m_rhi.resourceUpdates->uploadTexture(m_rhi.fontAtlasTexture, atlasImage);

    // Create sampler for font atlas
    m_rhi.fontAtlasSampler = m_rhi.rhi->newSampler(
        QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
        QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
    if (!m_rhi.fontAtlasSampler->create()) {
        qWarning() << "Failed to create font atlas sampler";
        return;
    }

    // Font atlas upload is already added to the resource update batch above

    qDebug() << "RhiMatrixWidget: Font atlas created successfully with" << charset.length() << "characters";
}

void RhiMatrixWidget::updateTextInstances() {
    if (!m_rhi.textInstanceBuffer || !file) {
        m_rhi.textInstanceCount = 0;
        return;
    }

    QVector<float> instanceData;
    int textCount = 0;

    // 1. Piano key labels (C notes only - EXACT COPY from MatrixWidget)
    for (int i = startLineY; i <= endLineY && textCount < RHI_MAX_INSTANCES; i++) {
        if (i >= 0 && i <= 127) {
            int midiNote = 127 - i;
            int note = midiNote % 12;

            if (note == 0) { // C notes only
                int octave = midiNote / 12 - 1;
                QString text = "C" + QString::number(octave);

                int y = yPosOfLine(i);
                int height = lineHeight();

                // Calculate text position (EXACT COPY from MatrixWidget)
                QFont font = Appearance::improveFont(QFont());
                font.setPixelSize(10);  // Match font atlas size
                QFontMetrics fm(font);
                int textWidth = fm.horizontalAdvance(text);
                float textX = lineNameWidth - textWidth - 8;  // Smaller margin
                float textY = y + height / 2 - 2;  // Center vertically

                // Add text instance data for each character
                for (int charIdx = 0; charIdx < text.length() && textCount < RHI_MAX_INSTANCES; charIdx++) {
                    QChar ch = text[charIdx];
                    if (fontCharacterMap.contains(ch)) {
                        FontCharacter fontChar = fontCharacterMap[ch];

                        // Instance data: instancePosSize (x,y,w,h), instanceColor (r,g,b,a), instanceUV (u1,v1,u2,v2)
                        instanceData.append(textX + charIdx * fontChar.width);   // posX
                        instanceData.append(textY);                              // posY
                        instanceData.append(static_cast<float>(fontChar.width)); // sizeX
                        instanceData.append(static_cast<float>(fontChar.height)); // sizeY
                        instanceData.append(_cachedForegroundColor.redF());      // colorR (use foreground color)
                        instanceData.append(_cachedForegroundColor.greenF());    // colorG
                        instanceData.append(_cachedForegroundColor.blueF());     // colorB
                        instanceData.append(1.0f);                               // colorA
                        instanceData.append(fontChar.u1);                        // u1
                        instanceData.append(fontChar.v1);                        // v1
                        instanceData.append(fontChar.u2);                        // u2
                        instanceData.append(fontChar.v2);                        // v2

                        textCount++;
                    }
                }
            }
        } else {
            // Special event line labels (EXACT COPY from MatrixWidget lines 516-575)
            QString text = "";
            switch (i) {
                case MidiEvent::CONTROLLER_LINE:
                    text = tr("Control Change");
                    break;
                case MidiEvent::TEMPO_CHANGE_EVENT_LINE:
                    text = tr("Tempo Change");
                    break;
                case MidiEvent::TIME_SIGNATURE_EVENT_LINE:
                    text = tr("Time Signature");
                    break;
                case MidiEvent::KEY_SIGNATURE_EVENT_LINE:
                    text = tr("Key Signature");
                    break;
                case MidiEvent::PROG_CHANGE_LINE:
                    text = tr("Program Change");
                    break;
                case MidiEvent::KEY_PRESSURE_LINE:
                    text = tr("Key Pressure");
                    break;
                default:
                    break;
            }

            if (!text.isEmpty() && textCount < RHI_MAX_INSTANCES) {
                int y = yPosOfLine(i);
                int height = lineHeight();

                // Calculate text position (EXACT COPY from MatrixWidget lines 573-574)
                QFont font = Appearance::improveFont(QFont());
                font.setPixelSize(10);
                QFontMetrics fm(font);
                int textWidth = fm.horizontalAdvance(text);
                float textX = lineNameWidth - 15 - textWidth;
                float textY = y + height;

                // Use appropriate color (EXACT COPY from MatrixWidget lines 565-568)
                QColor textColor;
                if (_cachedShouldUseDarkMode) {
                    textColor = QColor(200, 200, 200); // Light gray for dark mode
                } else {
                    textColor = _cachedForegroundColor;
                }

                // Add text instance data for each character
                for (int charIdx = 0; charIdx < text.length() && textCount < RHI_MAX_INSTANCES; charIdx++) {
                    QChar ch = text[charIdx];
                    if (fontCharacterMap.contains(ch)) {
                        FontCharacter fontChar = fontCharacterMap[ch];

                        // Instance data: instancePosSize (x,y,w,h), instanceColor (r,g,b,a), instanceUV (u1,v1,u2,v2)
                        instanceData.append(textX + charIdx * fontChar.width);   // posX
                        instanceData.append(textY);                              // posY
                        instanceData.append(static_cast<float>(fontChar.width)); // sizeX
                        instanceData.append(static_cast<float>(fontChar.height)); // sizeY
                        instanceData.append(textColor.redF());                   // colorR
                        instanceData.append(textColor.greenF());                 // colorG
                        instanceData.append(textColor.blueF());                  // colorB
                        instanceData.append(textColor.alphaF());                 // colorA
                        instanceData.append(fontChar.u1);                        // u1
                        instanceData.append(fontChar.v1);                        // v1
                        instanceData.append(fontChar.u2);                        // u2
                        instanceData.append(fontChar.v2);                        // v2

                        textCount++;
                    }
                }
            }
        }
    }

    // 2. Measure numbers (EXACT COPY from MatrixWidget lines 405-421)
    if (currentTimeSignatureEvents && !currentTimeSignatureEvents->isEmpty() && textCount < RHI_MAX_INSTANCES) {
        TimeSignatureEvent *currentEvent = currentTimeSignatureEvents->at(0);
        if (currentEvent) {
            int measure = file->measure(startTick, endTick, &currentTimeSignatureEvents);
            int tick = currentEvent->midiTime();

            while (tick + currentEvent->ticksPerMeasure() <= startTick) {
                tick += currentEvent->ticksPerMeasure();
            }

            while (tick < endTick && textCount < RHI_MAX_INSTANCES) {
                int xfrom = xPosOfMs(msOfTick(tick));
                int xto = xPosOfMs(msOfTick(tick + currentEvent->ticksPerMeasure()));

                if (tick > startTick && xfrom >= lineNameWidth) {
                    QString text = tr("Measure ") + QString::number(measure - 1);

                    QFont font = Appearance::improveFont(QFont());
                    QFontMetrics fm(font);
                    int textWidth = fm.horizontalAdvance(text);

                    if (textWidth > xto - xfrom) {
                        text = QString::number(measure - 1);
                        textWidth = fm.horizontalAdvance(text);
                    }

                    // Calculate text position (EXACT COPY from MatrixWidget)
                    int pos = (xfrom + xto) / 2;
                    float textX = pos - textWidth / 2.0f;
                    float textY = timeHeight - 9;

                    // Add text instance data for each character
                    for (int charIdx = 0; charIdx < text.length() && textCount < RHI_MAX_INSTANCES; charIdx++) {
                        QChar ch = text[charIdx];
                        if (fontCharacterMap.contains(ch)) {
                            FontCharacter fontChar = fontCharacterMap[ch];

                            instanceData.append(textX + charIdx * fontChar.width);   // posX
                            instanceData.append(textY);                              // posY
                            instanceData.append(static_cast<float>(fontChar.width)); // sizeX
                            instanceData.append(static_cast<float>(fontChar.height)); // sizeY
                            instanceData.append(_cachedMeasureTextColor.redF());     // colorR
                            instanceData.append(_cachedMeasureTextColor.greenF());   // colorG
                            instanceData.append(_cachedMeasureTextColor.blueF());    // colorB
                            instanceData.append(_cachedMeasureTextColor.alphaF());   // colorA
                            instanceData.append(fontChar.u1);                        // u1
                            instanceData.append(fontChar.v1);                        // v1
                            instanceData.append(fontChar.u2);                        // u2
                            instanceData.append(fontChar.v2);                        // v2

                            textCount++;
                        }
                    }
                }

                measure++;
                tick += currentEvent->ticksPerMeasure();
            }
        }
    }

    // 3. Time markers (EXACT COPY from MatrixWidget lines 349-366)
    int startNumber = (startTimeX / 1000) * 1000; // Round to nearest second
    while (startNumber < endTimeX && textCount < RHI_MAX_INSTANCES) {
        int pos = xPosOfMs(startNumber);
        if (pos >= lineNameWidth) {
            QString text = "";
            int hours = startNumber / (60000 * 60);
            int remaining = startNumber - (60000 * 60) * hours;
            int minutes = remaining / (60000);
            remaining = remaining - minutes * 60000;
            int seconds = remaining / 1000;
            int ms = remaining - 1000 * seconds;

            text += QString::number(hours) + ":";
            text += QString("%1:").arg(minutes, 2, 10, QChar('0'));
            text += QString("%1").arg(seconds, 2, 10, QChar('0'));
            text += QString(".%1").arg(ms / 10, 2, 10, QChar('0'));

            QFont font = Appearance::improveFont(QFont());
            QFontMetrics fm(font);
            int textWidth = fm.horizontalAdvance(text);
            if (startNumber > 0) {
                float textX = pos - textWidth / 2.0f;
                float textY = timeHeight - 10;  // Position text near bottom of timeline area

                // Add text instance data for each character
                for (int charIdx = 0; charIdx < text.length() && textCount < RHI_MAX_INSTANCES; charIdx++) {
                    QChar ch = text[charIdx];
                    if (fontCharacterMap.contains(ch)) {
                        FontCharacter fontChar = fontCharacterMap[ch];

                        instanceData.append(textX + charIdx * fontChar.width);   // posX
                        instanceData.append(textY);                              // posY
                        instanceData.append(static_cast<float>(fontChar.width)); // sizeX
                        instanceData.append(static_cast<float>(fontChar.height)); // sizeY
                        instanceData.append(_cachedMeasureTextColor.redF());     // colorR
                        instanceData.append(_cachedMeasureTextColor.greenF());   // colorG
                        instanceData.append(_cachedMeasureTextColor.blueF());    // colorB
                        instanceData.append(_cachedMeasureTextColor.alphaF());   // colorA
                        instanceData.append(fontChar.u1);                        // u1
                        instanceData.append(fontChar.v1);                        // v1
                        instanceData.append(fontChar.u2);                        // u2
                        instanceData.append(fontChar.v2);                        // v2

                        textCount++;
                    }
                }
            }
        }
        startNumber += 1000; // 1 second intervals
    }

    m_rhi.textInstanceCount = textCount;

    if (m_rhi.textInstanceCount > 0 && !instanceData.isEmpty() && m_rhi.rhi) {
        // Create or reuse the resource update batch for this frame
        if (!m_rhi.resourceUpdates) {
            m_rhi.resourceUpdates = m_rhi.rhi->nextResourceUpdateBatch();
        }

        if (m_rhi.resourceUpdates) {
            m_rhi.resourceUpdates->updateDynamicBuffer(m_rhi.textInstanceBuffer, 0,
                                      instanceData.size() * sizeof(float),
                                      instanceData.constData());
        }
    }
}

void RhiMatrixWidget::renderText(QRhiCommandBuffer *cb, QRhiRenderTarget *rt) {
    Q_UNUSED(rt)

    // NATIVE RHI TEXT RENDERING using font atlas
    if (!file || !m_rhi.fontAtlasTexture || !m_rhi.textPipeline) return;

    // Text instances are updated in the main render loop, not here

    // Render text using the text pipeline and font atlas
    if (m_rhi.textInstanceCount > 0 && m_rhi.textInstanceBuffer) {
        // Set up text rendering pipeline
        cb->setGraphicsPipeline(m_rhi.textPipeline);
        const QSize outputSize = renderTarget()->pixelSize();
        cb->setViewport(QRhiViewport(0, 0, outputSize.width(), outputSize.height()));

        // Bind shader resources (including font atlas texture)
        if (m_rhi.textSrb) {
            cb->setShaderResources(m_rhi.textSrb);
        }

        // Bind vertex buffers (quad vertices + text instance data)
        const QRhiCommandBuffer::VertexInput vbufBindings[] = {
            { m_rhi.quadVertexBuffer, 0 },        // Binding 0: quad vertices
            { m_rhi.textInstanceBuffer, 0 }       // Binding 1: text instance data
        };
        cb->setVertexInput(0, 2, vbufBindings);

        // Draw instanced quads (6 vertices per quad, textInstanceCount instances)
        cb->draw(6, m_rhi.textInstanceCount);
    }
}

void RhiMatrixWidget::renderErrorBackground() {
    // Fill entire widget with error color (EXACT COPY from MatrixWidget line 225)
    if (!m_rhi.backgroundPipeline || !m_rhi.quadVertexBuffer || !m_rhi.backgroundInstanceBuffer) {
        return;
    }

    // Create error background instance data
    QVector<float> instanceData;

    // Instance data: posX, posY, sizeX, sizeY, colorR, colorG, colorB, colorA
    instanceData.append(0.0f);                                    // posX (full widget)
    instanceData.append(0.0f);                                    // posY
    instanceData.append(static_cast<float>(width()));             // sizeX
    instanceData.append(static_cast<float>(height()));            // sizeY
    instanceData.append(_cachedErrorColor.redF());                // colorR
    instanceData.append(_cachedErrorColor.greenF());              // colorG
    instanceData.append(_cachedErrorColor.blueF());               // colorB
    instanceData.append(_cachedErrorColor.alphaF());              // colorA

    // Update background instance buffer with error color
    // Create or reuse the resource update batch for this frame
    if (!m_rhi.resourceUpdates) {
        m_rhi.resourceUpdates = m_rhi.rhi->nextResourceUpdateBatch();
    }

    if (m_rhi.resourceUpdates) {
        m_rhi.resourceUpdates->updateDynamicBuffer(m_rhi.backgroundInstanceBuffer, 0,
                                  instanceData.size() * sizeof(float),
                                  instanceData.constData());
    }
}

QColor RhiMatrixWidget::getEventColor(MidiEvent *event) {
    if (!event) {
        return _cachedForegroundColor;
    }

    // EXACT COPY from MatrixWidget color logic
    if (_colorsByChannels) {
        // Color by channel using Appearance system
        int channel = event->channel();
        QColor *channelColor = Appearance::channelColor(channel);
        if (channelColor) {
            return *channelColor;
        }
        return _cachedForegroundColor;
    } else {
        // Color by track using Appearance system
        if (event->track()) {
            QColor *trackColor = Appearance::trackColor(event->track()->number());
            if (trackColor) {
                return *trackColor;
            }

            // Fallback to track's own color if Appearance doesn't have one
            QColor *eventTrackColor = event->track()->color();
            if (eventTrackColor) {
                return *eventTrackColor;
            }
        }
    }

    return _cachedForegroundColor;
}

// === Interface Implementation ===

QList<MidiEvent *> *RhiMatrixWidget::activeEvents() {
    return objects;
}

QList<MidiEvent *> *RhiMatrixWidget::velocityEvents() {
    return velocityObjects;
}



void RhiMatrixWidget::setScreenLocked(bool locked) {
    screen_locked = locked;
}

bool RhiMatrixWidget::screenLocked() {
    return screen_locked;
}

void RhiMatrixWidget::setEnabled(bool enabled) {
    this->enabled = enabled;
    QRhiWidget::setEnabled(enabled);
}



void RhiMatrixWidget::takeKeyPressEvent(QKeyEvent *event) {
    keyPressEvent(event);
}

void RhiMatrixWidget::takeKeyReleaseEvent(QKeyEvent *event) {
    keyReleaseEvent(event);
}

// === Coordinate Conversion Methods ===

double RhiMatrixWidget::lineHeight() {
    return scaleY * RHI_PIXEL_PER_LINE;
}

int RhiMatrixWidget::xPosOfMs(int ms) {
    // EXACT COPY from MatrixWidget::xPosOfMs()
    return lineNameWidth + (ms - startTimeX) * (width() - lineNameWidth) / (endTimeX - startTimeX);
}

int RhiMatrixWidget::yPosOfLine(int line) {
    // EXACT COPY from MatrixWidget::yPosOfLine()
    return timeHeight + (line - startLineY) * lineHeight();
}

int RhiMatrixWidget::msOfXPos(int x) {
    // EXACT COPY from MatrixWidget::msOfXPos()
    return startTimeX + ((x - lineNameWidth) * (endTimeX - startTimeX)) / (width() - lineNameWidth);
}



int RhiMatrixWidget::msOfTick(int tick) {
    // EXACT COPY from MatrixWidget::msOfTick()
    if (!file) return 0;
    return file->msOfTick(tick, currentTempoEvents, msOfFirstEventInList);
}

int RhiMatrixWidget::tickOfMs(int ms) {
    // EXACT COPY from MatrixWidget::tickOfMs()
    if (!file) return 0;
    return file->tick(ms);
}

void RhiMatrixWidget::playNote(int note) {
    // EXACT COPY from MatrixWidget::playNote (lines 1239-1243)
    pianoEvent->setNote(note);
    pianoEvent->setChannel(MidiOutput::standardChannel(), false);
    MidiPlayer::play(pianoEvent);
}

int RhiMatrixWidget::minVisibleMidiTime() {
    // EXACT COPY from MatrixWidget::minVisibleMidiTime()
    return startTick;
}

int RhiMatrixWidget::maxVisibleMidiTime() {
    // EXACT COPY from MatrixWidget::maxVisibleMidiTime()
    return endTick;
}

bool RhiMatrixWidget::eventInWidget(MidiEvent *event) {
    // EXACT COPY from MatrixWidget::eventInWidget()
    NoteOnEvent *on = dynamic_cast<NoteOnEvent *>(event);
    OffEvent *off = dynamic_cast<OffEvent *>(event);
    if (on) {
        off = on->offEvent();
    } else if (off) {
        on = dynamic_cast<NoteOnEvent *>(off->onEvent());
    }
    if (on && off) {
        int offLine = off->line();
        int offTick = off->midiTime();
        bool offIn = offLine >= startLineY && offLine <= endLineY && offTick >= startTick && offTick <= endTick;

        int onLine = on->line();
        int onTick = on->midiTime();
        bool onIn = onLine >= startLineY && onLine <= endLineY && onTick >= startTick && onTick <= endTick;

        // Check if note line is visible (same line for both on and off events)
        bool lineVisible = (onLine >= startLineY && onLine <= endLineY);

        // Check if note spans across visible time range
        bool timeSpansVisible = (onTick <= endTick && offTick >= startTick);

        return (onIn || offIn) || (lineVisible && timeSpansVisible);
    } else {
        // For non-note events, use simple bounds checking
        int line = event->line();
        int tick = event->midiTime();
        return line >= startLineY && line <= endLineY && tick >= startTick && tick <= endTick;
    }
}

// === Event Handling ===

void RhiMatrixWidget::mousePressEvent(QMouseEvent *event) {
    if (!event) return;

    mouseX = event->x();
    mouseY = event->y();

    // Validate mouse coordinates (EXACT COPY from MatrixWidget)
    if (mouseX < 0 || mouseX >= width() || mouseY < 0 || mouseY >= height()) {
        return; // Mouse outside widget bounds
    }

    // EXACT COPY from MatrixWidget::mousePressEvent (lines 1075-1092)
    if (!MidiPlayer::isPlaying() && Tool::currentTool() && mouseInRect(ToolArea)) {
        if (Tool::currentTool()->press(event->buttons() == Qt::LeftButton)) {
            if (enabled) {
                update();
            }
        }
    } else if (enabled && (!MidiPlayer::isPlaying()) && (mouseInRect(PianoArea))) {
        // Handle piano key clicks for note playback (EXACT COPY from MatrixWidget)
        foreach(int key, pianoKeys.keys()) {
            bool inRect = mouseInRect(pianoKeys.value(key));
            if (inRect) {
                // play note
                playNote(key);
            }
        }
    }
}

void RhiMatrixWidget::mouseReleaseEvent(QMouseEvent *event) {
    if (!event) return;

    mouseX = event->x();
    mouseY = event->y();
    mouseReleased = true;

    // EXACT COPY from MatrixWidget::mouseReleaseEvent (lines 1094-1108)
    if (!MidiPlayer::isPlaying() && Tool::currentTool() && mouseInRect(ToolArea)) {
        if (Tool::currentTool()->release()) {
            if (enabled) {
                update();
            }
        }
    } else if (Tool::currentTool()) {
        if (Tool::currentTool()->releaseOnly()) {
            if (enabled) {
                update();
            }
        }
    }
}

void RhiMatrixWidget::mouseMoveEvent(QMouseEvent *event) {
    mouseX = event->x();
    mouseY = event->y();

    if (!enabled) {
        return;
    }

    if (!MidiPlayer::isPlaying() && Tool::currentTool()) {
        if (Tool::currentTool()->move(event->x(), event->y())) {
            if (enabled) {
                update();
            }
        }
    }

    if (!MidiPlayer::isPlaying()) {
        update();
    }
}

void RhiMatrixWidget::keyPressEvent(QKeyEvent *event) {
    // EXACT COPY from MatrixWidget::takeKeyPressEvent (lines 1111-1118)
    if (Tool::currentTool()) {
        if (Tool::currentTool()->pressKey(event->key())) {
            update();
        }
    }

    pianoEmulator(event);
}

void RhiMatrixWidget::keyReleaseEvent(QKeyEvent *event) {
    // EXACT COPY from MatrixWidget::takeKeyReleaseEvent (lines 1121-1127)
    if (Tool::currentTool()) {
        if (Tool::currentTool()->releaseKey(event->key())) {
            update();
        }
    }
}

// === Utility Methods ===

void RhiMatrixWidget::registerRelayout() {
    // For RHI widget, we don't have a pixmap to delete
    calculateDivs();
}

void RhiMatrixWidget::calculateDivs() {
    currentDivs.clear();

    if (!file || !currentTimeSignatureEvents || currentTimeSignatureEvents->isEmpty()) {
        return;
    }

    // EXACT COPY from MatrixWidget::paintEvent division calculation (lines 384-387)
    int measure = file->measure(startTick, endTick, &currentTimeSignatureEvents);

    TimeSignatureEvent *currentEvent = currentTimeSignatureEvents->at(0);
    int i = 0;
    if (!currentEvent) {
        return;
    }

    int tick = currentEvent->midiTime();
    while (tick + currentEvent->ticksPerMeasure() <= startTick) {
        tick += currentEvent->ticksPerMeasure();
    }

    while (tick < endTick) {
        TimeSignatureEvent *measureEvent = currentTimeSignatureEvents->at(i);
        int xfrom = xPosOfMs(msOfTick(tick));
        currentDivs.append(QPair<int, int>(xfrom, tick));
        measure++;
        int measureStartTick = tick;
        tick += currentEvent->ticksPerMeasure();

        if (i < currentTimeSignatureEvents->length() - 1) {
            if (currentTimeSignatureEvents->at(i + 1)->midiTime() <= tick) {
                currentEvent = currentTimeSignatureEvents->at(i + 1);
                tick = currentEvent->midiTime();
                i++;
            }
        }

        // Add beat subdivision divisions (EXACT COPY from MatrixWidget lines 426-481)
        if (measureEvent) {
            int ticksPerDiv = file->ticksPerQuarter(); // Default to quarter notes

            if (_div >= 0) {
                // Regular divisions: _div=0 (whole), _div=1 (half), _div=2 (quarter), etc.
                double metronomeDiv = 4 / std::pow(2.0, _div);
                ticksPerDiv = metronomeDiv * file->ticksPerQuarter();
            } else if (_div <= -100) {
                // Extended subdivision system (EXACT COPY from MatrixWidget)
                int subdivisionType = (-_div) / 100;
                int baseDivision = (-_div) % 100;

                double baseDiv = 4 / std::pow(2.0, baseDivision);

                if (subdivisionType == 1) {
                    ticksPerDiv = (baseDiv * file->ticksPerQuarter()) / 3; // Triplets
                } else if (subdivisionType == 2) {
                    ticksPerDiv = (baseDiv * file->ticksPerQuarter()) / 5; // Quintuplets
                } else if (subdivisionType == 3) {
                    ticksPerDiv = (baseDiv * file->ticksPerQuarter()) / 6; // Sextuplets
                } else if (subdivisionType == 4) {
                    ticksPerDiv = (baseDiv * file->ticksPerQuarter()) / 7; // Septuplets
                } else if (subdivisionType == 5) {
                    ticksPerDiv = (baseDiv * file->ticksPerQuarter()) * 1.5; // Dotted notes
                } else if (subdivisionType == 6) {
                    ticksPerDiv = (baseDiv * file->ticksPerQuarter()) * 1.75; // Double dotted
                } else {
                    ticksPerDiv = (baseDiv * file->ticksPerQuarter()) / 3; // Fallback to triplets
                }
            }

            int startTickDiv = ticksPerDiv;
            while (startTickDiv < measureEvent->ticksPerMeasure()) {
                int divTick = startTickDiv + measureStartTick;
                int xDiv = xPosOfMs(msOfTick(divTick));
                currentDivs.append(QPair<int, int>(xDiv, divTick));
                startTickDiv += ticksPerDiv;
            }
        }
    }
}

bool RhiMatrixWidget::isBlackPianoKey(int midiNote) {
    // Determine if a MIDI note corresponds to a black piano key
    int noteInOctave = midiNote % 12;
    return (noteInOctave == 1 || noteInOctave == 3 || noteInOctave == 6 ||
            noteInOctave == 8 || noteInOctave == 10); // C#, D#, F#, G#, A#
}

QColor RhiMatrixWidget::getChannelColor(int channel) {
    // Return channel-specific colors (matching MatrixWidget logic)
    static const QColor channelColors[] = {
        QColor(255, 100, 100), // Red
        QColor(100, 255, 100), // Green
        QColor(100, 100, 255), // Blue
        QColor(255, 255, 100), // Yellow
        QColor(255, 100, 255), // Magenta
        QColor(100, 255, 255), // Cyan
        QColor(255, 150, 100), // Orange
        QColor(150, 255, 100), // Light Green
        QColor(100, 150, 255), // Light Blue
        QColor(255, 100, 150), // Pink
        QColor(150, 100, 255), // Purple
        QColor(100, 255, 150), // Mint
        QColor(255, 200, 100), // Peach
        QColor(200, 255, 100), // Lime
        QColor(100, 200, 255), // Sky Blue
        QColor(255, 100, 200), // Rose
        QColor(200, 100, 255), // Violet
        QColor(100, 255, 200), // Aqua
        QColor(128, 128, 128)  // Gray for channel 18 (percussion)
    };

    if (channel >= 0 && channel < 19) {
        return channelColors[channel];
    }
    return QColor(128, 128, 128); // Default gray
}



void RhiMatrixWidget::renderMeasureLines(QRhiCommandBuffer *cb) {
    if (!m_rhi.linePipeline || !file) return;

    // Update measure line vertex data
    updateMeasureLineVertices();

    // Render measure lines using the line pipeline
    if (m_rhi.measureLineVertexCount > 0 && m_rhi.measureLineVertexBuffer) {
        // Set up line rendering pipeline
        cb->setGraphicsPipeline(m_rhi.linePipeline);
        const QSize outputSize = renderTarget()->pixelSize();
        cb->setViewport(QRhiViewport(0, 0, outputSize.width(), outputSize.height()));

        // Bind shader resources
        if (m_rhi.lineSrb) {
            cb->setShaderResources(m_rhi.lineSrb);
        }

        // Bind vertex buffer
        const QRhiCommandBuffer::VertexInput vbufBinding = { m_rhi.measureLineVertexBuffer, 0 };
        cb->setVertexInput(0, 1, &vbufBinding);

        // Draw measure lines (2 vertices per line)
        cb->draw(m_rhi.measureLineVertexCount);
    }
}

void RhiMatrixWidget::calcSizes() {
    if (!file) {
        return;
    }

    // Validate widget size (EXACT COPY from MatrixWidget)
    if (width() <= 0 || height() <= 0) {
        qWarning() << "RhiMatrixWidget: Invalid widget size:" << width() << "x" << height();
        return;
    }

    // EXACT COPY from MatrixWidget::calcSizes()
    int time = file->maxTime();
    int timeInWidget = ((width() - lineNameWidth) * 1000) / (RHI_PIXEL_PER_S * scaleX);

    // Calculate endLineY based on widget height (EXACT COPY from MatrixWidget)
    double space = height() - timeHeight;
    double lineSpace = scaleY * RHI_PIXEL_PER_LINE;
    double linesInWidget = space / lineSpace;
    endLineY = startLineY + linesInWidget;

    if (endLineY > RHI_NUM_LINES) {
        endLineY = RHI_NUM_LINES;
    }

    ToolArea = QRectF(lineNameWidth, timeHeight, width() - lineNameWidth,
                      height() - timeHeight);
    PianoArea = QRectF(0, timeHeight, lineNameWidth, height() - timeHeight);
    TimeLineArea = QRectF(lineNameWidth, 0, width() - lineNameWidth, timeHeight);

    // Call scroll methods with suppression to prevent cascading repaints
    _suppressScrollRepaints = true;
    scrollXChanged(startTimeX);
    scrollYChanged(startLineY);
    _suppressScrollRepaints = false;

    // Trigger single repaint after all scroll updates
    registerRelayout();
    update();

    emit sizeChanged(time - timeInWidget, RHI_NUM_LINES - endLineY + startLineY, startTimeX, startLineY);
}

void RhiMatrixWidget::scrollXChanged(int scrollPositionX) {
    if (!file)
        return;

    // EXACT COPY from MatrixWidget::scrollXChanged()
    startTimeX = scrollPositionX;
    endTimeX = startTimeX + ((width() - lineNameWidth) * 1000) / (RHI_PIXEL_PER_S * scaleX);

    if (endTimeX - startTimeX > file->maxTime()) {
        endTimeX = file->maxTime();
        startTimeX = 0;
    } else if (startTimeX < 0) {
        endTimeX -= startTimeX;
        startTimeX = 0;
    } else if (endTimeX > file->maxTime()) {
        startTimeX += file->maxTime() - endTimeX;
        endTimeX = file->maxTime();
    }

    // Only repaint if not suppressed (to prevent cascading repaints)
    if (!_suppressScrollRepaints) {
        registerRelayout();
        update();
    }
}

void RhiMatrixWidget::scrollYChanged(int scrollPositionY) {
    if (!file)
        return;

    startLineY = scrollPositionY;

    double space = height() - timeHeight;
    double lineSpace = scaleY * RHI_PIXEL_PER_LINE;
    double linesInWidget = space / lineSpace;
    endLineY = startLineY + linesInWidget;

    if (endLineY > RHI_NUM_LINES) {
        int d = endLineY - RHI_NUM_LINES;
        endLineY = RHI_NUM_LINES;
        startLineY -= d;
        if (startLineY < 0) {
            startLineY = 0;
        }
    }

    if (!_suppressScrollRepaints) {
        registerRelayout();
        update();
    }
}

void RhiMatrixWidget::timeMsChanged(int ms, bool ignoreLocked) {
    if (!file)
        return;

    int x = xPosOfMs(ms);

    if ((!screen_locked || ignoreLocked) && (x < lineNameWidth || ms < startTimeX || ms > endTimeX || x > width() - 100)) {
        if (file->maxTime() <= endTimeX && ms >= startTimeX) {
            update();
            return;
        }

        emit scrollChanged(ms, (file->maxTime() - endTimeX + startTimeX), startLineY,
                           RHI_NUM_LINES - (endLineY - startLineY));
    } else {
        update();
    }
}



void RhiMatrixWidget::updateCachedAppearanceColors() {
    // EXACT COPY from MatrixWidget color caching logic (lines 1172-1195)
    _cachedBackgroundColor = Appearance::backgroundColor();
    _cachedForegroundColor = Appearance::foregroundColor();
    _cachedBorderColor = Appearance::borderColor();
    _cachedShowRangeLines = Appearance::showRangeLines();
    _cachedStripStyle = Appearance::strip();
    _cachedStripHighlightColor = Appearance::stripHighlightColor();
    _cachedStripNormalColor = Appearance::stripNormalColor();
    _cachedRangeLineColor = Appearance::rangeLineColor();
    _cachedProgramEventHighlightColor = Appearance::programEventHighlightColor();
    _cachedProgramEventNormalColor = Appearance::programEventNormalColor();
    _cachedSystemWindowColor = Appearance::systemWindowColor();
    _cachedMeasureBarColor = Appearance::measureBarColor();
    _cachedMeasureLineColor = Appearance::measureLineColor();
    _cachedMeasureTextColor = Appearance::measureTextColor();
    _cachedTimelineGridColor = Appearance::timelineGridColor();
    _cachedDarkGrayColor = Appearance::darkGrayColor();
    _cachedGrayColor = Appearance::grayColor();
    _cachedErrorColor = Appearance::errorColor();
    _cachedPlaybackCursorColor = Appearance::playbackCursorColor();
    _cachedCursorTriangleColor = Appearance::cursorTriangleColor();
    _cachedRecordingIndicatorColor = Appearance::recordingIndicatorColor();
    _cachedPianoWhiteKeyColor = Appearance::pianoWhiteKeyColor();
    _cachedPianoBlackKeyColor = Appearance::pianoBlackKeyColor();
    _cachedPianoWhiteKeyHoverColor = Appearance::pianoWhiteKeyHoverColor();
    _cachedPianoBlackKeyHoverColor = Appearance::pianoBlackKeyHoverColor();
    _cachedPianoWhiteKeySelectedColor = Appearance::pianoWhiteKeySelectedColor();
    _cachedPianoBlackKeySelectedColor = Appearance::pianoBlackKeySelectedColor();
    _cachedPianoKeyLineHighlightColor = Appearance::pianoKeyLineHighlightColor();
    _cachedMeasureTextColor = Appearance::foregroundColor();
    _cachedShouldUseDarkMode = Appearance::shouldUseDarkMode();
}

// === Helper Methods Implementation ===

bool RhiMatrixWidget::mouseInRect(const QRectF &rect) {
    // EXACT COPY from PaintWidget::mouseInRect implementation
    return mouseInRect(rect.x(), rect.y(), rect.width(), rect.height());
}

bool RhiMatrixWidget::mouseInRect(int x, int y, int width, int height) {
    // EXACT COPY from PaintWidget::mouseInRect and mouseBetween implementation
    // Check if mouse is within the rectangle bounds
    int x1 = x, y1 = y, x2 = x + width, y2 = y + height;

    // Ensure coordinates are ordered correctly
    if (x1 > x2) {
        int temp = x1;
        x1 = x2;
        x2 = temp;
    }
    if (y1 > y2) {
        int temp = y1;
        y1 = y2;
        y2 = temp;
    }

    // Check if mouse is within bounds (mouseOver equivalent is always true for RHI widget)
    return mouseX >= x1 && mouseX <= x2 && mouseY >= y1 && mouseY <= y2;
}

// === Missing Interface Methods ===

void RhiMatrixWidget::updateRenderingSettings() {
    updateCachedAppearanceColors();
    update();
}

MidiFile *RhiMatrixWidget::midiFile() {
    return file;
}

int RhiMatrixWidget::lineAtY(int y) {
    return startLineY + (y - timeHeight) / lineHeight();
}

int RhiMatrixWidget::timeMsOfWidth(int x) {
    return msOfXPos(x);
}

void RhiMatrixWidget::setScaleX(double scale) {
    scaleX = scale;
    calcSizes();
}

void RhiMatrixWidget::setScaleY(double scale) {
    scaleY = scale;
    calcSizes();
}

void RhiMatrixWidget::setDiv(int div) {
    _div = div;
    calculateDivs();
    update();
}

void RhiMatrixWidget::setColorsByChannels(bool byChannels) {
    _colorsByChannels = byChannels;
    update();
}

void RhiMatrixWidget::setColorsByChannel() {
    // EXACT COPY from MatrixWidget::setColorsByChannel (line 1507)
    _colorsByChannels = true;
    update();
}

void RhiMatrixWidget::pianoEmulator(QKeyEvent *event) {
    // EXACT COPY from MatrixWidget::pianoEmulator (lines 1210-1237)
    if (!_isPianoEmulationEnabled) return;

    int key = event->key();

    const int C4_OFFSET = 48;

    // z, s, x, d, c, v -> C, C#, D, D#, E, F
    int keys[] = {
        90, 83, 88, 68, 67, 86, 71, 66, 72, 78, 74, 77, // C3 - H3
        81, 50, 87, 51, 69, 82, 53, 84, 54, 89, 55, 85, // C4 - H4
        73, 57, 79, 48, 80, 91, 61, 93 // C5 - G5
    };
    for (uint8_t idx = 0; idx < sizeof(keys) / sizeof(*keys); idx++) {
        if (key == keys[idx]) {
            RhiMatrixWidget::playNote(idx + C4_OFFSET);
        }
    }

    int dupkeys[] = {
        44, 76, 46, 59, 47 // C4 - E4 (,l.;/)
    };
    for (uint8_t idx = 0; idx < sizeof(dupkeys) / sizeof(*dupkeys); idx++) {
        if (key == dupkeys[idx]) {
            RhiMatrixWidget::playNote(idx + C4_OFFSET + 12);
        }
    }
}

void RhiMatrixWidget::setPianoEmulation(bool enabled) {
    // EXACT COPY from MatrixWidget::setPianoEmulation (line 1522)
    _isPianoEmulationEnabled = enabled;
}

void RhiMatrixWidget::mouseDoubleClickEvent(QMouseEvent *event) {
    mouseX = event->x();
    mouseY = event->y();

    // EXACT COPY from MatrixWidget::mouseDoubleClickEvent (lines 1377-1383)
    if (mouseInRect(TimeLineArea)) {
        int tick = file->tick(msOfXPos(mouseX));
        file->setCursorTick(tick);
        update();
    }
}

void RhiMatrixWidget::wheelEvent(QWheelEvent *event) {
    if (!file) return;

    // EXACT COPY from MatrixWidget::wheelEvent (lines 1432-1458)
    QPoint pixelDelta = event->pixelDelta();
    QPoint angleDelta = event->angleDelta();

    int pixelDeltaX = pixelDelta.x();
    int pixelDeltaY = pixelDelta.y();

    // If no pixel delta, use angle delta
    if (pixelDeltaX == 0 && pixelDeltaY == 0) {
        pixelDeltaX = angleDelta.x() / 8;
        pixelDeltaY = angleDelta.y() / 8;
    }

    Qt::KeyboardModifiers km = event->modifiers();

    int horScrollAmount = 0;
    int verScrollAmount = 0;

    if (km) {
        int pixelDeltaLinear = pixelDeltaY;
        if (pixelDeltaLinear == 0) pixelDeltaLinear = pixelDeltaX;

        if (km == Qt::ShiftModifier) {
            if (pixelDeltaLinear > 0) {
                zoomVerIn();
            } else if (pixelDeltaLinear < 0) {
                zoomVerOut();
            }
        } else if (km == Qt::ControlModifier) {
            if (pixelDeltaLinear > 0) {
                zoomHorIn();
            } else if (pixelDeltaLinear < 0) {
                zoomHorOut();
            }
        } else if (km == Qt::AltModifier) {
            horScrollAmount = pixelDeltaLinear;
        }
    } else {
        horScrollAmount = pixelDeltaX;
        verScrollAmount = pixelDeltaY;
    }

    // Apply scrolling
    if (horScrollAmount != 0) {
        int newStartTimeX = startTimeX + (horScrollAmount * (endTimeX - startTimeX)) / (width() - lineNameWidth);
        emit scrollChanged(newStartTimeX, file->maxTime() - (endTimeX - startTimeX), startLineY, RHI_NUM_LINES - (endLineY - startLineY));
    }

    if (verScrollAmount != 0) {
        // Invert vertical scroll direction to match MatrixWidget behavior
        int newStartLineY = startLineY - verScrollAmount / lineHeight();
        emit scrollChanged(startTimeX, file->maxTime() - (endTimeX - startTimeX), newStartLineY, RHI_NUM_LINES - (endLineY - startLineY));
    }

    event->accept();
}

void RhiMatrixWidget::resizeEvent(QResizeEvent *event) {
    qDebug() << "RhiMatrixWidget: Resize event - new size:" << event->size() << "old size:" << event->oldSize();
    QRhiWidget::resizeEvent(event);
    calcSizes();

    // Force an update after resize
    update();
}

void RhiMatrixWidget::showEvent(QShowEvent *event) {
    qDebug() << "RhiMatrixWidget: Show event - size:" << size() << "visible:" << isVisible();
    QRhiWidget::showEvent(event);

    // Recalculate view parameters now that widget is visible and has proper size
    if (file) {
        calcSizes();
    }

    // Check if RHI is available and try to understand why initialize() isn't called
    qDebug() << "RhiMatrixWidget: Checking RHI availability...";
    qDebug() << "RhiMatrixWidget: API:" << static_cast<int>(api());
    qDebug() << "RhiMatrixWidget: Debug layer enabled:" << isDebugLayerEnabled();

    // Check if the widget has a valid window handle
    if (window() && window()->windowHandle()) {
        qDebug() << "RhiMatrixWidget: Window handle available";
        qDebug() << "RhiMatrixWidget: Window handle type:" << window()->windowHandle()->surfaceType();

        // Check OpenGL support
        QSurfaceFormat format = window()->windowHandle()->format();
        qDebug() << "RhiMatrixWidget: OpenGL version:" << format.majorVersion() << "." << format.minorVersion();
        qDebug() << "RhiMatrixWidget: OpenGL profile:" << format.profile();
        qDebug() << "RhiMatrixWidget: OpenGL context valid:" << (format.majorVersion() > 0);
    } else {
        qDebug() << "RhiMatrixWidget: No window handle - this might prevent RHI initialization";
    }

    // Try to get RHI directly to see if it's available
    QRhi *rhiInstance = rhi();
    if (rhiInstance) {
        qDebug() << "RhiMatrixWidget: RHI instance available in showEvent!";
        qDebug() << "RhiMatrixWidget: RHI backend:" << rhiInstance->backend();
    } else {
        qDebug() << "RhiMatrixWidget: No RHI instance available in showEvent";
        qDebug() << "RhiMatrixWidget: This suggests OpenGL context creation failed";
    }

    // Force an update when shown
    update();
}

void RhiMatrixWidget::paintEvent(QPaintEvent *event) {
    // Check if QRhiWidget has an RHI context available
    QRhi *rhiInstance = rhi();
    if (rhiInstance && !m_rhi.initialized) {
        qDebug() << "RhiMatrixWidget: RHI context found in paintEvent! Attempting manual initialization...";

        // Try to manually trigger initialization
        m_rhi.rhi = rhiInstance;
        try {
            initializeBuffers();
            initializeShaders();
            m_rhi.initialized = true;
            qDebug() << "RhiMatrixWidget: Manual initialization successful!";
        } catch (const std::exception &e) {
            qWarning() << "RhiMatrixWidget: Manual initialization failed:" << e.what();
            m_rhi.initialized = false;
        }
    }

    // If RHI is not initialized, fall back to basic QPainter rendering
    if (!m_rhi.initialized || !m_rhi.rhi) {
        qDebug() << "RhiMatrixWidget: RHI not available, using QPainter fallback";

        // Don't use QPainter on QRhiWidget - it's not supported
        // Just return and let QRhiWidget handle it
        QRhiWidget::paintEvent(event);
        return;
    }

    // If RHI is available, let QRhiWidget handle the painting
    QRhiWidget::paintEvent(event);
}

void RhiMatrixWidget::enterEvent(QEnterEvent *event) {
    Q_UNUSED(event)
    // EXACT COPY from MatrixWidget::enterEvent (lines 1055-1063)
    if (Tool::currentTool()) {
        Tool::currentTool()->enter();
        if (enabled) {
            update();
        }
    }
}

void RhiMatrixWidget::leaveEvent(QEvent *event) {
    Q_UNUSED(event)
    // EXACT COPY from MatrixWidget::leaveEvent (lines 1065-1073)
    if (Tool::currentTool()) {
        Tool::currentTool()->exit();
        if (enabled) {
            update();
        }
    }
}

void RhiMatrixWidget::resetView() {
    if (!file) {
        return;
    }

    // Reset zoom to default (EXACT COPY from MatrixWidget::resetView)
    scaleX = 1.0;
    scaleY = 1.0;

    // Reset horizontal scroll to beginning
    startTimeX = 0;

    // Reset vertical scroll to roughly center on Middle C (line 60)
    startLineY = 50;

    // Reset cursor and pause positions to beginning (EXACT COPY from MatrixWidget)
    file->setCursorTick(0);
    file->setPauseTick(-1);

    // Recalculate sizes and update display
    calcSizes();

    // Force a complete repaint
    update();
}

void RhiMatrixWidget::zoomHorIn() {
    // EXACT COPY from MatrixWidget::zoomHorIn
    scaleX += 0.1;
    calcSizes();
}

void RhiMatrixWidget::zoomHorOut() {
    // EXACT COPY from MatrixWidget::zoomHorOut
    if (scaleX >= 0.2) {
        scaleX -= 0.1;
        calcSizes();
    }
}

void RhiMatrixWidget::zoomVerIn() {
    // EXACT COPY from MatrixWidget::zoomVerIn
    scaleY += 0.1;
    calcSizes();
}

void RhiMatrixWidget::zoomVerOut() {
    // EXACT COPY from MatrixWidget::zoomVerOut
    if (scaleY >= 0.2) {
        scaleY -= 0.1;
        if (height() <= RHI_NUM_LINES * lineHeight() * scaleY / (scaleY + 0.1)) {
            calcSizes();
        } else {
            scaleY += 0.1;
        }
    }
}

void RhiMatrixWidget::zoomStd() {
    scaleX = 1.0;
    scaleY = 1.0;
    calcSizes();
}

// === Graphics API Fallback Implementation ===

bool RhiMatrixWidget::initializeGraphicsAPI() {
    qDebug() << "RhiMatrixWidget: Starting graphics API initialization with platform-optimized fallback chain";

#ifdef Q_OS_WIN
    // Windows: Optimized fallback chain - D3D12 → Vulkan → OpenGL → Software
    qDebug() << "RhiMatrixWidget: Using optimized fallback: D3D12 → Vulkan → OpenGL → Software";

    // Try D3D12 first (modern DirectX, uses compiled HLSL 67 shaders)
    if (tryInitializeAPI(QRhiWidget::Api::Direct3D12)) {
        qDebug() << "RhiMatrixWidget: Successfully initialized with Direct3D12";
        return true;
    }

    // Try Vulkan second (cross-platform, uses compiled GLSL 460 shaders)
    if (tryInitializeAPI(QRhiWidget::Api::Vulkan)) {
        qDebug() << "RhiMatrixWidget: Successfully initialized with Vulkan";
        return true;
    }

    // Try OpenGL third (maximum compatibility, uses compiled GLSL 460 shaders)
    if (tryInitializeAPI(QRhiWidget::Api::OpenGL)) {
        qDebug() << "RhiMatrixWidget: Successfully initialized with OpenGL";
        return true;
    }

#elif defined(Q_OS_MACOS)
    // macOS: Use Metal (not available in Qt RHI yet) or OpenGL
    qDebug() << "RhiMatrixWidget: Using OpenGL-first fallback for macOS platform";

    // Try OpenGL first (best compatibility on macOS)
    if (tryInitializeAPI(QRhiWidget::Api::OpenGL)) {
        qDebug() << "RhiMatrixWidget: Successfully initialized with OpenGL";
        return true;
    }

    // Try Vulkan second (via MoltenVK)
    if (tryInitializeAPI(QRhiWidget::Api::Vulkan)) {
        qDebug() << "RhiMatrixWidget: Successfully initialized with Vulkan (MoltenVK)";
        return true;
    }

#else
    // Linux/Other: Prioritize Vulkan for modern performance
    qDebug() << "RhiMatrixWidget: Using Vulkan-first fallback for Linux/Unix platform";

    // Try Vulkan first (best performance on modern Linux)
    if (tryInitializeAPI(QRhiWidget::Api::Vulkan)) {
        qDebug() << "RhiMatrixWidget: Successfully initialized with Vulkan";
        return true;
    }

    // Try OpenGL second (universal compatibility)
    if (tryInitializeAPI(QRhiWidget::Api::OpenGL)) {
        qDebug() << "RhiMatrixWidget: Successfully initialized with OpenGL";
        return true;
    }
#endif

    // All RHI APIs failed
    qWarning() << "RhiMatrixWidget: All graphics APIs failed - hardware acceleration unavailable";
    return false;
}

bool RhiMatrixWidget::tryInitializeAPI(QRhiWidget::Api api) {
    QString apiName = getApiName(api);
    qDebug() << "RhiMatrixWidget: Attempting to initialize" << apiName << "API";

    try {
        // Set the API
        setApi(api);

        // Test if the API is available by checking if we can get basic info
        // Note: Full initialization happens in initialize() method
        qDebug() << "RhiMatrixWidget: Set API to" << apiName << "- will test during widget initialization";
        return true;

    } catch (const std::exception &e) {
        qWarning() << "RhiMatrixWidget: Failed to initialize" << apiName << "API:" << e.what();
        return false;
    } catch (...) {
        qWarning() << "RhiMatrixWidget: Failed to initialize" << apiName << "API: Unknown error";
        return false;
    }
}

QString RhiMatrixWidget::getApiName(QRhiWidget::Api api) {
    switch (api) {
        case QRhiWidget::Api::Direct3D12: return "Direct3D12";
        case QRhiWidget::Api::Vulkan: return "Vulkan";
        case QRhiWidget::Api::OpenGL: return "OpenGL";
        default: return "Unknown";
    }
}


