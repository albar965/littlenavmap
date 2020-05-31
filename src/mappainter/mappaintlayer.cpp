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

MapPaintLayer::MapPaintLayer(MapPaintWidget *widget, MapQuery *mapQueries)
  : mapQuery(mapQueries), mapWidget(widget)
{
  // Create the layer configuration
  initMapLayerSettings();

  mapScale = new MapScale();

  // Create all painters
  mapPainterNav = new MapPainterNav(mapWidget, mapScale);
  mapPainterIls = new MapPainterIls(mapWidget, mapScale);
  mapPainterAirport = new MapPainterAirport(mapWidget, mapScale, &NavApp::getRouteConst());
  mapPainterAirspace = new MapPainterAirspace(mapWidget, mapScale, &NavApp::getRouteConst());
  mapPainterMark = new MapPainterMark(mapWidget, mapScale);
  mapPainterRoute = new MapPainterRoute(mapWidget, mapScale, &NavApp::getRouteConst());
  mapPainterAircraft = new MapPainterAircraft(mapWidget, mapScale);
  mapPainterShip = new MapPainterShip(mapWidget, mapScale);
  mapPainterUser = new MapPainterUser(mapWidget, mapScale);
  mapPainterAltitude = new MapPainterAltitude(mapWidget, mapScale);
  mapPainterWeather = new MapPainterWeather(mapWidget, mapScale);
  mapPainterWind = new MapPainterWind(mapWidget, mapScale);
  mapPainterTop = new MapPainterTop(mapWidget, mapScale);

  // Default for visible object types
  objectTypes = map::MapObjectTypes(map::AIRPORT | map::VOR | map::NDB | map::AP_ILS | map::MARKER | map::WAYPOINT);
  objectDisplayTypes = map::DISPLAY_TYPE_NONE;
}

