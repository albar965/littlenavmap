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

#include "airportactionscontroller.h"
#include "common/maptypes.h"
#include "common/infobuildertypes.h"
#include "common/abstractinfobuilder.h"
#include "weather/weathercontext.h"
#include "webapi/webapirequest.h"

using InfoBuilderTypes::AirportInfoData;

#include <QDebug>

AirportActionsController::AirportActionsController(QObject *parent, bool verboseParam, AbstractInfoBuilder* infoBuilder) :
    AbstractLnmActionsController(parent, verboseParam, infoBuilder)
{
    if(verbose)
        qDebug() << Q_FUNC_INFO;
}

WebApiResponse AirportActionsController::infoAction(WebApiRequest request){
    if(verbose)
        qDebug() << Q_FUNC_INFO << request.parameters.value("ident");

    // Get a new response object
    WebApiResponse response = getResponse();

    // Query item
    map::MapAirport airport = getAirportByIdent(request.parameters.value("ident").toUpper());

    if(airport.isValid()){

        // Fetch related data
        const SqlRecord* airportInformation = getAirportInformation(airport.id);
        const AirportAdminNames airportAdminNames = getAirportAdminNames(airport);
        const int transitionAltitude = getTransitionAltitude(airport);

        const QTime sunrise = getSunrise(*airportInformation);
        const QTime sunset =  getSunset(*airportInformation);
        const QDateTime activeDateTime = getActiveDateTime();
        const QString activeDateTimeSource = getActiveDateTimeSource();

        // Compose data container
        AirportInfoData data = {
            airport,
            getWeatherContext(airport),
            nullptr,
            airportInformation,
            &airportAdminNames,
            &transitionAltitude,
            &sunrise,
            &sunset,
            &activeDateTime,
            &activeDateTimeSource
        };

        // Pass data to builder to assembly the return value
        response.body = infoBuilder->airport(data);
        response.status = 200;

    }else{

        response.body = "Airport not found";
        response.status = 404;

    }

    return response;

}
