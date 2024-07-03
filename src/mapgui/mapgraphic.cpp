#include "mapgraphic.h"
#include "mapgui/mapgraphicthread.h"

MapGraphic::MapGraphic( QWidget *parent ) : QWidget(parent) {
    QWidget::setAttribute(Qt::WA_OpaquePaintEvent);
    painter = new QPainter(this);
    paintDisplayAs = &MapGraphic::paintSphere;
    sphereRadiusInPixel = std::min(size().height(), size().width()) * 4.0f / 6.0f / 2.0f;     // earth covers 4/6. of viewport width or height
    //referenceCoverage = 2 * sphereRadiusInPixel / size().width();
    //referenceDistance = size().width() / 2 / sphereRadiusInPixel * earthRadius / tanDefaultLensHalfHorizontalOpeningAngle;
    oldZoom = ceilf(log2f(2 * sphereRadiusInPixel / tileWidth)) + 2;
    netManager = new QNetworkAccessManager(this);
    connect(netManager, &QNetworkAccessManager::finished,
            this, &MapGraphic::netReplyReceived);
}

MapGraphic::~MapGraphic() {
    delete painter;
    foreach (QString tileURLForLookup, tilesDownloaded.keys()) {
        QFile file(tileURLForLookup);
        if(file.open(QIODevice::WriteOnly)) {
            QDataStream out(&file);
            out << tilesDownloaded[tileURLForLookup];
            file.close();
        }
    }
}

void MapGraphic::paintSphere(QPaintEvent *event) {
    if(doPaint) {
        int regionHeight = event->region().boundingRect().height();
        int amountThreads = QThread::idealThreadCount() - 2;        // - 2 = 1 for the main thread, 1 for the os
        if(amountThreads > regionHeight * scaleFactor) {
            amountThreads = regionHeight * scaleFactor;
        }
        if(amountThreads < 1) {
            amountThreads = 1;
        }
        QList<threadData> threadDatas;
        int originalHeight = regionHeight * scaleFactor / amountThreads;
        int counter = 0;
        do {
            int height = (counter == amountThreads - 1) ? regionHeight * scaleFactor - counter * originalHeight : originalHeight;
            QImage *image = new QImage(event->region().boundingRect().width() * scaleFactor, height, QImage::Format_RGB32);
            if(image != nullptr) {
                threadData tData;
                tData.image = image;
                tData.currentTiles = &currentTiles;
                tData.tileWidth = tileWidth;
                tData.tileHeight = tileHeight;
                tData.xStart = event->region().boundingRect().left() * scaleFactor;
                tData.xEnd = tData.xStart + event->region().boundingRect().width() * scaleFactor;
                tData.yStart = event->region().boundingRect().top() * scaleFactor + counter * originalHeight;
                tData.yEnd = tData.yStart + height;
                tData.xOrigin = size().width() / 2 * scaleFactor;
                tData.yOrigin = size().height() / 2 * scaleFactor;
                tData.zoom = ceilf(log2f(2 * sphereRadiusInPixel / tileWidth)) + 2;
                tData.spin = sphereSpin;
                tData.tilt = sphereTilt;
                tData.radius = sphereRadiusInPixel * scaleFactor;
                tData.tryZoomedOut = tData.zoom != oldZoom || tData.spin != oldSpin || tData.tilt != oldTilt;
                tData.indexLastIdCompleted = 0;
                if(tData.zoom > zoomMax) {
                    tData.zoom = zoomMax;
                }
                tData.rastering = true;
                threadDatas.append(tData);
                (new MapGraphicThread(&tData))->start();
            }
        }
        while(++counter < amountThreads);

        int countRastering;
        do {
            countRastering = 0;
            foreach(threadData tData, threadDatas) {
                volatile int countQueued = tData.pixelQueue.count();
                for(int i = 0; i < countQueued; ++i) {
                    QString id = tData.pixelQueue.keys()[i];
                    QString url = tileURL
                                      .replace("{z}", id.split("/")[0])
                                      .replace("{x}", id.split("/")[1])
                                      .replace("{y}", id.split("/")[2]);
                    QUrl qUrl = QUrl(url);
                    if(!request2id.contains(qUrl.toString())) {
                        request2id[qUrl.toString()] = id;
                        netManager->get(QNetworkRequest(qUrl));
                    } else if(currentTiles.contains(id)) {
                        if(!tData.idsDelivered.contains(id)) {
                            tData.idsDelivered.append(id);
                        }
                    }
                }
                if(tData.rastering) {
                    ++countRastering;
                } else {
                    painter->drawPixmap(event->region().boundingRect().left(), tData.yStart / scaleFactor, QPixmap::fromImage(tData.image->scaled(event->region().boundingRect().width(), (tData.yEnd - tData.yStart) / scaleFactor, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)));
                    delete tData.image;
                }
            }
        } while(countRastering > 0);
    }
}

