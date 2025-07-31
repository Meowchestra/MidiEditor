#include "LayoutSettingsWidget.h"
#include "Appearance.h"
#include <QMainWindow>
#include <QCheckBox>
#include <QIcon>
#include <QDropEvent>
#include <QMimeData>
#include <QApplication>
#include <QTimer>
#include <QFile>
#include <QGroupBox>

// DraggableListWidget implementation
DraggableListWidget::DraggableListWidget(QWidget *parent) : QListWidget(parent) {
    setDragDropMode(QAbstractItemView::InternalMove);
    setDefaultDropAction(Qt::MoveAction);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setDropIndicatorShown(true); // Show drop indicator for better UX
}

void DraggableListWidget::dragEnterEvent(QDragEnterEvent *event) {
    // Accept drops from this list or other DraggableListWidget instances
    if (event->source() == this || qobject_cast<DraggableListWidget *>(event->source())) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void DraggableListWidget::dragMoveEvent(QDragMoveEvent *event) {
    // Accept drops from this list or other DraggableListWidget instances
    if (event->source() == this || qobject_cast<DraggableListWidget *>(event->source())) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void DraggableListWidget::dropEvent(QDropEvent *event) {
    DraggableListWidget *sourceList = qobject_cast<DraggableListWidget *>(event->source());
    if (event->source() == this || sourceList) {
        if (sourceList && sourceList != this) {
            // Cross-list drop: manually handle the move
            QListWidgetItem *draggedItem = sourceList->currentItem();
            if (draggedItem) {
                // Clone the item
                ToolbarActionItem *originalItem = static_cast<ToolbarActionItem *>(draggedItem);
                ToolbarActionItem *newItem = new ToolbarActionItem(originalItem->actionInfo, this);
                newItem->setFlags(originalItem->flags());
                newItem->setCheckState(originalItem->checkState());

                // Find drop position based on cursor position
                QListWidgetItem *targetItem = itemAt(event->position().toPoint());
                int dropIndex;

                // Simple approach: always drop at the end of the list
                dropIndex = count();

                // Insert at the drop position
                insertItem(dropIndex, newItem);

                // Remove from source list
                int sourceRow = sourceList->row(draggedItem);
                delete sourceList->takeItem(sourceRow);

                emit itemsReordered();
                event->accept();
            }
        } else {
            // Same-list drop: let Qt handle it
            QListWidget::dropEvent(event);
            emit itemsReordered();
            event->accept();
        }
    } else {
        event->ignore();
    }
}

void DraggableListWidget::startDrag(Qt::DropActions supportedActions) {
    QListWidget::startDrag(supportedActions);
}

// ToolbarActionItem implementation
ToolbarActionItem::ToolbarActionItem(const ToolbarActionInfo &info, QListWidget *parent)
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
LayoutSettingsWidget::LayoutSettingsWidget(QWidget *parent)
    : SettingsWidget("Customize Toolbar", parent), _twoRowMode(false) {
    // Initialize update timer for debouncing
    _updateTimer = new QTimer(this);
    _updateTimer->setSingleShot(true);
    _updateTimer->setInterval(100); // 100ms delay
    connect(_updateTimer, SIGNAL(timeout()), this, SLOT(triggerToolbarUpdate()));
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
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 5, 10, 10);
    mainLayout->setSpacing(10);

    // Enable customize toolbar checkbox
    _enableCustomizeCheckbox = new QCheckBox("Enable Customize Toolbar", this);
    _enableCustomizeCheckbox->setToolTip("Enable this to customize individual actions and their order. When disabled, uses ideal default layouts.");
    connect(_enableCustomizeCheckbox, SIGNAL(toggled(bool)), this, SLOT(customizeToolbarToggled(bool)));
    mainLayout->addWidget(_enableCustomizeCheckbox);

    // Row mode selection
    QGroupBox *rowModeGroup = new QGroupBox("Toolbar Layout", this);
    rowModeGroup->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    QVBoxLayout *rowModeLayout = new QVBoxLayout(rowModeGroup);

    _singleRowRadio = new QRadioButton("Single row (compact)", rowModeGroup);
    _doubleRowRadio = new QRadioButton("Double row (larger icons with text)", rowModeGroup);

    rowModeLayout->addWidget(_singleRowRadio);
    rowModeLayout->addWidget(_doubleRowRadio);

    connect(_singleRowRadio, SIGNAL(toggled(bool)), this, SLOT(rowModeChanged()));
    connect(_doubleRowRadio, SIGNAL(toggled(bool)), this, SLOT(rowModeChanged()));

    mainLayout->addWidget(rowModeGroup);

    // Toolbar Icon Size
    QHBoxLayout *iconSizeLayout = new QHBoxLayout();
    QLabel *iconSizeLabel = new QLabel("Toolbar Icon Size:", this);
    iconSizeLayout->addWidget(iconSizeLabel);

    _iconSizeSpinBox = new QSpinBox(this);
    _iconSizeSpinBox->setMinimum(16);
    _iconSizeSpinBox->setMaximum(32);
    _iconSizeSpinBox->setValue(Appearance::toolbarIconSize());
    _iconSizeSpinBox->setMinimumWidth(80); // Make it wider for better padding
    connect(_iconSizeSpinBox, SIGNAL(valueChanged(int)), this, SLOT(iconSizeChanged(int)));
    iconSizeLayout->addWidget(_iconSizeSpinBox);
    iconSizeLayout->addStretch();

    mainLayout->addLayout(iconSizeLayout);

    // Create container for customization options (initially hidden)
    _customizationWidget = new QWidget(this);
    _customizationWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    QVBoxLayout *customizationLayout = new QVBoxLayout(_customizationWidget);
    customizationLayout->setContentsMargins(0, 0, 0, 0);

    // Actions list - split for two-row mode
    QLabel *actionsLabel = new QLabel("Toolbar Actions (drag to reorder):", _customizationWidget);
    customizationLayout->addWidget(actionsLabel);

    // Create horizontal layout for split view
    _actionsLayout = new QHBoxLayout();

    // Left side - main actions list
    QVBoxLayout *leftLayout = new QVBoxLayout();
    QLabel *firstRowLabel = new QLabel("Row 1:", _customizationWidget);
    firstRowLabel->setStyleSheet("font-weight: bold;");
    leftLayout->addWidget(firstRowLabel);

    _actionsList = new DraggableListWidget(_customizationWidget);
    _actionsList->setObjectName("Row1List"); // Set object name for debug identification
    _actionsList->setMinimumHeight(300);
    connect(_actionsList, SIGNAL(itemsReordered()), this, SLOT(itemsReordered()));
    leftLayout->addWidget(_actionsList);

    // Right side - second row (initially hidden)
    QVBoxLayout *rightLayout = new QVBoxLayout();
    _secondRowLabel = new QLabel("Row 2:", _customizationWidget);
    _secondRowLabel->setStyleSheet("font-weight: bold;");
    rightLayout->addWidget(_secondRowLabel);

    _secondRowList = new DraggableListWidget(_customizationWidget);
    _secondRowList->setObjectName("Row2List"); // Set object name for debug identification
    _secondRowList->setMinimumHeight(300);
    connect(_secondRowList, SIGNAL(itemsReordered()), this, SLOT(itemsReordered()));
    rightLayout->addWidget(_secondRowList);

    _actionsLayout->addLayout(leftLayout);
    _actionsLayout->addLayout(rightLayout);

    // Initially hide second row
    _secondRowLabel->setVisible(false);
    _secondRowList->setVisible(false);

    customizationLayout->addLayout(_actionsLayout);

    // Reset button
    _resetButton = new QPushButton("Reset to Default", _customizationWidget);
    connect(_resetButton, SIGNAL(clicked()), this, SLOT(resetToDefault()));
    customizationLayout->addWidget(_resetButton);

    // Add the customization container to main layout (initially hidden)
    _customizationWidget->setVisible(false);
    mainLayout->addWidget(_customizationWidget);

    // Add a spacer at the end to prevent expansion when customization is hidden
    mainLayout->addStretch();

    setLayout(mainLayout);
}

void LayoutSettingsWidget::loadSettings() {
    try {
        // Temporarily disconnect signals to prevent triggering updates during loading
        disconnect(_singleRowRadio, SIGNAL(toggled(bool)), this, SLOT(rowModeChanged()));
        disconnect(_doubleRowRadio, SIGNAL(toggled(bool)), this, SLOT(rowModeChanged()));
        disconnect(_enableCustomizeCheckbox, SIGNAL(toggled(bool)), this, SLOT(customizeToolbarToggled(bool)));

        _twoRowMode = Appearance::toolbarTwoRowMode();

        if (_twoRowMode) {
            _doubleRowRadio->setChecked(true);
        } else {
            _singleRowRadio->setChecked(true);
        }

        // Load the customize toolbar setting
        bool customizeEnabled = Appearance::toolbarCustomizeEnabled();

        _enableCustomizeCheckbox->setChecked(customizeEnabled);
        _customizationWidget->setVisible(customizeEnabled);

        // Reconnect signals after loading
        connect(_singleRowRadio, SIGNAL(toggled(bool)), this, SLOT(rowModeChanged()));
        connect(_doubleRowRadio, SIGNAL(toggled(bool)), this, SLOT(rowModeChanged()));
        connect(_enableCustomizeCheckbox, SIGNAL(toggled(bool)), this, SLOT(customizeToolbarToggled(bool)));

        // Show/hide second row based on mode (only if customization is enabled)
        if (customizeEnabled) {
            _secondRowLabel->setVisible(_twoRowMode);
            _secondRowList->setVisible(_twoRowMode);
        }
    } catch (...) {
        // If loading fails, use safe defaults
        _twoRowMode = false;
        _singleRowRadio->setChecked(true);
        _enableCustomizeCheckbox->setChecked(false);
        _customizationWidget->setVisible(false);
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
            ToolbarActionItem *item = static_cast<ToolbarActionItem *>(_actionsList->item(i));
            actionOrder << item->actionInfo.id;
            bool isEnabled = (item->checkState() == Qt::Checked || item->actionInfo.essential);
            if (isEnabled) {
                enabledActions << item->actionInfo.id;
            }
        }

        // Add row separator if in two-row mode and there are Row 2 actions
        if (_twoRowMode && _secondRowList->count() > 0) {
            actionOrder << "row_separator";

            // Add Row 2 actions
            for (int i = 0; i < _secondRowList->count(); ++i) {
                ToolbarActionItem *item = static_cast<ToolbarActionItem *>(_secondRowList->item(i));
                actionOrder << item->actionInfo.id;
                if (item->checkState() == Qt::Checked || item->actionInfo.essential) {
                    enabledActions << item->actionInfo.id;
                }
            }
        }

        // Actually save the settings to Appearance
        Appearance::setToolbarActionOrder(actionOrder);
        Appearance::setToolbarEnabledActions(enabledActions);
    } catch (...) {
        // If saving fails, just continue - don't crash the settings dialog
    }
}

