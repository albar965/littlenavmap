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

#include "web/webmapcontroller.h"

#include "mapgui/mappaintwidget.h"
#include "mapgui/mapwidget.h"
#include "app/navapp.h"

#include <QDebug>
#include <QPixmap>

WebMapController::WebMapController(QWidget *parent, bool verboseParam)
  : QObject(parent), parentWidget(parent), verbose(verboseParam)
{
  qDebug() << Q_FUNC_INFO;
}

WebMapController::~WebMapController()
{
  qDebug() << Q_FUNC_INFO;
  deInit();
}

void WebMapController::init()
{
  qDebug() << Q_FUNC_INFO;

  deInit();

  // Create a map widget clone with the desired resolution
  mapPaintWidget = new MapPaintWidget(parentWidget, false /* no real widget - hidden */);

  // Activate painting
  mapPaintWidget->setActive();
}

void WebMapController::deInit()
{
  qDebug() << Q_FUNC_INFO;

  delete mapPaintWidget;
  mapPaintWidget = nullptr;
}

MapPixmap WebMapController::getPixmap(int width, int height)
{
  if(verbose)
    qDebug() << Q_FUNC_INFO << width << "x" << height;

  return getPixmapPosDistance(width, height, atools::geo::EMPTY_POS,
                              static_cast<float>(NavApp::getMapWidgetGui()->distance()), QLatin1String(""));
}

MapPixmap WebMapController::getPixmapObject(int width, int height, web::ObjectType type, const QString& ident,
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

MapPixmap WebMapController::getPixmapPosDistance(int width, int height, atools::geo::Pos pos, float distanceKm,
                                                 const QString& mapCommand, const QString& errorCase)
{
  if(verbose)
    qDebug() << Q_FUNC_INFO << width << "x" << height << pos << "distanceKm" << distanceKm << "cmd" << mapCommand;

  if(!pos.isValid())
  {
    if(errorCase == QLatin1String(""))
    {
      // Use current map position
      pos.setLonX(static_cast<float>(NavApp::getMapWidgetGui()->centerLongitude()));
      pos.setLatY(static_cast<float>(NavApp::getMapWidgetGui()->centerLatitude()));
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

MapPixmap WebMapController::getPixmapRect(int width, int height, atools::geo::Rect rect, const QString& errorCase)
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

      mapPaintWidget->showRectStreamlined(rect);

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

MapPaintWidget *WebMapController::getMapPaintWidget() const
{
  return mapPaintWidget;
}

void WebMapController::preDatabaseLoad()
{
  if(mapPaintWidget != nullptr)
    mapPaintWidget->preDatabaseLoad();
}

void WebMapController::postDatabaseLoad()
{
  if(mapPaintWidget != nullptr)
    mapPaintWidget->postDatabaseLoad();
}
