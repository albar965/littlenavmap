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


#include "common/maptypes.h"
#include "fs/sc/simconnectdata.h"

#include <QObject>

class Route;

namespace map { class WeatherContext; }
namespace atools {
    namespace sql {
        class SqlRecord;
    }
}

using atools::sql::SqlRecord;
using atools::fs::sc::SimConnectData;

/**
 * Generic info builder parameter interfaces.
 * Types used by AbstractInfoBuilder and derived classes.
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
        const QTime* sunrise = nullptr;
        const QTime* sunset = nullptr;
        const QDateTime* activeDateTime = nullptr;
        const QString* activeDateTimeSource = nullptr;
    };

    /**
     * @brief Data container for simconnect data
     */
    struct SimConnectInfoData{
        const SimConnectData* data;
        const float windSpeed;
        const float windDir;
    };

    /**
     * @brief Data container for ui data
     */
    struct UiInfoData{
        const int zoomUi;
        const int zoomWeb;
        const qreal distanceUi;
        const qreal distanceWeb;
    };

    /**
     * @brief Data container for map features data
     */
    struct MapFeaturesData{
        const QList<map::MapAirport> airports;
        const QList<map::MapNdb> ndbs;
        const QList<map::MapVor> vors;
        const QList<map::MapMarker> markers;
        const QList<map::MapWaypoint> waypoints;
    };

}

#endif // INFOBUILDERTYPES_H
