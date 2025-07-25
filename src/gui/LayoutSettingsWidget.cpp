#include "LayoutSettingsWidget.h"
#include "Appearance.h"
#include <QMainWindow>
#include <QIcon>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QDrag>
#include <QApplication>

// DraggableListWidget implementation
DraggableListWidget::DraggableListWidget(QWidget* parent) : QListWidget(parent) {
    setDragDropMode(QAbstractItemView::InternalMove);
    setDefaultDropAction(Qt::MoveAction);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setDropIndicatorShown(true); // Show drop indicator for better UX
}

void DraggableListWidget::dragEnterEvent(QDragEnterEvent* event) {
    // Accept drops from this list or other DraggableListWidget instances
    if (event->source() == this || qobject_cast<DraggableListWidget*>(event->source())) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void DraggableListWidget::dragMoveEvent(QDragMoveEvent* event) {
    // Accept drops from this list or other DraggableListWidget instances
    if (event->source() == this || qobject_cast<DraggableListWidget*>(event->source())) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void DraggableListWidget::dropEvent(QDropEvent* event) {
    if (event->source() == this || qobject_cast<DraggableListWidget*>(event->source())) {
        // Let Qt handle the drop, then emit our signal
        QListWidget::dropEvent(event);
        emit itemsReordered();
        event->accept();
    } else {
        event->ignore();
    }
}

void DraggableListWidget::startDrag(Qt::DropActions supportedActions) {
    QListWidget::startDrag(supportedActions);
}

// ToolbarActionItem implementation
ToolbarActionItem::ToolbarActionItem(const ToolbarActionInfo& info, QListWidget* parent)
    : QListWidgetItem(parent), actionInfo(info) {

    updateDisplay();
}

void ToolbarActionItem::updateDisplay() {
    QString displayText = actionInfo.name;
    if (actionInfo.essential) {
        displayText += " (Essential)";
    }
    setText(displayText);
    
    // Set icon if available
    if (!actionInfo.iconPath.isEmpty()) {
        setIcon(Appearance::adjustIconForDarkMode(actionInfo.iconPath));
    }
}

// LayoutSettingsWidget implementation
LayoutSettingsWidget::LayoutSettingsWidget(QWidget* parent)
    : SettingsWidget("Layout", parent), _twoRowMode(false) {
    setupUI();
    loadSettings();
    populateActionsList();

    // Connect item change signals for both lists
    connect(_actionsList, SIGNAL(itemChanged(QListWidgetItem*)), this, SLOT(itemCheckStateChanged(QListWidgetItem*)));
    connect(_secondRowList, SIGNAL(itemChanged(QListWidgetItem*)), this, SLOT(itemCheckStateChanged(QListWidgetItem*)));

    // Set object name so the theme refresh system can find us
    setObjectName("LayoutSettingsWidget");
}

void LayoutSettingsWidget::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 5, 10, 10);
    mainLayout->setSpacing(10);

    // Row mode selection
    QGroupBox* rowModeGroup = new QGroupBox("Toolbar Layout", this);
    QVBoxLayout* rowModeLayout = new QVBoxLayout(rowModeGroup);
    
    _singleRowRadio = new QRadioButton("Single row (compact)", rowModeGroup);
    _doubleRowRadio = new QRadioButton("Double row (larger icons with text)", rowModeGroup);
    
    rowModeLayout->addWidget(_singleRowRadio);
    rowModeLayout->addWidget(_doubleRowRadio);
    
    connect(_singleRowRadio, SIGNAL(toggled(bool)), this, SLOT(rowModeChanged()));
    connect(_doubleRowRadio, SIGNAL(toggled(bool)), this, SLOT(rowModeChanged()));
    
    mainLayout->addWidget(rowModeGroup);

    // Actions list - split for two-row mode
    QLabel* actionsLabel = new QLabel("Toolbar Actions (drag to reorder):", this);
    mainLayout->addWidget(actionsLabel);

    // Create horizontal layout for split view
    _actionsLayout = new QHBoxLayout();

    // Left side - main actions list
    QVBoxLayout* leftLayout = new QVBoxLayout();
    QLabel* firstRowLabel = new QLabel("Row 1:", this);
    firstRowLabel->setStyleSheet("font-weight: bold;");
    leftLayout->addWidget(firstRowLabel);

    _actionsList = new DraggableListWidget(this);
    _actionsList->setMinimumHeight(300);
    connect(_actionsList, SIGNAL(itemsReordered()), this, SLOT(itemsReordered()));
    leftLayout->addWidget(_actionsList);

    // Right side - second row (initially hidden)
    QVBoxLayout* rightLayout = new QVBoxLayout();
    _secondRowLabel = new QLabel("Row 2:", this);
    _secondRowLabel->setStyleSheet("font-weight: bold;");
    rightLayout->addWidget(_secondRowLabel);

    _secondRowList = new DraggableListWidget(this);
    _secondRowList->setMinimumHeight(300);
    connect(_secondRowList, SIGNAL(itemsReordered()), this, SLOT(itemsReordered()));
    rightLayout->addWidget(_secondRowList);

    _actionsLayout->addLayout(leftLayout);
    _actionsLayout->addLayout(rightLayout);

    // Initially hide second row
    _secondRowLabel->setVisible(false);
    _secondRowList->setVisible(false);

    mainLayout->addLayout(_actionsLayout);
    
    // Info label
    QLabel* infoLabel = new QLabel("• Check/uncheck to enable/disable actions\n"
                                   "• Essential actions (New, Open, Save, Undo, Redo) cannot be disabled\n"
                                   "• Drag items to reorder them in the toolbar\n"
                                   "• In two-row mode, drag items between Row 1 and Row 2\n"
                                   "• Changes are applied immediately to the toolbar", this);
    infoLabel->setStyleSheet("color: gray; font-size: 10px;");
    infoLabel->setWordWrap(true);
    mainLayout->addWidget(infoLabel);

    // Reset button
    _resetButton = new QPushButton("Reset to Default", this);
    connect(_resetButton, SIGNAL(clicked()), this, SLOT(resetToDefault()));
    mainLayout->addWidget(_resetButton);
    
    setLayout(mainLayout);
}

