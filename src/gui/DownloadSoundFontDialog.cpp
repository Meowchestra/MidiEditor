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

#ifdef FLUIDSYNTH_SUPPORT

#include "DownloadSoundFontDialog.h"

#include <QTabBar>
#include <QTableWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QMessageBox>
#include <QCoreApplication>
#include <QDir>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProgressDialog>
#include <QFile>
#include <QUrl>
#include <QFileInfo>
#include <QDesktopServices>
#include <QProcess>
#include <QDirIterator>
#include <QDateTime>

DownloadSoundFontDialog::DownloadSoundFontDialog(QWidget *parent)
    : QDialog(parent),
      _networkManager(new QNetworkAccessManager(this)),
      _downloadReply(nullptr),
      _progressDialog(nullptr),
      _downloadFile(nullptr)
{
    // Define available high-quality SoundFonts with mirrored URLs and pretty filenames
    _items = {
        // --- General Section ---
        {
            tr("GeneralUser GS"),
            tr("30.8 MB"),
            tr("SF2"),
            QStringLiteral("https://github.com/mrbumpy409/GeneralUser-GS/raw/refs/heads/main/GeneralUser-GS.sf2"),
            QStringLiteral("GeneralUser GS.sf2"),
            SoundFontCategory::General
        },
        {
            tr("MS Basic 2 (MuseScore 4)"),
            tr("205 MB"),
            tr("SF2"),
            QStringLiteral("https://github.com/Meowchestra/SoundFonts/releases/download/v2.0.0/MS_Basic-v2.0.0.sf2"),
            QStringLiteral("MS Basic 2.sf2"),
            SoundFontCategory::General
        },
        {
            tr("MS Basic 2 (Compressed)"),
            tr("38 MB"),
            tr("SF3"),
            QStringLiteral("https://github.com/Meowchestra/SoundFonts/releases/download/v2.0.0/MS_Basic-v2.0.0.sf3"),
            QStringLiteral("MS Basic 2.sf3"),
            SoundFontCategory::General
        },
        {
            tr("Arachno SoundFont"),
            tr("148 MB"),
            tr("SF2"),
            QStringLiteral("https://github.com/Meowchestra/SoundFonts/releases/download/Other/Arachno.SoundFont.-.Version.1.0.sf2"),
            QStringLiteral("Arachno SoundFont.sf2"),
            SoundFontCategory::General
        },
        {
            tr("SGM Pro"),
            tr("124 MB"),
            tr("SF2"),
            QStringLiteral("https://github.com/Meowchestra/SoundFonts/releases/download/Other/Shan.SGM-Pro.11.sf2"),
            QStringLiteral("SGM Pro.sf2"),
            SoundFontCategory::General
        },
        {
            tr("SGM"),
            tr("309 MB"),
            tr("SF2"),
            QStringLiteral("https://musical-artifacts.com/artifacts/855/SGM-v2.01-NicePianosGuitarsBass-V1.2.sf2"),
            QStringLiteral("SGM.sf2"),
            SoundFontCategory::General
        },
        {
            tr("FluidR3 GM+GS"),
            tr("144 MB"),
            tr("SF2"),
            QStringLiteral("https://musical-artifacts.com/artifacts/1229/FluidR3_GM_GS.sf2"),
            QStringLiteral("FluidR3.sf2"),
            SoundFontCategory::General
        },
        {
            tr("Microsoft GS Wavetable Synth"),
            tr("3.07 MB"),
            tr("SF2"),
            QStringLiteral("https://musical-artifacts.com/artifacts/5190/microsoft_gm_3.sf2"),
            QStringLiteral("Microsoft GS Wavetable Synth.sf2"),
            SoundFontCategory::General
        },
        {
            tr("B. Meowsic"),
            tr("18.9 MB"),
            tr("SF2"),
            QStringLiteral("https://musical-artifacts.com/artifacts/2617/Meowsic_Cat_Soundfont.sf2"),
            QStringLiteral("Meowsic.sf2"),
            SoundFontCategory::General
        },

        // --- Games Section ---
        {
            tr("Final Fantasy XIV (Standard)"),
            tr("14.2 MB"),
            tr("SF2"),
            QStringLiteral("https://github.com/Meowchestra/FFXIV-SoundFont/releases/download/v7.3.0/FFXIV-Standard.sf2"),
            QStringLiteral("FFXIV (Standard).sf2"),
            SoundFontCategory::Games
        },
        {
            tr("Final Fantasy XIV (Standard/Compressed)"),
            tr("3 MB"),
            tr("SF3"),
            QStringLiteral("https://github.com/Meowchestra/FFXIV-SoundFont/releases/download/v7.3.0/FFXIV-Standard.sf3"),
            QStringLiteral("FFXIV (Standard).sf3"),
            SoundFontCategory::Games
        },
        {
            tr("Final Fantasy XIV (Expanded)"),
            tr("14.2 MB"),
            tr("SF2"),
            QStringLiteral("https://github.com/Meowchestra/FFXIV-SoundFont/releases/download/v7.3.0/FFXIV-Expanded.sf2"),
            QStringLiteral("FFXIV (Expanded).sf2"),
            SoundFontCategory::Games
        },
        {
            tr("Final Fantasy XIV (Expanded/Compressed)"),
            tr("3 MB"),
            tr("SF3"),
            QStringLiteral("https://github.com/Meowchestra/FFXIV-SoundFont/releases/download/v7.3.0/FFXIV-Expanded.sf3"),
            QStringLiteral("FFXIV (Expanded).sf3"),
            SoundFontCategory::Games
        },
        {
            tr("Mabinogi (3MLE)"),
            tr("317 MB"),
            tr("DLS"),
            QStringLiteral("https://github.com/Meowchestra/SoundFonts/releases/download/Other/musicalnexus3-level2.dls"),
            QStringLiteral("Mabinogi (3MLE).dls"),
            SoundFontCategory::Games
        },
        {
            tr("Mabinogi (MabiMML)"),
            tr("36.5 MB"),
            tr("SF3"),
            QStringLiteral("https://github.com/Meowchestra/SoundFonts/releases/download/Other/mabi_instruments_high_quality.sf3"),
            QStringLiteral("Mabinogi (MabiMML).sf3"),
            SoundFontCategory::Games
        },
        {
            tr("MapleStory 2 (3MLE)"),
            tr("3.12 MB"),
            tr("DLS"),
            QStringLiteral("https://github.com/Meowchestra/SoundFonts/releases/download/Other/maplebeats-2.dls"),
            QStringLiteral("MapleStory 2 (3MLE).dls"),
            SoundFontCategory::Games
        },
        {
            tr("Old School RuneScape (OSRS)"),
            tr("30.8 MB"),
            tr("SF2"),
            QStringLiteral("https://musical-artifacts.com/artifacts/6871/Old_School_RuneScape.sf2"),
            QStringLiteral("Old School RuneScape.sf2"),
            SoundFontCategory::Games
        },

        // --- Legacy Section ---
        {
            tr("MS Basic (MuseScore 3)"),
            tr("466 MB"),
            tr("SF2"),
            QStringLiteral("https://musical-artifacts.com/artifacts/3001/MS_Basic.sf2"),
            QStringLiteral("MS Basic.sf2"),
            SoundFontCategory::Legacy
        },
        {
            tr("MS Basic (Compressed)"),
            tr("48.9 MB"),
            tr("SF3"),
            QStringLiteral("https://github.com/musescore/MuseScore/raw/refs/heads/master/share/sound/MS%20Basic.sf3"),
            QStringLiteral("MS Basic.sf3"),
            SoundFontCategory::Legacy
        },
        {
            tr("MuseScore General (MuseScore 3)"),
            tr("208 MB"),
            tr("SF2"),
            QStringLiteral("https://ftp.osuosl.org/pub/musescore/soundfont/MuseScore_General/MuseScore_General.sf2"),
            QStringLiteral("MuseScore General.sf2"),
            SoundFontCategory::Legacy
        },
        {
            tr("MuseScore General (Compressed)"),
            tr("35.9 MB"),
            tr("SF3"),
            QStringLiteral("https://ftp.osuosl.org/pub/musescore/soundfont/MuseScore_General/MuseScore_General.sf3"),
            QStringLiteral("MuseScore General.sf3"),
            SoundFontCategory::Legacy
        },
        {
            tr("FluidR3Mono GM (MuseScore 2)"),
            tr("22.6 MB"),
            tr("SF3"),
            QStringLiteral("https://github.com/musescore/MuseScore/raw/refs/heads/master/share/sound/FluidR3Mono_GM.sf3"),
            QStringLiteral("FluidR3Mono GM.sf3"),
            SoundFontCategory::Legacy
        },
        {
            tr("TimGM6mb (MuseScore 1)"),
            tr("5.7 MB"),
            tr("SF2"),
            QStringLiteral("https://sourceforge.net/p/mscore/code/HEAD/tree/trunk/mscore/share/sound/TimGM6mb.sf2?format=raw"),
            QStringLiteral("TimGM6mb.sf2"),
            SoundFontCategory::Legacy
        }
    };

    setupUI();
    populateTable();
}