void MapGraphic::paintRectangle(QPaintEvent *event) {
    if(doPaint) {

    }
}

QString MapGraphic::tileURLForLookup() {
    return tileURL.replace("/", "_").replace(":", "_").replace("?", "_");
}

void MapGraphic::netReplyReceived(QNetworkReply *reply) {
    QImage image = QImageReader(reply).read();
    if(!image.isNull()) {
        currentTiles[request2id[reply->request().url().toString()]] = image;
    }
    reply->deleteLater();
}

qreal MapGraphic::distance() const {
    return size().width() / 2 / sphereRadiusInPixel * earthRadius / tanDefaultLensHalfHorizontalOpeningAngle;
}

Marble::MarbleModel *MapGraphic::model() {
    return nullptr;
}

const Marble::MarbleModel *MapGraphic::model() const {
    return nullptr;
}

void MapGraphic::setKeys(QHash<QString, QString> keys) {

}

Marble::RenderStatus MapGraphic::renderStatus() const {
    return Marble::RenderStatus();
}

QPixmap MapGraphic::mapScreenShot() {
    return QPixmap();
}

Marble::Projection MapGraphic::projection() const {
    return Marble::Projection();
}

bool MapGraphic::geoCoordinates( int x, int y,
                                 qreal& lon, qreal& lat,
                                 Marble::GeoDataCoordinates::Unit ) const {
    x = 0.0;
    y = 0.0;
    return true;
}

void MapGraphic::centerOn( const qreal lon, const qreal lat, bool animated ) {

}

Marble::ViewportParams *MapGraphic::viewport() {
    return new Marble::ViewportParams();
}

const Marble::ViewportParams *MapGraphic::viewport() const {
    return new Marble::ViewportParams();
}

void MapGraphic::setMapThemeId( const QString& maptheme ) {
    int found = 0;
    // read, verify and set map theme configuration
    QFile file(maptheme);
    if(file.open(QIODevice::ReadOnly)) {
        QXmlStreamReader xml(QString(file.readAll()));
        file.close();
        int depth = 0;
        bool isTexture = false;
        while (!xml.atEnd()) {
            QStringRef name = xml.name();
            if(isTexture) {
                if(name == "tileSize") {
                    tileWidth = xml.attributes().value("width").toInt();
                    found += tileWidth ? 1 : 0;
                    tileHeight = xml.attributes().value("height").toInt();
                    found += tileHeight ? 1 : 0;
                } else if(name == "storageLayout") {
                    zoomMax = xml.attributes().value("maximumTileLevel").toInt();
                    if(zoomMax > 30) {
                        zoomMax = 30;
                    }
                    found += zoomMax ? 1 : 0;
                } else if(name == "downloadUrl") {
                    tileURL = xml.attributes().value("protocol") + "://" + xml.attributes().value("host") + xml.attributes().value("path");
                    found += xml.attributes().value("protocol").count() ? 1 : 0;
                    found += xml.attributes().value("host").count() ? 1 : 0;
                    found += xml.attributes().value("path").contains("{z}") ? 1 : 0;
                    found += xml.attributes().value("path").contains("{x}") ? 1 : 0;
                    found += xml.attributes().value("path").contains("{y}") ? 1 : 0;
                } else if(xml.isEndElement()) {
                    --depth;
                    if(depth == -1) {
                        break;
                    }
                } else if(xml.isStartElement()) {
                    ++depth;
                }
                xml.readNext();
            } else if(name == "texture") {
                isTexture = true;
                xml.readNext();
            } else if(name != "head") {
                xml.readNext();
            } else {
                xml.readNextStartElement();
            }
        }
    }
    doPaint = found == 8;
    if(doPaint) {
        if(!tilesDownloaded.contains(tileURLForLookup())) {
            QFile file(tileURLForLookup());
            if(file.open(QIODevice::ReadOnly)) {
                QDataStream in(&file);
                in >> tilesDownloaded[tileURLForLookup()];
                file.close();
            } else {
                tilesDownloaded[tileURLForLookup()] = QHash<QString, QImage>();
            }
        }
        currentTiles = tilesDownloaded[tileURLForLookup()];
    }
}

