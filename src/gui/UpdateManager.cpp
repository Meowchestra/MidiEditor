#include "UpdateManager.h"
#include <QJsonArray>
#include <QCoreApplication>
#include <QCheckBox>
#include "Appearance.h"
#include <QProcess>
#include <QDebug>
#include <QStandardPaths>
#include <QFileInfo>
#include <QSettings>

UpdateManager::UpdateManager(QObject *parent) : QObject(parent), reply(nullptr), downloadedFile(nullptr)
{
    manager = new QNetworkAccessManager(this);
    qDebug() << "UpdateManager initialized";
}

void UpdateManager::startUpdate(const QJsonObject &releaseData, bool immediate)
{
    isImmediate = immediate;
    m_latestVersion = releaseData["tag_name"].toString();
    
    // Determine if we are using the installer or portable version
    QString appDir = QCoreApplication::applicationDirPath();
    bool hasMaintenanceTool = QFile::exists(appDir + "/maintenancetool.exe");
    
    QString assetNamePattern = hasMaintenanceTool ? "MidiEditor-Installer.exe" : "MidiEditor-Portable.zip";
    isInstaller = hasMaintenanceTool;
    qDebug() << "Starting update check. Pattern:" << assetNamePattern << "IsInstaller:" << isInstaller;

    QJsonArray assets = releaseData["assets"].toArray();
    QString downloadUrl;
    
    for (const QJsonValue &value : assets) {
        QJsonObject asset = value.toObject();
        QString name = asset["name"].toString();
        if (name.contains(assetNamePattern, Qt::CaseInsensitive)) {
            downloadUrl = asset["browser_download_url"].toString();
            qDebug() << "Found matching asset:" << name << "URL:" << downloadUrl;
            break;
        }
    }

    if (downloadUrl.isEmpty()) {
        emit updateError(tr("Could not find suitable update asset in release."));
        return;
    }

    downloadFile(downloadUrl);
}

void UpdateManager::downloadFile(const QString &url)
{
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString fileName = url.section('/', -1);
    targetFilePath = tempDir + "/" + fileName;

    downloadedFile = new QFile(targetFilePath);
    if (!downloadedFile->open(QIODevice::WriteOnly)) {
        qCritical() << "Could not open file for writing:" << targetFilePath;
        emit updateError(tr("Could not create temporary file for download."));
        delete downloadedFile;
        downloadedFile = nullptr;
        return;
    }

    qDebug() << "Downloading to:" << targetFilePath;

    QNetworkRequest request((QUrl(url)));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    
    reply = manager->get(request);
    connect(reply, &QNetworkReply::finished, this, &UpdateManager::onDownloadFinished);
    connect(reply, &QNetworkReply::downloadProgress, this, &UpdateManager::onDownloadProgress);
}

void UpdateManager::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    emit downloadProgress(bytesReceived, bytesTotal);
}

void UpdateManager::onDownloadFinished()
{
    qDebug() << "Download finished";
    if (reply->error() != QNetworkReply::NoError) {
        qCritical() << "Download error:" << reply->errorString();
        emit updateError(tr("Download failed: %1").arg(reply->errorString()));
        downloadedFile->close();
        downloadedFile->remove();
        delete downloadedFile;
        downloadedFile = nullptr;
        reply->deleteLater();
        reply = nullptr;
        return;
    }

    downloadedFile->write(reply->readAll());
    downloadedFile->close();
    
    reply->deleteLater();
    reply = nullptr;

    if (isImmediate) {
        finalizeUpdate();
    } else {
        // Store the path for later
        QScopedPointer<QSettings> settings(Appearance::settings());
        settings->setValue("updater/pending_update_file", targetFilePath);
        settings->setValue("updater/is_installer", isInstaller);
        settings->setValue("updater/latest_version", m_latestVersion);
    }
}

void UpdateManager::finalizeUpdate()
{
    qDebug() << "Finalizing update. IsInstaller:" << isInstaller;
    if (isInstaller) {
        QProcess::startDetached(targetFilePath);
        emit readyToExit();
    } else {
        createPortableUpdateScript(targetFilePath);
    }
}

