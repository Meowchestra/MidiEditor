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

#ifndef OPENGLMISCWIDGET_H_
#define OPENGLMISCWIDGET_H_

#include "OpenGLPaintWidget.h"
#include "MiscWidget.h"
#include "MatrixWidget.h"

/**
 * \class OpenGLMiscWidget
 *
 * \brief OpenGL-accelerated version of MiscWidget for hardware-accelerated MIDI editing.
 *
 * OpenGLMiscWidget provides the exact same functionality as MiscWidget but with
 * OpenGL hardware acceleration. It's designed as a drop-in replacement that:
 *
 * - **Inherits from OpenGLPaintWidget**: Gets OpenGL acceleration automatically
 * - **Delegates to MiscWidget**: Uses composition to reuse all existing MiscWidget logic
 * - **Transparent Integration**: Same API and behavior as original MiscWidget
 * - **Performance Boost**: GPU-accelerated rendering for velocity/controller editing
 *
 * The implementation uses composition rather than inheritance to avoid complex
 * multiple inheritance issues while providing seamless OpenGL acceleration.
 */
class OpenGLMiscWidget : public OpenGLPaintWidget {
    Q_OBJECT

public:
    /**
     * \brief Creates a new OpenGL-accelerated MiscWidget.
     * \param matrixWidget The associated MatrixWidget
     * \param settings Application settings
     * \param parent The parent widget
     */
    OpenGLMiscWidget(MatrixWidget *matrixWidget, QSettings *settings, QWidget *parent = nullptr);

    /**
     * \brief Destructor.
     */
    ~OpenGLMiscWidget();

    // === Delegate all MiscWidget methods ===

    /**
     * \brief Gets the internal MiscWidget instance.
     * \return Pointer to the internal MiscWidget
     */
    MiscWidget *getMiscWidget() const { return _miscWidget; }

    // Forward all public MiscWidget methods with smooth visual feedback
    static QString modeToString(int mode) { return MiscWidget::modeToString(mode); }

    void setMode(int mode) {
        _miscWidget->setMode(mode);
        update();
    }

    void setEditMode(int mode) {
        _miscWidget->setEditMode(mode);
        update();
    }

    MatrixWidget *getMatrixWidget() const { return _miscWidget->getMatrixWidget(); }

public slots:
    // Forward all MiscWidget slots with smooth visual feedback
    void setChannel(int channel) {
        _miscWidget->setChannel(channel);
        update();
    }

    void setControl(int ctrl) {
        _miscWidget->setControl(ctrl);
        update();
    }

protected:
    /**
     * \brief Implements OpenGL-accelerated painting by delegating to MiscWidget.
     * \param painter OpenGL-accelerated QPainter
     */
    void paintContent(QPainter *painter) override;

    // === Event Handlers ===
    void mousePressEvent(QMouseEvent *event) override;

    void mouseReleaseEvent(QMouseEvent *event) override;

    void mouseMoveEvent(QMouseEvent *event) override;

    void wheelEvent(QWheelEvent *event) override;

    void enterEvent(QEnterEvent *event) override;

    void leaveEvent(QEvent *event) override;

    void keyPressEvent(QKeyEvent *event) override;

    void keyReleaseEvent(QKeyEvent *event) override;

    void resizeEvent(QResizeEvent *event) override;

private:
    /** \brief Internal MiscWidget instance that handles all the logic */
    MiscWidget *_miscWidget;
};

#endif // OPENGLMISCWIDGET_H_
