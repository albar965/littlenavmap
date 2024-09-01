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

#include "common/coordinateconverter.h"

#include "atools.h"
#include "geo/pos.h"
#include "geo/line.h"
#include "geo/linestring.h"

#include <marble/GeoDataLineString.h>
#include <marble/GeoDataLinearRing.h>
#include <marble/ViewportParams.h>

#include <QLineF>
#include <QPolygonF>

using namespace Marble;
using namespace atools::geo;

const QSize CoordinateConverter::DEFAULT_WTOS_SIZE(100, 100);

CoordinateConverter::CoordinateConverter(const ViewportParams *viewportParams)
  : viewport(viewportParams)
{
}

bool CoordinateConverter::isHidden(const atools::geo::Pos& coords) const
{
  qreal xr, yr;
  bool hidden = false;
  wToS(coords, xr, yr, DEFAULT_WTOS_SIZE, &hidden);
  return hidden;
}

Marble::GeoDataCoordinates CoordinateConverter::toGdc(const atools::geo::Pos& coords) const
{
  return GeoDataCoordinates(coords.getLonX(), coords.getLatY(), 0., DEG);
}

Marble::GeoDataLatLonBox CoordinateConverter::toGdc(const atools::geo::Rect& coords) const
{
  return GeoDataLatLonBox(coords.getNorth(), coords.getSouth(), coords.getEast(), coords.getWest(), DEG);
}

atools::geo::Rect CoordinateConverter::fromGdc(const Marble::GeoDataLatLonBox& coords) const
{
  return Rect(coords.west(DEG), coords.north(DEG), coords.east(DEG), coords.south(DEG));
}

Marble::GeoDataLineString CoordinateConverter::toGdcStr(const atools::geo::Pos& pos1, const atools::geo::Pos& pos2) const
{
  GeoDataLineString linestring;
  linestring.setTessellate(true);
  linestring.append(toGdc(pos1));
  linestring.append(toGdc(pos2));
  return linestring;
}

Marble::GeoDataLineString CoordinateConverter::toGdcStr(const atools::geo::Line& coords) const
{
  GeoDataLineString linestring;
  linestring.setTessellate(true);
  linestring.append(toGdc(coords.getPos1()));
  linestring.append(toGdc(coords.getPos2()));
  return linestring;
}

Marble::GeoDataLineString CoordinateConverter::toGdcStr(const atools::geo::LineString& coords) const
{
  GeoDataLineString linestring;
  linestring.setTessellate(true);

  for(const Pos& pos : coords)
    linestring.append(toGdc(pos));
  return linestring;
}

Pos CoordinateConverter::fromGdc(const Marble::GeoDataCoordinates& coords) const
{
  return Pos(coords.longitude(DEG), coords.latitude(DEG));
}

const LineString CoordinateConverter::fromGdcStr(const Marble::GeoDataLineString& coords) const
{
  LineString linestring;
  for(const GeoDataCoordinates& gdc : coords)
    linestring.append(fromGdc(gdc));
  return linestring;
}

bool CoordinateConverter::isVisible(const atools::geo::Pos& coords, const QSize& size, bool *isHidden) const
{
  qreal xr, yr;
  bool hidden;
  bool visible = wToS(coords, xr, yr, size, &hidden);
  if(isHidden != nullptr)
    *isHidden = hidden;
  return visible && !hidden;
}

bool CoordinateConverter::wToS(const atools::geo::Pos& coords, int& x, int& y, const QSize& size, bool *isHidden) const
{
  qreal xr, yr;
  bool hidden;
  bool visible = wToS(coords, xr, yr, size, &hidden);
  x = static_cast<int>(std::round(xr));
  y = static_cast<int>(std::round(yr));
  if(isHidden != nullptr)
    *isHidden = hidden;
  return visible && !hidden;
}

