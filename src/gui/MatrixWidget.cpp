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

#include "MatrixWidget.h"
#include "HybridMatrixWidget.h" // For MatrixRenderData
#include "../MidiEvent/MidiEvent.h"
#include "../MidiEvent/NoteOnEvent.h"
#include "../MidiEvent/OffEvent.h"
#include "../MidiEvent/TempoChangeEvent.h"
#include "../MidiEvent/TimeSignatureEvent.h"
#include "../midi/MidiChannel.h"
#include "../midi/MidiFile.h"
#include "../midi/MidiInput.h"
#include "../midi/MidiPlayer.h"
#include "../midi/MidiTrack.h"
#include "../protocol/Protocol.h"
#include "../tool/EditorTool.h"
#include "../tool/Selection.h"
#include "../tool/Tool.h"
#include "../gui/Appearance.h"

#include <QList>
#include <QPainterPath>
#include <QSettings>
#include <QThreadPool>
#include <QRunnable>
#include <QMutexLocker>
#include <QThread>


// QRunnable implementation for async rendering
class MatrixRenderTask : public QRunnable {
public:
    MatrixRenderTask(MatrixWidget *widget) : _widget(widget) {
        setAutoDelete(true); // Automatically delete when finished
    }

    void run() override {
        if (_widget) {
            _widget->renderPixmapAsync();
            _widget->_renderingInProgress.storeRelaxed(0); // Mark as finished
        }
    }

private:
    MatrixWidget *_widget;
};

MatrixWidget::MatrixWidget(QWidget *parent)
    : PaintWidget(parent) {
    // Initialize render data cache
    _renderData = nullptr;

    // Pure renderer initialization - business logic handled by HybridMatrixWidget
    screen_locked = false;
    file = 0;
    pixmap = 0;

    // Initialize event list pointers (will be set from render data)
    objects = nullptr;
    velocityObjects = nullptr;
    currentTempoEvents = nullptr;
    currentTimeSignatureEvents = nullptr;

    // Initialize legacy state (populated from render data)
    startTimeX = 0;
    startLineY = 50;
    endTimeX = 0;
    endLineY = 0;
    scaleX = 1;
    scaleY = 1;
    lineNameWidth = 110;
    timeHeight = 50;
    msOfFirstEventInList = 0;
    _div = 2;

    // Piano event for compatibility (HybridMatrixWidget handles actual piano emulation)
    pianoEvent = new NoteOnEvent(0, 100, 0, 0);

    // Widget configuration for rendering
    setMouseTracking(true);
    setFocusPolicy(Qt::ClickFocus);
    setRepaintOnMouseMove(false);


    setRepaintOnMousePress(false);
    setRepaintOnMouseRelease(false);

    // Initialize viewport cache
    _lastStartTick = _lastEndTick = _lastStartLineY = _lastEndLineY = -1;
    _lastScaleX = _lastScaleY = -1.0;

    // Initialize settings first
    _settings = new QSettings(QString("MidiEditor"), QString("NONE"));

    _useAsyncRendering = _settings->value("rendering/async_rendering", false).toBool();
    _renderingInProgress.storeRelaxed(0);
}

MatrixWidget::~MatrixWidget() {
    // Wait for async rendering to complete before destroying
    if (_useAsyncRendering) {
        // Wait for any running rendering tasks to complete
        while (_renderingInProgress.loadRelaxed() != 0) {
            QThread::msleep(10); // Wait 10ms and check again
        }
        // Wait for thread pool to finish any remaining tasks
        QThreadPool::globalInstance()->waitForDone(5000); // 5 second timeout
    }

    // Clear cache before destruction (good practice)
    clearSoftwareCache();


    delete _settings;
    delete _renderData;
}

// NEW: Pure renderer interface - receives data from HybridMatrixWidget
void MatrixWidget::setRenderData(const MatrixRenderData &data) {
    if (!_renderData) {
        _renderData = new MatrixRenderData();
    }
    *_renderData = data;

    // Update file reference from render data
    file = data.file;

    // Update ALL legacy state variables for rendering compatibility
    screen_locked = data.screenLocked;
    _colorsByChannels = data.colorsByChannels;
    _div = data.div;

    // Update viewport state
    startTick = data.startTick;
    endTick = data.endTick;
    startTimeX = data.startTimeX;
    endTimeX = data.endTimeX;
    startLineY = data.startLineY;
    endLineY = data.endLineY;
    timeHeight = data.timeHeight;
    lineNameWidth = data.lineNameWidth;
    scaleX = data.scaleX;
    scaleY = data.scaleY;
    msOfFirstEventInList = data.msOfFirstEventInList;

    // Update UI areas
    ToolArea = data.toolArea;
    PianoArea = data.pianoArea;
    TimeLineArea = data.timeLineArea;
    pianoKeys = data.pianoKeys;

    // Update PaintWidget mouse coordinates for mouseInRect to work
    mouseX = data.mouseX;
    mouseY = data.mouseY;
    mouseOver = data.mouseOver;

    // Update event list pointers (no copying for performance)
    objects = data.objects;
    velocityObjects = data.velocityObjects;
    currentTempoEvents = data.tempoEvents;
    currentTimeSignatureEvents = data.timeSignatureEvents;

    // Update divisions
    currentDivs = data.divs;

    // Invalidate pixmap cache to force redraw
    delete pixmap;
    pixmap = 0;


    update();
}

void MatrixWidget::setScreenLocked(bool b) {
    screen_locked = b;
}

bool MatrixWidget::screenLocked() {
    return screen_locked;
}

