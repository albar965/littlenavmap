/*****************************************************************************
* Copyright 2015-2025 Alexander Barthel alex@littlenavmap.org
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

namespace atools {
namespace fs {
namespace pln {
class Flightplan;
}
}
}
class Route;
class RouteLeg;

/* This class implementes the conversion from flight plans to ATS route descriptions , i.e. it writes the strings
 * for a given flight plan.
 */
class RouteStringWriter
{
public:
  /* Create a route string from a simple incomplete flight plan taken from FlightplanIO::loadGarminGfp().
   * Creates a string containing all procedures, airways and runways. */
  QString createGfpStringForFlightplan(const atools::fs::pln::Flightplan& flightplan) const;

  /*
   * Create a route string like
   * LOWI DCT NORIN UT23 ALGOI UN871 BAMUR Z2 KUDES UN871 BERSU Z55 ROTOS
   * UZ669 MILPA UL612 MOU UM129 LMG UN460 CNA DCT LFCY
   */
  QString createStringForRoute(const Route& route, float speedKts, rs::RouteStringOptions options) const;
  QStringList createStringListForRoute(const Route& route, float speedKts, rs::RouteStringOptions options) const;

  /*
   * Create a route string in garming flight plan format (GFP):
   * FPN/RI:F:KTEB:F:LGA.J70.JFK.J79.HOFFI.J121.HTO.J150.OFTUR:F:KMVY
   *
   * If procedures is true SIDs, STARs and approaches will be included according to Garmin spec.
   */
  QString createGfpStringForRoute(const Route& route, bool procedures, bool userWaypointOption, bool gfpCoordinates,
                                  bool departDestAsCoords) const;

private:
  QStringList createStringForRouteInternal(const Route& routeParam, float speedKts, rs::RouteStringOptions options) const;

  /*
   * Garming GFP format used for Flight1 GTN export
   *
   *  Flight Plan File Guidelines
   *
   * Garmin uses a text based flight plan format that is derived from the IMI/IEI
   * messages format specified in ARINC 702A-3. But that’s just a side note.
   * Let’s take a look at the syntax of a usual Garmin flight plan:
   * FPN/RI:F:AIRPORT:F:WAYPOINT:F:WAYPOINT.AIRWAY.WAYPOINT:F:AIRPORT
   * Every flight plan always starts with the “FPN/RI” identifier. The “:F:” specifies
   * the different flight plan segments. A flight plan segment can be the departure or arrival
   * airport, a waypoint or a number of waypoints that are connected via airways.
   *
   * The entry and exit waypoint of an airway are connected to the airway via a dot “.”.
   * The flight plan must be contained in the first line of the file. Anything after the first
   * line will be discarded and may result i
   * n importing failures. Flight plans can only contain
   * upper case letters, numbers, colons, parenthesis, commas and periods. Spaces or any other
   * special characters are not allowed. When saved the flight plan name must have a “.gfp” extension.
   *
   * Here's an example, it's basically a .txt file with the extension .gfp
   *
   * FPN/RI:F:KTEB:F:LGA.J70.JFK.J79.HOFFI.J121.HTO.J150.OFTUR:F:KMVY
   *
   * FPN/RI:F:KTEB:F:LGA:F:JFK:F:HOFFI:F:HTO:F:MONTT:F:OFTUR:F:KMVY
   */
  QString createGfpStringForRouteInternal(const Route& route, bool userWaypointOption) const;

  /* Garming GFP format with procedures. Used for Reality XP GTN export.
   *
   * Flight plan to depart from KPDX airport and arrive in KHIO through
   * Airway 448 and 204 using Runway 13 for final RNAV approach:
   * FPN/RI:DA:KPDX:D:LAVAA5.YKM:R:10R:F:YKM.V448.GEG.V204.HQM:F:SEA,N47261W 122186:AA:KHIO:A:HELNS5.SEA(13O):AP:R13
   *
   * Flight plan from KSLE to two user waypoints and then returning for the ILS approach to runway 31 via JAIME:
   * FPN/RI:F:KSLE:F:N45223W121419:F:N42568W122067:AA:KSLE:AP:I31.JAIME
   */
  QString createGfpStringForRouteInternalProc(const Route& route, bool userWaypointOption, bool gfpCoordinates,
                                              bool departDestAsCoords) const;

  /* Either ident or coordinates */
  QString legIdent(const RouteLeg& leg, rs::RouteStringOptions options, bool departDestAsCoords, bool departure, bool destination) const;
  QString legIdentCoords(const RouteLeg& leg, rs::RouteStringOptions options) const;

};

#endif // LITTLENAVMAP_ROUTESTRING_H
