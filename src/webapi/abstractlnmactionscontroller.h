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
#include "common/infobuildertypes.h"
#include "common/maptypes.h"
#include "navapp.h"
#include "sql/sqlrecord.h"
#include "fs/util/morsecode.h"

using atools::fs::util::MorseCode;
using atools::sql::SqlRecord;
using InfoBuilderTypes::AirportAdminNames;

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
    virtual ~AbstractLnmActionsController();
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

    // Common LNM logic
    map::MapAirport getAirportByIdent(QByteArray ident);
    map::WeatherContext getWeatherContext(map::MapAirport airport);
    const SqlRecord* getAirportInformation(int id);
    const AirportAdminNames getAirportAdminNames(map::MapAirport airport);
    int getTransitionAltitude(map::MapAirport airport);

private:
    MorseCode* morseCode;
};

#endif // ABSTRACTLNMACTIONSCONTROLLER_H