void MatrixWidget::paintEvent(QPaintEvent *event) {
    // Handle startup case when no file is loaded yet - just like original MatrixWidget
    if (!file) {
        // Original MatrixWidget just returns early, showing default widget background
        // There should always be a default file created by MainWindow after 200ms
        return;
    }

    QPainter *painter = new QPainter(this);

    QFont font = painter->font();
    font.setPixelSize(12);
    painter->setFont(font);
    painter->setClipping(false);

    bool totalRepaint = !pixmap;

    if (totalRepaint) {
        if (!_renderData) {
            delete painter;
            return;
        }

        // Check if viewport has changed significantly to optimize repaints
        bool viewportChanged = (_lastStartTick != _renderData->startTick || _lastEndTick != _renderData->endTick ||
                                _lastStartLineY != _renderData->startLineY || _lastEndLineY != _renderData->endLineY ||
                                _lastScaleX != _renderData->scaleX || _lastScaleY != _renderData->scaleY);

        // Update viewport cache
        _lastStartTick = _renderData->startTick;
        _lastEndTick = _renderData->endTick;
        _lastStartLineY = _renderData->startLineY;
        _lastEndLineY = _renderData->endLineY;
        _lastScaleX = _renderData->scaleX;
        _lastScaleY = _renderData->scaleY;

        // Handle async rendering if enabled
        if (_useAsyncRendering && viewportChanged) {
            // Wait for any previous async rendering to complete
            if (_renderingInProgress.loadRelaxed() != 0) {
                // Previous rendering still in progress, wait for it
                while (_renderingInProgress.loadRelaxed() != 0) {
                    QThread::msleep(1); // Brief wait
                }
            }

            // Start async rendering in background thread
            startAsyncRendering();

            // For now, draw a placeholder while async rendering is in progress
            painter->fillRect(rect(), QColor(50, 50, 50));
            painter->setPen(Qt::white);
            painter->drawText(10, 20, "Rendering...");
            delete painter;
            return;
        }

        this->pianoKeys.clear();

        QSize pixmapSize(width(), height());
        qreal devicePixelRatio = devicePixelRatioF();

        // Create pixmap with proper device pixel ratio for high-DPI displays
        pixmap = new QPixmap(pixmapSize * devicePixelRatio);
        pixmap->setDevicePixelRatio(devicePixelRatio);

        // Fill with background color to avoid artifacts
        pixmap->fill(Appearance::backgroundColor());
        QPainter *pixpainter = new QPainter(pixmap);

        // Configure rendering hints based on user settings
        if (!QApplication::arguments().contains("--no-antialiasing")) {
            pixpainter->setRenderHint(QPainter::Antialiasing);
        }

        // Apply user-configurable performance settings
        bool smoothPixmapTransform = _settings->value("rendering/smooth_pixmap_transform", true).toBool();
        bool losslessImageRendering = _settings->value("rendering/lossless_image_rendering", true).toBool();
        bool optimizedComposition = _settings->value("rendering/optimized_composition", false).toBool();

        pixpainter->setRenderHint(QPainter::SmoothPixmapTransform, smoothPixmapTransform);
        pixpainter->setRenderHint(QPainter::LosslessImageRendering, losslessImageRendering);

        // Use optimized composition mode if enabled
        if (optimizedComposition) {
            pixpainter->setCompositionMode(QPainter::CompositionMode_SourceOver);
        }
        // background shade
        pixpainter->fillRect(0, 0, width(), height(), Appearance::backgroundColor());

        QFont f = pixpainter->font();
        f.setPixelSize(12);
        pixpainter->setFont(f);
        pixpainter->setClipping(false);

        // Use render data event lists instead of local ones
        if (_renderData->objects) {
            for (int i = 0; i < _renderData->objects->length(); i++) {
                _renderData->objects->at(i)->setShown(false);
                OnEvent *onev = dynamic_cast<OnEvent *>(_renderData->objects->at(i));
                if (onev && onev->offEvent()) {
                    onev->offEvent()->setShown(false);
                }
            }
        }

        // Use render data values instead of calculating
        int startTick = _renderData->startTick;
        int endTick = _renderData->endTick;

        if (!_renderData->tempoEvents || _renderData->tempoEvents->isEmpty()) {
            pixpainter->fillRect(0, 0, width(), height(), Appearance::errorColor());
            delete pixpainter;
            return;
        }

        TempoChangeEvent *ev = dynamic_cast<TempoChangeEvent *>(_renderData->tempoEvents->at(0));
        if (!ev) {
            pixpainter->fillRect(0, 0, width(), height(), Appearance::errorColor());
            delete pixpainter;
            return;
        }
        int numLines = _renderData->endLineY - _renderData->startLineY;
        if (numLines == 0) {
            delete pixpainter;
            return;
        }

        // fill background of the line descriptions
        pixpainter->fillRect(_renderData->pianoArea, Appearance::systemWindowColor());

        // fill the pianos background
        int pianoKeys = _renderData->numLines;
        if (_renderData->endLineY > 127) {
            pianoKeys -= (_renderData->endLineY - 127);
        }
        if (pianoKeys > 0) {
            pixpainter->fillRect(0, _renderData->timeHeight, _renderData->lineNameWidth - 10,
                                 pianoKeys * _renderData->lineHeight, Appearance::pianoWhiteKeyColor());
        }


        // draw background of lines, pianokeys and linenames. when i increase ,the tune decrease.
        for (int i = _renderData->startLineY; i <= _renderData->endLineY; i++) {
            int startLine = yPosOfLine(i);
            QColor c;
            if (i <= 127) {
                bool isHighlighted = false;
                bool isRangeLine = false;

                // Check for C3/C6 range lines if enabled
                if (Appearance::showRangeLines()) {
                    // C3 = MIDI note 48, C6 = MIDI note 84
                    // Matrix widget uses inverted indexing (127-i), so:
                    // For C3: 127-48 = 79
                    // For C6: 127-84 = 43
                    if (i == 79 || i == 43) { // C3 or C6 lines
                        isRangeLine = true;
                    }
                }

                Appearance::stripStyle strip = Appearance::strip();
                switch (strip) {
                    case Appearance::onOctave:
                        // MIDI note 0 = C, so we want (127-i) % 12 == 0 for C notes
                        // Since i is inverted (127-i gives actual MIDI note), we need:
                        isHighlighted = ((127 - static_cast<unsigned int>(i)) % 12) == 0; // Highlight C notes (octave boundaries)
                        break;
                    case Appearance::onSharp:
                        isHighlighted = !((1 << (static_cast<unsigned int>(i) % 12)) & sharp_strip_mask);
                        break;
                    case Appearance::onEven:
                        isHighlighted = (static_cast<unsigned int>(i) % 2);
                        break;
                }

                if (isRangeLine) {
                    c = Appearance::rangeLineColor(); // Range line color (C3/C6)
                } else if (isHighlighted) {
                    c = Appearance::stripHighlightColor();
                } else {
                    c = Appearance::stripNormalColor();
                }
            } else {
                // Program events section (lines >127) - use different colors than strips
                if (i % 2 == 1) {
                    c = Appearance::programEventHighlightColor();
                } else {
                    c = Appearance::programEventNormalColor();
                }
            }
            pixpainter->fillRect(_renderData->lineNameWidth, startLine, width(), startLine + _renderData->lineHeight, c);
        }

        // paint measures and timeline background
        pixpainter->fillRect(0, 0, width(), _renderData->timeHeight, Appearance::systemWindowColor());

        pixpainter->setClipping(true);
        pixpainter->setClipRect(_renderData->lineNameWidth, 0, width() - _renderData->lineNameWidth - 2, height());

        pixpainter->setPen(Appearance::darkGrayColor());
        pixpainter->setBrush(Appearance::pianoWhiteKeyColor());
        pixpainter->drawRect(_renderData->lineNameWidth, 2, width() - _renderData->lineNameWidth - 1, _renderData->timeHeight - 2);
        pixpainter->setPen(Appearance::foregroundColor());

        pixpainter->fillRect(0, _renderData->timeHeight - 3, width(), 3, Appearance::systemWindowColor());

        // paint time text in ms
        int numbers = (width() - _renderData->lineNameWidth) / 80;
        if (numbers > 0) {
            int step = (_renderData->endTimeX - _renderData->startTimeX) / numbers;
            int realstep = 1;
            int nextfak = 2;
            int tenfak = 1;
            while (realstep <= step) {
                realstep = nextfak * tenfak;
                if (nextfak == 1) {
                    nextfak++;
                    continue;
                }
                if (nextfak == 2) {
                    nextfak = 5;
                    continue;
                }
                if (nextfak == 5) {
                    nextfak = 1;
                    tenfak *= 10;
                }
            }
            int startNumber = ((_renderData->startTimeX) / realstep);
            startNumber *= realstep;
            if (startNumber < _renderData->startTimeX) {
                startNumber += realstep;
            }
            if (Appearance::shouldUseDarkMode()) {
                pixpainter->setPen(QColor(200, 200, 200)); // Light gray for dark mode
            } else {
                pixpainter->setPen(Qt::gray); // Original color for light mode
            }
            while (startNumber < _renderData->endTimeX) {
                int pos = xPosOfMs(startNumber);
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
                int textlength = QFontMetrics(pixpainter->font()).horizontalAdvance(text);
                if (startNumber > 0) {
                    pixpainter->drawText(pos - textlength / 2, _renderData->timeHeight / 2 - 6, text);
                }
                pixpainter->drawLine(pos, _renderData->timeHeight / 2 - 1, pos, _renderData->timeHeight);
                startNumber += realstep;
            }
        }

        // draw measures foreground and text using pre-calculated data
        int measure = _renderData->measure;

        if (!_renderData->timeSignatureEvents || _renderData->timeSignatureEvents->isEmpty()) {
            return;
        }

        TimeSignatureEvent *currentEvent = _renderData->timeSignatureEvents->at(0);
        int i = 0;
        if (!currentEvent) {
            return;
        }
        int tick = currentEvent->midiTime();
        while (tick + currentEvent->ticksPerMeasure() <= startTick) {
            tick += currentEvent->ticksPerMeasure();
        }
        while (tick < endTick) {
            TimeSignatureEvent *measureEvent = _renderData->timeSignatureEvents->at(i);
            int xfrom = xPosOfMs(msOfTick(tick));
            measure++;
            int measureStartTick = tick;
            tick += currentEvent->ticksPerMeasure();
            if (i < _renderData->timeSignatureEvents->length() - 1) {
                if (_renderData->timeSignatureEvents->at(i + 1)->midiTime() <= tick) {
                    currentEvent = _renderData->timeSignatureEvents->at(i + 1);
                    tick = currentEvent->midiTime();
                    i++;
                }
            }
            int xto = xPosOfMs(msOfTick(tick));
            pixpainter->setBrush(Appearance::measureBarColor());
            pixpainter->setPen(Qt::NoPen);
            pixpainter->drawRoundedRect(xfrom + 2, _renderData->timeHeight / 2 + 4, xto - xfrom - 4,  _renderData->timeHeight / 2 - 10, 5, 5);
            if (tick > startTick) {
                pixpainter->setPen(Appearance::measureLineColor());
                pixpainter->drawLine(xfrom, _renderData->timeHeight / 2, xfrom, height());
                QString text = tr("Measure ") + QString::number(measure - 1);

                // Improve text rendering for high DPI displays
                QFont font = Appearance::improveFont(pixpainter->font());
                pixpainter->setFont(font);

                QFontMetrics fm(font);
                int textlength = fm.horizontalAdvance(text);
                if (textlength > xto - xfrom) {
                    text = QString::number(measure - 1);
                    textlength = fm.horizontalAdvance(text);
                }

                // Align text to pixel boundaries for sharper rendering
                int pos = (xfrom + xto) / 2;
                int textX = qRound(pos - textlength / 2.0);
                int textY = _renderData->timeHeight - 9;

                pixpainter->setPen(Appearance::measureTextColor());
                pixpainter->drawText(textX, textY, text);

                // Draw division lines using pre-calculated divisions from render data
                QPen oldPen = pixpainter->pen();
                QPen dashPen = QPen(Appearance::timelineGridColor(), 1, Qt::DashLine);
                pixpainter->setPen(dashPen);

                // Draw all division lines that fall within this measure
                for (const QPair<int, int> &div: _renderData->divs) {
                    int xDiv = div.first;
                    int divTick = div.second;

                    // Only draw divisions within this measure
                    if (divTick > measureStartTick && divTick < tick && xDiv >= xfrom && xDiv <= xto) {
                        pixpainter->drawLine(xDiv, _renderData->timeHeight, xDiv, height());
                    }
                }
                pixpainter->setPen(oldPen);
            }
        }

        // line between time texts and matrix area
        pixpainter->setPen(Appearance::borderColor());
        pixpainter->drawLine(0, _renderData->timeHeight, width(), _renderData->timeHeight);
        pixpainter->drawLine(_renderData->lineNameWidth, _renderData->timeHeight, _renderData->lineNameWidth, height());

        pixpainter->setPen(Appearance::foregroundColor());

        // paint the events
        pixpainter->setClipping(true);
        pixpainter->setClipRect(_renderData->lineNameWidth, _renderData->timeHeight, width() - _renderData->lineNameWidth, height() - _renderData->timeHeight);

        for (int i = 0; i < 19; i++) {
            paintChannel(pixpainter, i);
        }

        pixpainter->setClipping(false);

        pixpainter->setPen(Appearance::foregroundColor());

        delete pixpainter;
    }

    painter->drawPixmap(QRect(0, 0, width(), height()), *pixmap, pixmap->rect());

    painter->setRenderHint(QPainter::Antialiasing);
    // draw the piano / linenames
    for (int i = _renderData->startLineY; i <= _renderData->endLineY; i++) {
        int startLine = yPosOfLine(i);
        if (i >= 0 && i <= 127) {
            paintPianoKey(painter, 127 - i, 0, startLine,
                          _renderData->lineNameWidth, _renderData->lineHeight);
        } else {
            QString text = "";
            switch (i) {
                case MidiEvent::CONTROLLER_LINE: {
                    text = tr("Control Change");
                    break;
                }
                case MidiEvent::TEMPO_CHANGE_EVENT_LINE: {
                    text = tr("Tempo Change");
                    break;
                }
                case MidiEvent::TIME_SIGNATURE_EVENT_LINE: {
                    text = tr("Time Signature");
                    break;
                }
                case MidiEvent::KEY_SIGNATURE_EVENT_LINE: {
                    text = tr("Key Signature.");
                    break;
                }
                case MidiEvent::PROG_CHANGE_LINE: {
                    text = tr("Program Change");
                    break;
                }
                case MidiEvent::KEY_PRESSURE_LINE: {
                    text = tr("Key Pressure");
                    break;
                }
                case MidiEvent::CHANNEL_PRESSURE_LINE: {
                    text = tr("Channel Pressure");
                    break;
                }
                case MidiEvent::TEXT_EVENT_LINE: {
                    text = tr("Text");
                    break;
                }
                case MidiEvent::PITCH_BEND_LINE: {
                    text = tr("Pitch Bend");
                    break;
                }
                case MidiEvent::SYSEX_LINE: {
                    text = tr("System Exclusive");
                    break;
                }
                case MidiEvent::UNKNOWN_LINE: {
                    text = tr("(Unknown)");
                    break;
                }
            }
            if (Appearance::shouldUseDarkMode()) {
                painter->setPen(QColor(200, 200, 200)); // Light gray for dark mode
            } else {
                painter->setPen(Qt::darkGray); // Original color for light mode
            }
            font = painter->font();
            font.setPixelSize(10);
            painter->setFont(font);
            int textlength = QFontMetrics(font).horizontalAdvance(text);
            painter->drawText(_renderData->lineNameWidth - 15 - textlength, startLine + _renderData->lineHeight, text);
        }
    }
    if (Tool::currentTool()) {
        painter->setClipping(true);
        painter->setClipRect(_renderData->toolArea);
        Tool::currentTool()->draw(painter);
        painter->setClipping(false);
    }

    // Draw cursor line when mouse is in timeline area (enabled check removed - pure renderer)
    if (mouseInRect(_renderData->timeLineArea)) {
        painter->setPen(Appearance::playbackCursorColor());
        painter->drawLine(_renderData->mouseX, 0, _renderData->mouseX, height());
        painter->setPen(Appearance::foregroundColor());
    }

    if (MidiPlayer::isPlaying()) {
        painter->setPen(Appearance::playbackCursorColor());
        int x = xPosOfMs(MidiPlayer::timeMs());
        if (x >= _renderData->lineNameWidth) {
            painter->drawLine(x, 0, x, height());
        }
        painter->setPen(Appearance::foregroundColor());
    }

    // paint the cursorTick of file
    if (midiFile()->cursorTick() >= _renderData->startTick && midiFile()->cursorTick() <= _renderData->endTick) {
        painter->setPen(Qt::darkGray); // Original color for both modes
        int x = xPosOfMs(msOfTick(midiFile()->cursorTick()));
        painter->drawLine(x, 0, x, height());
        QPointF points[3] = {
            QPointF(x - 8, _renderData->timeHeight / 2 + 2),
            QPointF(x + 8, _renderData->timeHeight / 2 + 2),
            QPointF(x, _renderData->timeHeight - 2),
        };

        if (Appearance::shouldUseDarkMode()) {
            painter->setBrush(QBrush(Appearance::cursorTriangleColor(), Qt::SolidPattern));
        } else {
            painter->setBrush(QBrush(QColor(194, 230, 255), Qt::SolidPattern)); // Original color
        }

        painter->drawPolygon(points, 3);
        painter->setPen(Qt::gray); // Original color for both modes
    }

    // paint the pauseTick of file if >= 0
    if (!MidiPlayer::isPlaying() && midiFile()->pauseTick() >= _renderData->startTick && midiFile()->pauseTick() <= _renderData->endTick) {
        int x = xPosOfMs(msOfTick(midiFile()->pauseTick()));

        QPointF points[3] = {
            QPointF(x - 8, _renderData->timeHeight / 2 + 2),
            QPointF(x + 8, _renderData->timeHeight / 2 + 2),
            QPointF(x, _renderData->timeHeight - 2),
        };

        painter->setBrush(QBrush(Appearance::grayColor(), Qt::SolidPattern));

        painter->drawPolygon(points, 3);
    }

    // border
    painter->setPen(Appearance::borderColor());
    painter->drawLine(width() - 1, height() - 1, _renderData->lineNameWidth, height() - 1);
    painter->drawLine(width() - 1, height() - 1, width() - 1, 2);

    // if the recorder is recording, show red circle
    if (MidiInput::recording()) {
        painter->setBrush(Appearance::recordingIndicatorColor());
        painter->drawEllipse(width() - 20, _renderData->timeHeight + 5, 15, 15);
    }
    delete painter;

    // if MouseRelease was not used, delete it
    mouseReleased = false;

    if (totalRepaint) {
        emit objectListChanged();
    }
}

