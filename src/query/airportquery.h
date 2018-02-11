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

#ifndef LITTLENAVMAP_AIRPORTQUERY_H
#define LITTLENAVMAP_AIRPORTQUERY_H

#include "common/maptypes.h"
#include "mapgui/maplayer.h"

#include <QCache>
#include <QList>

#include <functional>

#include <marble/GeoDataLatLonBox.h>

namespace atools {
namespace geo {
class Rect;
}
namespace sql {
class SqlDatabase;
class SqlQuery;
}
}

class CoordinateConverter;
class MapTypesFactory;
class MapLayer;

/*
 * Provides map related database queries. Fill objects of the maptypes namespace and maintains a cache.
 * Objects from methods returning a pointer to a list might be deleted from the cache and should be copied
 * if they have to be kept between event loop calls.
 * All ids are database ids.
 */
class AirportQuery
  : public QObject
{
  Q_OBJECT

public:
  /*
   * @param sqlDb database for simulator scenery data
   * @param sqlDbNav for updated navaids
   */
  AirportQuery(QObject *parent, atools::sql::SqlDatabase *sqlDb, bool nav);
  ~AirportQuery();

  void getAirportAdminNamesById(int airportId, QString& city, QString& state, QString& country);

  void getAirportById(map::MapAirport& airport, int airportId);
  map::MapAirport getAirportById(int airportId);

  void getAirportByIdent(map::MapAirport& airport, const QString& ident);
  atools::geo::Pos getAirportCoordinatesByIdent(const QString& ident);

  bool hasProcedures(int airportId) const;
  bool hasProcedures(const QString& ident) const;

  /* Get the region for airports where it is missing. This uses an expensive query to get the
   * region from the nearest waypoints. Region is set in MapAirport.
   * This is a workaround for missing information and not precise. */
  void getAirportRegion(map::MapAirport& airport);

  /*
   * Get a parking spot of an airport by name and number
   * @param parkings result
   * @param airportId airport where parking spot should be fetched
   * @param name name like PARKING, GATE_P, etc.
   * @param number parking number
   */
  void getParkingByNameAndNumber(QList<map::MapParking>& parkings, int airportId, const QString& name, int number);
  void getParkingByName(QList<map::MapParking>& parkings, int airportId, const QString& name,
                        const atools::geo::Pos& sortByDistancePos);

  /*
   * Get a start position of an airport (runway, helipad and water)
   * @param start result
   * @param airportId airport where start position should be fetched
   * @param runwayEndName name like 24, 12C, 13W
   * @param position needed to distinguish between helipads and runways which can have the same name
   */
  void getStartByNameAndPos(map::MapStart& start, int airportId, const QString& runwayEndName,
                            const atools::geo::Pos& position);
  void getStartById(map::MapStart& start, int startId);

  /* Get best start position for an airport. This is the longest preferrably hard surfaced primary runway end */
  void getBestStartPositionForAirport(map::MapStart& start, int airportId, const QString& runwayName);

  /* Get a completely filled runway list for the airport */
  const QList<map::MapRunway> *getRunways(int airportId);
  QStringList getRunwayNames(int airportId);
  void getRunwayEndByNames(map::MapSearchResult& result, const QString& runwayName, const QString& airportIdent);

  const QList<map::MapApron> *getAprons(int airportId);

  const QList<map::MapTaxiPath> *getTaxiPaths(int airportId);

  const QList<map::MapParking> *getParkingsForAirport(int airportId);

  const QList<map::MapStart> *getStartPositionsForAirport(int airportId);

  const QList<map::MapHelipad> *getHelipads(int airportId);

  map::MapRunwayEnd getRunwayEndById(int id);

  /* Close all query objects thus disconnecting from the database */
  void initQueries();

  /* Create and prepare all queries */
  void deInitQueries();

  QHash<int, QList<map::MapParking> > getParkingCache() const;

  QHash<int, QList<map::MapHelipad> > getHelipadCache() const;

  static QStringList airportColumns(const atools::sql::SqlDatabase *db);

private:
  const QList<map::MapAirport> *fetchAirports(const Marble::GeoDataLatLonBox& rect,
                                              atools::sql::SqlQuery *query, bool reverse,
                                              bool lazy, bool overview);

  bool runwayCompare(const map::MapRunway& r1, const map::MapRunway& r2);

  const int queryRowLimit = 5000;

  /* true if third party navdata */
  bool navdata;

  MapTypesFactory *mapTypesFactory;
  atools::sql::SqlDatabase *db;

  /* ID/object caches */
  QCache<int, QList<map::MapRunway> > runwayCache;
  QCache<int, QList<map::MapApron> > apronCache;
  QCache<int, QList<map::MapTaxiPath> > taxipathCache;
  QCache<int, QList<map::MapParking> > parkingCache;
  QCache<int, QList<map::MapStart> > startCache;
  QCache<int, QList<map::MapHelipad> > helipadCache;

  QCache<QString, map::MapAirport> airportIdentCache;
  QCache<int, map::MapAirport> airportIdCache;

  /* Database queries */
  atools::sql::SqlQuery *runwayOverviewQuery = nullptr, *apronQuery = nullptr,
                        *parkingQuery = nullptr, *startQuery = nullptr, *startByIdQuery = nullptr,
                        *helipadQuery = nullptr,
                        *taxiparthQuery = nullptr, *runwaysQuery = nullptr,
                        *parkingTypeAndNumberQuery = nullptr,
                        *parkingNameQuery = nullptr;

  atools::sql::SqlQuery *airportByIdentQuery = nullptr, *airportCoordsByIdentQuery = nullptr;
  atools::sql::SqlQuery *runwayEndByIdQuery = nullptr, *runwayEndByNameQuery = nullptr;
  atools::sql::SqlQuery *airportByIdQuery = nullptr, *airportAdminByIdQuery = nullptr, *airportProcByIdQuery = nullptr,
                        *airportProcByIdentQuery = nullptr;
};

#endif // LITTLENAVMAP_AIRPORTQUERY_H
