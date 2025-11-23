/*
 * MidiEditor
 *
 * Control Change Settings Widget
 */

#include "ControlChangeSettingsWidget.h"
#include "Appearance.h"
#include "../midi/InstrumentDefinitions.h"
#include "../midi/MidiFile.h"

#include <QSettings>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>

ControlChangeSettingsWidget::ControlChangeSettingsWidget(QSettings *settings, QWidget *parent)
    : SettingsWidget(tr("Control Changes"), parent) {
    _settings = settings;

    QGridLayout *layout = new QGridLayout(this);
    setLayout(layout);

    _infoBox = createInfoBox(tr("Edit the names of Control Change (CC) messages."));
    layout->addWidget(_infoBox, 0, 0, 1, 2);

    // Action buttons
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->setContentsMargins(0, 0, 0, 0);
    
    QPushButton *clearBtn = new QPushButton(tr("Clear Configuration"), this);
    clearBtn->setToolTip(tr("Reset to default names"));
    connect(clearBtn, SIGNAL(clicked()), this, SLOT(clearSettings()));
    btnLayout->addWidget(clearBtn);
    
    btnLayout->addStretch();
    layout->addLayout(btnLayout, 1, 0, 1, 2);

    // Table for viewing/editing
    _tableWidget = new QTableWidget(128, 2, this);
    _tableWidget->setHorizontalHeaderLabels(QStringList() << tr("CC #") << tr("Name"));
    _tableWidget->verticalHeader()->setVisible(false);
    _tableWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    _tableWidget->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    connect(_tableWidget, SIGNAL(itemChanged(QTableWidgetItem*)), this, SLOT(onTableItemChanged(QTableWidgetItem*)));
    layout->addWidget(_tableWidget, 2, 0, 1, 2);

    layout->setRowStretch(2, 1);

    populateTable();
}

void ControlChangeSettingsWidget::clearSettings() {
    for(int i=0; i<128; i++){
        InstrumentDefinitions::instance()->setControlChangeName(i, "");
    }
    populateTable();
}

void ControlChangeSettingsWidget::populateTable() {
    _tableWidget->blockSignals(true);
    _tableWidget->clearContents();
    
    QMap<int, QString> names = InstrumentDefinitions::instance()->controlChangeNames();
    
    for (int i = 0; i < 128; i++) {
        QTableWidgetItem *numItem = new QTableWidgetItem(QString::number(i));
        numItem->setFlags(numItem->flags() & ~Qt::ItemIsEditable);
        _tableWidget->setItem(i, 0, numItem);
        
        QString name = names.value(i, "");
        QTableWidgetItem *nameItem;
        
        // Use MidiFile::controlChangeName(i) to get the name.
        // If custom name exists, it returns custom name.
        // If not, it returns default name.
        // We want to check if it is custom or default to gray out default.
        
        if (name.isEmpty()) {
            // It's a default name
            name = MidiFile::controlChangeName(i);
            nameItem = new QTableWidgetItem(name);
            nameItem->setForeground(QBrush(Qt::gray));
        } else {
            // It's a custom name
            nameItem = new QTableWidgetItem(name);
        }
        
        _tableWidget->setItem(i, 1, nameItem);
    }
    _tableWidget->blockSignals(false);
}

void ControlChangeSettingsWidget::onTableItemChanged(QTableWidgetItem *item) {
    if (item->column() == 1) {
        QTableWidgetItem *numItem = _tableWidget->item(item->row(), 0);
        if (!numItem) return;

        int control = numItem->text().toInt();
        QString name = item->text();
        
        InstrumentDefinitions::instance()->setControlChangeName(control, name);
        
        _tableWidget->blockSignals(true);
        if (name.isEmpty()) {
             // Reset to default
             item->setText(MidiFile::controlChangeName(control));
             item->setForeground(QBrush(Qt::gray));
        } else {
             item->setData(Qt::ForegroundRole, QVariant()); // Reset color
        }
        _tableWidget->blockSignals(false);
    }
}

void ControlChangeSettingsWidget::refreshColors() {
    if (_infoBox) {
        QLabel *label = qobject_cast<QLabel *>(_infoBox);
        if (label) {
            QColor bgColor = Appearance::infoBoxBackgroundColor();
            QColor textColor = Appearance::infoBoxTextColor();
            QString styleSheet = QString("color: rgb(%1, %2, %3); background-color: rgb(%4, %5, %6); padding: 5px")
                    .arg(textColor.red()).arg(textColor.green()).arg(textColor.blue())
                    .arg(bgColor.red()).arg(bgColor.green()).arg(bgColor.blue());
            label->setStyleSheet(styleSheet);
        }
    }
    update();
}

bool ControlChangeSettingsWidget::accept() {
    InstrumentDefinitions::instance()->saveOverrides(_settings);
    return true;
}
