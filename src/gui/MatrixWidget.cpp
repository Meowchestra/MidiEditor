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
#include "../midi/PlayerThread.h"
#include "../protocol/Protocol.h"
#include "../tool/EditorTool.h"
#include "../tool/EventTool.h"
#include "../tool/Selection.h"
#include "../tool/Tool.h"
#include "../gui/Appearance.h"
#include "../midi/MidiOutput.h"

#include <QList>
#include <QtCore/qmath.h>

#define NUM_LINES 139
#define PIXEL_PER_S 100
#define PIXEL_PER_LINE 11
#define PIXEL_PER_EVENT 15

MatrixWidget::MatrixWidget(QWidget* parent)
    : PaintWidget(parent) {

    screen_locked = false;
    startTimeX = 0;
    startLineY = 50;
    endTimeX = 0;
    endLineY = 0;
    file = 0;
    scaleX = 1;
    pianoEvent = new NoteOnEvent(0, 100, 0, 0);
    scaleY = 1;
    lineNameWidth = 110;
    timeHeight = 50;
    currentTempoEvents = new QList<MidiEvent*>;
    currentTimeSignatureEvents = new QList<TimeSignatureEvent*>;
    msOfFirstEventInList = 0;
    objects = new QList<MidiEvent*>;
    velocityObjects = new QList<MidiEvent*>;
    EditorTool::setMatrixWidget(this);
    setMouseTracking(true);
    setFocusPolicy(Qt::ClickFocus);

    setRepaintOnMouseMove(false);
    setRepaintOnMousePress(false);
    setRepaintOnMouseRelease(false);

    connect(MidiPlayer::playerThread(), SIGNAL(timeMsChanged(int)),
            this, SLOT(timeMsChanged(int)));

    pixmap = 0;
    _div = 2;

    #ifdef QT_DEBUG
    _lastRenderEventCount = 0;
    _lastUsedLOD = false;
    #endif

    // Initialize viewport cache
    _lastStartTick = _lastEndTick = _lastStartLineY = _lastEndLineY = -1;
    _lastScaleX = _lastScaleY = -1.0;
    _showingPerformanceWarning = false;

    // Initialize Qt6 threading
    _useAsyncRendering = false; // Disabled by default for stability
}

void MatrixWidget::setScreenLocked(bool b) {
    screen_locked = b;
}

bool MatrixWidget::screenLocked() {
    return screen_locked;
}

void MatrixWidget::timeMsChanged(int ms, bool ignoreLocked) {

    if (!file)
        return;

    int x = xPosOfMs(ms);

    if ((!screen_locked || ignoreLocked) && (x < lineNameWidth || ms < startTimeX || ms > endTimeX || x > width() - 100)) {

        // return if the last tick is already shown
        if (file->maxTime() <= endTimeX && ms >= startTimeX) {
            repaint();
            return;
        }

        // sets the new position and repaints
        emit scrollChanged(ms, (file->maxTime() - endTimeX + startTimeX), startLineY,
                           NUM_LINES - (endLineY - startLineY));
    } else {
        repaint();
    }
}

void MatrixWidget::scrollXChanged(int scrollPositionX) {

    if (!file)
        return;

    startTimeX = scrollPositionX;
    endTimeX = startTimeX + ((width() - lineNameWidth) * 1000) / (PIXEL_PER_S * scaleX);

    // more space than needed: scale x
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
    registerRelayout();
    repaint();
}

void MatrixWidget::scrollYChanged(int scrollPositionY) {
    if (!file)
        return;

    startLineY = scrollPositionY;

    double space = height() - timeHeight;
    double lineSpace = scaleY * PIXEL_PER_LINE;
    double linesInWidget = space / lineSpace;
    endLineY = startLineY + linesInWidget;

    if (endLineY > NUM_LINES) {
        int d = endLineY - NUM_LINES;
        endLineY = NUM_LINES;
        startLineY -= d;
        if (startLineY < 0) {
            startLineY = 0;
        }
    }
    registerRelayout();
    repaint();
}

