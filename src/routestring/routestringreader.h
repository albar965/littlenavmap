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
#include "common/mapflags.h"

#include <QStringList>
#include <QCoreApplication>

namespace atools {

namespace geo {
class Pos;
}

namespace fs {
namespace pln {
class Flightplan;
class FlightplanEntry;
}
}
}

namespace map {
struct MapResult;
struct MapAirwayWaypoint;
struct MapWaypoint;
struct MapAirport;
struct MapObjectRefExt;
struct MapAirway;
typedef QVector<map::MapObjectRefExt> MapObjectRefExtVector;
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
 * This class implements the conversion from ATS route descriptions to flight plans, i.e. it reads the strings
 * and constructs a flight plan.
 *
 * Error and warning messages are collected while parsing and can be extracted afterwards.
 */
class RouteStringReader
{
  Q_DECLARE_TR_FUNCTIONS(RouteString)

public:
  RouteStringReader(FlightplanEntryBuilder *flightplanEntryBuilder);
  virtual ~RouteStringReader();

  RouteStringReader(const RouteStringReader& other) = delete;
  RouteStringReader& operator=(const RouteStringReader& other) = delete;

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
  void insertMessage(const QString& message, int index);
  void appendWarning(const QString& message);
  void appendError(const QString& message);
  void addReport(atools::fs::pln::Flightplan *flightplan);

  /* Get the start and end index for the given waypoint ids*/
  void findIndexesInAirway(const QList<map::MapAirwayWaypoint>& allAirwayWaypoints, int lastId, int nextId, int& startIndex, int& endIndex,
                           const QString& airway);

  /* get waypoints by start and end index */
  void extractWaypoints(const QList<map::MapAirwayWaypoint>& allAirwayWaypoints, int startIndex, int endIndex,
                        QList<map::MapWaypoint>& airwayWaypoints);

  /* Build flight plan entry for the given search result. */
  void buildEntryForResult(atools::fs::pln::FlightplanEntry& entry, const map::MapResult& result, const atools::geo::Pos& nearestPos,
                           bool resolveWaypoints);

  /* Get a result set with the single closest element */
  void resultWithClosest(map::MapResult& resultWithClosest, const map::MapResult& result, const atools::geo::Pos& nearestPos,
                         map::MapTypes types);

  /* Get airport or any navaid for item. Also resolves coordinate formats. Optionally tries to match position
   * to waypoints like oceaninc or confluence points.*/
  void findWaypoints(map::MapResult& result, const QString& item, bool matchWaypoints);

  /* Get nearest waypoint for given position probably removing ones which are too far away. Changes given result.
   * Also checks airways and connections if lastResult is given. */
  void filterWaypoints(map::MapResult& result, atools::geo::Pos& lastPos, const atools::geo::Pos& destPos, const map::MapResult *lastResult,
                       const QString& item, float maxDistance);

  void filterAirways(QList<ParseEntry>& resultList, int i);
  QStringList cleanItemList(const QStringList& items, float *speedKnots, float *altFeet);
  void removeEmptyResults(QList<ParseEntry>& resultList);

  /* Fetch departure airport as well as SID */
  bool addDeparture(atools::fs::pln::Flightplan *flightplan, map::MapObjectRefExtVector *mapObjectRefs, QStringList& items);

  /* Fetch destination airport as well as STAR */
  bool addDestination(atools::fs::pln::Flightplan *flightplan, map::MapObjectRefExtVector *mapObjectRefs, QStringList& items,
                      rs::RouteStringOptions options);
  void destinationInternal(map::MapAirport& destination, proc::MapProcedureLegs& starLegs, QStringList& items, int& consume, int index);

  /* Remove time and runways from ident and return airport ident only. Also add warning messages */
  QString extractAirportIdent(QString ident);

  /* Extract the first coordinate for the list if no airports are used */
  atools::geo::Pos findFirstCoordinate(const QStringList& items);

  /* Create reference struct from given entry and map search result */
  map::MapObjectRefExt mapObjectRefFromEntry(const atools::fs::pln::FlightplanEntry& entry,
                                             const map::MapResult& result, const QString& name);

  /* Get airway segments with given name between  waypoints */
  map::MapAirway extractAirway(const QList<map::MapAirway>& airways, int waypointId1, int waypointId2, const QString& airwayName);

  /* Read and consume SID and maybe transition from list. Can be "SID", "SID.TRANS" or "SID TRANS" */
  void readSidAndTrans(QStringList& items, int& sidId, int& sidTransId, const map::MapAirport& departure);

  /* Try to read all STAR and transition variants. Number of items to delete is given in "consume" */
  void readStarAndTrans(QStringList& items, int& starId, int& starTransId, int& consume, const map::MapAirport& destination, int index);

  /* Read a space spearated STAR with transition. Can be "STAR TRANS" or "TRANS STAR". Items are not consumed. */
  void readStarAndTransSpace(const QString& star, QString trans, int& starId, int& starTransId, const map::MapAirport& destination);

  /* Read a dot spearated STAR with transition. Can be "STAR.TRANS" or "TRANS.STAR". Items are not consumed. */
  void readStarAndTransDot(const QString& starTrans, int& starId, int& starTransId, const map::MapAirport& destination);

  /* Convert to abbreviated SID: ENVA UTUNA1A -> ENVA UTUN1A */
  QString sidStarAbbrev(QString sid);

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
