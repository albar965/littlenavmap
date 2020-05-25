/*****************************************************************************
* Copyright 2015-2020 Alexander Barthel alex@littlenavmap.org
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

#ifndef ATOOLS_TRACKMANAGER_H
#define ATOOLS_TRACKMANAGER_H

#include "track/tracktypes.h"
#include "fs/userdata/datamanagerbase.h"
#include "common/mapflags.h"

namespace map {
struct MapObjectRefExt;
typedef QVector<map::MapObjectRefExt> MapObjectRefExtVector;
}

/*
 * Takes care of integrating tracks (NAT, PACOTS and AUSOTS) into the database.
 *
 * Track segments are added like airway segments. Waypoints are either copied if they exist in the nav database
 * or created using an offset id if waypoints are coordinates only.
 *
 * Base class wraps the track database.
 */
class TrackManager
  : public atools::fs::userdata::DataManagerBase
{
public:
  explicit TrackManager(atools::sql::SqlDatabase *trackDatabase, atools::sql::SqlDatabase *navDatabase);
  ~TrackManager();

  /* Clears database and loads the given tracks.
   * onlyValid: Do not load tracks that are currently not valid. */
  void loadTracks(const atools::track::TrackVectorType& tracks, bool onlyValid);

  /* More log messages if true */
  void setVerbose(bool value)
  {
    verbose = value;
  }

  /* Clears track tables in database. */
  void clearTracks();

  /* Get number of tracks for each type */
  QMap<atools::track::TrackType, int> getNumTracks();

  const QStringList& getErrorMessages() const
  {
    return errorMessages;
  }

private:
  /* Queries are initialized and closed when entering and leaving loadTracks(). */
  void initQueries();
  void deInitQueries();

  /* Add waypoint/trackpoint to hash returning waypoint id or new generated trackpoint id.
   * rec is an empty record for trackpoint table. */
  int addTrackpoint(QHash<int, atools::sql::SqlRecord>& trackpoints, atools::sql::SqlRecord rec,
                    map::MapObjectRefExt ref, int trackpointId);

  /* Add track metadata to records if not already present. metaId is incremented if inserted. */
  bool addTrackmeta(QHash<std::pair<atools::track::TrackType, QString>, atools::sql::SqlRecord>& records,
                    const atools::track::Track& track, int metaId, int startPointId, int endPointId);

  atools::sql::SqlQuery *waypointQuery = nullptr, *waypointNavQuery = nullptr,
                        *ndbQuery = nullptr, *vorQuery = nullptr, *airwayQuery = nullptr;

  bool verbose = false;

  /* Database for querying navaids. */
  atools::sql::SqlDatabase *dbNav = nullptr;
  QStringList errorMessages;
};

#endif // ATOOLS_TRACKMANAGER_H
