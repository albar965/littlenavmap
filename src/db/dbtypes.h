/*****************************************************************************
* Copyright 2015-2023 Alexander Barthel alex@littlenavmap.org
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

#ifndef LITTLENAVMAP_DBTYPES_H
#define LITTLENAVMAP_DBTYPES_H

#include "fs/fspaths.h"

#include <QHash>
#include <QString>
#include <QObject>

namespace navdb {
enum Status : quint8
{
  UNKNOWN,
  ALL, /* Only third party nav database */
  MIXED, /* Airports from simulator rest from nav database */
  OFF /* Only simulator database */
};

/* Defines the automatic correction and reason for the navdata source */
enum Correction : quint8
{
  CORRECT_NONE,
  CORRECT_MSFS_HAS_NAVIGRAPH, /* MSFS with Navigraph update found -> mixed mode */
  CORRECT_MSFS_NO_NAVIGRAPH, /* MSFS without navdata update found -> no navdatabase */
  CORRECT_FSX_P3D_UPDATED, /* Any FSX or P3D with updated cycle -> mixed or sim only mode */
  CORRECT_FSX_P3D_OUTDATED, /* Any FSX or P3D with old included AIRAC - sim only mode */
  CORRECT_XP_CYCLE_NAV_EQUAL, /* XP nav cycle is equal to sim cycle -> mixed mode */
  CORRECT_XP_CYCLE_NAV_SMALLER, /* XP nav cycle is equal to sim cycle -> no navdatabase */
  CORRECT_EMPTY, /* Sim database is empty - use navdata for all */
  CORRECT_ALL /* Navdata for all selected - change mode */
};

}

/* Combines path and scenery information for a flight simulator type */
struct FsPathType
{
  QString basePath /* Base path where fsx.exe or prepar3d.exe can be found */,
          sceneryCfg /* full path and name of scenery.cfg file */;
  bool hasDatabase = false, /* true if a database was found in the configuration Directory */
       isInstalled = false /* True if the simulator is installed on the system */;
  navdb::Status navDatabaseStatus = navdb::MIXED;
};

QDebug operator<<(QDebug out, const FsPathType& record);

QDataStream& operator<<(QDataStream& out, const FsPathType& obj);
QDataStream& operator>>(QDataStream& in, FsPathType& obj);

Q_DECLARE_METATYPE(FsPathType);
Q_DECLARE_TYPEINFO(FsPathType, Q_MOVABLE_TYPE);

/* Hash map for simulator type and FsPathType. Can be converted to QVariant */
class SimulatorTypeMap :
  private QHash<atools::fs::FsPaths::SimulatorType, FsPathType>
{
public:
  SimulatorTypeMap()
  {
  }

  /* Checks for fs installations and databases and populates the hash map */
  void fillDefault(navdb::Status navDatabaseStatus);

  /* Get the latest/newest simulator from all installed ones or databases found */
  atools::fs::FsPaths::SimulatorType getBest() const;

  /* Get lastest/newest installed simulator */
  atools::fs::FsPaths::SimulatorType getBestInstalled() const;

  /* Get files path for installed simulators in order of the given list */
  atools::fs::FsPaths::SimulatorType getBestInstalled(const atools::fs::FsPaths::SimulatorTypeVector& types) const;

  /* Get all installed simulators */
  QList<atools::fs::FsPaths::SimulatorType> getAllInstalled() const;

  /* Get all simulators that have a database */
  QList<atools::fs::FsPaths::SimulatorType> getAllHavingDatabase() const;

  /* Make needed base class methods public */
  using QHash::begin;
  using QHash::end;
  using QHash::constBegin;
  using QHash::constEnd;
  using QHash::keys;
  using QHash::isEmpty;
  using QHash::value;
  using QHash::contains;
  using QHash::remove;
  using QHash::size;
  using QHash::operator[];

private:
  friend QDataStream& operator<<(QDataStream& out, const SimulatorTypeMap& obj);

  friend QDataStream& operator>>(QDataStream& in, SimulatorTypeMap& obj);

  void fillOneDefault(atools::fs::FsPaths::SimulatorType type, navdb::Status navDatabaseStatus);

};

QDataStream& operator<<(QDataStream& out, const SimulatorTypeMap& obj);
QDataStream& operator>>(QDataStream& in, SimulatorTypeMap& obj);

Q_DECLARE_METATYPE(SimulatorTypeMap);

#endif // LITTLENAVMAP_DBTYPES_H
