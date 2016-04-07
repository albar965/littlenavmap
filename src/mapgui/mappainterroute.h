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

#ifndef MAPPAINTERROUTE_H
#define MAPPAINTERROUTE_H

#include "mappainter.h"

namespace Marble {
class GeoDataLineString;
}

class NavMapWidget;
class RouteController;

class MapPainterRoute :
  public MapPainter
{
public:
  MapPainterRoute(NavMapWidget *widget, MapQuery *mapQuery, MapScale *mapScale,
                  RouteController *controller, bool verbose);
  virtual ~MapPainterRoute();

  virtual void render(const PaintContext *context) override;

private:
  RouteController *routeController;
  NavMapWidget *navMapWidget;
  void paintRoute(const MapLayer *mapLayer, Marble::GeoPainter *painter, bool fast);

  void paintAirport(const MapLayer *mapLayer, Marble::GeoPainter *painter, int x, int y,
                    const maptypes::MapAirport& obj);
  void paintVor(const MapLayer *mapLayer, Marble::GeoPainter *painter, int x, int y,
                const maptypes::MapVor& obj);
  void paintNdb(const MapLayer *mapLayer, Marble::GeoPainter *painter, int x, int y,
                const maptypes::MapNdb& obj);
  void paintWaypoint(const MapLayer *mapLayer, Marble::GeoPainter *painter, const QColor& col, int x, int y,
                     const maptypes::MapWaypoint& obj);

  void paintWaypointText(const MapLayer *mapLayer, Marble::GeoPainter *painter, int x, int y,
                         const maptypes::MapWaypoint& obj);
  void paintNdbText(const MapLayer *mapLayer, Marble::GeoPainter *painter, int x, int y,
                    const maptypes::MapNdb& obj);
  void paintVorText(const MapLayer *mapLayer, Marble::GeoPainter *painter, int x, int y,
                    const maptypes::MapVor& obj);

  void paintAirportText(const MapLayer *mapLayer, Marble::GeoPainter *painter, int x, int y,
                        const maptypes::MapAirport& obj);

  void paintText(const MapLayer *mapLayer, Marble::GeoPainter *painter, const QColor& color, int x, int y,
                 const QString& text);

  void paintUserpoint(const MapLayer *mapLayer, Marble::GeoPainter *painter, int x, int y);

};

#endif // MAPPAINTERROUTE_H