void LayoutSettingsWidget::loadSettings() {
    try {
        _twoRowMode = Appearance::toolbarTwoRowMode();

        if (_twoRowMode) {
            _doubleRowRadio->setChecked(true);
        } else {
            _singleRowRadio->setChecked(true);
        }
    } catch (...) {
        // If loading fails, use safe defaults
        _twoRowMode = false;
        _singleRowRadio->setChecked(true);
    }
}

void LayoutSettingsWidget::saveSettings() {
    try {
        Appearance::setToolbarTwoRowMode(_twoRowMode);

        // Save action order and enabled states
        QStringList actionOrder;
        QStringList enabledActions;

        // Add Row 1 actions
        for (int i = 0; i < _actionsList->count(); ++i) {
            ToolbarActionItem* item = static_cast<ToolbarActionItem*>(_actionsList->item(i));
            actionOrder << item->actionInfo.id;
            if (item->checkState() == Qt::Checked || item->actionInfo.essential) {
                enabledActions << item->actionInfo.id;
            }
        }

        // Add row separator if in two-row mode and there are Row 2 actions
        if (_twoRowMode && _secondRowList->count() > 0) {
            actionOrder << "row_separator";

            // Add Row 2 actions
            for (int i = 0; i < _secondRowList->count(); ++i) {
                ToolbarActionItem* item = static_cast<ToolbarActionItem*>(_secondRowList->item(i));
                actionOrder << item->actionInfo.id;
                if (item->checkState() == Qt::Checked || item->actionInfo.essential) {
                    enabledActions << item->actionInfo.id;
                }
            }
        }

        Appearance::setToolbarActionOrder(actionOrder);
        Appearance::setToolbarEnabledActions(enabledActions);

        // Phase 2: Trigger toolbar rebuild when settings change
        // Find the MainWindow and trigger toolbar rebuild
        QWidget* widget = this;
        while (widget && !qobject_cast<QMainWindow*>(widget)) {
            widget = widget->parentWidget();
        }
        if (widget) {
            QMetaObject::invokeMethod(widget, "updateAll", Qt::QueuedConnection);
        }

    } catch (...) {
        // If saving fails, just continue - don't crash the settings dialog
    }
}

