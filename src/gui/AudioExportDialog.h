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

#ifndef AUDIOEXPORTDIALOG_H
#define AUDIOEXPORTDIALOG_H

#ifdef FLUIDSYNTH_SUPPORT

#include <QDialog>

class MidiFile;
class QComboBox;
class QRadioButton;
class QSpinBox;
class QCheckBox;
class QLabel;
class QPushButton;
class QProgressBar;
class QGroupBox;

struct AudioExportSettings;

/**
 * \class AudioExportDialog
 *
 * \brief Self-contained dialog for configuring and executing audio export.
 *
 * Provides format, quality, range, and reverb tail options.
 * Handles the full export lifecycle: configuration → progress → completion.
 */
class AudioExportDialog : public QDialog {
    Q_OBJECT

public:
    explicit AudioExportDialog(MidiFile *file, QWidget *parent = nullptr);

private slots:
    void onExportClicked();
    void onExportProgress(int percent);
    void onExportFinished(bool success, const QString &path);
    void onExportCancelled();
    void onCancelExport();
    void updateEstimatedSize();
    void onFormatChanged(int index);
    void onAudioConfigChanged(int index);
    void onRangeChanged();
    void centerOnParent();

    // Completion dialog actions
    void openExportedFile();
    void openExportedFolder();

private:
    void buildConfigPage();
    void buildCompletionPage();

    void showConfigPage();
    void showCompletionPage(bool success);

    int lastActualEventTick() const;

    AudioExportSettings currentSettings() const;
    double estimateDurationSeconds() const;
    double estimateFileSize() const;
    QString formatFileSize(double bytes) const;
    QString formatDuration(double seconds) const;

    MidiFile *_file;
    QString _exportedPath;

    // Config page widgets
    QWidget *_configPage;
    QLabel *_sourceLabel;
    QLabel *_durationLabel;
    QRadioButton *_rangeFullSong;
    QRadioButton *_rangeSelection;
    QRadioButton *_rangeCustom;
    QSpinBox *_rangeFrom;
    QSpinBox *_rangeTo;
    QComboBox *_formatCombo;
    QComboBox *_rateCombo;
    QComboBox *_bitsCombo;
    QCheckBox *_reverbTailCheck;
    QCheckBox *_trimSilenceCheck;
    QLabel *_soundFontListLabel;
    QLabel *_estimatedSizeLabel;
    QPushButton *_exportBtn;
    QPushButton *_cancelConfigBtn;

    // Completion page widgets
    QWidget *_completionPage;
    QLabel *_completionIcon;
    QLabel *_completionFileLabel;
    QLabel *_completionDetailsLabel;
    QPushButton *_openFileBtn;
    QPushButton *_openFolderBtn;
    QPushButton *_closeBtn;
};

#endif // FLUIDSYNTH_SUPPORT
#endif // AUDIOEXPORTDIALOG_H
