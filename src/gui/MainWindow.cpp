/*
 * MidiEditor
 * Copyright (C) 2010  Markus Schwenk
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.+
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "MainWindow.h"

#include <QAction>
#include <QActionGroup>
#include <QComboBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QIcon>
#include <QInputDialog>
#include <QLabel>
#include <QList>
#include <QMap>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QMultiMap>
#include <QProcess>
#include <QScrollArea>
#include <QSettings>
#include <QSplitter>
#include <QStringList>
#include <QTabWidget>
#include <QTextEdit>
#include <QTextStream>
#include <QToolBar>
#include <QToolButton>
#include <QDesktopServices>

#include "Appearance.h"
#include "AboutDialog.h"
#include "ChannelListWidget.h"
#include "ClickButton.h"
#include "EventWidget.h"
#include "FileLengthDialog.h"
#include "InstrumentChooser.h"
#include "MatrixWidget.h"
#include "MiscWidget.h"
#include "NToleQuantizationDialog.h"
#include "ProtocolWidget.h"
#include "RecordDialog.h"
#include "SelectionNavigator.h"
#include "SettingsDialog.h"
#include "TrackListWidget.h"
#include "TransposeDialog.h"
#include "TweakTarget.h"

#include "../tool/DeleteOverlapsTool.h"
#include "DeleteOverlapsDialog.h"
#include "../tool/EraserTool.h"
#include "../tool/EventMoveTool.h"
#include "../tool/EventTool.h"
#include "../tool/GlueTool.h"
#include "../tool/MeasureTool.h"
#include "../tool/NewNoteTool.h"
#include "../tool/ScissorsTool.h"
#include "../tool/SelectTool.h"
#include "../tool/Selection.h"
#include "../tool/SizeChangeTool.h"
#include "../tool/StandardTool.h"
#include "../tool/TempoTool.h"
#include "../tool/TimeSignatureTool.h"
#include "../tool/Tool.h"
#include "../tool/ToolButton.h"

#include "../Terminal.h"
#include "../protocol/Protocol.h"

#include "../MidiEvent/MidiEvent.h"
#include "../MidiEvent/NoteOnEvent.h"
#include "../MidiEvent/OffEvent.h"
#include "../MidiEvent/OnEvent.h"
#include "../MidiEvent/TextEvent.h"
#include "../MidiEvent/TimeSignatureEvent.h"
#include "../midi/Metronome.h"
#include "../midi/MidiChannel.h"
#include "../midi/MidiFile.h"
#include "../midi/MidiInput.h"
#include "../midi/MidiOutput.h"
#include "../midi/MidiPlayer.h"
#include "../midi/MidiTrack.h"
#include "../midi/PlayerThread.h"

#include "CompleteMidiSetupDialog.h"
#include <QtCore/qmath.h>

MainWindow::MainWindow(QString initFile)
    : QMainWindow()
    , _initFile(initFile) {
    file = 0;
    _settings = new QSettings(QString("MidiEditor"), QString("NONE"));

    _moveSelectedEventsToChannelMenu = 0;
    _moveSelectedEventsToTrackMenu = 0;
    _toolbarWidget = nullptr;

    Appearance::init(_settings);

    bool alternativeStop = _settings->value("alt_stop", false).toBool();
    MidiOutput::isAlternativePlayer = alternativeStop;
    bool ticksOK;
    int ticksPerQuarter = _settings->value("ticks_per_quarter", 192).toInt(&ticksOK);
    MidiFile::defaultTimePerQuarter = ticksPerQuarter;
    bool magnet = _settings->value("magnet", false).toBool();
    EventTool::enableMagnet(magnet);

    MidiInput::setThruEnabled(_settings->value("thru", false).toBool());
    Metronome::setEnabled(_settings->value("metronome", false).toBool());
    bool loudnessOk;
    Metronome::setLoudness(_settings->value("metronome_loudness", 100).toInt(&loudnessOk));

    _quantizationGrid = _settings->value("quantization", 3).toInt();

    // metronome
    connect(MidiPlayer::playerThread(),
            SIGNAL(measureChanged(int, int)), Metronome::instance(), SLOT(measureUpdate(int, int)));
    connect(MidiPlayer::playerThread(),
            SIGNAL(measureUpdate(int, int)), Metronome::instance(), SLOT(measureUpdate(int, int)));
    connect(MidiPlayer::playerThread(),
            SIGNAL(meterChanged(int, int)), Metronome::instance(), SLOT(meterChanged(int, int)));
    connect(MidiPlayer::playerThread(),
            SIGNAL(playerStopped()), Metronome::instance(), SLOT(playbackStopped()));
    connect(MidiPlayer::playerThread(),
            SIGNAL(playerStarted()), Metronome::instance(), SLOT(playbackStarted()));

    startDirectory = QDir::homePath();

    if (_settings->value("open_path").toString() != "") {
        startDirectory = _settings->value("open_path").toString();
    } else {
        _settings->setValue("open_path", startDirectory);
    }

    // read recent paths
    _recentFilePaths = _settings->value("recent_file_list").toStringList();

    EditorTool::setMainWindow(this);

    setWindowTitle(QApplication::applicationName() + " " + QApplication::applicationVersion());
    // Note: setWindowIcon doesn't use QAction, so we keep the direct approach
    setWindowIcon(Appearance::adjustIconForDarkMode(":/run_environment/graphics/icon.png"));

    QWidget* central = new QWidget(this);
    QGridLayout* centralLayout = new QGridLayout(central);
    centralLayout->setContentsMargins(3, 3, 3, 5);

    // there is a vertical split
    QSplitter* mainSplitter = new QSplitter(Qt::Horizontal, central);
    //mainSplitter->setHandleWidth(0);

    // The left side
    QSplitter* leftSplitter = new QSplitter(Qt::Vertical, mainSplitter);
    leftSplitter->setHandleWidth(0);
    mainSplitter->addWidget(leftSplitter);
    leftSplitter->setContentsMargins(0, 0, 0, 0);

    // The right side
    QSplitter* rightSplitter = new QSplitter(Qt::Vertical, mainSplitter);
    //rightSplitter->setHandleWidth(0);
    mainSplitter->addWidget(rightSplitter);

    // Set the sizes of mainSplitter
    mainSplitter->setStretchFactor(0, 1);
    mainSplitter->setStretchFactor(1, 0);
    mainSplitter->setContentsMargins(0, 0, 0, 0);

    // the channelWidget and the trackWidget are tabbed
    QTabWidget* upperTabWidget = new QTabWidget(rightSplitter);
    rightSplitter->addWidget(upperTabWidget);
    rightSplitter->setContentsMargins(0, 0, 0, 0);

    // protocolList and EventWidget are tabbed
    lowerTabWidget = new QTabWidget(rightSplitter);
    rightSplitter->addWidget(lowerTabWidget);

    // MatrixArea
    QWidget* matrixArea = new QWidget(leftSplitter);
    leftSplitter->addWidget(matrixArea);
    matrixArea->setContentsMargins(0, 0, 0, 0);
    mw_matrixWidget = new MatrixWidget(matrixArea);
    vert = new QScrollBar(Qt::Vertical, matrixArea);
    QGridLayout* matrixAreaLayout = new QGridLayout(matrixArea);
    matrixAreaLayout->setHorizontalSpacing(6);
    QWidget* placeholder0 = new QWidget(matrixArea);
    placeholder0->setFixedHeight(50);
    matrixAreaLayout->setContentsMargins(0, 0, 0, 0);
    matrixAreaLayout->addWidget(mw_matrixWidget, 0, 0, 2, 1);
    matrixAreaLayout->addWidget(placeholder0, 0, 1, 1, 1);
    matrixAreaLayout->addWidget(vert, 1, 1, 1, 1);
    matrixAreaLayout->setColumnStretch(0, 1);
    matrixArea->setLayout(matrixAreaLayout);

    bool screenLocked = _settings->value("screen_locked", false).toBool();
    mw_matrixWidget->setScreenLocked(screenLocked);
    int div = _settings->value("div", 2).toInt();
    mw_matrixWidget->setDiv(div);

    // VelocityArea
    QWidget* velocityArea = new QWidget(leftSplitter);
    velocityArea->setContentsMargins(0, 0, 0, 0);
    leftSplitter->addWidget(velocityArea);
    hori = new QScrollBar(Qt::Horizontal, velocityArea);
    hori->setSingleStep(500);
    hori->setPageStep(5000);
    QGridLayout* velocityAreaLayout = new QGridLayout(velocityArea);
    velocityAreaLayout->setContentsMargins(0, 0, 0, 0);
    velocityAreaLayout->setHorizontalSpacing(6);
    _miscWidgetControl = new QWidget(velocityArea);
    _miscWidgetControl->setFixedWidth(110 - velocityAreaLayout->horizontalSpacing());

    velocityAreaLayout->addWidget(_miscWidgetControl, 0, 0, 1, 1);
    // there is a Scrollbar on the right side of the velocityWidget doing
    // nothing but making the VelocityWidget as big as the matrixWidget
    QScrollBar* scrollNothing = new QScrollBar(Qt::Vertical, velocityArea);
    scrollNothing->setMinimum(0);
    scrollNothing->setMaximum(0);
    velocityAreaLayout->addWidget(scrollNothing, 0, 2, 1, 1);
    velocityAreaLayout->addWidget(hori, 1, 1, 1, 1);
    velocityAreaLayout->setRowStretch(0, 1);
    velocityArea->setLayout(velocityAreaLayout);

    _miscWidget = new MiscWidget(mw_matrixWidget, velocityArea);
    _miscWidget->setContentsMargins(0, 0, 0, 0);
    velocityAreaLayout->addWidget(_miscWidget, 0, 1, 1, 1);

    // controls for velocity widget
    _miscControlLayout = new QGridLayout(_miscWidgetControl);
    _miscControlLayout->setHorizontalSpacing(0);
    //_miscWidgetControl->setContentsMargins(0,0,0,0);
    //_miscControlLayout->setContentsMargins(0,0,0,0);
    _miscWidgetControl->setLayout(_miscControlLayout);
    _miscMode = new QComboBox(_miscWidgetControl);
    for (int i = 0; i < MiscModeEnd; i++) {
        _miscMode->addItem(MiscWidget::modeToString(i));
    }
    _miscMode->view()->setMinimumWidth(_miscMode->minimumSizeHint().width());
    //_miscControlLayout->addWidget(new QLabel("Mode:", _miscWidgetControl), 0, 0, 1, 3);
    _miscControlLayout->addWidget(_miscMode, 1, 0, 1, 3);
    connect(_miscMode, SIGNAL(currentIndexChanged(int)), this, SLOT(changeMiscMode(int)));

    //_miscControlLayout->addWidget(new QLabel("Control:", _miscWidgetControl), 2, 0, 1, 3);
    _miscController = new QComboBox(_miscWidgetControl);
    for (int i = 0; i < 128; i++) {
        _miscController->addItem(MidiFile::controlChangeName(i));
    }
    _miscController->view()->setMinimumWidth(_miscController->minimumSizeHint().width());
    _miscControlLayout->addWidget(_miscController, 3, 0, 1, 3);
    connect(_miscController, SIGNAL(currentIndexChanged(int)), _miscWidget, SLOT(setControl(int)));

    //_miscControlLayout->addWidget(new QLabel("Channel:", _miscWidgetControl), 4, 0, 1, 3);
    _miscChannel = new QComboBox(_miscWidgetControl);
    for (int i = 0; i < 15; i++) {
        _miscChannel->addItem("Channel " + QString::number(i));
    }
    _miscChannel->view()->setMinimumWidth(_miscChannel->minimumSizeHint().width());
    _miscControlLayout->addWidget(_miscChannel, 5, 0, 1, 3);
    connect(_miscChannel, SIGNAL(currentIndexChanged(int)), _miscWidget, SLOT(setChannel(int)));
    _miscControlLayout->setRowStretch(6, 1);
    _miscMode->setCurrentIndex(0);
    _miscChannel->setEnabled(false);
    _miscController->setEnabled(false);

    setSingleMode = new QAction("Single mode", this);
    Appearance::setActionIcon(setSingleMode, ":/run_environment/graphics/tool/misc_single.png");
    setSingleMode->setCheckable(true);
    setFreehandMode = new QAction("Free-hand mode", this);
    Appearance::setActionIcon(setFreehandMode, ":/run_environment/graphics/tool/misc_freehand.png");
    setFreehandMode->setCheckable(true);
    setLineMode = new QAction("Line mode", this);
    Appearance::setActionIcon(setLineMode, ":/run_environment/graphics/tool/misc_line.png");
    setLineMode->setCheckable(true);

    QActionGroup* group = new QActionGroup(this);
    group->setExclusive(true);
    group->addAction(setSingleMode);
    group->addAction(setFreehandMode);
    group->addAction(setLineMode);
    setSingleMode->setChecked(true);
    connect(group, SIGNAL(triggered(QAction*)), this, SLOT(selectModeChanged(QAction*)));

    QToolButton* btnSingle = new QToolButton(_miscWidgetControl);
    btnSingle->setDefaultAction(setSingleMode);
    QToolButton* btnHand = new QToolButton(_miscWidgetControl);
    btnHand->setDefaultAction(setFreehandMode);
    QToolButton* btnLine = new QToolButton(_miscWidgetControl);
    btnLine->setDefaultAction(setLineMode);

    _miscControlLayout->addWidget(btnSingle, 9, 0, 1, 1);
    _miscControlLayout->addWidget(btnHand, 9, 1, 1, 1);
    _miscControlLayout->addWidget(btnLine, 9, 2, 1, 1);

    // Set the sizes of leftSplitter
    leftSplitter->setStretchFactor(0, 8);
    leftSplitter->setStretchFactor(1, 1);

    // Track
    QWidget* tracks = new QWidget(upperTabWidget);
    QGridLayout* tracksLayout = new QGridLayout(tracks);
    tracks->setLayout(tracksLayout);
    QToolBar* tracksTB = new QToolBar(tracks);
    tracksTB->setIconSize(QSize(20, 20));
    tracksLayout->addWidget(tracksTB, 0, 0, 1, 1);

    QAction* newTrack = new QAction(tr("Add track"), this);
    Appearance::setActionIcon(newTrack, ":/run_environment/graphics/tool/add.png");
    connect(newTrack, SIGNAL(triggered()), this,
            SLOT(addTrack()));
    tracksTB->addAction(newTrack);

    tracksTB->addSeparator();

    _allTracksAudible = new QAction(tr("All tracks audible"), this);
    Appearance::setActionIcon(_allTracksAudible, ":/run_environment/graphics/tool/all_audible.png");
    connect(_allTracksAudible, SIGNAL(triggered()), this,
            SLOT(unmuteAllTracks()));
    tracksTB->addAction(_allTracksAudible);

    _allTracksMute = new QAction(tr("Mute all tracks"), this);
    Appearance::setActionIcon(_allTracksMute, ":/run_environment/graphics/tool/all_mute.png");
    connect(_allTracksMute, SIGNAL(triggered()), this,
            SLOT(muteAllTracks()));
    tracksTB->addAction(_allTracksMute);

    tracksTB->addSeparator();

    _allTracksVisible = new QAction(tr("Show all tracks"), this);
    Appearance::setActionIcon(_allTracksVisible, ":/run_environment/graphics/tool/all_visible.png");
    connect(_allTracksVisible, SIGNAL(triggered()), this,
            SLOT(allTracksVisible()));
    tracksTB->addAction(_allTracksVisible);

    _allTracksInvisible = new QAction(tr("Hide all tracks"), this);
    Appearance::setActionIcon(_allTracksInvisible, ":/run_environment/graphics/tool/all_invisible.png");
    connect(_allTracksInvisible, SIGNAL(triggered()), this,
            SLOT(allTracksInvisible()));
    tracksTB->addAction(_allTracksInvisible);

    _trackWidget = new TrackListWidget(tracks);
    connect(_trackWidget, SIGNAL(trackRenameClicked(int)), this, SLOT(renameTrack(int)), Qt::QueuedConnection);
    connect(_trackWidget, SIGNAL(trackRemoveClicked(int)), this, SLOT(removeTrack(int)), Qt::QueuedConnection);
    connect(_trackWidget, SIGNAL(trackClicked(MidiTrack*)), this, SLOT(editTrackAndChannel(MidiTrack*)), Qt::QueuedConnection);

    tracksLayout->addWidget(_trackWidget, 1, 0, 1, 1);
    upperTabWidget->addTab(tracks, tr("Tracks"));

    // Channels
    QWidget* channels = new QWidget(upperTabWidget);
    QGridLayout* channelsLayout = new QGridLayout(channels);
    channels->setLayout(channelsLayout);
    QToolBar* channelsTB = new QToolBar(channels);
    channelsTB->setIconSize(QSize(20, 20));
    channelsLayout->addWidget(channelsTB, 0, 0, 1, 1);

    _allChannelsAudible = new QAction(tr("All channels audible"), this);
    Appearance::setActionIcon(_allChannelsAudible, ":/run_environment/graphics/tool/all_audible.png");
    connect(_allChannelsAudible, SIGNAL(triggered()), this,
            SLOT(unmuteAllChannels()));
    channelsTB->addAction(_allChannelsAudible);

    _allChannelsMute = new QAction(tr("Mute all channels"), this);
    Appearance::setActionIcon(_allChannelsMute, ":/run_environment/graphics/tool/all_mute.png");
    connect(_allChannelsMute, SIGNAL(triggered()), this,
            SLOT(muteAllChannels()));
    channelsTB->addAction(_allChannelsMute);

    channelsTB->addSeparator();

    _allChannelsVisible = new QAction(tr("Show all channels"), this);
    Appearance::setActionIcon(_allChannelsVisible, ":/run_environment/graphics/tool/all_visible.png");
    connect(_allChannelsVisible, SIGNAL(triggered()), this,
            SLOT(allChannelsVisible()));
    channelsTB->addAction(_allChannelsVisible);

    _allChannelsInvisible = new QAction(tr("Hide all channels"), this);
    Appearance::setActionIcon(_allChannelsInvisible, ":/run_environment/graphics/tool/all_invisible.png");
    connect(_allChannelsInvisible, SIGNAL(triggered()), this,
            SLOT(allChannelsInvisible()));
    channelsTB->addAction(_allChannelsInvisible);

    channelWidget = new ChannelListWidget(channels);
    connect(channelWidget, SIGNAL(channelStateChanged()), this, SLOT(updateChannelMenu()), Qt::QueuedConnection);
    connect(channelWidget, SIGNAL(selectInstrumentClicked(int)), this, SLOT(setInstrumentForChannel(int)), Qt::QueuedConnection);
    channelsLayout->addWidget(channelWidget, 1, 0, 1, 1);
    upperTabWidget->addTab(channels, tr("Channels"));

    // terminal
    Terminal::initTerminal(_settings->value("start_cmd", "").toString(),
                           _settings->value("in_port", "").toString(),
                           _settings->value("out_port", "").toString());
    //upperTabWidget->addTab(Terminal::terminal()->console(), "Terminal");

    // Protocollist
    protocolWidget = new ProtocolWidget(lowerTabWidget);
    lowerTabWidget->addTab(protocolWidget, tr("Protocol"));

    // EventWidget
    _eventWidget = new EventWidget(lowerTabWidget);
    Selection::_eventWidget = _eventWidget;
    lowerTabWidget->addTab(_eventWidget, tr("Event"));
    MidiEvent::setEventWidget(_eventWidget);

    // below add two rows for choosing track/channel new events shall be assigned to
    QWidget* chooser = new QWidget(rightSplitter);
    chooser->setMinimumWidth(350);
    rightSplitter->addWidget(chooser);
    QGridLayout* chooserLayout = new QGridLayout(chooser);
    QLabel* trackchannelLabel = new QLabel(tr("Add new events to ..."));
    chooserLayout->addWidget(trackchannelLabel, 0, 0, 1, 2);
    QLabel* channelLabel = new QLabel(tr("Channel: "), chooser);
    chooserLayout->addWidget(channelLabel, 2, 0, 1, 1);
    _chooseEditChannel = new QComboBox(chooser);
    for (int i = 0; i < 16; i++) {
        if (i == 9) _chooseEditChannel->addItem(tr("Percussion channel"));
        else _chooseEditChannel->addItem(tr("Channel ") + QString::number(i)); // TODO: Display channel instrument | UDP: But **file** is *nullptr*
    }
    connect(_chooseEditChannel, SIGNAL(activated(int)), this, SLOT(editChannel(int)));

    chooserLayout->addWidget(_chooseEditChannel, 2, 1, 1, 1);
    QLabel* trackLabel = new QLabel(tr("Track: "), chooser);
    chooserLayout->addWidget(trackLabel, 1, 0, 1, 1);
    _chooseEditTrack = new QComboBox(chooser);
    chooserLayout->addWidget(_chooseEditTrack, 1, 1, 1, 1);
    connect(_chooseEditTrack, SIGNAL(activated(int)), this, SLOT(editTrack(int)));
    chooserLayout->setColumnStretch(1, 1);
    // connect Scrollbars and Widgets
    connect(vert, SIGNAL(valueChanged(int)), mw_matrixWidget,
            SLOT(scrollYChanged(int)));
    connect(hori, SIGNAL(valueChanged(int)), mw_matrixWidget,
            SLOT(scrollXChanged(int)));

    connect(channelWidget, SIGNAL(channelStateChanged()), mw_matrixWidget,
            SLOT(repaint()));
    connect(mw_matrixWidget, SIGNAL(sizeChanged(int, int, int, int)), this,
            SLOT(matrixSizeChanged(int, int, int, int)));

    connect(mw_matrixWidget, SIGNAL(scrollChanged(int, int, int, int)), this,
            SLOT(scrollPositionsChanged(int, int, int, int)));

    setCentralWidget(central);

    QWidget* buttons = setupActions(central);

    rightSplitter->setStretchFactor(0, 5);
    rightSplitter->setStretchFactor(1, 5);

    // Add the Widgets to the central Layout
    centralLayout->setSpacing(0);
    centralLayout->addWidget(buttons, 0, 0);
    centralLayout->addWidget(mainSplitter, 1, 0);
    centralLayout->setRowStretch(1, 1);
    central->setLayout(centralLayout);

    if (_settings->value("colors_from_channel", false).toBool()) {
        colorsByChannel();
    } else {
        colorsByTrack();
    }
    copiedEventsChanged();
    setAcceptDrops(true);

    currentTweakTarget = new TimeTweakTarget(this);
    selectionNavigator = new SelectionNavigator(this);

    QTimer::singleShot(200, this, SLOT(loadInitFile()));
}

void MainWindow::loadInitFile() {
    if (_initFile != "")
        loadFile(_initFile);
    else
        newFile();
}

void MainWindow::dropEvent(QDropEvent* ev) {
    QList<QUrl> urls = ev->mimeData()->urls();
    foreach (QUrl url, urls) {
        QString newFile = url.toLocalFile();
        if (!newFile.isEmpty()) {
            loadFile(newFile);
            break;
        }
    }
}

void MainWindow::dragEnterEvent(QDragEnterEvent* ev) {
    ev->accept();
}

void MainWindow::scrollPositionsChanged(int startMs, int maxMs, int startLine,
                                        int maxLine) {
    hori->setMaximum(maxMs);
    hori->setValue(startMs);
    vert->setMaximum(maxLine);
    vert->setValue(startLine);
}

void MainWindow::setFile(MidiFile* newFile) {

    // Store reference to old file for cleanup
    MidiFile* oldFile = this->file;

    EventTool::clearSelection();
    Selection::setFile(newFile);
    Metronome::instance()->setFile(newFile);
    protocolWidget->setFile(newFile);
    channelWidget->setFile(newFile);
    _trackWidget->setFile(newFile);
    eventWidget()->setFile(newFile);

    Tool::setFile(newFile);
    this->file = newFile;
    connect(newFile, SIGNAL(trackChanged()), this, SLOT(updateTrackMenu()));
    setWindowTitle(QApplication::applicationName() + " - " + newFile->path() + "[*]");
    connect(newFile, SIGNAL(cursorPositionChanged()), channelWidget, SLOT(update()));
    connect(newFile, SIGNAL(recalcWidgetSize()), mw_matrixWidget, SLOT(calcSizes()));
    connect(newFile->protocol(), SIGNAL(actionFinished()), this, SLOT(markEdited()));
    connect(newFile->protocol(), SIGNAL(actionFinished()), eventWidget(), SLOT(reload()));
    connect(newFile->protocol(), SIGNAL(actionFinished()), this, SLOT(checkEnableActionsForSelection()));
    mw_matrixWidget->setFile(newFile);
    updateChannelMenu();
    updateTrackMenu();
    mw_matrixWidget->update();
    _miscWidget->update();
    checkEnableActionsForSelection();

    // Clean up the old file after everything has been switched to the new file
    // This ensures all widgets have switched to the new file before cleanup
    if (oldFile) {
        delete oldFile;
    }
}

MidiFile* MainWindow::getFile() {
    return file;
}

MatrixWidget* MainWindow::matrixWidget() {
    return mw_matrixWidget;
}

void MainWindow::matrixSizeChanged(int maxScrollTime, int maxScrollLine,
                                   int vX, int vY) {
    vert->setMaximum(maxScrollLine);
    hori->setMaximum(maxScrollTime);
    vert->setValue(vY);
    hori->setValue(vX);
    mw_matrixWidget->repaint();
}

void MainWindow::playStop() {
    if (MidiPlayer::isPlaying()) {
        stop();
    } else {
        play();
    }
}

void MainWindow::play() {
    if (!MidiOutput::isConnected()) {
        CompleteMidiSetupDialog* d = new CompleteMidiSetupDialog(this, false, true);
        d->setModal(true);
        d->exec();
        return;
    }
    if (file && !MidiInput::recording() && !MidiPlayer::isPlaying()) {
        mw_matrixWidget->timeMsChanged(file->msOfTick(file->cursorTick()), true);

        _miscWidget->setEnabled(false);
        channelWidget->setEnabled(false);
        protocolWidget->setEnabled(false);
        mw_matrixWidget->setEnabled(false);
        _trackWidget->setEnabled(false);
        eventWidget()->setEnabled(false);

        MidiPlayer::play(file);
        connect(MidiPlayer::playerThread(),
                SIGNAL(playerStopped()), this, SLOT(stop()));

#ifdef __WINDOWS_MM__
        connect(MidiPlayer::playerThread(),
                SIGNAL(timeMsChanged(int)), mw_matrixWidget, SLOT(timeMsChanged(int)));
#endif
    }
}

void MainWindow::record() {

    if (!MidiOutput::isConnected() || !MidiInput::isConnected()) {
        CompleteMidiSetupDialog* d = new CompleteMidiSetupDialog(this, !MidiInput::isConnected(), !MidiOutput::isConnected());
        d->setModal(true);
        d->exec();
        return;
    }

    if (!file) {
        newFile();
    }

    if (!MidiInput::recording() && !MidiPlayer::isPlaying()) {
        // play current file
        if (file) {

            if (file->pauseTick() >= 0) {
                file->setCursorTick(file->pauseTick());
                file->setPauseTick(-1);
            }

            mw_matrixWidget->timeMsChanged(file->msOfTick(file->cursorTick()), true);

            _miscWidget->setEnabled(false);
            channelWidget->setEnabled(false);
            protocolWidget->setEnabled(false);
            mw_matrixWidget->setEnabled(false);
            _trackWidget->setEnabled(false);
            eventWidget()->setEnabled(false);
            MidiPlayer::play(file);
            MidiInput::startInput();
            connect(MidiPlayer::playerThread(),
                    SIGNAL(playerStopped()), this, SLOT(stop()));
#ifdef __WINDOWS_MM__
            connect(MidiPlayer::playerThread(),
                    SIGNAL(timeMsChanged(int)), mw_matrixWidget, SLOT(timeMsChanged(int)));
#endif
        }
    }
}

void MainWindow::pause() {
    if (file) {
        if (MidiPlayer::isPlaying()) {
            file->setPauseTick(file->tick(MidiPlayer::timeMs()));
            stop(false, false, false);
        }
    }
}

void MainWindow::stop(bool autoConfirmRecord, bool addEvents, bool resetPause) {

    if (!file) {
        return;
    }

    disconnect(MidiPlayer::playerThread(),
               SIGNAL(playerStopped()), this, SLOT(stop()));

    if (resetPause) {
        file->setPauseTick(-1);
        mw_matrixWidget->update();
    }
    if (!MidiInput::recording() && MidiPlayer::isPlaying()) {
        MidiPlayer::stop();
        _miscWidget->setEnabled(true);
        channelWidget->setEnabled(true);
        _trackWidget->setEnabled(true);
        protocolWidget->setEnabled(true);
        mw_matrixWidget->setEnabled(true);
        eventWidget()->setEnabled(true);
        mw_matrixWidget->timeMsChanged(MidiPlayer::timeMs(), true);
        _trackWidget->setEnabled(true);
        panic();
    }

    MidiTrack* track = file->track(NewNoteTool::editTrack());
    if (!track) {
        return;
    }

    if (MidiInput::recording()) {
        MidiPlayer::stop();
        panic();
        _miscWidget->setEnabled(true);
        channelWidget->setEnabled(true);
        protocolWidget->setEnabled(true);
        mw_matrixWidget->setEnabled(true);
        _trackWidget->setEnabled(true);
        eventWidget()->setEnabled(true);
        QMultiMap<int, MidiEvent*> events = MidiInput::endInput(track);

        if (events.isEmpty() && !autoConfirmRecord) {
            QMessageBox::information(this, tr("Information"), tr("No events recorded."));
        } else {
            RecordDialog* dialog = new RecordDialog(file, events, _settings, this);
            dialog->setModal(true);
            if (!autoConfirmRecord) {
                dialog->show();
            } else {
                if (addEvents) {
                    dialog->enter();
                }
            }
        }
    }
}

void MainWindow::forward() {
    if (!file)
        return;

    QList<TimeSignatureEvent*>* eventlist = new QList<TimeSignatureEvent*>;
    int ticksleft;
    int oldTick = file->cursorTick();
    if (file->pauseTick() >= 0) {
        oldTick = file->pauseTick();
    }
    if (MidiPlayer::isPlaying() && !MidiInput::recording()) {
        oldTick = file->tick(MidiPlayer::timeMs());
        stop(true);
    }
    file->measure(oldTick, oldTick, &eventlist, &ticksleft);

    int newTick = oldTick - ticksleft + eventlist->last()->ticksPerMeasure();
    file->setPauseTick(-1);
    if (newTick <= file->endTick()) {
        file->setCursorTick(newTick);
        mw_matrixWidget->timeMsChanged(file->msOfTick(newTick), true);
    }
    mw_matrixWidget->update();
}

void MainWindow::back() {
    if (!file)
        return;

    QList<TimeSignatureEvent*>* eventlist = new QList<TimeSignatureEvent*>;
    int ticksleft;
    int oldTick = file->cursorTick();
    if (file->pauseTick() >= 0) {
        oldTick = file->pauseTick();
    }
    if (MidiPlayer::isPlaying() && !MidiInput::recording()) {
        oldTick = file->tick(MidiPlayer::timeMs());
        stop(true);
    }
    file->measure(oldTick, oldTick, &eventlist, &ticksleft);
    int newTick = oldTick;
    if (ticksleft > 0) {
        newTick -= ticksleft;
    } else {
        newTick -= eventlist->last()->ticksPerMeasure();
    }
    file->measure(newTick, newTick, &eventlist, &ticksleft);
    if (ticksleft > 0) {
        newTick -= ticksleft;
    }
    file->setPauseTick(-1);
    if (newTick >= 0) {
        file->setCursorTick(newTick);
        mw_matrixWidget->timeMsChanged(file->msOfTick(newTick), true);
    }
    mw_matrixWidget->update();
}

void MainWindow::backToBegin() {
    if (!file)
        return;

    file->setPauseTick(0);
    file->setCursorTick(0);

    mw_matrixWidget->update();
}

void MainWindow::forwardMarker() {
    if (!file)
        return;

    int oldTick = file->cursorTick();
    if (file->pauseTick() >= 0) {
        oldTick = file->pauseTick();
    }
    if (MidiPlayer::isPlaying() && !MidiInput::recording()) {
        oldTick = file->tick(MidiPlayer::timeMs());
        stop(true);
    }

    int newTick = -1;

    foreach (MidiEvent* event, file->channel(16)->eventMap()->values()) {
        int eventTick = event->midiTime();
        if (eventTick <= oldTick) continue;
        TextEvent* textEvent = dynamic_cast<TextEvent*>(event);

        if (textEvent && textEvent->type() == TextEvent::MARKER) {
            newTick = eventTick;
            break;
        }
    }

    if (newTick < 0) return;
    file->setPauseTick(newTick);
    file->setCursorTick(newTick);
    mw_matrixWidget->timeMsChanged(file->msOfTick(newTick), true);
    mw_matrixWidget->update();
}

void MainWindow::backMarker() {
    if (!file)
        return;

    int oldTick = file->cursorTick();
    if (file->pauseTick() >= 0) {
        oldTick = file->pauseTick();
    }
    if (MidiPlayer::isPlaying() && !MidiInput::recording()) {
        oldTick = file->tick(MidiPlayer::timeMs());
        stop(true);
    }

    int newTick = 0;
    QList<MidiEvent*> events = file->channel(16)->eventMap()->values();

    for (int eventNumber = events.size() - 1; eventNumber >= 0; eventNumber--) {
        MidiEvent* event = events.at(eventNumber);
        int eventTick = event->midiTime();
        if (eventTick >= oldTick) continue;
        TextEvent* textEvent = dynamic_cast<TextEvent*>(event);

        if (textEvent && textEvent->type() == TextEvent::MARKER) {
            newTick = eventTick;
            break;
        }
    }

    file->setPauseTick(newTick);
    file->setCursorTick(newTick);
    mw_matrixWidget->timeMsChanged(file->msOfTick(newTick), true);
    mw_matrixWidget->update();
}

void MainWindow::save() {
    if (!file)
        return;

    if (QFile(file->path()).exists()) {

        bool printMuteWarning = false;

        for (int i = 0; i < 16; i++) {
            MidiChannel* ch = file->channel(i);
            if (ch->mute()) {
                printMuteWarning = true;
            }
        }
        foreach (MidiTrack* track, *(file->tracks())) {
            if (track->muted()) {
                printMuteWarning = true;
            }
        }

        if (printMuteWarning) {
            QMessageBox::information(this, tr("Channels/Tracks mute"),
                                     tr("One or more channels/tracks are not audible. They will be audible in the saved file."),
                                     tr("Save file"), 0, 0);
        }

        if (!file->save(file->path())) {
            QMessageBox::warning(this, tr("Error"), QString(tr("The file could not be saved. Please make sure that the destination directory exists and that you have the correct access rights to write into this directory.")));
        } else {
            setWindowModified(false);
        }
    } else {
        saveas();
    }
}

void MainWindow::saveas() {

    if (!file)
        return;

    QString oldPath = file->path();
    QFile* f = new QFile(oldPath);
    QString dir = startDirectory;
    if (f->exists()) {
        QFileInfo(*f).dir().path();
    }
    QString newPath = QFileDialog::getSaveFileName(this, tr("Save file as..."),
                      dir);

    if (newPath == "") {
        return;
    }

    // automatically add '.mid' extension
    if (!newPath.endsWith(".mid", Qt::CaseInsensitive) && !newPath.endsWith(".midi", Qt::CaseInsensitive)) {
        newPath.append(".mid");
    }

    if (file->save(newPath)) {

        bool printMuteWarning = false;

        for (int i = 0; i < 16; i++) {
            MidiChannel* ch = file->channel(i);
            if (ch->mute() || !ch->visible()) {
                printMuteWarning = true;
            }
        }
        foreach (MidiTrack* track, *(file->tracks())) {
            if (track->muted() || track->hidden()) {
                printMuteWarning = true;
            }
        }

        if (printMuteWarning) {
            QMessageBox::information(this, tr("Channels/Tracks mute"),
                                     tr("One or more channels/tracks are not audible. They will be audible in the saved file."),
                                     tr("Save file"), 0, 0);
        }

        file->setPath(newPath);
        setWindowTitle(QApplication::applicationName() + " - " + file->path() + "[*]");
        updateRecentPathsList();
        setWindowModified(false);
    } else {
        QMessageBox::warning(this, tr("Error"), QString(tr("The file could not be saved. Please make sure that the destination directory exists and that you have the correct access rights to write into this directory.")));
    }
}

void MainWindow::load() {
    QString oldPath = startDirectory;
    if (file) {
        oldPath = file->path();
        if (!file->saved()) {
            saveBeforeClose();
        }
    }

    QFile* f = new QFile(oldPath);
    QString dir = startDirectory;
    if (f->exists()) {
        QFileInfo(*f).dir().path();
    }
    QString newPath = QFileDialog::getOpenFileName(this, tr("Open file"),
                      dir, tr("MIDI Files(*.mid *.midi);;All Files(*)"));

    if (!newPath.isEmpty()) {
        openFile(newPath);
    }
}

void MainWindow::loadFile(QString nfile) {
    QString oldPath = startDirectory;
    if (file) {
        oldPath = file->path();
        if (!file->saved()) {
            saveBeforeClose();
        }
    }
    if (!nfile.isEmpty()) {
        openFile(nfile);
    }
}

void MainWindow::openFile(QString filePath) {

    bool ok = true;

    QFile nf(filePath);

    if (!nf.exists()) {

        QMessageBox::warning(this, tr("Error"), QString(tr("The file [") + filePath + tr("]does not exist!")));
        return;
    }

    startDirectory = QFileInfo(nf).absoluteDir().path() + "/";

    MidiFile* mf = new MidiFile(filePath, &ok);

    if (ok) {
        stop();
        setFile(mf);
        updateRecentPathsList();
    } else {
        QMessageBox::warning(this, tr("Error"), QString(tr("The file is damaged and cannot be opened. ")));
    }
}

void MainWindow::redo() {
    if (file)
        file->protocol()->redo(true);
    updateTrackMenu();
}

void MainWindow::undo() {
    if (file)
        file->protocol()->undo(true);
    updateTrackMenu();
}

EventWidget* MainWindow::eventWidget() {
    return _eventWidget;
}

void MainWindow::muteAllChannels() {
    if (!file)
        return;
    file->protocol()->startNewAction(tr("Mute all channels"));
    for (int i = 0; i < 19; i++) {
        file->channel(i)->setMute(true);
    }
    file->protocol()->endAction();
    channelWidget->update();
}

void MainWindow::unmuteAllChannels() {
    if (!file)
        return;
    file->protocol()->startNewAction(tr("All channels audible"));
    for (int i = 0; i < 19; i++) {
        file->channel(i)->setMute(false);
    }
    file->protocol()->endAction();
    channelWidget->update();
}

void MainWindow::allChannelsVisible() {
    if (!file)
        return;
    file->protocol()->startNewAction(tr("All channels visible"));
    for (int i = 0; i < 19; i++) {
        file->channel(i)->setVisible(true);
    }
    file->protocol()->endAction();
    channelWidget->update();
}

void MainWindow::allChannelsInvisible() {
    if (!file)
        return;
    file->protocol()->startNewAction(tr("Hide all channels"));
    for (int i = 0; i < 19; i++) {
        file->channel(i)->setVisible(false);
    }
    file->protocol()->endAction();
    channelWidget->update();
}

void MainWindow::closeEvent(QCloseEvent* event) {

    if (!file || file->saved()) {
        event->accept();
    } else {
        bool sbc = saveBeforeClose();

        if(sbc) event->accept();
        else event->ignore();
    }

    if (MidiOutput::outputPort() != "") {
        _settings->setValue("out_port", MidiOutput::outputPort());
    }
    if (MidiInput::inputPort() != "") {
        _settings->setValue("in_port", MidiInput::inputPort());
    }

    bool ok;
    int numStart = _settings->value("numStart_v3.5", -1).toInt(&ok);
    _settings->setValue("numStart_v3.5", numStart + 1);

    // save the current Path
    _settings->setValue("open_path", startDirectory);
    _settings->setValue("alt_stop", MidiOutput::isAlternativePlayer);
    _settings->setValue("ticks_per_quarter", MidiFile::defaultTimePerQuarter);
    _settings->setValue("screen_locked", mw_matrixWidget->screenLocked());
    _settings->setValue("magnet", EventTool::magnetEnabled());

    _settings->setValue("div", mw_matrixWidget->div());
    _settings->setValue("colors_from_channel", mw_matrixWidget->colorsByChannel());

    _settings->setValue("metronome", Metronome::enabled());
    _settings->setValue("metronome_loudness", Metronome::loudness());
    _settings->setValue("thru", MidiInput::thru());
    _settings->setValue("quantization", _quantizationGrid);

    Appearance::writeSettings(_settings);
}

void MainWindow::about() {
    AboutDialog* d = new AboutDialog(this);
    d->setModal(true);
    d->show();
}

void MainWindow::setFileLengthMs() {
    if (!file)
        return;

    FileLengthDialog* d = new FileLengthDialog(file, this);
    d->setModal(true);
    d->show();
}

void MainWindow::setStartDir(QString dir) {
    startDirectory = dir;
}

bool MainWindow::saveBeforeClose() {
    switch (QMessageBox::question(this, tr("Save file?"), tr("Save file ") + file->path() + tr(" before closing?"), tr("Save"), tr("Close without saving"), tr("Cancel"), 0, 2)) {
        case 0:
            // save
            if (QFile(file->path()).exists())
                file->save(file->path());
            else saveas();
            return true;

        case 2:
            // break
            return false;

        default:
            return true;
    }
}

void MainWindow::newFile() {
    if (file) {
        if (!file->saved()) {
            saveBeforeClose();
        }
    }

    // create new File
    MidiFile* f = new MidiFile();

    setFile(f);

    editTrack(1);
    setWindowTitle(QApplication::applicationName() + tr(" - Untitled Document[*]"));
}

void MainWindow::panic() {
    MidiPlayer::panic();
}

void MainWindow::screenLockPressed(bool enable) {
    mw_matrixWidget->setScreenLocked(enable);
}

void MainWindow::scaleSelection() {
    bool ok;
    double scale = QInputDialog::getDouble(this, tr("Scalefactor"),
                                           tr("Scalefactor:"), 1.0, 0, 2147483647, 17, &ok);
    if (ok && scale > 0 && Selection::instance()->selectedEvents().size() > 0 && file) {
        // find minimum
        int minTime = 2147483647;
        foreach (MidiEvent* e, Selection::instance()->selectedEvents()) {
            if (e->midiTime() < minTime) {
                minTime = e->midiTime();
            }
        }

        file->protocol()->startNewAction(tr("Scale events"), 0);
        foreach (MidiEvent* e, Selection::instance()->selectedEvents()) {
            e->setMidiTime((e->midiTime() - minTime) * scale + minTime);
            OnEvent* on = dynamic_cast<OnEvent*>(e);
            if (on) {
                MidiEvent* off = on->offEvent();
                off->setMidiTime((off->midiTime() - minTime) * scale + minTime);
            }
        }
        file->protocol()->endAction();
    }
}

void MainWindow::alignLeft() {
    if (Selection::instance()->selectedEvents().size() > 1 && file) {
        // find minimum
        int minTime = 2147483647;
        foreach (MidiEvent* e, Selection::instance()->selectedEvents()) {
            if (e->midiTime() < minTime) {
                minTime = e->midiTime();
            }
        }

        file->protocol()->startNewAction(tr("Align left"), new QImage(":/run_environment/graphics/tool/align_left.png"));
        foreach (MidiEvent* e, Selection::instance()->selectedEvents()) {
            int onTime = e->midiTime();
            e->setMidiTime(minTime);
            OnEvent* on = dynamic_cast<OnEvent*>(e);
            if (on) {
                MidiEvent* off = on->offEvent();
                off->setMidiTime(minTime + (off->midiTime() - onTime));
            }
        }
        file->protocol()->endAction();
    }
}

void MainWindow::alignRight() {
    if (Selection::instance()->selectedEvents().size() > 1 && file) {
        // find maximum
        int maxTime = 0;
        foreach (MidiEvent* e, Selection::instance()->selectedEvents()) {
            OnEvent* on = dynamic_cast<OnEvent*>(e);
            if (on) {
                MidiEvent* off = on->offEvent();
                if (off->midiTime() > maxTime) {
                    maxTime = off->midiTime();
                }
            }
        }

        file->protocol()->startNewAction(tr("Align right"), new QImage(":/run_environment/graphics/tool/align_right.png"));
        foreach (MidiEvent* e, Selection::instance()->selectedEvents()) {
            int onTime = e->midiTime();
            OnEvent* on = dynamic_cast<OnEvent*>(e);
            if (on) {
                MidiEvent* off = on->offEvent();
                e->setMidiTime(maxTime - (off->midiTime() - onTime));
                off->setMidiTime(maxTime);
            }
        }
        file->protocol()->endAction();
    }
}

void MainWindow::equalize() {
    if (Selection::instance()->selectedEvents().size() > 1 && file) {
        // find average
        int avgStart = 0;
        int avgTime = 0;
        int count = 0;
        foreach (MidiEvent* e, Selection::instance()->selectedEvents()) {
            OnEvent* on = dynamic_cast<OnEvent*>(e);
            if (on) {
                MidiEvent* off = on->offEvent();
                avgStart += e->midiTime();
                avgTime += (off->midiTime() - e->midiTime());
                count++;
            }
        }
        if (count > 1) {
            avgStart /= count;
            avgTime /= count;

            file->protocol()->startNewAction(tr("Equalize"), new QImage(":/run_environment/graphics/tool/equalize.png"));
            foreach (MidiEvent* e, Selection::instance()->selectedEvents()) {
                OnEvent* on = dynamic_cast<OnEvent*>(e);
                if (on) {
                    MidiEvent* off = on->offEvent();
                    e->setMidiTime(avgStart);
                    off->setMidiTime(avgStart + avgTime);
                }
            }
        }
        file->protocol()->endAction();
    }
}

void MainWindow::glueSelection() {
    if (!file) {
        return;
    }

    // Ensure the current file is set for the tool
    Tool::setFile(file);

    // Create a temporary GlueTool instance to perform the operation
    // Respect channels (only merge notes within the same channel)
    GlueTool glueTool;
    glueTool.performGlueOperation(true); // true = respect channels
    updateAll();
}

void MainWindow::glueSelectionAllChannels() {
    if (!file) {
        return;
    }

    // Ensure the current file is set for the tool
    Tool::setFile(file);

    // Create a temporary GlueTool instance to perform the operation
    // Don't respect channels (merge notes across all channels on the same track)
    GlueTool glueTool;
    glueTool.performGlueOperation(false); // false = ignore channels
    updateAll();
}

void MainWindow::deleteOverlaps() {
    if (!file) {
        return;
    }

    // Show the delete overlaps dialog
    DeleteOverlapsDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        // Ensure the current file is set for the tool
        Tool::setFile(file);

        // Get user selections from dialog
        DeleteOverlapsTool::OverlapMode mode = dialog.getSelectedMode();
        bool respectChannels = dialog.getRespectChannels();
        bool respectTracks = dialog.getRespectTracks();

        // Create a temporary DeleteOverlapsTool instance to perform the operation
        DeleteOverlapsTool deleteOverlapsTool;
        deleteOverlapsTool.performDeleteOverlapsOperation(mode, respectChannels, respectTracks);
        updateAll();
    }
}

void MainWindow::resetView() {
    if (!file) {
        return;
    }

    // Call the matrix widget's reset view function
    mw_matrixWidget->resetView();
}

void MainWindow::deleteSelectedEvents() {
    bool showsSelected = false;
    if (Tool::currentTool()) {
        EventTool* eventTool = dynamic_cast<EventTool*>(Tool::currentTool());
        if (eventTool) {
            showsSelected = eventTool->showsSelection();
        }
    }
    if (showsSelected && Selection::instance()->selectedEvents().size() > 0 && file) {

        file->protocol()->startNewAction(tr("Remove event(s)"));

        // Group events by channel to minimize protocol entries
        QMap<int, QList<MidiEvent*>> eventsByChannel;
        foreach (MidiEvent* ev, Selection::instance()->selectedEvents()) {
            eventsByChannel[ev->channel()].append(ev);
        }

        // Remove events channel by channel to create fewer protocol entries
        foreach (int channelNum, eventsByChannel.keys()) {
            MidiChannel* channel = file->channel(channelNum);
            ProtocolEntry* toCopy = channel->copy();

            // Remove all events from this channel at once
            foreach (MidiEvent* ev, eventsByChannel[channelNum]) {
                // Handle track name events
                if (channelNum == 16 && (MidiEvent*)(ev->track()->nameEvent()) == ev) {
                    ev->track()->setNameEvent(0);
                }

                // Remove the event and its off event if it exists
                channel->eventMap()->remove(ev->midiTime(), ev);
                OnEvent* on = dynamic_cast<OnEvent*>(ev);
                if (on && on->offEvent()) {
                    channel->eventMap()->remove(on->offEvent()->midiTime(), on->offEvent());
                }
            }

            // Create single protocol entry for this channel
            channel->protocol(toCopy, channel);
        }

        Selection::instance()->clearSelection();
        eventWidget()->reportSelectionChangedByTool();
        file->protocol()->endAction();
    }
}

void MainWindow::deleteChannel(QAction* action) {

    if (!file) {
        return;
    }

    int num = action->data().toInt();
    file->protocol()->startNewAction(tr("Remove all events from channel ") + QString::number(num));
    foreach (MidiEvent* event, file->channel(num)->eventMap()->values()) {
        if (Selection::instance()->selectedEvents().contains(event)) {
            EventTool::deselectEvent(event);
        }
    }
    Selection::instance()->setSelection(Selection::instance()->selectedEvents());

    file->channel(num)->deleteAllEvents();
    file->protocol()->endAction();
}

void MainWindow::moveSelectedEventsToChannel(QAction* action) {

    if (!file) {
        return;
    }

    int num = action->data().toInt();
    MidiChannel* channel = file->channel(num);

    if (Selection::instance()->selectedEvents().size() > 0) {
        file->protocol()->startNewAction(tr("Move selected events to channel ") + QString::number(num));
        foreach (MidiEvent* ev, Selection::instance()->selectedEvents()) {
            file->channel(ev->channel())->removeEvent(ev);
            ev->setChannel(num, true);
            OnEvent* onevent = dynamic_cast<OnEvent*>(ev);
            if (onevent) {
                channel->insertEvent(onevent->offEvent(), onevent->offEvent()->midiTime());
                onevent->offEvent()->setChannel(num);
            }
            channel->insertEvent(ev, ev->midiTime());
        }

        file->protocol()->endAction();
    }
}

void MainWindow::moveSelectedEventsToTrack(QAction* action) {

    if (!file) {
        return;
    }

    int num = action->data().toInt();
    MidiTrack* track = file->track(num);

    if (Selection::instance()->selectedEvents().size() > 0) {
        file->protocol()->startNewAction(tr("Move selected events to track ") + QString::number(num));
        foreach (MidiEvent* ev, Selection::instance()->selectedEvents()) {
            ev->setTrack(track, true);
            OnEvent* onevent = dynamic_cast<OnEvent*>(ev);
            if (onevent) {
                onevent->offEvent()->setTrack(track);
            }
        }

        file->protocol()->endAction();
    }
}

void MainWindow::updateRecentPathsList() {

    // if file opened put it at the top of the list
    if (file) {

        QString currentPath = file->path();
        QStringList newList;
        newList.append(currentPath);

        foreach (QString str, _recentFilePaths) {
            if (str != currentPath && newList.size() < 10) {
                newList.append(str);
            }
        }

        _recentFilePaths = newList;
    }

    // save list
    QVariant list(_recentFilePaths);
    _settings->setValue("recent_file_list", list);

    // update menu
    _recentPathsMenu->clear();
    foreach (QString path, _recentFilePaths) {
        QFile f(path);
        QString name = QFileInfo(f).fileName();

        QVariant variant(path);
        QAction* openRecentFileAction = new QAction(name, this);
        openRecentFileAction->setData(variant);
        _recentPathsMenu->addAction(openRecentFileAction);
    }
}

void MainWindow::openRecent(QAction* action) {

    QString path = action->data().toString();

    if (file) {
        if (!file->saved()) {
            saveBeforeClose();
        }
    }

    openFile(path);
}

void MainWindow::updateChannelMenu() {

    // delete channel events menu
    foreach (QAction* action, _deleteChannelMenu->actions()) {
        int channel = action->data().toInt();
        if (file) {
            action->setText(QString::number(channel) + " " + MidiFile::instrumentName(file->channel(channel)->progAtTick(0)));
        }
    }

    // move events to channel...
    foreach (QAction* action, _moveSelectedEventsToChannelMenu->actions()) {
        int channel = action->data().toInt();
        if (file) {
            action->setText(QString::number(channel) + " " + MidiFile::instrumentName(file->channel(channel)->progAtTick(0)));
        }
    }

    // paste events to channel...
    foreach (QAction* action, _pasteToChannelMenu->actions()) {
        int channel = action->data().toInt();
        if (file && channel >= 0) {
            action->setText(QString::number(channel) + " " + MidiFile::instrumentName(file->channel(channel)->progAtTick(0)));
        }
    }

    // select all events from channel...
    foreach (QAction* action, _selectAllFromChannelMenu->actions()) {
        int channel = action->data().toInt();
        if (file) {
            action->setText(QString::number(channel) + " " + MidiFile::instrumentName(file->channel(channel)->progAtTick(0)));
        }
    }

    _chooseEditChannel->setCurrentIndex(NewNoteTool::editChannel());
}

void MainWindow::updateTrackMenu() {

    _moveSelectedEventsToTrackMenu->clear();
    _chooseEditTrack->clear();
    _selectAllFromTrackMenu->clear();

    if (!file) {
        return;
    }

    for (int i = 0; i < file->numTracks(); i++) {
        QVariant variant(i);
        QAction* moveToTrackAction = new QAction(QString::number(i) + " " + file->tracks()->at(i)->name(), this);
        moveToTrackAction->setData(variant);
		
        QString formattedKeySequence = QString("Shift+%1").arg(i);
        moveToTrackAction->setShortcut(QKeySequence::fromString(formattedKeySequence));

        _moveSelectedEventsToTrackMenu->addAction(moveToTrackAction);
    }

    for (int i = 0; i < file->numTracks(); i++) {
        QVariant variant(i);
        QAction* select = new QAction(QString::number(i) + " " + file->tracks()->at(i)->name(), this);
        select->setData(variant);
        _selectAllFromTrackMenu->addAction(select);
    }

    for (int i = 0; i < file->numTracks(); i++) {
        _chooseEditTrack->addItem(tr("Track ") + QString::number(i) + ": " + file->tracks()->at(i)->name());
    }
    if (NewNoteTool::editTrack() >= file->numTracks()) {
        NewNoteTool::setEditTrack(0);
    }
    _chooseEditTrack->setCurrentIndex(NewNoteTool::editTrack());

    _pasteToTrackMenu->clear();
    QActionGroup* pasteTrackGroup = new QActionGroup(this);
    pasteTrackGroup->setExclusive(true);

    bool checked = false;
    for (int i = -2; i < file->numTracks(); i++) {
        QVariant variant(i);
        QString text = QString::number(i);
        if (i == -2) {
            text = tr("Same as selected for new events");
        } else if (i == -1) {
            text = tr("Keep track");
        } else {
            text = tr("Track ") + QString::number(i) + ": " + file->tracks()->at(i)->name();
        }
        QAction* pasteToTrackAction = new QAction(text, this);
        pasteToTrackAction->setData(variant);
        pasteToTrackAction->setCheckable(true);
        _pasteToTrackMenu->addAction(pasteToTrackAction);
        pasteTrackGroup->addAction(pasteToTrackAction);
        if (i == EventTool::pasteTrack()) {
            pasteToTrackAction->setChecked(true);
            checked = true;
        }
    }
    if (!checked) {
        _pasteToTrackMenu->actions().first()->setChecked(true);
        EventTool::setPasteTrack(0);
    }
}

void MainWindow::muteChannel(QAction* action) {
    int channel = action->data().toInt();
    if (file) {
        file->protocol()->startNewAction(tr("Mute channel"));
        file->channel(channel)->setMute(action->isChecked());
        updateChannelMenu();
        channelWidget->update();
        file->protocol()->endAction();
    }
}
void MainWindow::soloChannel(QAction* action) {
    int channel = action->data().toInt();
    if (file) {
        file->protocol()->startNewAction(tr("Select solo channel"));
        for (int i = 0; i < 16; i++) {
            file->channel(i)->setSolo(i == channel && action->isChecked());
        }
        file->protocol()->endAction();
    }
    channelWidget->update();
    updateChannelMenu();
}

void MainWindow::viewChannel(QAction* action) {
    int channel = action->data().toInt();
    if (file) {
        file->protocol()->startNewAction(tr("Channel visibility changed"));
        file->channel(channel)->setVisible(action->isChecked());
        updateChannelMenu();
        channelWidget->update();
        file->protocol()->endAction();
    }
}

void MainWindow::keyPressEvent(QKeyEvent* event) {
    // First, let Qt handle any shortcuts
    QMainWindow::keyPressEvent(event);

    // If the event wasn't accepted by a shortcut, forward it to the matrix widget
    if (!event->isAccepted()) {
        mw_matrixWidget->takeKeyPressEvent(event);
    }
}

void MainWindow::keyReleaseEvent(QKeyEvent* event) {
    // First, let Qt handle any shortcuts
    QMainWindow::keyReleaseEvent(event);

    // If the event wasn't accepted by a shortcut, forward it to the matrix widget
    if (!event->isAccepted()) {
        mw_matrixWidget->takeKeyReleaseEvent(event);
    }
}

void MainWindow::showEventWidget(bool show) {
    if (show) {
        lowerTabWidget->setCurrentIndex(1);
    } else {
        lowerTabWidget->setCurrentIndex(0);
    }
}

void MainWindow::renameTrackMenuClicked(QAction* action) {
    int track = action->data().toInt();
    renameTrack(track);
}

void MainWindow::renameTrack(int tracknumber) {

    if (!file) {
        return;
    }

    file->protocol()->startNewAction(tr("Edit Track Name"));

    bool ok;
    QString text = QInputDialog::getText(this, tr("Set Track Name"),
                                         tr("Track name (Track ") + QString::number(tracknumber) + tr(")"), QLineEdit::Normal,
                                         file->tracks()->at(tracknumber)->name(), &ok);
    if (ok && !text.isEmpty()) {
        file->tracks()->at(tracknumber)->setName(text);
    }

    file->protocol()->endAction();
    updateTrackMenu();
}

void MainWindow::removeTrackMenuClicked(QAction* action) {
    int track = action->data().toInt();
    removeTrack(track);
}

void MainWindow::removeTrack(int tracknumber) {

    if (!file) {
        return;
    }
    MidiTrack* track = file->track(tracknumber);
    file->protocol()->startNewAction(tr("Remove track"));
    foreach (MidiEvent* event, Selection::instance()->selectedEvents()) {
        if (event->track() == track) {
            EventTool::deselectEvent(event);
        }
    }
    Selection::instance()->setSelection(Selection::instance()->selectedEvents());
    if (!file->removeTrack(track)) {
        QMessageBox::warning(this, tr("Error"), QString(tr("The selected track can\'t be removed!\n It\'s the last track of the file.")));
    }
    file->protocol()->endAction();
    updateTrackMenu();
}

void MainWindow::addTrack() {

    if (file) {

        bool ok;
        QString text = QInputDialog::getText(this, tr("Set Track Name"),
                                             tr("Track name (New Track)"), QLineEdit::Normal,
                                             tr("New Track"), &ok);
        if (ok && !text.isEmpty()) {

            file->protocol()->startNewAction("Add track");
            file->addTrack();
            file->tracks()->at(file->numTracks() - 1)->setName(text);
            file->protocol()->endAction();

            updateTrackMenu();
        }
    }
}

void MainWindow::muteAllTracks() {
    if (!file)
        return;
    file->protocol()->startNewAction(tr("Mute all tracks"));
    foreach (MidiTrack* track, *(file->tracks())) {
        track->setMuted(true);
    }
    file->protocol()->endAction();
    _trackWidget->update();
}

void MainWindow::unmuteAllTracks() {
    if (!file)
        return;
    file->protocol()->startNewAction(tr("All tracks audible"));
    foreach (MidiTrack* track, *(file->tracks())) {
        track->setMuted(false);
    }
    file->protocol()->endAction();
    _trackWidget->update();
}

void MainWindow::allTracksVisible() {
    if (!file)
        return;
    file->protocol()->startNewAction(tr("Show all tracks"));
    foreach (MidiTrack* track, *(file->tracks())) {
        track->setHidden(false);
    }
    file->protocol()->endAction();
    _trackWidget->update();
}

void MainWindow::allTracksInvisible() {
    if (!file)
        return;
    file->protocol()->startNewAction(tr("Hide all tracks"));
    foreach (MidiTrack* track, *(file->tracks())) {
        track->setHidden(true);
    }
    file->protocol()->endAction();
    _trackWidget->update();
}

void MainWindow::showTrackMenuClicked(QAction* action) {
    int track = action->data().toInt();
    if (file) {
        file->protocol()->startNewAction(tr("Show track"));
        file->track(track)->setHidden(!(action->isChecked()));
        updateTrackMenu();
        _trackWidget->update();
        file->protocol()->endAction();
    }
}

void MainWindow::muteTrackMenuClicked(QAction* action) {
    int track = action->data().toInt();
    if (file) {
        file->protocol()->startNewAction(tr("Mute track"));
        file->track(track)->setMuted(action->isChecked());
        updateTrackMenu();
        _trackWidget->update();
        file->protocol()->endAction();
    }
}

void MainWindow::selectAllFromChannel(QAction* action) {

    if (!file) {
        return;
    }
    int channel = action->data().toInt();
    file->protocol()->startNewAction("Select all events from channel " + QString::number(channel));
    EventTool::clearSelection();
    file->channel(channel)->setVisible(true);
    foreach (MidiEvent* e, file->channel(channel)->eventMap()->values()) {
        if (e->track()->hidden()) {
            e->track()->setHidden(false);
        }
        EventTool::selectEvent(e, false, false, false);
    }
    Selection::instance()->setSelection(Selection::instance()->selectedEvents());

    file->protocol()->endAction();
}

void MainWindow::selectAllFromTrack(QAction* action) {

    if (!file) {
        return;
    }

    int track = action->data().toInt();
    file->protocol()->startNewAction("Select all events from track " + QString::number(track));
    EventTool::clearSelection();
    file->track(track)->setHidden(false);
    for (int channel = 0; channel < 16; channel++) {
        foreach (MidiEvent* e, file->channel(channel)->eventMap()->values()) {
            if (e->track()->number() == track) {
                file->channel(e->channel())->setVisible(true);
                EventTool::selectEvent(e, false, false, false);
            }
        }
    }
    Selection::instance()->setSelection(Selection::instance()->selectedEvents());
    file->protocol()->endAction();
}

void MainWindow::selectAll() {

    if (!file) {
        return;
    }

    file->protocol()->startNewAction("Select all");

    for (int i = 0; i < 16; i++) {
        foreach (MidiEvent* event, file->channel(i)->eventMap()->values()) {
            EventTool::selectEvent(event, false, true, false);
        }
    }
    Selection::instance()->setSelection(Selection::instance()->selectedEvents());

    file->protocol()->endAction();
}

void MainWindow::transposeNSemitones() {

    if (!file) {
        return;
    }

    QList<NoteOnEvent*> events;
    foreach (MidiEvent* event, Selection::instance()->selectedEvents()) {
        NoteOnEvent* on = dynamic_cast<NoteOnEvent*>(event);
        if (on) {
            events.append(on);
        }
    }

    if (events.isEmpty()) {
        return;
    }

    TransposeDialog* d = new TransposeDialog(events, file, this);
    d->setModal(true);
    d->show();
}

void MainWindow::copy() {
    EventTool::copyAction();
}

void MainWindow::paste() {
    EventTool::pasteAction();
}

void MainWindow::markEdited() {
    setWindowModified(true);
}

void MainWindow::colorsByChannel() {
    mw_matrixWidget->setColorsByChannel();
    _colorsByChannel->setChecked(true);
    _colorsByTracks->setChecked(false);
    mw_matrixWidget->registerRelayout();
    mw_matrixWidget->update();
    _miscWidget->update();
}
void MainWindow::colorsByTrack() {
    mw_matrixWidget->setColorsByTracks();
    _colorsByChannel->setChecked(false);
    _colorsByTracks->setChecked(true);
    mw_matrixWidget->registerRelayout();
    mw_matrixWidget->update();
    _miscWidget->update();
}

void MainWindow::editChannel(int i, bool assign) {
    NewNoteTool::setEditChannel(i);

    // assign channel to track
    if (assign && file && file->track(NewNoteTool::editTrack())) {
        file->track(NewNoteTool::editTrack())->assignChannel(i);
    }

    MidiOutput::setStandardChannel(i);

    int prog = file->channel(i)->progAtTick(file->cursorTick());
    MidiOutput::sendProgram(i, prog);

    updateChannelMenu();
}

void MainWindow::editTrack(int i, bool assign) {
    NewNoteTool::setEditTrack(i);

    // assign channel to track
    if (assign && file && file->track(i)) {
        file->track(i)->assignChannel(NewNoteTool::editChannel());
    }
    updateTrackMenu();
}

void MainWindow::editTrackAndChannel(MidiTrack* track) {
    editTrack(track->number(), false);
    if (track->assignedChannel() > -1) {
        editChannel(track->assignedChannel(), false);
    }
}

void MainWindow::setInstrumentForChannel(int i) {
    InstrumentChooser* d = new InstrumentChooser(file, i, this);
    d->setModal(true);
    d->exec();

    if (i == NewNoteTool::editChannel()) {
        editChannel(i);
    }
    updateChannelMenu();
}

void MainWindow::instrumentChannel(QAction* action) {
    if (file) {
        setInstrumentForChannel(action->data().toInt());
    }
}

void MainWindow::spreadSelection() {

    if (!file) {
        return;
    }

    bool ok;
    float numMs = float(QInputDialog::getDouble(this, tr("Set spread-time"),
                        tr("Spread time [ms]"), 10,
                        5, 500, 2, &ok));

    if (!ok) {
        numMs = 1;
    }

    QMultiMap<int, int> spreadChannel[19];

    foreach (MidiEvent* event, Selection::instance()->selectedEvents()) {
        if (!spreadChannel[event->channel()].values(event->line()).contains(event->midiTime())) {
            spreadChannel[event->channel()].insert(event->line(), event->midiTime());
        }
    }

    file->protocol()->startNewAction(tr("Spread events"));
    int numSpreads = 0;
    for (int i = 0; i < 19; i++) {

        MidiChannel* channel = file->channel(i);

        QList<int> seenBefore;

        foreach (int line, spreadChannel[i].keys()) {

            if (seenBefore.contains(line)) {
                continue;
            }

            seenBefore.append(line);

            foreach (int position, spreadChannel[i].values(line)) {

                QList<MidiEvent*> eventsWithAllLines = channel->eventMap()->values(position);

                QList<MidiEvent*> events;
                foreach (MidiEvent* event, eventsWithAllLines) {
                    if (event->line() == line) {
                        events.append(event);
                    }
                }

                //spread events for the channel at the given position
                int num = events.count();
                if (num > 1) {

                    float timeToInsert = file->msOfTick(position) + numMs * num / 2;

                    for (int y = 0; y < num; y++) {

                        MidiEvent* toMove = events.at(y);

                        toMove->setMidiTime(file->tick(timeToInsert), true);
                        numSpreads++;

                        timeToInsert -= numMs;
                    }
                }
            }
        }
    }
    file->protocol()->endAction();

    QMessageBox::information(this, tr("Spreading done"), QString(tr("Spreaded ") + QString::number(numSpreads) + tr(" events")));
}

void MainWindow::manual() {
    QDesktopServices::openUrl(QUrl("https://meowchestra.github.io/MidiEditor/manual/", QUrl::TolerantMode));
}

void MainWindow::changeMiscMode(int mode) {
    _miscWidget->setMode(mode);
    if (mode == VelocityEditor || mode == TempoEditor) {
        _miscChannel->setEnabled(false);
    } else {
        _miscChannel->setEnabled(true);
    }
    if (mode == ControllEditor || mode == KeyPressureEditor) {
        _miscController->setEnabled(true);
        _miscController->clear();

        if (mode == ControllEditor) {
            for (int i = 0; i < 128; i++) {
                _miscController->addItem(MidiFile::controlChangeName(i));
            }
        } else {
            for (int i = 0; i < 128; i++) {
                _miscController->addItem(tr("Note: ") + QString::number(i));
            }
        }

        _miscController->view()->setMinimumWidth(_miscController->minimumSizeHint().width());
    } else {
        _miscController->setEnabled(false);
    }
}

void MainWindow::selectModeChanged(QAction* action) {
    if (action == setSingleMode) {
        _miscWidget->setEditMode(SINGLE_MODE);
    }
    if (action == setLineMode) {
        _miscWidget->setEditMode(LINE_MODE);
    }
    if (action == setFreehandMode) {
        _miscWidget->setEditMode(MOUSE_MODE);
    }
}

QWidget* MainWindow::setupActions(QWidget* parent) {

    // Menubar
    QMenu* fileMB = menuBar()->addMenu(tr("File"));
    QMenu* editMB = menuBar()->addMenu(tr("Edit"));
    QMenu* toolsMB = menuBar()->addMenu(tr("Tools"));
    QMenu* viewMB = menuBar()->addMenu(tr("View"));
    QMenu* playbackMB = menuBar()->addMenu(tr("Playback"));
    QMenu* midiMB = menuBar()->addMenu(tr("Midi"));
    QMenu* helpMB = menuBar()->addMenu(tr("Help"));

    // File
    QAction* newAction = new QAction(tr("New"), this);
    newAction->setShortcut(QKeySequence::New);
    Appearance::setActionIcon(newAction, ":/run_environment/graphics/tool/new.png");
    connect(newAction, SIGNAL(triggered()), this, SLOT(newFile()));
    fileMB->addAction(newAction);
    _actionMap["new"] = newAction;

    QAction* loadAction = new QAction(tr("Open..."), this);
    loadAction->setShortcut(QKeySequence::Open);
    Appearance::setActionIcon(loadAction, ":/run_environment/graphics/tool/load.png");
    connect(loadAction, SIGNAL(triggered()), this, SLOT(load()));
    fileMB->addAction(loadAction);
    _actionMap["open"] = loadAction;

    _recentPathsMenu = new QMenu(tr("Open recent..."), this);
    _recentPathsMenu->setIcon(Appearance::adjustIconForDarkMode(":/run_environment/graphics/tool/noicon.png"));
    fileMB->addMenu(_recentPathsMenu);
    connect(_recentPathsMenu, SIGNAL(triggered(QAction*)), this, SLOT(openRecent(QAction*)));

    updateRecentPathsList();

    fileMB->addSeparator();

    QAction* saveAction = new QAction(tr("Save"), this);
    saveAction->setShortcut(QKeySequence::Save);
    Appearance::setActionIcon(saveAction, ":/run_environment/graphics/tool/save.png");
    connect(saveAction, SIGNAL(triggered()), this, SLOT(save()));
    fileMB->addAction(saveAction);
    _actionMap["save"] = saveAction;

    QAction* saveAsAction = new QAction(tr("Save as..."), this);
    saveAsAction->setShortcut(QKeySequence::SaveAs);
    Appearance::setActionIcon(saveAsAction, ":/run_environment/graphics/tool/saveas.png");
    connect(saveAsAction, SIGNAL(triggered()), this, SLOT(saveas()));
    fileMB->addAction(saveAsAction);

    fileMB->addSeparator();

    QAction* quitAction = new QAction(tr("Quit"), this);
    quitAction->setShortcut(QKeySequence::Quit);
    Appearance::setActionIcon(quitAction, ":/run_environment/graphics/tool/noicon.png");
    connect(quitAction, SIGNAL(triggered()), this, SLOT(close()));
    fileMB->addAction(quitAction);

    // Edit
    undoAction = new QAction(tr("Undo"), this);
    undoAction->setShortcut(QKeySequence::Undo);
    Appearance::setActionIcon(undoAction, ":/run_environment/graphics/tool/undo.png");
    connect(undoAction, SIGNAL(triggered()), this, SLOT(undo()));
    editMB->addAction(undoAction);
    _actionMap["undo"] = undoAction;

    redoAction = new QAction(tr("Redo"), this);
    redoAction->setShortcut(QKeySequence::Redo);
    Appearance::setActionIcon(redoAction, ":/run_environment/graphics/tool/redo.png");
    connect(redoAction, SIGNAL(triggered()), this, SLOT(redo()));
    editMB->addAction(redoAction);
    _actionMap["redo"] = redoAction;

    editMB->addSeparator();

    QAction* selectAllAction = new QAction(tr("Select all"), this);
    selectAllAction->setToolTip(tr("Select all visible events"));
    selectAllAction->setShortcut(QKeySequence::SelectAll);
    connect(selectAllAction, SIGNAL(triggered()), this, SLOT(selectAll()));
    editMB->addAction(selectAllAction);

    _selectAllFromChannelMenu = new QMenu(tr("Select all events from channel..."), editMB);
    editMB->addMenu(_selectAllFromChannelMenu);
    connect(_selectAllFromChannelMenu, SIGNAL(triggered(QAction*)), this, SLOT(selectAllFromChannel(QAction*)));

    for (int i = 0; i < 16; i++) {
        QVariant variant(i);
        QAction* delChannelAction = new QAction(QString::number(i), this);
        delChannelAction->setData(variant);
        _selectAllFromChannelMenu->addAction(delChannelAction);
    }

    _selectAllFromTrackMenu = new QMenu(tr("Select all events from track..."), editMB);
    editMB->addMenu(_selectAllFromTrackMenu);
    connect(_selectAllFromTrackMenu, SIGNAL(triggered(QAction*)), this, SLOT(selectAllFromTrack(QAction*)));

    for (int i = 0; i < 16; i++) {
        QVariant variant(i);
        QAction* delChannelAction = new QAction(QString::number(i), this);
        delChannelAction->setData(variant);
        _selectAllFromTrackMenu->addAction(delChannelAction);
    }

    editMB->addSeparator();

    QAction* navigateSelectionUpAction = new QAction(tr("Navigate selection up"), editMB);
    navigateSelectionUpAction->setShortcut(QKeySequence(Qt::Key_Up));
    connect(navigateSelectionUpAction, SIGNAL(triggered()), this, SLOT(navigateSelectionUp()));
    editMB->addAction(navigateSelectionUpAction);

    QAction* navigateSelectionDownAction = new QAction(tr("Navigate selection down"), editMB);
    navigateSelectionDownAction->setShortcut(QKeySequence(Qt::Key_Down));
    connect(navigateSelectionDownAction, SIGNAL(triggered()), this, SLOT(navigateSelectionDown()));
    editMB->addAction(navigateSelectionDownAction);

    QAction* navigateSelectionLeftAction = new QAction(tr("Navigate selection left"), editMB);
    navigateSelectionLeftAction->setShortcut(QKeySequence(Qt::Key_Left));
    connect(navigateSelectionLeftAction, SIGNAL(triggered()), this, SLOT(navigateSelectionLeft()));
    editMB->addAction(navigateSelectionLeftAction);

    QAction* navigateSelectionRightAction = new QAction(tr("Navigate selection right"), editMB);
    navigateSelectionRightAction->setShortcut(QKeySequence(Qt::Key_Right));
    connect(navigateSelectionRightAction, SIGNAL(triggered()), this, SLOT(navigateSelectionRight()));
    editMB->addAction(navigateSelectionRightAction);

    editMB->addSeparator();

    QAction* copyAction = new QAction(tr("Copy events"), this);
    _activateWithSelections.append(copyAction);
    Appearance::setActionIcon(copyAction, ":/run_environment/graphics/tool/copy.png");
    copyAction->setShortcut(QKeySequence::Copy);
    connect(copyAction, SIGNAL(triggered()), this, SLOT(copy()));
    editMB->addAction(copyAction);
    _actionMap["copy"] = copyAction;

    _pasteAction = new QAction(tr("Paste events"), this);
    _pasteAction->setToolTip(tr("Paste events at cursor position"));
    _pasteAction->setShortcut(QKeySequence::Paste);
    Appearance::setActionIcon(_pasteAction, ":/run_environment/graphics/tool/paste.png");
    connect(_pasteAction, SIGNAL(triggered()), this, SLOT(paste()));
    _actionMap["paste"] = _pasteAction;

    _pasteToTrackMenu = new QMenu(tr("Paste to track..."));
    _pasteToChannelMenu = new QMenu(tr("Paste to channel..."));
    _pasteOptionsMenu = new QMenu(tr("Paste options..."));
    _pasteOptionsMenu->addMenu(_pasteToChannelMenu);
    QActionGroup* pasteChannelGroup = new QActionGroup(this);
    pasteChannelGroup->setExclusive(true);
    connect(_pasteToChannelMenu, SIGNAL(triggered(QAction*)), this, SLOT(pasteToChannel(QAction*)));
    connect(_pasteToTrackMenu, SIGNAL(triggered(QAction*)), this, SLOT(pasteToTrack(QAction*)));

    for (int i = -2; i < 16; i++) {
        QVariant variant(i);
        QString text = QString::number(i);
        if (i == -2) {
            text = tr("Same as selected for new events");
        }
        if (i == -1) {
            text = tr("Keep channel");
        }
        QAction* pasteToChannelAction = new QAction(text, this);
        pasteToChannelAction->setData(variant);
        pasteToChannelAction->setCheckable(true);
        _pasteToChannelMenu->addAction(pasteToChannelAction);
        pasteChannelGroup->addAction(pasteToChannelAction);
        pasteToChannelAction->setChecked(i < 0);
    }
    _pasteOptionsMenu->addMenu(_pasteToTrackMenu);
    editMB->addAction(_pasteAction);
    editMB->addMenu(_pasteOptionsMenu);

    editMB->addSeparator();

    QAction* configAction = new QAction(tr("Settings"), this);
    Appearance::setActionIcon(configAction, ":/run_environment/graphics/tool/config.png");
    connect(configAction, SIGNAL(triggered()), this, SLOT(openConfig()));
    editMB->addAction(configAction);

    // Tools
    QMenu* toolsToolsMenu = new QMenu(tr("Current tool..."), toolsMB);

    StandardTool* tool = new StandardTool();
    Tool::setCurrentTool(tool);
    stdToolAction = new ToolButton(tool, QKeySequence(Qt::Key_F1), toolsToolsMenu);
    toolsToolsMenu->addAction(stdToolAction);
    tool->buttonClick();
    _actionMap["standard_tool"] = stdToolAction;

    QAction* newNoteAction = new ToolButton(new NewNoteTool(), QKeySequence(Qt::Key_F2), toolsToolsMenu);
    toolsToolsMenu->addAction(newNoteAction);
    _actionMap["new_note"] = newNoteAction;
    QAction* removeNotesAction = new ToolButton(new EraserTool(), QKeySequence(Qt::Key_F3), toolsToolsMenu);
    toolsToolsMenu->addAction(removeNotesAction);
    _actionMap["remove_notes"] = removeNotesAction;

    toolsToolsMenu->addSeparator();

    QAction* selectSingleAction = new ToolButton(new SelectTool(SELECTION_TYPE_SINGLE), QKeySequence(Qt::Key_F4), toolsToolsMenu);
    toolsToolsMenu->addAction(selectSingleAction);
    _actionMap["select_single"] = selectSingleAction;
    QAction* selectBoxAction = new ToolButton(new SelectTool(SELECTION_TYPE_BOX), QKeySequence(Qt::Key_F5), toolsToolsMenu);
    toolsToolsMenu->addAction(selectBoxAction);
    _actionMap["select_box"] = selectBoxAction;
    QAction* selectLeftAction = new ToolButton(new SelectTool(SELECTION_TYPE_LEFT), QKeySequence(Qt::Key_F6), toolsToolsMenu);
    toolsToolsMenu->addAction(selectLeftAction);
    _actionMap["select_left"] = selectLeftAction;
    QAction* selectRightAction = new ToolButton(new SelectTool(SELECTION_TYPE_RIGHT), QKeySequence(Qt::Key_F7), toolsToolsMenu);
    toolsToolsMenu->addAction(selectRightAction);
    _actionMap["select_right"] = selectRightAction;

    toolsToolsMenu->addSeparator();

    QAction* moveAllAction = new ToolButton(new EventMoveTool(true, true), QKeySequence(Qt::Key_F8), toolsToolsMenu);
    _activateWithSelections.append(moveAllAction);
    toolsToolsMenu->addAction(moveAllAction);
    _actionMap["move_all"] = moveAllAction;

    QAction* moveLRAction = new ToolButton(new EventMoveTool(false, true), QKeySequence(Qt::Key_F9), toolsToolsMenu);
    _activateWithSelections.append(moveLRAction);
    toolsToolsMenu->addAction(moveLRAction);
    _actionMap["move_lr"] = moveLRAction;

    QAction* moveUDAction = new ToolButton(new EventMoveTool(true, false), QKeySequence(Qt::Key_F10), toolsToolsMenu);
    _activateWithSelections.append(moveUDAction);
    toolsToolsMenu->addAction(moveUDAction);
    _actionMap["move_ud"] = moveUDAction;

    QAction* sizeChangeAction = new ToolButton(new SizeChangeTool(), QKeySequence(Qt::Key_F11), toolsToolsMenu);
    _activateWithSelections.append(sizeChangeAction);
    toolsToolsMenu->addAction(sizeChangeAction);
    _actionMap["size_change"] = sizeChangeAction;

    toolsToolsMenu->addSeparator();

    QAction* measureAction= new ToolButton(new MeasureTool(), QKeySequence(Qt::Key_F12), toolsToolsMenu);
    toolsToolsMenu->addAction(measureAction);
    _actionMap["measure"] = measureAction;
    QAction* timeSignatureAction= new ToolButton(new TimeSignatureTool(), QKeySequence(Qt::Key_F13), toolsToolsMenu);
    toolsToolsMenu->addAction(timeSignatureAction);
    _actionMap["time_signature"] = timeSignatureAction;
    QAction* tempoAction= new ToolButton(new TempoTool(), QKeySequence(Qt::Key_F14), toolsToolsMenu);
    toolsToolsMenu->addAction(tempoAction);
    _actionMap["tempo"] = tempoAction;

    toolsMB->addMenu(toolsToolsMenu);

    // Tweak

    QMenu* tweakMenu = new QMenu(tr("Tweak..."), toolsMB);

    QAction* tweakTimeAction = new QAction(tr("Time"), tweakMenu);
    tweakTimeAction->setShortcut(QKeySequence(Qt::Key_1 | Qt::CTRL));
    tweakTimeAction->setCheckable(true);
    connect(tweakTimeAction, SIGNAL(triggered()), this, SLOT(tweakTime()));
    tweakMenu->addAction(tweakTimeAction);

    QAction* tweakStartTimeAction = new QAction(tr("Start time"), tweakMenu);
    tweakStartTimeAction->setShortcut(QKeySequence(Qt::Key_2 | Qt::CTRL));
    tweakStartTimeAction->setCheckable(true);
    connect(tweakStartTimeAction, SIGNAL(triggered()), this, SLOT(tweakStartTime()));
    tweakMenu->addAction(tweakStartTimeAction);

    QAction* tweakEndTimeAction = new QAction(tr("End time"), tweakMenu);
    tweakEndTimeAction->setShortcut(QKeySequence(Qt::Key_3 | Qt::CTRL));
    tweakEndTimeAction->setCheckable(true);
    connect(tweakEndTimeAction, SIGNAL(triggered()), this, SLOT(tweakEndTime()));
    tweakMenu->addAction(tweakEndTimeAction);

    QAction* tweakNoteAction = new QAction(tr("Note"), tweakMenu);
    tweakNoteAction->setShortcut(QKeySequence(Qt::Key_4 | Qt::CTRL));
    tweakNoteAction->setCheckable(true);
    connect(tweakNoteAction, SIGNAL(triggered()), this, SLOT(tweakNote()));
    tweakMenu->addAction(tweakNoteAction);

    QAction* tweakValueAction = new QAction(tr("Value"), tweakMenu);
    tweakValueAction->setShortcut(QKeySequence(Qt::Key_5 | Qt::CTRL));
    tweakValueAction->setCheckable(true);
    connect(tweakValueAction, SIGNAL(triggered()), this, SLOT(tweakValue()));
    tweakMenu->addAction(tweakValueAction);

    QActionGroup* tweakTargetActionGroup = new QActionGroup(this);
    tweakTargetActionGroup->setExclusive(true);
    tweakTargetActionGroup->addAction(tweakTimeAction);
    tweakTargetActionGroup->addAction(tweakStartTimeAction);
    tweakTargetActionGroup->addAction(tweakEndTimeAction);
    tweakTargetActionGroup->addAction(tweakNoteAction);
    tweakTargetActionGroup->addAction(tweakValueAction);
    tweakTimeAction->setChecked(true);

    tweakMenu->addSeparator();

    QAction* tweakSmallDecreaseAction = new QAction(tr("Small decrease"), tweakMenu);
    tweakSmallDecreaseAction->setShortcut(QKeySequence(Qt::Key_9 | Qt::CTRL));
    connect(tweakSmallDecreaseAction, SIGNAL(triggered()), this, SLOT(tweakSmallDecrease()));
    tweakMenu->addAction(tweakSmallDecreaseAction);

    QAction* tweakSmallIncreaseAction = new QAction(tr("Small increase"), tweakMenu);
    tweakSmallIncreaseAction->setShortcut(QKeySequence(Qt::Key_0 | Qt::CTRL));
    connect(tweakSmallIncreaseAction, SIGNAL(triggered()), this, SLOT(tweakSmallIncrease()));
    tweakMenu->addAction(tweakSmallIncreaseAction);

    QAction* tweakMediumDecreaseAction = new QAction(tr("Medium decrease"), tweakMenu);
    tweakMediumDecreaseAction->setShortcut(QKeySequence(Qt::Key_9 | Qt::CTRL | Qt::ALT));
    connect(tweakMediumDecreaseAction, SIGNAL(triggered()), this, SLOT(tweakMediumDecrease()));
    tweakMenu->addAction(tweakMediumDecreaseAction);

    QAction* tweakMediumIncreaseAction = new QAction(tr("Medium increase"), tweakMenu);
    tweakMediumIncreaseAction->setShortcut(QKeySequence(Qt::Key_0 | Qt::CTRL | Qt::ALT));
    connect(tweakMediumIncreaseAction, SIGNAL(triggered()), this, SLOT(tweakMediumIncrease()));
    tweakMenu->addAction(tweakMediumIncreaseAction);

    QAction* tweakLargeDecreaseAction = new QAction(tr("Large decrease"), tweakMenu);
    tweakLargeDecreaseAction->setShortcut(QKeySequence(Qt::Key_9 | Qt::CTRL | Qt::ALT | Qt::SHIFT));
    connect(tweakLargeDecreaseAction, SIGNAL(triggered()), this, SLOT(tweakLargeDecrease()));
    tweakMenu->addAction(tweakLargeDecreaseAction);

    QAction* tweakLargeIncreaseAction = new QAction(tr("Large increase"), tweakMenu);
    tweakLargeIncreaseAction->setShortcut(QKeySequence(Qt::Key_0 | Qt::CTRL | Qt::ALT | Qt::SHIFT));
    connect(tweakLargeIncreaseAction, SIGNAL(triggered()), this, SLOT(tweakLargeIncrease()));
    tweakMenu->addAction(tweakLargeIncreaseAction);

    toolsMB->addMenu(tweakMenu);

    QAction* deleteAction = new QAction(tr("Remove events"), this);
    _activateWithSelections.append(deleteAction);
    deleteAction->setToolTip(tr("Remove selected events"));
    deleteAction->setShortcut(QKeySequence::Delete);
    Appearance::setActionIcon(deleteAction, ":/run_environment/graphics/tool/eraser.png");
    connect(deleteAction, SIGNAL(triggered()), this, SLOT(deleteSelectedEvents()));
    toolsMB->addAction(deleteAction);
    _actionMap["delete"] = deleteAction;

    toolsMB->addSeparator();

    QAction* alignLeftAction = new QAction(tr("Align left"), this);
    _activateWithSelections.append(alignLeftAction);
    alignLeftAction->setShortcut(QKeySequence(Qt::Key_Left | Qt::CTRL));
    Appearance::setActionIcon(alignLeftAction, ":/run_environment/graphics/tool/align_left.png");
    connect(alignLeftAction, SIGNAL(triggered()), this, SLOT(alignLeft()));
    toolsMB->addAction(alignLeftAction);
    _actionMap["align_left"] = alignLeftAction;

    QAction* alignRightAction = new QAction(tr("Align right"), this);
    _activateWithSelections.append(alignRightAction);
    Appearance::setActionIcon(alignRightAction, ":/run_environment/graphics/tool/align_right.png");
    alignRightAction->setShortcut(QKeySequence(Qt::Key_Right | Qt::CTRL));
    connect(alignRightAction, SIGNAL(triggered()), this, SLOT(alignRight()));
    toolsMB->addAction(alignRightAction);
    _actionMap["align_right"] = alignRightAction;

    QAction* equalizeAction = new QAction(tr("Equalize selection"), this);
    _activateWithSelections.append(equalizeAction);
    Appearance::setActionIcon(equalizeAction, ":/run_environment/graphics/tool/equalize.png");
    equalizeAction->setShortcut(QKeySequence(Qt::Key_Up | Qt::CTRL));
    connect(equalizeAction, SIGNAL(triggered()), this, SLOT(equalize()));
    toolsMB->addAction(equalizeAction);
    _actionMap["equalize"] = equalizeAction;

    toolsMB->addSeparator();

    QAction* glueNotesAction = new QAction(tr("Glue notes (same channel)"), this);
    glueNotesAction->setShortcut(QKeySequence(Qt::Key_G | Qt::CTRL));
    Appearance::setActionIcon(glueNotesAction, ":/run_environment/graphics/tool/glue.png");
    connect(glueNotesAction, SIGNAL(triggered()), this, SLOT(glueSelection()));
    _activateWithSelections.append(glueNotesAction);
    toolsMB->addAction(glueNotesAction);
    _actionMap["glue"] = glueNotesAction;

    QAction* glueNotesAllChannelsAction = new QAction(tr("Glue notes (all channels)"), this);
    glueNotesAllChannelsAction->setShortcut(QKeySequence(Qt::Key_G | Qt::CTRL | Qt::SHIFT));
    Appearance::setActionIcon(glueNotesAllChannelsAction, ":/run_environment/graphics/tool/glue.png");
    connect(glueNotesAllChannelsAction, SIGNAL(triggered()), this, SLOT(glueSelectionAllChannels()));
    _activateWithSelections.append(glueNotesAllChannelsAction);
    toolsMB->addAction(glueNotesAllChannelsAction);
    _actionMap["glue_all_channels"] = glueNotesAllChannelsAction;

    QAction* scissorsAction = new ToolButton(new ScissorsTool(), QKeySequence(Qt::Key_X | Qt::CTRL), toolsMB);
    toolsMB->addAction(scissorsAction);
    _actionMap["scissors"] = scissorsAction;

    QAction* deleteOverlapsAction = new QAction(tr("Delete overlaps"), this);
    deleteOverlapsAction->setShortcut(QKeySequence(Qt::Key_D | Qt::CTRL));
    Appearance::setActionIcon(deleteOverlapsAction, ":/run_environment/graphics/tool/deleteoverlap.png");
    connect(deleteOverlapsAction, SIGNAL(triggered()), this, SLOT(deleteOverlaps()));
    _activateWithSelections.append(deleteOverlapsAction);
    toolsMB->addAction(deleteOverlapsAction);
    _actionMap["delete_overlaps"] = deleteOverlapsAction;

    toolsMB->addSeparator();

    QAction* quantizeAction = new QAction(tr("Quantify selection"), this);
    _activateWithSelections.append(quantizeAction);
    Appearance::setActionIcon(quantizeAction, ":/run_environment/graphics/tool/quantize.png");
    quantizeAction->setShortcut(QKeySequence(Qt::Key_Q | Qt::CTRL));
    connect(quantizeAction, SIGNAL(triggered()), this, SLOT(quantizeSelection()));
    toolsMB->addAction(quantizeAction);
    _actionMap["quantize"] = quantizeAction;

    QMenu* quantMenu = new QMenu(tr("Quantization fractions..."), viewMB);
    QActionGroup* quantGroup = new QActionGroup(viewMB);
    quantGroup->setExclusive(true);

    for (int i = 0; i <= 5; i++) {
        QVariant variant(i);
        QString text = "";

        if (i == 0) {
            text = tr("Whole note");
        } else if (i == 1) {
            text = tr("Half note");
        } else if (i == 2) {
            text = tr("Quarter note");
        } else {
            text = QString::number((int)qPow(2, i)) + tr("th note");
        }

        QAction* a = new QAction(text, this);
        a->setData(variant);
        quantGroup->addAction(a);
        quantMenu->addAction(a);
        a->setCheckable(true);
        a->setChecked(i == _quantizationGrid);
    }
    connect(quantMenu, SIGNAL(triggered(QAction*)), this, SLOT(quantizationChanged(QAction*)));
    toolsMB->addMenu(quantMenu);

    QAction* quantizeNToleAction = new QAction(tr("Quantify tuplet"), this);
    _activateWithSelections.append(quantizeNToleAction);
    quantizeNToleAction->setShortcut(QKeySequence(Qt::Key_H | Qt::CTRL | Qt::SHIFT));
    connect(quantizeNToleAction, SIGNAL(triggered()), this, SLOT(quantizeNtoleDialog()));
    toolsMB->addAction(quantizeNToleAction);

    QAction* quantizeNToleActionRepeat = new QAction(tr("Repeat tuplet quantization"), this);
    _activateWithSelections.append(quantizeNToleActionRepeat);
    quantizeNToleActionRepeat->setShortcut(QKeySequence(Qt::Key_H | Qt::CTRL));
    connect(quantizeNToleActionRepeat, SIGNAL(triggered()), this, SLOT(quantizeNtole()));
    toolsMB->addAction(quantizeNToleActionRepeat);

    toolsMB->addSeparator();

    QAction* transposeAction = new QAction(tr("Transpose selection"), this);
    Appearance::setActionIcon(transposeAction, ":/run_environment/graphics/tool/transpose.png");
    _activateWithSelections.append(transposeAction);
    transposeAction->setShortcut(QKeySequence(Qt::Key_T | Qt::CTRL));
    connect(transposeAction, SIGNAL(triggered()), this, SLOT(transposeNSemitones()));
    toolsMB->addAction(transposeAction);
    _actionMap["transpose"] = transposeAction;

    QAction* transposeOctaveUpAction = new QAction(tr("Transpose octave up"), this);
    Appearance::setActionIcon(transposeOctaveUpAction, ":/run_environment/graphics/tool/transpose_up.png");
    _activateWithSelections.append(transposeOctaveUpAction);
    transposeOctaveUpAction->setShortcut(QKeySequence(Qt::Key_Up | Qt::SHIFT));
    connect(transposeOctaveUpAction, SIGNAL(triggered()), this, SLOT(transposeSelectedNotesOctaveUp()));
    toolsMB->addAction(transposeOctaveUpAction);
    _actionMap["transpose_up"] = transposeOctaveUpAction;

    QAction* transposeOctaveDownAction = new QAction(tr("Transpose octave down"), this);
    Appearance::setActionIcon(transposeOctaveDownAction, ":/run_environment/graphics/tool/transpose_down.png");
    _activateWithSelections.append(transposeOctaveDownAction);
    transposeOctaveDownAction->setShortcut(QKeySequence(Qt::Key_Down | Qt::SHIFT));
    connect(transposeOctaveDownAction, SIGNAL(triggered()), this, SLOT(transposeSelectedNotesOctaveDown()));
    toolsMB->addAction(transposeOctaveDownAction);
    _actionMap["transpose_down"] = transposeOctaveDownAction;

    toolsMB->addSeparator();

    QAction* addTrackAction = new QAction(tr("Add track"), toolsMB);
    toolsMB->addAction(addTrackAction);
    connect(addTrackAction, SIGNAL(triggered()), this, SLOT(addTrack()));

    toolsMB->addSeparator();

    _deleteChannelMenu = new QMenu(tr("Remove events from channel..."), toolsMB);
    toolsMB->addMenu(_deleteChannelMenu);
    connect(_deleteChannelMenu, SIGNAL(triggered(QAction*)), this, SLOT(deleteChannel(QAction*)));

    for (int i = 0; i < 16; i++) {
        QVariant variant(i);
        QAction* delChannelAction = new QAction(QString::number(i), this);
        delChannelAction->setData(variant);
        _deleteChannelMenu->addAction(delChannelAction);
    }

    _moveSelectedEventsToChannelMenu = new QMenu(tr("Move events to channel..."), editMB);
    toolsMB->addMenu(_moveSelectedEventsToChannelMenu);
    connect(_moveSelectedEventsToChannelMenu, SIGNAL(triggered(QAction*)), this, SLOT(moveSelectedEventsToChannel(QAction*)));

    for (int i = 0; i < 16; i++) {
        QVariant variant(i);
        QAction* moveToChannelAction = new QAction(QString::number(i), this);
        moveToChannelAction->setData(variant);
        _moveSelectedEventsToChannelMenu->addAction(moveToChannelAction);
    }

    _moveSelectedEventsToTrackMenu = new QMenu(tr("Move events to track..."), editMB);
    toolsMB->addMenu(_moveSelectedEventsToTrackMenu);
    connect(_moveSelectedEventsToTrackMenu, SIGNAL(triggered(QAction*)), this, SLOT(moveSelectedEventsToTrack(QAction*)));

    toolsMB->addSeparator();

    QAction* setFileLengthMs = new QAction(tr("Set file duration"), this);
    connect(setFileLengthMs, SIGNAL(triggered()), this, SLOT(setFileLengthMs()));
    toolsMB->addAction(setFileLengthMs);

    QAction* scaleSelection = new QAction(tr("Scale events"), this);
    _activateWithSelections.append(scaleSelection);
    connect(scaleSelection, SIGNAL(triggered()), this, SLOT(scaleSelection()));
    toolsMB->addAction(scaleSelection);
    _actionMap["scale_selection"] = scaleSelection;

    toolsMB->addSeparator();

    QAction* magnetAction = new QAction(tr("Magnet"), editMB);
    toolsMB->addAction(magnetAction);
    magnetAction->setShortcut(QKeySequence(Qt::Key_M | Qt::CTRL));
    Appearance::setActionIcon(magnetAction, ":/run_environment/graphics/tool/magnet.png");
    magnetAction->setCheckable(true);
    magnetAction->setChecked(false);
    magnetAction->setChecked(EventTool::magnetEnabled());
    connect(magnetAction, SIGNAL(toggled(bool)), this, SLOT(enableMagnet(bool)));
    _actionMap["magnet"] = magnetAction;

    // View
    QMenu* zoomMenu = new QMenu(tr("Zoom..."), viewMB);
    QAction* zoomHorOutAction = new QAction(tr("Horizontal out"), this);
    zoomHorOutAction->setShortcut(QKeySequence(Qt::Key_Minus | Qt::CTRL));
    Appearance::setActionIcon(zoomHorOutAction, ":/run_environment/graphics/tool/zoom_hor_out.png");
    connect(zoomHorOutAction, SIGNAL(triggered()),
            mw_matrixWidget, SLOT(zoomHorOut()));
    zoomMenu->addAction(zoomHorOutAction);
    _actionMap["zoom_hor_out"] = zoomHorOutAction;

    QAction* zoomHorInAction = new QAction(tr("Horizontal in"), this);
    Appearance::setActionIcon(zoomHorInAction, ":/run_environment/graphics/tool/zoom_hor_in.png");
    zoomHorInAction->setShortcut(QKeySequence(Qt::Key_Equal | Qt::CTRL));
    connect(zoomHorInAction, SIGNAL(triggered()),
            mw_matrixWidget, SLOT(zoomHorIn()));
    zoomMenu->addAction(zoomHorInAction);
    _actionMap["zoom_hor_in"] = zoomHorInAction;

    QAction* zoomVerOutAction = new QAction(tr("Vertical out"), this);
    Appearance::setActionIcon(zoomVerOutAction, ":/run_environment/graphics/tool/zoom_ver_out.png");
    zoomVerOutAction->setShortcut(QKeySequence(Qt::Key_Minus | Qt::SHIFT));
    connect(zoomVerOutAction, SIGNAL(triggered()),
            mw_matrixWidget, SLOT(zoomVerOut()));
    zoomMenu->addAction(zoomVerOutAction);
    _actionMap["zoom_ver_out"] = zoomVerOutAction;

    QAction* zoomVerInAction = new QAction(tr("Vertical in"), this);
    Appearance::setActionIcon(zoomVerInAction, ":/run_environment/graphics/tool/zoom_ver_in.png");
    zoomVerInAction->setShortcut(QKeySequence(Qt::Key_Equal | Qt::SHIFT));
    connect(zoomVerInAction, SIGNAL(triggered()),
            mw_matrixWidget, SLOT(zoomVerIn()));
    zoomMenu->addAction(zoomVerInAction);
    _actionMap["zoom_ver_in"] = zoomVerInAction;

    zoomMenu->addSeparator();

    QAction* zoomStdAction = new QAction(tr("Restore default zoom"), this);
    zoomStdAction->setShortcut(QKeySequence(Qt::Key_Backspace | Qt::CTRL));
    connect(zoomStdAction, SIGNAL(triggered()),
            mw_matrixWidget, SLOT(zoomStd()));
    zoomMenu->addAction(zoomStdAction);

    viewMB->addMenu(zoomMenu);

    viewMB->addSeparator();

    QAction* resetViewAction = new QAction(tr("Reset view"), this);
    resetViewAction->setShortcut(QKeySequence(Qt::Key_Backspace | Qt::CTRL | Qt::SHIFT));
    resetViewAction->setToolTip(tr("Reset zoom, scroll position, and cursor to defaults"));
    connect(resetViewAction, SIGNAL(triggered()), this, SLOT(resetView()));
    viewMB->addAction(resetViewAction);
    _actionMap["reset_view"] = resetViewAction;

    viewMB->addSeparator();

    viewMB->addAction(_allChannelsVisible);
    viewMB->addAction(_allChannelsInvisible);
    viewMB->addAction(_allTracksVisible);
    viewMB->addAction(_allTracksInvisible);

    viewMB->addSeparator();

    QMenu* colorMenu = new QMenu(tr("Colors..."), viewMB);
    _colorsByChannel = new QAction(tr("From channels"), this);
    _colorsByChannel->setCheckable(true);
    connect(_colorsByChannel, SIGNAL(triggered()), this, SLOT(colorsByChannel()));
    colorMenu->addAction(_colorsByChannel);

    _colorsByTracks = new QAction(tr("From tracks"), this);
    _colorsByTracks->setCheckable(true);
    connect(_colorsByTracks, SIGNAL(triggered()), this, SLOT(colorsByTrack()));
    colorMenu->addAction(_colorsByTracks);

    viewMB->addMenu(colorMenu);

    viewMB->addSeparator();

    QMenu* divMenu = new QMenu(tr("Raster..."), viewMB);
    QActionGroup* divGroup = new QActionGroup(viewMB);
    divGroup->setExclusive(true);

    for (int i = -1; i <= 5; i++) {
        QVariant variant(i);
        QString text = tr("Off");
        if (i == 0) {
            text = tr("Whole note");
        } else if (i == 1) {
            text = tr("Half note");
        } else if (i == 2) {
            text = tr("Quarter note");
        } else if (i > 0) {
            text = QString::number((int)qPow(2, i)) + tr("th note");
        }
        QAction* a = new QAction(text, this);
        a->setData(variant);
        divGroup->addAction(a);
        divMenu->addAction(a);
        a->setCheckable(true);
        a->setChecked(i == mw_matrixWidget->div());
    }
    connect(divMenu, SIGNAL(triggered(QAction*)), this, SLOT(divChanged(QAction*)));
    viewMB->addMenu(divMenu);

    // Playback
    QAction* playStopAction = new QAction("PlayStop", this);
    QList<QKeySequence> playStopActionShortcuts;
    playStopActionShortcuts << QKeySequence(Qt::Key_Space)
                            << QKeySequence(Qt::Key_P | Qt::CTRL);
    playStopAction->setShortcuts(playStopActionShortcuts);
    connect(playStopAction, SIGNAL(triggered()), this, SLOT(playStop()));
    playbackMB->addAction(playStopAction);

    QAction* playAction = new QAction(tr("Play"), this);
    Appearance::setActionIcon(playAction, ":/run_environment/graphics/tool/play.png");
    connect(playAction, SIGNAL(triggered()), this, SLOT(play()));
    playbackMB->addAction(playAction);
    _actionMap["play"] = playAction;

    QAction* pauseAction = new QAction(tr("Pause"), this);
    Appearance::setActionIcon(pauseAction, ":/run_environment/graphics/tool/pause.png");
#ifdef Q_OS_MAC
    pauseAction->setShortcut(QKeySequence(Qt::Key_Space | Qt::META));
#else
    pauseAction->setShortcut(QKeySequence(Qt::Key_Space | Qt::CTRL));
#endif
    connect(pauseAction, SIGNAL(triggered()), this, SLOT(pause()));
    playbackMB->addAction(pauseAction);
    _actionMap["pause"] = pauseAction;

    QAction* recAction = new QAction(tr("Record"), this);
    Appearance::setActionIcon(recAction, ":/run_environment/graphics/tool/record.png");
    recAction->setShortcut(QKeySequence(Qt::Key_R | Qt::CTRL));
    connect(recAction, SIGNAL(triggered()), this, SLOT(record()));
    playbackMB->addAction(recAction);
    _actionMap["record"] = recAction;

    QAction* stopAction = new QAction(tr("Stop"), this);
    Appearance::setActionIcon(stopAction, ":/run_environment/graphics/tool/stop.png");
    connect(stopAction, SIGNAL(triggered()), this, SLOT(stop()));
    playbackMB->addAction(stopAction);
    _actionMap["stop"] = stopAction;

    playbackMB->addSeparator();

    QAction* backToBeginAction = new QAction(tr("Back to begin"), this);
    Appearance::setActionIcon(backToBeginAction, ":/run_environment/graphics/tool/back_to_begin.png");
    QList<QKeySequence> backToBeginActionShortcuts;
    backToBeginActionShortcuts << QKeySequence(Qt::Key_Up | Qt::ALT)
                               << QKeySequence(Qt::Key_Home | Qt::ALT)
                               << QKeySequence(Qt::Key_J | Qt::SHIFT);
    backToBeginAction->setShortcuts(backToBeginActionShortcuts);
    connect(backToBeginAction, SIGNAL(triggered()), this, SLOT(backToBegin()));
    playbackMB->addAction(backToBeginAction);
    _actionMap["back_to_begin"] = backToBeginAction;

    QAction* backAction = new QAction(tr("Previous measure"), this);
    Appearance::setActionIcon(backAction, ":/run_environment/graphics/tool/back.png");
    QList<QKeySequence> backActionShortcuts;
    backActionShortcuts << QKeySequence(Qt::Key_Left | Qt::ALT);
    backAction->setShortcuts(backActionShortcuts);
    connect(backAction, SIGNAL(triggered()), this, SLOT(back()));
    playbackMB->addAction(backAction);
    _actionMap["back"] = backAction;

    QAction* forwAction = new QAction(tr("Next measure"), this);
    Appearance::setActionIcon(forwAction, ":/run_environment/graphics/tool/forward.png");
    QList<QKeySequence> forwActionShortcuts;
    forwActionShortcuts << QKeySequence(Qt::Key_Right | Qt::ALT);
    forwAction->setShortcuts(forwActionShortcuts);
    connect(forwAction, SIGNAL(triggered()), this, SLOT(forward()));
    playbackMB->addAction(forwAction);
    _actionMap["forward"] = forwAction;

    playbackMB->addSeparator();

    QAction* backMarkerAction = new QAction(tr("Previous marker"), this);
    Appearance::setActionIcon(backMarkerAction, ":/run_environment/graphics/tool/back_marker.png");
    QList<QKeySequence> backMarkerActionShortcuts;
    backMarkerAction->setShortcut(QKeySequence(Qt::Key_Comma | Qt::ALT));
    connect(backMarkerAction, SIGNAL(triggered()), this, SLOT(backMarker()));
    playbackMB->addAction(backMarkerAction);
    _actionMap["back_marker"] = backMarkerAction;

    QAction* forwMarkerAction = new QAction(tr("Next marker"), this);
    Appearance::setActionIcon(forwMarkerAction, ":/run_environment/graphics/tool/forward_marker.png");
    QList<QKeySequence> forwMarkerActionShortcuts;
    forwMarkerAction->setShortcut(QKeySequence(Qt::Key_Period | Qt::ALT));
    connect(forwMarkerAction, SIGNAL(triggered()), this, SLOT(forwardMarker()));
    playbackMB->addAction(forwMarkerAction);
    _actionMap["forward_marker"] = forwMarkerAction;

    playbackMB->addSeparator();

    QMenu* speedMenu = new QMenu(tr("Playback speed..."));
    connect(speedMenu, SIGNAL(triggered(QAction*)), this, SLOT(setSpeed(QAction*)));

    QList<double> speeds;
    speeds.append(0.25);
    speeds.append(0.5);
    speeds.append(0.75);
    speeds.append(1);
    speeds.append(1.25);
    speeds.append(1.5);
    speeds.append(1.75);
    speeds.append(2);
    QActionGroup* speedGroup = new QActionGroup(this);
    speedGroup->setExclusive(true);

    foreach (double s, speeds) {
        QAction* speedAction = new QAction(QString::number(s), this);
        speedAction->setData(QVariant::fromValue(s));
        speedMenu->addAction(speedAction);
        speedGroup->addAction(speedAction);
        speedAction->setCheckable(true);
        speedAction->setChecked(s == 1);
    }

    playbackMB->addMenu(speedMenu);

    playbackMB->addSeparator();

    playbackMB->addAction(_allChannelsAudible);
    playbackMB->addAction(_allChannelsMute);
    playbackMB->addAction(_allTracksAudible);
    playbackMB->addAction(_allTracksMute);

    playbackMB->addSeparator();

    QAction* lockAction = new QAction(tr("Lock screen while playing"), this);
    Appearance::setActionIcon(lockAction, ":/run_environment/graphics/tool/screen_unlocked.png");
    lockAction->setCheckable(true);
    connect(lockAction, SIGNAL(toggled(bool)), this, SLOT(screenLockPressed(bool)));
    playbackMB->addAction(lockAction);
    lockAction->setChecked(mw_matrixWidget->screenLocked());
    _actionMap["lock"] = lockAction;

    QAction* metronomeAction = new QAction(tr("Metronome"), this);
    Appearance::setActionIcon(metronomeAction, ":/run_environment/graphics/tool/metronome.png");
    metronomeAction->setCheckable(true);
    metronomeAction->setChecked(Metronome::enabled());
    connect(metronomeAction, SIGNAL(toggled(bool)), this, SLOT(enableMetronome(bool)));
    playbackMB->addAction(metronomeAction);
    _actionMap["metronome"] = metronomeAction;

    QAction* pianoEmulationAction = new QAction(tr("Piano emulation"), this);
    pianoEmulationAction->setCheckable(true);
    pianoEmulationAction->setChecked(mw_matrixWidget->getPianoEmulation());
    connect(pianoEmulationAction, SIGNAL(toggled(bool)), this, SLOT(togglePianoEmulation(bool)));
    playbackMB->addAction(pianoEmulationAction);

    // Midi
    QAction* configAction2 = new QAction(tr("Settings"), this);
    Appearance::setActionIcon(configAction2, ":/run_environment/graphics/tool/config.png");
    connect(configAction2, SIGNAL(triggered()), this, SLOT(openConfig()));
    midiMB->addAction(configAction2);

    QAction* thruAction = new QAction(tr("Connect Midi In/Out"), this);
    Appearance::setActionIcon(thruAction, ":/run_environment/graphics/tool/connection.png");
    thruAction->setCheckable(true);
    thruAction->setChecked(MidiInput::thru());
    connect(thruAction, SIGNAL(toggled(bool)), this, SLOT(enableThru(bool)));
    midiMB->addAction(thruAction);
    _actionMap["thru"] = thruAction;

    midiMB->addSeparator();

    QAction* panicAction = new QAction(tr("Midi panic"), this);
    Appearance::setActionIcon(panicAction, ":/run_environment/graphics/tool/panic.png");
    panicAction->setShortcut(QKeySequence(Qt::Key_Escape));
    connect(panicAction, SIGNAL(triggered()), this, SLOT(panic()));
    midiMB->addAction(panicAction);
    _actionMap["panic"] = panicAction;

    // Help
    QAction* aboutAction = new QAction(tr("About MidiEditor"), this);
    connect(aboutAction, SIGNAL(triggered()), this, SLOT(about()));
    helpMB->addAction(aboutAction);

    QAction* manualAction = new QAction(tr("Manual"), this);
    connect(manualAction, SIGNAL(triggered()), this, SLOT(manual()));
    helpMB->addAction(manualAction);

    // Phase 2: Use full custom toolbar with settings integration
    _toolbarWidget = createCustomToolbar(parent);
    return _toolbarWidget;
}

QWidget* MainWindow::createSimpleCustomToolbar(QWidget* parent) {
    // Step 1: Simple custom toolbar with hard-coded safe layout
    // No settings dependencies, no complex logic - just basic customization

    QWidget* buttonBar = new QWidget(parent);
    QGridLayout* btnLayout = new QGridLayout(buttonBar);

    buttonBar->setLayout(btnLayout);
    btnLayout->setSpacing(0);
    buttonBar->setContentsMargins(0, 0, 0, 0);

    QToolBar* toolBar = new QToolBar("Custom", buttonBar);
    toolBar->setFloatable(false);
    toolBar->setContentsMargins(0, 0, 0, 0);
    toolBar->layout()->setSpacing(3);
    int iconSize = Appearance::toolbarIconSize();
    toolBar->setIconSize(QSize(iconSize, iconSize));
    toolBar->setStyleSheet("QToolBar { border: 0px }");

    // Add actions manually to ensure proper layout and functionality
    // This replicates the essential parts of the original toolbar

    // File actions
    if (_actionMap.contains("new")) toolBar->addAction(_actionMap["new"]);

    // Simple open action (recent files menu will be added in later steps)
    if (_actionMap.contains("open")) {
        toolBar->addAction(_actionMap["open"]);
    }

    if (_actionMap.contains("save")) toolBar->addAction(_actionMap["save"]);
    toolBar->addSeparator();

    // Edit actions
    if (_actionMap.contains("undo")) toolBar->addAction(_actionMap["undo"]);
    if (_actionMap.contains("redo")) toolBar->addAction(_actionMap["redo"]);
    toolBar->addSeparator();

    // Tool actions
    if (_actionMap.contains("standard_tool")) toolBar->addAction(_actionMap["standard_tool"]);
    if (_actionMap.contains("new_note")) toolBar->addAction(_actionMap["new_note"]);
    if (_actionMap.contains("copy")) toolBar->addAction(_actionMap["copy"]);

    // Simple paste action (menu will be added in later steps)
    if (_actionMap.contains("paste")) {
        toolBar->addAction(_actionMap["paste"]);
    }

    toolBar->addSeparator();

    // Playback actions
    if (_actionMap.contains("play")) toolBar->addAction(_actionMap["play"]);
    if (_actionMap.contains("pause")) toolBar->addAction(_actionMap["pause"]);
    if (_actionMap.contains("stop")) toolBar->addAction(_actionMap["stop"]);

    btnLayout->setColumnStretch(4, 1);
    btnLayout->addWidget(toolBar, 0, 0, 1, 1);

    return buttonBar;
}

void MainWindow::pasteToChannel(QAction* action) {
    EventTool::setPasteChannel(action->data().toInt());
}

void MainWindow::pasteToTrack(QAction* action) {
    EventTool::setPasteTrack(action->data().toInt());
}

void MainWindow::divChanged(QAction* action) {
    mw_matrixWidget->setDiv(action->data().toInt());
}

void MainWindow::enableMagnet(bool enable) {
    EventTool::enableMagnet(enable);
}

void MainWindow::openConfig() {
    SettingsDialog* d = new SettingsDialog(tr("Settings"), _settings, this);
    connect(d, SIGNAL(settingsChanged()), this, SLOT(updateAll()));
    d->show();
}

void MainWindow::enableMetronome(bool enable) {
    Metronome::setEnabled(enable);
}

void MainWindow::enableThru(bool enable) {
    MidiInput::setThruEnabled(enable);
}

void MainWindow::rebuildToolbar() {
    if (_toolbarWidget) {
        try {
            // Remove the old toolbar
            QWidget* parent = _toolbarWidget->parentWidget();
            if (!parent) {
                return; // No parent, can't rebuild
            }

            _toolbarWidget->setParent(nullptr);
            delete _toolbarWidget;
            _toolbarWidget = nullptr;

            // Create new toolbar
            _toolbarWidget = createCustomToolbar(parent);
            if (!_toolbarWidget) {
                return; // Failed to create toolbar
            }

            // Add it back to the layout
            QGridLayout* layout = qobject_cast<QGridLayout*>(parent->layout());
            if (layout) {
                layout->addWidget(_toolbarWidget, 0, 0);
            }
        } catch (...) {
            // If rebuild fails, create a minimal toolbar
            _toolbarWidget = nullptr; // Ensure it's not left dangling
            try {
                // Try to get the parent from the central widget
                QWidget* central = centralWidget();
                if (central) {
                    _toolbarWidget = new QWidget(central);
                    QGridLayout* layout = qobject_cast<QGridLayout*>(central->layout());
                    if (layout) {
                        layout->addWidget(_toolbarWidget, 0, 0);
                    }
                }
            } catch (...) {
                // If even that fails, just leave _toolbarWidget as nullptr
                _toolbarWidget = nullptr;
            }
        }
    }
}

QAction* MainWindow::getActionById(const QString& actionId) {
    return _actionMap.value(actionId, nullptr);
}

QWidget* MainWindow::createCustomToolbar(QWidget* parent) {
    QWidget* buttonBar = new QWidget(parent);
    QGridLayout* btnLayout = new QGridLayout(buttonBar);

    buttonBar->setLayout(btnLayout);
    btnLayout->setSpacing(0);
    buttonBar->setContentsMargins(0, 0, 0, 0);

    // Safety check - if Appearance is not initialized, create a simple toolbar
    bool twoRowMode = false;
    QStringList actionOrder;
    QStringList enabledActions;

    bool customizeEnabled = false;
    try {
        twoRowMode = Appearance::toolbarTwoRowMode();
        customizeEnabled = Appearance::toolbarCustomizeEnabled();
        actionOrder = Appearance::toolbarActionOrder();
        enabledActions = Appearance::toolbarEnabledActions();
    } catch (...) {
        // If there's any issue with settings, use safe defaults
        twoRowMode = false;
        customizeEnabled = false;
        actionOrder.clear();
        enabledActions.clear();
    }

    // If no custom order is set, use default based on row mode
    if (actionOrder.isEmpty()) {
        // These are the old defaults that include essential actions - they cause duplicates
        // We should only use the new defaults that don't include essential actions
        actionOrder.clear(); // Force use of new defaults
    }

    // If no enabled actions are set, enable all by default
    if (enabledActions.isEmpty()) {
        for (const QString& actionId : actionOrder) {
            if (!actionId.startsWith("separator") && actionId != "row_separator") {
                enabledActions << actionId;
            }
        }
    }

    // Essential actions that can't be disabled (only for single row mode)
    QStringList essentialActions;
    if (!twoRowMode) {
        essentialActions << "new" << "open" << "save" << "separator1" << "undo" << "redo";
    }

    // Use custom settings only if customization is enabled, otherwise use defaults
    if (!customizeEnabled || actionOrder.isEmpty()) {
        if (twoRowMode) {
            // Two-row default layout: editing tools on top, playback + view on bottom
            actionOrder << "separator2" // First separator after essential icons
                       << "standard_tool" << "select_left" << "select_right" << "separator3"
                       << "new_note" << "remove_notes" << "copy" << "paste" << "separator4"
                       << "glue" << "scissors" << "delete_overlaps" << "separator5"
                       << "align_left" << "equalize" << "align_right" << "separator6"
                       << "quantize" << "magnet" << "separator7"
                       << "measure" << "time_signature" << "tempo"
                       << "row_separator" // Split point for second row
                       << "separator8" // First separator in second row
                       << "back_to_begin" << "back_marker" << "back" << "play" << "pause"
                       << "stop" << "record" << "forward" << "forward_marker" << "separator9"
                       << "metronome" << "separator10"
                       << "zoom_hor_in" << "zoom_hor_out" << "zoom_ver_in" << "zoom_ver_out"
                       << "lock" << "separator11" << "thru";
        } else {
            // Single-row default layout: start after essential actions
            actionOrder << "separator2" // First separator after essential icons (redo)
                       << "standard_tool" << "select_left" << "select_right" << "separator3"
                       << "new_note" << "remove_notes" << "copy" << "paste" << "separator4"
                       << "glue" << "scissors" << "delete_overlaps" << "separator5"
                       << "back_to_begin" << "back_marker" << "back" << "play" << "pause"
                       << "stop" << "record" << "forward" << "forward_marker" << "separator6"
                       << "metronome" << "align_left" << "equalize" << "align_right" << "separator7"
                       << "zoom_hor_in" << "zoom_hor_out" << "zoom_ver_in" << "zoom_ver_out"
                       << "lock" << "separator8" << "quantize" << "magnet" << "separator9"
                       << "thru" << "separator10" << "measure" << "time_signature" << "tempo";
        }

        // Enable all non-separator actions by default
        for (const QString& actionId : actionOrder) {
            if (!actionId.startsWith("separator") && actionId != "row_separator") {
                enabledActions << actionId;
            }
        }
    }

    // Only prepend essential actions for single row mode
    if (!twoRowMode) {
        QStringList finalActionOrder = essentialActions;
        finalActionOrder.append(actionOrder);
        actionOrder = finalActionOrder;
    }

    // Always enable essential actions
    for (const QString& actionId : essentialActions) {
        if (!actionId.startsWith("separator") && !enabledActions.contains(actionId)) {
            enabledActions << actionId;
        }
    }

    int iconSize = Appearance::toolbarIconSize();

    if (twoRowMode) {
        // Create three separate toolbars: essential (larger), top row, bottom row
        QToolBar* essentialToolBar = new QToolBar("Essential", buttonBar);
        QToolBar* topToolBar = new QToolBar("Top", buttonBar);
        QToolBar* bottomToolBar = new QToolBar("Bottom", buttonBar);

        // Essential toolbar setup (larger icons)
        int essentialIconSize = iconSize + 8;
        essentialToolBar->setFloatable(false);
        essentialToolBar->setContentsMargins(0, 0, 0, 0);
        essentialToolBar->layout()->setSpacing(3);
        essentialToolBar->setIconSize(QSize(essentialIconSize, essentialIconSize));
        essentialToolBar->setStyleSheet("QToolBar { border: 0px }");
        essentialToolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);

        // Top toolbar setup
        topToolBar->setFloatable(false);
        topToolBar->setContentsMargins(0, 0, 0, 0);
        topToolBar->layout()->setSpacing(3);
        topToolBar->setIconSize(QSize(iconSize, iconSize));
        topToolBar->setStyleSheet("QToolBar { border: 0px }");
        topToolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
        topToolBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

        // Bottom toolbar setup
        bottomToolBar->setFloatable(false);
        bottomToolBar->setContentsMargins(0, 0, 0, 0);
        bottomToolBar->layout()->setSpacing(3);
        bottomToolBar->setIconSize(QSize(iconSize, iconSize));
        bottomToolBar->setStyleSheet("QToolBar { border: 0px }");
        bottomToolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
        bottomToolBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

        QToolBar* currentToolBar = topToolBar;

        // First, add essential actions to essential toolbar
        QStringList essentialActions;
        essentialActions << "new" << "open" << "save" << "undo" << "redo";

        for (const QString& actionId : essentialActions) {
            QAction* action = getActionById(actionId);
            if (action) {
                // Special handling for open action to add recent files menu
                if (actionId == "open") {
                    QAction* openWithRecentAction = new QAction(action->text(), essentialToolBar);
                    openWithRecentAction->setIcon(action->icon());
                    openWithRecentAction->setShortcut(action->shortcut());
                    openWithRecentAction->setToolTip(action->toolTip());
                    connect(openWithRecentAction, SIGNAL(triggered()), this, SLOT(load()));
                    if (_recentPathsMenu) {
                        openWithRecentAction->setMenu(_recentPathsMenu);
                    }
                    action = openWithRecentAction;
                }

                essentialToolBar->addAction(action);

                // Set text under icon for essential actions
                QWidget* toolButton = essentialToolBar->widgetForAction(action);
                if (QToolButton* button = qobject_cast<QToolButton*>(toolButton)) {
                    button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
                }
            }
        }

        // Add actions to appropriate toolbar
        for (const QString& actionId : actionOrder) {
            // Essential actions are already handled above, skip them here
            if (actionId == "new" || actionId == "open" || actionId == "save" ||
                actionId == "undo" || actionId == "redo") {
                continue;
            }

            if (actionId == "row_separator") {
                currentToolBar = bottomToolBar;
                continue;
            }

            if (actionId.startsWith("separator")) {
                // Always add the first separator in each row (separator2 and separator8) to separate from essential toolbar
                if (actionId == "separator2" || actionId == "separator8") {
                    currentToolBar->addSeparator();
                } else {
                    // For other separators, only add if there are actions and the last action isn't already a separator
                    if (currentToolBar->actions().count() > 0) {
                        QAction* lastAction = currentToolBar->actions().last();
                        if (!lastAction->isSeparator()) {
                            currentToolBar->addSeparator();
                        }
                    }
                }
                continue;
            }

            // Skip disabled actions only if customization is enabled
            if (customizeEnabled && !enabledActions.isEmpty() && !enabledActions.contains(actionId)) {
                continue; // Skip disabled actions
            }

            // Use current toolbar for all non-essential actions

            QAction* action = getActionById(actionId);

            // Special handling for open action to add recent files menu (for non-essential open actions)
            if (actionId == "open" && action) {
                // Create a new action with the recent files menu for the toolbar
                QAction* openWithRecentAction = new QAction(action->text(), currentToolBar);
                openWithRecentAction->setIcon(action->icon());
                openWithRecentAction->setShortcut(action->shortcut());
                openWithRecentAction->setToolTip(action->toolTip());
                connect(openWithRecentAction, SIGNAL(triggered()), this, SLOT(load()));
                if (_recentPathsMenu) {
                    openWithRecentAction->setMenu(_recentPathsMenu);
                }
                action = openWithRecentAction;
            }

            if (!action) {
                // Handle special actions that need custom creation
                if (actionId == "open") {
                    // Create special open action with recent files menu
                    action = new QAction(tr("Open..."), currentToolBar);
                    Appearance::setActionIcon(action, ":/run_environment/graphics/tool/load.png");
                    connect(action, SIGNAL(triggered()), this, SLOT(load()));
                    if (_recentPathsMenu) {
                        action->setMenu(_recentPathsMenu);
                    }
                } else if (actionId == "paste") {
                    // Create special paste action with options menu
                    action = new QAction(tr("Paste events"), currentToolBar);
                    action->setToolTip(tr("Paste events at cursor position"));
                    Appearance::setActionIcon(action, ":/run_environment/graphics/tool/paste.png");
                    connect(action, SIGNAL(triggered()), this, SLOT(paste()));
                    if (_pasteOptionsMenu) {
                        action->setMenu(_pasteOptionsMenu);
                    }
                } else {
                    // Create a placeholder action if the real one doesn't exist
                    action = new QAction(actionId, currentToolBar);
                    action->setEnabled(false);
                    action->setToolTip("Action not yet implemented: " + actionId);

                    // Try to find the icon path for this action from our default list
                    try {
                        QList<ToolbarActionInfo> defaultActions = getDefaultActionsForPlaceholder();
                        for (const ToolbarActionInfo& info : defaultActions) {
                            if (info.id == actionId && !info.iconPath.isEmpty()) {
                                Appearance::setActionIcon(action, info.iconPath);
                                break;
                            }
                        }
                    } catch (...) {
                        // If icon setting fails, just continue without icon
                    }
                }
            }

            if (action) {
                try {
                    currentToolBar->addAction(action);
                } catch (...) {
                    // If adding action fails, skip it
                }
            }
        }

        // Layout: Essential toolbar on left, content toolbars stacked on right
        btnLayout->setColumnStretch(1, 1);
        btnLayout->setColumnMinimumWidth(1, 400); // Ensure content toolbars have minimum width
        btnLayout->addWidget(essentialToolBar, 0, 0, 2, 1); // Spans both rows
        btnLayout->addWidget(topToolBar, 0, 1, 1, 1);
        btnLayout->addWidget(bottomToolBar, 1, 1, 1, 1);

    } else {
        // Single-row mode
        QToolBar* toolBar = new QToolBar("Main", buttonBar);
        toolBar->setFloatable(false);
        toolBar->setContentsMargins(0, 0, 0, 0);
        toolBar->layout()->setSpacing(3);
        toolBar->setIconSize(QSize(iconSize, iconSize));
        toolBar->setStyleSheet("QToolBar { border: 0px }");

        // Add actions to toolbar based on order and enabled state
        for (const QString& actionId : actionOrder) {
            if (actionId.startsWith("separator") || actionId == "row_separator") {
                if (actionId != "row_separator") {
                    // Only add separator if there are actions in the toolbar and the last action isn't already a separator
                    if (toolBar->actions().count() > 0) {
                        QAction* lastAction = toolBar->actions().last();
                        if (!lastAction->isSeparator()) {
                            toolBar->addSeparator();
                        }
                    }
                }
                continue;
            }

            // Skip disabled actions only if customization is enabled
            if (customizeEnabled && !enabledActions.isEmpty() && !enabledActions.contains(actionId)) {
                continue; // Skip disabled actions
            }

            QAction* action = getActionById(actionId);

            // Special handling for open action to add recent files menu
            if (actionId == "open" && action) {
                // Create a new action with the recent files menu for the toolbar
                QAction* openWithRecentAction = new QAction(action->text(), toolBar);
                openWithRecentAction->setIcon(action->icon());
                openWithRecentAction->setShortcut(action->shortcut());
                openWithRecentAction->setToolTip(action->toolTip());
                connect(openWithRecentAction, SIGNAL(triggered()), this, SLOT(load()));
                if (_recentPathsMenu) {
                    openWithRecentAction->setMenu(_recentPathsMenu);
                }
                action = openWithRecentAction;
            }

            if (!action) {
                // Handle special actions that need custom creation
                if (actionId == "open") {
                    // Create special open action with recent files menu
                    action = new QAction(tr("Open..."), toolBar);
                    Appearance::setActionIcon(action, ":/run_environment/graphics/tool/load.png");
                    connect(action, SIGNAL(triggered()), this, SLOT(load()));
                    if (_recentPathsMenu) {
                        action->setMenu(_recentPathsMenu);
                    }
                } else if (actionId == "paste") {
                    // Create special paste action with options menu
                    action = new QAction(tr("Paste events"), toolBar);
                    action->setToolTip(tr("Paste events at cursor position"));
                    Appearance::setActionIcon(action, ":/run_environment/graphics/tool/paste.png");
                    connect(action, SIGNAL(triggered()), this, SLOT(paste()));
                    if (_pasteOptionsMenu) {
                        action->setMenu(_pasteOptionsMenu);
                    }
                } else {
                    // Create a placeholder action if the real one doesn't exist
                    action = new QAction(actionId, toolBar);
                    action->setEnabled(false);
                    action->setToolTip("Action not yet implemented: " + actionId);

                    // Try to find the icon path for this action from our default list
                    try {
                        QList<ToolbarActionInfo> defaultActions = getDefaultActionsForPlaceholder();
                        for (const ToolbarActionInfo& info : defaultActions) {
                            if (info.id == actionId && !info.iconPath.isEmpty()) {
                                Appearance::setActionIcon(action, info.iconPath);
                                break;
                            }
                        }
                    } catch (...) {
                        // If icon setting fails, just continue without icon
                    }
                }
            }

            if (action) {
                try {
                    toolBar->addAction(action);

                    // In two-row mode, add text labels only for essential actions
                    if (twoRowMode && (actionId == "new" || actionId == "open" || actionId == "save" ||
                                      actionId == "undo" || actionId == "redo")) {
                        // Set the toolbar style for this specific action
                        QWidget* toolButton = toolBar->widgetForAction(action);
                        if (QToolButton* button = qobject_cast<QToolButton*>(toolButton)) {
                            button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
                            // Make essential action icons slightly larger
                            button->setIconSize(QSize(iconSize + 4, iconSize + 4));
                        }
                    }
                } catch (...) {
                    // If adding action fails, skip it
                }
            }
        }

        btnLayout->setColumnStretch(4, 1);
        btnLayout->addWidget(toolBar, 0, 0, 1, 1);
    }

    return buttonBar;
}

void MainWindow::updateToolbarContents(QWidget* toolbarWidget, QGridLayout* btnLayout) {
    // This method updates the contents of an existing toolbar widget without replacing it
    // It uses the same logic as createCustomToolbar but works with an existing widget

    btnLayout->setSpacing(0);
    toolbarWidget->setContentsMargins(0, 0, 0, 0);

    // Get current settings
    bool twoRowMode = false;
    QStringList actionOrder;
    QStringList enabledActions;
    bool customizeEnabled = false;

    try {
        twoRowMode = Appearance::toolbarTwoRowMode();
        customizeEnabled = Appearance::toolbarCustomizeEnabled();
        actionOrder = Appearance::toolbarActionOrder();
        enabledActions = Appearance::toolbarEnabledActions();
    } catch (...) {
        // If there's any issue with settings, use safe defaults
        twoRowMode = false;
        customizeEnabled = false;
        actionOrder.clear();
        enabledActions.clear();
    }

    // Essential actions that can't be disabled (only for single row mode)
    QStringList essentialActions;
    if (!twoRowMode) {
        essentialActions << "new" << "open" << "save" << "separator1" << "undo" << "redo";
    }

    // Use custom settings only if customization is enabled, otherwise use defaults
    if (!customizeEnabled || actionOrder.isEmpty()) {
        if (twoRowMode) {
            // Two-row default layout: editing tools on top, playback + view on bottom
            actionOrder << "separator2" // First separator after essential icons
                       << "standard_tool" << "select_left" << "select_right" << "separator3"
                       << "new_note" << "remove_notes" << "copy" << "paste" << "separator4"
                       << "glue" << "scissors" << "delete_overlaps" << "separator5"
                       << "align_left" << "equalize" << "align_right" << "separator6"
                       << "quantize" << "magnet" << "separator7"
                       << "measure" << "time_signature" << "tempo"
                       << "row_separator" // Split point for second row
                       << "separator8" // First separator in second row
                       << "back_to_begin" << "back_marker" << "back" << "play" << "pause"
                       << "stop" << "record" << "forward" << "forward_marker" << "separator9"
                       << "metronome" << "separator10"
                       << "zoom_hor_in" << "zoom_hor_out" << "zoom_ver_in" << "zoom_ver_out"
                       << "lock" << "separator11" << "thru";
        } else {
            // Single-row default layout: start after essential actions
            actionOrder << "separator2" // First separator after essential icons (redo)
                       << "standard_tool" << "select_left" << "select_right" << "separator3"
                       << "new_note" << "remove_notes" << "copy" << "paste" << "separator4"
                       << "glue" << "scissors" << "delete_overlaps" << "separator5"
                       << "back_to_begin" << "back_marker" << "back" << "play" << "pause"
                       << "stop" << "record" << "forward" << "forward_marker" << "separator6"
                       << "metronome" << "align_left" << "equalize" << "align_right" << "separator7"
                       << "zoom_hor_in" << "zoom_hor_out" << "zoom_ver_in" << "zoom_ver_out"
                       << "lock" << "separator8" << "quantize" << "magnet" << "separator9"
                       << "thru" << "separator10" << "measure" << "time_signature" << "tempo";
        }

        // Enable all non-separator actions by default
        for (const QString& actionId : actionOrder) {
            if (!actionId.startsWith("separator") && actionId != "row_separator") {
                enabledActions << actionId;
            }
        }
    }

    // Only prepend essential actions for single row mode
    if (!twoRowMode) {
        QStringList finalActionOrder = essentialActions;
        finalActionOrder.append(actionOrder);
        actionOrder = finalActionOrder;
    }

    // Always enable essential actions
    for (const QString& actionId : essentialActions) {
        if (!actionId.startsWith("separator") && !enabledActions.contains(actionId)) {
            enabledActions << actionId;
        }
    }

    int iconSize = Appearance::toolbarIconSize();

    if (twoRowMode) {
        // Create three separate toolbars: essential (larger), top row, bottom row
        QToolBar* essentialToolBar = new QToolBar("Essential", toolbarWidget);
        QToolBar* topToolBar = new QToolBar("Top", toolbarWidget);
        QToolBar* bottomToolBar = new QToolBar("Bottom", toolbarWidget);

        // Essential toolbar setup (larger icons)
        int essentialIconSize = iconSize + 8;
        essentialToolBar->setFloatable(false);
        essentialToolBar->setContentsMargins(0, 0, 0, 0);
        essentialToolBar->layout()->setSpacing(3);
        essentialToolBar->setIconSize(QSize(essentialIconSize, essentialIconSize));
        essentialToolBar->setStyleSheet("QToolBar { border: 0px }");
        essentialToolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);

        // Top toolbar setup
        topToolBar->setFloatable(false);
        topToolBar->setContentsMargins(0, 0, 0, 0);
        topToolBar->layout()->setSpacing(3);
        topToolBar->setIconSize(QSize(iconSize, iconSize));
        topToolBar->setStyleSheet("QToolBar { border: 0px }");
        topToolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
        topToolBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

        // Bottom toolbar setup
        bottomToolBar->setFloatable(false);
        bottomToolBar->setContentsMargins(0, 0, 0, 0);
        bottomToolBar->layout()->setSpacing(3);
        bottomToolBar->setIconSize(QSize(iconSize, iconSize));
        bottomToolBar->setStyleSheet("QToolBar { border: 0px }");
        bottomToolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
        bottomToolBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

        QToolBar* currentToolBar = topToolBar;

        // First, add essential actions to essential toolbar
        QStringList essentialActionsList;
        essentialActionsList << "new" << "open" << "save" << "undo" << "redo";

        for (const QString& actionId : essentialActionsList) {
            QAction* action = getActionById(actionId);
            if (action) {
                // Special handling for open action to add recent files menu
                if (actionId == "open") {
                    QAction* openWithRecentAction = new QAction(action->text(), essentialToolBar);
                    openWithRecentAction->setIcon(action->icon());
                    openWithRecentAction->setShortcut(action->shortcut());
                    openWithRecentAction->setToolTip(action->toolTip());
                    connect(openWithRecentAction, SIGNAL(triggered()), this, SLOT(load()));
                    if (_recentPathsMenu) {
                        openWithRecentAction->setMenu(_recentPathsMenu);
                    }
                    action = openWithRecentAction;
                }

                essentialToolBar->addAction(action);

                // Set text under icon for essential actions
                QWidget* toolButton = essentialToolBar->widgetForAction(action);
                if (QToolButton* button = qobject_cast<QToolButton*>(toolButton)) {
                    button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
                }
            }
        }

        // Add actions to appropriate toolbar
        for (const QString& actionId : actionOrder) {
            // Essential actions are already handled above, skip them here
            if (actionId == "new" || actionId == "open" || actionId == "save" ||
                actionId == "undo" || actionId == "redo") {
                continue;
            }

            if (actionId == "row_separator") {
                currentToolBar = bottomToolBar;
                continue;
            }

            if (actionId.startsWith("separator")) {
                // Always add the first separator in each row (separator2 and separator8) to separate from essential toolbar
                if (actionId == "separator2" || actionId == "separator8") {
                    currentToolBar->addSeparator();
                } else {
                    // For other separators, only add if there are actions and the last action isn't already a separator
                    if (currentToolBar->actions().count() > 0) {
                        QAction* lastAction = currentToolBar->actions().last();
                        if (!lastAction->isSeparator()) {
                            currentToolBar->addSeparator();
                        }
                    }
                }
                continue;
            }

            // Skip disabled actions only if customization is enabled
            if (customizeEnabled && !enabledActions.isEmpty() && !enabledActions.contains(actionId)) {
                continue; // Skip disabled actions
            }

            // Use current toolbar for all non-essential actions

            QAction* action = getActionById(actionId);

            // Special handling for open action to add recent files menu (for non-essential open actions)
            if (actionId == "open" && action) {
                // Create a new action with the recent files menu for the toolbar
                QAction* openWithRecentAction = new QAction(action->text(), currentToolBar);
                openWithRecentAction->setIcon(action->icon());
                openWithRecentAction->setShortcut(action->shortcut());
                openWithRecentAction->setToolTip(action->toolTip());
                connect(openWithRecentAction, SIGNAL(triggered()), this, SLOT(load()));
                if (_recentPathsMenu) {
                    openWithRecentAction->setMenu(_recentPathsMenu);
                }
                action = openWithRecentAction;
            }

            if (!action) {
                // Handle special actions that need custom creation
                if (actionId == "open") {
                    // Create special open action with recent files menu
                    action = new QAction(tr("Open..."), currentToolBar);
                    Appearance::setActionIcon(action, ":/run_environment/graphics/tool/load.png");
                    connect(action, SIGNAL(triggered()), this, SLOT(load()));
                    if (_recentPathsMenu) {
                        action->setMenu(_recentPathsMenu);
                    }
                } else if (actionId == "paste") {
                    // Create paste action if not found
                    action = new QAction(tr("Paste"), currentToolBar);
                    Appearance::setActionIcon(action, ":/run_environment/graphics/tool/paste.png");
                    connect(action, SIGNAL(triggered()), this, SLOT(paste()));
                }
            }

            if (action) {
                try {
                    currentToolBar->addAction(action);
                } catch (...) {
                    // If adding action fails, skip it
                }
            }
        }

        // Layout: Essential toolbar on left, content toolbars stacked on right
        btnLayout->setColumnStretch(1, 1);
        btnLayout->setColumnMinimumWidth(1, 400); // Ensure content toolbars have minimum width
        btnLayout->addWidget(essentialToolBar, 0, 0, 2, 1); // Spans both rows
        btnLayout->addWidget(topToolBar, 0, 1, 1, 1);
        btnLayout->addWidget(bottomToolBar, 1, 1, 1, 1);

    } else {
        // Single-row mode: Create one toolbar
        QToolBar* toolBar = new QToolBar("Main", toolbarWidget);
        toolBar->setFloatable(false);
        toolBar->setContentsMargins(0, 0, 0, 0);
        toolBar->layout()->setSpacing(3);
        toolBar->setIconSize(QSize(iconSize, iconSize));
        toolBar->setStyleSheet("QToolBar { border: 0px }");

        // Add actions to toolbar based on order and enabled state
        for (const QString& actionId : actionOrder) {
            if (actionId.startsWith("separator") || actionId == "row_separator") {
                if (actionId != "row_separator") {
                    // Only add separator if there are actions in the toolbar and the last action isn't already a separator
                    if (toolBar->actions().count() > 0) {
                        QAction* lastAction = toolBar->actions().last();
                        if (!lastAction->isSeparator()) {
                            toolBar->addSeparator();
                        }
                    }
                }
                continue;
            }

            // Skip disabled actions only if customization is enabled
            if (customizeEnabled && !enabledActions.isEmpty() && !enabledActions.contains(actionId)) {
                continue; // Skip disabled actions
            }

            QAction* action = getActionById(actionId);

            // Special handling for open action to add recent files menu
            if (actionId == "open" && action) {
                // Create a new action with the recent files menu for the toolbar
                QAction* openWithRecentAction = new QAction(action->text(), toolBar);
                openWithRecentAction->setIcon(action->icon());
                openWithRecentAction->setShortcut(action->shortcut());
                openWithRecentAction->setToolTip(action->toolTip());
                connect(openWithRecentAction, SIGNAL(triggered()), this, SLOT(load()));
                if (_recentPathsMenu) {
                    openWithRecentAction->setMenu(_recentPathsMenu);
                }
                action = openWithRecentAction;
            }

            if (!action) {
                // Handle special actions that need custom creation
                if (actionId == "open") {
                    // Create special open action with recent files menu
                    action = new QAction(tr("Open..."), toolBar);
                    Appearance::setActionIcon(action, ":/run_environment/graphics/tool/load.png");
                    connect(action, SIGNAL(triggered()), this, SLOT(load()));
                    if (_recentPathsMenu) {
                        action->setMenu(_recentPathsMenu);
                    }
                } else if (actionId == "paste") {
                    // Create paste action if not found
                    action = new QAction(tr("Paste"), toolBar);
                    Appearance::setActionIcon(action, ":/run_environment/graphics/tool/paste.png");
                    connect(action, SIGNAL(triggered()), this, SLOT(paste()));
                }
            }

            if (action) {
                try {
                    toolBar->addAction(action);
                } catch (...) {
                    // If adding action fails, skip it
                }
            }
        }

        btnLayout->setColumnStretch(4, 1);
        btnLayout->addWidget(toolBar, 0, 0, 1, 1);
    }
}

QList<ToolbarActionInfo> MainWindow::getDefaultActionsForPlaceholder() {
    // This is a simplified version of the default actions list for placeholder icons
    // We include this here to avoid circular dependencies with LayoutSettingsWidget
    QList<ToolbarActionInfo> actions;

    actions << ToolbarActionInfo{"new", "New", ":/run_environment/graphics/tool/new.png", nullptr, true, true, "File"};
    actions << ToolbarActionInfo{"open", "Open", ":/run_environment/graphics/tool/load.png", nullptr, true, true, "File"};
    actions << ToolbarActionInfo{"save", "Save", ":/run_environment/graphics/tool/save.png", nullptr, true, true, "File"};
    actions << ToolbarActionInfo{"undo", "Undo", ":/run_environment/graphics/tool/undo.png", nullptr, true, true, "Edit"};
    actions << ToolbarActionInfo{"redo", "Redo", ":/run_environment/graphics/tool/redo.png", nullptr, true, true, "Edit"};
    actions << ToolbarActionInfo{"standard_tool", "Standard Tool", ":/run_environment/graphics/tool/select.png", nullptr, true, false, "Tools"};
    actions << ToolbarActionInfo{"select_left", "Select Left", ":/run_environment/graphics/tool/select_left.png", nullptr, true, false, "Tools"};
    actions << ToolbarActionInfo{"select_right", "Select Right", ":/run_environment/graphics/tool/select_right.png", nullptr, true, false, "Tools"};
    actions << ToolbarActionInfo{"new_note", "New Note", ":/run_environment/graphics/tool/newnote.png", nullptr, true, false, "Edit"};
    actions << ToolbarActionInfo{"remove_notes", "Remove Notes", ":/run_environment/graphics/tool/eraser.png", nullptr, true, false, "Edit"};
    actions << ToolbarActionInfo{"copy", "Copy", ":/run_environment/graphics/tool/copy.png", nullptr, true, false, "Edit"};
    actions << ToolbarActionInfo{"paste", "Paste", ":/run_environment/graphics/tool/paste.png", nullptr, true, false, "Edit"};
    actions << ToolbarActionInfo{"glue", "Glue Notes (Same Channel)", ":/run_environment/graphics/tool/glue.png", nullptr, true, false, "Tools"};
    actions << ToolbarActionInfo{"glue_all_channels", "Glue Notes (All Channels)", ":/run_environment/graphics/tool/glue.png", nullptr, true, false, "Tools"};
    actions << ToolbarActionInfo{"scissors", "Scissors", ":/run_environment/graphics/tool/scissors.png", nullptr, true, false, "Tools"};
    actions << ToolbarActionInfo{"delete_overlaps", "Delete Overlaps", ":/run_environment/graphics/tool/deleteoverlap.png", nullptr, true, false, "Tools"};
    actions << ToolbarActionInfo{"size_change", "Size Change", ":/run_environment/graphics/tool/change_size.png", nullptr, true, false, "Tools"};
    actions << ToolbarActionInfo{"back_to_begin", "Back to Begin", ":/run_environment/graphics/tool/back_to_begin.png", nullptr, true, false, "Playback"};
    actions << ToolbarActionInfo{"back_marker", "Back Marker", ":/run_environment/graphics/tool/back_marker.png", nullptr, true, false, "Playback"};
    actions << ToolbarActionInfo{"back", "Back", ":/run_environment/graphics/tool/back.png", nullptr, true, false, "Playback"};
    actions << ToolbarActionInfo{"play", "Play", ":/run_environment/graphics/tool/play.png", nullptr, true, false, "Playback"};
    actions << ToolbarActionInfo{"pause", "Pause", ":/run_environment/graphics/tool/pause.png", nullptr, true, false, "Playback"};
    actions << ToolbarActionInfo{"stop", "Stop", ":/run_environment/graphics/tool/stop_record.png", nullptr, true, false, "Playback"};
    actions << ToolbarActionInfo{"record", "Record", ":/run_environment/graphics/tool/record.png", nullptr, true, false, "Playback"};
    actions << ToolbarActionInfo{"forward", "Forward", ":/run_environment/graphics/tool/forward.png", nullptr, true, false, "Playback"};
    actions << ToolbarActionInfo{"forward_marker", "Forward Marker", ":/run_environment/graphics/tool/forward_marker.png", nullptr, true, false, "Playback"};
    actions << ToolbarActionInfo{"metronome", "Metronome", ":/run_environment/graphics/tool/metronome.png", nullptr, true, false, "Playback"};
    actions << ToolbarActionInfo{"align_left", "Align Left", ":/run_environment/graphics/tool/align_left.png", nullptr, true, false, "Tools"};
    actions << ToolbarActionInfo{"equalize", "Equalize", ":/run_environment/graphics/tool/equalize.png", nullptr, true, false, "Tools"};
    actions << ToolbarActionInfo{"align_right", "Align Right", ":/run_environment/graphics/tool/align_right.png", nullptr, true, false, "Tools"};
    actions << ToolbarActionInfo{"zoom_hor_in", "Zoom Horizontal In", ":/run_environment/graphics/tool/zoom_hor_in.png", nullptr, true, false, "View"};
    actions << ToolbarActionInfo{"zoom_hor_out", "Zoom Horizontal Out", ":/run_environment/graphics/tool/zoom_hor_out.png", nullptr, true, false, "View"};
    actions << ToolbarActionInfo{"zoom_ver_in", "Zoom Vertical In", ":/run_environment/graphics/tool/zoom_ver_in.png", nullptr, true, false, "View"};
    actions << ToolbarActionInfo{"zoom_ver_out", "Zoom Vertical Out", ":/run_environment/graphics/tool/zoom_ver_out.png", nullptr, true, false, "View"};
    actions << ToolbarActionInfo{"lock", "Lock Screen", ":/run_environment/graphics/tool/screen_unlocked.png", nullptr, true, false, "View"};
    actions << ToolbarActionInfo{"quantize", "Quantize", ":/run_environment/graphics/tool/quantize.png", nullptr, true, false, "Tools"};
    actions << ToolbarActionInfo{"magnet", "Magnet", ":/run_environment/graphics/tool/magnet.png", nullptr, true, false, "Tools"};
    actions << ToolbarActionInfo{"thru", "MIDI Thru", ":/run_environment/graphics/tool/connection.png", nullptr, true, false, "MIDI"};
    actions << ToolbarActionInfo{"measure", "Measure", ":/run_environment/graphics/tool/measure.png", nullptr, true, false, "View"};
    actions << ToolbarActionInfo{"time_signature", "Time Signature", ":/run_environment/graphics/tool/meter.png", nullptr, true, false, "View"};
    actions << ToolbarActionInfo{"tempo", "Tempo", ":/run_environment/graphics/tool/tempo.png", nullptr, true, false, "View"};

    return actions;
}

void MainWindow::togglePianoEmulation(bool mode) {
    mw_matrixWidget->setPianoEmulation(mode);
}

void MainWindow::quantizationChanged(QAction* action) {
    _quantizationGrid = action->data().toInt();
}

void MainWindow::quantizeSelection() {

    if (!file) {
        return;
    }

    // get list with all quantization ticks
    QList<int> ticks = file->quantization(_quantizationGrid);

    file->protocol()->startNewAction(tr("Quantify events"), new QImage(":/run_environment/graphics/tool/quantize.png"));
    foreach (MidiEvent* e, Selection::instance()->selectedEvents()) {
        int onTime = e->midiTime();
        e->setMidiTime(quantize(onTime, ticks));
        OnEvent* on = dynamic_cast<OnEvent*>(e);
        if (on) {
            MidiEvent* off = on->offEvent();
            off->setMidiTime(quantize(off->midiTime(), ticks)-1);
            if (off->midiTime() <= on->midiTime()) {
                int idx = ticks.indexOf(off->midiTime()+1);
                if ((idx >= 0) && (ticks.size() > idx + 1)) {
                    off->setMidiTime(ticks.at(idx + 1) - 1);
                }
            }
        }
    }
    file->protocol()->endAction();
}

int MainWindow::quantize(int t, QList<int> ticks) {

    int min = -1;

    for (int j = 0; j < ticks.size(); j++) {

        if (min < 0) {
            min = j;
            continue;
        }

        int i = ticks.at(j);

        int dist = t - i;

        int a = qAbs(dist);
        int b = qAbs(t - ticks.at(min));

        if (a < b) {
            min = j;
        }

        if (dist < 0) {
            return ticks.at(min);
        }
    }
    return ticks.last();
}

void MainWindow::quantizeNtoleDialog() {

    if (!file || Selection::instance()->selectedEvents().isEmpty()) {
        return;
    }

    NToleQuantizationDialog* d = new NToleQuantizationDialog(this);
    d->setModal(true);
    if (d->exec()) {

        quantizeNtole();
    }
}

void MainWindow::quantizeNtole() {

    if (!file || Selection::instance()->selectedEvents().isEmpty()) {
        return;
    }

    // get list with all quantization ticks
    QList<int> ticks = file->quantization(_quantizationGrid);

    file->protocol()->startNewAction(tr("Quantify tuplet"), new QImage(":/run_environment/graphics/tool/quantize.png"));

    // find minimum starting time
    int startTick = -1;
    foreach (MidiEvent* e, Selection::instance()->selectedEvents()) {
        int onTime = e->midiTime();
        if ((startTick < 0) || (onTime < startTick)) {
            startTick = onTime;
        }
    }

    // quantize start tick
    startTick = quantize(startTick, ticks);

    // compute new quantization grid
    QList<int> ntoleTicks;
    int ticksDuration = (NToleQuantizationDialog::replaceNumNum * file->ticksPerQuarter() * 4) / (qPow(2, NToleQuantizationDialog::replaceDenomNum));
    int fractionSize = ticksDuration / NToleQuantizationDialog::ntoleNNum;

    for (int i = 0; i <= NToleQuantizationDialog::ntoleNNum; i++) {
        ntoleTicks.append(startTick + i * fractionSize);
    }

    // quantize
    foreach (MidiEvent* e, Selection::instance()->selectedEvents()) {
        int onTime = e->midiTime();
        e->setMidiTime(quantize(onTime, ntoleTicks));
        OnEvent* on = dynamic_cast<OnEvent*>(e);
        if (on) {
            MidiEvent* off = on->offEvent();
            off->setMidiTime(quantize(off->midiTime(), ntoleTicks));
            if (off->midiTime() == on->midiTime()) {
                int idx = ntoleTicks.indexOf(off->midiTime());
                if ((idx >= 0) && (ntoleTicks.size() > idx + 1)) {
                    off->setMidiTime(ntoleTicks.at(idx + 1));
                } else if ((ntoleTicks.size() == idx + 1)) {
                    on->setMidiTime(ntoleTicks.at(idx - 1));
                }
            }
        }
    }
    file->protocol()->endAction();
}

void MainWindow::setSpeed(QAction* action) {
    double d = action->data().toDouble();
    MidiPlayer::setSpeedScale(d);
}

void MainWindow::checkEnableActionsForSelection() {
    bool enabled = Selection::instance()->selectedEvents().size() > 0;
    foreach (QAction* action, _activateWithSelections) {
        action->setEnabled(enabled);
    }
    if (_moveSelectedEventsToChannelMenu) {
        _moveSelectedEventsToChannelMenu->setEnabled(enabled);
    }
    if (_moveSelectedEventsToTrackMenu) {
        _moveSelectedEventsToTrackMenu->setEnabled(enabled);
    }
    if (Tool::currentTool() && Tool::currentTool()->button() && !Tool::currentTool()->button()->isEnabled()) {
        stdToolAction->trigger();
    }
    if (file) {
        undoAction->setEnabled(file->protocol()->stepsBack() > 1);
        redoAction->setEnabled(file->protocol()->stepsForward() > 0);
    }
}

void MainWindow::toolChanged() {
    checkEnableActionsForSelection();
    _miscWidget->update();
    mw_matrixWidget->update();
}

void MainWindow::copiedEventsChanged() {
    bool enable = EventTool::copiedEvents->size() > 0;
    _pasteAction->setEnabled(enable);
}

void MainWindow::updateAll() {
    mw_matrixWidget->registerRelayout();
    mw_matrixWidget->update();
    channelWidget->update();
    _trackWidget->update();
    _miscWidget->update();
}

void MainWindow::rebuildToolbarFromSettings() {
    // Dedicated method for rebuilding toolbar when settings change
    if (_toolbarWidget) {
        try {
            // Clear existing toolbar contents while keeping the widget in place
            QGridLayout* toolbarLayout = qobject_cast<QGridLayout*>(_toolbarWidget->layout());
            if (toolbarLayout) {
                // Remove all child widgets but keep the layout
                while (toolbarLayout->count() > 0) {
                    QLayoutItem* item = toolbarLayout->takeAt(0);
                    if (item) {
                        if (item->widget()) {
                            item->widget()->deleteLater();
                        }
                        delete item;
                    }
                }

                // Now rebuild the toolbar contents directly in the existing widget
                updateToolbarContents(_toolbarWidget, toolbarLayout);

                // Refresh icons
                refreshToolbarIcons();
            } else {
                // If no layout found, fall back to complete rebuild
                rebuildToolbar();
                refreshToolbarIcons();
            }
        } catch (...) {
            // If update fails, fall back to complete rebuild
            rebuildToolbar();
            refreshToolbarIcons();
        }
    }
}

void MainWindow::refreshToolbarIcons() {
    // The individual actions will be refreshed by Appearance::refreshAllIcons()
    // We just need to make sure our toolbar is up to date
    if (_toolbarWidget) {
        try {
            _toolbarWidget->update();

            // Also refresh any child toolbars
            QList<QToolBar*> toolbars = _toolbarWidget->findChildren<QToolBar*>();
            for (QToolBar* toolbar : toolbars) {
                if (toolbar) {
                    toolbar->update();
                }
            }
        } catch (...) {
            // If refresh fails, don't crash - just continue
        }
    }
}

void MainWindow::tweakTime() {
    delete currentTweakTarget;
    currentTweakTarget = new TimeTweakTarget(this);
}

void MainWindow::tweakStartTime() {
    delete currentTweakTarget;
    currentTweakTarget = new StartTimeTweakTarget(this);
}

void MainWindow::tweakEndTime() {
    delete currentTweakTarget;
    currentTweakTarget = new EndTimeTweakTarget(this);
}

void MainWindow::tweakNote() {
    delete currentTweakTarget;
    currentTweakTarget = new NoteTweakTarget(this);
}

void MainWindow::tweakValue() {
    delete currentTweakTarget;
    currentTweakTarget = new ValueTweakTarget(this);
}

void MainWindow::tweakSmallDecrease() {
    currentTweakTarget->smallDecrease();
}

void MainWindow::tweakSmallIncrease() {
    currentTweakTarget->smallIncrease();
}

void MainWindow::tweakMediumDecrease() {
    currentTweakTarget->mediumDecrease();
}

void MainWindow::tweakMediumIncrease() {
    currentTweakTarget->mediumIncrease();
}

void MainWindow::tweakLargeDecrease() {
    currentTweakTarget->largeDecrease();
}

void MainWindow::tweakLargeIncrease() {
    currentTweakTarget->largeIncrease();
}

void MainWindow::navigateSelectionUp() {
    selectionNavigator->up();
}

void MainWindow::navigateSelectionDown() {
    selectionNavigator->down();
}

void MainWindow::navigateSelectionLeft() {
    selectionNavigator->left();
}

void MainWindow::navigateSelectionRight() {
    selectionNavigator->right();
}

void MainWindow::transposeSelectedNotesOctaveUp() {
    if (!file) {
        return;
    }

    QList<MidiEvent*> selectedEvents = Selection::instance()->selectedEvents();
    if (selectedEvents.isEmpty()) {
        return;
    }

    file->protocol()->startNewAction("Transpose octave up");

    foreach (MidiEvent* event, selectedEvents) {
        NoteOnEvent* noteOnEvent = dynamic_cast<NoteOnEvent*>(event);
        if (noteOnEvent) {
            int newNote = noteOnEvent->note() + 12; // Move up one octave (12 semitones)
            if (newNote <= 127) { // MIDI note range is 0-127
                noteOnEvent->setNote(newNote);
            }
        }
    }

    file->protocol()->endAction();
    updateAll();
}

void MainWindow::transposeSelectedNotesOctaveDown() {
    if (!file) {
        return;
    }

    QList<MidiEvent*> selectedEvents = Selection::instance()->selectedEvents();
    if (selectedEvents.isEmpty()) {
        return;
    }

    file->protocol()->startNewAction("Transpose octave down");

    foreach (MidiEvent* event, selectedEvents) {
        NoteOnEvent* noteOnEvent = dynamic_cast<NoteOnEvent*>(event);
        if (noteOnEvent) {
            int newNote = noteOnEvent->note() - 12; // Move down one octave (12 semitones)
            if (newNote >= 0) { // MIDI note range is 0-127
                noteOnEvent->setNote(newNote);
            }
        }
    }

    file->protocol()->endAction();
    updateAll();
}
