/*****************************************************************************
* Copyright 2015-2024 Alexander Barthel alex@littlenavmap.org
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

#include <QPen>
#include <QFont>
#include <QDateTime>

class Queries;
namespace atools {
namespace geo {
class LineString;
}
}

namespace Marble {
class GeoDataLineString;
class GeoPainter;
}

class MapLayer;
class MapPaintWidget;
class MapScale;
class MapWidget;
class SymbolPainter;
class Route;

namespace map {
struct MapAirport;
struct MapRef;
struct MapHolding;
struct MapAirportMsa;

}

/* Struct that is passed to all painters. It is created from scratch for each paint event. */
struct PaintContext
{
  const MapLayer *mapLayer, /* Layer for the current zoom distance also affected by detail level
                             * Used for visibility of map objects */
                 *mapLayerText, /* layer for the current zoom distance also affected by text and label detail level
                                 * Used for visibility of labels */
                 *mapLayerEffective, /* Layer for the current zoom distance not affected by detail level.
                                      *  Used to determine text visibility and object sizes. */
                 *mapLayerRoute, /* Layer for the current zoom distance and more details for route. */
                 *mapLayerRouteText; /* Layer for the current zoom distance and more details for route labels. */

  Marble::GeoPainter *painter;
  Marble::ViewportParams *viewport;
  Marble::ViewContext viewContext;
  float zoomDistanceMeter;

  bool drawFast; /* true if reduced details should be used */
  bool lazyUpdate; /* postpone reloading until map is still */
  bool darkMap; /* CartoDark or similar. Not Night mode */

  map::MapTypes objectTypes; /* Object types that should be drawn */
  map::MapDisplayTypes objectDisplayTypes; /* Object types that should be drawn */
  map::MapAirspaceFilter airspaceFilterByLayer; /* Airspaces */
  map::MapAirspaceType airspaceTextsByLayer;

  atools::geo::Rect viewportRect; /* Rectangle of current viewport */
  QRect screenRect; /* Screen coordinate rect */

  opts::MapScrollDetail mapScrollDetail; /* Option that indicates the detail level when drawFast is true */
  QFont defaultFont /* Default widget font */;
  float distanceNm; /* Zoom distance in NM */
  float distanceKm; /* Zoom distance in KM as in map widget */
  QStringList userPointTypes, /* In menu selected types */
              userPointTypesAll; /* All available tyes */
  bool userPointTypeUnknown; /* Show unknown types */

  const Route *route;

  optsac::DisplayOptionsUserAircraft dispOptsUser;
  optsac::DisplayOptionsAiAircraft dispOptsAi;
  optsd::DisplayOptionsAirport dispOptsAirport;
  optsd::DisplayOptionsRose dispOptsRose;
  optsd::DisplayOptionsMeasurement dispOptsMeasurement;
  optsd::DisplayOptionsRoute dispOptsRoute;

  /* ===============================================================================
   * Flags from options dialog */
  opts::Flags flags;
  opts2::Flags2 flags2;

  map::MapWeatherSource weatherSource;
  bool visibleWidget;
  bool paintCopyright = true, paintWindHeader = true, webMap = false;
  int mimimumRunwayLengthFt = -1; /* Value from toolbar */
  int currentDistanceMarkerId = -1;

  /* ===============================================================================
   * Ids which are filled during painting and are passes between painters */

  // All waypoints from the route and add them to the map to avoid duplicate drawing
  // Same for procedure preview
  QSet<map::MapRef> routeProcIdMap, /* Navaids on plan */
                    routeProcIdMapRec /* Recommended navaids */;

  /* Airports drawn having parking spots which require tooltips and more */
  QSet<int> *shownDetailAirportIds;

  /* All navaids drawn for route and procedures. Points to vector in MapScreenIndex */
  QVector<map::MapRef> *routeDrawnNavaids;

  /* ===============================================================================
   * Text sizes and line thickness in percent / 100 as set in options dialog */
  float textSizeAircraftAi = 1.f;
  float symbolSizeNavaid = 1.f;
  float symbolSizeUserpoint = 1.f;
  float symbolSizeHighlight = 1.f;
  float thicknessFlightplan = 1.f;
  float textSizeNavaid = 1.f;
  float textSizeAirspace = 1.f;
  float textSizeUserpoint = 1.f;
  float textSizeAirway = 1.f;
  float thicknessAirway = 1.f;
  float textSizeCompassRose = 1.f;
  float textSizeRangeUserFeature = 1.f;
  float textSizeRangeMeasurement = 1.f;
  float symbolSizeAirport = 1.f;
  float symbolSizeAirportWeather = 1.f;
  float symbolSizeWindBarbs = 1.f;
  float symbolSizeAircraftAi = 1.f;
  float textSizeFlightplan = 1.f;
  float textSizeAircraftUser = 1.f;
  float symbolSizeAircraftUser = 1.f;
  float textSizeAirport = 1.f;
  float textSizeAirportRunway = 1.f;
  float textSizeAirportTaxiway = 1.f;
  float thicknessTrail = 1.f;
  float thicknessUserFeature = 1.f;
  float thicknessMeasurement = 1.f;
  float thicknessCompassRose = 1.f;
  float textSizeMora = 1.f;
  float transparencyMora = 1.f;
  float textSizeAirportMsa = 1.f;
  float transparencyAirportMsa = 1.f;
  float transparencyFlightplan = 1.f;
  float transparencyHighlight = 1.f;

