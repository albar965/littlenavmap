/*****************************************************************************
* Copyright 2015-2016 Alexander Barthel albar965@mailbox.org
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
                               [ = ](const TYPE &a1, const TYPE &a2)->bool
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

/* Inserts element into list sorted by screen distance of tower to xs/ys using ids set for deduplication */
template<typename TYPE>
void insertSortedByTowerDistance(const CoordinateConverter& conv, QList<TYPE>& list, int xs, int ys,
                                 TYPE type)
{
  auto it = std::lower_bound(list.begin(), list.end(), type,
                             [ = ](const TYPE &a1, const TYPE &a2)->bool
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
