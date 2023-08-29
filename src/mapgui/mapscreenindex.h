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

#ifndef LITTLENAVMAP_MAPSCREENINDEX_H
#define LITTLENAVMAP_MAPSCREENINDEX_H

#include "common/mapflags.h"

#include <QDateTime>
#include <QHash>

namespace atools {
namespace fs {
namespace sc {
class SimConnectData;
class SimConnectAircraft;
class SimConnectUserAircraft;
}
}
namespace util {
class MovingAverageTime;
}
namespace geo {
class Line;
class Pos;
}
}

namespace proc {
struct MapProcedureLeg;
struct MapProcedureLegs;
}

namespace map {
struct DistanceMarker;
struct HoldingMarker;
struct MapAirport;
struct MapAirportMsa;
struct MapAirspace;
struct MapAirway;
struct MapHelipad;
struct MapHolding;
struct MapIls;
struct MapLogbookEntry;
struct MapNdb;
struct MapRef;
struct MapParking;
struct MapResult;
struct MapUserpoint;
struct MapUserpointRoute;
struct MapVor;
struct MapWaypoint;
struct MsaMarker;
struct PatternMarker;
struct RangeMarker;
}

namespace Marble {
class GeoDataLatLonBox;
}

class MapPaintWidget;
class AirwayTrackQuery;
class AirportQuery;
class MapPaintLayer;
class MapQuery;
class CoordinateConverter;

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

  /* Do not allow implicit copying */
  MapScreenIndex(MapScreenIndex const&) = delete;
  MapScreenIndex& operator=(MapScreenIndex const&) = delete;

  /*
   * Finds all objects near the screen coordinates with maximal distance of maxDistance to xs/ys.
   * Gets airways, visible map objects like airports, navaids and highlighted objects.
   * @param result Result ordered by distance to xs/ys
   * @param xs/ys Screen coordinates.
   * @param maxDistance maximum distance to xs/ys
   */
  void getAllNearest(const QPoint& point, int maxDistance, map::MapResult& result, map::MapObjectQueryTypes types) const;

  /* Get nearest map features (only the endpoint)
   * or -1 if nothing was found near the cursor position. Index points into the list of getDistanceMarks */
  int getNearestDistanceMarkId(int xs, int ys, int maxDistance, bool *origin = nullptr) const;
  int getNearestRangeMarkId(int xs, int ys, int maxDistance) const;
  int getNearestTrafficPatternId(int xs, int ys, int maxDistance) const;
  int getNearestHoldId(int xs, int ys, int maxDistance) const;
  int getNearestAirportMsaId(int xs, int ys, int maxDistance) const;

  /* Get index of nearest flight plan leg or -1 if nothing was found nearby or cursor is not along a leg. */
  int getNearestRouteLegIndex(int xs, int ys, int maxDistance) const;

  /* Get index of nearest flight plan waypoint or -1 if nothing was found nearby. */
  int getNearestRoutePointIndex(int xs, int ys, int maxDistance, bool editableOnly) const;

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
  void changeSearchHighlights(const map::MapResult& newHighlights);

  const map::MapResult& getSearchHighlights() const
  {
    return *searchHighlights;
  }

  /* Multiple procedures and legs preview from search */
  const QVector<proc::MapProcedureLegs>& getProcedureHighlights() const
  {
    return procedureHighlights;
  }

  void setProcedureHighlights(const QVector<proc::MapProcedureLegs>& value);

  /* Single procedure from search selection */
  const proc::MapProcedureLegs& getProcedureHighlight() const
  {
    return *procedureHighlight;
  }

  void setProcedureHighlight(const proc::MapProcedureLegs& newHighlight);

  /* Single selected leg in the procedure search tree */
  const proc::MapProcedureLeg& getProcedureLegHighlight() const
  {
    return *procedureLegHighlight;
  }

  void setProcedureLegHighlight(const proc::MapProcedureLeg& newLegHighlight);

  /* Get range rings */
  const QHash<int, map::RangeMarker>& getRangeMarks() const
  {
    return rangeMarks;
  }

  /* Get distance measurement lines */
  const QHash<int, map::DistanceMarker>& getDistanceMarks() const
  {
    return distanceMarks;
  }

  /* Get for editing */
  map::DistanceMarker& getDistanceMark(int id)
  {
    return distanceMarks[id];
  }

  /* Airfield traffic patterns. */
  const QHash<int, map::PatternMarker>& getPatternMarks() const
  {
    return patternMarks;
  }

  /* Holdings. */
  const QHash<int, map::HoldingMarker>& getHoldingMarks() const
  {
    return holdingMarks;
  }

  /* Airport MSA. */
  const QHash<int, map::MsaMarker>& getMsaMarks() const
  {
    return msaMarks;
  }

  /* Add user features. Id has to be set before. */
  void addRangeMark(const map::RangeMarker& obj);
  void addPatternMark(const map::PatternMarker& obj);
  void addDistanceMark(const map::DistanceMarker& obj);
  void addHoldingMark(const map::HoldingMarker& obj);
  void addMsaMark(const map::MsaMarker& obj);

  /* Remove user features by generated id */
  void removeRangeMark(int id);
  void removePatternMark(int id);
  void removeDistanceMark(int id);
  void removeHoldingMark(int id);
  void removeMsaMark(int id);

  void clearAllMarkers(map::MapTypes types);

  /* Update measurement lines positions */
  void updateDistanceMarkerFromPos(int id, const atools::geo::Pos& pos);
  void updateDistanceMarkerToPos(int id, const atools::geo::Pos& pos);

  /* Update measurement lines all fields except id where the parameter is used. */
  void updateDistanceMarker(int id, const map::DistanceMarker& marker);

  // ====================
  const atools::fs::sc::SimConnectUserAircraft& getUserAircraft() const;

  const atools::fs::sc::SimConnectData& getSimConnectData() const
  {
    return *simData;
  }

  const atools::fs::sc::SimConnectUserAircraft& getLastUserAircraft() const;

  const QVector<atools::fs::sc::SimConnectAircraft>& getAiAircraft() const;

  void clearSimData();

  void updateSimData(const atools::fs::sc::SimConnectData& data);

  void updateLastSimData(const atools::fs::sc::SimConnectData& data);

  void setProfileHighlight(const atools::geo::Pos& value);

  const atools::geo::Pos& getProfileHighlight() const;

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

  const QVector<map::MapRef>& getRouteDrawnNavaidsConst() const
  {
    return routeDrawnNavaids;
  }

  QVector<map::MapRef> *getRouteDrawnNavaids()
  {
    return &routeDrawnNavaids;
  }

  /* Get average ground speed and turn speed in degrees per second for user aircraft. Average is calculated for 2 seconds. */
  void getAverageGroundAndTurnSpeed(float& groundSpeedKts, float& turnSpeedDegPerSec) const;

