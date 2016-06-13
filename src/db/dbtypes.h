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

#ifndef DBTYPES_H
#define DBTYPES_H

#include "fs/fspaths.h"

#include <QHash>
#include <QString>
#include <QObject>

struct FsPathType
{
  QString basePath, sceneryCfg;
  bool hasDatabase = false, hasRegistry = false;
};

QDebug operator<<(QDebug out, const FsPathType& record);

QDataStream& operator<<(QDataStream& out, const FsPathType& obj);
QDataStream& operator>>(QDataStream& in, FsPathType& obj);

Q_DECLARE_METATYPE(FsPathType);
Q_DECLARE_TYPEINFO(FsPathType, Q_MOVABLE_TYPE);

class FsPathTypeMap :
  public QHash<atools::fs::FsPaths::SimulatorType, FsPathType>
{
public:
  void fillDefault();
  atools::fs::FsPaths::SimulatorType getBestSimulator();

  QList<atools::fs::FsPaths::SimulatorType> getAllRegistryPaths() const;

  QList<atools::fs::FsPaths::SimulatorType> getAllDatabasePaths() const;

  atools::fs::FsPaths::SimulatorType getBestLoadingSimulator();

private:
  void fillOneDefault(atools::fs::FsPaths::SimulatorType type);

};

QDataStream& operator<<(QDataStream& out, const FsPathTypeMap& obj);
QDataStream& operator>>(QDataStream& in, FsPathTypeMap& obj);

Q_DECLARE_METATYPE(FsPathTypeMap);

#endif // DBTYPES_H
