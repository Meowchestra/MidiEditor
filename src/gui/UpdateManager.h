#ifndef UPDATEMANAGER_H
#define UPDATEMANAGER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonObject>
#include <QFile>
#include <QDir>

class UpdateManager : public QObject
{
    Q_OBJECT
public:
    explicit UpdateManager(QObject *parent = nullptr);

    void startUpdate(const QJsonObject &releaseData, bool immediate = true);
    void applyUpdateAfterExit();

signals:
    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void updateError(QString error);
    void readyToExit();

private slots:
    void onDownloadFinished();
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);

private:
    void downloadFile(const QString &url);
    void finalizeUpdate();
    void createPortableUpdateScript(const QString &zipPath);

    QNetworkAccessManager *manager;
    QNetworkReply *reply;
    QFile *downloadedFile;
    QString targetFilePath;
    bool isInstaller;
    bool isImmediate;
    QString m_latestVersion;
};

#endif // UPDATEMANAGER_H
