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

#ifndef LITTLENAVMAP_MAPRESULT_H
#define LITTLENAVMAP_MAPRESULT_H

#include "common/maptypes.h"

namespace map {
// =====================================================================
/*
 * Mixed search result for e.g. queries on a bounding rectangle for map display or for all get nearest methods.
 * Contains aggregated lists for all possible objects.
 * Does not use inheritance and not virtual methods to avoid overhead */
struct MapResult
{
  /* Create from base class. Inspects type and adds one object to this */
  MapResult& addFromMapBase(const map::MapBase *base);
  static MapResult createFromMapBase(const map::MapBase *base);

  QList<MapAirport> airports;
  QSet<int> airportIds; /* Ids used to deduplicate when merging highlights and nearest */

  QList<MapRunway> runways;
  QList<MapRunwayEnd> runwayEnds;
  QList<MapAirport> towers;
  QList<MapParking> parkings;
  QList<MapHelipad> helipads;

  QList<MapWaypoint> waypoints;
  QSet<int> waypointIds; /* Ids used to deduplicate */

  QList<MapVor> vors;
  QSet<int> vorIds; /* Ids used to deduplicate */

  QList<MapNdb> ndbs;
  QSet<int> ndbIds; /* Ids used to deduplicate */

  QList<MapMarker> markers;
  QList<MapIls> ils;

  QList<MapAirway> airways;
  QList<MapAirspace> airspaces;

  /* User defined route points */
  QList<MapUserpointRoute> userpointsRoute;

  /* User defined waypoints */
  QList<MapUserpoint> userpoints;
  QSet<int> userpointIds; /* Ids used to deduplicate */

  /* Logbook entries */
  QList<MapLogbookEntry> logbookEntries;

  map::MapUserAircraft userAircraft;

  QList<map::MapAiAircraft> aiAircraft; /* Simulator AI */
  QList<map::MapOnlineAircraft> onlineAircraft; /* Online networks */
  QSet<int> onlineAircraftIds; /* Ids used to deduplicate */

  atools::geo::Pos windPos;
  QList<map::PatternMarker> patternMarks;
  QList<map::RangeMarker> rangeMarks;
  QList<map::DistanceMarker> distanceMarks;
  QList<map::HoldingMarker> holdingMarks;
  QList<map::MsaMarker> msaMarks;

  QList<map::MapHolding> holdings;
  QSet<int> holdingIds; /* Ids used to deduplicate */

  QList<map::MapAirportMsa> airportMsa;
  QSet<int> airportMsaIds; /* Ids used to deduplicate */

  QList<map::MapProcedurePoint> procPoints;

  /* Nearest aircraft trail segment for user aircraft and logbook preview */
  map::AircraftTrailSegment trailSegment, trailSegmentLog;

  /* true if none of the types exists in this result */
  bool isEmpty(const map::MapTypes& types = map::ALL) const
  {
    return size(types) == 0;
  }

  /* Number of map objects for the given types */
  int size(const map::MapTypes& types = map::ALL) const;

  /* true if contains given types */
  bool hasTypes(const map::MapTypes& types = map::ALL) const
  {
    return size(types) > 0;
  }

  /* Get id and type from the result. Vector of types defines priority. true if something was found.
   * id is set to -1 if nothing was found. */
  bool getIdAndType(int& id, MapTypes& type, const std::initializer_list<MapTypes>& types) const;
  map::MapRef getRef(const std::initializer_list<MapTypes>& types) const;

  /* Get id. This assumes there is only one object for the given type. Returns -1 if not found. */
  int getId(const MapTypes& type) const;

  /* Get routeIndex. This assumes there is only one object for the given type. Returns -1 if not found.
   * Only for flight plan related types. */
  int getRouteIndex(const MapTypes& type) const;

  /* Get position and returns first for the list of types defining priority by order */
  const atools::geo::Pos& getPosition(const std::initializer_list<MapTypes>& types) const;

  /* As above for ident */
  QString getIdent(const std::initializer_list<MapTypes>& types) const;

  /* As above for region */
  QString getRegion(const std::initializer_list<MapTypes>& types) const;

  /* Remove the given types only */
  MapResult& clear(const MapTypes& types = map::ALL);

  /* Remove all except first for the given types only */
  MapResult& clearAllButFirst(const MapTypes& types = map::ALL);

  /* Sets routeIndex for all flight plan related types to -1 */
  MapResult& clearRouteIndex(const MapTypes& types = map::ALL);

  /* Give online airspaces/centers priority */
  void moveOnlineAirspacesToFront();
  map::MapResult moveOnlineAirspacesToFront() const;

  bool hasAirports() const
  {
    return !airports.isEmpty();
  }

  bool hasAirportMsa() const
  {
    return !airportMsa.isEmpty();
  }

  bool hasAirways() const
  {
    return !airways.isEmpty();
  }

  bool hasVor() const
  {
    return !vors.isEmpty();
  }

  bool hasNdb() const
  {
    return !ndbs.isEmpty();
  }

  bool hasUserpoints() const
  {
    return !userpoints.isEmpty();
  }

  bool hasLogEntries() const
  {
    return !logbookEntries.isEmpty();
  }

  bool hasProcedurePoints() const
  {
    return !procPoints.isEmpty();
  }

  bool hasHoldings() const
  {
    return !holdings.isEmpty();
  }

  bool hasParkings() const
  {
    return !parkings.isEmpty();
  }

