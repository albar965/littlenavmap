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

#include "mapgui/mapscreenindex.h"

#include "navapp.h"
#include "common/proctypes.h"
#include "route/routecontroller.h"
#include "online/onlinedatacontroller.h"
#include "logbook/logdatacontroller.h"
#include "mapgui/mapscale.h"
#include "mapgui/mapfunctions.h"
#include "mapgui/mapwidget.h"
#include "mappainter/mappaintlayer.h"
#include "mapgui/maplayer.h"
#include "common/maptypes.h"
#include "query/airportquery.h"
#include "mapgui/mapmarkhandler.h"
#include "common/maptools.h"
#include "query/mapquery.h"
#include "query/airwaytrackquery.h"
#include "query/airportquery.h"
#include "common/coordinateconverter.h"
#include "common/constants.h"
#include "settings/settings.h"
#include "airspace/airspacecontroller.h"
#include "geo/linestring.h"

#include <marble/GeoDataLineString.h>

using atools::geo::Pos;
using atools::geo::Line;
using atools::geo::LineString;
using atools::geo::Rect;
using map::MapAirway;
using Marble::GeoDataLineString;
using Marble::GeoDataCoordinates;

MapScreenIndex::MapScreenIndex(MapPaintWidget *mapPaintWidgetParam, MapPaintLayer *mapPaintLayer)
  : mapPaintWidget(mapPaintWidgetParam), paintLayer(mapPaintLayer)
{
  mapQuery = NavApp::getMapQuery();
  airwayQuery = NavApp::getAirwayTrackQuery();
  airportQuery = NavApp::getAirportQuerySim();
}

MapScreenIndex::~MapScreenIndex()
{

}

void MapScreenIndex::copy(const MapScreenIndex& other)
{
  simData = other.simData;
  lastSimData = other.lastSimData;
  searchHighlights = other.searchHighlights;
  approachLegHighlights = other.approachLegHighlights;
  approachHighlight = other.approachHighlight;
  airspaceHighlights = other.airspaceHighlights;
  airwayHighlights = other.airwayHighlights;
  profileHighlight = other.profileHighlight;
  routeHighlights = other.routeHighlights;
  rangeMarks = other.rangeMarks;
  distanceMarks = other.distanceMarks;
  trafficPatterns = other.trafficPatterns;
  routeLines = other.routeLines;
  airwayLines = other.airwayLines;
  logEntryLines = other.logEntryLines;
  airspacePolygons = other.airspacePolygons;
  ilsPolygons = other.ilsPolygons;
  ilsLines = other.ilsLines;
  routePoints = other.routePoints;
}

void MapScreenIndex::updateAirspaceScreenGeometryInternal(QSet<map::MapAirspaceId>& ids, map::MapAirspaceSources source,
                                                          const Marble::GeoDataLatLonBox& curBox, bool highlights)
{
  const MapScale *scale = paintLayer->getMapScale();
  AirspaceController *controller = NavApp::getAirspaceController();

  if(scale->isValid() && controller != nullptr && paintLayer != nullptr)
  {
    AirspaceVector airspaces;

    // Get displayed airspaces ================================
    if(!highlights && paintLayer->getShownMapObjects().testFlag(map::AIRSPACE))
      controller->getAirspaces(airspaces,
                               curBox, paintLayer->getMapLayer(), mapPaintWidget->getShownAirspaceTypesByLayer(),
                               NavApp::getRouteConst().getCruisingAltitudeFeet(), false, source);

    // Get highlighted airspaces from info window ================================
    for(const map::MapAirspace& airspace : airspaceHighlights)
      airspaces.append(&airspace);

    // Get highlighted airspaces from online center search ================================
    for(const map::MapAirspace& airspace : searchHighlights.airspaces)
      airspaces.append(&airspace);

    CoordinateConverter conv(mapPaintWidget->viewport());
    for(const map::MapAirspace *airspace : airspaces)
    {
      if(!(airspace->type & mapPaintWidget->getShownAirspaceTypesByLayer().types) && !highlights)
        continue;

      Marble::GeoDataLatLonBox airspacebox = conv.toGdc(airspace->bounding);

      // Check if airspace overlaps with current screen and is not already in list
      if(airspacebox.intersects(curBox) && !ids.contains(airspace->combinedId()))
      {

        const atools::geo::LineString *lines = controller->getAirspaceGeometry(airspace->combinedId());
        if(lines != nullptr)
        {
          QVector<QPolygonF> polygons = conv.wToS(*lines);

          for(const QPolygonF& poly : polygons)
          {
            // Cut off all polygon parts that are not visible on screen
            airspacePolygons.append(std::make_pair(airspace->combinedId(),
                                                   poly.intersected(QPolygon(mapPaintWidget->rect())).toPolygon()));
            ids.insert(airspace->combinedId());
          }
        }
      }
    }
  }
}

