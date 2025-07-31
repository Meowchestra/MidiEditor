#ifndef LAYOUTSETTINGSWIDGET_H
#define LAYOUTSETTINGSWIDGET_H

#include "SettingsWidget.h"
#include "ToolbarActionInfo.h"
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

class QAction;

class DraggableListWidget : public QListWidget {
    Q_OBJECT

public:
    explicit DraggableListWidget(QWidget *parent = nullptr);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;

    void dragMoveEvent(QDragMoveEvent *event) override;

    void dropEvent(QDropEvent *event) override;

    void startDrag(Qt::DropActions supportedActions) override;

signals:
    void itemsReordered();
};

class ToolbarActionItem : public QListWidgetItem {
public:
    ToolbarActionItem(const ToolbarActionInfo &info, QListWidget *parent = nullptr);

    ToolbarActionInfo actionInfo;

    void updateDisplay();
};

class LayoutSettingsWidget : public SettingsWidget {
    Q_OBJECT

public:
    LayoutSettingsWidget(QWidget *parent = nullptr);

    QIcon icon() override;

    virtual bool accept() override;

    // Static methods for consolidated default configurations (accessible to MainWindow)
    static QStringList getComprehensiveActionOrder();

    static QStringList getDefaultEnabledActions();

    static void getDefaultRowDistribution(QStringList &row1Actions, QStringList &row2Actions);

    static QStringList getEssentialActionIds();

    static QList<ToolbarActionInfo> getEssentialActionInfos();

    // Static methods for default toolbar (when customization is disabled)
    static QStringList getDefaultToolbarOrder();

    static QStringList getDefaultToolbarEnabledActions();

    static void getDefaultToolbarRowDistribution(QStringList &row1Actions, QStringList &row2Actions);

public slots:
    void customizeToolbarToggled(bool customizeToolbarEnabled);

    void rowModeChanged();

    void actionEnabledChanged();

    void resetToDefault();

    void itemsReordered();

    void itemCheckStateChanged(QListWidgetItem *item);

    void refreshIcons(); // Refresh icons when theme changes
    void iconSizeChanged(int size);

    void debouncedToolbarUpdate(); // Debounced toolbar update

private:
    void setupUI();

    void loadSettings();

    void saveSettings();

    void triggerToolbarUpdate();

    void populateActionsList();

    void populateActionsList(bool forceRepopulation);

    void redistributeActions();

    void updateActionOrder();

    QList<ToolbarActionInfo> getDefaultActions();

    QCheckBox *_enableCustomizeCheckbox;
    QRadioButton *_singleRowRadio;
    QRadioButton *_doubleRowRadio;
    QSpinBox *_iconSizeSpinBox;
    DraggableListWidget *_actionsList;
    DraggableListWidget *_secondRowList; // For two-row mode
    QLabel *_secondRowLabel;
    QHBoxLayout *_actionsLayout;
    QPushButton *_resetButton;
    QWidget *_customizationWidget; // Container for customization options

    QList<ToolbarActionInfo> _availableActions;
    bool _twoRowMode;
    QTimer *_updateTimer; // Timer for debouncing toolbar updates
};

#endif // LAYOUTSETTINGSWIDGET_H
