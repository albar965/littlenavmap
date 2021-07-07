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

#include "abstractlnmactionscontroller.h"
#include "navapp.h"
#include "query/airportquery.h"
#include "query/infoquery.h"
#include "query/mapquery.h"
#include "gui/mainwindow.h"

AbstractLnmActionsController::AbstractLnmActionsController(QObject *parent, bool verboseParam, AbstractInfoBuilder* infoBuilder) :
    AbstractActionsController(parent, verboseParam, infoBuilder)
{
    qDebug() << Q_FUNC_INFO;
    morseCode = new MorseCode("&nbsp;", "&nbsp;&nbsp;&nbsp;");
}

AbstractLnmActionsController::~AbstractLnmActionsController(){
    delete morseCode;
}

NavApp* AbstractLnmActionsController::getNavApp(){
    return NavApp::navAppInstance();
}

MapQuery* AbstractLnmActionsController::getMapQuery(){
    return getNavApp()->getMapQuery();
}

WaypointTrackQuery* AbstractLnmActionsController::getWaypointTrackQuery(){
    return getNavApp()->getWaypointTrackQuery();
}

InfoQuery* AbstractLnmActionsController::getInfoQuery(){
    return getNavApp()->getInfoQuery();
}

AirportQuery* AbstractLnmActionsController::getAirportQuery(AbstractLnmActionsController::AirportQueryType type){
    switch (type) {
    case NAV:
        return getNavApp()->getAirportQueryNav();
        break;
    case SIM:
        return getNavApp()->getAirportQuerySim();
        break;
    }
}

MainWindow* AbstractLnmActionsController::getMainWindow(){
    return getNavApp()->getMainWindow();
}

MorseCode* AbstractLnmActionsController::getMorseCode(){
    return morseCode;
}

map::MapAirport AbstractLnmActionsController::getAirportByIdent(QByteArray ident){
    map::MapAirport airport;
    getAirportQuery(AirportQueryType::SIM)->getAirportByIdent(airport,ident);
    return airport;
};
map::WeatherContext AbstractLnmActionsController::getWeatherContext(map::MapAirport airport){
    map::WeatherContext weatherContext;
    getMainWindow()->buildWeatherContext(weatherContext, airport);
    return weatherContext;
};
const SqlRecord* AbstractLnmActionsController::getAirportInformation(int id){
    return getInfoQuery()->getAirportInformation(id);
}
const AirportAdminNames AbstractLnmActionsController::getAirportAdminNames(map::MapAirport airport){
    QString city, state, country;
    getAirportQuery(AirportQueryType::SIM)->getAirportAdminNamesById(airport.id, city, state, country);
    return {city, state, country};
}

const int AbstractLnmActionsController::getTransitionAltitude(map::MapAirport airport){
    // Get transition altitude from nav database
    map::MapAirport navAirport = airport;
    getMapQuery()->getAirportNavReplace(navAirport);
    if(navAirport.isValid() && navAirport.transitionAltitude > 0)
      return navAirport.transitionAltitude;
    return -1;
}