void LayoutSettingsWidget::triggerToolbarUpdate() {
    // Trigger toolbar rebuild immediately when user makes changes

    try {
        QWidget *widget = this;
        while (widget && !qobject_cast<QMainWindow *>(widget)) {
            widget = widget->parentWidget();
        }
        if (widget) {
            // Use DirectConnection for immediate execution
            QMetaObject::invokeMethod(widget, "rebuildToolbarFromSettings", Qt::DirectConnection);
        }
    } catch (...) {
        // If toolbar update fails, just continue
    }
}

void LayoutSettingsWidget::populateActionsList() {
    populateActionsList(false);
}

void LayoutSettingsWidget::populateActionsList(bool forceRepopulation) {
    // Only populate if customize is enabled
    if (!_enableCustomizeCheckbox->isChecked()) {
        return;
    }

    // Only populate if lists are empty or forcing repopulation
    if ((_actionsList->count() > 0 || _secondRowList->count() > 0) && !forceRepopulation) {
        return;
    }

    // Block signals during population to prevent cascading updates
    _actionsList->blockSignals(true);
    _secondRowList->blockSignals(true);

    // Clear lists
    _actionsList->clear();
    _secondRowList->clear();

    // Get available actions
    _availableActions = getDefaultActions();

    // ALWAYS use clean defaults of the FULL customized list
    // This means: comprehensive action order with default enabled/disabled states
    QStringList orderToUse = getComprehensiveActionOrder(); // Full list in proper order
    QStringList defaultEnabledActions = getDefaultEnabledActions(); // Default enabled states

    // Use empty enabled actions list to force default enabled state
    QStringList enabledActions;

    // Get row distribution
    QStringList row1Actions, row2Actions;
    if (_twoRowMode) {
        getDefaultRowDistribution(row1Actions, row2Actions);
    } else {
        row1Actions = orderToUse;
    }

    // Populate Row 1
    for (const QString &actionId: row1Actions) {
        for (ToolbarActionInfo &info: _availableActions) {
            if (info.id == actionId) {
                info.enabled = enabledActions.isEmpty()
                                   ? (defaultEnabledActions.contains(actionId) || info.essential)
                                   : (enabledActions.contains(actionId) || info.essential);

                ToolbarActionItem *item = new ToolbarActionItem(info, _actionsList);
                item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
                item->setCheckState(info.enabled ? Qt::Checked : Qt::Unchecked);
                break;
            }
        }
    }

    // Populate Row 2 (if in two-row mode)
    if (_twoRowMode) {
        for (const QString &actionId: row2Actions) {
            for (ToolbarActionInfo &info: _availableActions) {
                if (info.id == actionId) {
                    info.enabled = enabledActions.isEmpty()
                                       ? (defaultEnabledActions.contains(actionId) || info.essential)
                                       : (enabledActions.contains(actionId) || info.essential);

                    ToolbarActionItem *item = new ToolbarActionItem(info, _secondRowList);
                    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
                    item->setCheckState(info.enabled ? Qt::Checked : Qt::Unchecked);
                    break;
                }
            }
        }
    }
    // Unblock signals and connect item change handlers
    _actionsList->blockSignals(false);
    _secondRowList->blockSignals(false);

    // Connect signals for item changes
    connect(_actionsList, SIGNAL(itemChanged(QListWidgetItem*)), this, SLOT(itemCheckStateChanged(QListWidgetItem*)));
    connect(_secondRowList, SIGNAL(itemChanged(QListWidgetItem*)), this, SLOT(itemCheckStateChanged(QListWidgetItem*)));
}


