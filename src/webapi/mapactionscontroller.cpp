/*****************************************************************************
* Copyright 2015-2023 Alexander Barthel alex@littlenavmap.org
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*****************************************************************************/

#include "mapactionscontroller.h"
#include "abstractlnmactionscontroller.h"
#include "common/infobuildertypes.h"
#include "common/abstractinfobuilder.h"

#include "query/mapquery.h"
#include "query/waypointtrackquery.h"
#include "mapgui/mappaintwidget.h"
#include "mapgui/mapthemehandler.h"
#include "mappainter/mappaintlayer.h"
#include "mapgui/mapwidget.h"
#include "app/navapp.h"
#include "common/mapresult.h"
#include "web/webmapcontroller.h"

#include <QDebug>
#include <QBuffer>
#include <QPixmap>

using InfoBuilderTypes::MapFeaturesData;

MapActionsController::MapActionsController(QObject *parent, bool verboseParam, AbstractInfoBuilder* infoBuilder) :
    AbstractLnmActionsController(parent, verboseParam, infoBuilder), parentWidget((QWidget *)parent) // WARNING: Uncertain cast (QWidget *) QObject
{
    qDebug() << Q_FUNC_INFO;
    init();
}

WebApiResponse MapActionsController::imageAction(WebApiRequest request){

    WebApiResponse response = getResponse();

    atools::geo::Rect rect(
        request.parameters.value("leftlon").toFloat(),
        request.parameters.value("toplat").toFloat(),
        request.parameters.value("rightlon").toFloat(),
        request.parameters.value("bottomlat").toFloat()
    );

    int detailFactor = request.parameters.value("detailfactor").toInt();

    MapPixmap map = getPixmapRect(
        request.parameters.value("width").toInt(),
        request.parameters.value("height").toInt(),
        rect,
        detailFactor
    );

    QString format = QString(request.parameters.value("format"));
    int quality = request.parameters.value("quality").toInt();

    if(map.isValid())
    {
      // ===========================================================================
      // Write pixmap as image
      QByteArray bytes;
      QBuffer buffer(&bytes);
      buffer.open(QIODevice::WriteOnly);

      if(format == QLatin1String("jpg"))
      {
          response.headers.replace("Content-Type", "image/jpg");
          map.pixmap.save(&buffer, "PNG", quality);
      }
      else if(format == QLatin1String("png"))
      {
          response.headers.replace("Content-Type", "image/png");
          map.pixmap.save(&buffer, "PNG", quality);
      }
      else
        // Should never happen
        qWarning() << Q_FUNC_INFO << "invalid format";

      // Add copyright/attributions to header
      response.headers.insert("Image-Attributions",
                              NavApp::getMapThemeHandler()->getTheme(mapPaintWidget->getCurrentThemeId()).getCopyright().toUtf8());

      response.status = 200;
      response.body = bytes;
    }
    return response;

}

