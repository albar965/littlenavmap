/*****************************************************************************
* Copyright 2015-2016 Alexander Barthel albar965@mailbox.org
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

#include "connect/connectclient.h"
#include "gui/mainwindow.h"
#include "mapgui/mapwidget.h"
#include "mapgui/maplayersettings.h"
#include "mapgui/mappainteraircraft.h"
#include "mapgui/mappainterairport.h"
#include "mapgui/mappainterils.h"
#include "mapgui/mappaintermark.h"
#include "mapgui/mappainternav.h"
#include "mapgui/mappainterroute.h"
#include "mapgui/mapscale.h"
#include "route/routecontroller.h"
#include "options/optiondata.h"

#include <QElapsedTimer>

#include <marble/GeoPainter.h>

using namespace Marble;
using namespace atools::geo;

MapPaintLayer::MapPaintLayer(MapWidget *widget, MapQuery *mapQueries)
  : mapQuery(mapQueries), mapWidget(widget)
{
  initLayers();

  mapScale = new MapScale();
  mapPainterNav = new MapPainterNav(mapWidget, mapQuery, mapScale, false);
  mapPainterIls = new MapPainterIls(mapWidget, mapQuery, mapScale, false);
  mapPainterAirport = new MapPainterAirport(mapWidget, mapQuery, mapScale, false);
  mapPainterMark = new MapPainterMark(mapWidget, mapQuery, mapScale, false);
  mapPainterRoute = new MapPainterRoute(mapWidget, mapQuery, mapScale,
                                        mapWidget->getRouteController(), false);
  mapPainterAircraft = new MapPainterAircraft(mapWidget, mapQuery, mapScale, false);

  objectTypes = maptypes::MapObjectTypes(
    maptypes::AIRPORT | maptypes::VOR | maptypes::NDB | maptypes::AP_ILS | maptypes::MARKER |
    maptypes::WAYPOINT);
}

MapPaintLayer::~MapPaintLayer()
{
  delete mapPainterIls;
  delete mapPainterNav;
  delete mapPainterAirport;
  delete mapPainterMark;
  delete mapPainterRoute;

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

void MapPaintLayer::setShowMapFeatures(maptypes::MapObjectTypes type, bool show)
{
  if(show)
    objectTypes |= type;
  else
    objectTypes &= ~type;
}

void MapPaintLayer::setDetailFactor(int factor)
{
  detailFactor = factor;
  updateLayers();
}

void MapPaintLayer::routeChanged()
{
  qDebug() << "route changed";
  const RouteMapObjectList& routeMapObjects = mapWidget->getRouteController()->getRouteMapObjects();

  forcePaint.clear();
  for(const RouteMapObject& obj : routeMapObjects)
    forcePaint.insert({obj.getId(), obj.getMapObjectType()});
}

void MapPaintLayer::initLayers()
{
  if(layers != nullptr)
    delete layers;

  layers = new MapLayerSettings();

  MapLayer defLayer = MapLayer(0).airport().airportName().airportIdent().
                      airportSoft().airportNoRating().airportOverviewRunway().airportSource(layer::ALL).

                      vor().ndb().waypoint().marker().ils().airway().

                      vorRouteIdent().vorRouteInfo().ndbRouteIdent().ndbRouteInfo().waypointRouteName().
                      airportRouteInfo();

  layers->
  append(defLayer.clone(0.3f).airportDiagram().airportDiagramDetail().airportDiagramDetail2().
         airportSymbolSize(20).airportInfo().
         waypointSymbolSize(14).waypointName().
         vorSymbolSize(24).vorIdent().vorInfo().vorLarge().
         ndbSymbolSize(24).ndbIdent().ndbInfo().
         ilsIdent().ilsInfo().
         airwayIdent().airwayInfo().
         markerSymbolSize(24).markerInfo()).

  append(defLayer.clone(1.f).airportDiagram().airportDiagramDetail().airportSymbolSize(20).airportInfo().
         waypointSymbolSize(14).waypointName().
         vorSymbolSize(24).vorIdent().vorInfo().vorLarge().
         ndbSymbolSize(24).ndbIdent().ndbInfo().
         ilsIdent().ilsInfo().
         airwayIdent().airwayInfo().
         markerSymbolSize(24).markerInfo()).

  append(defLayer.clone(5.f).airportDiagram().airportSymbolSize(20).airportInfo().
         waypointSymbolSize(10).waypointName().
         vorSymbolSize(24).vorIdent().vorInfo().vorLarge().
         ndbSymbolSize(24).ndbIdent().ndbInfo().
         ilsIdent().ilsInfo().
         airwayIdent().airwayInfo().
         markerSymbolSize(24).markerInfo()).

  append(defLayer.clone(10.f).airportSymbolSize(18).airportInfo().
         waypointSymbolSize(8).waypointName().
         vorSymbolSize(22).vorIdent().vorInfo().vorLarge().
         ndbSymbolSize(22).ndbIdent().ndbInfo().
         ilsIdent().ilsInfo().
         airwayIdent().
         markerSymbolSize(24)).

  append(defLayer.clone(25.f).airportSymbolSize(18).airportInfo().
         waypointSymbolSize(8).
         vorSymbolSize(22).vorIdent().vorInfo().vorLarge().
         ndbSymbolSize(22).ndbIdent().ndbInfo().
         ilsIdent().ilsInfo().
         airwayIdent().
         markerSymbolSize(24)).

  append(defLayer.clone(50.f).airportSymbolSize(18).airportInfo().
         waypoint(false).
         vorSymbolSize(20).vorIdent().vorInfo().vorLarge().
         ndbSymbolSize(20).ndbIdent().ndbInfo().
         airwayIdent().
         marker(false)).

  append(defLayer.clone(100.f).airportSymbolSize(12).
         airportOverviewRunway(false).waypoint(false).
         vorSymbolSize(16).vorIdent().
         ndbSymbolSize(16).ndbIdent().
         marker(false)).

  append(defLayer.clone(150.f).airportSymbolSize(10).minRunwayLength(2500).
         airportOverviewRunway(false).airportName(false).
         waypoint(false).
         vorSymbolSize(12).
         ndbSymbolSize(12).
         marker(false).ils(false)).

  append(defLayer.clone(200.f).airportSymbolSize(10).
         airportOverviewRunway(false).airportName(false).airportSource(layer::MEDIUM).
         vorSymbolSize(8).ndb(false).waypoint(false).marker(false).ils(false).airway(false)).

  append(defLayer.clone(300.f).airportSymbolSize(10).
         airportOverviewRunway(false).airportName(false).airportSource(layer::MEDIUM).
         vor(false).ndb(false).waypoint(false).marker(false).ils(false).airway(false).
         airportRouteInfo(false).waypointRouteName(false)).

  append(defLayer.clone(1200.f).airportSymbolSize(10).
         airportOverviewRunway(false).airportName(false).airportSource(layer::LARGE).
         vor(false).ndb(false).waypoint(false).marker(false).ils(false).airway(false).
         airportRouteInfo(false).vorRouteInfo(false).ndbRouteInfo(false).waypointRouteName(false)).

  append(defLayer.clone(DISTANCE_CUT_OFF_LIMIT).airportSymbolSize(5).
         airportOverviewRunway(false).airportName(false).airportIdent(false).airportSource(layer::LARGE).
         vor(false).ndb(false).waypoint(false).marker(false).ils(false).airway(false).
         airportRouteInfo(false).vorRouteInfo(false).ndbRouteInfo(false).waypointRouteName(false)).

  // Make sure that there is always an layer
  append(defLayer.clone(100000.f).airportSymbolSize(5).
         airportOverviewRunway(false).airportName(false).airportIdent(false).airportSource(layer::LARGE).
         airport(false).vor(false).ndb(false).waypoint(false).marker(false).ils(false).airway(false).
         airportRouteInfo(false).vorRouteInfo(false).ndbRouteInfo(false).waypointRouteName(false));

  layers->finishAppend();
  qDebug() << *layers;
}

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
    mapScale->update(viewport, mapWidget->distance());

    opts::MapScrollDetail mapScrollDetail = OptionData::instance().getMapScrollDetail();

    // Check if no painting wanted during scroll
    if(!(mapScrollDetail == opts::NONE && mapWidget->viewContext() == Marble::Animation))
    {

      QElapsedTimer t;
      t.start();

      updateLayers();

      PaintContext context;
      context.mapLayer = mapLayer;
      context.mapLayerEffective = mapLayerEffective;
      context.painter = painter;
      context.viewport = viewport;
      context.objectTypes = objectTypes;
      context.viewContext = mapWidget->viewContext();
      context.drawFast = mapScrollDetail == opts::FULL ? false : mapWidget->viewContext() ==
                         Marble::Animation;
      context.mapScrollDetail = mapScrollDetail;

      context.defaultFont = painter->font();
      context.defaultFont.setBold(true);
      context.defaultFontScaled = context.defaultFont;
      context.defaultFontScaled.setPointSizeF(context.defaultFontScaled.pointSizeF() *
                                              OptionData::instance().getMapTextSize() / 100.f);
      painter->setFont(context.defaultFontScaled);

      const GeoDataLatLonAltBox& box = viewport->viewLatLonAltBox();
      context.viewportRect = atools::geo::Rect(box.west(GeoDataCoordinates::Degree),
                                               box.north(GeoDataCoordinates::Degree),
                                               box.east(GeoDataCoordinates::Degree),
                                               box.south(GeoDataCoordinates::Degree));

      if(objectTypes.testFlag(maptypes::ROUTE) && !forcePaint.isEmpty())
        context.forcePaintObjects = &forcePaint;
      else
        context.forcePaintObjects = nullptr;

      context.symbolScale = OptionData::instance().getMapSymbolSize() / 100.f;

      if(mapWidget->distance() < DISTANCE_CUT_OFF_LIMIT)
      {
        if(context.mapLayerEffective->isAirportDiagram())
        {
          mapPainterIls->render(&context);
          mapPainterAirport->render(&context);
          mapPainterNav->render(&context);
        }
        else
        {
          mapPainterIls->render(&context);
          mapPainterNav->render(&context);
          mapPainterAirport->render(&context);
        }
      }
      mapPainterRoute->render(&context);
      mapPainterMark->render(&context);

      if(mapWidget->getParentWindow()->getConnectClient()->isConnected())
        mapPainterAircraft->render(&context);

      // if(navMapWidget->viewContext() == Marble::Still)
      // qDebug() << "Time for rendering" << t.elapsed() << "ms";
    }
  }

  return true;
}
