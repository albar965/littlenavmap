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

#include "common/jsoninfobuilder.h"
#include "common/maptypes.h"

// Use JSON library
#include "json/nlohmann/json.hpp"
using JSON = nlohmann::json;

JsonInfoBuilder::JsonInfoBuilder(QObject *parent)
  : AbstractInfoBuilder(parent)
{
  contentTypeHeader = "application/json";
}

JsonInfoBuilder::~JsonInfoBuilder()
{

}

QByteArray JsonInfoBuilder::airport(AirportInfoData airportInfoData) const
{
    JSON json = {
        { "ident", qUtf8Printable(airportInfoData.airport.ident) },
        { "ICAO", qUtf8Printable(airportInfoData.airport.icao) },
        { "name", qUtf8Printable(airportInfoData.airport.name) },
        { "region", qUtf8Printable(airportInfoData.airport.region) },
        { "closed", airportInfoData.airport.closed() },
        { "addon", airportInfoData.airport.addon() },
        { "elevation", qUtf8Printable(Unit::altFeet(airportInfoData.airport.getPosition().getAltitude())) },
        { "magneticDeclination", qUtf8Printable(map::magvarText(airportInfoData.airport.magvar)) },
    };

    if(airportInfoData.airportInformation!=nullptr){
        json["rating"] = airportInfoData.airportInformation->valueInt("rating");
        json["IATA"] = qUtf8Printable(airportInfoData.airportInformation->valueStr("iata"));
    }

    if(airportInfoData.airportAdminNames!=nullptr){
        json["city"] = qUtf8Printable(airportInfoData.airportAdminNames->city);
        json["state"] = qUtf8Printable(airportInfoData.airportAdminNames->state);
        json["country"] = qUtf8Printable(airportInfoData.airportAdminNames->country);
    }

    return json.dump().data();
}

