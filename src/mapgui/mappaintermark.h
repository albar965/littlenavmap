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

#ifndef LITTLENAVMAP_MAPPAINTERMARK_H
#define LITTLENAVMAP_MAPPAINTERMARK_H

#include "mapgui/mappainter.h"

namespace Marble {
class GeoDataLineString;
}

class MapWidget;

/*
 * Paint all marks, distance measure lines, range rings, selected object highlights
 * and magnetic pole indications.
 */
class MapPainterMark :
  public MapPainter
{
  Q_DECLARE_TR_FUNCTIONS(MapPainter)

public:
  MapPainterMark(MapWidget *mapWidget, MapScale *mapScale);
  virtual ~MapPainterMark();

  virtual void render(PaintContext *context) override;

private:
  void paintMark(const PaintContext *context);
  void paintHome(const PaintContext *context);
  void paintHighlights(PaintContext *context);
  void paintRangeRings(const PaintContext *context);
  void paintDistanceMarkers(const PaintContext *context);
  void paintRouteDrag(const PaintContext *context);
  void paintCompassRose(const PaintContext *context);
  void paintUserpointDrag(const PaintContext *context);

};

#endif // LITTLENAVMAP_MAPPAINTERMARK_H
