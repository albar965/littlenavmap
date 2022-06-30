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
#include "route/routecontroller.h"
#include "mapgui/maplayer.h"
#include "online/onlinedatacontroller.h"
#include "logbook/logdatacontroller.h"
#include "mapgui/mapscale.h"
#include "mapgui/mapfunctions.h"
#include "mapgui/mapwidget.h"
#include "mappainter/mappaintlayer.h"
#include "mapgui/mapmarkhandler.h"
#include "common/maptools.h"
#include "query/mapquery.h"
#include "query/airwaytrackquery.h"
#include "query/airportquery.h"
#include "common/constants.h"
#include "settings/settings.h"
#include "airspace/airspacecontroller.h"

#include <marble/GeoDataLineString.h>

using atools::geo::Pos;
using atools::geo::Line;
using atools::geo::LineString;
using atools::geo::Rect;
using map::MapAirway;
using Marble::GeoDataLineString;
using Marble::GeoDataCoordinates;

template<typename TYPE>
void assignIdAndInsert(const QString& settingsName, QHash<int, TYPE>& hash)
{
  atools::settings::Settings& s = atools::settings::Settings::instance();

  for(auto obj :s.valueVar(settingsName).value<QList<TYPE> >())
  {
    obj.id = map::getNextUserFeatureId();
    hash.insert(obj.id, obj);
  }
}

// ==============================================================================

MapScreenIndex::MapScreenIndex(MapPaintWidget *mapPaintWidgetParam, MapPaintLayer *mapPaintLayer)
  : mapWidget(mapPaintWidgetParam), paintLayer(mapPaintLayer)
{
  airportQuery = NavApp::getAirportQuerySim();

  searchHighlights = new map::MapResult;
  procedureHighlight = new proc::MapProcedureLegs;
  procedureLegHighlight = new proc::MapProcedureLeg;
}

MapScreenIndex::~MapScreenIndex()
{
  delete searchHighlights;
  delete procedureLegHighlight;
  delete procedureHighlight;
}

void MapScreenIndex::copy(const MapScreenIndex& other)
{
  simData = other.simData;
  lastSimData = other.lastSimData;
  *searchHighlights = *other.searchHighlights;
  *procedureLegHighlight = *other.procedureLegHighlight;
  *procedureHighlight = *other.procedureHighlight;
  procedureHighlights = other.procedureHighlights;
  airspaceHighlights = other.airspaceHighlights;
  airwayHighlights = other.airwayHighlights;
  profileHighlight = other.profileHighlight;
  routeHighlights = other.routeHighlights;
  rangeMarks = other.rangeMarks;
  distanceMarks = other.distanceMarks;
  patternMarks = other.patternMarks;
  holdingMarks = other.holdingMarks;
  msaMarks = other.msaMarks;
  routeLines = other.routeLines;
  airwayLines = other.airwayLines;
  logEntryLines = other.logEntryLines;
  airspacePolygons = other.airspacePolygons;
  ilsPolygons = other.ilsPolygons;
  ilsLines = other.ilsLines;
  routePointsEditable = other.routePointsEditable;
  routePointsAll = other.routePointsAll;
}

