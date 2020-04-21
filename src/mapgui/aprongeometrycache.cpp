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

#include "mapgui/aprongeometrycache.h"
#include "fs/common/xpgeometry.h"
#include "common/coordinateconverter.h"
#include "common/maptypes.h"

#include <QPainterPath>

// ======= Key  ===============================================================
uint qHash(const ApronGeometryCache::Key& key)
{
  return static_cast<uint>(key.apronId) ^ key.fast ^ static_cast<uint>(key.zoomDistanceMeter);
}

ApronGeometryCache::Key::Key(int apronIdParam, float zoomDistanceMeterParam, bool fastParam)
  : apronId(apronIdParam), zoomDistanceMeter(static_cast<int>(zoomDistanceMeterParam)), fast(fastParam)
{

}

bool ApronGeometryCache::Key::operator==(const ApronGeometryCache::Key& other) const
{
  return apronId == other.apronId && fast == other.fast && zoomDistanceMeter == other.zoomDistanceMeter;
}

bool ApronGeometryCache::Key::operator!=(const ApronGeometryCache::Key& other) const
{
  return !(*this == other);
}

// ======= ApronGeometryCache ===============================================================
ApronGeometryCache::ApronGeometryCache()
  : geometryCache(CACHE_SIZE)
{

}

ApronGeometryCache::~ApronGeometryCache()
{
  delete converter;
}

void ApronGeometryCache::clear()
{
  geometryCache.clear();
}

void ApronGeometryCache::setViewportParams(const Marble::ViewportParams *viewport)
{
  if(converter != nullptr)
    delete converter;

  // Create a new converter for the viewport
  converter = new CoordinateConverter(viewport);
}

QPainterPath ApronGeometryCache::getApronGeometry(const map::MapApron& apron, float zoomDistanceMeter, bool fast)
{
  Q_ASSERT(converter != nullptr);

#if !defined(DEBUG_NO_XP_APRON_CACHE)

  // Build key and get path from the cache
  Key key(apron.id, zoomDistanceMeter, fast);
  QPainterPath *painterPath = geometryCache.object(key);

  if(painterPath != nullptr)
  {
    // Found - create a copy and translate it to the needed coordinates
    QPainterPath boundaryPath(*painterPath);

    // Calculate the coordinates of the reference point (first one) and move the whole path into required place for drawing
    bool visible;
    QPointF refPoint = converter->wToSF(apron.geometry.boundary.first().node,
                                        CoordinateConverter::DEFAULT_WTOS_SIZE, &visible);
    boundaryPath.translate(refPoint.x(), refPoint.y());

    return boundaryPath;
  }
  else
#endif
  {
    // qDebug() << Q_FUNC_INFO << "Creating new apron";

    // Nothing in cache - create the apron boundary
    QPainterPath boundaryPath = pathForBoundary(apron.geometry.boundary, fast);

    // Substract holes
    for(atools::fs::common::Boundary hole : apron.geometry.holes)
      boundaryPath = boundaryPath.subtracted(pathForBoundary(hole, fast));

#if !defined(DEBUG_NO_XP_APRON_CACHE)

    // Calculate reference point (first)
    bool visible;
    QPointF refPoint = converter->wToSF(
      apron.geometry.boundary.first().node, CoordinateConverter::DEFAULT_WTOS_SIZE, &visible);

    // Create a copy for the cache
    painterPath = new QPainterPath(boundaryPath);

    // Move the whole path, so that the reference is at 0,0
    painterPath->translate(-refPoint.x(), -refPoint.y());

    // Insert path with reference 0,0
    geometryCache.insert(key, painterPath);
#endif

    // Return copy with correct position for drawing
    return boundaryPath;
  }
}

/* Calculate X-Plane aprons including bezier curves */
QPainterPath ApronGeometryCache::pathForBoundary(const atools::fs::common::Boundary& boundaryNodes, bool fast)
{
  bool visible;
  QPainterPath apronPath;
  atools::fs::common::Node lastNode;

  // Create a copy and close the geometry
  atools::fs::common::Boundary boundary = boundaryNodes;

  if(!boundary.isEmpty())
    boundary.append(boundary.first());

  int i = 0;
  for(const atools::fs::common::Node& node : boundary)
  {
    QPointF lastPt = converter->wToSF(lastNode.node, CoordinateConverter::DEFAULT_WTOS_SIZE, &visible);
    QPointF pt = converter->wToSF(node.node, CoordinateConverter::DEFAULT_WTOS_SIZE, &visible);

    if(i == 0)
      // First point
      apronPath.moveTo(converter->wToS(node.node, CoordinateConverter::DEFAULT_WTOS_SIZE, &visible));
    else if(fast)
      // Use lines only for fast drawing
      apronPath.lineTo(pt);
    else
    {
      if(lastNode.control.isValid() && node.control.isValid())
      {
        // Two successive control points - use cubic curve
        QPointF controlPoint1 = converter->wToSF(lastNode.control, CoordinateConverter::DEFAULT_WTOS_SIZE, &visible);
        QPointF controlPoint2 = converter->wToSF(node.control, CoordinateConverter::DEFAULT_WTOS_SIZE, &visible);
        apronPath.cubicTo(controlPoint1, pt + (pt - controlPoint2), pt);
      }
      else if(lastNode.control.isValid())
      {
        // One control point from last - use quad curve
        if(lastPt != pt)
          apronPath.quadTo(converter->wToSF(lastNode.control,
                                            CoordinateConverter::DEFAULT_WTOS_SIZE, &visible), pt);
      }
      else if(node.control.isValid())
      {
        // One control point from current - use quad curve
        if(lastPt != pt)
          apronPath.quadTo(pt + (pt - converter->wToSF(node.control,
                                                       CoordinateConverter::DEFAULT_WTOS_SIZE, &visible)), pt);
      }
      else
        // No control point - simple line
        apronPath.lineTo(pt);
    }

    lastNode = node;
    i++;
  }
  return apronPath;
}
