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

#include "mappaintermark.h"
#include "navmapwidget.h"
#include "mapscale.h"
#include "mapgui/mapquery.h"
#include "common/mapcolors.h"
#include "geo/calculations.h"

#include <algorithm>
#include <marble/GeoDataLineString.h>
#include <marble/GeoPainter.h>
#include <marble/MarbleWidget.h>
#include <marble/ViewportParams.h>

using namespace Marble;

MapPainterMark::MapPainterMark(NavMapWidget *widget, MapQuery *mapQuery, MapScale *mapScale, bool verboseMsg)
  : MapPainter(widget, mapQuery, mapScale, verboseMsg), navMapWidget(widget)
{
}

MapPainterMark::~MapPainterMark()
{

}

void MapPainterMark::paint(const MapLayer *mapLayer, GeoPainter *painter, ViewportParams *viewport,
                           maptypes::MapObjectTypes objectTypes)
{
  Q_UNUSED(mapLayer);
  Q_UNUSED(viewport);
  Q_UNUSED(objectTypes);

  bool drawFast = widget->viewContext() == Marble::Animation;
  setRenderHints(painter);

  painter->save();
  paintHighlights(mapLayer, painter, drawFast);
  paintMark(painter);
  paintHome(painter);
  paintRangeRings(mapLayer, painter, viewport, drawFast);
  paintDistanceMarkers(mapLayer, painter);
  painter->restore();
}

void MapPainterMark::paintMark(GeoPainter *painter)
{
  int x, y;
  if(wToS(navMapWidget->getMarkPos(), x, y))
  {
    painter->setPen(mapcolors::markBackPen);

    painter->drawLine(x, y - 10, x, y + 10);
    painter->drawLine(x - 10, y, x + 10, y);

    painter->setPen(mapcolors::markFillPen);
    painter->drawLine(x, y - 8, x, y + 8);
    painter->drawLine(x - 8, y, x + 8, y);
  }
}

void MapPainterMark::paintHome(GeoPainter *painter)
{
  int x, y;
  if(wToS(navMapWidget->getHomePos(), x, y))
  {
    painter->setPen(mapcolors::homeBackPen);
    painter->setBrush(mapcolors::homeFillColor);

    QPolygon roof;
    painter->drawRect(x - 5, y - 5, 10, 10);
    roof << QPoint(x, y - 10) << QPoint(x + 7, y - 3) << QPoint(x - 7, y - 3);
    painter->drawConvexPolygon(roof);
    painter->drawPoint(x, y);
  }
}

void MapPainterMark::paintHighlights(const MapLayer *mapLayer, GeoPainter *painter, bool fast)
{
  using namespace maptypes;

  const MapSearchResult& highlightResults = navMapWidget->getHighlightMapObjects();
  int size = 6;

  QList<atools::geo::Pos> positions;

  for(const MapAirport *ap : highlightResults.airports)
    positions.append(ap->position);
  for(const MapWaypoint *wp : highlightResults.waypoints)
    positions.append(wp->position);
  for(const MapVor *vor : highlightResults.vors)
    positions.append(vor->position);
  for(const MapNdb *ndb : highlightResults.ndbs)
    positions.append(ndb->position);

  if(mapLayer != nullptr)
    if(mapLayer->isAirport())
      size = std::max(size, mapLayer->getAirportSymbolSize());

  painter->setBrush(Qt::NoBrush);
  painter->setPen(QPen(QBrush(mapcolors::highlightColorFast), size / 3, Qt::SolidLine, Qt::FlatCap));
  for(const atools::geo::Pos& pos : positions)
  {
  int x, y;
  if(wToS(pos, x, y))
  {
    if(!fast)
    {
      painter->setPen(QPen(QBrush(mapcolors::highlightBackColor), size / 3 + 2, Qt::SolidLine, Qt::FlatCap));
      painter->drawEllipse(QPoint(x, y), size, size);
      painter->setPen(QPen(QBrush(mapcolors::highlightColor), size / 3, Qt::SolidLine, Qt::FlatCap));
    }
    painter->drawEllipse(QPoint(x, y), size, size);
  }
  }
}

