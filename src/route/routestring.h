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

#ifndef LITTLENAVMAP_ROUTESTRING_H
#define LITTLENAVMAP_ROUTESTRING_H

#include "common/maptypes.h"

#include <QStringList>
#include <QApplication>

namespace atools {
namespace fs {
namespace pln {
class Flightplan;
class FlightplanEntry;
}
}
}

class MapQuery;
class AirportQuery;
class ProcedureQuery;
class FlightplanEntryBuilder;
class Route;

namespace rs {

enum RouteStringOption
{
  NONE = 0,
  DCT = 1 << 0, /* Add DCT */
  START_AND_DEST = 1 << 1, /* Add departure and/or destination ident */
  ALT_AND_SPEED = 1 << 2, /* Add altitude and speed restriction */
  SID_STAR = 1 << 3, /* Add sid, star and transition names if selected */
  SID_STAR_GENERIC = 1 << 4, /* Always add SID and STAR keyword */
  GFP = 1 << 5, /* Produce Garmin flight plan format using special coordinate format */
  NO_AIRWAYS = 1 << 6, /* Add all waypoints instead of from/airway/to triplet */

  SID_STAR_SPACE = 1 << 7, /* Separate SID/STAR and transition with space. Not ATS compliant. */
  RUNWAY = 1 << 8, /* Add departure runway if available. Not ATS compliant. */
  APPROACH = 1 << 9, /* Add approach ARINC name and transition after destination. Not ATS compliant. */
  FLIGHTLEVEL = 1 << 10, /* Append flight level at end of string. Not ATS compliant. */
  GFP_COORDS = 1 << 11, /* Suffix all navaids with coordinates for new GFP format */
  USR_WPT = 1 << 12, /* User waypoints for all navaids to avoid locked waypoints from Garmin */
  SKYVECTOR_COORDS = 1 << 13, /* Skyvector coordinate format */

  DEFAULT_OPTIONS = START_AND_DEST | ALT_AND_SPEED | SID_STAR
};

Q_DECLARE_FLAGS(RouteStringOptions, RouteStringOption);
Q_DECLARE_OPERATORS_FOR_FLAGS(rs::RouteStringOptions);
}

/*
 * This class implementes the conversion from ATS route descriptions to flight plans and vice versa.
 * Additional functionality is available to generate route strings for various export formats.
 * Error and warning messages are collected while parsing and can be extracted afterwards.
 */
class RouteString
{
  Q_DECLARE_TR_FUNCTIONS(RouteString)

public:
  RouteString(FlightplanEntryBuilder *flightplanEntryBuilder = nullptr);
  virtual ~RouteString();

  /* Create a flight plan for the given route string and include speed and altitude if given.
   *  Get error messages with getMessages() */
  bool createRouteFromString(const QString& routeString, atools::fs::pln::Flightplan& flightplan);
  bool createRouteFromString(const QString& routeString, atools::fs::pln::Flightplan& flightplan,
                             float& speedKts, bool& altIncluded);

  /*
   * Create a route string like
   * LOWI DCT NORIN UT23 ALGOI UN871 BAMUR Z2 KUDES UN871 BERSU Z55 ROTOS
   * UZ669 MILPA UL612 MOU UM129 LMG UN460 CNA DCT LFCY
   */
  static QString createStringForRoute(const Route& route, float speed, rs::RouteStringOptions options);
  static QStringList createStringForRouteList(const Route& route, float speed, rs::RouteStringOptions options);

  /*
   * Create a route string in garming flight plan format (GFP):
   * FPN/RI:F:KTEB:F:LGA.J70.JFK.J79.HOFFI.J121.HTO.J150.OFTUR:F:KMVY
   *
   * If procedures is true SIDs, STARs and approaches will be included according to Garmin spec.
   */
  static QString createGfpStringForRoute(const Route& route, bool procedures, bool userWaypointOption);

  const QStringList& getMessages() const
  {
    return messages;
  }

  /* Remove all invalid characters and simplify string */
  static QStringList cleanRouteString(const QString& string);

  void setPlaintextMessages(bool value)
  {
    plaintextMessages = value;
  }

private:
  /* Internal parsing structure which holds all found potential candidates from a search */
  struct ParseEntry
  {
    QString item, airway;
    map::MapSearchResult result;
  };

  /* Add messages to log */
  void appendMessage(const QString& message);
  void appendWarning(const QString& message);
  void appendError(const QString& message);

  bool addDeparture(atools::fs::pln::Flightplan& flightplan, QStringList& cleanItems);
  void findIndexesInAirway(const QList<map::MapAirwayWaypoint>& allAirwayWaypoints, int lastId,
                           int nextId, int& startIndex, int& endIndex, const QString& airway);
  void extractWaypoints(const QList<map::MapAirwayWaypoint>& allAirwayWaypoints, int startIndex, int endIndex,
                        QList<map::MapWaypoint>& airwayWaypoints);

  static QStringList createStringForRouteInternal(const Route& route, float speed, rs::RouteStringOptions options);

  /* Garming GFP format */
  static QString createGfpStringForRouteInternal(const Route& route, bool userWaypointOption);

  /* Garming GFP format with procedures */
  static QString createGfpStringForRouteInternalProc(const Route& route, bool userWaypointOption);

  void buildEntryForResult(atools::fs::pln::FlightplanEntry& entry, const map::MapSearchResult& result,
                           const atools::geo::Pos& nearestPos);

  /* Get a result set with the single closest element */
  void resultWithClosest(map::MapSearchResult& resultWithClosest, const map::MapSearchResult& result,
                         const atools::geo::Pos& nearestPos, map::MapObjectTypes types);

  void findWaypoints(map::MapSearchResult& result, const QString& item);
  void filterWaypoints(map::MapSearchResult& result, atools::geo::Pos& lastPos, const map::MapSearchResult *lastResult,
                       float maxDistance);
  void filterAirways(QList<ParseEntry>& resultList, int i);
  QStringList cleanItemList(const QStringList& items, float& speedKnots, float& altFeet);
  void removeEmptyResults(QList<ParseEntry>& resultList);
  bool addDestination(atools::fs::pln::Flightplan& flightplan, QStringList& cleanItems);

  MapQuery *mapQuery = nullptr;
  AirportQuery *airportQuerySim = nullptr;
  ProcedureQuery *procQuery = nullptr;
  FlightplanEntryBuilder *entryBuilder = nullptr;
  QStringList messages;
  bool plaintextMessages = false;

};

#endif // LITTLENAVMAP_ROUTESTRING_H
