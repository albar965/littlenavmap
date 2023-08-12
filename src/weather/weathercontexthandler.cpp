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

#include "weathercontexthandler.h"

#include "app/navapp.h"
#include "common/maptypes.h"
#include "weather/weathercontext.h"
#include "connect/connectclient.h"
#include "options/optiondata.h"
#include "weather/weatherreporter.h"

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
      weatherContext.fsMetar = weatherReporter->getXplaneMetar(ident, airport.position);
    else
      weatherContext.fsMetar = connectClient->requestWeather(ident, airport.position, false /* station only */);
  }

  if(activeSky)
  {
    weatherContext.asMetar = weatherReporter->getActiveSkyMetar(ident);
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
      currentWeatherContext->fsMetar = weatherReporter->getXplaneMetar(ident, airport.position);
      changed = true;
    }
    else if(NavApp::isConnected())
    {
      // FSX/P3D - Flight simulator fetched weather
      atools::fs::weather::MetarResult metar =
        connectClient->requestWeather(ident, airport.position, false /* station only */);

      if(newAirport || (!metar.isEmpty() && metar != currentWeatherContext->fsMetar))
      {
        // Airport has changed or METAR has changed
        currentWeatherContext->fsMetar = metar;
        changed = true;
      }
    }
    else
    {
      if(!currentWeatherContext->fsMetar.isEmpty())
      {
        // If there was a previous metar and the new one is empty we were being disconnected
        currentWeatherContext->fsMetar = atools::fs::weather::MetarResult();
        changed = true;
      }
    }
  }

  if(flags & optsw::WEATHER_INFO_ACTIVESKY)
  {
    fillActiveSkyType(*currentWeatherContext, ident);

    QString metarStr = weatherReporter->getActiveSkyMetar(ident);
    if(newAirport || (!metarStr.isEmpty() && metarStr != currentWeatherContext->asMetar))
    {
      // Airport has changed or METAR has changed
      currentWeatherContext->asMetar = metarStr;
      changed = true;
    }
  }

  if(flags & optsw::WEATHER_INFO_NOAA)
  {
    atools::fs::weather::MetarResult noaaMetar = weatherReporter->getNoaaMetar(ident, airport.position);
    if(newAirport || (!noaaMetar.isEmpty() && noaaMetar != currentWeatherContext->noaaMetar))
    {
      // Airport has changed or METAR has changed
      currentWeatherContext->noaaMetar = noaaMetar;
      changed = true;
    }
  }

  if(flags & optsw::WEATHER_INFO_VATSIM)
  {
    atools::fs::weather::MetarResult vatsimMetar = weatherReporter->getVatsimMetar(ident, airport.position);
    if(newAirport || (!vatsimMetar.isEmpty() && vatsimMetar != currentWeatherContext->vatsimMetar))
    {
      // Airport has changed or METAR has changed
      currentWeatherContext->vatsimMetar = vatsimMetar;
      changed = true;
    }
  }

  if(flags & optsw::WEATHER_INFO_IVAO)
  {
    atools::fs::weather::MetarResult ivaoMetar = weatherReporter->getIvaoMetar(ident, airport.position);
    if(newAirport || (!ivaoMetar.isEmpty() && ivaoMetar != currentWeatherContext->ivaoMetar))
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