void MatrixWidget::paintChannel(QPainter *painter, int channel) {
    if (!file->channel(channel)->visible()) {
        return;
    }
    QColor cC = *file->channel(channel)->color();

    // filter events
    QMultiMap<int, MidiEvent *> *map = file->channelEvents(channel);

    // Early exit for empty channels
    if (!map || map->isEmpty()) {
        return;
    }

    // Two-pass approach for performance while maintaining correctness:
    // Pass 1: Process events that start within an extended viewport (for performance)
    // Pass 2: For note events, check for long notes that start before extended viewport

    // Calculate extended search range - look back/forward based on typical note lengths
    int startTick = _renderData->startTick;
    int endTick = _renderData->endTick;
    int startLineY = _renderData->startLineY;
    int endLineY = _renderData->endLineY;

    int maxNoteLengthEstimate = qMin(endTick - startTick, file->ticksPerQuarter() * 16); // Max 16 beats
    int searchStartTick = startTick - maxNoteLengthEstimate;
    int searchEndTick = endTick + maxNoteLengthEstimate;
    if (searchStartTick < 0) searchStartTick = 0;

    QMultiMap<int, MidiEvent *>::iterator it = map->lowerBound(searchStartTick);
    QMultiMap<int, MidiEvent *>::iterator endIt = map->upperBound(searchEndTick);

    while (it != endIt && it != map->end()) {
        MidiEvent *event = it.value();
        // Quick Y-axis (line) culling before expensive eventInWidget() call
        int line = event->line();
        if (line < startLineY - 1 || line > endLineY + 1) {
            it++;
            continue;
        }

        if (eventInWidget(event)) {
            // insert all Events in objects, set their coordinates
            // Only onEvents are inserted. When there is an On
            // and an OffEvent, the OnEvent will hold the coordinates

            OffEvent *offEvent = dynamic_cast<OffEvent *>(event);
            OnEvent *onEvent = dynamic_cast<OnEvent *>(event);

            int x, width;
            int y = yPosOfLine(line);
            int height = lineHeight();

            if (onEvent || offEvent) {
                if (onEvent) {
                    offEvent = onEvent->offEvent();
                } else if (offEvent) {
                    onEvent = dynamic_cast<OnEvent *>(offEvent->onEvent());
                }

                width = xPosOfMs(msOfTick(offEvent->midiTime())) - xPosOfMs(msOfTick(onEvent->midiTime()));
                x = xPosOfMs(msOfTick(onEvent->midiTime()));
                event = onEvent;
                if (objects->contains(event)) {
                    it++;
                    continue;
                }
            } else {
                width = _renderData->pixelPerEvent;
                x = xPosOfMs(msOfTick(event->midiTime()));
            }

            event->setX(x);
            event->setY(y);
            event->setWidth(width);
            event->setHeight(height);

            if (!(event->track()->hidden())) {
                if (!_colorsByChannels) {
                    cC = *event->track()->color();
                }
                event->draw(painter, cC);

                if (Selection::instance()->selectedEvents().contains(event)) {
                    painter->setPen(Qt::gray); // Original color for both modes
                    painter->drawLine(_renderData->lineNameWidth, y, this->width(), y);
                    painter->drawLine(_renderData->lineNameWidth, y + _renderData->pixelPerEvent, this->width(), y + _renderData->pixelPerEvent);
                    painter->setPen(Appearance::foregroundColor());
                }
                objects->prepend(event);
            }
        }

        if (!(event->track()->hidden())) {
            // append event to velocityObjects if its not a offEvent and if it
            // is in the x-Area
            OffEvent *offEvent = dynamic_cast<OffEvent *>(event);
            if (!offEvent && event->midiTime() >= startTick && event->midiTime() <= endTick && !velocityObjects-> contains(event)) {
                event->setX(xPosOfMs(msOfTick(event->midiTime())));

                velocityObjects->prepend(event);
            }
        }
        it++;
    }

    // Fallback: Check for very long notes that might have been missed
    // This ensures we don't break the original bug fix for long sustains
    if (searchStartTick > 0) {
        // Look for OnEvents that start before our search range but might still be visible
        QMultiMap<int, MidiEvent *>::iterator fallbackIt = map->begin();
        QMultiMap<int, MidiEvent *>::iterator fallbackEnd = map->lowerBound(searchStartTick);

        while (fallbackIt != fallbackEnd) {
            MidiEvent *event = fallbackIt.value();
            OnEvent *onEvent = dynamic_cast<OnEvent *>(event);

            // Only check OnEvents (notes) that might span the viewport
            if (onEvent && onEvent->offEvent()) {
                int noteEndTick = onEvent->offEvent()->midiTime();

                // If this note ends after our viewport starts, it should be visible
                if (noteEndTick > startTick && !objects->contains(onEvent)) {
                    if (eventInWidget(onEvent)) {
                        int line = onEvent->line();
                        int x = xPosOfMs(msOfTick(onEvent->midiTime()));
                        int y = yPosOfLine(line);
                        int width = xPosOfMs(msOfTick(noteEndTick)) - x;
                        int height = lineHeight();

                        onEvent->setX(x);
                        onEvent->setY(y);
                        onEvent->setWidth(width);
                        onEvent->setHeight(height);

                        if (!onEvent->track()->hidden()) {
                            QColor color = _colorsByChannels ? cC : *onEvent->track()->color();
                            onEvent->draw(painter, color);

                            if (Selection::instance()->selectedEvents().contains(onEvent)) {
                                painter->setPen(Qt::gray);
                                painter->drawLine(_renderData->lineNameWidth, y, this->width(), y);
                                painter->drawLine(_renderData->lineNameWidth, y + height, this->width(), y + height);
                                painter->setPen(Appearance::foregroundColor());
                            }
                            objects->prepend(onEvent);
                        }
                    }
                }
            }
            fallbackIt++;
        }
    }
}

