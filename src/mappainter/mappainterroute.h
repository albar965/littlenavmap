/*****************************************************************************
* Copyright 2015-2022 Alexander Barthel alex@littlenavmap.org
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

#include "mappainter/mappainter.h"

#include "geo/line.h"

namespace Marble {
class GeoDataLineString;
}

class MapWidget;
class RouteLeg;
class TextPlacement;

namespace proc {
struct MapProcedureLegs;
struct MapProcedureLeg;
}

namespace map {
struct MapAirport;
struct MapVor;
struct MapNdb;
struct MapWaypoint;
struct MapUserpointRoute;
}

/*
 * Draws the flight plan line and all enroute navaid and departure and destination airports (airport symbols only).
 * Airport diagrams and route overview are drawn by the airport painter.
 */
class MapPainterRoute :
  public MapPainter
{
  Q_DECLARE_TR_FUNCTIONS(MapPainter)

public:
  MapPainterRoute(MapPaintWidget *mapPaintWidget, MapScale *mapScale, PaintContext *paintContext);
  virtual ~MapPainterRoute() override;

  virtual void render() override;

private:
  struct DrawText
  {
    DrawText()
    {
    }

    DrawText(const atools::geo::Line& lineParam, bool distanceParam, bool courseParam)
      : line(lineParam), distance(distanceParam), course(courseParam)
    {
    }

    /* Line for text placement */
    atools::geo::Line line;

    /* Draw distance and/or course information */
    bool distance, course;
  };

  void paintRoute();
  void paintRecommended(int passedRouteLeg, QSet<map::MapObjectRef>& idMap);

  void paintAirport(float x, float y, const map::MapAirport& obj);
  void paintVor(float x, float y, const map::MapVor& obj, bool preview);
  void paintNdb(float x, float y, const map::MapNdb& obj, bool preview);
  void paintWaypoint(float x, float y, const map::MapWaypoint& obj, bool preview);
  void paintWaypoint(const QColor& col, float x, float y, bool preview);

  void paintProcedure(proc::MapProcedureLeg& lastLegPoint, QSet<map::MapObjectRef>& idMap, const proc::MapProcedureLegs& legs, int legsRouteOffset, const QColor& color,
                      bool preview, bool previewAll);
  void paintWaypointText(float x, float y, const map::MapWaypoint& obj, bool drawTextDetails, bool drawAsRoute,
                         const QStringList *additionalText);
  void paintNdbText(float x, float y, const map::MapNdb& obj, bool drawTextDetails, bool drawAsRoute, const QStringList *additionalText);
  void paintVorText(float x, float y, const map::MapVor& obj, bool drawTextDetails, bool drawAsRoute, const QStringList *additionalText);
  void paintAirportText(float x, float y, const map::MapAirport& obj, bool drawAsRoute);
  void paintText(const QColor& color, float x, float y, bool drawTextDetails, QStringList texts, bool drawAsRoute,
                 textatt::TextAttributes atts = textatt::NONE);
  void paintUserpoint(float x, float y, const map::MapUserpointRoute& obj, bool preview);
  void paintProcedurePoint(float x, float y, bool preview);

  void paintProcedurePoint(proc::MapProcedureLeg& lastLegPoint, QSet<map::MapObjectRef>& idMap, const proc::MapProcedureLegs& legs, int index, bool preview,
                           bool previewAll, bool drawTextFlag);

  void drawSymbols(const QBitArray& visibleStartPoints, const QList<QPointF>& startPoints, bool preview);

  void drawRouteSymbolText(const QBitArray& visibleStartPoints, const QList<QPointF>& startPoints);

  void paintProcedureSegment(const proc::MapProcedureLegs& legs, int index, QVector<QLineF>& lastLines, QVector<DrawText> *drawTextLines,
                             bool noText, bool previewAll, bool draw);

  void paintTopOfDescentAndClimb();

  QLineF paintProcedureTurn(QVector<QLineF>& lastLines, QLineF line, const proc::MapProcedureLeg& leg, QPainter *painter,
                            const QPointF& intersectPoint, bool draw);
  void paintProcedureBow(const proc::MapProcedureLeg *prevLeg, QVector<QLineF>& lastLines, QPainter *painter, QLineF line,
                         const proc::MapProcedureLeg& leg, const QPointF& intersectPoint, bool draw);

  /* Waypoint Underlays */
  void paintProcedureUnderlay(const proc::MapProcedureLeg& leg, float x, float y, float size);

  void drawStartParking();
  void drawWindBarbs(const QBitArray& visibleStartPoints, const QList<QPointF>& startPoints);
  void drawWindBarbAtWaypoint(float windSpeed, float windDir, float x, float y);
  void drawRouteInternal(QStringList routeTexts, QVector<atools::geo::Line> lines, int passedRouteLeg);
  QString buildLegText(const RouteLeg& leg);
  QString buildLegText(float distance, float courseMag, float courseTrue);

  /* Active or normal color depending on options setting */
  QColor flightplanActiveColor() const;
  QColor flightplanActiveProcColor() const;
  void paintInboundOutboundTexts(const TextPlacement& textPlacement, int passedRouteLeg, bool vor);

  /* Avoid drawing duplicate navaids from flight plan and preview */
  QSet<map::MapObjectRef> routeProcIdMap;

};

#endif // LITTLENAVMAP_MAPPAINTERROUTE_H