void LayoutSettingsWidget::customizeToolbarToggled(bool customizeToolbarEnabled) {
    // Save the customize toolbar setting
    Appearance::setToolbarCustomizeEnabled(customizeToolbarEnabled);

    // Show/hide the customization options
    _customizationWidget->setVisible(customizeToolbarEnabled);

    if (customizeToolbarEnabled) {
        // ALWAYS reset to clean defaults when enabling customization
        // This prevents corruption from previous states
        Appearance::setToolbarActionOrder(QStringList()); // Clear any existing custom order
        Appearance::setToolbarEnabledActions(QStringList()); // Clear any existing custom enabled state

        // Block signals during repopulation to prevent cascading updates
        _actionsList->blockSignals(true);
        _secondRowList->blockSignals(true);

        // Clear and repopulate with fresh defaults for current mode
        _actionsList->clear();
        _secondRowList->clear();
        populateActionsList(true); // This will now use clean defaults

        // Unblock signals after repopulation is complete
        _actionsList->blockSignals(false);
        _secondRowList->blockSignals(false);

        // Show/hide second row based on current mode
        _secondRowLabel->setVisible(_twoRowMode);
        _secondRowList->setVisible(_twoRowMode);

        // Save the clean defaults so MainWindow can build the toolbar
        saveSettings();
        // Update toolbar to show the clean defaults
        triggerToolbarUpdate();
    } else {
        // When disabling customization, clear custom settings and use defaults
        Appearance::setToolbarActionOrder(QStringList());
        Appearance::setToolbarEnabledActions(QStringList());
        // Clear the lists to save memory
        _actionsList->clear();
        _secondRowList->clear();
        // Keep the row mode setting but use default layouts
        triggerToolbarUpdate();
    }
}