void MatrixWidget::paintPianoKey(QPainter *painter, int number, int x, int y,
                                 int width, int height) {
    int borderRight = 10;
    width = width - borderRight;
    if (number >= 0 && number <= 127) {
        double scaleHeightBlack = 0.5;
        double scaleWidthBlack = 0.6;

        bool isBlack = false;
        bool blackOnTop = false;
        bool blackBeneath = false;
        QString name = "";

        switch (number % 12) {
            case 0: {
                // C
                blackOnTop = true;
                name = "";
                int i = number / 12;
                //if(i<4){
                //  name="C";{
                //      for(int j = 0; j<3-i; j++){
                //          name+="'";
                //      }
                //  }
                //} else {
                //  name = "c";
                //  for(int j = 0; j<i-4; j++){
                //      name+="'";
                //  }
                //}
                name = "C" + QString::number(i - 1);
                break;
            }
            // Cis
            case 1: {
                isBlack = true;
                break;
            }
            // D
            case 2: {
                blackOnTop = true;
                blackBeneath = true;
                break;
            }
            // Dis
            case 3: {
                isBlack = true;
                break;
            }
            // E
            case 4: {
                blackBeneath = true;
                break;
            }
            // F
            case 5: {
                blackOnTop = true;
                break;
            }
            // fis
            case 6: {
                isBlack = true;
                break;
            }
            // G
            case 7: {
                blackOnTop = true;
                blackBeneath = true;
                break;
            }
            // gis
            case 8: {
                isBlack = true;
                break;
            }
            // A
            case 9: {
                blackOnTop = true;
                blackBeneath = true;
                break;
            }
            // ais
            case 10: {
                isBlack = true;
                break;
            }
            // H
            case 11: {
                blackBeneath = true;
                break;
            }
        }

        if (127 - number == _renderData->startLineY) {
            blackOnTop = false;
        }

        bool selected = _renderData->mouseY >= y && _renderData->mouseY <= y + height && _renderData->mouseX > _renderData->lineNameWidth && _renderData->mouseOver;
        foreach(MidiEvent* event, Selection::instance()->selectedEvents()) {
            if (event->line() == 127 - number) {
                selected = true;
                break;
            }
        }

        QPolygon keyPolygon;

        bool inRect = false;
        if (isBlack) {
            painter->drawLine(x, y + height / 2, x + width, y + height / 2);
            y += (height - height * scaleHeightBlack) / 2;
            QRect playerRect;
            playerRect.setX(x);
            playerRect.setY(y);
            playerRect.setWidth(width * scaleWidthBlack);
            playerRect.setHeight(height * scaleHeightBlack + 0.5);
            QColor c = Appearance::pianoBlackKeyColor();
            if (mouseInRect(playerRect)) {
                c = Appearance::pianoBlackKeyHoverColor();
                inRect = true;
            }
            painter->fillRect(playerRect, c);

            keyPolygon.append(QPoint(x, y));
            keyPolygon.append(QPoint(x, y + height * scaleHeightBlack));
            keyPolygon.append(QPoint(x + width * scaleWidthBlack, y + height * scaleHeightBlack));
            keyPolygon.append(QPoint(x + width * scaleWidthBlack, y));
            pianoKeys.insert(number, playerRect);
        } else {
            if (!blackOnTop) {
                keyPolygon.append(QPoint(x, y));
                keyPolygon.append(QPoint(x + width, y));
            } else {
                keyPolygon.append(QPoint(x, y - height * scaleHeightBlack / 2));
                keyPolygon.append(QPoint(x + width * scaleWidthBlack, y - height * scaleHeightBlack / 2));
                keyPolygon.append(QPoint(x + width * scaleWidthBlack, y - height * scaleHeightBlack));
                keyPolygon.append(QPoint(x + width, y - height * scaleHeightBlack));
            }
            if (!blackBeneath) {
                painter->drawLine(x, y + height, x + width, y + height);
                keyPolygon.append(QPoint(x + width, y + height));
                keyPolygon.append(QPoint(x, y + height));
            } else {
                keyPolygon.append(QPoint(x + width, y + height + height * scaleHeightBlack));
                keyPolygon.append(QPoint(x + width * scaleWidthBlack, y + height + height * scaleHeightBlack));
                keyPolygon.append(QPoint(x + width * scaleWidthBlack, y + height + height * scaleHeightBlack / 2));
                keyPolygon.append(QPoint(x, y + height + height * scaleHeightBlack / 2));
            }
            inRect = mouseInRect(x, y, width, height);
            pianoKeys.insert(number, QRect(x, y, width, height));
        }

        if (isBlack) {
            if (inRect) {
                painter->setBrush(Appearance::pianoBlackKeyHoverColor());
            } else if (selected) {
                painter->setBrush(Appearance::pianoBlackKeySelectedColor());
            } else {
                painter->setBrush(Appearance::pianoBlackKeyColor());
            }
        } else {
            if (inRect) {
                painter->setBrush(Appearance::pianoWhiteKeyHoverColor());
            } else if (selected) {
                painter->setBrush(Appearance::pianoWhiteKeySelectedColor());
            } else {
                painter->setBrush(Appearance::pianoWhiteKeyColor());
            }
        }
        painter->setPen(Appearance::darkGrayColor());
        painter->drawPolygon(keyPolygon, Qt::OddEvenFill);

        if (name != "") {
            // Improve text rendering for piano key names
            QFont font = Appearance::improveFont(painter->font());
            painter->setFont(font);

            painter->setPen(Qt::gray); // Original color for both modes
            QFontMetrics fm(font);
            int textlength = fm.horizontalAdvance(name);

            // Align text to pixel boundaries for sharper rendering
            int textX = x + width - textlength - 2;
            int textY = y + height - 1;

            painter->drawText(textX, textY, name);
            painter->setPen(Appearance::foregroundColor());
        }
        if (inRect) {
            // mark the current Line (enabled check removed - pure renderer)
            QColor lineColor = Appearance::pianoKeyLineHighlightColor();
            painter->fillRect(x + width + borderRight, yPosOfLine(127 - number),
                              this->width() - x - width - borderRight, height, lineColor);
        }
    }
}