void MapGraphic::setShowClouds( bool visible ) {

}

quint64 MapGraphic::volatileTileCacheLimit() const {
    return 0;
}

void MapGraphic::setVolatileTileCacheLimit( quint64 kiloBytes ) {

}

void MapGraphic::setShowSunShading( bool visible ) {

}

void MapGraphic::setSunShadingDimFactor(qreal dimFactor) {

}

bool MapGraphic::showSunShading() const {
    return false;
}

void MapGraphic::setShowPlaces( bool visible ) {

}

void MapGraphic::setShowCities( bool visible ) {

}

void MapGraphic::setShowOtherPlaces( bool visible ) {

}

void MapGraphic::centerOn( const Marble::GeoDataLatLonBox& box, bool animated ) {

}

int MapGraphic::zoom() const {
    return 0;
}

int MapGraphic::zoomStep() const {
    return 1;
}

void MapGraphic::zoomIn( Marble::FlyToMode mode ) {

}

int MapGraphic::maximumZoom() const {
    return 1;
}

void MapGraphic::zoomViewBy( int zoomStep, Marble::FlyToMode mode ) {

}

bool MapGraphic::screenCoordinates( qreal lon, qreal lat,
                       qreal& x, qreal& y ) const {
    return false;
}

void MapGraphic::zoomOut( Marble::FlyToMode mode ) {

}

qreal MapGraphic::centerLatitude() const {
    return 0.0;
}

qreal MapGraphic::centerLongitude() const {
    return 0.0;
}

void MapGraphic::setDistance( qreal distance ) {
    // sphereRadiusInPixel = referenceCoverage / (distance / referenceDistance) * size().width() / 2;
    sphereRadiusInPixel = earthRadius / tanDefaultLensHalfHorizontalOpeningAngle * size().width() / 2 / distance;
}

void MapGraphic::setProjection( Marble::Projection projection ) {
    if(projection == Marble::Projection::Mercator) {
        paintDisplayAs = &MapGraphic::paintSphere;
    } else {
        paintDisplayAs = &MapGraphic::paintRectangle;
    }
}

bool MapGraphic::showGrid() const {
    return false;
}

void MapGraphic::setShowGrid( bool visible ) {

}

bool MapGraphic::showPlaces() const {
    return false;
}

bool MapGraphic::showCities() const {
    return false;
}

bool MapGraphic::showTerrain() const {
    return false;
}

void MapGraphic::setShowTerrain( bool visible ) {

}

bool MapGraphic::showOtherPlaces() const {
    return false;
}

bool MapGraphic::showIceLayer() const {
    return false;
}

void MapGraphic::setShowIceLayer( bool visible ) {

}

QList<Marble::AbstractFloatItem *> MapGraphic::floatItems() const {
    return QList<Marble::AbstractFloatItem *>();
}

void MapGraphic::setMapQualityForViewContext( Marble::MapQuality quality, Marble::ViewContext viewContext ) {

}

void MapGraphic::setAnimationsEnabled( bool enabled ) {

}

void MapGraphic::addLayer( Marble::LayerInterface *layer ) {

}

void MapGraphic::removeLayer( Marble::LayerInterface *layer ) {

}

void MapGraphic::setZoom( int zoom, Marble::FlyToMode mode ) {

}

int MapGraphic::minimumZoom() const {
    return 0;
}

void MapGraphic::resizeEvent( QResizeEvent *event ) {
    tanDefaultLensHalfHorizontalOpeningAngle = event->size().width() / 2 / sphereRadiusInPixel * earthRadius / distance();
}

void MapGraphic::paintEvent( QPaintEvent *event ) {
    (this->*paintDisplayAs)(event);
}

Marble::ViewContext MapGraphic::viewContext() const {
    return Marble::ViewContext();
}

