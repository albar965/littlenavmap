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

void MapQuery::getRunwaysForOverview(int airportId, QList<MapRunway>& runways)
{
  using atools::sql::SqlQuery;
  using atools::geo::Pos;

  SqlQuery query(db);

  query.prepare(
    "select length, heading, lonx, laty, primary_lonx, primary_laty, secondary_lonx, secondary_laty "
    "from runway where airport_id = :airportId and length > 8000");
  query.bindValue(":airportId", airportId);
  query.exec();

  while(query.next())
  {
    runways.append({query.value("length").toInt(),
                    static_cast<int>(std::roundf(query.value("heading").toFloat())),
                    0,
                    QString(),
                    Pos(query.value("lonx").toFloat(), query.value("laty").toFloat()),
                    Pos(query.value("primary_lonx").toFloat(), query.value("primary_laty").toFloat()),
                    Pos(query.value("secondary_lonx").toFloat(), query.value("secondary_laty").toFloat())});
  }
}

void MapQuery::getRunways(int airportId, QList<MapRunway>& runways)
{
  using atools::sql::SqlQuery;
  using atools::geo::Pos;

  SqlQuery query(db);

  query.prepare(
    "select length, heading, width, surface, lonx, laty, primary_lonx, primary_laty, secondary_lonx, secondary_laty "
    "from runway where airport_id = :airportId");
  query.bindValue(":airportId", airportId);
  query.exec();

  while(query.next())
  {
    runways.append({query.value("length").toInt(),
                    static_cast<int>(std::roundf(query.value("heading").toFloat())),
                    0,
                    QString(),
                    Pos(query.value("lonx").toFloat(), query.value("laty").toFloat()),
                    Pos(query.value("primary_lonx").toFloat(), query.value("primary_laty").toFloat()),
                    Pos(query.value("secondary_lonx").toFloat(), query.value("secondary_laty").toFloat())});
  }
}

//void MapQuery::getAirportsOverview(int minRunwayLength, QList<MapAirport>& ap)
//{
//}

void MapQuery::getAirports(const Marble::GeoDataLatLonAltBox& rect, QList<MapAirport>& ap)
{
  using namespace Marble;
  using atools::sql::SqlQuery;
  using atools::geo::Pos;
  using atools::geo::Rect;

  SqlQuery query(db);

  query.prepare(
    "select airport_id, ident, name, rating, "
    "has_avgas, has_jetfuel, has_tower, is_closed, is_military, is_addon,"
    "num_approach, num_runway_hard, num_runway_soft, num_runway_water, num_runway_light,"
    "longest_runway_length, longest_runway_heading, mag_var, "
    "lonx, laty, left_lonx, top_laty, right_lonx, bottom_laty "
    "from airport "
    "where lonx between :leftx and :rightx and laty between :bottomy and :topy "
    "order by rating asc, longest_runway_length");

  query.bindValue(":leftx", rect.west(GeoDataCoordinates::Degree) - 0.1);
  query.bindValue(":rightx", rect.east(GeoDataCoordinates::Degree) + 0.1);
  query.bindValue(":bottomy", rect.south(GeoDataCoordinates::Degree) - 0.1);
  query.bindValue(":topy", rect.north(GeoDataCoordinates::Degree) + 0.1);
  query.exec();
  while(query.next())
  {
  int flags = 0;

  flags |= flag(query, "rating", SCENERY);
  flags |= flag(query, "has_avgas", FUEL);
  flags |= flag(query, "has_jetfuel", FUEL);
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
             static_cast<int>(std::roundf(query.value("mag_var").toFloat())),
             Pos(query.value("lonx").toFloat(), query.value("laty").toFloat()),
             Rect(query.value("left_lonx").toFloat(), query.value("top_laty").toFloat(),
                  query.value("right_lonx").toFloat(), query.value("bottom_laty").toFloat())});
  }
}

int MapQuery::flag(const atools::sql::SqlQuery& query, const QString& field, MapAirportFlags flag)
{
  if(query.isNull(field))
    return NONE;
  else
    return query.value(field).toInt() > 0 ? flag : NONE;
}
