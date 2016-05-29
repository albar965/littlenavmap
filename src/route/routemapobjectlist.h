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

#ifndef ROUTEMAPOBJECTLIST_H
#define ROUTEMAPOBJECTLIST_H

#include "routemapobject.h"

class RouteMapObjectList :
  public QList<RouteMapObject>
{
public:
  RouteMapObjectList();
  virtual ~RouteMapObjectList();

  int getNearestLegIndex(const atools::geo::Pos& pos, float *crossTrackDistance = nullptr) const;

  bool getRouteDistances(const atools::geo::Pos& pos, float *distFromStartNm, float *distToDestNm,
                         float *nearestLegDistance = nullptr, float *crossTrackDistance = nullptr,
                         int *nearestLegIndex = nullptr) const;

  float getTotalDistance() const
  {
    return totalDistance;
  }

  void setTotalDistance(float value)
  {
    totalDistance = value;
  }

private:
  float totalDistance = 0.f;

};

#endif // ROUTEMAPOBJECTLIST_H
