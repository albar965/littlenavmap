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

#include "mapgui/mapscreenindex.h"

#include "airspace/airspacecontroller.h"
#include "common/aircrafttrail.h"
#include "common/constants.h"
#include "common/maptools.h"
#include "fs/gpx/gpxtypes.h"
#include "fs/sc/simconnectdata.h"
#include "logbook/logdatacontroller.h"
#include "mapgui/mapairporthandler.h"
#include "mapgui/mapfunctions.h"
#include "mapgui/maplayer.h"
#include "mapgui/mapmarkhandler.h"
#include "mapgui/mappaintwidget.h"
#include "mapgui/mapscale.h"
#include "mappainter/mappaintlayer.h"
#include "app/navapp.h"
#include "online/onlinedatacontroller.h"
#include "query/airportquery.h"
#include "query/airwaytrackquery.h"
#include "query/mapquery.h"
#include "route/route.h"
#include "settings/settings.h"
#include "util/average.h"

#include <marble/GeoDataLineString.h>

using atools::geo::Pos;
using atools::geo::Line;
using atools::geo::LineString;
using atools::geo::Rect;
using map::MapAirway;
using Marble::GeoDataLineString;
using Marble::GeoDataCoordinates;
using atools::fs::sc::SimConnectUserAircraft;
using atools::fs::sc::SimConnectData;

// Calculate averages for ground speed and turn speed for 4 seconds
const static qint64 TURN_PATH_AVERAGE_TIME_MS = 4000L;

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

  simData = new SimConnectData;
  lastSimData = new SimConnectData;
  lastUserAircraftForAverage = new SimConnectUserAircraft;
  searchHighlights = new map::MapResult;
  procedureHighlight = new proc::MapProcedureLegs;
  procedureLegHighlight = new proc::MapProcedureLeg;
  movingAverageSimAircraft = new atools::util::MovingAverageTime(TURN_PATH_AVERAGE_TIME_MS);
  profileHighlight = new atools::geo::Pos;
}

MapScreenIndex::~MapScreenIndex()
{
  delete searchHighlights;
  delete procedureLegHighlight;
  delete procedureHighlight;
  delete movingAverageSimAircraft;
  delete simData;
  delete lastSimData;
  delete lastUserAircraftForAverage;
  delete profileHighlight;
}