private:
  void getNearestAirways(int xs, int ys, int maxDistance, map::MapResult& result) const;
  void getNearestLogEntries(int xs, int ys, int maxDistance, map::MapResult& result) const;

  void getNearestIls(int xs, int ys, int maxDistance, map::MapResult& result) const;
  void getNearestAirspaces(int xs, int ys, map::MapResult& result) const;
  void getNearestHighlights(int xs, int ys, int maxDistance, map::MapResult& result, map::MapObjectQueryTypes types) const;
  void getNearestProcedureHighlights(int xs, int ys, int maxDistance, map::MapResult& result, map::MapObjectQueryTypes types) const;
  void nearestProcedureHighlightsInternal(int xs, int ys, int maxDistance, map::MapResult& result, map::MapObjectQueryTypes types,
                                          const QVector<proc::MapProcedureLegs>& procedureLegs, bool previewAll) const;
  void updateAirspaceScreenGeometryInternal(QSet<map::MapAirspaceId>& ids, map::MapAirspaceSources source,
                                            const Marble::GeoDataLatLonBox& curBox, bool highlights);
  void updateAirwayScreenGeometryInternal(QSet<int>& ids, const Marble::GeoDataLatLonBox& curBox, bool highlight);

  void updateLineScreenGeometry(QList<std::pair<int, QLine> >& index, int id, const atools::geo::Line& line,
                                const Marble::GeoDataLatLonBox& curBox, const CoordinateConverter& conv);

  /* Fill average values for ground speed and turn speed for turn path display. */
  void updateAverageTurn();

  QSet<int> nearestLineIds(const QList<std::pair<int, QLine> >& lineList, int xs, int ys, int maxDistance, bool lineDistanceOnly) const;

  template<typename TYPE>
  int getNearestId(int xs, int ys, int maxDistance, const QHash<int, TYPE>& typeList) const;

  atools::fs::sc::SimConnectData *simData, *lastSimData;

  /* Average values for ground speed and turn speed for turn path display. */
  atools::fs::sc::SimConnectUserAircraft *lastUserAircraftForAverage;
  QDateTime lastUserAircraftForAverageTs;

  /* Moving average for speed and lateral angular speed. Value1 is GS and value2 is turn speed (track change per second). */
  atools::util::MovingAverageTime *movingAverageSimAircraft;

  MapPaintWidget *mapWidget;
  AirportQuery *airportQuery;
  MapPaintLayer *paintLayer;

  /* All highlights from search windows - also online airspaces */
  map::MapResult *searchHighlights;

  /* Cleared before and filled while painting with all navaids that were actually painted from route and procedures including recommended.
   * This includes route userpoints */
  QVector<map::MapRef> routeDrawnNavaids;

  /* One procedure highlight from selection */
  proc::MapProcedureLeg *procedureLegHighlight = nullptr;
  /* More than one leg from multi preview */
  proc::MapProcedureLegs *procedureHighlight = nullptr;

  /* Highlights or leg fix and related fix */
  QVector<proc::MapProcedureLegs> procedureHighlights;

  /* All airspace highlights from information window */
  QList<map::MapAirspace> airspaceHighlights;

  /* All airway highlights from information window */
  QList<QList<map::MapAirway> > airwayHighlights;

  /* Circle from elevation profile */
  atools::geo::Pos *profileHighlight;

  /* Circles from route table */
  QList<int> routeHighlights;

  /* Objects that will be saved */
  QHash<int, map::RangeMarker> rangeMarks;
  QHash<int, map::DistanceMarker> distanceMarks;
  QHash<int, map::PatternMarker> patternMarks;
  QHash<int, map::HoldingMarker> holdingMarks;
  QHash<int, map::MsaMarker> msaMarks;

  /* Cached screen coordinates for flight plan to ease mouse cursor change. */
  QList<std::pair<int, QLine> > routeLines;
  QList<std::pair<int, QPoint> > routePointsEditable; /* Editable points */
  QList<std::pair<int, QPoint> > routePointsAll; /* All points */

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
