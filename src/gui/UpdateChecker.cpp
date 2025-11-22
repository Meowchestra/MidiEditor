#include "UpdateChecker.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QCoreApplication>
#include <QVersionNumber>
#include <QUrl>
#include <QNetworkRequest>


UpdateChecker::UpdateChecker(QObject *parent) : QObject(parent)
{
    manager = new QNetworkAccessManager(this);
    connect(manager, &QNetworkAccessManager::finished, this, &UpdateChecker::onResult);
}

void UpdateChecker::checkForUpdates()
{
    QNetworkRequest request(QUrl("https://api.github.com/repos/Meowchestra/MidiEditor/releases/latest"));
    // GitHub API requires a User-Agent
    request.setHeader(QNetworkRequest::UserAgentHeader, "MidiEditor");
    manager->get(request);
}

void UpdateChecker::onResult(QNetworkReply *reply)
{
    if (reply->error() != QNetworkReply::NoError) {
        emit errorOccurred(reply->errorString());
        reply->deleteLater();
        return;
    }

    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    QJsonObject obj = doc.object();

    QString tagName = obj["tag_name"].toString();
    // Remove 'v' prefix if present
    if (tagName.startsWith("v")) {
        tagName = tagName.mid(1);
    }

    QString currentVersionStr = QCoreApplication::applicationVersion();
    // Remove 'v' prefix from current version if present (just in case)
    if (currentVersionStr.startsWith("v")) {
        currentVersionStr = currentVersionStr.mid(1);
    }

    QVersionNumber currentVersion = QVersionNumber::fromString(currentVersionStr);
    QVersionNumber latestVersion = QVersionNumber::fromString(tagName);

    if (latestVersion > currentVersion) {
        emit updateAvailable(tagName, obj["html_url"].toString());
    } else {
        emit noUpdateAvailable();
    }

    reply->deleteLater();
}
