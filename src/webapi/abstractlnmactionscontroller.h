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

#ifndef ABSTRACTLNMACTIONSCONTROLLER_H
#define ABSTRACTLNMACTIONSCONTROLLER_H

#include "webapi/abstractactionscontroller.h"

namespace atools {
    namespace geo {
        class Pos;
    }
    namespace fs {
        namespace util {
            class MorseCode;
        }
        namespace sc {
            class SimConnectData;
        }
    }
    namespace sql {
        class SqlRecord;
    }
}
namespace InfoBuilderTypes {
    struct AirportAdminNames;
}
namespace ageo = atools::geo;
namespace map {
    struct MapAirport;
    struct WeatherContext;
}

class NavApp;
class MapQuery;
class WaypointTrackQuery;
class InfoQuery;
class AirportQuery;
class MainWindow;

using atools::fs::util::MorseCode;
using atools::sql::SqlRecord;
using InfoBuilderTypes::AirportAdminNames;
using atools::geo::Pos;
using atools::fs::sc::SimConnectData;

/**
 * @brief The base class for all Little Navmap API action controllers
 */
class AbstractLnmActionsController :
        public AbstractActionsController
{
    Q_OBJECT
    Q_ENUMS(AirportQueryType)
public:
    Q_INVOKABLE AbstractLnmActionsController(QObject *parent, bool verboseParam, AbstractInfoBuilder* infoBuilder);
    virtual ~AbstractLnmActionsController() override;
    enum AirportQueryType { SIM, NAV };
protected:
    // Main LNM objects
    NavApp* getNavApp();
    MapQuery* getMapQuery();
    WaypointTrackQuery* getWaypointTrackQuery();
    InfoQuery* getInfoQuery();
    AirportQuery* getAirportQuery(AirportQueryType type);
    MainWindow* getMainWindow();
    MorseCode* getMorseCode();

    // Common LNM model interface
    map::MapAirport getAirportByIdent(QByteArray ident);
    map::WeatherContext getWeatherContext(map::MapAirport& airport);
    const SqlRecord* getAirportInformation(int id);
    const AirportAdminNames getAirportAdminNames(map::MapAirport& airport);
    int getTransitionAltitude(map::MapAirport& airport);
    const QTime getSunset(const SqlRecord& airportInformation);
    const QTime getSunrise(const SqlRecord& airportInformation);
    const QTime getSunset(const Pos& pos);
    const QTime getSunrise(const Pos& pos);
    const QDateTime getActiveDateTime();
    const QString getActiveDateTimeSource();
    const SimConnectData getSimConnectData();
private:
    MorseCode* morseCode;
    QTime calculateSunriseSunset(const Pos& pos, float zenith);
    Pos getPosFromAirportInformation(const SqlRecord& airportInformation);
};

#endif // ABSTRACTLNMACTIONSCONTROLLER_H
