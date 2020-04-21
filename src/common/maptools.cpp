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

#include "common/maptools.h"

#include "common/maptypes.h"

namespace maptools {

struct RwKey
{
  RwKey(const RwEnd& end)
  {
    head = end.head;
    cross = end.cross;
    soft = end.soft;
  }

  int head, cross;
  bool soft;
};

inline bool operator<(const RwKey& key1, const RwKey& key2)
{
  if(key1.head != key2.head)
    // Sort first by headwind
    return key1.head > key2.head;
  else if(key1.soft != key2.soft)
    // Hard runways to front
    return key1.soft < key2.soft;
  else
    // Last criteria crosswind
    return key1.cross > key2.cross;
}

RwEnd::RwEnd(const QString& name, const QString& surf, int lengthParam, float headWind, float crossWind)
{
  minlength = maxlength = lengthParam;
  names.append(name);
  soft = map::isSoftSurface(surf) || map::isWaterSurface(surf);

  head = atools::roundToInt(headWind);

  // Don't care about crosswind direction
  cross = atools::roundToInt(std::abs(crossWind));
}

void RwVector::appendRwEnd(const QString& name, const QString& surface, int length, float heading)
{
  if(speed >= minSpeed)
  {
    float headWind, crossWind;
    atools::geo::windForCourse(headWind, crossWind, speed, direction, heading);

    if(headWind > minSpeed)
    {
      RwEnd end(name, surface, length, headWind, crossWind);
      append(end);
    }
  }
}

void RwVector::sortRunwayEnds()
{
  totalNumber = 0;

  if(size() > 1)
  {
    QMap<RwKey, RwEnd> endMap;

    for(const RwEnd& end : *this)
    {
      RwKey key(end);
      if(endMap.contains(key))
      {
        RwEnd& e = endMap[key];

        // Merge entries
        e.names.append(end.names);
        e.minlength = std::min(e.minlength, end.minlength);
        e.maxlength = std::max(e.maxlength, end.maxlength);
      }
      else
        endMap.insert(key, end);
    }

    clear();

    for(const RwEnd& end :  endMap.values())
    {
      append(end);
      totalNumber += end.names.size();
    }
  }
  else if(size() == 1)
    totalNumber = first().names.size();
}

} // namespace maptools
