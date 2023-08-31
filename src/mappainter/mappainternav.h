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

#ifndef LITTLENAVMAP_MAPPAINTERNAV_H
#define LITTLENAVMAP_MAPPAINTERNAV_H

#include "mappainter/mappainter.h"

#include "common/maptypes.h"

class SymbolPainter;

/*
 * Draws VOR, NDB, markers, waypoints and airways. Flight plan navaids are drawn separately in MapPainterRoute.
 */
class MapPainterNav :
  public MapPainter
{
  Q_DECLARE_TR_FUNCTIONS(MapPainter)

public:
  MapPainterNav(MapPaintWidget *mapPaintWidget, MapScale *mapScale, PaintContext *paintContext);
  virtual ~MapPainterNav() override;

  virtual void render() override;

private:
  void paintNdbs(const QHash<int, map::MapNdb>& ndbs, bool drawFast);
  void paintVors(const QHash<int, map::MapVor>& vors, bool drawFast);
  void paintWaypoints(const QHash<int, map::MapWaypoint>& waypoints);

  void paintMarkers(const QList<map::MapMarker> *markers, bool drawFast);
  void paintAirways(const QList<map::MapAirway> *airways, bool fast, bool track);

};

#endif // LITTLENAVMAP_MAPPAINTERAIRPORT_H
