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

class AirportQuery;
class AirwayTrackQuery;
class MapLayer;
class MapPaintWidget;
class MapQuery;
class MapScale;
class MapWidget;
class SymbolPainter;
class WaypointTrackQuery;
class AircraftTrack;

namespace map {
struct MapAirport;
struct MapObjectRef;

}

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
  float zoomDistanceMeter;
  bool drawFast; /* true if reduced details should be used */
  bool lazyUpdate; /* postpone reloading until map is still */
  map::MapObjectTypes objectTypes; /* Object types that should be drawn */
  map::MapObjectDisplayTypes objectDisplayTypes; /* Object types that should be drawn */
  map::MapAirspaceFilter airspaceFilterByLayer; /* Airspaces */
  atools::geo::Rect viewportRect; /* Rectangle of current viewport */
  opts::MapScrollDetail mapScrollDetail; /* Option that indicates the detail level when drawFast is true */
  QFont defaultFont /* Default widget font */;
  float distance; /* Zoom distance in NM */
  QStringList userPointTypes, /* In menu selected types */
              userPointTypesAll; /* All available tyes */
  bool userPointTypeUnknown; /* Show unknown types */

  // All waypoints from the route and add them to the map to avoid duplicate drawing
  QSet<map::MapObjectRef> routeIdMap;

  optsd::DisplayOptions dispOpts;
  optsd::DisplayOptionsRose dispOptsRose;
  optsd::DisplayOptionsMeasurement dispOptsMeasurement;
  optsd::DisplayOptionsRoute dispOptsRoute;
  opts::Flags flags;
  opts2::Flags2 flags2;
  map::MapWeatherSource weatherSource;
  bool visibleWidget;

  float textSizeAircraftAi = 1.f;
  float symbolSizeNavaid = 1.f;
  float thicknessFlightplan = 1.f;
  float textSizeNavaid = 1.f;
  float textSizeAirway = 1.f;
  float thicknessAirway = 1.f;
  float textSizeCompassRose = 1.f;
  float textSizeRangeDistance = 1.f;
  float symbolSizeAirport = 1.f;
  float symbolSizeAirportWeather = 1.f;
  float symbolSizeWindBarbs = 1.f;
  float symbolSizeAircraftAi = 1.f;
  float textSizeFlightplan = 1.f;
  float textSizeAircraftUser = 1.f;
  float symbolSizeAircraftUser = 1.f;
  float textSizeAirport = 1.f;
  float thicknessTrail = 1.f;
  float thicknessRangeDistance = 1.f;
  float thicknessCompassRose = 1.f;
  float textSizeMora = 1.f;
  float transparencyMora = 1.f;

  // Needs to be larger than number of highest level airports
  static Q_DECL_CONSTEXPR int MAX_OBJECT_COUNT = 8000;
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

  bool  dOpt(const optsd::DisplayOptions& opts) const
  {
    return dispOpts & opts;
  }

  bool  dOptRose(const optsd::DisplayOptionsRose& opts) const
  {
    return dispOptsRose & opts;
  }

  bool  dOptMeasurement(const optsd::DisplayOptionsMeasurement& opts) const
  {
    return dispOptsMeasurement & opts;
  }

  bool  dOptRoute(const optsd::DisplayOptionsRoute& opts) const
  {
    return dispOptsRoute & opts;
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

  /* Calculate label text flags for route waypoints */
  textflags::TextFlags airportTextFlags() const;
  textflags::TextFlags airportTextFlagsRoute(bool drawAsRoute, bool drawAsLog) const;

};

struct PaintAirportType
{
  const map::MapAirport *airport;
  QPointF point;
};

/*
 * Base class for all map painters
 */