void MapScreenIndex::resetAirspaceOnlineScreenGeometry()
{
  // Clear internal caches
  NavApp::getAirspaceController()->resetAirspaceOnlineScreenGeometry();
}

void MapScreenIndex::resetIlsScreenGeometry()
{
  ilsPolygons.clear();
  ilsLines.clear();
}

void MapScreenIndex::updateAirspaceScreenGeometry(const Marble::GeoDataLatLonBox& curBox)
{
  airspacePolygons.clear();
  if(paintLayer == nullptr || paintLayer->getMapLayer() == nullptr)
    return;

  // Use ID set to check for duplicates between calls
  QSet<map::MapAirspaceId> ids;

  // First get geometry from highlights
  updateAirspaceScreenGeometryInternal(ids, NavApp::getAirspaceController()->getAirspaceSources(), curBox, true);

  if(!paintLayer->getMapLayer()->isAirspace() || !paintLayer->getShownMapObjects().testFlag(map::AIRSPACE))
    return;

  if(paintLayer->getMapLayerEffective()->isAirportDiagram())
    return;

  // Do not put into index if nothing is drawn
  if(mapPaintWidget->distance() >= layer::DISTANCE_CUT_OFF_LIMIT)
    return;

  // Get geometry from visible airspaces
  updateAirspaceScreenGeometryInternal(ids, NavApp::getAirspaceController()->getAirspaceSources(), curBox, false);
}

void MapScreenIndex::updateIlsScreenGeometry(const Marble::GeoDataLatLonBox& curBox)
{
  resetIlsScreenGeometry();

  if(paintLayer == nullptr || paintLayer->getMapLayer() == nullptr)
    return;

  if(!paintLayer->getMapLayer()->isIls() || !paintLayer->getShownMapObjects().testFlag(map::ILS))
    return;

  // Do not put into index if nothing is drawn
  if(mapPaintWidget->distance() >= layer::DISTANCE_CUT_OFF_LIMIT)
    return;

  if(paintLayer->getShownMapObjects().testFlag(map::ILS))
  {
    const MapScale *scale = paintLayer->getMapScale();

    if(scale->isValid())
    {
      const QList<map::MapIls> *ilsList = mapQuery->getIls(curBox, paintLayer->getMapLayer(), false);

      if(ilsList != nullptr)
      {
        CoordinateConverter conv(mapPaintWidget->viewport());
        for(const map::MapIls& ils : *ilsList)
        {
          Marble::GeoDataLatLonBox ilsbox(ils.bounding.getNorth(), ils.bounding.getSouth(),
                                          ils.bounding.getEast(), ils.bounding.getWest(),
                                          Marble::GeoDataCoordinates::Degree);

          if(ilsbox.intersects(curBox))
          {
            updateLineScreenGeometry(ilsLines, ils.id, ils.centerLine(), curBox, conv);

            QPolygon polygon;
            bool hidden;
            for(const Pos& pos : ils.boundary())
            {
              int xs, ys;
              conv.wToS(pos, xs, ys, CoordinateConverter::DEFAULT_WTOS_SIZE, &hidden);
              if(!hidden)
                polygon.append(QPoint(xs, ys));
            }
            polygon = polygon.intersected(QPolygon(mapPaintWidget->rect()));
            if(!polygon.isEmpty())
              ilsPolygons.append(std::make_pair(ils.id, polygon));
          }
        }
      }
    }
  }
}

