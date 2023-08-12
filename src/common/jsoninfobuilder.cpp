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

#include "common/jsoninfobuilder.h"
#include "common/infobuildertypes.h"

#include "sql/sqlrecord.h"
#include "weather/weathercontext.h"

using InfoBuilderTypes::AirportInfoData;

JsonInfoBuilder::JsonInfoBuilder(QObject *parent)
  : AbstractInfoBuilder(parent)
{
  contentTypeHeader = "application/json";
}

JsonInfoBuilder::~JsonInfoBuilder()
{

}

JSON JsonInfoBuilder::coordinatesToJSON(QMap<QString,float> map) const
{
    return {
        {"lat", map["lat"]},
        {"lon", map["lon"]},
    };
}


QByteArray JsonInfoBuilder::airport(AirportInfoData airportInfoData) const
{

    AirportInfoData& data = airportInfoData;

    JSON json = {
        { "ident", qUtf8Printable(data.airport.ident) },
        { "icao", qUtf8Printable(data.airport.icao) },
        { "name", qUtf8Printable(data.airport.name) },
        { "region", qUtf8Printable(data.airport.region) },
        { "closed", data.airport.closed() },
        { "elevation", data.airport.getAltitude() },
        { "magneticDeclination", data.airport.magvar },
        { "position", nullptr },
        { "rating", nullptr },
        { "iata", nullptr },
        { "city", nullptr },
        { "state", nullptr },
        { "country", nullptr },
        { "transitionAltitude", nullptr },
        { "facilities", JSON::array()},
        { "runways", JSON::array()},
        { "parking", JSON::object()},
        { "longestRunwayLength", nullptr },
        { "longestRunwayHeading", nullptr },
        { "longestRunwaySurface", nullptr },
        { "metar", JSON::object()},
        { "sunrise", nullptr},
        { "sunset", nullptr},
        { "activeDateTime", nullptr},
        { "activeDateTimeSource", nullptr},
        { "com", JSON::object()},
    };

    /* Null fields populated if available */

    if(data.airportInformation!=nullptr){
        json["rating"] = data.airportInformation->valueInt("rating");
        json["iata"] = qUtf8Printable(data.airportInformation->valueStr("iata"));

        // Position
        json["position"] = coordinatesToJSON(getCoordinates(data.airportInformation));

        if(data.airportInformation->valueInt("num_parking_gate") > 0){
            json["parking"].push_back({ "gates", data.airportInformation->valueInt("num_parking_gate") });
        }
        if(data.airportInformation->valueInt("num_jetway") > 0){
            json["parking"].push_back({ "jetWays", data.airportInformation->valueInt("num_jetway") });
        }
        if(data.airportInformation->valueInt("num_parking_ga_ramp") > 0){
            json["parking"].push_back({ "gaRamps", data.airportInformation->valueInt("num_parking_ga_ramp") });
        }
        if(data.airportInformation->valueInt("num_parking_cargo") > 0){
            json["parking"].push_back({ "cargo", data.airportInformation->valueInt("num_parking_cargo") });
        }
        if(data.airportInformation->valueInt("num_parking_mil_cargo") > 0){
            json["parking"].push_back({ "militaryCargo", data.airportInformation->valueInt("num_parking_mil_cargo") });
        }
        if(data.airportInformation->valueInt("num_parking_mil_combat") > 0){
            json["parking"].push_back({ "militaryCombat", data.airportInformation->valueInt("num_parking_mil_combat") });
        }
        if(data.airportInformation->valueInt("num_helipad") > 0){
            json["parking"].push_back({ "helipads", data.airportInformation->valueInt("num_helipad") });
        }

    }

    if(data.airportAdminNames!=nullptr){
        json["city"] = qUtf8Printable(data.airportAdminNames->city);
        json["state"] = qUtf8Printable(data.airportAdminNames->state);
        json["country"] = qUtf8Printable(data.airportAdminNames->country);
    }

    if(data.transitionAltitude!=nullptr && *data.transitionAltitude > 0){
        json["transitionAltitude"] = *data.transitionAltitude;
    }

    if(!data.airport.noRunways()){
        json["longestRunwayLength"] = data.airport.longestRunwayLength;
        if(data.airportInformation!=nullptr){
            json["longestRunwayWidth"] = data.airportInformation->valueInt("longest_runway_width");
            json["longestRunwayHeading"] = qUtf8Printable(getHeadingsStringByMagVar(data.airportInformation->valueFloat("longest_runway_heading"),data.airport.magvar));
            json["longestRunwaySurface"] = qUtf8Printable(data.airportInformation->valueStr("longest_runway_surface"));
        }
    }

    if(data.sunrise!= nullptr){
        json["sunrise"]=qUtf8Printable(data.sunrise->toString());
    }
    if(data.sunset!= nullptr){
        json["sunset"]=qUtf8Printable(data.sunset->toString());
    }

    if(data.activeDateTime!=nullptr){
        json["activeDateTime"]=qUtf8Printable(data.activeDateTime->toString());
    }
    if(data.activeDateTimeSource!=nullptr){
        json["activeDateTimeSource"]=qUtf8Printable(*data.activeDateTimeSource);
    }

    /* METAR */

    // Simulator
    if(!data.weatherContext.fsMetar.isEmpty()){
        json["metar"].push_back({ "simulator", {
                {"station", qUtf8Printable(data.weatherContext.fsMetar.metarForStation)},
                {"nearest", qUtf8Printable(data.weatherContext.fsMetar.metarForNearest)},
                {"interpolated", qUtf8Printable(data.weatherContext.fsMetar.metarForInterpolated)},
            }
        });
    }

    // Active Sky
    if(!data.weatherContext.asMetar.isEmpty()){
        json["metar"].push_back({ "activesky", {
                {"station", qUtf8Printable(data.weatherContext.asMetar)},
            }
        });
    }

    // NOAA
    if(
       !data.weatherContext.noaaMetar.metarForStation.isEmpty() ||
       !data.weatherContext.noaaMetar.metarForNearest.isEmpty()
      ){
        json["metar"].push_back({ "noaa", {
                {"station", qUtf8Printable(data.weatherContext.noaaMetar.metarForStation)},
                {"nearest", qUtf8Printable(data.weatherContext.noaaMetar.metarForNearest)},
            }
        });
    }

    // VATSIM
    if(
       !data.weatherContext.vatsimMetar.metarForStation.isEmpty() ||
       !data.weatherContext.vatsimMetar.metarForNearest.isEmpty()
      ){
        json["metar"].push_back({ "vatsim", {
                {"station", qUtf8Printable(data.weatherContext.vatsimMetar.metarForStation)},
                {"nearest", qUtf8Printable(data.weatherContext.vatsimMetar.metarForNearest)},
            }
        });
    }

    // IVAO
    if(
       !data.weatherContext.ivaoMetar.metarForStation.isEmpty() ||
       !data.weatherContext.ivaoMetar.metarForStation.isEmpty()
      ){
        json["metar"].push_back({"ivao", {
                {"station", qUtf8Printable(data.weatherContext.ivaoMetar.metarForStation)},
                {"nearest", qUtf8Printable(data.weatherContext.ivaoMetar.metarForNearest)},
            }
        });
    }

    /* COM */

    if(data.airport.towerFrequency > 0){
        json["com"].push_back({"Tower:", data.airport.towerFrequency});
    }
    if(data.airport.atisFrequency > 0){
        json["com"].push_back({"ATIS:", data.airport.atisFrequency});
    }
    if(data.airport.awosFrequency > 0){
        json["com"].push_back({"AWOS:", data.airport.awosFrequency});
    }
    if(data.airport.asosFrequency > 0){
        json["com"].push_back({"ASOS:", data.airport.asosFrequency});
    }
    if(data.airport.unicomFrequency > 0){
        json["com"].push_back({"UNICOM:", data.airport.unicomFrequency});
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

  if(!data.airport.icao.isEmpty() && data.airport.ident != data.airport.icao)
    json["icao"] = qUtf8Printable(data.airport.icao);
  if(!data.airport.iata.isEmpty() && data.airport.ident != data.airport.iata)
    json["iata"] = qUtf8Printable(data.airport.iata);
  if(!data.airport.faa.isEmpty() && data.airport.ident != data.airport.faa)
    json["faa"] = qUtf8Printable(data.airport.faa);
  if(!data.airport.local.isEmpty() && data.airport.ident != data.airport.local)
    json["local"] = qUtf8Printable(data.airport.local);

  return json.dump().data();
}


QByteArray JsonInfoBuilder::siminfo(SimConnectInfoData simconnectInfoData) const
{

    SimConnectData data = *simconnectInfoData.data;

    JSON json;

    if(!data.isEmptyReply() && data.isUserAircraftValid()){

        json = {
           { "active", true},
           { "simconnect_status", qUtf8Printable(data.getStatusText()) },
           { "position", coordinatesToJSON(getCoordinates(data.getUserAircraft().getPosition())) },
           { "indicated_speed", data.getUserAircraft().getIndicatedSpeedKts() },
           { "true_airspeed", data.getUserAircraft().getTrueAirspeedKts() },
           { "ground_speed", data.getUserAircraft().getGroundSpeedKts() },
           { "sea_level_pressure", data.getUserAircraft().getSeaLevelPressureMbar() },
           { "vertical_speed", data.getUserAircraft().getVerticalSpeedFeetPerMin() },
           { "indicated_altitude", data.getUserAircraft().getIndicatedAltitudeFt() },
           { "ground_altitude", data.getUserAircraft().getGroundAltitudeFt() },
           { "altitude_above_ground", data.getUserAircraft().getAltitudeAboveGroundFt() },
           { "heading", data.getUserAircraft().getHeadingDegMag() },
           { "wind_direction", simconnectInfoData.windDir },
           { "wind_speed", simconnectInfoData.windSpeed },
       };

    }else{
        json = {
            { "active", false}
        };
    }

  return json.dump().data();
}


QByteArray JsonInfoBuilder::uiinfo(UiInfoData uiInfoData) const
{

    UiInfoData data = uiInfoData;

    JSON json;

       json = {
           { "zoom_ui", data.zoomUi},
           { "zoom_web", data.zoomWeb},
           { "distance_ui", data.distanceUi},
           { "distance_web", data.distanceWeb},
       };

    return json.dump().data();
}

QByteArray JsonInfoBuilder::features(MapFeaturesData mapFeaturesData) const
{

    MapFeaturesData data = mapFeaturesData;

    JSON json;

       json = {
           { "airports", JSON::object() },
           { "ndbs", JSON::object() },
           { "vors", JSON::object() },
           { "markers", JSON::object() },
           { "waypoints", JSON::object() },
       };

       json["airports"].push_back({ "count", data.airports.count() });
       json["airports"].push_back({ "result", JSON::array() });

       json["ndbs"].push_back({ "count", data.ndbs.count() });
       json["ndbs"].push_back({ "result", JSON::array() });

       json["vors"].push_back({ "count", data.vors.count() });
       json["vors"].push_back({ "result", JSON::array() });

       json["markers"].push_back({ "count", data.markers.count() });
       json["markers"].push_back({ "result", JSON::array() });

       json["waypoints"].push_back({ "count", data.waypoints.count() });
       json["waypoints"].push_back({ "result", JSON::array() });



       for(int i = 0; i < data.airports.count(); ++i){

           map::MapAirport airport =  data.airports[i];

           json["airports"]["result"][i]["object_id"] = airport.id;
           json["airports"]["result"][i]["type_id"] = map::AIRPORT;
           json["airports"]["result"][i]["ident"] = qUtf8Printable(airport.ident);
           json["airports"]["result"][i]["name"] = qUtf8Printable(airport.name);
           json["airports"]["result"][i]["position"] = coordinatesToJSON(getCoordinates(airport.position));
           json["airports"]["result"][i]["elevation"] =  airport.getAltitude();

//           JSON airportJson;

//           airportJson = {
//               { "ident", qUtf8Printable(airport.ident) },
//               { "name", qUtf8Printable(airport.name)},
//               { "position", coordinatesToJSON(getCoordinates(airport.position))},
//               { "elevation", airport.getPosition().getAltitude() },
//           };

//           json["airports"]["result"].push_back(airportJson);
       }

       for(int i = 0; i < data.ndbs.count(); ++i){

           map::MapNdb ndb =  data.ndbs[i];

           json["ndbs"]["result"][i]["object_id"] = ndb.id;
           json["ndbs"]["result"][i]["type_id"] = map::NDB;
           json["ndbs"]["result"][i]["ident"] = qUtf8Printable(ndb.ident);
           json["ndbs"]["result"][i]["name"] = qUtf8Printable(ndb.name);
           json["ndbs"]["result"][i]["position"] = coordinatesToJSON(getCoordinates(ndb.position));
           json["ndbs"]["result"][i]["elevation"] =  ndb.getAltitude();

       }

       for(int i = 0; i < data.vors.count(); ++i){

           map::MapVor vor =  data.vors[i];

           json["vors"]["result"][i]["object_id"] = vor.id;
           json["vors"]["result"][i]["type_id"] = map::VOR;
           json["vors"]["result"][i]["ident"] = qUtf8Printable(vor.ident);
           json["vors"]["result"][i]["name"] = qUtf8Printable(vor.name);
           json["vors"]["result"][i]["position"] = coordinatesToJSON(getCoordinates(vor.position));
           json["vors"]["result"][i]["elevation"] =  vor.getAltitude();

       }

       for(int i = 0; i < data.markers.count(); ++i){

           map::MapMarker marker =  data.markers[i];

           json["markers"]["result"][i]["object_id"] = marker.id;
           json["markers"]["result"][i]["type_id"] = map::MARKER;
           json["markers"]["result"][i]["ident"] = qUtf8Printable(marker.ident);
           json["markers"]["result"][i]["type"] = qUtf8Printable(marker.type);
           json["markers"]["result"][i]["position"] = coordinatesToJSON(getCoordinates(marker.position));
           json["markers"]["result"][i]["elevation"] =  marker.getAltitude();

       }

       for(int i = 0; i < data.waypoints.count(); ++i){

           map::MapWaypoint waypoint =  data.waypoints[i];

           json["waypoints"]["result"][i]["object_id"] = waypoint.id;
           json["waypoints"]["result"][i]["type_id"] = map::WAYPOINT;
           json["waypoints"]["result"][i]["ident"] = qUtf8Printable(waypoint.ident);
           json["waypoints"]["result"][i]["type"] = qUtf8Printable(waypoint.type);
           json["waypoints"]["result"][i]["position"] = coordinatesToJSON(getCoordinates(waypoint.position));
           json["waypoints"]["result"][i]["elevation"] =  waypoint.getAltitude();

       }

    return json.dump().data();

}

QByteArray JsonInfoBuilder::feature(MapFeaturesData mapFeaturesData) const
{

    MapFeaturesData data = mapFeaturesData;

    JSON json;

       json = {
           { "airports", JSON::object() },
           { "ndbs", JSON::object() },
           { "vors", JSON::object() },
           { "markers", JSON::object() },
           { "waypoints", JSON::object() },
       };

       json["airports"].push_back({ "count", data.airports.count() });
       json["airports"].push_back({ "result", JSON::array() });

       json["ndbs"].push_back({ "count", data.ndbs.count() });
       json["ndbs"].push_back({ "result", JSON::array() });

       json["vors"].push_back({ "count", data.vors.count() });
       json["vors"].push_back({ "result", JSON::array() });

       json["markers"].push_back({ "count", data.markers.count() });
       json["markers"].push_back({ "result", JSON::array() });

       json["waypoints"].push_back({ "count", data.waypoints.count() });
       json["waypoints"].push_back({ "result", JSON::array() });

       for(int i = 0; i < data.airports.count(); ++i){

           map::MapAirport airport =  data.airports[i];

           json["airports"]["result"][i]["object_id"] = airport.id;
           json["airports"]["result"][i]["type_id"] = map::AIRPORT;
           json["airports"]["result"][i]["ident"] = qUtf8Printable(airport.ident);
           json["airports"]["result"][i]["name"] = qUtf8Printable(airport.name);
           json["airports"]["result"][i]["position"] = coordinatesToJSON(getCoordinates(airport.position));
           json["airports"]["result"][i]["elevation"] =  airport.getAltitude();

//           JSON airportJson;

//           airportJson = {
//               { "ident", qUtf8Printable(airport.ident) },
//               { "name", qUtf8Printable(airport.name)},
//               { "position", coordinatesToJSON(getCoordinates(airport.position))},
//               { "elevation", airport.getPosition().getAltitude() },
//           };

//           json["airports"]["result"].push_back(airportJson);
       }

       for(int i = 0; i < data.ndbs.count(); ++i){

           map::MapNdb ndb =  data.ndbs[i];

           json["ndbs"]["result"][i]["object_id"] = ndb.id;
           json["ndbs"]["result"][i]["type_id"] = map::NDB;
           json["ndbs"]["result"][i]["ident"] = qUtf8Printable(ndb.ident);
           json["ndbs"]["result"][i]["name"] = qUtf8Printable(ndb.name);
           json["ndbs"]["result"][i]["position"] = coordinatesToJSON(getCoordinates(ndb.position));
           json["ndbs"]["result"][i]["elevation"] =  ndb.getAltitude();

       }

       for(int i = 0; i < data.vors.count(); ++i){

           map::MapVor vor =  data.vors[i];

           json["vors"]["result"][i]["object_id"] = vor.id;
           json["vors"]["result"][i]["type_id"] = map::VOR;
           json["vors"]["result"][i]["ident"] = qUtf8Printable(vor.ident);
           json["vors"]["result"][i]["name"] = qUtf8Printable(vor.name);
           json["vors"]["result"][i]["position"] = coordinatesToJSON(getCoordinates(vor.position));
           json["vors"]["result"][i]["elevation"] =  vor.getAltitude();

       }

       for(int i = 0; i < data.markers.count(); ++i){

           map::MapMarker marker =  data.markers[i];

           json["markers"]["result"][i]["object_id"] = marker.id;
           json["markers"]["result"][i]["type_id"] = map::MARKER;
           json["markers"]["result"][i]["ident"] = qUtf8Printable(marker.ident);
           json["markers"]["result"][i]["type"] = qUtf8Printable(marker.type);
           json["markers"]["result"][i]["position"] = coordinatesToJSON(getCoordinates(marker.position));
           json["markers"]["result"][i]["elevation"] =  marker.getAltitude();

       }

       for(int i = 0; i < data.waypoints.count(); ++i){

           map::MapWaypoint waypoint =  data.waypoints[i];

           json["waypoints"]["result"][i]["object_id"] = waypoint.id;
           json["waypoints"]["result"][i]["type_id"] = map::WAYPOINT;
           json["waypoints"]["result"][i]["ident"] = qUtf8Printable(waypoint.ident);
           json["waypoints"]["result"][i]["type"] = qUtf8Printable(waypoint.type);
           json["waypoints"]["result"][i]["position"] = coordinatesToJSON(getCoordinates(waypoint.position));
           json["waypoints"]["result"][i]["elevation"] =  waypoint.getAltitude();

       }

    return json.dump().data();

}