  bool hasUserpointsRoute() const
  {
    return !userpointsRoute.isEmpty();
  }

  bool hasIls() const
  {
    return !ils.isEmpty();
  }

  bool hasRunwayEnds() const
  {
    return !runwayEnds.isEmpty();
  }

  bool hasRunways() const
  {
    return !runways.isEmpty();
  }

  bool hasWaypoints() const
  {
    return !waypoints.isEmpty();
  }

  bool hasAirspaces() const
  {
    return !airspaces.isEmpty();
  }

  /* Enroute navaids not ILS */
  bool hasNavaids() const
  {
    return hasVor() || hasNdb() || hasWaypoints();
  }

  bool hasAnyAircraft() const
  {
    return hasUserAircraft() || hasOnlineAircraft() || hasAiAircraft();
  }

  bool hasUserAircraft() const
  {
    return userAircraft.isValid();
  }

  bool hasOnlineAircraft() const
  {
    return !onlineAircraft.isEmpty();
  }

  bool hasAiAircraft() const
  {
    return !aiAircraft.isEmpty();
  }

  /* Special methods for the online and navdata airspaces which are stored mixed */
  bool hasSimNavUserAirspaces() const;
  bool hasOnlineAirspaces() const;
  void clearNavdataAirspaces();
  void clearOnlineAirspaces();
  const MapAirspace *firstSimNavUserAirspace() const;
  const MapAirspace *firstOnlineAirspace() const;
  int numSimNavUserAirspaces() const;
  int numOnlineAirspaces() const;

  const QList<MapAirspace> getSimNavUserAirspaces() const;
  const QList<MapAirspace> getOnlineAirspaces() const;

  QString objectText(MapTypes type, int elideName = 1000) const;

  /* Remove all invalid objects */
  void removeInvalid();

  /* Remove all objects having no route index, i.e. which are not a part of the flight plan */
  void removeNoRouteIndex();

private:
  template<typename TYPE>
  void clearAllButFirst(QList<TYPE>& list)
  {
    while(list.size() > 1)
      list.removeLast();
  }

  template<typename TYPE>
  void removeNoRouteIndex(QList<TYPE>& list, QSet<int> *ids = nullptr);

  template<typename TYPE>
  void removeInvalid(QList<TYPE>& list, QSet<int> *ids = nullptr);

  template<class TYPE>
  void setRouteIndex(QList<TYPE>& list, const MapTypes& types, const MapTypes& type, int routeIndex = -1);

};

QDebug operator<<(QDebug out, const map::MapResult& record);

// =====================================================================
/* Mixed search result using inherited objects */
/* Maintains only pointers to the original objects and creates a copy the MapSearchResult. */
/* Alternatively uses pointers to refer to another result which must not be changed in this case. */
struct MapResultIndex
  : public QVector<const map::MapBase *>
{
  /* Sorting callback */
  typedef std::function<bool (const MapBase *, const MapBase *)> SortFunction;

  /* Add all result object pointers to list. Result and all objects are copied. */
  MapResultIndex& add(const map::MapResult& resultParam, const map::MapTypes& types = map::ALL);

  /* Add all result objects to list. Result and all objects have to be kept to avoid invalid pointers. */
  MapResultIndex& addRef(const map::MapResult& resultParm, const map::MapTypes& types = map::ALL);

  /* Sort objects by given type list. First in type list is put to start of list. */
  MapResultIndex& sort(const QVector<map::MapTypes>& types)
  {
    return sort(types, nullptr);
  }

  /* Sort by callback */
  MapResultIndex& sort(const map::MapResultIndex::SortFunction& sortFunc)
  {
    return sort({}, sortFunc);
  }

  /* Sort by type list and then by callback */
  MapResultIndex& sort(const QVector<map::MapTypes>& types, const map::MapResultIndex::SortFunction& sortFunc);

  /* Sort objects by distance to pos in the list */
  MapResultIndex& sort(const atools::geo::Pos& pos, bool sortNearToFar = true);

  /* Sort objects alphabetically */
  // MapSearchResultIndex& sort();

  /* Remove all objects which are more far away  from pos than max distance */
  MapResultIndex& remove(const atools::geo::Pos& pos, float maxDistanceNm);

private:
  /* Last index is including */
  template<typename TYPE>
  void addToIndex(const QList<TYPE>& list, int firstIndex = 0, int lastIndex = -1)
  {
    if(lastIndex == -1)
      lastIndex = list.size() - 1;

    for(int i = firstIndex; i <= lastIndex; i++)
      // Add object pointer to this
      append(&list.at(i));
  }

  template<typename TYPE>
  void addToIndexIf(const QList<TYPE>& list, const MapTypes& types)
  {
    if(types.testFlag(TYPE::staticType()))
    {
      for(const TYPE& obj : list)
        append(&obj);
    }
  }

  template<typename TYPE>
  void addToIndexRangeIf(const QList<TYPE>& from, QList<TYPE>& to, const MapTypes& types)
  {
    if(types.testFlag(TYPE::staticType()))
    {
      int startIndex = to.size();

      // Add whole list to target
      to.append(from);

      // Add object pointers from target list to this
      addToIndex(to, startIndex);
    }
  }

  map::MapResult result;
};

} // namespace map

Q_DECLARE_TYPEINFO(map::MapResult, Q_MOVABLE_TYPE);

#endif // LITTLENAVMAP_MAPRESULT_H
