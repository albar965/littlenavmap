#include "mapdownloader.h"

MapDownloader::MapDownloader()
    : QObject{}
{}

MapDownloader::~MapDownloader() {
    delete netManager;
}

void MapDownloader::equip(QSet<QString> *idsMissing, QString &tileURL)
{
    this->idsMissing = idsMissing;
    this->tileURL = tileURL;

    receivedTiles = new QHash<QString, QImage>();
    localFailedTiles = new QSet<QString>();
}

void MapDownloader::run() {
    qDebug() << "MGO: running thread";
    netManager = new QNetworkAccessManager(this);
    netManager->setTransferTimeout(3500);

    countToRequest = idsMissing->count();

    foreach(QString missingId, *idsMissing) {
        QStringList parts = missingId.split("/");
        QUrl qUrl = QUrl(QString(tileURL).replace("{z}", parts[0]).replace("{x}", parts[1]).replace("{y}", parts[2]));
        request2id.insert(qUrl.toString(), missingId);
        QNetworkReply *reply = netManager->get(QNetworkRequest(qUrl));
        connect(reply, &QNetworkReply::finished,
                this, [=](){ this->netReplyReceived(reply); });
        connect(reply, &QNetworkReply::errorOccurred,
                this, [=](QNetworkReply::NetworkError code){ this->netErrorOccurred(code, reply); });
        connect(reply, &QNetworkReply::sslErrors,
                this, [=](const QList<QSslError> &errors){ this->netSSLErrorOccurred(errors, reply); });
        qDebug() << "MGO: image requested " << qUrl.toString();
    }

    doneRequesting = true;

    --countReceived;
    checkEnd();
}

void MapDownloader::checkEnd() {
    if(doneRequesting && ++countReceived >= countToRequest) {
        emit finished(receivedTiles, localFailedTiles, idsMissing, tileURL);
    }
}

void MapDownloader::netReplyReceived(QNetworkReply *reply) {
    qDebug() << "MGO: reply received for " << reply->request().url().toString();
    if(reply->error() == QNetworkReply::NoError) {
        QImage image = QImageReader(reply).read();
        qDebug() << "MGO: reply 2 image";
        if(!image.isNull()) {
            receivedTiles->insert(request2id[reply->request().url().toString()], image);
            qDebug() << "MGO: image assigned";
            checkEnd();
            return;
        }
    }
    if(!localFailedTiles->contains(request2id[reply->request().url().toString()])) {
        localFailedTiles->insert(request2id[reply->request().url().toString()]);
        checkEnd();
    }
}

void MapDownloader::netErrorOccurred(QNetworkReply::NetworkError code, QNetworkReply *reply) {
    qDebug() << "MGO: net error " << code << " for " << reply->request().url().toString();
    localFailedTiles->insert(request2id[reply->request().url().toString()]);
    checkEnd();
}

void MapDownloader::netSSLErrorOccurred(const QList<QSslError> &errors, QNetworkReply *reply) {
    qDebug() << "MGO: net SSL error(s) for " << reply->request().url().toString();
    foreach(QSslError e, errors) {
        qDebug() << "MGO: net SSL error " << e.errorString();
    }
    localFailedTiles->insert(request2id[reply->request().url().toString()]);
    checkEnd();
}