bool CoordinateConverter::wToS(const atools::geo::Pos& coords, float& x, float& y, const QSize& size, bool *isHidden) const
{
  qreal xr, yr;
  bool hidden;
  bool visible = wToS(coords, xr, yr, size, &hidden);
  x = static_cast<float>(xr);
  y = static_cast<float>(yr);
  if(isHidden != nullptr)
    *isHidden = hidden;
  return visible && !hidden;
}

bool CoordinateConverter::wToS(const atools::geo::Line& coords, QLineF& line, const QSize& size, bool *isHidden) const
{
  if(!coords.isValid())
  {
    line = QLineF();
    if(isHidden != nullptr)
      *isHidden = true;
    return false;
  }

  double x, y;
  bool hidden1, hidden2, visible1, visible2;
  visible1 = wToS(coords.getPos1(), x, y, size, &hidden1);
  line.setP1(QPointF(x, y));

  visible2 = wToS(coords.getPos2(), x, y, size, &hidden2);
  line.setP2(QPointF(x, y));

  if(isHidden != nullptr)
    *isHidden = hidden1 && hidden2;

  return (visible1 || visible2) && (!hidden1 || !hidden2);
}

bool CoordinateConverter::wToS(const atools::geo::Pos& coords, double& x, double& y, const QSize& size, bool *isHidden) const
{
  if(!coords.isValid())
  {
    x = y = 0.;
    if(isHidden != nullptr)
      *isHidden = true;
    return false;
  }

  bool hidden;
  bool visible = wToS(toGdc(coords), x, y, size, &hidden);
  if(isHidden != nullptr)
    *isHidden = hidden;
  return visible && !hidden;
}

bool CoordinateConverter::wToS(const atools::geo::PosD& coords, double& x, double& y, const QSize& size, bool *isHidden) const
{
  if(!coords.isValid())
  {
    x = y = 0.;
    if(isHidden != nullptr)
      *isHidden = true;
    return false;
  }

  bool hidden;
  bool visible = wToS(Marble::GeoDataCoordinates(coords.getLonX(), coords.getLatY(), 0., DEG), x, y, size, &hidden);
  if(isHidden != nullptr)
    *isHidden = hidden;
  return visible && !hidden;
}

bool CoordinateConverter::wToS(const Marble::GeoDataCoordinates& coords, int& x, int& y, const QSize& size, bool *isHidden) const
{
  qreal xr, yr;
  bool hidden;
  bool visible = wToS(coords, xr, yr, size, &hidden);
  x = static_cast<int>(std::round(xr));
  y = static_cast<int>(std::round(yr));
  if(isHidden != nullptr)
    *isHidden = hidden;
  return visible && !hidden;
}

QPointF CoordinateConverter::wToSF(const atools::geo::Pos& coords, const QSize& size, bool *visible, bool *isHidden) const
{
  double xr, yr;
  bool isVisible = wToS(coords, xr, yr, size, isHidden);

  if(visible != nullptr)
  {
    *visible = isVisible;
    return QPointF(xr, yr);
  }
  else
  {
    if(isVisible)
      return QPointF(xr, yr);
    else
      return QPointF();
  }
}

QPoint CoordinateConverter::wToS(const atools::geo::Pos& coords, const QSize& size, bool *visible, bool *isHidden) const
{
  int xr, yr;
  bool isVisible = wToS(coords, xr, yr, size, isHidden);

  if(visible != nullptr)
  {
    *visible = isVisible;
    return QPoint(xr, yr);
  }
  else
  {
    if(isVisible)
      return QPoint(xr, yr);
    else
      return QPoint();
  }
}

QPoint CoordinateConverter::wToS(const Marble::GeoDataCoordinates& coords, const QSize& size, bool *visible, bool *isHidden) const
{
  int xr, yr;
  bool isVisible = wToS(coords, xr, yr, size, isHidden);

  if(visible != nullptr)
  {
    *visible = isVisible;
    return QPoint(xr, yr);
  }
  else
  {
    if(isVisible)
      return QPoint(xr, yr);
    else
      return QPoint();
  }
}

