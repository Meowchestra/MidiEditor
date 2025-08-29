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

#include "TrackListWidget.h"
#include "ColoredWidget.h"
#include "Appearance.h"

#include "../midi/MidiFile.h"
#include "../midi/MidiTrack.h"
#include "../protocol/Protocol.h"

#include <QAction>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QGridLayout>
#include <QLabel>
#include <QMimeData>
#include <QPainter>
#include <QToolBar>
#include <QWidget>

#define ROW_HEIGHT 85

TrackListItem::TrackListItem(MidiTrack *track, TrackListWidget *parent)
    : QWidget(parent) {
    trackList = parent;
    this->track = track;

    setContentsMargins(0, 0, 0, 0);
    QGridLayout *layout = new QGridLayout(this);
    setLayout(layout);
    layout->setVerticalSpacing(1);

    colored = new ColoredWidget(*(track->color()), this);
    layout->addWidget(colored, 0, 0, 2, 1);
    QString text = tr("Track ") + QString::number(track->number());
    QLabel *text1 = new QLabel(text, this);
    text1->setFixedHeight(15);
    layout->addWidget(text1, 0, 1, 1, 1);

    trackNameLabel = new QLabel(tr("New Track"), this);
    trackNameLabel->setFixedHeight(15);
    layout->addWidget(trackNameLabel, 1, 1, 1, 1);

    QToolBar *toolBar = new QToolBar(this);
    toolBar->setIconSize(QSize(12, 12));
    QPalette palette = toolBar->palette();
    palette.setColor(QPalette::Window, Appearance::toolbarBackgroundColor());
    toolBar->setPalette(palette);
    // visibility
    visibleAction = new QAction(tr("Track visible"), toolBar);
    Appearance::setActionIcon(visibleAction, ":/run_environment/graphics/trackwidget/visible.png");
    visibleAction->setCheckable(true);
    visibleAction->setChecked(true);
    toolBar->addAction(visibleAction);
    connect(visibleAction, SIGNAL(toggled(bool)), this, SLOT(toggleVisibility(bool)));

    // audibility
    loudAction = new QAction(tr("Track audible"), toolBar);
    Appearance::setActionIcon(loudAction, ":/run_environment/graphics/trackwidget/loud.png");
    loudAction->setCheckable(true);
    loudAction->setChecked(true);
    toolBar->addAction(loudAction);
    connect(loudAction, SIGNAL(toggled(bool)), this, SLOT(toggleAudibility(bool)));

    toolBar->addSeparator();

    // name
    QAction *renameAction = new QAction(tr("Rename track"), toolBar);
    Appearance::setActionIcon(renameAction, ":/run_environment/graphics/trackwidget/rename.png");
    toolBar->addAction(renameAction);
    connect(renameAction, SIGNAL(triggered()), this, SLOT(renameTrack()));

    // remove
    QAction *removeAction = new QAction(tr("Remove track"), toolBar);
    Appearance::setActionIcon(removeAction, ":/run_environment/graphics/trackwidget/remove.png");
    toolBar->addAction(removeAction);
    connect(removeAction, SIGNAL(triggered()), this, SLOT(removeTrack()));

    layout->addWidget(toolBar, 2, 1, 1, 1);

    layout->setRowStretch(2, 1);
    setContentsMargins(5, 1, 5, 0);
    setFixedHeight(ROW_HEIGHT);
}

void TrackListItem::toggleVisibility(bool visible) {
    QString text = tr("Hide track");
    if (visible) {
        text = tr("Show track");
    }
    trackList->midiFile()->protocol()->startNewAction(text);
    track->setHidden(!visible);
    trackList->midiFile()->protocol()->endAction();
}

void TrackListItem::toggleAudibility(bool audible) {
    QString text = tr("Mute track");
    if (audible) {
        text = tr("Track audible");
    }
    trackList->midiFile()->protocol()->startNewAction(text);
    track->setMuted(!audible);
    trackList->midiFile()->protocol()->endAction();
}

void TrackListItem::renameTrack() {
    emit trackRenameClicked(track->number());
}

void TrackListItem::removeTrack() {
    emit trackRemoveClicked(track->number());
}

void TrackListItem::onBeforeUpdate() {
    trackNameLabel->setText(track->name());

    if (visibleAction->isChecked() == track->hidden()) {
        disconnect(visibleAction, SIGNAL(toggled(bool)), this, SLOT(toggleVisibility(bool)));
        visibleAction->setChecked(!track->hidden());
        connect(visibleAction, SIGNAL(toggled(bool)), this, SLOT(toggleVisibility(bool)));
    }

    if (loudAction->isChecked() == track->muted()) {
        disconnect(loudAction, SIGNAL(toggled(bool)), this, SLOT(toggleAudibility(bool)));
        loudAction->setChecked(!track->muted());
        connect(loudAction, SIGNAL(toggled(bool)), this, SLOT(toggleAudibility(bool)));
    }
    colored->setColor(*(track->color()));
}

