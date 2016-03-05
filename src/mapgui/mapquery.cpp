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
#include "maplayer.h"

#include <QSqlRecord>
#include <marble/GeoDataLatLonAltBox.h>

using namespace Marble;

MapQuery::MapQuery(atools::sql::SqlDatabase *sqlDb)
  : db(sqlDb)
{

}

void MapQuery::getAirports(const Marble::GeoDataLatLonAltBox& rect, const MapLayer *mapLayer,
                           QList<MapAirport>& airports)
{
  switch(mapLayer->getDataSource())
  {
    case layer::ALL:
      getAirports(rect, airports);
      break;
    case layer::MEDIUM:
      getAirportsMedium(rect, airports);
      break;
    case layer::LARGE:
      getAirportsLarge(rect, airports);
      break;
  }
}

void MapQuery::getAirports(const Marble::GeoDataLatLonAltBox& rect, QList<MapAirport>& airports)
{
  using namespace Marble;
  using atools::sql::SqlQuery;
  using atools::geo::Pos;
  using atools::geo::Rect;

  SqlQuery query(db);

  query.prepare(
    "select airport_id, ident, name, rating, "
    "has_avgas, has_jetfuel, "
    "tower_frequency, atis_frequency, awos_frequency, asos_frequency, unicom_frequency, "
    "is_closed, is_military, is_addon,"
    "num_approach, num_runway_hard, num_runway_soft, num_runway_water, num_runway_light,"
    "longest_runway_length, longest_runway_heading, mag_var, "
    "altitude, lonx, laty, left_lonx, top_laty, right_lonx, bottom_laty "
    "from airport "
    "where lonx between :leftx and :rightx and laty between :bottomy and :topy "
    "order by rating asc, longest_runway_length");

  bindCoordinateRect(rect, query);
  query.exec();
  while(query.next())
    airports.append(getMapAirport(query));
}

void MapQuery::getAirportsMedium(const Marble::GeoDataLatLonAltBox& rect, QList<MapAirport>& ap)
{
  using namespace Marble;
  using atools::sql::SqlQuery;

  SqlQuery query(db);

  query.prepare(
    "select airport_id, ident, name, rating, "
    "has_avgas, has_jetfuel, "
    "tower_frequency, "
    "is_closed, is_military, is_addon,"
    "num_runway_hard, num_runway_soft, num_runway_water, "
    "longest_runway_length, longest_runway_heading, mag_var, "
    "lonx, laty, left_lonx, top_laty, right_lonx, bottom_laty "
    "from airport_medium "
    "where lonx between :leftx and :rightx and laty between :bottomy and :topy "
    "order by longest_runway_length");

  bindCoordinateRect(rect, query);
  query.exec();

  while(query.next())
    ap.append(getMapAirport(query));
}

void MapQuery::getAirportsLarge(const Marble::GeoDataLatLonAltBox& rect, QList<MapAirport>& ap)
{
  using namespace Marble;
  using atools::sql::SqlQuery;

  SqlQuery query(db);

  query.prepare(
    "select airport_id, ident, name, rating, "
    "has_avgas, has_jetfuel, "
    "tower_frequency, "
    "is_closed, is_military, is_addon,"
    "num_runway_hard, num_runway_soft, num_runway_water, "
    "longest_runway_length, longest_runway_heading, mag_var, "
    "lonx, laty, left_lonx, top_laty, right_lonx, bottom_laty "
    "from airport_large "
    "where lonx between :leftx and :rightx and laty between :bottomy and :topy");

  bindCoordinateRect(rect, query);
  query.exec();

  while(query.next())
    ap.append(getMapAirport(query));
}

MapAirport MapQuery::getMapAirport(const atools::sql::SqlQuery& query)
{
  using atools::geo::Pos;
  using atools::geo::Rect;

  MapAirport ap;
  QSqlRecord rec = query.record();

  ap.id = query.value("airport_id").toInt();
  ap.ident = query.value("ident").toString();
  ap.name = query.value("name").toString();
  ap.longestRunwayLength = query.value("longest_runway_length").toInt();
  ap.longestRunwayHeading = static_cast<int>(std::roundf(query.value("longest_runway_heading").toFloat()));

  if(rec.contains("tower_frequency"))
    ap.towerFrequency = query.value("tower_frequency").toInt();
  if(rec.contains("atis_frequency"))
    ap.atisFrequency = query.value("atis_frequency").toInt();
  if(rec.contains("awos_frequency"))
    ap.awosFrequency = query.value("awos_frequency").toInt();
  if(rec.contains("asos_frequency"))
    ap.asosFrequency = query.value("asos_frequency").toInt();
  if(rec.contains("unicom_frequency"))
    ap.unicomFrequency = query.value("unicom_frequency").toInt();

  if(rec.contains("altitude"))
    ap.altitude = static_cast<int>(std::roundf(query.value("altitude").toFloat()));

  ap.flags = getFlags(query);
  ap.magvar = query.value("mag_var").toFloat();
  ap.coords = Pos(query.value("lonx").toFloat(), query.value("laty").toFloat());
  ap.bounding = Rect(query.value("left_lonx").toFloat(), query.value("top_laty").toFloat(),
                     query.value("right_lonx").toFloat(), query.value("bottom_laty").toFloat());

  return ap;
}

void MapQuery::getRunwaysForOverview(int airportId, QList<MapRunway>& runways)
{
  using atools::sql::SqlQuery;
  using atools::geo::Pos;

  SqlQuery query(db);

  query.prepare(
    "select length, heading, lonx, laty, primary_lonx, primary_laty, secondary_lonx, secondary_laty "
    "from runway where airport_id = :airportId and length > 4000");
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

int MapQuery::flag(const atools::sql::SqlQuery& query, const QString& field, MapAirportFlags flag)
{
  if(!query.record().contains(field) || query.isNull(field))
    return NONE;
  else
    return query.value(field).toInt() > 0 ? flag : NONE;
}

void MapQuery::bindCoordinateRect(const Marble::GeoDataLatLonAltBox& rect, atools::sql::SqlQuery& query)
{
  query.bindValue(":leftx", rect.west(GeoDataCoordinates::Degree) - 0.1);
  query.bindValue(":rightx", rect.east(GeoDataCoordinates::Degree) + 0.1);
  query.bindValue(":bottomy", rect.south(GeoDataCoordinates::Degree) - 0.1);
  query.bindValue(":topy", rect.north(GeoDataCoordinates::Degree) + 0.1);
}

int MapQuery::getFlags(const atools::sql::SqlQuery& query)
{
  int flags = 0;
  flags |= flag(query, "rating", SCENERY);
  flags |= flag(query, "has_avgas", FUEL);
  flags |= flag(query, "has_jetfuel", FUEL);
  flags |= flag(query, "tower_frequency", TOWER);
  flags |= flag(query, "is_closed", CLOSED);
  flags |= flag(query, "is_military", MIL);
  flags |= flag(query, "is_addon", ADDON);
  flags |= flag(query, "num_approach", APPR);
  flags |= flag(query, "num_runway_hard", HARD);
  flags |= flag(query, "num_runway_soft", SOFT);
  flags |= flag(query, "num_runway_water", WATER);
  flags |= flag(query, "num_runway_light", LIGHT);
  return flags;
}
