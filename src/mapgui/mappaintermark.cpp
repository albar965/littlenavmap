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

#include "mapgui/mappaintermark.h"

#include "mapgui/mapwidget.h"
#include "mapgui/mapscale.h"
#include "mapgui/maplayer.h"
#include "common/mapcolors.h"
#include "geo/calculations.h"
#include "common/symbolpainter.h"
#include "atools.h"
#include "common/constants.h"
#include "route/routemapobject.h"
#include "route/routemapobjectlist.h"

#include <marble/GeoDataLineString.h>
#include <marble/GeoPainter.h>

using namespace Marble;
using namespace atools::geo;
using namespace maptypes;

MapPainterMark::MapPainterMark(MapWidget *mapWidget, MapQuery *mapQuery, MapScale *mapScale)
  : MapPainter(mapWidget, mapQuery, mapScale)
{
}

MapPainterMark::~MapPainterMark()
{

}

void MapPainterMark::render(const PaintContext *context)
{
  setRenderHints(context->painter);

  context->painter->save();
  paintHighlights(context);
  paintMark(context);
  paintHome(context);
  paintRangeRings(context);
  paintDistanceMarkers(context);
  paintRouteDrag(context);
  paintMagneticPoles(context);
  context->painter->restore();
}

/* Draw black yellow cross for search distance marker */
void MapPainterMark::paintMark(const PaintContext *context)
{
  int x, y;
  if(wToS(mapWidget->getSearchMarkPos(), x, y))
  {
    int size = context->symSize(10);
    int size2 = context->symSize(8);

    context->painter->setPen(mapcolors::markBackPen);

    context->painter->drawLine(x, y - size, x, y + size);
    context->painter->drawLine(x - size, y, x + size, y);

    context->painter->setPen(mapcolors::markFillPen);
    context->painter->drawLine(x, y - size2, x, y + size2);
    context->painter->drawLine(x - size2, y, x + size2, y);
  }
}

/* Draw two indications for the magnetic poles in 2007 */
void MapPainterMark::paintMagneticPoles(const PaintContext *context)
{
  GeoPainter *painter = context->painter;

  painter->setPen(mapcolors::magneticPolePen);

  int size = context->symSize(5);

  int x, y;
  if(wToS(MAG_NORTH_POLE_2007, x, y))
  {
    painter->drawEllipse(x, y, size, size);
    symbolPainter->textBox(painter, {tr("Magnetic North"), tr("2007")},
                           painter->pen(), x + size, y, textatt::NONE, 0);
  }

  if(wToS(MAG_SOUTH_POLE_2007, x, y))
  {
    painter->drawEllipse(x, y, size, size);
    symbolPainter->textBox(painter, {tr("Magnetic South"), tr("2007")},
                           painter->pen(), x + size, y, textatt::NONE, 0);
  }
}

/* Paint the center of the home position */
void MapPainterMark::paintHome(const PaintContext *context)
{
  GeoPainter *painter = context->painter;

  int x, y;
  if(wToS(mapWidget->getHomePos(), x, y))
  {
    painter->setPen(mapcolors::homeBackPen);
    painter->setBrush(mapcolors::homeFillColor);

    int size = context->symSize(10);

    painter->drawRect(x - (size / 2), y - (size / 2), size, size);

    QPolygon roof;
    roof << QPoint(x, y - size) << QPoint(x + size - 3, y - 3) << QPoint(x - size + 3, y - 3);
    painter->drawConvexPolygon(roof);
    painter->drawPoint(x, y);
  }
}

