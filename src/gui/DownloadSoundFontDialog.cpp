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

#include <QVBoxLayout>
#include <QHBoxLayout>
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
    // Define available high-quality SoundFonts
    _items = {
        {
            tr("GeneralUser GS"),
            tr("30.8 MB"),
            tr("SF2"),
            QStringLiteral("https://raw.githubusercontent.com/mrbumpy409/GeneralUser-GS/refs/heads/main/GeneralUser-GS.sf2"),
            QStringLiteral("GeneralUser-GS.sf2"),
            false
        },
        {
            tr("MS Basic 2 (MuseScore 4)"),
            tr("205 MB"),
            tr("SF2"),
            QStringLiteral("https://github.com/Meowchestra/MS_Basic/releases/download/v2.0.0/MS_Basic-v2.0.0.sf2"),
            QStringLiteral("MS_Basic-v2.0.0.sf2"),
            false
        },
        {
            tr("MS Basic 2 (Compressed)"),
            tr("38 MB"),
            tr("SF3"),
            QStringLiteral("https://github.com/Meowchestra/MS_Basic/releases/download/v2.0.0/MS_Basic-v2.0.0.sf3"),
            QStringLiteral("MS_Basic-v2.0.0.sf3"),
            false
        },
        {
            tr("SGM"),
            tr("309 MB"),
            tr("SF2"),
            QStringLiteral("https://musical-artifacts.com/artifacts/855/SGM-v2.01-NicePianosGuitarsBass-V1.2.sf2"),
            QStringLiteral("SGM-v2.01-NicePianosGuitarsBass-V1.2.sf2"),
            false
        },
        {
            tr("FluidR3 GM+GS"),
            tr("144 MB"),
            tr("SF2"),
            QStringLiteral("https://musical-artifacts.com/artifacts/1229/FluidR3_GM_GS.sf2"),
            QStringLiteral("FluidR3_GM_GS.sf2"),
            false
        },
        {
            tr("Microsoft GS Wavetable Synth"),
            tr("3.07 MB"),
            tr("SF2"),
            QStringLiteral("https://musical-artifacts.com/artifacts/5190/microsoft_gm_3.sf2"),
            QStringLiteral("microsoft_gm_3.sf2"),
            false
        },
        {
            tr("B. Meowsic"),
            tr("18.9 MB"),
            tr("SF2"),
            QStringLiteral("https://musical-artifacts.com/artifacts/2617/Meowsic_Cat_Soundfont.sf2"),
            QStringLiteral("Meowsic_Cat_Soundfont.sf2"),
            false
        },
        {
            tr("FFXIV Standard"),
            tr("14.2 MB"),
            tr("SF2"),
            QStringLiteral("https://github.com/Meowchestra/FFXIV-SoundFont/releases/download/v7.3.0/FFXIV-Standard.sf2"),
            QStringLiteral("FFXIV-Standard.sf2"),
            false
        },
        {
            tr("FFXIV Standard (Compressed)"),
            tr("3 MB"),
            tr("SF3"),
            QStringLiteral("https://github.com/Meowchestra/FFXIV-SoundFont/releases/download/v7.3.0/FFXIV-Standard.sf3"),
            QStringLiteral("FFXIV-Standard.sf3"),
            false
        },
        {
            tr("FFXIV Expanded"),
            tr("14.2 MB"),
            tr("SF2"),
            QStringLiteral("https://github.com/Meowchestra/FFXIV-SoundFont/releases/download/v7.3.0/FFXIV-Expanded.sf2"),
            QStringLiteral("FFXIV-Expanded.sf2"),
            false
        },
        {
            tr("FFXIV Expanded (Compressed)"),
            tr("3 MB"),
            tr("SF3"),
            QStringLiteral("https://github.com/Meowchestra/FFXIV-SoundFont/releases/download/v7.3.0/FFXIV-Expanded.sf3"),
            QStringLiteral("FFXIV-Expanded.sf3"),
            false
        },
        // --- Legacy section ---
        {
            tr("MS Basic (MuseScore 3)"),
            tr("466 MB"),
            tr("SF2"),
            QStringLiteral("https://musical-artifacts.com/artifacts/3001/MS_Basic.sf2"),
            QStringLiteral("MS_Basic.sf2"),
            true
        },
        {
            tr("MS Basic (Compressed)"),
            tr("48.9 MB"),
            tr("SF3"),
            QStringLiteral("https://raw.githubusercontent.com/musescore/MuseScore/refs/heads/master/share/sound/MS%20Basic.sf3"),
            QStringLiteral("MS Basic.sf3"),
            true
        },
        {
            tr("MuseScore_General (MuseScore 3)"),
            tr("208 MB"),
            tr("SF2"),
            QStringLiteral("https://ftp.osuosl.org/pub/musescore/soundfont/MuseScore_General/MuseScore_General.sf2"),
            QStringLiteral("MuseScore_General.sf2"),
            true
        },
        {
            tr("MuseScore_General (Compressed)"),
            tr("35.9 MB"),
            tr("SF3"),
            QStringLiteral("https://ftp.osuosl.org/pub/musescore/soundfont/MuseScore_General/MuseScore_General.sf3"),
            QStringLiteral("MuseScore_General.sf3"),
            true
        },
        {
            tr("FluidR3Mono_GM (MuseScore 2)"),
            tr("22.6 MB"),
            tr("SF3"),
            QStringLiteral("https://raw.githubusercontent.com/musescore/MuseScore/refs/heads/master/share/sound/FluidR3Mono_GM.sf3"),
            QStringLiteral("FluidR3Mono_GM.sf3"),
            true
        },
        {
            tr("TimGM6mb (MuseScore 1)"),
            tr("5.7 MB"),
            tr("SF2"),
            QStringLiteral("https://sourceforge.net/p/mscore/code/HEAD/tree/trunk/mscore/share/sound/TimGM6mb.sf2?format=raw"),
            QStringLiteral("TimGM6mb.sf2"),
            true
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

    QLabel *infoLabel = new QLabel(tr("Select a SoundFont to download and install automatically.\n"
                                      "Files will be saved to the soundfonts folder."));
    mainLayout->addWidget(infoLabel);

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

    QHBoxLayout *bottomLayout = new QHBoxLayout();
    
    _showLegacyCheckBox = new QCheckBox(tr("Show Legacy SoundFonts"), this);
    bottomLayout->addWidget(_showLegacyCheckBox);
    connect(_showLegacyCheckBox, &QCheckBox::toggled, this, &DownloadSoundFontDialog::populateTable);
    
    bottomLayout->addStretch();

    QHBoxLayout *btnLayout = new QHBoxLayout();
    
    _findMoreBtn = new QPushButton(tr("Find More Online..."), this);
    btnLayout->addWidget(_findMoreBtn);
    
    btnLayout->addStretch();
    
    _downloadBtn = new QPushButton(tr("Download Selected"), this);
    _closeBtn = new QPushButton(tr("Close"), this);
    
    btnLayout->addWidget(_downloadBtn);
    btnLayout->addWidget(_closeBtn);
    mainLayout->addLayout(bottomLayout);
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
    _table->setRowCount(_items.size());
    QString currentDir = getSoundFontsDirectory();
    bool showLegacy = _showLegacyCheckBox->isChecked();

    for (int i = 0; i < _items.size(); ++i) {
        const auto &item = _items[i];
        
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
        
        // Hide legacy items if checkbox is unchecked
        _table->setRowHidden(i, item.isLegacy && !showLegacy);
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
    if (row < 0 || row >= _items.size()) return;

    const auto &item = _items[row];
    QUrl url(item.url);
    _currentDownloadName = item.filename;
    
    QString destPath = QDir(getSoundFontsDirectory()).filePath(_currentDownloadName);
    
    if (QFile::exists(destPath)) {
        QMessageBox::information(this, tr("Already Downloaded"), tr("This SoundFont is already downloaded."));
        emit soundFontDownloaded(destPath);
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
            QString extractedFile = extractZipAndFindSoundFont(destPath);
            if (!extractedFile.isEmpty()) {
                finalPath = extractedFile;
                QMessageBox::information(this, tr("Extraction Complete"), tr("ZIP file extracted successfully. Proceeding to add SoundFont."));
            } else {
                QMessageBox::warning(this, tr("Extraction Failed"), tr("Downloaded the ZIP, but could not extract the SF2 file automatically. You may need to unzip it manually."));
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

QString DownloadSoundFontDialog::extractZipAndFindSoundFont(const QString &zipPath) const {
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
        finalDestPath = baseDir.filePath(foundInfo.fileName());
        
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
