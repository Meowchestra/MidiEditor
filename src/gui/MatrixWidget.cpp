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
#include <iterator>
#include "../MidiEvent/NoteOnEvent.h"
#include "../MidiEvent/OffEvent.h"
#include "../MidiEvent/ControlChangeEvent.h"
#include "../MidiEvent/ProgChangeEvent.h"
#include "../MidiEvent/TextEvent.h"
#include "../MidiEvent/TempoChangeEvent.h"
#include "../MidiEvent/TimeSignatureEvent.h"
#include "../midi/MidiChannel.h"
#include "../midi/MidiFile.h"
#include "../midi/MidiInput.h"
#include "../midi/MidiPlayer.h"
#include "../midi/MidiTrack.h"
#include "../midi/MidiPlayer.h"
#include "../midi/PlayerThread.h"
#include "../protocol/Protocol.h"
#include "../tool/Tool.h"
#include "../tool/EditorTool.h"
#include "../tool/Selection.h"
#include "../tool/StandardTool.h"
#include "Appearance.h"
#include "../tool/SelectTool.h"
#include "../tool/MeasureTool.h"
#include "../tool/EventTool.h"
#include "MainWindow.h"
#include <algorithm>
#include <set>
#include "../gui/Appearance.h"
#include "../gui/ChannelVisibilityManager.h"
#include "../midi/MidiOutput.h"

#include <QList>
#include <QSettings>
#include <QApplication>
#include <QDateTime>
#include <cmath>
#include <QContextMenuEvent>
#include <QMenu>
#include <QInputDialog>
#include <QWidgetAction>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QToolButton>
#include <QLabel>
#ifdef Q_OS_WIN
#include <windows.h>
#endif
#include "../MidiEvent/ControlChangeEvent.h"
#include "../MidiEvent/ProgChangeEvent.h"
#include "../MidiEvent/TextEvent.h"
#include "TransposeDialog.h"
#include "MainWindow.h"
#include "../tool/StandardTool.h"
#include "../tool/SelectTool.h"
#include "../tool/EventTool.h"

MatrixWidget::MatrixWidget(QSettings *settings, QWidget *parent)
    : PaintWidget(parent), _settings(settings) {
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
    currentTempoEvents = new QList<MidiEvent *>;
    currentTimeSignatureEvents = new QList<TimeSignatureEvent *>;
    msOfFirstEventInList = 0;
    objects = new QList<MidiEvent *>;
    velocityObjects = new QList<MidiEvent *>;
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

    // Cache rendering settings to avoid reading from QSettings on every paint event
    _antialiasing = _settings->value("rendering/antialiasing", true).toBool();
    _smoothPixmapTransform = _settings->value("rendering/smooth_pixmap_transform", true).toBool();

    // Initialize scroll repaint suppression flag
    _suppressScrollRepaints = false;

    // Cache appearance colors to avoid expensive theme checks on every paint event
    updateCachedAppearanceColors();
    
    // Set initial timeline height
    timeHeight = _hasVisibleMarkers ? 66 : 50;

    initKeyMap();
    _pressedPianoNote = -1;
    _pressedPianoTime = 0;
    _currentOctaveShift = 0;
    _isOctaveUpPressed = false;
    _isOctaveDownPressed = false;
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

    _lastRenderedMs = ms;
    bool isPlaying = MidiPlayer::isPlaying();
    bool wasPlayingOld = _wasPlaying;
    bool wasLockedOld = _wasLocked;
    _wasPlaying = isPlaying;
    _wasLocked = screen_locked;

    int x = xPosOfMs(ms);
    bool smoothScroll = Appearance::smoothPlaybackScrolling();

    if (!screen_locked || ignoreLocked) {
        if (smoothScroll && (isPlaying || ignoreLocked)) {
            int viewportDurationMs = ((width() - lineNameWidth) * 1000) / (PIXEL_PER_S * scaleX);
            int targetAnchorMs = viewportDurationMs * 0.20; // 20% from the left
            double targetStartTime = (double)ms - targetAnchorMs;
            if (targetStartTime < 0) targetStartTime = 0;
            
            // --- INITIAL JUMP / UNLOCK SNAP ---
            // Trigger jump if:
            // 1. Playback just started (and cursor is off-screen)
            // 2. We just unlocked the view during playback (and cursor is off-screen)
            bool jumpCondition = (isPlaying && !wasPlayingOld) || (isPlaying && !screen_locked && wasLockedOld);
            
            if (jumpCondition && (ms < startTimeX || ms > endTimeX)) {
               startTimeX = ms;
               endTimeX = startTimeX + viewportDurationMs;
               registerRelayout();
            }

            // Current screen offset of the cursor (in ms)
            int currentOffset = ms - startTimeX;

            // --- STATIONARY ZONE (0% -> 20%) ---
            // If the cursor is behind the anchor, keep the viewport stationary.
            // This allows the cursor to physically travel across the screen until it hits 20%.
            if (currentOffset >= 0 && currentOffset <= targetAnchorMs && targetStartTime <= startTimeX) {
                // Ensure endTimeX is updated even in stationary zone (to handle resizes/jumps)
                endTimeX = startTimeX + viewportDurationMs;
                update();
                return;
            }

            // --- ELASTIC CATCH-UP & TRACKING LOGIC ---
            double distanceToTarget = targetStartTime - startTimeX;
            double calculatedNewStart;
            
            if (std::abs(distanceToTarget) < 50.0) {
                calculatedNewStart = targetStartTime;
            } else {
                double panSpeedFactor = 0.04; 
                calculatedNewStart = startTimeX + (distanceToTarget * panSpeedFactor);
            }

            int finalStartTime = (int)std::round(calculatedNewStart);

            // BOUNDS CLAMPING
            if (finalStartTime < 0) finalStartTime = 0;
            int maxStart = file->maxTime() - viewportDurationMs;
            if (finalStartTime > maxStart) finalStartTime = maxStart;
            if (finalStartTime < 0) finalStartTime = 0;

            // Apply new position ONLY if it actually changed
            if (finalStartTime != startTimeX) {
                startTimeX = finalStartTime;
                endTimeX = startTimeX + viewportDurationMs;
                registerRelayout();
                update();
                emit scrollChanged(startTimeX, (file->maxTime() - viewportDurationMs), startLineY,
                                   NUM_LINES - (endLineY - startLineY));
            } else {
                endTimeX = startTimeX + viewportDurationMs;
                update();
            }
            return;
            
        } else if (!smoothScroll && (x < lineNameWidth || ms < startTimeX || ms > endTimeX || x > width() - 100)) {
            // Standard bounding (page turn) mode
            if (file->maxTime() <= endTimeX && ms >= startTimeX) {
                update();
                return;
            }

            // sets the new position and repaints
            emit scrollChanged(ms, (file->maxTime() - endTimeX + startTimeX), startLineY,
                               NUM_LINES - (endLineY - startLineY));
            return;
        }
    }
    
    update();
}

void MatrixWidget::scrollXChanged(int scrollPositionX) {
    if (!file || (scrollPositionX == startTimeX && !_suppressScrollRepaints))
        return;

    startTimeX = scrollPositionX;
    endTimeX = startTimeX + ((width() - lineNameWidth) * 1000) / (PIXEL_PER_S * scaleX);

    if (startTimeX < 0) {
        endTimeX -= startTimeX;
        startTimeX = 0;
    } else if (endTimeX > file->maxTime()) {
        startTimeX += file->maxTime() - endTimeX;
        endTimeX = file->maxTime();
        if (startTimeX < 0) startTimeX = 0;
    }

    // Only repaint if not suppressed (to prevent cascading repaints)
    if (!_suppressScrollRepaints) {
        registerRelayout();
        update();
    }
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

    // Only repaint if not suppressed (to prevent cascading repaints)
    if (!_suppressScrollRepaints) {
        registerRelayout();
        update();
    }
}