void MapScreenIndex::updateLogEntryScreenGeometry(const Marble::GeoDataLatLonBox& curBox)
{
  logEntryLines.clear();

  const MapScale *scale = paintLayer->getMapScale();

  if(scale->isValid())
  {
    map::MapObjectDisplayTypes types = paintLayer->getShownMapObjectDisplayTypes();

    if(types.testFlag(map::LOGBOOK_DIRECT) || types.testFlag(map::LOGBOOK_ROUTE))
    {
      CoordinateConverter conv(mapPaintWidget->viewport());
      for(map::MapLogbookEntry& entry : searchHighlights.logbookEntries)
      {
        if(entry.isValid())
        {
          if(types.testFlag(map::LOGBOOK_DIRECT))
            updateLineScreenGeometry(logEntryLines, entry.id, entry.line(), curBox, conv);

          if(types.testFlag(map::LOGBOOK_ROUTE))
          {
            // Get geometry for flight plan if preview is enabled
            const atools::geo::LineString *geo = NavApp::getLogdataController()->getRouteGeometry(entry.id);
            if(geo != nullptr)
            {
              for(int i = 0; i < geo->size() - 1; i++)
                updateLineScreenGeometry(logEntryLines, entry.id, Line(geo->at(i), geo->at(i + 1)), curBox, conv);
            }
          }
        }
      }
    }
  }
}

void MapScreenIndex::updateAirwayScreenGeometry(const Marble::GeoDataLatLonBox& curBox)
{
  if(paintLayer == nullptr || paintLayer->getMapLayer() == nullptr)
    return;

  airwayLines.clear();

  // Use ID set to check for duplicates between calls
  QSet<int> ids;

  // First get geometry from highlights
  updateAirwayScreenGeometryInternal(ids, curBox, true /* highlight */);

  // Get geometry from visible airspaces
  updateAirwayScreenGeometryInternal(ids, curBox, false /* highlight */);
}

void MapScreenIndex::updateAirwayScreenGeometryInternal(QSet<int>& ids, const Marble::GeoDataLatLonBox& curBox,
                                                        bool highlight)
{
  const MapScale *scale = paintLayer->getMapScale();

  if(scale->isValid())
  {
    CoordinateConverter conv(mapPaintWidget->viewport());
    if(!highlight)
    {
      // Get geometry from visible airspaces
      bool showJet = paintLayer->getShownMapObjects().testFlag(map::AIRWAYJ);
      bool showVictor = paintLayer->getShownMapObjects().testFlag(map::AIRWAYV);
      if(paintLayer->getMapLayer()->isAirway() && (showJet || showVictor))
      {
        // Airways are visible on map - get them from the cache/database
        QList<MapAirway> airways;
        airwayQuery->getAirways(airways, curBox, paintLayer->getMapLayer(), false);

        for(int i = 0; i < airways.size(); i++)
        {
          const MapAirway& airway = airways.at(i);
          if(ids.contains(airway.id))
            continue;

          if((airway.type == map::AIRWAY_VICTOR && !showVictor) ||
             (airway.type == map::AIRWAY_JET && !showJet))
            // Not visible by map setting
            continue;

          updateLineScreenGeometry(airwayLines, airway.id, Line(airway.from, airway.to), curBox, conv);
          ids.insert(airway.id);
        }
      }

      bool showTrack = paintLayer->getShownMapObjects().testFlag(map::TRACK);
      if(paintLayer->getMapLayer()->isTrack() && showTrack)
      {
        // Airways are visible on map - get them from the cache/database
        QList<MapAirway> tracks;
        airwayQuery->getTracks(tracks, curBox, paintLayer->getMapLayer(), false);

        for(int i = 0; i < tracks.size(); i++)
        {
          const MapAirway& track = tracks.at(i);
          if(ids.contains(track.id))
            continue;

          if(track.isTrack() && !showTrack)
            // Not visible by map setting
            continue;

          updateLineScreenGeometry(airwayLines, track.id, Line(track.from, track.to), curBox, conv);
          ids.insert(track.id);
        }
      }
    }
    else
    {
      // Get geometry from highlights
      for(const QList<map::MapAirway>& airwayList : airwayHighlights)
      {
        for(const map::MapAirway& airway : airwayList)
        {
          if(ids.contains(airway.id))
            continue;
          updateLineScreenGeometry(airwayLines, airway.id, Line(airway.from, airway.to), curBox, conv);
          ids.insert(airway.id);
        }
      }
    }
  }
}

