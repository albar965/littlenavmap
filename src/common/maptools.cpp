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
#include "fs/util/fsutil.h"

namespace maptools {

struct RwKey
{
  RwKey(const RwEnd& end)
  {
    headWind = end.headWind;
    crossWind = end.crossWind;
    softRunway = end.softRunway;
  }

  int headWind, crossWind;
  bool softRunway;
};

inline bool operator<(const RwKey& key1, const RwKey& key2)
{
  if(key1.headWind != key2.headWind)
    // Sort first by headwind
    return key1.headWind > key2.headWind;
  else if(key1.softRunway != key2.softRunway)
    // Hard runways to front
    return key1.softRunway < key2.softRunway;
  else
    // Last criteria crosswind
    return key1.crossWind > key2.crossWind;
}

RwEnd::RwEnd(const QString& name, const QString& surface, int lengthParam, float headWindParam, float crossWindParam)
{
  minlength = maxlength = lengthParam;
  names.append(name);
  softRunway = map::isSoftSurface(surface) || map::isWaterSurface(surface);

  headWind = atools::roundToInt(headWindParam);

  // Don't care about crosswind direction
  crossWind = atools::roundToInt(std::abs(crossWindParam));
}

void RwVector::appendRwEnd(const QString& name, const QString& surface, int length, float heading)
{
  if(speed >= minSpeed)
  {
    float headWind, crossWind;
    atools::geo::windForCourse(headWind, crossWind, speed, direction, heading);

    if(headWind >= minSpeed)
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

    for(auto it = endMap.constBegin(); it != endMap.constEnd(); ++it)
    {
      append(it.value());
      totalNumber += it.value().names.size();
    }
  }
  else if(size() == 1)
    totalNumber = constFirst().names.size();
}

QStringList RwVector::getSortedRunways(int minHeadWind)
{
  QStringList runways;
  for(const maptools::RwEnd& end : *this)
  {
    if(end.headWind <= minHeadWind)
      break;
    runways.append(end.names);
  }

  // Sort by number and designator
  std::sort(runways.begin(), runways.end(), atools::fs::util::compareRunwayNumber);
  return runways;
}

} // namespace maptools
