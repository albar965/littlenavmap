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

MapPainterNav::MapPainterNav(Marble::MarbleWidget *widget, MapQuery *mapQuery, MapScale *mapScale,
                             bool verboseMsg)
  : MapPainter(widget, mapQuery, mapScale, verboseMsg)
{
}

MapPainterNav::~MapPainterNav()
{
}

void MapPainterNav::paint(const MapLayer *mapLayer, Marble::GeoPainter *painter,
                          Marble::ViewportParams *viewport, maptypes::MapObjectTypes objectTypes)
{
  using namespace maptypes;

  if(mapLayer == nullptr)
    return;

  bool drawFast = widget->viewContext() == Marble::Animation;

  const GeoDataLatLonAltBox& curBox = viewport->viewLatLonAltBox();
  QElapsedTimer t;
  t.start();

  setRenderHints(painter);

  if(mapLayer->isWaypoint() && objectTypes.testFlag(maptypes::WAYPOINT))
  {
  const QList<MapWaypoint> *waypoints = query->getWaypoints(curBox, mapLayer, drawFast);
  if(waypoints != nullptr)
  {
    if(widget->viewContext() == Marble::Still && verbose)
    {
      qDebug() << "Number of waypoints" << waypoints->size();
      qDebug() << "Time for query" << t.elapsed() << " ms";
      qDebug() << curBox.toString();
      qDebug() << *mapLayer;
      t.restart();
    }

    for(const MapWaypoint& waypoint : *waypoints)
    {
      int x, y;
      bool visible = wToS(waypoint.position, x, y);

      if(visible)
        symbolPainter->drawWaypointSymbol(painter, waypoint, x, y,
                                          mapLayer->getWaypointSymbolSize(), false, drawFast);

      if(mapLayer->isWaypointName())
        symbolPainter->drawWaypointText(painter, waypoint, x, y, textflags::IDENT,
                                        mapLayer->getWaypointSymbolSize(),
                                        false, drawFast);
    }
  }
  }

  if(mapLayer->isVor() && objectTypes.testFlag(maptypes::VOR))
  {
    const QList<MapVor> *vors = query->getVors(curBox, mapLayer, drawFast);
    if(vors != nullptr)
    {
      if(widget->viewContext() == Marble::Still && verbose)
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
        bool visible = wToS(vor.position, x, y);

        if(visible)
        {
          symbolPainter->drawVorSymbol(painter, vor, x, y, mapLayer->getVorSymbolSize(), false, drawFast,
                                       mapLayer->isVorLarge() ? mapLayer->getVorSymbolSize() * 5 : 0);

          textflags::TextFlags flags;

          if(mapLayer->isVorInfo())
            flags = textflags::IDENT | textflags::TYPE | textflags::FREQ;
          else if(mapLayer->isVorIdent())
            flags = textflags::IDENT;

          symbolPainter->drawVorText(painter, vor, x, y,
                                     flags, mapLayer->getVorSymbolSize(), false, drawFast);
        }
      }
    }
  }

  if(mapLayer->isNdb() && objectTypes.testFlag(maptypes::NDB))
  {
    const QList<MapNdb> *ndbs = query->getNdbs(curBox, mapLayer, drawFast);
    if(ndbs != nullptr)
    {
      if(widget->viewContext() == Marble::Still && verbose)
      {
        qDebug() << "Number of ndbs" << ndbs->size();
        qDebug() << "Time for query" << t.elapsed() << " ms";
        qDebug() << curBox.toString();
        qDebug() << *mapLayer;
        t.restart();
      }

      for(const MapNdb& ndb : *ndbs)
      {
        int x, y;
        bool visible = wToS(ndb.position, x, y);

        if(visible)
        {
          symbolPainter->drawNdbSymbol(painter, ndb, x, y, mapLayer->getNdbSymbolSize(), false, drawFast);

          textflags::TextFlags flags;

          if(mapLayer->isNdbInfo())
            flags = textflags::IDENT | textflags::TYPE | textflags::FREQ;
          else if(mapLayer->isNdbIdent())
            flags = textflags::IDENT;

          symbolPainter->drawNdbText(painter, ndb, x, y,
                                     flags, mapLayer->getNdbSymbolSize(), false, drawFast);
        }
      }
    }
  }

  if(mapLayer->isMarker() && objectTypes.testFlag(maptypes::ILS))
  {
    const QList<MapMarker> *markers = query->getMarkers(curBox, mapLayer, drawFast);
    if(markers != nullptr)
    {
      if(widget->viewContext() == Marble::Still && verbose)
      {
        qDebug() << "Number of marker" << markers->size();
        qDebug() << "Time for query" << t.elapsed() << " ms";
        qDebug() << curBox.toString();
        qDebug() << *mapLayer;
        t.restart();
      }

      for(const MapMarker& marker : *markers)
      {
        int x, y;
        bool visible = wToS(marker.position, x, y);

        if(visible)
        {
          symbolPainter->drawMarkerSymbol(painter, marker, x, y, mapLayer->getNdbSymbolSize(), drawFast);

          if(mapLayer->isMarkerInfo())
          {
            QString type = marker.type.toLower();
            type[0] = type.at(0).toUpper();
            x -= mapLayer->getMarkerSymbolSize() / 2 + 2;
            symbolPainter->textBox(painter, {type}, mapcolors::markerSymbolColor, x, y,
                                   textatt::BOLD | textatt::RIGHT, 0);
          }
        }
      }
    }
  }

  if(widget->viewContext() == Marble::Still && verbose)
    qDebug() << "Time for paint" << t.elapsed() << " ms";
}