void MapScreenIndex::updateLineScreenGeometry(QList<std::pair<int, QLine> >& index,
                                              int id, const atools::geo::Line& line,
                                              const Marble::GeoDataLatLonBox& curBox,
                                              const CoordinateConverter& conv)
{
  Marble::GeoDataLineString geoLineStr = conv.toGdcStr(line);
  QRect mapGeo = mapPaintWidget->rect();

  QList<Marble::GeoDataLatLonBox> curBoxCorrectedList = query::splitAtAntiMeridian(curBox);
  QVector<Marble::GeoDataLineString *> geoLineStringVector = geoLineStr.toDateLineCorrected();
  for(Marble::GeoDataLatLonBox curBoxCorrected : curBoxCorrectedList)
  {
    for(const Marble::GeoDataLineString *lineCorrected : geoLineStringVector)
    {
      if(lineCorrected->latLonAltBox().intersects(curBoxCorrected))
      {
        QVector<QPolygonF> polys = conv.wToS(*lineCorrected);
        for(const QPolygonF& p : polys)
        {
          for(int k = 0; k < p.size() - 1; k++)
          {
            QLine line(p.at(k).toPoint(), p.at(k + 1).toPoint());
            QRect rect(line.p1(), line.p2());
            rect = rect.normalized();
            // Avoid points or flat rectangles (lines)
            rect.adjust(-1, -1, 1, 1);

            if(mapGeo.intersects(rect))
              index.append(std::make_pair(id, line));
          }
        }
      }
    }
  }
  qDeleteAll(geoLineStringVector);
}

void MapScreenIndex::saveState() const
{
  atools::settings::Settings& s = atools::settings::Settings::instance();
  s.setValueVar(lnm::MAP_DISTANCEMARKERS, QVariant::fromValue<QList<map::DistanceMarker> >(distanceMarks));
  s.setValueVar(lnm::MAP_RANGEMARKERS, QVariant::fromValue<QList<map::RangeMarker> >(rangeMarks));
  s.setValueVar(lnm::MAP_TRAFFICPATTERNS, QVariant::fromValue<QList<map::TrafficPattern> >(trafficPatterns));
  s.setValueVar(lnm::MAP_HOLDS, QVariant::fromValue<QList<map::Hold> >(holds));
}

void MapScreenIndex::restoreState()
{
  atools::settings::Settings& s = atools::settings::Settings::instance();
  distanceMarks = s.valueVar(lnm::MAP_DISTANCEMARKERS).value<QList<map::DistanceMarker> >();
  rangeMarks = s.valueVar(lnm::MAP_RANGEMARKERS).value<QList<map::RangeMarker> >();
  trafficPatterns = s.valueVar(lnm::MAP_TRAFFICPATTERNS).value<QList<map::TrafficPattern> >();
  holds = s.valueVar(lnm::MAP_HOLDS).value<QList<map::Hold> >();
}

void MapScreenIndex::updateRouteScreenGeometry(const Marble::GeoDataLatLonBox& curBox)
{
  const Route& route = NavApp::getRouteConst();

  routeLines.clear();
  routePoints.clear();

  QList<std::pair<int, QPoint> > airportPoints;
  QList<std::pair<int, QPoint> > otherPoints;

  CoordinateConverter conv(mapPaintWidget->viewport());
  const MapScale *scale = paintLayer->getMapScale();
  if(scale->isValid())
  {
    Pos p1;
    for(int i = 0; i < route.size(); i++)
    {
      const RouteLeg& routeLeg = route.value(i);

      const Pos& p2 = routeLeg.getPosition();
      int x2, y2;
      if(conv.wToS(p2, x2, y2))
      {
        map::MapObjectTypes type = routeLeg.getMapObjectType();
        if(type == map::AIRPORT && (i == route.getDepartureAirportLegIndex() ||
                                    i == route.getDestinationAirportLegIndex() ||
                                    routeLeg.isAlternate()))
          // Departure, destination or alternate airport - put to from of list for higher priority
          airportPoints.append(std::make_pair(i, QPoint(x2, y2)));
        else if(route.canEditPoint(i))
          otherPoints.append(std::make_pair(i, QPoint(x2, y2)));
      }

      if(!route.canEditLeg(i))
      {
        p1 = p2;
        continue;
      }

      if(p1.isValid())
        updateLineScreenGeometry(routeLines, i - 1, Line(p1, p2), curBox, conv);
      p1 = p2;
    }
    routePoints.append(airportPoints);
    routePoints.append(otherPoints);
  }
}

