/*****************************************************************************
* Copyright 2015-2025 Alexander Barthel alex@littlenavmap.org
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

#include "geo/coordinateconverter.h"
#include "geo/rect.h"
#include "options/optiondata.h"

#include <marble/MarbleGlobal.h>

#include <QPen>
#include <QFont>
#include <QDateTime>
#include <QCoreApplication>

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
class PaintContext;

namespace map {
struct MapAirport;
struct MapRef;
struct MapHolding;
struct MapAirportMsa;

}

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
  void drawPolygons(Marble::GeoPainter *painter, const QList<QPolygonF *>& polygons) const;
  void drawPolygon(Marble::GeoPainter *painter, const QPolygonF& polygon) const;

  void drawPolyline(Marble::GeoPainter *painter, const atools::geo::LineString& linestring) const;
  void drawPolylines(Marble::GeoPainter *painter, const QList<QPolygonF *>& polygons) const;
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
                         const QString& text, const QString& text2, const QColor& textColor, const QColor& textColorBackground,
                         const QList<float>& inboundArrows = QList<float>(),
                         const QList<float>& outboundArrows = QList<float>()) const;

  /* Draw PI turn */
  void paintProcedureTurnWithText(QPainter *painter, float x, float y, float turnHeading, float distanceNm, bool left,
                                  QLineF *extensionLine, const QString& text, const QColor& textColor,
                                  const QColor& textColorBackground) const;

  void paintAircraftTrail(const QList<atools::geo::LineString>& lineStrings, float minAlt, float maxAlt,
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

  PaintContext *context = nullptr;
  SymbolPainter *symbolPainter = nullptr;
  MapPaintWidget *mapPaintWidget = nullptr;
  MapScale *scale = nullptr;
  const Queries *queries = nullptr; // Derived from MapPaintWidget which can be GUI or web queries
};

#endif // LITTLENAVMAP_MAPPAINTER_H
