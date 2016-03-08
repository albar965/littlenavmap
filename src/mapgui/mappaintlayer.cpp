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
#include "mapgui/navmapwidget.h"
#include "maplayersettings.h"
#include "mappainterairport.h"
#include "mappaintermark.h"
#include "mapquery.h"
#include "mapscale.h"
#include "geo/calculations.h"
#include <cmath>
#include <marble/MarbleModel.h>
#include <marble/GeoDataPlacemark.h>
#include <marble/GeoDataDocument.h>
#include <marble/GeoDataTreeModel.h>
#include <marble/MarbleWidgetPopupMenu.h>
#include <marble/MarbleWidgetInputHandler.h>
#include <marble/GeoDataStyle.h>
#include <marble/GeoDataIconStyle.h>
#include <marble/GeoDataCoordinates.h>
#include <marble/GeoPainter.h>
#include <marble/LayerInterface.h>
#include <marble/ViewportParams.h>
#include <marble/MarbleLocale.h>
#include <marble/MarbleWidget.h>
#include <marble/ViewportParams.h>

#include <QContextMenuEvent>
#include <QElapsedTimer>

using namespace Marble;
using namespace atools::geo;

MapPaintLayer::MapPaintLayer(NavMapWidget *widget, MapQuery *mapQueries)
  : mapQuery(mapQueries), navMapWidget(widget)
{
  delete layers;

  initLayers();

  mapScale = new MapScale();
  mapPainterAirport = new MapPainterAirport(navMapWidget, mapQuery, mapScale);
  mapPainters.append(mapPainterAirport);
  mapPainters.append(new MapPainterMark(navMapWidget, mapQuery, mapScale));
}

MapPaintLayer::~MapPaintLayer()
{
  qDeleteAll(mapPainters);
  mapPainters.clear();
  delete layers;
  delete mapScale;
}

void MapPaintLayer::preDatabaseLoad()
{
  databaseLoadStatus = true;
}

void MapPaintLayer::postDatabaseLoad()
{
  databaseLoadStatus = true;
}

MapAirport MapPaintLayer::getAirportAtPos(int xs, int ys)
{
  return mapPainterAirport->getAirportAtPos(xs, ys);
}

void MapPaintLayer::initLayers()
{
  if(layers != nullptr)
    delete layers;

  layers = new MapLayerSettings();

  MapLayer defApLayer = MapLayer(0).airports().airportName().airportIdent().
                        airportSoft().airportNoRating().airportOverviewRunway().airportSource(layer::ALL);
  layers->
  append(defApLayer.clone(0.7f).airportDiagram().airportDiagramDetail().airportSymbolSize(20).airportInfo()).
  append(defApLayer.clone(5.f).airportDiagram().airportSymbolSize(20).airportInfo()).

  append(defApLayer.clone(50.f).airportSymbolSize(18).airportInfo()).

  append(defApLayer.clone(100.f).airportSymbolSize(14)).

  append(defApLayer.clone(150.f).airportSymbolSize(10).minRunwayLength(2500).
         airportOverviewRunway(false).airportName(false)).

  append(defApLayer.clone(300.f).airportSymbolSize(10).
         airportOverviewRunway(false).airportName(false).airportSource(layer::MEDIUM)).

  append(defApLayer.clone(1200.f).airportSymbolSize(10).
         airportOverviewRunway(false).airportName(false).airportSource(layer::LARGE));

  layers->finishAppend();
  qDebug() << *layers;
}

bool MapPaintLayer::render(GeoPainter *painter, ViewportParams *viewport,
                           const QString& renderPos, GeoSceneLayer *layer)
{
  Q_UNUSED(renderPos);
  Q_UNUSED(layer);

  if(!databaseLoadStatus)
  {
    mapScale->update(viewport, navMapWidget->distance());

    const MapLayer *mapLayer = layers->getLayer(static_cast<float>(navMapWidget->distance()));

    if(mapLayer != nullptr)
      for(MapPainter *mapPainter : mapPainters)
        mapPainter->paint(mapLayer, painter, viewport);
  }

  return true;
}
