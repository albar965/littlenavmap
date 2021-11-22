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

#include "mappainter/mappaintlayer.h"

#include "navapp.h"
#include "mappainter/mappainterweather.h"
#include "mappainter/mappainterwind.h"
#include "connect/connectclient.h"
#include "common/mapcolors.h"
#include "mapgui/mapwidget.h"
#include "mapgui/maplayersettings.h"
#include "mappainter/mappainteraircraft.h"
#include "mappainter/mappaintermsa.h"
#include "mappainter/mappaintertrack.h"
#include "mappainter/mappaintership.h"
#include "mappainter/mappainterairport.h"
#include "mappainter/mappainterairspace.h"
#include "mappainter/mappainterils.h"
#include "mappainter/mappaintermark.h"
#include "mappainter/mappainternav.h"
#include "mappainter/mappainterroute.h"
#include "mappainter/mappainteruser.h"
#include "mappainter/mappainteraltitude.h"
#include "mappainter/mappaintertop.h"
#include "mapgui/mapscale.h"
#include "userdata/userdatacontroller.h"
#include "route/route.h"
#include "geo/calculations.h"
#include "options/optiondata.h"

#include <QElapsedTimer>

#include <marble/GeoPainter.h>

using namespace Marble;
using namespace atools::geo;

MapPaintLayer::MapPaintLayer(MapPaintWidget *widget)
  : mapWidget(widget)
{
  // Create the layer configuration
  initMapLayerSettings();

  mapScale = new MapScale();

  // Create all painters
  mapPainterNav = new MapPainterNav(mapWidget, mapScale, &context);
  mapPainterIls = new MapPainterIls(mapWidget, mapScale, &context);
  mapPainterAirport = new MapPainterAirport(mapWidget, mapScale, &context);
  mapPainterMsa = new MapPainterMsa(mapWidget, mapScale, &context);
  mapPainterAirspace = new MapPainterAirspace(mapWidget, mapScale, &context);
  mapPainterMark = new MapPainterMark(mapWidget, mapScale, &context);
  mapPainterRoute = new MapPainterRoute(mapWidget, mapScale, &context);
  mapPainterAircraft = new MapPainterAircraft(mapWidget, mapScale, &context);
  mapPainterTrack = new MapPainterTrack(mapWidget, mapScale, &context);
  mapPainterShip = new MapPainterShip(mapWidget, mapScale, &context);
  mapPainterUser = new MapPainterUser(mapWidget, mapScale, &context);
  mapPainterAltitude = new MapPainterAltitude(mapWidget, mapScale, &context);
  mapPainterWeather = new MapPainterWeather(mapWidget, mapScale, &context);
  mapPainterWind = new MapPainterWind(mapWidget, mapScale, &context);
  mapPainterTop = new MapPainterTop(mapWidget, mapScale, &context);

  // Default for visible object types
  objectTypes = map::MapTypes(map::AIRPORT_ALL_AND_ADDON) | map::MapTypes(map::VOR) | map::MapTypes(map::NDB) | map::MapTypes(map::AP_ILS) |
                map::MapTypes(map::MARKER) | map::MapTypes(map::WAYPOINT);
  objectDisplayTypes = map::DISPLAY_TYPE_NONE;
}

MapPaintLayer::~MapPaintLayer()
{
  delete mapPainterNav;
  delete mapPainterIls;
  delete mapPainterAirport;
  delete mapPainterMsa;
  delete mapPainterAirspace;
  delete mapPainterMark;
  delete mapPainterRoute;
  delete mapPainterAircraft;
  delete mapPainterTrack;
  delete mapPainterShip;
  delete mapPainterUser;
  delete mapPainterAltitude;
  delete mapPainterWeather;
  delete mapPainterWind;
  delete mapPainterTop;

  delete layers;
  delete mapScale;
}

void MapPaintLayer::copySettings(const MapPaintLayer& other)
{
  objectTypes = other.objectTypes;
  objectDisplayTypes = other.objectDisplayTypes;
  airspaceTypes = other.airspaceTypes;
  weatherSource = other.weatherSource;
  sunShading = other.sunShading;

  // Updates layers too
  setDetailFactor(other.detailFactor);
}

void MapPaintLayer::preDatabaseLoad()
{
  databaseLoadStatus = true;
}

void MapPaintLayer::postDatabaseLoad()
{
  databaseLoadStatus = false;
}

void MapPaintLayer::setShowMapObjects(map::MapTypes type, map::MapTypes mask)
{
  objectTypes &= ~mask;
  objectTypes |= type;
}

