/*****************************************************************************
* Copyright 2015-2018 Alexander Barthel albar965@mailbox.org
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

#ifndef LITTLENAVMAP_MAPPAINTERAIRPORT_H
#define LITTLENAVMAP_MAPPAINTERAIRPORT_H

#include "mapgui/mappainter.h"

#include "fs/common/xpgeometry.h"

class SymbolPainter;

namespace map {
struct MapAirport;
struct MapApron;

struct MapRunway;

}

class Route;

/*
 * Draws airport symbols, runway overview and complete airport diagram. Airport details are also drawn for
 * the flight plan.
 */
class MapPainterAirport :
  public MapPainter
{
  Q_DECLARE_TR_FUNCTIONS(MapPainter)

public:
  MapPainterAirport(MapWidget *mapWidget, MapScale *mapScale, const Route *routeParam);
  virtual ~MapPainterAirport();

  virtual void render(PaintContext *context) override;

private:
  void drawAirportSymbol(PaintContext *context, const map::MapAirport& ap, float x, float y);

  // void drawWindPointer(const PaintContext *context, const maptypes::MapAirport& ap, int x, int y);

  void drawAirportDiagram(const PaintContext *context, const map::MapAirport& airport);
  void drawAirportDiagramBackround(const PaintContext *context, const map::MapAirport& airport);
  void drawAirportSymbolOverview(const PaintContext *context, const map::MapAirport& ap, float x, float y);
  void runwayCoords(const QList<map::MapRunway> *runways, QList<QPoint> *centers, QList<QRect> *rects,
                    QList<QRect> *innerRects, QList<QRect> *outlineRects);
  void drawFsApron(const PaintContext *context, const map::MapApron& apron);
  void drawXplaneApron(const PaintContext *context, const map::MapApron& apron, bool fast);

  /* All sizes in pixel */
  static Q_DECL_CONSTEXPR int RUNWAY_HEADING_FONT_SIZE = 12;
  static Q_DECL_CONSTEXPR int RUNWAY_TEXT_FONT_SIZE = 16;
  static Q_DECL_CONSTEXPR int RUNWAY_NUMBER_FONT_SIZE = 20;
  static Q_DECL_CONSTEXPR int TAXIWAY_TEXT_MIN_LENGTH = 20;
  static Q_DECL_CONSTEXPR int RUNWAY_OVERVIEW_MIN_LENGTH_FEET = 8000;
  static Q_DECL_CONSTEXPR float AIRPORT_DIAGRAM_BACKGROUND_METER = 200.f;
  const Route *route;

  QPainterPath pathForBoundary(const atools::fs::common::Boundary& boundaryNodes,
                               bool fast);

};

#endif // LITTLENAVMAP_MAPPAINTERAIRPORT_H