void MapScreenIndex::copy(const MapScreenIndex& other)
{
  // Copy content of pointer objects
  *simData = *other.simData;
  *lastSimData = *other.lastSimData;
  *searchHighlights = *other.searchHighlights;
  *procedureLegHighlight = *other.procedureLegHighlight;
  *procedureHighlight = *other.procedureHighlight;
  *lastUserAircraftForAverage = *other.lastUserAircraftForAverage;
  *profileHighlight = *other.profileHighlight;
  *movingAverageSimAircraft = *other.movingAverageSimAircraft;

  // Copy content of aggregated members
  procedureHighlights = other.procedureHighlights;
  airspaceHighlights = other.airspaceHighlights;
  airwayHighlights = other.airwayHighlights;
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
  lastUserAircraftForAverageTs = other.lastUserAircraftForAverageTs;
  routeDrawnNavaids = other.routeDrawnNavaids;
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
    map::MapAirspaceFilter filter = mapWidget->getShownAirspaceTypesByLayer();

    AirspaceVector airspaces;

    // Get displayed airspaces ================================
    bool overflow = false;
    if(!highlights && paintLayer->getShownMapTypes().testFlag(map::AIRSPACE))
      controller->getAirspaces(airspaces, curBox, paintLayer->getMapLayer(), filter,
                               NavApp::getRouteConst().getCruiseAltitudeFt(), false /* lazy */, source, overflow);

    // Get highlighted airspaces from info window ================================
    for(const map::MapAirspace& airspace : qAsConst(airspaceHighlights))
    {
      if(airspace.hasValidGeometry())
        airspaces.append(&airspace);
    }

    // Get highlighted airspaces from online center search ================================
    for(const map::MapAirspace& airspace : qAsConst(searchHighlights->airspaces))
    {
      if(airspace.hasValidGeometry())
        airspaces.append(&airspace);
    }

    CoordinateConverter conv(mapWidget->viewport());
    for(const map::MapAirspace *airspace : qAsConst(airspaces))
    {
      if(!(airspace->type & filter.types) && !highlights)
        continue;

      Marble::GeoDataLatLonBox airspacebox = conv.toGdc(airspace->bounding);

      // Check if airspace overlaps with current screen and is not already in list
      if(airspacebox.intersects(curBox) && !ids.contains(airspace->combinedId()))
      {
        const atools::geo::LineString *lines = controller->getAirspaceGeometry(airspace->combinedId());
        if(lines != nullptr)
        {
          const QVector<QPolygonF *> polys = conv.createPolygons(*lines, mapWidget->rect());
          for(const QPolygonF *poly : qAsConst(polys))
          {
            // Cut off all polygon parts that are not visible on screen
            airspacePolygons.append(std::make_pair(airspace->combinedId(), poly->intersected(QPolygon(mapWidget->rect())).toPolygon()));
            ids.insert(airspace->combinedId());
          }
          conv.releasePolygons(polys);
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

  if(!paintLayer->getMapLayer()->isAnyAirspace() || !paintLayer->getShownMapTypes().testFlag(map::AIRSPACE))
    return;

  // Do not put into index if nothing is drawn
  if(mapWidget->isDistanceCutOff())
    return;

  // Get geometry from visible airspaces
  updateAirspaceScreenGeometryInternal(ids, NavApp::getAirspaceController()->getAirspaceSources(), curBox, false /* highlights */);
}

void MapScreenIndex::updateIlsScreenGeometry(const Marble::GeoDataLatLonBox& curBox)
{
  resetIlsScreenGeometry();

  if(paintLayer == nullptr || paintLayer->getMapLayer() == nullptr)
    return;

  map::MapTypes types = paintLayer->getShownMapTypes();
  map::MapDisplayTypes displayTypes = paintLayer->getShownMapDisplayTypes();

  if(!paintLayer->getMapLayer()->isIls())
    // No ILS at this zoom distance
    return;

  // Do not put into index if nothing is drawn
  if(mapWidget->isDistanceCutOff())
    return;

  const MapScale *scale = paintLayer->getMapScale();
  if(!scale->isValid())
    return;

  QVector<map::MapIls> ilsVector;
  QSet<int> routeIlsIds;
  if(paintLayer->getShownMapDisplayTypes().testFlag(map::FLIGHTPLAN))
  {
    // Get ILS from flight plan which are also painted in the profile - only if plan is shown
    ilsVector = NavApp::getRouteConst().getDestRunwayIlsMap();
    for(const map::MapIls& ils : qAsConst(ilsVector))
      routeIlsIds.insert(ils.id);
  }

  // Not flight plan ILS are hidden with the airports
  if((types.testFlag(map::ILS) || displayTypes.testFlag(map::GLS)) && types.testFlag(map::AIRPORT))
  {
    // ILS enabled - add from map cache
    bool overflow = false;
    const QList<map::MapIls> *ilsListPtr = mapWidget->getMapQuery()->getIls(curBox, paintLayer->getMapLayer(), false /* lazy */, overflow);
    if(ilsListPtr != nullptr)
    {
      for(const map::MapIls& ils : *ilsListPtr)
      {
        if(!routeIlsIds.contains(ils.id))
        {
          if(ils.isAnyGlsRnp() && !displayTypes.testFlag(map::GLS))
            continue;

          if(!ils.isAnyGlsRnp() && !types.testFlag(map::ILS))
            continue;

          // Check if airport is to be shown - hide ILS if not - show ILS if no airport ident in navaid
          if(!ils.airportIdent.isEmpty())
          {
            map::MapAirport airport = airportQuery->getAirportByIdent(ils.airportIdent);
            if(airport.isValid() && !airport.isVisible(paintLayer->getShownMapTypes(), NavApp::getMapAirportHandler()->getMinimumRunwayFt(),
                                                       paintLayer->getMapLayer()))
              continue;
          }

          ilsVector.append(ils);
        }
      }
    }
  }

  CoordinateConverter conv(mapWidget->viewport());
  for(const map::MapIls& ils : qAsConst(ilsVector))
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
      const LineString boundary = ils.boundary();
      for(const Pos& pos : boundary)
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
    map::MapDisplayTypes types = paintLayer->getShownMapDisplayTypes();

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
            const atools::fs::gpx::GpxData *gpxData = NavApp::getLogdataController()->getGpxData(entry.id);
            if(gpxData != nullptr)
            {
              for(int i = 0; i < gpxData->flightplan.size() - 1; i++)
                updateLineScreenGeometry(logEntryLines, entry.id,
                                         Line(gpxData->flightplan.at(i).getPosition(), gpxData->flightplan.at(i + 1).getPosition()),
                                         curBox, conv);
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
      bool showJet = paintLayer->getShownMapTypes().testFlag(map::AIRWAYJ);
      bool showVictor = paintLayer->getShownMapTypes().testFlag(map::AIRWAYV);
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

      bool showTrack = paintLayer->getShownMapTypes().testFlag(map::TRACK);
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
      for(const QList<map::MapAirway>& airwayList : qAsConst(airwayHighlights))
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
  for(const Marble::GeoDataLatLonBox& curBoxCorrected : qAsConst(curBoxCorrectedList))
  {
    for(const Marble::GeoDataLineString *lineCorrected : qAsConst(geoLineStringVector))
    {
      if(lineCorrected->latLonAltBox().intersects(curBoxCorrected))
      {
        QVector<QPolygonF> polys = conv.wToS(*lineCorrected);
        for(const QPolygonF& p : qAsConst(polys))
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

void MapScreenIndex::setProcedureHighlights(const QVector<proc::MapProcedureLegs>& value)
{
  procedureHighlights = value;
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

void MapScreenIndex::updateDistanceMarkerFromPos(int id, const atools::geo::Pos& pos)
{
  distanceMarks[id].from = pos;
}

void MapScreenIndex::updateDistanceMarkerToPos(int id, const atools::geo::Pos& pos)
{
  distanceMarks[id].to = distanceMarks[id].position = pos;
}

void MapScreenIndex::updateDistanceMarker(int id, const map::DistanceMarker& marker)
{
  distanceMarks[id] = marker;
  distanceMarks[id].id = id;
}

const atools::fs::sc::SimConnectUserAircraft& MapScreenIndex::getUserAircraft() const
{
  return simData->getUserAircraftConst();
}

const atools::fs::sc::SimConnectUserAircraft& MapScreenIndex::getLastUserAircraft() const
{
  return lastSimData->getUserAircraftConst();
}

const QVector<atools::fs::sc::SimConnectAircraft>& MapScreenIndex::getAiAircraft() const
{
  return simData->getAiAircraftConst();
}

void MapScreenIndex::clearSimData()
{
  updateSimData(SimConnectData());
}

void MapScreenIndex::updateSimData(const atools::fs::sc::SimConnectData& data)
{
  *simData = data;
  updateAverageTurn();
}

void MapScreenIndex::updateAverageTurn()
{
  if(NavApp::isConnectedAndAircraftFlying())
  {
    // Sim is connected and aircraft is flying
    const SimConnectUserAircraft& userAircraft = simData->getUserAircraftConst();

#ifdef DEBUG_INFORMATION_TURN
    qDebug() << Q_FUNC_INFO << "userAircraft.getZuluTime()" << userAircraft.getZuluTime().toMSecsSinceEpoch();
#endif

    // Have a last aircraft and timestamp to calculate values
    if(lastUserAircraftForAverage->isValid() && lastUserAircraftForAverageTs.isValid())
    {
      // Time difference in milliseconds
      double timeDiff = lastUserAircraftForAverageTs.msecsTo(userAircraft.getZuluTime());

      if(timeDiff < 0.)
      {
        // Time changed backwards reset all
        lastUserAircraftForAverageTs = QDateTime();
        movingAverageSimAircraft->reset();
      }
      else
      {
        // Turn right is positive and turn left is negative
        double headingChange = atools::geo::angleAbsDiffSign(lastUserAircraftForAverage->getTrackDegTrue(), userAircraft.getTrackDegTrue());

        if(timeDiff > 0.)
        {
          // Heading change in degree per second
          double lateralAngularVelocity = headingChange / timeDiff * 1000.;

#ifdef DEBUG_INFORMATION_TURN
          qDebug() << Q_FUNC_INFO << "headingChange" << headingChange;
          qDebug() << Q_FUNC_INFO << "timeDiff" << timeDiff;
          qDebug() << Q_FUNC_INFO << "lateralAngularVelocity" << lateralAngularVelocity;
          movingAverageSimAircraft->debugDumpContainerSizes();
#endif

          // Sample groundspeed/turnspeed and timestamp
          movingAverageSimAircraft->addSamples(userAircraft.getGroundSpeedKts(), static_cast<float>(lateralAngularVelocity),
                                               userAircraft.getZuluTime().toMSecsSinceEpoch());
        }
      }
    }
    *lastUserAircraftForAverage = simData->getUserAircraftConst();
    lastUserAircraftForAverageTs = userAircraft.getZuluTime();
  }
  else
  {
    // Disconnected or aircraft on ground - clear all data
    lastUserAircraftForAverageTs = QDateTime();
    movingAverageSimAircraft->reset();
  }
}

void MapScreenIndex::getAverageGroundAndTurnSpeed(float& groundSpeedKts, float& turnSpeedDegPerSec) const
{
  movingAverageSimAircraft->getAverages(groundSpeedKts, turnSpeedDegPerSec);
#ifdef DEBUG_INFORMATION_TURN
  qDebug() << Q_FUNC_INFO << "groundSpeedKts" << groundSpeedKts;
  qDebug() << Q_FUNC_INFO << "turnSpeedDegPerSec" << turnSpeedDegPerSec;
#endif
}

void MapScreenIndex::updateLastSimData(const atools::fs::sc::SimConnectData& data)
{
  *lastSimData = data;
}

void MapScreenIndex::setProfileHighlight(const atools::geo::Pos& value)
{
  *profileHighlight = value;
}

const Pos& MapScreenIndex::getProfileHighlight() const
{
  return *profileHighlight;
}

void MapScreenIndex::updateRouteScreenGeometry(const Marble::GeoDataLatLonBox& curBox)
{
  const Route& route = NavApp::getRouteConst();

  bool missed = paintLayer->getShownMapTypes().testFlag(map::MISSED_APPROACH);
  bool alternate = paintLayer->getShownMapDisplayTypes().testFlag(map::FLIGHTPLAN_ALTERNATE);

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

      // Do not add alternate legs if they are hidden
      if(routeLeg.isAlternate() && !alternate)
        continue;

      const Pos& p2 = routeLeg.getPosition();
      int x2, y2;
      if(conv.wToS(p2, x2, y2))
      {
        map::MapTypes type = routeLeg.getMapType();
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

void MapScreenIndex::getAllNearest(const QPoint& point, int maxDistance, map::MapResult& result, map::MapObjectQueryTypes types) const
{
  using maptools::insertSortedByDistance;
  const MapLayer *mapLayer = paintLayer->getMapLayer();

  if(mapLayer == nullptr)
    return;

  int xs = point.x(), ys = point.y();
  CoordinateConverter conv(mapWidget->viewport());

  map::MapTypes shown = paintLayer->getShownMapTypes();
  map::MapDisplayTypes shownDisplay = paintLayer->getShownMapDisplayTypes();

  // Check for user aircraft ======================================================
  result.userAircraft.clear();
  if(shown & map::AIRCRAFT && NavApp::isConnectedAndAircraft())
  {
    const SimConnectUserAircraft& user = simData->getUserAircraftConst();
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
      for(const atools::fs::sc::SimConnectAircraft& obj : simData->getAiAircraftConst())
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
  bool hideAiOnGround = OptionData::instance().getFlags().testFlag(opts::MAP_AI_HIDE_GROUND);

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

    if(ac.isValid() && !ac.isAnyBoat() && mapfunc::aircraftVisible(ac, mapLayer, hideAiOnGround))
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
      if(mapfunc::aircraftVisible(obj, mapLayer, hideAiOnGround))
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

  const Route& route = NavApp::getRouteConst();
  // Flight plan objects =============================================================
  if(shownDisplay.testFlag(map::FLIGHTPLAN))
  {
    map::MapObjectQueryTypes queryTypes = types | map::QUERY_PROCEDURES | map::QUERY_PROC_POINTS;

    // Add points and procedures if missed is visible
    if(shown.testFlag(map::MISSED_APPROACH))
      queryTypes |= map::QUERY_PROC_MISSED_POINTS | map::QUERY_PROCEDURES_MISSED;

    // Query alternates if shown
    if(shownDisplay.testFlag(map::FLIGHTPLAN_ALTERNATE))
      queryTypes |= map::QUERY_ALTERNATE;

    // Get copies from flight plan if visible
    route.getNearest(conv, xs, ys, maxDistance, result, queryTypes, routeDrawnNavaids);

    if(types.testFlag(map::QUERY_PROC_RECOMMENDED))
      route.getNearestRecommended(conv, xs, ys, maxDistance, result, queryTypes, routeDrawnNavaids);
  }

  // Get points of procedure preview
  if(types.testFlag(map::QUERY_PREVIEW_PROC_POINTS))
  {
    map::MapObjectQueryTypes queryTypes = types | map::QUERY_PROCEDURES | map::QUERY_PROC_POINTS;
    if(shown.testFlag(map::MISSED_APPROACH))
      queryTypes |= map::QUERY_PROC_MISSED_POINTS;
    getNearestProcedureHighlights(xs, ys, maxDistance, result, queryTypes);
  }

  // Get copies from highlightMapObjects and marks (user features)
  getNearestHighlights(xs, ys, maxDistance, result, types);

  // Get objects from cache - already present objects will be skipped
  // Airway included to fetch waypoints
  map::MapTypes mapTypes = shown &
                           (map::AIRPORT_ALL_AND_ADDON | map::AIRPORT_MSA | map::VOR | map::NDB | map::WAYPOINT | map::MARKER |
                            map::HOLDING | map::AIRWAYJ | map::TRACK | map::AIRWAYV | map::USERPOINT | map::LOGBOOK);

  map::MapDisplayTypes displayTypes = shownDisplay;

  bool airportDiagram = mapLayer->isAirportDiagram() &&
                        OptionData::instance().getDisplayOptionsAirport().testFlag(optsd::ITEM_AIRPORT_DETAIL_PARKING);

  // Airports actually drawn having parking spots which require tooltips and more
  const QSet<int>& shownDetailAirportIds = paintLayer->getShownDetailAirportIds();
  mapWidget->getMapQuery()->getNearestScreenObjects(conv, mapLayer, shownDetailAirportIds, airportDiagram, mapTypes, displayTypes,
                                                    xs, ys, maxDistance, result);

  // Update all incomplete objects, especially from search preview
  for(map::MapAirport& airport : result.airports)
  {
    if(!airport.complete())
      airportQuery->getAirportById(airport, airport.getId());
  }

  if(shownDisplay.testFlag(map::FLIGHTPLAN))
    route.updateAirportRouteIndex(result);
  else
    route.clearAirportRouteIndex(result);

  // Check if pointer is near a wind barb in the one degree grid
  if(paintLayer->getShownMapDisplayTypes().testFlag(map::WIND_BARBS))
  {
    // Round screen position to nearest full degree grid cell
    atools::geo::Pos pos = conv.sToW(xs, ys).snapToGrid();
    if(pos.isValid())
    {
      if(mapfunc::windBarbVisible(pos, mapLayer, mapWidget->viewport()->projection() == Marble::Spherical))
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

  // Aircraft trails ========================================================
  auto coordinateFunc = [this](float lonX, float latY, double& xt, double& yt) {
                          mapWidget->screenCoordinates(lonX, latY, xt, yt);
                        };
  Pos pos = conv.sToW(xs, ys);

  if(pos.isValid())
  {
    if(types.testFlag(map::QUERY_AIRCRAFT_TRAIL))
      // Get nearest (one) trail segment and provide screen coordinate conversion function
      result.trailSegment = mapWidget->getAircraftTrail().findNearest(point, pos, maxDistance,
                                                                      mapWidget->viewport()->viewLatLonAltBox(), coordinateFunc);

    // Trail is only shown for single selection
    if(types.testFlag(map::QUERY_AIRCRAFT_TRAIL_LOG) && searchHighlights->logbookEntries.size() == 1)
    {
      const atools::fs::gpx::GpxData *gpxData =
        NavApp::getLogdataController()->getGpxData(searchHighlights->logbookEntries.constFirst().id);
      if(gpxData != nullptr)
      {
        AircraftTrail trail;
        trail.fillTrailFromGpxData(*gpxData);

        // Get nearest (one) trail segment from logbook preview and provide screen coordinate conversion function
        result.trailSegmentLog = trail.findNearest(point, pos, maxDistance, mapWidget->viewport()->viewLatLonAltBox(), coordinateFunc);
      }
    }
  }
}

void MapScreenIndex::getNearestHighlights(int xs, int ys, int maxDistance, map::MapResult& result, map::MapObjectQueryTypes types) const
{
  using maptools::insertSorted;
  using maptools::insertSortedFromTo;
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
  QSet<int> userFeatureIds;
  if(types & map::QUERY_MARK_HOLDINGS && NavApp::getMapMarkHandler()->getMarkTypes().testFlag(map::MARK_HOLDING))
    insertSorted(conv, xs, ys, holdingMarks.values(), result.holdingMarks, &userFeatureIds, maxDistance);

  if(types & map::QUERY_MARK_MSA && NavApp::getMapMarkHandler()->getMarkTypes().testFlag(map::MARK_MSA))
  {
    userFeatureIds.clear();
    insertSorted(conv, xs, ys, msaMarks.values(), result.msaMarks, &result.airportMsaIds, maxDistance);
  }

  if(types & map::QUERY_MARK_PATTERNS && NavApp::getMapMarkHandler()->getMarkTypes().testFlag(map::MARK_PATTERNS))
  {
    userFeatureIds.clear();
    insertSorted(conv, xs, ys, patternMarks.values(), result.patternMarks, &userFeatureIds, maxDistance);
  }

  if(types & map::QUERY_MARK_RANGE && NavApp::getMapMarkHandler()->getMarkTypes().testFlag(map::MARK_RANGE))
  {
    userFeatureIds.clear();
    insertSorted(conv, xs, ys, rangeMarks.values(), result.rangeMarks, &userFeatureIds, maxDistance);
  }

  if(types & map::QUERY_MARK_DISTANCE && NavApp::getMapMarkHandler()->getMarkTypes().testFlag(map::MARK_DISTANCE))
  {
    userFeatureIds.clear();
    insertSortedFromTo(conv, xs, ys, distanceMarks.values(), result.distanceMarks, &userFeatureIds, maxDistance);
  }
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

      if(leg.isMissed() && !types.testFlag(map::QUERY_PROC_MISSED_POINTS))
        continue;

      proc::MapProcedureRef ref(0 /* airportId */, 0 /* runwayEndId */, leg.procedureId, leg.transitionId, leg.legId, leg.mapType);
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

      // Remove artificial waypoints which were created to procedures or airways
      result.waypoints.erase(std::remove_if(result.waypoints.begin(), result.waypoints.end(), [](const map::MapWaypoint& wp) -> bool {
        return wp.artificial != map::WAYPOINT_ARTIFICIAL_NONE;
      }), result.waypoints.end());
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

int MapScreenIndex::getNearestDistanceMarkId(int xs, int ys, int maxDistance, bool *origin) const
{
  if(NavApp::getMapMarkHandler()->getMarkTypes() & map::MARK_DISTANCE)
  {
    CoordinateConverter conv(mapWidget->viewport());
    int x, y;
    for(const map::DistanceMarker& type : distanceMarks)
    {
      // Look for endpoints first
      if(conv.wToS(type.getPositionTo(), x, y) && atools::geo::manhattanDistance(x, y, xs, ys) < maxDistance)
      {
        if(origin != nullptr)
          *origin = false;
        return type.id;
      }
    }

    for(const map::DistanceMarker& type : distanceMarks)
    {
      // Look for origin points second
      if(conv.wToS(type.getPositionFrom(), x, y) && atools::geo::manhattanDistance(x, y, xs, ys) < maxDistance)
      {
        if(origin != nullptr)
          *origin = true;
        return type.id;
      }
    }
  }
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
  if(!paintLayer->getShownMapDisplayTypes().testFlag(map::FLIGHTPLAN))
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

void MapScreenIndex::getNearestAirspaces(int xs, int ys, map::MapResult& result) const
{
  for(const std::pair<map::MapAirspaceId, QPolygon>& polyPair : airspacePolygons)
  {
    if(polyPair.second.containsPoint(QPoint(xs, ys), Qt::OddEvenFill))
      result.airspaces.append(NavApp::getAirspaceController()->getAirspaceById(polyPair.first));
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
  if(paintLayer->getShownMapDisplayTypes().testFlag(map::LOGBOOK_DIRECT) ||
     paintLayer->getShownMapDisplayTypes().testFlag(map::LOGBOOK_ROUTE))
  {
    const QSet<int> nearestIds = nearestLineIds(logEntryLines, xs, ys, maxDistance, false /* also distance to points */);
    for(int id : nearestIds)
      maptools::insertSortedByDistance(conv, result.logbookEntries, &ids, xs, ys,
                                       NavApp::getLogdataController()->getLogEntryById(id));
  }
}

void MapScreenIndex::getNearestIls(int xs, int ys, int maxDistance, map::MapResult& result) const
{
  if(!paintLayer->getShownMapTypes().testFlag(map::ILS) && // Display and object type
     !paintLayer->getShownMapDisplayTypes().testFlag(map::GLS) && // Display type
     !paintLayer->getShownMapDisplayTypes().testFlag(map::FLIGHTPLAN))
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
  MapQuery *mapQuery = mapWidget->getMapQuery();
  for(int id : qAsConst(ilsIds))
    result.ils.append(mapQuery->getIlsById(id));
}

/* Get all airways near cursor position */
void MapScreenIndex::getNearestAirways(int xs, int ys, int maxDistance, map::MapResult& result) const
{
  AirwayTrackQuery *airwayTrackQuery = mapWidget->getAirwayTrackQuery();
  const QSet<int> nearestIds = nearestLineIds(airwayLines, xs, ys, maxDistance, true /* lineDistanceOnly */);
  for(int id : nearestIds)
    result.airways.append(airwayTrackQuery->getAirwayById(id));
}

int MapScreenIndex::getNearestRouteLegIndex(int xs, int ys, int maxDistance) const
{
  if(!paintLayer->getShownMapDisplayTypes().testFlag(map::FLIGHTPLAN))
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