void MapPaintLayer::setShowMapObject(map::MapTypes type, bool show)
{
  if(show)
    objectTypes |= type;
  else
    objectTypes &= ~type;
}

void MapPaintLayer::setShowMapObjectDisplay(map::MapObjectDisplayTypes type, bool show)
{
  if(show)
    objectDisplayTypes |= type;
  else
    objectDisplayTypes &= ~type;
}

void MapPaintLayer::setShowAirspaces(map::MapAirspaceFilter types)
{
  airspaceTypes = types;
}

void MapPaintLayer::setDetailFactor(int factor)
{
  detailFactor = factor;
  updateLayers();
}

map::MapAirspaceFilter MapPaintLayer::getShownAirspacesTypesByLayer() const
{
  if(mapLayer == nullptr)
    return map::MapAirspaceFilter({map::AIRSPACE_NONE, map::AIRSPACE_FLAG_NONE});

  // Mask out all types that are not visible in the current layer
  map::MapAirspaceFilter filter = airspaceTypes;
  if(!mapLayer->isAirspaceIcao())
    filter.types &= ~map::AIRSPACE_CLASS_ICAO;

  if(!mapLayer->isAirspaceFg())
    filter.types &= ~map::AIRSPACE_CLASS_FG;

  if(!mapLayer->isAirspaceFirUir())
    filter.types &= ~map::AIRSPACE_FIR_UIR;

  if(!mapLayer->isAirspaceCenter())
    filter.types &= ~map::AIRSPACE_CENTER;

  if(!mapLayer->isAirspaceRestricted())
    filter.types &= ~map::AIRSPACE_RESTRICTED;

  if(!mapLayer->isAirspaceSpecial())
    filter.types &= ~map::AIRSPACE_SPECIAL;

  if(!mapLayer->isAirspaceOther())
    filter.types &= ~map::AIRSPACE_OTHER;

  return filter;
}

void MapPaintLayer::initQueries()
{
  mapPainterNav->initQueries();
  mapPainterIls->initQueries();
  mapPainterMsa->initQueries();
  mapPainterAirport->initQueries();
  mapPainterAirspace->initQueries();
  mapPainterMark->initQueries();
  mapPainterRoute->initQueries();
  mapPainterAircraft->initQueries();
  mapPainterTrack->initQueries();
  mapPainterShip->initQueries();
  mapPainterUser->initQueries();
  mapPainterAltitude->initQueries();
  mapPainterWeather->initQueries();
  mapPainterWind->initQueries();
  mapPainterTop->initQueries();
}

