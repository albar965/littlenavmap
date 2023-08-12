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

#include "abstractlnmactionscontroller.h"

#include "app/navapp.h"
#include "common/infobuildertypes.h"
#include "common/maptypes.h"
#include "fs/sc/simconnectdata.h"
#include "fs/util/morsecode.h"
#include "geo/calculations.h"
#include "gui/mainwindow.h"
#include "mapgui/mappaintwidget.h"
#include "query/airportquery.h"
#include "query/infoquery.h"
#include "query/mapquery.h"
#include "sql/sqlrecord.h"
#include "weather/weathercontext.h"
#include "weather/weathercontexthandler.h"

namespace ageo = atools::geo;
using atools::fs::util::MorseCode;
using atools::sql::SqlRecord;
using InfoBuilderTypes::AirportAdminNames;
using atools::geo::Pos;
using atools::fs::sc::SimConnectData;

AbstractLnmActionsController::AbstractLnmActionsController(QObject *parent, bool verboseParam, AbstractInfoBuilder* infoBuilder) :
    AbstractActionsController(parent, verboseParam, infoBuilder)
{
    if(verbose)
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
    // Get "own" MapQuery since it contains caches releated to shown screen area
    if(getNavApp()->getMapPaintWidgetWeb() != nullptr)
      return getNavApp()->getMapPaintWidgetWeb()->getMapQuery();
    else
      return nullptr;
}

WaypointTrackQuery* AbstractLnmActionsController::getWaypointTrackQuery(){
    // Get "own" WaypointQuery since it contains caches releated to shown screen area
    if(getNavApp()->getMapPaintWidgetWeb() != nullptr)
      return getNavApp()->getMapPaintWidgetWeb()->getWaypointTrackQuery();
    else
      return nullptr;
}

InfoQuery* AbstractLnmActionsController::getInfoQuery(){
    return getNavApp()->getInfoQuery();
}

AirportQuery* AbstractLnmActionsController::getAirportQuery(AbstractLnmActionsController::AirportQueryType type){
  // Airport query and caches are independent of the MapPaintWidget instance
    switch (type) {
    case NAV:
        return getNavApp()->getAirportQueryNav();
        break;
    case SIM:
        return getNavApp()->getAirportQuerySim();
        break;
    }
    return nullptr;
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
map::WeatherContext AbstractLnmActionsController::getWeatherContext(map::MapAirport& airport){
    map::WeatherContext weatherContext;
    getMainWindow()->getWeatherContextHandler()->buildWeatherContextInfo(weatherContext, airport);
    return weatherContext;
};
const SqlRecord* AbstractLnmActionsController::getAirportInformation(int id){
    return getInfoQuery()->getAirportInformation(id);
}
const AirportAdminNames AbstractLnmActionsController::getAirportAdminNames(map::MapAirport& airport){
    QString city, state, country;
    getAirportQuery(AirportQueryType::SIM)->getAirportAdminNamesById(airport.id, city, state, country);
    return {city, state, country};
}
int AbstractLnmActionsController::getTransitionAltitude(map::MapAirport& airport){
    // Get transition altitude from nav database
    if(getMapQuery() != nullptr)
    {
      map::MapAirport navAirport = airport;
      getMapQuery()->getAirportNavReplace(navAirport);
      if(navAirport.isValid() && navAirport.transitionAltitude > 0)
        return navAirport.transitionAltitude;
    }
    return -1;
}

const QTime AbstractLnmActionsController::getSunset(const SqlRecord& airportInformation){
    return calculateSunriseSunset(getPosFromAirportInformation(airportInformation),ageo::SUNSET_CIVIL);
};
const QTime AbstractLnmActionsController::getSunrise(const SqlRecord& airportInformation){
    return calculateSunriseSunset(getPosFromAirportInformation(airportInformation),ageo::SUNRISE_CIVIL);
};
const QTime AbstractLnmActionsController::getSunset(const Pos& pos){
    return calculateSunriseSunset(pos,ageo::SUNSET_CIVIL);
};
const QTime AbstractLnmActionsController::getSunrise(const Pos &pos){
    return calculateSunriseSunset(pos,ageo::SUNRISE_CIVIL);
};
QTime AbstractLnmActionsController::calculateSunriseSunset(const Pos &pos, float zenith){
    QTime result;
    QDateTime datetime =
      getNavApp()->isConnectedAndAircraft() ? getNavApp()->getUserAircraft().getZuluTime() : QDateTime::currentDateTimeUtc();

    if(datetime.isValid())
    {
        bool neverRises, neverSets;
        result = ageo::calculateSunriseSunset(neverRises, neverSets, pos,
                                                 datetime.date(), zenith);
    }
    return result;
};
Pos AbstractLnmActionsController::getPosFromAirportInformation(const SqlRecord &airportInformation){
    Pos pos(airportInformation.valueFloat("lonx"), airportInformation.valueFloat("laty"));
    return pos;
}

const QDateTime AbstractLnmActionsController::getActiveDateTime(){
    return getNavApp()->isConnectedAndAircraft() ? getNavApp()->getUserAircraft().getZuluTime() : QDateTime::currentDateTimeUtc();

};
const QString AbstractLnmActionsController::getActiveDateTimeSource(){
    return getNavApp()->isConnectedAndAircraft() ? tr("simulator date") : tr("real date");
};

const SimConnectData AbstractLnmActionsController::getSimConnectData(){
    return getNavApp()->getSimConnectData();
};