void MatrixWidget::paintEvent(QPaintEvent *event) {
    if (!file)
        return;

    // PERFORMANCE: Update selection caches once per paint cycle to avoid O(N_selection) loops in sub-methods
    _cachedSelection.clear();
    _cachedSelectedRows.clear();
    _cachedDrawnRowHighlights.clear();
    const QList<MidiEvent*>& selectedEvents = Selection::instance()->selectedEvents();
    for (MidiEvent* event : selectedEvents) {
        _cachedSelection.insert(event);
        _cachedSelectedRows.insert(event->line());
    }

    QPainter *painter = new QPainter(this);
    QFont font = Appearance::improveFont(painter->font());
    font.setPixelSize(12);
    painter->setFont(font);
    painter->setClipping(false);

    bool totalRepaint = !pixmap;

    if (totalRepaint) {
        this->pianoKeys.clear();
        qreal dpr = devicePixelRatioF();
        pixmap = new QPixmap(size() * dpr);
        pixmap->setDevicePixelRatio(dpr);
        pixmap->fill(_cachedBackgroundColor);
        QPainter *pixpainter = new QPainter(pixmap);

        // Apply cached user-configurable performance settings
        pixpainter->setRenderHint(QPainter::Antialiasing, _antialiasing);
        pixpainter->setRenderHint(QPainter::SmoothPixmapTransform, _smoothPixmapTransform);

        // background shade
        pixpainter->fillRect(0, 0, width(), height(), _cachedBackgroundColor);

        QFont f = Appearance::improveFont(pixpainter->font());
        f.setPixelSize(12);
        pixpainter->setFont(f);
        pixpainter->setClipping(false);

        for (int i = 0; i < objects->length(); i++) {
            objects->at(i)->setShown(false);
            OnEvent *onev = dynamic_cast<OnEvent *>(objects->at(i));
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

        TempoChangeEvent *ev = dynamic_cast<TempoChangeEvent *>(
            currentTempoEvents->at(0));
        if (!ev) {
            pixpainter->fillRect(0, 0, width(), height(), _cachedErrorColor);
            delete pixpainter;
            return;
        }
        int numLines = endLineY - startLineY;
        if (numLines == 0) {
            delete pixpainter;
            return;
        }

        // fill background of the line descriptions
        pixpainter->fillRect(PianoArea, _cachedSystemWindowColor);

        // fill the pianos background
        int pianoKeys = numLines;
        if (endLineY > 127) {
            pianoKeys -= (endLineY - 127);
        }
        if (pianoKeys > 0) {
            pixpainter->fillRect(0, timeHeight, lineNameWidth - 10,
                                 pianoKeys * lineHeight(), _cachedPianoWhiteKeyColor);
        }

        // draw background of lines, pianokeys and linenames. when i increase ,the tune decrease.
        // Use cached appearance values to avoid expensive calls during paint

        for (int i = startLineY; i <= endLineY; i++) {
            int startLine = yPosOfLine(i);
            QColor c;
            if (i <= 127) {
                bool isHighlighted = false;
                bool isRangeLine = false;

                // Check for C3/C6 range lines if enabled
                if (_cachedShowRangeLines) {
                    // C3 = MIDI note 48, C6 = MIDI note 84
                    // Matrix widget uses inverted indexing (127-i), so:
                    // For C3: 127-48 = 79
                    // For C6: 127-84 = 43
                    if (i == 79 || i == 43) {
                        // C3 or C6 lines
                        isRangeLine = true;
                    }
                }
                switch (_cachedStripStyle) {
                    case Appearance::onOctave:
                        // MIDI note 0 = C, so we want (127-i) % 12 == 0 for C notes
                        // Since i is inverted (127-i gives actual MIDI note), we need:
                        isHighlighted = ((127 - static_cast<unsigned int>(i)) % 12) == 0;
                        // Highlight C notes (octave boundaries)
                        break;
                    case Appearance::onSharp:
                        isHighlighted = !((1 << (static_cast<unsigned int>(i) % 12)) & sharp_strip_mask);
                        break;
                    case Appearance::onEven:
                        isHighlighted = (static_cast<unsigned int>(i) % 2);
                        break;
                }

                if (isRangeLine) {
                    c = _cachedRangeLineColor; // Range line color (C3/C6)
                } else if (_cachedStripStyle == Appearance::rainbowOctaves || 
                           _cachedStripStyle == Appearance::rainbowOctavesScale ||
                           _cachedStripStyle == Appearance::rainbowOctavesAlternating) {
                    int octave = (127 - i) / 12;
                    int noteInOctave = (127 - i) % 12;
                    
                    int hue = (octave * 40) % 360;
                    int saturation, value;
                    
                    bool isWhiteKey = !((1 << (static_cast<unsigned int>(i) % 12)) & sharp_strip_mask);

                    if (_cachedShouldUseDarkMode) {
                        saturation = 70;
                        value = 50;
                        
                        if (_cachedStripStyle == Appearance::rainbowOctavesScale) {
                            if (!isWhiteKey) {
                                value -= 8;
                            }
                        } else if (_cachedStripStyle == Appearance::rainbowOctavesAlternating && (noteInOctave % 2 == 1)) {
                            value -= 8;
                        }
                    } else {
                        saturation = 25;
                        value = 250;

                        if (_cachedStripStyle == Appearance::rainbowOctavesScale) {
                            if (!isWhiteKey) {
                                value -= 12;
                            }
                        } else if (_cachedStripStyle == Appearance::rainbowOctavesAlternating && (noteInOctave % 2 == 1)) {
                            value -= 12;
                        }
                    }
                    c = QColor::fromHsv(hue, saturation, value);
                } else if (isHighlighted) {
                    c = _cachedStripHighlightColor;
                } else {
                    c = _cachedStripNormalColor;
                }
            } else {
                // Program events section (lines >127) - use different colors than strips
                if (i % 2 == 1) {
                    c = _cachedProgramEventHighlightColor;
                } else {
                    c = _cachedProgramEventNormalColor;
                }
            }
            int nextLineY = yPosOfLine(i + 1);
            pixpainter->fillRect(lineNameWidth, startLine, width(),
                                 nextLineY - startLine, c);
        }

        // Fixed ruler height
        int rulerHeight = 50;

        // paint measures and timeline background
        pixpainter->fillRect(0, 0, width(), timeHeight, _cachedSystemWindowColor);

        pixpainter->setClipping(true);
        pixpainter->setClipRect(lineNameWidth, 0, width() - lineNameWidth - 2,
                                height());

        pixpainter->setPen(_cachedDarkGrayColor);
        pixpainter->setBrush(_cachedPianoWhiteKeyColor);
        
        // Box for measures/time text: Top, Left, and Right lines only (no bottom border for a cleaner look)
        pixpainter->drawLine(lineNameWidth, 2, width() - 1, 2);
        pixpainter->drawLine(lineNameWidth, 2, lineNameWidth, rulerHeight);
        pixpainter->drawLine(width() - 1, 2, width() - 1, rulerHeight);
        
        // Background for the ruler
        pixpainter->fillRect(lineNameWidth + 1, 3, width() - lineNameWidth - 2, rulerHeight - 4, _cachedPianoWhiteKeyColor);
        
        pixpainter->setPen(_cachedForegroundColor);

        if (_markerRowHeight > 0) {
            // Background for markers - match the ruler background
            pixpainter->fillRect(lineNameWidth, rulerHeight, width() - lineNameWidth, _markerRowHeight, _cachedPianoWhiteKeyColor);
            
            // Minimal top separator from ruler
            pixpainter->setPen(_cachedBorderColor);
            pixpainter->drawLine(lineNameWidth, rulerHeight, width(), rulerHeight);
        }

        // Clean transition to matrix area - no ugly thick borders
        pixpainter->setPen(_cachedBorderColor);
        pixpainter->drawLine(0, timeHeight - 1, width(), timeHeight - 1);

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
            if (_cachedShouldUseDarkMode) {
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
                    pixpainter->drawText(pos - textlength / 2, 19, text);
                }
                pixpainter->drawLine(pos, 24, pos, 50);
                startNumber += realstep;
            }
        }

        // draw measures foreground and text
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
            int xto = xPosOfMs(msOfTick(tick));
            pixpainter->setBrush(_cachedMeasureBarColor);
            pixpainter->setPen(Qt::NoPen);
            pixpainter->drawRoundedRect(xfrom + 2, 29, xto - xfrom - 4, 15, 5, 5);
            if (tick > startTick) {
                pixpainter->setPen(_cachedMeasureLineColor);
                // Extend measure lines from Y=25 through the marker gutter to height()
                // This ensures they align with the ruler and grid
                pixpainter->drawLine(xfrom, 25, xfrom, height());

                QString text = tr("Measure ") + QString::number(measure - 1);

                QFont font = pixpainter->font();
                QFontMetrics fm(font);
                int textlength = fm.horizontalAdvance(text);
                if (textlength > xto - xfrom) {
                    text = QString::number(measure - 1);
                    textlength = fm.horizontalAdvance(text);
                }

                // Align text to pixel boundaries for sharper rendering
                int pos = (xfrom + xto) / 2;
                int textX = static_cast<int>(std::round(pos - textlength / 2.0));
                
                // Base textY off a constant distance from the top constraint
                // so measure numbers don't shift when timeline height increases for markers
                int textY = 41;

                pixpainter->setPen(_cachedMeasureTextColor);
                pixpainter->drawText(textX, textY, text);

                if (_div >= 0 || _div <= -100) {
                    int ticksPerDiv;

                    if (_div >= 0) {
                        // Regular divisions: _div=0 (whole), _div=1 (half), _div=2 (quarter), etc.
                        // Formula: 4 / 2^_div quarters per division
                        double metronomeDiv = 4 / std::pow(2.0, _div);
                        ticksPerDiv = metronomeDiv * file->ticksPerQuarter();
                    } else if (_div <= -100) {
                        // Extended subdivision system: 
                        // -100 to -199: Triplets (÷3)
                        // -200 to -299: Quintuplets (÷5)
                        // -300 to -399: Sextuplets (÷6)
                        // -400 to -499: Septuplets (÷7)
                        // -500 to -599: Dotted notes (×1.5)
                        // -600 to -699: Double dotted notes (×1.75)

                        int subdivisionType = (-_div) / 100; // 1=triplets, 2=quintuplets, etc.
                        int baseDivision = (-_div) % 100; // Extract base division

                        double baseDiv = 4 / std::pow(2.0, baseDivision);

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
                    QPen dashPen = QPen(_cachedTimelineGridColor, 1, Qt::DashLine);
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

        // line between headers and matrixarea
        pixpainter->setPen(_cachedBorderColor);
        pixpainter->drawLine(0, timeHeight, width(), timeHeight);
        
        // Divider between headers and play area (lineNameWidth divider)
        pixpainter->drawLine(lineNameWidth, timeHeight, lineNameWidth, height());

        pixpainter->setPen(_cachedForegroundColor);

        // paint the events
        pixpainter->setClipping(true);
        pixpainter->setClipRect(lineNameWidth, timeHeight, width() - lineNameWidth,
                                height() - timeHeight);

        for (int i = 0; i < 19; i++) {
            paintChannel(pixpainter, i);
        }

        pixpainter->setClipping(false);

        delete pixpainter;
    }

    painter->drawPixmap(0, 0, *pixmap);

    painter->setClipping(true);
    painter->setClipRect(lineNameWidth, 0, width() - lineNameWidth, height());
    paintTimelineMarkers(painter);
    painter->setClipping(false);

    paintRecordingPreview(painter);

    painter->setRenderHint(QPainter::Antialiasing);
    // draw the piano / linenames
    _pendingKeyLabels.clear();
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
                    text = tr("Key Signature");
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

            if (text != "") {
                if (_cachedShouldUseDarkMode) {
                    painter->setPen(QColor(200, 200, 200)); // Light gray for dark mode
                } else {
                    painter->setPen(_cachedDarkGrayColor); // Use dark gray for better contrast in light mode
                }
                QFont font = Appearance::improveFont(painter->font());
                font.setPixelSize(10);
                painter->setFont(font);
                int textlength = QFontMetrics(font).horizontalAdvance(text);
                painter->drawText(lineNameWidth - 15 - textlength, startLine + lineHeight(), text);
            }
        }
    }

    // Second pass: Draw keybind labels on top of all piano keys
    // This ensures white key labels are always visible in front of black key tips
    if (!_pendingKeyLabels.isEmpty()) {
        QFont oldFont = painter->font();
        QFont boldFont = oldFont;
        boldFont.setBold(true);
        boldFont.setPixelSize(11); // Decreased from 12 for better fit
        painter->setFont(boldFont);
        QFontMetrics fm(boldFont);

        for (const PianoKeyLabel &label : _pendingKeyLabels) {
            int textW = fm.horizontalAdvance(label.keyName);
            int textX = label.keyX + (label.keyW - textW) / 2;
            int textY = label.keyY + (label.keyH + fm.ascent() - fm.descent()) / 2;

            if (label.isBlack || _cachedShouldUseDarkMode) {
                // Black key or Dark Mode white key: white text with full dark outline (embroidered)
                painter->setPen(QColor(0, 0, 0, 180));
                painter->drawText(textX - 1, textY - 1, label.keyName);
                painter->drawText(textX + 1, textY - 1, label.keyName);
                painter->drawText(textX - 1, textY + 1, label.keyName);
                painter->drawText(textX + 1, textY + 1, label.keyName);
                painter->drawText(textX, textY - 1, label.keyName);
                painter->drawText(textX, textY + 1, label.keyName);
                painter->drawText(textX - 1, textY, label.keyName);
                painter->drawText(textX + 1, textY, label.keyName);
                painter->setPen(Qt::white);
                painter->drawText(textX, textY, label.keyName);
            } else {
                // White key (Light Mode): dark text with full light outline for contrast
                QColor outline(255, 255, 255, 220);
                painter->setPen(outline);
                painter->drawText(textX - 1, textY - 1, label.keyName);
                painter->drawText(textX + 1, textY - 1, label.keyName);
                painter->drawText(textX - 1, textY + 1, label.keyName);
                painter->drawText(textX + 1, textY + 1, label.keyName);
                painter->drawText(textX - 1, textY, label.keyName);
                painter->drawText(textX + 1, textY, label.keyName);
                painter->drawText(textX, textY - 1, label.keyName);
                painter->drawText(textX, textY + 1, label.keyName);
                painter->setPen(QColor(50, 50, 50));
                painter->drawText(textX, textY, label.keyName);
            }
        }

        painter->setFont(oldFont);
        painter->setPen(_cachedForegroundColor);
    }

    if (Tool::currentTool()) {
        painter->setClipping(true);
        painter->setClipRect(ToolArea);
        Tool::currentTool()->draw(painter);
        painter->setClipping(false);
    }
    
    if (MidiPlayer::isPlaying()) {
        painter->setPen(_cachedPlaybackCursorColor);
        int x;
        if (Appearance::smoothPlaybackScrolling()) {
            // Under Spring-Camera, the viewport itself is smooth.
            // We use _lastRenderedMs to sync exactly with the camera's clock.
            x = xPosOfMs(_lastRenderedMs);
        } else {
            x = xPosOfMs(MidiPlayer::timeMs());
        }
        if (x >= lineNameWidth) {
            painter->drawLine(x, 0, x, height());
        }
        painter->setPen(_cachedForegroundColor);
    } else if (mouseInRect(TimeLineArea) && _lastHoverMs >= startTimeX && _lastHoverMs <= endTimeX) {
        // Red hover line (restricted to TimeLineArea as requested)
        painter->setPen(_cachedPlaybackCursorColor);
        int x = xPosOfMs(_lastHoverMs);
        if (x >= lineNameWidth) {
            painter->drawLine(x, 0, x, height());
        }
        painter->setPen(_cachedForegroundColor);
    }

    // Timeline area is the top 50px. Use this to bind the cursor instead of timeHeight
    // which may include the marker row (e.g. 66px) causing the cursor to stretch down.
    int rulerHeight = 50;

    // paint the cursorTick of file
    if (midiFile()->cursorTick() >= startTick && midiFile()->cursorTick() <= endTick) {
        painter->setPen(Qt::darkGray); // Original color for both modes
        int x = xPosOfMs(msOfTick(midiFile()->cursorTick()));
        painter->drawLine(x, 0, x, height());
        QPointF points[3] = {
            QPointF(x - 8, rulerHeight / 2 + 2),
            QPointF(x + 8, rulerHeight / 2 + 2),
            QPointF(x, rulerHeight - 2),
        };

        painter->setBrush(QBrush(_cachedCursorTriangleColor, Qt::SolidPattern));

        painter->drawPolygon(points, 3);
        painter->setPen(Qt::gray); // Original color for both modes
    }

    // paint the pauseTick of file if >= 0
    if (!MidiPlayer::isPlaying() && midiFile()->pauseTick() >= startTick && midiFile()->pauseTick() <= endTick) {
        int x = xPosOfMs(msOfTick(midiFile()->pauseTick()));

        QPointF points[3] = {
            QPointF(x - 8, rulerHeight / 2 + 2),
            QPointF(x + 8, rulerHeight / 2 + 2),
            QPointF(x, rulerHeight - 2),
        };

        painter->setBrush(QBrush(Appearance::grayColor(), Qt::SolidPattern));

        painter->drawPolygon(points, 3);
    }

    // border
    painter->setPen(_cachedBorderColor);
    painter->drawLine(width() - 1, height() - 1, lineNameWidth, height() - 1);
    painter->drawLine(width() - 1, height() - 1, width() - 1, 2);

    // if the recorder is recording, show red circle
    if (MidiInput::recording()) {
        painter->setBrush(_cachedRecordingIndicatorColor);
        painter->drawEllipse(width() - 20, timeHeight + 5, 15, 15);
    }
    delete painter;

    // if MouseRelease was not used, delete it
    mouseReleased = false;

    if (totalRepaint) {
        emit objectListChanged();
    }
}

