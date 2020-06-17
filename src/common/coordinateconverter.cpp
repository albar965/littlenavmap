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

#include "common/coordinateconverter.h"

#include "atools.h"
#include "geo/pos.h"
#include "geo/line.h"
#include "geo/linestring.h"

#include <marble/GeoDataLineString.h>
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

CoordinateConverter::~CoordinateConverter()
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
  return GeoDataLatLonBox(coords.getNorth(), coords.getSouth(),
                          coords.getEast(), coords.getWest(), DEG);
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

LineString CoordinateConverter::fromGdcStr(const GeoDataLineString& coords) const
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

bool CoordinateConverter::wToS(const atools::geo::Pos& coords, int& x, int& y, const QSize& size,
                               bool *isHidden) const
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

bool CoordinateConverter::wToS(const atools::geo::Pos& coords, float& x, float& y, const QSize& size,
                               bool *isHidden) const
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

bool CoordinateConverter::wToS(const atools::geo::Line& coords, QLineF& line, const QSize& size,
                               bool *isHidden) const
{
  if(!coords.isValid())
  {
    line = QLineF();
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

bool CoordinateConverter::wToS(const atools::geo::Pos& coords, double& x, double& y, const QSize& size,
                               bool *isHidden) const
{
  if(!coords.isValid())
  {
    x = y = 0.;
    return false;
  }

  bool hidden;
  bool visible = wToS(Marble::GeoDataCoordinates(coords.getLonX(), coords.getLatY(), 0, DEG), x, y, size, &hidden);
  if(isHidden != nullptr)
    *isHidden = hidden;
  return visible && !hidden;
}

bool CoordinateConverter::wToS(const Marble::GeoDataCoordinates& coords, double& x, double& y,
                               const QSize& size, bool *isHidden) const
{
  return wToSInternal(coords, x, y, size, isHidden);
}

bool CoordinateConverter::wToS(const Marble::GeoDataCoordinates& coords, int& x, int& y, const QSize& size,
                               bool *isHidden) const
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

QPointF CoordinateConverter::wToSF(const atools::geo::Pos& coords, const QSize& size, bool *visible,
                                   bool *isHidden) const
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

QPoint CoordinateConverter::wToS(const atools::geo::Pos& coords, const QSize& size, bool *visible,
                                 bool *isHidden) const
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

QPoint CoordinateConverter::wToS(const Marble::GeoDataCoordinates& coords, const QSize& size, bool *visible,
                                 bool *isHidden) const
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

QPointF CoordinateConverter::wToSF(const GeoDataCoordinates& coords, const QSize& size, bool *visible,
                                   bool *isHidden) const
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
  for(const QPolygonF *poly : polygons)
    retval.append(*poly);
  qDeleteAll(polygons);

  return retval;
}

bool CoordinateConverter::sToW(int x, int y, atools::geo::Pos& pos) const
{
  qreal lon, lat;
  bool visible = viewport->geoCoordinates(x, y, lon, lat, DEG);

  pos.setLonX(lon);
  pos.setLatY(lat);
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

bool CoordinateConverter::wToSInternal(const Marble::GeoDataCoordinates& coords, double& x, double& y,
                                       const QSize& size, bool *isHidden) const
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
