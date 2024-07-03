#include "mapgraphicthread.h"

MapGraphicThread::MapGraphicThread(MapGraphic::threadData* tData)
    : QThread{}
{
    this->tData = tData;
}

MapGraphicThread::lonLat MapGraphicThread::view2LonLatFrom(int x, int y, int xOrigin, int yOrigin, float radiusSq, float spin, float tilt) {
    int xC = x - xOrigin;
    int yC = y - yOrigin;
    float r2Sq = radiusSq - xC * xC;
    if (yC * yC <= r2Sq)
    {
        if (r2Sq > 0.0F)
        {
            float r2Sqrt = sqrtf(r2Sq);
            float yC2 = yC / r2Sqrt;
            float cAT = cosf(tilt);
            float ySpace = r2Sqrt * (yC2 * cAT - copysignf(sqrtf((1.0F - yC2 * yC2) * (1.0F - cAT * cAT)), tilt));                     // == r2Sqrt * sinf(asinf(yC / r2Sqrt) - tilt);
            float r2ScAT = r2Sqrt * cAT;
            float r3Sqr = radiusSq - ySpace * ySpace;
            lonLat value;
            value.longitude = fmodf(spin + (((tilt > 0.0F && -yC > r2ScAT) || (tilt < 0.0F && yC > r2ScAT)) ? -1.0F : 1.0F) * (r3Sqr > 0.0F ? sqrtf(r3Sqr) > abs(xC) ? acosf(-xC / sqrtf(r3Sqr)) : (PI_By_2 + copysignf(PI_By_2, xC)) : PI_By_2) + PI_Times_2, PI_Times_2);
            value.latitude = asinf(ySpace / sqrtf(radiusSq));
            return value;
        }
        else
        {
            lonLat value;
            value.longitude = fmodf(spin + (xC > 0 ? PI : 0.0F) + PI_Times_2, PI_Times_2);
            value.latitude = 0.0F;
            return value;
        }
    }
    lonLat value;
    value.longitude = 0.0F;
    value.latitude = 2.0F;
    return value;
}

float MapGraphicThread::stretchWebMercator(float t) {
    return logf(tanf(t/2.0f + PI/4.0f));
}

MapGraphicThread::tilesXY MapGraphicThread::lonLat2TilesXYFrom(lonLat angles, int zoom) {
    tilesXY xy;
    float amount = powf(2, zoom);
    float xTile = angles.longitude * amount / PI_Times_2;
    angles.latitude = stretchWebMercator(angles.latitude);
    angles.latitude = fabsf(angles.latitude) < cutoffLatitude ? angles.latitude : copysignf(cutoffLatitude, angles.latitude);
    float yTile = (angles.latitude - -cutoffLatitude - .0001F) * amount / (2.0F * cutoffLatitude);            // - .0001 = prevent exact 1 as result (values in image are [0,1), [ = including, ) = excluding )
    xy.x = (int)xTile;                                                 // (int) floors towards 0
    xy.y = (int)yTile;
    int length = sprintf_s(xy.id, 25, idFormat, zoom, xy.x, xy.y);
    if (length == 0) {
        strcpy(xy.id, "0/0/0");
        xTile = angles.longitude / PI_Times_2;
        if (xTile < 0.0F) {
            xTile = 0.0F;
        }
        else if (xTile >= 1.0F) {
            xTile = .9999F;
        }
        yTile = (angles.longitude - -cutoffLatitude - .0001F) / (2.0F * cutoffLatitude);
        if (yTile < 0.0F) {
            yTile = 0.0F;
        }
        else if (yTile >= 1.0F) {
            yTile = .9999F;
        }
        xy.x = 0;
        xy.y = 0;
    }
    xy.xt = (int)((xTile - xy.x) * tData->tileWidth);
    xy.yt = (int)((yTile - xy.y) * tData->tileHeight);
    return xy;
}

void MapGraphicThread::run() {
    tData->image = new QImage(tData->width, tData->height, QImage::Format_RGB32);
    if(tData->image != nullptr) {
        QPainter painter(tData->image);
        QHash<QString, QList<queuedPixel>> pixelQueue;
        int xStart = tData->xStart;
        int xEnd = tData->xEnd;
        int yStart = tData->yStart;
        int yEnd = tData->yEnd;
        for (int y = yStart; y < yEnd; ++y) {
            for (int x = xStart; x < xEnd; ++x) {
                lonLat angles = view2LonLatFrom(x, y, tData->xOrigin, tData->yOrigin, tData->radius * tData->radius, tData->spin, tData->tilt);
                if (angles.latitude != 2.0F) {
                    tilesXY xy = lonLat2TilesXYFrom(angles, tData->zoom);
                    if (tData->currentTiles->contains(xy.id))
                    {
                        painter.setPen(tData->currentTiles->value(xy.id).pixelColor(xy.xt, xy.yt));
                        painter.drawPoint(x - xStart, y - yStart);
                        continue;
                    }
                    else
                    {
                        bool notPainted = true;
                        if (tData->tryZoomedOut) {
                            tilesXY xy = lonLat2TilesXYFrom(angles, tData->zoom - 1);
                            if (tData->currentTiles->contains(xy.id))
                            {
                                painter.setPen(tData->currentTiles->value(xy.id).pixelColor(xy.xt, xy.yt));
                                painter.drawPoint(x - xStart, y - yStart);
                                notPainted = false;
                            }
                        }
                        if (notPainted) {
                            painter.setPen(Qt::GlobalColor::black);
                            painter.drawPoint(x - xStart, y - yStart);
                        }

                        if(!pixelQueue.contains(QString(xy.id))) {
                            qDebug() << "MGO: about to add missing id";
                            tData->idsMissing.push_back(QString(xy.id));
                            qDebug() << "MGO: missing id added";
                        }
                        pixelQueue[QString(xy.id)].append(queuedPixel{
                            .xv = x,
                            .yv = y,
                            .xt = xy.xt,
                            .yt = xy.yt
                        });
                    }
                }
                else {
                    painter.setPen(Qt::GlobalColor::black);
                    painter.drawPoint(x - xStart, y - yStart);
                }
            }
        }
        qDebug() << "MGO: image iterated through";
        while(pixelQueue.count() > tData->indexLastIdCompleted) {
            volatile int count = tData->idsDelivered.size();
            qDebug() << "MGO: waiting for tiles";
            auto it = tData->idsDelivered.begin();
            std::advance(it, tData->indexLastIdCompleted);
            for(; tData->indexLastIdCompleted < count; ++tData->indexLastIdCompleted) {
                QString id = *it++;
                qDebug() << "MGO: missed tile about to be used " << id;
                foreach(queuedPixel qp, pixelQueue[id]) {
                    painter.setPen(tData->currentTiles->value(id).pixelColor(qp.xt, qp.yt));
                    painter.drawPoint(qp.xv - xStart, qp.yv - yStart);
                }
                qDebug() << "MGO: missed tile used";
            }
        }
    }
    tData->rastering = false;
}
