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

#ifndef LITTLENAVMAP_MAPPAINTER_H
#define LITTLENAVMAP_MAPPAINTER_H

#include "common/coordinateconverter.h"
#include "common/mapflags.h"
#include "options/optiondata.h"
#include "geo/rect.h"

#include <marble/MarbleWidget.h>
#include <QPen>
#include <QApplication>

namespace atools {
namespace geo {
class LineString;
}
}

namespace Marble {
class GeoDataLineString;
class GeoPainter;
}

class SymbolPainter;
class MapLayer;
class MapQuery;
class AirspaceQuery;
class AirportQuery;
class MapScale;
class MapWidget;

/* Struct that is passed on each paint event to all painters */
struct PaintContext
{
  const MapLayer *mapLayer; /* layer for the current zoom distance also affected by detail level
                             *  should be used to visibility of map objects */
  const MapLayer *mapLayerEffective; /* layer for the current zoom distance not affected by detail level.
                                      *  Should be used to determine text visibility and object sizes. */
  Marble::GeoPainter *painter;
  Marble::ViewportParams *viewport;
  Marble::ViewContext viewContext;
  bool drawFast; /* true if reduced details should be used */
  bool lazyUpdate; /* postpone reloading until map is still */
  map::MapObjectTypes objectTypes; /* Object types that should be drawn */
  map::MapAirspaceFilter airspaceFilterByLayer; /* Airspaces */
  atools::geo::Rect viewportRect; /* Rectangle of current viewport */
  opts::MapScrollDetail mapScrollDetail; /* Option that indicates the detail level when drawFast is true */
  QFont defaultFont /* Default widget font */;
  float distance; /* Zoom distance in NM */
  QStringList userPointTypes, /* In menu selected types */
              userPointTypesAll; /* All available tyes */
  bool userPointTypeUnknown; /* Show unknown types */

  opts::DisplayOptions dispOpts;
  opts::Flags flags;
  opts::Flags2 flags2;

  float textSizeAircraftAi = 1.f;
  float symbolSizeNavaid = 1.f;
  float thicknessFlightplan = 1.f;
  float textSizeNavaid = 1.f;
  float symbolSizeAirport = 1.f;
  float symbolSizeAircraftAi = 1.f;
  float textSizeFlightplan = 1.f;
  float textSizeAircraftUser = 1.f;
  float symbolSizeAircraftUser = 1.f;
  float textSizeAirport = 1.f;
  float thicknessTrail = 1.f;
  float thicknessRangeDistance = 1.f;
  float thicknessCompassRose = 1.f;

  // Needs to be larger than number of highest level airports
  static Q_DECL_CONSTEXPR int MAX_OBJECT_COUNT = 4000;
  int objectCount = 0;

  /* Increase drawn object count and return true if exceeded */
  bool objCount()
  {
    objectCount++;
    return objectCount > MAX_OBJECT_COUNT;
  }

  bool isOverflow()
  {
    return objectCount > MAX_OBJECT_COUNT;
  }

  bool  dOpt(const opts::DisplayOptions& opts) const
  {
    return dispOpts & opts;
  }

  /* Calculate real symbol size */
  int sz(float scale, int size) const
  {
    return static_cast<int>(std::round(scale * size));
  }

  int sz(float scale, float size) const
  {
    return static_cast<int>(std::round(scale * size));
  }

  int sz(float scale, double size) const
  {
    return static_cast<int>(std::round(scale * size));
  }

  float szF(float scale, int size) const
  {
    return scale * size;
  }

  float szF(float scale, float size) const
  {
    return scale * size;
  }

  float szF(float scale, double size) const
  {
    return scale * static_cast<float>(size);
  }

  /* Calculate and set font based on scale */
  void szFont(float scale) const;

};

/*
 * Base class for all map painters
 */
class MapPainter :
  public CoordinateConverter
{
  Q_DECLARE_TR_FUNCTIONS(MapPainter)

public:
  MapPainter(MapWidget *marbleWidget, MapScale *mapScale);
  virtual ~MapPainter();

  virtual void render(PaintContext *context) = 0;

protected:
  /* Draw a circle and return text placement hints (xtext and ytext). Number of points used
   * for the circle depends on the zoom distance */
  void paintCircle(Marble::GeoPainter *painter, const atools::geo::Pos& centerPos,
                   float radiusNm, bool fast, int& xtext, int& ytext);

  void drawLineString(const PaintContext *context, const Marble::GeoDataLineString& linestring);
  void drawLineString(const PaintContext *context, const atools::geo::LineString& linestring);
  void drawLine(const PaintContext *context, const atools::geo::Line& line);

  /* No GC and no rhumb */
  void drawLineStraight(const PaintContext *context, const atools::geo::Line& line);

  void paintArc(QPainter *painter, const QPointF& p1, const QPointF& p2, const QPointF& center, bool left);

  void paintHoldWithText(QPainter *painter, float x, float y, float direction, float lengthNm, float minutes, bool left,
                         const QString& text, const QString& text2,
                         const QColor& textColor, const QColor& textColorBackground);

  void paintProcedureTurnWithText(QPainter *painter, float x, float y, float turnHeading, float distanceNm, bool left,
                                  QLineF *extensionLine, const QString& text, const QColor& textColor,
                                  const QColor& textColorBackground);

  /* Minimum points to use for a circle */
  const int CIRCLE_MIN_POINTS = 16;
  /* Maximum points to use for a circle */
  const int CIRCLE_MAX_POINTS = 72;

  SymbolPainter *symbolPainter;
  MapWidget *mapWidget;
  MapQuery *mapQuery;
  AirspaceQuery *airspaceQuery, *airspaceQueryOnline;
  AirportQuery *airportQuery;
  MapScale *scale;

};

#endif // LITTLENAVMAP_MAPPAINTER_H