void MapScreenIndex::getAllNearest(int xs, int ys, int maxDistance, map::MapSearchResult& result,
                                   map::MapObjectQueryTypes types) const
{
  using maptools::insertSortedByDistance;

  CoordinateConverter conv(mapPaintWidget->viewport());
  const MapLayer *mapLayer = paintLayer->getMapLayer();
  const MapLayer *mapLayerEffective = paintLayer->getMapLayerEffective();

  map::MapObjectTypes shown = paintLayer->getShownMapObjects();
  map::MapObjectDisplayTypes shownDisplay = paintLayer->getShownMapObjectDisplayTypes();

  // Check for user aircraft
  result.userAircraft = atools::fs::sc::SimConnectUserAircraft();
  if(shown & map::AIRCRAFT && NavApp::isConnectedAndAircraft())
  {
    const atools::fs::sc::SimConnectUserAircraft& user = simData.getUserAircraftConst();
    int x, y;
    if(conv.wToS(user.getPosition(), x, y))
    {
      if(atools::geo::manhattanDistance(x, y, xs, ys) < maxDistance)
        result.userAircraft = user;
    }
  }

  // Check for AI / multiplayer aircraft from simulator ==============================
  int x, y;

  // Add boats ======================================
  result.aiAircraft.clear();
  if(NavApp::isConnected())
  {
    if(shown & map::AIRCRAFT_AI_SHIP && mapLayer->isAiShipLarge())
    {
      for(const atools::fs::sc::SimConnectAircraft& obj : simData.getAiAircraftConst())
      {
        if(obj.isAnyBoat() && (obj.getModelRadiusCorrected() * 2 > layer::LARGE_SHIP_SIZE || mapLayer->isAiShipSmall()))
        {
          if(conv.wToS(obj.getPosition(), x, y))
            if((atools::geo::manhattanDistance(x, y, xs, ys)) < maxDistance)
              insertSortedByDistance(conv, result.aiAircraft, nullptr, xs, ys, obj);
        }
      }
    }
  }

  if(shown & map::AIRCRAFT_AI)
  {
    // Add AI or injected multiplayer aircraft ======================================
    if(NavApp::isConnected() || mapPaintWidget->getUserAircraft().isDebug())
    {
      for(const atools::fs::sc::SimConnectAircraft& obj : mapPaintWidget->getAiAircraft())
      {
        if(!obj.isAnyBoat() && mapfunc::aircraftVisible(obj, mapLayer))
        {
          if(conv.wToS(obj.getPosition(), x, y))
          {
            if((atools::geo::manhattanDistance(x, y, xs, ys)) < maxDistance)
            {
              // Add online network shadow aircraft from simulator to online list
              atools::fs::sc::SimConnectAircraft shadow;
              if(NavApp::getOnlinedataController()->getShadowAircraft(shadow, obj))
                insertSortedByDistance(conv, result.onlineAircraft, &result.onlineAircraftIds, xs, ys, shadow);

              insertSortedByDistance(conv, result.aiAircraft, nullptr, xs, ys, obj);
            }
          }
        }
      }
    }

    // Add online clients ======================================
    for(const atools::fs::sc::SimConnectAircraft& obj : *NavApp::getOnlinedataController()->getAircraftFromCache())
    {
      if(mapfunc::aircraftVisible(obj, mapLayer))
      {
        if(conv.wToS(obj.getPosition(), x, y))
          if((atools::geo::manhattanDistance(x, y, xs, ys)) < maxDistance)
            insertSortedByDistance(conv, result.onlineAircraft, &result.onlineAircraftIds, xs, ys, obj);
      }
    }
  }

  // Airways use a screen coordinate buffer
  getNearestLogEntries(xs, ys, maxDistance, result);
  getNearestAirways(xs, ys, maxDistance, result);
  getNearestAirspaces(xs, ys, result);
  getNearestIls(xs, ys, maxDistance, result);

  if(shownDisplay.testFlag(map::FLIGHTPLAN))
  {
    // Get copies from flight plan if visible
    NavApp::getRouteConst().getNearest(conv, xs, ys, maxDistance, result,
                                       types | map::QUERY_PROCEDURES | map::QUERY_PROC_POINTS);

    // Get points of procedure preview
    getNearestProcedureHighlights(xs, ys, maxDistance, result, types);
  }

  // Get copies from highlightMapObjects
  getNearestHighlights(xs, ys, maxDistance, result, types);

  // Get objects from cache - already present objects will be skipped
  // Airway included to fetch waypoints
  mapQuery->getNearestScreenObjects(conv, mapLayer, mapLayerEffective->isAirportDiagram(),
                                    shown & (map::AIRPORT_ALL | map::VOR | map::NDB | map::WAYPOINT | map::MARKER |
                                             map::AIRWAYJ | map::TRACK | map::AIRWAYV | map::USERPOINT | map::LOGBOOK),
                                    xs, ys, maxDistance, result);

  // Update all incomplete objects, especially from search
  for(map::MapAirport& obj : result.airports)
  {
    if(!obj.complete())
    {
      int routeIndex = obj.routeIndex;
      airportQuery->getAirportById(obj, obj.getId());
      obj.routeIndex = routeIndex;
    }
  }

  // Check if pointer is near a wind barb in the one degree grid
  if(paintLayer->getShownMapObjectDisplayTypes() & map::WIND_BARBS)
  {
    // Round screen position to nearest grid cell
    atools::geo::Pos pos = conv.sToW(xs, ys).snapToGrid();
    if(pos.isValid())
    {
      int xg, yg;
      if(conv.wToS(pos, xg, yg))
      {
        // Screen distance in pixel to the nearest cell
        if(atools::geo::manhattanDistance(xg, yg, xs, ys) < maxDistance)
          result.windPos = pos;
      }
    }
  }
}

