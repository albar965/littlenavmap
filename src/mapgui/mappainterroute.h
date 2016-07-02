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

#ifndef LITTLENAVMAP_MAPPAINTERROUTE_H
#define LITTLENAVMAP_MAPPAINTERROUTE_H

#include "mapgui/mappainter.h"

namespace Marble {
class GeoDataLineString;
}

class MapWidget;
class RouteController;

class MapPainterRoute :
  public MapPainter
{
public:
  MapPainterRoute(MapWidget *mapWidget, MapQuery *mapQuery, MapScale *mapScale,
                  RouteController *controller, bool verbose);
  virtual ~MapPainterRoute();

  virtual void render(const PaintContext *context) override;

private:
  RouteController *routeController;

  void paintRoute(const PaintContext* context);

  void paintAirport(const PaintContext* context, int x, int y,
                    const maptypes::MapAirport& obj);
  void paintVor(const PaintContext* context, int x, int y,
                const maptypes::MapVor& obj);
  void paintNdb(const PaintContext* context, int x, int y,
                const maptypes::MapNdb& obj);
  void paintWaypoint(const PaintContext* context, const QColor& col, int x, int y,
                     const maptypes::MapWaypoint& obj);

  void paintWaypointText(const PaintContext* context, int x, int y,
                         const maptypes::MapWaypoint& obj);
  void paintNdbText(const PaintContext* context, int x, int y,
                    const maptypes::MapNdb& obj);
  void paintVorText(const PaintContext* context, int x, int y,
                    const maptypes::MapVor& obj);

  void paintAirportText(const PaintContext* context, int x, int y,
                        const maptypes::MapAirport& obj);

  void paintText(const PaintContext* context, const QColor& color, int x, int y,
                 const QString& text);

  void paintUserpoint(const PaintContext* context, int x, int y);

};

#endif // LITTLENAVMAP_MAPPAINTERROUTE_H
