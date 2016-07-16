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

#include "db/dbtypes.h"

#include <QDebug>
#include <QDataStream>

using atools::fs::FsPaths;

void FsPathTypeMap::fillOneDefault(atools::fs::FsPaths::SimulatorType type)
{
  if(FsPaths::hasSim(type))
  {
    FsPathType& path = (*this)[type];
    if(path.basePath.isEmpty())
      path.basePath = FsPaths::getBasePath(type);
    if(path.sceneryCfg.isEmpty())
      path.sceneryCfg = FsPaths::getSceneryLibraryPath(type);

    // If already present or not - this one has a registry entry
    path.hasRegistry = true;
  }
  else
  {
    // Not in the list anymore - remove it - can be adder later again if database is found
    if(contains(type))
      remove(type);
  }
}

void FsPathTypeMap::fillDefault()
{
  for(atools::fs::FsPaths::SimulatorType type : FsPaths::ALL_SIMULATOR_TYPES)
    fillOneDefault(type);
}

atools::fs::FsPaths::SimulatorType FsPathTypeMap::getBestSimulator()
{
  if(contains(atools::fs::FsPaths::P3D_V3))
    return atools::fs::FsPaths::P3D_V3;
  else if(contains(atools::fs::FsPaths::P3D_V2))
    return atools::fs::FsPaths::P3D_V2;
  else if(contains(atools::fs::FsPaths::FSX_SE))
    return atools::fs::FsPaths::FSX_SE;
  else if(contains(atools::fs::FsPaths::FSX))
    return atools::fs::FsPaths::FSX;

  return atools::fs::FsPaths::UNKNOWN;
}

atools::fs::FsPaths::SimulatorType FsPathTypeMap::getBestLoadingSimulator()
{
  if(contains(atools::fs::FsPaths::P3D_V3) && value(atools::fs::FsPaths::P3D_V3).hasRegistry)
    return atools::fs::FsPaths::P3D_V3;
  else if(contains(atools::fs::FsPaths::P3D_V2) && value(atools::fs::FsPaths::P3D_V2).hasRegistry)
    return atools::fs::FsPaths::P3D_V2;
  else if(contains(atools::fs::FsPaths::FSX_SE) && value(atools::fs::FsPaths::FSX_SE).hasRegistry)
    return atools::fs::FsPaths::FSX_SE;
  else if(contains(atools::fs::FsPaths::FSX) && value(atools::fs::FsPaths::FSX).hasRegistry)
    return atools::fs::FsPaths::FSX;

  return atools::fs::FsPaths::UNKNOWN;
}

QList<atools::fs::FsPaths::SimulatorType> FsPathTypeMap::getAllRegistryPaths() const
{
  QList<atools::fs::FsPaths::SimulatorType> retval;
  for(atools::fs::FsPaths::SimulatorType p : keys())
    if(value(p).hasRegistry)
      retval.append(p);
  return retval;
}

QList<atools::fs::FsPaths::SimulatorType> FsPathTypeMap::getAllDatabasePaths() const
{
  QList<atools::fs::FsPaths::SimulatorType> retval;
  for(atools::fs::FsPaths::SimulatorType p : keys())
    if(value(p).hasDatabase)
      retval.append(p);
  return retval;
}

QDebug operator<<(QDebug out, const FsPathType& record)
{
  QDebugStateSaver saver(out);
  out.nospace() << "FsPathType["
  << "registry entry " << record.hasRegistry
  << ", has database " << record.hasDatabase
  << ", base path " << record.basePath
  << ", scenery config " << record.sceneryCfg
  << "]";
  return out;
}

QDataStream& operator<<(QDataStream& out, const FsPathType& obj)
{
  out << obj.basePath << obj.sceneryCfg;
  return out;
}

QDataStream& operator>>(QDataStream& in, FsPathType& obj)
{
  in >> obj.basePath >> obj.sceneryCfg;
  return in;
}

QDataStream& operator<<(QDataStream& out, const FsPathTypeMap& obj)
{
  QHash<atools::fs::FsPaths::SimulatorType, FsPathType> hash(obj);
  out << hash;
  return out;
}

QDataStream& operator>>(QDataStream& in, FsPathTypeMap& obj)
{
  QHash<atools::fs::FsPaths::SimulatorType, FsPathType> hash;
  in >> hash;
  obj.swap(hash);
  return in;
}