void MatrixWidget::setFile(MidiFile *f) {
    // Pure renderer - stores file reference for coordinate calculations

    file = f;


    clearSoftwareCache();

    if (file) {
        // Connect signals for rendering updates (this is the only thing we should do)
        connect(file->protocol(), SIGNAL(actionFinished()), this, SLOT(registerRelayout()));
        connect(file->protocol(), SIGNAL(actionFinished()), this, SLOT(update()));
    } else {
        // Clear event lists when no file
        objects = nullptr;
        velocityObjects = nullptr;
        currentTempoEvents = nullptr;
        currentTimeSignatureEvents = nullptr;
    }
}

void MatrixWidget::scrollXChanged(int scrollPositionX) {
    // Interface compatibility - scrolling handled by HybridMatrixWidget
}

void MatrixWidget::scrollYChanged(int scrollPositionY) {
    // Interface compatibility - scrolling handled by HybridMatrixWidget
}

void MatrixWidget::calcSizes() {
    // Interface compatibility - size calculations handled by HybridMatrixWidget
}

MidiFile *MatrixWidget::midiFile() {
    return file;
}


void MatrixWidget::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event); // Properly handle the resize event
    // Interface compatibility - resizing handled by HybridMatrixWidget
}


int MatrixWidget::yPosOfLine(int line) {
    if (!_renderData) return 0;
    return _renderData->timeHeight + (line - _renderData->startLineY) * _renderData->lineHeight;
}