TrackListWidget::TrackListWidget(QWidget *parent)
    : QListWidget(parent) {
    setSelectionMode(QAbstractItemView::SingleSelection);
    setStyleSheet("QListWidget::item { border-bottom: 1px solid lightGray; }");
    file = 0;
    connect(this, SIGNAL(itemClicked(QListWidgetItem*)), this, SLOT(chooseTrack(QListWidgetItem*)));
    
    // Enable drag-and-drop for track reordering
    setDragDropMode(QAbstractItemView::InternalMove);
    setDefaultDropAction(Qt::MoveAction);
}

void TrackListWidget::setFile(MidiFile *f) {
    file = f;
    connect(file->protocol(), SIGNAL(actionFinished()), this, SLOT(update()));
    update();
}

void TrackListWidget::chooseTrack(QListWidgetItem *item) {
    // Use row index to get track from trackorder, not Qt::UserRole
    int row = this->row(item);
    if (row >= 0 && row < trackorder.size()) {
        MidiTrack *track = trackorder.at(row);
        emit trackClicked(track);
    }
}

void TrackListWidget::update() {
    if (!file) {
        clear();
        items.clear();
        trackorder.clear();
        QListWidget::update();
        return;
    }

    bool rebuild = false;
    QList<MidiTrack *> oldTracks = trackorder;
    QList<MidiTrack *> realTracks = *file->tracks();

    if (oldTracks.size() != realTracks.size()) {
        rebuild = true;
    } else {
        for (int i = 0; i < oldTracks.size(); i++) {
            if (oldTracks.at(i) != realTracks.at(i)) {
                rebuild = true;
                break;
            }
        }
    }

    if (rebuild) {
        clear();
        items.clear();
        trackorder.clear();

        foreach(MidiTrack* track, realTracks) {
            TrackListItem *widget = new TrackListItem(track, this);
            QListWidgetItem *item = new QListWidgetItem();
            item->setSizeHint(QSize(0, ROW_HEIGHT));
            item->setData(Qt::UserRole, track->number());
            addItem(item);
            setItemWidget(item, widget);
            items.insert(track, widget);
            trackorder.append(track);
            connect(widget, SIGNAL(trackRenameClicked(int)), this, SIGNAL(trackRenameClicked(int)));
            connect(widget, SIGNAL(trackRemoveClicked(int)), this, SIGNAL(trackRemoveClicked(int)));
        }
    }

    foreach(TrackListItem* item, items.values()) {
        item->onBeforeUpdate();
    }

    QListWidget::update();
}

MidiFile *TrackListWidget::midiFile() {
    return file;
}

void TrackListWidget::dropEvent(QDropEvent *event) {
    if (event->source() == this && (event->dropAction() == Qt::MoveAction ||
                                    dragDropMode() == QAbstractItemView::InternalMove)) {
        
        // Get selected items
        QList<QListWidgetItem*> selected = selectedItems();
        if (selected.isEmpty()) {
            event->ignore();
            return;
        }
        
        // Get source position BEFORE Qt processes the drop
        int from = row(selected.first());
        
        // Get the item at the drop position
        QListWidgetItem *dropItem = itemAt(event->pos());
        if (!dropItem) {
            event->ignore();
            return;
        }
        
        int to = row(dropItem);
        
        if (from == to || from < 0 || to < 0) {
            event->ignore();
            return;
        }
        
        // IMPORTANT: Block Qt's default move behavior
        event->setDropAction(Qt::IgnoreAction);
        
        // Perform our custom reordering
        reorderTracks(from, to);
        
        // Update the selection to the new position
        setCurrentRow(to);
        
        // Accept but with IgnoreAction to prevent Qt from moving items
        event->accept();
    } else {
        QListWidget::dropEvent(event);
    }
}

bool TrackListWidget::dropMimeData(int index, const QMimeData *data, Qt::DropAction action) {
    Q_UNUSED(index)
    Q_UNUSED(data)
    Q_UNUSED(action)
    // We handle drops in dropEvent, so just return true to indicate we can handle it
    return true;
}

void TrackListWidget::reorderTracks(int fromIndex, int toIndex) {
    if (!file || fromIndex == toIndex || fromIndex < 0 || toIndex < 0 ||
        fromIndex >= trackorder.size() || toIndex >= trackorder.size()) {
        return;
    }

    // Start protocol action for undo/redo support
    file->protocol()->startNewAction(tr("Reorder tracks"));

    // Get the track being moved
    MidiTrack *track = trackorder[fromIndex];
    
    // Remove from current position
    trackorder.removeAt(fromIndex);
    
    // Insert at new position
    trackorder.insert(toIndex, track);

    // Update track numbers to match their new positions
    for (int i = 0; i < trackorder.size(); i++) {
        trackorder[i]->setNumber(i);
    }

    // Update the MidiFile's track list to match our new order
    QList<MidiTrack *> *fileTracks = file->tracks();
    fileTracks->clear();
    *fileTracks = trackorder;

    // End protocol action
    file->protocol()->endAction();

    // Force rebuild by clearing trackorder so update() detects a change
    trackorder.clear();
    update();
    
    // Emit signal to notify other components
    emit trackOrderChanged();
}