void MapScreenIndex::updateAirspaceScreenGeometryInternal(QSet<map::MapAirspaceId>& ids, map::MapAirspaceSources source,
                                                          const Marble::GeoDataLatLonBox& curBox, bool highlights)
{
  const MapScale *scale = paintLayer->getMapScale();
  AirspaceController *controller = NavApp::getAirspaceController();

  if(paintLayer->getMapLayer() == nullptr)
    return;

  if(scale->isValid() && controller != nullptr && paintLayer != nullptr)
  {
    AirspaceVector airspaces;

    // Get displayed airspaces ================================
    bool overflow = false;
    if(!highlights && paintLayer->getShownMapObjects().testFlag(map::AIRSPACE))
      controller->getAirspaces(airspaces, curBox, paintLayer->getMapLayer(), mapWidget->getShownAirspaceTypesByLayer(),
                               NavApp::getRouteConst().getCruisingAltitudeFeet(), false, source, overflow);

    // Get highlighted airspaces from info window ================================
    for(const map::MapAirspace& airspace : airspaceHighlights)
    {
      if(airspace.hasValidGeometry())
        airspaces.append(&airspace);
    }

    // Get highlighted airspaces from online center search ================================
    for(const map::MapAirspace& airspace : searchHighlights->airspaces)
    {
      if(airspace.hasValidGeometry())
        airspaces.append(&airspace);
    }

    CoordinateConverter conv(mapWidget->viewport());
    for(const map::MapAirspace *airspace : airspaces)
    {
      if(!(airspace->type & mapWidget->getShownAirspaceTypesByLayer().types) && !highlights)
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
            airspacePolygons.append(std::make_pair(airspace->combinedId(), poly.intersected(QPolygon(mapWidget->rect())).toPolygon()));
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
  updateAirspaceScreenGeometryInternal(ids, NavApp::getAirspaceController()->getAirspaceSources(), curBox, true /* highlights */);

  if(!paintLayer->getMapLayer()->isAirspace() || !paintLayer->getShownMapObjects().testFlag(map::AIRSPACE))
    return;

  // Do not put into index if nothing is drawn
  if(mapWidget->distance() >= layer::DISTANCE_CUT_OFF_LIMIT_KM)
    return;

  // Get geometry from visible airspaces
  updateAirspaceScreenGeometryInternal(ids, NavApp::getAirspaceController()->getAirspaceSources(), curBox, false /* highlights */);
}

void MapScreenIndex::updateIlsScreenGeometry(const Marble::GeoDataLatLonBox& curBox)
{
  resetIlsScreenGeometry();

  if(paintLayer == nullptr || paintLayer->getMapLayer() == nullptr)
    return;

  map::MapTypes types = paintLayer->getShownMapObjects();
  map::MapObjectDisplayTypes displayTypes = paintLayer->getShownMapObjectDisplayTypes();

  if(!paintLayer->getMapLayer()->isIls())
    // No ILS at this zoom distance
    return;

  // Do not put into index if nothing is drawn
  if(mapWidget->distance() >= layer::DISTANCE_CUT_OFF_LIMIT_KM)
    return;

  const MapScale *scale = paintLayer->getMapScale();
  if(!scale->isValid())
    return;

  QVector<map::MapIls> ilsVector;
  QSet<int> routeIlsIds;
  if(paintLayer->getShownMapObjectDisplayTypes().testFlag(map::FLIGHTPLAN))
  {
    // Get ILS from flight plan which are also painted in the profile - only if plan is shown
    ilsVector = NavApp::getRouteConst().getDestRunwayIlsMap();
    for(const map::MapIls& ils : ilsVector)
      routeIlsIds.insert(ils.id);
  }

  if(types.testFlag(map::ILS) || displayTypes.testFlag(map::GLS))
  {
    // ILS enabled - add from map cache
    bool overflow = false;
    const QList<map::MapIls> *ilsListPtr =
      mapWidget->getMapQuery()->getIls(curBox, paintLayer->getMapLayer(), false, overflow);
    if(ilsListPtr != nullptr)
    {
      for(const map::MapIls& ils : *ilsListPtr)
      {
        if(ils.isAnyGlsRnp() && !displayTypes.testFlag(map::GLS))
          continue;

        if(!ils.isAnyGlsRnp() && !types.testFlag(map::ILS))
          continue;

        if(!routeIlsIds.contains(ils.id))
          ilsVector.append(ils);
      }
    }
  }

  CoordinateConverter conv(mapWidget->viewport());
  for(const map::MapIls& ils : ilsVector)
  {
    if(!ils.hasGeometry)
      continue;

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
      polygon = polygon.intersected(QPolygon(mapWidget->rect()));
      if(!polygon.isEmpty())
        ilsPolygons.append(std::make_pair(ils.id, polygon));
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
      CoordinateConverter conv(mapWidget->viewport());
      for(map::MapLogbookEntry& entry : searchHighlights->logbookEntries)
      {
        if(entry.isValid())
        {
          if(types.testFlag(map::LOGBOOK_DIRECT))
            updateLineScreenGeometry(logEntryLines, entry.id, entry.line(), curBox, conv);

          if(types.testFlag(map::LOGBOOK_ROUTE) && searchHighlights->logbookEntries.size() == 1)
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

  // Get geometry from visible airways
  updateAirwayScreenGeometryInternal(ids, curBox, false /* highlight */);
}

void MapScreenIndex::updateAirwayScreenGeometryInternal(QSet<int>& ids, const Marble::GeoDataLatLonBox& curBox, bool highlight)
{
  const MapScale *scale = paintLayer->getMapScale();
  AirwayTrackQuery *airwayTrackQuery = mapWidget->getAirwayTrackQuery();

  if(scale->isValid())
  {
    CoordinateConverter conv(mapWidget->viewport());
    if(!highlight)
    {
      // Get geometry from visible airspaces
      bool showJet = paintLayer->getShownMapObjects().testFlag(map::AIRWAYJ);
      bool showVictor = paintLayer->getShownMapObjects().testFlag(map::AIRWAYV);
      if(paintLayer->getMapLayer()->isAirway() && (showJet || showVictor))
      {
        // Airways are visible on map - get them from the cache/database
        QList<MapAirway> airways;
        airwayTrackQuery->getAirways(airways, curBox, paintLayer->getMapLayer(), false);

        for(int i = 0; i < airways.size(); i++)
        {
          const MapAirway& airway = airways.at(i);
          if(ids.contains(airway.id))
            continue;

          if((airway.type == map::AIRWAY_VICTOR && !showVictor) || (airway.type == map::AIRWAY_JET && !showJet))
            // Not visible by map setting
            continue;

          updateLineScreenGeometry(airwayLines, airway.id, Line(airway.from, airway.to), curBox, conv);
          ids.insert(airway.id);
        }
      }

      bool showTrack = paintLayer->getShownMapObjects().testFlag(map::TRACK);
      if(paintLayer->getMapLayer()->isTrack() && showTrack)
      {
        // Tracks are visible on map - get them from the cache/database
        QList<MapAirway> tracks;
        airwayTrackQuery->getTracks(tracks, curBox, paintLayer->getMapLayer(), false);

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
  QRect mapGeo = mapWidget->rect();

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
            QLine l(p.at(k).toPoint(), p.at(k + 1).toPoint());
            QRect rect(l.p1(), l.p2());
            rect = rect.normalized();
            // Avoid points or flat rectangles (lines)
            rect.adjust(-1, -1, 1, 1);

            if(mapGeo.intersects(rect))
              index.append(std::make_pair(id, l));
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
  s.setValueVar(lnm::MAP_DISTANCEMARKERS, QVariant::fromValue<QList<map::DistanceMarker> >(distanceMarks.values()));
  s.setValueVar(lnm::MAP_RANGEMARKERS, QVariant::fromValue<QList<map::RangeMarker> >(rangeMarks.values()));
  s.setValueVar(lnm::MAP_TRAFFICPATTERNS, QVariant::fromValue<QList<map::PatternMarker> >(patternMarks.values()));
  s.setValueVar(lnm::MAP_HOLDINGS, QVariant::fromValue<QList<map::HoldingMarker> >(holdingMarks.values()));
  s.setValueVar(lnm::MAP_AIRPORT_MSA, QVariant::fromValue<QList<map::MsaMarker> >(msaMarks.values()));
}

void MapScreenIndex::restoreState()
{
  assignIdAndInsert<map::DistanceMarker>(lnm::MAP_DISTANCEMARKERS, distanceMarks);
  assignIdAndInsert<map::RangeMarker>(lnm::MAP_RANGEMARKERS, rangeMarks);
  assignIdAndInsert<map::PatternMarker>(lnm::MAP_TRAFFICPATTERNS, patternMarks);
  assignIdAndInsert<map::HoldingMarker>(lnm::MAP_HOLDINGS, holdingMarks);
  assignIdAndInsert<map::MsaMarker>(lnm::MAP_AIRPORT_MSA, msaMarks);
}

void MapScreenIndex::changeSearchHighlights(const map::MapResult& newHighlights)
{
  *searchHighlights = newHighlights;
}

void MapScreenIndex::setProcedureHighlight(const proc::MapProcedureLegs& newHighlight)
{
  *procedureHighlight = newHighlight;
}

void MapScreenIndex::setProcedureLegHighlight(const proc::MapProcedureLeg& newLegHighlight)
{
  *procedureLegHighlight = newLegHighlight;
}

void MapScreenIndex::addRangeMark(const map::RangeMarker& obj)
{
#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << obj.id;
#endif
  rangeMarks.insert(obj.id, obj);
}

void MapScreenIndex::addPatternMark(const map::PatternMarker& obj)
{
#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << obj.id;
#endif
  patternMarks.insert(obj.id, obj);
}

void MapScreenIndex::addDistanceMark(const map::DistanceMarker& obj)
{
#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << obj.id;
#endif
  distanceMarks.insert(obj.id, obj);
}

void MapScreenIndex::addHoldingMark(const map::HoldingMarker& obj)
{
#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << obj.id;
#endif
  holdingMarks.insert(obj.id, obj);
}

void MapScreenIndex::addMsaMark(const map::MsaMarker& obj)
{
#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << obj.id;
#endif
  msaMarks.insert(obj.id, obj);
}

void MapScreenIndex::removeRangeMark(int id)
{
#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << id;
#endif
  rangeMarks.remove(id);
}

void MapScreenIndex::removePatternMark(int id)
{
#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << id;
#endif
  patternMarks.remove(id);
}

void MapScreenIndex::removeDistanceMark(int id)
{
#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << id;
#endif
  distanceMarks.remove(id);
}

void MapScreenIndex::removeHoldingMark(int id)
{
#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << id;
#endif
  holdingMarks.remove(id);
}

void MapScreenIndex::removeMsaMark(int id)
{
#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << id;
#endif
  msaMarks.remove(id);
}

void MapScreenIndex::clearAllMarkers(map::MapTypes types)
{
  if(types.testFlag(map::MARK_RANGE))
    rangeMarks.clear();

  if(types.testFlag(map::MARK_DISTANCE))
    distanceMarks.clear();

  if(types.testFlag(map::MARK_PATTERNS))
    patternMarks.clear();

  if(types.testFlag(map::MARK_HOLDING))
    holdingMarks.clear();

  if(types.testFlag(map::MARK_MSA))
    msaMarks.clear();
}

void MapScreenIndex::updateDistanceMarkerTo(int id, const atools::geo::Pos& pos)
{
  distanceMarks[id].to = distanceMarks[id].position = pos;
}

void MapScreenIndex::updateDistanceMarker(int id, const map::DistanceMarker& marker)
{
  distanceMarks[id] = marker;
  distanceMarks[id].id = id;
}

void MapScreenIndex::updateRouteScreenGeometry(const Marble::GeoDataLatLonBox& curBox)
{
  const Route& route = NavApp::getRouteConst();

  map::MapTypes shown = paintLayer->getShownMapObjects();
  bool missed = shown.testFlag(map::MISSED_APPROACH);

  routeLines.clear();
  routePointsEditable.clear();
  routePointsAll.clear();

  QList<std::pair<int, QPoint> > airportPoints;
  QList<std::pair<int, QPoint> > otherPointsEditable;
  QList<std::pair<int, QPoint> > otherPointsNotEditable;

  CoordinateConverter conv(mapWidget->viewport());
  const MapScale *scale = paintLayer->getMapScale();
  if(scale->isValid())
  {
    Pos p1;
    int departIndex = route.getDepartureAirportLegIndex();
    int destIndex = route.getDestinationAirportLegIndex();
    for(int i = 0; i < route.size(); i++)
    {
      const RouteLeg& routeLeg = route.value(i);

      // Do not add the missed legs if they are not shown
      if(routeLeg.getProcedureLeg().isMissed() && !missed)
        continue;

      const Pos& p2 = routeLeg.getPosition();
      int x2, y2;
      if(conv.wToS(p2, x2, y2))
      {
        map::MapTypes type = routeLeg.getMapObjectType();
        if(type == map::AIRPORT && (i == departIndex || i == destIndex || routeLeg.isAlternate()))
          // Departure, destination or alternate airport - put to from of list for higher priority
          airportPoints.append(std::make_pair(i, QPoint(x2, y2)));
        else if(route.canEditPoint(i))
          otherPointsEditable.append(std::make_pair(i, QPoint(x2, y2)));
        else
          otherPointsNotEditable.append(std::make_pair(i, QPoint(x2, y2)));
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
    routePointsEditable.append(airportPoints);
    routePointsEditable.append(otherPointsEditable);

    routePointsAll.append(airportPoints);
    routePointsAll.append(otherPointsEditable);
    routePointsAll.append(otherPointsNotEditable);
  }
}

void MapScreenIndex::getAllNearest(int xs, int ys, int maxDistance, map::MapResult& result, map::MapObjectQueryTypes types) const
{
  using maptools::insertSortedByDistance;
  const MapLayer *mapLayer = paintLayer->getMapLayer();

  if(mapLayer == nullptr)
    return;

  CoordinateConverter conv(mapWidget->viewport());

  map::MapTypes shown = paintLayer->getShownMapObjects();
  map::MapObjectDisplayTypes shownDisplay = paintLayer->getShownMapObjectDisplayTypes();

  // Check for user aircraft ======================================================
  result.userAircraft.clear();
  if(shown & map::AIRCRAFT && NavApp::isConnectedAndAircraft())
  {
    const atools::fs::sc::SimConnectUserAircraft& user = simData.getUserAircraftConst();
    int x, y;
    if(conv.wToS(user.getPosition(), x, y))
    {
      if(atools::geo::manhattanDistance(x, y, xs, ys) < maxDistance)
        result.userAircraft = map::MapUserAircraft(user);
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
        if(obj.isValid() && obj.isAnyBoat() && (obj.getModelRadiusCorrected() * 2 > layer::LARGE_SHIP_SIZE || mapLayer->isAiShipSmall()))
        {
          if(conv.wToS(obj.getPosition(), x, y))
          {
            if((atools::geo::manhattanDistance(x, y, xs, ys)) < maxDistance)
              insertSortedByDistance(conv, result.aiAircraft, nullptr, xs, ys, map::MapAiAircraft(obj));
          }
        }
      }
    }
  }

  bool onlineEnabled = shown.testFlag(map::AIRCRAFT_ONLINE) && NavApp::isOnlineNetworkActive();
  bool aiEnabled = shown.testFlag(map::AIRCRAFT_AI) && NavApp::isConnected();

  // Add AI or injected multiplayer aircraft ======================================
  for(const atools::fs::sc::SimConnectAircraft& ac : mapWidget->getAiAircraft())
  {
    // Skip boats
    if(ac.isAnyBoat())
      continue;

    // Skip shadow aircraft if online is disabled
    if(!onlineEnabled && ac.isOnlineShadow())
      continue;

    // Skip AI aircraft (means not shadow) if AI is disabled
    if(!aiEnabled && !ac.isOnlineShadow())
      continue;

    if(ac.isValid() && !ac.isAnyBoat() && mapfunc::aircraftVisible(ac, mapLayer))
    {
      if(conv.wToS(ac.getPosition(), x, y))
      {
        if((atools::geo::manhattanDistance(x, y, xs, ys)) < maxDistance)
        {
          // Add online network shadow aircraft from simulator to online list
          atools::fs::sc::SimConnectAircraft shadow = NavApp::getOnlinedataController()->getShadowedOnlineAircraft(ac);
          if(shadow.isValid())
            insertSortedByDistance(conv, result.onlineAircraft, &result.onlineAircraftIds, xs, ys, map::MapOnlineAircraft(shadow));

          insertSortedByDistance(conv, result.aiAircraft, nullptr, xs, ys, map::MapAiAircraft(ac));
        }
      }
    }
  }

  if(onlineEnabled)
  {
    // Add online clients ======================================
    for(const atools::fs::sc::SimConnectAircraft& obj : *NavApp::getOnlinedataController()->getAircraftFromCache())
    {
      if(mapfunc::aircraftVisible(obj, mapLayer))
      {
        if(obj.isValid() && conv.wToS(obj.getPosition(), x, y))
          if((atools::geo::manhattanDistance(x, y, xs, ys)) < maxDistance)
            insertSortedByDistance(conv, result.onlineAircraft, &result.onlineAircraftIds, xs, ys, map::MapOnlineAircraft(obj));
      }
    }
  }

  // Features with geometry using a screen coordinate buffer
  getNearestLogEntries(xs, ys, maxDistance, result);
  getNearestAirways(xs, ys, maxDistance, result);
  getNearestAirspaces(xs, ys, result);
  getNearestIls(xs, ys, maxDistance, result);

  // Flight plan objects =============================================================
  if(shownDisplay.testFlag(map::FLIGHTPLAN))
  {
    map::MapObjectQueryTypes queryTypes = types | map::QUERY_PROCEDURES | map::QUERY_PROC_POINTS;

    if(shown.testFlag(map::MISSED_APPROACH))
      queryTypes |= map::QUERY_PROC_MISSED_POINTS | map::QUERY_PROCEDURES_MISSED;

    // Get copies from flight plan if visible
    NavApp::getRouteConst().getNearest(conv, xs, ys, maxDistance, result, queryTypes, routeDrawnNavaids);

    if(types.testFlag(map::QUERY_PROC_RECOMMENDED))
      NavApp::getRouteConst().getNearestRecommended(conv, xs, ys, maxDistance, result, queryTypes, routeDrawnNavaids);
  }

  // Get points of procedure preview
  if(types.testFlag(map::QUERY_PREVIEW_PROC_POINTS))
    getNearestProcedureHighlights(xs, ys, maxDistance, result, types);

  // Get copies from highlightMapObjects and marks (user features)
  getNearestHighlights(xs, ys, maxDistance, result, types);

  // Get objects from cache - already present objects will be skipped
  // Airway included to fetch waypoints
  map::MapTypes mapTypes = shown &
                           (map::AIRPORT_ALL_AND_ADDON | map::AIRPORT_MSA | map::VOR | map::NDB | map::WAYPOINT | map::MARKER |
                            map::HOLDING | map::AIRWAYJ | map::TRACK | map::AIRWAYV | map::USERPOINT | map::LOGBOOK);

  mapWidget->getMapQuery()->getNearestScreenObjects(conv, mapLayer, mapLayer->isAirportDiagram() &&
                                                    OptionData::instance().getDisplayOptionsAirport().
                                                    testFlag(optsd::ITEM_AIRPORT_DETAIL_PARKING),
                                                    mapTypes, xs, ys, maxDistance, result);

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

void MapScreenIndex::getNearestHighlights(int xs, int ys, int maxDistance, map::MapResult& result, map::MapObjectQueryTypes types) const
{
  using maptools::insertSorted;
  CoordinateConverter conv(mapWidget->viewport());

  insertSorted(conv, xs, ys, searchHighlights->airports, result.airports, &result.airportIds, maxDistance);
  insertSorted(conv, xs, ys, searchHighlights->vors, result.vors, &result.vorIds, maxDistance);
  insertSorted(conv, xs, ys, searchHighlights->ndbs, result.ndbs, &result.ndbIds, maxDistance);
  insertSorted(conv, xs, ys, searchHighlights->holdings, result.holdings, &result.holdingIds, maxDistance);
  insertSorted(conv, xs, ys, searchHighlights->waypoints, result.waypoints, &result.waypointIds, maxDistance);
  insertSorted(conv, xs, ys, searchHighlights->userpoints, result.userpoints, &result.userpointIds, maxDistance);
  insertSorted(conv, xs, ys, searchHighlights->airspaces, result.airspaces, nullptr, maxDistance);
  insertSorted(conv, xs, ys, searchHighlights->airways, result.airways, nullptr, maxDistance);
  insertSorted(conv, xs, ys, searchHighlights->ils, result.ils, nullptr, maxDistance);
  insertSorted(conv, xs, ys, searchHighlights->aiAircraft, result.aiAircraft, nullptr, maxDistance);
  insertSorted(conv, xs, ys, searchHighlights->onlineAircraft, result.onlineAircraft, &result.onlineAircraftIds, maxDistance);
  insertSorted(conv, xs, ys, searchHighlights->runwayEnds, result.runwayEnds, nullptr, maxDistance);

  // Add only if requested and visible on map
  if(types & map::QUERY_MARK_HOLDINGS && NavApp::getMapMarkHandler()->getMarkTypes().testFlag(map::MARK_HOLDING))
    insertSorted(conv, xs, ys, holdingMarks.values(), result.holdingMarks, nullptr, maxDistance);

  if(types & map::QUERY_MARK_MSA && NavApp::getMapMarkHandler()->getMarkTypes().testFlag(map::MARK_MSA))
    insertSorted(conv, xs, ys, msaMarks.values(), result.msaMarks, &result.airportMsaIds, maxDistance);

  if(types & map::QUERY_MARK_PATTERNS && NavApp::getMapMarkHandler()->getMarkTypes().testFlag(map::MARK_PATTERNS))
    insertSorted(conv, xs, ys, patternMarks.values(), result.patternMarks, nullptr, maxDistance);

  if(types & map::QUERY_MARK_RANGE && NavApp::getMapMarkHandler()->getMarkTypes().testFlag(map::MARK_RANGE))
    insertSorted(conv, xs, ys, rangeMarks.values(), result.rangeMarks, nullptr, maxDistance);

  if(types & map::QUERY_MARK_DISTANCE && NavApp::getMapMarkHandler()->getMarkTypes().testFlag(map::MARK_DISTANCE))
    insertSorted(conv, xs, ys, distanceMarks.values(), result.distanceMarks, nullptr, maxDistance);
}

void MapScreenIndex::getNearestProcedureHighlights(int xs, int ys, int maxDistance, map::MapResult& result,
                                                   map::MapObjectQueryTypes types) const
{
  nearestProcedureHighlightsInternal(xs, ys, maxDistance, result, types, procedureHighlights, true /* previewAll  */);
  nearestProcedureHighlightsInternal(xs, ys, maxDistance, result, types, {*procedureHighlight}, false /* previewAll  */);
}

void MapScreenIndex::nearestProcedureHighlightsInternal(int xs, int ys, int maxDistance, map::MapResult& result,
                                                        map::MapObjectQueryTypes types,
                                                        const QVector<proc::MapProcedureLegs>& procedureLegs, bool previewAll) const
{
  CoordinateConverter conv(mapWidget->viewport());
  int x, y;

  using maptools::insertSorted;

  QSet<proc::MapProcedureRef> ids;

  for(const proc::MapProcedureLegs& legs : procedureLegs)
  {
    for(int i = 0; i < legs.size(); i++)
    {
      const proc::MapProcedureLeg& leg = legs.at(i);

      if(previewAll && leg.isMissed())
        // Multi preview does not include missed
        continue;

      proc::MapProcedureRef ref(0 /* airportId */, 0 /* runwayEndId */, leg.approachId, leg.transitionId, leg.legId, leg.mapType);
      if(ids.contains(ref))
        continue;
      else
        ids.insert(ref);

      // Add navaids from procedure
      insertSorted(conv, xs, ys, leg.navaids.airports, result.airports, &result.airportIds, maxDistance);
      insertSorted(conv, xs, ys, leg.navaids.vors, result.vors, &result.vorIds, maxDistance);
      insertSorted(conv, xs, ys, leg.navaids.ndbs, result.ndbs, &result.ndbIds, maxDistance);
      insertSorted(conv, xs, ys, leg.navaids.waypoints, result.waypoints, &result.waypointIds, maxDistance);
      insertSorted(conv, xs, ys, leg.navaids.userpoints, result.userpoints, &result.userpointIds, maxDistance);
      insertSorted(conv, xs, ys, leg.navaids.airspaces, result.airspaces, nullptr, maxDistance);
      insertSorted(conv, xs, ys, leg.navaids.airways, result.airways, nullptr, maxDistance);
      insertSorted(conv, xs, ys, leg.navaids.ils, result.ils, nullptr, maxDistance);
      insertSorted(conv, xs, ys, leg.navaids.runwayEnds, result.runwayEnds, nullptr, maxDistance);

      if(types.testFlag(map::QUERY_PROC_POINTS) || types.testFlag(map::QUERY_PREVIEW_PROC_POINTS))
      {
        // No need to filter missed since this is always shown on highlight
        if(conv.wToS(leg.line.getPos2(), x, y))
        {
          if((atools::geo::manhattanDistance(x, y, xs, ys)) < maxDistance)
            result.procPoints.append(map::MapProcedurePoint(legs, i, -1 /* routeIndex */, true /* preview */, previewAll));
        }
      }
    }
  }
}

int MapScreenIndex::getNearestTrafficPatternId(int xs, int ys, int maxDistance) const
{
  if(NavApp::getMapMarkHandler()->getMarkTypes() & map::MARK_PATTERNS)
    return getNearestId(xs, ys, maxDistance, patternMarks);
  else
    return -1;
}

int MapScreenIndex::getNearestHoldId(int xs, int ys, int maxDistance) const
{
  if(NavApp::getMapMarkHandler()->getMarkTypes() & map::MARK_HOLDING)
    return getNearestId(xs, ys, maxDistance, holdingMarks);
  else
    return -1;
}

int MapScreenIndex::getNearestAirportMsaId(int xs, int ys, int maxDistance) const
{
  if(NavApp::getMapMarkHandler()->getMarkTypes() & map::MARK_MSA)
    return getNearestId(xs, ys, maxDistance, msaMarks);
  else
    return -1;
}

int MapScreenIndex::getNearestRangeMarkId(int xs, int ys, int maxDistance) const
{
  if(NavApp::getMapMarkHandler()->getMarkTypes() & map::MARK_RANGE)
    return getNearestId(xs, ys, maxDistance, rangeMarks);
  else
    return -1;
}

int MapScreenIndex::getNearestDistanceMarkId(int xs, int ys, int maxDistance) const
{
  if(NavApp::getMapMarkHandler()->getMarkTypes() & map::MARK_DISTANCE)
    return getNearestId(xs, ys, maxDistance, distanceMarks);
  else
    return -1;
}

template<typename TYPE>
int MapScreenIndex::getNearestId(int xs, int ys, int maxDistance, const QHash<int, TYPE>& typeList) const
{
  CoordinateConverter conv(mapWidget->viewport());
  int x, y;
  for(const TYPE& type : typeList)
  {
    if(conv.wToS(type.getPosition(), x, y) && atools::geo::manhattanDistance(x, y, xs, ys) < maxDistance)
      return type.id;
  }
  return -1;
}

int MapScreenIndex::getNearestRoutePointIndex(int xs, int ys, int maxDistance, bool editableOnly) const
{
  if(!paintLayer->getShownMapObjectDisplayTypes().testFlag(map::FLIGHTPLAN))
    return -1;

  int minIndex = -1;
  float minDist = map::INVALID_DISTANCE_VALUE;

  for(const std::pair<int, QPoint>& rsp : (editableOnly ? routePointsEditable : routePointsAll))
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
void MapScreenIndex::getNearestAirspaces(int xs, int ys, map::MapResult& result) const
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
void MapScreenIndex::getNearestLogEntries(int xs, int ys, int maxDistance, map::MapResult& result) const
{
  CoordinateConverter conv(mapWidget->viewport());
  QSet<int> ids; // Deduplicate

  // Look for logbook entry endpoints (departure and destination)
  for(int i = searchHighlights->logbookEntries.size() - 1; i >= 0; i--)
  {
    const map::MapLogbookEntry& l = searchHighlights->logbookEntries.at(i);
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

void MapScreenIndex::getNearestIls(int xs, int ys, int maxDistance, map::MapResult& result) const
{
  if(!paintLayer->getShownMapObjects().testFlag(map::ILS) &&
     !paintLayer->getShownMapObjectDisplayTypes().testFlag(map::FLIGHTPLAN))
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

  MapQuery *mapQuery = mapWidget->getMapQuery();
  // Get ILS map objects for ids
  for(int id : ilsIds)
  {
    map::MapIls ils = mapQuery->getIlsById(id);
    result.ils.append(ils);
  }
}

/* Get all airways near cursor position */
void MapScreenIndex::getNearestAirways(int xs, int ys, int maxDistance, map::MapResult& result) const
{
  AirwayTrackQuery *airwayTrackQuery = mapWidget->getAirwayTrackQuery();
  for(int id : nearestLineIds(airwayLines, xs, ys, maxDistance, true /* lineDistanceOnly */))
    result.airways.append(airwayTrackQuery->getAirwayById(id));
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
