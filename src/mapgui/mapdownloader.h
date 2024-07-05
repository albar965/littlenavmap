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

    void equip(QList<std::list<QString>*> *idsMissing, QString tileURL, MapGraphic *caller);

public slots:
    void run();

signals:
    void finished();

private:
    QList<std::list<QString>*> *idsMissing;
    QString tileURL;
    MapGraphic *caller;

    QNetworkAccessManager *netManager;

    QHash<QString, QString> request2id;
    int countReceived = 0;

    void netReplyReceived(QNetworkReply *reply);
    void netErrorOccurred(QNetworkReply::NetworkError code, QNetworkReply *reply);
    void netSSLErrorOccurred(const QList<QSslError> &errors, QNetworkReply *reply);
};

#endif // MAPDOWNLOADER_H