void MatrixWidget::paintChannel(QPainter *painter, int channel) {
    // Use global visibility manager to avoid corrupted MidiChannel access
    if (!ChannelVisibilityManager::instance().isChannelVisible(channel)) {
        return;
    }
    QColor cC = *file->channel(channel)->color();

    // filter events
    QMultiMap<int, MidiEvent *> *map = file->channelEvents(channel);

    // PERFORMANCE: Create local QSets for fast lookups during this paint cycle
    // These are rebuilt each time so they can't get out of sync
    QSet<MidiEvent*> localObjectsSet;
    QSet<MidiEvent*> localVelocityObjectsSet;

    // PERFORMANCE: Use const iterator for faster iteration
    // Note: Spatial culling disabled to avoid missing notes that extend into viewport
    // The color caching optimization provides the main performance benefit
    QMultiMap<int, MidiEvent *>::const_iterator it = map->constBegin();
    QMultiMap<int, MidiEvent *>::const_iterator end = map->constEnd();

    for (; it != end; ++it) {
        MidiEvent *currentEvent = it.value();

        // Fast early rejection: check line visibility first (cheapest test)
        int line = currentEvent->line();
        if (line < startLineY || line > endLineY) {
            continue;
        }

        // Only do full eventInWidget check if line is visible
        if (!eventInWidget(currentEvent)) {
            continue;
        }

        // insert all Events in objects, set their coordinates
        // Only onEvents are inserted. When there is an On
        // and an OffEvent, the OnEvent will hold the coordinates
        // (line already declared above for early rejection check)

        // Cache dynamic_cast results to avoid repeated calls
        OffEvent *offEvent = dynamic_cast<OffEvent *>(currentEvent);
        OnEvent *onEvent = dynamic_cast<OnEvent *>(currentEvent);

        MidiEvent *event = currentEvent; // The event we'll process (may be reassigned to onEvent)

        // PERFORMANCE: Cache coordinate calculations (VTune shows GraphicObject::y taking 0.173s)
        int x, width;
        int y = yPosOfLine(line);
        int height = lineHeight();

        if (onEvent || offEvent) {
            if (onEvent) {
                offEvent = onEvent->offEvent();
            } else if (offEvent) {
                onEvent = dynamic_cast<OnEvent *>(offEvent->onEvent());
            }

            // Calculate raw coordinates
            int rawX = xPosOfMs(msOfTick(onEvent->midiTime()));
            int rawEndX = xPosOfMs(msOfTick(offEvent->midiTime()));

            // Clamp coordinates to viewport for partially visible notes
            x = qMax(rawX, lineNameWidth); // Don't start before the piano area
            int endX = qMin(rawEndX, this->width()); // Don't extend beyond widget width
            width = qMax(endX - x, 1); // Ensure minimum width of 1 pixel

            event = onEvent;
            if (localObjectsSet.contains(event)) {
                continue;
            }
        } else {
            width = PIXEL_PER_EVENT;
            x = xPosOfMs(msOfTick(currentEvent->midiTime()));
        }

        event->setX(x);
        event->setY(y);
        event->setWidth(width);
        event->setHeight(height);

        if (!(event->track()->hidden())) {
            // Get event color - either by channel or by track
            QColor eventColor = cC; // Use channel color by default
            if (!_colorsByChannels) {
                // Use track color - the Appearance class now handles performance optimization
                eventColor = *event->track()->color();
            }
            event->draw(painter, eventColor);

            if (_cachedSelection.contains(event)) {
                // PERFORMANCE: Only draw horizontal selection lines once per row to avoid massive overdraw
                if (!_cachedDrawnRowHighlights.contains(line)) {
                    painter->setPen(Qt::gray);
                    painter->drawLine(lineNameWidth, y, this->width(), y);
                    painter->drawLine(lineNameWidth, y + height, this->width(), y + height);
                    painter->setPen(_cachedForegroundColor);
                    _cachedDrawnRowHighlights.insert(line);
                }
            }
            objects->prepend(event);
            localObjectsSet.insert(event);
        }

        // append event to velocityObjects if its not a offEvent and if it
        // is in the x-Area
        MidiEvent *originalEvent = (onEvent || offEvent) ? currentEvent : event;
        if (!(originalEvent->track()->hidden())) {
            OffEvent *velocityOffEvent = dynamic_cast<OffEvent *>(originalEvent);
            if (!velocityOffEvent && originalEvent->midiTime() >= startTick && originalEvent->midiTime() <= endTick && !
                localVelocityObjectsSet.contains(originalEvent)) {
                originalEvent->setX(xPosOfMs(msOfTick(originalEvent->midiTime())));
                velocityObjects->prepend(originalEvent);
                localVelocityObjectsSet.insert(originalEvent);
            }
        }
    }
}

void MatrixWidget::paintPianoKey(QPainter *painter, int number, int x, int y,
                                 int width, int height) {
    int borderRight = 10;
    width = width - borderRight;
    if (number >= 0 && number <= 127) {
        int line = 127 - number;
        height = yPosOfLine(line + 1) - y;

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

        // Only highlight if using emulation keybinds OR if the key is explicitly latched/marked
        bool isActive = false;
        if (_latchedNotes.contains(number) || _markedNotes.contains(number)) {
            isActive = true;
        } else if (Appearance::accentKeyHighlight() && _cachedSelectedRows.contains(127 - number)) {
            // Also highlight if the row is selected and the Accent setting is enabled
            isActive = true;
        } else if (_isPianoEmulationEnabled && _activeEmulationNotes.contains(number)) {
            // Emulation notes are only active if emulation is enabled
            isActive = true;
        }
        
        // Selected is now just used for the hover effect in the matrix area (or as a fallback if Accent Highlight is disabled)
        bool selected = (mouseY >= y && mouseY < y + height && mouseX > lineNameWidth && mouseOver) ||
                        (!Appearance::accentKeyHighlight() && _cachedSelectedRows.contains(127 - number));

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
            QColor c = _cachedPianoBlackKeyColor;
            inRect = mouseOver && (mouseX <= lineNameWidth) && (number == _lastHoverPianoKey);
            painter->fillRect(playerRect, inRect ? _cachedPianoBlackKeyHoverColor : (isActive ? _cachedPianoBlackKeyActiveColor : (selected ? _cachedPianoBlackKeySelectedColor : c)));

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
            inRect = mouseOver && (mouseX <= lineNameWidth) && (number == _lastHoverPianoKey);
            pianoKeys.insert(number, QRect(x, y, width, height));
        }

        if (isBlack) {
            if (inRect) {
                painter->setBrush(_cachedPianoBlackKeyHoverColor);
            } else if (isActive) {
                painter->setBrush(_cachedPianoBlackKeyActiveColor);
            } else if (selected) {
                painter->setBrush(_cachedPianoBlackKeySelectedColor);
            } else {
                painter->setBrush(_cachedPianoBlackKeyColor);
            }
        } else {
            if (inRect) {
                painter->setBrush(_cachedPianoWhiteKeyHoverColor);
            } else if (isActive) {
                painter->setBrush(_cachedPianoWhiteKeyActiveColor);
            } else if (selected) {
                painter->setBrush(_cachedPianoWhiteKeySelectedColor);
            } else {
                painter->setBrush(_cachedPianoWhiteKeyColor);
            }
        }
        painter->setPen(_cachedDarkGrayColor);
        painter->drawPolygon(keyPolygon, Qt::OddEvenFill);

        if (name != "") {
            painter->setPen(Qt::gray); // Original color for both modes
            QFontMetrics fm(painter->font());
            int textlength = fm.horizontalAdvance(name);

            // Align text to pixel boundaries for sharper rendering
            int textX = x + width - textlength - 2;
            int textY = y + height - 1;

            painter->drawText(textX, textY, name);
            painter->setPen(_cachedForegroundColor);
        }
        
        bool isMapped = false;
        QString keyName;
        int unshiftedNumber = number - (_currentOctaveShift * 12);
        if (_isPianoEmulationEnabled && _noteToKeyMap.contains(unshiftedNumber)) {
            isMapped = true;
            keyName = QKeySequence(_noteToKeyMap.value(unshiftedNumber)).toString();
        }
        
        if (isMapped && !keyName.isEmpty()) {
            // Collect label data for deferred two-pass rendering
            // (drawn in paintEvent after ALL piano keys so white labels overlap black key tips)
            PianoKeyLabel label;
            label.keyName = keyName;
            label.isBlack = isBlack;
            if (isBlack) {
                // Position centered in the black key body
                label.keyX = x;
                label.keyY = y;
                label.keyW = (int)(width * scaleWidthBlack);
                label.keyH = (int)(height * scaleHeightBlack);
            } else {
                // Compute the actual visual top and bottom edges of the white key's wide section
                double topEdge = blackOnTop ? (y - height * scaleHeightBlack) : y;
                double bottomEdge = blackBeneath ? (y + height + height * scaleHeightBlack) : (y + height);
                
                label.keyX = x;
                label.keyY = (int)topEdge;
                label.keyW = width;
                label.keyH = (int)(bottomEdge - topEdge);
            }
            _pendingKeyLabels.append(label);
        }

        if (inRect && enabled) {
            // mark the current Line
            painter->fillRect(x + width + borderRight, yPosOfLine(127 - number),
                              this->width() - x - width - borderRight, height, _cachedPianoKeyLineHighlightColor);
        }
    }
}