DownloadSoundFontDialog::~DownloadSoundFontDialog() {
    if (_downloadReply) {
        _downloadReply->abort();
        _downloadReply->deleteLater();
    }
    if (_downloadFile) {
        _downloadFile->close();
        delete _downloadFile;
    }
}

void DownloadSoundFontDialog::setupUI() {
    setWindowTitle(tr("Download SoundFonts"));
    setMinimumSize(500, 300);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);

    mainLayout->addWidget(infoLabel);

    // Tab bar for categories
    _categoryTabBar = new QTabBar(this);
    _categoryTabBar->addTab(tr("General"));
    _categoryTabBar->addTab(tr("Games"));
    _categoryTabBar->addTab(tr("Legacy"));
    _categoryTabBar->setExpanding(true);
    _categoryTabBar->setDrawBase(true);
    _categoryTabBar->setShape(QTabBar::RoundedNorth);
    mainLayout->addWidget(_categoryTabBar);

    connect(_categoryTabBar, &QTabBar::currentChanged, this, &DownloadSoundFontDialog::populateTable);

    _table = new QTableWidget(this);
    _table->setColumnCount(3);
    _table->setHorizontalHeaderLabels({tr("Name"), tr("Size"), tr("Format")});
    _table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    _table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    _table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    _table->setSelectionBehavior(QAbstractItemView::SelectRows);
    _table->setSelectionMode(QAbstractItemView::SingleSelection);
    _table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    mainLayout->addWidget(_table);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    
    _findMoreBtn = new QPushButton(tr("Find More Online..."), this);
    btnLayout->addWidget(_findMoreBtn);
    
    btnLayout->addStretch();
    
    _downloadBtn = new QPushButton(tr("Download Selected"), this);
    _closeBtn = new QPushButton(tr("Close"), this);
    
    btnLayout->addWidget(_downloadBtn);
    btnLayout->addWidget(_closeBtn);
    mainLayout->addLayout(btnLayout);

    connect(_downloadBtn, SIGNAL(clicked()), this, SLOT(onDownloadButtonClicked()));
    connect(_findMoreBtn, SIGNAL(clicked()), this, SLOT(openMusicalArtifacts()));
    connect(_closeBtn, SIGNAL(clicked()), this, SLOT(close()));
    connect(_table, &QTableWidget::itemSelectionChanged, this, [this]() {
        _downloadBtn->setEnabled(!_table->selectedItems().isEmpty());
    });
    
    _downloadBtn->setEnabled(false);
}

