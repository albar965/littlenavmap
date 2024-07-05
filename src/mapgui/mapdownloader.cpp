#include "mapdownloader.h"
#include "mapgui/mapgraphic.h"

MapDownloader::MapDownloader()
    : QObject{}
{

}

MapDownloader::~MapDownloader() {
    delete netManager;
}

void MapDownloader::equip(QList<std::list<QString>*> *idsMissing, QString tileURL, MapGraphic *caller)
{
    this->idsMissing = idsMissing;
    this->tileURL = tileURL;
    this->caller = caller;

    netManager = new QNetworkAccessManager(this);
    netManager->setTransferTimeout(3500);
}

void MapDownloader::run() {
    qDebug() << "MGO: running thread";
    foreach(std::list<QString> *missingIds, *idsMissing) {
        foreach(QString missingId, *missingIds) {
            QStringList parts = missingId.split("/");
            QUrl qUrl = QUrl(QString(tileURL).replace("{z}", parts[0]).replace("{x}", parts[1]).replace("{y}", parts[2]));
            if(!request2id.contains(qUrl.toString()) && !caller->failedTiles.contains(missingId)) {
                request2id.insert(qUrl.toString(), missingId);
                qDebug() << "MGO: net about to be used for " << qUrl.toString();
                QNetworkReply *reply = netManager->get(QNetworkRequest(qUrl));
                qDebug() << "MGO: net connect 1 " << (bool)connect(reply, &QNetworkReply::finished,
                                                                    this, [=](){ this->netReplyReceived(reply); });
                qDebug() << "MGO: net connect 2 " << (bool)connect(reply, &QNetworkReply::errorOccurred,
                                                                    this, [=](QNetworkReply::NetworkError code){ this->netErrorOccurred(code, reply); });
                qDebug() << "MGO: net connect 3 " << (bool)connect(reply, &QNetworkReply::sslErrors,
                                                                    this, [=](const QList<QSslError> &errors){ this->netSSLErrorOccurred(errors, reply); });
                qDebug() << "MGO: image requested " << qUrl.toString();
            }
        }
    }

    foreach(std::list<QString> *missingIds, *idsMissing) {
        delete missingIds;
    }
    delete idsMissing;
}


void MapDownloader::netReplyReceived(QNetworkReply *reply) {
    qDebug() << "MGO: reply received for " << reply->request().url().toString();
    if(reply->error() == QNetworkReply::NoError) {
        QImage image = QImageReader(reply).read();
        qDebug() << "MGO: reply 2 image";
        if(!image.isNull()) {
            caller->currentTiles->insert(request2id[reply->request().url().toString()], image);
            qDebug() << "MGO: image assigned";
            reply->deleteLater();
            qDebug() << "MGO: reply scheduled for deletion";
            ++countReceived;
            if(countReceived >= request2id.count()) {
                emit finished();
            }
            return;
        }
    }
    reply->deleteLater();
    if(!caller->failedTiles.contains(request2id[reply->request().url().toString()])) {
        caller->failedTiles.insert(request2id[reply->request().url().toString()], true);
        ++countReceived;
        if(countReceived >= request2id.count()) {
            emit finished();
        }
    }
}

void MapDownloader::netErrorOccurred(QNetworkReply::NetworkError code, QNetworkReply *reply) {
    qDebug() << "MGO: net error " << code << " for " << reply->request().url().toString();
    caller->failedTiles.insert(request2id[reply->request().url().toString()], true);
    ++countReceived;
    if(countReceived >= request2id.count()) {
        emit finished();
    }
}

void MapDownloader::netSSLErrorOccurred(const QList<QSslError> &errors, QNetworkReply *reply) {
    qDebug() << "MGO: net SSL error(s) for " << reply->request().url().toString();
    foreach(QSslError e, errors) {
        qDebug() << "MGO: net SSL error " << e.errorString();
    }
    caller->failedTiles.insert(request2id[reply->request().url().toString()], true);
    ++countReceived;
    if(countReceived >= request2id.count()) {
        emit finished();
    }
}
