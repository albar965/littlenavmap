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

struct FsPath
{
  QString basePath, sceneryCfg;
  atools::fs::FsPaths::SimulatorType type;
};

QDataStream& operator<<(QDataStream& out, const FsPath& obj);
QDataStream& operator>>(QDataStream& in, FsPath& obj);

Q_DECLARE_METATYPE(FsPath);
Q_DECLARE_TYPEINFO(FsPath, Q_MOVABLE_TYPE);

class FsPathMapList :
  public QHash<atools::fs::FsPaths::SimulatorType, FsPath>
{
public:
  void fillDefault();
  atools::fs::FsPaths::SimulatorType getLatestSimulator();

private:
  void fillOneDefault(atools::fs::FsPaths::SimulatorType type);

};

QDataStream& operator<<(QDataStream& out, const FsPathMapList& obj);
QDataStream& operator>>(QDataStream& in, FsPathMapList& obj);

Q_DECLARE_METATYPE(FsPathMapList);

#endif // DBTYPES_H
