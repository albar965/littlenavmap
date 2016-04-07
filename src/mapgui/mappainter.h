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

#ifndef MAPPAINTER_H
#define MAPPAINTER_H

#include "common/coordinateconverter.h"
#include "common/maptypes.h"
#include <marble/MarbleWidget.h>

#include <QPen>

namespace atools {
namespace geo {
class Pos;
}
}

namespace Marble {
class GeoDataLineString;
class GeoPainter;
}

class SymbolPainter;
class MapLayer;
class MapQuery;
class MapScale;

/* Map of id and type */
typedef QPair<int, maptypes::MapObjectTypes> ForcePaintType;

struct PaintContext
{
  const MapLayer *mapLayer;
  const MapLayer *mapLayerEffective;
  Marble::GeoPainter *painter;
  Marble::ViewportParams *viewport;
  maptypes::MapObjectTypes objectTypes;
  QSet<ForcePaintType> *forcePaintObjects;
};

class MapPainter :
  public CoordinateConverter
{
public:
  MapPainter(Marble::MarbleWidget *marbleWidget, MapQuery *mapQuery, MapScale *mapScale, bool verboseMsg);
  virtual ~MapPainter();

  virtual void render(const PaintContext *context) = 0;

protected:
  const Marble::GeoDataCoordinates::Unit DEG = Marble::GeoDataCoordinates::Degree;
  const Marble::GeoDataCoordinates::BearingType INITBRG = Marble::GeoDataCoordinates::InitialBearing;
  const Marble::GeoDataCoordinates::BearingType FINALBRG = Marble::GeoDataCoordinates::FinalBearing;

  void setRenderHints(Marble::GeoPainter *painter);
  void paintCircle(Marble::GeoPainter *painter, const atools::geo::Pos& pos, int radiusNm, bool fast,
                   int& xtext, int& ytext);

  bool findTextPos(const atools::geo::Pos& pos1, const atools::geo::Pos& pos2, Marble::GeoPainter *painter,
                   int w, int h, int& x, int& y, float *bearing);
  bool findTextPos(const atools::geo::Pos& pos1, const atools::geo::Pos& pos2, Marble::GeoPainter *painter,
                   float distance, int w, int h, int& x, int& y, float *bearing);
  bool findTextPosRhumb(const atools::geo::Pos& pos1, const atools::geo::Pos& pos2,
                        Marble::GeoPainter *painter, float distance, int w, int h, int& x, int& y);

  const float FIND_TEXT_POS_STEP = 0.02f;

  SymbolPainter *symbolPainter;
  Marble::MarbleWidget *widget;
  MapQuery *query;
  MapScale *scale;
  bool verbose = false;

};

#endif // MAPPAINTER_H
