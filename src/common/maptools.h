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

#ifndef LITTLENAVMAP_MAPTOOLS_H
#define LITTLENAVMAP_MAPTOOLS_H

#include "common/coordinateconverter.h"
#include "geo/calculations.h"
#include "common/mapflags.h"
#include "geo/pos.h"

#include <QList>
#include <QSet>
#include <algorithm>
#include <functional>

class CoordinateConverter;

namespace maptools {

/*
 * Container conversion functions for map::MapBase derived structs and objects
 */

/* Vector to hash with id as key */
template<typename TYPE>
QHash<int, TYPE> toHash(const QVector<TYPE>& list)
{
  QHash<int, TYPE> retval;
  for(const TYPE& t : list)
    retval.insert(t.id, t);

  return retval;
}

/* List to hash with id as key */
template<typename TYPE>
QHash<int, TYPE> toHash(const QList<TYPE>& list)
{
  QHash<int, TYPE> retval;
  for(const TYPE& t : list)
    retval.insert(t.id, t);

  return retval;
}

/* Insert vector in hash with id as key. Duplicates are removed. */
template<typename TYPE>
void insert(QHash<int, TYPE>& map, const QVector<TYPE>& list)
{
  for(const TYPE& t : list)
    map.insert(t.id, t);
}

/* Insert list in hash with id as key. Duplicates are removed. */
template<typename TYPE>
void insert(QHash<int, TYPE>& map, const QList<TYPE>& list)
{
  for(const TYPE& t : list)
    map.insert(t.id, t);
}

/* Copy other map to map removing duplicates */
template<typename TYPE>
void insert(QHash<int, TYPE>& map, const QHash<int, TYPE>& other)
{
  for(auto it = other.constBegin(); it != other.constEnd(); ++it)
    map.insert(it.key(), it.value());
}

/*
 * Distance sorting and other functions.
 */

/* Erase all elements in the list except the closest. Returns distance in meter to the closest */
template<typename TYPE>
float removeFarthest(const atools::geo::Pos& pos, QList<TYPE>& list)
{
  float closestDist = std::numeric_limits<float>::max();

  if(list.isEmpty())
    return closestDist;

  TYPE closestType;
  for(const TYPE& entry : list)
  {
    float dist = entry.getPosition().distanceMeterTo(pos);
    if(dist < closestDist)
    {
      closestType = entry;
      closestDist = dist;
    }
  }
  list.clear();
  list.append(closestType);
  return closestDist;
}

/* Erase all elements that are farther away than maxDistanceMeter */
template<typename TYPE>
void removeByDistance(QList<TYPE>& list, const atools::geo::Pos& pos, int maxDistanceMeter)
{
  if(list.isEmpty() || !pos.isValid() || !(maxDistanceMeter < map::INVALID_INDEX_VALUE))
    return;

  auto it = std::remove_if(list.begin(), list.end(), [maxDistanceMeter, &pos](const TYPE& type) -> bool {
      return type.getPosition().distanceMeterTo(pos) > maxDistanceMeter;
    });

  if(it != list.end())
    list.erase(it, list.end());

}

/* Erase all elements that are farther away than maxDistanceMeter */
template<typename TYPE>
void removeByDistance(QList<TYPE>& list, const atools::geo::Pos& pos, float maxDistanceMeter)
{
  if(list.isEmpty() || !pos.isValid() || !(maxDistanceMeter < map::INVALID_DISTANCE_VALUE))
    return;

  auto it = std::remove_if(list.begin(), list.end(), [maxDistanceMeter, &pos](const TYPE& type) -> bool {
      return type.getPosition().distanceMeterTo(pos) > maxDistanceMeter;
    });

  if(it != list.end())
    list.erase(it, list.end());

}

/* Sorts elements by distance to a point */
template<typename TYPE>
void removeByDirection(QList<TYPE>& list, const atools::geo::Pos& pos, int lastDirection)
{
  if(list.isEmpty() || !pos.isValid())
    return;

  auto it = std::remove_if(list.begin(), list.end(), [lastDirection, &pos](const TYPE& type) -> bool {
      int crs = 360 + atools::geo::normalizeCourse(type.getPosition().angleTo(pos));
      int crs2 = 360 + atools::geo::normalizeCourse(lastDirection);
      return atools::absInt(crs - crs2) > 120;
    });

  if(it != list.end())
    list.erase(it, list.end());

}

/* Sorts elements by distance to a point */
template<typename TYPE>
void sortByDistance(QList<TYPE>& list, const atools::geo::Pos& pos)
{
  if(list.size() <= 1 || !pos.isValid())
    return;

  std::sort(list.begin(), list.end(), [&pos](const TYPE& t1, const TYPE& t2) -> bool {
      return t1.getPosition().distanceMeterTo(pos) < t2.getPosition().distanceMeterTo(pos);
    });
}

template<typename TYPE>
void sortByDistance(QVector<TYPE>& list, const atools::geo::Pos& pos)
{
  if(list.size() <= 1 || !pos.isValid())
    return;

  std::sort(list.begin(), list.end(), [&pos](const TYPE& t1, const TYPE& t2) -> bool {
      return t1.getPosition().distanceMeterTo(pos) < t2.getPosition().distanceMeterTo(pos);
    });
}

/* Sorts elements by distance to a point including simple altitude difference */
template<typename TYPE>
void sortByDistanceAndAltitude(QList<TYPE>& list, const atools::geo::Pos& pos, float altitudeWeight = 1.f)
{
  if(list.size() <= 1 || !pos.isValid())
    return;

  std::sort(list.begin(), list.end(), [&pos, altitudeWeight](const TYPE& t1, const TYPE& t2) -> bool {
      return t1.getPosition().distanceMeterTo3d(pos, altitudeWeight) < t2.getPosition().distanceMeterTo3d(pos, altitudeWeight);
    });
}

template<typename TYPE>
void sortByDistanceAndAltitude(QVector<TYPE>& list, const atools::geo::Pos& pos, float altitudeWeight = 1.f)
{
  if(list.size() <= 1 || !pos.isValid())
    return;

  std::sort(list.begin(), list.end(), [&pos, altitudeWeight](const TYPE& t1, const TYPE& t2) -> bool {
      return t1.getPosition().distanceMeterTo3d(pos, altitudeWeight) < t2.getPosition().distanceMeterTo3d(pos, altitudeWeight);
    });
}

/* Functions will stop adding of number of elements exceeds this value */
static Q_DECL_CONSTEXPR int MAX_LIST_ENTRIES = 5;

/* Inserts element into list sorted by screen distance to xs/ys using ids set for deduplication */
template<typename TYPE>
void insertSortedByDistance(const CoordinateConverter& conv, QList<TYPE>& list, QSet<int> *ids, int xs, int ys, TYPE type)
{
  if(list.size() > MAX_LIST_ENTRIES)
    return;

  if(ids == nullptr || !ids->contains(type.getId()))
  {
    auto it = std::lower_bound(list.begin(), list.end(), type, [&conv, xs, ys](const TYPE& a1, const TYPE& a2) -> bool {
        int x1, y1, x2, y2;
        conv.wToS(a1.getPosition(), x1, y1);
        conv.wToS(a2.getPosition(), x2, y2);
        return atools::geo::manhattanDistance(x1, y1, xs, ys) < atools::geo::manhattanDistance(x2, y2, xs, ys);
      });
    list.insert(it, type);

    if(ids != nullptr)
      ids->insert(type.getId());
  }
}

/* Inserts elements from list into result sorted by screen distance to xs/ys using ids set for deduplication
 * Uses getPosition from the TYPE. */
template<typename TYPE>
void insertSorted(const CoordinateConverter& conv, int xs, int ys, const QList<TYPE>& list, QList<TYPE>& result,
                  QSet<int> *ids, int maxDistance)
{
  int x, y;
  for(const TYPE& obj : list)
  {
    if(obj.getPosition().isValid() && conv.wToS(obj.getPosition(), x, y))
    {
      if(atools::geo::manhattanDistance(x, y, xs, ys) < maxDistance)
        maptools::insertSortedByDistance(conv, result, ids, xs, ys, obj);
    }
  }
}

/* Inserts elements from list into result sorted by screen distance to xs/ys using ids set for deduplication.
 *  Uses getPositionTo() and getPositionFrom() from the TYPE. */
template<typename TYPE>
void insertSortedFromTo(const CoordinateConverter& conv, int xs, int ys, const QList<TYPE>& list, QList<TYPE>& result,
                        QSet<int> *ids, int maxDistance)
{
  int x, y;
  for(const TYPE& obj : list)
  {
    // Also deduplicates results preferring ones from getPositionTo()
    if(obj.getPosition().isValid() && conv.wToS(obj.getPositionTo(), x, y))
      if((atools::geo::manhattanDistance(x, y, xs, ys)) < maxDistance)
        maptools::insertSortedByDistance(conv, result, ids, xs, ys, obj);

    if(obj.getPosition().isValid() && conv.wToS(obj.getPositionFrom(), x, y))
      if((atools::geo::manhattanDistance(x, y, xs, ys)) < maxDistance)
        maptools::insertSortedByDistance(conv, result, ids, xs, ys, obj);
  }
}

/* Inserts element into list sorted by screen distance of tower to xs/ys using ids set for deduplication */
template<typename TYPE>
void insertSortedByTowerDistance(const CoordinateConverter& conv, QList<TYPE>& list, int xs, int ys,
                                 TYPE type)
{
  auto it = std::lower_bound(list.begin(), list.end(), type, [&conv, xs, ys](const TYPE& a1, const TYPE& a2) -> bool {
      int x1, y1, x2, y2;
      conv.wToS(a1.towerCoords, x1, y1);
      conv.wToS(a2.towerCoords, x2, y2);
      return atools::geo::manhattanDistance(x1, y1, xs, ys) < atools::geo::manhattanDistance(x2, y2, xs, ys);
    });
  list.insert(it, type);
}

template<typename TYPE>
bool containsId(const QList<TYPE>& list, int id)
{
  for(const TYPE& t : list)
    if(t.getId() == id)
      return true;

  return false;
}

template<typename TYPE>
TYPE byId(const QList<TYPE>& list, int id)
{
  for(const TYPE& t : list)
    if(t.getId() == id)
      return t;

  return TYPE();
}

template<typename TYPE>
void removeById(QList<TYPE>& list, int id)
{
  auto it = std::remove_if(list.begin(), list.end(), [id](const TYPE& p) -> bool {
      return p.getId() == id;
    });

  if(it != list.end())
    list.erase(it, list.end());
}

template<typename TYPE>
void removeDuplicatesById(QList<TYPE>& list)
{
  std::sort(list.begin(), list.end(), [](const TYPE& obj1, const TYPE& obj2) {
      return obj1.getId() < obj2.getId();
    });
  list.erase(std::unique(list.begin(), list.end()), list.end());
}

template<typename TYPE>
void removeDuplicatesById(QVector<TYPE>& vector)
{
  std::sort(vector.begin(), vector.end(), [](const TYPE& obj1, const TYPE& obj2) {
      return obj1.getId() < obj2.getId();
    });
  vector.erase(std::unique(vector.begin(), vector.end()), vector.end());
}

// ==============================================================================
/* Runway sorting tools. Allows to sort runways by headwind and crosswind */
struct RwEnd
{
  RwEnd(const QString& name, const QString& surface, int lengthParam, float headWindParam, float crossWindParam);
  RwEnd()
  {

  }

  QStringList names;
  bool softRunway;
  int crossWind, headWind, minlength, maxlength;
};

/* List of runway ends */
class RwVector
  : public QVector<maptools::RwEnd>
{
public:
  RwVector(float windSpeed, float windDirectionDeg)
    : speed(windSpeed), direction(windDirectionDeg)
  {

  }

  /* Add runway to the list - will be omitted if wind is too low */
  void appendRwEnd(const QString& name, const QString& surface, int length, float heading);

  /* Sort runway ends by headwind and crosswind and combine ends with the same wind. */
  void sortRunwayEnds();

  /* Covers merged runway numbers with equal wind/heading too */
  int getTotalNumber() const
  {
    return totalNumber;
  }

  /* All runways with a headwind below will be omitted. Default is two knots. */
  void setMinHeadWindSpeed(float value)
  {
    minSpeed = value;
  }

  /* Get names numerically sorted having minHeadWind */
  QStringList getSortedRunways(int minHeadWind);

private:
  float speed, direction, minSpeed = 0.5f;
  int totalNumber = 0;
};

} // namespace maptools

#endif // LITTLENAVMAP_MAPTOOLS_H