QPointF CoordinateConverter::wToSF(const Marble::GeoDataCoordinates& coords, const QSize& size, bool *visible, bool *isHidden) const
{
  double xr, yr;
  bool isVisible = wToS(coords, xr, yr, size, isHidden);

  if(visible != nullptr)
  {
    *visible = isVisible;
    return QPointF(xr, yr);
  }
  else
  {
    if(isVisible)
      return QPointF(xr, yr);
    else
      return QPointF();
  }
}

QVector<QPolygonF> CoordinateConverter::wToS(const atools::geo::Pos& pos1, const atools::geo::Pos& pos2) const
{
  return wToS(toGdcStr(Line(pos1, pos2)));
}

QVector<QPolygonF> CoordinateConverter::wToS(const atools::geo::Line& line) const
{
  return wToS(toGdcStr(line));
}

QVector<QPolygonF> CoordinateConverter::wToS(const atools::geo::LineString& line) const
{
  GeoDataLineString lineStr;
  lineStr.setTessellate(true);
  for(const atools::geo::Pos& pos : line)
    lineStr << toGdc(pos);
  return wToS(lineStr);
}

QVector<QPolygonF> CoordinateConverter::wToS(const Marble::GeoDataLineString& line) const
{
  QVector<QPolygonF *> polygons;
  viewport->screenCoordinates(line, polygons);

  // Copy polygons and delete pointers
  QVector<QPolygonF> retval;
  for(const QPolygonF *poly : qAsConst(polygons))
    retval.append(*poly);
  qDeleteAll(polygons);

  return retval;
}

bool CoordinateConverter::sToW(int x, int y, atools::geo::Pos& pos) const
{
  qreal lon, lat;
  bool visible = viewport->geoCoordinates(x, y, lon, lat, DEG);

  pos.setLonX(static_cast<float>(lon));
  pos.setLatY(static_cast<float>(lat));
  return visible;
}

atools::geo::Pos CoordinateConverter::sToW(int x, int y) const
{
  qreal lon, lat;
  bool visible = viewport->geoCoordinates(x, y, lon, lat, DEG);
  return visible ? atools::geo::Pos(lon, lat) : atools::geo::EMPTY_POS;
}

atools::geo::Pos CoordinateConverter::sToW(const QPoint& point) const
{
  return sToW(point.x(), point.y());
}

atools::geo::Pos CoordinateConverter::sToW(const QPointF& point) const
{
  return sToW(atools::roundToInt(point.x()), atools::roundToInt(point.y()));
}

bool CoordinateConverter::wToSPoints(const atools::geo::Pos& pos, QVector<double>& x, double& y, const QSize& size, bool *isHidden) const
{
  return wToSPoints(toGdc(pos), x, y, size, isHidden);
}

bool CoordinateConverter::wToSPoints(const atools::geo::Pos& pos, QVector<float>& x, float& y, const QSize& size, bool *isHidden) const
{
  QVector<double> xDouble;
  double yDouble;
  bool retval = wToSPoints(toGdc(pos), xDouble, yDouble, size, isHidden);
  for(double xd : xDouble)
    x.append(static_cast<float>(xd));
  y = static_cast<float>(yDouble);
  return retval;
}

bool CoordinateConverter::wToSPoints(const Marble::GeoDataCoordinates& coords, QVector<double>& x, double& y, const QSize& size,
                                     bool *isHidden) const
{
  bool hidden;
  int numPoints;
  QVarLengthArray<qreal, 100> xordinates(100);
  bool visible = viewport->screenCoordinates(coords, xordinates.data(), y, numPoints, size, hidden);

  for(int i = 0; i < numPoints; i++)
    x.append(xordinates.at(i));

  if(isHidden != nullptr)
    *isHidden = hidden;
  return visible && !hidden;
}

