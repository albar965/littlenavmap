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

#include "airportactionscontroller.h"
#include "abstractlnmactionscontroller.h"

#include <QDebug>

AirportActionsController::AirportActionsController(QObject *parent, bool verboseParam, AbstractInfoBuilder* infoBuilder) :
    AbstractLnmActionsController(parent, verboseParam, infoBuilder)
{
    qDebug() << Q_FUNC_INFO;
}

WebApiResponse AirportActionsController::infoAction(WebApiRequest request){
    qDebug() << Q_FUNC_INFO << request.parameters.value("ident");

    WebApiResponse response = getResponse();

    map::MapAirport airport = getAirportByIdent(request.parameters.value("ident").toUpper());

    if(airport.id > 0){

        map::WeatherContext weatherContext = getWeatherContext(airport);

        response.body = infoBuilder->airport(airport, weatherContext, nullptr);
        response.status = 200;

    }else{

        response.body = "Airport not found";
        response.status = 404;

    }

    return response;

}
