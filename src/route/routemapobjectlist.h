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

#ifndef LITTLENAVMAP_ROUTEMAPOBJECTLIST_H
#define LITTLENAVMAP_ROUTEMAPOBJECTLIST_H

#include "routemapobject.h"

#include "fs/pln/flightplan.h"

class CoordinateConverter;

class RouteMapObjectList :
  public QList<RouteMapObject>
{
public:
  RouteMapObjectList();
  virtual ~RouteMapObjectList();

  int getNearestLegOrPointIndex(const atools::geo::Pos& pos) const;
  int getNearestLegIndex(const atools::geo::Pos& pos, float& crossTrackDistance) const;
  int getNearestPointIndex(const atools::geo::Pos& pos, float& pointDistance) const;

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

  const static float INVALID_DISTANCE_VALUE;

  const atools::fs::pln::Flightplan& getFlightplan() const
  {
    return flightplan;
  }

  atools::fs::pln::Flightplan& getFlightplan()
  {
    return flightplan;
  }

  void setFlightplan(const atools::fs::pln::Flightplan& value)
  {
    flightplan = value;
  }

  void getNearest(const CoordinateConverter& conv, int xs, int ys, int screenDistance,
                  maptypes::MapSearchResult& mapobjects) const;

  bool hasDepartureParking() const;

  bool hasDepartureStart() const;

  bool isFlightplanEmpty() const;
  bool hasValidDeparture() const;
  bool hasValidDestination() const;
  bool hasDepartureHelipad() const;

  bool hasEntries() const;
  bool hasRoute() const;

private:
  float totalDistance = 0.f;
  atools::fs::pln::Flightplan flightplan;

};

#endif // LITTLENAVMAP_ROUTEMAPOBJECTLIST_H
