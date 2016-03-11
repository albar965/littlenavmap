/*****************************************************************************
* Copyright 2015-2016 Alexander Barthel albar965@mailbox.org
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

#include "mappainternav.h"
#include "symbolpainter.h"

#include "mapgui/mapscale.h"
#include "mapgui/maplayer.h"
#include "mapgui/mapquery.h"
#include "geo/calculations.h"
#include "common/maptypes.h"
#include "common/mapcolors.h"

#include <QElapsedTimer>

#include <marble/GeoPainter.h>
#include <marble/ViewportParams.h>

using namespace Marble;
using namespace atools::geo;

MapPainterNav::MapPainterNav(Marble::MarbleWidget *widget, MapQuery *mapQuery, MapScale *mapScale)
  : MapPainter(widget, mapQuery, mapScale)
{
  symbolPainter = new SymbolPainter();
}

MapPainterNav::~MapPainterNav()
{
  delete symbolPainter;
}

void MapPainterNav::paint(const MapLayer *mapLayer, Marble::GeoPainter *painter,
                          Marble::ViewportParams *viewport)
{
  if(mapLayer == nullptr)
    return;

  bool drawFast = widget->viewContext() == Marble::Animation;

  const GeoDataLatLonAltBox& curBox = viewport->viewLatLonAltBox();
  QElapsedTimer t;
  t.start();

  setRenderHints(painter);

  const QList<MapVor> *vors = query->getVors(curBox, mapLayer, drawFast);
  if(vors != nullptr)
  {
    if(widget->viewContext() == Marble::Still)
    {
      qDebug() << "Number of vors" << vors->size();
      qDebug() << "Time for query" << t.elapsed() << " ms";
      qDebug() << curBox.toString();
      qDebug() << *mapLayer;
      t.restart();
    }

    for(const MapVor& vor : *vors)
    {
      int x, y;
      bool visible = wToS(vor.pos, x, y);

      if(visible)
        if(mapLayer->isVor())
          symbolPainter->drawVorSymbol(painter, vor, x, y, mapLayer->getVorSymbolSize(), drawFast);
    }
  }

  const QList<MapNdb> *ndbs = query->getNdbs(curBox, mapLayer, drawFast);
  if(ndbs != nullptr)
  {
    if(widget->viewContext() == Marble::Still)
    {
      qDebug() << "Number of ndbs" << vors->size();
      qDebug() << "Time for query" << t.elapsed() << " ms";
      qDebug() << curBox.toString();
      qDebug() << *mapLayer;
      t.restart();
    }

    for(const MapNdb& ndb : *ndbs)
    {
      int x, y;
      bool visible = wToS(ndb.pos, x, y);

      if(visible)
        if(mapLayer->isNdb())
          symbolPainter->drawNdbSymbol(painter, ndb, x, y, mapLayer->getNdbSymbolSize(), drawFast);
    }
  }

  const QList<MapWaypoint> *waypoints = query->getWaypoints(curBox, mapLayer, drawFast);
  if(waypoints != nullptr)
  {
    if(widget->viewContext() == Marble::Still)
    {
      qDebug() << "Number of waypoints" << vors->size();
      qDebug() << "Time for query" << t.elapsed() << " ms";
      qDebug() << curBox.toString();
      qDebug() << *mapLayer;
      t.restart();
    }

    for(const MapWaypoint& waypoint : *waypoints)
    {
      int x, y;
      bool visible = wToS(waypoint.pos, x, y);

      if(visible)
        if(mapLayer->isWaypoint())
          symbolPainter->drawWaypointSymbol(painter, waypoint, x, y,
                                            mapLayer->getWaypointSymbolSize(), drawFast);
    }
  }

  if(widget->viewContext() == Marble::Still)
    qDebug() << "Time for paint" << t.elapsed() << " ms";
}
