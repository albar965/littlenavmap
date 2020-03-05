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

#ifndef LNM_APRONGEOMETRYCACHE_H
#define LNM_APRONGEOMETRYCACHE_H

#include "fs/common/xpgeometry.h"

#include <QCache>
#include <QPainterPath>

class QPainterPath;
class CoordinateConverter;
namespace Marble {
class ViewportParams;
}
namespace map {
struct MapApron;

}

/*
 * Caches the complex X-Plane apron geometry in screen coordinates by zoom level and draw fast flag.
 *
 * This will keep a copy of a QPainterPath of an apron for a given zoomlevel until the cache overflows.
 */
class ApronGeometryCache
{
public:
  ApronGeometryCache();
  ~ApronGeometryCache();

  /* Get apron geometry in screen coordinates from the cache or create it from map::MapApron.
   * Combined key is apron ID, zoom and draw fast flag */
  QPainterPath getApronGeometry(const map::MapApron& apron, float zoomDistanceMeter, bool fast);

  /* Clear the cache */
  void clear();

  /* Has to be set before using it */
  void setViewportParams(const Marble::ViewportParams *viewport);

private:
  /* Cache key used to identify a QPainterPath for an apron */
  struct Key
  {
    Key(int apronIdParam, float zoomDistanceMeterParam, bool fastParam);

    int apronId;
    int zoomDistanceMeter;
    bool fast; /* Draw fast flag - no curves if true */

    bool operator!=(const ApronGeometryCache::Key& other) const;
    bool operator==(const ApronGeometryCache::Key& other) const;

  };

  friend uint qHash(const ApronGeometryCache::Key& key);

  /* Calculate X-Plane aprons including bezier curves */
  QPainterPath pathForBoundary(const atools::fs::common::Boundary& boundaryNodes, bool fast);

  /* Some airport have more than 100 apron parts */
  static const int CACHE_SIZE = 2000;

  /* Used to convert world to screen coordinates */
  CoordinateConverter *converter = nullptr;
  QCache<Key, QPainterPath> geometryCache;
};

#endif // LNM_APRONGEOMETRYCACHE_H
