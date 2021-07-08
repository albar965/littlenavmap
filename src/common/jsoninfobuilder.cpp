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

    AirportInfoData& data = airportInfoData;

    JSON json = {
        { "ident", qUtf8Printable(data.airport.ident) },
        { "ICAO", qUtf8Printable(data.airport.icao) },
        { "name", qUtf8Printable(data.airport.name) },
        { "region", qUtf8Printable(data.airport.region) },
        { "closed", data.airport.closed() },
        { "addon", data.airport.addon() },
        { "elevation", qUtf8Printable(Unit::altFeet(data.airport.getPosition().getAltitude())) },
        { "magneticDeclination", qUtf8Printable(map::magvarText(data.airport.magvar)) },
        { "rating", nullptr },
        { "IATA", nullptr },
        { "city", nullptr },
        { "state", nullptr },
        { "country", nullptr },
        { "transitionAltitude", nullptr },
        { "facilities", JSON::array()},
        { "runways", JSON::array()},
        { "longestRunwayLength", nullptr },
        { "longestRunwayHeading", nullptr },
        { "longestRunwaySurface", nullptr },
        { "metar", JSON::array()},
    };

    /* Null fields populated if available */

    if(data.airportInformation!=nullptr){
        json["rating"] = data.airportInformation->valueInt("rating");
        json["IATA"] = qUtf8Printable(data.airportInformation->valueStr("iata"));
    }

    if(data.airportAdminNames!=nullptr){
        json["city"] = qUtf8Printable(data.airportAdminNames->city);
        json["state"] = qUtf8Printable(data.airportAdminNames->state);
        json["country"] = qUtf8Printable(data.airportAdminNames->country);
    }

    if(data.transitionAltitude!=nullptr && *data.transitionAltitude > 0){
        json["transitionAltitude"] = qUtf8Printable(Unit::altFeet(*data.transitionAltitude));
    }

    if(!data.airport.noRunways()){
        json["longestRunwayLength"] = qUtf8Printable(Unit::distShortFeet(data.airport.longestRunwayLength));
        if(data.airportInformation!=nullptr){
            json["longestRunwayWidth"] = qUtf8Printable(Unit::distShortFeet(data.airportInformation->valueInt("longest_runway_width")));
            json["longestRunwayHeading"] = qUtf8Printable(getHeadingsStringByMagVar(data.airportInformation->valueFloat("longest_runway_heading"),data.airport.magvar));
            json["longestRunwaySurface"] = qUtf8Printable(data.airportInformation->valueStr("longest_runway_surface"));
        }
    }

    /* METAR */

    // Simulator
    if(!data.weatherContext.fsMetar.isEmpty()){
        json["metar"].push_back({{ "simulator", {
                {"station", qUtf8Printable(data.weatherContext.fsMetar.metarForStation)},
                {"nearest", qUtf8Printable(data.weatherContext.fsMetar.metarForNearest)},
                {"interpolated", qUtf8Printable(data.weatherContext.fsMetar.metarForInterpolated)},
            }
        }});
    }

    // Active Sky
    if(!data.weatherContext.asMetar.isEmpty()){
        json["metar"].push_back({{ "activesky", {
                {"station", qUtf8Printable(data.weatherContext.asMetar)},
            }
        }});
    }

    // NOAA
    if(
       !data.weatherContext.noaaMetar.metarForStation.isEmpty() ||
       !data.weatherContext.noaaMetar.metarForNearest.isEmpty()
      ){
        json["metar"].push_back({{ "noaa", {
                {"station", qUtf8Printable(data.weatherContext.noaaMetar.metarForStation)},
                {"nearest", qUtf8Printable(data.weatherContext.noaaMetar.metarForNearest)},
            }
        }});
    }

    // VATSIM
    if(
       !data.weatherContext.vatsimMetar.metarForStation.isEmpty() ||
       !data.weatherContext.vatsimMetar.metarForNearest.isEmpty()
      ){
        json["metar"].push_back({{ "vatsim", {
                {"station", qUtf8Printable(data.weatherContext.vatsimMetar.metarForStation)},
                {"nearest", qUtf8Printable(data.weatherContext.vatsimMetar.metarForNearest)},
            }
        }});
    }

    // IVAO
    if(
       !data.weatherContext.ivaoMetar.metarForStation.isEmpty() ||
       !data.weatherContext.ivaoMetar.metarForStation.isEmpty()
      ){
        json["metar"].push_back({{"ivao", {
                {"station", qUtf8Printable(data.weatherContext.ivaoMetar.metarForStation)},
                {"nearest", qUtf8Printable(data.weatherContext.ivaoMetar.metarForNearest)},
            }
        }});
    }

    /* Facilities */

    if(data.airport.closed())
        json["facilities"].push_back(qUtf8Printable(tr("Closed")));
    if(data.airport.addon())
        json["facilities"].push_back(qUtf8Printable(tr("Add-on")));
    if(data.airport.is3d())
        json["facilities"].push_back(qUtf8Printable(tr("3D")));
    if(data.airport.flags.testFlag(map::MapAirportFlag::AP_MIL))
        json["facilities"].push_back(qUtf8Printable(tr("Military")));
    if(data.airport.apron())
        json["facilities"].push_back(qUtf8Printable(tr("Aprons")));
    if(data.airport.taxiway())
        json["facilities"].push_back(qUtf8Printable(tr("Taxiways")));
    if(data.airport.towerObject())
        json["facilities"].push_back(qUtf8Printable(tr("Tower Object")));
    if(data.airport.parking())
        json["facilities"].push_back(qUtf8Printable(tr("Parking")));
    if(data.airport.helipad())
        json["facilities"].push_back(qUtf8Printable(tr("Helipads")));
    if(data.airport.flags.testFlag(map::MapAirportFlag::AP_AVGAS))
        json["facilities"].push_back(qUtf8Printable(tr("Avgas")));
    if(data.airport.flags.testFlag(map::MapAirportFlag::AP_JETFUEL))
        json["facilities"].push_back(qUtf8Printable(tr("Jetfuel")));
    if(data.airport.flags.testFlag(map::MapAirportFlag::AP_ILS))
        json["facilities"].push_back(qUtf8Printable(tr("ILS")));
    if(data.airport.vasi())
        json["facilities"].push_back(qUtf8Printable(tr("VASI")));
    if(data.airport.als())
        json["facilities"].push_back(qUtf8Printable(tr("ALS")));
    if(data.airport.flatten == 1)
        json["facilities"].push_back(qUtf8Printable(tr("Flatten")));
    if(data.airport.flatten == 0)
        json["facilities"].push_back(qUtf8Printable(tr("No Flatten")));

    /* Runways */

    if(data.airport.hard())
        json["runways"].push_back(qUtf8Printable(tr("Hard")));
    if(data.airport.soft())
        json["runways"].push_back(qUtf8Printable(tr("Soft")));
    if(data.airport.water())
        json["runways"].push_back(qUtf8Printable(tr("Water")));
    if(data.airport.closedRunways())
        json["runways"].push_back(qUtf8Printable(tr("Closed")));
    if(data.airport.flags.testFlag(map::MapAirportFlag::AP_LIGHT))
        json["runways"].push_back(qUtf8Printable(tr("Lighted")));

    /* Fields added only if available */

    if(data.airport.xpident.length() > 0 && data.airport.ident != data.airport.xpident)
        json["x-plane-ident"] = qUtf8Printable(data.airport.xpident);

    return json.dump().data();
}

