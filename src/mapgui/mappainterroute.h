/*****************************************************************************
* Copyright 2015-2017 Alexander Barthel albar965@mailbox.org
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
class RouteMapObjectList;

/*
 * Draws the flight plan line and all enroute navaid and departure and destination airports (airport symbols only).
 * Airport diagrams and route overview are drawn by the airport painter.
 */
class MapPainterRoute :
  public MapPainter
{
  Q_DECLARE_TR_FUNCTIONS(MapPainter)

public:
  MapPainterRoute(MapWidget *mapWidget, MapQuery *mapQuery, MapScale *mapScale, RouteController *controller);
  virtual ~MapPainterRoute();

  virtual void render(PaintContext *context) override;

private:
  RouteController *routeController;

  void paintRoute(const PaintContext *context);

  void paintAirport(const PaintContext *context, int x, int y, const maptypes::MapAirport& obj);
  void paintVor(const PaintContext *context, int x, int y, const maptypes::MapVor& obj);
  void paintNdb(const PaintContext *context, int x, int y);
  void paintWaypoint(const PaintContext *context, const QColor& col, int x, int y);
  void paintApproachPreview(const PaintContext *context, const maptypes::MapApproachLegList& transition,
                            const maptypes::MapApproachLegList& approach);
  void paintWaypointText(const PaintContext *context, int x, int y, const maptypes::MapWaypoint& obj,
                         const QStringList *addtionalText = nullptr);
  void paintNdbText(const PaintContext *context, int x, int y, const maptypes::MapNdb& obj,
                    const QStringList *addtionalText = nullptr);
  void paintVorText(const PaintContext *context, int x, int y, const maptypes::MapVor& obj,
                    const QStringList *addtionalText = nullptr);
  void paintAirportText(const PaintContext *context, int x, int y, const maptypes::MapAirport& obj);
  void paintText(const PaintContext *context, const QColor& color, int x, int y, const QStringList& texts);
  void paintUserpoint(const PaintContext *context, int x, int y);

  static Q_DECL_CONSTEXPR int MIN_LENGTH_FOR_TEXT = 80;
  void drawSymbols(const PaintContext *context, const RouteMapObjectList& routeMapObjects,
                   const QBitArray& visibleStartPoints, const QList<QPoint>& startPoints);

  void drawSymbolText(const PaintContext *context, const RouteMapObjectList& routeMapObjects,
                      const QBitArray& visibleStartPoints, const QList<QPoint>& startPoints);

  void paintApproachPoint(const PaintContext *context, const maptypes::MapApproachLeg& leg);

  void paintApproachSegment(const PaintContext *context, const maptypes::MapApproachLegList& legs,
                            const maptypes::MapApproachLegList *nextLegs,
                            int index);

  void paintArc(QPainter *painter, float x1, float y1, float x2, float y2, float x0, float y0, bool left);
  void paintArc(QPainter *painter, const QPoint& p1, const QPoint& p2, const QPoint& p0, bool left);
  void paintArc(QPainter *painter, const QPointF& p1, const QPointF& p2, const QPointF& p0, bool left);

};

#endif // LITTLENAVMAP_MAPPAINTERROUTE_H