/* Initialize the layer settings that define what is drawn at what zoom distance (text, size, etc.) */
void MapPaintLayer::initMapLayerSettings()
{
  if(layers != nullptr)
    delete layers;

  // Load a list of map layers that define content for each zoom distance
  // File can be overloaded in the settings folder
  // Map widget is updated if the file changes
  layers = new MapLayerSettings();
  layers->connectMapSettingsUpdated(mapWidget);
  layers->loadFromFile();

#ifdef DEBUG_LAYER_SETTINGS_LEGACY
  // Create a default layer with all features enabled
  // Features are switched off step by step when adding new (higher) layers
  MapLayer defLayer = MapLayer(0);

  layers->startAppend();
  // Lowest layer including everything (airport diagram and details)
  // airport diagram, large VOR, NDB, ILS, waypoint, airway, marker
  layers->
  append(defLayer.clone(0.2f).
         airportFontScale(1.f).
         airportMaxTextLength(30).
         airportMsaSymbolScale(8.f).
         airportMinorFontScale(1.f).
         airportMinorMaxTextLength(30).
         airportMinorSymbolSize(20).
         airportSymbolSize(20).
         markerSymbolSize(24).
         ndbSymbolSize(30).
         userpointMaxTextLength(30).
         userpointSymbolSize(28).
         vorSymbolSize(30).
         waypointSymbolSize(14).
         windBarbsSymbolSize(22)).

  // Less airport diagram details
  append(layers->cloneLast(0.3f).
         airportDiagramDetail3(false).
         airportMsaSymbolScale(7.f).
         windBarbsSymbolSize(20)).

  // airport diagram, large VOR, NDB, ILS, waypoint, airway, marker
  append(layers->cloneLast(1.f).
         aiAircraftGroundText(false).
         airportDiagramDetail2(false).
         airportMsaSymbolScale(6.f).
         ndbSymbolSize(28).
         vorSymbolSize(28)).

  // airport diagram, large VOR, NDB, ILS, waypoint, airway, marker
  append(layers->cloneLast(5.f).
         airportDiagramDetail(false).
         airportMsaSymbolScale(5.f).
         ndbSymbolSize(26).
         userpointMaxTextLength(20).
         userpointSymbolSize(26).
         vorSymbolSize(26).
         waypointSymbolSize(10).
         windBarbsSymbolSize(18)).

  // airport, large VOR, NDB, ILS, waypoint, airway, marker
  append(layers->cloneLast(10.f).
         airportDiagram(false).
         airportMaxTextLength(20).
         airportMsaSymbolScale(4.f).
         airportMinorMaxTextLength(15).
         airportMinorSymbolSize(18).
         airportSymbolSize(18).
         airwayInfo(false).
         markerInfo(false).
         ndbSymbolSize(24).
         vorSymbolSize(24).
         waypointSymbolSize(8).
         windBarbsSymbolSize(16)).

  // airport, large VOR, NDB, ILS, waypoint, airway, marker
  append(layers->cloneLast(25.f).
         airportMsaDetails(false).
         approachTextDetail(false).
         ndbSymbolSize(22).
         userpointSymbolSize(24).
         vorSymbolSize(22)).

  // airport, large VOR, NDB, ILS, airway, waypoints, ILS
  append(layers->cloneLast(50.f).
         aiAircraftText(false).
         aiShipSmall(false).
         airportDiagramRunway(false).
         airportMaxTextLength(10).
         airportMinorFontScale(0.9f).
         airportMinorInfo(false).
         airportMinorMaxTextLength(8).
         airportMinorSymbolSize(16).
         airportSymbolSize(16).
         holdingInfo2(false).
         ilsIdent(false).
         ilsInfo(false).
         marker(false).
         ndbSymbolSize(20).
         userpointMaxTextLength(10).
         vorSymbolSize(20).
         waypointName(false).
         waypointSymbolSize(6)).

  // airport, VOR, NDB, ILS, airway, waypoints, ILS
  append(layers->cloneLast(100.f).
         aiAircraftGround(false).
         airportInfo(false).
         airportMinorFontScale(0.8f).
         airportMinorName(false).
         airportMinorSymbolSize(12).
         airportSymbolSize(14).
         airwayIdent(false).
         holdingInfo(false).
         ndbInfo(false).
         ndbSymbolSize(16).
         vorInfo(false).
         vorLarge(false).
         waypointSymbolSize(3).
         windBarbsSymbolSize(14)).

  // airport, VOR, NDB, airway, waypoints ILS
  append(layers->cloneLast(150.f).
         airportName(false).
         airportOverviewRunway(false).
         airportMinorSymbolSize(10).
         airportSymbolSize(12).
         approachText(false).
         ndbIdent(false).
         ndbSymbolSize(12).
         userpointMaxTextLength(8).
         userpointSymbolSize(22).
         vorIdent(false).
         vorSymbolSize(12).
         waypoint(false)).

  // airport > 2000, VOR, NDB, ILS, airways
  append(layers->cloneLast(250.f).
         airportMinorIdent(false).
         airportMinorSymbolSize(8).
         airportSymbolSize(10).
         minRunwayLength(2000).
         ndbSymbolSize(8).
         onlineAircraftText(false).
         userpointSymbolSize(16).
         vorSymbolSize(8)).

  // airport > 4000, VOR, NDB, airways
  append(layers->cloneLast(450.f).
         aiAircraftSize(26).
         airportRouteInfo(false).
         airportMinor(false).
         airwayWaypoint(false).
         ils(false).
         minRunwayLength(4000).
         ndbSymbolSize(4).
         trackInfo(false).
         userpointInfo(false).
         vorSymbolSize(6).
         waypointRouteName(false).
         windBarbsSymbolSize(12)).

  // airport > 6000, VOR
  append(layers->cloneLast(750.f).
         aiAircraftSize(24).
         aiShipLarge(false).
         airportFontScale(0.9f).
         airportMsa(false).
         airportSymbolSize(8).
         airspaceOther(false).
         airspaceRestricted(false).
         airspaceSpecial(false).
         airway(false).
         holding(false).
         minRunwayLength(6000).
         ndb(false).
         ndbRouteInfo(false).
         userpointSymbolSize(12).
         vorRouteInfo(false).
         vorSymbolSize(3)).

  // airport > 8000
  append(layers->cloneLast(1200.f).
         aiAircraftSize(20).
         aiAircraftSmall(false).
         airportFontScale(0.8f).
         airportSymbolSize(6).
         airspaceFg(false).
         airspaceIcao(false).
         approachDetail(false).
         minRunwayLength(8000).
         trackWaypoint(false).
         vor(false).
         windBarbsSymbolSize(10)).

  // airport > 8000
  append(layers->cloneLast(2400.f).
         aiAircraftSize(10).
         airportIdent(false).
         airportSymbolSize(4).
         airportWeather(false).
         airportWeatherDetails(false).
         airspaceCenter(false).
         mora(false).
         ndbRouteIdent(false).
         vorRouteIdent(false).
         windBarbsSymbolSize(6)).

  // Cut off limit 4000 km - only airport dots
  append(layers->cloneLast(layer::DISTANCE_CUT_OFF_LIMIT_KM).
         aiAircraftLarge(false).
         airportSymbolSize(3).
         approach(false).
         windBarbs(false)).

  // Make sure that there is always a layer
  append(layers->cloneLast(100000.f).
         airport(false).
         airportNoRating(false).
         airspaceFirUir(false).
         onlineAircraft(false).
         routeTextAndDetail(false).
         track(false).
         trackIdent(false).
         userpoint(false));

  // Sort layers
  layers->finishAppend();
#endif
}