WebApiResponse MapActionsController::featuresAction(WebApiRequest request){

    WebApiResponse response = getResponse();

    atools::geo::Rect rect(
        request.parameters.value("leftlon").toFloat(),
        request.parameters.value("toplat").toFloat(),
        request.parameters.value("rightlon").toFloat(),
        request.parameters.value("bottomlat").toFloat()
    );

    bool overflow = false;

    // Init dummy image request
    WebApiRequest *imageRequest = new WebApiRequest();
    imageRequest->parameters = QMap<QByteArray,QByteArray>();
    imageRequest->parameters.insert("leftlon",request.parameters.value("leftlon")),
    imageRequest->parameters.insert("toplat",request.parameters.value("toplat")),
    imageRequest->parameters.insert("rightlon",request.parameters.value("rightlon")),
    imageRequest->parameters.insert("bottomlat",request.parameters.value("bottomlat")),
    imageRequest->parameters.insert("detailfactor",request.parameters.value("detailfactor"));
    imageRequest->parameters.insert("width","300");
    imageRequest->parameters.insert("height","300");
    imageRequest->parameters.insert("format","jpg");
    imageRequest->parameters.insert("quality","1");

    // Perform dummy image request
    imageAction(*imageRequest);

    // Extract results created during dummy image request
    const QList<map::MapAirport> airports = *mapPaintWidget->getMapQuery()->getAirportsByRect(rect,mapPaintWidget->getMapPaintLayer()->getMapLayer(), false,map::NONE,overflow);

    const QList<map::MapNdb> ndbs = *mapPaintWidget->getMapQuery()->getNdbsByRect(rect,mapPaintWidget->getMapPaintLayer()->getMapLayer(), false,overflow);
    const QList<map::MapVor> vors = *mapPaintWidget->getMapQuery()->getVorsByRect(rect,mapPaintWidget->getMapPaintLayer()->getMapLayer(), false,overflow);
    const QList<map::MapMarker> markers = *mapPaintWidget->getMapQuery()->getMarkersByRect(rect,mapPaintWidget->getMapPaintLayer()->getMapLayer(), false,overflow);
    const QList<map::MapWaypoint> waypoints = mapPaintWidget->getWaypointTrackQuery()->getWaypointsByRect(rect,mapPaintWidget->getMapPaintLayer()->getMapLayer(), false,overflow);

    MapFeaturesData data = {
        airports,
        ndbs,
        vors,
        markers,
        waypoints
    };

    response.body = infoBuilder->features(data);

    return response;

}

WebApiResponse MapActionsController::featureAction(WebApiRequest request){

    WebApiResponse response = getResponse();

    int object_id = request.parameters.value("object_id").toInt();
    int type_id = request.parameters.value("type_id").toInt();

    map::MapResult result;

    switch (type_id) {
        case map::WAYPOINT:
            result.waypoints.append(mapPaintWidget->getWaypointTrackQuery()->getWaypointById(object_id));
            break;
        default:
            mapPaintWidget->getMapQuery()->getMapObjectById(result,type_id,map::AIRSPACE_SRC_NONE,object_id,false);
            break;
    }

    MapFeaturesData data = {
        result.airports,
        result.ndbs,
        result.vors,
        result.markers,
        result.waypoints
    };

    response.body = infoBuilder->feature(data);

    return response;

}

MapActionsController::~MapActionsController()
{
  qDebug() << Q_FUNC_INFO;
  deInit();
}

void MapActionsController::init()
{
  qDebug() << Q_FUNC_INFO;

  deInit();

  // Create a map widget clone with the desired resolution
  mapPaintWidget = new MapPaintWidget(parentWidget, false /* no real widget - hidden */);
  // Ensure MapPaintLayer::mapLayer initialisation
  mapPaintWidget->getMapPaintLayer()->updateLayers();

  // Activate painting
  mapPaintWidget->setActive();
}

void MapActionsController::deInit()
{
  qDebug() << Q_FUNC_INFO;

  delete mapPaintWidget;
  mapPaintWidget = nullptr;
}

MapPixmap MapActionsController::getPixmap(int width, int height)
{
  if(verbose)
    qDebug() << Q_FUNC_INFO << width << "x" << height;

  return getPixmapPosDistance(width, height, atools::geo::EMPTY_POS,
                              static_cast<float>(mapPaintWidget->distance()), QLatin1String(""));
}

