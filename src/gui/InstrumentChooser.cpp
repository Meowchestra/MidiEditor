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

#include "InstrumentChooser.h"

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QCompleter>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QStyleOptionViewItem>
#include <QStyle>

#include "../MidiEvent/MidiEvent.h"
#include "../MidiEvent/ProgChangeEvent.h"
#include "../midi/MidiChannel.h"
#include "../midi/MidiFile.h"
#include "../protocol/Protocol.h"
#include "../support/FFXIVChannelFixer.h"
#include "../tool/NewNoteTool.h"
#include <algorithm>
#include <QSettings>
#include <QStandardItem>
#include "Appearance.h"

class InstrumentChooserDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);

        // Clear the text before letting the native style draw it, so that we
        // don't get double overlapping text in themes like Windows 11 dark mode
        opt.text = "";

        // Draw background
        opt.widget->style()->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);

        QString text = index.data(Qt::DisplayRole).toString();
        QString gmText = index.data(Qt::UserRole).toString();

        QRect textRect = opt.rect.adjusted(3, 0, -35, 0);
        QRect gmRect = opt.rect.adjusted(0, 0, -5, 0);

        painter->save();
        painter->setFont(opt.font);
        painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, text);
        painter->setPen(opt.palette.color(QPalette::Text).lighter());
        painter->drawText(gmRect, Qt::AlignRight | Qt::AlignVCenter, gmText);
        painter->restore();
    }
};

InstrumentChooser::InstrumentChooser(MidiFile *file, int channel, QSettings *settings, QWidget *parent)
    : QDialog(parent) {
    _file = file;
    _channel = channel;
    _settings = settings;
    
    // Fallback if settings pointer is null
    if (!_settings) {
        _settings = Appearance::settings(this);
    }

    setWindowTitle(tr("Channel Instrument"));
    QLabel *starttext = new QLabel(tr("Choose Instrument for Channel ") + QString::number(channel), this);

    QLabel *text = new QLabel(tr("Instrument: "), this);
    _box = new QComboBox(this);
    _box->setEditable(false);
    _box->setItemDelegate(new InstrumentChooserDelegate(_box));
    
    bool condense = _settings->value("game_support/ffxiv_condense_instruments", false).toBool();
    int currentProgram = _file->channel(_channel)->progAtTick(0);

    if (condense) {
        QStringList ffxivNames = FFXIVChannelFixer::allInstrumentNames();
        QSet<int> ffxivProgs;
        for (const QString &name : ffxivNames) {
            int p = FFXIVChannelFixer::programForInstrument(name);
            if (p >= 0) ffxivProgs.insert(p);
        }

        int sortMethod = _settings->value("game_support/ffxiv_instrument_sort_method", 0).toInt();
        if (sortMethod == 1) {
            // FFXIV Group Sort - with group headers
            QList<FFXIVChannelFixer::InstrumentGroup> groups = FFXIVChannelFixer::ffxivInstrumentGroups();
            for (const auto &group : groups) {
                // Add group header
                _box->addItem("--- " + group.name + " ---");
                int headerIndex = _box->count() - 1;
                _box->setItemData(headerIndex, -1, Qt::UserRole + 1); // Mark as header
                
                // Make header look bold
                QStandardItemModel *model = qobject_cast<QStandardItemModel*>(_box->model());
                if (model) {
                    QStandardItem *item = model->item(headerIndex);
                    if (item) {
                        item->setEnabled(false);
                        QFont font = item->font();
                        font.setBold(true);
                        item->setFont(font);
                    }
                }

                for (int p : group.programs) {
                    _box->addItem(MidiFile::instrumentName(p), QString("[%1]").arg(p, 3, 10, QChar('0')));
                    _box->setItemData(_box->count() - 1, p, Qt::UserRole + 1);
                    if (p == currentProgram) {
                        _box->setCurrentIndex(_box->count() - 1);
                    }
                }
            }
        } else {
            // Numeric Sort
            QList<int> sortedProgs = ffxivProgs.values();
            std::sort(sortedProgs.begin(), sortedProgs.end());
            for (int p : sortedProgs) {
                _box->addItem(MidiFile::instrumentName(p), QString("[%1]").arg(p, 3, 10, QChar('0')));
                _box->setItemData(_box->count() - 1, p, Qt::UserRole + 1);
                if (p == currentProgram) {
                    _box->setCurrentIndex(_box->count() - 1);
                }
            }
        }
    } else {
        for (int i = 0; i < 128; i++) {
            _box->addItem(MidiFile::instrumentName(i), QString("[%1]").arg(i, 3, 10, QChar('0')));
            _box->setItemData(i, i, Qt::UserRole + 1);
        }
        _box->setCurrentIndex(currentProgram);
    }

    QLabel *endText = new QLabel(tr("<b>Note:</b> This places a Program Change event at <b>tick 0</b>"
        "<br>on the <b>currently selected track</b>."
        "<br><br><b>Warning:</b> If there is another Program Change Event after"
        "<br>this tick, the instrument selected there will be audible instead!"
        "<br>If you want all other Program Change Events to be"
        "<br>removed, select the box below."));

    _removeOthers = new QCheckBox(tr("Remove Other Program Change Events"), this);

    QPushButton *breakButton = new QPushButton(tr("Cancel"));
    connect(breakButton, SIGNAL(clicked()), this, SLOT(hide()));
    QPushButton *acceptButton = new QPushButton(tr("Accept"));
    connect(acceptButton, SIGNAL(clicked()), this, SLOT(accept()));

    QGridLayout *layout = new QGridLayout(this);
    layout->addWidget(starttext, 0, 0, 1, 3);
    layout->addWidget(text, 1, 0, 1, 1);
    layout->addWidget(_box, 1, 1, 1, 2);
    layout->addWidget(endText, 2, 1, 1, 2);
    layout->addWidget(_removeOthers, 3, 1, 1, 2);
    layout->addWidget(breakButton, 4, 0, 1, 1);
    layout->addWidget(acceptButton, 4, 2, 1, 1);
    layout->setColumnStretch(1, 1);
}

void InstrumentChooser::accept() {
    int program = _box->itemData(_box->currentIndex(), Qt::UserRole + 1).toInt();
    bool removeOthers = _removeOthers->isChecked();
    MidiTrack *track = 0;

    // get events
    QList<ProgChangeEvent *> events;
    foreach(MidiEvent* event, _file->channel(_channel)->eventMap()->values()) {
        ProgChangeEvent *prg = dynamic_cast<ProgChangeEvent *>(event);
        if (prg) {
            events.append(prg);
            track = prg->track();
        }
    }
    if (!track) {
        int currentTrackIdx = NewNoteTool::editTrack();
        track = _file->track(currentTrackIdx);
        if (!track) {
            track = _file->track(0); // Safe fallback
        }
    }

    ProgChangeEvent *event = 0;

    _file->protocol()->startNewAction(tr("Channel Instrument Changed"));
    if (events.size() > 0 && events.first()->midiTime() == 0) {
        event = events.first();
        event->setProgram(program);
    } else {
        event = new ProgChangeEvent(_channel, program, track);
        _file->channel(_channel)->insertEvent(event, 0);
    }

    if (removeOthers) {
        foreach(ProgChangeEvent* toRemove, events) {
            if (toRemove != event) {
                _file->channel(_channel)->removeEvent(toRemove);
            }
        }
    }
    _file->protocol()->endAction();
    hide();
}