/* Update the stored layer pointers after zoom distance has changed */
void MapPaintLayer::updateLayers()
{
  if(noRender())
    mapLayerEffective = mapLayer = mapLayerRoute = nullptr;
  else
  {
    float distKm = static_cast<float>(mapWidget->distance());
    // Get the uncorrected effective layer - route painting is independent of declutter
    mapLayerEffective = layers->getLayer(distKm);
    mapLayer = layers->getLayer(distKm, detailFactor);
    mapLayerRoute = layers->getLayer(distKm, detailFactor + 1);
  }
}

bool MapPaintLayer::noRender() const
{
  const ViewportParams *viewport = mapWidget->viewport();

  if(viewport->projection() == Marble::Mercator)
    // Do not draw if Mercator wraps around whole planet
    return viewport->viewLatLonAltBox().width(GeoDataCoordinates::Degree) >= 350.;
  else if(viewport->projection() == Marble::Spherical)
    // Limit drawing to maximum zoom distance
    return mapWidget->distance() > layer::NO_DRAW_LIMIT_KM;

  return false;
}

void MapPaintLayer::dumpMapLayers() const
{
  qDebug() << Q_FUNC_INFO << *layers;
}

bool MapPaintLayer::render(GeoPainter *painter, ViewportParams *viewport, const QString& renderPos, GeoSceneLayer *layer)
{
  Q_UNUSED(renderPos)
  Q_UNUSED(layer)

  if(!databaseLoadStatus && !mapWidget->isNoNavPaint())
  {
    // Update map scale for screen distance approximation
    mapScale->update(viewport, mapWidget->distance());
    updateLayers();

    // What to draw while scrolling or zooming map
    opts::MapScrollDetail mapScrollDetail = OptionData::instance().getMapScrollDetail();

    // Check if no painting wanted during scroll
    if(!(mapScrollDetail == opts::NONE && mapWidget->viewContext() == Marble::Animation) && !noRender())
    {
#ifdef DEBUG_INFORMATION_PAINT
      qDebug() << Q_FUNC_INFO << "layer" << *mapLayer;
#endif

      context = PaintContext();
      context.route = &NavApp::getRouteConst();
      context.mapLayer = mapLayer;
      context.mapLayerRoute = mapLayerRoute;
      context.mapLayerEffective = mapLayerEffective;
      context.painter = painter;
      context.viewport = viewport;
      context.objectTypes = objectTypes;
      context.objectDisplayTypes = objectDisplayTypes;
      context.airspaceFilterByLayer = getShownAirspacesTypesByLayer();
      context.viewContext = mapWidget->viewContext();
      context.drawFast = (mapScrollDetail == opts::FULL || mapScrollDetail == opts::HIGHER) ?
                         false : mapWidget->viewContext() == Marble::Animation;
      context.lazyUpdate = mapScrollDetail == opts::FULL ? false : mapWidget->viewContext() == Marble::Animation;
      context.mapScrollDetail = mapScrollDetail;
      context.distanceKm = static_cast<float>(mapWidget->distance());
      context.distanceNm = atools::geo::meterToNm(context.distanceKm * 1000.f);

      context.userPointTypes = NavApp::getUserdataController()->getSelectedTypes();
      context.userPointTypesAll = NavApp::getUserdataController()->getAllTypes();
      context.userPointTypeUnknown = NavApp::getUserdataController()->isSelectedUnknownType();
      context.zoomDistanceMeter = static_cast<float>(mapWidget->distance() * 1000.);
      context.darkMap = mapWidget->isDarkMap();

      context.mimimumRunwayLengthFt = minimumRunwayLenghtFt;

      // Copy default font
      context.defaultFont = painter->font();
      painter->setFont(context.defaultFont);

      const GeoDataLatLonAltBox& box = viewport->viewLatLonAltBox();
      context.viewportRect = atools::geo::Rect(box.west(GeoDataCoordinates::Degree),
                                               box.north(GeoDataCoordinates::Degree),
                                               box.east(GeoDataCoordinates::Degree),
                                               box.south(GeoDataCoordinates::Degree));

      context.screenRect = mapWidget->rect();

      const OptionData& od = OptionData::instance();

      context.symbolSizeAircraftAi = od.getDisplaySymbolSizeAircraftAi() / 100.f;
      context.symbolSizeAircraftUser = od.getDisplaySymbolSizeAircraftUser() / 100.f;
      context.symbolSizeAirport = od.getDisplaySymbolSizeAirport() / 100.f;
      context.symbolSizeAirportWeather = od.getDisplaySymbolSizeAirportWeather() / 100.f;
      context.symbolSizeWindBarbs = od.getDisplaySymbolSizeWindBarbs() / 100.f;
      context.symbolSizeNavaid = od.getDisplaySymbolSizeNavaid() / 100.f;
      context.symbolSizeUserpoint = od.getDisplaySymbolSizeUserpoint() / 100.f;
      context.textSizeAircraftAi = od.getDisplayTextSizeAircraftAi() / 100.f;
      context.textSizeAircraftUser = od.getDisplayTextSizeAircraftUser() / 100.f;
      context.textSizeAirport = od.getDisplayTextSizeAirport() / 100.f;
      context.textSizeFlightplan = od.getDisplayTextSizeFlightplan() / 100.f;
      context.textSizeNavaid = od.getDisplayTextSizeNavaid() / 100.f;
      context.textSizeUserpoint = od.getDisplayTextSizeUserpoint() / 100.f;
      context.textSizeAirway = od.getDisplayTextSizeAirway() / 100.f;
      context.textSizeCompassRose = od.getDisplayTextSizeCompassRose() / 100.f;
      context.textSizeMora = od.getDisplayTextSizeMora() / 100.f;
      context.transparencyMora = od.getDisplayTransparencyMora() / 100.f;
      context.textSizeAirportMsa = od.getDisplayTextSizeAirportMsa() / 100.f;
      context.transparencyAirportMsa = od.getDisplayTransparencyAirportMsa() / 100.f;
      context.transparencyFlightplan = od.getDisplayTransparencyFlightplan() / 100.f;
      context.transparencyHighlight = od.getDisplayMapHighlightTransparent() / 100.f;
      context.textSizeRangeDistance = od.getDisplayTextSizeRangeDistance() / 100.f;
      context.thicknessFlightplan = od.getDisplayThicknessFlightplan() / 100.f;
      context.thicknessTrail = od.getDisplayThicknessTrail() / 100.f;
      context.thicknessRangeDistance = od.getDisplayThicknessRangeDistance() / 100.f;
      context.thicknessCompassRose = od.getDisplayThicknessCompassRose() / 100.f;
      context.thicknessAirway = od.getDisplayThicknessAirway() / 100.f;

      context.dispOptsUser = od.getDisplayOptionsUserAircraft();
      context.dispOptsAi = od.getDisplayOptionsAiAircraft();
      context.dispOptsAirport = od.getDisplayOptionsAirport();
      context.dispOptsRose = od.getDisplayOptionsRose();
      context.dispOptsMeasurement = od.getDisplayOptionsMeasurement();
      context.dispOptsRoute = od.getDisplayOptionsRoute();
      context.flags = od.getFlags();
      context.flags2 = od.getFlags2();

      context.weatherSource = weatherSource;
      context.visibleWidget = mapWidget->isVisibleWidget();

      // ====================================
      // Get all waypoints from the route and add them to the map to avoid duplicate drawing
      if(context.objectDisplayTypes.testFlag(map::FLIGHTPLAN))
      {
        const Route& route = NavApp::getRouteConst();
        for(int i = 0; i < route.size(); i++)
        {
          const RouteLeg& routeLeg = route.value(i);
          map::MapTypes type = routeLeg.getMapObjectType();
          if(type == map::AIRPORT || type == map::VOR || type == map::NDB || type == map::WAYPOINT)
            context.routeProcIdMap.insert(map::MapObjectRef(routeLeg.getId(), routeLeg.getMapObjectType()));
          else if(type == map::PROCEDURE)
          {
            if(!routeLeg.getProcedureLeg().isMissed() || context.objectTypes & map::MISSED_APPROACH)
            {
              const map::MapResult& navaids = routeLeg.getProcedureLeg().navaids;
              if(navaids.hasWaypoints())
                context.routeProcIdMap.insert({navaids.waypoints.first().id, map::WAYPOINT});
              if(navaids.hasVor())
                context.routeProcIdMap.insert({navaids.vors.first().id, map::VOR});
              if(navaids.hasNdb())
                context.routeProcIdMap.insert({navaids.ndbs.first().id, map::NDB});
            }
          }
        }
      }

      // ====================================
      // Get navaids from procedure highlight to avoid duplicate drawing

      if(context.mapLayerRoute->isApproach())
      {
        const proc::MapProcedureLegs& procs = mapWidget->getProcedureHighlight();
        for(int i = 0; i < procs.size(); i++)
        {
          const map::MapResult& navaids = procs.at(i).navaids;
          if(navaids.hasWaypoints())
            context.routeProcIdMap.insert({navaids.waypoints.first().id, map::WAYPOINT});
          if(navaids.hasVor())
            context.routeProcIdMap.insert({navaids.vors.first().id, map::VOR});
          if(navaids.hasNdb())
            context.routeProcIdMap.insert({navaids.ndbs.first().id, map::NDB});
        }
      }

      // ====================================
      // Get airports from logbook highlight to avoid duplicate drawing
      const map::MapResult& highlightResultsSearch = mapWidget->getSearchHighlights();
      for(const map::MapLogbookEntry& entry : highlightResultsSearch.logbookEntries)
      {
        if(entry.departurePos.isValid())
          context.routeProcIdMap.insert({entry.departure.id, map::AIRPORT});
        if(entry.destinationPos.isValid())
          context.routeProcIdMap.insert({entry.destination.id, map::AIRPORT});
      }

      // Set render hints depending on context (moving, still) =====================
      if(mapWidget->viewContext() == Marble::Still)
      {
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setRenderHint(QPainter::TextAntialiasing, true);
        painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
      }
      else if(mapWidget->viewContext() == Marble::Animation)
      {
        painter->setRenderHint(QPainter::Antialiasing, false);
        painter->setRenderHint(QPainter::TextAntialiasing, false);
        painter->setRenderHint(QPainter::SmoothPixmapTransform, false);
      }

      // =========================================================================
      // Draw ====================================

      // Altitude below all others
      mapPainterAltitude->render();

      // Ship below other navaids and airports
      mapPainterShip->render();

      if(mapWidget->distance() < layer::DISTANCE_CUT_OFF_LIMIT_KM)
      {
        if(!context.isObjectOverflow())
          mapPainterAirspace->render();

        if(!context.isObjectOverflow())
          mapPainterIls->render();

        if(context.mapLayer->isAirportDiagram())
        {
          if(!context.isObjectOverflow())
            mapPainterAirport->render();

          if(!context.isObjectOverflow())
            mapPainterNav->render();
        }
        else
        {
          if(!context.isObjectOverflow())
            mapPainterMsa->render();

          if(!context.isObjectOverflow())
            mapPainterNav->render();

          if(!context.isObjectOverflow())
            mapPainterAirport->render();
        }
      }

      if(!context.isObjectOverflow())
        mapPainterUser->render();

      if(!context.isObjectOverflow())
        mapPainterWind->render();

      // if(!context.isOverflow()) always paint route even if number of objects is too large
      mapPainterRoute->render();

      if(!context.isObjectOverflow())
        mapPainterWeather->render();

      if(context.mapLayer->isAirportDiagram() && !context.isObjectOverflow())
        mapPainterMsa->render();

      if(!context.isObjectOverflow())
        mapPainterTrack->render();

      mapPainterMark->render();

      mapPainterAircraft->render();

      mapPainterTop->render();
    }

    if(!mapWidget->isPrinting() && mapWidget->isVisibleWidget())
      // Dim the map by drawing a semi-transparent black rectangle - but not for printing or web services
      mapcolors::darkenPainterRect(*painter);
  }
  return true;
}
