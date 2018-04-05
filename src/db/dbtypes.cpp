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

#include "db/dbtypes.h"

#include <QDebug>
#include <QDataStream>
#include <QDir>

using atools::fs::FsPaths;

void SimulatorTypeMap::fillDefault()
{
  for(atools::fs::FsPaths::SimulatorType type : FsPaths::getAllSimulatorTypes())
    fillOneDefault(type);
}

atools::fs::FsPaths::SimulatorType SimulatorTypeMap::getBest()
{
  if(contains(atools::fs::FsPaths::P3D_V4) && value(atools::fs::FsPaths::P3D_V4).hasDatabase)
    return atools::fs::FsPaths::P3D_V4;
  else if(contains(atools::fs::FsPaths::P3D_V3) && value(atools::fs::FsPaths::P3D_V3).hasDatabase)
    return atools::fs::FsPaths::P3D_V3;
  else if(contains(atools::fs::FsPaths::P3D_V2) && value(atools::fs::FsPaths::P3D_V2).hasDatabase)
    return atools::fs::FsPaths::P3D_V2;
  else if(contains(atools::fs::FsPaths::FSX_SE) && value(atools::fs::FsPaths::FSX_SE).hasDatabase)
    return atools::fs::FsPaths::FSX_SE;
  else if(contains(atools::fs::FsPaths::FSX) && value(atools::fs::FsPaths::FSX).hasDatabase)
    return atools::fs::FsPaths::FSX;
  else if(contains(atools::fs::FsPaths::XPLANE11) && value(atools::fs::FsPaths::XPLANE11).hasDatabase)
    return atools::fs::FsPaths::XPLANE11;

  return atools::fs::FsPaths::UNKNOWN;
}

atools::fs::FsPaths::SimulatorType SimulatorTypeMap::getBestInstalled()
{
  if(contains(atools::fs::FsPaths::P3D_V4) && value(atools::fs::FsPaths::P3D_V4).isInstalled)
    return atools::fs::FsPaths::P3D_V4;
  else if(contains(atools::fs::FsPaths::P3D_V3) && value(atools::fs::FsPaths::P3D_V3).isInstalled)
    return atools::fs::FsPaths::P3D_V3;
  else if(contains(atools::fs::FsPaths::P3D_V2) && value(atools::fs::FsPaths::P3D_V2).isInstalled)
    return atools::fs::FsPaths::P3D_V2;
  else if(contains(atools::fs::FsPaths::FSX_SE) && value(atools::fs::FsPaths::FSX_SE).isInstalled)
    return atools::fs::FsPaths::FSX_SE;
  else if(contains(atools::fs::FsPaths::FSX) && value(atools::fs::FsPaths::FSX).isInstalled)
    return atools::fs::FsPaths::FSX;
  else if(contains(atools::fs::FsPaths::XPLANE11) && value(atools::fs::FsPaths::XPLANE11).isInstalled)
    return atools::fs::FsPaths::FSX;

  return atools::fs::FsPaths::UNKNOWN;
}

QList<atools::fs::FsPaths::SimulatorType> SimulatorTypeMap::getAllInstalled() const
{
  QList<atools::fs::FsPaths::SimulatorType> retval;
  for(atools::fs::FsPaths::SimulatorType simType : keys())
    if(value(simType).isInstalled)
      retval.append(simType);
  return retval;
}

QList<atools::fs::FsPaths::SimulatorType> SimulatorTypeMap::getAllHavingDatabase() const
{
  QList<atools::fs::FsPaths::SimulatorType> retval;
  for(atools::fs::FsPaths::SimulatorType simType : keys())
    if(value(simType).hasDatabase)
      retval.append(simType);
  return retval;
}

void SimulatorTypeMap::fillOneDefault(atools::fs::FsPaths::SimulatorType type)
{
  // Simulator is installed - create a new entry or update the present one
  FsPathType& path = (*this)[type];
  if(path.basePath.isEmpty())
    path.basePath = FsPaths::getBasePath(type);
  if(path.sceneryCfg.isEmpty())
    path.sceneryCfg = FsPaths::getSceneryLibraryPath(type);

  if(type == FsPaths::XPLANE11)
    path.isInstalled = !path.basePath.isEmpty();
  else
    // If already present or not - this one has a registry entry
    path.isInstalled = FsPaths::hasSim(type);
}

QDebug operator<<(QDebug out, const FsPathType& record)
{
  QDebugStateSaver saver(out);
  out.nospace() << "FsPathType["
                << "registry entry " << record.isInstalled
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

  obj.basePath = QDir::toNativeSeparators(obj.basePath);
  obj.sceneryCfg = QDir::toNativeSeparators(obj.sceneryCfg);

  return in;
}

QDataStream& operator<<(QDataStream& out, const SimulatorTypeMap& obj)
{
  out << static_cast<QHash<atools::fs::FsPaths::SimulatorType, FsPathType> >(obj);
  return out;
}

QDataStream& operator>>(QDataStream& in, SimulatorTypeMap& obj)
{
  QHash<atools::fs::FsPaths::SimulatorType, FsPathType> hash;

  // Copy all entries to a basic type first
  in >> hash;
  obj.swap(hash);
  return in;
}
