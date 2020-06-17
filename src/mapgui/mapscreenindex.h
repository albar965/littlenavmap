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

#ifndef LITTLENAVMAP_MAPSCREENINDEX_H
#define LITTLENAVMAP_MAPSCREENINDEX_H

#include "fs/sc/simconnectdata.h"

#include "route/route.h"

namespace map {
struct MapSearchResult;

}

namespace Marble {
class GeoDataLatLonBox;
}

class MapPaintWidget;
class AirwayTrackQuery;
class AirportQuery;
class MapPaintLayer;

/*
 * Keeps an indes of certain map objects like flight plan lines, airway lines in screen coordinates
 * to allow mouse over reaction.
 * Also maintains distance measurement lines and range rings.
 * All get nearest methods return objects sorted by distance
 */
class MapScreenIndex
{
public:
  MapScreenIndex(MapPaintWidget *mapPaintWidgetParam, MapPaintLayer *mapPaintLayer);
  ~MapScreenIndex();

  void copy(const MapScreenIndex& other);

  /*
   * Finds all objects near the screen coordinates with maximal distance of maxDistance to xs/ys.
   * Gets airways, visible map objects like airports, navaids and lightlighted objects.
   * @param result Result ordered by distance to xs/ys
   * @param xs/ys Screen coordinates.
   * @param maxDistance maximum distance to xs/ys
   */
  void getAllNearest(int xs, int ys, int maxDistance, map::MapSearchResult& result,
                     map::MapObjectQueryTypes types) const;

  /* Get nearest distance measurement line index (only the endpoint)
   * or -1 if nothing was found near the cursor position. Index points into the list of getDistanceMarks */
  int getNearestDistanceMarkIndex(int xs, int ys, int maxDistance) const;

  /* Get nearest range rings index (only the centerpoint)
   * or -1 if nothing was found near the cursor position. Index points into the list of getRangeMarks */
  int getNearestRangeMarkIndex(int xs, int ys, int maxDistance) const;
  int getNearestTrafficPatternIndex(int xs, int ys, int maxDistance) const;
  int getNearestHoldIndex(int xs, int ys, int maxDistance) const;

  /* Get index of nearest flight plan leg or -1 if nothing was found nearby or cursor is not along a leg. */
  int getNearestRouteLegIndex(int xs, int ys, int maxDistance) const;

  /* Get index of nearest flight plan waypoint or -1 if nothing was found nearby. */
  int getNearestRoutePointIndex(int xs, int ys, int maxDistance) const;

  /* Update geometry after a route or scroll or map change */
  void updateAllGeometry(const Marble::GeoDataLatLonBox& curBox);
  void updateRouteScreenGeometry(const Marble::GeoDataLatLonBox& curBox);
  void updateAirwayScreenGeometry(const Marble::GeoDataLatLonBox& curBox);

  void updateAirspaceScreenGeometry(const Marble::GeoDataLatLonBox& curBox);
  void updateIlsScreenGeometry(const Marble::GeoDataLatLonBox& curBox);
  void updateLogEntryScreenGeometry(const Marble::GeoDataLatLonBox& curBox);

  /* Clear internal caches */
  void resetAirspaceOnlineScreenGeometry();
  void resetIlsScreenGeometry();

  /* Save and restore distance markers and range rings */
  void saveState() const;
  void restoreState();

  /* Get objects that are highlighted because of selected flight plan legs in the table */
  const QList<int>& getRouteHighlights() const
  {
    return routeHighlights;
  }

  void setRouteHighlights(const QList<int>& value)
  {
    routeHighlights = value;
  }

  /* Get objects that are highlighted because of selected rows in a search result table */
  void changeSearchHighlights(const map::MapSearchResult& newHighlights)
  {
    searchHighlights = newHighlights;
  }

  const map::MapSearchResult& getSearchHighlights() const
  {
    return searchHighlights;
  }

  void setApproachLegHighlights(const proc::MapProcedureLeg *leg)
  {
    if(leg != nullptr)
      approachLegHighlights = *leg;
    else
      approachLegHighlights = proc::MapProcedureLeg();
  }

  const proc::MapProcedureLeg& getApproachLegHighlights() const
  {
    return approachLegHighlights;
  }

  /* Get range rings */
  QList<map::RangeMarker>& getRangeMarks()
  {
    return rangeMarks;
  }

  const QList<map::RangeMarker>& getRangeMarks() const
  {
    return rangeMarks;
  }

  /* Get distance measurement lines */
  QList<map::DistanceMarker>& getDistanceMarks()
  {
    return distanceMarks;
  }

  const QList<map::DistanceMarker>& getDistanceMarks() const
  {
    return distanceMarks;
  }

  /* Airfield traffic patterns. */
  QList<map::TrafficPattern>& getTrafficPatterns()
  {
    return trafficPatterns;
  }

  const QList<map::TrafficPattern>& getTrafficPatterns() const
  {
    return trafficPatterns;
  }

  /* Airfield traffic patterns. */
  QList<map::Hold>& getHolds()
  {
    return holds;
  }

  const QList<map::Hold>& getHolds() const
  {
    return holds;
  }

  const atools::fs::sc::SimConnectUserAircraft& getUserAircraft() const
  {
    return simData.getUserAircraftConst();
  }

