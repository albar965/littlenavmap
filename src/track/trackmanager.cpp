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

#include "track/trackmanager.h"

#include "routestring/routestringreader.h"
#include "sql/sqldatabase.h"
#include "sql/sqlrecord.h"
#include "sql/sqlquery.h"
#include "atools.h"
#include "route/flightplanentrybuilder.h"
#include "geo/rect.h"
#include "sql/sqltransaction.h"
#include "sql/sqlutil.h"
#include "io/binaryutil.h"

#include <QDataStream>
#include <QElapsedTimer>
#include <navapp.h>

using atools::sql::SqlDatabase;
using atools::sql::SqlTransaction;
using atools::sql::SqlQuery;
using atools::sql::SqlUtil;
using atools::sql::SqlRecord;
using atools::sql::SqlRecordVector;
using atools::track::TrackVectorType;
using atools::track::TrackType;
using atools::track::Track;

TrackManager::TrackManager(SqlDatabase *trackDatabase, SqlDatabase *navDatabase)
  : atools::fs::userdata::DataManagerBase(trackDatabase, "track", "track_id",
                                          ":/atools/resources/sql/fs/track/create_track_schema.sql",
                                          ":/atools/resources/sql/fs/track/drop_track_schema.sql"),
  dbNav(navDatabase)
{
}

TrackManager::~TrackManager()
{
  deInitQueries();
}

void TrackManager::initQueries()
{
  deInitQueries();

  waypointQuery = new SqlQuery(dbNav);
  waypointQuery->prepare("select * from waypoint where waypoint_id = ? limit 1");

  waypointNavQuery = new SqlQuery(dbNav);
  waypointNavQuery->prepare("select waypoint_id from waypoint where nav_id = ? and type= ? limit 1");

  vorQuery = new SqlQuery(dbNav);
  vorQuery->prepare("select * from vor where vor_id = ? limit 1");

  ndbQuery = new SqlQuery(dbNav);
  ndbQuery->prepare("select * from ndb where ndb_id = ? limit 1");

  airwayQuery = new SqlQuery(dbNav);
  airwayQuery->prepare("select minimum_altitude, maximum_altitude, direction "
                       "from airway where airway_id = ? limit 1");
}

void TrackManager::deInitQueries()
{
  delete waypointQuery;
  waypointQuery = nullptr;

  delete waypointNavQuery;
  waypointNavQuery = nullptr;

  delete vorQuery;
  vorQuery = nullptr;

  delete ndbQuery;
  ndbQuery = nullptr;

  delete airwayQuery;
  airwayQuery = nullptr;
}