double MatrixWidget::lineHeight() {
    if (!_renderData) return 0;
    if (_renderData->endLineY - _renderData->startLineY == 0) return 0;
    return (double) (height() - _renderData->timeHeight) / (double) (_renderData->endLineY - _renderData->startLineY);
}

void MatrixWidget::enterEvent(QEvent *event) {
    PaintWidget::enterEvent(event);
    // Tool handling is done by HybridMatrixWidget - pure renderer doesn't control updates
}

void MatrixWidget::leaveEvent(QEvent *event) {
    PaintWidget::leaveEvent(event);
    // Tool handling is done by HybridMatrixWidget - pure renderer doesn't control updates
}

void MatrixWidget::forceCompleteRedraw() {
    // Force complete redraw by invalidating cached pixmap
    delete pixmap;
    pixmap = 0;
    repaint();
}

void MatrixWidget::startAsyncRendering() {
    // Mark rendering as in progress
    _renderingInProgress.storeRelaxed(1);

    // Create and start async rendering task using QThreadPool
    MatrixRenderTask *task = new MatrixRenderTask(this);
    QThreadPool::globalInstance()->start(task);
}

void MatrixWidget::renderPixmapAsync() {
    // This method runs in a background thread
    QMutexLocker locker(&_renderMutex);

    if (!_renderData) return;

    QPixmap *newPixmap = new QPixmap(size() * devicePixelRatio());
    newPixmap->setDevicePixelRatio(devicePixelRatio());
    newPixmap->fill(Appearance::backgroundColor());

    QPainter pixpainter(newPixmap);
    pixpainter.setClipping(false);

    // Configure rendering hints
    if (!QApplication::arguments().contains("--no-antialiasing")) {
        pixpainter.setRenderHint(QPainter::Antialiasing);
    }

    // Apply performance settings
    bool smoothPixmapTransform = _settings->value("rendering/smooth_pixmap_transform", true).toBool();
    bool losslessImageRendering = _settings->value("rendering/lossless_image_rendering", true).toBool();
    bool optimizedComposition = _settings->value("rendering/optimized_composition", false).toBool();

    pixpainter.setRenderHint(QPainter::SmoothPixmapTransform, smoothPixmapTransform);
    pixpainter.setRenderHint(QPainter::LosslessImageRendering, losslessImageRendering);

    if (optimizedComposition) {
        pixpainter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    }

    // Render background
    pixpainter.fillRect(0, 0, width(), height(), Appearance::backgroundColor());

    // Render all channels (this is the expensive part)
    for (int channel = 0; channel < 16; channel++) {
        if (_renderData->file && _renderData->file->channel(channel)->visible()) {
            paintChannel(&pixpainter, channel);
        }
    }

    // When rendering is complete, update the main pixmap on the UI thread
    QMetaObject::invokeMethod(this, [this, newPixmap]() {
        QMutexLocker locker(&_renderMutex);
        delete pixmap;
        pixmap = newPixmap;
        update(); // Trigger repaint with new pixmap
    }, Qt::QueuedConnection);
}