void LayoutSettingsWidget::rowModeChanged() {
    _twoRowMode = _doubleRowRadio->isChecked();

    // Always save the row mode preference first
    Appearance::setToolbarTwoRowMode(_twoRowMode);

    if (_customizationWidget->isVisible()) {
        // ALWAYS reset to clean defaults when switching modes with customization enabled
        // This prevents order corruption and ensures consistent behavior
        Appearance::setToolbarActionOrder(QStringList()); // Clear any existing custom order
        Appearance::setToolbarEnabledActions(QStringList()); // Clear any existing custom enabled state

        // Show/hide second row based on mode
        _secondRowLabel->setVisible(_twoRowMode);
        _secondRowList->setVisible(_twoRowMode);

        // Block signals during repopulation to prevent cascading updates
        _actionsList->blockSignals(true);
        _secondRowList->blockSignals(true);

        // Clear and repopulate with fresh defaults for new mode
        _actionsList->clear();
        _secondRowList->clear();
        populateActionsList(true); // This will use clean defaults for the new mode

        // Unblock signals after repopulation is complete
        _actionsList->blockSignals(false);
        _secondRowList->blockSignals(false);

        // Save the clean defaults so MainWindow can build the toolbar
        saveSettings();
        // Update toolbar to show the clean defaults for new mode
        triggerToolbarUpdate();
    } else {
        // When customization is disabled, just update the toolbar with default layout
        // Clear any existing custom settings to ensure defaults are used
        Appearance::setToolbarActionOrder(QStringList());
        Appearance::setToolbarEnabledActions(QStringList());
        // Save the row mode setting so default toolbar respects it
        Appearance::setToolbarTwoRowMode(_twoRowMode);
        // Force immediate toolbar update
        triggerToolbarUpdate();
    }
}

void LayoutSettingsWidget::redistributeActions() {
    // Collect all current actions and their states
    QMap<QString, bool> actionStates;

    // Collect from Row 1
    for (int i = 0; i < _actionsList->count(); ++i) {
        ToolbarActionItem *item = static_cast<ToolbarActionItem *>(_actionsList->item(i));
        actionStates[item->actionInfo.id] = (item->checkState() == Qt::Checked) || item->actionInfo.essential;
    }

    // Collect from Row 2 (if visible)
    for (int i = 0; i < _secondRowList->count(); ++i) {
        ToolbarActionItem *item = static_cast<ToolbarActionItem *>(_secondRowList->item(i));
        actionStates[item->actionInfo.id] = (item->checkState() == Qt::Checked) || item->actionInfo.essential;
    }

    // Clear both lists
    _actionsList->clear();
    _secondRowList->clear();

    // Manually repopulate without triggering signals to avoid performance issues
    _availableActions = getDefaultActions();

    // Use consolidated default order (single source of truth)
    QStringList orderToUse = getComprehensiveActionOrder();

    if (_twoRowMode) {
        // Two-row mode: split actions logically
        QStringList row1Actions, row2Actions;

        // Use consolidated default row distribution (single source of truth)
        QStringList row1DefaultActions, row2DefaultActions;
        getDefaultRowDistribution(row1DefaultActions, row2DefaultActions);

        // Distribute actions
        for (const QString &actionId: orderToUse) {
            if (row1DefaultActions.contains(actionId)) {
                row1Actions << actionId;
            } else if (row2DefaultActions.contains(actionId)) {
                row2Actions << actionId;
            }
        }

        // Populate Row 1
        for (const QString &actionId: row1Actions) {
            for (ToolbarActionInfo &info: _availableActions) {
                if (info.id == actionId) {
                    ToolbarActionItem *item = new ToolbarActionItem(info, _actionsList);
                    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
                    bool enabled = actionStates.contains(actionId) ? actionStates[actionId] : info.essential;
                    item->setCheckState(enabled ? Qt::Checked : Qt::Unchecked);
                    break;
                }
            }
        }

        // Populate Row 2
        for (const QString &actionId: row2Actions) {
            for (ToolbarActionInfo &info: _availableActions) {
                if (info.id == actionId) {
                    ToolbarActionItem *item = new ToolbarActionItem(info, _secondRowList);
                    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
                    bool enabled = actionStates.contains(actionId) ? actionStates[actionId] : info.essential;
                    item->setCheckState(enabled ? Qt::Checked : Qt::Unchecked);
                    break;
                }
            }
        }
    } else {
        // Single-row mode: put all actions in Row 1
        for (const QString &actionId: orderToUse) {
            for (ToolbarActionInfo &info: _availableActions) {
                if (info.id == actionId) {
                    ToolbarActionItem *item = new ToolbarActionItem(info, _actionsList);
                    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
                    bool enabled = actionStates.contains(actionId) ? actionStates[actionId] : info.essential;
                    item->setCheckState(enabled ? Qt::Checked : Qt::Unchecked);
                    break;
                }
            }
        }
    }
}

void LayoutSettingsWidget::actionEnabledChanged() {
    // Only save if customization is enabled
    if (_enableCustomizeCheckbox->isChecked()) {
        saveSettings();
        triggerToolbarUpdate();
    }
}

