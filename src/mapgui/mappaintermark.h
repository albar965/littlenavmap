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

#ifndef MAPPAINTERMARK_H
#define MAPPAINTERMARK_H

#include "mappainter.h"

namespace Marble {
class GeoDataLineString;
}

class MapWidget;

class MapPainterMark :
  public MapPainter
{
public:
  MapPainterMark(MapWidget *mapWidget, MapQuery *mapQuery, MapScale *mapScale, bool verboseMsg);
  virtual ~MapPainterMark();

  virtual void render(const PaintContext *context) override;

private:
  void paintMark(Marble::GeoPainter *painter);
  void paintHome(Marble::GeoPainter *painter);
  void paintHighlights(const MapLayer *mapLayerEff, Marble::GeoPainter *painter,
                       bool fast);
  void paintRangeRings(Marble::GeoPainter *painter,
                       Marble::ViewportParams *viewport, bool fast);
  void paintDistanceMarkers(Marble::GeoPainter *painter, bool fast);
  void paintRouteDrag(Marble::GeoPainter *painter);

};

#endif // MAPPAINTERMARK_H
