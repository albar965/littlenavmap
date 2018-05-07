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

#ifndef LITTLENAVMAP_MAPPAINTERNAV_H
#define LITTLENAVMAP_MAPPAINTERNAV_H

#include "mapgui/mappainter.h"

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
  MapPainterNav(MapWidget *mapWidget, MapScale *mapScale);
  virtual ~MapPainterNav();

  virtual void render(PaintContext *context) override;

private:
  void paintMarkers(PaintContext *context, const QList<map::MapMarker> *markers, bool drawFast);
  void paintNdbs(PaintContext *context, const QList<map::MapNdb> *ndbs, bool drawFast);
  void paintVors(PaintContext *context, const QList<map::MapVor> *vors, bool drawFast);
  void paintWaypoints(PaintContext *context, const QList<map::MapWaypoint> *waypoints,
                      bool drawWaypoint, bool drawFast);
  void paintAirways(PaintContext *context, const QList<map::MapAirway> *airways, bool fast);

};

#endif // LITTLENAVMAP_MAPPAINTERAIRPORT_H
