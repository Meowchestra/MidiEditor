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

    // === MiscWidget API Delegation ===
    // All methods delegate to the internal MiscWidget instance

    /**
     * \brief Converts a mode constant to a human-readable string.
     * \param mode The mode constant to convert
     * \return String representation of the mode
     */
    static QString modeToString(int mode) { return MiscWidget::modeToString(mode); }

    /**
     * \brief Sets the editor mode (velocity, controller, etc.).
     * \param mode The editor mode constant
     */
    void setMode(int mode) {
        _miscWidget->setMode(mode);
        update();
    }

    /**
     * \brief Sets the editing interaction mode.
     * \param mode The edit mode (SINGLE_MODE, LINE_MODE, MOUSE_MODE)
     */
    void setEditMode(int mode) {
        _miscWidget->setEditMode(mode);
        update();
    }

    /**
     * \brief Gets the associated MatrixWidget.
     * \return Pointer to the MatrixWidget
     */
    MatrixWidget *getMatrixWidget() const { return _miscWidget->getMatrixWidget(); }

public slots:
    // === MiscWidget Slot Delegation ===
    // All slots delegate to the internal MiscWidget and trigger OpenGL updates

    /**
     * \brief Sets the active MIDI channel for editing.
     * \param channel The MIDI channel (0-15)
     */
    void setChannel(int channel) {
        _miscWidget->setChannel(channel);
        update();
    }

    /**
     * \brief Sets the active controller number for controller editing.
     * \param ctrl The controller number (0-127)
     */
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
    // All event handlers delegate to the internal MiscWidget

    /**
     * \brief Handles mouse press events for value editing.
     * \param event The mouse press event
     */
    void mousePressEvent(QMouseEvent *event) override;

    /**
     * \brief Handles mouse release events for value editing.
     * \param event The mouse release event
     */
    void mouseReleaseEvent(QMouseEvent *event) override;

    /**
     * \brief Handles mouse move events for value editing and selection.
     * \param event The mouse move event
     */
    void mouseMoveEvent(QMouseEvent *event) override;

    /**
     * \brief Handles wheel events for zooming and scrolling.
     * \param event The wheel event
     */
    void wheelEvent(QWheelEvent *event) override;

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
     * \brief Handles key press events for shortcuts.
     * \param event The key press event
     */
    void keyPressEvent(QKeyEvent *event) override;

    /**
     * \brief Handles key release events.
     * \param event The key release event
     */
    void keyReleaseEvent(QKeyEvent *event) override;

    /**
     * \brief Handles resize events to update the internal widget.
     * \param event The resize event
     */
    void resizeEvent(QResizeEvent *event) override;

private:
    // === Internal Components ===

    /** \brief Internal MiscWidget instance that handles all the logic */
    MiscWidget *_miscWidget;
};

#endif // OPENGLMISCWIDGET_H_