  int objectCount = 0;
  bool queryOverflow = false;

  /* Increase drawn object count and return true if exceeded */
  bool objCount()
  {
    objectCount++;
    return objectCount >= map::MAX_MAP_OBJECTS;
  }

  bool isObjectOverflow() const
  {
    return objectCount >= map::MAX_MAP_OBJECTS;
  }

  int getObjectCount() const
  {
    return objectCount;
  }

  void setQueryOverflow(bool overflow)
  {
    queryOverflow |= overflow;
  }

  bool isQueryOverflow() const
  {
    return queryOverflow;
  }

  bool  dOptUserAc(optsac::DisplayOptionsUserAircraft opts) const
  {
    return dispOptsUser & opts;
  }

  bool  dOptAiAc(optsac::DisplayOptionsAiAircraft opts) const
  {
    return dispOptsAi & opts;
  }

  bool  dOptAp(optsd::DisplayOptionsAirport opts) const
  {
    return dispOptsAirport & opts;
  }

  bool  dOptRose(optsd::DisplayOptionsRose opts) const
  {
    return dispOptsRose & opts;
  }

  bool  dOptMeasurement(optsd::DisplayOptionsMeasurement opts) const
  {
    return dispOptsMeasurement & opts;
  }

  bool  dOptRoute(optsd::DisplayOptionsRoute opts) const
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

  /* Calculate label text flags for route waypoints depending on layer settings */
  textflags::TextFlags airportTextFlags() const;
  textflags::TextFlags airportTextFlagsMinor() const;
  textflags::TextFlags airportTextFlagsRoute(bool drawAsRoute, bool drawAsLog) const;

  void startTimer(const QString& label)
  {
    if(verboseDraw)
      renderTimesMs.insert(label, QDateTime::currentMSecsSinceEpoch());
  }

  void endTimer(const QString& label)
  {
    if(verboseDraw)
      renderTimesMs.insert(label, QDateTime::currentMSecsSinceEpoch() - renderTimesMs.value(label));
  }

  void clearTimer()
  {
    if(verboseDraw)
      renderTimesMs.clear();
  }

  bool verboseDraw = false;
  QMap<QString, qint64> renderTimesMs;
};

/* Used to collect airports for drawing. Needs to copy airport since it might be removed from the cache. */
class AirportPaintData
{
public:
  AirportPaintData();
  AirportPaintData(const map::MapAirport& ap, float x, float y);
  AirportPaintData(const AirportPaintData& other);

  ~AirportPaintData();

  AirportPaintData& operator=(const AirportPaintData& other);

  const map::MapAirport& getAirport() const
  {
    return *airport;
  }

  const QPointF& getPoint() const
  {
    return point;
  }

private:
  map::MapAirport *airport = nullptr;
  QPointF point;
};

// =============================================================================================

/*
 * Base class for all map painters
 */