void MapPainterMark::paintRangeRings(const MapLayer *mapLayer, GeoPainter *painter, ViewportParams *viewport,
                                     bool fast)
{
  Q_UNUSED(mapLayer);
  Q_UNUSED(fast);
  const QList<maptypes::RangeRings>& rangeRings = navMapWidget->getRangeRings();
  const GeoDataLatLonAltBox& viewBox = viewport->viewLatLonAltBox();

  painter->setBrush(Qt::NoBrush);
  painter->setPen(QPen(QBrush(mapcolors::rangeRingColor), 3, Qt::SolidLine, Qt::RoundCap, Qt::MiterJoin));

  for(const maptypes::RangeRings& rings : rangeRings)
  {
    auto maxIter = std::max_element(rings.ranges.begin(), rings.ranges.end());
    if(maxIter != rings.ranges.end())
    {
      bool visible;
      QPoint center = wToS(rings.position, &visible);

      int maxDiameter = *maxIter;

      atools::geo::Rect rect(rings.position, atools::geo::nmToMeter(maxDiameter / 2 * 5 / 4));

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
      else if(rings.type == maptypes::ILS)
      {
        color = mapcolors::ilsSymbolColor;
        textColor = mapcolors::ilsSymbolColor;
      }

      if(viewBox.intersects(GeoDataLatLonBox(rect.getNorth(), rect.getSouth(), rect.getEast(), rect.getWest(),
                                             GeoDataCoordinates::Degree)) /* && !fast*/)
      {
        painter->setPen(QPen(QBrush(textColor), 4, Qt::SolidLine, Qt::RoundCap,
                             Qt::MiterJoin));
        painter->drawEllipse(center, 4, 4);
        painter->setPen(QPen(QBrush(color), 3, Qt::SolidLine, Qt::RoundCap,
                             Qt::MiterJoin));
        painter->drawEllipse(center, 4, 4);

        for(int diameter : rings.ranges)
        {
          int xt, yt;
          paintCircle(painter, rings.position, diameter, fast, xt, yt);

          if(xt != -1 && yt != -1)
          {
            painter->setPen(textColor);

            QString txt;
            if(rings.text.isEmpty())
              txt = QString::number(diameter) + " nm";
            else
              txt = rings.text;

            xt -= painter->fontMetrics().width(txt) / 2;
            yt += painter->fontMetrics().height() / 2 - painter->fontMetrics().descent();

            textBox(painter, {txt}, painter->pen(), xt, yt);
            painter->setPen(QPen(QBrush(color), 3, Qt::SolidLine, Qt::RoundCap, Qt::MiterJoin));
          }
        }
      }
    }
  }
}

void MapPainterMark::paintDistanceMarkers(const MapLayer *mapLayer, GeoPainter *painter)
{
  Q_UNUSED(mapLayer);

  QFontMetrics metrics = painter->fontMetrics();
  painter->setBrush(QColor(Qt::white));
  painter->setPen(QPen(QBrush(Qt::black), 3, Qt::SolidLine, Qt::RoundCap, Qt::MiterJoin));

  const QList<maptypes::DistanceMarker>& distanceMarkers = navMapWidget->getDistanceMarkers();

  for(const maptypes::DistanceMarker& m : distanceMarkers)
  {
    int dist = static_cast<int>(std::roundf(atools::geo::meterToNm(m.from.distanceMeterTo(m.to))));

    GeoDataCoordinates from(m.from.getLonX(), m.from.getLatY(), 0, GeoDataCoordinates::Degree);
    GeoDataCoordinates to(m.to.getLonX(), m.to.getLatY(), 0, GeoDataCoordinates::Degree);
    GeoDataLineString line;
    line.append(from);
    line.append(to);
    line.setTessellate(true);

    painter->drawPolyline(line);

    qreal initBearing = atools::geo::normalizeCourse(
      from.bearing(to, GeoDataCoordinates::Degree, GeoDataCoordinates::InitialBearing));
    qreal finalBearing = atools::geo::normalizeCourse(
      from.bearing(to, GeoDataCoordinates::Degree, GeoDataCoordinates::FinalBearing));

    QStringList texts;
    texts.append(QString::number(initBearing, 'f', 0) + "° -> " +
                 QString::number(finalBearing, 'f', 0) + "°");
    texts.append(QString::number(dist) + " nm");

    int xt = -1, yt = -1;
    if(findTextPos(line, painter, metrics.width(texts.at(0)), metrics.height() * 2, xt, yt))
      textBox(painter, texts, painter->pen(), xt, yt, textatt::BOLD | textatt::CENTER);
  }
}
