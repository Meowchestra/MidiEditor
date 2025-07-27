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

#include "MatrixWidget.h"
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLShaderProgram>
#include <QMatrix4x4>

/**
 * @brief OpenGL-accelerated version of MatrixWidget for improved performance
 * 
 * This widget uses OpenGL for hardware-accelerated rendering of MIDI events,
 * providing significant performance improvements when displaying thousands of events.
 * Falls back to regular MatrixWidget if OpenGL is not available.
 */
class AcceleratedMatrixWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    explicit AcceleratedMatrixWidget(QWidget* parent = nullptr);
    ~AcceleratedMatrixWidget();

    // Forward MatrixWidget interface
    void setFile(MidiFile* file);
    MidiFile* midiFile();
    
    // OpenGL-specific methods
    bool isOpenGLAccelerated() const { return _openglInitialized; }
    void setUseOpenGL(bool enabled);

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int width, int height) override;
    
    // Event handling
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private slots:
    void onMatrixWidgetUpdate();

private:
    // OpenGL rendering methods
    void setupShaders();
    void setupBuffers();
    void renderEvents();
    void renderBackground();
    void renderPianoKeys();
    
    // Batch rendering for performance
    void batchRenderRectangles(const QVector<QRectF>& rects, const QColor& color);
    void updateEventBuffers();
    
    // Fallback to software rendering
    void fallbackToSoftwareRendering();

    // OpenGL resources
    QOpenGLShaderProgram* _shaderProgram;
    QOpenGLBuffer* _vertexBuffer;
    QOpenGLBuffer* _colorBuffer;
    QOpenGLVertexArrayObject* _vao;
    QMatrix4x4 _projectionMatrix;
    
    // State
    bool _openglInitialized;
    bool _useOpenGL;
    MatrixWidget* _fallbackWidget;  // Software fallback
    
    // Event data for GPU rendering
    struct EventVertex {
        float x, y, width, height;
        float r, g, b, a;
    };
    QVector<EventVertex> _eventVertices;
    bool _eventBuffersNeedUpdate;
    
    // Performance monitoring
    QElapsedTimer _frameTimer;
    int _frameCount;
    float _averageFPS;
};

#endif // ACCELERATEDMATRIXWIDGET_H
