/*
 * MidiEditor
 *
 * Instrument Settings Widget
 */

#include "InstrumentSettingsWidget.h"
#include "Appearance.h"
#include "../midi/InstrumentDefinitions.h"

#include <QSettings>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QFileDialog>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>

InstrumentSettingsWidget::InstrumentSettingsWidget(QSettings *settings, QWidget *parent)
    : SettingsWidget(tr("Instruments"), parent) {
    _settings = settings;

    QGridLayout *layout = new QGridLayout(this);
    setLayout(layout);

    _infoBox = createInfoBox(tr("Load an instrument definition file (.ins) to map MIDI program numbers to instrument names. You can also edit names in the table below."));
    layout->addWidget(_infoBox, 0, 0, 1, 3);

    // File selection
    layout->addWidget(new QLabel(tr("Definition file:"), this), 1, 0);
    _fileEdit = new QLineEdit(this);
    _fileEdit->setReadOnly(true);
    layout->addWidget(_fileEdit, 1, 1);
    
    QPushButton *browseBtn = new QPushButton(tr("Browse..."), this);
    connect(browseBtn, SIGNAL(clicked()), this, SLOT(browseFile()));
    layout->addWidget(browseBtn, 1, 2);

    // Action buttons
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->setContentsMargins(0, 0, 0, 0);
    
    QPushButton *clearBtn = new QPushButton(tr("Clear Configuration"), this);
    clearBtn->setToolTip(tr("Reset to default (no instrument definitions)"));
    connect(clearBtn, SIGNAL(clicked()), this, SLOT(clearSettings()));
    btnLayout->addWidget(clearBtn);
    
    btnLayout->addStretch();
    layout->addLayout(btnLayout, 2, 1, 1, 2);

    // Instrument selection
    layout->addWidget(new QLabel(tr("Instrument:"), this), 3, 0);
    _instrumentBox = new QComboBox(this);
    connect(_instrumentBox, SIGNAL(currentIndexChanged(int)), this, SLOT(instrumentChanged(int)));
    layout->addWidget(_instrumentBox, 3, 1, 1, 2);
    
    // Table for viewing/editing
    _tableWidget = new QTableWidget(128, 2, this);
    _tableWidget->setHorizontalHeaderLabels(QStringList() << tr("Program") << tr("Name"));
    _tableWidget->verticalHeader()->setVisible(false);
    _tableWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    _tableWidget->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    connect(_tableWidget, SIGNAL(itemChanged(QTableWidgetItem*)), this, SLOT(onTableItemChanged(QTableWidgetItem*)));
    layout->addWidget(_tableWidget, 4, 0, 1, 3);

    layout->setRowStretch(4, 1);

    // Initialize from settings or current state
    QString file = _settings->value("InstrumentDefinitions/file", "").toString();
    QString instrument = _settings->value("InstrumentDefinitions/instrument", "").toString();

    // If InstrumentDefinitions is already loaded (e.g. from previous session initialization), use that
    if (!InstrumentDefinitions::instance()->currentFile().isEmpty()) {
        file = InstrumentDefinitions::instance()->currentFile();
        instrument = InstrumentDefinitions::instance()->currentInstrument();
    }

    if (!file.isEmpty()) {
        _fileEdit->setText(file);
        loadFile();
        
        int index = _instrumentBox->findText(instrument);
        if (index >= 0) {
            _instrumentBox->setCurrentIndex(index);
        }
    } else {
        populateTable();
    }
}

void InstrumentSettingsWidget::browseFile() {
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open Instrument Definition"), 
        _fileEdit->text(), tr("Instrument Definitions (*.ins);;All Files (*)"));
        
    if (!fileName.isEmpty()) {
        _fileEdit->setText(fileName);
        loadFile();
    }
}

void InstrumentSettingsWidget::clearSettings() {
    _fileEdit->clear();
    _instrumentBox->clear();
    InstrumentDefinitions::instance()->clear();
    populateTable();
}

void InstrumentSettingsWidget::loadFile() {
    QString fileName = _fileEdit->text();
    if (fileName.isEmpty()) return;

    // Temporarily block signals to avoid triggering instrumentChanged during repopulation
    _instrumentBox->blockSignals(true);
    
    if (InstrumentDefinitions::instance()->load(fileName)) {
        _instrumentBox->clear();
        _instrumentBox->addItems(InstrumentDefinitions::instance()->instruments());
        
        // If previously selected instrument exists, select it
        QString current = InstrumentDefinitions::instance()->currentInstrument();
        int index = _instrumentBox->findText(current);
        if (index >= 0) {
            _instrumentBox->setCurrentIndex(index);
        }
        
        populateTable();
    }
    
    _instrumentBox->blockSignals(false);
}

void InstrumentSettingsWidget::instrumentChanged(int index) {
    if (index >= 0) {
        QString name = _instrumentBox->itemText(index);
        InstrumentDefinitions::instance()->selectInstrument(name);
        populateTable();
    }
}

void InstrumentSettingsWidget::populateTable() {
    _tableWidget->blockSignals(true);
    _tableWidget->clearContents();
    
    QMap<int, QString> names = InstrumentDefinitions::instance()->instrumentNames();
    
    for (int i = 0; i < 128; i++) {
        QTableWidgetItem *numItem = new QTableWidgetItem(QString::number(i));
        numItem->setFlags(numItem->flags() & ~Qt::ItemIsEditable);
        _tableWidget->setItem(i, 0, numItem);
        
        QString name = names.value(i, "");
        QTableWidgetItem *nameItem;
        
        if (name.isEmpty()) {
            name = InstrumentDefinitions::instance()->gmInstrumentName(i);
            nameItem = new QTableWidgetItem(name);
            nameItem->setForeground(QBrush(Qt::gray));
        } else {
            nameItem = new QTableWidgetItem(name);
        }
        
        _tableWidget->setItem(i, 1, nameItem);
    }
    _tableWidget->blockSignals(false);
}

void InstrumentSettingsWidget::onTableItemChanged(QTableWidgetItem *item) {
    if (item->column() == 1) {
        QTableWidgetItem *numItem = _tableWidget->item(item->row(), 0);
        if (!numItem) return; // Guard against null item (crash fix)

        int program = numItem->text().toInt();
        QString name = item->text();
        
        // Check if it was the default GM name being edited
        QString gmName = InstrumentDefinitions::instance()->gmInstrumentName(program);

        InstrumentDefinitions::instance()->setInstrumentName(program, name);
        
        _tableWidget->blockSignals(true);
        if (name.isEmpty()) {
             item->setText(gmName);
             item->setForeground(QBrush(Qt::gray));
        } else {
             item->setData(Qt::ForegroundRole, QVariant()); // Reset color
        }
        _tableWidget->blockSignals(false);
    }
}

void InstrumentSettingsWidget::refreshColors() {
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

bool InstrumentSettingsWidget::accept() {
    _settings->setValue("InstrumentDefinitions/file", _fileEdit->text());
    _settings->setValue("InstrumentDefinitions/instrument", _instrumentBox->currentText());
    InstrumentDefinitions::instance()->saveOverrides(_settings);
    return true;
}
