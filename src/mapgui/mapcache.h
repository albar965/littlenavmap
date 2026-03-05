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

#ifndef LNM_MAPCACHE_H
#define LNM_MAPCACHE_H

#include "atools.h"
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

class TaxiNameKey
{
public:
  explicit TaxiNameKey(int airportIdParam, const QString& taxiNameParam)
    : airportId(airportIdParam), taxiName(taxiNameParam)
  {

  }

  bool operator==(const TaxiNameKey& other) const
  {
    return airportId == other.airportId && taxiName == other.taxiName;
  }

  bool operator!=(const TaxiNameKey& other) const
  {
    return !operator==(other);
  }

private:
  friend size_t qHash(const TaxiNameKey& key, size_t seed);

  int airportId;
  QString taxiName;
};

class TaxiNameValue
{

public:
  explicit TaxiNameValue(const QPointF& airportPointParam, const QPointF& taxiTextPointParam)
    : airportPoint(airportPointParam), taxiTextPoint(taxiTextPointParam)
  {
  }

  const QPointF& getAirportPoint() const
  {
    return airportPoint;
  }

  const QPointF& getTaxiTextPoint() const
  {
    return taxiTextPoint;
  }

private:
  QPointF airportPoint, taxiTextPoint;
};

/*
 * General cache for map display features in screen space.
 *
 * Caches the complex X-Plane apron geometry in screen coordinates by zoom level and draw fast flag.
 * This will keep a copy of a QPainterPath of an apron for a given zoomlevel until the cache overflows.
 *
 * Also caches draw points for taxiway labels when moving or zooming map.
 */
class MapCache
{
public:
  MapCache();
  ~MapCache();

  MapCache(const MapCache& other) = delete;
  MapCache& operator=(const MapCache& other) = delete;

  /* Get apron geometry in screen coordinates from the cache or create it from map::MapApron.
   * Combined key is apron ID, zoom and draw fast flag */
  QPainterPath getApronGeometry(const map::MapApron& apron, float zoomDistanceMeter, bool fast);

  /* Clear the cache */
  void clear();

  /* Has to be set before using it */
  void setViewportParams(const Marble::ViewportParams *viewport);

  /* Caches taxiway names and draw positions. Key is airport id and taxiway name. */
  QMultiHash<TaxiNameKey, TaxiNameValue> *getTaxiNameCache()
  {
    return &airportTaxiNameCache;
  }

private:
  /* Internal cache key used to identify a QPainterPath for an apron */
  class ApronKey
  {
public:
    ApronKey(int apronIdParam, float zoomDistanceMeterParam, bool fastParam)
      : apronId(apronIdParam), zoomDistanceMeter(static_cast<int>(zoomDistanceMeterParam)), fast(fastParam)
    {

    }

    bool operator==(const MapCache::ApronKey& other) const
    {
      return apronId == other.apronId && fast == other.fast && atools::almostEqual(zoomDistanceMeter, other.zoomDistanceMeter, 5.f);
    }

    bool operator!=(const MapCache::ApronKey& other) const
    {
      return !(*this == other);
    }

private:
    friend size_t qHash(const MapCache::ApronKey& key, size_t seed);

    int apronId;
    float zoomDistanceMeter;
    bool fast; /* Draw fast flag - no curves if true */
  };

  friend size_t qHash(const MapCache::ApronKey& key, size_t seed);

  /* Calculate X-Plane aprons including bezier curves */
  QPainterPath pathForBoundary(const atools::fs::common::Boundary& boundaryNodes, bool fast);

  /* Some airport have more than 100 apron parts */
  static const int CACHE_SIZE = 2000;

  /* Used to convert world to screen coordinates */
  CoordinateConverter *converter = nullptr;
  QCache<ApronKey, QPainterPath> geometryCache;

  /* Maps airport id / taxiway name to airport and taxi label position when drawing */
  QMultiHash<TaxiNameKey, TaxiNameValue> airportTaxiNameCache;
};

#endif // LNM_MAPCACHE_H