void DownloadSoundFontDialog::populateTable() {
    _table->clearContents();
    
    // Determine target category based on tab index
    SoundFontCategory targetCategory = SoundFontCategory::General;
    int tabIndex = _categoryTabBar->currentIndex();
    if (tabIndex == 1) targetCategory = SoundFontCategory::Games;
    else if (tabIndex == 2) targetCategory = SoundFontCategory::Legacy;

    // Filter items
    QList<int> visibleIndices;
    for (int i = 0; i < _items.size(); ++i) {
        if (_items[i].category == targetCategory) {
            visibleIndices.append(i);
        }
    }

    _table->setRowCount(visibleIndices.size());
    QString currentDir = getSoundFontsDirectory();

    for (int i = 0; i < visibleIndices.size(); ++i) {
        int originalIndex = visibleIndices[i];
        const auto &item = _items[originalIndex];
        
        QTableWidgetItem *nameItem = new QTableWidgetItem(item.name);
        QTableWidgetItem *sizeItem = new QTableWidgetItem(item.size);
        QTableWidgetItem *formatItem = new QTableWidgetItem(item.format);
        
        // Check if already downloaded
        QString destPath = QDir(currentDir).filePath(item.filename);
        if (QFile::exists(destPath)) {
            nameItem->setText(item.name + tr(" (Downloaded)"));
            nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEnabled);
            sizeItem->setFlags(sizeItem->flags() & ~Qt::ItemIsEnabled);
            formatItem->setFlags(formatItem->flags() & ~Qt::ItemIsEnabled);
        }
        
        _table->setItem(i, 0, nameItem);
        _table->setItem(i, 1, sizeItem);
        _table->setItem(i, 2, formatItem);
        
        // Store the original index in the name item's user data for retrieval in download slot
        nameItem->setData(Qt::UserRole, originalIndex);
    }
}

