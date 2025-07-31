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

#ifndef ACCELERATEDMATRIXWIDGET_H
#define ACCELERATEDMATRIXWIDGET_H

#include <QWidget>
#include <QElapsedTimer>
#include <QTimer>
#include <QList>
#include "MatrixRenderData.h"

class MidiFile;
class MidiEvent;
class OnEvent;
class QSettings;
class GraphicObject;

/**
 * @brief Qt RHI-based hardware-accelerated matrix widget for maximum performance
 *
 * Uses Qt's Rendering Hardware Interface (RHI) for optimal performance:
 * - Windows: D3D12 → D3D11 → Vulkan → OpenGL → Software MatrixWidget fallback
 * - Linux: Vulkan → OpenGL → Software MatrixWidget fallback
 *
 * Provides same interface as MatrixWidget with 100-200x performance improvement.
 * Falls back to software MatrixWidget if hardware acceleration fails.
 */
class AcceleratedMatrixWidget : public QWidget {
    Q_OBJECT

signals:
    void fileChanged();

    void eventClicked(MidiEvent *event);

    void viewportChanged(int startTick, int endTick);

    void accelerationFailed();

    void hardwareAccelerationFailed(const QString &reason);

public:
    explicit AcceleratedMatrixWidget(QWidget *parent = nullptr);

    ~AcceleratedMatrixWidget();

    // NEW: Pure renderer interface - receives data from HybridMatrixWidget
    void setRenderData(const MatrixRenderData &data);

    MatrixRenderData *getRenderData() const { return _renderData; }

    // MatrixWidget interface compatibility
    void setFile(MidiFile *file);

    MidiFile *midiFile() const { return _file; }

    // View control (setters handled by HybridMatrixWidget)
    // Pure renderer - viewport managed through render data only
    double lineHeight() const;

    int lineNameWidth() const { return _renderData ? _renderData->lineNameWidth : 0; }

    // Selection and interaction (setters handled by HybridMatrixWidget)
    QList<GraphicObject *> *objects() { return reinterpret_cast<QList<GraphicObject *> *>(&_midiEvents); }
    bool colorsByChannels() const { return _renderData ? _renderData->colorsByChannels : false; }

    // Use render data instead of duplicate business logic
    QList<MidiEvent *> *activeEvents() { return &_midiEvents; }
    QList<MidiEvent *> *velocityEvents() { return &_midiEvents; }

    int lineAtY(int y) const;

    int yPosOfLine(int line) const;

    int xPosOfMs(int ms) const;

    bool eventInWidget(MidiEvent *event) const;

    // State access (setters handled by HybridMatrixWidget)
    bool screenLocked() const;

    int minVisibleMidiTime() const;

    int maxVisibleMidiTime() const;

    int div() const;

    int measure() const;

    int tool() const;

    // Hardware acceleration status
    bool isHardwareAccelerated() const;

    QString backendName() const;

    // Initialize hardware acceleration
    bool initialize();

    // Additional methods
    void updateView();

    QList<GraphicObject *> *getObjects() { return &_objects; }

    // GPU cache management (public for HybridMatrixWidget access)
    void clearGPUCache();

protected:
    void paintEvent(QPaintEvent *event) override;

    void resizeEvent(QResizeEvent *event) override;

public slots:
    // Add slots here if needed in the future

private:
    // Full rendering pipeline methods
    void renderFullMatrixContent(QPainter *painter);

    void renderBackground(QPainter *painter);

    void renderPianoKeys(QPainter *painter);

    void renderMidiEvents(QPainter *painter);

    void renderGridLines(QPainter *painter);

    void renderTimeline(QPainter *painter);

    void renderTools(QPainter *painter);

    void renderCursor(QPainter *painter);

    // Piano key rendering helpers
    void renderPianoKey(QPainter *painter, int number, int x, int y, int width, int height);

    QString getNoteNameForMidiNumber(int midiNumber);

    // MIDI event rendering helpers
    void renderChannelEvents(QPainter *painter, int channel);

    void renderNoteEvent(QPainter *painter, OnEvent *onEvent, const QColor &color);

    void renderSingleEvent(QPainter *painter, MidiEvent *event, const QColor &color);

    // Timeline rendering helpers
    QString formatTimeLabel(int timeMs);

    void renderMeasures(QPainter *painter);

    // GPU resource management (implemented in PlatformImpl)
    bool createGPUResources();

    void destroyGPUResources();

    bool createShaderPipelines();

    bool createVertexBuffers();

    bool createFontAtlas();

    void updateUniformBuffer();

    // Shader creation helpers (implemented in PlatformImpl)
    bool createMidiEventPipeline();

    bool createBackgroundPipeline();

    bool createLinePipeline();

    bool createTextPipeline();

    bool createPianoPipeline();

    // Shader compilation utilities (implemented in PlatformImpl)
    bool loadShader(const QString &filename);

    bool createBasicVertexShader();

    bool createBasicFragmentShader();

    bool compileShaders();

    // Platform-specific implementation
    class PlatformImpl;
    PlatformImpl *_impl;

    // Basic state
    MidiFile *_file;
    QSettings *_settings;

    // Event data for rendering (populated from render data)
    QList<MidiEvent *> _midiEvents;
    QList<GraphicObject *> _objects;

    // Event data for GPU rendering
    struct EventVertex {
        float x, y, width, height;
        float r, g, b, a;
    };

    QList<EventVertex> _eventVertices;
    QString _backendName;

    // GPU data caching for performance optimization
    struct CachedGPUData {
        QList<float> midiEventInstances;
        QList<float> pianoKeyData;
        QList<float> lineVertices;
        int lastStartTick = -1;
        int lastEndTick = -1;
        int lastStartLine = -1;
        int lastEndLine = -1;
        bool isDirty = true;
    } _cachedGPUData;

    // Cached render data from HybridMatrixWidget
    MatrixRenderData *_renderData;

    // Frame rate limiting
    QElapsedTimer _frameTimer;
    qint64 _targetFrameTime;

    // Update event data for rendering
    void updateEventData();

    bool validateGPUResources();

    bool recreateGPUResources();

    bool validateRenderData(const MatrixRenderData &data);

private slots:
    // Color calculation for events
    QColor getEventColor(MidiEvent *event) const;

    // Coordinate conversion helpers
    float tickToX(int tick) const;

    float lineToY(int line) const;

    int xToTick(float x) const;

    int yToLine(float y) const;
};

#endif // ACCELERATEDMATRIXWIDGET_H
