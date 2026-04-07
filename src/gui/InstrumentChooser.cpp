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

InstrumentChooser::InstrumentChooser(MidiFile *f, int channel, QWidget *parent)
    : QDialog(parent) {
    _file = f;
    _channel = channel;

    setWindowTitle(tr("Channel Instrument"));
    QLabel *starttext = new QLabel(tr("Choose Instrument for Channel ") + QString::number(channel), this);

    QLabel *text = new QLabel(tr("Instrument: "), this);
    _box = new QComboBox(this);
    _box->setEditable(false);
    _box->setItemDelegate(new InstrumentChooserDelegate(_box));
    for (int i = 0; i < 128; i++) {
        _box->addItem(MidiFile::instrumentName(i), QString("[%1]").arg(i, 3, 10, QChar('0')));
    }
    _box->setCurrentIndex(_file->channel(_channel)->progAtTick(0));

    QLabel *endText = new QLabel(tr("<b>Warning:</b> this will edit the event at tick 0 of the file."
        "<br>If there is a Program Change Event after this tick,"
        "<br>the instrument selected there will be audible!"
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
    int program = _box->currentIndex();
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
        track = _file->track(0);
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