void MapScreenIndex::getNearestHighlights(int xs, int ys, int maxDistance, map::MapSearchResult& result,
                                          map::MapObjectQueryTypes types) const
{
  using maptools::insertSorted;
  CoordinateConverter conv(mapPaintWidget->viewport());
  insertSorted(conv, xs, ys, searchHighlights.airports, result.airports, &result.airportIds, maxDistance);
  insertSorted(conv, xs, ys, searchHighlights.vors, result.vors, &result.vorIds, maxDistance);
  insertSorted(conv, xs, ys, searchHighlights.ndbs, result.ndbs, &result.ndbIds, maxDistance);
  insertSorted(conv, xs, ys, searchHighlights.waypoints, result.waypoints, &result.waypointIds, maxDistance);
  insertSorted(conv, xs, ys, searchHighlights.userpoints, result.userpoints, &result.userpointIds, maxDistance);
  insertSorted(conv, xs, ys, searchHighlights.airspaces, result.airspaces, nullptr, maxDistance);
  insertSorted(conv, xs, ys, searchHighlights.airways, result.airways, nullptr, maxDistance);
  insertSorted(conv, xs, ys, searchHighlights.ils, result.ils, nullptr, maxDistance);
  insertSorted(conv, xs, ys, searchHighlights.aiAircraft, result.aiAircraft, nullptr, maxDistance);
  insertSorted(conv, xs, ys, searchHighlights.onlineAircraft, result.onlineAircraft, &result.onlineAircraftIds,
               maxDistance);
  insertSorted(conv, xs, ys, searchHighlights.runwayEnds, result.runwayEnds, nullptr, maxDistance);

  // Add only if requested and visible on map
  if(types & map::QUERY_HOLDS && NavApp::getMapMarkHandler()->getMarkTypes() & map::MARK_HOLDS)
    insertSorted(conv, xs, ys, holds, result.holds, nullptr, maxDistance);

  if(types & map::QUERY_PATTERNS && NavApp::getMapMarkHandler()->getMarkTypes() & map::MARK_PATTERNS)
    insertSorted(conv, xs, ys, trafficPatterns, result.trafficPatterns, nullptr, maxDistance);

  if(types & map::QUERY_RANGEMARKER && NavApp::getMapMarkHandler()->getMarkTypes() & map::MARK_RANGE_RINGS)
    insertSorted(conv, xs, ys, rangeMarks, result.rangeMarkers, nullptr, maxDistance);
}