void LayoutSettingsWidget::populateActionsList() {
    try {
        _actionsList->clear();
        _availableActions = getDefaultActions();

        // Load custom order if available
        QStringList customOrder;
        QStringList enabledActions;

        try {
            customOrder = Appearance::toolbarActionOrder();
            enabledActions = Appearance::toolbarEnabledActions();
        } catch (...) {
            // If settings loading fails, use empty lists (will trigger defaults)
            customOrder.clear();
            enabledActions.clear();
        }
    
    // Get the default action order based on current mode
    QStringList defaultOrder;
    if (_twoRowMode) {
        // Two-row default layout
        defaultOrder << "new" << "open" << "save" << "separator1"
                    << "undo" << "redo" << "separator2"
                    << "standard_tool" << "select_left" << "select_right" << "separator3"
                    << "new_note" << "remove_notes" << "copy" << "paste" << "separator4"
                    << "glue" << "glue_all_channels" << "scissors" << "delete_overlaps" << "size_change" << "separator5"
                    << "align_left" << "equalize" << "align_right" << "separator6"
                    << "quantize" << "magnet" << "separator7"
                    << "measure" << "time_signature" << "tempo"
                    << "row_separator" // Split point for second row
                    << "back_to_begin" << "back_marker" << "back" << "play" << "pause"
                    << "stop" << "record" << "forward" << "forward_marker" << "separator8"
                    << "metronome" << "separator9"
                    << "zoom_hor_in" << "zoom_hor_out" << "zoom_ver_in" << "zoom_ver_out"
                    << "lock" << "separator10" << "thru";
    } else {
        // Single-row default (current Step 1 toolbar)
        defaultOrder << "new" << "open" << "save" << "undo" << "redo"
                    << "standard_tool" << "new_note" << "copy" << "paste"
                    << "play" << "pause" << "stop";
    }

    QStringList orderToUse = customOrder.isEmpty() ? defaultOrder : customOrder;

    // Split actions between Row 1 and Row 2 based on row_separator
    QStringList row1Actions;
    QStringList row2Actions;
    bool inRow2 = false;

    for (const QString& actionId : orderToUse) {
        if (actionId == "row_separator") {
            inRow2 = true;
            continue;
        }

        if (inRow2) {
            row2Actions << actionId;
        } else {
            row1Actions << actionId;
        }
    }

    // Populate Row 1
    for (const QString& actionId : row1Actions) {
        for (ToolbarActionInfo& info : _availableActions) {
            if (info.id == actionId) {
                info.enabled = enabledActions.isEmpty() ?
                    (defaultOrder.contains(actionId) && !actionId.startsWith("separator")) || info.essential :
                    (enabledActions.contains(actionId) || info.essential);
                ToolbarActionItem* item = new ToolbarActionItem(info, _actionsList);
                break;
            }
        }
    }

    // Populate Row 2 (if in two-row mode)
    if (_twoRowMode && !row2Actions.isEmpty()) {
        for (const QString& actionId : row2Actions) {
            for (ToolbarActionInfo& info : _availableActions) {
                if (info.id == actionId) {
                    info.enabled = enabledActions.isEmpty() ?
                        (defaultOrder.contains(actionId) && !actionId.startsWith("separator")) || info.essential :
                        (enabledActions.contains(actionId) || info.essential);
                    ToolbarActionItem* item = new ToolbarActionItem(info, _secondRowList);
                    break;
                }
            }
        }
    }
    
    // Use a simpler approach with checkable items
    for (int i = 0; i < _actionsList->count(); ++i) {
        ToolbarActionItem* item = static_cast<ToolbarActionItem*>(_actionsList->item(i));
        item->setCheckState(item->actionInfo.enabled ? Qt::Checked : Qt::Unchecked);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);

        // Essential actions cannot be unchecked
        if (item->actionInfo.essential) {
            item->setFlags(item->flags() & ~Qt::ItemIsUserCheckable);
            item->setCheckState(Qt::Checked);
        }

    }

        // Connect item change signal once for the entire list
        connect(_actionsList, SIGNAL(itemChanged(QListWidgetItem*)), this, SLOT(itemCheckStateChanged(QListWidgetItem*)));

    } catch (...) {
        // If anything fails, create a minimal list with just essential actions
        _actionsList->clear();
        QList<ToolbarActionInfo> essentialActions;
        essentialActions << ToolbarActionInfo{"new", "New", ":/run_environment/graphics/tool/new.png", nullptr, true, true, "File"};
        essentialActions << ToolbarActionInfo{"open", "Open", ":/run_environment/graphics/tool/load.png", nullptr, true, true, "File"};
        essentialActions << ToolbarActionInfo{"save", "Save", ":/run_environment/graphics/tool/save.png", nullptr, true, true, "File"};
        essentialActions << ToolbarActionInfo{"undo", "Undo", ":/run_environment/graphics/tool/undo.png", nullptr, true, true, "Edit"};
        essentialActions << ToolbarActionInfo{"redo", "Redo", ":/run_environment/graphics/tool/redo.png", nullptr, true, true, "Edit"};

        for (const ToolbarActionInfo& info : essentialActions) {
            ToolbarActionItem* item = new ToolbarActionItem(info, _actionsList);
            item->setCheckState(Qt::Checked);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        }
    }
}