void MatrixWidget::setFile(MidiFile *f) {
    stopAllEmulationNotes();
    file = f;

    scaleX = 1;
    scaleY = 1;

    startTimeX = 0;
    // Roughly vertically center on Middle C.
    startLineY = 50;

    connect(file->protocol(), SIGNAL(actionFinished()), this, SLOT(registerRelayout()));
    connect(file->protocol(), SIGNAL(actionFinished()), this, SLOT(update()));

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

void MatrixWidget::calcSizes() {
    if (!file) {
        return;
    }
    int time = file->maxTime();
    int timeInWidget = ((width() - lineNameWidth) * 1000) / (PIXEL_PER_S * scaleX);

    // Timeline Ruler is always 50px
    int rulerHeight = 50;
    _markerRowHeight = _hasVisibleMarkers ? 16 : 0;
    timeHeight = rulerHeight + _markerRowHeight;

    TimeLineArea = QRectF(lineNameWidth, 0, width() - lineNameWidth, rulerHeight);
    MarkerArea = QRectF(lineNameWidth, rulerHeight, width() - lineNameWidth, _markerRowHeight);
    PianoArea = QRectF(0, timeHeight, lineNameWidth, height() - timeHeight);
    ToolArea = QRectF(lineNameWidth, timeHeight, width() - lineNameWidth,
                      height() - timeHeight);

    // Call scroll methods with suppression to prevent cascading repaints
    _suppressScrollRepaints = true;
    scrollXChanged(startTimeX);
    scrollYChanged(startLineY);
    _suppressScrollRepaints = false;

    // Trigger single repaint after all scroll updates
    registerRelayout();
    update();

    emit sizeChanged(time - timeInWidget, NUM_LINES - endLineY + startLineY, startTimeX, startLineY);
}

MidiFile *MatrixWidget::midiFile() {
    return file;
}

void MatrixWidget::mouseMoveEvent(QMouseEvent *event) {
    PaintWidget::mouseMoveEvent(event);

    if (!enabled) {
        return;
    }

    int mouseX = qRound(event->position().x());
    int mouseY = qRound(event->position().y());
    int currentMs = msOfXPos(mouseX);
    
    // Determine the note under the cursor (or aligned with it in the matrix)
    int currentPianoKey = -1;
    if (mouseInRect(PianoArea)) {
        // Accurate note detection using the hit-test map (handles black keys)
        // Optimized loop using const_iterator; we check black keys as they overlap white keys
        int hitNote = -1;
        for (auto it = pianoKeys.constBegin(); it != pianoKeys.constEnd(); ++it) {
            const QRect &r = it.value();
            if (mouseX >= r.x() && mouseX < r.x() + r.width() &&
                mouseY >= r.y() && mouseY < r.y() + r.height()) {
                int note = it.key();
                // Check if it's a black key (1, 3, 6, 8, 10 in standard octave)
                if ((1 << (note % 12)) & 0x54A) {
                    hitNote = note;
                    break; // Black key found, it's definitely the intended one
                }
                hitNote = note; // Remember white key, but keep looking for a black key on top
            }
        }
        currentPianoKey = hitNote;

        // Fallback for white key "tips" (the right side of a black key's row)
        // If we are in the piano area but missed the black key rect, we should hit the white key below or above.
        if (currentPianoKey == -1) {
            int currentLine = startLineY;
            if (lineHeight() > 0) {
                currentLine = (mouseY - timeHeight) / lineHeight() + startLineY;
                while (yPosOfLine(currentLine + 1) <= mouseY) currentLine++;
                while (yPosOfLine(currentLine) > mouseY) currentLine--;
            }
            int note = 127 - currentLine;
            if ((1 << (note % 12)) & 0x54A) {
                // Split the black key row horizontally. Top half belongs to note + 1, bottom half to note - 1.
                int rowY = yPosOfLine(currentLine);
                int nextY = yPosOfLine(currentLine + 1);
                if (mouseY < rowY + (nextY - rowY) / 2.0) {
                    currentPianoKey = note + 1;
                } else {
                    currentPianoKey = note - 1;
                }
            } else {
                currentPianoKey = note;
            }
        }
    }
    
    // Fallback or alignment: If in matrix, use linear calculation
    // This ensures vertical movement in the matrix also triggers hover updates for the piano roll
    if (currentPianoKey == -1) {
        int currentLine = startLineY;
        if (lineHeight() > 0) {
            currentLine = (mouseY - timeHeight) / lineHeight() + startLineY;
            while (yPosOfLine(currentLine + 1) <= mouseY) currentLine++;
            while (yPosOfLine(currentLine) > mouseY) currentLine--;
        }
        currentPianoKey = 127 - currentLine;
    }
    
    bool inMarkerArea = mouseInRect(MarkerArea);
    MidiEvent *hoveredMarker = (inMarkerArea && !_draggedMarker) ? findTimelineMarkerNear(mouseX, mouseY) : nullptr;

    bool needsUpdate = false;

    if (!MidiPlayer::isPlaying() && Tool::currentTool()) {
        if (_draggedMarker && (event->buttons() & Qt::LeftButton)) {
            _isDraggingMarker = true;
            
            // Apply the drag tick offset to prevent snapping on pickup
            int mouseTick = file->tick(currentMs);
            int newTick = mouseTick + _draggedMarkerTickOffset;
            if (newTick < 0) newTick = 0;


            if (newTick != _draggedMarker->midiTime()) {
                // Update event position in the MIDI map real-time so drawing and hit-testing stay in sync
                MidiChannel *channel = file->channel(_draggedMarker->channel());
                if (channel) {
                    channel->removeEvent(_draggedMarker, false);
                    _draggedMarker->setMidiTime(newTick, false);
                    channel->insertEvent(_draggedMarker, newTick, false);
                    _draggedMarkerMs = currentMs;
                }
                needsUpdate = true;
            }
            setCursor(Qt::ClosedHandCursor);
        } else {
            // Check if tool movement requires an update (e.g. drawing notes or selection box)
            if (Tool::currentTool()->move(mouseX, mouseY) || (event->buttons() != Qt::NoButton)) {
                needsUpdate = true;
            }
            
            // Handle measure dragging if Measure select tool is active
            if (SelectTool *st = dynamic_cast<SelectTool *>(Tool::currentTool())) {
                if (st->stoolType() == SELECTION_TYPE_MEASURE && (event->buttons() & Qt::LeftButton)) {
                    needsUpdate = true;
                }
            }
            
            // Cursor updates
            if (inMarkerArea) {
                if (hoveredMarker) {
                    setCursor(Qt::OpenHandCursor);
                } else {
                    setCursor(Qt::ArrowCursor);
                }
            } else if (mouseInRect(TimeLineArea)) {
                setCursor(Qt::ArrowCursor);
            }
        }
    } else if (MidiPlayer::isPlaying()) {
        setCursor(Qt::ArrowCursor);
    }

    // Optimization: Only update if hover state changed, unless we already flagged for update.
    // IMPORTANT: If we are holding the mouse on the piano, we MUST process even if the hover key 
    // hasn't changed, because we might be waiting for the glissando delay to pass.
    if (!needsUpdate) {
        bool isPianoHolding = (event->buttons() == Qt::LeftButton) && mouseInRect(PianoArea);
        if (currentMs != _lastHoverMs || hoveredMarker != _lastHoverMarker || 
            currentPianoKey != _lastHoverPianoKey || inMarkerArea != _isGutterHovered ||
            (isPianoHolding && currentPianoKey != _pressedPianoNote)) {
            needsUpdate = true;
        }
    }

    if (needsUpdate) {


        // Piano glissando: Play notes as the user drags across the keyboard
        if (enabled && !MidiPlayer::isPlaying() && (event->buttons() == Qt::LeftButton) && mouseInRect(PianoArea) && 
            !(event->modifiers() & Qt::AltModifier) && currentPianoKey != -1) {
            
            qint64 now = QDateTime::currentMSecsSinceEpoch();
            int dragDistance = (event->pos() - _pressedPianoPos).manhattanLength();
            
            // Arming check: Must hold for 120ms OR move 8px to arm glissando.
            // This prevents rapid clicks from accidentally triggering a slide.
            if (now - _pressedPianoTime < 120 && dragDistance < 8) return;
            
            // Only switch notes if the target key is different from the currently sounding glissando note
            if (currentPianoKey != _pressedPianoNote) {
                // Use a 40ms delay for a fluid, professional feel.
                if (now - _lastGlissandoTime > 40) {
                    if (_pressedPianoNote != -1 && !_latchedNotes.contains(_pressedPianoNote)) {
                        stopNote(_pressedPianoNote);
                    }
                    
                    startNote(currentPianoKey);
                    _pressedPianoNote = currentPianoKey;
                    _lastGlissandoTime = now;
                }
            }
        }

        _lastHoverMs = currentMs;
        _lastHoverMarker = hoveredMarker;
        _lastHoverPianoKey = currentPianoKey;
        _isGutterHovered = inMarkerArea;
        update();
    }
}

void MatrixWidget::resizeEvent(QResizeEvent *event) {
    Q_UNUSED(event);
    calcSizes();
}

int MatrixWidget::xPosOfMs(int ms) {
    return lineNameWidth + (ms - startTimeX) * (width() - lineNameWidth) / (endTimeX - startTimeX);
}

int MatrixWidget::yPosOfLine(int line) {
    return timeHeight + (line - startLineY) * lineHeight();
}

int MatrixWidget::lineAtY(int y) {
    int currentLine = startLineY;
    if (lineHeight() > 0) {
        currentLine = (y - timeHeight) / lineHeight() + startLineY;
        while (yPosOfLine(currentLine + 1) <= y) currentLine++;
        while (yPosOfLine(currentLine) > y) currentLine--;
    }
    return currentLine;
}

double MatrixWidget::lineHeight() {
    if (endLineY - startLineY == 0)
        return 0;
    return (double) (height() - timeHeight) / (double) (endLineY - startLineY);
}

void MatrixWidget::enterEvent(QEnterEvent *event) {
    PaintWidget::enterEvent(event);
    if (Tool::currentTool()) {
        Tool::currentTool()->enter();
        if (enabled) {
            update();
        }
    }
}

void MatrixWidget::leaveEvent(QEvent *event) {
    PaintWidget::leaveEvent(event);
    if (Tool::currentTool()) {
        Tool::currentTool()->exit();
        if (enabled) {
            update();
        }
    }
}

void MatrixWidget::mousePressEvent(QMouseEvent *event) {
    EditorTool* currentTool = Tool::currentTool();

    // Special case for MeasureTool: Right-Click for context menu, Middle-Click for deletion
    if (dynamic_cast<MeasureTool*>(currentTool)) {
        if (event->button() == Qt::RightButton) {
            showContextMenu(event->globalPos(), event->pos());
            return;
        }
        if (event->button() == Qt::MiddleButton) {
            // Allow fall-through to tool press() handling
        }
    } else {
        // Standard behavior: Middle-Click for context menu
        if (event->button() == Qt::MiddleButton) {
            showContextMenu(event->globalPos(), event->pos());
            return;
        }
        
        if (event->button() == Qt::RightButton && (event->modifiers() & Qt::ControlModifier)) {
            // Ignore the press; let contextMenuEvent handle it cleanly on release
            return;
        }
    }

    PaintWidget::mousePressEvent(event);

    if (enabled && mouseInRect(MarkerArea) && event->button() == Qt::LeftButton && Tool::currentTool()) {
        int clickX = qRound(event->position().x());
        int clickY = qRound(event->position().y());
        _draggedMarker = findTimelineMarkerNear(clickX, clickY);
        if (_draggedMarker) {
            _draggedMarkerOriginalTime = _draggedMarker->midiTime();
            // Calculate offset to ensure marker follows cursor exactly without jumping on pickup
            _draggedMarkerTickOffset = _draggedMarker->midiTime() - file->tick(msOfXPos(clickX));
            _draggedMarkerMs = msOfXPos(clickX);

            if (file && file->protocol()) {
                file->protocol()->startNewAction(tr("Move Marker"));
            }

            // Select the event for immediate inspection in the Event Widget
            QList<MidiEvent *> selections;
            selections.append(_draggedMarker);
            Selection::instance()->setSelection(selections);

            update();
            return;
        }
    }

    if (!MidiPlayer::isPlaying() && Tool::currentTool() && mouseInRect(ToolArea)) {
        if (Tool::currentTool()->press(event->button() == Qt::LeftButton)) {
            if (enabled) {
                update();
            }
        }
    } else if (enabled && (!MidiPlayer::isPlaying()) && event->modifiers().testFlag(Qt::AltModifier)
               && event->button() == Qt::LeftButton && mouseInRect(TimeLineArea) && file) {
        // Alt+click on timeline: select all events in this measure
        // Use the logic implemented in SelectTool by creating a temp instance.
        SelectTool tempTool(SELECTION_TYPE_MEASURE);
        tempTool.setMatrixWidget(this);
        tempTool.selectMeasure(file->tick(msOfXPos(mouseX)), event->modifiers());
        update();
    } else if (enabled && (!MidiPlayer::isPlaying()) && event->modifiers().testFlag(Qt::AltModifier)
               && event->button() == Qt::LeftButton && mouseInRect(PianoArea) && file) {
        // Alt+click on piano key: select all events on this row
        // Use high-accuracy hit detection matching hover/glissando logic
        int hitNote = -1;
        for (auto it = pianoKeys.constBegin(); it != pianoKeys.constEnd(); ++it) {
            const QRect &r = it.value();
            if (mouseX >= r.x() && mouseX < r.x() + r.width() &&
                mouseY >= r.y() && mouseY < r.y() + r.height()) {
                int note = it.key();
                if ((1 << (note % 12)) & 0x54A) {
                    hitNote = note;
                    break;
                }
                hitNote = note;
            }
        }
        
        if (hitNote == -1) {
            // Fallback for white key "tips" (clicking the right side of a black key's row)
            int currentLine = startLineY;
            if (lineHeight() > 0) {
                currentLine = (mouseY - timeHeight) / lineHeight() + startLineY;
                while (yPosOfLine(currentLine + 1) <= mouseY) currentLine++;
                while (yPosOfLine(currentLine) > mouseY) currentLine--;
            }
            int note = 127 - currentLine;
            if ((1 << (note % 12)) & 0x54A) {
                int rowY = yPosOfLine(currentLine);
                int nextY = yPosOfLine(currentLine + 1);
                if (mouseY < rowY + (nextY - rowY) / 2.0) {
                    hitNote = note + 1;
                } else {
                    hitNote = note - 1;
                }
            } else {
                hitNote = note;
            }
        }

        if (hitNote != -1) {
            SelectTool tempTool(SELECTION_TYPE_ROW);
            tempTool.setMatrixWidget(this);
            tempTool.selectRow(127 - hitNote, event->modifiers());
            update();
        }
    } else if (enabled && (!MidiPlayer::isPlaying()) && (mouseInRect(PianoArea))) {
        // Find the correct note, giving priority to black keys (sharps) as they are drawn on top
        int hitNote = -1;
        for (auto it = pianoKeys.constBegin(); it != pianoKeys.constEnd(); ++it) {
            const QRect &r = it.value();
            if (mouseX >= r.x() && mouseX < r.x() + r.width() &&
                mouseY >= r.y() && mouseY < r.y() + r.height()) {
                int note = it.key();
                if ((1 << (note % 12)) & 0x54A) {
                    hitNote = note;
                    break;
                }
                hitNote = note;
            }
        }
        
        // Fallback for white key "tips" (clicking the right side of a black key's row)
        if (hitNote == -1) {
            int currentLine = startLineY;
            if (lineHeight() > 0) {
                currentLine = (mouseY - timeHeight) / lineHeight() + startLineY;
                while (yPosOfLine(currentLine + 1) <= mouseY) currentLine++;
                while (yPosOfLine(currentLine) > mouseY) currentLine--;
            }
            int note = 127 - currentLine;
            if ((1 << (note % 12)) & 0x54A) {
                int rowY = yPosOfLine(currentLine);
                int nextY = yPosOfLine(currentLine + 1);
                if (mouseY < rowY + (nextY - rowY) / 2.0) {
                    hitNote = note + 1;
                } else {
                    hitNote = note - 1;
                }
            } else {
                hitNote = note;
            }
        }
        
        if (hitNote != -1) {
            if (event->button() == Qt::LeftButton) {
                _pressedPianoNote = hitNote;
                _pressedPianoTime = QDateTime::currentMSecsSinceEpoch();
                _pressedPianoPos = event->pos();
                startNote(hitNote);
                
                // We no longer call Tool::press() here to prevent box selection from starting on the piano roll.
                // Selection logic for rows is now handled explicitly in the Alt+Click block above.
            } else if (event->button() == Qt::RightButton) {
                if (event->modifiers() & Qt::AltModifier) {
                    // Alt+Right-click: Toggle latch (infinite sustain with audio)
                    if (_latchedNotes.contains(hitNote)) {
                        _latchedNotes.remove(hitNote);
                        stopNote(hitNote);
                    } else {
                        _latchedNotes.insert(hitNote);
                        startNote(hitNote);
                    }
                } else {
                    // Right-click only: Toggle visual mark (no audio looping, just visual active state)
                    if (_markedNotes.contains(hitNote)) {
                        _markedNotes.remove(hitNote);
                    } else {
                        _markedNotes.insert(hitNote);
                    }
                    
                    _pressedPianoNote = hitNote;
                    _pressedPianoTime = QDateTime::currentMSecsSinceEpoch();
                    startNote(hitNote);
                }
            }
        }
    }
}

void MatrixWidget::mouseReleaseEvent(QMouseEvent *event) {
    if (_pressedPianoNote != -1 && (event->button() == Qt::LeftButton || event->button() == Qt::RightButton)) {
        qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - _pressedPianoTime;
        if (elapsed < 300) { // Slightly longer 300ms preview for a more musical feel
            int noteToStop = _pressedPianoNote;
            qint64 startTime = _pressedPianoTime;
            QTimer::singleShot(300 - elapsed, this, [this, noteToStop, startTime]() {
                // Only stop the note if this specific click hasn't been superseded by a newer one
                if (_pressedPianoNote == noteToStop && _pressedPianoTime == startTime) {
                    if (!_latchedNotes.contains(noteToStop)) {
                        stopNote(noteToStop);
                    }
                } else if (_pressedPianoNote != noteToStop) {
                    // If we moved to a new note, we should still ensure the old one eventually stops
                    if (!_latchedNotes.contains(noteToStop)) {
                        stopNote(noteToStop);
                    }
                }
            });
        } else {
            if (!_latchedNotes.contains(_pressedPianoNote)) {
                stopNote(_pressedPianoNote);
            }
        }
        _pressedPianoNote = -1;
    }
    if (_draggedMarker) {
        if (file && file->protocol() && _draggedMarkerOriginalTime >= 0) {
            // Revert time to create the "before" state for the protocol
            int newTime = _draggedMarker->midiTime();
            _draggedMarker->setMidiTime(_draggedMarkerOriginalTime, false);
            
            // Create a copy of the old state to pass into the protocol
            ProtocolEntry *toCopy = _draggedMarker->copy();
            
            // Set the time back into the new position WITHOUT making an extra protocol entry
            _draggedMarker->setMidiTime(newTime, false);
            
            // Submit the action to the protocol manually
            _draggedMarker->protocol(toCopy, _draggedMarker);
            
            file->protocol()->endAction();
        }
        _draggedMarker = nullptr;
        _draggedMarkerOriginalTime = -1;
        if (enabled) update();
        return;
    }

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

bool MatrixWidget::takeKeyPressEvent(QKeyEvent *event) {
    if (event->isAutoRepeat()) return false;

    bool handled = false;
    if (Tool::currentTool()) {
        if (Tool::currentTool()->pressKey(event->key())) {
            update();
            handled = true;
        }
    }

    if (pianoEmulator(event)) handled = true;
    return handled;
}

bool MatrixWidget::takeKeyReleaseEvent(QKeyEvent *event) {
    if (event->isAutoRepeat()) return false;

    bool handled = false;
    if (Tool::currentTool()) {
        if (Tool::currentTool()->releaseKey(event->key())) {
            update();
            handled = true;
        }
    }

    if (pianoEmulator(event)) handled = true;
    return handled;
}

void MatrixWidget::updateRenderingSettings() {
    // Update cached rendering settings from QSettings
    // This method should be called whenever rendering settings change in the settings dialog
    // to refresh the cached values and avoid expensive I/O during paint events
    _antialiasing = _settings->value("rendering/antialiasing", true).toBool();
    _smoothPixmapTransform = _settings->value("rendering/smooth_pixmap_transform", true).toBool();

    initKeyMap();

    // Update cached appearance colors to avoid expensive theme checks
    updateCachedAppearanceColors();

    // Force a redraw to apply the new settings
    registerRelayout();
    update();
}

void MatrixWidget::updateCachedAppearanceColors() {
    // Cache all appearance colors and settings to avoid expensive calls during paint events
    // This should be called whenever the theme changes (light/dark mode, etc.)
    _cachedBackgroundColor = Appearance::backgroundColor();
    _cachedForegroundColor = Appearance::foregroundColor();
    _cachedShouldUseDarkMode = Appearance::shouldUseDarkMode();
    _cachedShowRangeLines = Appearance::showRangeLines();
    _cachedStripStyle = Appearance::strip();
    
    // Check marker visibility
    _hasVisibleMarkers = Appearance::showTextEventMarkers() || 
                         Appearance::showProgramChangeMarkers() || 
                         Appearance::showControlChangeMarkers();
    
    int newTimeHeight = _hasVisibleMarkers ? 66 : 50;
    if (newTimeHeight != timeHeight) {
        timeHeight = newTimeHeight;
        // Recalculate areas immediately to prevent overlap/layout glitches
        calcSizes();
    }
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
    _cachedBorderColor = Appearance::borderColor();

    // Cache piano key colors
    _cachedPianoBlackKeyColor = Appearance::pianoBlackKeyColor();
    _cachedPianoBlackKeyHoverColor = Appearance::pianoBlackKeyHoverColor();
    _cachedPianoBlackKeySelectedColor = Appearance::pianoBlackKeySelectedColor();
    _cachedPianoBlackKeyActiveColor = Appearance::pianoBlackKeyActiveColor();
    _cachedPianoWhiteKeyColor = Appearance::pianoWhiteKeyColor();
    _cachedPianoWhiteKeyHoverColor = Appearance::pianoWhiteKeyHoverColor();
    _cachedPianoWhiteKeySelectedColor = Appearance::pianoWhiteKeySelectedColor();
    _cachedPianoWhiteKeyActiveColor = Appearance::pianoWhiteKeyActiveColor();
    _cachedPianoKeyLineHighlightColor = Appearance::pianoKeyLineHighlightColor();

    // Cache theme state to avoid expensive shouldUseDarkMode() calls
    _cachedShouldUseDarkMode = Appearance::shouldUseDarkMode();
}

bool MatrixWidget::pianoEmulator(QKeyEvent *event) {
    if (!_isPianoEmulationEnabled) return false;

    int key = event->key();
    
    // Check octave shift via native virtual key (supports Left/Right Shift differentiation)
    int octaveUpKey = _settings->value("piano_emulation/octave_up", Qt::Key_Period).toInt();   // default: '.'
    int octaveDownKey = _settings->value("piano_emulation/octave_down", Qt::Key_Comma).toInt(); // default: ','
    
    // Convert to uppercase for GetAsyncKeyState if it's a standard letter/number key
    int physUpKey = octaveUpKey;
    int physDownKey = octaveDownKey;
    
    // Handle standard keys mapping for GetAsyncKeyState
    if (octaveUpKey == Qt::Key_Period) physUpKey = VK_OEM_PERIOD;
    if (octaveDownKey == Qt::Key_Comma) physDownKey = VK_OEM_COMMA;
    
    quint32 nativeVK = event->nativeVirtualKey();
    
    // Also match by Qt key for non-native modifiers (Ctrl, Alt, Space, Tab)
    bool isOctaveUp = false;
    bool isOctaveDown = false;
    
    if (octaveUpKey == 0xA0 || octaveUpKey == 0xA1) {
        // Check specific L/R shift via native VK
        isOctaveUp = (nativeVK == (quint32)octaveUpKey);
        // Fallback: if nativeVK is generic VK_SHIFT (0x10) or Qt reports Key_Shift
        if (!isOctaveUp && (nativeVK == 0x10 || key == Qt::Key_Shift)) {
            // Use Windows API to check specific key state
#ifdef Q_OS_WIN
            isOctaveUp = (GetAsyncKeyState(octaveUpKey) & 0x8000) != 0;
#endif
        }
    } else if (octaveUpKey != 0) {
        isOctaveUp = (key == octaveUpKey);
    }
    
    if (octaveDownKey == 0xA0 || octaveDownKey == 0xA1) {
        isOctaveDown = (nativeVK == (quint32)octaveDownKey);
        if (!isOctaveDown && (nativeVK == 0x10 || key == Qt::Key_Shift)) {
#ifdef Q_OS_WIN
            isOctaveDown = (GetAsyncKeyState(octaveDownKey) & 0x8000) != 0;
#endif
        }
    } else if (octaveDownKey != 0) {
        isOctaveDown = (key == octaveDownKey);
    }
    
    if (isOctaveUp || isOctaveDown) {
        if (!event->isAutoRepeat()) {
            if (isOctaveUp) _isOctaveUpPressed = (event->type() == QEvent::KeyPress);
            if (isOctaveDown) _isOctaveDownPressed = (event->type() == QEvent::KeyPress);
            
#ifdef Q_OS_WIN
            // Physical state double-check to prevent "sticky" behavior
            if (physUpKey != 0) _isOctaveUpPressed = (GetAsyncKeyState(physUpKey) & 0x8000) != 0;
            if (physDownKey != 0) _isOctaveDownPressed = (GetAsyncKeyState(physDownKey) & 0x8000) != 0;
#endif
            
            _currentOctaveShift = _isOctaveUpPressed ? 1 : (_isOctaveDownPressed ? -1 : 0);
            update();
        }
        return true;
    }
    
    // On ANY key release, re-check physical state of octave modifier keys
    // This catches cases where the release event for the modifier was lost
    if (event->type() == QEvent::KeyRelease) {
#ifdef Q_OS_WIN
        bool physUp = (physUpKey != 0) ? ((GetAsyncKeyState(physUpKey) & 0x8000) != 0) : false;
        bool physDown = (physDownKey != 0) ? ((GetAsyncKeyState(physDownKey) & 0x8000) != 0) : false;
        if (_isOctaveUpPressed != physUp || _isOctaveDownPressed != physDown) {
            _isOctaveUpPressed = physUp;
            _isOctaveDownPressed = physDown;
            _currentOctaveShift = _isOctaveUpPressed ? 1 : (_isOctaveDownPressed ? -1 : 0);
            update();
        }
#endif
    }

    if (event->type() == QEvent::KeyPress) {
        if (event->isAutoRepeat()) return false;
        if (!_keyToNoteMap.contains(key)) return false;
        
        int note = _keyToNoteMap.value(key);
        note += (_currentOctaveShift * 12);
        
        _playingKeyToNote.insert(key, note);
        startNote(note);
        return true;
    } else if (event->type() == QEvent::KeyRelease) {
        if (event->isAutoRepeat()) return false;
        
        if (_playingKeyToNote.contains(key)) {
            int note = _playingKeyToNote.take(key);
            stopNote(note);
            return true;
        }
        
        if (_keyToNoteMap.contains(key)) return true;
    }
    
    return false;
}

void MatrixWidget::initKeyMap() {
    _keyToNoteMap.clear();
    _noteToKeyMap.clear();

    if (_settings->contains("piano_emulation/keys")) {
        QVariantMap map = _settings->value("piano_emulation/keys").toMap();
        for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
            int offset = it.key().toInt();
            int key = it.value().toInt();
            if (key != 0) {
                _keyToNoteMap.insert(key, offset);
                _noteToKeyMap.insert(offset, key);
            }
        }
    } else {
        const int C4_OFFSET = 60;

        // Default: 1-octave QWERTY layout (C4 to C5)
        _keyToNoteMap.insert(Qt::Key_Q, C4_OFFSET + 0);   // C4
        _keyToNoteMap.insert(Qt::Key_2, C4_OFFSET + 1);   // C#4
        _keyToNoteMap.insert(Qt::Key_W, C4_OFFSET + 2);   // D4
        _keyToNoteMap.insert(Qt::Key_3, C4_OFFSET + 3);   // D#4
        _keyToNoteMap.insert(Qt::Key_E, C4_OFFSET + 4);   // E4
        _keyToNoteMap.insert(Qt::Key_R, C4_OFFSET + 5);   // F4
        _keyToNoteMap.insert(Qt::Key_5, C4_OFFSET + 6);   // F#4
        _keyToNoteMap.insert(Qt::Key_T, C4_OFFSET + 7);   // G4
        _keyToNoteMap.insert(Qt::Key_6, C4_OFFSET + 8);   // G#4
        _keyToNoteMap.insert(Qt::Key_Y, C4_OFFSET + 9);   // A4
        _keyToNoteMap.insert(Qt::Key_7, C4_OFFSET + 10);  // A#4
        _keyToNoteMap.insert(Qt::Key_U, C4_OFFSET + 11);  // B4
        _keyToNoteMap.insert(Qt::Key_I, C4_OFFSET + 12);  // C5
        
        // Build reverse map for defaults
        for (auto it = _keyToNoteMap.constBegin(); it != _keyToNoteMap.constEnd(); ++it) {
            _noteToKeyMap.insert(it.value(), it.key());
        }
    }
}

void MatrixWidget::startNote(int note) {
    // If note is already playing (rapid re-click), force-stop it first
    if (_activeEmulationNotes.contains(note)) {
        stopNote(note);
    }

    int ch = MidiOutput::standardChannel();
    if (ch < 0 || ch >= 16) return;

    // Sync program and controls
    if (file) {
        int prog = file->channel(ch)->progAtTick(file->cursorTick());
        MidiOutput::sendProgram(ch, prog);
        
        // Reset volume/expression for preview
        QByteArray reset;
        reset.append((char)(0xB0 | ch)); reset.append((char)7); reset.append((char)127);
        MidiOutput::sendCommand(reset);
        reset.clear();
        reset.append((char)(0xB0 | ch)); reset.append((char)11); reset.append((char)127);
        MidiOutput::sendCommand(reset);
    }

    // Play audio
    QByteArray on;
    on.append((char)(0x90 | ch));
    on.append((char)note);
    on.append((char)100); // Velocity
    MidiOutput::sendCommand(on);

    // Recording
    if (MidiInput::recording()) {
        MidiInput::injectMessage(0x90 | ch, note, 100);
        if (file) {
            // Use the playback tick (not cursor tick) so ghost notes start at the playhead
            int playTick = file->tick(MidiPlayer::isPlaying() ? MidiPlayer::timeMs() : msOfTick(file->cursorTick()));
            _noteStartTicks[note] = playTick;
        }
    }

    _activeEmulationNotes.insert(note);
    update();
}

void MatrixWidget::stopNote(int note) {
    if (!_activeEmulationNotes.contains(note)) return;

    int ch = MidiOutput::standardChannel();
    if (ch < 0 || ch >= 16) return;

    // Stop audio
    QByteArray off;
    off.append((char)(0x80 | ch));
    off.append((char)note);
    off.append((char)0);
    MidiOutput::sendCommand(off);

    // Recording: Clean up the ghost preview tick when the note stops
    if (MidiInput::recording()) {
        MidiInput::injectMessage(0x80 | ch, note, 0);
        _noteStartTicks.remove(note);
    }

    _activeEmulationNotes.remove(note);
    _latchedNotes.remove(note);
    update();
}

void MatrixWidget::stopAllEmulationNotes() {
    // Send panic to all active channels for any emulation notes
    for (int note : _activeEmulationNotes) {
        int ch = MidiOutput::standardChannel();
        if (ch >= 0 && ch < 16) {
            QByteArray off;
            off.append((char)(0x80 | ch));
            off.append((char)note);
            off.append((char)0);
            MidiOutput::sendCommand(off);

            if (MidiInput::recording()) {
                MidiInput::injectMessage(0x80 | ch, note, 0);
            }
        }
    }
    
    _activeEmulationNotes.clear();
    _latchedNotes.clear();
    _pressedPianoNote = -1;
    _pressedPianoTime = 0;
    _currentOctaveShift = 0;
    _isOctaveUpPressed = false;
    _isOctaveDownPressed = false;
    _isGutterHovered = false;
    update();
}

void MatrixWidget::playNote(int note) {
    int ch = MidiOutput::standardChannel();

    // Always re-sync the channel's program with the synthesizer before playing,
    // so the preview reflects any instrument changes or deleted program events.
    if (file && ch >= 0 && ch < 16) {
        int prog = file->channel(ch)->progAtTick(file->cursorTick());
        MidiOutput::sendProgram(ch, prog);

        // Ensure the channel is audible by resetting Volume (CC 7) and Expression (CC 11)
        QByteArray vol;
        vol.append((char)(0xB0 | ch));
        vol.append((char)7);
        vol.append((char)127);
        MidiOutput::sendCommand(vol);

        QByteArray exp;
        exp.append((char)(0xB0 | ch));
        exp.append((char)11);
        exp.append((char)127);
        MidiOutput::sendCommand(exp);
    }

    pianoEvent->setNote(note);
    pianoEvent->setChannel(ch, false);
    MidiPlayer::play(pianoEvent);
}

QList<MidiEvent *> *MatrixWidget::activeEvents() {
    return objects;
}

QList<MidiEvent *> *MatrixWidget::velocityEvents() {
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

bool MatrixWidget::eventInWidget(MidiEvent *event) {
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
    if (scaleX <= 3.0) { // Prevent excessive zoom in
        scaleX += 0.1;
        calcSizes();
    }
}

void MatrixWidget::zoomHorOut() {
    if (scaleX >= 0.2) {
        scaleX -= 0.1;
        calcSizes();
    }
}

void MatrixWidget::zoomVerIn() {
    if (scaleY <= 3.0) { // Prevent excessive zoom in
        scaleY += 0.1;
        calcSizes();
    }
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

void MatrixWidget::mouseDoubleClickEvent(QMouseEvent *event) {
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

void MatrixWidget::wheelEvent(QWheelEvent *event) {
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
        } else if (km == (Qt::ControlModifier | Qt::ShiftModifier)) {
            // Ctrl+Shift+wheel for scroll bar-like vertical scrolling (multiple lines at once)
            if (pixelDeltaLinear != 0) {
                // Use a larger scroll multiplier to match scroll bar behavior
                int scrollMultiplier = 5; // Scroll 5x more than normal wheel scrolling
                verScrollAmount = pixelDeltaLinear * scrollMultiplier;
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
            emit scrollChanged(startTimeX, maxTimeInFile - widgetRange, startLineY,
                               NUM_LINES - (endLineY - startLineY));
        }

        if (verScrollAmount != 0) {
            // Calculate normal scroll amount based on zoom level
            double scrollDelta = -verScrollAmount / (scaleY * PIXEL_PER_LINE);
            int linesToScroll = (int) round(scrollDelta);

            // Ensure we always move at least 1 line when zoomed in to avoid "dead" scrolls
            if (linesToScroll == 0 && verScrollAmount != 0) {
                linesToScroll = (verScrollAmount > 0) ? -1 : 1;
            }

            int newStartLineY = startLineY + linesToScroll;

            if (newStartLineY < 0) {
                newStartLineY = 0;
            }

            // endline too large handled in scrollYchanged()
            scrollYChanged(newStartLineY);
            emit scrollChanged(startTimeX, maxTimeInFile - widgetRange, startLineY,
                               NUM_LINES - (endLineY - startLineY));
        }
    }
}

void MatrixWidget::keyPressEvent(QKeyEvent *event) {
    takeKeyPressEvent(event);
}

void MatrixWidget::keyReleaseEvent(QKeyEvent *event) {
    takeKeyReleaseEvent(event);
}

void MatrixWidget::contextMenuEvent(QContextMenuEvent* event)
{
    // Context menu requires Ctrl + Right Click
    if (!(QApplication::keyboardModifiers() & Qt::ControlModifier)) {
        return;
    }

    showContextMenu(event->globalPos(), event->pos());
}

void MatrixWidget::showContextMenu(const QPoint& globalPos, const QPoint& localPos)
{
    // Only appear for Standard Tool and certain Select Tools
    EditorTool* currentTool = Tool::currentTool();
    
    // Handle MeasureTool specifically
    if (MeasureTool* mt = dynamic_cast<MeasureTool*>(currentTool)) {
        // Only show context menu if not near a divider (insertion point)
        int dist, measureX;
        // Hack: We need mouseX in the tool, but we can calculate it from localPos.x()
        // closestMeasureStart is private, so we'll just check it here.
        int ms = msOfXPos(localPos.x());
        int tick = file->tick(ms);
        int mStart, mEnd;
        file->measure(tick, &mStart, &mEnd);
        int x1 = xPosOfMs(file->msOfTick(mStart));
        int x2 = xPosOfMs(file->msOfTick(mEnd));
        int dStart = abs(localPos.x() - x1);
        int dEnd = abs(localPos.x() - x2);
        
        if (dStart >= 20 && dEnd >= 20) {
            QMenu contextMenu(this);
            QAction* delAction = contextMenu.addAction(tr("Delete Selected Measures"));
            connect(delAction, &QAction::triggered, [mt](){ mt->deleteSelectedMeasures(); });
            
            QAction* unselectAction = contextMenu.addAction(tr("Unselect All"));
            connect(unselectAction, &QAction::triggered, [mt](){ mt->clearSelection(); });
            
            contextMenu.exec(globalPos);
        }
        return;
    }

    bool allowedTool = false;
    if (dynamic_cast<StandardTool*>(currentTool) || dynamic_cast<SelectTool*>(currentTool)) {
        allowedTool = true;
    }

    if (!allowedTool || !file) {
        return;
    }

    QList<MidiEvent*> selectedEvents = Selection::instance()->selectedEvents();
    bool hasSelection = !selectedEvents.isEmpty();
    bool hasClipboard = (EventTool::copiedEvents && !EventTool::copiedEvents->isEmpty()) || EventTool::hasSharedClipboardData();

    // Create the context menu
    QMenu contextMenu(this);

    // Top Actions (Windows 11 Style Horizontal Bar)
    QWidget* actionHeader = new QWidget(&contextMenu);
    QHBoxLayout* headerLayout = new QHBoxLayout(actionHeader);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(0);

    // Creates a context menu action button with native hover state.
    // Uses QToolButton as the background host to inherit styling but overrides internals
    // with a rigorous layout to guarantee text/icon alignment regardless of icon scaling.
    auto createToolButton = [&](const QString& text, const QString& iconPath, bool enabled, int iconSize = 24) {
        QToolButton* btn = new QToolButton(actionHeader);
        btn->setAutoRaise(true); // Native flat style & hover highlight
        btn->setEnabled(enabled);
        btn->setStyleSheet("QToolButton { padding: 0px; margin: 0px; }");

        QVBoxLayout* vLayout = new QVBoxLayout(btn);
        vLayout->setContentsMargins(2, 4, 2, 2);
        vLayout->setSpacing(0);

        QLabel* iconLabel = new QLabel(btn);
        QIcon icon = Appearance::adjustIconForDarkMode(iconPath);
        qreal dpr = btn->devicePixelRatioF();
        QPixmap pixmap = icon.pixmap(QSize(iconSize, iconSize), dpr, enabled ? QIcon::Normal : QIcon::Disabled);
        iconLabel->setPixmap(pixmap);
        iconLabel->setAlignment(Qt::AlignCenter);
        iconLabel->setAttribute(Qt::WA_TransparentForMouseEvents);

        QLabel* textLabel = new QLabel(text, btn);
        textLabel->setAlignment(Qt::AlignHCenter | Qt::AlignBottom);
        textLabel->setEnabled(enabled);
        textLabel->setAttribute(Qt::WA_TransparentForMouseEvents);

        vLayout->addWidget(iconLabel, 1, Qt::AlignCenter);
        vLayout->addWidget(textLabel, 0, Qt::AlignHCenter);

        btn->setMinimumHeight(48);
        btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

        return btn;
    };

    QToolButton* copyBtn = createToolButton(tr("Copy"), ":/run_environment/graphics/tool/copy.png", hasSelection);
    QToolButton* pasteBtn = createToolButton(tr("Paste"), ":/run_environment/graphics/tool/paste.png", hasClipboard);
    // Delete icon has significant invisible padding in graphic file; bumping standard to 32px helps it match 24px core
    QToolButton* deleteBtn = createToolButton(tr("Delete"), ":/run_environment/graphics/tool/eraser.png", hasSelection, 32);

    headerLayout->addWidget(copyBtn);
    headerLayout->addWidget(pasteBtn);
    headerLayout->addWidget(deleteBtn);

    QWidgetAction* headerAction = new QWidgetAction(&contextMenu);
    headerAction->setDefaultWidget(actionHeader);
    contextMenu.addAction(headerAction);

    // Connect buttons to close menu and trigger actions
    connect(copyBtn, &QToolButton::clicked, [&](){ 
        contextMenu.close(); 
        EditorTool::mainWindow()->copy(); 
    });
    connect(pasteBtn, &QToolButton::clicked, [&](){ 
        contextMenu.close(); 
        int tick = file->tick(msOfXPos(localPos.x()));
        EditorTool::mainWindow()->pasteAt(tick); 
    });
    connect(deleteBtn, &QToolButton::clicked, [&](){ 
        contextMenu.close(); 
        EditorTool::mainWindow()->deleteSelectedEvents(); 
    });

    contextMenu.addSeparator();

    // Selection Operations
    QAction* quantizeAction = contextMenu.addAction(tr("Quantize Selection"));
    // Appearance::setActionIcon(quantizeAction, ":/run_environment/graphics/tool/quantize.png");
    quantizeAction->setEnabled(hasSelection);

    contextMenu.addSeparator();

    // Move to Track
    QMenu* moveToTrackMenu = contextMenu.addMenu(tr("Move Events to Track..."));
    moveToTrackMenu->setEnabled(hasSelection);
    for (int i = 0; i < file->numTracks(); i++) {
        QString trackName = file->tracks()->at(i)->name();
        QAction* action = moveToTrackMenu->addAction(tr("Track %1: %2").arg(i).arg(trackName));
        action->setData(i);
    }

    // Move to Channel
    QMenu* moveToChannelMenu = contextMenu.addMenu(tr("Move Events to Channel..."));
    moveToChannelMenu->setEnabled(hasSelection);
    for (int i = 0; i < 16; i++) {
        QString instName;
        if (i == 9) {
            instName = tr("Percussion");
        } else {
            // Use the instrument at the first event's tick for better accuracy
            int tick = 0;
            if (!selectedEvents.isEmpty()) {
                tick = selectedEvents.first()->midiTime();
            }
            instName = MidiFile::instrumentName(file->channel(i)->progAtTick(tick));
        }
        QAction* action = moveToChannelMenu->addAction(tr("Channel %1: %2").arg(i).arg(instName));
        action->setData(i);
    }

    contextMenu.addSeparator();

    // Transpose actions
    QMenu* transposeSubMenu = contextMenu.addMenu(tr("Transpose Selection"));
    // Appearance::setActionIcon(transposeSubMenu->menuAction(), ":/run_environment/graphics/tool/transpose.png");
    transposeSubMenu->setEnabled(hasSelection);
    QAction* transposeAction = transposeSubMenu->addAction(tr("Transpose..."));
    // Appearance::setActionIcon(transposeAction, ":/run_environment/graphics/tool/transpose.png");
    QAction* octaveUpAction = transposeSubMenu->addAction(tr("Octave Up"));
    // Appearance::setActionIcon(octaveUpAction, ":/run_environment/graphics/tool/transpose_up.png");
    QAction* octaveDownAction = transposeSubMenu->addAction(tr("Octave Down"));
    // Appearance::setActionIcon(octaveDownAction, ":/run_environment/graphics/tool/transpose_down.png");

    // General actions
    QAction* scaleAction = contextMenu.addAction(tr("Scale Events"));
    scaleAction->setEnabled(hasSelection);
    contextMenu.addSeparator();

    // Duration Adjustments (Per-Note)
    contextMenu.addSeparator();
    QAction* fitBetweenAction = contextMenu.addAction(tr("Stretch to Fill Neighbors"));
    fitBetweenAction->setEnabled(hasSelection);
    QAction* extendStartAction = contextMenu.addAction(tr("Extend Start to Previous End"));
    extendStartAction->setEnabled(hasSelection);
    QAction* extendEndAction = contextMenu.addAction(tr("Extend End to Next Start"));
    extendEndAction->setEnabled(hasSelection);

    // Snapping (Block-Based Movement)
    contextMenu.addSeparator();
    QAction* snapStartAction = contextMenu.addAction(tr("Snap Start to Previous End"));
    snapStartAction->setEnabled(hasSelection);
    QAction* snapEndAction = contextMenu.addAction(tr("Snap End to Next Start"));
    snapEndAction->setEnabled(hasSelection);

    // Export
    contextMenu.addSeparator();
    QAction* exportAudioAction = contextMenu.addAction(tr("Export Selection as Audio..."));
    exportAudioAction->setEnabled(hasSelection && MidiOutput::isFluidSynthOutput());

    // Show menu and get selected action
    QAction* selectedAction = contextMenu.exec(globalPos);

    if (!selectedAction) {
        return; // User cancelled
    }

    // Sort selection for logical left-to-right processing
    QList<MidiEvent*> sortedSelection = Selection::instance()->selectedEvents();
    std::sort(sortedSelection.begin(), sortedSelection.end(), [](MidiEvent* a, MidiEvent* b) {
        return a->midiTime() < b->midiTime();
    });

    // Handle Actions
    if (!selectedAction) {
        return; // User cancelled OR horizontal action was triggered manually via connect
    }

    // Handle Quantize
    if (selectedAction == quantizeAction) {
        EditorTool::mainWindow()->quantizeSelection();
    }
    // Handle Move to Channel
    else if (moveToChannelMenu->actions().contains(selectedAction)) {
        EditorTool::mainWindow()->moveSelectedEventsToChannel(selectedAction);
    }
    // Handle Move to Track
    else if (moveToTrackMenu->actions().contains(selectedAction)) {
        EditorTool::mainWindow()->moveSelectedEventsToTrack(selectedAction);
    }
    // Handle Transpose
    else if (selectedAction == transposeAction) {
        EditorTool::mainWindow()->transposeNSemitones();
    }
    // Handle Transpose Octave Up
    else if (selectedAction == octaveUpAction) {
        EditorTool::mainWindow()->transposeSelectedNotesOctaveUp();
    }
    // Handle Export Selection as Audio
    else if (selectedAction == exportAudioAction) {
        EditorTool::mainWindow()->exportAudio();
    }
    // Handle Transpose Octave Down
    else if (selectedAction == octaveDownAction) {
        EditorTool::mainWindow()->transposeSelectedNotesOctaveDown();
    }
    // Handle Scale Events
    else if (selectedAction == scaleAction) {
        EditorTool::mainWindow()->scaleSelection();
    }
    // Handle Stretch to Fill Neighbors (Per-note)
    else if (selectedAction == fitBetweenAction) {
        file->protocol()->startNewAction(tr("Stretch to Fill Neighbors"));
        foreach (MidiEvent* ev, sortedSelection) {
            NoteOnEvent* note = dynamic_cast<NoteOnEvent*>(ev);
            if (!note) continue;
            NoteOnEvent* prevNote = findPreviousNoteInTrack(note);
            NoteOnEvent* nextNote = findNextNoteInTrack(note);
            if (prevNote && nextNote) {
                note->setMidiTime(prevNote->offEvent()->midiTime());
                note->offEvent()->setMidiTime(nextNote->midiTime());
            }
        }
        file->protocol()->endAction();
    }
    // Handle Extend Start to Previous End (Per-note)
    else if (selectedAction == extendStartAction) {
        file->protocol()->startNewAction(tr("Extend Start to Previous End"));
        foreach (MidiEvent* ev, sortedSelection) {
            NoteOnEvent* note = dynamic_cast<NoteOnEvent*>(ev);
            if (!note) continue;
            NoteOnEvent* prevNote = findPreviousNoteInTrack(note);
            if (prevNote) {
                note->setMidiTime(prevNote->offEvent()->midiTime());
            }
        }
        file->protocol()->endAction();
    }
    // Handle Extend End to Next Start (Per-note)
    else if (selectedAction == extendEndAction) {
        file->protocol()->startNewAction(tr("Extend End to Next Start"));
        foreach (MidiEvent* ev, sortedSelection) {
            NoteOnEvent* note = dynamic_cast<NoteOnEvent*>(ev);
            if (!note) continue;
            NoteOnEvent* nextNote = findNextNoteInTrack(note);
            if (nextNote) {
                note->offEvent()->setMidiTime(nextNote->midiTime());
            }
        }
        file->protocol()->endAction();
    }
    // Handle Snap Start to Previous End (Block-based)
    else if (selectedAction == snapStartAction) {
        if (!sortedSelection.isEmpty()) {
            NoteOnEvent* earliest = nullptr;
            foreach (MidiEvent* ev, sortedSelection) {
                if ((earliest = dynamic_cast<NoteOnEvent*>(ev))) break;
            }
            if (earliest) {
                std::set<MidiEvent*> selectionSet(sortedSelection.begin(), sortedSelection.end());
                NoteOnEvent* prevNote = findPreviousNoteInTrack(earliest, selectionSet);
                if (prevNote) {
                    int offset = prevNote->offEvent()->midiTime() - earliest->midiTime();
                    file->protocol()->startNewAction(tr("Snap Start to Previous End"));
                    foreach (MidiEvent* ev, sortedSelection) {
                        ev->setMidiTime(ev->midiTime() + offset);
                        NoteOnEvent* note = dynamic_cast<NoteOnEvent*>(ev);
                        if (note && note->offEvent()) {
                            note->offEvent()->setMidiTime(note->offEvent()->midiTime() + offset);
                        }
                    }
                    file->protocol()->endAction();
                }
            }
        }
    }
    // Handle Snap End to Next Start (Block-based)
    // Find the note with the furthest end tick (max offEvent time) to snap to the next note
    else if (selectedAction == snapEndAction) {
        if (!sortedSelection.isEmpty()) {
            NoteOnEvent* furthestEnd = nullptr;
            int maxEndTick = -1;
            foreach (MidiEvent* ev, sortedSelection) {
                NoteOnEvent* n = dynamic_cast<NoteOnEvent*>(ev);
                if (n && n->offEvent()) {
                    int endTick = n->offEvent()->midiTime();
                    if (endTick > maxEndTick) {
                        maxEndTick = endTick;
                        furthestEnd = n;
                    }
                }
            }
            if (furthestEnd) {
                std::set<MidiEvent*> selectionSet(sortedSelection.begin(), sortedSelection.end());
                NoteOnEvent* nextNote = findNextNoteInTrack(furthestEnd, selectionSet);
                if (nextNote) {
                    int offset = nextNote->midiTime() - furthestEnd->offEvent()->midiTime();
                    file->protocol()->startNewAction(tr("Snap End to Next Start"));
                    foreach (MidiEvent* ev, sortedSelection) {
                        ev->setMidiTime(ev->midiTime() + offset);
                        NoteOnEvent* note = dynamic_cast<NoteOnEvent*>(ev);
                        if (note && note->offEvent()) {
                            note->offEvent()->setMidiTime(note->offEvent()->midiTime() + offset);
                        }
                    }
                    file->protocol()->endAction();
                }
            }
        }
    }
    update();
}

NoteOnEvent* MatrixWidget::findPreviousNoteInTrack(NoteOnEvent* note, const std::set<MidiEvent*>& exclude)
{
    if (!note || !file) {
        return nullptr;
    }

    MidiTrack* track = note->track();
    int currentTick = note->midiTime();

    NoteOnEvent* lastNote = nullptr;
    int latestOffTick = -1;

    for (int channel = 0; channel < 16; channel++) {
        QMultiMap<int, MidiEvent*>* map = file->channelEvents(channel);
        // Look at all notes starting strictly before currentTick
        QMultiMap<int, MidiEvent*>::iterator it = map->lowerBound(currentTick);

        while (it != map->begin()) {
            --it;
            MidiEvent* event = it.value();

            if (event->track() == track) {
                NoteOnEvent* noteOn = dynamic_cast<NoteOnEvent*>(event);
                if (noteOn && noteOn != note && !exclude.count(noteOn)) {
                    int offTick = noteOn->offEvent()->midiTime();
                    if (offTick <= currentTick && offTick > latestOffTick) {
                        latestOffTick = offTick;
                        lastNote = noteOn;
                    }
                }
            }
        }
    }
    return lastNote;
}

NoteOnEvent* MatrixWidget::findNextNoteInTrack(NoteOnEvent* note, const std::set<MidiEvent*>& exclude)
{
    if (!note || !file) {
        return nullptr;
    }

    MidiTrack* track = note->track();
    int currentTick = note->offEvent()->midiTime();

    NoteOnEvent* nextNote = nullptr;
    int earliestOnTick = -1;

    for (int channel = 0; channel < 16; channel++) {
        QMultiMap<int, MidiEvent*>* map = file->channelEvents(channel);
        // Look at all notes starting at or after currentTick
        QMultiMap<int, MidiEvent*>::iterator it = map->lowerBound(currentTick);

        while (it != map->end()) {
            MidiEvent* event = it.value();

            if (event->track() == track) {
                NoteOnEvent* noteOn = dynamic_cast<NoteOnEvent*>(event);
                if (noteOn && noteOn != note && !exclude.count(noteOn)) {
                    int onTick = noteOn->midiTime();
                    if (earliestOnTick == -1 || onTick < earliestOnTick) {
                        earliestOnTick = onTick;
                        nextNote = noteOn;
                    }
                }
            }
            ++it;
        }
    }

    return nextNote;
}

void MatrixWidget::setColorsByChannel() {
    _colorsByChannels = true;
    update();
}

void MatrixWidget::setColorsByTracks() {
    _colorsByChannels = false;
    update();
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

void MatrixWidget::paintTimelineMarkers(QPainter *painter) {
    if (!file || !_hasVisibleMarkers) return;

    painter->setPen(_cachedBorderColor);
    painter->drawLine(lineNameWidth, timeHeight - 1, width(), timeHeight - 1);

    // Collect all active markers
    QList<MidiEvent*> markers;
    for (int ch = 0; ch < 19; ch++) {
        if (!ChannelVisibilityManager::instance().isChannelVisible(ch)) continue;
        
        QMultiMap<int, MidiEvent*> *events = file->channelEvents(ch);
        if (!events) continue;
        
        auto it = events->lowerBound(startTick);
        while (it != events->end() && it.key() <= endTick) {
            MidiEvent *ev = it.value();
            bool isMarker = false;
            
            if (dynamic_cast<ProgChangeEvent*>(ev)) isMarker = Appearance::showProgramChangeMarkers();
            else if (dynamic_cast<ControlChangeEvent*>(ev)) isMarker = Appearance::showControlChangeMarkers();
            else if (dynamic_cast<TextEvent*>(ev)) isMarker = Appearance::showTextEventMarkers();
            
            if (isMarker && ev->track() && !ev->track()->hidden()) {
                markers.append(ev);
            }
            it++;
        }
    }

    // Draw dashed lines AND labels
    painter->save();
    QFont font = Appearance::improveFont(painter->font());
    font.setPixelSize(10);
    font.setBold(true);
    painter->setFont(font);

    // Filtered markers list (to handle layering during drag)
    foreach(MidiEvent *ev, markers) {
        int x = xPosOfMs(msOfTick(ev->midiTime()));
        if (x < lineNameWidth) continue;

        // Determine if this marker is being interacted with
        bool isHovered = (ev == _lastHoverMarker && mouseInRect(MarkerArea));
        bool isDragged = (ev == _draggedMarker);
        bool isActive = isHovered || isDragged;

        // Visual properties
        QColor markerColor;
        if (Appearance::markerColorMode() == Appearance::ColorByChannel) {
            markerColor = *Appearance::channelColor(ev->channel());
        } else {
            markerColor = *ev->track()->color();
        }
        QString text = "";
        if (dynamic_cast<ProgChangeEvent*>(ev)) text = "PC";
        else if (dynamic_cast<ControlChangeEvent*>(ev)) text = "CC";
        else if (dynamic_cast<TextEvent*>(ev)) text = "Txt";

        // Layout
        int yMarker = timeHeight - _markerRowHeight + 8;
        int drawX = x;

        // Use marker color for the drag guide
        QPen markerDashPen(markerColor, 1, Qt::DashLine);
        painter->setPen(markerDashPen);
        
        int eventY = yPosOfLine(ev->line()) + (lineHeight() / 2);
        int stopY = qBound(timeHeight, eventY, height());
        // Guide line should be drawn BEHIND the marker icon
        if (Appearance::showMarkerGuideLines()) {
            painter->drawLine(drawX, timeHeight, drawX, stopY);
        }

        painter->setBrush(isActive ? markerColor.lighter(130) : markerColor);
        if (isDragged) {
            QColor dragColor = markerColor.lighter(130);
            dragColor.setAlpha(180);
            painter->setBrush(dragColor);
        }
        
        painter->setPen(isActive ? Qt::white : Qt::black);
        painter->drawRect(drawX, yMarker - 6, 24, 12);
        
        int luminance = (markerColor.red() * 299 + markerColor.green() * 587 + markerColor.blue() * 114) / 1000;
        QColor textColor = (luminance > 128 && !isActive) ? Qt::black : Qt::white;
        painter->setPen(textColor);
        painter->drawText(drawX + 2, yMarker + 4, text);
    }
    painter->restore();
}

MidiEvent *MatrixWidget::findTimelineMarkerNear(int x, int y) {
    if (!file || !mouseInRect(MarkerArea)) return nullptr;

    int threshold = 34;
    MidiEvent *bestMatch = nullptr;
    int closestDist = threshold;

    for (int ch = 0; ch < 19; ch++) {
        if (!ChannelVisibilityManager::instance().isChannelVisible(ch)) continue;
        
        QMultiMap<int, MidiEvent*> *events = file->channelEvents(ch);
        if (!events) continue;
        
        int mouseTick = file->tick(msOfXPos(x));
        int tickThreshold = file->tick(msOfXPos(x + threshold)) - mouseTick;
        
        auto it = events->lowerBound(mouseTick - tickThreshold);
        while (it != events->end() && it.key() <= mouseTick + tickThreshold) {
            MidiEvent *ev = it.value();
            bool isMarker = (dynamic_cast<ProgChangeEvent*>(ev) && Appearance::showProgramChangeMarkers()) ||
                            (dynamic_cast<ControlChangeEvent*>(ev) && Appearance::showControlChangeMarkers()) ||
                            (dynamic_cast<TextEvent*>(ev) && Appearance::showTextEventMarkers());
            
            if (isMarker && ev->track() && !ev->track()->hidden()) {
                int evX = xPosOfMs(msOfTick(ev->midiTime()));
                
                // Prioritize clicks actually inside the marker's visual label box [evX, evX + 24]
                int dist;
                if (x >= evX && x <= evX + 24) {
                    dist = 0; // Perfect hit
                } else {
                    dist = threshold + qAbs(evX - x); // Outside - less likely to match than an direct box hit
                }

                if (dist <= closestDist) {
                    closestDist = dist;
                    bestMatch = ev;
                }
            }
            it++;
        }
    }
    return bestMatch;
}

void MatrixWidget::paintRecordingPreview(QPainter *painter) {
    if (!file || !MidiInput::recording() || _noteStartTicks.isEmpty()) return;

    painter->save();
    painter->setClipping(true);
    painter->setClipRect(lineNameWidth, timeHeight, width() - lineNameWidth, height() - timeHeight);

    int currentTick = file->cursorTick();
    
    // Use a more vibrant color for ghost notes during recording so they follow the red cursor clearly
    int ch = MidiOutput::standardChannel();
    QColor ghostColor;
    if (ch >= 0 && ch < 16 && file->channel(ch)) {
        ghostColor = *file->channel(ch)->color();
    } else {
        ghostColor = QColor(255, 60, 60); // Vibrant red fallback
    }
    ghostColor.setAlpha(160); // Slightly more opaque for better visibility

    for (auto it = _noteStartTicks.constBegin(); it != _noteStartTicks.constEnd(); ++it) {
        int note = it.key();
        int startTick = it.value();
        int line = 127 - note;

        // Skip if outside vertical view
        if (line < startLineY || line > endLineY) continue;

        int x = xPosOfMs(msOfTick(startTick));
        
        // Sync endX with the actual visual playback cursor position
        int currentMs = (Appearance::smoothPlaybackScrolling()) ? _lastRenderedMs : MidiPlayer::timeMs();
        int endX = xPosOfMs(qMax((int)msOfTick(startTick), currentMs));
        
        // Skip if outside horizontal view
        if (endX < lineNameWidth || x > width()) continue;

        int y = yPosOfLine(line);
        int h = lineHeight();
        int w = qMax(endX - x, 2); // Minimum 2px width for visibility

        painter->setBrush(ghostColor);
        painter->setPen(ghostColor.darker());
        painter->drawRect(x, y, w, h);
    }

    painter->restore();
}