bool CoordinateConverter::wToSInternal(const Marble::GeoDataCoordinates& coords, double& x, double& y, const QSize& size,
                                       bool *isHidden) const
{
  bool hidden;
  int numPoints;
  QVarLengthArray<qreal, 100> xordinates(100);
  bool visible = viewport->screenCoordinates(coords, xordinates.data(), y, numPoints, size, hidden);

  if(numPoints == 0)
    visible = viewport->screenCoordinates(coords, x, y, hidden);
  else
  {
    int foundIndexMaybeOffScreen = -1, foundIndexOnScreen = -1;
    // Do not paint repetitions for the Mercator projection
    // Determine the smallest one which is visible
    for(int i = 0; i < numPoints; i++)
    {
      qreal xordinate = xordinates.at(i);

      if(foundIndexMaybeOffScreen == -1)
        foundIndexMaybeOffScreen = i;

      if(foundIndexOnScreen == -1 && xordinate >= 0 && xordinate <= viewport->width())
        foundIndexOnScreen = i;
    }

    if(foundIndexOnScreen != -1)
      // Found on screen
      x = xordinates.at(foundIndexOnScreen);
    else if(foundIndexMaybeOffScreen != -1)
    {
      x = xordinates.at(foundIndexMaybeOffScreen);
      visible = false;
    }
    else
    {
      // nothing found - hidden
      visible = false;
      hidden = true;
    }
  }

#ifdef DEBUG_INFORMATION_COORDS
  if(x < -5000 || x > 5000)
  {
    qWarning() << Q_FUNC_INFO << x;
    visible = viewport->screenCoordinates(coords, xordinates[0], y, hidden);
  }
#endif

  if(isHidden != nullptr)
    *isHidden = hidden;
  return visible && !hidden;
}

const QVector<QPolygonF *> CoordinateConverter::createPolygons(const atools::geo::LineString& linestring, const QRectF& screenRect) const
{
  QVector<QPolygonF *> polys;
  for(const LineString& ls : linestring.splitAtAntiMeridianList())
  {
    const QVector<QPolygonF *> polyVector = createPolygonsInternal(ls, screenRect);

    if(!polyVector.isEmpty())
      polys.append(polyVector);
  }
  return polys;
}

const QVector<QPolygonF *> CoordinateConverter::createPolygonsInternal(const atools::geo::LineString& linestring,
                                                                       const QRectF& screenRect) const
{
  Marble::GeoDataLinearRing linearRing;
  linearRing.setTessellate(true);

  for(const Pos& pos : linestring)
    linearRing.append(toGdc(pos));

  QVector<QPolygonF *> polygons;
  if(viewport->viewLatLonAltBox().intersects(linearRing.latLonAltBox()) && viewport->resolves(linearRing.latLonAltBox()))
    // Function might return two polygons in Mercator where one is not visible on the screen
    // Polygons will contain more points than the world coordinate polygon
    viewport->screenCoordinates(linearRing, polygons);

  if(polygons.size() > 1)
  {
    // Remove all invisible which appear especially in Mercator projection
    QPolygonF screenPolygon(screenRect);
    polygons.erase(std::remove_if(polygons.begin(), polygons.end(), [&screenPolygon](const QPolygonF *polygon)->bool {
      return !polygon->intersects(screenPolygon);
    }), polygons.end());

    // Remove all points which are too close together
    for(QPolygonF *polygon : qAsConst(polygons))
    {
      // Remove all consecutive duplicate elements from the range
      polygon->erase(std::unique(polygon->begin(), polygon->end(), [](QPointF& p1, QPointF& p2) -> bool {
        return atools::almostEqual(p1.x(), p2.x(), 0.5) && atools::almostEqual(p1.y(), p2.y(), 0.5);
      }), polygon->end());

      // Close again
      if(!polygon->isClosed() && !polygon->isEmpty())
        polygon->append(polygon->constFirst());
    }
  }

  return polygons;
}

void CoordinateConverter::releasePolygons(const QVector<QPolygonF *>& polygons) const
{
  qDeleteAll(polygons);
}