void MapScreenIndex::getNearestProcedureHighlights(int xs, int ys, int maxDistance, map::MapSearchResult& result,
                                                   map::MapObjectQueryTypes types) const
{
  CoordinateConverter conv(mapPaintWidget->viewport());
  int x, y;

  using maptools::insertSorted;

  for(int i = 0; i < approachHighlight.size(); i++)
  {
    const proc::MapProcedureLeg& leg = approachHighlight.at(i);

    insertSorted(conv, xs, ys, leg.navaids.airports, result.airports, &result.airportIds, maxDistance);
    insertSorted(conv, xs, ys, leg.navaids.vors, result.vors, &result.vorIds, maxDistance);
    insertSorted(conv, xs, ys, leg.navaids.ndbs, result.ndbs, &result.ndbIds, maxDistance);
    insertSorted(conv, xs, ys, leg.navaids.waypoints, result.waypoints, &result.waypointIds, maxDistance);
    insertSorted(conv, xs, ys, leg.navaids.userpoints, result.userpoints, &result.userpointIds, maxDistance);
    insertSorted(conv, xs, ys, leg.navaids.airspaces, result.airspaces, nullptr, maxDistance);
    insertSorted(conv, xs, ys, leg.navaids.airways, result.airways, nullptr, maxDistance);
    insertSorted(conv, xs, ys, leg.navaids.ils, result.ils, nullptr, maxDistance);
    insertSorted(conv, xs, ys, leg.navaids.runwayEnds, result.runwayEnds, nullptr, maxDistance);

    if(types & map::QUERY_PROC_POINTS)
    {
      if(conv.wToS(leg.line.getPos2(), x, y))
      {
        if((atools::geo::manhattanDistance(x, y, xs, ys)) < maxDistance)
          result.procPoints.append(proc::MapProcedurePoint(leg, true /* preview */));
      }
    }
  }
}

int MapScreenIndex::getNearestTrafficPatternIndex(int xs, int ys, int maxDistance) const
{
  if(NavApp::getMapMarkHandler()->getMarkTypes() & map::MARK_PATTERNS)
    return getNearestIndex(xs, ys, maxDistance, trafficPatterns);
  else
    return -1;
}

int MapScreenIndex::getNearestHoldIndex(int xs, int ys, int maxDistance) const
{
  if(NavApp::getMapMarkHandler()->getMarkTypes() & map::MARK_HOLDS)
    return getNearestIndex(xs, ys, maxDistance, holds);
  else
    return -1;
}

int MapScreenIndex::getNearestRangeMarkIndex(int xs, int ys, int maxDistance) const
{
  if(NavApp::getMapMarkHandler()->getMarkTypes() & map::MARK_RANGE_RINGS)
    return getNearestIndex(xs, ys, maxDistance, rangeMarks);
  else
    return -1;
}

int MapScreenIndex::getNearestDistanceMarkIndex(int xs, int ys, int maxDistance) const
{
  if(NavApp::getMapMarkHandler()->getMarkTypes() & map::MARK_MEASUREMENT)
    return getNearestIndex(xs, ys, maxDistance, distanceMarks);
  else
    return -1;
}

template<typename TYPE>
int MapScreenIndex::getNearestIndex(int xs, int ys, int maxDistance, const QList<TYPE>& typeList) const
{
  CoordinateConverter conv(mapPaintWidget->viewport());
  int index = 0;
  int x, y;
  for(const TYPE& type : typeList)
  {
    if(conv.wToS(type.getPosition(), x, y) && atools::geo::manhattanDistance(x, y, xs, ys) < maxDistance)
      return index;

    index++;
  }
  return -1;
}

int MapScreenIndex::getNearestRoutePointIndex(int xs, int ys, int maxDistance) const
{
  if(!paintLayer->getShownMapObjectDisplayTypes().testFlag(map::FLIGHTPLAN))
    return -1;

  int minIndex = -1;
  float minDist = map::INVALID_DISTANCE_VALUE;

  for(const std::pair<int, QPoint>& rsp : routePoints)
  {
    const QPoint& point = rsp.second;
    float dist = atools::geo::manhattanDistance(point.x(), point.y(), xs, ys);
    if(dist < minDist && dist < maxDistance)
    {
      minDist = dist;
      minIndex = rsp.first;
    }
  }

  return minIndex;
}

void MapScreenIndex::updateAllGeometry(const Marble::GeoDataLatLonBox& curBox)
{
  updateRouteScreenGeometry(curBox);
  updateAirwayScreenGeometry(curBox);
  updateLogEntryScreenGeometry(curBox);
  updateAirspaceScreenGeometry(curBox);
  updateIlsScreenGeometry(curBox);
}

/* Get all airways near cursor position */
void MapScreenIndex::getNearestAirspaces(int xs, int ys, map::MapSearchResult& result) const
{
  for(int i = 0; i < airspacePolygons.size(); i++)
  {
    const std::pair<map::MapAirspaceId, QPolygon>& polyPair = airspacePolygons.at(i);

    if(polyPair.second.containsPoint(QPoint(xs, ys), Qt::OddEvenFill))
    {
      map::MapAirspace airspace;
      NavApp::getAirspaceController()->getAirspaceById(airspace, polyPair.first);
      result.airspaces.append(airspace);
    }
  }
}

