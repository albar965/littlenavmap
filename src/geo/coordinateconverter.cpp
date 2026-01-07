/*****************************************************************************
* Copyright 2015-2026 Alexander Barthel alex@littlenavmap.org
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

#include "geo/coordinateconverter.h"

#include "atools.h"
#include "geo/marbleconverter.h"
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

bool CoordinateConverter::isHidden(const atools::geo::Pos& coords) const
{
  qreal xr, yr;
  bool hidden = false;
  wToS(coords, xr, yr, DEFAULT_WTOS_SIZE, &hidden);
  return hidden;
}

QRectF CoordinateConverter::correctBounding(const QRectF& boundingRect)
{
  const static QMarginsF MARGINS(1., 1., 1., 1.);
  return boundingRect.normalized().marginsAdded(MARGINS);
}

QRect CoordinateConverter::correctBounding(const QRect& boundingRect)
{
  const static QMargins MARGINS(1, 1, 1, 1);
  return boundingRect.normalized().marginsAdded(MARGINS);
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
  bool visible = wToS(mconvert::toGdc(coords), x, y, size, &hidden);
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
  bool visible = wToS(mconvert::toGdc(coords), x, y, size, &hidden);
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

QList<QPolygonF> CoordinateConverter::wToS(const atools::geo::Pos& pos1, const atools::geo::Pos& pos2) const
{
  return wToS(mconvert::toGdcStr(Line(pos1, pos2)));
}

QList<QPolygonF> CoordinateConverter::wToS(const atools::geo::Line& line) const
{
  return wToS(mconvert::toGdcStr(line));
}

QList<QPolygonF> CoordinateConverter::wToS(const atools::geo::LineString& linestring) const
{
  return wToS(mconvert::toGdcStr(linestring));
}

QList<QPolygonF> CoordinateConverter::wToS(const Marble::GeoDataLineString& line) const
{
  QList<QPolygonF *> polygons;
  viewport->screenCoordinates(line, polygons);

  // Copy polygons and delete pointers
  QList<QPolygonF> retval;
  for(const QPolygonF *poly : std::as_const(polygons))
    retval.append(*poly);
  qDeleteAll(polygons);

  return retval;
}

bool CoordinateConverter::sToW(int x, int y, atools::geo::Pos& pos) const
{
  qreal lon, lat;
  bool visible = viewport->geoCoordinates(x, y, lon, lat, mconvert::DEG);

  pos.setLonX(static_cast<float>(lon));
  pos.setLatY(static_cast<float>(lat));
  return visible;
}

atools::geo::Pos CoordinateConverter::sToW(int x, int y) const
{
  qreal lon, lat;
  bool visible = viewport->geoCoordinates(x, y, lon, lat, mconvert::DEG);
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

bool CoordinateConverter::wToSPoints(const atools::geo::Pos& pos, QList<double>& x, double& y, const QSize& size, bool *isHidden) const
{
  return wToSPoints(mconvert::toGdc(pos), x, y, size, isHidden);
}

bool CoordinateConverter::wToSPoints(const atools::geo::Pos& pos, QList<float>& x, float& y, const QSize& size, bool *isHidden) const
{
  QList<double> xDouble;
  double yDouble;
  bool retval = wToSPoints(mconvert::toGdc(pos), xDouble, yDouble, size, isHidden);
  for(double xd : std::as_const(xDouble))
    x.append(static_cast<float>(xd));
  y = static_cast<float>(yDouble);
  return retval;
}

bool CoordinateConverter::wToSPoints(const Marble::GeoDataCoordinates& coords, QList<double>& x, double& y, const QSize& size,
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

const QList<QPolygonF *> CoordinateConverter::createPolygons(const atools::geo::LineString& linestring, const QRectF& screenRect) const
{
  QList<QPolygonF *> polys;
  for(const LineString& ls : linestring.splitAtAntiMeridianList())
  {
    const QList<QPolygonF *> polyVector = createPolygonsInternal(ls, screenRect);

    if(!polyVector.isEmpty())
      polys.append(polyVector);
  }
  return polys;
}

const QList<QPolygonF *> CoordinateConverter::createPolygonsInternal(const atools::geo::LineString& linestring,
                                                                     const QRectF& screenRect) const
{
  Marble::GeoDataLinearRing linearRing = mconvert::toGdcRing(linestring);

  QList<QPolygonF *> polygons;
  if(viewport->viewLatLonAltBox().intersects(linearRing.latLonAltBox()) && viewport->resolves(linearRing.latLonAltBox()))
    // Function might return two polygons in Mercator where one is not visible on the screen
    // Polygons will contain more points than the world coordinate polygon
    viewport->screenCoordinates(linearRing, polygons);

  if(polygons.size() > 1)
  {
    // Remove all invisible polygons from end to start
    QPolygonF screenPolygon(screenRect);
    for(int i = polygons.size() - 1; i >= 0; i--)
    {
      const QPolygonF *polygon = polygons.at(i);
      if(!polygon->intersects(screenPolygon))
      {
        delete polygon;
        polygons.remove(i);
      }
    }

    // Remove all points which are too close together
    for(QPolygonF *polygon : std::as_const(polygons))
    {
      // Remove all consecutive duplicate points from the range
      polygon->erase(std::unique(polygon->begin(), polygon->end(), [](const QPointF& p1, const QPointF& p2) -> bool {
        return atools::almostEqual(p1.x(), p2.x(), 0.5) && atools::almostEqual(p1.y(), p2.y(), 0.5);
      }), polygon->end());

      // Close again
      if(!polygon->isClosed() && !polygon->isEmpty())
        polygon->append(polygon->constFirst());
    }
  }

  return polygons;
}

bool CoordinateConverter::resolves(const Marble::GeoDataLatLonBox& box) const
{
  // boxes alsmost spanning the whole globe are always in range
  return viewport->resolves(box) && box.width(GeoDataCoordinates::Degree) > 359. ? true : viewport->viewLatLonAltBox().intersects(box);
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
  return resolves(mconvert::toGdc(rect));
}

bool CoordinateConverter::resolves(const atools::geo::Line& line) const
{
  return resolves(mconvert::toGdc(line.getPos1()), mconvert::toGdc(line.getPos2()));
}

const QList<QPolygonF *> CoordinateConverter::createPolylinesInternal(const atools::geo::LineString& linestring,
                                                                      const QRectF& screenRect, bool splitLongLines) const
{
  QList<QPolygonF *> polylineVector;

  // Build Marble geometry object
  if(!linestring.isEmpty())
  {
    GeoDataLineString geoLineStr;
    geoLineStr.setTessellate(true);

    if(splitLongLines)
    {
      for(int i = 0; i < linestring.size() - 1; i++)
      {
        for(const Line& splitLine : atools::geo::splitAtAntiMeridian(linestring.at(i), linestring.at(i + 1)))
        {
          // Split long lines to work around the buggy visibility check in Marble resulting in disappearing line segments
          // Do a quick check using Manhattan distance in degree
          LineString lines;
          if(splitLine.lengthSimple() > 30.f)
            splitLine.interpolatePoints(splitLine.lengthMeter(), 20, lines);
          else if(splitLine.lengthSimple() > 5.f)
            splitLine.interpolatePoints(splitLine.lengthMeter(), 5, lines);
          else
            lines.append(splitLine.getPos1());

          // Append split points or single point
          for(const Pos& pos : std::as_const(lines))
            geoLineStr << mconvert::toGdc(pos);
        }
      }
    }
    else
    {
      // Skip generation if intermediate points and create direct
      for(const Pos& pos : linestring)
        geoLineStr << mconvert::toGdc(pos);
    }

    // Add last point
    geoLineStr << mconvert::toGdc(linestring.constLast());

#ifdef DEBUG_INFORMATION_LINERENDER
    qDebug() << Q_FUNC_INFO << "=========================================";
    for(const GeoDataCoordinates& c : geoLineStr)
      qDebug() << Q_FUNC_INFO << "long" << c.longitude(GeoDataCoordinates::Degree) << "lat" << c.latitude(GeoDataCoordinates::Degree);
#endif

    // Build polyline vector ==============================================
    QList<GeoDataLineString *> geoLineStrCorrected = geoLineStr.toDateLineCorrected();
    for(const GeoDataLineString *geoLine : std::as_const(geoLineStrCorrected))
    {
      if(viewport->viewLatLonAltBox().intersects(geoLine->latLonAltBox()))
      {
        QList<QPolygonF *> polygons;
        viewport->screenCoordinates(*geoLine, polygons);

        if(!polygons.isEmpty())
        {
          // Remove invisible polylines by bounding rect first - intersects might crash on large polygons - from end to start
          for(int i = polygons.size() - 1; i >= 0; i--)
          {
            const QPolygonF *polygon = polygons.at(i);
            if(polygon == nullptr || polygon->isEmpty() || !correctBounding(polygon->boundingRect()).intersects(screenRect))
            {
              delete polygon;
              polygons.remove(i);
            }
          }

          if(!polygons.isEmpty())
          {
            // Now do more accurate removal from end to start
            QPolygonF screenPolygon(screenRect);
            for(int i = polygons.size() - 1; i >= 0; i--)
            {
              const QPolygonF *polygon = polygons.at(i);
              if(polygon == nullptr || polygon->isEmpty() || !polygon->intersects(screenPolygon))
              {
                delete polygon;
                polygons.remove(i);
              }
            }

            if(!polygons.isEmpty())
              polylineVector.append(polygons);
          }
        }
      }
    }
    qDeleteAll(geoLineStrCorrected);
  }
  return polylineVector;
}
