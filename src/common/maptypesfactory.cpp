/*****************************************************************************
* Copyright 2015-2018 Alexander Barthel albar965@mailbox.org
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

#include "common/maptypesfactory.h"

#include <cmath>
#include "sql/sqlrecord.h"
#include "geo/calculations.h"
#include "common/maptypes.h"

using namespace atools::geo;
using atools::sql::SqlRecord;
using namespace map;

MapTypesFactory::MapTypesFactory()
{

}

MapTypesFactory::~MapTypesFactory()
{

}

void MapTypesFactory::fillAirport(const SqlRecord& record, map::MapAirport& airport, bool complete, bool nav,
                                  bool xplane)
{
  fillAirportBase(record, airport, complete);
  airport.navdata = nav;
  airport.xplane = xplane;

  if(complete)
  {
    airport.flags = fillAirportFlags(record, false);
    if(record.contains("has_tower_object"))
      airport.towerCoords = Pos(record.valueFloat("tower_lonx"), record.valueFloat("tower_laty"));

    airport.atisFrequency = record.valueInt("atis_frequency");
    airport.awosFrequency = record.valueInt("awos_frequency");
    airport.asosFrequency = record.valueInt("asos_frequency");
    airport.unicomFrequency = record.valueInt("unicom_frequency");

    airport.position = Pos(record.valueFloat("lonx"), record.valueFloat("laty"),
                           record.valueFloat("altitude"));

    airport.region = record.valueStr("region", QString());
  }
  else
    airport.position = Pos(record.valueFloat("lonx"), record.valueFloat("laty"), 0.f);
}

void MapTypesFactory::fillAirportForOverview(const SqlRecord& record, map::MapAirport& airport, bool nav, bool xplane)
{
  fillAirportBase(record, airport, true);
  airport.navdata = nav;
  airport.xplane = xplane;

  airport.flags = fillAirportFlags(record, true);
  airport.position = Pos(record.valueFloat("lonx"), record.valueFloat("laty"), 0.f);
}

void MapTypesFactory::fillRunway(const atools::sql::SqlRecord& record, map::MapRunway& runway, bool overview)
{
  if(!overview)
  {
    runway.surface = record.valueStr("surface");
    runway.shoulder = record.valueStr("shoulder", QString()); // Optional X-Plane field
    runway.primaryName = record.valueStr("primary_name");
    runway.secondaryName = record.valueStr("secondary_name");
    runway.edgeLight = record.valueStr("edge_light");
    runway.width = record.valueInt("width");
    runway.primaryOffset = record.valueInt("primary_offset_threshold");
    runway.secondaryOffset = record.valueInt("secondary_offset_threshold");
    runway.primaryBlastPad = record.valueInt("primary_blast_pad");
    runway.secondaryBlastPad = record.valueInt("secondary_blast_pad");
    runway.primaryOverrun = record.valueInt("primary_overrun");
    runway.secondaryOverrun = record.valueInt("secondary_overrun");
    runway.primaryClosed = record.valueBool("primary_closed_markings");
    runway.secondaryClosed = record.valueBool("secondary_closed_markings");
  }
  else
  {
    runway.width = 0;
    runway.primaryOffset = 0;
    runway.secondaryOffset = 0;
    runway.primaryBlastPad = 0;
    runway.secondaryBlastPad = 0;
    runway.primaryOverrun = 0;
    runway.secondaryOverrun = 0;
    runway.primaryClosed = 0;
    runway.secondaryClosed = 0;
  }

  runway.primaryEndId = record.valueInt("primary_end_id", -1);
  runway.secondaryEndId = record.valueInt("secondary_end_id", -1);

  // Optional in AirportQuery::getRunways
  runway.airportId = record.valueInt("airport_id", -1);

  runway.length = record.valueInt("length");
  runway.heading = record.valueFloat("heading");
  runway.position = Pos(record.valueFloat("lonx"), record.valueFloat("laty"));
  runway.primaryPosition = Pos(record.valueFloat("primary_lonx"), record.valueFloat("primary_laty"));
  runway.secondaryPosition = Pos(record.valueFloat("secondary_lonx"), record.valueFloat("secondary_laty"));
}

void MapTypesFactory::fillRunwayEnd(const atools::sql::SqlRecord& record, MapRunwayEnd& end, bool nav)
{
  end.navdata = nav;
  end.name = record.valueStr("name");
  end.position = Pos(record.valueFloat("lonx"), record.valueFloat("laty"));
  end.secondary = record.valueStr("end_type") == "S";
  end.heading = record.valueFloat("heading");
}

void MapTypesFactory::fillAirportBase(const SqlRecord& record, map::MapAirport& ap, bool complete)
{
  ap.id = record.valueInt("airport_id");

  if(complete)
  {
    ap.towerFrequency = record.valueInt("tower_frequency");
    ap.ident = record.valueStr("ident");
    ap.name = record.valueStr("name");
    ap.rating = record.valueInt("rating", -1);
    ap.longestRunwayLength = record.valueInt("longest_runway_length");
    ap.longestRunwayHeading = static_cast<int>(std::round(record.valueFloat("longest_runway_heading")));
    ap.magvar = record.valueFloat("mag_var");
    ap.transitionAltitude = record.valueInt("transition_altitude", 0);

    ap.bounding = Rect(record.valueFloat("left_lonx"), record.valueFloat("top_laty"),
                       record.valueFloat("right_lonx"), record.valueFloat("bottom_laty"));
    ap.flags |= AP_COMPLETE;
  }
}

map::MapAirportFlags MapTypesFactory::fillAirportFlags(const SqlRecord& record, bool overview)
{
  MapAirportFlags flags = 0;
  flags |= airportFlag(record, "num_helipad", AP_HELIPAD);
  flags |= airportFlag(record, "has_avgas", AP_AVGAS);
  flags |= airportFlag(record, "has_jetfuel", AP_JETFUEL);
  flags |= airportFlag(record, "tower_frequency", AP_TOWER);
  flags |= airportFlag(record, "is_closed", AP_CLOSED);
  flags |= airportFlag(record, "is_military", AP_MIL);
  flags |= airportFlag(record, "is_addon", AP_ADDON);
  flags |= airportFlag(record, "is_3d", AP_3D);
  flags |= airportFlag(record, "num_runway_hard", AP_HARD);
  flags |= airportFlag(record, "num_runway_soft", AP_SOFT);
  flags |= airportFlag(record, "num_runway_water", AP_WATER);

  if(!overview)
  {
    flags |= airportFlag(record, "num_approach", AP_PROCEDURE);
    flags |= airportFlag(record, "num_runway_light", AP_LIGHT);
    flags |= airportFlag(record, "num_runway_end_ils", AP_ILS);

    flags |= airportFlag(record, "num_apron", AP_APRON);
    flags |= airportFlag(record, "num_taxi_path", AP_TAXIWAY);
    flags |= airportFlag(record, "has_tower_object", AP_TOWER_OBJ);

    flags |= airportFlag(record, "num_parking_gate", AP_PARKING);
    flags |= airportFlag(record, "num_parking_ga_ramp", AP_PARKING);
    flags |= airportFlag(record, "num_parking_cargo", AP_PARKING);
    flags |= airportFlag(record, "num_parking_mil_cargo", AP_PARKING);
    flags |= airportFlag(record, "num_parking_mil_combat", AP_PARKING);

    flags |= airportFlag(record, "num_runway_end_vasi", AP_VASI);
    flags |= airportFlag(record, "num_runway_end_als", AP_ALS);
    flags |= airportFlag(record, "num_boundary_fence", AP_FENCE);
    flags |= airportFlag(record, "num_runway_end_closed", AP_RW_CLOSED);

  }
  else
  {
    if(record.valueInt("rating") > 0)
    {
      // Force non empty airports for overview results
      flags |= AP_APRON;
      flags |= AP_TAXIWAY;
      flags |= AP_TOWER_OBJ;
    }
  }

  return flags;
}

map::MapAirportFlags MapTypesFactory::airportFlag(const SqlRecord& record, const QString& field,
                                                  map::MapAirportFlags flag)
{
  if(!record.contains(field) || record.isNull(field) || record.valueInt(field) == 0)
    return AP_NONE;
  else
    return flag;
}

void MapTypesFactory::fillVor(const SqlRecord& record, map::MapVor& vor)
{
  fillVorBase(record, vor);

  vor.dmeOnly = record.valueInt("dme_only") > 0;
  vor.hasDme = !record.isNull("dme_altitude");
}

void MapTypesFactory::fillVorFromNav(const SqlRecord& record, map::MapVor& vor)
{
  fillVorBase(record, vor);

  QString navType = record.valueStr("nav_type");
  if(navType == "TC")
  {
    vor.dmeOnly = false;
    vor.hasDme = true;
    vor.tacan = true;
    vor.vortac = false;
  }
  else if(navType == "TCD")
  {
    vor.dmeOnly = true;
    vor.hasDme = true;
    vor.tacan = true;
    vor.vortac = false;
  }
  else if(navType == "VT")
  {
    vor.dmeOnly = false;
    vor.hasDme = true;
    vor.tacan = false;
    vor.vortac = true;
  }
  else if(navType == "VTD")
  {
    vor.dmeOnly = true;
    vor.hasDme = true;
    vor.tacan = false;
    vor.vortac = true;
  }
  else if(navType == "VD")
  {
    vor.dmeOnly = false;
    vor.hasDme = true;
    vor.tacan = false;
    vor.vortac = false;
  }
  else if(navType == "D")
  {
    vor.dmeOnly = true;
    vor.hasDme = true;
    vor.tacan = false;
    vor.vortac = false;
  }
  else if(navType == "V")
  {
    vor.dmeOnly = false;
    vor.hasDme = false;
    vor.tacan = false;
    vor.vortac = false;
  }

  // Adapt to nav_search table frequency scaling
  vor.frequency /= 10;
}

void MapTypesFactory::fillVorBase(const SqlRecord& record, map::MapVor& vor)
{
  vor.id = record.valueInt("vor_id");
  vor.ident = record.valueStr("ident");
  vor.region = record.valueStr("region");
  vor.name = atools::capString(record.valueStr("name"));

  // Check also for types from the nav_search table and VORTACs
  QString type = record.valueStr("type");
  if(type == "VH" || type == "VTH")
    vor.type = "H";
  else if(type == "VL" || type == "VTL")
    vor.type = "L";
  else if(type == "VT" || type == "VTT")
    vor.type = "T";
  else
    vor.type = type;

  vor.tacan = type == "TC";
  vor.vortac = type.startsWith("VT");

  vor.channel = record.valueStr("channel");
  vor.frequency = record.valueInt("frequency");

  vor.range = record.valueInt("range");
  vor.magvar = record.valueFloat("mag_var");

  if(record.isNull("altitude"))
    vor.position = Pos(record.valueFloat("lonx"), record.valueFloat("laty"), INVALID_ALTITUDE_VALUE);
  else
    vor.position = Pos(record.valueFloat("lonx"), record.valueFloat("laty"), record.valueFloat("altitude"));
}

void MapTypesFactory::fillUserdataPoint(const SqlRecord& rec, map::MapUserpoint& obj)
{
  obj.id = rec.valueInt("userdata_id");
  obj.ident = rec.valueStr("ident");
  obj.region = rec.valueStr("region");
  obj.name = rec.valueStr("name");
  obj.type = rec.valueStr("type");
  obj.description = rec.valueStr("description");
  obj.tags = rec.valueStr("tags");
  obj.temp = rec.valueBool("temp", false);
  obj.position = atools::geo::Pos(rec.valueFloat("lonx"), rec.valueFloat("laty"));
}

void MapTypesFactory::fillNdb(const SqlRecord& record, map::MapNdb& ndb)
{
  ndb.id = record.valueInt("ndb_id");
  ndb.ident = record.valueStr("ident");
  ndb.region = record.valueStr("region");
  ndb.name = atools::capString(record.valueStr("name"));
  ndb.type = record.valueStr("type");
  ndb.frequency = record.valueInt("frequency");
  ndb.range = record.valueInt("range");
  ndb.magvar = record.valueFloat("mag_var");

  if(record.isNull("altitude"))
    ndb.position = Pos(record.valueFloat("lonx"), record.valueFloat("laty"), INVALID_ALTITUDE_VALUE);
  else
    ndb.position = Pos(record.valueFloat("lonx"), record.valueFloat("laty"), record.valueFloat("altitude"));
}

void MapTypesFactory::fillHelipad(const SqlRecord& record, map::MapHelipad& helipad)
{
  helipad.position = Pos(record.value("lonx").toFloat(), record.value("laty").toFloat());

  helipad.start = record.isNull("start_number") ? -1 : record.value("start_number").toInt();

  helipad.id = record.valueInt("helipad_id");
  helipad.startId = record.isNull("start_id") ? -1 : record.valueInt("start_id");
  helipad.airportId = record.valueInt("airport_id");
  helipad.runwayName = record.value("runway_name").toString();
  helipad.width = record.value("width").toInt();
  helipad.length = record.value("length").toInt();
  helipad.heading = static_cast<int>(std::roundf(record.value("heading").toFloat()));
  helipad.surface = record.value("surface").toString();
  helipad.type = record.value("type").toString();
  helipad.transparent = record.value("is_transparent").toInt() > 0;
  helipad.closed = record.value("is_closed").toInt() > 0;
}

void MapTypesFactory::fillWaypoint(const SqlRecord& record, map::MapWaypoint& waypoint)
{
  waypoint.id = record.valueInt("waypoint_id");
  waypoint.ident = record.valueStr("ident");
  waypoint.region = record.valueStr("region");
  // waypoint.airportIdent = record.valueStr("region");
  waypoint.type = record.valueStr("type");
  waypoint.magvar = record.valueFloat("mag_var");
  waypoint.hasVictorAirways = record.valueInt("num_victor_airway") > 0;
  waypoint.hasJetAirways = record.valueInt("num_jet_airway") > 0;
  waypoint.position = Pos(record.valueFloat("lonx"), record.valueFloat("laty"));
}

void MapTypesFactory::fillWaypointFromNav(const SqlRecord& record, map::MapWaypoint& waypoint)
{
  waypoint.id = record.valueInt("waypoint_id");
  waypoint.ident = record.valueStr("ident");
  waypoint.region = record.valueStr("region");
  waypoint.type = record.valueStr("type");
  waypoint.magvar = record.valueFloat("mag_var");
  waypoint.hasVictorAirways = record.valueInt("waypoint_num_victor_airway") > 0;
  waypoint.hasJetAirways = record.valueInt("waypoint_num_jet_airway") > 0;
  waypoint.position = Pos(record.valueFloat("lonx"), record.valueFloat("laty"));
}

void MapTypesFactory::fillAirway(const SqlRecord& record, map::MapAirway& airway)
{
  airway.id = record.valueInt("airway_id");
  airway.type = airwayTypeFromString(record.valueStr("airway_type"));
  airway.name = record.valueStr("airway_name");
  airway.minAltitude = record.valueInt("minimum_altitude");

  if(record.contains("maximum_altitude"))
    airway.maxAltitude = record.valueInt("maximum_altitude");

  if(record.contains("direction"))
  {
    QString dir = record.valueStr("direction");
    if(dir == "F")
      airway.direction = map::DIR_FORWARD;
    else if(dir == "B")
      airway.direction = map::DIR_BACKWARD;
    else
      // 'N'
      airway.direction = map::DIR_BOTH;
  }

  airway.fragment = record.valueInt("airway_fragment_no");
  airway.sequence = record.valueInt("sequence_no");
  airway.fromWaypointId = record.valueInt("from_waypoint_id");
  airway.toWaypointId = record.valueInt("to_waypoint_id");
  airway.from = Pos(record.valueFloat("from_lonx"), record.valueFloat("from_laty"));
  airway.to = Pos(record.valueFloat("to_lonx"), record.valueFloat("to_laty"));

  float north = std::max(airway.from.getLatY(), airway.to.getLatY());
  float south = std::min(airway.from.getLatY(), airway.to.getLatY());
  float east = std::max(airway.from.getLonX(), airway.to.getLonX());
  float west = std::min(airway.from.getLonX(), airway.to.getLonX());
  if(east - west > 180.f)
    std::swap(east, west);
  airway.bounding = Rect(west, north, east, south);
}

void MapTypesFactory::fillMarker(const SqlRecord& record, map::MapMarker& marker)
{
  marker.id = record.valueInt("marker_id");
  marker.type = record.valueStr("type");
  marker.ident = record.valueStr("ident");
  marker.heading = static_cast<int>(std::round(record.valueFloat("heading")));
  marker.position = Pos(record.valueFloat("lonx"),
                        record.valueFloat("laty"));
}

void MapTypesFactory::fillIls(const SqlRecord& record, map::MapIls& ils)
{
  ils.id = record.valueInt("ils_id");
  ils.ident = record.valueStr("ident");
  ils.name = record.valueStr("name");
  ils.region = record.valueStr("region", QString());
  ils.heading = record.valueFloat("loc_heading");
  ils.width = record.isNull("loc_width") ? INVALID_COURSE_VALUE : record.valueFloat("loc_width");
  ils.magvar = record.valueFloat("mag_var");
  ils.slope = record.valueFloat("gs_pitch");

  ils.frequency = record.valueInt("frequency");
  ils.range = record.valueInt("range");
  ils.hasDme = record.valueInt("dme_range") > 0;

  ils.position = Pos(record.valueFloat("lonx"), record.valueFloat("laty"),
                     record.valueFloat("altitude"));
  ils.pos1 = Pos(record.valueFloat("end1_lonx"), record.valueFloat("end1_laty"));
  ils.pos2 = Pos(record.valueFloat("end2_lonx"), record.valueFloat("end2_laty"));
  ils.posmid = Pos(record.valueFloat("end_mid_lonx"), record.valueFloat("end_mid_laty"));

  ils.bounding = Rect(ils.position);
  ils.bounding.extend(ils.pos1);
  ils.bounding.extend(ils.pos2);
}

void MapTypesFactory::fillParking(const SqlRecord& record, map::MapParking& parking)
{
  parking.id = record.valueInt("parking_id");
  parking.airportId = record.valueInt("airport_id");
  parking.type = record.valueStr("type");
  parking.name = record.valueStr("name");
  parking.airlineCodes = record.valueStr("airline_codes");

  parking.position = Pos(record.valueFloat("lonx"), record.valueFloat("laty"));
  parking.jetway = record.valueInt("has_jetway") > 0;
  parking.number = record.valueInt("number");

  parking.heading = static_cast<int>(std::round(record.valueFloat("heading")));
  parking.radius = static_cast<int>(std::round(record.valueFloat("radius")));
}

void MapTypesFactory::fillStart(const SqlRecord& record, map::MapStart& start)
{
  start.id = record.valueInt("start_id");
  start.airportId = record.valueInt("airport_id");
  start.type = record.valueStr("type");
  start.runwayName = record.valueStr("runway_name");
  start.helipadNumber = record.valueInt("number");
  start.position = Pos(record.valueFloat("lonx"), record.valueFloat("laty"), record.valueFloat("altitude"));
  start.heading = static_cast<int>(std::roundf(record.valueFloat("heading")));
}

void MapTypesFactory::fillAirspace(const SqlRecord& record, map::MapAirspace& airspace, bool online)
{
  if(record.contains("boundary_id"))
    airspace.id = record.valueInt("boundary_id");
  else if(record.contains("atc_id"))
    airspace.id = record.valueInt("atc_id");

  airspace.online = online;

  airspace.type = map::airspaceTypeFromDatabase(record.valueStr("type"));
  airspace.name = record.valueStr(online ? "callsign" : "name");
  airspace.comType = record.valueStr("com_type");

  for(const QString& str : record.valueStr("com_frequency", QString()).split("&"))
    airspace.comFrequencies.append(str.toInt());

  // Use default values for online network ATC centers
  airspace.comName = record.valueStr("com_name", QString());
  airspace.minAltitudeType = record.valueStr("min_altitude_type", QString());
  airspace.maxAltitudeType = record.valueStr("max_altitude_type", QString());
  airspace.maxAltitude = record.valueInt("max_altitude", 0);
  airspace.minAltitude = record.valueInt("min_altitude", 60000);

  // explicit Rect(double leftLonX, double topLatY, double rightLonX, double bottomLatY);
  airspace.bounding = Rect(record.valueFloat("min_lonx"), record.valueFloat("max_laty"),
                           record.valueFloat("max_lonx"), record.valueFloat("min_laty"));
}