QString DownloadSoundFontDialog::getSoundFontsDirectory() const {
    QString appDir = QCoreApplication::applicationDirPath();
    QDir dir(appDir);
    if (!dir.exists("soundfonts")) {
        dir.mkpath("soundfonts");
    }
    dir.cd("soundfonts");
    return dir.absolutePath();
}

void DownloadSoundFontDialog::onDownloadButtonClicked() {
    int row = _table->currentRow();
    if (row < 0) return;
    
    // Retrieve original index from user data
    QTableWidgetItem *nameItem = _table->item(row, 0);
    if (!nameItem) return;
    int originalIndex = nameItem->data(Qt::UserRole).toInt();
    if (originalIndex < 0 || originalIndex >= _items.size()) return;

    const auto &item = _items[originalIndex];
    QUrl url(item.url);
    _currentDownloadName = item.filename;
    
    // If the URL is a ZIP but we want an SF2/SF3, download as ZIP first
    if (url.path().endsWith(".zip", Qt::CaseInsensitive) && !item.filename.endsWith(".zip", Qt::CaseInsensitive)) {
        _currentDownloadName += ".zip";
    }
    
    QString destPath = QDir(getSoundFontsDirectory()).filePath(_currentDownloadName);
    QString installedPath = QDir(getSoundFontsDirectory()).filePath(item.filename);
    
    if (QFile::exists(installedPath)) {
        QMessageBox::information(this, tr("Already Downloaded"), tr("This SoundFont is already downloaded."));
        emit soundFontDownloaded(installedPath);
        return;
    }

    _downloadFile = new QFile(destPath);
    if (!_downloadFile->open(QIODevice::WriteOnly)) {
        QMessageBox::critical(this, tr("Error"), tr("Could not create file for writing:\n%1").arg(destPath));
        delete _downloadFile;
        _downloadFile = nullptr;
        return;
    }

    QNetworkRequest request(url);
    // Follow redirects
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    _downloadReply = _networkManager->get(request);
    connect(_downloadReply, SIGNAL(downloadProgress(qint64,qint64)), this, SLOT(soundFontDownloadProgress(qint64,qint64)));
    connect(_downloadReply, SIGNAL(finished()), this, SLOT(soundFontDownloadFinished()));
    connect(_downloadReply, &QNetworkReply::readyRead, this, [this]() {
        if (_downloadFile && _downloadReply) {
            _downloadFile->write(_downloadReply->readAll());
        }
    });

    _progressDialog = new QProgressDialog(tr("Downloading %1...").arg(item.name), tr("Cancel"), 0, 100, this);
    _progressDialog->setWindowTitle(tr("Download"));
    _progressDialog->setWindowModality(Qt::WindowModal);
    _progressDialog->setMinimumDuration(0);
    _progressDialog->setValue(0);

    connect(_progressDialog, SIGNAL(canceled()), _downloadReply, SLOT(abort()));
}

void DownloadSoundFontDialog::soundFontDownloadProgress(qint64 bytesReceived, qint64 bytesTotal) {
    if (_progressDialog && bytesTotal > 0) {
        _progressDialog->setMaximum(100);
        _progressDialog->setValue((bytesReceived * 100) / bytesTotal);
    }
}

