/*****************************************************************************
* Copyright 2015-2020 Alexander Barthel alex@littlenavmap.org
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

#include <navapp.h>

#include "mapgui/mappaintwidget.h"
#include "mapgui/mapwidget.h"
#include "navapp.h"

#include <QDebug>
#include <QBuffer>
#include <QPixmap>

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

    MapPixmap map = getPixmapRect(
        request.parameters.value("width").toInt(),
        request.parameters.value("height").toInt(),
        rect
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
      response.headers.insert("Image-Attributions", mapPaintWidget->getMapCopyright().toUtf8());

      response.status = 200;
      response.body = bytes;
    }
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
                              static_cast<float>(NavApp::getMapWidget()->distance()), QLatin1String(""));
}

MapPixmap MapActionsController::getPixmapObject(int width, int height, web::ObjectType type, const QString& ident,
                                            float distanceKm)
{
  if(verbose)
    qDebug() << Q_FUNC_INFO << width << "x" << height << "type" << type << "ident" << ident << "distanceKm" <<
      distanceKm;

  MapPixmap mapPixmap;
  switch(type)
  {
    case web::USER_AIRCRAFT: {
      mapPixmap = getPixmapPosDistance(width, height, NavApp::getUserAircraftPos(), distanceKm, QLatin1String(""), tr("No user aircraft"));
      break;
    }

    case web::ROUTE: {
      mapPixmap = getPixmapRect(width, height, NavApp::getRouteRect(), tr("No flight plan"));
      break;
    }

    case web::AIRPORT: {
      mapPixmap = getPixmapPosDistance(width, height, NavApp::getAirportPos(ident), distanceKm, QLatin1String(""), tr("Airport %1 not found").arg(ident));
      break;
    }
  }
  return mapPixmap;
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
      pos.setLonX(static_cast<float>(NavApp::getMapWidget()->centerLongitude()));
      pos.setLatY(static_cast<float>(NavApp::getMapWidget()->centerLatitude()));
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
    // Copy all map settings
    mapPaintWidget->copySettings(*NavApp::getMapWidget());

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

MapPixmap MapActionsController::getPixmapRect(int width, int height, atools::geo::Rect rect, const QString& errorCase)
{
  if(verbose)
    qDebug() << Q_FUNC_INFO << width << "x" << height << rect;

  if(rect.isValid())
  {
    if(mapPaintWidget != nullptr)
    {
      // Copy all map settings
      mapPaintWidget->copySettings(*NavApp::getMapWidget());

      // Do not center world rectangle when resizing
      mapPaintWidget->setKeepWorldRect(false);

      mapPaintWidget->showRectStreamlined(rect, false);

      // Disable dynamic/live features
      mapPaintWidget->setShowMapFeatures(map::AIRCRAFT_ALL,false);
      mapPaintWidget->setShowMapFeatures(map::AIRCRAFT_TRACK,false);

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