class MapPainter :
  public CoordinateConverter
{
  Q_DECLARE_TR_FUNCTIONS(MapPainter)

public:
  MapPainter(MapPaintWidget *marbleWidget, MapScale *mapScale);
  virtual ~MapPainter();

  virtual void render(PaintContext *context) = 0;

protected:
  /* Draw a circle and return text placement hints (xtext and ytext). Number of points used
   * for the circle depends on the zoom distance */
  void paintCircle(Marble::GeoPainter *painter, const atools::geo::Pos& centerPos,
                   float radiusNm, bool fast, int& xtext, int& ytext);

  /* Drawing functions for simple geometry */
  void drawLineString(Marble::GeoPainter *painter, const atools::geo::LineString& linestring);
  void drawLine(Marble::GeoPainter *painter, const atools::geo::Line& line);
  void drawCircle(Marble::GeoPainter *painter, const atools::geo::Pos& center, int radius);

  /* Draw simple text with current settings. Corners are the text corners pointing to the position */
  void drawText(Marble::GeoPainter *painter, const atools::geo::Pos& pos, const QString& text, bool topCorner,
                bool leftCorner);

  void drawCross(Marble::GeoPainter *painter, int x, int y, int size);

  /* No GC and no rhumb */
  void drawLineStraight(Marble::GeoPainter *painter, const atools::geo::Line& line);

  /* Save versions of drawLine which check for valid coordinates and bounds */
  void drawLine(QPainter *painter, const QLineF& line);

  void drawLine(QPainter *painter, const QLine& line)
  {
    drawLine(painter, QLineF(line));
  }

  void drawLine(QPainter *painter, const QPoint& p1, const QPoint& p2)
  {
    drawLine(painter, QLineF(p1, p2));
  }

  void drawLine(QPainter *painter, const QPointF& p1, const QPointF& p2)
  {
    drawLine(painter, QLineF(p1, p2));
  }

  void paintArc(QPainter *painter, const QPointF& p1, const QPointF& p2, const QPointF& center, bool left);

  void paintHoldWithText(QPainter *painter, float x, float y, float direction, float lengthNm, float minutes, bool left,
                         const QString& text, const QString& text2,
                         const QColor& textColor, const QColor& textColorBackground,
                         QVector<float> inboundArrows = QVector<float>(),
                         QVector<float> outboundArrows = QVector<float>());

  void paintProcedureTurnWithText(QPainter *painter, float x, float y, float turnHeading, float distanceNm, bool left,
                                  QLineF *extensionLine, const QString& text, const QColor& textColor,
                                  const QColor& textColorBackground);

  /* Arrow pointing upwards or downwards */
  QPolygonF buildArrow(float size, bool downwards = false);

  /* Draw arrow at line position. pos = 0 is beginning and pos = 1 is end of line */
  void paintArrowAlongLine(QPainter *painter, const QLineF& line, const QPolygonF& arrow, float pos = 0.5f);
  void paintArrowAlongLine(QPainter *painter, const atools::geo::Line& line, const QPolygonF& arrow, float pos = 0.5f);

  static bool sortAirportFunction(const PaintAirportType& pap1, const PaintAirportType& pap2);

  /* Interface method to QPixmapCache*/
  void getPixmap(QPixmap& pixmap, const QString& resource, int size);

  /* Paint aircraft track or line string. Optimized for large amount of points */
  void paintTrack(Marble::GeoPainter *painter, const AircraftTrack& aircraftTrack, bool mercator);
  void paintTrack(Marble::GeoPainter *painter, const atools::geo::LineString& linestring, bool mercator);

  /* Minimum length in pixel of a track segment to be drawn */
  static Q_DECL_CONSTEXPR int TRACK_MIN_LINE_LENGTH = 5;

  /* Minimum points to use for a circle */
  const int CIRCLE_MIN_POINTS = 16;
  /* Maximum points to use for a circle */
  const int CIRCLE_MAX_POINTS = 72;

  SymbolPainter *symbolPainter;
  MapPaintWidget *mapPaintWidget;
  MapQuery *mapQuery;
  AirwayTrackQuery *airwayQuery;
  WaypointTrackQuery *waypointQuery;
  AirportQuery *airportQuery;
  MapScale *scale;

private:
  /* Adapter which allows passing AircraftTrack or a LineString to paintTrackInternal */
  struct TrackAdapter
  {
    virtual const atools::geo::Pos& at(int i) const = 0;
    virtual int size() const = 0;

  };

  /* Draw a long line with many small segments and optimize drawing */
  void paintTrackInternal(Marble::GeoPainter *painter, const TrackAdapter& linestring, bool mercator);

};

#endif // LITTLENAVMAP_MAPPAINTER_H