void DownloadSoundFontDialog::soundFontDownloadFinished() {
    if (_progressDialog) {
        _progressDialog->deleteLater();
        _progressDialog = nullptr;
    }

    if (!_downloadReply || !_downloadFile) {
        return;
    }

    _downloadFile->close();
    QString destPath = _downloadFile->fileName();

    if (_downloadReply->error() != QNetworkReply::NoError) {
        if (_downloadReply->error() != QNetworkReply::OperationCanceledError) {
            QMessageBox::critical(this, tr("Download Failed"), tr("Failed to download SoundFont: %1").arg(_downloadReply->errorString()));
        }
        _downloadFile->remove(); // delete partial file
    } else {
        // Successfully downloaded
        QString finalPath = destPath;
        
        // Auto-extract ZIP files using system tar (available on Win10+, Mac, Linux)
        if (destPath.endsWith(".zip", Qt::CaseInsensitive)) {
            int row = _table->currentRow();
            QString targetFilename;
            if (row >= 0) {
                QTableWidgetItem *nameItem = _table->item(row, 0);
                if (nameItem) {
                    int originalIndex = nameItem->data(Qt::UserRole).toInt();
                    if (originalIndex >= 0 && originalIndex < _items.size()) {
                        targetFilename = _items[originalIndex].filename;
                    }
                }
            }
            
            QString extractedFile = extractZipAndFindSoundFont(destPath, targetFilename);
            if (!extractedFile.isEmpty()) {
                finalPath = extractedFile;
                QMessageBox::information(this, tr("Extraction Complete"), tr("ZIP file extracted successfully. Proceeding to add SoundFont."));
            } else {
                QMessageBox::warning(this, tr("Extraction Failed"), tr("Downloaded the ZIP, but could not extract the SoundFont file automatically. You may need to unzip it manually."));
            }
        } else {
            // For non-ZIP files, ensure they are named correctly as per "pretty" filename
            int row = _table->currentRow();
            if (row >= 0) {
                QTableWidgetItem *nameItem = _table->item(row, 0);
                if (nameItem) {
                    int originalIndex = nameItem->data(Qt::UserRole).toInt();
                    if (originalIndex >= 0 && originalIndex < _items.size()) {
                        QString targetName = _items[originalIndex].filename;
                        QString targetPath = QDir(getSoundFontsDirectory()).filePath(targetName);
                        if (destPath != targetPath) {
                            if (QFile::exists(targetPath)) QFile::remove(targetPath);
                            QFile::rename(destPath, targetPath);
                            finalPath = targetPath;
                        }
                    }
                }
            }
        }
        
        populateTable(); // Refresh table to show "Installed"
        QMessageBox::information(this, tr("Download Complete"), tr("SoundFont saved successfully and added to your configuration."));
        emit soundFontDownloaded(finalPath);
    }

    delete _downloadFile;
    _downloadFile = nullptr;

    _downloadReply->deleteLater();
    _downloadReply = nullptr;
}

QString DownloadSoundFontDialog::extractZipAndFindSoundFont(const QString &zipPath, const QString &targetFilename) const {
    QFileInfo zipInfo(zipPath);
    QDir baseDir(zipInfo.absolutePath());
    
    // Create a temporary extraction folder to avoid cluttering the soundfonts directory
    QString tempDirName = "temp_extract_" + QString::number(QDateTime::currentMSecsSinceEpoch());
    if (!baseDir.mkdir(tempDirName)) {
        return QString();
    }
    
    QString tempDirPath = baseDir.filePath(tempDirName);
    
    // Natively extract using the OS 'tar' command (modern Windows 10+ and unix have this)
    QProcess process;
    process.setWorkingDirectory(tempDirPath);
    process.start("tar", QStringList() << "-xf" << zipInfo.absoluteFilePath());
    
    if (!process.waitForFinished(30000) || process.exitCode() != 0) {
        QDir(tempDirPath).removeRecursively();
        return QString(); // Timed out or failed
    }
    
    // Search the temporary directory for the actual .sf2, .sf3, or .dls file
    QString foundSfPath;
    QDirIterator it(tempDirPath, QStringList() << "*.sf2" << "*.sf3" << "*.dls", QDir::Files, QDirIterator::Subdirectories);
    if (it.hasNext()) {
        foundSfPath = it.next();
    }
    
    QString finalDestPath;
    if (!foundSfPath.isEmpty()) {
        QFileInfo foundInfo(foundSfPath);
        
        // Use targetFilename if provided, otherwise keep original name
        QString finalName = targetFilename.isEmpty() ? foundInfo.fileName() : targetFilename;
        finalDestPath = baseDir.filePath(finalName);
        
        // Remove existing file to allow overwrite
        if (QFile::exists(finalDestPath)) {
            QFile::remove(finalDestPath);
        }
        // Move the file into the main soundfonts directory
        QFile::rename(foundSfPath, finalDestPath);
    }
    
    // Clean up: Delete the temporary extraction directory (and all unwanted documents/folders inside)
    QDir(tempDirPath).removeRecursively();
    // Clean up: Delete the original downloaded ZIP file
    QFile::remove(zipPath);
    
    return finalDestPath;
}

void DownloadSoundFontDialog::openMusicalArtifacts() {
    QDesktopServices::openUrl(QUrl("https://musical-artifacts.com"));
}

#endif // FLUIDSYNTH_SUPPORT