void MatrixWidget::paintEvent(QPaintEvent* event) {

    if (!file)
        return;

    QPainter* painter = new QPainter(this);
    QFont font = painter->font();
    font.setPixelSize(12);
    painter->setFont(font);
    painter->setClipping(false);

    bool totalRepaint = !pixmap;

    if (totalRepaint) {
        // Check if viewport has changed significantly to optimize repaints
        bool viewportChanged = (_lastStartTick != startTick || _lastEndTick != endTick ||
                               _lastStartLineY != startLineY || _lastEndLineY != endLineY ||
                               _lastScaleX != scaleX || _lastScaleY != scaleY);

        // Update viewport cache
        _lastStartTick = startTick; _lastEndTick = endTick;
        _lastStartLineY = startLineY; _lastEndLineY = endLineY;
        _lastScaleX = scaleX; _lastScaleY = scaleY;

        this->pianoKeys.clear();

        // Qt6 optimized pixmap creation with device pixel ratio support
        QSize pixmapSize(width(), height());
        qreal devicePixelRatio = devicePixelRatioF();

        // Create pixmap with proper device pixel ratio for high-DPI displays
        pixmap = new QPixmap(pixmapSize * devicePixelRatio);
        pixmap->setDevicePixelRatio(devicePixelRatio);

        // Fill with background color to avoid artifacts
        pixmap->fill(Appearance::backgroundColor());
        QPainter* pixpainter = new QPainter(pixmap);

        // Qt6 optimized rendering hints
        if (!QApplication::arguments().contains("--no-antialiasing")) {
            pixpainter->setRenderHint(QPainter::Antialiasing);
        }

        // Qt6 performance optimizations
        pixpainter->setRenderHint(QPainter::SmoothPixmapTransform, false); // Disable for performance
        pixpainter->setRenderHint(QPainter::LosslessImageRendering, false); // Prefer speed over quality when zoomed out

        // Enable Qt6 optimized composition modes
        if (getRenderTier() != RENDER_FULL_DETAIL) {
            pixpainter->setCompositionMode(QPainter::CompositionMode_SourceOver); // Fastest composition
        }
        // background shade
        pixpainter->fillRect(0, 0, width(), height(), Appearance::backgroundColor());

        QFont f = pixpainter->font();
        f.setPixelSize(12);
        pixpainter->setFont(f);
        pixpainter->setClipping(false);

        for (int i = 0; i < objects->length(); i++) {
            objects->at(i)->setShown(false);
            OnEvent* onev = dynamic_cast<OnEvent*>(objects->at(i));
            if (onev && onev->offEvent()) {
                onev->offEvent()->setShown(false);
            }
        }
        objects->clear();
        velocityObjects->clear();
        currentTempoEvents->clear();
        currentTimeSignatureEvents->clear();
        currentDivs.clear();

        startTick = file->tick(startTimeX, endTimeX, &currentTempoEvents,
                               &endTick, &msOfFirstEventInList);

        TempoChangeEvent* ev = dynamic_cast<TempoChangeEvent*>(
                                   currentTempoEvents->at(0));
        if (!ev) {
            pixpainter->fillRect(0, 0, width(), height(), Appearance::errorColor());
            delete pixpainter;
            return;
        }
        int numLines = endLineY - startLineY;
        if (numLines == 0) {
            delete pixpainter;
            return;
        }

        // fill background of the line descriptions
        pixpainter->fillRect(PianoArea, Appearance::systemWindowColor());

        // fill the pianos background
        int pianoKeys = numLines;
        if (endLineY > 127) {
            pianoKeys -= (endLineY - 127);
        }
        if (pianoKeys > 0) {
            pixpainter->fillRect(0, timeHeight, lineNameWidth - 10,
                                 pianoKeys * lineHeight(), Appearance::pianoWhiteKeyColor());
        }


        // draw background of lines, pianokeys and linenames. when i increase ,the tune decrease.
        for (int i = startLineY; i <= endLineY; i++) {
            int startLine = yPosOfLine(i);
            QColor c;
            if(i<=127){
                bool isHighlighted = false;
                bool isRangeLine = false;

                // Check for C3/C6 range lines if enabled
                if (Appearance::showRangeLines()) {
                    // C3 = MIDI note 48, C6 = MIDI note 84
                    // Matrix widget uses inverted indexing (127-i), so:
                    // For C3: 127-48 = 79
                    // For C6: 127-84 = 43
                    if (i == 79 || i == 43) {  // C3 or C6 lines
                        isRangeLine = true;
                    }
                }

                Appearance::stripStyle strip = Appearance::strip();
                switch (strip) {
                    case Appearance::onOctave :
                        // MIDI note 0 = C, so we want (127-i) % 12 == 0 for C notes
                        // Since i is inverted (127-i gives actual MIDI note), we need:
                        isHighlighted = ((127 - static_cast<unsigned int>(i)) % 12) == 0 ;  // Highlight C notes (octave boundaries)
                    break;
                    case Appearance::onSharp :
                        isHighlighted = ! ( (1 << (static_cast<unsigned int>(i) % 12)) & sharp_strip_mask) ;
                    break;
                    case Appearance::onEven :
                        isHighlighted = (static_cast<unsigned int>(i) % 2);
                    break;

                }

                if (isRangeLine) {
                    c = Appearance::rangeLineColor();  // Range line color (C3/C6)
                } else if (isHighlighted){
                    c = Appearance::stripHighlightColor();
                }else{
                    c = Appearance::stripNormalColor();
                }
            }else{
                // Program events section (lines >127) - use different colors than strips
                if (i % 2 == 1) {
                    c = Appearance::programEventHighlightColor();
                }else{
                    c = Appearance::programEventNormalColor();
                }
            }
            pixpainter->fillRect(lineNameWidth, startLine, width(),
                                 startLine + lineHeight(), c);
        }

        // paint measures and timeline background
        pixpainter->fillRect(0, 0, width(), timeHeight, Appearance::systemWindowColor());

        pixpainter->setClipping(true);
        pixpainter->setClipRect(lineNameWidth, 0, width() - lineNameWidth - 2,
                                height());

        pixpainter->setPen(Appearance::darkGrayColor());
        pixpainter->setBrush(Appearance::pianoWhiteKeyColor());
        pixpainter->drawRect(lineNameWidth, 2, width() - lineNameWidth - 1, timeHeight - 2);
        pixpainter->setPen(Appearance::foregroundColor());

        pixpainter->fillRect(0, timeHeight - 3, width(), 3, Appearance::systemWindowColor());

        // paint time text in ms
        int numbers = (width() - lineNameWidth) / 80;
        if (numbers > 0) {
            int step = (endTimeX - startTimeX) / numbers;
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
            int startNumber = ((startTimeX) / realstep);
            startNumber *= realstep;
            if (startNumber < startTimeX) {
                startNumber += realstep;
            }
            if (Appearance::shouldUseDarkMode()) {
                pixpainter->setPen(QColor(200, 200, 200)); // Light gray for dark mode
            } else {
                pixpainter->setPen(Qt::gray); // Original color for light mode
            }
            while (startNumber < endTimeX) {
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
                    pixpainter->drawText(pos - textlength / 2, timeHeight / 2 - 6, text);
                }
                pixpainter->drawLine(pos, timeHeight / 2 - 1, pos, timeHeight);
                startNumber += realstep;
            }
        }

        // draw measures foreground and text
        int measure = file->measure(startTick, endTick, &currentTimeSignatureEvents);

        TimeSignatureEvent* currentEvent = currentTimeSignatureEvents->at(0);
        int i = 0;
        if (!currentEvent) {
            return;
        }
        int tick = currentEvent->midiTime();
        while (tick + currentEvent->ticksPerMeasure() <= startTick) {
            tick += currentEvent->ticksPerMeasure();
        }
        while (tick < endTick) {
            TimeSignatureEvent* measureEvent = currentTimeSignatureEvents->at(i);
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
            int xto = xPosOfMs(msOfTick(tick));
            pixpainter->setBrush(Appearance::measureBarColor());
            pixpainter->setPen(Qt::NoPen);
            pixpainter->drawRoundedRect(xfrom + 2, timeHeight / 2 + 4, xto - xfrom - 4, timeHeight / 2 - 10, 5, 5);
            if (tick > startTick) {
                pixpainter->setPen(Appearance::measureLineColor());
                pixpainter->drawLine(xfrom, timeHeight / 2, xfrom, height());
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
                int textY = timeHeight - 9;

                pixpainter->setPen(Appearance::measureTextColor());
                pixpainter->drawText(textX, textY, text);

                if (_div >= 0 || _div <= -100) {
                    int ticksPerDiv;

                    if (_div >= 0) {
                        // Regular divisions: _div=0 (whole), _div=1 (half), _div=2 (quarter), etc.
                        // Formula: 4 / 2^_div quarters per division
                        double metronomeDiv = 4 / (double)qPow(2, _div);
                        ticksPerDiv = metronomeDiv * file->ticksPerQuarter();
                    } else if (_div <= -100) {
                        // Extended subdivision system:
                        // -100 to -199: Triplets (÷3)
                        // -200 to -299: Quintuplets (÷5)
                        // -300 to -399: Sextuplets (÷6)
                        // -400 to -499: Septuplets (÷7)
                        // -500 to -599: Dotted notes (×1.5)
                        // -600 to -699: Double dotted notes (×1.75)

                        int subdivisionType = (-_div) / 100;  // 1=triplets, 2=quintuplets, etc.
                        int baseDivision = (-_div) % 100;     // Extract base division

                        double baseDiv = 4 / (double)qPow(2, baseDivision);

                        if (subdivisionType == 1) {
                            // Triplets: divide by 3
                            ticksPerDiv = (baseDiv * file->ticksPerQuarter()) / 3;
                        } else if (subdivisionType == 2) {
                            // Quintuplets: divide by 5
                            ticksPerDiv = (baseDiv * file->ticksPerQuarter()) / 5;
                        } else if (subdivisionType == 3) {
                            // Sextuplets: divide by 6
                            ticksPerDiv = (baseDiv * file->ticksPerQuarter()) / 6;
                        } else if (subdivisionType == 4) {
                            // Septuplets: divide by 7
                            ticksPerDiv = (baseDiv * file->ticksPerQuarter()) / 7;
                        } else if (subdivisionType == 5) {
                            // Dotted notes: multiply by 1.5
                            ticksPerDiv = (baseDiv * file->ticksPerQuarter()) * 1.5;
                        } else if (subdivisionType == 6) {
                            // Double dotted notes: multiply by 1.75
                            ticksPerDiv = (baseDiv * file->ticksPerQuarter()) * 1.75;
                        } else {
                            // Fallback to triplets for unknown types
                            ticksPerDiv = (baseDiv * file->ticksPerQuarter()) / 3;
                        }
                    }

                    int startTickDiv = ticksPerDiv;
                    QPen oldPen = pixpainter->pen();
                    QPen dashPen = QPen(Appearance::timelineGridColor(), 1, Qt::DashLine);
                    pixpainter->setPen(dashPen);
                    while (startTickDiv < measureEvent->ticksPerMeasure()) {
                        int divTick = startTickDiv + measureStartTick;
                        int xDiv = xPosOfMs(msOfTick(divTick));
                        currentDivs.append(QPair<int, int>(xDiv, divTick));
                        pixpainter->drawLine(xDiv, timeHeight, xDiv, height());
                        startTickDiv += ticksPerDiv;
                    }
                    pixpainter->setPen(oldPen);
                }
            }
        }

        // line between time texts and matrixarea
        pixpainter->setPen(Appearance::borderColor());
        pixpainter->drawLine(0, timeHeight, width(), timeHeight);
        pixpainter->drawLine(lineNameWidth, timeHeight, lineNameWidth, height());

        pixpainter->setPen(Appearance::foregroundColor());

        // paint the events
        pixpainter->setClipping(true);
        pixpainter->setClipRect(lineNameWidth, timeHeight, width() - lineNameWidth,
                                height() - timeHeight);
        // Use adaptive rendering based on zoom level and complexity
        RenderTier renderTier = getRenderTier();

        // Start performance timing
        _renderTimer.start();

        #ifdef QT_DEBUG
        _lastUsedLOD = (renderTier != RENDER_FULL_DETAIL);
        _lastRenderEventCount = 0;
        #endif

        for (int i = 0; i < 19; i++) {
            switch (renderTier) {
                case RENDER_FULL_DETAIL:
                    paintChannel(pixpainter, i);
                    break;
                case RENDER_MEDIUM_DETAIL:
                    paintChannelMediumDetail(pixpainter, i);
                    break;
                case RENDER_LOW_DETAIL:
                    paintChannelLOD(pixpainter, i);
                    break;
            }
        }

        // Check render performance and provide feedback
        qint64 renderTime = _renderTimer.elapsed();

        #ifdef QT_DEBUG
        if (renderTier != RENDER_FULL_DETAIL || renderTime > 100) {
            const char* tierNames[] = {"FULL", "MEDIUM", "LOW"};
            qDebug() << "MatrixWidget: Used" << tierNames[renderTier] << "rendering for"
                     << _lastRenderEventCount << "events, time range:" << (endTick - startTick)
                     << "render time:" << renderTime << "ms";
        }
        #endif

        // Show performance warning if rendering is consistently slow
        if (renderTime > 200 && renderTier == RENDER_FULL_DETAIL) {
            if (!_showingPerformanceWarning) {
                _showingPerformanceWarning = true;
                // Could emit a signal here to show a status bar message
                // emit performanceWarning("Rendering is slow - consider zooming in for better performance");
            }
        } else {
            _showingPerformanceWarning = false;
        }
        pixpainter->setClipping(false);

        pixpainter->setPen(Appearance::foregroundColor());

        delete pixpainter;
    }

    // Qt6 optimized pixmap drawing with device pixel ratio support
    painter->drawPixmap(QRect(0, 0, width(), height()), *pixmap, pixmap->rect());

    painter->setRenderHint(QPainter::Antialiasing);
    // draw the piano / linenames
    for (int i = startLineY; i <= endLineY; i++) {
        int startLine = yPosOfLine(i);
        if (i >= 0 && i <= 127) {
            paintPianoKey(painter, 127 - i, 0, startLine,
                          lineNameWidth, lineHeight());
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
            painter->drawText(lineNameWidth - 15 - textlength, startLine + lineHeight(), text);
        }
    }
    if (Tool::currentTool()) {
        painter->setClipping(true);
        painter->setClipRect(ToolArea);
        Tool::currentTool()->draw(painter);
        painter->setClipping(false);
    }

    if (enabled && mouseInRect(TimeLineArea)) {
        painter->setPen(Appearance::playbackCursorColor());
        painter->drawLine(mouseX, 0, mouseX, height());
        painter->setPen(Appearance::foregroundColor());
    }

    if (MidiPlayer::isPlaying()) {
        painter->setPen(Appearance::playbackCursorColor());
        int x = xPosOfMs(MidiPlayer::timeMs());
        if (x >= lineNameWidth) {
            painter->drawLine(x, 0, x, height());
        }
        painter->setPen(Appearance::foregroundColor());
    }

    // paint the cursorTick of file
    if (midiFile()->cursorTick() >= startTick && midiFile()->cursorTick() <= endTick) {
        painter->setPen(Qt::darkGray); // Original color for both modes
        int x = xPosOfMs(msOfTick(midiFile()->cursorTick()));
        painter->drawLine(x, 0, x, height());
        QPointF points[3] = {
            QPointF(x - 8, timeHeight / 2 + 2),
            QPointF(x + 8, timeHeight / 2 + 2),
            QPointF(x, timeHeight - 2),
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
    if (!MidiPlayer::isPlaying() && midiFile()->pauseTick() >= startTick && midiFile()->pauseTick() <= endTick) {
        int x = xPosOfMs(msOfTick(midiFile()->pauseTick()));

        QPointF points[3] = {
            QPointF(x - 8, timeHeight / 2 + 2),
            QPointF(x + 8, timeHeight / 2 + 2),
            QPointF(x, timeHeight - 2),
        };

        painter->setBrush(QBrush(Appearance::grayColor(), Qt::SolidPattern));

        painter->drawPolygon(points, 3);
    }

    // border
    painter->setPen(Appearance::borderColor());
    painter->drawLine(width() - 1, height() - 1, lineNameWidth, height() - 1);
    painter->drawLine(width() - 1, height() - 1, width() - 1, 2);

    // if the recorder is recording, show red circle
    if (MidiInput::recording()) {
        painter->setBrush(Appearance::recordingIndicatorColor());
        painter->drawEllipse(width() - 20, timeHeight + 5, 15, 15);
    }
    delete painter;

    // if MouseRelease was not used, delete it
    mouseReleased = false;

    if (totalRepaint) {
        emit objectListChanged();
    }
}

void MatrixWidget::paintChannel(QPainter* painter, int channel) {
    if (!file->channel(channel)->visible()) {
        return;
    }
    QColor cC = *file->channel(channel)->color();

    // filter events
    QMultiMap<int, MidiEvent*>* map = file->channelEvents(channel);

    // Early exit for empty channels
    if (!map || map->isEmpty()) {
        return;
    }

    // Two-pass approach for performance while maintaining correctness:
    // Pass 1: Process events that start within an extended viewport (for performance)
    // Pass 2: For note events, check for long notes that start before extended viewport

    // Calculate extended search range - look back/forward based on typical note lengths
    int maxNoteLengthEstimate = qMin(endTick - startTick, file->ticksPerQuarter() * 16); // Max 16 beats
    int searchStartTick = startTick - maxNoteLengthEstimate;
    int searchEndTick = endTick + maxNoteLengthEstimate;
    if (searchStartTick < 0) searchStartTick = 0;

    QMultiMap<int, MidiEvent*>::iterator it = map->lowerBound(searchStartTick);
    QMultiMap<int, MidiEvent*>::iterator endIt = map->upperBound(searchEndTick);

    while (it != endIt && it != map->end()) {
        MidiEvent* event = it.value();
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

            OffEvent* offEvent = dynamic_cast<OffEvent*>(event);
            OnEvent* onEvent = dynamic_cast<OnEvent*>(event);

            int x, width;
            int y = yPosOfLine(line);
            int height = lineHeight();

            if (onEvent || offEvent) {
                if (onEvent) {
                    offEvent = onEvent->offEvent();
                } else if (offEvent) {
                    onEvent = dynamic_cast<OnEvent*>(offEvent->onEvent());
                }

                width = xPosOfMs(msOfTick(offEvent->midiTime())) - xPosOfMs(msOfTick(onEvent->midiTime()));
                x = xPosOfMs(msOfTick(onEvent->midiTime()));
                event = onEvent;
                if (objects->contains(event)) {
                    it++;
                    continue;
                }
            } else {
                width = PIXEL_PER_EVENT;
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
                    painter->drawLine(lineNameWidth, y, this->width(), y);
                    painter->drawLine(lineNameWidth, y + height, this->width(), y + height);
                    painter->setPen(Appearance::foregroundColor());
                }
                objects->prepend(event);
            }
        }

        if (!(event->track()->hidden())) {
            // append event to velocityObjects if its not a offEvent and if it
            // is in the x-Area
            OffEvent* offEvent = dynamic_cast<OffEvent*>(event);
            if (!offEvent && event->midiTime() >= startTick && event->midiTime() <= endTick && !velocityObjects->contains(event)) {
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
        QMultiMap<int, MidiEvent*>::iterator fallbackIt = map->begin();
        QMultiMap<int, MidiEvent*>::iterator fallbackEnd = map->lowerBound(searchStartTick);

        while (fallbackIt != fallbackEnd) {
            MidiEvent* event = fallbackIt.value();
            OnEvent* onEvent = dynamic_cast<OnEvent*>(event);

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
                                painter->drawLine(lineNameWidth, y, this->width(), y);
                                painter->drawLine(lineNameWidth, y + height, this->width(), y + height);
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

void MatrixWidget::paintPianoKey(QPainter* painter, int number, int x, int y,
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

        if (127 - number == startLineY) {
            blackOnTop = false;
        }

        bool selected = mouseY >= y && mouseY <= y + height && mouseX > lineNameWidth && mouseOver;
        foreach (MidiEvent* event, Selection::instance()->selectedEvents()) {
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
        if (inRect && enabled) {
            // mark the current Line
            QColor lineColor = Appearance::pianoKeyLineHighlightColor();
            painter->fillRect(x + width + borderRight, yPosOfLine(127 - number),
                              this->width() - x - width - borderRight, height, lineColor);
        }
    }
}

void MatrixWidget::setFile(MidiFile* f) {

    file = f;

    scaleX = 1;
    scaleY = 1;

    startTimeX = 0;
    // Roughly vertically center on Middle C.
    startLineY = 50;

    connect(file->protocol(), SIGNAL(actionFinished()), this,
            SLOT(registerRelayout()));
    connect(file->protocol(), SIGNAL(actionFinished()), this, SLOT(update()));

    calcSizes();

    // scroll down to see events
    int maxNote = -1;
    for (int channel = 0; channel < 16; channel++) {

        QMultiMap<int, MidiEvent*>* map = file->channelEvents(channel);

        QMultiMap<int, MidiEvent*>::iterator it = map->lowerBound(0);
        while (it != map->end()) {
            NoteOnEvent* onev = dynamic_cast<NoteOnEvent*>(it.value());
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

void MatrixWidget::calcSizes() {
    if (!file) {
        return;
    }
    int time = file->maxTime();
    int timeInWidget = ((width() - lineNameWidth) * 1000) / (PIXEL_PER_S * scaleX);

    ToolArea = QRectF(lineNameWidth, timeHeight, width() - lineNameWidth,
                      height() - timeHeight);
    PianoArea = QRectF(0, timeHeight, lineNameWidth, height() - timeHeight);
    TimeLineArea = QRectF(lineNameWidth, 0, width() - lineNameWidth, timeHeight);

    scrollXChanged(startTimeX);
    scrollYChanged(startLineY);

    emit sizeChanged(time - timeInWidget, NUM_LINES - endLineY + startLineY, startTimeX,
                     startLineY);
}

MidiFile* MatrixWidget::midiFile() {
    return file;
}

void MatrixWidget::mouseMoveEvent(QMouseEvent* event) {
    PaintWidget::mouseMoveEvent(event);

    if (!enabled) {
        return;
    }

    if (!MidiPlayer::isPlaying() && Tool::currentTool()) {
        Tool::currentTool()->move(event->x(), event->y());
    }

    if (!MidiPlayer::isPlaying()) {
        repaint();
    }
}

void MatrixWidget::resizeEvent(QResizeEvent* event) {
    Q_UNUSED(event);
    calcSizes();
}

int MatrixWidget::xPosOfMs(int ms) {
    return lineNameWidth + (ms - startTimeX) * (width() - lineNameWidth) / (endTimeX - startTimeX);
}

int MatrixWidget::yPosOfLine(int line) {
    return timeHeight + (line - startLineY) * lineHeight();
}

double MatrixWidget::lineHeight() {
    if (endLineY - startLineY == 0)
        return 0;
    return (double)(height() - timeHeight) / (double)(endLineY - startLineY);
}

void MatrixWidget::enterEvent(QEvent* event) {
    PaintWidget::enterEvent(event);
    if (Tool::currentTool()) {
        Tool::currentTool()->enter();
        if (enabled) {
            update();
        }
    }
}
void MatrixWidget::leaveEvent(QEvent* event) {
    PaintWidget::leaveEvent(event);
    if (Tool::currentTool()) {
        Tool::currentTool()->exit();
        if (enabled) {
            update();
        }
    }
}
void MatrixWidget::mousePressEvent(QMouseEvent* event) {
    PaintWidget::mousePressEvent(event);
    if (!MidiPlayer::isPlaying() && Tool::currentTool() && mouseInRect(ToolArea)) {
        if (Tool::currentTool()->press(event->buttons() == Qt::LeftButton)) {
            if (enabled) {
                update();
            }
        }
    } else if (enabled && (!MidiPlayer::isPlaying()) && (mouseInRect(PianoArea))) {
        foreach (int key, pianoKeys.keys()) {
            bool inRect = mouseInRect(pianoKeys.value(key));
            if (inRect) {
                // play note
                playNote(key);
            }
        }
    }
}
void MatrixWidget::mouseReleaseEvent(QMouseEvent* event) {
    PaintWidget::mouseReleaseEvent(event);
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

void MatrixWidget::takeKeyPressEvent(QKeyEvent* event) {
    if (Tool::currentTool()) {
        if (Tool::currentTool()->pressKey(event->key())) {
            repaint();
        }
    }

    pianoEmulator(event);
}

void MatrixWidget::takeKeyReleaseEvent(QKeyEvent* event) {
    if (Tool::currentTool()) {
        if (Tool::currentTool()->releaseKey(event->key())) {
            repaint();
        }
    }
}

void MatrixWidget::forceCompleteRedraw() {
    // Force complete redraw by invalidating cached pixmap
    delete pixmap;
    pixmap = 0;
    repaint();
}

MatrixWidget::RenderTier MatrixWidget::getRenderTier() const {
    if (!file || width() <= lineNameWidth) return RENDER_FULL_DETAIL;

    double timeRange = endTick - startTick;
    if (timeRange <= 0) return RENDER_FULL_DETAIL;

    double pixelsPerTick = (double)(width() - lineNameWidth) / timeRange;

    // Estimate event density for adaptive optimization
    int estimatedEventCount = 0;
    for (int i = 0; i < 19; i++) {
        if (file->channel(i)->visible()) {
            QMultiMap<int, MidiEvent*>* map = file->channelEvents(i);
            if (map) {
                // Quick estimate: count events in a sample range
                int sampleRange = qMin((int)timeRange, file->ticksPerQuarter() * 4);
                int sampleStart = startTick;
                int sampleEnd = startTick + sampleRange;

                auto sampleIt = map->lowerBound(sampleStart);
                auto sampleEndIt = map->upperBound(sampleEnd);
                int sampleCount = std::distance(sampleIt, sampleEndIt);

                // Extrapolate to full range
                if (sampleRange > 0) {
                    estimatedEventCount += (sampleCount * timeRange) / sampleRange;
                }
            }
        }
    }

    // Qt6 optimized render tier thresholds - less aggressive LODs due to better GPU performance
    if (pixelsPerTick < 0.05 || timeRange > LOD_TIME_THRESHOLD || estimatedEventCount > MAX_EVENTS_PER_FRAME * 3) {
        return RENDER_LOW_DETAIL;  // Very zoomed out or very dense - use LOD
    } else if (pixelsPerTick < 0.5 || timeRange > MEDIUM_DETAIL_TIME_THRESHOLD || estimatedEventCount > MAX_EVENTS_PER_FRAME * 1.5) {
        return RENDER_MEDIUM_DETAIL;  // Moderately zoomed out or dense - use medium detail
    } else {
        return RENDER_FULL_DETAIL;  // Normal zoom and density - use full detail
    }
}

bool MatrixWidget::shouldUseLevelOfDetail() const {
    return getRenderTier() == RENDER_LOW_DETAIL;
}

void MatrixWidget::paintChannelMediumDetail(QPainter* painter, int channel) {
    if (!file) return;

    QMultiMap<int, MidiEvent*>* map = file->channelEvents(channel);
    if (!map || map->isEmpty()) return;

    QColor cC;
    if (_colorsByChannels) {
        cC = *file->channel(channel)->color();
    }

    // Medium detail: Skip some optimizations but use simplified rendering
    // Use smaller sample interval than LOD but larger than full detail
    double timeRange = endTick - startTick;
    double pixelsPerTick = (double)(width() - lineNameWidth) / timeRange;
    int skipFactor = qMax(1, (int)(1.0 / pixelsPerTick / 2)); // Skip every N events when very dense

    // Use time-based bounds but with tighter range than LOD
    int maxNoteLengthEstimate = qMin(endTick - startTick, file->ticksPerQuarter() * 8);
    int searchStartTick = startTick - maxNoteLengthEstimate;
    if (searchStartTick < 0) searchStartTick = 0;

    QMultiMap<int, MidiEvent*>::iterator it = map->lowerBound(searchStartTick);
    QMultiMap<int, MidiEvent*>::iterator endIt = map->upperBound(endTick);

    int eventCount = 0;
    while (it != endIt && it != map->end()) {
        MidiEvent* event = it.value();

        // Skip some events when they're very dense (adaptive sampling)
        if (skipFactor > 1 && (eventCount % skipFactor) != 0) {
            eventCount++;
            it++;
            continue;
        }

        // Quick Y-axis culling
        int line = event->line();
        if (line < startLineY - 1 || line > endLineY + 1) {
            it++;
            continue;
        }

        if (eventInWidget(event)) {
            // Skip events on hidden tracks
            if (event->track()->hidden()) {
                it++;
                continue;
            }

            if (!_colorsByChannels) {
                cC = *event->track()->color();
            }
            paintEventMediumDetail(painter, event, cC);
        }

        eventCount++;
        it++;
    }
}

void MatrixWidget::paintChannelLOD(QPainter* painter, int channel) {
    if (!file) return;

    QMultiMap<int, MidiEvent*>* map = file->channelEvents(channel);
    if (!map || map->isEmpty()) return;

    QColor cC;
    if (_colorsByChannels) {
        cC = *file->channel(channel)->color();
    }

    // For LOD, we'll sample events at regular intervals instead of drawing every event
    int sampleInterval = (endTick - startTick) / (width() - lineNameWidth); // One sample per pixel column
    if (sampleInterval < 1) sampleInterval = 1;

    // Use time-based bounds for better performance, but account for long notes
    int maxNoteLengthEstimate = qMin(endTick - startTick, file->ticksPerQuarter() * 8); // Shorter estimate for LOD
    int searchStartTick = startTick - maxNoteLengthEstimate;
    if (searchStartTick < 0) searchStartTick = 0;

    QMultiMap<int, MidiEvent*>::iterator it = map->lowerBound(searchStartTick);
    QMultiMap<int, MidiEvent*>::iterator endIt = map->upperBound(endTick);

    int lastSampleTick = startTick - sampleInterval;

    while (it != endIt && it != map->end()) {
        MidiEvent* event = it.value();

        // Sample events at regular intervals
        if (event->midiTime() - lastSampleTick >= sampleInterval) {
            if (eventInWidget(event)) {
                int line = event->line();

                // Skip events on hidden tracks
                if (event->track()->hidden()) {
                    it++;
                    continue;
                }

                // For LOD, use simplified rendering
                if (line >= startLineY && line <= endLineY) {
                    if (!_colorsByChannels) {
                        cC = *event->track()->color();
                    }
                    paintEventLOD(painter, event, cC);
                }

                lastSampleTick = event->midiTime();
            }
        }
        it++;
    }
}

void MatrixWidget::paintEventMediumDetail(QPainter* painter, MidiEvent* event, const QColor& color) {
    // Medium detail rendering - simplified but still recognizable
    int x = xPosOfMs(msOfTick(event->midiTime()));
    int y = yPosOfLine(event->line());
    int height = (int)lineHeight();

    OnEvent* onEvent = dynamic_cast<OnEvent*>(event);
    if (onEvent && onEvent->offEvent()) {
        // For note events, draw simplified rectangles (no rounded corners)
        int endX = xPosOfMs(msOfTick(onEvent->offEvent()->midiTime()));
        int width = endX - x;

        if (width < 1) width = 1; // Ensure minimum visibility

        // Use flat rectangles instead of rounded for performance
        painter->setPen(Appearance::borderColor());
        painter->setBrush(color);
        painter->drawRect(x, y + height/4, width, height/2);

        // Add selection highlighting if needed
        if (Selection::instance()->selectedEvents().contains(onEvent)) {
            painter->setPen(Qt::gray);
            painter->drawLine(lineNameWidth, y, width(), y);
            painter->drawLine(lineNameWidth, y + height, width(), y + height);
        }
    } else {
        // Non-note events - draw small rectangles
        painter->setPen(color);
        painter->setBrush(color);
        int size = qMax(2, height/3);
        painter->drawRect(x-1, y + height/2 - size/2, size, size);
    }
}

void MatrixWidget::paintEventLOD(QPainter* painter, MidiEvent* event, const QColor& color) {
    // Simplified rendering for LOD - just draw small rectangles or lines
    int x = xPosOfMs(msOfTick(event->midiTime()));
    int y = yPosOfLine(event->line());

    OnEvent* onEvent = dynamic_cast<OnEvent*>(event);
    if (onEvent && onEvent->offEvent()) {
        // For note events, draw a simple line instead of a full rectangle
        int endX = xPosOfMs(msOfTick(onEvent->offEvent()->midiTime()));
        int width = endX - x;

        if (width < 2) {
            // Very short notes - just draw a vertical line
            painter->setPen(QPen(color, 1));
            painter->drawLine(x, y, x, y + (int)lineHeight());
        } else {
            // Draw a thin rectangle
            painter->setPen(color);
            painter->setBrush(color);
            painter->drawRect(x, y + (int)lineHeight()/3, width, (int)lineHeight()/3);
        }
    } else {
        // Non-note events - draw a small square
        painter->setPen(color);
        painter->setBrush(color);
        int size = qMax(1, (int)lineHeight()/4);
        painter->drawRect(x-1, y + (int)lineHeight()/2 - size/2, size, size);
    }
}

void MatrixWidget::batchDrawEvents(QPainter* painter, const QList<MidiEvent*>& events, const QColor& color) {
    if (events.isEmpty()) return;

    // Qt6 optimized batch drawing
    painter->setPen(Appearance::borderColor());
    painter->setBrush(color);

    // Use QPainterPath for better performance with many rectangles
    QPainterPath path;
    for (MidiEvent* event : events) {
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

void MatrixWidget::pianoEmulator(QKeyEvent* event) {
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
            MatrixWidget::playNote(idx + C4_OFFSET);
        }
    }

    int dupkeys[] = {
        44, 76, 46, 59, 47 // C4 - E4 (,l.;/)
    };
    for (uint8_t idx = 0; idx < sizeof(dupkeys) / sizeof(*dupkeys); idx++) {
        if (key == dupkeys[idx]) {
            MatrixWidget::playNote(idx + C4_OFFSET + 12);
        }
    }
}

void MatrixWidget::playNote(int note) {
    pianoEvent->setNote(note);
    pianoEvent->setChannel(MidiOutput::standardChannel(), false);
    MidiPlayer::play(pianoEvent);
}

QList<MidiEvent*>* MatrixWidget::activeEvents() {
    return objects;
}

QList<MidiEvent*>* MatrixWidget::velocityEvents() {
    return velocityObjects;
}

int MatrixWidget::msOfXPos(int x) {
    return startTimeX + ((x - lineNameWidth) * (endTimeX - startTimeX)) / (width() - lineNameWidth);
}

int MatrixWidget::msOfTick(int tick) {
    return file->msOfTick(tick, currentTempoEvents, msOfFirstEventInList);
}

int MatrixWidget::timeMsOfWidth(int w) {
    return (w * (endTimeX - startTimeX)) / (width() - lineNameWidth);
}

bool MatrixWidget::eventInWidget(MidiEvent* event) {
    NoteOnEvent* on = dynamic_cast<NoteOnEvent*>(event);
    OffEvent* off = dynamic_cast<OffEvent*>(event);
    if (on) {
        off = on->offEvent();
    } else if (off) {
        on = dynamic_cast<NoteOnEvent*>(off->onEvent());
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

        // Check all possible time overlap scenarios:
        // 1. Note starts before viewport and ends after viewport (spans completely)
        // 2. Note starts before viewport and ends inside viewport
        // 3. Note starts inside viewport and ends after viewport
        // 4. Note starts and ends inside viewport
        // All of these can be captured by: note starts before viewport ends AND note ends after viewport starts
        bool timeOverlaps = (onTick < endTick && offTick > startTick);

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
    return (y - timeHeight) / lineHeight() + startLineY;
}

void MatrixWidget::zoomStd() {
    scaleX = 1;
    scaleY = 1;
    calcSizes();
}

void MatrixWidget::resetView() {
    if (!file) {
        return;
    }

    // Reset zoom to default
    scaleX = 1;
    scaleY = 1;

    // Reset horizontal scroll to beginning
    startTimeX = 0;

    // Reset vertical scroll to roughly center on Middle C (line 60)
    startLineY = 50;

    // Reset cursor and pause positions to beginning
    file->setCursorTick(0);
    file->setPauseTick(-1);

    // Recalculate sizes and update display
    calcSizes();

    // Force a complete repaint
    registerRelayout();
    update();
}

void MatrixWidget::zoomHorIn() {
    scaleX += 0.1;
    calcSizes();
}

void MatrixWidget::zoomHorOut() {
    if (scaleX >= 0.2) {
        scaleX -= 0.1;
        calcSizes();
    }
}

void MatrixWidget::zoomVerIn() {
    scaleY += 0.1;
    calcSizes();
}

void MatrixWidget::zoomVerOut() {
    if (scaleY >= 0.2) {
        scaleY -= 0.1;
        if (height() <= NUM_LINES * lineHeight() * scaleY / (scaleY + 0.1)) {
            calcSizes();
        } else {
            scaleY += 0.1;
        }
    }
}

void MatrixWidget::mouseDoubleClickEvent(QMouseEvent* event) {
    if (mouseInRect(TimeLineArea)) {
        int tick = file->tick(msOfXPos(mouseX));
        file->setCursorTick(tick);
        update();
    }
}

void MatrixWidget::registerRelayout() {
    delete pixmap;
    pixmap = 0;
}

int MatrixWidget::minVisibleMidiTime() {
    return startTick;
}

int MatrixWidget::maxVisibleMidiTime() {
    return endTick;
}

void MatrixWidget::wheelEvent(QWheelEvent* event) {
    /*
     * Qt has some underdocumented behaviors for reporting wheel events, so the
     * following were determined empirically:
     *
     * 1.  Some platforms use pixelDelta and some use angleDelta; you need to
     *     handle both.
     *
     * 2.  The documentation for angleDelta is very convoluted, but it boils
     *     down to a scaling factor of 8 to convert to pixels.  Note that
     *     some mouse wheels scroll very coarsely, but this should result in an
     *     equivalent amount of movement as seen in other programs, even when
     *     that means scrolling by multiple lines at a time.
     *
     * 3.  When a modifier key is held, the X and Y may be swapped in how
     *     they're reported, but which modifiers these are differ by platform.
     *     If you want to reserve the modifiers for your own use, you have to
     *     counteract this explicitly.
     *
     * 4.  A single-dimensional scrolling device (mouse wheel) seems to be
     *     reported in the Y dimension of the pixelDelta or angleDelta, but is
     *     subject to the same X/Y swapping when modifiers are pressed.
     */

    Qt::KeyboardModifiers km = event->modifiers();
    QPoint pixelDelta = event->pixelDelta();
    int pixelDeltaX = pixelDelta.x();
    int pixelDeltaY = pixelDelta.y();

    if ((pixelDeltaX == 0) && (pixelDeltaY == 0)) {
        QPoint angleDelta = event->angleDelta();
        pixelDeltaX = angleDelta.x() / 8;
        pixelDeltaY = angleDelta.y() / 8;
    }

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

    if (file) {
        int maxTimeInFile = file->maxTime();
        int widgetRange = endTimeX - startTimeX;

        if (horScrollAmount != 0) {
            int scroll = -1 * horScrollAmount * widgetRange / 1000;

            int newStartTime = startTimeX + scroll;

            scrollXChanged(newStartTime);
            emit scrollChanged(startTimeX, maxTimeInFile - widgetRange, startLineY, NUM_LINES - (endLineY - startLineY));
        }

        if (verScrollAmount != 0) {
            int newStartLineY = startLineY - (verScrollAmount / (scaleY * PIXEL_PER_LINE));

            if (newStartLineY < 0)
                newStartLineY = 0;

            // endline too large handled in scrollYchanged()
            scrollYChanged(newStartLineY);
            emit scrollChanged(startTimeX, maxTimeInFile - widgetRange, startLineY, NUM_LINES - (endLineY - startLineY));
        }
    }
}

void MatrixWidget::keyPressEvent(QKeyEvent* event) {
    takeKeyPressEvent(event);
}

void MatrixWidget::keyReleaseEvent(QKeyEvent* event) {
    takeKeyReleaseEvent(event);
}

void MatrixWidget::setColorsByChannel() {
    _colorsByChannels = true;
}
void MatrixWidget::setColorsByTracks() {
    _colorsByChannels = false;
}

bool MatrixWidget::colorsByChannel() {
    return _colorsByChannels;
}

bool MatrixWidget::getPianoEmulation() {
    return _isPianoEmulationEnabled;
}

void MatrixWidget::setPianoEmulation(bool mode) {
    _isPianoEmulationEnabled = mode;
}

void MatrixWidget::setDiv(int div) {
    _div = div;
    registerRelayout();
    update();
}

QList<QPair<int, int> > MatrixWidget::divs() {
    return currentDivs;
}

int MatrixWidget::div() {
    return _div;
}