QSet<int> MapScreenIndex::nearestLineIds(const QList<std::pair<int, QLine> >& lineList, int xs, int ys,
                                         int maxDistance, bool lineDistanceOnly) const
{
  QSet<int> ids;
  for(int i = 0; i < lineList.size(); i++)
  {
    const std::pair<int, QLine>& linePair = lineList.at(i);
    const QLine& line = linePair.second;

    if(atools::geo::distanceToLine(xs, ys, line.x1(), line.y1(), line.x2(), line.y2(), lineDistanceOnly) < maxDistance)
      ids.insert(linePair.first);
  }
  return ids;
}

/* Get all airways near cursor position */
void MapScreenIndex::getNearestLogEntries(int xs, int ys, int maxDistance, map::MapSearchResult& result) const
{
  CoordinateConverter conv(mapPaintWidget->viewport());
  QSet<int> ids; // Deduplicate

  // Look for logbook entry endpoints (departure and destination)
  for(int i = searchHighlights.logbookEntries.size() - 1; i >= 0; i--)
  {
    const map::MapLogbookEntry& l = searchHighlights.logbookEntries.at(i);
    int x, y;
    if(conv.wToS(l.departurePos, x, y) || conv.wToS(l.destinationPos, x, y))
      if((atools::geo::manhattanDistance(x, y, xs, ys)) < maxDistance)
        maptools::insertSortedByDistance(conv, result.logbookEntries, &ids, xs, ys, l);
  }

  // Look for route and direct line geometry
  if(paintLayer->getShownMapObjectDisplayTypes().testFlag(map::LOGBOOK_DIRECT) ||
     paintLayer->getShownMapObjectDisplayTypes().testFlag(map::LOGBOOK_ROUTE))
  {
    for(int id : nearestLineIds(logEntryLines, xs, ys, maxDistance, false /* also distance to points */))
      maptools::insertSortedByDistance(conv, result.logbookEntries, &ids, xs, ys,
                                       NavApp::getLogdataController()->getLogEntryById(id));
  }
}

void MapScreenIndex::getNearestIls(int xs, int ys, int maxDistance, map::MapSearchResult& result) const
{
  if(!paintLayer->getShownMapObjects().testFlag(map::ILS))
    return;

  // Get nearest center lines (also considering buffer)
  QSet<int> ilsIds = nearestLineIds(ilsLines, xs, ys, maxDistance, false /* lineDistanceOnly */);

  // Get nearest ILS by geometry - duplicates are removed in set
  for(int i = 0; i < ilsPolygons.size(); i++)
  {
    const std::pair<int, QPolygon>& polyPair = ilsPolygons.at(i);
    if(polyPair.second.containsPoint(QPoint(xs, ys), Qt::OddEvenFill))
      ilsIds.insert(polyPair.first);
  }

  // Get ILS map objects for ids
  for(int id : ilsIds)
  {
    map::MapIls ils = mapQuery->getIlsById(id);
    result.ils.append(ils);
  }
}

/* Get all airways near cursor position */
void MapScreenIndex::getNearestAirways(int xs, int ys, int maxDistance, map::MapSearchResult& result) const
{
  for(int id : nearestLineIds(airwayLines, xs, ys, maxDistance, true /* lineDistanceOnly */))
    result.airways.append(airwayQuery->getAirwayById(id));
}

int MapScreenIndex::getNearestRouteLegIndex(int xs, int ys, int maxDistance) const
{
  if(!paintLayer->getShownMapObjectDisplayTypes().testFlag(map::FLIGHTPLAN))
    return -1;

  int minIndex = -1;
  float minDist = std::numeric_limits<float>::max();

  for(int i = 0; i < routeLines.size(); i++)
  {
    const std::pair<int, QLine>& line = routeLines.at(i);

    QLine l = line.second;

    float dist = atools::geo::distanceToLine(xs, ys, l.x1(), l.y1(), l.x2(), l.y2(), true /* no dist to points */);

    if(dist < minDist && dist < maxDistance)
    {
      minDist = dist;
      minIndex = line.first;
    }
  }
  return minIndex;
}
