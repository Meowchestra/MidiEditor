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
        this->pianoKeys.clear();
        pixmap = new QPixmap(width(), height());
        QPainter* pixpainter = new QPainter(pixmap);
        if (!QApplication::arguments().contains("--no-antialiasing")) {
            pixpainter->setRenderHint(QPainter::Antialiasing);
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
                        // Regular divisions
                        double metronomeDiv = 4 / (double)qPow(2, _div);
                        ticksPerDiv = metronomeDiv * file->ticksPerQuarter();
                    } else {
                        // Triplet divisions (negative values -100 and below)
                        int baseDivision = (-_div) - 100; // Extract base division (2 for quarter, 3 for eighth)
                        double baseDiv = 4 / (double)qPow(2, baseDivision);
                        // For triplets, divide by 3 instead of the normal power of 2
                        ticksPerDiv = (baseDiv * file->ticksPerQuarter()) / 3;
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
        for (int i = 0; i < 19; i++) {
            paintChannel(pixpainter, i);
        }
        pixpainter->setClipping(false);

        pixpainter->setPen(Appearance::foregroundColor());

        delete pixpainter;
    }

    painter->drawPixmap(0, 0, *pixmap);

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

    QMultiMap<int, MidiEvent*>::iterator it = map->lowerBound(startTick);
    while (it != map->end() && it.key() <= endTick) {
        MidiEvent* event = it.value();
        if (eventInWidget(event)) {
            // insert all Events in objects, set their coordinates
            // Only onEvents are inserted. When there is an On
            // and an OffEvent, the OnEvent will hold the coordinates
            int line = event->line();

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

        // Check if note spans across the viewport (starts before and ends after)
        bool spansViewport = (onLine >= startLineY && onLine <= endLineY) &&
                            (onTick < startTick && offTick > endTick);

        off->setShown(offIn || spansViewport);
        on->setShown(onIn || spansViewport);

        return offIn || onIn || spansViewport;

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
