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

#ifndef LITTLENAVMAP_DBTYPES_H
#define LITTLENAVMAP_DBTYPES_H

#include "fs/fspaths.h"

#include <QHash>
#include <QString>
#include <QObject>

/* Combines path and scenery information for a flight simulator type */
struct FsPathType
{
  QString basePath /* Base path where fsx.exe or prepar3d.exe can be found */,
          sceneryCfg /* full path and name of scenery.cfg file */;
  bool hasDatabase = false, /* true if a database was found in the configuration direcory */
       isInstalled = false /* True if the simulator is installed on the system */;
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

  /* Checks for fs installtions and databases and populates the hash map */
  void fillDefault();

  /* Get the latest/newest simulator from all installed ones or databases found */
  atools::fs::FsPaths::SimulatorType getBest();

  /* Get lastest/newest installed simulator */
  atools::fs::FsPaths::SimulatorType getBestInstalled();

  /* Get all installed simulators */
  QList<atools::fs::FsPaths::SimulatorType> getAllInstalled() const;

  /* Get all simulators that have a database */
  QList<atools::fs::FsPaths::SimulatorType> getAllHavingDatabase() const;

  /* Make needed base class methods public */
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

  void fillOneDefault(atools::fs::FsPaths::SimulatorType type);

};

QDataStream& operator<<(QDataStream& out, const SimulatorTypeMap& obj);
QDataStream& operator>>(QDataStream& in, SimulatorTypeMap& obj);

Q_DECLARE_METATYPE(SimulatorTypeMap);

#endif // LITTLENAVMAP_DBTYPES_H