void MatrixWidget::batchDrawEvents(QPainter *painter, const QList<MidiEvent *> &events, const QColor &color) {
    if (events.isEmpty()) return;

    painter->setPen(Appearance::borderColor());
    painter->setBrush(color);

    // Use QPainterPath for better performance with many rectangles
    QPainterPath path;
    for (MidiEvent *event: events) {
        if (event && !event->track()->hidden()) {
            QRectF rect(event->x(), event->y(), event->width(), event->height());
            path.addRoundedRect(rect, 1, 1);
        }
    }

    // Draw all rectangles in one operation
    if (!path.isEmpty()) {
        painter->drawPath(path);
    }
}

int MatrixWidget::msOfXPos(int x) {
    if (!_renderData) return 0;
    if (width() <= _renderData->lineNameWidth) return _renderData->startTimeX;
    return _renderData->startTimeX + ((x - _renderData->lineNameWidth) * (_renderData->endTimeX - _renderData->startTimeX)) / (width() - _renderData->lineNameWidth);
}

int MatrixWidget::xPosOfMs(int ms) {
    if (!_renderData) return 0;
    if (_renderData->endTimeX <= _renderData->startTimeX) return _renderData->lineNameWidth;
    return _renderData->lineNameWidth + (ms - _renderData->startTimeX) * (width() - _renderData->lineNameWidth) / (_renderData->endTimeX - _renderData->startTimeX);
}

