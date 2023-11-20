/*****************************************************************************
* Copyright 2015-2023 Alexander Barthel alex@littlenavmap.org
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
struct MapRunwayEnd;
struct MapResult;
struct MapAirwayWaypoint;
struct MapWaypoint;
struct MapAirport;
struct MapRefExt;
struct MapAirway;
typedef QVector<map::MapRefExt> MapRefExtVector;
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
  bool createRouteFromString(const QString& routeString, rs::RouteStringOptions options, atools::fs::pln::Flightplan *flightplan,
                             map::MapRefExtVector *mapObjectRefs = nullptr, float *speedKtsParam = nullptr, bool *altIncludedParam = nullptr);

  /* Set to true to generate non HTML messages */
  void setPlaintextMessages(bool value)
  {
    plaintextMessages = value;
  }

  bool hasWarningMessages() const
  {
    return !warningMessages.isEmpty();
  }

  bool hasErrorMessages() const
  {
    return errorMessages.isEmpty();
  }

  /* Get messages in order of error, warning and info messages separated by an empty line */
  const QStringList getAllMessages() const;

private:
  /* Internal parsing structure which holds all found potential candidates from a search */
  struct ParseEntry;

  /* Add messages to log */
  void appendMessage(const QString& message);
  void insertMessage(const QString& message, int index);
  void appendWarning(const QString& message);
  void appendError(const QString& message);
  void addReport(atools::fs::pln::Flightplan *flightplan, const QString& rawRouteString, float cruiseAltitudeFt);

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

  /* Remove all extaneous characters that do not belong to a route string */
  QStringList cleanItemList(const QStringList& items, float& speedKnots, float& altFeet);
  void removeEmptyResults(QList<ParseEntry>& resultList);

  /* Fetch departure airport as well as SID */
  bool addDeparture(atools::fs::pln::Flightplan *flightplan, map::MapRefExtVector *mapObjectRefs, QStringList& items,
                    QString& sidTransWp);

  /* Fetch destination airport as well as STAR */
  bool addDestination(atools::fs::pln::Flightplan *flightplan, QList<atools::fs::pln::FlightplanEntry> *alternates,
                      map::MapRefExtVector *mapObjectRefs, QStringList& items, QString& starTransWp, rs::RouteStringOptions options);
  void destinationInternal(map::MapAirport& destination, proc::MapProcedureLegs& starLegs, proc::MapProcedureLegs& approachLegs,
                           QStringList& items, QString& starTransWp, map::MapRunwayEnd& runwayEnd, int& consume, int index);

  /* Remove time from ident and return airport ident and runway. Also add warnings and messages */
  void extractAirportIdentDeparture(QString item, QString& airport, QString& runway);

  /* Remove time from ident and return airport ident and approach. Also add warnings and messages */
  void extractAirportIdentDestination(QString item, QString& airport, QString& approach, QString& transition, QString& runway);

  /* Extract the first coordinate for the list if no airports are used */
  atools::geo::Pos findFirstCoordinate(const QStringList& items);

  /* Create reference struct from given entry and map search result */
  map::MapRefExt mapObjectRefFromEntry(const atools::fs::pln::FlightplanEntry& entry, const map::MapResult& result, const QString& name);

  /* Get airway segments with given name between  waypoints */
  map::MapAirway extractAirway(const QList<map::MapAirway>& airways, int waypointId1, int waypointId2, const QString& airwayName);

  /* Read and consume SID and maybe transition from list. Can be "SID", "SID.TRANS" or "SID TRANS" */
  void readSidAndTrans(QStringList& items, QString& sidTransWp, int& sidId, int& sidTransId, const map::MapAirport& departure,
                       const QString& runway);

  /* Try to read all STAR and transition variants. Number of items to delete is given in "consume" */
  void readStarAndTrans(QStringList& items, QString& startTransWp, const QString& runway, int& starId, int& starTransId, int& consume,
                        const map::MapAirport& destination, int index);

  /* Read a space spearated STAR with transition. Can be "STAR TRANS" or "TRANS STAR". Items are not consumed. */
  void readStarAndTransSpace(const QString& star, QString trans, const QString& runway, int& starId, int& starTransId,
                             const map::MapAirport& destination);

  /* Read a dot spearated STAR with transition. Can be "STAR.TRANS" or "TRANS.STAR". Items are not consumed. */
  void readStarAndTransDot(const QString& starTrans, const QString& runway, int& starId, int& starTransId,
                           const map::MapAirport& destination);

  void readApproachAndTrans(const QString& approachArinc, const QString& approachTrans, int& starId, int& starTransId,
                            const map::MapAirport& destination);

  /* Convert to abbreviated SID: ENVA UTUNA1A -> ENVA UTUN1A */
  QString sidStarAbbrev(QString sid);

  /* First try exact match and then all possible idents. */
  void airportSim(map::MapAirport& airport, const QString& ident);

  MapQuery *mapQuery = nullptr;
  AirwayTrackQuery *airwayQuery = nullptr;
  WaypointTrackQuery *waypointQuery = nullptr;
  AirportQuery *airportQuerySim = nullptr, *airportQueryNav = nullptr;
  ProcedureQuery *procQuery = nullptr;
  FlightplanEntryBuilder *entryBuilder = nullptr;
  QStringList errorMessages, warningMessages, logMessages;
  bool plaintextMessages = false;
};

#endif // LITTLENAVMAP_ROUTESTRINGREADER_H
