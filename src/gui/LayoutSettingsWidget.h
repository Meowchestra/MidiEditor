#ifndef LAYOUTSETTINGSWIDGET_H_
#define LAYOUTSETTINGSWIDGET_H_

// Project includes
#include "SettingsWidget.h"
#include "ToolbarActionInfo.h"

// Qt includes
#include <QWidget>
#include <QGridLayout>
#include <QLabel>
#include <QCheckBox>
#include <QSpinBox>
#include <QPushButton>
#include <QListWidgetItem>
#include <QVBoxLayout>
#include <QButtonGroup>
#include <QRadioButton>

// Forward declarations
class QAction;

/**
 * \class DraggableListWidget
 *
 * \brief List widget with drag-and-drop support for reordering items.
 *
 * DraggableListWidget extends QListWidget to provide drag-and-drop
 * functionality for reordering toolbar actions. It supports:
 *
 * - **Drag and drop**: Reorder items by dragging
 * - **Visual feedback**: Shows drop indicators during drag operations
 * - **Change notification**: Emits signals when items are reordered
 */
class DraggableListWidget : public QListWidget {
    Q_OBJECT

public:
    /**
     * \brief Creates a new DraggableListWidget.
     * \param parent The parent widget
     */
    explicit DraggableListWidget(QWidget *parent = nullptr);

protected:
    /**
     * \brief Handles drag enter events.
     * \param event The drag enter event
     */
    void dragEnterEvent(QDragEnterEvent *event) override;

    /**
     * \brief Handles drag move events.
     * \param event The drag move event
     */
    void dragMoveEvent(QDragMoveEvent *event) override;

    /**
     * \brief Handles drop events to reorder items.
     * \param event The drop event
     */
    void dropEvent(QDropEvent *event) override;

    /**
     * \brief Starts a drag operation.
     * \param supportedActions The supported drag actions
     */
    void startDrag(Qt::DropActions supportedActions) override;

signals:
    /**
     * \brief Emitted when items are reordered.
     */
    void itemsReordered();
};

/**
 * \class ToolbarActionItem
 *
 * \brief List widget item representing a toolbar action.
 *
 * ToolbarActionItem encapsulates a toolbar action with its associated
 * information and provides methods for display updates.
 */
class ToolbarActionItem : public QListWidgetItem {
public:
    /**
     * \brief Creates a new ToolbarActionItem.
     * \param info The toolbar action information
     * \param parent The parent list widget
     */
    ToolbarActionItem(const ToolbarActionInfo &info, QListWidget *parent = nullptr);

    /** \brief The toolbar action information */
    ToolbarActionInfo actionInfo;

    /**
     * \brief Updates the display of this item.
     */
    void updateDisplay();
};

/**
 * \class LayoutSettingsWidget
 *
 * \brief Settings widget for customizing toolbar layout and appearance.
 *
 * LayoutSettingsWidget provides comprehensive toolbar customization options:
 *
 * - **Action management**: Enable/disable toolbar actions
 * - **Order customization**: Drag-and-drop reordering of actions
 * - **Row configuration**: Single or double-row toolbar layout
 * - **Icon sizing**: Adjustable toolbar icon sizes
 * - **Default presets**: Reset to default configurations
 * - **Real-time preview**: Immediate toolbar updates
 *
 * The widget supports both simple and advanced customization modes,
 * allowing users to tailor the toolbar to their workflow preferences.
 */
class LayoutSettingsWidget : public SettingsWidget {
    Q_OBJECT

public:
    /**
     * \brief Creates a new LayoutSettingsWidget.
     * \param parent The parent widget
     */
    LayoutSettingsWidget(QWidget *parent = nullptr);

    /**
     * \brief Gets the icon for this settings panel.
     * \return QIcon for the layout settings
     */
    QIcon icon() override;

    /**
     * \brief Validates and applies the layout settings.
     * \return True if settings are valid and applied successfully
     */
    virtual bool accept() override;

    // === Static Methods for Default Configurations ===

    /**
     * \brief Gets the comprehensive action order for toolbar.
     * \return QStringList containing all action IDs in default order
     */
    static QStringList getComprehensiveActionOrder();

    /**
     * \brief Gets the default enabled actions.
     * \return QStringList containing default enabled action IDs
     */
    static QStringList getDefaultEnabledActions();

