/*****************************************************************************
* Copyright 2015-2025 Alexander Barthel alex@littlenavmap.org
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
#include "atools.h"
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

MapActionsController::MapActionsController(QObject *parent, bool verboseParam, AbstractInfoBuilder *infoBuilderParam)
  : AbstractLnmActionsController(parent, verboseParam, infoBuilderParam)
{
  qDebug() << Q_FUNC_INFO;
  init();
}

WebApiResponse MapActionsController::imageAction(WebApiRequest request)
{
  WebApiResponse response = getResponse();

  atools::geo::Rect rect(
    request.parameters.value("leftlon").toFloat(),
    request.parameters.value("toplat").toFloat(),
    request.parameters.value("rightlon").toFloat(),
    request.parameters.value("bottomlat").toFloat()
    );

  int detailFactor = request.parameters.value("detailfactor").toInt();
  MapPixmap map = getPixmapRect(request.parameters.value("width").toInt(),
                                // The dynamic map does not work when returning exact 256 height as requested
                                // Workaround needed since getPixmapRect() now returns the exact size as requested
                                300 /*request.parameters.value("height").toInt()*/,
                                rect, detailFactor,
                                QString(), false /* ignoreUiScale */);

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

WebApiResponse MapActionsController::featuresAction(WebApiRequest request)
{
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
  imageRequest->parameters = QMap<QByteArray, QByteArray>();
  imageRequest->parameters.insert("leftlon", request.parameters.value("leftlon"));
  imageRequest->parameters.insert("toplat", request.parameters.value("toplat"));
  imageRequest->parameters.insert("rightlon", request.parameters.value("rightlon"));
  imageRequest->parameters.insert("bottomlat", request.parameters.value("bottomlat"));
  imageRequest->parameters.insert("detailfactor", request.parameters.value("detailfactor"));
  imageRequest->parameters.insert("width", "300");
  imageRequest->parameters.insert("height", "300");
  imageRequest->parameters.insert("format", "jpg");
  imageRequest->parameters.insert("quality", "1");

  // Perform dummy image request
  imageAction(*imageRequest);

  // Extract results created during dummy image request
  QList<map::MapAirport> airports;
  QList<map::MapNdb> ndbs;
  QList<map::MapVor> vors;
  QList<map::MapMarker> markers;
  QList<map::MapWaypoint> waypoints;

  Queries *queries = mapPaintWidget->getQueries();
  {
    QueryLocker locker(queries);
    airports = *queries->getMapQuery()->getAirportsByRect(rect, mapPaintWidget->getMapPaintLayer()->getMapLayer(),
                                                          false, map::NONE, overflow);
  }

  {
    QueryLocker locker(queries);
    ndbs = *queries->getMapQuery()->getNdbsByRect(rect, mapPaintWidget->getMapPaintLayer()->getMapLayer(), false, overflow);
  }

  {
    QueryLocker locker(queries);
    vors = *queries->getMapQuery()->getVorsByRect(rect, mapPaintWidget->getMapPaintLayer()->getMapLayer(), false, overflow);
  }

  {
    QueryLocker locker(queries);
    markers = *queries->getMapQuery()->getMarkersByRect(rect, mapPaintWidget->getMapPaintLayer()->getMapLayer(), false, overflow);
  }

  {
    QueryLocker locker(queries);
    waypoints = queries->getWaypointTrackQuery()->getWaypointsByRect(rect, mapPaintWidget->getMapPaintLayer()->getMapLayer(),
                                                                     false, overflow);
  }

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

WebApiResponse MapActionsController::featureAction(WebApiRequest request)
{
  WebApiResponse response = getResponse();

  int object_id = request.parameters.value("object_id").toInt();
  int type_id = request.parameters.value("type_id").toInt();

  map::MapResult result;

  {
    Queries *queries = mapPaintWidget->getQueries();
    QueryLocker locker(queries);
    switch(type_id)
    {
      case map::WAYPOINT:
        result.waypoints.append(queries->getWaypointTrackQuery()->getWaypointById(object_id));
        break;

      default:
        queries->getMapQuery()->getMapObjectById(result, type_id, map::AIRSPACE_SRC_NONE, object_id, false);
        break;
    }
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

  // Create a map widget clone with the desired resolution
  if(mapPaintWidget == nullptr)
    mapPaintWidget = new MapPaintWidget(dynamic_cast<QWidget *>(parent()), QueryManager::instance()->getQueriesWeb(),
                                        false /* no real widget - hidden */, true /* web */);

  // Copy all map settings except trail
  mapPaintWidget->copySettings(*NavApp::getMapWidgetGui(), false /* deep */);

  // Ensure MapPaintLayer::mapLayer initialisation
  mapPaintWidget->getMapPaintLayer()->updateLayers();

  // Activate painting
  mapPaintWidget->setActive();
}

void MapActionsController::deInit()
{
  // Close queries to allow closing the databases
  if(mapPaintWidget != nullptr)
    mapPaintWidget->preDatabaseLoad();

  ATOOLS_DELETE_LOG(mapPaintWidget);
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
      // Use current map position
      pos = NavApp::getMapWidgetGui()->getCenterPos();
    else
    {
      if(verbose)
        qWarning() << Q_FUNC_INFO << errorCase;
      MapPixmap mappixmap;
      mappixmap.error = errorCase;
      return mappixmap;
    }
  }

  if(mapPaintWidget != nullptr)
  {
    // Lock whole widget
    MapPaintWidgetLocker locker(mapPaintWidget);
    QueryLocker queryLocker(mapPaintWidget->getQueries());

    // Copy all map settings except trail
    mapPaintWidget->copySettings(*NavApp::getMapWidgetGui(), false /* deep */);

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
        if(verbose)
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
    mappixmap.pos = mapPaintWidget->getCenterPos();

    return mappixmap;
  }
  else
  {
    if(verbose)
      qWarning() << Q_FUNC_INFO << "mapPaintWidget is null";
    return MapPixmap();
  }
}

MapPixmap MapActionsController::getPixmapRect(int width, int height, atools::geo::Rect rect, int detailFactor, const QString& errorCase,
                                              bool ignoreUiScale)
{
  if(verbose)
    qDebug() << Q_FUNC_INFO << width << "x" << height << rect;

  if(rect.isValid())
  {
    if(mapPaintWidget != nullptr)
    {
      MapPaintWidgetLocker locker(mapPaintWidget);
      QueryLocker queryLocker(mapPaintWidget->getQueries());

      // Copy all map settings except trail
      mapPaintWidget->copySettings(*NavApp::getMapWidgetGui(), false /* deep */);

      mapPaintWidget->showRectStreamlined(rect, false);

      // Disable dynamic/live features
      mapPaintWidget->setShowMapObject(map::AIRCRAFT_ALL, false);
      mapPaintWidget->setShowMapObject(map::AIRCRAFT_TRAIL, false);

      // Set detail factor
      mapPaintWidget->getMapPaintLayer()->setDetailLevel(detailFactor, detailFactor);

      // Disable copyright note and wind
      mapPaintWidget->setPaintCopyright(false);
      mapPaintWidget->setPaintWindHeader(false);

      MapPixmap mapPixmap;

      // No distance requested. Therefore requested is equal to actual
      mapPixmap.correctedDistanceKm = mapPixmap.requestedDistanceKm = static_cast<float>(mapPaintWidget->distance());
      mapPixmap.pixmap = mapPaintWidget->getPixmap(width, height, ignoreUiScale);
      mapPixmap.pos = mapPaintWidget->getCenterPos();

      return mapPixmap;
    }
    else
    {
      if(verbose)
        qWarning() << Q_FUNC_INFO << "mapPaintWidget is null";
      return MapPixmap();
    }
  }
  else
  {
    if(verbose)
      qWarning() << Q_FUNC_INFO << errorCase;
    MapPixmap mapPixmap;
    mapPixmap.error = errorCase;
    return mapPixmap;
  }
}
