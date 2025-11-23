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
#include <QFileDialog>
#include <QFileInfo>
#include <QIcon>
#include <QInputDialog>
#include <QList>
#include <QMap>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QSettings>
#include <QSplitter>
#include <QTabWidget>
#include <QToolBar>
#include <QToolButton>
#include <QDesktopServices>
#include <QKeyEvent>
#include <QTimer>

#include <cmath>
#include <algorithm>
#include <QComboBox>

#include "Appearance.h"
#include "AboutDialog.h"
#include "ChannelVisibilityManager.h"
#include "ChannelListWidget.h"
#include "CompleteMidiSetupDialog.h"
#include "DeleteOverlapsDialog.h"
#include "ExplodeChordsDialog.h"
#include "EventWidget.h"
#include "FileLengthDialog.h"
#include "InstrumentChooser.h"
#include "LayoutSettingsWidget.h"
#include "MatrixWidget.h"
#include "OpenGLMatrixWidget.h"
#include "OpenGLMiscWidget.h"
#include "MiscWidget.h"
#include "PerformanceSettingsWidget.h"
#include "NToleQuantizationDialog.h"
#include "ProtocolWidget.h"
#include "RecordDialog.h"
#include "SelectionNavigator.h"
#include "SettingsDialog.h"
#include "StrummerDialog.h"
#include "TrackListWidget.h"
#include "TransposeDialog.h"
#include "TweakTarget.h"
#include "UpdateChecker.h"

#include "../tool/DeleteOverlapsTool.h"
#include "../tool/EraserTool.h"
#include "../tool/EventMoveTool.h"
#include "../tool/EventTool.h"
#include "../tool/GlueTool.h"
#include "../tool/MeasureTool.h"
#include "../tool/NewNoteTool.h"
#include "../tool/ScissorsTool.h"
#include "../tool/SelectTool.h"
#include "../tool/Selection.h"
#include "../tool/SharedClipboard.h"
#include "../tool/SizeChangeTool.h"
#include "../tool/StandardTool.h"
#include "../tool/StrummerTool.h"
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
#include "../MidiEvent/PitchBendEvent.h"
#include "../midi/Metronome.h"
#include "../midi/MidiChannel.h"
#include "../midi/MidiFile.h"
#include "../midi/MidiInput.h"
#include "../midi/MidiOutput.h"
#include "../midi/MidiPlayer.h"
#include "../midi/MidiTrack.h"
#include "../midi/PlayerThread.h"
#include "../midi/InstrumentDefinitions.h"

MainWindow::MainWindow(QString initFile)
    : QMainWindow()
      , _initFile(initFile) {
    file = 0;
    _settings = new QSettings(QString("MidiEditor"), QString("NONE"));

    _moveSelectedEventsToChannelMenu = 0;
    _moveSelectedEventsToTrackMenu = 0;
    _toolbarWidget = nullptr;
    _updateChecker = nullptr;
    _silentUpdateCheck = false;

    Appearance::init(_settings);

    bool alternativeStop = _settings->value("alt_stop", false).toBool();
    MidiOutput::isAlternativePlayer = alternativeStop;
    bool ticksOK;
    int ticksPerQuarter = _settings->value("ticks_per_quarter", 192).toInt(&ticksOK);
    MidiFile::defaultTimePerQuarter = ticksPerQuarter;
    bool magnet = _settings->value("magnet", false).toBool();
    EventTool::enableMagnet(magnet);

    MidiInput::setThruEnabled(_settings->value("thru", false).toBool());

    // Initialize shared clipboard for inter-process copy/paste
    SharedClipboard::instance()->initialize();
    Metronome::setEnabled(_settings->value("metronome", false).toBool());
    bool loudnessOk;
    Metronome::setLoudness(_settings->value("metronome_loudness", 100).toInt(&loudnessOk));

    _quantizationGrid = _settings->value("quantization", 3).toInt();

    // Load instrument definitions
    QString insFile = _settings->value("InstrumentDefinitions/file").toString();
    if (!insFile.isEmpty()) {
        InstrumentDefinitions::instance()->load(insFile);
        QString insName = _settings->value("InstrumentDefinitions/instrument").toString();
        if (!insName.isEmpty()) {
            InstrumentDefinitions::instance()->selectInstrument(insName);
        }
    }
    // Always load overrides
    InstrumentDefinitions::instance()->loadOverrides(_settings);

    // metronome
    connect(MidiPlayer::playerThread(), SIGNAL(measureChanged(int, int)), Metronome::instance(), SLOT(measureUpdate(int, int)));
    connect(MidiPlayer::playerThread(), SIGNAL(measureUpdate(int, int)), Metronome::instance(), SLOT(measureUpdate(int, int)));
    connect(MidiPlayer::playerThread(), SIGNAL(meterChanged(int, int)), Metronome::instance(), SLOT(meterChanged(int, int)));
    connect(MidiPlayer::playerThread(), SIGNAL(playerStopped()), Metronome::instance(), SLOT(playbackStopped()));
    connect(MidiPlayer::playerThread(), SIGNAL(playerStarted()), Metronome::instance(), SLOT(playbackStarted()));

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

    QWidget *central = new QWidget(this);
    QGridLayout *centralLayout = new QGridLayout(central);
    centralLayout->setContentsMargins(3, 3, 3, 5);

    // there is a vertical split
    mainSplitter = new QSplitter(Qt::Horizontal, central);
    //mainSplitter->setHandleWidth(0);

    // The left side
    QSplitter *leftSplitter = new QSplitter(Qt::Vertical, mainSplitter);
    leftSplitter->setHandleWidth(2); // Enable handle width for misc widget collapse/expand
    mainSplitter->addWidget(leftSplitter);
    leftSplitter->setContentsMargins(0, 0, 0, 0);

    // The right side
    rightSplitter = new QSplitter(Qt::Vertical, mainSplitter);
    //rightSplitter->setHandleWidth(0);
    mainSplitter->addWidget(rightSplitter);

    // Set the sizes of mainSplitter
    mainSplitter->setStretchFactor(0, 1);
    mainSplitter->setStretchFactor(1, 0);
    mainSplitter->setContentsMargins(0, 0, 0, 0);

    // the channelWidget and the trackWidget are tabbed
    upperTabWidget = new QTabWidget(rightSplitter);
    rightSplitter->addWidget(upperTabWidget);
    rightSplitter->setContentsMargins(0, 0, 0, 0);

    // protocolList and EventWidget are tabbed
    lowerTabWidget = new QTabWidget(rightSplitter);
    rightSplitter->addWidget(lowerTabWidget);

    // MatrixArea
    QWidget *matrixArea = new QWidget(leftSplitter);
    leftSplitter->addWidget(matrixArea);
    matrixArea->setContentsMargins(0, 0, 0, 0);

    // Create MatrixWidget - use direct OpenGL acceleration approach
    bool useHardwareAcceleration = _settings->value("rendering/hardware_acceleration", false).toBool();

    // Check for high DPI scaling issues with OpenGL
    qreal dpr = devicePixelRatio();
    bool hasHighDpiScaling = (dpr != 1.0 && !Appearance::ignoreSystemScaling());

    if (useHardwareAcceleration && hasHighDpiScaling) {
        qWarning() << "MainWindow: High DPI scaling detected (DPR:" << dpr << ") with hardware acceleration enabled.";
        qWarning() << "MainWindow: Automatically disabling hardware acceleration due to Qt6 high DPI compatibility issues.";
        qWarning() << "MainWindow: To use hardware acceleration, enable 'Ignore system UI scaling' in Performance settings.";
        useHardwareAcceleration = false; // Automatically disable to prevent rendering issues
    }

    QWidget *matrixContainer;

    if (useHardwareAcceleration) {
        // Use direct OpenGL acceleration
        OpenGLMatrixWidget *openglMatrix = new OpenGLMatrixWidget(_settings, matrixArea);
        mw_matrixWidget = openglMatrix->getMatrixWidget(); // Get the internal MatrixWidget for data access
        _matrixWidgetContainer = openglMatrix; // Store the displayed widget for UI operations
        matrixContainer = openglMatrix;
        // Set up OpenGL container for cursor operations
        EditorTool::setOpenGLContainer(openglMatrix);
        // Direct OpenGL acceleration - no separate accelerator needed
        qDebug() << "Created MatrixWidget with direct OpenGL acceleration";
    } else {
        // Use software rendering
        mw_matrixWidget = new MatrixWidget(_settings, matrixArea);
        _matrixWidgetContainer = mw_matrixWidget; // Same widget for both data and UI
        matrixContainer = mw_matrixWidget;
        // No OpenGL container needed for software rendering
        EditorTool::setOpenGLContainer(nullptr);
        // Software rendering - no accelerator needed
        qDebug() << "Created MatrixWidget with software rendering";
    }

    vert = new QScrollBar(Qt::Vertical, matrixArea);
    QGridLayout *matrixAreaLayout = new QGridLayout(matrixArea);
    matrixAreaLayout->setHorizontalSpacing(6);
    QWidget *placeholder0 = new QWidget(matrixArea);
    placeholder0->setFixedHeight(50);
    matrixAreaLayout->setContentsMargins(0, 0, 0, 0);
    matrixAreaLayout->addWidget(matrixContainer, 0, 0, 2, 1);
    matrixAreaLayout->addWidget(placeholder0, 0, 1, 1, 1);
    matrixAreaLayout->addWidget(vert, 1, 1, 1, 1);
    matrixAreaLayout->setColumnStretch(0, 1);
    matrixArea->setLayout(matrixAreaLayout);

    bool screenLocked = _settings->value("screen_locked", false).toBool();
    mw_matrixWidget->setScreenLocked(screenLocked);
    int div = _settings->value("div", 2).toInt();
    mw_matrixWidget->setDiv(div);

    // VelocityArea
    QWidget *velocityArea = new QWidget(leftSplitter);
    velocityArea->setContentsMargins(0, 0, 0, 0);
    leftSplitter->addWidget(velocityArea);
    
    QGridLayout *velocityAreaLayout = new QGridLayout(velocityArea);
    velocityAreaLayout->setContentsMargins(0, 0, 0, 0);
    velocityAreaLayout->setHorizontalSpacing(6);
    _miscWidgetControl = new QWidget(velocityArea);
    _miscWidgetControl->setFixedWidth(110 - velocityAreaLayout->horizontalSpacing());

    velocityAreaLayout->addWidget(_miscWidgetControl, 0, 0, 1, 1);
    // there is a Scrollbar on the right side of the velocityWidget doing
    // nothing but making the VelocityWidget as big as the matrixWidget
    QScrollBar *scrollNothing = new QScrollBar(Qt::Vertical, velocityArea);
    scrollNothing->setMinimum(0);
    scrollNothing->setMaximum(0);
    velocityAreaLayout->addWidget(scrollNothing, 0, 2, 1, 1);
    velocityAreaLayout->setRowStretch(0, 1);
    velocityArea->setLayout(velocityAreaLayout);

    // Create horizontal scrollbar container as separate splitter widget - but make it non-resizable
    QWidget *scrollBarArea = new QWidget(leftSplitter);
    scrollBarArea->setContentsMargins(0, 0, 0, 0);
    scrollBarArea->setFixedHeight(20);
    scrollBarArea->setMinimumHeight(20);
    scrollBarArea->setMaximumHeight(20);
    scrollBarArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    leftSplitter->addWidget(scrollBarArea);
    
    // Make scrollbar area completely non-collapsible and non-resizable
    leftSplitter->setCollapsible(leftSplitter->count() - 1, false);
    leftSplitter->handle(leftSplitter->count() - 1)->setDisabled(true);
    leftSplitter->handle(leftSplitter->count() - 1)->hide();
    
    // Ensure the handle between matrixArea and velocityArea remains functional
    // and has higher priority than the scrollbar area
    if (leftSplitter->count() >= 2) {
        leftSplitter->handle(0)->setEnabled(true);
        leftSplitter->handle(0)->show();
        leftSplitter->handle(0)->raise(); // Bring to front for priority over scrollbar
    }
    
    hori = new QScrollBar(Qt::Horizontal, scrollBarArea);
    hori->setMinimum(0);
    hori->setValue(0);
    hori->setSingleStep(500);
    hori->setPageStep(5000);
    
    QHBoxLayout *scrollBarLayout = new QHBoxLayout(scrollBarArea);
    scrollBarLayout->setContentsMargins(110, 0, 0, 0); // Align with matrix widget
    scrollBarLayout->addWidget(hori);
    scrollBarArea->setLayout(scrollBarLayout);

    // Create MiscWidget
    QWidget *miscContainer;

    if (useHardwareAcceleration) {
        // Use direct OpenGL acceleration
        // Connect MiscWidget to the internal MatrixWidget for data access
        OpenGLMiscWidget *openglMisc = new OpenGLMiscWidget(mw_matrixWidget, _settings, velocityArea);
        _miscWidget = openglMisc->getMiscWidget(); // Get the internal MiscWidget for data access
        _miscWidgetContainer = openglMisc; // Store the displayed widget for UI operations
        miscContainer = openglMisc;
        // Direct OpenGL acceleration - no separate accelerator needed
        qDebug() << "Created MiscWidget with direct OpenGL acceleration";
    } else {
        // Use software rendering
        _miscWidget = new MiscWidget(mw_matrixWidget, velocityArea);
        _miscWidgetContainer = _miscWidget; // Same widget for both data and UI
        miscContainer = _miscWidget;
        // Software rendering - no accelerator needed
        qDebug() << "Created MiscWidget with software rendering";
    }

    _miscWidget->setContentsMargins(0, 0, 0, 0);
    velocityAreaLayout->addWidget(miscContainer, 0, 1, 1, 1);

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
        QString name = MidiFile::controlChangeName(i);
        if (name == MidiFile::tr("undefined")) {
            _miscController->addItem(QString::number(i) + ": ");
        } else {
            _miscController->addItem(QString::number(i) + ": " + name);
        }
    }
    _miscController->view()->setMinimumWidth(_miscController->minimumSizeHint().width());
    _miscControlLayout->addWidget(_miscController, 3, 0, 1, 3);
    connect(_miscController, SIGNAL(currentIndexChanged(int)), _miscWidgetContainer, SLOT(setControl(int)));

    //_miscControlLayout->addWidget(new QLabel("Channel:", _miscWidgetControl), 4, 0, 1, 3);
    _miscChannel = new QComboBox(_miscWidgetControl);
    for (int i = 0; i < 15; i++) {
        _miscChannel->addItem("Channel " + QString::number(i));
    }
    _miscChannel->view()->setMinimumWidth(_miscChannel->minimumSizeHint().width());
    _miscControlLayout->addWidget(_miscChannel, 5, 0, 1, 3);
    connect(_miscChannel, SIGNAL(currentIndexChanged(int)), _miscWidgetContainer, SLOT(setChannel(int)));
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

    QActionGroup *group = new QActionGroup(this);
    group->setExclusive(true);
    group->addAction(setSingleMode);
    group->addAction(setFreehandMode);
    group->addAction(setLineMode);
    setSingleMode->setChecked(true);
    connect(group, SIGNAL(triggered(QAction*)), this, SLOT(selectModeChanged(QAction*)));

    QToolButton *btnSingle = new QToolButton(_miscWidgetControl);
    btnSingle->setDefaultAction(setSingleMode);
    QToolButton *btnHand = new QToolButton(_miscWidgetControl);
    btnHand->setDefaultAction(setFreehandMode);
    QToolButton *btnLine = new QToolButton(_miscWidgetControl);
    btnLine->setDefaultAction(setLineMode);

    _miscControlLayout->addWidget(btnSingle, 9, 0, 1, 1);
    _miscControlLayout->addWidget(btnHand, 9, 1, 1, 1);
    _miscControlLayout->addWidget(btnLine, 9, 2, 1, 1);

    // Set the sizes of leftSplitter
    leftSplitter->setStretchFactor(0, 8);  // matrixArea
    leftSplitter->setStretchFactor(1, 1);  // velocityArea
    leftSplitter->setStretchFactor(2, 0);  // scrollBarArea (fixed height, non-resizable)

    // Track
    tracksWidget = new QWidget(upperTabWidget);
    QGridLayout *tracksLayout = new QGridLayout(tracksWidget);
    tracksWidget->setLayout(tracksLayout);
    QToolBar *tracksTB = new QToolBar(tracksWidget);
    tracksTB->setIconSize(QSize(20, 20));
    tracksLayout->addWidget(tracksTB, 0, 0, 1, 1);

    QAction *newTrack = new QAction(tr("Add track"), this);
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

    _trackWidget = new TrackListWidget(tracksWidget);
    connect(_trackWidget, SIGNAL(trackRenameClicked(int)), this, SLOT(renameTrack(int)), Qt::QueuedConnection);
    connect(_trackWidget, SIGNAL(trackRemoveClicked(int)), this, SLOT(removeTrack(int)), Qt::QueuedConnection);
    connect(_trackWidget, SIGNAL(trackClicked(MidiTrack*)), this, SLOT(editTrackAndChannel(MidiTrack*)), Qt::QueuedConnection);

    tracksLayout->addWidget(_trackWidget, 1, 0, 1, 1);
    upperTabWidget->addTab(tracksWidget, tr("Tracks"));

    // Channels
    channelsWidget = new QWidget(upperTabWidget);
    QGridLayout *channelsLayout = new QGridLayout(channelsWidget);
    channelsWidget->setLayout(channelsLayout);
    QToolBar *channelsTB = new QToolBar(channelsWidget);
    channelsTB->setIconSize(QSize(20, 20));
    channelsLayout->addWidget(channelsTB, 0, 0, 1, 1);

    _allChannelsAudible = new QAction(tr("All channels audible"), this);
    Appearance::setActionIcon(_allChannelsAudible, ":/run_environment/graphics/tool/all_audible.png");
    connect(_allChannelsAudible, SIGNAL(triggered()), this, SLOT(unmuteAllChannels()));
    channelsTB->addAction(_allChannelsAudible);

    _allChannelsMute = new QAction(tr("Mute all channels"), this);
    Appearance::setActionIcon(_allChannelsMute, ":/run_environment/graphics/tool/all_mute.png");
    connect(_allChannelsMute, SIGNAL(triggered()), this, SLOT(muteAllChannels()));
    channelsTB->addAction(_allChannelsMute);

    channelsTB->addSeparator();

    _allChannelsVisible = new QAction(tr("Show all channels"), this);
    Appearance::setActionIcon(_allChannelsVisible, ":/run_environment/graphics/tool/all_visible.png");
    connect(_allChannelsVisible, SIGNAL(triggered()), this, SLOT(allChannelsVisible()));
    channelsTB->addAction(_allChannelsVisible);

    _allChannelsInvisible = new QAction(tr("Hide all channels"), this);
    Appearance::setActionIcon(_allChannelsInvisible, ":/run_environment/graphics/tool/all_invisible.png");
    connect(_allChannelsInvisible, SIGNAL(triggered()), this, SLOT(allChannelsInvisible()));
    channelsTB->addAction(_allChannelsInvisible);

    channelWidget = new ChannelListWidget(channelsWidget);
    connect(channelWidget, SIGNAL(channelStateChanged()), this, SLOT(updateChannelMenu()), Qt::QueuedConnection);
    connect(channelWidget, SIGNAL(selectInstrumentClicked(int)), this, SLOT(setInstrumentForChannel(int)), Qt::QueuedConnection);
    channelsLayout->addWidget(channelWidget, 1, 0, 1, 1);
    upperTabWidget->addTab(channelsWidget, tr("Channels"));

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
    chooserWidget = new QWidget(rightSplitter);
    chooserWidget->setMinimumWidth(350);
    rightSplitter->addWidget(chooserWidget);
    QGridLayout *chooserLayout = new QGridLayout(chooserWidget);
    QLabel *trackchannelLabel = new QLabel(tr("Add new events to ..."));
    chooserLayout->addWidget(trackchannelLabel, 0, 0, 1, 2);
    QLabel *channelLabel = new QLabel(tr("Channel: "), chooserWidget);
    chooserLayout->addWidget(channelLabel, 2, 0, 1, 1);
    _chooseEditChannel = new QComboBox(chooserWidget);
    for (int i = 0; i < 16; i++) {
        if (i == 9) _chooseEditChannel->addItem(tr("Percussion channel"));
        else _chooseEditChannel->addItem(tr("Channel ") + QString::number(i)); // TODO: Display channel instrument | UDP: But **file** is *nullptr*
    }
    connect(_chooseEditChannel, SIGNAL(activated(int)), this, SLOT(editChannel(int)));

    chooserLayout->addWidget(_chooseEditChannel, 2, 1, 1, 1);
    QLabel *trackLabel = new QLabel(tr("Track: "), chooserWidget);
    chooserLayout->addWidget(trackLabel, 1, 0, 1, 1);
    _chooseEditTrack = new QComboBox(chooserWidget);
    chooserLayout->addWidget(_chooseEditTrack, 1, 1, 1, 1);
    connect(_chooseEditTrack, SIGNAL(activated(int)), this, SLOT(editTrack(int)));
    chooserLayout->setColumnStretch(1, 1);
    // connect Scrollbars and Widgets
    // Connect to the actual displayed widget (OpenGL or software)
    connect(vert, SIGNAL(valueChanged(int)), matrixContainer, SLOT(scrollYChanged(int)));
    connect(hori, SIGNAL(valueChanged(int)), matrixContainer, SLOT(scrollXChanged(int)));

    connect(channelWidget, SIGNAL(channelStateChanged()), matrixContainer, SLOT(update()));
    connect(mw_matrixWidget, SIGNAL(sizeChanged(int, int, int, int)), this, SLOT(matrixSizeChanged(int, int, int, int)));
    connect(mw_matrixWidget, SIGNAL(scrollChanged(int, int, int, int)), this, SLOT(scrollPositionsChanged(int, int, int, int)));

    setCentralWidget(central);

    QWidget *buttons = setupActions(central);

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

    // Initialize shared clipboard immediately
    initializeSharedClipboard();

    // Apply widget size constraints based on settings
    applyWidgetSizeConstraints();

    // Load initial file immediately - no need for artificial delay
    loadInitFile();

    // Check for updates silently on startup
    QTimer::singleShot(2000, this, [this](){ checkForUpdates(true); });
}

