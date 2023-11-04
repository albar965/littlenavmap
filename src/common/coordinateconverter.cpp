/*****************************************************************************
* Copyright 2015-2023 Alexander Barthel alex@littlenavmap.org
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

GeoDataCoordinates CoordinateConverter::toGdc(const Pos& coords) const
{
  return GeoDataCoordinates(coords.getLonX(), coords.getLatY(), 0., DEG);
}

Marble::GeoDataLatLonBox CoordinateConverter::toGdc(const atools::geo::Rect& coords) const
{
  return GeoDataLatLonBox(coords.getNorth(), coords.getSouth(), coords.getEast(), coords.getWest(), DEG);
}

Rect CoordinateConverter::fromGdc(const GeoDataLatLonBox& coords) const
{
  return Rect(coords.west(DEG), coords.north(DEG), coords.east(DEG), coords.south(DEG));
}

GeoDataLineString CoordinateConverter::toGdcStr(const Pos& pos1, const Pos& pos2) const
{
  GeoDataLineString linestring;
  linestring.setTessellate(true);
  linestring.append(toGdc(pos1));
  linestring.append(toGdc(pos2));
  return linestring;
}

GeoDataLineString CoordinateConverter::toGdcStr(const Line& coords) const
{
  GeoDataLineString linestring;
  linestring.setTessellate(true);
  linestring.append(toGdc(coords.getPos1()));
  linestring.append(toGdc(coords.getPos2()));
  return linestring;
}

GeoDataLineString CoordinateConverter::toGdcStr(const LineString& coords) const
{
  GeoDataLineString linestring;
  linestring.setTessellate(true);

  for(const Pos& pos : coords)
    linestring.append(toGdc(pos));
  return linestring;
}

Pos CoordinateConverter::fromGdc(const GeoDataCoordinates& coords) const
{
  return Pos(coords.longitude(DEG), coords.latitude(DEG));
}

const LineString CoordinateConverter::fromGdcStr(const GeoDataLineString& coords) const
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
  bool visible = wToS(Marble::GeoDataCoordinates(coords.getLonX(), coords.getLatY(), 0, DEG), x, y, size, &hidden);
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
  bool visible = wToS(Marble::GeoDataCoordinates(coords.getLonX(), coords.getLatY(), 0, DEG), x, y, size, &hidden);
  if(isHidden != nullptr)
    *isHidden = hidden;
  return visible && !hidden;
}

bool CoordinateConverter::wToS(const Marble::GeoDataCoordinates& coords, double& x, double& y, const QSize& size, bool *isHidden) const
{
  return wToSInternal(coords, x, y, size, isHidden);
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

QPointF CoordinateConverter::wToSF(const GeoDataCoordinates& coords, const QSize& size, bool *visible, bool *isHidden) const
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

QVector<QPolygonF> CoordinateConverter::wToS(const Pos& pos1, const Pos& pos2) const
{
  GeoDataLineString lineStr;
  lineStr.setTessellate(true);
  lineStr << toGdc(pos1) << toGdc(pos2);
  return wToS(lineStr);
}

QVector<QPolygonF> CoordinateConverter::wToS(const Line& line) const
{
  GeoDataLineString lineStr;
  lineStr.setTessellate(true);
  lineStr << toGdc(line.getPos1()) << toGdc(line.getPos2());
  return wToS(lineStr);
}

QVector<QPolygonF> CoordinateConverter::wToS(const LineString& line) const
{
  GeoDataLineString lineStr;
  lineStr.setTessellate(true);
  for(const atools::geo::Pos& pos : line)
    lineStr << toGdc(pos);
  return wToS(lineStr);
}

QVector<QPolygonF> CoordinateConverter::wToS(const GeoDataLineString& line) const
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

bool CoordinateConverter::sToW(int x, int y, Marble::GeoDataCoordinates& coords) const
{
  qreal lon, lat;
  bool visible = viewport->geoCoordinates(x, y, lon, lat, DEG);

  coords.setLongitude(lon, DEG);
  coords.setLatitude(lat, DEG);
  return visible;
}

atools::geo::Pos CoordinateConverter::sToW(int x, int y) const
{
  qreal lon, lat;
  bool visible = viewport->geoCoordinates(x, y, lon, lat, DEG);

  if(visible)
    return atools::geo::Pos(lon, lat);
  else
    return atools::geo::Pos();
}

Pos CoordinateConverter::sToW(const QPoint& point) const
{
  return sToW(point.x(), point.y());
}

Pos CoordinateConverter::sToW(const QPointF& point) const
{
  return sToW(atools::roundToInt(point.x()), atools::roundToInt(point.y()));
}

bool CoordinateConverter::wToSInternal(const Marble::GeoDataCoordinates& coords, double& x, double& y, const QSize& size,
                                       bool *isHidden) const
{
  bool hidden;
  int numPoints;
  qreal xordinates[100];
  bool visible = viewport->screenCoordinates(coords, xordinates, y, numPoints, size, hidden);

  if(numPoints == 0)
    visible = viewport->screenCoordinates(coords, xordinates[0], y, hidden);

  // Do not paint repetitions for the Mercator projection
  // The fist coordinate is good enough here
  x = xordinates[0];

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
    polys.append(createPolygonsInternal(ls, screenRect));
  return polys;
}

const QVector<QPolygonF *> CoordinateConverter::createPolygonsInternal(const atools::geo::LineString& linestring,
                                                                       const QRectF& screenRect) const
{
  Marble::GeoDataLinearRing linearRing;
  linearRing.setTessellate(true);

  for(const Pos& pos : linestring)
    linearRing.append(Marble::GeoDataCoordinates(pos.getLonX(), pos.getLatY(), 0, DEG));

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
      if(!polygon->isClosed())
        polygon->append(polygon->constFirst());
    }
  }

  return polygons;
}

void CoordinateConverter::releasePolygons(const QVector<QPolygonF *>& polygons) const
{
  qDeleteAll(polygons);
}

const QVector<QPolygonF *> CoordinateConverter::createPolylines(const atools::geo::LineString& linestring, const QRectF& screenRect) const
{
  return createPolylinesInternal(linestring, screenRect);
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
                                                                        const QRectF& screenRect) const
{
  QVector<QPolygonF *> polylineVector;

  const float LATY_CORRECTION = 0.00001f;
  LineString correctedLines = linestring;

  // Avoid the straight line Marble draws wrongly for equal latitudes - needed to force GC path
  for(int i = 0; i < correctedLines.size() - 1; i++)
  {
    Pos& p1(correctedLines[i]);
    Pos& p2(correctedLines[i + 1]);

    if(atools::almostEqual(p1.getLatY(), p2.getLatY()) && std::abs(p1.getLonX() - p2.getLonX()) > 0.5f)
    {
      // Move latitude a bit up and down if equal and more than half a degree apart
      p1.setLatY(p1.getLatY() + LATY_CORRECTION);
      p2.setLatY(p2.getLatY() - LATY_CORRECTION);
    }
  }

  // Build Marble geometry object
  if(!correctedLines.isEmpty())
  {
    GeoDataLineString geoLineStr;
    geoLineStr.setTessellate(true);

    for(int i = 0; i < correctedLines.size() - 1; i++)
    {
      Line line(correctedLines.at(i), correctedLines.at(i + 1));

      // Split long lines to work around the buggy visibility check in Marble resulting in disappearing line segments
      // Do a quick check using Manhattan distance in degree
      LineString ls;
      if(line.lengthSimple() > 30.f)
        line.interpolatePoints(line.lengthMeter(), 20, ls);
      else if(line.lengthSimple() > 5.f)
        line.interpolatePoints(line.lengthMeter(), 5, ls);
      else
        ls.append(line.getPos1());

      // Append split points or single point
      for(const Pos& pos : qAsConst(ls))
        geoLineStr << GeoDataCoordinates(pos.getLonX(), pos.getLatY(), 0, DEG);
    }

    // Add last point
    geoLineStr << GeoDataCoordinates(correctedLines.constLast().getLonX(), correctedLines.constLast().getLatY(), 0, DEG);

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