const QVector<QPolygonF *> CoordinateConverter::createPolylines(const atools::geo::LineString& linestring, const QRectF& screenRect,
                                                                bool splitLongLines) const
{
  return createPolylinesInternal(linestring, screenRect, splitLongLines);
}

bool CoordinateConverter::resolves(const Marble::GeoDataLatLonBox& box) const
{
  return viewport->resolves(box) && viewport->viewLatLonAltBox().intersects(box);
}

bool CoordinateConverter::resolves(const Marble::GeoDataCoordinates& coord1, const Marble::GeoDataCoordinates& coord2) const
{
  Marble::GeoDataLineString line;
  line.setTessellate(true);
  line << coord1 << coord2;
  return resolves(line.latLonAltBox());
}

bool CoordinateConverter::resolves(const atools::geo::Rect& rect) const
{
  return resolves(toGdc(rect));
}

bool CoordinateConverter::resolves(const atools::geo::Line& line) const
{
  return resolves(toGdc(line.getPos1()), toGdc(line.getPos2()));
}

const QVector<QPolygonF *> CoordinateConverter::createPolylinesInternal(const atools::geo::LineString& linestring,
                                                                        const QRectF& screenRect, bool splitLongLines) const
{
  QVector<QPolygonF *> polylineVector;

  // Build Marble geometry object
  if(!linestring.isEmpty())
  {
    GeoDataLineString geoLineStr;
    geoLineStr.setTessellate(true);

    if(splitLongLines)
    {
      for(int i = 0; i < linestring.size() - 1; i++)
      {
        Line line(linestring.at(i), linestring.at(i + 1));

        // Split long lines to work around the buggy visibility check in Marble resulting in disappearing line segments
        // Do a quick check using Manhattan distance in degree
        LineString lines;
        if(line.lengthSimple() > 30.f)
          line.interpolatePoints(line.lengthMeter(), 20, lines);
        else if(line.lengthSimple() > 5.f)
          line.interpolatePoints(line.lengthMeter(), 5, lines);
        else
          lines.append(line.getPos1());

        // Append split points or single point
        for(const Pos& pos : qAsConst(lines))
          geoLineStr << toGdc(pos);
      }
    }
    else
    {
      // Skip generation if intermediate points and create direct
      for(const Pos& pos : linestring)
        geoLineStr << toGdc(pos);
    }

    // Add last point
    geoLineStr << GeoDataCoordinates(linestring.constLast().getLonX(), linestring.constLast().getLatY(), 0, DEG);

#ifdef DEBUG_INFORMATION_LINERENDER
    qDebug() << Q_FUNC_INFO << "=========================================";
    for(const GeoDataCoordinates& c : geoLineStr)
      qDebug() << Q_FUNC_INFO << "long" << c.longitude(GeoDataCoordinates::Degree) << "lat" << c.latitude(GeoDataCoordinates::Degree);
#endif

    // Build polyline vector ==============================================
    QVector<GeoDataLineString *> geoLineStrCorrected = geoLineStr.toDateLineCorrected();
    for(const GeoDataLineString *geoLine : qAsConst(geoLineStrCorrected))
    {
      if(viewport->viewLatLonAltBox().intersects(geoLine->latLonAltBox()))
      {
        QVector<QPolygonF *> polylines;
        viewport->screenCoordinates(*geoLine, polylines);

        // Remove invisible polylines
        QPolygonF screenPolygon(screenRect);
        polylines.erase(std::remove_if(polylines.begin(), polylines.end(), [&screenPolygon](const QPolygonF *polyline)->bool {
          return !polyline->intersects(screenPolygon);
        }), polylines.end());

        polylineVector.append(polylines);
      }
    }
    qDeleteAll(geoLineStrCorrected);
  }
  return polylineVector;
}

void CoordinateConverter::releasePolylines(const QVector<QPolygonF *>& polylines) const
{
  qDeleteAll(polylines);
}
