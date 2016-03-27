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

#ifndef MAPTOOLS_H
#define MAPTOOLS_H

#include "common/coordinateconverter.h"
#include "geo/calculations.h"

#include <QList>
#include <QSet>
#include <algorithm>
#include <functional>

class CoordinateConverter;

namespace maptools {

const int MAX_LIST_ENTRIES = 5;

template<typename TYPE>
void insertSortedByDistance(const CoordinateConverter& conv, QList<const TYPE *>& list, QSet<int> *ids,
                            int xs, int ys, const TYPE *type)
{
  if(type == nullptr || list.size() > MAX_LIST_ENTRIES)
    return;

  if(ids == nullptr || !ids->contains(type->getId()))
  {
    auto it = std::lower_bound(list.begin(), list.end(), type,
                               [ = ](const TYPE * a1, const TYPE * a2)->bool
                               {
                                 int x1, y1, x2, y2;
                                 conv.wToS(a1->getPosition(), x1, y1);
                                 conv.wToS(a2->getPosition(), x2, y2);
                                 return atools::geo::manhattanDistance(x1, y1, xs, ys) <
                                 atools::geo::manhattanDistance(x2, y2, xs, ys);
                               });
    list.insert(it, type);

    if(ids != nullptr)
      ids->insert(type->getId());
  }
}

template<typename TYPE>
void insertSortedByTowerDistance(const CoordinateConverter& conv, QList<const TYPE *>& list, int xs,
                                 int ys,
                                 const TYPE *type)
{
  auto it = std::lower_bound(list.begin(), list.end(), type,
                             [ = ](const TYPE * a1, const TYPE * a2)->bool
                             {
                               int x1, y1, x2, y2;
                               conv.wToS(a1->towerCoords, x1, y1);
                               conv.wToS(a2->towerCoords, x2, y2);
                               return atools::geo::manhattanDistance(x1, y1, xs, ys) <
                               atools::geo::manhattanDistance(x2, y2, xs, ys);
                             });
  list.insert(it, type);
}

} // namespace maptools

#endif // MAPTOOLS_H
