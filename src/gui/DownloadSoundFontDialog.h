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

#ifndef DOWNLOADSOUNDFONTDIALOG_H_
#define DOWNLOADSOUNDFONTDIALOG_H_

#ifdef FLUIDSYNTH_SUPPORT

#include <QDialog>
#include <QString>
#include <QList>

class QTableWidget;
class QPushButton;
class QNetworkAccessManager;
class QNetworkReply;
class QProgressDialog;
class QFile;

struct SoundFontDownloadItem {
    QString name;
    QString size;
    QString format;
    QString url;
    QString filename;
    bool isLegacy;
};

/**
 * \class DownloadSoundFontDialog
 *
 * \brief UI for discovering and downloading free high-quality SoundFonts
 * into the application's data directory.
 */
class DownloadSoundFontDialog : public QDialog {
    Q_OBJECT

public:
    explicit DownloadSoundFontDialog(QWidget *parent = nullptr);
    ~DownloadSoundFontDialog();

signals:
    /**
     * \brief Emitted when a SoundFont is successfully downloaded
     * \param path The absolute path to the downloaded SoundFont
     */
    void soundFontDownloaded(const QString &path);

private slots:
    void onDownloadButtonClicked();
    void soundFontDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void soundFontDownloadFinished();
    void openMusicalArtifacts();

private:
    void setupUI();
    void populateTable();
    QString getSoundFontsDirectory() const;
    QString extractZipAndFindSoundFont(const QString &zipPath) const;

    QTableWidget *_table;
    QCheckBox *_showLegacyCheckBox;
    QPushButton *_downloadBtn;
    QPushButton *_findMoreBtn;
    QPushButton *_closeBtn;

    QList<SoundFontDownloadItem> _items;

    // Networking
    QNetworkAccessManager *_networkManager;
    QNetworkReply *_downloadReply;
    QProgressDialog *_progressDialog;
    QFile *_downloadFile;
    QString _currentDownloadName;
};

#endif // FLUIDSYNTH_SUPPORT

#endif // DOWNLOADSOUNDFONTDIALOG_H_
