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

#ifndef LITTLENAVMAP_MAPPAINTERNAV_H
#define LITTLENAVMAP_MAPPAINTERNAV_H

#include "mapgui/mappainter.h"

#include "mapgui/mapquery.h"

class SymbolPainter;

class MapPainterNav :
  public MapPainter
{
  Q_DECLARE_TR_FUNCTIONS(MapPainter)

public:
  MapPainterNav(MapWidget *mapWidget, MapQuery *mapQuery, MapScale *mapScale, bool verboseMsg);
  virtual ~MapPainterNav();

  virtual void render(const PaintContext *context) override;

private:
  void paintMarkers(const PaintContext *context, const QList<maptypes::MapMarker> *markers, bool drawFast);
  void paintNdbs(const PaintContext *context, const QList<maptypes::MapNdb> *ndbs, bool drawFast);
  void paintVors(const PaintContext *context, const QList<maptypes::MapVor> *vors, bool drawFast);
  void paintWaypoints(const PaintContext *context, const QList<maptypes::MapWaypoint> *waypoints,
                      bool drawWaypoint, bool drawFast);
  void paintAirways(const PaintContext *context, const QList<maptypes::MapAirway> *airways,
                    const Marble::GeoDataLatLonAltBox& curBox, bool fast);

};

#endif // LITTLENAVMAP_MAPPAINTERAIRPORT_H
