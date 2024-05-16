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

#include "weathercontexthandler.h"

#include "app/navapp.h"
#include "common/maptypes.h"
#include "weather/weathercontext.h"
#include "connect/connectclient.h"
#include "options/optiondata.h"
#include "weather/weatherreporter.h"

using atools::fs::weather::Metar;

WeatherContextHandler::WeatherContextHandler(WeatherReporter *weatherReporterParam, ConnectClient *connectClientParam)
  : weatherReporter(weatherReporterParam), connectClient(connectClientParam)
{
  currentWeatherContext = new map::WeatherContext;
}

WeatherContextHandler::~WeatherContextHandler()
{
  delete currentWeatherContext;
}

void WeatherContextHandler::buildWeatherContextInfo(map::WeatherContext& weatherContext, const map::MapAirport& airport) const
{
  optsw::FlagsWeather flags = OptionData::instance().getFlagsWeather();
  buildWeatherContext(weatherContext, airport,
                      flags.testFlag(optsw::WEATHER_INFO_FS),
                      flags.testFlag(optsw::WEATHER_INFO_ACTIVESKY),
                      flags.testFlag(optsw::WEATHER_INFO_NOAA),
                      flags.testFlag(optsw::WEATHER_INFO_VATSIM),
                      flags.testFlag(optsw::WEATHER_INFO_IVAO));
}

void WeatherContextHandler::buildWeatherContextTooltip(map::WeatherContext& weatherContext, const map::MapAirport& airport) const
{
  optsw::FlagsWeather flags = OptionData::instance().getFlagsWeather();
  buildWeatherContext(weatherContext, airport,
                      flags.testFlag(optsw::WEATHER_TOOLTIP_FS),
                      flags.testFlag(optsw::WEATHER_TOOLTIP_ACTIVESKY),
                      flags.testFlag(optsw::WEATHER_TOOLTIP_NOAA),
                      flags.testFlag(optsw::WEATHER_TOOLTIP_VATSIM),
                      flags.testFlag(optsw::WEATHER_TOOLTIP_IVAO));
}

void WeatherContextHandler::buildWeatherContext(map::WeatherContext& weatherContext, const map::MapAirport& airport,
                                                bool simulator, bool activeSky, bool noaa, bool vatsim, bool ivao) const
{
  const QString& ident = airport.metarIdent();
  weatherContext.ident = ident;

  if(simulator)
  {
    if(atools::fs::FsPaths::isAnyXplane(NavApp::getCurrentSimulatorDb()))
      weatherContext.simMetar = weatherReporter->getXplaneMetar(ident, airport.position);
    else
      weatherContext.simMetar = connectClient->requestWeather(ident, airport.position, false /* station only */);
  }

  if(activeSky)
  {
    weatherContext.activeSkyMetar = weatherReporter->getActiveSkyMetar(ident, airport.position);
    fillActiveSkyType(weatherContext, ident);
  }

  if(noaa)
    weatherContext.noaaMetar = weatherReporter->getNoaaMetar(ident, airport.position);

  if(vatsim)
    weatherContext.vatsimMetar = weatherReporter->getVatsimMetar(ident, airport.position);

  if(ivao)
    weatherContext.ivaoMetar = weatherReporter->getIvaoMetar(ident, airport.position);
}

/* Fill active sky information into the weather context */
void WeatherContextHandler::fillActiveSkyType(map::WeatherContext& weatherContext, const QString& airportIdent) const
{
  weatherContext.asType = weatherReporter->getCurrentActiveSkyName();
  weatherContext.isAsDeparture = false;
  weatherContext.isAsDestination = false;

  if(weatherReporter->getActiveSkyDepartureIdent() == airportIdent)
    weatherContext.isAsDeparture = true;

  if(weatherReporter->getActiveSkyDestinationIdent() == airportIdent)
    weatherContext.isAsDestination = true;
}

bool WeatherContextHandler::buildWeatherContextInfoFull(map::WeatherContext& weatherContext, const map::MapAirport& airport)
{
  optsw::FlagsWeather flags = OptionData::instance().getFlagsWeather();
  const QString& ident = airport.metarIdent();
  bool newAirport = currentWeatherContext->ident != ident;

#ifdef DEBUG_INFORMATION_WEATHER
  qDebug() << Q_FUNC_INFO;
#endif

  currentWeatherContext->ident = ident;

  bool changed = false;
  if(flags & optsw::WEATHER_INFO_FS)
  {
    if(atools::fs::FsPaths::isAnyXplane(NavApp::getCurrentSimulatorDb()))
    {
      currentWeatherContext->simMetar = weatherReporter->getXplaneMetar(ident, airport.position);
      changed = true;
    }
    else if(NavApp::isConnected())
    {
      // FSX/P3D - Flight simulator fetched weather
      Metar metar = connectClient->requestWeather(ident, airport.position, false /* station only */);

      if(newAirport || (metar.hasAnyMetar() && metar != currentWeatherContext->simMetar))
      {
        // Airport has changed or METAR has changed
        currentWeatherContext->simMetar = metar;
        changed = true;
      }
    }
    else
    {
      if(currentWeatherContext->simMetar.hasAnyMetar())
      {
        // If there was a previous metar and the new one is empty we were being disconnected
        currentWeatherContext->simMetar.clearAll();
        changed = true;
      }
    }
  }

  if(flags & optsw::WEATHER_INFO_ACTIVESKY)
  {
    fillActiveSkyType(*currentWeatherContext, ident);

    Metar asMetar = weatherReporter->getActiveSkyMetar(ident, airport.position);
    if(newAirport || (asMetar.hasAnyMetar() && asMetar != currentWeatherContext->activeSkyMetar))
    {
      // Airport has changed or METAR has changed
      currentWeatherContext->activeSkyMetar = asMetar;
      changed = true;
    }
  }

  if(flags & optsw::WEATHER_INFO_NOAA)
  {
    Metar noaaMetar = weatherReporter->getNoaaMetar(ident, airport.position);
    if(newAirport || (noaaMetar.hasAnyMetar() && noaaMetar != currentWeatherContext->noaaMetar))
    {
      // Airport has changed or METAR has changed
      currentWeatherContext->noaaMetar = noaaMetar;
      changed = true;
    }
  }

  if(flags & optsw::WEATHER_INFO_VATSIM)
  {
    Metar vatsimMetar = weatherReporter->getVatsimMetar(ident, airport.position);
    if(newAirport || (vatsimMetar.hasAnyMetar() && vatsimMetar != currentWeatherContext->vatsimMetar))
    {
      // Airport has changed or METAR has changed
      currentWeatherContext->vatsimMetar = vatsimMetar;
      changed = true;
    }
  }

  if(flags & optsw::WEATHER_INFO_IVAO)
  {
    Metar ivaoMetar = weatherReporter->getIvaoMetar(ident, airport.position);
    if(newAirport || (ivaoMetar.hasAnyMetar() && ivaoMetar != currentWeatherContext->ivaoMetar))
    {
      // Airport has changed or METAR has changed
      currentWeatherContext->ivaoMetar = ivaoMetar;
      changed = true;
    }
  }

  weatherContext = *currentWeatherContext;

#ifdef DEBUG_INFORMATION_WEATHER
  if(changed)
    qDebug() << Q_FUNC_INFO << "changed" << changed << weatherContext;
#endif

  return changed;
}

void WeatherContextHandler::clearWeatherContext()
{
  qDebug() << Q_FUNC_INFO;

  // Clear all weather and fetch new
  *currentWeatherContext = map::WeatherContext();
}