MainWindow::~MainWindow() {
    // Ensure proper cleanup order to prevent QRhi resource leaks and QPixmap errors
    qDebug() << "MainWindow: Starting destructor cleanup sequence";

    // Perform early cleanup if it hasn't been done already
    performEarlyCleanup();

    // Clean up shared clipboard resources
    SharedClipboard::instance()->cleanup();

    // Clean up static appearance resources to prevent QPixmap/QColor issues after QApplication shutdown
    Appearance::cleanup();
    InstrumentDefinitions::cleanup();

    qDebug() << "MainWindow: Destructor cleanup sequence completed";
}

void MainWindow::performEarlyCleanup() {
    static bool cleanupPerformed = false;
    if (cleanupPerformed) {
        return; // Prevent multiple cleanup calls
    }
    cleanupPerformed = true;

    qDebug() << "MainWindow: Performing early OpenGL cleanup";

    // Set shutdown flag immediately to prevent any QPixmap creation during cleanup
    Appearance::setShuttingDown(true);

    // Stop any ongoing MIDI operations first
    if (MidiPlayer::isPlaying()) {
        MidiPlayer::stop();
    }

    // End any ongoing MIDI input recording
    if (MidiInput::recording()) {
        MidiInput::endInput(nullptr); // Pass nullptr since we're just stopping, not saving
    }

    // Clean up OpenGL widgets explicitly while OpenGL context is still valid
    if (OpenGLMatrixWidget *openglMatrix = qobject_cast<OpenGLMatrixWidget*>(_matrixWidgetContainer)) {
        qDebug() << "MainWindow: Early cleanup of OpenGL matrix widget";
        openglMatrix->setParent(nullptr);
        delete openglMatrix;
        _matrixWidgetContainer = nullptr;
        mw_matrixWidget = nullptr;
    }

    if (_miscWidgetContainer && _miscWidgetContainer != _miscWidget) {
        qDebug() << "MainWindow: Early cleanup of OpenGL misc widget";
        _miscWidgetContainer->setParent(nullptr);
        delete _miscWidgetContainer;
        _miscWidgetContainer = nullptr;
        _miscWidget = nullptr;
    }

    // Force immediate processing of any pending events
    QApplication::processEvents(QEventLoop::AllEvents);

    qDebug() << "MainWindow: Early OpenGL cleanup completed";
}

void MainWindow::initializeSharedClipboard() {
    // Initialize shared clipboard
    SharedClipboard::instance()->initialize();

    // Update paste action state to check shared clipboard
    copiedEventsChanged();
}

void MainWindow::updatePasteActionState() {
    // Simple check - enable paste if shared clipboard is available
    if (_pasteAction && EventTool::copiedEvents->size() == 0) {
        SharedClipboard *clipboard = SharedClipboard::instance();
        bool sharedClipboardAvailable = clipboard->initialize();
        _pasteAction->setEnabled(sharedClipboardAvailable);
    }
}

void MainWindow::loadInitFile() {
    if (_initFile != "")
        loadFile(_initFile);
    else
        newFile();
}

void MainWindow::dropEvent(QDropEvent *ev) {
    QList<QUrl> urls = ev->mimeData()->urls();
    foreach(QUrl url, urls) {
        QString newFile = url.toLocalFile();
        if (!newFile.isEmpty()) {
            loadFile(newFile);
            break;
        }
    }
}

void MainWindow::dragEnterEvent(QDragEnterEvent *ev) {
    ev->accept();
}

void MainWindow::scrollPositionsChanged(int startMs, int maxMs, int startLine,
                                        int maxLine) {
    hori->setMinimum(0);
    hori->setMaximum(maxMs);
    // Force startMs to 0 if it's very close to 0 to eliminate dead space
    int clampedStartMs = (startMs < 10) ? 0 : startMs;
    hori->setValue(clampedStartMs);
    vert->setMaximum(maxLine);
    vert->setValue(startLine);
}

void MainWindow::setFile(MidiFile *newFile) {
    // Store reference to old file for cleanup
    MidiFile *oldFile = this->file;

    EventTool::clearSelection();
    Selection::setFile(newFile);

    // Reset channel visibility to show all channels when loading a new file
    ChannelVisibilityManager::instance().resetAllVisible();

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

    // Connect recalcWidgetSize to the matrix widget container
    connect(newFile, SIGNAL(recalcWidgetSize()), _matrixWidgetContainer, SLOT(calcSizes()));
    connect(newFile->protocol(), SIGNAL(actionFinished()), this, SLOT(markEdited()));
    connect(newFile->protocol(), SIGNAL(actionFinished()), eventWidget(), SLOT(reload()));
    connect(newFile->protocol(), SIGNAL(actionFinished()), this, SLOT(checkEnableActionsForSelection()));
    // Set file on the appropriate widget based on rendering mode
    if (OpenGLMatrixWidget *openglMatrix = qobject_cast<OpenGLMatrixWidget*>(_matrixWidgetContainer)) {
        // Using OpenGL acceleration - set file on OpenGL widget (which delegates to internal widget)
        openglMatrix->setFile(newFile);
    } else {
        // Using software rendering - set file directly on MatrixWidget
        mw_matrixWidget->setFile(newFile);
    }
    updateChannelMenu();
    updateTrackMenu();
    _matrixWidgetContainer->update();
    _miscWidgetContainer->update();

    // Update paste action state when file changes
    copiedEventsChanged();
    checkEnableActionsForSelection();

    // Clean up the old file after everything has been switched to the new file
    // This ensures all widgets have switched to the new file before cleanup
    if (oldFile) {
        delete oldFile;
    }
}

MidiFile *MainWindow::getFile() {
    return file;
}

MatrixWidget *MainWindow::matrixWidget() {
    return mw_matrixWidget;
}

void MainWindow::matrixSizeChanged(int maxScrollTime, int maxScrollLine,
                                   int vX, int vY) {
    // Set scroll bar ranges
    vert->setMaximum(maxScrollLine);
    hori->setMinimum(0);
    hori->setMaximum(maxScrollTime);
    
    // Set scroll bar values - ensure horizontal starts at 0 for new files
    vert->setValue(qMax(0, qMin(vY, maxScrollLine)));
    // For horizontal: clamp small values to 0 to eliminate dead space
    int clampedVX = (vX < 10) ? 0 : vX;
    hori->setValue(qMax(0, qMin(clampedVX, maxScrollTime)));

    // Update the matrix widget
    _matrixWidgetContainer->update();
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
        CompleteMidiSetupDialog *d = new CompleteMidiSetupDialog(this, false, true);
        d->setModal(true);
        d->exec();
        return;
    }
    if (file && !MidiInput::recording() && !MidiPlayer::isPlaying()) {
        // Update playback cursor position using the appropriate widget type
        if (OpenGLMatrixWidget *openglMatrix = qobject_cast<OpenGLMatrixWidget*>(_matrixWidgetContainer)) {
            openglMatrix->timeMsChanged(file->msOfTick(file->cursorTick()), true);
        } else if (MatrixWidget *matrixWidget = qobject_cast<MatrixWidget*>(_matrixWidgetContainer)) {
            matrixWidget->timeMsChanged(file->msOfTick(file->cursorTick()), true);
        }

        _miscWidgetContainer->setEnabled(false);
        channelWidget->setEnabled(false);
        protocolWidget->setEnabled(false);
        // PERFORMANCE: Don't disable matrix widget during playback to allow mouse wheel scrolling
        // The widget itself handles playback state checks for editing operations
        // _matrixWidgetContainer->setEnabled(false);
        _trackWidget->setEnabled(false);
        eventWidget()->setEnabled(false);

        MidiPlayer::play(file);
        connect(MidiPlayer::playerThread(), SIGNAL(playerStopped()), this, SLOT(stop()));

        // Connect playback cursor updates for all platforms (not just Windows)
        // This is essential for the playback cursor to move during playback
        connect(MidiPlayer::playerThread(), SIGNAL(timeMsChanged(int)), _matrixWidgetContainer, SLOT(timeMsChanged(int)));
    }
}