  const atools::fs::sc::SimConnectData& getSimConnectData() const
  {
    return simData;
  }

  const atools::fs::sc::SimConnectUserAircraft& getLastUserAircraft() const
  {
    return lastSimData.getUserAircraftConst();
  }

  const QVector<atools::fs::sc::SimConnectAircraft>& getAiAircraft() const
  {
    return simData.getAiAircraftConst();
  }

  void updateSimData(const atools::fs::sc::SimConnectData& data)
  {
    simData = data;
  }

  bool isUserAircraftValid() const
  {
    return simData.getUserAircraftConst().getPosition().isValid();
  }

  void updateLastSimData(const atools::fs::sc::SimConnectData& data)
  {
    lastSimData = data;
  }

  const proc::MapProcedureLegs& getProcedureHighlight() const
  {
    return approachHighlight;
  }

  proc::MapProcedureLegs& getProcedureHighlight()
  {
    return approachHighlight;
  }

  void setProfileHighlight(const atools::geo::Pos& value)
  {
    profileHighlight = value;
  }

  const atools::geo::Pos& getProfileHighlight() const
  {
    return profileHighlight;
  }

  const QList<map::MapAirspace>& getAirspaceHighlights() const
  {
    return airspaceHighlights;
  }

  const QList<QList<map::MapAirway> >& getAirwayHighlights() const
  {
    return airwayHighlights;
  }

  void changeAirspaceHighlights(const QList<map::MapAirspace>& value)
  {
    airspaceHighlights = value;
  }

  void changeAirwayHighlights(const QList<QList<map::MapAirway> >& value)
  {
    airwayHighlights = value;
  }

  /* For debug functions */
  QList<std::pair<int, QLine> > getAirwayLines() const
  {
    return airwayLines;
  }

private:
  void getNearestAirways(int xs, int ys, int maxDistance, map::MapSearchResult& result) const;
  void getNearestLogEntries(int xs, int ys, int maxDistance, map::MapSearchResult& result) const;

  void getNearestIls(int xs, int ys, int maxDistance, map::MapSearchResult& result) const;
  void getNearestAirspaces(int xs, int ys, map::MapSearchResult& result) const;
  void getNearestHighlights(int xs, int ys, int maxDistance, map::MapSearchResult& result,
                            map::MapObjectQueryTypes types) const;
  void getNearestProcedureHighlights(int xs, int ys, int maxDistance, map::MapSearchResult& result,
                                     map::MapObjectQueryTypes types) const;
  void updateAirspaceScreenGeometryInternal(QSet<map::MapAirspaceId>& ids,
                                            map::MapAirspaceSources source,
                                            const Marble::GeoDataLatLonBox& curBox, bool highlights);
  void updateAirwayScreenGeometryInternal(QSet<int>& ids, const Marble::GeoDataLatLonBox& curBox, bool highlight);

  void updateLineScreenGeometry(QList<std::pair<int, QLine> >& index, int id, const atools::geo::Line& line,
                                const Marble::GeoDataLatLonBox& curBox,
                                const CoordinateConverter& conv);

  QSet<int> nearestLineIds(const QList<std::pair<int, QLine> >& lineList, int xs, int ys, int maxDistance,
                           bool lineDistanceOnly) const;

  template<typename TYPE>
  int getNearestIndex(int xs, int ys, int maxDistance, const QList<TYPE>& typeList) const;

  atools::fs::sc::SimConnectData simData, lastSimData;
  MapPaintWidget *mapPaintWidget;
  MapQuery *mapQuery;
  AirwayTrackQuery *airwayQuery;
  AirportQuery *airportQuery;
  MapPaintLayer *paintLayer;

  /* All highlights from search windows - also online airspaces */
  map::MapSearchResult searchHighlights;
  proc::MapProcedureLeg approachLegHighlights;
  proc::MapProcedureLegs approachHighlight;

  /* All airspace highlights from information window */
  QList<map::MapAirspace> airspaceHighlights;

  /* All airway highlights from information window */
  QList<QList<map::MapAirway> > airwayHighlights;

  /* Circle from elevation profile */
  atools::geo::Pos profileHighlight;

  /* Circles from route table */
  QList<int> routeHighlights;

  /* Objects that will be saved */
  QList<map::RangeMarker> rangeMarks;
  QList<map::DistanceMarker> distanceMarks;
  QList<map::TrafficPattern> trafficPatterns;
  QList<map::Hold> holds;

  /* Cached screen coordinates for flight plan to ease mouse cursor change. */
  QList<std::pair<int, QLine> > routeLines;
  QList<std::pair<int, QPoint> > routePoints;

  /* Geometry objects that are cached in screen coordinate system for faster access to tooltips etc. */
  QList<std::pair<int, QLine> > airwayLines;

  /* Collects logbook entry route and direct line geometry */
  QList<std::pair<int, QLine> > logEntryLines;
  QList<std::pair<map::MapAirspaceId, QPolygon> > airspacePolygons;
  QList<std::pair<int, QPolygon> > ilsPolygons;
  QList<std::pair<int, QLine> > ilsLines; /* Index ILS center lines separately to allow
                                           * tooltips when getting the cursor near a line */

};

#endif // LITTLENAVMAP_MAPSCREENINDEX_H