int MatrixWidget::msOfTick(int tick) {
    // Use render data tempo events
    if (!_renderData || !_renderData->tempoEvents) return 0;
    return file->msOfTick(tick, _renderData->tempoEvents, _renderData->msOfFirstEventInList);
}

int MatrixWidget::timeMsOfWidth(int w) {
    if (!_renderData) return 0;
    return (w * (_renderData->endTimeX - _renderData->startTimeX)) / (width() - _renderData->lineNameWidth);
}

bool MatrixWidget::eventInWidget(MidiEvent *event) {
    if (!_renderData) return false;

    int startTick = _renderData->startTick;
    int endTick = _renderData->endTick;
    int startLineY = _renderData->startLineY;
    int endLineY = _renderData->endLineY;

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
        bool offIn = offLine >= _renderData->startLineY && offLine <= _renderData->endLineY && offTick >= _renderData->startTick && offTick <= _renderData->endTick;

        int onLine = on->line();
        int onTick = on->midiTime();
        bool onIn = onLine >= _renderData->startLineY && onLine <= _renderData->endLineY && onTick >= _renderData->startTick && onTick <= _renderData->endTick;

        // Check if note line is visible (same line for both on and off events)
        bool lineVisible = (onLine >= _renderData->startLineY && onLine <= _renderData->endLineY);

        // Check all possible time overlap scenarios:
        // 1. Note starts before viewport and ends after viewport (spans completely)
        // 2. Note starts before viewport and ends inside viewport
        // 3. Note starts inside viewport and ends after viewport
        // 4. Note starts and ends inside viewport
        // All of these can be captured by: note starts before viewport ends AND note ends after viewport starts
        bool timeOverlaps = (onTick < _renderData->endTick && offTick > _renderData->startTick);

        // Show note if:
        // 1. Either start or end is fully visible (both time and line), OR
        // 2. Note line is visible AND note overlaps viewport in time
        bool shouldShow = offIn || onIn || (lineVisible && timeOverlaps);

        off->setShown(shouldShow);
        on->setShown(shouldShow);

        return shouldShow;
    } else {
        int line = event->line();
        int tick = event->midiTime();
        bool shown = line >= startLineY && line <= endLineY && tick >= startTick && tick <= endTick;
        event->setShown(shown);

        return shown;
    }
}

int MatrixWidget::lineAtY(int y) {
    if (!_renderData) return 0;
    double lh = lineHeight();
    if (lh == 0) return _renderData->startLineY;
    return (y - _renderData->timeHeight) / lh + _renderData->startLineY;
}

// Duplicate method definitions removed - already defined above


void MatrixWidget::mouseDoubleClickEvent(QMouseEvent *event) {
    // Business logic is handled by HybridMatrixWidget - just update rendering
    update();
}

void MatrixWidget::clearSoftwareCache() {
    // Clear pixmap cache
    if (pixmap) {
        delete pixmap;
        pixmap = nullptr;
    }

    // Clear piano key cache
    pianoKeys.clear();

    // Reset viewport cache
    _lastStartTick = -1;
    _lastEndTick = -1;
    _lastStartLineY = -1;
    _lastEndLineY = -1;
    _lastScaleX = -1.0;
    _lastScaleY = -1.0;
}

void MatrixWidget::registerRelayout() {
    delete pixmap;
    pixmap = 0;
}
