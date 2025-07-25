#ifndef LAYOUTSETTINGSWIDGET_H
#define LAYOUTSETTINGSWIDGET_H

#include "SettingsWidget.h"
#include "ToolbarActionInfo.h"
#include <QWidget>
#include <QGridLayout>
#include <QLabel>
#include <QCheckBox>
#include <QSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QListWidget>
#include <QListWidgetItem>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QButtonGroup>
#include <QRadioButton>

class QAction;

class DraggableListWidget : public QListWidget {
    Q_OBJECT

public:
    explicit DraggableListWidget(QWidget* parent = nullptr);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void startDrag(Qt::DropActions supportedActions) override;

signals:
    void itemsReordered();
};

class ToolbarActionItem : public QListWidgetItem {
public:
    ToolbarActionItem(const ToolbarActionInfo& info, QListWidget* parent = nullptr);

    ToolbarActionInfo actionInfo;

    void updateDisplay();
};

class LayoutSettingsWidget : public SettingsWidget {
    Q_OBJECT

public:
    LayoutSettingsWidget(QWidget* parent = nullptr);
    QIcon icon() override;

public slots:
    void rowModeChanged();
    void actionEnabledChanged();
    void resetToDefault();
    void itemsReordered();
    void itemCheckStateChanged(QListWidgetItem* item);
    void refreshIcons(); // Refresh icons when theme changes

private:
    void setupUI();
    void loadSettings();
    void saveSettings();
    void populateActionsList();
    void updateActionOrder();
    QList<ToolbarActionInfo> getDefaultActions();
    
    QRadioButton* _singleRowRadio;
    QRadioButton* _doubleRowRadio;
    DraggableListWidget* _actionsList;
    DraggableListWidget* _secondRowList; // For two-row mode
    QLabel* _secondRowLabel;
    QHBoxLayout* _actionsLayout;
    QPushButton* _resetButton;
    
    QList<ToolbarActionInfo> _availableActions;
    bool _twoRowMode;
};

#endif // LAYOUTSETTINGSWIDGET_H
