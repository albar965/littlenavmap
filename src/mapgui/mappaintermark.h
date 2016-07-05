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

#ifndef LITTLENAVMAP_MAPPAINTERMARK_H
#define LITTLENAVMAP_MAPPAINTERMARK_H

#include "mapgui/mappainter.h"

namespace Marble {
class GeoDataLineString;
}

class MapWidget;

class MapPainterMark :
  public MapPainter
{
  Q_DECLARE_TR_FUNCTIONS(MapPainter)

public:
  MapPainterMark(MapWidget *mapWidget, MapQuery *mapQuery, MapScale *mapScale, bool verboseMsg);
  virtual ~MapPainterMark();

  virtual void render(const PaintContext *context) override;

private:
  void paintMark(const PaintContext *context);
  void paintHome(const PaintContext *context);
  void paintHighlights(const PaintContext *context);

  void paintRangeRings(const PaintContext *context);

  void paintDistanceMarkers(const PaintContext *context);
  void paintRouteDrag(const PaintContext *context);

  void paintMagneticPoles(const PaintContext *context);

};

#endif // LITTLENAVMAP_MAPPAINTERMARK_H
