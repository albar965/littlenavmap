/*****************************************************************************
* Copyright 2015-2026 Alexander Barthel alex@littlenavmap.org
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

#include "common/maptypes.h"
#include "fs/common/binarymsageometry.h"
#include "geo/calculations.h"
#include "io/binaryutil.h"
#include "sql/sqlrecord.h"
#include "fs/util/fsutil.h"

using namespace atools::geo;
using atools::sql::SqlRecord;
using namespace map;

void MapTypesFactory::fillAirport(const SqlRecord& record, map::MapAirport& airport, bool complete, bool nav, bool xplane)
{
  fillAirportBase(record, airport, complete);
  airport.navdata = nav;
  airport.xplane = xplane;

  if(complete)
  {
    airport.flags = fillAirportFlags(record, false);
    if(record.contains(QStringLiteral("has_tower_object")))
      airport.towerCoords = Pos(record.valueFloat(QStringLiteral("tower_lonx")), record.valueFloat(QStringLiteral("tower_laty")));

    airport.atisFrequency = record.valueInt(QStringLiteral("atis_frequency"));
    airport.awosFrequency = record.valueInt(QStringLiteral("awos_frequency"));
    airport.asosFrequency = record.valueInt(QStringLiteral("asos_frequency"));
    airport.unicomFrequency = record.valueInt(QStringLiteral("unicom_frequency"));
    airport.position = Pos(record.valueFloat(QStringLiteral("lonx")), record.valueFloat(QStringLiteral("laty")),
                           record.valueFloat(QStringLiteral("altitude")));
    airport.region = record.valueStr(QStringLiteral("region"), QString());
  }
  else
    airport.position = Pos(record.valueFloat(QStringLiteral("lonx")), record.valueFloat(QStringLiteral("laty")), 0.f);
}

void MapTypesFactory::fillRunway(const atools::sql::SqlRecord& record, map::MapRunway& runway, bool overview)
{
  runway.id = record.valueInt(QStringLiteral("runway_id"));

  if(!overview)
  {
    runway.surface = record.valueStr(QStringLiteral("surface"));
    runway.shoulder = record.valueStr(QStringLiteral("shoulder"), QString()); // Optional X-Plane field
    runway.primaryName = record.valueStr(QStringLiteral("primary_name"));
    runway.secondaryName = record.valueStr(QStringLiteral("secondary_name"));
    runway.edgeLight = record.valueStr(QStringLiteral("edge_light"));
    runway.width = record.valueFloat(QStringLiteral("width"));
    runway.primaryOffset = record.valueFloat(QStringLiteral("primary_offset_threshold"));
    runway.secondaryOffset = record.valueFloat(QStringLiteral("secondary_offset_threshold"));
    runway.primaryBlastPad = record.valueFloat(QStringLiteral("primary_blast_pad"));
    runway.secondaryBlastPad = record.valueFloat(QStringLiteral("secondary_blast_pad"));
    runway.primaryOverrun = record.valueFloat(QStringLiteral("primary_overrun"));
    runway.secondaryOverrun = record.valueFloat(QStringLiteral("secondary_overrun"));
    runway.primaryClosed = record.valueBool(QStringLiteral("primary_closed_markings"));
    runway.secondaryClosed = record.valueBool(QStringLiteral("secondary_closed_markings"));
  }
  else
  {
    runway.width = 0.f;
    runway.primaryOffset = 0.f;
    runway.secondaryOffset = 0.f;
    runway.primaryBlastPad = 0.f;
    runway.secondaryBlastPad = 0.f;
    runway.primaryOverrun = 0.f;
    runway.secondaryOverrun = 0.f;
    runway.primaryClosed = false;
    runway.secondaryClosed = false;
  }

  runway.primaryEndId = record.valueInt(QStringLiteral("primary_end_id"), -1);
  runway.secondaryEndId = record.valueInt(QStringLiteral("secondary_end_id"), -1);

  // Optional in AirportQuery::getRunways
  runway.airportId = record.valueInt(QStringLiteral("airport_id"), -1);

  runway.smoothness = record.valueFloat(QStringLiteral("smoothness"), -1.f);
  runway.length = record.valueFloat(QStringLiteral("length"));
  runway.heading = record.valueFloat(QStringLiteral("heading"));
  runway.patternAlt = record.valueFloat(QStringLiteral("pattern_altitude"), 0.f);

  float altitude = record.valueFloat(QStringLiteral("altitude"), 0.f);
  runway.position = Pos(record.valueFloat(QStringLiteral("lonx")), record.valueFloat(QStringLiteral("laty")), altitude);
  runway.primaryPosition = Pos(record.valueFloat(QStringLiteral("primary_lonx")), record.valueFloat(QStringLiteral("primary_laty")),
                               altitude);
  runway.secondaryPosition = Pos(record.valueFloat(QStringLiteral("secondary_lonx")), record.valueFloat(QStringLiteral("secondary_laty")),
                                 altitude);
}

void MapTypesFactory::fillRunwayEnd(const atools::sql::SqlRecord& record, MapRunwayEnd& end, bool nav)
{
  end.navdata = nav;
  end.name = record.valueStr(QStringLiteral("name"));
  end.position = Pos(record.valueFloat(QStringLiteral("lonx")), record.valueFloat(QStringLiteral("laty")),
                     record.valueFloat(QStringLiteral("altitude"), 0.f));
  end.secondary = record.valueStr(QStringLiteral("end_type")) == QStringLiteral("S");
  end.heading = record.valueFloat(QStringLiteral("heading"));
  end.id = record.valueInt(QStringLiteral("runway_end_id"));
  end.leftVasiPitch = record.valueFloat(QStringLiteral("left_vasi_pitch"));
  end.rightVasiPitch = record.valueFloat(QStringLiteral("right_vasi_pitch"));
  end.leftVasiType = record.valueStr(QStringLiteral("left_vasi_type"));
  end.rightVasiType = record.valueStr(QStringLiteral("right_vasi_type"));
  end.pattern = record.valueStr(QStringLiteral("is_pattern"), QString());
}

void MapTypesFactory::fillAirportBase(const SqlRecord& record, map::MapAirport& ap, bool complete)
{
  ap.id = record.valueInt(QStringLiteral("airport_id"));
  ap.rating = record.valueInt(QStringLiteral("rating"));

  if(complete)
  {
    ap.towerFrequency = record.valueInt(QStringLiteral("tower_frequency"));
    ap.ident = record.valueStr(QStringLiteral("ident"));
    ap.icao = record.valueStr(QStringLiteral("icao"), QString());
    ap.iata = record.valueStr(QStringLiteral("iata"), QString());
    ap.faa = record.valueStr(QStringLiteral("faa"), QString());
    ap.local = record.valueStr(QStringLiteral("local"), QString());
    ap.name = record.valueStr(QStringLiteral("name"));
    ap.type = static_cast<map::MapAirportType>(record.valueInt(QStringLiteral("type"), map::AP_TYPE_NONE));
    ap.longestRunwayLength = record.valueInt(QStringLiteral("longest_runway_length"));
    ap.longestRunwayHeading = atools::roundToInt(record.valueFloat(QStringLiteral("longest_runway_heading")));
    ap.magvar = record.valueFloat(QStringLiteral("mag_var"));
    ap.transitionAltitude = record.valueFloat(QStringLiteral("transition_altitude"), 0.f);
    ap.transitionLevel = record.valueFloat(QStringLiteral("transition_level"), 0.f);

    if(record.contains(QStringLiteral("flatten")))
      ap.flatten = record.isNull(QStringLiteral("flatten")) ? -1 : record.valueInt(QStringLiteral("flatten"));

    ap.bounding = Rect(record.valueFloat(QStringLiteral("left_lonx")), record.valueFloat(QStringLiteral("top_laty")),
                       record.valueFloat(QStringLiteral("right_lonx")), record.valueFloat(QStringLiteral("bottom_laty")));
    ap.flags |= AP_COMPLETE;
  }
}

map::MapAirportFlags MapTypesFactory::fillAirportFlags(const SqlRecord& record, bool overview)
{
  MapAirportFlags flags = AP_NONE;
  flags |= airportFlag(record, QStringLiteral("num_helipad"), AP_HELIPAD);
  flags |= airportFlag(record, QStringLiteral("has_avgas"), AP_AVGAS);
  flags |= airportFlag(record, QStringLiteral("has_jetfuel"), AP_JETFUEL);
  flags |= airportFlag(record, QStringLiteral("tower_frequency"), AP_TOWER);
  flags |= airportFlag(record, QStringLiteral("is_closed"), AP_CLOSED);
  flags |= airportFlag(record, QStringLiteral("is_military"), AP_MIL);
  flags |= airportFlag(record, QStringLiteral("is_addon"), AP_ADDON);
  flags |= airportFlag(record, QStringLiteral("is_3d"), AP_3D);
  flags |= airportFlag(record, QStringLiteral("num_runway_hard"), AP_HARD);
  flags |= airportFlag(record, QStringLiteral("num_runway_soft"), AP_SOFT);
  flags |= airportFlag(record, QStringLiteral("num_runway_water"), AP_WATER);

  if(!overview)
  {
    // The procedure flag is not accurate for mixed mode databases and is updated later on by
    // AirportQuery::hasAirportProcedures()
    flags |= airportFlag(record, QStringLiteral("num_approach"), AP_PROCEDURE);
    flags |= airportFlag(record, QStringLiteral("num_runway_light"), AP_LIGHT);
    flags |= airportFlag(record, QStringLiteral("num_runway_end_ils"), AP_ILS);

    flags |= airportFlag(record, QStringLiteral("num_apron"), AP_APRON);
    flags |= airportFlag(record, QStringLiteral("num_taxi_path"), AP_TAXIWAY);
    flags |= airportFlag(record, QStringLiteral("has_tower_object"), AP_TOWER_OBJ);

    flags |= airportFlag(record, QStringLiteral("num_parking_gate"), AP_PARKING);
    flags |= airportFlag(record, QStringLiteral("num_parking_ga_ramp"), AP_PARKING);
    flags |= airportFlag(record, QStringLiteral("num_parking_cargo"), AP_PARKING);
    flags |= airportFlag(record, QStringLiteral("num_parking_mil_cargo"), AP_PARKING);
    flags |= airportFlag(record, QStringLiteral("num_parking_mil_combat"), AP_PARKING);

    flags |= airportFlag(record, QStringLiteral("num_runway_end_vasi"), AP_VASI);
    flags |= airportFlag(record, QStringLiteral("num_runway_end_als"), AP_ALS);
    flags |= airportFlag(record, QStringLiteral("num_runway_end_closed"), AP_RW_CLOSED);

  }
  else
  {
    if(record.valueInt(QStringLiteral("rating")) > 0)
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

  vor.dmeOnly = record.valueInt(QStringLiteral("dme_only")) > 0;
  vor.hasDme = !record.isNull(QStringLiteral("dme_altitude"));
}

void MapTypesFactory::fillVorFromNav(const SqlRecord& record, map::MapVor& vor)
{
  fillVorBase(record, vor);

  QString navType = record.valueStr(QStringLiteral("nav_type"));
  if(navType == QStringLiteral("TC"))
  {
    vor.dmeOnly = false;
    vor.hasDme = true;
    vor.tacan = true;
    vor.vortac = false;
  }
  else if(navType == QStringLiteral("TCD"))
  {
    vor.dmeOnly = true;
    vor.hasDme = true;
    vor.tacan = true;
    vor.vortac = false;
  }
  else if(navType == QStringLiteral("VT"))
  {
    vor.dmeOnly = false;
    vor.hasDme = true;
    vor.tacan = false;
    vor.vortac = true;
  }
  else if(navType == QStringLiteral("VTD"))
  {
    vor.dmeOnly = true;
    vor.hasDme = true;
    vor.tacan = false;
    vor.vortac = true;
  }
  else if(navType == QStringLiteral("VD"))
  {
    vor.dmeOnly = false;
    vor.hasDme = true;
    vor.tacan = false;
    vor.vortac = false;
  }
  else if(navType == QStringLiteral("D"))
  {
    vor.dmeOnly = true;
    vor.hasDme = true;
    vor.tacan = false;
    vor.vortac = false;
  }
  else if(navType == QStringLiteral("V"))
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
  vor.id = record.valueInt(QStringLiteral("vor_id"));
  vor.ident = record.valueStr(QStringLiteral("ident"));
  vor.region = record.valueStr(QStringLiteral("region"));
  vor.name = atools::capString(record.valueStr(QStringLiteral("name")));

  // Check also for types from the nav_search table and VORTACs
  QString type = record.valueStr(QStringLiteral("type"));
  if(type == QStringLiteral("VH") || type == QStringLiteral("VTH"))
    vor.type = QStringLiteral("H");
  else if(type == QStringLiteral("VL") || type == QStringLiteral("VTL"))
    vor.type = QStringLiteral("L");
  else if(type == QStringLiteral("VT") || type == QStringLiteral("VTT"))
    vor.type = QStringLiteral("T");
  else
    vor.type = type;

  vor.tacan = type == QStringLiteral("TC");
  vor.vortac = type.startsWith(QStringLiteral("VT"));

  vor.channel = record.valueStr(QStringLiteral("channel"));
  vor.frequency = record.valueInt(QStringLiteral("frequency"));

  vor.range = record.valueInt(QStringLiteral("range"));
  vor.magvar = record.valueFloat(QStringLiteral("mag_var"));

  if(record.isNull(QStringLiteral("altitude")))
    vor.position = Pos(record.valueFloat(QStringLiteral("lonx")), record.valueFloat(QStringLiteral("laty")), INVALID_ALTITUDE_VALUE);
  else
    vor.position = Pos(record.valueFloat(QStringLiteral("lonx")), record.valueFloat(QStringLiteral("laty")),
                       record.valueFloat(QStringLiteral("altitude")));
}

void MapTypesFactory::fillUserdataPoint(const SqlRecord& rec, map::MapUserpoint& obj)
{
  if(!rec.isEmpty())
  {
    obj.id = rec.valueInt(QStringLiteral("userdata_id"));
    obj.ident = rec.valueStr(QStringLiteral("ident"));
    obj.region = rec.valueStr(QStringLiteral("region"));
    obj.name = rec.valueStr(QStringLiteral("name"));
    obj.type = rec.valueStr(QStringLiteral("type"));
    obj.description = rec.valueStr(QStringLiteral("description"));
    obj.tags = rec.valueStr(QStringLiteral("tags"));
    obj.temp = rec.valueBool(QStringLiteral("temp"), false);
    obj.position = atools::geo::Pos(rec.valueFloat(QStringLiteral("lonx")), rec.valueFloat(QStringLiteral("laty")));
  }
}

void MapTypesFactory::fillLogbookEntry(const atools::sql::SqlRecord& rec, MapLogbookEntry& obj)
{
  if(rec.isEmpty())
    return;

  obj.id = rec.valueInt(QStringLiteral("logbook_id"));
  obj.departureIdent = rec.valueStr(QStringLiteral("departure_ident")).toUpper();
  obj.departureName = rec.valueStr(QStringLiteral("departure_name"));
  obj.departureRunway = rec.valueStr(QStringLiteral("departure_runway"));

  obj.departurePos = atools::geo::Pos(rec.value(QStringLiteral("departure_lonx")), rec.value(QStringLiteral("departure_laty")),
                                      rec.value(QStringLiteral("departure_alt")));

  obj.destinationIdent = rec.valueStr(QStringLiteral("destination_ident")).toUpper();
  obj.destinationName = rec.valueStr(QStringLiteral("destination_name"));
  obj.destinationRunway = rec.valueStr(QStringLiteral("destination_runway"));

  obj.destinationPos = atools::geo::Pos(rec.value(QStringLiteral("destination_lonx")), rec.value(QStringLiteral("destination_laty")),
                                        rec.value(QStringLiteral("destination_alt")));

  obj.routeString = rec.valueStr(QStringLiteral("route_string"));
  obj.simulator = rec.valueStr(QStringLiteral("simulator"));
  obj.description = rec.valueStr(QStringLiteral("description"));

  obj.aircraftType = rec.valueStr(QStringLiteral("aircraft_type"));
  obj.aircraftRegistration = rec.valueStr(QStringLiteral("aircraft_registration"));
  obj.simulator = rec.valueStr(QStringLiteral("simulator"));
  obj.distanceNm = rec.valueFloat(QStringLiteral("distance"));
  obj.distanceFlownNm = rec.valueFloat(QStringLiteral("distance_flown"), 0.f);
  obj.distanceGcNm = atools::geo::meterToNm(obj.lineString().lengthMeter());

  if(!rec.isNull(QStringLiteral("departure_time")) && !rec.isNull(QStringLiteral("destination_time")))
  {
    // Calculate travel time if all times are valid
    obj.travelTimeRealHours =
      static_cast<float>(rec.valueDateTime(QStringLiteral("departure_time")).secsTo(rec.valueDateTime(QStringLiteral("destination_time"))) /
                         3600.);

    // Avoid negative values
    if(obj.travelTimeRealHours < 0.f)
      obj.travelTimeRealHours = 0.f;
  }

  if(!rec.isNull(QStringLiteral("departure_time_sim")) && !rec.isNull(QStringLiteral("destination_time_sim")))
  {
    obj.travelTimeSimHours =
      static_cast<float>(rec.valueDateTime(QStringLiteral("departure_time_sim")).secsTo(rec.valueDateTime(QStringLiteral(
                                                                                                            "destination_time_sim"))) /
                         3600.);
    if(obj.travelTimeSimHours < 0.f)
      obj.travelTimeSimHours = 0.f;
  }

  obj.performanceFile = atools::nativeCleanPath(rec.valueStr(QStringLiteral("performance_file")));
  obj.routeFile = atools::nativeCleanPath(rec.valueStr(QStringLiteral("flightplan_file")));

  if(obj.departurePos.isValid() && obj.destinationPos.isValid())
    obj.position = Line(obj.departurePos, obj.destinationPos).boundingRect().getCenter();
  else if(obj.departurePos.isValid())
    obj.position = obj.departurePos;
  else if(obj.destinationPos.isValid())
    obj.position = obj.destinationPos;
}

void MapTypesFactory::fillNdb(const SqlRecord& record, map::MapNdb& ndb)
{
  ndb.id = record.valueInt(QStringLiteral("ndb_id"));
  ndb.ident = record.valueStr(QStringLiteral("ident"));
  ndb.region = record.valueStr(QStringLiteral("region"));
  ndb.name = atools::capString(record.valueStr(QStringLiteral("name")));
  ndb.type = record.valueStr(QStringLiteral("type"));
  ndb.frequency = record.valueInt(QStringLiteral("frequency"));
  ndb.range = record.valueInt(QStringLiteral("range"));
  ndb.magvar = record.valueFloat(QStringLiteral("mag_var"));

  if(record.isNull(QStringLiteral("altitude")))
    ndb.position = Pos(record.valueFloat(QStringLiteral("lonx")), record.valueFloat(QStringLiteral("laty")), INVALID_ALTITUDE_VALUE);
  else
    ndb.position = Pos(record.valueFloat(QStringLiteral("lonx")), record.valueFloat(QStringLiteral("laty")),
                       record.valueFloat(QStringLiteral("altitude")));
}

void MapTypesFactory::fillHelipad(const SqlRecord& record, map::MapHelipad& helipad)
{
  helipad.position = Pos(record.value(QStringLiteral("lonx")).toFloat(), record.value(QStringLiteral("laty")).toFloat());

  helipad.start = record.isNull(QStringLiteral("start_number")) ? -1 : record.value(QStringLiteral("start_number")).toInt();

  helipad.id = record.valueInt(QStringLiteral("helipad_id"));
  helipad.startId = record.isNull(QStringLiteral("start_id")) ? -1 : record.valueInt(QStringLiteral("start_id"));
  helipad.airportId = record.valueInt(QStringLiteral("airport_id"));
  helipad.runwayName = record.value(QStringLiteral("runway_name")).toString();
  helipad.width = record.value(QStringLiteral("width")).toInt();
  helipad.length = record.value(QStringLiteral("length")).toInt();
  helipad.heading = static_cast<int>(std::roundf(record.value(QStringLiteral("heading")).toFloat()));
  helipad.surface = record.value(QStringLiteral("surface")).toString();
  helipad.type = record.value(QStringLiteral("type")).toString();
  helipad.transparent = record.value(QStringLiteral("is_transparent")).toInt() > 0;
  helipad.closed = record.value(QStringLiteral("is_closed")).toInt() > 0;
}

void MapTypesFactory::fillWaypoint(const SqlRecord& record, map::MapWaypoint& waypoint, bool track)
{
  waypoint.id = record.valueInt(track ? QStringLiteral("trackpoint_id") : QStringLiteral("waypoint_id"));
  waypoint.ident = record.valueStr(QStringLiteral("ident"));
  waypoint.region = record.valueStr(QStringLiteral("region"));
  waypoint.name = record.valueStr(QStringLiteral("name"), QString());
  waypoint.type = record.valueStr(QStringLiteral("type"));
  waypoint.arincType = record.valueStr(QStringLiteral("arinc_type"), QString());
  waypoint.magvar = record.valueFloat(QStringLiteral("mag_var"));
  waypoint.hasVictorAirways = record.valueInt(QStringLiteral("num_victor_airway")) > 0;
  waypoint.hasJetAirways = record.valueInt(QStringLiteral("num_jet_airway")) > 0;
  waypoint.artificial = static_cast<map::MapWaypointArtificial>(record.valueInt(QStringLiteral("artificial"),
                                                                                map::WAYPOINT_ARTIFICIAL_NONE));
  waypoint.hasTracks = track;
  waypoint.position = Pos(record.valueFloat(QStringLiteral("lonx")), record.valueFloat(QStringLiteral("laty")));
}

void MapTypesFactory::fillWaypointFromNav(const SqlRecord& record, map::MapWaypoint& waypoint)
{
  waypoint.id = record.valueInt(QStringLiteral("waypoint_id"));
  waypoint.ident = record.valueStr(QStringLiteral("ident"));
  waypoint.region = record.valueStr(QStringLiteral("region"));
  waypoint.name = record.valueStr(QStringLiteral("name"), QString());
  waypoint.type = record.valueStr(QStringLiteral("type"));
  waypoint.arincType = record.valueStr(QStringLiteral("arinc_type"), QString());
  waypoint.magvar = record.valueFloat(QStringLiteral("mag_var"));
  waypoint.hasVictorAirways = record.valueInt(QStringLiteral("waypoint_num_victor_airway")) > 0;
  waypoint.hasJetAirways = record.valueInt(QStringLiteral("waypoint_num_jet_airway")) > 0;
  waypoint.artificial = static_cast<map::MapWaypointArtificial>(record.valueInt(QStringLiteral("artificial"),
                                                                                map::WAYPOINT_ARTIFICIAL_NONE));
  waypoint.position = Pos(record.valueFloat(QStringLiteral("lonx")), record.valueFloat(QStringLiteral("laty")));
}

void MapTypesFactory::fillAirwayOrTrack(const SqlRecord& record, map::MapAirway& airway, bool track)
{
  airway.sequence = record.valueInt(QStringLiteral("sequence_no"));
  airway.fromWaypointId = record.valueInt(QStringLiteral("from_waypoint_id"));
  airway.toWaypointId = record.valueInt(QStringLiteral("to_waypoint_id"));
  airway.from = Pos(record.valueFloat(QStringLiteral("from_lonx")), record.valueFloat(QStringLiteral("from_laty")));
  airway.to = Pos(record.valueFloat(QStringLiteral("to_lonx")), record.valueFloat(QStringLiteral("to_laty")));

  // Correct special cases at the anti-meridian to avoid "round-the-globe" case
  // Happens if an airway or track starts or ends at the anti-meridian
  if(airway.from.getLonX() < 0.f && atools::almostEqual(airway.to.getLonX(), 180.f))
    airway.to.setLonX(-180.f);
  if(airway.to.getLonX() < 0.f && atools::almostEqual(airway.from.getLonX(), 180.f))
    airway.from.setLonX(-180.f);

  if(airway.from.getLonX() > 0.f && atools::almostEqual(airway.to.getLonX(), -180.f))
    airway.to.setLonX(180.f);
  if(airway.to.getLonX() > 0.f && atools::almostEqual(airway.from.getLonX(), -180.f))
    airway.from.setLonX(180.f);

  if(airway.from.isValid() && airway.to.isValid())
  {
    Line line(airway.from, airway.to);
    airway.bounding = line.boundingRect();
    airway.position = airway.bounding.getCenter();
    airway.westCourse = line.isWestCourse();
    airway.eastCourse = line.isEastCourse();
  }
  else
    airway.eastCourse = airway.westCourse = false;

  if(track)
  {
    airway.id = record.valueInt(QStringLiteral("track_id"));
    airway.name = record.valueStr(QStringLiteral("track_name"));
    airway.fragment = record.valueInt(QStringLiteral("track_fragment_no"));
    airway.airwayId = record.valueInt(QStringLiteral("airway_id"));

    char type = atools::strToChar(record.valueStr(QStringLiteral("track_type")));
    if(type == 'N')
      airway.type = map::TRACK_NAT;
    else if(type == 'P')
      airway.type = map::TRACK_PACOTS;
    else
      qWarning() << Q_FUNC_INFO << "Invalid track type" << record.valueStr("track_type");

    airway.routeType = map::RT_TRACK;

    // All points are plotted in direction
    airway.direction = map::DIR_FORWARD;

    airway.minAltitude = record.valueInt(QStringLiteral("airway_minimum_altitude"));
    if(record.contains(QStringLiteral("airway_maximum_altitude")))
      airway.maxAltitude = record.valueInt(QStringLiteral("airway_maximum_altitude"));
    else
      airway.maxAltitude = map::MapAirway::MAX_ALTITUDE_LIMIT_FT;

    airway.altitudeLevelsEast = atools::io::readVector<quint16,
                                                       quint16>(record.value(QStringLiteral("altitude_levels_east")).toByteArray());
    airway.altitudeLevelsWest = atools::io::readVector<quint16,
                                                       quint16>(record.value(QStringLiteral("altitude_levels_west")).toByteArray());

    // from_waypoint_name varchar(15),      -- Original name - also for coordinate formats
    // to_waypoint_name varchar(15),        -- "
  }
  else
  {
    airway.id = record.valueInt(QStringLiteral("airway_id"));
    airway.type = airwayTrackTypeFromString(record.valueStr(QStringLiteral("airway_type")));
    airway.routeType = airwayRouteTypeFromString(record.valueStr(QStringLiteral("route_type"), QString()));
    airway.name = record.valueStr(QStringLiteral("airway_name"));

    airway.minAltitude = record.valueInt(QStringLiteral("minimum_altitude"));
    if(record.contains(QStringLiteral("maximum_altitude")) && record.valueInt(QStringLiteral("maximum_altitude")) > 0)
      airway.maxAltitude = record.valueInt(QStringLiteral("maximum_altitude"));
    else
      airway.maxAltitude = map::MapAirway::MAX_ALTITUDE_LIMIT_FT;

    if(record.contains(QStringLiteral("direction")))
    {
      char dir = atools::strToChar(record.valueStr(QStringLiteral("direction")));
      if(dir == 'F')
        airway.direction = map::DIR_FORWARD;
      else if(dir == 'B')
        airway.direction = map::DIR_BACKWARD;
      else // if(dir == 'N')
        airway.direction = map::DIR_BOTH;
      // else
      // qWarning() << Q_FUNC_INFO << "Invalid airway directions" << dir;
    }

    airway.fragment = record.valueInt(QStringLiteral("airway_fragment_no"));
  }
}

void MapTypesFactory::fillMarker(const SqlRecord& record, map::MapMarker& marker)
{
  marker.id = record.valueInt(QStringLiteral("marker_id"));
  marker.type = record.valueStr(QStringLiteral("type"));
  marker.ident = record.valueStr(QStringLiteral("ident"));
  marker.heading = static_cast<int>(std::round(record.valueFloat(QStringLiteral("heading"))));
  marker.position = Pos(record.valueFloat(QStringLiteral("lonx")),
                        record.valueFloat(QStringLiteral("laty")));
}

void MapTypesFactory::fillIls(const SqlRecord& record, map::MapIls& ils, float runwayHeadingTrue)
{
  ils.id = record.valueInt(QStringLiteral("ils_id"));
  ils.airportIdent = record.valueStr(QStringLiteral("loc_airport_ident"));
  ils.runwayEndId = record.valueInt(QStringLiteral("loc_runway_end_id"));
  ils.runwayName = record.valueStr(QStringLiteral("loc_runway_name"));
  ils.ident = record.valueStr(QStringLiteral("ident"));
  ils.name = record.valueStr(QStringLiteral("name"));
  ils.region = record.valueStr(QStringLiteral("region"), QString());

  ils.type = static_cast<map::IlsType>(atools::strToChar(record.valueStr(QStringLiteral("type"), QString())));
  ils.perfIndicator = record.valueStr(QStringLiteral("perf_indicator"), QString());
  ils.provider = record.valueStr(QStringLiteral("provider"), QString());

  ils.displayHeading = ils.heading = atools::geo::normalizeCourse(record.valueFloat(QStringLiteral("loc_heading")));
  ils.width = record.valueFloat(QStringLiteral("loc_width"));
  ils.magvar = record.valueFloat(QStringLiteral("mag_var"));
  ils.slope = record.valueFloat(QStringLiteral("gs_pitch"));

  ils.frequency = record.valueInt(QStringLiteral("frequency"));
  ils.range = record.valueInt(QStringLiteral("range"));
  ils.hasDme = record.valueInt(QStringLiteral("dme_range")) > 0;
  ils.hasBackcourse = record.valueInt(QStringLiteral("has_backcourse")) > 0;

  if(!record.isNull(QStringLiteral("lonx")) && !record.isNull(QStringLiteral("laty")))
  {
    ils.corrected = false;
    ils.position = Pos(record.valueFloat(QStringLiteral("lonx")), record.valueFloat(QStringLiteral("laty")),
                       record.valueFloat(QStringLiteral("altitude")));

    // Read DME antenna position if available (separate from LOC antenna)
    if(!record.isNull(QStringLiteral("dme_lonx")) && !record.isNull(QStringLiteral("dme_laty")))
      ils.dmePos = Pos(record.valueFloat(QStringLiteral("dme_lonx")), record.valueFloat(QStringLiteral("dme_laty")));
    else if(ils.hasDme)
      // Fallback: if DME exists but no separate position, use LOC position
      ils.dmePos = ils.position;

    // Align with runway heading if given and angles are close
    if(runwayHeadingTrue < map::INVALID_HEADING_VALUE)
    {
      // Adjust ILS display heading if value is really close to runway heading
      float diff = atools::geo::angleAbsDiff(ils.displayHeading, runwayHeadingTrue);
      if(diff < 1.f)
      {
#ifdef DEBUG_INFORMATION
        qDebug() << Q_FUNC_INFO << "Correcting ILS" << ils.ident << "diff" << diff;
#endif
        ils.displayHeading = runwayHeadingTrue;

        // Calculate the geometry with slightly corrected angle
        atools::fs::util::calculateIlsGeometry(ils.position, ils.displayHeading, ils.localizerWidth(),
                                               atools::fs::util::DEFAULT_FEATHER_LEN_NM, ils.pos1, ils.pos2, ils.posmid);
        ils.corrected = true;
      }
    }

    if(!ils.corrected)
    {
      // Use pre-calculated geometry for high level zoom since runway heading is not given
      if(!record.isNull(QStringLiteral("end1_lonx")) && !record.isNull(QStringLiteral("end1_laty")) &&
         !record.isNull(QStringLiteral("end2_lonx")) && !record.isNull(QStringLiteral("end2_laty")) &&
         !record.isNull(QStringLiteral("end_mid_lonx")) && !record.isNull(QStringLiteral("end_mid_laty")))
      {
        ils.pos1 = Pos(record.valueFloat(QStringLiteral("end1_lonx")), record.valueFloat(QStringLiteral("end1_laty")));
        ils.pos2 = Pos(record.valueFloat(QStringLiteral("end2_lonx")), record.valueFloat(QStringLiteral("end2_laty")));
        ils.posmid = Pos(record.valueFloat(QStringLiteral("end_mid_lonx")), record.valueFloat(QStringLiteral("end_mid_laty")));
      }
      else
        // Coordinates are not complete from database
        atools::fs::util::calculateIlsGeometry(ils.position, ils.displayHeading, ils.localizerWidth(),
                                               atools::fs::util::DEFAULT_FEATHER_LEN_NM, ils.pos1, ils.pos2, ils.posmid);
    }
  }

  ils.hasGeometry = ils.position.isValid() && ils.pos1.isValid() && ils.pos2.isValid() && ils.posmid.isValid();
  ils.bounding = Rect(LineString({ils.position, ils.pos1, ils.pos2}));
}

map::MapType MapTypesFactory::strToType(const QString& navType)
{
  if(navType == QStringLiteral("W"))
    return map::WAYPOINT;
  else if(navType == QStringLiteral("N"))
    return map::NDB;
  else if(navType == QStringLiteral("V"))
    return map::VOR; // VOR/TACAN/DME
  else if(navType == QStringLiteral("A"))
    return map::AIRPORT;
  else if(navType == QStringLiteral("I"))
    return map::ILS;
  else if(navType == QStringLiteral("R"))
    return map::RUNWAYEND;

  return map::NONE;
}

void MapTypesFactory::fillHolding(const atools::sql::SqlRecord& record, map::MapHolding& holding)
{
  holding.id = record.valueInt(QStringLiteral("holding_id"));
  holding.airportIdent = record.valueStr(QStringLiteral("airport_ident"));
  // airport_ident varchar(5),           -- ICAO ident
  holding.navIdent = record.valueStr(QStringLiteral("nav_ident"));
  // region varchar(2),                  -- ICAO two letter region identifier
  holding.position = Pos(record.valueFloat(QStringLiteral("lonx")), record.valueFloat(QStringLiteral("laty")));

  holding.navType = strToType(record.valueStr(QStringLiteral("nav_type")));
  holding.name = record.valueStr(QStringLiteral("name"));

  holding.vorType = record.valueStr(QStringLiteral("vor_type"));
  holding.vorTacan = holding.vorType == QStringLiteral("TC");
  holding.vorVortac = holding.vorType.startsWith(QStringLiteral("VT"));
  holding.vorDmeOnly = record.valueInt(QStringLiteral("vor_dme_only"));
  holding.vorHasDme = record.valueInt(QStringLiteral("vor_has_dme"));

  // if(atools::almostEqual(record.valueFloat("mag_var"), 0.f) && (holding.vorType.isEmpty() || holding.vorDmeOnly))
  //// Calculate variance if not given except for VOR, VORTAC, VORDME and TACAN
  // holding.magvar = NavApp::getMagVar(holding.position);
  // else
  holding.magvar = record.valueFloat(QStringLiteral("mag_var")); // Magnetic variance in degree < 0 for West and > 0 for East

  holding.courseTrue = atools::geo::normalizeCourse(record.valueFloat(QStringLiteral("course")) + holding.magvar);

  holding.turnLeft = record.valueStr(QStringLiteral("turn_direction")) == QStringLiteral("L");
  holding.length = record.valueFloat(QStringLiteral("leg_length"));
  holding.time = record.valueFloat(QStringLiteral("leg_time"));
  holding.minAltititude = record.valueFloat(QStringLiteral("minimum_altitude"));
  holding.maxAltititude = record.valueFloat(QStringLiteral("maximum_altitude"));
  holding.speedLimit = record.valueFloat(QStringLiteral("speed_limit"));

  holding.speedKts = 0.f;
}

void MapTypesFactory::fillAirportMsa(const atools::sql::SqlRecord& record, map::MapAirportMsa& airportMsa)
{
  // left_lonx, top_laty, right_lonx, bottom_laty, radius, lonx, laty, geometry
  airportMsa.id = record.valueInt(QStringLiteral("airport_msa_id"));
  airportMsa.airportIdent = record.valueStr(QStringLiteral("airport_ident"));
  airportMsa.navIdent = record.valueStr(QStringLiteral("nav_ident"));
  airportMsa.region = record.valueStr(QStringLiteral("region"));
  airportMsa.multipleCode = record.valueStr(QStringLiteral("multiple_code"));

  airportMsa.vorType = record.valueStr(QStringLiteral("vor_type"));
  airportMsa.vorTacan = airportMsa.vorType == QStringLiteral("TC");
  airportMsa.vorVortac = airportMsa.vorType.startsWith(QStringLiteral("VT"));
  airportMsa.vorDmeOnly = record.valueInt(QStringLiteral("vor_dme_only")) > 0;
  airportMsa.vorHasDme = record.valueInt(QStringLiteral("vor_has_dme")) > 0;

  airportMsa.radius = record.valueFloat(QStringLiteral("radius"));
  airportMsa.trueBearing = record.valueInt(QStringLiteral("true_bearing")) > 0;
  airportMsa.magvar = record.valueFloat(QStringLiteral("mag_var"));
  airportMsa.navType = strToType(record.valueStr(QStringLiteral("nav_type")));
  airportMsa.position = Pos(record.valueFloat(QStringLiteral("lonx")), record.valueFloat(QStringLiteral("laty")));

  atools::fs::common::BinaryMsaGeometry geo(record.valueBytes(QStringLiteral("geometry")));
  airportMsa.bearings = geo.getBearings();
  airportMsa.altitudes = geo.getAltitudes();
  airportMsa.geometry = geo.getGeometry();
  airportMsa.labelPositions = geo.getLabelPositions();
  airportMsa.bearingEndPositions = geo.getBearingEndPositions();

  airportMsa.bounding = Rect(record.valueFloat(QStringLiteral("left_lonx")), record.valueFloat(QStringLiteral("top_laty")),
                             record.valueFloat(QStringLiteral("right_lonx")), record.valueFloat(QStringLiteral("bottom_laty")));
}

void MapTypesFactory::fillParking(const SqlRecord& record, map::MapParking& parking)
{
  parking.id = record.valueInt(QStringLiteral("parking_id"));
  parking.airportId = record.valueInt(QStringLiteral("airport_id"));
  parking.type = record.valueStr(QStringLiteral("type"));
  parking.name = record.valueStr(QStringLiteral("name"));
  parking.suffix = record.valueStr(QStringLiteral("suffix"), QString());
  parking.airlineCodes = record.valueStr(QStringLiteral("airline_codes"));

  parking.position = Pos(record.valueFloat(QStringLiteral("lonx")), record.valueFloat(QStringLiteral("laty")));
  parking.jetway = record.valueInt(QStringLiteral("has_jetway")) > 0;
  parking.number = record.valueInt(QStringLiteral("number"));

  parking.heading = record.isNull(QStringLiteral("heading")) ? map::INVALID_HEADING_VALUE : record.valueFloat(QStringLiteral("heading"));
  parking.radius = static_cast<int>(std::round(record.valueFloat(QStringLiteral("radius"))));

  // Calculate a short text if using X-Plane parking names
  if(parking.number == -1)
  {
    // Look at name components
    const QStringList texts = parking.name.split(QStringLiteral(" "));
    QStringList textsShort;
    bool ok = false;
    for(const QString& txt : texts)
    {
      // Try to extract number
      txt.toInt(&ok);

      // Try to extract prefixed number like B1, A101
      if(!ok)
        txt.mid(1).toInt(&ok);

      // Try suffixed number like 1C, 23D
      if(!ok)
      {
        QString temp(txt);
        temp.chop(1);
        temp.toInt(&ok);
      }

      // Any single upper case letter
      if(!ok)
        ok = txt.size() == 1 && txt.at(0) >= 'A' && txt.at(0) <= 'Z';

      if(ok)
        // Found one number with or without suffix or prefix to build short text
        textsShort.append(txt);
    }
    textsShort.removeAll(QString());

    if(!textsShort.isEmpty())
    {
      // Use first character and last numbers
      textsShort.prepend(texts.constFirst().at(0));
      parking.nameShort = textsShort.join(QStringLiteral(" "));
    }
  }
}

void MapTypesFactory::fillStart(const SqlRecord& record, map::MapStart& start)
{
  start.id = record.valueInt(QStringLiteral("start_id"));
  start.airportId = record.valueInt(QStringLiteral("airport_id"));
  start.type = atools::charAt(record.valueStr(QStringLiteral("type")), 0);
  start.runwayName = record.valueStr(QStringLiteral("runway_name"));
  start.helipadNumber = record.isNull(QStringLiteral("number")) ? -1 : record.valueInt(QStringLiteral("number"), -1);
  start.position = Pos(record.valueFloat(QStringLiteral("lonx")), record.valueFloat(QStringLiteral("laty")),
                       record.valueFloat(QStringLiteral("altitude")));
  start.heading = record.valueFloat(QStringLiteral("heading"));
}

void MapTypesFactory::fillAirspace(const SqlRecord& record, map::MapAirspace& airspace, map::MapAirspaceSource src)
{
  if(record.contains(QStringLiteral("boundary_id")))
    airspace.id = record.valueInt(QStringLiteral("boundary_id"));
  else if(record.contains(QStringLiteral("atc_id")))
    airspace.id = record.valueInt(QStringLiteral("atc_id"));

  airspace.src = src;

  airspace.type = map::airspaceTypeFromDatabase(record.valueStr(QStringLiteral("type")));
  airspace.name =
    airspace.isOnline() ? record.valueStr(QStringLiteral("callsign")).trimmed() :
    atools::fs::util::capNavString(record.valueStr(QStringLiteral("name")));
  airspace.comType = record.valueStr(QStringLiteral("com_type"));

  QString comCol;
  if(record.contains(QStringLiteral("com_frequency")))
    comCol = QStringLiteral("com_frequency");
  else if(record.contains(QStringLiteral("frequency")))
    comCol = QStringLiteral("frequency");

  const QStringList split = record.valueStr(comCol, QString()).split(QStringLiteral("&"));
  for(const QString& str : split)
  {
    bool ok;
    int frequency = str.toInt(&ok);

    if(frequency > 0 && ok)
      airspace.comFrequencies.append(frequency);
  }

  // Use default values for online network ATC centers
  airspace.comName = atools::fs::util::capNavString(record.valueStr(QStringLiteral("com_name"), QString()).trimmed());
  airspace.multipleCode = record.valueStr(QStringLiteral("multiple_code"), QString()).trimmed();
  airspace.restrictiveDesignation = atools::fs::util::capNavString(record.valueStr(QStringLiteral("restrictive_designation"), QString()));
  airspace.restrictiveType = record.valueStr(QStringLiteral("restrictive_type"), QString());
  airspace.timeCode = record.valueStr(QStringLiteral("time_code"), QString());
  airspace.minAltitudeType = record.valueStr(QStringLiteral("min_altitude_type"), QString());
  airspace.maxAltitudeType = record.valueStr(QStringLiteral("max_altitude_type"), QString());
  airspace.maxAltitude = record.valueInt(QStringLiteral("max_altitude"), 0);
  airspace.minAltitude = record.valueInt(QStringLiteral("min_altitude"), 70000);

  Pos topLeft(record.value(QStringLiteral("min_lonx")), record.value(QStringLiteral("max_laty")));
  Pos bottomRight(record.value(QStringLiteral("max_lonx")), record.value(QStringLiteral("min_laty")));

  if(topLeft.isValid() && bottomRight.isValid())
  {
    airspace.bounding = Rect(topLeft, bottomRight);
    airspace.position = airspace.bounding.getCenter();
  }
}
