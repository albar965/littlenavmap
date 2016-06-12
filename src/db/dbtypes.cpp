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

#include "dbtypes.h"

#include <QDataStream>

using atools::fs::FsPaths;

void FsPathMapList::fillOneDefault(atools::fs::FsPaths::SimulatorType type)
{
  if(FsPaths::hasSim(type))
  {
    if(!contains(type))
    {
      FsPath& path = (*this)[type];
      path.type = type;
      path.basePath = FsPaths::getBasePath(type);
      path.sceneryCfg = FsPaths::getSceneryLibraryPath(type);
    }
  }
  else
  {
    // Not in the list anymore - remove
    if(contains(type))
      remove(type);
  }
}

void FsPathMapList::fillDefault()
{
  for(atools::fs::FsPaths::SimulatorType type : FsPaths::ALL_SIMULATOR_TYPES)
    fillOneDefault(type);
}

atools::fs::FsPaths::SimulatorType FsPathMapList::getLatestSimulator()
{
  // Get the newest simulator for default values
  if(FsPaths::hasSim(atools::fs::FsPaths::P3D_V3))
    return atools::fs::FsPaths::P3D_V3;
  else if(FsPaths::hasSim(atools::fs::FsPaths::P3D_V2))
    return atools::fs::FsPaths::P3D_V2;
  else if(FsPaths::hasSim(atools::fs::FsPaths::FSX_SE))
    return atools::fs::FsPaths::FSX_SE;
  else if(FsPaths::hasSim(atools::fs::FsPaths::FSX))
    return atools::fs::FsPaths::FSX;

  return atools::fs::FsPaths::UNKNOWN;
}

QDataStream& operator<<(QDataStream& out, const FsPath& obj)
{
  out << obj.basePath << obj.sceneryCfg << obj.type;
  return out;
}

QDataStream& operator>>(QDataStream& in, FsPath& obj)
{
  in >> obj.basePath >> obj.sceneryCfg >> obj.type;
  return in;
}

QDataStream& operator<<(QDataStream& out, const FsPathMapList& obj)
{
  QHash<atools::fs::FsPaths::SimulatorType, FsPath> hash(obj);
  out << hash;
  return out;
}

QDataStream& operator>>(QDataStream& in, FsPathMapList& obj)
{
  QHash<atools::fs::FsPaths::SimulatorType, FsPath> hash;
  in >> hash;
  obj.swap(hash);
  return in;
}
