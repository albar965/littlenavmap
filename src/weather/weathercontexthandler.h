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

#ifndef LNM_WEATHERCONTEXTHANDLER_H
#define LNM_WEATHERCONTEXTHANDLER_H

#include <QObject>

class QString;
class WeatherReporter;
class ConnectClient;

namespace map {
struct MapAirport;
struct WeatherContext;
}

/*
 * Keeps weather contexts for all configured sources and display types (info, tooltip, etc.) to avoid too many updates.
 */
class WeatherContextHandler :
  public QObject
{
  Q_OBJECT

public:
  explicit WeatherContextHandler(WeatherReporter *weatherReporterParam, ConnectClient *connectClientParam);
  virtual ~WeatherContextHandler() override;

  /* Update the current weather context for the information window. Returns true if any
   * weather has changed or an update in the user interface is needed */
  bool buildWeatherContextInfoFull(map::WeatherContext& weatherContext, const map::MapAirport& airport);

  /* Build a normal temporary info weather context - used by printing */
  void buildWeatherContextInfo(map::WeatherContext& weatherContext, const map::MapAirport& airport) const;

  /* Build a temporary weather context for the map tooltip */
  void buildWeatherContextTooltip(map::WeatherContext& weatherContext, const map::MapAirport& airport) const;

  void clearWeatherContext();
  void buildWeatherContext(map::WeatherContext& weatherContext, const map::MapAirport& airport, bool simulator, bool activeSky, bool noaa,
                           bool vatsim, bool ivao) const;

private:
  void fillActiveSkyType(map::WeatherContext& weatherContext, const QString& airportIdent) const;

  map::WeatherContext *currentWeatherContext = nullptr;
  WeatherReporter *weatherReporter;
  ConnectClient *connectClient;
};

#endif // LNM_WEATHERCONTEXTHANDLER_H