/* Draw rings around objects that are selected on the search or flight plan tables */
void MapPainterMark::paintHighlights(const PaintContext *context)
{
  // Draw hightlights from the search result view ------------------------------------------
  const MapSearchResult& highlightResults = mapWidget->getSearchHighlights();
  int size = context->symSize(6);

  QList<Pos> positions;

  for(const MapAirport& ap : highlightResults.airports)
    positions.append(ap.position);
  for(const MapWaypoint& wp : highlightResults.waypoints)
    positions.append(wp.position);
  for(const MapVor& vor : highlightResults.vors)
    positions.append(vor.position);
  for(const MapNdb& ndb : highlightResults.ndbs)
    positions.append(ndb.position);

  GeoPainter *painter = context->painter;
  if(context->mapLayerEffective->isAirport())
    size = context->symSize(std::max(size, context->mapLayerEffective->getAirportSymbolSize()));

  painter->setBrush(Qt::NoBrush);
  painter->setPen(QPen(QBrush(mapcolors::highlightColorFast), size / 3, Qt::SolidLine, Qt::FlatCap));
  for(const Pos& pos : positions)
  {
    int x, y;
    if(wToS(pos, x, y))
    {
      if(!context->drawFast)
      {
        painter->setPen(QPen(QBrush(mapcolors::highlightBackColor), size / 3 + 2, Qt::SolidLine, Qt::FlatCap));
        painter->drawEllipse(QPoint(x, y), size, size);
        painter->setPen(QPen(QBrush(mapcolors::highlightColor), size / 3, Qt::SolidLine, Qt::FlatCap));
      }
      painter->drawEllipse(QPoint(x, y), size, size);
    }
  }

  if(context->mapLayerEffective->isAirport())
    size = std::max(size, context->mapLayerEffective->getAirportSymbolSize());

  // Draw hightlights from the flight plan view ------------------------------------------
  const RouteMapObjectList& routeHighlightResults = mapWidget->getRouteHighlights();
  positions.clear();
  for(const RouteMapObject& rmo : routeHighlightResults)
    positions.append(rmo.getPosition());

  painter->setBrush(Qt::NoBrush);
  painter->setPen(QPen(QBrush(mapcolors::routeHighlightColorFast), size / 3, Qt::SolidLine, Qt::FlatCap));
  for(const Pos& pos : positions)
  {
    int x, y;
    if(wToS(pos, x, y))
    {
      if(!context->drawFast)
      {
        painter->setPen(QPen(QBrush(mapcolors::routeHighlightBackColor), size / 3 + 2, Qt::SolidLine,
                             Qt::FlatCap));
        painter->drawEllipse(QPoint(x, y), size, size);
        painter->setPen(QPen(QBrush(mapcolors::routeHighlightColor), size / 3, Qt::SolidLine, Qt::FlatCap));
      }
      painter->drawEllipse(QPoint(x, y), size, size);
    }
  }
}

/* Draw all rang rings. This includes the red rings and the radio navaid ranges. */
void MapPainterMark::paintRangeRings(const PaintContext *context)
{
  const QList<maptypes::RangeMarker>& rangeRings = mapWidget->getRangeRings();
  GeoPainter *painter = context->painter;

  painter->setBrush(Qt::NoBrush);

  for(const maptypes::RangeMarker& rings : rangeRings)
  {
    // Get the biggest ring to check visibility
    QVector<int>::const_iterator maxRingIter = std::max_element(rings.ranges.begin(), rings.ranges.end());

    if(maxRingIter != rings.ranges.end())
    {
      int maxRadiusNm = *maxRingIter;

      Rect rect(rings.center, nmToMeter(maxRadiusNm));

      if(context->viewportRect.overlaps(rect) /*&& !fast*/)
      {
        // Ring is visible - the rest of the visibility check is done in paintCircle

        // Select color according to source
        QColor color = mapcolors::rangeRingColor, textColor = mapcolors::rangeRingTextColor;
        if(rings.type == maptypes::VOR)
        {
          color = mapcolors::vorSymbolColor;
          textColor = mapcolors::vorSymbolColor;
        }
        else if(rings.type == maptypes::NDB)
        {
          color = mapcolors::ndbSymbolColor;
          textColor = mapcolors::ndbSymbolColor;
        }
        // ILS is currently not used
        // else if(rings.type == maptypes::ILS)
        // {
        // color = mapcolors::ilsSymbolColor;
        // textColor = mapcolors::ilsTextColor;
        // }

        painter->setPen(QPen(QBrush(color), 3, Qt::SolidLine, Qt::RoundCap, Qt::MiterJoin));

        bool centerVisible;
        QPoint center = wToS(rings.center, DEFAULT_WTOS_SIZE, &centerVisible);
        if(centerVisible)
        {
          // Draw small center point
          painter->setPen(QPen(QBrush(color), 3, Qt::SolidLine, Qt::RoundCap, Qt::MiterJoin));
          painter->drawEllipse(center, 4, 4);
        }

        // Draw all rings
        for(int radius : rings.ranges)
        {
          int xt, yt;
          paintCircle(painter, rings.center, radius, context->drawFast, xt, yt);

          if(xt != -1 && yt != -1)
          {
            // paintCirle found a text position - draw text
            painter->setPen(textColor);

            QString txt;
            if(rings.text.isEmpty())
              txt = QLocale().toString(radius) + tr(" nm");
            else
              txt = rings.text;

            xt -= painter->fontMetrics().width(txt) / 2;
            yt += painter->fontMetrics().height() / 2 - painter->fontMetrics().descent();

            symbolPainter->textBox(painter, {txt}, painter->pen(), xt, yt);
            painter->setPen(QPen(QBrush(color), 3, Qt::SolidLine, Qt::RoundCap, Qt::MiterJoin));
          }
        }
      }
    }
  }
}

