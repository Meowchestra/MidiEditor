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

#ifndef TRACKRENAMEDIALOG_H_
#define TRACKRENAMEDIALOG_H_

#include <QDialog>

class QLineEdit;
class QToolButton;
class QSettings;

/**
 * \class TrackRenameDialog
 *
 * \brief A dialog for renaming tracks, with optional FFXIV instrument presets.
 */
class TrackRenameDialog : public QDialog {
    Q_OBJECT

public:
    TrackRenameDialog(const QString &currentName, int trackNumber, QSettings *settings, QWidget *parent = nullptr);

    QString name() const;

private slots:
    void showPresets();
    void setPresetName();

private:
    QLineEdit *_nameEdit;
    QToolButton *_presetsButton;
    QSettings *_settings;
    int _trackNumber;
};

#endif // TRACKRENAMEDIALOG_H_