void MainWindow::record() {
    if (!MidiOutput::isConnected() || !MidiInput::isConnected()) {
        CompleteMidiSetupDialog *d = new CompleteMidiSetupDialog(this, !MidiInput::isConnected(), !MidiOutput::isConnected());
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

            // Update playback cursor position using the appropriate widget type
            if (OpenGLMatrixWidget *openglMatrix = qobject_cast<OpenGLMatrixWidget*>(_matrixWidgetContainer)) {
                openglMatrix->timeMsChanged(file->msOfTick(file->cursorTick()), true);
            } else if (MatrixWidget *matrixWidget = qobject_cast<MatrixWidget*>(_matrixWidgetContainer)) {
                matrixWidget->timeMsChanged(file->msOfTick(file->cursorTick()), true);
            }

            _miscWidgetContainer->setEnabled(false);
            channelWidget->setEnabled(false);
            protocolWidget->setEnabled(false);
            // PERFORMANCE: Don't disable matrix widget during playback to allow mouse wheel scrolling
            // The widget itself handles playback state checks for editing operations
            // _matrixWidgetContainer->setEnabled(false);
            _trackWidget->setEnabled(false);
            eventWidget()->setEnabled(false);
            MidiPlayer::play(file);
            MidiInput::startInput();
            connect(MidiPlayer::playerThread(), SIGNAL(playerStopped()), this, SLOT(stop()));
            // Connect playback cursor updates for all platforms (not just Windows)
            // This is essential for the playback cursor to move during playback
            connect(MidiPlayer::playerThread(), SIGNAL(timeMsChanged(int)), _matrixWidgetContainer, SLOT(timeMsChanged(int)));
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

    disconnect(MidiPlayer::playerThread(), SIGNAL(playerStopped()), this, SLOT(stop()));

    if (resetPause) {
        file->setPauseTick(-1);
        _matrixWidgetContainer->update();
    }
    if (!MidiInput::recording() && MidiPlayer::isPlaying()) {
        MidiPlayer::stop();
        _miscWidgetContainer->setEnabled(true);
        channelWidget->setEnabled(true);
        _trackWidget->setEnabled(true);
        protocolWidget->setEnabled(true);
        // Matrix widget was never disabled, so no need to re-enable
        // _matrixWidgetContainer->setEnabled(true);
        eventWidget()->setEnabled(true);
        // Update playback cursor position using the appropriate widget type
        if (OpenGLMatrixWidget *openglMatrix = qobject_cast<OpenGLMatrixWidget*>(_matrixWidgetContainer)) {
            openglMatrix->timeMsChanged(MidiPlayer::timeMs(), true);
        } else if (MatrixWidget *matrixWidget = qobject_cast<MatrixWidget*>(_matrixWidgetContainer)) {
            matrixWidget->timeMsChanged(MidiPlayer::timeMs(), true);
        }
        _trackWidget->setEnabled(true);
        panic();
    }

    MidiTrack *track = file->track(NewNoteTool::editTrack());
    if (!track) {
        return;
    }

    if (MidiInput::recording()) {
        MidiPlayer::stop();
        panic();
        _miscWidgetContainer->setEnabled(true);
        channelWidget->setEnabled(true);
        protocolWidget->setEnabled(true);
        _matrixWidgetContainer->setEnabled(true);
        _trackWidget->setEnabled(true);
        eventWidget()->setEnabled(true);
        QMultiMap<int, MidiEvent *> events = MidiInput::endInput(track);

        if (events.isEmpty() && !autoConfirmRecord) {
            QMessageBox::information(this, tr("Information"), tr("No events recorded."));
        } else {
            RecordDialog *dialog = new RecordDialog(file, events, _settings, this);
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

    QList<TimeSignatureEvent *> *eventlist = new QList<TimeSignatureEvent *>;
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
        // Update playback cursor position using the appropriate widget type
        if (OpenGLMatrixWidget *openglMatrix = qobject_cast<OpenGLMatrixWidget*>(_matrixWidgetContainer)) {
            openglMatrix->timeMsChanged(file->msOfTick(newTick), true);
        } else if (MatrixWidget *matrixWidget = qobject_cast<MatrixWidget*>(_matrixWidgetContainer)) {
            matrixWidget->timeMsChanged(file->msOfTick(newTick), true);
        }
    }
    _matrixWidgetContainer->update();
}

void MainWindow::back() {
    if (!file)
        return;

    QList<TimeSignatureEvent *> *eventlist = new QList<TimeSignatureEvent *>;
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
        // Update playback cursor position using the appropriate widget type
        if (OpenGLMatrixWidget *openglMatrix = qobject_cast<OpenGLMatrixWidget*>(_matrixWidgetContainer)) {
            openglMatrix->timeMsChanged(file->msOfTick(newTick), true);
        } else if (MatrixWidget *matrixWidget = qobject_cast<MatrixWidget*>(_matrixWidgetContainer)) {
            matrixWidget->timeMsChanged(file->msOfTick(newTick), true);
        }
    }
    _matrixWidgetContainer->update();
}

void MainWindow::backToBegin() {
    if (!file)
        return;

    file->setPauseTick(0);
    file->setCursorTick(0);

    _matrixWidgetContainer->update();
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

    foreach(MidiEvent* event, file->channel(16)->eventMap()->values()) {
        int eventTick = event->midiTime();
        if (eventTick <= oldTick) continue;
        TextEvent *textEvent = dynamic_cast<TextEvent *>(event);

        if (textEvent && textEvent->type() == TextEvent::MARKER) {
            newTick = eventTick;
            break;
        }
    }

    if (newTick < 0) return;
    file->setPauseTick(newTick);
    file->setCursorTick(newTick);
    // Update playback cursor position using the appropriate widget type
    if (OpenGLMatrixWidget *openglMatrix = qobject_cast<OpenGLMatrixWidget*>(_matrixWidgetContainer)) {
        openglMatrix->timeMsChanged(file->msOfTick(newTick), true);
    } else if (MatrixWidget *matrixWidget = qobject_cast<MatrixWidget*>(_matrixWidgetContainer)) {
        matrixWidget->timeMsChanged(file->msOfTick(newTick), true);
    }
    _matrixWidgetContainer->update();
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
    QList<MidiEvent *> events = file->channel(16)->eventMap()->values();

    for (int eventNumber = events.size() - 1; eventNumber >= 0; eventNumber--) {
        MidiEvent *event = events.at(eventNumber);
        int eventTick = event->midiTime();
        if (eventTick >= oldTick) continue;
        TextEvent *textEvent = dynamic_cast<TextEvent *>(event);

        if (textEvent && textEvent->type() == TextEvent::MARKER) {
            newTick = eventTick;
            break;
        }
    }

    file->setPauseTick(newTick);
    file->setCursorTick(newTick);
    // Update playback cursor position using the appropriate widget type
    if (OpenGLMatrixWidget *openglMatrix = qobject_cast<OpenGLMatrixWidget*>(_matrixWidgetContainer)) {
        openglMatrix->timeMsChanged(file->msOfTick(newTick), true);
    } else if (MatrixWidget *matrixWidget = qobject_cast<MatrixWidget*>(_matrixWidgetContainer)) {
        matrixWidget->timeMsChanged(file->msOfTick(newTick), true);
    }
    _matrixWidgetContainer->update();
}

void MainWindow::save() {
    if (!file)
        return;

    if (QFile(file->path()).exists()) {
        bool printMuteWarning = false;

        for (int i = 0; i < 16; i++) {
            MidiChannel *ch = file->channel(i);
            if (ch->mute()) {
                printMuteWarning = true;
            }
        }
        foreach(MidiTrack* track, *(file->tracks())) {
            if (track->muted()) {
                printMuteWarning = true;
            }
        }

        if (printMuteWarning) {
            QMessageBox::information(this, tr("Channels/Tracks mute"), tr("One or more channels/tracks are not audible. They will be audible in the saved file."), QMessageBox::Ok);
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
    QFile *f = new QFile(oldPath);
    QString dir = startDirectory;
    if (f->exists()) {
        QFileInfo(*f).dir().path();
    }
    QString newPath = QFileDialog::getSaveFileName(this, tr("Save file as..."), dir);

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
            MidiChannel *ch = file->channel(i);
            if (ch->mute() || !ch->visible()) {
                printMuteWarning = true;
            }
        }
        foreach(MidiTrack* track, *(file->tracks())) {
            if (track->muted() || track->hidden()) {
                printMuteWarning = true;
            }
        }

        if (printMuteWarning) {
            QMessageBox::information(this, tr("Channels/Tracks mute"), tr("One or more channels/tracks are not audible. They will be audible in the saved file."), QMessageBox::Ok);
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

    QFile *f = new QFile(oldPath);
    QString dir = startDirectory;
    if (f->exists()) {
        QFileInfo(*f).dir().path();
    }
    QString newPath = QFileDialog::getOpenFileName(this, tr("Open file"), dir, tr("MIDI Files(*.mid *.midi);;All Files(*)"));

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

    MidiFile *mf = new MidiFile(filePath, &ok);

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

EventWidget *MainWindow::eventWidget() {
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

    // Use global visibility manager to avoid corrupted MidiChannel access
    for (int i = 0; i < 19; i++) {
        ChannelVisibilityManager::instance().setChannelVisible(i, true);

        // Also try to update MidiChannel object (with safety)
        try {
            file->channel(i)->setVisible(true);
        } catch (...) {
            // Ignore if MidiChannel is corrupted
        }
    }

    file->protocol()->endAction();
    channelWidget->update();
}

void MainWindow::allChannelsInvisible() {
    if (!file)
        return;
    file->protocol()->startNewAction(tr("Hide all channels"));

    // Use global visibility manager to avoid corrupted MidiChannel access
    for (int i = 0; i < 19; i++) {
        ChannelVisibilityManager::instance().setChannelVisible(i, false);

        // Also try to update MidiChannel object (with safety)
        try {
            file->channel(i)->setVisible(false);
        } catch (...) {
            // Ignore if MidiChannel is corrupted
        }
    }

    file->protocol()->endAction();
    channelWidget->update();
}

void MainWindow::closeEvent(QCloseEvent *event) {
    bool shouldClose = false;

    if (!file || file->saved()) {
        shouldClose = true;
        event->accept();
    } else {
        bool sbc = saveBeforeClose();

        if (sbc) {
            shouldClose = true;
            event->accept();
        } else {
            event->ignore();
        }
    }

    // Only perform early cleanup if we're actually closing
    if (shouldClose) {
        performEarlyCleanup();
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

    // Only save matrix widget settings if the widget is still valid
    if (mw_matrixWidget) {
        _settings->setValue("screen_locked", mw_matrixWidget->screenLocked());
        _settings->setValue("div", mw_matrixWidget->div());
        _settings->setValue("colors_from_channel", mw_matrixWidget->colorsByChannel());
    }

    _settings->setValue("magnet", EventTool::magnetEnabled());
    _settings->setValue("metronome", Metronome::enabled());
    _settings->setValue("metronome_loudness", Metronome::loudness());
    _settings->setValue("thru", MidiInput::thru());
    _settings->setValue("quantization", _quantizationGrid);

    Appearance::writeSettings(_settings);

    // Cleanup shared clipboard resources
    SharedClipboard::instance()->cleanup();
}

void MainWindow::about() {
    AboutDialog *d = new AboutDialog(this);
    d->setModal(true);

    // Ensure the about dialog inherits the current application style and palette
    d->setPalette(QApplication::palette());

    // Apply the same style as the application
    QApplication *app = qobject_cast<QApplication *>(QApplication::instance());
    if (app) {
        QString appStyleSheet = app->styleSheet();
        if (!appStyleSheet.isEmpty()) {
            d->setStyleSheet(appStyleSheet);
        }
    }

    // Force style refresh on the dialog
    QTimer::singleShot(0, [d]() {
        if (d) {
            d->style()->polish(d);
            d->update();
        }
    });

    d->show();
}

void MainWindow::setFileLengthMs() {
    if (!file)
        return;

    FileLengthDialog *d = new FileLengthDialog(file, this);
    d->setModal(true);
    d->show();
}

void MainWindow::setStartDir(QString dir) {
    startDirectory = dir;
}

bool MainWindow::saveBeforeClose() {
    QMessageBox msgBox(this);
    msgBox.setWindowTitle(tr("Save file?"));
    msgBox.setText(tr("Save file ") + file->path() + tr(" before closing?"));
    msgBox.addButton(tr("Save"), QMessageBox::AcceptRole);
    msgBox.addButton(tr("Close without saving"), QMessageBox::DestructiveRole);
    msgBox.addButton(tr("Cancel"), QMessageBox::RejectRole);
    msgBox.setDefaultButton(qobject_cast<QPushButton *>(msgBox.buttons().at(2))); // Cancel is default

    int result = msgBox.exec();

    // Map button roles to actions
    QAbstractButton *clickedButton = msgBox.clickedButton();
    QMessageBox::ButtonRole role = msgBox.buttonRole(clickedButton);

    switch (role) {
        case QMessageBox::AcceptRole:
            // save
            if (QFile(file->path()).exists())
                file->save(file->path());
            else saveas();
            return true;

        case QMessageBox::RejectRole:
            // cancel - break
            return false;

        default: // DestructiveRole - close without saving
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
    MidiFile *f = new MidiFile();

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
    double scale = QInputDialog::getDouble(this, tr("Scalefactor"), tr("Scalefactor:"), 1.0, 0, 2147483647, 17, &ok);
    if (ok && scale > 0 && Selection::instance()->selectedEvents().size() > 0 && file) {
        // find minimum
        int minTime = 2147483647;
        foreach(MidiEvent* e, Selection::instance()->selectedEvents()) {
            if (e->midiTime() < minTime) {
                minTime = e->midiTime();
            }
        }

        file->protocol()->startNewAction(tr("Scale events"), 0);
        foreach(MidiEvent* e, Selection::instance()->selectedEvents()) {
            e->setMidiTime((e->midiTime() - minTime) * scale + minTime);
            OnEvent *on = dynamic_cast<OnEvent *>(e);
            if (on) {
                MidiEvent *off = on->offEvent();
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
        foreach(MidiEvent* e, Selection::instance()->selectedEvents()) {
            if (e->midiTime() < minTime) {
                minTime = e->midiTime();
            }
        }

        file->protocol()->startNewAction(tr("Align left"), new QImage(":/run_environment/graphics/tool/align_left.png"));
        foreach(MidiEvent* e, Selection::instance()->selectedEvents()) {
            int onTime = e->midiTime();
            e->setMidiTime(minTime);
            OnEvent *on = dynamic_cast<OnEvent *>(e);
            if (on) {
                MidiEvent *off = on->offEvent();
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
        foreach(MidiEvent* e, Selection::instance()->selectedEvents()) {
            OnEvent *on = dynamic_cast<OnEvent *>(e);
            if (on) {
                MidiEvent *off = on->offEvent();
                if (off->midiTime() > maxTime) {
                    maxTime = off->midiTime();
                }
            }
        }

        file->protocol()->startNewAction(tr("Align right"), new QImage(":/run_environment/graphics/tool/align_right.png"));
        foreach(MidiEvent* e, Selection::instance()->selectedEvents()) {
            int onTime = e->midiTime();
            OnEvent *on = dynamic_cast<OnEvent *>(e);
            if (on) {
                MidiEvent *off = on->offEvent();
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
        foreach(MidiEvent* e, Selection::instance()->selectedEvents()) {
            OnEvent *on = dynamic_cast<OnEvent *>(e);
            if (on) {
                MidiEvent *off = on->offEvent();
                avgStart += e->midiTime();
                avgTime += (off->midiTime() - e->midiTime());
                count++;
            }
        }
        if (count > 1) {
            avgStart /= count;
            avgTime /= count;

            file->protocol()->startNewAction(tr("Equalize"), new QImage(":/run_environment/graphics/tool/equalize.png"));
            foreach(MidiEvent* e, Selection::instance()->selectedEvents()) {
                OnEvent *on = dynamic_cast<OnEvent *>(e);
                if (on) {
                    MidiEvent *off = on->offEvent();
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

void MainWindow::strumNotes() {
    if (!file) {
        return;
    }

    StrummerDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        Tool::setFile(file);

        StrummerTool strummerTool;
        strummerTool.performStrum(
            dialog.getStartStrength(),
            dialog.getStartTension(),
            dialog.getEndStrength(),
            dialog.getEndTension(),
            dialog.getVelocityStrength(),
            dialog.getVelocityTension(),
            dialog.getPreserveEnd(),
            dialog.getAlternateDirection(),
            dialog.getUseStepStrength(),
            dialog.getIgnoreTrack()
        );
        updateAll();
    }
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

    // Call the matrix widget's reset view function using the appropriate widget type
    if (OpenGLMatrixWidget *openglMatrix = qobject_cast<OpenGLMatrixWidget*>(_matrixWidgetContainer)) {
        openglMatrix->resetView();
    } else if (MatrixWidget *matrixWidget = qobject_cast<MatrixWidget*>(_matrixWidgetContainer)) {
        matrixWidget->resetView();
    }
}

void MainWindow::deleteSelectedEvents() {
    bool showsSelected = false;
    if (Tool::currentTool()) {
        EventTool *eventTool = dynamic_cast<EventTool *>(Tool::currentTool());
        if (eventTool) {
            showsSelected = eventTool->showsSelection();
        }
    }
    if (showsSelected && Selection::instance()->selectedEvents().size() > 0 && file) {
        file->protocol()->startNewAction(tr("Remove event(s)"));

        // Group events by channel to minimize protocol entries
        QMap<int, QList<MidiEvent *> > eventsByChannel;
        foreach(MidiEvent* ev, Selection::instance()->selectedEvents()) {
            eventsByChannel[ev->channel()].append(ev);
        }

        // Remove events channel by channel to create fewer protocol entries
        foreach(int channelNum, eventsByChannel.keys()) {
            MidiChannel *channel = file->channel(channelNum);
            ProtocolEntry *toCopy = channel->copy();

            // Remove all events from this channel at once
            foreach(MidiEvent* ev, eventsByChannel[channelNum]) {
                // Handle track name events
                if (channelNum == 16 && (MidiEvent *) (ev->track()->nameEvent()) == ev) {
                    ev->track()->setNameEvent(0);
                }

                // Remove the event and its off event if it exists
                channel->eventMap()->remove(ev->midiTime(), ev);
                OnEvent *on = dynamic_cast<OnEvent *>(ev);
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

void MainWindow::deleteChannel(QAction *action) {
    if (!file) {
        return;
    }

    int num = action->data().toInt();
    file->protocol()->startNewAction(tr("Remove all events from channel ") + QString::number(num));
    foreach(MidiEvent* event, file->channel(num)->eventMap()->values()) {
        if (Selection::instance()->selectedEvents().contains(event)) {
            EventTool::deselectEvent(event);
        }
    }
    Selection::instance()->setSelection(Selection::instance()->selectedEvents());

    file->channel(num)->deleteAllEvents();
    file->protocol()->endAction();
}

void MainWindow::moveSelectedEventsToChannel(QAction *action) {
    if (!file) {
        return;
    }

    int num = action->data().toInt();
    MidiChannel *channel = file->channel(num);

    if (Selection::instance()->selectedEvents().size() > 0) {
        file->protocol()->startNewAction(tr("Move selected events to channel ") + QString::number(num));
        foreach(MidiEvent* ev, Selection::instance()->selectedEvents()) {
            file->channel(ev->channel())->removeEvent(ev);
            ev->setChannel(num, true);
            OnEvent *onevent = dynamic_cast<OnEvent *>(ev);
            if (onevent) {
                channel->insertEvent(onevent->offEvent(), onevent->offEvent()->midiTime());
                onevent->offEvent()->setChannel(num);
            }
            channel->insertEvent(ev, ev->midiTime());
        }

        file->protocol()->endAction();
    }
}

void MainWindow::moveSelectedEventsToTrack(QAction *action) {
    if (!file) {
        return;
    }

    int num = action->data().toInt();
    MidiTrack *track = file->track(num);

    if (Selection::instance()->selectedEvents().size() > 0) {
        file->protocol()->startNewAction(tr("Move selected events to track ") + QString::number(num));
        foreach(MidiEvent* ev, Selection::instance()->selectedEvents()) {
            ev->setTrack(track, true);
            OnEvent *onevent = dynamic_cast<OnEvent *>(ev);
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

        foreach(QString str, _recentFilePaths) {
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
    foreach(QString path, _recentFilePaths) {
        QFile f(path);
        QString name = QFileInfo(f).fileName();

        QVariant variant(path);
        QAction *openRecentFileAction = new QAction(name, this);
        openRecentFileAction->setData(variant);
        _recentPathsMenu->addAction(openRecentFileAction);
    }
}

void MainWindow::openRecent(QAction *action) {
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
    foreach(QAction* action, _deleteChannelMenu->actions()) {
        int channel = action->data().toInt();
        if (file) {
            action->setText(QString::number(channel) + " " + MidiFile::instrumentName(file->channel(channel)->progAtTick(0)));
        }
    }

    // move events to channel...
    foreach(QAction* action, _moveSelectedEventsToChannelMenu->actions()) {
        int channel = action->data().toInt();
        if (file) {
            action->setText(QString::number(channel) + " " + MidiFile::instrumentName(file->channel(channel)->progAtTick(0)));
        }
    }

    // paste events to channel...
    foreach(QAction* action, _pasteToChannelMenu->actions()) {
        int channel = action->data().toInt();
        if (file && channel >= 0) {
            action->setText(QString::number(channel) + " " + MidiFile::instrumentName(file->channel(channel)->progAtTick(0)));
        }
    }

    // select all events from channel...
    foreach(QAction* action, _selectAllFromChannelMenu->actions()) {
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
        QAction *moveToTrackAction = new QAction(QString::number(i) + " " + file->tracks()->at(i)->name(), this);
        moveToTrackAction->setData(variant);

        QString formattedKeySequence = QString("Shift+%1").arg(i);
        moveToTrackAction->setShortcut(QKeySequence::fromString(formattedKeySequence));

        _moveSelectedEventsToTrackMenu->addAction(moveToTrackAction);
    }

    for (int i = 0; i < file->numTracks(); i++) {
        QVariant variant(i);
        QAction *select = new QAction(QString::number(i) + " " + file->tracks()->at(i)->name(), this);
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
    QActionGroup *pasteTrackGroup = new QActionGroup(this);
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
        QAction *pasteToTrackAction = new QAction(text, this);
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

void MainWindow::muteChannel(QAction *action) {
    int channel = action->data().toInt();
    if (file) {
        file->protocol()->startNewAction(tr("Mute channel"));
        file->channel(channel)->setMute(action->isChecked());
        updateChannelMenu();
        channelWidget->update();
        file->protocol()->endAction();
    }
}

void MainWindow::soloChannel(QAction *action) {
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

void MainWindow::viewChannel(QAction *action) {
    int channel = action->data().toInt();
    if (file) {
        file->protocol()->startNewAction(tr("Channel visibility changed"));
        file->channel(channel)->setVisible(action->isChecked());
        updateChannelMenu();
        channelWidget->update();
        file->protocol()->endAction();
    }
}

void MainWindow::keyPressEvent(QKeyEvent *event) {
    // First, let Qt handle any shortcuts
    QMainWindow::keyPressEvent(event);

    // If the event wasn't accepted by a shortcut, forward it to the matrix widget
    if (!event->isAccepted()) {
        if (OpenGLMatrixWidget *openglMatrix = qobject_cast<OpenGLMatrixWidget*>(_matrixWidgetContainer)) {
            openglMatrix->takeKeyPressEvent(event);
        } else if (MatrixWidget *matrixWidget = qobject_cast<MatrixWidget*>(_matrixWidgetContainer)) {
            matrixWidget->takeKeyPressEvent(event);
        }
    }
}

void MainWindow::keyReleaseEvent(QKeyEvent *event) {
    // First, let Qt handle any shortcuts
    QMainWindow::keyReleaseEvent(event);

    // If the event wasn't accepted by a shortcut, forward it to the matrix widget
    if (!event->isAccepted()) {
        if (OpenGLMatrixWidget *openglMatrix = qobject_cast<OpenGLMatrixWidget*>(_matrixWidgetContainer)) {
            openglMatrix->takeKeyReleaseEvent(event);
        } else if (MatrixWidget *matrixWidget = qobject_cast<MatrixWidget*>(_matrixWidgetContainer)) {
            matrixWidget->takeKeyReleaseEvent(event);
        }
    }
}

void MainWindow::showEventWidget(bool show) {
    if (show) {
        lowerTabWidget->setCurrentIndex(1);
    } else {
        lowerTabWidget->setCurrentIndex(0);
    }
}

void MainWindow::renameTrackMenuClicked(QAction *action) {
    int track = action->data().toInt();
    renameTrack(track);
}

void MainWindow::renameTrack(int tracknumber) {
    if (!file) {
        return;
    }

    file->protocol()->startNewAction(tr("Edit Track Name"));

    bool ok;
    QString text = QInputDialog::getText(this, tr("Set Track Name"), tr("Track name (Track ") + QString::number(tracknumber) + tr(")"), QLineEdit::Normal, file->tracks()->at(tracknumber)->name(), &ok);
    if (ok && !text.isEmpty()) {
        file->tracks()->at(tracknumber)->setName(text);
    }

    file->protocol()->endAction();
    updateTrackMenu();
}

void MainWindow::removeTrackMenuClicked(QAction *action) {
    int track = action->data().toInt();
    removeTrack(track);
}

void MainWindow::removeTrack(int tracknumber) {
    if (!file) {
        return;
    }
    MidiTrack *track = file->track(tracknumber);
    file->protocol()->startNewAction(tr("Remove track"));
    foreach(MidiEvent* event, Selection::instance()->selectedEvents()) {
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
        QString text = QInputDialog::getText(this, tr("Set Track Name"), tr("Track name (New Track)"), QLineEdit::Normal, tr("New Track"), &ok);
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
    foreach(MidiTrack* track, *(file->tracks())) {
        track->setMuted(true);
    }
    file->protocol()->endAction();
    _trackWidget->update();
}

void MainWindow::unmuteAllTracks() {
    if (!file)
        return;
    file->protocol()->startNewAction(tr("All tracks audible"));
    foreach(MidiTrack* track, *(file->tracks())) {
        track->setMuted(false);
    }
    file->protocol()->endAction();
    _trackWidget->update();
}

void MainWindow::allTracksVisible() {
    if (!file)
        return;
    file->protocol()->startNewAction(tr("Show all tracks"));
    foreach(MidiTrack* track, *(file->tracks())) {
        track->setHidden(false);
    }
    file->protocol()->endAction();
    _trackWidget->update();
}

void MainWindow::allTracksInvisible() {
    if (!file)
        return;
    file->protocol()->startNewAction(tr("Hide all tracks"));
    foreach(MidiTrack* track, *(file->tracks())) {
        track->setHidden(true);
    }
    file->protocol()->endAction();
    _trackWidget->update();
}

void MainWindow::showTrackMenuClicked(QAction *action) {
    int track = action->data().toInt();
    if (file) {
        file->protocol()->startNewAction(tr("Show track"));
        file->track(track)->setHidden(!(action->isChecked()));
        updateTrackMenu();
        _trackWidget->update();
        file->protocol()->endAction();
    }
}

void MainWindow::muteTrackMenuClicked(QAction *action) {
    int track = action->data().toInt();
    if (file) {
        file->protocol()->startNewAction(tr("Mute track"));
        file->track(track)->setMuted(action->isChecked());
        updateTrackMenu();
        _trackWidget->update();
        file->protocol()->endAction();
    }
}

void MainWindow::selectAllFromChannel(QAction *action) {
    if (!file) {
        return;
    }
    int channel = action->data().toInt();
    file->protocol()->startNewAction("Select all events from channel " + QString::number(channel));
    EventTool::clearSelection();
    file->channel(channel)->setVisible(true);

    // Collect events for batch selection
    QList<MidiEvent *> eventsToSelect;
    foreach(MidiEvent* e, file->channel(channel)->eventMap()->values()) {
        if (e->track()->hidden()) {
            e->track()->setHidden(false);
        }

        // Skip OffEvents
        OffEvent *offevent = dynamic_cast<OffEvent *>(e);
        if (!offevent) {
            eventsToSelect.append(e);
        }
    }

    // Batch select all events
    EventTool::batchSelectEvents(eventsToSelect);

    file->protocol()->endAction();
}

void MainWindow::selectAllFromTrack(QAction *action) {
    if (!file) {
        return;
    }

    int track = action->data().toInt();
    file->protocol()->startNewAction("Select all events from track " + QString::number(track));
    EventTool::clearSelection();
    file->track(track)->setHidden(false);

    // Collect events for batch selection
    QList<MidiEvent *> eventsToSelect;
    for (int channel = 0; channel < 16; channel++) {
        foreach(MidiEvent* e, file->channel(channel)->eventMap()->values()) {
            if (e->track()->number() == track) {
                file->channel(e->channel())->setVisible(true);

                // Skip OffEvents
                OffEvent *offevent = dynamic_cast<OffEvent *>(e);
                if (!offevent) {
                    eventsToSelect.append(e);
                }
            }
        }
    }

    // Batch select all events
    EventTool::batchSelectEvents(eventsToSelect);
    file->protocol()->endAction();
}

void MainWindow::selectAll() {
    if (!file) {
        return;
    }

    file->protocol()->startNewAction("Select all");

    // Collect all valid events in a single pass for better performance
    QList<MidiEvent *> eventsToSelect;

    // Estimate total events across all channels for better memory allocation
    int estimatedEvents = 0;
    for (int i = 0; i < 16; i++) {
        if (ChannelVisibilityManager::instance().isChannelVisible(i)) {
            estimatedEvents += file->channel(i)->eventMap()->size();
        }
    }
    eventsToSelect.reserve(estimatedEvents); // Reserve based on actual event count

    for (int i = 0; i < 16; i++) {
        // Only process visible channels using the global visibility manager
        if (!ChannelVisibilityManager::instance().isChannelVisible(i)) {
            continue;
        }

        const QMultiMap<int, MidiEvent *> *eventMap = file->channel(i)->eventMap();
        foreach(MidiEvent* event, eventMap->values()) {
            // Pre-filter events to avoid redundant checks later
            if (event->track()->hidden()) {
                continue;
            }

            // Skip OffEvents as they're handled by EventTool::selectEvent anyway
            OffEvent *offevent = dynamic_cast<OffEvent *>(event);
            if (offevent) {
                continue;
            }

            eventsToSelect.append(event);
        }
    }

    // Batch select all events at once to minimize UI updates
    EventTool::batchSelectEvents(eventsToSelect);

    file->protocol()->endAction();
}

void MainWindow::convertPitchBendToNotes() {
    if (!file) {
        return;
    }

    // Collect selected note-on events
    QList<NoteOnEvent *> notes;
    foreach (MidiEvent *event, Selection::instance()->selectedEvents()) {
        NoteOnEvent *on = dynamic_cast<NoteOnEvent *>(event);
        if (on && on->offEvent()) {
            notes.append(on);
        }
    }

    if (notes.isEmpty()) {
        return;
    }

    // Prompt user for pitch bend range
    bool ok;
    QStringList items;
    items << tr("±2 semitones (General MIDI default)")
          << tr("±12 semitones (Guitar/Bass VSTs)")
          << tr("±24 semitones (Extreme pitch modulation)")
          << tr("Custom...");
    
    QString item = QInputDialog::getItem(this, 
                                         tr("Pitch Bend Range"),
                                         tr("Select pitch bend sensitivity range:"),
                                         items, 0, false, &ok);
    
    if (!ok) {
        return; // User cancelled
    }
    
    double bendRangeSemis = 2.0; // default
    
    if (item == items[0]) {
        bendRangeSemis = 2.0;
    } else if (item == items[1]) {
        bendRangeSemis = 12.0;
    } else if (item == items[2]) {
        bendRangeSemis = 24.0;
    } else { // Custom
        bendRangeSemis = QInputDialog::getDouble(this,
                                                  tr("Custom Pitch Bend Range"),
                                                  tr("Enter pitch bend range in semitones (±):"),
                                                  2.0, 1.0, 96.0, 1, &ok);
        if (!ok) {
            return; // User cancelled
        }
    }

    file->protocol()->startNewAction(tr("Convert pitch bends to notes"));

    foreach (NoteOnEvent *on, notes) {
        int ch = on->channel();
        MidiChannel *channel = file->channel(ch);
        if (!channel) continue;

        int t0 = on->midiTime();
        int t1 = on->offEvent()->midiTime();
        if (t1 <= t0) continue;

        // Gather pitch bend events on this channel within [t0, t1)
        QMultiMap<int, MidiEvent *> *emap = channel->eventMap();

        // Find last pitch bend value at or before t0
        int startValue = 8192; // neutral
        if (emap && !emap->isEmpty()) {
            QMultiMap<int, MidiEvent *>::const_iterator it = emap->upperBound(t0);
            if (it != emap->begin()) {
                do {
                    --it;
                    PitchBendEvent *pb = dynamic_cast<PitchBendEvent *>(it.value());
                    if (pb) {
                        startValue = pb->value();
                        break;
                    }
                } while (it != emap->begin());
            }
        }

        // Collect change points (segment boundaries)
        QList<int> boundaries;
        boundaries.append(t0);
        QMap<int, int> bendAtTime; // time -> value

        if (emap && !emap->isEmpty()) {
            QMultiMap<int, MidiEvent *>::const_iterator it2 = emap->lowerBound(t0);
            while (it2 != emap->end() && it2.key() < t1) {
                PitchBendEvent *pb = dynamic_cast<PitchBendEvent *>(it2.value());
                if (pb) {
                    int tick = it2.key();
                    // Avoid duplicate consecutive boundaries at same tick
                    if (boundaries.isEmpty() || boundaries.last() != tick) {
                        boundaries.append(tick);
                    }
                    bendAtTime.insert(tick, pb->value());
                }
                ++it2;
            }
        }
        if (boundaries.last() != t1) {
            boundaries.append(t1);
        }

        // Helper to convert bend value to nearest semitone offset
        auto bendToSemi = [bendRangeSemis](int value) -> int {
            double norm = (static_cast<double>(value) - 8192.0) / 8192.0; // approx -1..+1
            double semis = norm * bendRangeSemis;
            int offset = static_cast<int>(semis >= 0 ? std::floor(semis + 0.5) : std::ceil(semis - 0.5));
            return offset;
        };

        // Create replacement notes for each segment
        for (int i = 0; i < boundaries.size() - 1; ++i) {
            int segStart = boundaries.at(i);
            int segEnd = boundaries.at(i + 1);
            if (segEnd <= segStart) continue;

            int bendVal = (i == 0 ? startValue : bendAtTime.value(segStart, startValue));
            int offset = bendToSemi(bendVal);
            int newNote = on->note() + offset;
            if (newNote < 0) newNote = 0;
            if (newNote > 127) newNote = 127;

            channel->insertNote(newNote, segStart, segEnd, on->velocity(), on->track());
        }

        // Remove original note
        channel->removeEvent(on);

        // Remove pitch bend events in [t0, t1) - they're now represented as discrete notes
        // Note: Each selected note is processed independently, so overlapping notes
        // will each remove only the pitch bends within their own time range
        QList<MidiEvent *> removeList;
        if (emap && !emap->isEmpty()) {
            QMultiMap<int, MidiEvent *>::const_iterator it3 = emap->lowerBound(t0);
            while (it3 != emap->end() && it3.key() < t1) {
                PitchBendEvent *pb = dynamic_cast<PitchBendEvent *>(it3.value());
                if (pb) {
                    removeList.append(pb);
                }
                ++it3;
            }
        }
        foreach (MidiEvent *ev, removeList) {
            channel->removeEvent(ev);
        }
    }

    file->protocol()->endAction();

    updateAll();
}

void MainWindow::explodeChordsToTracks() {
    if (!file) {
        return;
    }

    // Determine source scope: selected notes on current edit track, otherwise all notes on that track
    MidiTrack *sourceTrack = file->track(NewNoteTool::editTrack());
    if (!sourceTrack) {
        return;
    }

    // Show dialog
    ExplodeChordsDialog *dialog = new ExplodeChordsDialog(file, sourceTrack, this);
    if (dialog->exec() != QDialog::Accepted) {
        delete dialog;
        return;
    }

    // Get settings from dialog
    typedef ExplodeChordsDialog::SplitStrategy SplitStrategy;
    typedef ExplodeChordsDialog::GroupMode GroupMode;
    
    SplitStrategy strategy = dialog->splitStrategy();
    GroupMode groupMode = dialog->groupMode();
    int minNotes = dialog->minimumNotes();
    bool insertAtEnd = dialog->insertAtEnd();
    bool keepOriginal = dialog->keepOriginalNotes();
    
    delete dialog;

    // Collect candidate notes
    QList<NoteOnEvent *> selectedNotes;
    foreach (MidiEvent *ev, Selection::instance()->selectedEvents()) {
        NoteOnEvent *on = dynamic_cast<NoteOnEvent *>(ev);
        if (on && on->offEvent() && on->track() == sourceTrack) {
            selectedNotes.append(on);
        }
    }

    bool useSelection = !selectedNotes.isEmpty();

    // Build map: key -> list of notes (a chord group)
    // key is either start tick or start tick + length encoded
    struct NoteWrap { NoteOnEvent *on; int tick; int len; };
    QMap<qint64, QList<NoteWrap>> groups;

    auto addNoteToGroups = [&](NoteOnEvent *on){
        int t = on->midiTime();
        int l = on->offEvent() ? (on->offEvent()->midiTime() - on->midiTime()) : 0;
        if (l < 0) l = 0;
        qint64 key = (strategy == SplitStrategy::SAME_START) ? qint64(t) : ( (qint64(t) << 32) | qint64(l & 0xFFFFFFFF) );
        groups[key].append({on, t, l});
    };

    if (useSelection) {
        for (NoteOnEvent *on : selectedNotes) addNoteToGroups(on);
    } else {
        // All notes on source track across channels 0..15
        for (int ch = 0; ch < 16; ++ch) {
            QMultiMap<int, MidiEvent *> *emap = file->channel(ch)->eventMap();
            for (auto it = emap->begin(); it != emap->end(); ++it) {
                NoteOnEvent *on = dynamic_cast<NoteOnEvent *>(it.value());
                if (on && on->offEvent() && on->track() == sourceTrack) {
                    addNoteToGroups(on);
                }
            }
        }
    }

    // Filter to only groups with at least minNotes and same start (and length if chosen)
    QList<QList<NoteWrap>> chordGroups;
    for (auto it = groups.begin(); it != groups.end(); ++it) {
        QList<NoteWrap> list = it.value();
        if (list.size() >= minNotes) {
            chordGroups.append(list);
        }
    }
    if (chordGroups.isEmpty()) {
        return; // Nothing to do
    }

    file->protocol()->startNewAction(tr("Explode chords to tracks"));

    // Find source track index for insertion
    int sourceTrackIdx = file->tracks()->indexOf(sourceTrack);
    QList<MidiTrack*> newTracks;

    // Helper to sort by pitch descending (top voice first)
    auto sortByPitchDesc = [](const NoteWrap &a, const NoteWrap &b){
        NoteOnEvent *A = a.on; NoteOnEvent *B = b.on;
        return A->note() > B->note();
    };

    // Helper to copy or move note
    auto processNote = [&](NoteOnEvent *on, MidiTrack *dst) {
        if (!on || !on->offEvent()) return;
        if (keepOriginal) {
            // Copy note to new track
            int ch = on->channel();
            file->channel(ch)->insertNote(on->note(), on->midiTime(), 
                                          on->offEvent()->midiTime(), 
                                          on->velocity(), dst);
        } else {
            // Move note to new track
            on->setTrack(dst);
            on->offEvent()->setTrack(dst);
        }
    };

    if (groupMode == ExplodeChordsDialog::ALL_CHORDS_ONE_TRACK) {
        // Single destination track
        file->addTrack();
        MidiTrack *dst = file->tracks()->last();
        dst->setName(sourceTrack->name() + tr(" - Chord 1"));
        newTracks.append(dst);
        for (const auto &grp : chordGroups) {
            for (const NoteWrap &nw : grp) {
                processNote(nw.on, dst);
            }
        }
    } else if (groupMode == ExplodeChordsDialog::EACH_CHORD_OWN_TRACK) {
        int idx = 1;
        for (auto grp : chordGroups) {
            std::sort(grp.begin(), grp.end(), sortByPitchDesc);
            file->addTrack();
            MidiTrack *dst = file->tracks()->last();
            dst->setName(sourceTrack->name() + tr(" - Chord ") + QString::number(idx++));
            newTracks.append(dst);
            for (const NoteWrap &nw : grp) {
                processNote(nw.on, dst);
            }
        }
    } else { // VOICES_ACROSS_CHORDS
        // Determine max chord size
        int maxSize = 0;
        for (const auto &grp : chordGroups) maxSize = std::max(maxSize, static_cast<int>(grp.size()));
        // Create destination tracks per voice
        QVector<MidiTrack*> voiceTracks;
        voiceTracks.reserve(maxSize);
        for (int v = 0; v < maxSize; ++v) {
            file->addTrack();
            MidiTrack *dst = file->tracks()->last();
            dst->setName(sourceTrack->name() + tr(" - Chord ") + QString::number(v + 1));
            voiceTracks.append(dst);
            newTracks.append(dst);
        }
        // Process notes: for each chord, sort and map nth by pitch to nth track
        for (auto grp : chordGroups) {
            std::sort(grp.begin(), grp.end(), sortByPitchDesc);
            for (int i = 0; i < grp.size(); ++i) {
                MidiTrack *dst = voiceTracks[i];
                processNote(grp[i].on, dst);
            }
        }
    }

    // Reorder tracks if not inserting at end
    if (!insertAtEnd && sourceTrackIdx >= 0) {
        // Move new tracks to directly after source track
        QList<MidiTrack*> *trackList = file->tracks();
        
        // Remove new tracks from their current positions (at end)
        for (MidiTrack *newTrack : newTracks) {
            trackList->removeOne(newTrack);
        }
        
        // Insert them right after the source track
        int insertPos = sourceTrackIdx + 1;
        for (MidiTrack *newTrack : newTracks) {
            trackList->insert(insertPos++, newTrack);
        }
        
        // Renumber all tracks
        int n = 0;
        foreach(MidiTrack* track, *trackList) {
            track->setNumber(n++);
        }
    }

    file->protocol()->endAction();

    updateAll();
}

void MainWindow::transposeNSemitones() {
    if (!file) {
        return;
    }

    QList<NoteOnEvent *> events;
    foreach(MidiEvent* event, Selection::instance()->selectedEvents()) {
        NoteOnEvent *on = dynamic_cast<NoteOnEvent *>(event);
        if (on) {
            events.append(on);
        }
    }

    if (events.isEmpty()) {
        return;
    }

    TransposeDialog *d = new TransposeDialog(events, file, this);
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
    _matrixWidgetContainer->update();
    _miscWidgetContainer->update();
}

void MainWindow::colorsByTrack() {
    mw_matrixWidget->setColorsByTracks();
    _colorsByChannel->setChecked(false);
    _colorsByTracks->setChecked(true);
    mw_matrixWidget->registerRelayout();
    _matrixWidgetContainer->update();
    _miscWidgetContainer->update();
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

void MainWindow::editTrackAndChannel(MidiTrack *track) {
    editTrack(track->number(), false);
    if (track->assignedChannel() > -1) {
        editChannel(track->assignedChannel(), false);
    }
}

void MainWindow::setInstrumentForChannel(int i) {
    InstrumentChooser *d = new InstrumentChooser(file, i, this);
    d->setModal(true);
    d->exec();

    if (i == NewNoteTool::editChannel()) {
        editChannel(i);
    }
    updateChannelMenu();
}

void MainWindow::instrumentChannel(QAction *action) {
    if (file) {
        setInstrumentForChannel(action->data().toInt());
    }
}

void MainWindow::spreadSelection() {
    if (!file) {
        return;
    }

    bool ok;
    float numMs = float(QInputDialog::getDouble(this, tr("Set spread-time"), tr("Spread time [ms]"), 10, 5, 500, 2, &ok));

    if (!ok) {
        numMs = 1;
    }

    QMultiMap<int, int> spreadChannel[19];

    foreach(MidiEvent* event, Selection::instance()->selectedEvents()) {
        if (!spreadChannel[event->channel()].values(event->line()).contains(event->midiTime())) {
            spreadChannel[event->channel()].insert(event->line(), event->midiTime());
        }
    }

    file->protocol()->startNewAction(tr("Spread events"));
    int numSpreads = 0;
    for (int i = 0; i < 19; i++) {
        MidiChannel *channel = file->channel(i);

        QList<int> seenBefore;

        foreach(int line, spreadChannel[i].keys()) {
            if (seenBefore.contains(line)) {
                continue;
            }

            seenBefore.append(line);

            foreach(int position, spreadChannel[i].values(line)) {
                QList<MidiEvent *> eventsWithAllLines = channel->eventMap()->values(position);

                QList<MidiEvent *> events;
                foreach(MidiEvent* event, eventsWithAllLines) {
                    if (event->line() == line) {
                        events.append(event);
                    }
                }

                //spread events for the channel at the given position
                int num = events.count();
                if (num > 1) {
                    float timeToInsert = file->msOfTick(position) + numMs * num / 2;

                    for (int y = 0; y < num; y++) {
                        MidiEvent *toMove = events.at(y);

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
    // Use the container widget for UI operations (handles both OpenGL and software)
    if (OpenGLMiscWidget *openglMisc = qobject_cast<OpenGLMiscWidget*>(_miscWidgetContainer)) {
        openglMisc->setMode(mode);
    } else if (MiscWidget *miscWidget = qobject_cast<MiscWidget*>(_miscWidgetContainer)) {
        miscWidget->setMode(mode);
    }
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
                QString name = MidiFile::controlChangeName(i);
                if (name == MidiFile::tr("undefined")) {
                    _miscController->addItem(QString::number(i) + ": ");
                } else {
                    _miscController->addItem(QString::number(i) + ": " + name);
                }
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

void MainWindow::selectModeChanged(QAction *action) {
    // Use the container widget for UI operations (handles both OpenGL and software)
    if (action == setSingleMode) {
        if (OpenGLMiscWidget *openglMisc = qobject_cast<OpenGLMiscWidget*>(_miscWidgetContainer)) {
            openglMisc->setEditMode(SINGLE_MODE);
        } else if (MiscWidget *miscWidget = qobject_cast<MiscWidget*>(_miscWidgetContainer)) {
            miscWidget->setEditMode(SINGLE_MODE);
        }
    }
    if (action == setLineMode) {
        if (OpenGLMiscWidget *openglMisc = qobject_cast<OpenGLMiscWidget*>(_miscWidgetContainer)) {
            openglMisc->setEditMode(LINE_MODE);
        } else if (MiscWidget *miscWidget = qobject_cast<MiscWidget*>(_miscWidgetContainer)) {
            miscWidget->setEditMode(LINE_MODE);
        }
    }
    if (action == setFreehandMode) {
        if (OpenGLMiscWidget *openglMisc = qobject_cast<OpenGLMiscWidget*>(_miscWidgetContainer)) {
            openglMisc->setEditMode(MOUSE_MODE);
        } else if (MiscWidget *miscWidget = qobject_cast<MiscWidget*>(_miscWidgetContainer)) {
            miscWidget->setEditMode(MOUSE_MODE);
        }
    }
}

QWidget *MainWindow::setupActions(QWidget *parent) {
    // Menubar
    QMenu *fileMB = menuBar()->addMenu(tr("File"));
    QMenu *editMB = menuBar()->addMenu(tr("Edit"));
    QMenu *toolsMB = menuBar()->addMenu(tr("Tools"));
    QMenu *viewMB = menuBar()->addMenu(tr("View"));
    QMenu *playbackMB = menuBar()->addMenu(tr("Playback"));
    QMenu *midiMB = menuBar()->addMenu(tr("Midi"));
    QMenu *helpMB = menuBar()->addMenu(tr("Help"));

    // File
    QAction *newAction = new QAction(tr("New"), this);
    newAction->setShortcut(QKeySequence::New);
    _defaultShortcuts["new"] = QList<QKeySequence>() << newAction->shortcut();
    Appearance::setActionIcon(newAction, ":/run_environment/graphics/tool/new.png");
    connect(newAction, SIGNAL(triggered()), this, SLOT(newFile()));
    fileMB->addAction(newAction);
    _actionMap["new"] = newAction;

    QAction *loadAction = new QAction(tr("Open..."), this);
    loadAction->setShortcut(QKeySequence::Open);
    _defaultShortcuts["open"] = QList<QKeySequence>() << loadAction->shortcut();
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

    QAction *saveAction = new QAction(tr("Save"), this);
    saveAction->setShortcut(QKeySequence::Save);
    _defaultShortcuts["save"] = QList<QKeySequence>() << saveAction->shortcut();
    Appearance::setActionIcon(saveAction, ":/run_environment/graphics/tool/save.png");
    connect(saveAction, SIGNAL(triggered()), this, SLOT(save()));
    fileMB->addAction(saveAction);
    _actionMap["save"] = saveAction;

    QAction *saveAsAction = new QAction(tr("Save as..."), this);
    saveAsAction->setShortcut(QKeySequence::SaveAs);
    _defaultShortcuts["save_as"] = QList<QKeySequence>() << saveAsAction->shortcut();
    Appearance::setActionIcon(saveAsAction, ":/run_environment/graphics/tool/saveas.png");
    connect(saveAsAction, SIGNAL(triggered()), this, SLOT(saveas()));
    fileMB->addAction(saveAsAction);
    _actionMap["save_as"] = saveAsAction;

    fileMB->addSeparator();

    QAction *quitAction = new QAction(tr("Quit"), this);
    quitAction->setShortcut(QKeySequence::Quit);
    _defaultShortcuts["quit"] = QList<QKeySequence>() << quitAction->shortcut();
    Appearance::setActionIcon(quitAction, ":/run_environment/graphics/tool/noicon.png");
    connect(quitAction, SIGNAL(triggered()), this, SLOT(close()));
    fileMB->addAction(quitAction);
    _actionMap["quit"] = quitAction;

    // Edit
    undoAction = new QAction(tr("Undo"), this);
    undoAction->setShortcut(QKeySequence::Undo);
    _defaultShortcuts["undo"] = QList<QKeySequence>() << undoAction->shortcut();
    Appearance::setActionIcon(undoAction, ":/run_environment/graphics/tool/undo.png");
    connect(undoAction, SIGNAL(triggered()), this, SLOT(undo()));
    editMB->addAction(undoAction);
    _actionMap["undo"] = undoAction;

    redoAction = new QAction(tr("Redo"), this);
    redoAction->setShortcut(QKeySequence::Redo);
    _defaultShortcuts["redo"] = QList<QKeySequence>() << redoAction->shortcut();
    Appearance::setActionIcon(redoAction, ":/run_environment/graphics/tool/redo.png");
    connect(redoAction, SIGNAL(triggered()), this, SLOT(redo()));
    editMB->addAction(redoAction);
    _actionMap["redo"] = redoAction;

    editMB->addSeparator();

    QAction *selectAllAction = new QAction(tr("Select all"), this);
    selectAllAction->setToolTip(tr("Select all visible events"));
    selectAllAction->setShortcut(QKeySequence::SelectAll);
    _defaultShortcuts["select_all"] = QList<QKeySequence>() << selectAllAction->shortcut();
    connect(selectAllAction, SIGNAL(triggered()), this, SLOT(selectAll()));
    editMB->addAction(selectAllAction);
    _actionMap["select_all"] = selectAllAction;

    _selectAllFromChannelMenu = new QMenu(tr("Select all events from channel..."), editMB);
    editMB->addMenu(_selectAllFromChannelMenu);
    connect(_selectAllFromChannelMenu, SIGNAL(triggered(QAction*)), this, SLOT(selectAllFromChannel(QAction*)));

    for (int i = 0; i < 16; i++) {
        QVariant variant(i);
        QAction *delChannelAction = new QAction(QString::number(i), this);
        delChannelAction->setData(variant);
        _selectAllFromChannelMenu->addAction(delChannelAction);
    }

    _selectAllFromTrackMenu = new QMenu(tr("Select all events from track..."), editMB);
    editMB->addMenu(_selectAllFromTrackMenu);
    connect(_selectAllFromTrackMenu, SIGNAL(triggered(QAction*)), this, SLOT(selectAllFromTrack(QAction*)));

    for (int i = 0; i < 16; i++) {
        QVariant variant(i);
        QAction *delChannelAction = new QAction(QString::number(i), this);
        delChannelAction->setData(variant);
        _selectAllFromTrackMenu->addAction(delChannelAction);
    }

    editMB->addSeparator();

    QAction *navigateSelectionUpAction = new QAction(tr("Navigate selection up"), editMB);
    navigateSelectionUpAction->setShortcut(QKeySequence(Qt::Key_Up));
    _defaultShortcuts["navigate_up"] = QList<QKeySequence>() << navigateSelectionUpAction->shortcut();
    connect(navigateSelectionUpAction, SIGNAL(triggered()), this, SLOT(navigateSelectionUp()));
    editMB->addAction(navigateSelectionUpAction);
    _actionMap["navigate_up"] = navigateSelectionUpAction;

    QAction *navigateSelectionDownAction = new QAction(tr("Navigate selection down"), editMB);
    navigateSelectionDownAction->setShortcut(QKeySequence(Qt::Key_Down));
    _defaultShortcuts["navigate_down"] = QList<QKeySequence>() << navigateSelectionDownAction->shortcut();
    connect(navigateSelectionDownAction, SIGNAL(triggered()), this, SLOT(navigateSelectionDown()));
    editMB->addAction(navigateSelectionDownAction);
    _actionMap["navigate_down"] = navigateSelectionDownAction;

    QAction *navigateSelectionLeftAction = new QAction(tr("Navigate selection left"), editMB);
    navigateSelectionLeftAction->setShortcut(QKeySequence(Qt::Key_Left));
    _defaultShortcuts["navigate_left"] = QList<QKeySequence>() << navigateSelectionLeftAction->shortcut();
    connect(navigateSelectionLeftAction, SIGNAL(triggered()), this, SLOT(navigateSelectionLeft()));
    editMB->addAction(navigateSelectionLeftAction);
    _actionMap["navigate_left"] = navigateSelectionLeftAction;

    QAction *navigateSelectionRightAction = new QAction(tr("Navigate selection right"), editMB);
    navigateSelectionRightAction->setShortcut(QKeySequence(Qt::Key_Right));
    _defaultShortcuts["navigate_right"] = QList<QKeySequence>() << navigateSelectionRightAction->shortcut();
    connect(navigateSelectionRightAction, SIGNAL(triggered()), this, SLOT(navigateSelectionRight()));
    editMB->addAction(navigateSelectionRightAction);
    _actionMap["navigate_right"] = navigateSelectionRightAction;

    editMB->addSeparator();

    QAction *copyAction = new QAction(tr("Copy events"), this);
    _activateWithSelections.append(copyAction);
    Appearance::setActionIcon(copyAction, ":/run_environment/graphics/tool/copy.png");
    copyAction->setShortcut(QKeySequence::Copy);
    _defaultShortcuts["copy"] = QList<QKeySequence>() << copyAction->shortcut();
    connect(copyAction, SIGNAL(triggered()), this, SLOT(copy()));
    editMB->addAction(copyAction);
    _actionMap["copy"] = copyAction;

    _pasteAction = new QAction(tr("Paste events"), this);
    _pasteAction->setToolTip(tr("Paste events at cursor position"));
    _pasteAction->setShortcut(QKeySequence::Paste);
    _defaultShortcuts["paste"] = QList<QKeySequence>() << _pasteAction->shortcut();
    Appearance::setActionIcon(_pasteAction, ":/run_environment/graphics/tool/paste.png");
    connect(_pasteAction, SIGNAL(triggered()), this, SLOT(paste()));
    _actionMap["paste"] = _pasteAction;

    _pasteToTrackMenu = new QMenu(tr("Paste to track..."));
    _pasteToChannelMenu = new QMenu(tr("Paste to channel..."));
    _pasteOptionsMenu = new QMenu(tr("Paste options..."));
    _pasteOptionsMenu->addMenu(_pasteToChannelMenu);
    QActionGroup *pasteChannelGroup = new QActionGroup(this);
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
        QAction *pasteToChannelAction = new QAction(text, this);
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

    QAction *configAction = new QAction(tr("Settings"), this);
    Appearance::setActionIcon(configAction, ":/run_environment/graphics/tool/config.png");
    connect(configAction, SIGNAL(triggered()), this, SLOT(openConfig()));
    editMB->addAction(configAction);

    // Tools
    QMenu *toolsToolsMenu = new QMenu(tr("Current tool..."), toolsMB);

    StandardTool *tool = new StandardTool();
    Tool::setCurrentTool(tool);
    stdToolAction = new ToolButton(tool, QKeySequence(Qt::Key_F1), toolsToolsMenu);
    _defaultShortcuts["standard_tool"] = QList<QKeySequence>() << stdToolAction->shortcut();
    toolsToolsMenu->addAction(stdToolAction);
    tool->buttonClick();
    _actionMap["standard_tool"] = stdToolAction;

    QAction *newNoteAction = new ToolButton(new NewNoteTool(), QKeySequence(Qt::Key_F2), toolsToolsMenu);
    _defaultShortcuts["new_note"] = QList<QKeySequence>() << newNoteAction->shortcut();
    toolsToolsMenu->addAction(newNoteAction);
    _actionMap["new_note"] = newNoteAction;
    QAction *removeNotesAction = new ToolButton(new EraserTool(), QKeySequence(Qt::Key_F3), toolsToolsMenu);
    _defaultShortcuts["remove_notes"] = QList<QKeySequence>() << removeNotesAction->shortcut();
    toolsToolsMenu->addAction(removeNotesAction);
    _actionMap["remove_notes"] = removeNotesAction;

    toolsToolsMenu->addSeparator();

    QAction *selectSingleAction = new ToolButton(new SelectTool(SELECTION_TYPE_SINGLE), QKeySequence(Qt::Key_F4), toolsToolsMenu);
    _defaultShortcuts["select_single"] = QList<QKeySequence>() << selectSingleAction->shortcut();
    toolsToolsMenu->addAction(selectSingleAction);
    _actionMap["select_single"] = selectSingleAction;
    QAction *selectBoxAction = new ToolButton(new SelectTool(SELECTION_TYPE_BOX), QKeySequence(Qt::Key_F5), toolsToolsMenu);
    _defaultShortcuts["select_box"] = QList<QKeySequence>() << selectBoxAction->shortcut();
    toolsToolsMenu->addAction(selectBoxAction);
    _actionMap["select_box"] = selectBoxAction;
    QAction *selectLeftAction = new ToolButton(new SelectTool(SELECTION_TYPE_LEFT), QKeySequence(Qt::Key_F6), toolsToolsMenu);
    _defaultShortcuts["select_left"] = QList<QKeySequence>() << selectLeftAction->shortcut();
    toolsToolsMenu->addAction(selectLeftAction);
    _actionMap["select_left"] = selectLeftAction;
    QAction *selectRightAction = new ToolButton(new SelectTool(SELECTION_TYPE_RIGHT), QKeySequence(Qt::Key_F7), toolsToolsMenu);
    _defaultShortcuts["select_right"] = QList<QKeySequence>() << selectRightAction->shortcut();
    toolsToolsMenu->addAction(selectRightAction);
    _actionMap["select_right"] = selectRightAction;

    toolsToolsMenu->addSeparator();

    QAction *moveAllAction = new ToolButton(new EventMoveTool(true, true), QKeySequence(Qt::Key_F8), toolsToolsMenu);
    _defaultShortcuts["move_all"] = QList<QKeySequence>() << moveAllAction->shortcut();
    _activateWithSelections.append(moveAllAction);
    toolsToolsMenu->addAction(moveAllAction);
    _actionMap["move_all"] = moveAllAction;

    QAction *moveLRAction = new ToolButton(new EventMoveTool(false, true), QKeySequence(Qt::Key_F9), toolsToolsMenu);
    _defaultShortcuts["move_lr"] = QList<QKeySequence>() << moveLRAction->shortcut();
    _activateWithSelections.append(moveLRAction);
    toolsToolsMenu->addAction(moveLRAction);
    _actionMap["move_lr"] = moveLRAction;

    QAction *moveUDAction = new ToolButton(new EventMoveTool(true, false), QKeySequence(Qt::Key_F10), toolsToolsMenu);
    _defaultShortcuts["move_ud"] = QList<QKeySequence>() << moveUDAction->shortcut();
    _activateWithSelections.append(moveUDAction);
    toolsToolsMenu->addAction(moveUDAction);
    _actionMap["move_ud"] = moveUDAction;

    QAction *sizeChangeAction = new ToolButton(new SizeChangeTool(), QKeySequence(Qt::Key_F11), toolsToolsMenu);
    _defaultShortcuts["size_change"] = QList<QKeySequence>() << sizeChangeAction->shortcut();
    _activateWithSelections.append(sizeChangeAction);
    toolsToolsMenu->addAction(sizeChangeAction);
    _actionMap["size_change"] = sizeChangeAction;

    toolsToolsMenu->addSeparator();

    QAction *measureAction = new ToolButton(new MeasureTool(), QKeySequence(QKeyCombination(Qt::CTRL, Qt::Key_F1)), toolsToolsMenu);
    _defaultShortcuts["measure"] = QList<QKeySequence>() << measureAction->shortcut();
    toolsToolsMenu->addAction(measureAction);
    _actionMap["measure"] = measureAction;
    QAction *timeSignatureAction = new ToolButton(new TimeSignatureTool(), QKeySequence(QKeyCombination(Qt::CTRL, Qt::Key_F2)), toolsToolsMenu);
    _defaultShortcuts["time_signature"] = QList<QKeySequence>() << timeSignatureAction->shortcut();
    toolsToolsMenu->addAction(timeSignatureAction);
    _actionMap["time_signature"] = timeSignatureAction;
    QAction *tempoAction = new ToolButton(new TempoTool(), QKeySequence(QKeyCombination(Qt::CTRL, Qt::Key_F3)), toolsToolsMenu);
    _defaultShortcuts["tempo"] = QList<QKeySequence>() << tempoAction->shortcut();
    toolsToolsMenu->addAction(tempoAction);
    _actionMap["tempo"] = tempoAction;

    toolsMB->addMenu(toolsToolsMenu);

    // Tweak

    QMenu *tweakMenu = new QMenu(tr("Tweak..."), toolsMB);

    QAction *tweakTimeAction = new QAction(tr("Time"), tweakMenu);
    tweakTimeAction->setShortcut(QKeySequence(QKeyCombination(Qt::CTRL, Qt::Key_1)));
    _defaultShortcuts["tweak_time"] = QList<QKeySequence>() << tweakTimeAction->shortcut();
    tweakTimeAction->setCheckable(true);
    connect(tweakTimeAction, SIGNAL(triggered()), this, SLOT(tweakTime()));
    tweakMenu->addAction(tweakTimeAction);
    _actionMap["tweak_time"] = tweakTimeAction;

    QAction *tweakStartTimeAction = new QAction(tr("Start time"), tweakMenu);
    tweakStartTimeAction->setShortcut(QKeySequence(QKeyCombination(Qt::CTRL, Qt::Key_2)));
    _defaultShortcuts["tweak_start_time"] = QList<QKeySequence>() << tweakStartTimeAction->shortcut();
    tweakStartTimeAction->setCheckable(true);
    connect(tweakStartTimeAction, SIGNAL(triggered()), this, SLOT(tweakStartTime()));
    tweakMenu->addAction(tweakStartTimeAction);
    _actionMap["tweak_start_time"] = tweakStartTimeAction;

    QAction *tweakEndTimeAction = new QAction(tr("End time"), tweakMenu);
    tweakEndTimeAction->setShortcut(QKeySequence(QKeyCombination(Qt::CTRL, Qt::Key_3)));
    _defaultShortcuts["tweak_end_time"] = QList<QKeySequence>() << tweakEndTimeAction->shortcut();
    tweakEndTimeAction->setCheckable(true);
    connect(tweakEndTimeAction, SIGNAL(triggered()), this, SLOT(tweakEndTime()));
    tweakMenu->addAction(tweakEndTimeAction);
    _actionMap["tweak_end_time"] = tweakEndTimeAction;

    QAction *tweakNoteAction = new QAction(tr("Note"), tweakMenu);
    tweakNoteAction->setShortcut(QKeySequence(QKeyCombination(Qt::CTRL, Qt::Key_4)));
    _defaultShortcuts["tweak_note"] = QList<QKeySequence>() << tweakNoteAction->shortcut();
    tweakNoteAction->setCheckable(true);
    connect(tweakNoteAction, SIGNAL(triggered()), this, SLOT(tweakNote()));
    tweakMenu->addAction(tweakNoteAction);
    _actionMap["tweak_note"] = tweakNoteAction;

    QAction *tweakValueAction = new QAction(tr("Value"), tweakMenu);
    tweakValueAction->setShortcut(QKeySequence(QKeyCombination(Qt::CTRL, Qt::Key_5)));
    _defaultShortcuts["tweak_value"] = QList<QKeySequence>() << tweakValueAction->shortcut();
    tweakValueAction->setCheckable(true);
    connect(tweakValueAction, SIGNAL(triggered()), this, SLOT(tweakValue()));
    tweakMenu->addAction(tweakValueAction);
    _actionMap["tweak_value"] = tweakValueAction;

    QActionGroup *tweakTargetActionGroup = new QActionGroup(this);
    tweakTargetActionGroup->setExclusive(true);
    tweakTargetActionGroup->addAction(tweakTimeAction);
    tweakTargetActionGroup->addAction(tweakStartTimeAction);
    tweakTargetActionGroup->addAction(tweakEndTimeAction);
    tweakTargetActionGroup->addAction(tweakNoteAction);
    tweakTargetActionGroup->addAction(tweakValueAction);
    tweakTimeAction->setChecked(true);

    tweakMenu->addSeparator();

    QAction *tweakSmallDecreaseAction = new QAction(tr("Small decrease"), tweakMenu);
    tweakSmallDecreaseAction->setShortcut(QKeySequence(QKeyCombination(Qt::CTRL, Qt::Key_9)));
    _defaultShortcuts["tweak_small_decrease"] = QList<QKeySequence>() << tweakSmallDecreaseAction->shortcut();
    connect(tweakSmallDecreaseAction, SIGNAL(triggered()), this, SLOT(tweakSmallDecrease()));
    tweakMenu->addAction(tweakSmallDecreaseAction);
    _actionMap["tweak_small_decrease"] = tweakSmallDecreaseAction;

    QAction *tweakSmallIncreaseAction = new QAction(tr("Small increase"), tweakMenu);
    tweakSmallIncreaseAction->setShortcut(QKeySequence(QKeyCombination(Qt::CTRL, Qt::Key_0)));
    _defaultShortcuts["tweak_small_increase"] = QList<QKeySequence>() << tweakSmallIncreaseAction->shortcut();
    connect(tweakSmallIncreaseAction, SIGNAL(triggered()), this, SLOT(tweakSmallIncrease()));
    tweakMenu->addAction(tweakSmallIncreaseAction);
    _actionMap["tweak_small_increase"] = tweakSmallIncreaseAction;

    QAction *tweakMediumDecreaseAction = new QAction(tr("Medium decrease"), tweakMenu);
    tweakMediumDecreaseAction->setShortcut(QKeySequence(QKeyCombination(Qt::CTRL | Qt::ALT, Qt::Key_9)));
    _defaultShortcuts["tweak_medium_decrease"] = QList<QKeySequence>() << tweakMediumDecreaseAction->shortcut();
    connect(tweakMediumDecreaseAction, SIGNAL(triggered()), this, SLOT(tweakMediumDecrease()));
    tweakMenu->addAction(tweakMediumDecreaseAction);
    _actionMap["tweak_medium_decrease"] = tweakMediumDecreaseAction;

    QAction *tweakMediumIncreaseAction = new QAction(tr("Medium increase"), tweakMenu);
    tweakMediumIncreaseAction->setShortcut(QKeySequence(QKeyCombination(Qt::CTRL | Qt::ALT, Qt::Key_0)));
    _defaultShortcuts["tweak_medium_increase"] = QList<QKeySequence>() << tweakMediumIncreaseAction->shortcut();
    connect(tweakMediumIncreaseAction, SIGNAL(triggered()), this, SLOT(tweakMediumIncrease()));
    tweakMenu->addAction(tweakMediumIncreaseAction);
    _actionMap["tweak_medium_increase"] = tweakMediumIncreaseAction;

    QAction *tweakLargeDecreaseAction = new QAction(tr("Large decrease"), tweakMenu);
    tweakLargeDecreaseAction->setShortcut(QKeySequence(QKeyCombination(Qt::CTRL | Qt::ALT | Qt::SHIFT, Qt::Key_9)));
    _defaultShortcuts["tweak_large_decrease"] = QList<QKeySequence>() << tweakLargeDecreaseAction->shortcut();
    connect(tweakLargeDecreaseAction, SIGNAL(triggered()), this, SLOT(tweakLargeDecrease()));
    tweakMenu->addAction(tweakLargeDecreaseAction);
    _actionMap["tweak_large_decrease"] = tweakLargeDecreaseAction;

    QAction *tweakLargeIncreaseAction = new QAction(tr("Large increase"), tweakMenu);
    tweakLargeIncreaseAction->setShortcut(QKeySequence(QKeyCombination(Qt::CTRL | Qt::ALT | Qt::SHIFT, Qt::Key_0)));
    _defaultShortcuts["tweak_large_increase"] = QList<QKeySequence>() << tweakLargeIncreaseAction->shortcut();
    connect(tweakLargeIncreaseAction, SIGNAL(triggered()), this, SLOT(tweakLargeIncrease()));
    tweakMenu->addAction(tweakLargeIncreaseAction);
    _actionMap["tweak_large_increase"] = tweakLargeIncreaseAction;

    toolsMB->addMenu(tweakMenu);

    QAction *deleteAction = new QAction(tr("Remove events"), this);
    _activateWithSelections.append(deleteAction);
    deleteAction->setToolTip(tr("Remove selected events"));
    deleteAction->setShortcut(QKeySequence::Delete);
    _defaultShortcuts["delete"] = QList<QKeySequence>() << deleteAction->shortcut();
    Appearance::setActionIcon(deleteAction, ":/run_environment/graphics/tool/eraser.png");
    connect(deleteAction, SIGNAL(triggered()), this, SLOT(deleteSelectedEvents()));
    toolsMB->addAction(deleteAction);
    _actionMap["delete"] = deleteAction;

    toolsMB->addSeparator();

    QAction *alignLeftAction = new QAction(tr("Align left"), this);
    _activateWithSelections.append(alignLeftAction);
    alignLeftAction->setShortcut(QKeySequence(QKeyCombination(Qt::CTRL, Qt::Key_Left)));
    _defaultShortcuts["align_left"] = QList<QKeySequence>() << alignLeftAction->shortcut();
    Appearance::setActionIcon(alignLeftAction, ":/run_environment/graphics/tool/align_left.png");
    connect(alignLeftAction, SIGNAL(triggered()), this, SLOT(alignLeft()));
    toolsMB->addAction(alignLeftAction);
    _actionMap["align_left"] = alignLeftAction;

    QAction *alignRightAction = new QAction(tr("Align right"), this);
    _activateWithSelections.append(alignRightAction);
    Appearance::setActionIcon(alignRightAction, ":/run_environment/graphics/tool/align_right.png");
    alignRightAction->setShortcut(QKeySequence(QKeyCombination(Qt::CTRL, Qt::Key_Right)));
    _defaultShortcuts["align_right"] = QList<QKeySequence>() << alignRightAction->shortcut();
    connect(alignRightAction, SIGNAL(triggered()), this, SLOT(alignRight()));
    toolsMB->addAction(alignRightAction);
    _actionMap["align_right"] = alignRightAction;

    QAction *equalizeAction = new QAction(tr("Equalize selection"), this);
    _activateWithSelections.append(equalizeAction);
    Appearance::setActionIcon(equalizeAction, ":/run_environment/graphics/tool/equalize.png");
    equalizeAction->setShortcut(QKeySequence(QKeyCombination(Qt::CTRL, Qt::Key_Up)));
    _defaultShortcuts["equalize"] = QList<QKeySequence>() << equalizeAction->shortcut();
    connect(equalizeAction, SIGNAL(triggered()), this, SLOT(equalize()));
    toolsMB->addAction(equalizeAction);
    _actionMap["equalize"] = equalizeAction;

    toolsMB->addSeparator();

    QAction *glueNotesAction = new QAction(tr("Glue notes (same channel)"), this);
    glueNotesAction->setShortcut(QKeySequence(QKeyCombination(Qt::CTRL, Qt::Key_G)));
    _defaultShortcuts["glue"] = QList<QKeySequence>() << glueNotesAction->shortcut();
    Appearance::setActionIcon(glueNotesAction, ":/run_environment/graphics/tool/glue.png");
    connect(glueNotesAction, SIGNAL(triggered()), this, SLOT(glueSelection()));
    _activateWithSelections.append(glueNotesAction);
    toolsMB->addAction(glueNotesAction);
    _actionMap["glue"] = glueNotesAction;

    QAction *glueNotesAllChannelsAction = new QAction(tr("Glue notes (all channels)"), this);
    glueNotesAllChannelsAction->setShortcut(QKeySequence(QKeyCombination(Qt::CTRL | Qt::SHIFT, Qt::Key_G)));
    _defaultShortcuts["glue_all_channels"] = QList<QKeySequence>() << glueNotesAllChannelsAction->shortcut();
    Appearance::setActionIcon(glueNotesAllChannelsAction, ":/run_environment/graphics/tool/glue.png");
    connect(glueNotesAllChannelsAction, SIGNAL(triggered()), this, SLOT(glueSelectionAllChannels()));
    _activateWithSelections.append(glueNotesAllChannelsAction);
    toolsMB->addAction(glueNotesAllChannelsAction);
    _actionMap["glue_all_channels"] = glueNotesAllChannelsAction;

    QAction *scissorsAction = new ToolButton(new ScissorsTool(), QKeySequence(QKeyCombination(Qt::CTRL, Qt::Key_X)), toolsMB);
    _defaultShortcuts["scissors"] = QList<QKeySequence>() << scissorsAction->shortcut();
    toolsMB->addAction(scissorsAction);
    _actionMap["scissors"] = scissorsAction;

    QAction *deleteOverlapsAction = new QAction(tr("Delete overlaps"), this);
    deleteOverlapsAction->setShortcut(QKeySequence(QKeyCombination(Qt::CTRL, Qt::Key_D)));
    _defaultShortcuts["delete_overlaps"] = QList<QKeySequence>() << deleteOverlapsAction->shortcut();
    Appearance::setActionIcon(deleteOverlapsAction, ":/run_environment/graphics/tool/deleteoverlap.png");
    connect(deleteOverlapsAction, SIGNAL(triggered()), this, SLOT(deleteOverlaps()));
    _activateWithSelections.append(deleteOverlapsAction);
    toolsMB->addAction(deleteOverlapsAction);
    _actionMap["delete_overlaps"] = deleteOverlapsAction;

    toolsMB->addSeparator();

    QAction *convertPitchBendAction = new QAction(tr("Convert pitch bends to notes"), this);
    convertPitchBendAction->setShortcut(QKeySequence(QKeyCombination(Qt::CTRL, Qt::Key_B)));
    _defaultShortcuts["convert_pitch_bend_to_notes"] = QList<QKeySequence>() << convertPitchBendAction->shortcut();
    connect(convertPitchBendAction, SIGNAL(triggered()), this, SLOT(convertPitchBendToNotes()));
    toolsMB->addAction(convertPitchBendAction);
    _actionMap["convert_pitch_bend_to_notes"] = convertPitchBendAction;

    QAction *explodeChordsAction = new QAction(tr("Explode chords to tracks"), this);
    explodeChordsAction->setShortcut(QKeySequence(QKeyCombination(Qt::CTRL, Qt::Key_E)));
    _defaultShortcuts["explode_chords_to_tracks"] = QList<QKeySequence>() << explodeChordsAction->shortcut();
    connect(explodeChordsAction, SIGNAL(triggered()), this, SLOT(explodeChordsToTracks()));
    toolsMB->addAction(explodeChordsAction);
    _actionMap["explode_chords_to_tracks"] = explodeChordsAction;

    QAction *strumAction = new QAction(tr("Strum selection"), this);
    strumAction->setShortcut(QKeySequence(QKeyCombination(Qt::CTRL | Qt::ALT, Qt::Key_S)));
    _defaultShortcuts["strum"] = QList<QKeySequence>() << strumAction->shortcut();
    connect(strumAction, SIGNAL(triggered()), this, SLOT(strumNotes()));
    toolsMB->addAction(strumAction);
    _actionMap["strum"] = strumAction;

    toolsMB->addSeparator();

    QAction *quantizeAction = new QAction(tr("Quantify selection"), this);
    _activateWithSelections.append(quantizeAction);
    Appearance::setActionIcon(quantizeAction, ":/run_environment/graphics/tool/quantize.png");
    quantizeAction->setShortcut(QKeySequence(QKeyCombination(Qt::CTRL, Qt::Key_Q)));
    _defaultShortcuts["quantize"] = QList<QKeySequence>() << quantizeAction->shortcut();
    connect(quantizeAction, SIGNAL(triggered()), this, SLOT(quantizeSelection()));
    toolsMB->addAction(quantizeAction);
    _actionMap["quantize"] = quantizeAction;

    QMenu *quantMenu = new QMenu(tr("Quantization fractions..."), viewMB);
    QActionGroup *quantGroup = new QActionGroup(viewMB);
    quantGroup->setExclusive(true);

    // Regular Notes submenu
    QMenu *quantRegularMenu = new QMenu(tr("Regular Notes"), quantMenu);
    quantMenu->addMenu(quantRegularMenu);

    for (int i = 0; i <= 5; i++) {
        QVariant variant(i);
        QString text;
        if (i == 0) {
            text = tr("Whole note");
        } else if (i == 1) {
            text = tr("Half note");
        } else if (i == 2) {
            text = tr("Quarter note");
        } else {
            int noteValue = (int) qPow(2, i);
            if (noteValue == 32) {
                text = tr("32nd note");
            } else {
                text = QString::number(noteValue) + tr("th note");
            }
        }
        QAction *a = new QAction(text, this);
        a->setData(variant);
        quantGroup->addAction(a);
        quantRegularMenu->addAction(a);
        a->setCheckable(true);
        a->setChecked(i == _quantizationGrid);
    }

    quantMenu->addSeparator();

    // Triplets submenu
    QMenu *quantTripletsMenu = new QMenu(tr("Triplets"), quantMenu);
    quantMenu->addMenu(quantTripletsMenu);

    QStringList tripletNames = QStringList()
                               << tr("Whole note triplets")
                               << tr("Half note triplets")
                               << tr("Quarter note triplets")
                               << tr("8th note triplets")
                               << tr("16th note triplets")
                               << tr("32nd note triplets");

    for (int i = 0; i < tripletNames.size(); i++) {
        QAction *tripletAction = new QAction(tripletNames[i], this);
        tripletAction->setData(QVariant(-100 - i));
        quantGroup->addAction(tripletAction);
        quantTripletsMenu->addAction(tripletAction);
        tripletAction->setCheckable(true);
        tripletAction->setChecked((-100 - i) == _quantizationGrid);
    }

    quantMenu->addSeparator();

    // Other Tuplets submenu
    QMenu *quantTupletsMenu = new QMenu(tr("Other Tuplets"), quantMenu);
    quantMenu->addMenu(quantTupletsMenu);

    // Quintuplets submenu (most commonly used tuplets after triplets)
    QMenu *quantQuintupletsMenu = new QMenu(tr("Quintuplets"), quantTupletsMenu);
    quantTupletsMenu->addMenu(quantQuintupletsMenu);

    QStringList quintupletNames = QStringList()
                                  << tr("Quarter quintuplets")
                                  << tr("8th quintuplets")
                                  << tr("16th quintuplets");
    for (int i = 0; i < quintupletNames.size(); i++) {
        QAction *quintupletAction = new QAction(quintupletNames[i], this);
        quintupletAction->setData(QVariant(-202 - i));
        quantGroup->addAction(quintupletAction);
        quantQuintupletsMenu->addAction(quintupletAction);
        quintupletAction->setCheckable(true);
        quintupletAction->setChecked((-202 - i) == _quantizationGrid);
    }

    // Sextuplets submenu (for advanced users)
    QMenu *quantSextupletsMenu = new QMenu(tr("Sextuplets"), quantTupletsMenu);
    quantTupletsMenu->addMenu(quantSextupletsMenu);

    QStringList quantSextupletNames = QStringList()
                                      << tr("Quarter sextuplets")
                                      << tr("8th sextuplets");
    for (int i = 0; i < quantSextupletNames.size(); i++) {
        QAction *sextupletAction = new QAction(quantSextupletNames[i], this);
        sextupletAction->setData(QVariant(-302 - i));
        quantGroup->addAction(sextupletAction);
        quantSextupletsMenu->addAction(sextupletAction);
        sextupletAction->setCheckable(true);
        sextupletAction->setChecked((-302 - i) == _quantizationGrid);
    }

    quantMenu->addSeparator();

    // Dotted Notes submenu (important for compound meters)
    QMenu *quantDottedMenu = new QMenu(tr("Dotted Notes"), quantMenu);
    quantMenu->addMenu(quantDottedMenu);

    QStringList quantDottedNames = QStringList()
                                   << tr("Dotted quarter notes")
                                   << tr("Dotted 8th notes");
    QList<int> quantDottedValues = QList<int>() << -502 << -503;

    for (int i = 0; i < quantDottedNames.size(); i++) {
        QAction *dottedAction = new QAction(quantDottedNames[i], this);
        dottedAction->setData(QVariant(quantDottedValues[i]));
        quantGroup->addAction(dottedAction);
        quantDottedMenu->addAction(dottedAction);
        dottedAction->setCheckable(true);
        dottedAction->setChecked(quantDottedValues[i] == _quantizationGrid);
    }

    connect(quantMenu, SIGNAL(triggered(QAction*)), this, SLOT(quantizationChanged(QAction*)));
    toolsMB->addMenu(quantMenu);

    QAction *quantizeNToleAction = new QAction(tr("Quantify tuplet"), this);
    _activateWithSelections.append(quantizeNToleAction);
    quantizeNToleAction->setShortcut(QKeySequence(QKeyCombination(Qt::CTRL | Qt::SHIFT, Qt::Key_H)));
    _defaultShortcuts["quantize_ntuplet_dialog"] = QList<QKeySequence>() << quantizeNToleAction->shortcut();
    connect(quantizeNToleAction, SIGNAL(triggered()), this, SLOT(quantizeNtoleDialog()));
    toolsMB->addAction(quantizeNToleAction);
    _actionMap["quantize_ntuplet_dialog"] = quantizeNToleAction;

    QAction *quantizeNToleActionRepeat = new QAction(tr("Repeat tuplet quantization"), this);
    _activateWithSelections.append(quantizeNToleActionRepeat);
    quantizeNToleActionRepeat->setShortcut(QKeySequence(QKeyCombination(Qt::CTRL, Qt::Key_H)));
    _defaultShortcuts["quantize_ntuplet_repeat"] = QList<QKeySequence>() << quantizeNToleActionRepeat->shortcut();
    connect(quantizeNToleActionRepeat, SIGNAL(triggered()), this, SLOT(quantizeNtole()));
    toolsMB->addAction(quantizeNToleActionRepeat);
    _actionMap["quantize_ntuplet_repeat"] = quantizeNToleActionRepeat;

    toolsMB->addSeparator();

    QAction *transposeAction = new QAction(tr("Transpose selection"), this);
    Appearance::setActionIcon(transposeAction, ":/run_environment/graphics/tool/transpose.png");
    _activateWithSelections.append(transposeAction);
    transposeAction->setShortcut(QKeySequence(QKeyCombination(Qt::CTRL, Qt::Key_T)));
    _defaultShortcuts["transpose"] = QList<QKeySequence>() << transposeAction->shortcut();
    connect(transposeAction, SIGNAL(triggered()), this, SLOT(transposeNSemitones()));
    toolsMB->addAction(transposeAction);
    _actionMap["transpose"] = transposeAction;

    QAction *transposeOctaveUpAction = new QAction(tr("Transpose octave up"), this);
    Appearance::setActionIcon(transposeOctaveUpAction, ":/run_environment/graphics/tool/transpose_up.png");
    _activateWithSelections.append(transposeOctaveUpAction);
    transposeOctaveUpAction->setShortcut(QKeySequence(QKeyCombination(Qt::SHIFT, Qt::Key_Up)));
    _defaultShortcuts["transpose_up"] = QList<QKeySequence>() << transposeOctaveUpAction->shortcut();
    connect(transposeOctaveUpAction, SIGNAL(triggered()), this, SLOT(transposeSelectedNotesOctaveUp()));
    toolsMB->addAction(transposeOctaveUpAction);
    _actionMap["transpose_up"] = transposeOctaveUpAction;

    QAction *transposeOctaveDownAction = new QAction(tr("Transpose octave down"), this);
    Appearance::setActionIcon(transposeOctaveDownAction, ":/run_environment/graphics/tool/transpose_down.png");
    _activateWithSelections.append(transposeOctaveDownAction);
    transposeOctaveDownAction->setShortcut(QKeySequence(QKeyCombination(Qt::SHIFT, Qt::Key_Down)));
    _defaultShortcuts["transpose_down"] = QList<QKeySequence>() << transposeOctaveDownAction->shortcut();
    connect(transposeOctaveDownAction, SIGNAL(triggered()), this, SLOT(transposeSelectedNotesOctaveDown()));
    toolsMB->addAction(transposeOctaveDownAction);
    _actionMap["transpose_down"] = transposeOctaveDownAction;

    toolsMB->addSeparator();

    QAction *addTrackAction = new QAction(tr("Add track"), toolsMB);
    toolsMB->addAction(addTrackAction);
    connect(addTrackAction, SIGNAL(triggered()), this, SLOT(addTrack()));

    toolsMB->addSeparator();

    _deleteChannelMenu = new QMenu(tr("Remove events from channel..."), toolsMB);
    toolsMB->addMenu(_deleteChannelMenu);
    connect(_deleteChannelMenu, SIGNAL(triggered(QAction*)), this, SLOT(deleteChannel(QAction*)));

    for (int i = 0; i < 16; i++) {
        QVariant variant(i);
        QAction *delChannelAction = new QAction(QString::number(i), this);
        delChannelAction->setData(variant);
        _deleteChannelMenu->addAction(delChannelAction);
    }

    _moveSelectedEventsToChannelMenu = new QMenu(tr("Move events to channel..."), editMB);
    toolsMB->addMenu(_moveSelectedEventsToChannelMenu);
    connect(_moveSelectedEventsToChannelMenu, SIGNAL(triggered(QAction*)), this, SLOT(moveSelectedEventsToChannel(QAction*)));

    for (int i = 0; i < 16; i++) {
        QVariant variant(i);
        QAction *moveToChannelAction = new QAction(QString::number(i), this);
        moveToChannelAction->setData(variant);
        _moveSelectedEventsToChannelMenu->addAction(moveToChannelAction);
    }

    _moveSelectedEventsToTrackMenu = new QMenu(tr("Move events to track..."), editMB);
    toolsMB->addMenu(_moveSelectedEventsToTrackMenu);
    connect(_moveSelectedEventsToTrackMenu, SIGNAL(triggered(QAction*)), this, SLOT(moveSelectedEventsToTrack(QAction*)));

    toolsMB->addSeparator();

    QAction *setFileLengthMs = new QAction(tr("Set file duration"), this);
    connect(setFileLengthMs, SIGNAL(triggered()), this, SLOT(setFileLengthMs()));
    toolsMB->addAction(setFileLengthMs);

    QAction *scaleSelection = new QAction(tr("Scale events"), this);
    _activateWithSelections.append(scaleSelection);
    connect(scaleSelection, SIGNAL(triggered()), this, SLOT(scaleSelection()));
    toolsMB->addAction(scaleSelection);
    _actionMap["scale_selection"] = scaleSelection;

    toolsMB->addSeparator();

    QAction *magnetAction = new QAction(tr("Magnet"), editMB);
    toolsMB->addAction(magnetAction);
    magnetAction->setShortcut(QKeySequence(QKeyCombination(Qt::CTRL, Qt::Key_M)));
    _defaultShortcuts["magnet"] = QList<QKeySequence>() << magnetAction->shortcut();
    Appearance::setActionIcon(magnetAction, ":/run_environment/graphics/tool/magnet.png");
    magnetAction->setCheckable(true);
    magnetAction->setChecked(false);
    magnetAction->setChecked(EventTool::magnetEnabled());
    connect(magnetAction, SIGNAL(toggled(bool)), this, SLOT(enableMagnet(bool)));
    _actionMap["magnet"] = magnetAction;

    // View
    QMenu *zoomMenu = new QMenu(tr("Zoom..."), viewMB);
    QAction *zoomHorOutAction = new QAction(tr("Horizontal out"), this);
    zoomHorOutAction->setShortcut(QKeySequence(QKeyCombination(Qt::CTRL, Qt::Key_Minus)));
    _defaultShortcuts["zoom_hor_out"] = QList<QKeySequence>() << zoomHorOutAction->shortcut();
    Appearance::setActionIcon(zoomHorOutAction, ":/run_environment/graphics/tool/zoom_hor_out.png");
    connect(zoomHorOutAction, SIGNAL(triggered()), _matrixWidgetContainer, SLOT(zoomHorOut()));
    zoomMenu->addAction(zoomHorOutAction);
    _actionMap["zoom_hor_out"] = zoomHorOutAction;

    QAction *zoomHorInAction = new QAction(tr("Horizontal in"), this);
    Appearance::setActionIcon(zoomHorInAction, ":/run_environment/graphics/tool/zoom_hor_in.png");
    zoomHorInAction->setShortcut(QKeySequence(QKeyCombination(Qt::CTRL, Qt::Key_Equal)));
    _defaultShortcuts["zoom_hor_in"] = QList<QKeySequence>() << zoomHorInAction->shortcut();
    connect(zoomHorInAction, SIGNAL(triggered()), _matrixWidgetContainer, SLOT(zoomHorIn()));
    zoomMenu->addAction(zoomHorInAction);
    _actionMap["zoom_hor_in"] = zoomHorInAction;

    QAction *zoomVerOutAction = new QAction(tr("Vertical out"), this);
    Appearance::setActionIcon(zoomVerOutAction, ":/run_environment/graphics/tool/zoom_ver_out.png");
    zoomVerOutAction->setShortcut(QKeySequence(QKeyCombination(Qt::SHIFT, Qt::Key_Minus)));
    _defaultShortcuts["zoom_ver_out"] = QList<QKeySequence>() << zoomVerOutAction->shortcut();
    connect(zoomVerOutAction, SIGNAL(triggered()), _matrixWidgetContainer, SLOT(zoomVerOut()));
    zoomMenu->addAction(zoomVerOutAction);
    _actionMap["zoom_ver_out"] = zoomVerOutAction;

    QAction *zoomVerInAction = new QAction(tr("Vertical in"), this);
    Appearance::setActionIcon(zoomVerInAction, ":/run_environment/graphics/tool/zoom_ver_in.png");
    zoomVerInAction->setShortcut(QKeySequence(QKeyCombination(Qt::SHIFT, Qt::Key_Equal)));
    _defaultShortcuts["zoom_ver_in"] = QList<QKeySequence>() << zoomVerInAction->shortcut();
    connect(zoomVerInAction, SIGNAL(triggered()), _matrixWidgetContainer, SLOT(zoomVerIn()));
    zoomMenu->addAction(zoomVerInAction);
    _actionMap["zoom_ver_in"] = zoomVerInAction;

    zoomMenu->addSeparator();

    QAction *zoomStdAction = new QAction(tr("Restore default zoom"), this);
    zoomStdAction->setShortcut(QKeySequence(QKeyCombination(Qt::CTRL, Qt::Key_Backspace)));
    _defaultShortcuts["zoom_std"] = QList<QKeySequence>() << zoomStdAction->shortcut();
    connect(zoomStdAction, SIGNAL(triggered()), _matrixWidgetContainer, SLOT(zoomStd()));
    zoomMenu->addAction(zoomStdAction);
    _actionMap["zoom_std"] = zoomStdAction;

    viewMB->addMenu(zoomMenu);

    viewMB->addSeparator();

    QAction *resetViewAction = new QAction(tr("Reset view"), this);
    resetViewAction->setShortcut(QKeySequence(QKeyCombination(Qt::CTRL | Qt::SHIFT, Qt::Key_Backspace)));
    _defaultShortcuts["reset_view"] = QList<QKeySequence>() << resetViewAction->shortcut();
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

    QMenu *colorMenu = new QMenu(tr("Colors..."), viewMB);
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

    QMenu *divMenu = new QMenu(tr("Raster..."), viewMB);
    QActionGroup *divGroup = new QActionGroup(viewMB);
    divGroup->setExclusive(true);

    // Off option
    QAction *offAction = new QAction(tr("Off"), this);
    offAction->setData(QVariant(-1));
    divGroup->addAction(offAction);
    divMenu->addAction(offAction);
    offAction->setCheckable(true);
    offAction->setChecked(-1 == mw_matrixWidget->div());

    divMenu->addSeparator();

    // Regular Notes submenu
    QMenu *regularMenu = new QMenu(tr("Regular Notes"), divMenu);
    divMenu->addMenu(regularMenu);

    for (int i = 0; i <= 5; i++) {
        QVariant variant(i);
        QString text;
        if (i == 0) {
            text = tr("Whole note");
        } else if (i == 1) {
            text = tr("Half note");
        } else if (i == 2) {
            text = tr("Quarter note");
        } else {
            int noteValue = (int) qPow(2, i);
            if (noteValue == 32) {
                text = tr("32nd note");
            } else {
                text = QString::number(noteValue) + tr("th note");
            }
        }
        QAction *a = new QAction(text, this);
        a->setData(variant);
        divGroup->addAction(a);
        regularMenu->addAction(a);
        a->setCheckable(true);
        a->setChecked(i == mw_matrixWidget->div());
    }

    divMenu->addSeparator();

    // Triplets submenu
    QMenu *tripletsMenu = new QMenu(tr("Triplets"), divMenu);
    divMenu->addMenu(tripletsMenu);

    QStringList rasterTripletNames = QStringList()
                                     << tr("Whole note triplets")
                                     << tr("Half note triplets")
                                     << tr("Quarter note triplets")
                                     << tr("8th note triplets")
                                     << tr("16th note triplets")
                                     << tr("32nd note triplets");

    for (int i = 0; i < rasterTripletNames.size(); i++) {
        QAction *tripletAction = new QAction(rasterTripletNames[i], this);
        tripletAction->setData(QVariant(-100 - i));
        divGroup->addAction(tripletAction);
        tripletsMenu->addAction(tripletAction);
        tripletAction->setCheckable(true);
        tripletAction->setChecked((-100 - i) == mw_matrixWidget->div());
    }

    divMenu->addSeparator();

    // Other Tuplets submenu
    QMenu *tupletsMenu = new QMenu(tr("Other Tuplets"), divMenu);
    divMenu->addMenu(tupletsMenu);

    // Quintuplets submenu
    QMenu *quintupletsMenu = new QMenu(tr("Quintuplets"), tupletsMenu);
    tupletsMenu->addMenu(quintupletsMenu);

    QStringList rasterQuintupletNames = QStringList()
                                        << tr("Quarter quintuplets")
                                        << tr("8th quintuplets")
                                        << tr("16th quintuplets");
    for (int i = 0; i < rasterQuintupletNames.size(); i++) {
        QAction *quintupletAction = new QAction(rasterQuintupletNames[i], this);
        quintupletAction->setData(QVariant(-202 - i)); // -202, -203, -204
        divGroup->addAction(quintupletAction);
        quintupletsMenu->addAction(quintupletAction);
        quintupletAction->setCheckable(true);
        quintupletAction->setChecked((-202 - i) == mw_matrixWidget->div());
    }

    // Sextuplets submenu
    QMenu *sextupletsMenu = new QMenu(tr("Sextuplets"), tupletsMenu);
    tupletsMenu->addMenu(sextupletsMenu);

    QStringList rasterSextupletNames = QStringList()
                                       << tr("Quarter sextuplets")
                                       << tr("8th sextuplets")
                                       << tr("16th sextuplets");
    for (int i = 0; i < rasterSextupletNames.size(); i++) {
        QAction *sextupletAction = new QAction(rasterSextupletNames[i], this);
        sextupletAction->setData(QVariant(-302 - i)); // -302, -303, -304
        divGroup->addAction(sextupletAction);
        sextupletsMenu->addAction(sextupletAction);
        sextupletAction->setCheckable(true);
        sextupletAction->setChecked((-302 - i) == mw_matrixWidget->div());
    }

    // Septuplets submenu
    QMenu *septupletsMenu = new QMenu(tr("Septuplets"), tupletsMenu);
    tupletsMenu->addMenu(septupletsMenu);

    QStringList rasterSeptupletNames = QStringList()
                                       << tr("Quarter septuplets")
                                       << tr("8th septuplets")
                                       << tr("16th septuplets");
    for (int i = 0; i < rasterSeptupletNames.size(); i++) {
        QAction *septupletAction = new QAction(rasterSeptupletNames[i], this);
        septupletAction->setData(QVariant(-402 - i)); // -402, -403, -404
        divGroup->addAction(septupletAction);
        septupletsMenu->addAction(septupletAction);
        septupletAction->setCheckable(true);
        septupletAction->setChecked((-402 - i) == mw_matrixWidget->div());
    }

    divMenu->addSeparator();

    // Dotted Notes submenu
    QMenu *dottedMenu = new QMenu(tr("Dotted Notes"), divMenu);
    divMenu->addMenu(dottedMenu);

    // Regular dotted notes
    QStringList rasterDottedNames = QStringList()
                                    << tr("Dotted half notes")
                                    << tr("Dotted quarter notes")
                                    << tr("Dotted 8th notes");
    QList<int> rasterDottedValues = QList<int>() << -501 << -502 << -503;

    for (int i = 0; i < rasterDottedNames.size(); i++) {
        QAction *dottedAction = new QAction(rasterDottedNames[i], this);
        dottedAction->setData(QVariant(rasterDottedValues[i]));
        divGroup->addAction(dottedAction);
        dottedMenu->addAction(dottedAction);
        dottedAction->setCheckable(true);
        dottedAction->setChecked(rasterDottedValues[i] == mw_matrixWidget->div());
    }

    dottedMenu->addSeparator();

    // Double dotted notes (1.75x duration)
    QAction *doubleDottedQuarter = new QAction(tr("Double dotted quarters"), this);
    doubleDottedQuarter->setData(QVariant(-602));
    divGroup->addAction(doubleDottedQuarter);
    dottedMenu->addAction(doubleDottedQuarter);
    doubleDottedQuarter->setCheckable(true);
    doubleDottedQuarter->setChecked(-602 == mw_matrixWidget->div());

    connect(divMenu, SIGNAL(triggered(QAction*)), this, SLOT(divChanged(QAction*)));
    viewMB->addMenu(divMenu);

    // Playback
    QAction *playStopAction = new QAction("PlayStop", this);
    QList<QKeySequence> playStopActionShortcuts;
    playStopActionShortcuts << QKeySequence(Qt::Key_Space)
                            << QKeySequence(QKeyCombination(Qt::CTRL, Qt::Key_P));
    playStopAction->setShortcuts(playStopActionShortcuts);
    _defaultShortcuts["play_stop"] = playStopActionShortcuts;
    connect(playStopAction, SIGNAL(triggered()), this, SLOT(playStop()));
    playbackMB->addAction(playStopAction);
    _actionMap["play_stop"] = playStopAction;

    QAction *playAction = new QAction(tr("Play"), this);
    Appearance::setActionIcon(playAction, ":/run_environment/graphics/tool/play.png");
    connect(playAction, SIGNAL(triggered()), this, SLOT(play()));
    playbackMB->addAction(playAction);
    _actionMap["play"] = playAction;

    QAction *pauseAction = new QAction(tr("Pause"), this);
    Appearance::setActionIcon(pauseAction, ":/run_environment/graphics/tool/pause.png");
#ifdef Q_OS_MAC
    pauseAction->setShortcut(QKeySequence(QKeyCombination(Qt::META, Qt::Key_Space)));
#else
    pauseAction->setShortcut(QKeySequence(QKeyCombination(Qt::CTRL, Qt::Key_Space)));
#endif
    _defaultShortcuts["pause"] = QList<QKeySequence>() << pauseAction->shortcut();
    connect(pauseAction, SIGNAL(triggered()), this, SLOT(pause()));
    playbackMB->addAction(pauseAction);
    _actionMap["pause"] = pauseAction;

    QAction *recAction = new QAction(tr("Record"), this);
    Appearance::setActionIcon(recAction, ":/run_environment/graphics/tool/record.png");
    recAction->setShortcut(QKeySequence(QKeyCombination(Qt::CTRL, Qt::Key_R)));
    _defaultShortcuts["record"] = QList<QKeySequence>() << recAction->shortcut();
    connect(recAction, SIGNAL(triggered()), this, SLOT(record()));
    playbackMB->addAction(recAction);
    _actionMap["record"] = recAction;

    QAction *stopAction = new QAction(tr("Stop"), this);
    Appearance::setActionIcon(stopAction, ":/run_environment/graphics/tool/stop.png");
    connect(stopAction, SIGNAL(triggered()), this, SLOT(stop()));
    playbackMB->addAction(stopAction);
    _actionMap["stop"] = stopAction;

    playbackMB->addSeparator();

    QAction *backToBeginAction = new QAction(tr("Back to begin"), this);
    Appearance::setActionIcon(backToBeginAction, ":/run_environment/graphics/tool/back_to_begin.png");
    QList<QKeySequence> backToBeginActionShortcuts;
    backToBeginActionShortcuts << QKeySequence(QKeyCombination(Qt::ALT, Qt::Key_Up))
                               << QKeySequence(QKeyCombination(Qt::ALT, Qt::Key_Home))
                               << QKeySequence(QKeyCombination(Qt::SHIFT, Qt::Key_J));
    backToBeginAction->setShortcuts(backToBeginActionShortcuts);
    _defaultShortcuts["back_to_begin"] = backToBeginActionShortcuts;
    connect(backToBeginAction, SIGNAL(triggered()), this, SLOT(backToBegin()));
    playbackMB->addAction(backToBeginAction);
    _actionMap["back_to_begin"] = backToBeginAction;

    QAction *backAction = new QAction(tr("Previous measure"), this);
    Appearance::setActionIcon(backAction, ":/run_environment/graphics/tool/back.png");
    QList<QKeySequence> backActionShortcuts;
    backActionShortcuts << QKeySequence(QKeyCombination(Qt::ALT, Qt::Key_Left));
    backAction->setShortcuts(backActionShortcuts);
    _defaultShortcuts["back"] = backActionShortcuts;
    connect(backAction, SIGNAL(triggered()), this, SLOT(back()));
    playbackMB->addAction(backAction);
    _actionMap["back"] = backAction;

    QAction *forwAction = new QAction(tr("Next measure"), this);
    Appearance::setActionIcon(forwAction, ":/run_environment/graphics/tool/forward.png");
    QList<QKeySequence> forwActionShortcuts;
    forwActionShortcuts << QKeySequence(QKeyCombination(Qt::ALT, Qt::Key_Right));
    forwAction->setShortcuts(forwActionShortcuts);
    _defaultShortcuts["forward"] = forwActionShortcuts;
    connect(forwAction, SIGNAL(triggered()), this, SLOT(forward()));
    playbackMB->addAction(forwAction);
    _actionMap["forward"] = forwAction;

    playbackMB->addSeparator();

    QAction *backMarkerAction = new QAction(tr("Previous marker"), this);
    Appearance::setActionIcon(backMarkerAction, ":/run_environment/graphics/tool/back_marker.png");
    QList<QKeySequence> backMarkerActionShortcuts;
    backMarkerAction->setShortcut(QKeySequence(QKeyCombination(Qt::ALT, Qt::Key_Comma)));
    _defaultShortcuts["back_marker"] = QList<QKeySequence>() << backMarkerAction->shortcut();
    connect(backMarkerAction, SIGNAL(triggered()), this, SLOT(backMarker()));
    playbackMB->addAction(backMarkerAction);
    _actionMap["back_marker"] = backMarkerAction;

    QAction *forwMarkerAction = new QAction(tr("Next marker"), this);
    Appearance::setActionIcon(forwMarkerAction, ":/run_environment/graphics/tool/forward_marker.png");
    QList<QKeySequence> forwMarkerActionShortcuts;
    forwMarkerAction->setShortcut(QKeySequence(QKeyCombination(Qt::ALT, Qt::Key_Period)));
    _defaultShortcuts["forward_marker"] = QList<QKeySequence>() << forwMarkerAction->shortcut();
    connect(forwMarkerAction, SIGNAL(triggered()), this, SLOT(forwardMarker()));
    playbackMB->addAction(forwMarkerAction);
    _actionMap["forward_marker"] = forwMarkerAction;

    playbackMB->addSeparator();

    QMenu *speedMenu = new QMenu(tr("Playback speed..."));
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
    QActionGroup *speedGroup = new QActionGroup(this);
    speedGroup->setExclusive(true);

    foreach(double s, speeds) {
        QAction *speedAction = new QAction(QString::number(s), this);
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

    QAction *lockAction = new QAction(tr("Lock screen while playing"), this);
    Appearance::setActionIcon(lockAction, ":/run_environment/graphics/tool/screen_unlocked.png");
    lockAction->setCheckable(true);
    connect(lockAction, SIGNAL(toggled(bool)), this, SLOT(screenLockPressed(bool)));
    playbackMB->addAction(lockAction);
    lockAction->setChecked(mw_matrixWidget->screenLocked());
    _actionMap["lock"] = lockAction;

    QAction *metronomeAction = new QAction(tr("Metronome"), this);
    Appearance::setActionIcon(metronomeAction, ":/run_environment/graphics/tool/metronome.png");
    metronomeAction->setCheckable(true);
    metronomeAction->setChecked(Metronome::enabled());
    connect(metronomeAction, SIGNAL(toggled(bool)), this, SLOT(enableMetronome(bool)));
    playbackMB->addAction(metronomeAction);
    _actionMap["metronome"] = metronomeAction;

    QAction *pianoEmulationAction = new QAction(tr("Piano emulation"), this);
    pianoEmulationAction->setCheckable(true);
    pianoEmulationAction->setChecked(mw_matrixWidget->getPianoEmulation());
    connect(pianoEmulationAction, SIGNAL(toggled(bool)), this, SLOT(togglePianoEmulation(bool)));
    playbackMB->addAction(pianoEmulationAction);

    // Midi
    QAction *configAction2 = new QAction(tr("Settings"), this);
    Appearance::setActionIcon(configAction2, ":/run_environment/graphics/tool/config.png");
    connect(configAction2, SIGNAL(triggered()), this, SLOT(openConfig()));
    midiMB->addAction(configAction2);

    QAction *thruAction = new QAction(tr("Connect Midi In/Out"), this);
    Appearance::setActionIcon(thruAction, ":/run_environment/graphics/tool/connection.png");
    thruAction->setCheckable(true);
    thruAction->setChecked(MidiInput::thru());
    connect(thruAction, SIGNAL(toggled(bool)), this, SLOT(enableThru(bool)));
    midiMB->addAction(thruAction);
    _actionMap["thru"] = thruAction;

    midiMB->addSeparator();

    QAction *panicAction = new QAction(tr("Midi panic"), this);
    Appearance::setActionIcon(panicAction, ":/run_environment/graphics/tool/panic.png");
    panicAction->setShortcut(QKeySequence(Qt::Key_Escape));
    _defaultShortcuts["panic"] = QList<QKeySequence>() << panicAction->shortcut();
    connect(panicAction, SIGNAL(triggered()), this, SLOT(panic()));
    midiMB->addAction(panicAction);
    _actionMap["panic"] = panicAction;

    // Help
    QAction *aboutAction = new QAction(tr("About MidiEditor"), this);
    connect(aboutAction, SIGNAL(triggered()), this, SLOT(about()));
    helpMB->addAction(aboutAction);

    QAction *manualAction = new QAction(tr("Manual"), this);
    connect(manualAction, SIGNAL(triggered()), this, SLOT(manual()));
    helpMB->addAction(manualAction);

    QAction *checkUpdatesAction = new QAction(tr("Check for updates"), this);
    connect(checkUpdatesAction, &QAction::triggered, this, [this](){ checkForUpdates(false); });
    helpMB->addAction(checkUpdatesAction);

    // Use full custom toolbar with settings integration
    // Apply any stored custom shortcuts from settings after defining defaults
    applyStoredShortcuts();

    _toolbarWidget = createCustomToolbar(parent);
    return _toolbarWidget;
}

QWidget *MainWindow::createSimpleCustomToolbar(QWidget *parent) {
    // Step 1: Simple custom toolbar with hard-coded safe layout
    // No settings dependencies, no complex logic - just basic customization

    QWidget *buttonBar = new QWidget(parent);
    QGridLayout *btnLayout = new QGridLayout(buttonBar);

    buttonBar->setLayout(btnLayout);
    btnLayout->setSpacing(0);
    buttonBar->setContentsMargins(0, 0, 0, 0);

    QToolBar *toolBar = new QToolBar("Custom", buttonBar);
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

    // Remove any trailing separators
    removeTrailingSeparators(toolBar);

    btnLayout->setColumnStretch(4, 1);
    btnLayout->addWidget(toolBar, 0, 0, 1, 1);

    return buttonBar;
}

void MainWindow::pasteToChannel(QAction *action) {
    EventTool::setPasteChannel(action->data().toInt());
}

void MainWindow::pasteToTrack(QAction *action) {
    EventTool::setPasteTrack(action->data().toInt());
}

void MainWindow::divChanged(QAction *action) {
    mw_matrixWidget->setDiv(action->data().toInt());
}

void MainWindow::enableMagnet(bool enable) {
    EventTool::enableMagnet(enable);
}

void MainWindow::checkForUpdates(bool silent) {
    _silentUpdateCheck = silent;
    if (!_updateChecker) {
        _updateChecker = new UpdateChecker(this);
        connect(_updateChecker, &UpdateChecker::updateAvailable, this, [this](QString version, QString url){
            QMessageBox msgBox(this);
            msgBox.setWindowTitle(tr("Update Available"));
            msgBox.setText(tr("A new version %1 is available. Do you want to download it?").arg(version));
            msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
            if (msgBox.exec() == QMessageBox::Yes) {
                QDesktopServices::openUrl(QUrl(url));
            }
        });
        connect(_updateChecker, &UpdateChecker::noUpdateAvailable, this, [this](){
            if (!_silentUpdateCheck) {
                QMessageBox::information(this, tr("Update Check"), tr("You are using the latest version."));
            }
        });
        connect(_updateChecker, &UpdateChecker::errorOccurred, this, [this](QString error){
            if (!_silentUpdateCheck) {
                QMessageBox::warning(this, tr("Update Check Failed"), tr("Could not check for updates: %1").arg(error));
            } else {
                qWarning() << "Silent update check failed:" << error;
            }
        });
    }
    _updateChecker->checkForUpdates();
}

void MainWindow::openConfig() {
    SettingsDialog *d = new SettingsDialog(tr("Settings"), _settings, this);
    connect(d, SIGNAL(settingsChanged()), this, SLOT(updateAll()));
    // Note: We don't connect settingsChanged() to updateRenderingMode() here because
    // we connect directly to PerformanceSettingsWidget for immediate updates

    // Connect to PerformanceSettingsWidget for immediate updates
    // Find the PerformanceSettingsWidget in the dialog and connect to its renderingModeChanged signal
    QList<PerformanceSettingsWidget*> perfWidgets = d->findChildren<PerformanceSettingsWidget*>();
    if (!perfWidgets.isEmpty()) {
        PerformanceSettingsWidget *perfWidget = perfWidgets.first();
        connect(perfWidget, &PerformanceSettingsWidget::renderingModeChanged,
                this, &MainWindow::updateRenderingMode);
        qDebug() << "Connected PerformanceSettingsWidget for immediate rendering updates";
    }

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
            QWidget *parent = _toolbarWidget->parentWidget();
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
            QGridLayout *layout = qobject_cast<QGridLayout *>(parent->layout());
            if (layout) {
                layout->addWidget(_toolbarWidget, 0, 0);
            }
        } catch (...) {
            // If rebuild fails, create a minimal toolbar
            _toolbarWidget = nullptr; // Ensure it's not left dangling
            try {
                // Try to get the parent from the central widget
                QWidget *central = centralWidget();
                if (central) {
                    _toolbarWidget = new QWidget(central);
                    QGridLayout *layout = qobject_cast<QGridLayout *>(central->layout());
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

QAction *MainWindow::getActionById(const QString &actionId) {
    return _actionMap.value(actionId, nullptr);
}

QWidget *MainWindow::createCustomToolbar(QWidget *parent) {
    QWidget *buttonBar = new QWidget(parent);
    QGridLayout *btnLayout = new QGridLayout(buttonBar);

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
        for (const QString &actionId: actionOrder) {
            if (!actionId.startsWith("separator") && actionId != "row_separator") {
                enabledActions << actionId;
            }
        }
    }

    // Essential actions that can't be disabled (only for single row mode)
    QStringList essentialActions;
    if (!twoRowMode) {
        essentialActions = LayoutSettingsWidget::getEssentialActionIds();
    }

    // Use custom settings only if customization is enabled AND we have valid action order
    // If customization is disabled, always use defaults regardless of stored action order
    if (!customizeEnabled) {
        // Use minimal default toolbar (not comprehensive - that's for customize UI only)
        actionOrder = LayoutSettingsWidget::getDefaultToolbarOrder();

        if (twoRowMode) {
            // Add row separator for double row mode using default toolbar row distribution
            QStringList row1Actions, row2Actions;
            LayoutSettingsWidget::getDefaultToolbarRowDistribution(row1Actions, row2Actions);

            // Rebuild action order with row separator
            actionOrder.clear();
            actionOrder << row1Actions << "row_separator" << row2Actions;
        }

        // Use default toolbar enabled actions (all actions in default toolbar are enabled)
        enabledActions = LayoutSettingsWidget::getDefaultToolbarEnabledActions();
    }

    // Only prepend essential actions for single row mode
    if (!twoRowMode) {
        QStringList finalActionOrder = essentialActions;
        finalActionOrder.append(actionOrder);
        actionOrder = finalActionOrder;
    }

    // Always enable essential actions (including their separators)
    for (const QString &actionId: essentialActions) {
        if (!enabledActions.contains(actionId)) {
            enabledActions << actionId;
        }
    }

    int iconSize = Appearance::toolbarIconSize();

    // CRITICAL: Reset all grid layout constraints before setting up new layout
    // This prevents issues when switching between single/double row modes

    for (int col = 0; col < 10; col++) { // Clear up to 10 columns
        btnLayout->setColumnStretch(col, 0);
        btnLayout->setColumnMinimumWidth(col, 0);
    }
    for (int row = 0; row < 5; row++) { // Clear up to 5 rows
        btnLayout->setRowStretch(row, 0);
        btnLayout->setRowMinimumHeight(row, 0);
    }

    if (twoRowMode) {
        // Create three separate toolbars: essential (larger), top row, bottom row
        QToolBar *essentialToolBar = new QToolBar("Essential", buttonBar);
        QToolBar *topToolBar = new QToolBar("Top", buttonBar);
        QToolBar *bottomToolBar = new QToolBar("Bottom", buttonBar);

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

        QToolBar *currentToolBar = topToolBar;

        // First, add essential actions to essential toolbar
        QStringList essentialActions = LayoutSettingsWidget::getEssentialActionIds();

        for (const QString &actionId: essentialActions) {
            if (actionId.startsWith("separator")) {
                essentialToolBar->addSeparator();
                continue;
            }

            QAction *action = getActionById(actionId);
            if (action) {
                // Special handling for open action to add recent files menu
                if (actionId == "open") {
                    QAction *openWithRecentAction = new QAction(action->text(), essentialToolBar);
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
                QWidget *toolButton = essentialToolBar->widgetForAction(action);
                if (QToolButton *button = qobject_cast<QToolButton *>(toolButton)) {
                    button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
                }
            }
        }

        // Add actions to appropriate toolbar
        QStringList essentialIds = LayoutSettingsWidget::getEssentialActionIds();
        for (const QString &actionId: actionOrder) {
            // Essential actions are already handled above, skip them here
            if (essentialIds.contains(actionId)) {
                continue;
            }

            if (actionId == "row_separator") {
                currentToolBar = bottomToolBar;
                continue;
            }

            if (actionId.startsWith("separator")) {
                // Only add separators if they are enabled and there are already actions in the toolbar
                if (enabledActions.contains(actionId) && currentToolBar->actions().count() > 0) {
                    QAction *lastAction = currentToolBar->actions().last();
                    if (!lastAction->isSeparator()) {
                        currentToolBar->addSeparator();
                    }
                }
                continue;
            }

            // Skip disabled actions only if customization is enabled
            if (customizeEnabled && !enabledActions.isEmpty() && !enabledActions.contains(actionId)) {
                continue; // Skip disabled actions
            }

            // Use current toolbar for all non-essential actions

            QAction *action = getActionById(actionId);

            // Special handling for open action to add recent files menu (for non-essential open actions)
            if (actionId == "open" && action) {
                // Create a new action with the recent files menu for the toolbar
                QAction *openWithRecentAction = new QAction(action->text(), currentToolBar);
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
                        for (const ToolbarActionInfo &info: defaultActions) {
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

        // Remove any trailing separators from content toolbars only (not essential toolbar)
        removeTrailingSeparators(topToolBar);
        removeTrailingSeparators(bottomToolBar);

        // Layout: Essential toolbar on left, content toolbars stacked on right
        btnLayout->setColumnStretch(1, 1);
        btnLayout->setColumnMinimumWidth(1, 800); // Ensure content toolbars have adequate width for double row
        btnLayout->addWidget(essentialToolBar, 0, 0, 2, 1); // Spans both rows
        btnLayout->addWidget(topToolBar, 0, 1, 1, 1);
        btnLayout->addWidget(bottomToolBar, 1, 1, 1, 1);
    } else {
        // Single-row mode
        QToolBar *toolBar = new QToolBar("Main", buttonBar);
        toolBar->setFloatable(false);
        toolBar->setContentsMargins(0, 0, 0, 0);
        toolBar->layout()->setSpacing(3);
        toolBar->setIconSize(QSize(iconSize, iconSize));
        toolBar->setStyleSheet("QToolBar { border: 0px }");

        // Add actions to toolbar based on order and enabled state
        for (const QString &actionId: actionOrder) {
            if (actionId.startsWith("separator") || actionId == "row_separator") {
                if (actionId != "row_separator") {
                    // Only add separator if it's enabled and there are actions in the toolbar and the last action isn't already a separator
                    if (enabledActions.contains(actionId) && toolBar->actions().count() > 0) {
                        QAction *lastAction = toolBar->actions().last();
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

            QAction *action = getActionById(actionId);

            // Special handling for open action to add recent files menu
            if (actionId == "open" && action) {
                // Create a new action with the recent files menu for the toolbar
                QAction *openWithRecentAction = new QAction(action->text(), toolBar);
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
                        for (const ToolbarActionInfo &info: defaultActions) {
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
                    QStringList essentialIds = LayoutSettingsWidget::getEssentialActionIds();
                    if (twoRowMode && essentialIds.contains(actionId) && !actionId.startsWith("separator")) {
                        // Set the toolbar style for this specific action
                        QWidget *toolButton = toolBar->widgetForAction(action);
                        if (QToolButton *button = qobject_cast<QToolButton *>(toolButton)) {
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

        // Remove any trailing separators
        removeTrailingSeparators(toolBar);

        btnLayout->setColumnStretch(4, 1);
        btnLayout->addWidget(toolBar, 0, 0, 1, 1);
    }

    return buttonBar;
}

void MainWindow::updateToolbarContents(QWidget *toolbarWidget, QGridLayout *btnLayout) {
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
        essentialActions = LayoutSettingsWidget::getEssentialActionIds();
    }

    // Use custom settings only if customization is enabled AND we have valid action order
    // If customization is disabled, always use defaults regardless of stored action order
    if (!customizeEnabled) {
        // Use minimal default toolbar (not comprehensive - that's for customize UI only)
        actionOrder = LayoutSettingsWidget::getDefaultToolbarOrder();

        if (twoRowMode) {
            // Add row separator for double row mode using default toolbar row distribution
            QStringList row1Actions, row2Actions;
            LayoutSettingsWidget::getDefaultToolbarRowDistribution(row1Actions, row2Actions);

            // Rebuild action order with row separator
            actionOrder.clear();
            actionOrder << row1Actions << "row_separator" << row2Actions;
        }

        // Use default toolbar enabled actions (all actions in default toolbar are enabled)
        enabledActions = LayoutSettingsWidget::getDefaultToolbarEnabledActions();
    }

    // Only prepend essential actions for single row mode
    if (!twoRowMode) {
        QStringList finalActionOrder = essentialActions;
        finalActionOrder.append(actionOrder);
        actionOrder = finalActionOrder;
    }

    // Always enable essential actions (including their separators)
    for (const QString &actionId: essentialActions) {
        if (!enabledActions.contains(actionId)) {
            enabledActions << actionId;
        }
    }

    int iconSize = Appearance::toolbarIconSize();

    // CRITICAL: Reset all grid layout constraints before setting up new layout
    // This prevents issues when switching between single/double row modes

    for (int col = 0; col < 10; col++) { // Clear up to 10 columns
        btnLayout->setColumnStretch(col, 0);
        btnLayout->setColumnMinimumWidth(col, 0);
    }
    for (int row = 0; row < 5; row++) { // Clear up to 5 rows
        btnLayout->setRowStretch(row, 0);
        btnLayout->setRowMinimumHeight(row, 0);
    }

    if (twoRowMode) {
        // Create three separate toolbars: essential (larger), top row, bottom row
        QToolBar *essentialToolBar = new QToolBar("Essential", toolbarWidget);
        QToolBar *topToolBar = new QToolBar("Top", toolbarWidget);
        QToolBar *bottomToolBar = new QToolBar("Bottom", toolbarWidget);

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

        QToolBar *currentToolBar = topToolBar;

        // First, add essential actions to essential toolbar
        QStringList essentialActionsList = LayoutSettingsWidget::getEssentialActionIds();

        for (const QString &actionId: essentialActionsList) {
            if (actionId.startsWith("separator")) {
                essentialToolBar->addSeparator();
                continue;
            }

            QAction *action = getActionById(actionId);
            if (action) {
                // Special handling for open action to add recent files menu
                if (actionId == "open") {
                    QAction *openWithRecentAction = new QAction(action->text(), essentialToolBar);
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
                QWidget *toolButton = essentialToolBar->widgetForAction(action);
                if (QToolButton *button = qobject_cast<QToolButton *>(toolButton)) {
                    button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
                }
            }
        }

        // Add actions to appropriate toolbar
        QStringList essentialIds = LayoutSettingsWidget::getEssentialActionIds();
        for (const QString &actionId: actionOrder) {
            // Essential actions are already handled above, skip them here
            if (essentialIds.contains(actionId)) {
                continue;
            }

            if (actionId == "row_separator") {
                currentToolBar = bottomToolBar;
                continue;
            }

            if (actionId.startsWith("separator")) {
                if (enabledActions.contains(actionId) && !currentToolBar->actions().isEmpty()) {
                    QAction *lastAction = currentToolBar->actions().last();
                    // Avoid adding a separator if the last action was already one.
                    if (lastAction && !lastAction->isSeparator()) {
                        currentToolBar->addSeparator();
                    }
                }
                continue;
            }

            // Skip disabled actions only if customization is enabled
            if (customizeEnabled && !enabledActions.isEmpty() && !enabledActions.contains(actionId)) {
                continue; // Skip disabled actions
            }

            // Use current toolbar for all non-essential actions

            QAction *action = getActionById(actionId);

            // Special handling for open action to add recent files menu (for non-essential open actions)
            if (actionId == "open" && action) {
                // Create a new action with the recent files menu for the toolbar
                QAction *openWithRecentAction = new QAction(action->text(), currentToolBar);
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

        // Remove any trailing separators from content toolbars only (not essential toolbar)
        removeTrailingSeparators(topToolBar);
        removeTrailingSeparators(bottomToolBar);

        // Layout: Essential toolbar on left, content toolbars stacked on right
        btnLayout->setColumnStretch(1, 1);
        btnLayout->setColumnMinimumWidth(1, 800); // Ensure content toolbars have adequate width for double row
        btnLayout->addWidget(essentialToolBar, 0, 0, 2, 1); // Spans both rows
        btnLayout->addWidget(topToolBar, 0, 1, 1, 1);
        btnLayout->addWidget(bottomToolBar, 1, 1, 1, 1);
    } else {
        // Single-row mode: Create one toolbar
        QToolBar *toolBar = new QToolBar("Main", toolbarWidget);
        toolBar->setFloatable(false);
        toolBar->setContentsMargins(0, 0, 0, 0);
        toolBar->layout()->setSpacing(3);
        toolBar->setIconSize(QSize(iconSize, iconSize));
        toolBar->setStyleSheet("QToolBar { border: 0px }");

        // Add actions to toolbar based on order and enabled state
        for (const QString &actionId: actionOrder) {
            if (actionId.startsWith("separator") || actionId == "row_separator") {
                if (actionId != "row_separator") {
                    // Only add separator if it's enabled and there are actions in the toolbar and the last action isn't already a separator
                    if (enabledActions.contains(actionId) && toolBar->actions().count() > 0) {
                        QAction *lastAction = toolBar->actions().last();
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

            QAction *action = getActionById(actionId);

            // Special handling for open action to add recent files menu
            if (actionId == "open" && action) {
                // Create a new action with the recent files menu for the toolbar
                QAction *openWithRecentAction = new QAction(action->text(), toolBar);
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

        // Remove any trailing separators
        removeTrailingSeparators(toolBar);

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
    actions << ToolbarActionInfo{ "size_change", "Size Change", ":/run_environment/graphics/tool/change_size.png", nullptr, true, false, "Tools"};
    actions << ToolbarActionInfo{"back_to_begin", "Back to Begin", ":/run_environment/graphics/tool/back_to_begin.png", nullptr, true, false, "Playback"};
    actions << ToolbarActionInfo{"back_marker", "Back Marker", ":/run_environment/graphics/tool/back_marker.png", nullptr, true, false, "Playback"};
    actions << ToolbarActionInfo{"back", "Back", ":/run_environment/graphics/tool/back.png", nullptr, true, false, "Playback"};
    actions << ToolbarActionInfo{"play", "Play", ":/run_environment/graphics/tool/play.png", nullptr, true, false, "Playback"};
    actions << ToolbarActionInfo{"pause", "Pause", ":/run_environment/graphics/tool/pause.png", nullptr, true, false, "Playback"};
    actions << ToolbarActionInfo{"stop", "Stop", ":/run_environment/graphics/tool/stop.png", nullptr, true, false, "Playback"};
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

void MainWindow::applyStoredShortcuts() {
    if (!_settings) return;
    _settings->beginGroup("shortcuts");
    for (auto it = _actionMap.constBegin(); it != _actionMap.constEnd(); ++it) {
        const QString &id = it.key();
        QAction *action = it.value();
        if (!action) continue;
        QVariant v = _settings->value(id);
        if (!v.isValid()) continue;
        QStringList seqStrings = v.toStringList();
        QList<QKeySequence> seqs;
        for (const QString &s: seqStrings) {
            if (s.trimmed().isEmpty()) continue;
            seqs << QKeySequence::fromString(s.trimmed());
        }
        if (!seqs.isEmpty()) {
            if (seqs.size() == 1) action->setShortcut(seqs.first());
            else action->setShortcuts(seqs);
        }
    }
    _settings->endGroup();
}

void MainWindow::setActionShortcuts(const QString &actionId, const QList<QKeySequence> &seqs) {
    QAction *action = getActionById(actionId);
    if (!action) return;
    if (seqs.size() <= 1) {
        if (seqs.isEmpty()) action->setShortcut(QKeySequence());
        else action->setShortcut(seqs.first());
    } else {
        action->setShortcuts(seqs);
    }
}

void MainWindow::togglePianoEmulation(bool mode) {
    mw_matrixWidget->setPianoEmulation(mode);
}

void MainWindow::quantizationChanged(QAction *action) {
    _quantizationGrid = action->data().toInt();
}

void MainWindow::quantizeSelection() {
    if (!file) {
        return;
    }

    // get list with all quantization ticks
    QList<int> ticks = file->quantization(_quantizationGrid);

    file->protocol()->startNewAction(tr("Quantify events"), new QImage(":/run_environment/graphics/tool/quantize.png"));
    foreach(MidiEvent* e, Selection::instance()->selectedEvents()) {
        int onTime = e->midiTime();
        e->setMidiTime(quantize(onTime, ticks));
        OnEvent *on = dynamic_cast<OnEvent *>(e);
        if (on) {
            MidiEvent *off = on->offEvent();
            off->setMidiTime(quantize(off->midiTime(), ticks) - 1);
            if (off->midiTime() <= on->midiTime()) {
                int idx = ticks.indexOf(off->midiTime() + 1);
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

        int a = std::abs(dist);
        int b = std::abs(t - ticks.at(min));

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

    NToleQuantizationDialog *d = new NToleQuantizationDialog(this);
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
    foreach(MidiEvent* e, Selection::instance()->selectedEvents()) {
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
    foreach(MidiEvent* e, Selection::instance()->selectedEvents()) {
        int onTime = e->midiTime();
        e->setMidiTime(quantize(onTime, ntoleTicks));
        OnEvent *on = dynamic_cast<OnEvent *>(e);
        if (on) {
            MidiEvent *off = on->offEvent();
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

void MainWindow::setSpeed(QAction *action) {
    double d = action->data().toDouble();
    MidiPlayer::setSpeedScale(d);
}

void MainWindow::checkEnableActionsForSelection() {
    bool enabled = Selection::instance()->selectedEvents().size() > 0;
    foreach(QAction* action, _activateWithSelections) {
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
    _miscWidgetContainer->update();
    _matrixWidgetContainer->update();
}

void MainWindow::copiedEventsChanged() {
    // If shared clipboard is available, always enable paste
    // The paste operation itself will handle whether there's data or not
    SharedClipboard *clipboard = SharedClipboard::instance();
    bool sharedClipboardAvailable = clipboard->initialize();

    bool enable = EventTool::copiedEvents->size() > 0 || sharedClipboardAvailable;

    if (_pasteAction) {
        _pasteAction->setEnabled(enable);
    }
}

void MainWindow::updateAll() {
    // Update cached rendering settings when settings change
    // This ensures MatrixWidget uses the latest performance settings without
    // expensive QSettings I/O operations during paint events
    mw_matrixWidget->updateRenderingSettings();

    // Update all widgets
    channelWidget->update();
    _trackWidget->update();
    _miscWidgetContainer->update();
}

void MainWindow::updateRenderingMode() {
    // SIMPLIFIED: Hardware acceleration toggle - requires restart for now
    bool hardwareAccelEnabled = _settings->value("rendering/hardware_acceleration", false).toBool();

    qDebug() << "MainWindow::updateRenderingMode() called - Hardware acceleration" << (hardwareAccelEnabled ? "enabled" : "disabled");

    // For now, just log the change - the new OpenGL widgets are created at startup
    // Runtime switching can be implemented later if needed
    if (hardwareAccelEnabled) {
        qDebug() << "MainWindow: Hardware acceleration enabled - using direct OpenGL widgets";
    } else {
        qDebug() << "MainWindow: Hardware acceleration disabled - using software widgets";
    }
}

void MainWindow::rebuildToolbarFromSettings() {
    // Dedicated method for rebuilding toolbar when settings change
    // Prevent rapid successive rebuilds
    static bool isRebuilding = false;
    static QDateTime lastRebuild;
    QDateTime now = QDateTime::currentDateTime();

    // Check if we're in the middle of a style change
    static bool isStyleChanging = false;
    if (isStyleChanging) {
        return;
    }

    if (isRebuilding) {
        return;
    }

    // Debounce rapid rebuilds (prevent rebuilds within 100ms)
    if (lastRebuild.isValid() && lastRebuild.msecsTo(now) < 100) {
        return;
    }

    isRebuilding = true;
    lastRebuild = now;

    if (_toolbarWidget) {
        try {
            // Clear existing toolbar contents while keeping the widget in place
            QGridLayout *toolbarLayout = qobject_cast<QGridLayout *>(_toolbarWidget->layout());
            if (toolbarLayout) {
                bool twoRowMode = Appearance::toolbarTwoRowMode();

                // Find and immediately delete all child toolbars
                QList<QToolBar *> childToolbars = _toolbarWidget->findChildren<QToolBar *>();
                for (QToolBar *toolbar: childToolbars) {
                    toolbar->setParent(nullptr); // Remove from parent immediately
                    delete toolbar; // Delete immediately instead of deleteLater()
                }

                // Remove all layout items
                while (toolbarLayout->count() > 0) {
                    QLayoutItem *item = toolbarLayout->takeAt(0);
                    if (item) {
                        delete item;
                    }
                }

                // Reset toolbar size constraints to allow proper recalculation
                _toolbarWidget->setMinimumSize(0, 0);
                _toolbarWidget->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);

                // Now rebuild the toolbar contents directly in the existing widget
                updateToolbarContents(_toolbarWidget, toolbarLayout);

                // Update geometry without forcing window resize
                _toolbarWidget->updateGeometry();

                // Let the layout system recalculate naturally
                QTimer::singleShot(0, [this]() {
                    if (_toolbarWidget && _toolbarWidget->layout()) {
                        _toolbarWidget->layout()->invalidate();
                        _toolbarWidget->layout()->activate();
                    }
                });

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

    // Reset the rebuilding guard
    isRebuilding = false;
}

void MainWindow::refreshToolbarIcons() {
    // The individual actions will be refreshed by Appearance::refreshAllIcons()
    // We just need to make sure our toolbar is up to date
    if (_toolbarWidget) {
        try {
            _toolbarWidget->update();

            // Also refresh any child toolbars
            QList<QToolBar *> toolbars = _toolbarWidget->findChildren<QToolBar *>();
            for (QToolBar *toolbar: toolbars) {
                if (toolbar) {
                    toolbar->update();
                }
            }
        } catch (...) {
            // Error refreshing toolbar icons
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

    QList<MidiEvent *> selectedEvents = Selection::instance()->selectedEvents();
    if (selectedEvents.isEmpty()) {
        return;
    }

    file->protocol()->startNewAction("Transpose octave up");

    foreach(MidiEvent* event, selectedEvents) {
        NoteOnEvent *noteOnEvent = dynamic_cast<NoteOnEvent *>(event);
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

    QList<MidiEvent *> selectedEvents = Selection::instance()->selectedEvents();
    if (selectedEvents.isEmpty()) {
        return;
    }

    file->protocol()->startNewAction("Transpose octave down");

    foreach(MidiEvent* event, selectedEvents) {
        NoteOnEvent *noteOnEvent = dynamic_cast<NoteOnEvent *>(event);
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

void MainWindow::removeTrailingSeparators(QToolBar *toolbar) {
    if (!toolbar) return;

    QList<QAction *> actions = toolbar->actions();

    // Remove trailing separators from the end
    while (!actions.isEmpty() && actions.last()->isSeparator()) {
        QAction *lastAction = actions.takeLast();
        toolbar->removeAction(lastAction);
    }
}

void MainWindow::applyWidgetSizeConstraints() {
    // Check if widget size unlocking is enabled (requires restart)
    bool unlockSizes = _settings->value("unlock_widget_sizes", false).toBool();

    if (unlockSizes) {
        // Remove minimum size constraints to allow full resizing
        if (upperTabWidget) {
            upperTabWidget->setMinimumSize(0, 0);
            upperTabWidget->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
        }
        if (lowerTabWidget) {
            lowerTabWidget->setMinimumSize(0, 0);
            lowerTabWidget->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
        }
        if (chooserWidget) {
            chooserWidget->setMinimumWidth(0); // Allow width to resize/clip
            // Fix the height to prevent stretching when there are gaps
            chooserWidget->setMaximumHeight(chooserWidget->sizeHint().height());
            chooserWidget->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        }
        if (tracksWidget) {
            tracksWidget->setMinimumSize(0, 0);
            tracksWidget->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
        }
        if (channelsWidget) {
            channelsWidget->setMinimumSize(0, 0);
            channelsWidget->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
        }
        if (rightSplitter) {
            rightSplitter->setChildrenCollapsible(true);
            // Allow all widgets in the splitter to collapse to very small sizes
            for (int i = 0; i < rightSplitter->count(); ++i) {
                rightSplitter->setCollapsible(i, true);
            }
            // Allow the rightSplitter itself to resize smaller and clip content
            rightSplitter->setMinimumSize(0, 0);
            rightSplitter->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
        }
        if (mainSplitter) {
            mainSplitter->setChildrenCollapsible(true);
            // Allow the right side (index 1) to be collapsible but don't change stretch factors
            mainSplitter->setCollapsible(1, true);
        }
    }
    else
    {
        if (chooserWidget)
        {
            chooserWidget->setMaximumHeight(chooserWidget->sizeHint().height());
            chooserWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        }

        // Ensure the splitter can still collapse chooserWidget
        if (rightSplitter)
        {
            rightSplitter->setChildrenCollapsible(true);

            // Find chooserWidget's index in the splitter
            int chooserIndex = rightSplitter->indexOf(chooserWidget);
            if (chooserIndex != -1)
            {
                rightSplitter->setCollapsible(chooserIndex, true);
            }
        }
    }
}