/* Draw great circle and rhumb line distance measurment lines */
void MapPainterMark::paintDistanceMarkers(const PaintContext *context)
{
  GeoPainter *painter = context->painter;
  QFontMetrics metrics = painter->fontMetrics();

  const QList<maptypes::DistanceMarker>& distanceMarkers = mapWidget->getDistanceMarkers();

  for(const maptypes::DistanceMarker& m : distanceMarkers)
  {
    // Get color from marker
    painter->setPen(QPen(m.color, 2, Qt::SolidLine, Qt::RoundCap, Qt::MiterJoin));

    const int SYMBOL_SIZE = 5;
    int x, y;
    if(wToS(m.from, x, y))
      // Draw ellipse at start point
      painter->drawEllipse(QPoint(x, y), SYMBOL_SIZE, SYMBOL_SIZE);

    if(wToS(m.to, x, y))
    {
      // Draw cross at end point
      painter->drawLine(x - SYMBOL_SIZE, y, x + SYMBOL_SIZE, y);
      painter->drawLine(x, y - SYMBOL_SIZE, x, y + SYMBOL_SIZE);
    }

    painter->setPen(QPen(m.color, 3, Qt::SolidLine, Qt::RoundCap, Qt::MiterJoin));
    if(!m.isRhumbLine)
    {
      // Draw great circle line
      float distanceMeter = m.from.distanceMeterTo(m.to);

      GeoDataCoordinates from(m.from.getLonX(), m.from.getLatY(), 0, DEG);
      GeoDataCoordinates to(m.to.getLonX(), m.to.getLatY(), 0, DEG);

      // Draw line
      GeoDataLineString line;
      line.append(from);
      line.append(to);
      line.setTessellate(true);
      painter->drawPolyline(line);

      // Build and draw text
      qreal initBearing = normalizeCourse(from.bearing(to, DEG, INITBRG));
      qreal finalBearing = normalizeCourse(from.bearing(to, DEG, FINALBRG));

      QStringList texts;
      if(!m.text.isEmpty())
        texts.append(m.text);
      texts.append(QLocale().toString(atools::geo::normalizeCourse(initBearing), 'f', 0) + tr("°T ► ") +
                   QLocale().toString(atools::geo::normalizeCourse(finalBearing), 'f', 0) + tr("°T"));
      texts.append(atools::numberToString(meterToNm(distanceMeter)) + tr(" nm"));
      if(distanceMeter < 6000)
        // Add feet to text for short distances
        texts.append(QLocale().toString(meterToFeet(distanceMeter), 'f', 0) + tr(" ft"));

      if(m.from != m.to)
      {
        int xt = -1, yt = -1;
        if(findTextPos(m.from, m.to, painter, distanceMeter, metrics.width(texts.at(0)),
                       metrics.height() * 2, xt, yt, nullptr))
          symbolPainter->textBox(painter, texts, painter->pen(), xt, yt, textatt::BOLD | textatt::CENTER);
      }
    }
    else
    {
      // Draw a rhumb line with constant course
      float bearing = m.from.angleDegToRhumb(m.to);
      float magBearing = m.hasMagvar ? bearing - m.magvar : bearing;
      magBearing = atools::geo::normalizeCourse(magBearing);

      float distanceMeter = m.from.distanceMeterToRhumb(m.to);

      // Approximate the needed number of line segments
      int pixel = scale->getPixelIntForMeter(distanceMeter);
      int numPoints = std::min(std::max(pixel / (context->drawFast ? 200 : 20), 4), 72);

      Pos p1 = m.from, p2;

      // Draw line segments
      for(float d = 0.f; d < distanceMeter; d += distanceMeter / numPoints)
      {
        p2 = m.from.endpointRhumb(d, bearing);
        GeoDataLineString line;
        line.append(GeoDataCoordinates(p1.getLonX(), p1.getLatY(), 0, DEG));
        line.append(GeoDataCoordinates(p2.getLonX(), p2.getLatY(), 0, DEG));
        painter->drawPolyline(line);
        p1 = p2;
      }

      // Draw rest
      p2 = m.from.endpointRhumb(distanceMeter, bearing);
      GeoDataLineString line;
      line.append(GeoDataCoordinates(p1.getLonX(), p1.getLatY(), 0, DEG));
      line.append(GeoDataCoordinates(p2.getLonX(), p2.getLatY(), 0, DEG));
      painter->drawPolyline(line);

      // Build and draw text
      QStringList texts;
      if(!m.text.isEmpty())
        texts.append(m.text);
      texts.append(QLocale().toString(atools::geo::normalizeCourse(magBearing), 'f', 0) +
                   (m.hasMagvar ? tr("°M") : tr("°T")));
      texts.append(atools::numberToString(meterToNm(distanceMeter)) + tr(" nm"));
      if(distanceMeter < 6000)
        texts.append(QLocale().toString(meterToFeet(distanceMeter), 'f', 0) + tr(" ft"));

      if(m.from != m.to)
      {
        int xt = -1, yt = -1;
        if(findTextPosRhumb(m.from, m.to, painter, distanceMeter, metrics.width(texts.at(0)),
                            metrics.height() * 2, xt, yt))
          symbolPainter->textBox(painter, texts,
                                 painter->pen(), xt, yt, textatt::ITALIC | textatt::BOLD | textatt::CENTER);
      }
    }
  }
}

/* Draw route dragging/moving lines */
void MapPainterMark::paintRouteDrag(const PaintContext *context)
{
  atools::geo::Pos from, to;
  QPoint cur;

  // Flight plan editing use always three points except at the end
  mapWidget->getRouteDragPoints(from, to, cur);
  if(!cur.isNull())
  {
    GeoDataCoordinates curGeo;
    if(sToW(cur.x(), cur.y(), curGeo))
    {
      GeoDataLineString linestring;
      linestring.setTessellate(true);

      if(from.isValid())
        linestring.append(GeoDataCoordinates(from.getLonX(), from.getLatY(), 0, DEG));
      linestring.append(GeoDataCoordinates(curGeo));
      if(to.isValid())
        linestring.append(GeoDataCoordinates(to.getLonX(), to.getLatY(), 0, DEG));

      if(linestring.size() > 1)
      {
        context->painter->setPen(QPen(mapcolors::routeDragColor, 3, Qt::SolidLine, Qt::RoundCap,
                                      Qt::RoundJoin));
        context->painter->drawPolyline(linestring);
      }
    }
  }
}
