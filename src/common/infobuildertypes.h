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

#ifndef INFOBUILDERTYPES_H
#define INFOBUILDERTYPES_H


#include "sql/sqlrecord.h"

#include <QObject>

namespace map {
struct MapAirport;
struct WeatherContext;
}
class Route;

using atools::sql::SqlRecord;

/**
 * Types used by AbstractInfoBuilder and derived classes
 */
namespace InfoBuilderTypes {

    /**
     * @brief Data container for airport admin information
     */
    struct AirportAdminNames{
        const QString city;
        const QString state;
        const QString country;
    };

    /**
     * @brief Data container for airport information
     */
    struct AirportInfoData{
        // required
        const map::MapAirport& airport;
        const map::WeatherContext& weatherContext;
        // optional
        const Route *route = nullptr;
        const SqlRecord* airportInformation = nullptr;
        const AirportAdminNames* airportAdminNames = nullptr;
        const int* transitionAltitude = nullptr;
    };

}

#endif // INFOBUILDERTYPES_H
