/*****************************************************************************
* Copyright 2015-2024 Alexander Barthel alex@littlenavmap.org
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

#ifndef LNM_AIRSPACEQUERIES_H
#define LNM_AIRSPACEQUERIES_H

#include "common/maptypes.h"

namespace Marble {
class GeoDataLatLonBox;
}
namespace atools {
namespace sql {

class SqlRecord;
class SqlDatabase;
}
}
class AirspaceQuery;

typedef  QHash<map::MapAirspaceSources, AirspaceQuery *> AirspaceQueryMapType;
typedef  QVector<const map::MapAirspace *> AirspaceVector;

/*
 * Aggregates all four airspace database types: Navigraph, Sim, Online and User.
 */
class AirspaceQueries
{
public:
  ~AirspaceQueries();

  AirspaceQueries(const AirspaceQueries& other) = delete;
  AirspaceQueries& operator=(const AirspaceQueries& other) = delete;

  /* Get airspaces from all enabled sources for map display */
  void getAirspaces(AirspaceVector& airspaces, const Marble::GeoDataLatLonBox& rect, const MapLayer *mapLayer,
                    const map::MapAirspaceFilter& filter, float flightplanAltitude, bool lazy,
                    map::MapAirspaceSources sourcesParam, bool& overflow);

  /* Get Geometry for any airspace and source database */
  const atools::geo::LineString *getAirspaceGeometry(map::MapAirspaceId id);

  /* Get airspace or online center by id and source database. Airspace is not valid if not found. */
  void getAirspaceById(map::MapAirspace& airspace, map::MapAirspaceId id);
  map::MapAirspace getAirspaceById(map::MapAirspaceId id);

  bool hasAirspaceById(map::MapAirspaceId id);

  /* Get record with all rows from atc table (online center) */
  atools::sql::SqlRecord getOnlineAirspaceRecordById(int airspaceId);

  /* Get boundary merged with scenery and file metadata records. Not for online centers. */
  atools::sql::SqlRecord getAirspaceInfoRecordById(map::MapAirspaceId id);

  /* Tries to fetch online airspace geometry by name and facility. */
  const atools::geo::LineString *getOnlineAirspaceGeoByName(const QString& callsign, const QString& facilityType);

  /* Tries to fetch online airspace geometry by file name. */
  const atools::geo::LineString *getOnlineAirspaceGeoByFile(const QString& callsign);

  /* Get currently enabled souces for map display */
  map::MapAirspaceSources getAirspaceSources() const
  {
    return sources;
  }

  /* true if there are any airspaces in the enabled datababases. */
  bool hasAnyAirspaces() const;

  /* true if currently loading user airspaces */
  bool isLoadingUserAirspaces() const
  {
    return loadingUserAirspaces;
  }

  void setLoadingUserAirspaces(bool value = true)
  {
    loadingUserAirspaces = value;
  }

  void setSourceFlag(map::MapAirspaceSource flag, bool value)
  {
    sources.setFlag(flag, value);
  }

  bool hasSourceFlag(map::MapAirspaceSource flag)
  {
    return sources.testFlag(flag);
  }

  /* Get sources as string */
  QStringList getAirspaceSourcesStr() const;

  /* Clears cache for online database changes */
  void onlineClientAndAtcUpdated();

  /* Resets all queries */
  void resetAirspaceOnlineScreenGeometry();

  const map::MapAirspaceSources& getSources() const
  {
    return sources;
  }

  void setSources(const map::MapAirspaceSources& value)
  {
    sources = value;
  }

private:
  friend class Queries;
  friend class AirspaceController;

  explicit AirspaceQueries(atools::sql::SqlDatabase *dbSim, atools::sql::SqlDatabase *dbNav,
                           atools::sql::SqlDatabase *dbUser, atools::sql::SqlDatabase *dbOnline);

  /* Close all query objects thus disconnecting from the database */
  void initQueries();

  /* Create and prepare all queries */
  void deInitQueries();

  void preLoadAirspaces();
  void postLoadAirspaces();

  void getAirspacesInternal(AirspaceVector& airspaceVector, const Marble::GeoDataLatLonBox& rect,
                            const MapLayer *mapLayer, const map::MapAirspaceFilter& filter, float flightplanAltitude,
                            bool lazy, map::MapAirspaceSources src, bool& overflow);

  AirspaceQueryMapType airspaceQueries;
  map::MapAirspaceSources sources = map::AIRSPACE_SRC_NONE;
  bool loadingUserAirspaces = false;
};

#endif // LNM_AIRSPACEQUERIES_H