void LayoutSettingsWidget::rowModeChanged() {
    _twoRowMode = _doubleRowRadio->isChecked();

    // Show/hide second row based on mode
    _secondRowLabel->setVisible(_twoRowMode);
    _secondRowList->setVisible(_twoRowMode);

    if (_twoRowMode) {
        // Split actions between two rows when switching to two-row mode
        populateActionsList();
    }

    saveSettings();
}

void LayoutSettingsWidget::actionEnabledChanged() {
    saveSettings();
}

void LayoutSettingsWidget::itemCheckStateChanged(QListWidgetItem* item) {
    ToolbarActionItem* actionItem = static_cast<ToolbarActionItem*>(item);
    if (actionItem) {
        actionItem->actionInfo.enabled = (item->checkState() == Qt::Checked);
        saveSettings();
    }
}

void LayoutSettingsWidget::itemsReordered() {
    saveSettings();
}

void LayoutSettingsWidget::resetToDefault() {
    try {
        // Reset to default settings
        _singleRowRadio->setChecked(true);
        _twoRowMode = false;

        // Clear saved settings to force defaults
        Appearance::setToolbarActionOrder(QStringList());
        Appearance::setToolbarEnabledActions(QStringList());
        Appearance::setToolbarTwoRowMode(false);

        // Repopulate with defaults
        populateActionsList();
        saveSettings();
    } catch (...) {
        // If reset fails, just continue
    }
}

