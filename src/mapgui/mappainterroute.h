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

  struct DrawText
  {
    atools::geo::Line line;
    bool distance, course;
  };

  void paintRoute(const PaintContext *context);

  void paintAirport(const PaintContext *context, int x, int y, const maptypes::MapAirport& obj);
  void paintVor(const PaintContext *context, int x, int y, const maptypes::MapVor& obj);
  void paintNdb(const PaintContext *context, int x, int y);
  void paintWaypoint(const PaintContext *context, const QColor& col, int x, int y);
  void paintApproachPreview(const PaintContext *context,
                            const maptypes::MapApproachLegs& legs);
  void paintWaypointText(const PaintContext *context, int x, int y, const maptypes::MapWaypoint& obj,
                         const QStringList *addtionalText = nullptr);
  void paintNdbText(const PaintContext *context, int x, int y, const maptypes::MapNdb& obj,
                    const QStringList *addtionalText = nullptr);
  void paintVorText(const PaintContext *context, int x, int y, const maptypes::MapVor& obj,
                    const QStringList *addtionalText = nullptr);
  void paintAirportText(const PaintContext *context, int x, int y, const maptypes::MapAirport& obj);
  void paintText(const PaintContext *context, const QColor& color, int x, int y, const QStringList& texts);
  void paintUserpoint(const PaintContext *context, int x, int y);

  void drawSymbols(const PaintContext *context, const RouteMapObjectList& routeMapObjects,
                   const QBitArray& visibleStartPoints, const QList<QPointF>& startPoints);

  void drawSymbolText(const PaintContext *context, const RouteMapObjectList& routeMapObjects,
                      const QBitArray& visibleStartPoints, const QList<QPointF>& startPoints);

  void paintApproachPoints(const PaintContext *context, const maptypes::MapApproachLegs& legs, int index);

  void paintApproachSegment(const PaintContext *context, const maptypes::MapApproachLegs& legs,
                            int index, QLineF& lastLine, QVector<DrawText> *drawTextLines, bool background);

  void paintApproachpoint(const PaintContext *context, int x, int y);

};

#endif // LITTLENAVMAP_MAPPAINTERROUTE_H
