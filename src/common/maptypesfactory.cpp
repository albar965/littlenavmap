/*****************************************************************************
* Copyright 2015-2016 Alexander Barthel albar965@mailbox.org
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

#include "maptypesfactory.h"
#include "sql/sqlrecord.h"

using namespace atools::geo;
using atools::sql::SqlRecord;

MapTypesFactory::MapTypesFactory()
{

}

MapTypesFactory::~MapTypesFactory()
{

}

void MapTypesFactory::fillAirportForOverview(const SqlRecord& record, maptypes::MapAirport& ap)
{
  // "select airport_id, ident, name, rating, "
  // "has_avgas, has_jetfuel, "
  // "tower_frequency, "
  // "is_closed, is_military, is_addon,"
  // "num_runway_hard, num_runway_soft, num_runway_water, num_helipad, "
  // "longest_runway_length, longest_runway_heading, mag_var, "
  // "lonx, laty, left_lonx, top_laty, right_lonx, bottom_laty "

  fillAirportBase(record, ap, true);

  ap.flags = fillAirportFlags(record, true);
  ap.position = Pos(record.value("lonx").toFloat(), record.value("laty").toFloat(), 0.f);
}

void MapTypesFactory::fillAirport(const SqlRecord& record, maptypes::MapAirport& ap, bool complete)
{
  fillAirportBase(record, ap, complete);

  if(complete)
  {
    ap.flags = fillAirportFlags(record, false);
    if(record.contains("has_tower_object"))
      ap.towerCoords = Pos(record.value("tower_lonx").toFloat(), record.value("tower_laty").toFloat());

    ap.atisFrequency = record.value("atis_frequency").toInt();
    ap.awosFrequency = record.value("awos_frequency").toInt();
    ap.asosFrequency = record.value("asos_frequency").toInt();
    ap.unicomFrequency = record.value("unicom_frequency").toInt();

    ap.position = Pos(record.value("lonx").toFloat(), record.value("laty").toFloat(),
                      record.value("altitude").toFloat());
  }
  else
    ap.position = Pos(record.value("lonx").toFloat(), record.value("laty").toFloat(), 0.f);
}

void MapTypesFactory::fillAirportBase(const SqlRecord& record, maptypes::MapAirport& ap, bool complete)
{
  ap.id = record.value("airport_id").toInt();

  if(complete)
  {
    ap.towerFrequency = record.value("tower_frequency").toInt();
    ap.ident = record.value("ident").toString();
    ap.name = record.value("name").toString();
    ap.longestRunwayLength = record.value("longest_runway_length").toInt();
    ap.longestRunwayHeading = static_cast<int>(std::roundf(record.value("longest_runway_heading").toFloat()));
    ap.magvar = record.value("mag_var").toFloat();

    ap.bounding = Rect(record.value("left_lonx").toFloat(), record.value("top_laty").toFloat(),
                       record.value("right_lonx").toFloat(), record.value("bottom_laty").toFloat());
    ap.flags |= maptypes::AP_COMPLETE;
  }
}

maptypes::MapAirportFlags MapTypesFactory::fillAirportFlags(const SqlRecord& record, bool overview)
{
  using namespace maptypes;

  MapAirportFlags flags = 0;
  flags |= airportFlag(record, "num_helipad", AP_HELIPAD);
  flags |= airportFlag(record, "rating", AP_SCENERY);
  flags |= airportFlag(record, "has_avgas", AP_AVGAS);
  flags |= airportFlag(record, "has_jetfuel", AP_JETFUEL);
  flags |= airportFlag(record, "tower_frequency", AP_TOWER);
  flags |= airportFlag(record, "is_closed", AP_CLOSED);
  flags |= airportFlag(record, "is_military", AP_MIL);
  flags |= airportFlag(record, "is_addon", AP_ADDON);
  flags |= airportFlag(record, "num_runway_hard", AP_HARD);
  flags |= airportFlag(record, "num_runway_soft", AP_SOFT);
  flags |= airportFlag(record, "num_runway_water", AP_WATER);

  if(!overview)
  {
  flags |= airportFlag(record, "num_approach", AP_APPR);
  flags |= airportFlag(record, "num_runway_light", AP_LIGHT);
  flags |= airportFlag(record, "num_runway_end_ils", AP_ILS);
  }

  return flags;
}

maptypes::MapAirportFlags MapTypesFactory::airportFlag(const SqlRecord& record, const QString& field,
                                                       maptypes::MapAirportFlags flag)
{
  if(record.isNull(field) || record.value(field).toInt() == 0)
    return maptypes::AP_NONE;
  else
    return flag;
}

void MapTypesFactory::fillVor(const SqlRecord& record, maptypes::MapVor& vor)
{
  fillVorBase(record, vor);

  vor.dmeOnly = record.value("dme_only").toInt() > 0;
  vor.hasDme = !record.value("dme_altitude").isNull();
}

void MapTypesFactory::fillVorFromNav(const SqlRecord& record, maptypes::MapVor& vor)
{
  fillVorBase(record, vor);

  QString navType = record.value("nav_type").toString();
  if(navType == "VORDME")
  {
    vor.dmeOnly = false;
    vor.hasDme = true;
  }
  else if(navType == "DME")
  {
    vor.dmeOnly = true;
    vor.hasDme = true;
  }
  else if(navType == "VOR")
  {
    vor.dmeOnly = false;
    vor.hasDme = false;
  }

  // Adapt to nav_search table frequency scaling
  vor.frequency /= 10;
}

void MapTypesFactory::fillVorBase(const SqlRecord& record, maptypes::MapVor& vor)
{
  vor.id = record.value("vor_id").toInt();
  vor.ident = record.value("ident").toString();
  vor.region = record.value("region").toString();
  vor.name = record.value("name").toString();
  vor.type = record.value("type").toString();
  vor.frequency = record.value("frequency").toInt();
  vor.range = record.value("range").toInt();
  vor.magvar = record.value("mag_var").toFloat();
  vor.position = Pos(record.value("lonx").toFloat(), record.value("laty").toFloat(),
                     record.value("altitude").toFloat());
}

void MapTypesFactory::fillNdb(const SqlRecord& record, maptypes::MapNdb& ndb)
{
  ndb.id = record.value("ndb_id").toInt();
  ndb.ident = record.value("ident").toString();
  ndb.region = record.value("region").toString();
  ndb.name = record.value("name").toString();
  ndb.type = record.value("type").toString();
  ndb.frequency = record.value("frequency").toInt();
  ndb.range = record.value("range").toInt();
  ndb.magvar = record.value("mag_var").toFloat();
  ndb.position = Pos(record.value("lonx").toFloat(), record.value("laty").toFloat(),
                     record.value("altitude").toFloat());
}

void MapTypesFactory::fillWaypoint(const SqlRecord& record, maptypes::MapWaypoint& wp)
{
  wp.id = record.value("waypoint_id").toInt();
  wp.ident = record.value("ident").toString();
  wp.region = record.value("region").toString();
  wp.type = record.value("type").toString();
  wp.magvar = record.value("mag_var").toFloat();
  wp.hasVictor = record.value("num_victor_airway").toInt() > 0;
  wp.hasJet = record.value("num_jet_airway").toInt() > 0;
  wp.position = Pos(record.value("lonx").toFloat(), record.value("laty").toFloat());
}

void MapTypesFactory::fillWaypointFromNav(const SqlRecord& record, maptypes::MapWaypoint& wp)
{
  wp.id = record.value("waypoint_id").toInt();
  wp.ident = record.value("ident").toString();
  wp.region = record.value("region").toString();
  wp.type = record.value("type").toString();
  wp.magvar = record.value("mag_var").toFloat();
  wp.hasVictor = record.value("waypoint_num_victor_airway").toInt() > 0;
  wp.hasJet = record.value("waypoint_num_jet_airway").toInt() > 0;
  wp.position = Pos(record.value("lonx").toFloat(), record.value("laty").toFloat());
}

void MapTypesFactory::fillAirway(const SqlRecord& record, maptypes::MapAirway& airway)
{
  airway.id = record.value("airway_id").toInt();
  airway.type = maptypes::airwayTypeFromString(record.value("airway_type").toString());
  airway.name = record.value("airway_name").toString();
  airway.minalt = record.value("minimum_altitude").toInt();
  airway.fragment = record.value("airway_fragment_no").toInt();
  airway.sequence = record.value("sequence_no").toInt();
  airway.fromWpId = record.value("from_waypoint_id").toInt();
  airway.toWpId = record.value("to_waypoint_id").toInt();
  airway.from = Pos(record.value("from_lonx").toFloat(),
                    record.value("from_laty").toFloat());
  airway.to = Pos(record.value("to_lonx").toFloat(),
                  record.value("to_laty").toFloat());
  airway.bounding = Rect(airway.from);
  airway.bounding.extend(airway.to);
}

void MapTypesFactory::fillMarker(const SqlRecord& record, maptypes::MapMarker& marker)
{
  marker.id = record.value("marker_id").toInt();
  marker.type = record.value("type").toString();
  marker.heading = static_cast<int>(std::roundf(record.value("heading").toFloat()));
  marker.position = Pos(record.value("lonx").toFloat(),
                        record.value("laty").toFloat());
}

void MapTypesFactory::fillIls(const SqlRecord& record, maptypes::MapIls& ils)
{
  ils.id = record.value("ils_id").toInt();
  ils.ident = record.value("ident").toString();
  ils.name = record.value("name").toString();
  ils.heading = record.value("loc_heading").toFloat();
  ils.width = record.value("loc_width").toFloat();
  ils.magvar = record.value("mag_var").toFloat();
  ils.slope = record.value("gs_pitch").toFloat();

  ils.frequency = record.value("frequency").toInt();
  ils.range = record.value("range").toInt();
  ils.dme = record.value("dme_range").toInt() > 0;

  ils.position = Pos(record.value("lonx").toFloat(), record.value("laty").toFloat(),
                     record.value("altitude").toFloat());
  ils.pos1 = Pos(record.value("end1_lonx").toFloat(), record.value(
                   "end1_laty").toFloat());
  ils.pos2 = Pos(record.value("end2_lonx").toFloat(), record.value(
                   "end2_laty").toFloat());
  ils.posmid =
    Pos(record.value("end_mid_lonx").toFloat(), record.value("end_mid_laty").toFloat());

  ils.bounding = Rect(ils.position);
  ils.bounding.extend(ils.pos1);
  ils.bounding.extend(ils.pos2);
}

void MapTypesFactory::fillParking(const SqlRecord& record, maptypes::MapParking& parking)
{
  parking.id = record.value("parking_id").toInt();
  parking.airportId = record.value("airport_id").toInt();
  parking.type = record.value("type").toString();
  parking.name = record.value("name").toString();

  parking.position = Pos(record.value("lonx").toFloat(), record.value("laty").toFloat());
  parking.jetway = record.value("has_jetway").toInt() > 0;
  parking.number = record.value("number").toInt();

  parking.heading = static_cast<int>(std::roundf(record.value("heading").toFloat()));
  parking.radius = static_cast<int>(std::roundf(record.value("radius").toFloat()));
}
