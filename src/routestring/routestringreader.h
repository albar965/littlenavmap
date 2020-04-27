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

#ifndef LITTLENAVMAP_ROUTESTRINGREADER_H
#define LITTLENAVMAP_ROUTESTRINGREADER_H

#include "routestring/routestringtypes.h"
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

namespace proc {
struct MapProcedureLegs;
}

class MapQuery;
class AirwayTrackQuery;
class WaypointTrackQuery;
class AirportQuery;
class ProcedureQuery;
class FlightplanEntryBuilder;
class Route;

/*
 * This class implementes the conversion from ATS route descriptions to flight plans, i.e. it reads the strings
 * and constructs a flight plan.
 *
 * Error and warning messages are collected while parsing and can be extracted afterwards.
 */
class RouteStringReader
{
  Q_DECLARE_TR_FUNCTIONS(RouteString)

public:
  RouteStringReader(FlightplanEntryBuilder *flightplanEntryBuilder, bool verboseParam = false);
  virtual ~RouteStringReader();

  /* Create a flight plan for the given route string and include speed and altitude if given (pointers != null).
   * Fills either flightplan and/or mapObjectRefs if not null.
   * Get error, warning and information messages with getMessages() */
  bool createRouteFromString(const QString& routeString, rs::RouteStringOptions options,
                             atools::fs::pln::Flightplan *flightplan,
                             map::MapObjectRefExtVector *mapObjectRefs = nullptr,
                             float *speedKts = nullptr, bool *altIncluded = nullptr);

  /* Get error and warning messages */
  const QStringList& getMessages() const
  {
    return messages;
  }

  /* Set to true to generate non HTML messages */
  void setPlaintextMessages(bool value)
  {
    plaintextMessages = value;
  }

  bool hasWarningMessages() const
  {
    return hasWarnings;
  }

  bool hasErrorMessages() const
  {
    return hasErrors;
  }

private:
  /* Internal parsing structure which holds all found potential candidates from a search */
  struct ParseEntry;

  /* Add messages to log */
  void appendMessage(const QString& message);
  void appendWarning(const QString& message);
  void appendError(const QString& message);

  /* Get the start and end index for the given waypoint ids*/
  void findIndexesInAirway(const QList<map::MapAirwayWaypoint>& allAirwayWaypoints, int lastId,
                           int nextId, int& startIndex, int& endIndex, const QString& airway);

  /* get waypoints by start and end index */
  void extractWaypoints(const QList<map::MapAirwayWaypoint>& allAirwayWaypoints, int startIndex, int endIndex,
                        QList<map::MapWaypoint>& airwayWaypoints);

  /* Build flight plan entry for the given search result. */
  void buildEntryForResult(atools::fs::pln::FlightplanEntry& entry, const map::MapSearchResult& result,
                           const atools::geo::Pos& nearestPos);

  /* Get a result set with the single closest element */
  void resultWithClosest(map::MapSearchResult& resultWithClosest, const map::MapSearchResult& result,
                         const atools::geo::Pos& nearestPos, map::MapObjectTypes types);

  /* Get airport or any navaid for item. Also resolves coordinate formats. Optionally tries to match position
   * to waypoints like oceaninc or confluence points.*/
  void findWaypoints(map::MapSearchResult& result, const QString& item, bool matchWaypoints);

  /* Get nearest waypoint for given position probably removing ones which are too far away. Changes given result.
   * Also checks airways and connections if lastResult is given. */
  void filterWaypoints(map::MapSearchResult& result, atools::geo::Pos& lastPos, const map::MapSearchResult *lastResult,
                       float maxDistance);
  void filterAirways(QList<ParseEntry>& resultList, int i);
  QStringList cleanItemList(const QStringList& items, float *speedKnots, float *altFeet);
  void removeEmptyResults(QList<ParseEntry>& resultList);

  /* Fetch departure airport as well as SID */
  bool addDeparture(atools::fs::pln::Flightplan *flightplan, map::MapObjectRefExtVector *mapObjectRefs,
                    QStringList& cleanItems);

  /* Fetch destination airport as well as STAR */
  bool addDestination(atools::fs::pln::Flightplan *flightplan, map::MapObjectRefExtVector *mapObjectRefs,
                      QStringList& cleanItems, rs::RouteStringOptions options);
  void destinationInternal(map::MapAirport& destination, proc::MapProcedureLegs& starLegs,
                           const QStringList& cleanItems, int index);

  /* Remove time and runways from ident and return airport ident only. Also add warning messages */
  QString extractAirportIdent(QString ident);

  /* Extract the first coordinate for the list if no airports are used */
  atools::geo::Pos findFirstCoordinate(const QStringList& cleanItems);

  /* Create reference struct from given entry and map search result */
  map::MapObjectRefExt mapObjectRefFromEntry(const atools::fs::pln::FlightplanEntry& entry,
                                             const map::MapSearchResult& result, const QString& name);

  /* Get airway segments with given name between  waypoints */
  map::MapAirway extractAirway(const QList<map::MapAirway>& airways, int waypointId1, int waypointId2,
                               const QString& airwayName);

  bool verbose = false;
  MapQuery *mapQuery = nullptr;
  AirwayTrackQuery *airwayQuery = nullptr;
  WaypointTrackQuery *waypointQuery = nullptr;
  AirportQuery *airportQuerySim = nullptr;
  ProcedureQuery *procQuery = nullptr;
  FlightplanEntryBuilder *entryBuilder = nullptr;
  QStringList messages;
  bool plaintextMessages = false, hasWarnings = false, hasErrors = false;
};

#endif // LITTLENAVMAP_ROUTESTRINGREADER_H