void LayoutSettingsWidget::itemCheckStateChanged(QListWidgetItem *item) {
    ToolbarActionItem *actionItem = static_cast<ToolbarActionItem *>(item);
    if (actionItem) {
        actionItem->actionInfo.enabled = (item->checkState() == Qt::Checked);
        // Only save if customization is enabled (user is actively customizing)
        if (_enableCustomizeCheckbox->isChecked()) {
            saveSettings();
            triggerToolbarUpdate();
        }
    }
}

void LayoutSettingsWidget::itemsReordered() {
    // Only save if customization is enabled (user is actively customizing)
    if (_enableCustomizeCheckbox->isChecked()) {
        saveSettings();
        triggerToolbarUpdate();
    }
}

bool LayoutSettingsWidget::accept() {
    // Settings are already saved immediately when changed
    // No need to save again on dialog close
    return true;
}

void LayoutSettingsWidget::resetToDefault() {
    try {
        // Block signals during reset to prevent cascading updates
        disconnect(_singleRowRadio, SIGNAL(toggled(bool)), this, SLOT(rowModeChanged()));
        disconnect(_doubleRowRadio, SIGNAL(toggled(bool)), this, SLOT(rowModeChanged()));
        disconnect(_enableCustomizeCheckbox, SIGNAL(toggled(bool)), this, SLOT(customizeToolbarToggled(bool)));

        // Reset to default settings
        _singleRowRadio->setChecked(true);
        _twoRowMode = false;

        // Disable customization and hide customization options
        _enableCustomizeCheckbox->setChecked(false);
        _customizationWidget->setVisible(false);

        // Show/hide second row
        _secondRowLabel->setVisible(false);
        _secondRowList->setVisible(false);

        // Clear saved settings to force defaults
        Appearance::setToolbarActionOrder(QStringList());
        Appearance::setToolbarEnabledActions(QStringList());
        Appearance::setToolbarTwoRowMode(false);
        Appearance::setToolbarCustomizeEnabled(false);

        // Clear the action lists
        _actionsList->clear();
        _secondRowList->clear();

        // Reconnect signals
        connect(_singleRowRadio, SIGNAL(toggled(bool)), this, SLOT(rowModeChanged()));
        connect(_doubleRowRadio, SIGNAL(toggled(bool)), this, SLOT(rowModeChanged()));
        connect(_enableCustomizeCheckbox, SIGNAL(toggled(bool)), this, SLOT(customizeToolbarToggled(bool)));

        // Update toolbar to use defaults (no need to save since customization is disabled)
        triggerToolbarUpdate();
    } catch (...) {
        // If reset fails, just continue
    }
}

