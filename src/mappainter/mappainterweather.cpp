/*****************************************************************************
* Copyright 2015-2024 Alexander Barthel alex@littlenavmap.org
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
#include "fs/weather/metarparser.h"
#include "fs/weather/metar.h"
#include "mapgui/mapscale.h"
#include "mapgui/maplayer.h"
#include "query/mapquery.h"
#include "query/querymanager.h"
#include "util/paintercontextsaver.h"
#include "app/navapp.h"
#include "route/route.h"
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
    queries->getMapQuery()->getAirports(curBox, context->mapLayer, context->lazyUpdate, context->objectTypes, overflow);
  context->setQueryOverflow(overflow);

  // Collect all airports that are visible from cache ======================================
  QList<AirportPaintData> visibleAirportWeather;
  float x, y;
  bool hidden, visibleOnMap;
  if(airportCache != nullptr)
  {
    for(const MapAirport& airport : *airportCache)
    {
      visibleOnMap = wToS(airport.position, x, y, scale->getScreeenSizeForRect(airport.bounding), &hidden);

      if(!hidden && visibleOnMap)
        visibleAirportWeather.append(AirportPaintData(airport, x, y));
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
        visibleAirportWeather.append(AirportPaintData(airport, x, y));
    }

    if(context->route->getDestinationAirportLeg().isAirport())
    {
      const MapAirport& airport = context->route->getDestinationAirportLeg().getAirport();
      visibleOnMap = wToS(airport.position, x, y, scale->getScreeenSizeForRect(airport.bounding), &hidden);
      if(!hidden && visibleOnMap)
        visibleAirportWeather.append(AirportPaintData(airport, x, y));
    }

    QVector<MapAirport> alternates = context->route->getAlternateAirports();
    for(const map::MapAirport& airport : qAsConst(alternates))
    {
      visibleOnMap = wToS(airport.position, x, y, scale->getScreeenSizeForRect(airport.bounding), &hidden);
      if(!hidden && visibleOnMap)
        visibleAirportWeather.append(AirportPaintData(airport, x, y));
    }
  }

  // ================================
  // Limit weather display to the 10 most important/biggest airports if connected via network or SimConnect
  if((NavApp::isNetworkConnect() || NavApp::isSimConnect()) && NavApp::isConnectedActive() &&
     context->weatherSource == map::WEATHER_SOURCE_SIMULATOR)
  {
    visibleAirportWeather.erase(std::remove_if(visibleAirportWeather.begin(), visibleAirportWeather.end(),
                                               [](const AirportPaintData& airportPaintData) -> bool
    {
      return airportPaintData.getAirport().emptyDraw() || !airportPaintData.getAirport().hard() || airportPaintData.getAirport().closed();
    }), visibleAirportWeather.end());

    std::sort(visibleAirportWeather.begin(), visibleAirportWeather.end(),
              [](const AirportPaintData& airportPaintData1, const AirportPaintData& airportPaintData2) {
      return airportPaintData1.getAirport().longestRunwayLength > airportPaintData2.getAirport().longestRunwayLength;
    });
    visibleAirportWeather = visibleAirportWeather.mid(0, 10);
  }

  // Sort by airport display order
  using namespace std::placeholders;
  std::sort(visibleAirportWeather.begin(), visibleAirportWeather.end(), std::bind(&MapPainter::sortAirportFunction, this, _1, _2));

  WeatherReporter *reporter = NavApp::getWeatherReporter();
  for(const AirportPaintData& airportPaintData : qAsConst(visibleAirportWeather))
  {
    const atools::fs::weather::Metar& metar = reporter->getAirportWeather(airportPaintData.getAirport(), true /* stationOnly */);

    if(metar.hasStationMetar())
      drawAirportWeather(metar.getStation(), airportPaintData.getPoint());
  }
}

void MapPainterWeather::drawAirportWeather(const atools::fs::weather::MetarParser& metar, const QPointF& point)
{
  float size = context->szF(context->symbolSizeAirportWeather, context->mapLayer->getAirportSymbolSize());
  bool windBarbs = context->mapLayer->isAirportWeatherDetails();

  symbolPainter->drawAirportWeather(context->painter, metar,
                                    static_cast<float>(point.x() - size * 4. / 5.),
                                    static_cast<float>(point.y() - size * 4. / 5.), size,
                                    true /* Wind pointer*/, windBarbs, context->drawFast);
}
