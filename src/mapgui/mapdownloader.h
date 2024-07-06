#ifndef MAPDOWNLOADER_H
#define MAPDOWNLOADER_H

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QImageReader>

class MapGraphic;

class MapDownloader : public QObject
{
    Q_OBJECT

public:
    explicit MapDownloader();
    virtual ~MapDownloader();

    void equip(QSet<QString> *idsMissing, QSet<QString> *failedTiles, QString &tileURL);

public slots:
    void run();

signals:
    void finished(QHash<QString, QImage> *receivedTiles, QSet<QString> *localFailedTiles, QString url);

private:
    QSet<QString> *idsMissing;
    QSet<QString> *failedTiles;
    QString tileURL;

    QNetworkAccessManager *netManager;

    QHash<QString, QImage> *receivedTiles;
    QSet<QString> *localFailedTiles;

    QHash<QString, QString> request2id;
    int countToRequest;
    int countReceived = 0;

    void checkEnd();

    void netReplyReceived(QNetworkReply *reply);
    void netErrorOccurred(QNetworkReply::NetworkError code, QNetworkReply *reply);
    void netSSLErrorOccurred(const QList<QSslError> &errors, QNetworkReply *reply);
};

#endif // MAPDOWNLOADER_H