QList<ToolbarActionInfo> LayoutSettingsWidget::getDefaultActions() {
    QList<ToolbarActionInfo> actions;

    // Essential actions (cannot be disabled) - these should be enabled by default
    actions << ToolbarActionInfo{"new", "New", ":/run_environment/graphics/tool/new.png", nullptr, true, true, "File"};
    actions << ToolbarActionInfo{"open", "Open", ":/run_environment/graphics/tool/load.png", nullptr, true, true, "File"};
    actions << ToolbarActionInfo{"save", "Save", ":/run_environment/graphics/tool/save.png", nullptr, true, true, "File"};
    actions << ToolbarActionInfo{"separator1", "--- Separator ---", "", nullptr, true, false, "Separator"};
    actions << ToolbarActionInfo{"undo", "Undo", ":/run_environment/graphics/tool/undo.png", nullptr, true, true, "Edit"};
    actions << ToolbarActionInfo{"redo", "Redo", ":/run_environment/graphics/tool/redo.png", nullptr, true, true, "Edit"};

    // Tool actions - these were in the original toolbar, so enable by default
    actions << ToolbarActionInfo{"separator2", "--- Separator ---", "", nullptr, true, false, "Separator"};
    actions << ToolbarActionInfo{"standard_tool", "Standard Tool", ":/run_environment/graphics/tool/select.png", nullptr, true, false, "Tools"};
    actions << ToolbarActionInfo{"select_left", "Select Left", ":/run_environment/graphics/tool/select_left.png", nullptr, true, false, "Tools"};
    actions << ToolbarActionInfo{"select_right", "Select Right", ":/run_environment/graphics/tool/select_right.png", nullptr, true, false, "Tools"};

    // Edit actions
    actions << ToolbarActionInfo{"separator3", "--- Separator ---", "", nullptr, true, false, "Separator"};
    actions << ToolbarActionInfo{"new_note", "New Note", ":/run_environment/graphics/tool/newnote.png", nullptr, true, false, "Edit"};
    actions << ToolbarActionInfo{"remove_notes", "Remove Notes", ":/run_environment/graphics/tool/eraser.png", nullptr, true, false, "Edit"};
    actions << ToolbarActionInfo{"copy", "Copy", ":/run_environment/graphics/tool/copy.png", nullptr, true, false, "Edit"};
    actions << ToolbarActionInfo{"paste", "Paste", ":/run_environment/graphics/tool/paste.png", nullptr, true, false, "Edit"};

    // Tool actions
    actions << ToolbarActionInfo{"separator4", "--- Separator ---", "", nullptr, true, false, "Separator"};
    actions << ToolbarActionInfo{"glue", "Glue Notes (Same Channel)", ":/run_environment/graphics/tool/glue.png", nullptr, true, false, "Tools"};
    actions << ToolbarActionInfo{"glue_all_channels", "Glue Notes (All Channels)", ":/run_environment/graphics/tool/glue.png", nullptr, false, false, "Tools"}; // New feature, disabled by default
    actions << ToolbarActionInfo{"scissors", "Scissors", ":/run_environment/graphics/tool/scissors.png", nullptr, true, false, "Tools"};
    actions << ToolbarActionInfo{"delete_overlaps", "Delete Overlaps", ":/run_environment/graphics/tool/deleteoverlap.png", nullptr, true, false, "Tools"};
    actions << ToolbarActionInfo{"size_change", "Size Change", ":/run_environment/graphics/tool/change_size.png", nullptr, false, false, "Tools"}; // New feature, disabled by default

    // Playback actions
    actions << ToolbarActionInfo{"separator5", "--- Separator ---", "", nullptr, true, false, "Separator"};
    actions << ToolbarActionInfo{"back_to_begin", "Back to Begin", ":/run_environment/graphics/tool/back_to_begin.png", nullptr, true, false, "Playback"};
    actions << ToolbarActionInfo{"back_marker", "Back Marker", ":/run_environment/graphics/tool/back_marker.png", nullptr, true, false, "Playback"};
    actions << ToolbarActionInfo{"back", "Back", ":/run_environment/graphics/tool/back.png", nullptr, true, false, "Playback"};
    actions << ToolbarActionInfo{"play", "Play", ":/run_environment/graphics/tool/play.png", nullptr, true, false, "Playback"};
    actions << ToolbarActionInfo{"pause", "Pause", ":/run_environment/graphics/tool/pause.png", nullptr, true, false, "Playback"};
    actions << ToolbarActionInfo{"stop", "Stop", ":/run_environment/graphics/tool/stop_record.png", nullptr, true, false, "Playback"};
    actions << ToolbarActionInfo{"record", "Record", ":/run_environment/graphics/tool/record.png", nullptr, true, false, "Playback"};
    actions << ToolbarActionInfo{"forward", "Forward", ":/run_environment/graphics/tool/forward.png", nullptr, true, false, "Playback"};
    actions << ToolbarActionInfo{"forward_marker", "Forward Marker", ":/run_environment/graphics/tool/forward_marker.png", nullptr, true, false, "Playback"};

    // Additional tools - these were in the original toolbar, so enable by default
    actions << ToolbarActionInfo{"separator6", "--- Separator ---", "", nullptr, true, false, "Separator"};
    actions << ToolbarActionInfo{"metronome", "Metronome", ":/run_environment/graphics/tool/metronome.png", nullptr, true, false, "Playback"};
    actions << ToolbarActionInfo{"align_left", "Align Left", ":/run_environment/graphics/tool/align_left.png", nullptr, true, false, "Tools"};
    actions << ToolbarActionInfo{"equalize", "Equalize", ":/run_environment/graphics/tool/equalize.png", nullptr, true, false, "Tools"};
    actions << ToolbarActionInfo{"align_right", "Align Right", ":/run_environment/graphics/tool/align_right.png", nullptr, true, false, "Tools"};

    // Zoom actions
    actions << ToolbarActionInfo{"separator7", "--- Separator ---", "", nullptr, true, false, "Separator"};
    actions << ToolbarActionInfo{"zoom_hor_in", "Zoom Horizontal In", ":/run_environment/graphics/tool/zoom_hor_in.png", nullptr, true, false, "View"};
    actions << ToolbarActionInfo{"zoom_hor_out", "Zoom Horizontal Out", ":/run_environment/graphics/tool/zoom_hor_out.png", nullptr, true, false, "View"};
    actions << ToolbarActionInfo{"zoom_ver_in", "Zoom Vertical In", ":/run_environment/graphics/tool/zoom_ver_in.png", nullptr, true, false, "View"};
    actions << ToolbarActionInfo{"zoom_ver_out", "Zoom Vertical Out", ":/run_environment/graphics/tool/zoom_ver_out.png", nullptr, true, false, "View"};
    actions << ToolbarActionInfo{"lock", "Lock Screen", ":/run_environment/graphics/tool/screen_unlocked.png", nullptr, true, false, "View"};

    // Additional tools
    actions << ToolbarActionInfo{"separator8", "--- Separator ---", "", nullptr, true, false, "Separator"};
    actions << ToolbarActionInfo{"quantize", "Quantize", ":/run_environment/graphics/tool/quantize.png", nullptr, true, false, "Tools"};
    actions << ToolbarActionInfo{"magnet", "Magnet", ":/run_environment/graphics/tool/magnet.png", nullptr, true, false, "Tools"};

    // MIDI actions
    actions << ToolbarActionInfo{"separator9", "--- Separator ---", "", nullptr, true, false, "Separator"};
    actions << ToolbarActionInfo{"thru", "MIDI Thru", ":/run_environment/graphics/tool/connection.png", nullptr, true, false, "MIDI"};
    actions << ToolbarActionInfo{"separator10", "--- Separator ---", "", nullptr, true, false, "Separator"};
    actions << ToolbarActionInfo{"measure", "Measure", ":/run_environment/graphics/tool/measure.png", nullptr, true, false, "View"};
    actions << ToolbarActionInfo{"time_signature", "Time Signature", ":/run_environment/graphics/tool/meter.png", nullptr, true, false, "View"};
    actions << ToolbarActionInfo{"tempo", "Tempo", ":/run_environment/graphics/tool/tempo.png", nullptr, true, false, "View"};

    // Special separator for two-row mode
    actions << ToolbarActionInfo{"row_separator", "=== Second Row ===", "", nullptr, true, false, "Layout"};

    return actions;
}

QIcon LayoutSettingsWidget::icon() {
    return QIcon(); // No icon for Layout tab
}

void LayoutSettingsWidget::refreshIcons() {
    // Refresh all icons in the actions list when theme changes
    for (int i = 0; i < _actionsList->count(); ++i) {
        ToolbarActionItem* item = static_cast<ToolbarActionItem*>(_actionsList->item(i));
        if (item) {
            item->updateDisplay(); // This will call adjustIconForDarkMode
        }
    }
}
