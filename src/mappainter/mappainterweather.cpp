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

#include "mappainter/mappainterweather.h"

#include "common/symbolpainter.h"
#include "mapgui/mapscale.h"
#include "mapgui/maplayer.h"
#include "query/mapquery.h"
#include "util/paintercontextsaver.h"
#include "app/navapp.h"
#include "route/route.h"
#include "fs/weather/metar.h"
#include "weather/weatherreporter.h"

#include <marble/GeoPainter.h>
#include <marble/ViewportParams.h>

using namespace Marble;
using namespace atools::geo;
using namespace map;

MapPainterWeather::MapPainterWeather(MapPaintWidget *mapWidget, MapScale *mapScale, PaintContext *paintContext)
  : MapPainter(mapWidget, mapScale, paintContext)
{
}

MapPainterWeather::~MapPainterWeather()
{
}

void MapPainterWeather::render()
{
  bool drawWeather = context->objectDisplayTypes.testFlag(map::AIRPORT_WEATHER) && context->mapLayer->isAirportWeather();

  if(!drawWeather)
    return;

  atools::util::PainterContextSaver saver(context->painter);
  Q_UNUSED(saver)

  // Get airports from cache/database for the bounding rectangle and add them to the map
  bool overflow = false;

  const GeoDataLatLonAltBox& curBox = context->viewport->viewLatLonAltBox();
  const QList<MapAirport> *airportCache =
    mapQuery->getAirports(curBox, context->mapLayer, context->lazyUpdate, context->objectTypes, overflow);
  context->setQueryOverflow(overflow);

  // Collect all airports that are visible from cache ======================================
  QList<PaintAirportType> visibleAirportWeather;
  float x, y;
  bool hidden, visibleOnMap;
  if(airportCache != nullptr)
  {
    for(const MapAirport& airport : *airportCache)
    {
      visibleOnMap = wToS(airport.position, x, y, scale->getScreeenSizeForRect(airport.bounding), &hidden);

      if(!hidden && visibleOnMap)
        visibleAirportWeather.append(PaintAirportType(airport, x, y));
    }
  }

  // Collect all airports that are visible from route ======================================
  if(context->objectDisplayTypes.testFlag(map::FLIGHTPLAN))
  {
    if(context->route->getDepartureAirportLeg().isAirport())
    {
      const MapAirport& airport = context->route->getDepartureAirportLeg().getAirport();
      visibleOnMap = wToS(airport.position, x, y, scale->getScreeenSizeForRect(airport.bounding), &hidden);
      if(!hidden && visibleOnMap)
        visibleAirportWeather.append(PaintAirportType(airport, x, y));
    }

    if(context->route->getDestinationAirportLeg().isAirport())
    {
      const MapAirport& airport = context->route->getDestinationAirportLeg().getAirport();
      visibleOnMap = wToS(airport.position, x, y, scale->getScreeenSizeForRect(airport.bounding), &hidden);
      if(!hidden && visibleOnMap)
        visibleAirportWeather.append(PaintAirportType(airport, x, y));
    }

    QVector<MapAirport> alternates = context->route->getAlternateAirports();
    for(const map::MapAirport& airport : qAsConst(alternates))
    {
      visibleOnMap = wToS(airport.position, x, y, scale->getScreeenSizeForRect(airport.bounding), &hidden);
      if(!hidden && visibleOnMap)
        visibleAirportWeather.append(PaintAirportType(airport, x, y));
    }
  }

  // ================================
  // Limit weather display to the 10 most important/biggest airports if connected via network or SimConnect
  if((NavApp::isNetworkConnect() || NavApp::isSimConnect()) && NavApp::isConnectedActive() &&
     context->weatherSource == map::WEATHER_SOURCE_SIMULATOR)
  {
    visibleAirportWeather.erase(std::remove_if(visibleAirportWeather.begin(), visibleAirportWeather.end(),
                                               [](const PaintAirportType& ap) -> bool
    {
      return ap.airport->emptyDraw() || !ap.airport->hard() || ap.airport->closed();
    }), visibleAirportWeather.end());

    std::sort(visibleAirportWeather.begin(), visibleAirportWeather.end(),
              [](const PaintAirportType& ap1, const PaintAirportType& ap2) {
      return ap1.airport->longestRunwayLength > ap2.airport->longestRunwayLength;
    });
    visibleAirportWeather = visibleAirportWeather.mid(0, 10);
  }

  // Sort by airport display order
  using namespace std::placeholders;
  std::sort(visibleAirportWeather.begin(), visibleAirportWeather.end(), std::bind(&MapPainter::sortAirportFunction, this, _1, _2));

  WeatherReporter *reporter = NavApp::getWeatherReporter();
  for(const PaintAirportType& airportWeather: qAsConst(visibleAirportWeather))
  {
    atools::fs::weather::Metar metar =
      reporter->getAirportWeather(*airportWeather.airport, true /* stationOnly */);

    if(metar.isValid())
      drawAirportWeather(metar, static_cast<float>(airportWeather.point.x()), static_cast<float>(airportWeather.point.y()));
  }
}

void MapPainterWeather::drawAirportWeather(const atools::fs::weather::Metar& metar, float x, float y)
{
  float size = context->szF(context->symbolSizeAirportWeather, context->mapLayer->getAirportSymbolSize());
  bool windBarbs = context->mapLayer->isAirportWeatherDetails();

  symbolPainter->drawAirportWeather(context->painter, metar, x - size * 4.f / 5.f, y - size * 4.f / 5.f, size,
                                    true /* Wind pointer*/, windBarbs, context->drawFast);
}