QList<ToolbarActionInfo> LayoutSettingsWidget::getDefaultActions() {
    QList<ToolbarActionInfo> actions;

    // Essential actions (New, Open, Save, Undo, Redo) are defined in getEssentialActionIds() and getEssentialActionInfos()

    // Tool actions - these were in the original toolbar, so enable by default
    actions << ToolbarActionInfo{"standard_tool", "Standard Tool", ":/run_environment/graphics/tool/select.png", nullptr, true, false, "Tools"};
    actions << ToolbarActionInfo{"select_left", "Select Left", ":/run_environment/graphics/tool/select_left.png", nullptr, true, false, "Tools"};
    actions << ToolbarActionInfo{"select_right", "Select Right", ":/run_environment/graphics/tool/select_right.png", nullptr, true, false, "Tools"};

    // Additional selection tools (disabled by default)
    actions << ToolbarActionInfo{"select_single", "Select Single", ":/run_environment/graphics/tool/select_single.png", nullptr, false, false, "Tools"};
    actions << ToolbarActionInfo{"select_box", "Select Box", ":/run_environment/graphics/tool/select_box.png", nullptr, false, false, "Tools"};

    // Edit actions
    actions << ToolbarActionInfo{"separator2", "--- Separator ---", "", nullptr, true, false, "Separator"};
    actions << ToolbarActionInfo{"new_note", "New Note", ":/run_environment/graphics/tool/newnote.png", nullptr, true, false, "Edit"};
    actions << ToolbarActionInfo{"remove_notes", "Remove Notes", ":/run_environment/graphics/tool/eraser.png", nullptr, true, false, "Edit"};
    actions << ToolbarActionInfo{"copy", "Copy", ":/run_environment/graphics/tool/copy.png", nullptr, true, false, "Edit"};
    actions << ToolbarActionInfo{"paste", "Paste", ":/run_environment/graphics/tool/paste.png", nullptr, true, false, "Edit"};

    // Tool actions
    actions << ToolbarActionInfo{"separator3", "--- Separator ---", "", nullptr, true, false, "Separator"};
    actions << ToolbarActionInfo{"glue", "Glue Notes (Same Channel)", ":/run_environment/graphics/tool/glue.png", nullptr, true, false, "Tools"};
    actions << ToolbarActionInfo{"glue_all_channels", "Glue Notes (All Channels)", ":/run_environment/graphics/tool/glue.png", nullptr, false, false, "Tools"};
    actions << ToolbarActionInfo{"scissors", "Scissors", ":/run_environment/graphics/tool/scissors.png", nullptr, true, false, "Tools"};
    actions << ToolbarActionInfo{"delete_overlaps", "Delete Overlaps", ":/run_environment/graphics/tool/deleteoverlap.png", nullptr, true, false, "Tools"};

    // Playback actions
    actions << ToolbarActionInfo{"separator4", "--- Separator ---", "", nullptr, true, false, "Separator"};
    actions << ToolbarActionInfo{"back_to_begin", "Back to Begin", ":/run_environment/graphics/tool/back_to_begin.png", nullptr, true, false, "Playback"};
    actions << ToolbarActionInfo{"back_marker", "Back Marker", ":/run_environment/graphics/tool/back_marker.png", nullptr, true, false, "Playback"};
    actions << ToolbarActionInfo{"back", "Back", ":/run_environment/graphics/tool/back.png", nullptr, true, false, "Playback"};
    actions << ToolbarActionInfo{"play", "Play", ":/run_environment/graphics/tool/play.png", nullptr, true, false, "Playback"};
    actions << ToolbarActionInfo{"pause", "Pause", ":/run_environment/graphics/tool/pause.png", nullptr, true, false, "Playback"};
    actions << ToolbarActionInfo{"stop", "Stop", ":/run_environment/graphics/tool/stop.png", nullptr, true, false, "Playback"};
    actions << ToolbarActionInfo{"record", "Record", ":/run_environment/graphics/tool/record.png", nullptr, true, false, "Playback"};
    actions << ToolbarActionInfo{"forward", "Forward", ":/run_environment/graphics/tool/forward.png", nullptr, true, false, "Playback"};
    actions << ToolbarActionInfo{"forward_marker", "Forward Marker", ":/run_environment/graphics/tool/forward_marker.png", nullptr, true, false, "Playback"};

    // Additional tools - these were in the original toolbar, so enable by default
    actions << ToolbarActionInfo{"separator5", "--- Separator ---", "", nullptr, true, false, "Separator"};
    actions << ToolbarActionInfo{"metronome", "Metronome", ":/run_environment/graphics/tool/metronome.png", nullptr, true, false, "Playback" };
    actions << ToolbarActionInfo{"align_left", "Align Left", ":/run_environment/graphics/tool/align_left.png", nullptr, true, false, "Tools"};
    actions << ToolbarActionInfo{"equalize", "Equalize", ":/run_environment/graphics/tool/equalize.png", nullptr, true, false, "Tools"};
    actions << ToolbarActionInfo{"align_right", "Align Right", ":/run_environment/graphics/tool/align_right.png", nullptr, true, false, "Tools"};

    // Zoom actions
    actions << ToolbarActionInfo{"separator6", "--- Separator ---", "", nullptr, true, false, "Separator"};
    actions << ToolbarActionInfo{"zoom_hor_in", "Zoom Horizontal In", ":/run_environment/graphics/tool/zoom_hor_in.png", nullptr, true, false,"View"};
    actions << ToolbarActionInfo{"zoom_hor_out", "Zoom Horizontal Out", ":/run_environment/graphics/tool/zoom_hor_out.png", nullptr, true, false, "View"};
    actions << ToolbarActionInfo{"zoom_ver_in", "Zoom Vertical In", ":/run_environment/graphics/tool/zoom_ver_in.png", nullptr, true, false, "View"};
    actions << ToolbarActionInfo{"zoom_ver_out", "Zoom Vertical Out", ":/run_environment/graphics/tool/zoom_ver_out.png", nullptr, true, false, "View"};
    actions << ToolbarActionInfo{"lock", "Lock Screen", ":/run_environment/graphics/tool/screen_unlocked.png", nullptr, true, false, "View"};

    // Additional tools
    actions << ToolbarActionInfo{"separator7", "--- Separator ---", "", nullptr, true, false, "Separator"};
    actions << ToolbarActionInfo{"quantize", "Quantize", ":/run_environment/graphics/tool/quantize.png", nullptr, true, false, "Tools"};
    actions << ToolbarActionInfo{"magnet", "Magnet", ":/run_environment/graphics/tool/magnet.png", nullptr, true, false, "Tools"};

    // MIDI actions
    actions << ToolbarActionInfo{"separator8", "--- Separator ---", "", nullptr, true, false, "Separator"};
    actions << ToolbarActionInfo{"thru", "MIDI Thru", ":/run_environment/graphics/tool/connection.png", nullptr, true, false, "MIDI"};
    actions << ToolbarActionInfo{"separator9", "--- Separator ---", "", nullptr, true, false, "Separator"};
    actions << ToolbarActionInfo{"measure", "Measure", ":/run_environment/graphics/tool/measure.png", nullptr, true, false, "View"};
    actions << ToolbarActionInfo{"time_signature", "Time Signature", ":/run_environment/graphics/tool/meter.png", nullptr, true, false, "View"};
    actions << ToolbarActionInfo{"tempo", "Tempo", ":/run_environment/graphics/tool/tempo.png", nullptr, true, false, "View"};

    // Movement and editing tools (from MainWindow action map) - disabled by default but available
    actions << ToolbarActionInfo{"separator10", "--- Separator ---", "", nullptr, true, false, "Separator"};
    actions << ToolbarActionInfo{"move_all", "Move All Directions", ":/run_environment/graphics/tool/move_up_down_left_right.png", nullptr, false, false, "Tools"};
    actions << ToolbarActionInfo{"move_lr", "Move Left/Right", ":/run_environment/graphics/tool/move_left_right.png", nullptr, false, false, "Tools"};
    actions << ToolbarActionInfo{"move_ud", "Move Up/Down", ":/run_environment/graphics/tool/move_up_down.png", nullptr, false, false, "Tools"};
    actions << ToolbarActionInfo{"size_change", "Size Change", ":/run_environment/graphics/tool/change_size.png", nullptr, false, false, "Tools"};

    // Additional useful actions (only include those with icons)
    actions << ToolbarActionInfo{"separator11", "--- Separator ---", "", nullptr, true, false, "Separator"};
    actions << ToolbarActionInfo{"panic", "MIDI Panic", ":/run_environment/graphics/tool/panic.png", nullptr, false, false, "MIDI"};
    actions << ToolbarActionInfo{"separator12", "--- Separator ---", "", nullptr, true, false, "Separator"};
    actions << ToolbarActionInfo{"transpose", "Transpose Selection", ":/run_environment/graphics/tool/transpose.png", nullptr, false, false, "Tools"};
    actions << ToolbarActionInfo{"transpose_up", "Transpose Up", ":/run_environment/graphics/tool/transpose_up.png", nullptr, false, false, "Tools"};
    actions << ToolbarActionInfo{"transpose_down", "Transpose Down", ":/run_environment/graphics/tool/transpose_down.png", nullptr, false, false, "Tools"};

    return actions;
}