MapPixmap MapActionsController::getPixmapPosDistance(int width, int height, atools::geo::Pos pos, float distanceKm,
                                                 const QString& mapCommand, const QString& errorCase)
{
  if(verbose)
    qDebug() << Q_FUNC_INFO << width << "x" << height << pos << "distanceKm" << distanceKm << "cmd" << mapCommand;

  if(!pos.isValid())
  {
    if(errorCase == QLatin1String(""))
    {
      // Use current map position
      pos.setLonX(static_cast<float>(mapPaintWidget->centerLongitude()));
      pos.setLatY(static_cast<float>(mapPaintWidget->centerLatitude()));
    }
    else
    {
      qWarning() << Q_FUNC_INFO << errorCase;
      MapPixmap mappixmap;
      mappixmap.error = errorCase;
      return mappixmap;
    }
  }

  if(mapPaintWidget != nullptr)
  {
    QMutexLocker locker(&mapPaintWidgetMutex);

    // Copy all map settings
    mapPaintWidget->copySettings(*NavApp::getMapWidgetGui());

    // Do not center world rectangle when resizing map widget
    mapPaintWidget->setKeepWorldRect(false);

    // Jump to position without zooming for sharp map
    mapPaintWidget->showPosNotAdjusted(pos, distanceKm);

    if(!mapCommand.isEmpty())
    {
      // Move or zoom map by command
      if(mapCommand == QLatin1String("left"))
        mapPaintWidget->moveLeft(Marble::Instant);
      else if(mapCommand == QLatin1String("right"))
        mapPaintWidget->moveRight(Marble::Instant);
      else if(mapCommand == QLatin1String("up"))
        mapPaintWidget->moveUp(Marble::Instant);
      else if(mapCommand == QLatin1String("down"))
        mapPaintWidget->moveDown(Marble::Instant);
      else if(mapCommand == QLatin1String("in"))
        mapPaintWidget->zoomIn(Marble::Instant);
      else if(mapCommand == QLatin1String("out"))
        mapPaintWidget->zoomOut(Marble::Instant);
      else
      {
        qWarning() << Q_FUNC_INFO << "Invalid map command" << mapCommand;
        return MapPixmap();
      }
    }

    // Jump to next sharp level
    mapPaintWidget->zoomIn(Marble::Instant);
    mapPaintWidget->zoomOut(Marble::Instant);

    MapPixmap mappixmap;

    // The actual zoom distance
    mappixmap.correctedDistanceKm = static_cast<float>(mapPaintWidget->distance());

    if(mapCommand == QLatin1String("in") || mapCommand == QLatin1String("out"))
      // Requested is equal to result when zooming
      mappixmap.requestedDistanceKm = mappixmap.correctedDistanceKm;
    else
      // What was requested
      mappixmap.requestedDistanceKm = distanceKm;

    // Fill result object
    mappixmap.pixmap = mapPaintWidget->getPixmap(width, height);
    mappixmap.pos = mapPaintWidget->getCurrentViewCenterPos();

    return mappixmap;
  }
  else
  {
    qWarning() << Q_FUNC_INFO << "mapPaintWidget is null";
    return MapPixmap();
  }
}

MapPixmap MapActionsController::getPixmapRect(int width, int height, atools::geo::Rect rect, int detailFactor, const QString& errorCase)
{
  if(verbose)
    qDebug() << Q_FUNC_INFO << width << "x" << height << rect;

  if(rect.isValid())
  {
    if(mapPaintWidget != nullptr)
    {
      QMutexLocker locker(&mapPaintWidgetMutex);

      // Copy all map settings
      mapPaintWidget->copySettings(*NavApp::getMapWidgetGui());

      // Do not center world rectangle when resizing
      mapPaintWidget->setKeepWorldRect(false);

      mapPaintWidget->showRectStreamlined(rect, false);

      // Disable dynamic/live features
      mapPaintWidget->setShowMapObject(map::AIRCRAFT_ALL, false);
      mapPaintWidget->setShowMapObject(map::AIRCRAFT_TRAIL, false);

      // Set detail factor
      mapPaintWidget->getMapPaintLayer()->setDetailLevel(detailFactor);

      // Disable copyright note
      mapPaintWidget->setPaintCopyright(false);

      MapPixmap mapPixmap;

      // No distance requested. Therefore requested is equal to actual
      mapPixmap.correctedDistanceKm = mapPixmap.requestedDistanceKm = static_cast<float>(mapPaintWidget->distance());
      mapPixmap.pixmap = mapPaintWidget->getPixmap(width, height);
      mapPixmap.pos = mapPaintWidget->getCurrentViewCenterPos();

      return mapPixmap;
    }
    else
    {
      qWarning() << Q_FUNC_INFO << "mapPaintWidget is null";
      return MapPixmap();
    }
  }
  else
  {
    qWarning() << Q_FUNC_INFO << errorCase;
    MapPixmap mapPixmap;
    mapPixmap.error = errorCase;
    return mapPixmap;
  }
}
