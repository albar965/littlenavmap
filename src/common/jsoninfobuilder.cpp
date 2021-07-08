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
        { "rating", nullptr },
        { "IATA", nullptr },
        { "city", nullptr },
        { "state", nullptr },
        { "country", nullptr },
        { "transitionAltitude", nullptr },
        { "facilities", JSON::array()},
        { "runways", JSON::array()},
        { "longestRunwayLength", nullptr },
        { "metar", JSON::array()},
    };

    /* Fields populated if available */

    if(airportInfoData.airportInformation!=nullptr){
        json["rating"] = airportInfoData.airportInformation->valueInt("rating");
        json["IATA"] = qUtf8Printable(airportInfoData.airportInformation->valueStr("iata"));
    }

    if(airportInfoData.airportAdminNames!=nullptr){
        json["city"] = qUtf8Printable(airportInfoData.airportAdminNames->city);
        json["state"] = qUtf8Printable(airportInfoData.airportAdminNames->state);
        json["country"] = qUtf8Printable(airportInfoData.airportAdminNames->country);
    }

    if(airportInfoData.transitionAltitude!=nullptr && *airportInfoData.transitionAltitude > 0){
        json["transitionAltitude"] = qUtf8Printable(Unit::altFeet(*airportInfoData.transitionAltitude));
    }

    if(!airportInfoData.airport.noRunways()){
        json["longestRunwayLength"] = qUtf8Printable(Unit::distShortFeet(airportInfoData.airport.longestRunwayLength));
    }

    /* METAR */

    // Simulator
    if(!airportInfoData.weatherContext.fsMetar.isEmpty()){
        json["metar"].push_back({{ "simulator", {
                {"station", qUtf8Printable(airportInfoData.weatherContext.fsMetar.metarForStation)},
                {"nearest", qUtf8Printable(airportInfoData.weatherContext.fsMetar.metarForNearest)},
                {"interpolated", qUtf8Printable(airportInfoData.weatherContext.fsMetar.metarForInterpolated)},
            }
        }});
    }

    // Active Sky
    if(!airportInfoData.weatherContext.asMetar.isEmpty()){
        json["metar"].push_back({{ "activesky", {
                {"station", qUtf8Printable(airportInfoData.weatherContext.asMetar)},
            }
        }});
    }

    // NOAA
    if(
       !airportInfoData.weatherContext.noaaMetar.metarForStation.isEmpty() ||
       !airportInfoData.weatherContext.noaaMetar.metarForNearest.isEmpty()
      ){
        json["metar"].push_back({{ "noaa", {
                {"station", qUtf8Printable(airportInfoData.weatherContext.noaaMetar.metarForStation)},
                {"nearest", qUtf8Printable(airportInfoData.weatherContext.noaaMetar.metarForNearest)},
            }
        }});
    }

    // VATSIM
    if(
       !airportInfoData.weatherContext.vatsimMetar.metarForStation.isEmpty() ||
       !airportInfoData.weatherContext.vatsimMetar.metarForNearest.isEmpty()
      ){
        json["metar"].push_back({{ "vatsim", {
                {"station", qUtf8Printable(airportInfoData.weatherContext.vatsimMetar.metarForStation)},
                {"nearest", qUtf8Printable(airportInfoData.weatherContext.vatsimMetar.metarForNearest)},
            }
        }});
    }

    // IVAO
    if(
       !airportInfoData.weatherContext.ivaoMetar.metarForStation.isEmpty() ||
       !airportInfoData.weatherContext.ivaoMetar.metarForStation.isEmpty()
      ){
        json["metar"].push_back({{"ivao", {
                {"station", qUtf8Printable(airportInfoData.weatherContext.ivaoMetar.metarForStation)},
                {"nearest", qUtf8Printable(airportInfoData.weatherContext.ivaoMetar.metarForNearest)},
            }
        }});
    }

    /* Facilities */

    if(airportInfoData.airport.closed())
        json["facilities"].push_back(qUtf8Printable(tr("Closed")));
    if(airportInfoData.airport.addon())
        json["facilities"].push_back(qUtf8Printable(tr("Add-on")));
    if(airportInfoData.airport.is3d())
        json["facilities"].push_back(qUtf8Printable(tr("3D")));
    if(airportInfoData.airport.flags.testFlag(map::MapAirportFlag::AP_MIL))
        json["facilities"].push_back(qUtf8Printable(tr("Military")));
    if(airportInfoData.airport.apron())
        json["facilities"].push_back(qUtf8Printable(tr("Aprons")));
    if(airportInfoData.airport.taxiway())
        json["facilities"].push_back(qUtf8Printable(tr("Taxiways")));
    if(airportInfoData.airport.towerObject())
        json["facilities"].push_back(qUtf8Printable(tr("Tower Object")));
    if(airportInfoData.airport.parking())
        json["facilities"].push_back(qUtf8Printable(tr("Parking")));
    if(airportInfoData.airport.helipad())
        json["facilities"].push_back(qUtf8Printable(tr("Helipads")));
    if(airportInfoData.airport.flags.testFlag(map::MapAirportFlag::AP_AVGAS))
        json["facilities"].push_back(qUtf8Printable(tr("Avgas")));
    if(airportInfoData.airport.flags.testFlag(map::MapAirportFlag::AP_JETFUEL))
        json["facilities"].push_back(qUtf8Printable(tr("Jetfuel")));
    if(airportInfoData.airport.flags.testFlag(map::MapAirportFlag::AP_ILS))
        json["facilities"].push_back(qUtf8Printable(tr("ILS")));
    if(airportInfoData.airport.vasi())
        json["facilities"].push_back(qUtf8Printable(tr("VASI")));
    if(airportInfoData.airport.als())
        json["facilities"].push_back(qUtf8Printable(tr("ALS")));
    if(airportInfoData.airport.flatten == 1)
        json["facilities"].push_back(qUtf8Printable(tr("Flatten")));
    if(airportInfoData.airport.flatten == 0)
        json["facilities"].push_back(qUtf8Printable(tr("No Flatten")));

    /* Runways */

    if(airportInfoData.airport.hard())
        json["runways"].push_back(qUtf8Printable(tr("Hard")));
    if(airportInfoData.airport.soft())
        json["runways"].push_back(qUtf8Printable(tr("Soft")));
    if(airportInfoData.airport.water())
        json["runways"].push_back(qUtf8Printable(tr("Water")));
    if(airportInfoData.airport.closedRunways())
        json["runways"].push_back(qUtf8Printable(tr("Closed")));
    if(airportInfoData.airport.flags.testFlag(map::MapAirportFlag::AP_LIGHT))
        json["runways"].push_back(qUtf8Printable(tr("Lighted")));

    /* Fields added only if available */

    if(airportInfoData.airport.xpident.length() > 0 && airportInfoData.airport.ident != airportInfoData.airport.xpident)
        json["x-plane-ident"] = qUtf8Printable(airportInfoData.airport.xpident);

    return json.dump().data();
}

