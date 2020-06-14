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

#ifndef LITTLENAVMAP_ROUTESTRING_H
#define LITTLENAVMAP_ROUTESTRING_H

#include "routestring/routestringtypes.h"

#include <QStringList>

class Route;

/* This class implementes the conversion from flight plans to ATS route descriptions , i.e. it writes the strings
 * for a given flight plan.
 */
class RouteStringWriter
{
public:
  RouteStringWriter();
  ~RouteStringWriter();

  /*
   * Create a route string like
   * LOWI DCT NORIN UT23 ALGOI UN871 BAMUR Z2 KUDES UN871 BERSU Z55 ROTOS
   * UZ669 MILPA UL612 MOU UM129 LMG UN460 CNA DCT LFCY
   */
  QString createStringForRoute(const Route& route, float speed, rs::RouteStringOptions options) const;
  QStringList createStringForRouteList(const Route& route, float speed, rs::RouteStringOptions options) const;

  /*
   * Create a route string in garming flight plan format (GFP):
   * FPN/RI:F:KTEB:F:LGA.J70.JFK.J79.HOFFI.J121.HTO.J150.OFTUR:F:KMVY
   *
   * If procedures is true SIDs, STARs and approaches will be included according to Garmin spec.
   */
  QString createGfpStringForRoute(const Route& route, bool procedures, bool userWaypointOption) const;

private:
  QStringList createStringForRouteInternal(const Route& routeParam, float speed, rs::RouteStringOptions options) const;

  /* Garming GFP format */
  QString createGfpStringForRouteInternal(const Route& route, bool userWaypointOption) const;

  /* Garming GFP format with procedures */
  QString createGfpStringForRouteInternalProc(const Route& route, bool userWaypointOption) const;

};

#endif // LITTLENAVMAP_ROUTESTRING_H
