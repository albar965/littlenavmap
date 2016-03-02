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

#include "mapgui/mapquery.h"

#include "sql/sqlquery.h"
#include "geo/rect.h"

#include <marble/GeoDataLatLonAltBox.h>

using namespace Marble;

MapQuery::MapQuery(atools::sql::SqlDatabase *sqlDb)
  : db(sqlDb)
{

}

void MapQuery::getAirports(const Marble::GeoDataLatLonAltBox& rect, QList<MapAirport>& ap)
{
  using namespace Marble;
  using atools::sql::SqlQuery;
  SqlQuery query(db);

  query.prepare(
    "select airport_id, ident, name, "
    "has_avgas, has_jetfuel, has_tower, is_closed, is_military, is_addon,"
    "num_approach, num_runway_hard, num_runway_soft, num_runway_water, num_runway_light,"
    "longest_runway_length, longest_runway_heading, "
    "lonx, laty "
    "from airport "
    "where lonx between :leftx and :rightx and laty between :bottomy and :topy order by longest_runway_length");

  query.bindValue(":leftx", rect.west(GeoDataCoordinates::Degree));
  query.bindValue(":rightx", rect.east(GeoDataCoordinates::Degree));
  query.bindValue(":bottomy", rect.south(GeoDataCoordinates::Degree));
  query.bindValue(":topy", rect.north(GeoDataCoordinates::Degree));
  query.exec();
  while(query.next())
  {
  int flags = 0;

  flags |= flag(query, "has_avgas", AVGAS);
  flags |= flag(query, "has_jetfuel", JETFUEL);
  flags |= flag(query, "has_tower", TOWER);
  flags |= flag(query, "is_closed", CLOSED);
  flags |= flag(query, "is_military", MIL);
  flags |= flag(query, "is_addon", ADDON);
  flags |= flag(query, "num_approach", APPR);
  flags |= flag(query, "num_runway_hard", HARD);
  flags |= flag(query, "num_runway_soft", SOFT);
  flags |= flag(query, "num_runway_water", WATER);
  flags |= flag(query, "num_runway_light", LIGHT);

  ap.append({query.value("airport_id").toInt(),
             query.value("ident").toString(),
             query.value("name").toString(),
             query.value("longest_runway_length").toInt(),
             static_cast<int>(std::roundf(query.value("longest_runway_heading").toFloat())),
             flags,
             GeoDataCoordinates(query.value("lonx").toDouble(), query.value("laty").toDouble(),
                                0, GeoDataCoordinates::Degree)});
  }
}

int MapQuery::flag(const atools::sql::SqlQuery& query, const QString& field, MapAirportFlags flag)
{
  if(query.isNull(field))
    return NONE;
  else
    return query.value(field).toInt() > 0 ? flag : NONE;
}