void UpdateManager::createPortableUpdateScript(const QString &zipPath)
{
    QString appDir = QCoreApplication::applicationDirPath();
    QString appPath = QCoreApplication::applicationFilePath();
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString scriptPath = tempDir + "/update_midieditor.ps1";

    QFile script(scriptPath);
    if (script.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&script);
        // UTF-8 is default in Qt 6
        
        out << "$appDir = \"" << QDir::toNativeSeparators(appDir) << "\"\n";
        out << "$zipPath = \"" << QDir::toNativeSeparators(zipPath) << "\"\n";
        out << "$appPath = \"" << QDir::toNativeSeparators(appPath) << "\"\n";
        out << "$processId = " << QCoreApplication::applicationPid() << "\n\n";

        out << "$logFile = \"$env:TEMP\\MidiEditor_Update.log\"\n";
        out << "\"Starting update script...\" | Out-File -FilePath $logFile\n\n";

        out << "\"Script initialized. Target version: $m_latestVersion\" | Out-File -FilePath $logFile -Append\n\n";

        out << "# Wait for the process to exit with timeout\n";
        out << "$timeout = 10\n";
        out << "while ((Get-Process -Id $processId -ErrorAction SilentlyContinue) -and ($timeout -gt 0)) {\n";
        out << "    Start-Sleep -Seconds 1\n";
        out << "    $timeout--\n";
        out << "}\n";
        out << "if (Get-Process -Id $processId -ErrorAction SilentlyContinue) {\n";
        out << "    \"Process $processId hung. Forcing kill.\" | Out-File -FilePath $logFile -Append\n";
        out << "    Stop-Process -Id $processId -Force -ErrorAction SilentlyContinue\n";
        out << "}\n\n";

        out << "\"Process exited. Starting extraction...\" | Out-File -FilePath $logFile -Append\n";
        out << "Start-Sleep -Seconds 1\n\n";

        out << "Add-Type -AssemblyName System.Windows.Forms\n\n";
        out << "try {\n";
        out << "    $extractPath = \"$env:TEMP\\MidiEditor_Update_Extract\"\n";
        out << "    if (Test-Path $extractPath) { Remove-Item -Path $extractPath -Recurse -Force }\n";
        out << "    New-Item -ItemType Directory -Path $extractPath | Out-Null\n";
        out << "    Expand-Archive -Path $zipPath -DestinationPath $extractPath -Force\n\n";

        out << "    \"Extraction complete. Copying files...\" | Out-File -FilePath $logFile -Append\n";
        out << "    $contentPath = Join-Path $extractPath \"MidiEditor\"\n";
        out << "    if (Test-Path $contentPath) {\n";
        out << "        Copy-Item -Path \"$contentPath\\*\" -Destination $appDir -Recurse -Force\n";
        out << "    } else {\n";
        out << "        Copy-Item -Path \"$extractPath\\*\" -Destination $appDir -Recurse -Force\n";
        out << "    }\n\n";

        out << "    \"Copy complete. Cleaning up...\" | Out-File -FilePath $logFile -Append\n";
        out << "    Remove-Item -Path $extractPath -Recurse -Force\n";
        out << "    Remove-Item -Path $zipPath -Force\n\n";

        out << "    \"Relaunching $appPath\" | Out-File -FilePath $logFile -Append\n";
        out << "    Start-Process -FilePath \"$appPath\"\n";
        out << "} catch {\n";
        out << "    \"Error: $($_.Exception.Message)\" | Out-File -FilePath $logFile -Append\n";
        out << "    [System.Windows.Forms.MessageBox]::Show(\"Update failed: $($_.Exception.Message)\", \"MidiEditor Update\")\n";
        out << "}\n\n";

        out << "Remove-Item -Path $MyInvocation.MyCommand.Definition -Force\n";
        
        script.close();

        // Run the script
        QStringList arguments;
        arguments << "-WindowStyle" << "Hidden" << "-NoProfile" << "-ExecutionPolicy" << "Bypass" << "-File" << scriptPath;
        QProcess::startDetached("powershell.exe", arguments);
        
        emit readyToExit();
    } else {
        emit updateError(tr("Could not create update script."));
    }
}

void UpdateManager::applyUpdateAfterExit()
{
    QScopedPointer<QSettings> settings(Appearance::settings());
    QString zipPath = settings->value("updater/pending_update_file").toString();
    bool installer = settings->value("updater/is_installer").toBool();
    QString version = settings->value("updater/latest_version").toString();

    if (zipPath.isEmpty() || !QFile::exists(zipPath)) return;

    isInstaller = installer;
    targetFilePath = zipPath;
    m_latestVersion = version;
    isImmediate = true;

    finalizeUpdate();

    // Clear settings so it doesn't repeatedly try
    settings->remove("updater/pending_update_file");
    settings->remove("updater/is_installer");
    settings->remove("updater/latest_version");
}