void TrackManager::loadTracks(const TrackVectorType& tracks, bool onlyValid)
{
  SqlTransaction transaction(db);
  clearTracks();

  initQueries();

  QElapsedTimer timer;
  timer.start();

  // Generated ids with offset to distinguis from read airways and waypoints
  int trackpointId = atools::track::TRACKPOINT_ID_OFFSET, trackId = atools::track::TRACK_ID_OFFSET, trackmetaId = 1;

  FlightplanEntryBuilder builder;
  RouteStringReader reader(&builder, verbose);
  reader.setPlaintextMessages(true);

  // Maps trackpoint/waypoint (real or generated with offset) ids to records to insert into table trackpoint
  QHash<int, SqlRecord> trackpoints;

  // Maps type (NAT, PACOTS, etc.) and track name to metadata record
  QHash<std::pair<TrackType, QString>, SqlRecord> trackmeta;

  // Maps name to a fragment number for airway compatibility which needs name and fragment as a key
  QHash<QString, int> nameFragmentHash;

  QDateTime now = QDateTime::currentDateTimeUtc();
  // Read each track into the database ==================================================
  for(const Track& track : tracks)
  {
    if(verbose)
      qDebug() << Q_FUNC_INFO << track;

    if(onlyValid && (now < track.validFrom || now > track.validTo))
      continue;

    // Add or increment fragment number for a new name
    if(nameFragmentHash.contains(track.name))
      nameFragmentHash[track.name]++;
    else
      nameFragmentHash.insert(track.name, 1);

    // Read string into a list of references ====================================
    map::MapObjectRefExtVector refs;
    QString routeStr = track.route.join(" ");
    if(reader.createRouteFromString(routeStr, rs::READ_NO_AIRPORTS | rs::READ_MATCH_WAYPOINTS | rs::NO_TRACKS,
                                    nullptr, &refs))
    {
      if(verbose)
        qDebug() << Q_FUNC_INFO << refs;

      if(reader.hasWarningMessages() || reader.hasErrorMessages())
      {
        qWarning() << Q_FUNC_INFO << routeStr;
        qWarning() << Q_FUNC_INFO << reader.getMessages();
      }

      // Empty records
      SqlRecord rec = getEmptyRecord(); // track table
      SqlRecord trackpointRec = db->record("trackpoint");

      int startPointId = -1, endPointId = -1;
      // Write all waypoints to the database ===========================================
      for(int i = 1; i < refs.size(); i++)
      {
        const map::MapObjectRefExt& ref = refs.at(i);

        // Read airways later
        if(ref.objType & map::AIRWAY)
          continue;

        const map::MapObjectRefExt *refLast2 = i > 1 ? &refs.at(i - 2) : nullptr;
        const map::MapObjectRefExt& refLast1 = refs.at(i - 1);

        rec.setValue("track_id", trackId++);
        rec.setValue("trackmeta_id", trackmetaId);
        rec.setValue("track_name", track.name);
        rec.setValue("track_type", atools::charToStr(track.type));
        rec.setValue("sequence_no", i);
        rec.setValue("track_fragment_no", nameFragmentHash.value(track.name));

        if(!track.eastLevels.isEmpty())
          rec.setValue("altitude_levels_east", atools::io::writeVector<quint16, quint16>(track.eastLevels));
        if(!track.westLevels.isEmpty())
          rec.setValue("altitude_levels_west", atools::io::writeVector<quint16, quint16>(track.westLevels));

        int airwayId = -1;
        map::MapObjectRefExt fromRef, toRef;
        if(refLast2 != nullptr && refLast1.objType & map::AIRWAY)
        {
          // Previous entry is an airway - second previous is from waypoint
          fromRef = *refLast2;
          airwayId = refLast1.id;
        }
        else
          // No airway - previous is from waypoint
          fromRef = refLast1;

        // to waypoint
        toRef = ref;

        if(airwayId != -1)
        {
          // Save copy of certain airway fields ============
          rec.setValue("airway_id", airwayId);

          airwayQuery->bindValue(0, airwayId);
          airwayQuery->exec();
          if(airwayQuery->next())
          {
            rec.setValue("airway_minimum_altitude", airwayQuery->value("minimum_altitude"));
            rec.setValue("airway_maximum_altitude", airwayQuery->value("maximum_altitude"));
            rec.setValue("airway_direction", airwayQuery->value("direction"));
          }
        }

        // Add trackpoint/waypoint to hash and return id which can be generated or original waypoint id
        int fromId = addTrackpoint(trackpoints, trackpointRec, fromRef, trackpointId);

        // New generated id for waypoint in case it is needed
        trackpointId++;
        int toId = addTrackpoint(trackpoints, trackpointRec, toRef, trackpointId);

        // Remember start and end id (real or generated) for metadata
        if(i == 1)
          startPointId = fromId;
        if(i == refs.size() - 1)
          endPointId = toId;

        rec.setValue("from_waypoint_id", fromId);
        rec.setValue("from_waypoint_name", fromRef.name);
        rec.setValue("to_waypoint_id", toId);
        rec.setValue("to_waypoint_name", toRef.name);

        // Coordinates and bounding rectangle
        atools::geo::Rect rect(atools::geo::LineString({fromRef.position, toRef.position}));
        rec.setValue("left_lonx", rect.getWest());
        rec.setValue("top_laty", rect.getNorth());
        rec.setValue("right_lonx", rect.getEast());
        rec.setValue("bottom_laty", rect.getSouth());
        rec.setValue("from_lonx", fromRef.position.getLonX());
        rec.setValue("from_laty", fromRef.position.getLatY());
        rec.setValue("to_lonx", ref.position.getLonX());
        rec.setValue("to_laty", ref.position.getLatY());

        // Insert into database
        insertByRecordId(rec);

        // Set all to null
        rec.clearValues();
      }

      // Add to trackmeta table if a new track was found
      if(addTrackmeta(trackmeta, track, trackmetaId, startPointId, endPointId))
        trackmetaId++;

    }
    else
    {
      qWarning() << QString("Error when parsing track %1 (%2) with route %3.").
        arg(track.name).arg(track.typeString()).arg(track.route.join(" "));
      qWarning() << reader.getMessages();
    }
  }

  if(verbose)
    qDebug() << Q_FUNC_INFO << "after loading tracks" << timer.restart();

  // Write collected trackpoints into database
  insertRecords(trackpoints.values().toVector(), "trackpoint");

  // Write collected metadata into database
  insertRecords(trackmeta.values().toVector(), "trackmeta");

  transaction.commit();
  deInitQueries();

  if(verbose)
    qDebug() << Q_FUNC_INFO << "after loading trackpoints" << timer.restart();
}