QIcon LayoutSettingsWidget::icon() {
    return QIcon(); // No icon for Layout tab
}

void LayoutSettingsWidget::refreshIcons() {
    // Temporarily disconnect itemChanged signals to prevent cascade during theme changes
    disconnect(_actionsList, SIGNAL(itemChanged(QListWidgetItem*)), this, SLOT(itemCheckStateChanged(QListWidgetItem*)));
    disconnect(_secondRowList, SIGNAL(itemChanged(QListWidgetItem*)), this, SLOT(itemCheckStateChanged(QListWidgetItem*)));

    // Refresh all icons in both action lists when theme changes
    for (int i = 0; i < _actionsList->count(); ++i) {
        ToolbarActionItem *item = static_cast<ToolbarActionItem *>(_actionsList->item(i));
        if (item) {
            item->updateDisplay(); // This will call adjustIconForDarkMode
        }
    }

    // Also refresh icons in the second row list
    for (int i = 0; i < _secondRowList->count(); ++i) {
        ToolbarActionItem *item = static_cast<ToolbarActionItem *>(_secondRowList->item(i));
        if (item) {
            item->updateDisplay(); // This will call adjustIconForDarkMode
        }
    }

    // Reconnect the signals after refreshing
    connect(_actionsList, SIGNAL(itemChanged(QListWidgetItem*)), this, SLOT(itemCheckStateChanged(QListWidgetItem*)));
    connect(_secondRowList, SIGNAL(itemChanged(QListWidgetItem*)), this, SLOT(itemCheckStateChanged(QListWidgetItem*)));
}

void LayoutSettingsWidget::iconSizeChanged(int size) {
    // CRITICAL FIX: If customize is enabled but lists are empty, repopulate them
    if (_enableCustomizeCheckbox->isChecked() && _actionsList->count() == 0) {
        populateActionsList(true); // Force repopulation
        if (_actionsList->count() > 0) {
            saveSettings(); // Save the repopulated settings
        }
    }

    Appearance::setToolbarIconSize(size);
    // Icon size changes require complete rebuild, so use immediate update
    triggerToolbarUpdate(); // Unfortunately, icon size changes require complete rebuild
}

void LayoutSettingsWidget::debouncedToolbarUpdate() {
    // Start or restart the timer - this debounces rapid updates
    _updateTimer->start();
}

// ===== CONSOLIDATED DEFAULT CONFIGURATION METHODS =====
// These methods provide single source of truth for all default configurations

QStringList LayoutSettingsWidget::getComprehensiveActionOrder() {
    // Single source of truth for action order - optimized for single-row flow
    QStringList order;
    order << "standard_tool" << "select_left" << "select_right" << "select_single" << "select_box" << "separator2"
            << "new_note" << "remove_notes" << "copy" << "paste" << "separator3"
            << "glue" << "glue_all_channels" << "scissors" << "delete_overlaps" << "separator4"
            << "move_all" << "move_lr" << "move_ud" << "size_change" << "separator5"
            << "transpose" << "transpose_up" << "transpose_down" << "separator6"
            << "back_to_begin" << "back_marker" << "back" << "play" << "pause"
            << "stop" << "record" << "forward" << "forward_marker" << "separator7"
            << "metronome"
            << "align_left" << "equalize" << "align_right" << "separator9"
            << "zoom_hor_in" << "zoom_hor_out" << "zoom_ver_in" << "zoom_ver_out"
            << "lock" << "separator10"
            << "quantize" << "magnet" << "separator11"
            << "thru" << "panic" << "separator12"
            << "measure" << "time_signature" << "tempo";
    return order;
}

QStringList LayoutSettingsWidget::getDefaultEnabledActions() {
    // Single source of truth for which actions are enabled by default
    QStringList enabled;
    enabled << "standard_tool" << "select_left" << "select_right" << "separator2"
            << "new_note" << "remove_notes" << "copy" << "paste" << "separator3"
            << "glue" << "scissors" << "delete_overlaps" << "separator4"
            // separator5 disabled because move actions are disabled by default
            // separator6 disabled because transpose actions are disabled by default
            << "back_to_begin" << "back_marker" << "back" << "play" << "pause"
            << "stop" << "record" << "forward" << "forward_marker" << "separator7"
            << "metronome"
            << "align_left" << "equalize" << "align_right" << "separator9"
            << "zoom_hor_in" << "zoom_hor_out" << "zoom_ver_in" << "zoom_ver_out"
            << "lock" << "separator10"
            << "quantize" << "magnet" << "separator11"
            // thru and panic disabled by default
            << "separator12"
            << "measure" << "time_signature" << "tempo";
    return enabled;
}