class MapPainter :
  public CoordinateConverter
{
  Q_DECLARE_TR_FUNCTIONS(MapPainter)

public:
  MapPainter(MapPaintWidget *marbleWidget, MapScale *mapScale, PaintContext *paintContext);
  virtual ~MapPainter();

  MapPainter(const MapPainter& other) = delete;
  MapPainter& operator=(const MapPainter& other) = delete;

  virtual void render() = 0;

  bool sortAirportFunction(const AirportPaintData& airportPaintData1, const AirportPaintData& airportPaintData2);

  void initQueries();

protected:
  /* All wToSBuf() methods receive a margin parameter. Margins are applied to the screen rectangle for an
   * additional visibility check to avoid objects or texts popping out of view at the screen borders */
  bool wToSBuf(const atools::geo::Pos& coords, int& x, int& y, QSize size, const QMargins& margins, bool *hidden = nullptr) const;

  bool wToSBuf(const atools::geo::Pos& coords, int& x, int& y, const QMargins& margins, bool *hidden = nullptr) const
  {
    return wToSBuf(coords, x, y, DEFAULT_WTOS_SIZE, margins, hidden);
  }

  bool wToSBuf(const atools::geo::Pos& coords, float& x, float& y, QSize size, const QMargins& margins, bool *hidden = nullptr) const;

  bool wToSBuf(const atools::geo::Pos& coords, float& x, float& y, const QMargins& margins, bool *hidden = nullptr) const
  {
    return wToSBuf(coords, x, y, DEFAULT_WTOS_SIZE, margins, hidden);
  }

  bool wToSBuf(const atools::geo::Pos& coords, QPointF& point, const QMargins& margins, bool *hidden = nullptr) const;

  /* Draw a circle and return text placement hints (xtext and ytext). Number of points used
   * for the circle depends on the zoom distance. Optimized for large circles. */
  void paintCircle(Marble::GeoPainter *painter, const atools::geo::Pos& centerPos, float radiusNm, bool fast, QPoint *textPos) const;

  void paintArc(Marble::GeoPainter *painter, const atools::geo::Pos& centerPos, float radiusNm, float angleDegStart, float angleDegEnd,
                bool fast) const;

  void drawLine(Marble::GeoPainter *painter, const atools::geo::Line& line, bool forceDraw = false) const;

  void drawPolygon(Marble::GeoPainter *painter, const atools::geo::LineString& linestring) const;
  void drawPolygons(Marble::GeoPainter *painter, const QVector<QPolygonF *>& polygons) const;
  void drawPolygon(Marble::GeoPainter *painter, const QPolygonF& polygon) const;

  void drawPolyline(Marble::GeoPainter *painter, const atools::geo::LineString& linestring) const;
  void drawPolylines(Marble::GeoPainter *painter, const QVector<QPolygonF *>& polygons) const;
  void drawPolyline(Marble::GeoPainter *painter, const QPolygonF& polygon) const;

  /* Draw simple text with current settings. Corners are the text corners pointing to the position */
  void drawText(Marble::GeoPainter *painter, const atools::geo::Pos& pos, const QString& text, bool topCorner, bool leftCorner) const;

  /* Drawing functions for simple geometry */
  void drawCircle(Marble::GeoPainter *painter, const atools::geo::Pos& center, float radius) const;
  void drawCross(Marble::GeoPainter *painter, int x, int y, int size) const;

  /* No GC and no rhumb */
  void drawLineStraight(Marble::GeoPainter *painter, const atools::geo::Line& line) const;

  /* Save versions of drawLine which check for valid coordinates and bounds */
  void drawLine(QPainter *painter, const QLineF& line) const;

  void drawLine(QPainter *painter, const QLine& line) const
  {
    drawLine(painter, QLineF(line));
  }

  void drawLine(QPainter *painter, const QPoint& p1, const QPoint& p2) const
  {
    drawLine(painter, QLineF(p1, p2));
  }

  void drawLine(QPainter *painter, const QPointF& p1, const QPointF& p2) const
  {
    drawLine(painter, QLineF(p1, p2));
  }

  void paintArc(QPainter *painter, const QPointF& p1, const QPointF& p2, const QPointF& center, bool left) const;

  void paintHoldWithText(QPainter *painter, float x, float y, float direction, float lengthNm, float minutes, bool left,
                         const QString& text, const QString& text2,
                         const QColor& textColor, const QColor& textColorBackground,
                         QVector<float> inboundArrows = QVector<float>(),
                         QVector<float> outboundArrows = QVector<float>()) const;

  /* Draw PI turn */
  void paintProcedureTurnWithText(QPainter *painter, float x, float y, float turnHeading, float distanceNm, bool left,
                                  QLineF *extensionLine, const QString& text, const QColor& textColor,
                                  const QColor& textColorBackground) const;

  void paintAircraftTrail(const QVector<atools::geo::LineString>& lineStrings, float minAlt, float maxAlt,
                          const atools::geo::Pos& aircraftPos) const;

  /* Arrow pointing upwards or downwards */
  QPolygonF buildArrow(float size, bool downwards = false) const;

  /* Draw arrow at line position. pos = 0 is beginning and pos = 1 is end of line */
  void paintArrowAlongLine(QPainter *painter, const QLineF& line, const QPolygonF& arrow, float pos = 0.5f) const;
  void paintArrowAlongLine(QPainter *painter, const atools::geo::Line& line, const QPolygonF& arrow, float pos = 0.5f,
                           float minLengthPx = 0.f) const;

  /* Interface method to QPixmapCache*/
  void getPixmap(QPixmap& pixmap, const QString& resource, int size) const;

  /* Draw enroute as well as user defined holdings */
  void paintHoldingMarks(const QList<map::MapHolding>& holdings, const MapLayer *layer, const MapLayer *layerText,
                         bool user, bool drawFast, bool darkMap) const;

  /* Draw large semi-transparent MSA enabled by user */
  void paintMsaMarks(const QList<map::MapAirportMsa>& airportMsa, bool user, bool drawFast) const;

  /* Draw small flat circle for small radii or close zoom distances */
  void paintCircleSmallInternal(Marble::GeoPainter *painter, const atools::geo::Pos& centerPos, float radiusNm, bool fast,
                                QPoint *textPos) const;

  /* Draw a large spherical correct projected circle */
  void paintCircleLargeInternal(Marble::GeoPainter *painter, const atools::geo::Pos& centerPos, float radiusNm, bool fast,
                                QPoint *textPos) const;

  /* Minimum points to use for a circle */
  const int CIRCLE_MIN_POINTS = 16;
  /* Maximum points to use for a circle */
  const int CIRCLE_MAX_POINTS = 72;

  PaintContext *context = nullptr;
  SymbolPainter *symbolPainter = nullptr;
  MapPaintWidget *mapPaintWidget = nullptr;
  MapScale *scale = nullptr;
  const Queries *queries = nullptr; // Derived from MapPaintWidget which can be GUI or web queries

private:
};

#endif // LITTLENAVMAP_MAPPAINTER_H
