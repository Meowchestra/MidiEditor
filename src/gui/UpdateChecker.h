#ifndef UPDATECHECKER_H
#define UPDATECHECKER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonObject>

class UpdateChecker : public QObject
{
    Q_OBJECT
public:
    explicit UpdateChecker(QObject *parent = nullptr);
    void checkForUpdates(bool forceShowDialog = false);
    static QString filterChangelog(const QString &fullText, const QString &latest, const QString &current);

signals:
    void updateAvailable(QString version, QString url);
    void updateAvailableData(QString version, QJsonObject releaseData);
    void noUpdateAvailable();
    void errorOccurred(QString error);

private slots:
    void onResult(QNetworkReply *reply);
    void onChangelogResult(QNetworkReply *reply);

private:
    QNetworkAccessManager *manager;
    QJsonObject pendingReleaseObj;
    QString pendingLatestVersion;
    QString pendingCurrentVersion;
    bool m_forceShowDialog = false;
};

#endif // UPDATECHECKER_H
