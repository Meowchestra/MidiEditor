#ifndef UPDATECHECKER_H
#define UPDATECHECKER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>

class UpdateChecker : public QObject
{
    Q_OBJECT
public:
    explicit UpdateChecker(QObject *parent = nullptr);
    void checkForUpdates();

signals:
    void updateAvailable(QString version, QString url);
    void noUpdateAvailable();
    void errorOccurred(QString error);

private slots:
    void onResult(QNetworkReply *reply);

private:
    QNetworkAccessManager *manager;
};

#endif // UPDATECHECKER_H
