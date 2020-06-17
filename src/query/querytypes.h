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

#ifndef LNM_QUERYTYPES_H
#define LNM_QUERYTYPES_H

#include "sql/sqlrecord.h"
#include "sql/sqlquery.h"
#include "common/maptypes.h"

#include <QList>

#include <functional>

#include <marble/GeoDataCoordinates.h>
#include <marble/GeoDataLatLonBox.h>

namespace atools {
namespace geo {
class Rect;
}
namespace sql {
class SqlQuery;
}
}

class MapLayer;

namespace query {

void bindRect(const Marble::GeoDataLatLonBox& rect, atools::sql::SqlQuery *query, const QString& prefix = QString());
void bindRect(const atools::geo::Rect& rect, atools::sql::SqlQuery *query, const QString& prefix = QString());

/* Run query for rect potentially splitting at anti-meridian and call callback */
void fetchObjectsForRect(const atools::geo::Rect& rect, atools::sql::SqlQuery *query,
                         std::function<void(atools::sql::SqlQuery *query)> callback);

QList<Marble::GeoDataLatLonBox> splitAtAntiMeridian(const Marble::GeoDataLatLonBox& rect, double factor = 0.,
                                                    double increment = 0.);

/* Inflate rect by width and height in degrees. If it crosses the poles or date line it will be limited */
void inflateQueryRect(Marble::GeoDataLatLonBox& rect, double factor, double increment);

template<typename ID>
const atools::sql::SqlRecord *cachedRecord(QCache<ID, atools::sql::SqlRecord>& cache,
                                           atools::sql::SqlQuery *query, ID id);

template<typename ID>
const atools::sql::SqlRecordVector *cachedRecordVector(QCache<ID, atools::sql::SqlRecordVector>& cache,
                                                       atools::sql::SqlQuery *query, ID id);

/* Simple spatial cache that deals with objects in a bounding rectangle but does not run any queries to load data */
template<typename TYPE>
struct SimpleRectCache
{
  typedef std::function<bool (const MapLayer *curLayer, const MapLayer *mapLayer)> LayerCompareFunc;

  /*
   * @param rect bounding rectangle - all objects inside this rectangle are returned
   * @param mapLayer current map layer
   * @param lazy if true do not fetch new data but return the old potentially incomplete dataset
   * @return true after clearing the cache. The caller has to request new data
   */
  bool updateCache(const Marble::GeoDataLatLonBox& rect, const MapLayer *mapLayer, double factor, double increment,
                   bool lazy,
                   LayerCompareFunc funcSameLayer);
  void clear();
  void validate(int queryMaxRows);

  Marble::GeoDataLatLonBox curRect;
  const MapLayer *curMapLayer = nullptr;
  QList<TYPE> list;

};

// ---------------------------------------------------------------------------------

template<typename TYPE>
bool SimpleRectCache<TYPE>::updateCache(const Marble::GeoDataLatLonBox& rect, const MapLayer *mapLayer, double factor,
                                        double increment, bool lazy, LayerCompareFunc funcSameLayer)
{
  if(lazy)
    // Nothing changed11
    return false;

  // Store bounding rectangle and inflate it
  Marble::GeoDataLatLonBox cur(curRect);
  query::inflateQueryRect(cur, factor, increment);
#ifndef DEBUG_DISABLE_RECT_CACHE
  if(curRect.isEmpty() || !cur.contains(rect) || !funcSameLayer(curMapLayer, mapLayer))
#else
  Q_UNUSED(funcSameLayer)
#endif
  {
    // Rectangle not covered by loaded data or new layer selected
    list.clear();
    curRect = rect;
    curMapLayer = mapLayer;
    return true;
  }
  return false;
}

template<typename TYPE>
void SimpleRectCache<TYPE>::validate(int queryMaxRows)
{
  if(list.size() >= queryMaxRows)
  {
    curRect.clear();
    curMapLayer = nullptr;
  }
}

template<typename TYPE>
void SimpleRectCache<TYPE>::clear()
{
  list.clear();
  curRect.clear();
  curMapLayer = nullptr;
}

/* Get a record from the cache or get it from a database query */
template<typename ID>
const atools::sql::SqlRecord *cachedRecord(QCache<ID, atools::sql::SqlRecord>& cache, atools::sql::SqlQuery *query,
                                           ID id)
{
  atools::sql::SqlRecord *rec = cache.object(id);
  if(rec != nullptr)
  {
    // Found record in cache
    if(rec->isEmpty())
      // Empty record that indicates that no result was found
      return nullptr;
    else
      return rec;
  }
  else
  {
    query->exec();
    if(query->next())
    {
      // Insert it into the cache
      rec = new atools::sql::SqlRecord(query->record());
      cache.insert(id, rec);
    }
    else
      // Add empty record to indicate nothing found for this id
      cache.insert(id, new atools::sql::SqlRecord());
  }
  query->finish();
  return rec;
}

/* Get a record vector from the cache of get it from a database query */
template<typename ID>
const atools::sql::SqlRecordVector *cachedRecordVector(QCache<ID, atools::sql::SqlRecordVector>& cache,
                                                       atools::sql::SqlQuery *query, ID id)
{
  atools::sql::SqlRecordVector *rec = cache.object(id);
  if(rec != nullptr)
  {
    // Found record in cache
    if(rec->isEmpty())
      // Empty record that indicates that no result was found
      return nullptr;
    else
      return rec;
  }
  else
  {
    query->exec();

    rec = new atools::sql::SqlRecordVector;

    while(query->next())
      rec->append(query->record());

    // Insert it into the cache
    cache.insert(id, rec);

    if(rec->isEmpty())
      return nullptr;
    else
      return rec;
  }
  query->finish();
  // Keep this here although it is never executed since some compilers throw an error
  return nullptr;
}

/* Key for nearestCache combining all query parameters */
struct NearestCacheKeyNavaid
{
  atools::geo::Pos pos;
  float distanceNm;
  map::MapObjectTypes type;

  bool operator==(const query::NearestCacheKeyNavaid& other) const
  {
    return pos == other.pos && std::abs(distanceNm - other.distanceNm) < 0.01 && type == other.type;
  }

  bool operator!=(const query::NearestCacheKeyNavaid& other) const
  {
    return !operator==(other);
  }

};

inline uint qHash(const query::NearestCacheKeyNavaid& key)
{
  return qHash(key.pos) ^ qHash(key.type) ^ ::qHash(key.distanceNm);
}

} // namespace query

#endif // LNM_QUERYTYPES_H
