/*****************************************************************************
* Copyright 2015-2018 Alexander Barthel albar965@mailbox.org
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

/* Sorts elements by distance to a point */
template<typename TYPE>
void removeByDistance(QList<TYPE>& list, const atools::geo::Pos& pos, int maxDistanceMeter)
{
  if(list.isEmpty() || !pos.isValid() || !(maxDistanceMeter < map::INVALID_INDEX_VALUE))
    return;

  auto it = std::remove_if(list.begin(), list.end(),
                           [ = ](const TYPE& type) -> bool
    {
      return type.getPosition().distanceMeterTo(pos) > maxDistanceMeter;
    });

  if(it != list.end())
    list.erase(it, list.end());

}

/* Sorts elements by distance to a point */
template<typename TYPE>
void removeByDistance(QList<TYPE>& list, const atools::geo::Pos& pos, float maxDistanceMeter)
{
  if(list.isEmpty() || !pos.isValid() || !(maxDistanceMeter < map::INVALID_DISTANCE_VALUE))
    return;

  auto it = std::remove_if(list.begin(), list.end(),
                           [ = ](const TYPE& type) -> bool
    {
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

  auto it = std::remove_if(list.begin(), list.end(),
                           [ = ](const TYPE& type) -> bool
    {
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
  if(list.isEmpty() || !pos.isValid())
    return;

  std::sort(list.begin(), list.end(),
            [ = ](const TYPE& t1, const TYPE& t2) -> bool
    {
      return t1.getPosition().distanceMeterTo(pos) < t2.getPosition().distanceMeterTo(pos);
    });
}

/* Functions will stop adding of number of elements exceeds this value */
static Q_DECL_CONSTEXPR int MAX_LIST_ENTRIES = 5;

/* Inserts element into list sorted by screen distance to xs/ys using ids set for deduplication */
template<typename TYPE>
void insertSortedByDistance(const CoordinateConverter& conv, QList<TYPE>& list, QSet<int> *ids,
                            int xs, int ys, TYPE type)
{
  if(list.size() > MAX_LIST_ENTRIES)
    return;

  if(ids == nullptr || !ids->contains(type.getId()))
  {
    auto it = std::lower_bound(list.begin(), list.end(), type,
                               [ = ](const TYPE& a1, const TYPE& a2) -> bool
      {
        int x1, y1, x2, y2;
        conv.wToS(a1.getPosition(), x1, y1);
        conv.wToS(a2.getPosition(), x2, y2);
        return atools::geo::manhattanDistance(x1, y1, xs, ys) <
        atools::geo::manhattanDistance(x2, y2, xs, ys);
      });
    list.insert(it, type);

    if(ids != nullptr)
      ids->insert(type.getId());
  }
}

/* Inserts elements from list into result sorted by screen distance to xs/ys using ids set for deduplication */
template<typename TYPE>
void insertSorted(const CoordinateConverter& conv, int xs, int ys, const QList<TYPE>& list, QList<TYPE>& result,
                  QSet<int> *ids, int maxDistance)
{
  int x, y;
  for(const TYPE& obj : list)
  {
    if(conv.wToS(obj.getPosition(), x, y))
      if((atools::geo::manhattanDistance(x, y, xs, ys)) < maxDistance)
        maptools::insertSortedByDistance(conv, result, ids, xs, ys, obj);
  }
}

/* Inserts element into list sorted by screen distance of tower to xs/ys using ids set for deduplication */
template<typename TYPE>
void insertSortedByTowerDistance(const CoordinateConverter& conv, QList<TYPE>& list, int xs, int ys,
                                 TYPE type)
{
  auto it = std::lower_bound(list.begin(), list.end(), type,
                             [ = ](const TYPE& a1, const TYPE& a2) -> bool
    {
      int x1, y1, x2, y2;
      conv.wToS(a1.towerCoords, x1, y1);
      conv.wToS(a2.towerCoords, x2, y2);
      return atools::geo::manhattanDistance(x1, y1, xs, ys) <
      atools::geo::manhattanDistance(x2, y2, xs, ys);
    });
  list.insert(it, type);
}

} // namespace maptools

#endif // LITTLENAVMAP_MAPTOOLS_H