void MapGraphic::setViewContext( Marble::ViewContext viewContext ) {

}

void MapGraphic::moveDown( Marble::FlyToMode mode ) {

}

void MapGraphic::moveLeft( Marble::FlyToMode mode ) {

}

void MapGraphic::moveRight( Marble::FlyToMode mode ) {

}

void MapGraphic::moveUp( Marble::FlyToMode mode ) {

}

Marble::AbstractFloatItem * MapGraphic::floatItem( const QString &nameId ) const {
    return nullptr;
}

void MapGraphic::writePluginSettings( QSettings& settings ) const {

}

QList<Marble::RenderPlugin *> MapGraphic::renderPlugins() const {
    return QList<Marble::RenderPlugin *>();
}

void MapGraphic::setPropertyValue( const QString& name, bool value ) {

}

void MapGraphic::readPluginSettings( QSettings& settings ) {

}

Marble::MarbleWidgetInputHandler *MapGraphic::inputHandler() const {
    return nullptr;
}

void MapGraphic::clearVolatileTileCache() {

}

Marble::TextureLayer *MapGraphic::textureLayer() const {
    return new Marble::TextureLayer(nullptr, nullptr, nullptr, nullptr);
}

void MapGraphic::setRadius( int radius ) {

}

void MapGraphic::zoomView( int zoom, Marble::FlyToMode mode ) {

}

void MapGraphic::rotateBy( const qreal deltaLon, const qreal deltaLat, Marble::FlyToMode mode ) {

}

void MapGraphic::centerOn( const Marble::GeoDataCoordinates &point, bool animated ) {

}

void MapGraphic::centerOn( const Marble::GeoDataPlacemark& placemark, bool animated ) {

}

void MapGraphic::setCenterLatitude( qreal lat, Marble::FlyToMode mode ) {

}

void MapGraphic::setCenterLongitude( qreal lon, Marble::FlyToMode mode ) {

}

void MapGraphic::goHome( Marble::FlyToMode mode ) {

}

void MapGraphic::flyTo( const Marble::GeoDataLookAt &lookAt, Marble::FlyToMode mode ) {

}

void MapGraphic::setProjection( int projection ) {

}

void MapGraphic::setShowOverviewMap( bool visible ) {

}

void MapGraphic::setShowScaleBar( bool visible ) {

}

void MapGraphic::setShowCompass( bool visible ) {

}

void MapGraphic::setShowCityLights( bool visible ) {

}

void MapGraphic::setLockToSubSolarPoint( bool visible ) {

}

void MapGraphic::setSubSolarPointIconVisible( bool visible ) {

}

void MapGraphic::setShowAtmosphere( bool visible ) {

}

void MapGraphic::setShowCrosshairs( bool visible ) {

}

void MapGraphic::setShowRelief( bool visible ) {

}

void MapGraphic::setShowBorders( bool visible ) {

}

void MapGraphic::setShowRivers( bool visible ) {

}

void MapGraphic::setShowLakes( bool visible ) {

}

void MapGraphic::setShowFrameRate( bool visible ) {

}

void MapGraphic::setShowBackground( bool visible ) {

}

void MapGraphic::setShowTileId( bool visible ) {

}

void MapGraphic::setShowRuntimeTrace( bool visible ) {

}

bool MapGraphic::showRuntimeTrace() const {

}

void MapGraphic::setShowDebugPolygons( bool visible) {

}

bool MapGraphic::showDebugPolygons() const {

}

void MapGraphic::creatingTilesStart( Marble::TileCreator *creator, const QString& name, const QString& description ) {

}

void MapGraphic::reloadMap() {

}

void MapGraphic::downloadRegion( QVector<Marble::TileCoordsPyramid> const & ) {

}

void MapGraphic::notifyMouseClick( int x, int y ) {

}

void MapGraphic::setSelection( const QRect& region ) {

}

void MapGraphic::setInputEnabled( bool ) {

}

void MapGraphic::connectNotify(const QMetaMethod &signal) {

}

void MapGraphic::disconnectNotify(const QMetaMethod &signal) {

}

void MapGraphic::leaveEvent( QEvent *event ) {

}

void MapGraphic::changeEvent( QEvent * event ) {

}

void MapGraphic::customPaint( Marble::GeoPainter *painter ) {

}
