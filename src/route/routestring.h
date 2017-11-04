/*****************************************************************************
* Copyright 2015-2017 Alexander Barthel albar965@mailbox.org
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
  GFP = 1 << 5, /* Produce Garmin flight plan format */
  NO_AIRWAYS = 1 << 6, /* Add all waypoints instead of from/airway/to triplet */

  DEFAULT_OPTIONS = START_AND_DEST | ALT_AND_SPEED | SID_STAR
};

Q_DECLARE_FLAGS(RouteStringOptions, RouteStringOption);
Q_DECLARE_OPERATORS_FOR_FLAGS(rs::RouteStringOptions);
}

class RouteString
{
  Q_DECLARE_TR_FUNCTIONS(RouteString)

public:
  RouteString(FlightplanEntryBuilder *flightplanEntryBuilder = nullptr);
  virtual ~RouteString();

  /*
   * Create a route string like
   * LOWI DCT NORIN UT23 ALGOI UN871 BAMUR Z2 KUDES UN871 BERSU Z55 ROTOS
   * UZ669 MILPA UL612 MOU UM129 LMG UN460 CNA DCT LFCY
   */
  QString createStringForRoute(const Route& route, float speed, rs::RouteStringOptions options);

  /*
   * Create a route string in garming flight plan format (GFP):
   * FPN/RI:F:KTEB:F:LGA.J70.JFK.J79.HOFFI.J121.HTO.J150.OFTUR:F:KMVY
   */
  QString createGfpStringForRoute(const Route& route);

  bool createRouteFromString(const QString& routeString, atools::fs::pln::Flightplan& flightplan);
  bool createRouteFromString(const QString& routeString, atools::fs::pln::Flightplan& flightplan,
                             float& speedKts, bool& altIncluded);

  const QStringList& getMessages() const
  {
    return messages;
  }

  static QStringList cleanRouteString(const QString& string);

  void setPlaintextMessages(bool value)
  {
    plaintextMessages = value;
  }

private:
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

  QStringList createStringForRouteInternal(const Route& route, float speed, rs::RouteStringOptions options);
  void findWaypoints(map::MapSearchResult& result, const QString& item);
  void filterWaypoints(map::MapSearchResult& result, atools::geo::Pos& lastPos, int maxDistance);
  void filterAirways(QList<ParseEntry>& resultList, int i);
  QStringList cleanItemList(const QStringList& items, float& speedKnots, float& altFeet);
  void removeEmptyResults(QList<ParseEntry>& resultList);
  bool addDestination(atools::fs::pln::Flightplan& flightplan, QStringList& cleanItems);
  bool extractSpeedAndAltitude(const QString& item, float& speedKnots, float& altFeet);
  QString createSpeedAndAltitude(float speedKnots, float altFeet);

  MapQuery *mapQuery = nullptr;
  AirportQuery *airportQuery = nullptr;
  ProcedureQuery *procQuery = nullptr;
  FlightplanEntryBuilder *entryBuilder = nullptr;
  QStringList messages;
  bool plaintextMessages = false;
};

#endif // LITTLENAVMAP_ROUTESTRING_H
