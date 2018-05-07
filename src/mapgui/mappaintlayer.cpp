/*****************************************************************************
* Copyright 2015-2018 Alexander Barthel albar965@mailbox.org
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

#include "mapgui/mappaintlayer.h"

#include "navapp.h"
#include "connect/connectclient.h"
#include "mapgui/mapwidget.h"
#include "mapgui/maplayersettings.h"
#include "mapgui/mappainteraircraft.h"
#include "mapgui/mappaintership.h"
#include "mapgui/mappainterairport.h"
#include "mapgui/mappainterairspace.h"
#include "mapgui/mappainterils.h"
#include "mapgui/mappaintermark.h"
#include "mapgui/mappainternav.h"
#include "mapgui/mappainterroute.h"
#include "mapgui/mappainteruser.h"
#include "mapgui/mapscale.h"
#include "userdata/userdatacontroller.h"
#include "route/route.h"
#include "geo/calculations.h"
#include "options/optiondata.h"

#include <QElapsedTimer>

#include <marble/GeoPainter.h>

using namespace Marble;
using namespace atools::geo;

MapPaintLayer::MapPaintLayer(MapWidget *widget, MapQuery *mapQueries)
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

  // Default for visible object types
  objectTypes = map::MapObjectTypes(map::AIRPORT | map::VOR | map::NDB | map::AP_ILS | map::MARKER | map::WAYPOINT);
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

  delete layers;
  delete mapScale;
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

  // Create a list of map layers that define content for each zoom distance
  layers = new MapLayerSettings();

  // Create a default layer with all features enabled
  MapLayer defLayer = MapLayer(0).airport().approach().approachTextAndDetail().airportName().airportIdent().
                      airportSoft().airportNoRating().airportOverviewRunway().airportSource(layer::ALL).

                      vor().ndb().waypoint().marker().ils().airway().

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
  append(defLayer.clone(0.2f).airportDiagram().airportDiagramDetail().airportDiagramDetail2().airportDiagramDetail3().
         airportSymbolSize(20).airportInfo().
         waypointSymbolSize(14).waypointName().
         vorSymbolSize(24).vorIdent().vorInfo().vorLarge().
         ndbSymbolSize(24).ndbIdent().ndbInfo().
         ilsIdent().ilsInfo().
         airwayIdent().airwayInfo().airwayWaypoint().
         userpoint().userpointInfo().userpoinSymbolSize(24).userpointMaxTextLength(30).
         markerSymbolSize(24).markerInfo().
         airportMaxTextLength(30)).

  append(defLayer.clone(0.3f).airportDiagram().airportDiagramDetail().airportDiagramDetail2().
         airportSymbolSize(20).airportInfo().
         waypointSymbolSize(14).waypointName().
         vorSymbolSize(24).vorIdent().vorInfo().vorLarge().
         ndbSymbolSize(24).ndbIdent().ndbInfo().
         ilsIdent().ilsInfo().
         airwayIdent().airwayInfo().airwayWaypoint().
         userpoint().userpointInfo().userpoinSymbolSize(24).userpointMaxTextLength(30).
         markerSymbolSize(24).markerInfo().
         airportMaxTextLength(30)).

  // airport diagram, large VOR, NDB, ILS, waypoint, airway, marker
  append(defLayer.clone(1.f).airportDiagram().airportDiagramDetail().
         airportSymbolSize(20).airportInfo().
         aiAircraftGroundText(false).
         waypointSymbolSize(14).waypointName().
         vorSymbolSize(24).vorIdent().vorInfo().vorLarge().
         ndbSymbolSize(24).ndbIdent().ndbInfo().
         ilsIdent().ilsInfo().
         airwayIdent().airwayInfo().airwayWaypoint().
         userpoint().userpointInfo().userpoinSymbolSize(24).userpointMaxTextLength(30).
         markerSymbolSize(24).markerInfo().
         airportMaxTextLength(30)).

  // airport diagram, large VOR, NDB, ILS, waypoint, airway, marker
  append(defLayer.clone(5.f).airportDiagram().airportSymbolSize(20).airportInfo().
         waypointSymbolSize(10).waypointName().
         aiAircraftGroundText(false).
         vorSymbolSize(24).vorIdent().vorInfo().vorLarge().
         ndbSymbolSize(24).ndbIdent().ndbInfo().
         ilsIdent().ilsInfo().
         airwayIdent().airwayInfo().airwayWaypoint().
         userpoint().userpointInfo().userpoinSymbolSize(24).userpointMaxTextLength(20).
         markerSymbolSize(24).markerInfo().
         airportMaxTextLength(30)).

  // airport, large VOR, NDB, ILS, waypoint, airway, marker
  append(defLayer.clone(10.f).airportSymbolSize(18).airportInfo().
         waypointSymbolSize(8).waypointName().
         aiAircraftGround(false).aiAircraftGroundText(false).
         vorSymbolSize(22).vorIdent().vorInfo().vorLarge().
         ndbSymbolSize(22).ndbIdent().ndbInfo().
         ilsIdent().ilsInfo().
         airwayIdent().airwayWaypoint().
         userpoint().userpointInfo().userpoinSymbolSize(24).userpointMaxTextLength(20).
         markerSymbolSize(24).
         airportMaxTextLength(20)).

  // airport, large VOR, NDB, ILS, waypoint, airway, marker
  append(defLayer.clone(25.f).airportSymbolSize(18).airportInfo().
         waypointSymbolSize(8).
         aiAircraftGround(false).aiAircraftGroundText(false).
         vorSymbolSize(22).vorIdent().vorInfo().vorLarge().
         ndbSymbolSize(22).ndbIdent().ndbInfo().
         ilsIdent().ilsInfo().
         airwayIdent().airwayWaypoint().
         userpoint().userpointInfo().userpoinSymbolSize(24).userpointMaxTextLength(20).
         markerSymbolSize(24).
         airportMaxTextLength(20)).

  // airport, large VOR, NDB, ILS, airway
  append(defLayer.clone(50.f).airportSymbolSize(18).airportInfo().
         waypoint(false).
         aiAircraftGround(false).aiShipSmall(false).aiAircraftGroundText(false).aiAircraftText(false).
         vorSymbolSize(20).vorIdent().vorInfo().vorLarge().
         ndbSymbolSize(20).ndbIdent().ndbInfo().
         airwayIdent().airwayWaypoint().
         userpoint().userpointInfo().userpoinSymbolSize(24).userpointMaxTextLength(10).
         marker(false).
         airportMaxTextLength(16)).

  // airport, VOR, NDB, ILS, airway
  append(defLayer.clone(100.f).airportSymbolSize(12).
         airportOverviewRunway(false).
         waypoint(false).
         aiAircraftGround(false).aiShipSmall(false).aiAircraftGroundText(false).aiAircraftText(false).
         vorSymbolSize(16).vorIdent().
         ndbSymbolSize(16).ndbIdent().
         airwayWaypoint().
         userpoint().userpointInfo().userpoinSymbolSize(24).userpointMaxTextLength(10).
         marker(false).
         airportMaxTextLength(16)).

  // airport, VOR, NDB, airway
  append(defLayer.clone(150.f).airportSymbolSize(10).minRunwayLength(2500).
         airportOverviewRunway(false).airportName(false).
         approachTextAndDetail(false).
         aiAircraftGround(false).aiShipSmall(false).aiAircraftGroundText(false).aiAircraftText(false).
         waypoint(false).
         vorSymbolSize(12).
         ndbSymbolSize(12).
         airwayWaypoint().
         userpoint().userpointInfo().userpoinSymbolSize(22).userpointMaxTextLength(8).
         marker(false).ils(false).
         airportMaxTextLength(16)).

  // airport > 4000, VOR
  append(defLayer.clone(200.f).airportSymbolSize(10).minRunwayLength(layer::MAX_MEDIUM_RUNWAY_FT).
         airportOverviewRunway(false).airportName(false).airportSource(layer::MEDIUM).
         approachTextAndDetail(false).
         aiAircraftGround(false).aiShipSmall(false).aiAircraftGroundText(false).aiAircraftText(false).
         onlineAircraftText(false).
         airwayWaypoint().
         vorSymbolSize(8).ndb(false).waypoint(false).marker(false).ils(false).
         userpoint().userpointInfo().userpoinSymbolSize(16).userpointMaxTextLength(8).
         airportMaxTextLength(16)).

  // airport > 4000
  append(defLayer.clone(300.f).airportSymbolSize(10).minRunwayLength(layer::MAX_MEDIUM_RUNWAY_FT).
         airportOverviewRunway(false).airportName(false).airportSource(layer::MEDIUM).
         approachTextAndDetail(false).
         aiAircraftGround(false).aiAircraftSmall(false).aiShipSmall(false).
         aiAircraftGroundText(false).aiAircraftText(false).
         onlineAircraftText(false).
         ndb(false).waypoint(false).marker(false).ils(false).
         airportRouteInfo(false).waypointRouteName(false).
         userpoint().userpointInfo(false).userpoinSymbolSize(16).
         airportMaxTextLength(16)).

  // airport > 8000
  append(defLayer.clone(750.f).airportSymbolSize(10).minRunwayLength(layer::MAX_LARGE_RUNWAY_FT).
         airportOverviewRunway(false).airportName(false).airportSource(layer::LARGE).
         approachTextAndDetail(false).
         aiAircraftGround(false).aiAircraftSmall(false).aiShipLarge(false).aiShipSmall(false).
         aiAircraftGroundText(false).aiAircraftText(false).
         onlineAircraftText(false).
         airspaceOther(false).airspaceRestricted(false).airspaceSpecial(false).
         vor(false).ndb(false).waypoint(false).marker(false).ils(false).airway(false).
         airportRouteInfo(false).vorRouteInfo(false).ndbRouteInfo(false).waypointRouteName(false).
         userpoint().userpointInfo(false).userpoinSymbolSize(12).
         airportMaxTextLength(16)).

  // airport > 8000
  append(defLayer.clone(1200.f).airportSymbolSize(10).minRunwayLength(layer::MAX_LARGE_RUNWAY_FT).
         airportOverviewRunway(false).airportName(false).airportSource(layer::LARGE).
         approachTextAndDetail(false).
         aiAircraftGround(false).aiAircraftLarge(false).aiAircraftSmall(false).aiShipLarge(false).aiShipSmall(false).
         aiAircraftGroundText(false).aiAircraftText(false).
         onlineAircraftText(false).
         airspaceFir(false).airspaceOther(false).airspaceRestricted(false).airspaceSpecial(false).
         airspaceIcao(false).
         vor(false).ndb(false).waypoint(false).marker(false).ils(false).airway(false).
         airportRouteInfo(false).vorRouteInfo(false).ndbRouteInfo(false).waypointRouteName(false).
         userpoint().userpointInfo(false).
         airportMaxTextLength(16)).

  // Display only points for airports until the cutoff limit
  // airport > 8000
  append(defLayer.clone(layer::DISTANCE_CUT_OFF_LIMIT).airportSymbolSize(5).
         minRunwayLength(layer::MAX_LARGE_RUNWAY_FT).
         airportOverviewRunway(false).airportName(false).airportIdent(false).airportSource(layer::LARGE).
         approach(false).approachTextAndDetail(false).
         aiAircraftGround(false).aiAircraftLarge(false).aiAircraftSmall(false).aiShipLarge(false).aiShipSmall(false).
         aiAircraftGroundText(false).aiAircraftText(false).
         onlineAircraft(false).onlineAircraftText(false).
         airspaceCenter(false).airspaceFir(false).airspaceOther(false).
         airspaceRestricted(false).airspaceSpecial(false).airspaceIcao(false).
         vor(false).ndb(false).waypoint(false).marker(false).ils(false).airway(false).
         airportRouteInfo(false).vorRouteInfo(false).ndbRouteInfo(false).waypointRouteName(false).
         userpoint().userpointInfo(false).
         airportMaxTextLength(16)).

  // Make sure that there is always an layer
  append(defLayer.clone(100000.f).airportSymbolSize(5).minRunwayLength(layer::MAX_LARGE_RUNWAY_FT).
         airportOverviewRunway(false).airportName(false).airportIdent(false).airportSource(layer::LARGE).
         approach(false).approachTextAndDetail(false).
         aiAircraftGround(false).aiAircraftLarge(false).aiAircraftSmall(false).aiShipLarge(false).aiShipSmall(false).
         aiAircraftGroundText(false).aiAircraftText(false).
         onlineAircraft(false).onlineAircraftText(false).
         airspaceCenter(false).airspaceFir(false).airspaceOther(false).
         airspaceRestricted(false).airspaceSpecial(false).airspaceIcao(false).
         airport(false).vor(false).ndb(false).waypoint(false).marker(false).ils(false).airway(false).
         airportRouteInfo(false).vorRouteInfo(false).ndbRouteInfo(false).waypointRouteName(false).
         userpoint(false).userpointInfo(false).
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

bool MapPaintLayer::render(GeoPainter *painter, ViewportParams *viewport,
                           const QString& renderPos, GeoSceneLayer *layer)
{
  Q_UNUSED(renderPos);
  Q_UNUSED(layer);

  if(!databaseLoadStatus)
  {
    // Update map scale for screen distance approximation
    mapScale->update(viewport, mapWidget->distance());

    // What to draw while scrolling or zooming map
    opts::MapScrollDetail mapScrollDetail = OptionData::instance().getMapScrollDetail();

    // Check if no painting wanted during scroll
    if(!(mapScrollDetail == opts::NONE && mapWidget->viewContext() == Marble::Animation))
    {
      updateLayers();

      PaintContext context;
      context.mapLayer = mapLayer;
      context.mapLayerEffective = mapLayerEffective;
      context.painter = painter;
      context.viewport = viewport;
      context.objectTypes = objectTypes;
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
      context.symbolSizeNavaid = od.getDisplaySymbolSizeNavaid() / 100.f;
      context.textSizeAircraftAi = od.getDisplayTextSizeAircraftAi() / 100.f;
      context.textSizeAircraftUser = od.getDisplayTextSizeAircraftUser() / 100.f;
      context.textSizeAirport = od.getDisplayTextSizeAirport() / 100.f;
      context.textSizeFlightplan = od.getDisplayTextSizeFlightplan() / 100.f;
      context.textSizeNavaid = od.getDisplayTextSizeNavaid() / 100.f;
      context.thicknessFlightplan = od.getDisplayThicknessFlightplan() / 100.f;
      context.thicknessTrail = od.getDisplayThicknessTrail() / 100.f;
      context.thicknessRangeDistance = od.getDisplayThicknessRangeDistance() / 100.f;
      context.thicknessCompassRose = od.getDisplayThicknessCompassRose() / 100.f;

      context.dispOpts = od.getDisplayOptions();
      context.flags = od.getFlags();
      context.flags2 = od.getFlags2();

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

      // if(!context.isOverflow()) always paint route even if number of objets is too large
      mapPainterRoute->render(&context);

      // if(!context.isOverflow())
      mapPainterMark->render(&context);

      mapPainterAircraft->render(&context);

      if(context.isOverflow())
        overflow = PaintContext::MAX_OBJECT_COUNT;
      else
        overflow = 0;
    }

    // Dim the map by drawing a semi-transparent black rectangle
    if(OptionData::instance().isGuiStyleDark())
    {
      int dim = OptionData::instance().getGuiStyleMapDimming();
      QColor col = QColor::fromRgb(0, 0, 0, 255 - (255 * dim / 100));
      painter->fillRect(QRect(0, 0, painter->device()->width(), painter->device()->height()), col);
    }

  }
  return true;
}