MapPaintLayer::~MapPaintLayer()
{
  delete mapPainterIls;
  delete mapPainterNav;
  delete mapPainterAirport;
  delete mapPainterAirspace;
  delete mapPainterMark;
  delete mapPainterRoute;
  delete mapPainterAircraft;
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

void MapPaintLayer::setShowMapObjects(map::MapObjectTypes type, bool show)
{
  if(show)
    objectTypes |= type;
  else
    objectTypes &= ~type;
}

void MapPaintLayer::setShowMapObjectsDisplay(map::MapObjectDisplayTypes type, bool show)
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
  // Mask out all types that are not visible in the current layer
  map::MapAirspaceFilter filter = airspaceTypes;
  if(!mapLayer->isAirspaceIcao())
    filter.types &= ~map::AIRSPACE_ICAO;
  if(!mapLayer->isAirspaceFir())
    filter.types &= ~map::AIRSPACE_FIR;
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

/* Initialize the layer settings that define what is drawn at what zoom distance (text, size, etc.) */
void MapPaintLayer::initMapLayerSettings()
{
  // TODO move to configuration file
  if(layers != nullptr)
    delete layers;

  // =====================================================================================
  // Create a list of map layers that define content for each zoom distance
  layers = new MapLayerSettings();

  // Create a default layer with all features enabled
  // Features are switched off step by step when adding new (higher) layers
  MapLayer defLayer = MapLayer(0).airport().approach().approachTextAndDetail().airportName().airportIdent().
                      airportSoft().airportNoRating().airportOverviewRunway().airportSource(layer::ALL).

                      airportWeather().airportWeatherDetails().

                      windBarbs().

                      routeTextAndDetail().

                      minimumAltitude().

                      vor().ndb().waypoint().marker().ils().airway().track().

                      userpoint().userpointInfo().

                      aiAircraftGround().aiAircraftLarge().aiAircraftSmall().aiShipLarge().aiShipSmall().
                      aiAircraftGroundText().aiAircraftText().

                      onlineAircraft().onlineAircraftText().

                      airspaceCenter().airspaceFir().airspaceOther().airspaceRestricted().airspaceSpecial().
                      airspaceIcao().

                      vorRouteIdent().vorRouteInfo().ndbRouteIdent().ndbRouteInfo().waypointRouteName().
                      airportRouteInfo();

  // Lowest layer including everything (airport diagram and details)
  // airport diagram, large VOR, NDB, ILS, waypoint, airway, marker
  layers->
  append(defLayer.clone(0.2f).airportDiagramRunway().airportDiagram().
         airportDiagramDetail().airportDiagramDetail2().airportDiagramDetail3().
         airportSymbolSize(20).airportInfo().
         windBarbsSymbolSize(22).
         waypointSymbolSize(14).waypointName().
         vorSymbolSize(24).vorIdent().vorInfo().vorLarge().
         ndbSymbolSize(24).ndbIdent().ndbInfo().
         ilsIdent().ilsInfo().
         airwayIdent().airwayInfo().airwayWaypoint().
         trackIdent().trackInfo().trackWaypoint().
         userpoint().userpointInfo().userpoinSymbolSize(28).userpointMaxTextLength(30).
         markerSymbolSize(24).markerInfo().
         airportMaxTextLength(30)).

  append(defLayer.clone(0.3f).airportDiagramRunway().airportDiagram().airportDiagramDetail().airportDiagramDetail2().
         airportSymbolSize(20).airportInfo().
         windBarbsSymbolSize(20).
         waypointSymbolSize(14).waypointName().
         vorSymbolSize(24).vorIdent().vorInfo().vorLarge().
         ndbSymbolSize(24).ndbIdent().ndbInfo().
         ilsIdent().ilsInfo().
         airwayIdent().airwayInfo().airwayWaypoint().
         trackIdent().trackInfo().trackWaypoint().
         userpoint().userpointInfo().userpoinSymbolSize(28).userpointMaxTextLength(30).
         markerSymbolSize(24).markerInfo().
         airportMaxTextLength(30)).

  // airport diagram, large VOR, NDB, ILS, waypoint, airway, marker
  append(defLayer.clone(1.f).airportDiagramRunway().airportDiagram().airportDiagramDetail().
         airportSymbolSize(20).airportInfo().
         windBarbsSymbolSize(20).
         aiAircraftGroundText(false).
         waypointSymbolSize(14).waypointName().
         vorSymbolSize(24).vorIdent().vorInfo().vorLarge().
         ndbSymbolSize(24).ndbIdent().ndbInfo().
         ilsIdent().ilsInfo().
         airwayIdent().airwayInfo().airwayWaypoint().
         trackIdent().trackInfo().trackWaypoint().
         userpoint().userpointInfo().userpoinSymbolSize(28).userpointMaxTextLength(30).
         markerSymbolSize(24).markerInfo().
         airportMaxTextLength(30)).

  // airport diagram, large VOR, NDB, ILS, waypoint, airway, marker
  append(defLayer.clone(5.f).airportDiagramRunway().airportDiagram().airportSymbolSize(20).airportInfo().
         waypointSymbolSize(10).waypointName().
         windBarbsSymbolSize(18).
         aiAircraftGroundText(false).
         vorSymbolSize(24).vorIdent().vorInfo().vorLarge().
         ndbSymbolSize(24).ndbIdent().ndbInfo().
         ilsIdent().ilsInfo().
         airwayIdent().airwayInfo().airwayWaypoint().
         trackIdent().trackInfo().trackWaypoint().
         userpoint().userpointInfo().userpoinSymbolSize(26).userpointMaxTextLength(20).
         markerSymbolSize(24).markerInfo().
         airportMaxTextLength(30)).

  // airport, large VOR, NDB, ILS, waypoint, airway, marker
  append(defLayer.clone(10.f).airportDiagramRunway().airportSymbolSize(18).airportInfo().
         waypointSymbolSize(8).waypointName().
         windBarbsSymbolSize(16).
         aiAircraftGroundText(false).
         vorSymbolSize(22).vorIdent().vorInfo().vorLarge().
         ndbSymbolSize(22).ndbIdent().ndbInfo().
         ilsIdent().ilsInfo().
         airwayIdent().airwayWaypoint().
         trackIdent().trackInfo().trackWaypoint().
         userpoint().userpointInfo().userpoinSymbolSize(26).userpointMaxTextLength(20).
         markerSymbolSize(24).
         airportMaxTextLength(20)).

  // airport, large VOR, NDB, ILS, waypoint, airway, marker
  append(defLayer.clone(25.f).airportDiagramRunway().airportSymbolSize(18).airportInfo().
         waypointSymbolSize(8).
         windBarbsSymbolSize(16).
         aiAircraftGroundText(false).
         vorSymbolSize(22).vorIdent().vorInfo().vorLarge().
         ndbSymbolSize(22).ndbIdent().ndbInfo().
         ilsIdent().ilsInfo().
         airwayIdent().airwayWaypoint().
         trackIdent().trackInfo().trackWaypoint().
         userpoint().userpointInfo().userpoinSymbolSize(24).userpointMaxTextLength(20).
         markerSymbolSize(24).
         airportMaxTextLength(20)).

  // airport, large VOR, NDB, ILS, airway
  append(defLayer.clone(50.f).airportSymbolSize(18).airportInfo().
         waypoint(false).
         windBarbsSymbolSize(16).
         aiShipSmall(false).aiAircraftGroundText(false).aiAircraftText(false).
         vorSymbolSize(20).vorIdent().vorInfo().vorLarge().
         ndbSymbolSize(20).ndbIdent().ndbInfo().
         airwayIdent().airwayWaypoint().
         trackIdent().trackInfo().trackWaypoint().
         userpoint().userpointInfo().userpoinSymbolSize(24).userpointMaxTextLength(10).
         marker(false).
         airportMaxTextLength(16)).

  // airport, VOR, NDB, ILS, airway
  append(defLayer.clone(100.f).airportSymbolSize(12).
         waypoint(false).
         windBarbsSymbolSize(14).
         aiAircraftGround(false).aiShipSmall(false).aiAircraftGroundText(false).aiAircraftText(false).
         vorSymbolSize(16).vorIdent().
         ndbSymbolSize(16).ndbIdent().
         airwayWaypoint().
         trackIdent().trackInfo().trackWaypoint().
         userpoint().userpointInfo().userpoinSymbolSize(24).userpointMaxTextLength(10).
         marker(false).
         airportMaxTextLength(16)).

  // airport, VOR, NDB, airway
  append(defLayer.clone(150.f).airportSymbolSize(10).minRunwayLength(2500).
         airportOverviewRunway(false).airportName(false).
         windBarbsSymbolSize(14).
         approachTextAndDetail(false).
         aiAircraftGround(false).aiShipSmall(false).aiAircraftGroundText(false).aiAircraftText(false).
         waypoint(false).
         vorSymbolSize(12).
         ndbSymbolSize(12).
         airwayWaypoint().
         trackIdent().trackInfo().trackWaypoint().
         userpoint().userpointInfo().userpoinSymbolSize(22).userpointMaxTextLength(8).
         marker(false).ils(false).
         airportMaxTextLength(16)).

  // airport > 4000, VOR
  append(defLayer.clone(200.f).airportSymbolSize(10).minRunwayLength(layer::MAX_MEDIUM_RUNWAY_FT).
         airportOverviewRunway(false).airportName(false).airportSource(layer::MEDIUM).
         windBarbsSymbolSize(14).
         approachTextAndDetail(false).
         aiAircraftGround(false).aiShipSmall(false).aiAircraftGroundText(false).aiAircraftText(false).
         onlineAircraftText(false).
         airwayWaypoint().
         trackIdent().trackInfo().trackWaypoint().
         vorSymbolSize(8).ndb(false).waypoint(false).marker(false).ils(false).
         userpoint().userpointInfo().userpoinSymbolSize(16).userpointMaxTextLength(8).
         airportMaxTextLength(16)).

  // airport > 4000
  append(defLayer.clone(300.f).airportSymbolSize(10).minRunwayLength(layer::MAX_MEDIUM_RUNWAY_FT).
         airportOverviewRunway(false).airportName(false).airportSource(layer::MEDIUM).
         windBarbsSymbolSize(12).
         approachTextAndDetail(false).
         aiAircraftGround(false).aiShipSmall(false).
         aiAircraftGroundText(false).aiAircraftText(false).
         onlineAircraftText(false).
         trackIdent().trackInfo(false).trackWaypoint().
         ndb(false).waypoint(false).marker(false).ils(false).
         trackIdent().trackInfo(false).trackWaypoint().
         airportRouteInfo(false).waypointRouteName(false).
         userpoint().userpointInfo(false).userpoinSymbolSize(16).
         airportMaxTextLength(16)).

  // airport > 8000
  append(defLayer.clone(750.f).airportSymbolSize(10).minRunwayLength(layer::MAX_LARGE_RUNWAY_FT).
         airportOverviewRunway(false).airportName(false).airportSource(layer::LARGE).
         windBarbsSymbolSize(12).
         airportWeatherDetails(false).
         approachTextAndDetail(false).
         aiAircraftGround(false).aiShipLarge(false).aiShipSmall(false).
         aiAircraftGroundText(false).aiAircraftText(false).
         onlineAircraftText(false).
         airspaceOther(false).airspaceRestricted(false).airspaceSpecial(false).
         vor(false).ndb(false).waypoint(false).marker(false).ils(false).airway(false).
         trackIdent().trackInfo(false).trackWaypoint().
         airportRouteInfo(false).vorRouteInfo(false).ndbRouteInfo(false).waypointRouteName(false).
         userpoint().userpointInfo(false).userpoinSymbolSize(12).
         airportMaxTextLength(16)).

  // airport > 8000
  append(defLayer.clone(1200.f).airportSymbolSize(10).minRunwayLength(layer::MAX_LARGE_RUNWAY_FT).
         airportOverviewRunway(false).airportName(false).airportSource(layer::LARGE).
         airportWeather(false).airportWeatherDetails(false).
         windBarbsSymbolSize(10).
         approachTextAndDetail(false).
         aiAircraftGround(false).aiAircraftSmall(false).aiShipLarge(false).aiShipSmall(false).
         aiAircraftGroundText(false).aiAircraftText(false).
         onlineAircraftText(false).
         airspaceFir(false).airspaceOther(false).airspaceRestricted(false).airspaceSpecial(false).
         airspaceIcao(false).
         vor(false).ndb(false).waypoint(false).marker(false).ils(false).airway(false).
         trackIdent().trackInfo(false).trackWaypoint(false).
         airportRouteInfo(false).vorRouteInfo(false).ndbRouteInfo(false).waypointRouteName(false).
         userpoint().userpointInfo(false).userpoinSymbolSize(12).
         airportMaxTextLength(16)).

  // Display only points for airports until the cutoff limit
  // airport > 8000
  append(defLayer.clone(2400.f).airportSymbolSize(5).
         minRunwayLength(layer::MAX_LARGE_RUNWAY_FT).
         airportOverviewRunway(false).airportName(false).airportIdent(false).airportSource(layer::LARGE).
         airportWeather(false).airportWeatherDetails(false).
         windBarbsSymbolSize(6).
         minimumAltitude(false).
         approach(false).approachTextAndDetail(false).
         aiAircraftGround(false).aiAircraftSmall(false).aiShipLarge(false).aiShipSmall(false).
         aiAircraftGroundText(false).aiAircraftText(false).
         onlineAircraft(false).onlineAircraftText(false).
         airspaceCenter(false).airspaceFir(false).airspaceOther(false).
         airspaceRestricted(false).airspaceSpecial(false).airspaceIcao(false).
         vor(false).ndb(false).waypoint(false).marker(false).ils(false).airway(false).
         trackIdent().trackInfo(false).trackWaypoint(false).
         airportRouteInfo(false).vorRouteInfo(false).ndbRouteInfo(false).waypointRouteName(false).
         userpoint().userpointInfo(false).userpoinSymbolSize(12).
         airportMaxTextLength(16)).

  append(defLayer.clone(layer::DISTANCE_CUT_OFF_LIMIT).airportSymbolSize(5).
         minRunwayLength(layer::MAX_LARGE_RUNWAY_FT).
         airportOverviewRunway(false).airportName(false).airportIdent(false).airportSource(layer::LARGE).
         airportWeather(false).airportWeatherDetails(false).
         windBarbs(false).
         minimumAltitude(false).
         approach(false).approachTextAndDetail(false).
         aiAircraftGround(false).aiAircraftLarge(false).aiAircraftSmall(false).aiShipLarge(false).aiShipSmall(false).
         aiAircraftGroundText(false).aiAircraftText(false).
         onlineAircraft(false).onlineAircraftText(false).
         airspaceCenter(false).airspaceFir(false).airspaceOther(false).
         airspaceRestricted(false).airspaceSpecial(false).airspaceIcao(false).
         vor(false).ndb(false).waypoint(false).marker(false).ils(false).airway(false).
         trackIdent().trackInfo(false).trackWaypoint(false).
         airportRouteInfo(false).vorRouteInfo(false).ndbRouteInfo(false).waypointRouteName(false).
         userpoint().userpointInfo(false).userpoinSymbolSize(12).
         airportMaxTextLength(16)).

  // Make sure that there is always a layer
  append(defLayer.clone(100000.f).airportSymbolSize(5).minRunwayLength(layer::MAX_LARGE_RUNWAY_FT).
         airportOverviewRunway(false).airportName(false).airportIdent(false).airportSource(layer::LARGE).
         airportWeather(false).airportWeatherDetails(false).
         windBarbs(false).
         minimumAltitude(false).
         routeTextAndDetail(false).
         approach(false).approachTextAndDetail(false).
         aiAircraftGround(false).aiAircraftLarge(false).aiAircraftSmall(false).aiShipLarge(false).aiShipSmall(false).
         aiAircraftGroundText(false).aiAircraftText(false).
         onlineAircraft(false).onlineAircraftText(false).
         airspaceCenter(false).airspaceFir(false).airspaceOther(false).
         airspaceRestricted(false).airspaceSpecial(false).airspaceIcao(false).
         airport(false).vor(false).ndb(false).waypoint(false).marker(false).ils(false).airway(false).track(false).
         airportRouteInfo(false).vorRouteInfo(false).ndbRouteInfo(false).waypointRouteName(false).
         userpoint(false).userpointInfo(false).userpoinSymbolSize(12).
         airportMaxTextLength(16));

  // Sort layers
  layers->finishAppend();
  qDebug() << *layers;
}

/* Update the stored layer pointers after zoom distance has changed */
void MapPaintLayer::updateLayers()
{
  float dist = static_cast<float>(mapWidget->distance());
  // Get the uncorrected effective layer - route painting is independent of declutter
  mapLayerEffective = layers->getLayer(dist);
  mapLayer = layers->getLayer(dist, detailFactor);
}

bool MapPaintLayer::render(GeoPainter *painter, ViewportParams *viewport, const QString& renderPos,
                           GeoSceneLayer *layer)
{
  Q_UNUSED(renderPos)
  Q_UNUSED(layer)

  if(!databaseLoadStatus && !mapWidget->isNoNavPaint())
  {
    // Update map scale for screen distance approximation
    mapScale->update(viewport, mapWidget->distance());

    // What to draw while scrolling or zooming map
    opts::MapScrollDetail mapScrollDetail = OptionData::instance().getMapScrollDetail();

    // Check if no painting wanted during scroll
    if(!(mapScrollDetail == opts::NONE && mapWidget->viewContext() == Marble::Animation) && // Performance settings
       !(viewport->projection() == Marble::Mercator && // Do not draw if Mercator wraps around whole planet
         viewport->viewLatLonAltBox().width(GeoDataCoordinates::Degree) >= 359.))
    {
      updateLayers();

#ifdef DEBUG_INFORMATION_PAINT
      qDebug() << Q_FUNC_INFO << "layer" << *mapLayer;
#endif

      PaintContext context;
      context.mapLayer = mapLayer;
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
      context.distance = atools::geo::meterToNm(static_cast<float>(mapWidget->distance() * 1000.));

      context.userPointTypes = NavApp::getUserdataController()->getSelectedTypes();
      context.userPointTypesAll = NavApp::getUserdataController()->getAllTypes();
      context.userPointTypeUnknown = NavApp::getUserdataController()->isSelectedUnknownType();
      context.zoomDistanceMeter = static_cast<float>(mapWidget->distance() * 1000.);

      // Copy default font
      context.defaultFont = painter->font();
      context.defaultFont.setBold(true);
      painter->setFont(context.defaultFont);

      const GeoDataLatLonAltBox& box = viewport->viewLatLonAltBox();
      context.viewportRect = atools::geo::Rect(box.west(GeoDataCoordinates::Degree),
                                               box.north(GeoDataCoordinates::Degree),
                                               box.east(GeoDataCoordinates::Degree),
                                               box.south(GeoDataCoordinates::Degree));

      const OptionData& od = OptionData::instance();

      context.symbolSizeAircraftAi = od.getDisplaySymbolSizeAircraftAi() / 100.f;
      context.symbolSizeAircraftUser = od.getDisplaySymbolSizeAircraftUser() / 100.f;
      context.symbolSizeAirport = od.getDisplaySymbolSizeAirport() / 100.f;
      context.symbolSizeAirportWeather = od.getDisplaySymbolSizeAirportWeather() / 100.f;
      context.symbolSizeWindBarbs = od.getDisplaySymbolSizeWindBarbs() / 100.f;
      context.symbolSizeNavaid = od.getDisplaySymbolSizeNavaid() / 100.f;
      context.textSizeAircraftAi = od.getDisplayTextSizeAircraftAi() / 100.f;
      context.textSizeAircraftUser = od.getDisplayTextSizeAircraftUser() / 100.f;
      context.textSizeAirport = od.getDisplayTextSizeAirport() / 100.f;
      context.textSizeFlightplan = od.getDisplayTextSizeFlightplan() / 100.f;
      context.textSizeNavaid = od.getDisplayTextSizeNavaid() / 100.f;
      context.textSizeCompassRose = od.getDisplayTextSizeCompassRose() / 100.f;
      context.textSizeMora = od.getDisplayTextSizeMora() / 100.f;
      context.transparencyMora = od.getDisplayTransparencyMora() / 100.f;
      context.textSizeRangeDistance = od.getDisplayTextSizeRangeDistance() / 100.f;
      context.thicknessFlightplan = od.getDisplayThicknessFlightplan() / 100.f;
      context.thicknessTrail = od.getDisplayThicknessTrail() / 100.f;
      context.thicknessRangeDistance = od.getDisplayThicknessRangeDistance() / 100.f;
      context.thicknessCompassRose = od.getDisplayThicknessCompassRose() / 100.f;

      context.dispOpts = od.getDisplayOptions();
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
          map::MapObjectTypes type = routeLeg.getMapObjectType();
          if(type == map::AIRPORT || type == map::VOR || type == map::NDB || type == map::WAYPOINT)
            context.routeIdMap.insert(map::MapObjectRef(routeLeg.getId(), routeLeg.getMapObjectType()));
          else if(type == map::PROCEDURE)
          {
            if(!routeLeg.getProcedureLeg().isMissed() || context.objectTypes & map::MISSED_APPROACH)
            {
              const map::MapSearchResult& navaids = routeLeg.getProcedureLeg().navaids;
              if(navaids.hasWaypoints())
                context.routeIdMap.insert({navaids.waypoints.first().id, map::WAYPOINT});
              if(navaids.hasVor())
                context.routeIdMap.insert({navaids.vors.first().id, map::VOR});
              if(navaids.hasNdb())
                context.routeIdMap.insert({navaids.ndbs.first().id, map::NDB});
            }
          }
        }
      }

      // ====================================
      // Get airports from logbook highlight to avoid duplicate drawing
      const map::MapSearchResult& highlightResultsSearch = mapWidget->getSearchHighlights();
      for(const map::MapLogbookEntry& entry : highlightResultsSearch.logbookEntries)
      {
        if(entry.departurePos.isValid())
          context.routeIdMap.insert({entry.departure.id, map::AIRPORT});
        if(entry.destinationPos.isValid())
          context.routeIdMap.insert({entry.destination.id, map::AIRPORT});
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
      mapPainterAltitude->render(&context);

      // Ship below other navaids and airports
      mapPainterShip->render(&context);

      if(mapWidget->distance() < layer::DISTANCE_CUT_OFF_LIMIT)
      {
        if(!context.isOverflow())
          mapPainterAirspace->render(&context);

        if(context.mapLayerEffective->isAirportDiagram())
        {
          // Put ILS below and navaids on top of airport diagram
          mapPainterIls->render(&context);

          if(!context.isOverflow())
            mapPainterAirport->render(&context);

          if(!context.isOverflow())
            mapPainterNav->render(&context);
        }
        else
        {
          // Airports on top of all
          if(!context.isOverflow())
            mapPainterIls->render(&context);

          if(!context.isOverflow())
            mapPainterNav->render(&context);

          if(!context.isOverflow())
            mapPainterAirport->render(&context);
        }
      }

      if(!context.isOverflow())
        mapPainterUser->render(&context);

      mapPainterWind->render(&context);

      // if(!context.isOverflow()) always paint route even if number of objets is too large
      mapPainterRoute->render(&context);

      mapPainterWeather->render(&context);

      // if(!context.isOverflow())
      mapPainterMark->render(&context);

      mapPainterAircraft->render(&context);

      mapPainterTop->render(&context);

      if(context.isOverflow())
        overflow = PaintContext::MAX_OBJECT_COUNT;
      else
        overflow = 0;
    }

    if(!mapWidget->isPrinting() && mapWidget->isVisibleWidget())
      // Dim the map by drawing a semi-transparent black rectangle - but not for printing or web services
      mapcolors::darkenPainterRect(*painter);
  }
  return true;
}
