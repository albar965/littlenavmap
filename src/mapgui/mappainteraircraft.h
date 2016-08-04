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

#ifndef LITTLENAVMAP_MAPPAINTERAIRCRAFT_H
#define LITTLENAVMAP_MAPPAINTERAIRCRAFT_H

#include "mapgui/mappainter.h"

namespace Marble {
class GeoDataLineString;
}

class MapWidget;

/*
 * Draws the simulator user aircraft and aircraft track
 */
class MapPainterAircraft :
  public MapPainter
{
  Q_DECLARE_TR_FUNCTIONS(MapPainter)

public:
  MapPainterAircraft(MapWidget *mapWidget, MapQuery *mapQuery, MapScale *mapScale);
  virtual ~MapPainterAircraft();

  virtual void render(const PaintContext *context) override;

private:
  void paintAircraft(const PaintContext *context);
  void paintAircraftTrack(Marble::GeoPainter *painter);

  /* Aircraft symbol size in pixel */
  static Q_DECL_CONSTEXPR int AIRCRAFT_SYMBOL_SIZE = 40;

  /* Minimum length in pixel of a track segment to be drawn */
  static Q_DECL_CONSTEXPR int AIRCRAFT_TRACK_MIN_LINE_LENGTH = 5;
};

#endif // LITTLENAVMAP_MAPPAINTERMARKAIRCRAFT_H