int TrackManager::addTrackpoint(QHash<int, SqlRecord>& trackpoints, atools::sql::SqlRecord rec,
                                map::MapObjectRefExt ref, int trackpointId)
{
  int returnId = -1;

  // Try to find associated waypoint for VOR or NDB ============================
  if(ref.objType == map::VOR || ref.objType == map::NDB)
  {
    waypointNavQuery->bindValue(0, ref.id);
    waypointNavQuery->bindValue(1, ref.objType == map::VOR ? "V" : "N");
    waypointNavQuery->exec();
    if(waypointNavQuery->next())
    {
      // Change reference to waypoint ===================
      ref.id = waypointNavQuery->valueInt(0);
      ref.objType = map::WAYPOINT;
    }
    waypointNavQuery->finish();
  }

  rec.clearValues();

  if(ref.objType == map::WAYPOINT)
  {
    if(!trackpoints.contains(ref.id))
    {
      waypointQuery->bindValue(0, ref.id);
      waypointQuery->exec();
      if(waypointQuery->next())
      {
        // Waypoint new in list and found in database - insert a copy with waypoint_id ========================
        rec.setValue("trackpoint_id", ref.id);
        rec.setValue("nav_id", waypointQuery->value("nav_id"));
        rec.setValue("ident", waypointQuery->value("ident"));
        rec.setValue("region", waypointQuery->value("region"));
        rec.setValue("type", waypointQuery->value("type"));
        rec.setValue("num_victor_airway", waypointQuery->value("num_victor_airway"));
        rec.setValue("num_jet_airway", waypointQuery->value("num_jet_airway"));
        rec.setValue("mag_var", waypointQuery->value("mag_var"));
        rec.setValue("lonx", waypointQuery->value("lonx"));
        rec.setValue("laty", waypointQuery->value("laty"));
        trackpoints.insert(ref.id, rec);
        returnId = ref.id;
      }
      waypointQuery->finish();
    }
    else
      returnId = ref.id;
  }
  else if(ref.objType == map::VOR || ref.objType == map::NDB)
  {
    // VOR or NDB without associated waypoint ==========================================
    SqlQuery *navaidQuery = nullptr;
    QString type;
    if(ref.objType == map::VOR)
    {
      navaidQuery = vorQuery;
      type = "V";
    }
    else if(ref.objType == map::NDB)
    {
      navaidQuery = ndbQuery;
      type = "N";
    }

    if(navaidQuery != nullptr)
    {
      if(!trackpoints.contains(trackpointId))
      {
        navaidQuery->bindValue(0, ref.id);
        navaidQuery->exec();
        if(navaidQuery->next())
        {
          // Navaid new in list and found in database - insert a new VOR or NDB waypoint with generated id ===========
          rec.setValue("trackpoint_id", trackpointId);
          rec.setValue("nav_id", ref.id);
          rec.setValue("ident", navaidQuery->value("ident"));
          rec.setValue("region", navaidQuery->value("region"));
          rec.setValue("type", type);
          rec.setValue("num_victor_airway", 0);
          rec.setValue("num_jet_airway", 0);
          rec.setValue("mag_var", navaidQuery->value("mag_var"));
          rec.setValue("lonx", navaidQuery->value("lonx"));
          rec.setValue("laty", navaidQuery->value("laty"));
          trackpoints.insert(trackpointId, rec);
          returnId = trackpointId;
        }
        navaidQuery->finish();
      }
      else
        returnId = trackpointId;
    }
  }

  if(returnId == -1)
  {
    // Nothing found in database nor hash ===========================

    if(!trackpoints.contains(trackpointId))
    {
      // Create a new waypoint based on coordinates using a generated id =====================================
      rec.setValue("trackpoint_id", trackpointId);
      rec.setValue("ident", ref.name);
      rec.setValue("type", "WT");
      rec.setValue("num_victor_airway", 0);
      rec.setValue("num_jet_airway", 0);
      rec.setValue("mag_var", NavApp::getMagVar(ref.position));
      rec.setValue("lonx", ref.position.getLonX());
      rec.setValue("laty", ref.position.getLatY());
      trackpoints.insert(trackpointId, rec);
    }
    returnId = trackpointId;
  }
  return returnId;
}

bool TrackManager::addTrackmeta(QHash<std::pair<TrackType, QString>, SqlRecord>& records, const Track& track,
                                int metaId, int startPointId, int endPointId)
{
  auto key = std::make_pair(track.type, track.name);
  if(!records.contains(key))
  {
    SqlRecord rec = db->record("trackmeta");

    rec.setValue("trackmeta_id", metaId);
    rec.setValue("track_name", track.name);
    rec.setValue("track_type", atools::charToStr(track.type));
    rec.setValue("route", track.route.join(" "));
    rec.setValue("startpoint_id", startPointId);
    rec.setValue("endpoint_id", endPointId);
    rec.setValue("valid_from", track.validFrom.toString(Qt::ISODate));
    rec.setValue("valid_to", track.validTo.toString(Qt::ISODate));
    rec.setValue("download_timestamp", QDateTime::currentDateTime().toString(Qt::ISODate));
    records.insert(key, rec);
    return true;
  }
  return false;
}

void TrackManager::clearTracks()
{
  removeRows();
  removeRows("trackpoint");
  removeRows("trackmeta");
}

QMap<atools::track::TrackType, int> TrackManager::getNumTracks()
{
  SqlQuery query("select track_type, count(1) as cnt "
                 "from trackmeta group by track_type", getDatabase());

  QMap<atools::track::TrackType, int> retval;
  query.exec();
  while(query.next())
    retval.insert(static_cast<atools::track::TrackType>(atools::strToChar(query.valueStr(0))), query.valueInt(1));

  // Insert missing values
  if(!retval.contains(atools::track::AUSOTS))
    retval.insert(atools::track::AUSOTS, 0);
  if(!retval.contains(atools::track::PACOTS))
    retval.insert(atools::track::PACOTS, 0);
  if(!retval.contains(atools::track::NAT))
    retval.insert(atools::track::NAT, 0);

  return retval;
}
