#include "UpdateChecker.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QCoreApplication>
#include <QVersionNumber>
#include <QUrl>
#include <QNetworkRequest>
#include <QRegularExpression>

UpdateChecker::UpdateChecker(QObject *parent) : QObject(parent)
{
    manager = new QNetworkAccessManager(this);
}

void UpdateChecker::checkForUpdates(bool forceShowDialog)
{
    m_forceShowDialog = forceShowDialog;
    QNetworkRequest request(QUrl("https://api.github.com/repos/Meowchestra/MidiEditor/releases/latest"));
    request.setHeader(QNetworkRequest::UserAgentHeader, "MidiEditor");
    
    QNetworkReply *reply = manager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply](){
        this->onResult(reply);
    });
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
    pendingReleaseObj = doc.object();

    QString tagName = pendingReleaseObj["tag_name"].toString();
    if (tagName.startsWith("v")) {
        tagName = tagName.mid(1);
    }
    pendingLatestVersion = tagName;

    QString currentVersionStr = QCoreApplication::applicationVersion();
    if (currentVersionStr.startsWith("v")) {
        currentVersionStr = currentVersionStr.mid(1);
    }
    pendingCurrentVersion = currentVersionStr;

    QVersionNumber currentVersion = QVersionNumber::fromString(currentVersionStr);
    QVersionNumber latestVersion = QVersionNumber::fromString(tagName);

    if (m_forceShowDialog || latestVersion > currentVersion) {
        // Now fetch the changelog
        QNetworkRequest request(QUrl("https://raw.githubusercontent.com/Meowchestra/MidiEditor/main/CHANGELOG.md"));
        request.setHeader(QNetworkRequest::UserAgentHeader, "MidiEditor");
        
        QNetworkReply *clReply = manager->get(request);
        connect(clReply, &QNetworkReply::finished, this, [this, clReply](){
            this->onChangelogResult(clReply);
        });
    } else {
        emit noUpdateAvailable();
    }

    reply->deleteLater();
}

void UpdateChecker::onChangelogResult(QNetworkReply *reply)
{
    QString fullChangelog;
    if (reply->error() == QNetworkReply::NoError) {
        fullChangelog = QString::fromUtf8(reply->readAll());
    }

    QString currentVerForFilter = m_forceShowDialog ? "0.0.0" : pendingCurrentVersion;
    QString filtered = filterChangelog(fullChangelog, pendingLatestVersion, currentVerForFilter);
    pendingReleaseObj["body"] = filtered;

    emit updateAvailable(pendingLatestVersion, pendingReleaseObj["html_url"].toString());
    emit updateAvailableData(pendingLatestVersion, pendingReleaseObj);

    reply->deleteLater();
}

QString UpdateChecker::filterChangelog(const QString &fullText, const QString &latest, const QString &current)
{
    if (fullText.isEmpty()) return "";

    QStringList lines = fullText.split('\n');
    QString result;
    bool capturing = false;

    // We want to capture sections from 'latest' down to (but excluding) 'current'
    // Common header formats: "# 4.4.1", "## 4.4.1", "[4.4.1]"
    QRegularExpression versionHeader("^#+ \\s*(\\[?)([\\d\\.]+)(\\]?)");

    for (const QString &line : lines) {
        QRegularExpressionMatch match = versionHeader.match(line);
        if (match.hasMatch()) {
            QString ver = match.captured(2);
            QVersionNumber v = QVersionNumber::fromString(ver);
            QVersionNumber latestV = QVersionNumber::fromString(latest);
            QVersionNumber currentV = QVersionNumber::fromString(current);

            if (v > latestV) {
                capturing = false;
            } else if (v > currentV) {
                capturing = true;
            } else {
                // We've reached the current version or older
                break;
            }
        }

        if (capturing) {
            result += line + "\n";
        }
    }

    return result.trimmed();
}
