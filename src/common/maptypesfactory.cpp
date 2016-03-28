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

#include <QSqlRecord>

using namespace atools::geo;

MapTypesFactory::MapTypesFactory()
{

}

MapTypesFactory::~MapTypesFactory()
{

}

void MapTypesFactory::fillAirport(const QSqlRecord& record, maptypes::MapAirport& ap, bool complete)
{
  ap.id = record.value("airport_id").toInt();
  ap.ident = record.value("ident").toString();
  ap.name = record.value("name").toString();
  ap.longestRunwayLength = record.value("longest_runway_length").toInt();
  ap.longestRunwayHeading = static_cast<int>(std::roundf(record.value("longest_runway_heading").toFloat()));

  if(record.contains("has_tower_object"))
    ap.towerCoords = Pos(record.value("tower_lonx").toFloat(), record.value("tower_laty").toFloat());

  if(record.contains("tower_frequency"))
    ap.towerFrequency = record.value("tower_frequency").toInt();
  if(record.contains("atis_frequency"))
    ap.atisFrequency = record.value("atis_frequency").toInt();
  if(record.contains("awos_frequency"))
    ap.awosFrequency = record.value("awos_frequency").toInt();
  if(record.contains("asos_frequency"))
    ap.asosFrequency = record.value("asos_frequency").toInt();
  if(record.contains("unicom_frequency"))
    ap.unicomFrequency = record.value("unicom_frequency").toInt();

  if(record.contains("altitude"))
    ap.altitude = static_cast<int>(std::roundf(record.value("altitude").toFloat()));

  ap.flags = getFlags(record);

  if(complete)
    ap.flags |= maptypes::AP_COMPLETE;

  ap.magvar = record.value("mag_var").toFloat();
  ap.position = Pos(record.value("lonx").toFloat(), record.value("laty").toFloat());
  ap.bounding = Rect(record.value("left_lonx").toFloat(), record.value("top_laty").toFloat(),
                     record.value("right_lonx").toFloat(), record.value("bottom_laty").toFloat());

  ap.valid = true;
}

maptypes::MapAirportFlags MapTypesFactory::getFlags(const QSqlRecord& record)
{
  using namespace maptypes;

  MapAirportFlags flags = 0;
  flags |= flag(record, "num_helipad", AP_HELIPAD);
  flags |= flag(record, "rating", AP_SCENERY);
  flags |= flag(record, "has_avgas", AP_AVGAS);
  flags |= flag(record, "has_jetfuel", AP_JETFUEL);
  flags |= flag(record, "tower_frequency", AP_TOWER);
  flags |= flag(record, "is_closed", AP_CLOSED);
  flags |= flag(record, "is_military", AP_MIL);
  flags |= flag(record, "is_addon", AP_ADDON);
  flags |= flag(record, "num_approach", AP_APPR);
  flags |= flag(record, "num_runway_hard", AP_HARD);
  flags |= flag(record, "num_runway_soft", AP_SOFT);
  flags |= flag(record, "num_runway_water", AP_WATER);
  flags |= flag(record, "num_runway_light", AP_LIGHT);
  flags |= flag(record, "num_runway_end_ils", AP_ILS);

  return flags;
}

maptypes::MapAirportFlags MapTypesFactory::flag(const QSqlRecord& record, const QString& field,
                                                maptypes::MapAirportFlags flag)
{
  if(!record.contains(field) || record.isNull(field))
    return maptypes::AP_NONE;
  else
    return record.value(field).toInt() > 0 ? flag : maptypes::AP_NONE;
}

void MapTypesFactory::fillVor(const QSqlRecord& record, maptypes::MapVor& vor)
{
  vor.id = record.value("vor_id").toInt();
  vor.ident = record.value("ident").toString();
  vor.region = record.value("region").toString();
  vor.name = record.value("name").toString();
  vor.type = record.value("type").toString();
  vor.frequency = record.value("frequency").toInt();
  vor.range = record.value("range").toInt();
  vor.dmeOnly = record.value("dme_only").toInt() > 0;
  vor.hasDme = !record.value("dme_altitude").isNull();
  vor.magvar = record.value("mag_var").toFloat();
  vor.position = Pos(record.value("lonx").toFloat(), record.value("laty").toFloat());
}

void MapTypesFactory::fillNdb(const QSqlRecord& record, maptypes::MapNdb& ndb)
{
  ndb.id = record.value("ndb_id").toInt();
  ndb.ident = record.value("ident").toString();
  ndb.region = record.value("region").toString();
  ndb.name = record.value("name").toString();
  ndb.type = record.value("type").toString();
  ndb.frequency = record.value("frequency").toInt();
  ndb.range = record.value("range").toInt();
  ndb.magvar = record.value("mag_var").toFloat();
  ndb.position = Pos(record.value("lonx").toFloat(), record.value("laty").toFloat());
}

void MapTypesFactory::fillWaypoint(const QSqlRecord& record, maptypes::MapWaypoint& wp)
{
  wp.id = record.value("waypoint_id").toInt();
  wp.ident = record.value("ident").toString();
  wp.region = record.value("region").toString();
  wp.type = record.value("type").toString();
  wp.magvar = record.value("mag_var").toFloat();
  wp.position = Pos(record.value("lonx").toFloat(), record.value("laty").toFloat());
}

void MapTypesFactory::fillAirway(const QSqlRecord& record, maptypes::MapAirway& airway)
{
  airway.id = record.value("route_id").toInt();
  airway.type = record.value("route_type").toString();
  airway.name = record.value("route_name").toString();
  airway.minalt = record.value("minimum_altitude").toInt();
  airway.fragment = record.value("route_fragment_no").toInt();
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

void MapTypesFactory::fillMarker(const QSqlRecord& record, maptypes::MapMarker& marker)
{
  marker.id = record.value("marker_id").toInt();
  marker.type = record.value("type").toString();
  marker.heading = static_cast<int>(std::roundf(record.value("heading").toFloat()));
  marker.position = Pos(record.value("lonx").toFloat(),
                        record.value("laty").toFloat());
}

void MapTypesFactory::fillIls(const QSqlRecord& record, maptypes::MapIls& ils)
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

  ils.position = Pos(record.value("lonx").toFloat(), record.value("laty").toFloat());
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