    /**
     * \brief Gets the default row distribution for two-row mode.
     * \param row1Actions Reference to receive first row action IDs
     * \param row2Actions Reference to receive second row action IDs
     */
    static void getDefaultRowDistribution(QStringList &row1Actions, QStringList &row2Actions);

    /**
     * \brief Gets the essential action IDs that should always be available.
     * \return QStringList containing essential action IDs
     */
    static QStringList getEssentialActionIds();

    /**
     * \brief Gets the essential action information objects.
     * \return QList containing ToolbarActionInfo for essential actions
     */
    static QList<ToolbarActionInfo> getEssentialActionInfos();

    // === Static Methods for Default Toolbar (when customization is disabled) ===

    /**
     * \brief Gets the default toolbar order when customization is disabled.
     * \return QStringList containing default toolbar action order
     */
    static QStringList getDefaultToolbarOrder();

    /**
     * \brief Gets the default enabled actions when customization is disabled.
     * \return QStringList containing default enabled action IDs
     */
    static QStringList getDefaultToolbarEnabledActions();

    /**
     * \brief Gets the default row distribution when customization is disabled.
     * \param row1Actions Reference to receive first row action IDs
     * \param row2Actions Reference to receive second row action IDs
     */
    static void getDefaultToolbarRowDistribution(QStringList &row1Actions, QStringList &row2Actions);

public slots:
    /**
     * \brief Handles toolbar customization toggle.
     * \param customizeToolbarEnabled True if customization is enabled
     */
    void customizeToolbarToggled(bool customizeToolbarEnabled);

    /**
     * \brief Handles row mode changes (single/double row).
     */
    void rowModeChanged();

    /**
     * \brief Handles action enabled state changes.
     */
    void actionEnabledChanged();

    /**
     * \brief Resets toolbar to default configuration.
     */
    void resetToDefault();

    /**
     * \brief Handles item reordering in the lists.
     */
    void itemsReordered();

    /**
     * \brief Handles check state changes for list items.
     * \param item The item whose check state changed
     */
    void itemCheckStateChanged(QListWidgetItem *item);

    /**
     * \brief Refreshes icons when theme changes.
     */
    void refreshIcons();

    /**
     * \brief Handles icon size changes.
     * \param size The new icon size
     */
    void iconSizeChanged(int size);

    /**
     * \brief Performs debounced toolbar update.
     */
    void debouncedToolbarUpdate();

private:
    // === Setup and Management Methods ===

    /**
     * \brief Sets up the user interface.
     */
    void setupUI();

    /**
     * \brief Loads settings from configuration.
     */
    void loadSettings();

    /**
     * \brief Saves settings to configuration.
     */
    void saveSettings();

    /**
     * \brief Triggers a toolbar update.
     */
    void triggerToolbarUpdate();

    /**
     * \brief Populates the actions list.
     */
    void populateActionsList();

    /**
     * \brief Populates the actions list with optional force repopulation.
     * \param forceRepopulation True to force complete repopulation
     */
    void populateActionsList(bool forceRepopulation);

    /**
     * \brief Redistributes actions between rows.
     */
    void redistributeActions();

    /**
     * \brief Updates the action order.
     */
    void updateActionOrder();

    /**
     * \brief Gets the default actions list.
     * \return QList containing default ToolbarActionInfo objects
     */
    QList<ToolbarActionInfo> getDefaultActions();

    // === UI Components ===

    /** \brief Checkbox for enabling toolbar customization */
    QCheckBox *_enableCustomizeCheckbox;

    /** \brief Radio buttons for row mode selection */
    QRadioButton *_singleRowRadio;
    QRadioButton *_doubleRowRadio;

    /** \brief Spin box for icon size selection */
    QSpinBox *_iconSizeSpinBox;

    /** \brief List widgets for action management */
    DraggableListWidget *_actionsList;
    DraggableListWidget *_secondRowList; ///< For two-row mode

    /** \brief UI layout and control elements */
    QLabel *_secondRowLabel;
    QHBoxLayout *_actionsLayout;
    QPushButton *_resetButton;
    QWidget *_customizationWidget; ///< Container for customization options

    // === Data Members ===

    /** \brief List of available toolbar actions */
    QList<ToolbarActionInfo> _availableActions;

    /** \brief Flag indicating two-row mode */
    bool _twoRowMode;

    /** \brief Timer for debouncing toolbar updates */
    QTimer *_updateTimer;
};

#endif // LAYOUTSETTINGSWIDGET_H_