void LayoutSettingsWidget::getDefaultRowDistribution(QStringList &row1Actions, QStringList &row2Actions) {
    // Single source of truth for how actions are distributed between rows
    row1Actions.clear();
    row2Actions.clear();

    // Row 1: Editing and tool actions
    row1Actions << "standard_tool" << "select_left" << "select_right" << "select_single" << "select_box" << "separator2"
            << "new_note" << "remove_notes" << "copy" << "paste" << "separator3"
            << "glue" << "glue_all_channels" << "scissors" << "delete_overlaps" << "separator4"
            << "move_all" << "move_lr" << "move_ud" << "size_change" << "separator5"
            << "transpose" << "transpose_up" << "transpose_down" << "separator6"
            << "align_left" << "equalize" << "align_right" << "separator9"
            << "quantize" << "magnet" << "separator11"
            << "measure" << "time_signature" << "tempo";

    // Row 2: Playback and view actions
    row2Actions << "back_to_begin" << "back_marker" << "back" << "play" << "pause"
            << "stop" << "record" << "forward" << "forward_marker" << "separator7"
            << "metronome"
            << "zoom_hor_in" << "zoom_hor_out" << "zoom_ver_in" << "zoom_ver_out"
            << "lock" << "separator10" << "thru" << "panic";
}

QStringList LayoutSettingsWidget::getEssentialActionIds() {
    // Single source of truth for essential action IDs
    // These actions are always present and cannot be disabled
    // Includes separator2 after redo because customizable actions don't start with separators
    QStringList essential;
    essential << "new" << "open" << "save" << "separator1" << "undo" << "redo" << "separator2";
    return essential;
}

QList<ToolbarActionInfo> LayoutSettingsWidget::getEssentialActionInfos() {
    // Single source of truth for essential action info objects
    // Used for fallback scenarios when full action info is needed
    // Includes separator2 after redo because customizable actions don't start with separators
    QList<ToolbarActionInfo> essential;
    essential << ToolbarActionInfo{"new", "New", ":/run_environment/graphics/tool/new.png", nullptr, true, true, "File"};
    essential << ToolbarActionInfo{"open", "Open", ":/run_environment/graphics/tool/load.png", nullptr, true, true, "File"};
    essential << ToolbarActionInfo{"save", "Save", ":/run_environment/graphics/tool/save.png", nullptr, true, true, "File"};
    essential << ToolbarActionInfo{"separator1", "--- Separator ---", "", nullptr, true, true, "Separator"};
    essential << ToolbarActionInfo{"undo", "Undo", ":/run_environment/graphics/tool/undo.png", nullptr, true, true, "Edit"};
    essential << ToolbarActionInfo{"redo", "Redo", ":/run_environment/graphics/tool/redo.png", nullptr, true, true, "Edit"};
    essential << ToolbarActionInfo{"separator2", "--- Separator ---", "", nullptr, true, true, "Separator"};
    return essential;
}

// ===== DEFAULT TOOLBAR METHODS (when customization is disabled) =====
// These provide minimal default toolbars with only commonly used actions

QStringList LayoutSettingsWidget::getDefaultToolbarOrder() {
    // Minimal default toolbar order - only essential + commonly used actions
    // This is what users see when customization is disabled
    QStringList order;
    order << "standard_tool" << "select_left" << "select_right" << "separator2"
            << "new_note" << "remove_notes" << "copy" << "paste" << "separator3"
            << "glue" << "scissors" << "delete_overlaps" << "separator4"
            << "back_to_begin" << "back_marker" << "back" << "play" << "pause"
            << "stop" << "record" << "forward" << "forward_marker" << "separator5"
            << "metronome" << "align_left" << "equalize" << "align_right" << "separator6"
            << "zoom_hor_in" << "zoom_hor_out" << "zoom_ver_in" << "zoom_ver_out"
            << "lock" << "separator7" << "quantize" << "magnet" << "separator8"
            << "measure" << "time_signature" << "tempo";
    return order;
}

QStringList LayoutSettingsWidget::getDefaultToolbarEnabledActions() {
    // All actions in the default toolbar are enabled by default
    return getDefaultToolbarOrder();
}

void LayoutSettingsWidget::getDefaultToolbarRowDistribution(QStringList &row1Actions, QStringList &row2Actions) {
    // Default toolbar row distribution - simpler than comprehensive
    row1Actions.clear();
    row2Actions.clear();

    // Row 1: Editing tools
    row1Actions << "standard_tool" << "select_left" << "select_right" << "separator2"
            << "new_note" << "remove_notes" << "copy" << "paste" << "separator3"
            << "glue" << "scissors" << "delete_overlaps" << "separator4"
            << "separator5" // Keep for consistency even though move actions not in default toolbar
            << "separator6" // Keep for consistency even though transpose actions not in default toolbar
            << "align_left" << "equalize" << "align_right" << "separator6"
            << "quantize" << "magnet" << "separator8"
            << "measure" << "time_signature" << "tempo";

    // Row 2: Playback and view
    row2Actions << "back_to_begin" << "back_marker" << "back" << "play" << "pause"
            << "stop" << "record" << "forward" << "forward_marker" << "separator5"
            << "metronome" << "zoom_hor_in" << "zoom_hor_out" << "zoom_ver_in" << "zoom_ver_out"
            << "lock" << "separator7";
}
