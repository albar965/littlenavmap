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

#include "db/dbtypes.h"

#include <QDebug>
#include <QDataStream>
#include <QDir>

using atools::fs::FsPaths;

void SimulatorTypeMap::fillDefault(navdb::Status navDatabaseStatus)
{
  for(FsPaths::SimulatorType type : FsPaths::getAllSimulatorTypes())
    fillOneDefault(type, navDatabaseStatus);
}

atools::fs::FsPaths::SimulatorType SimulatorTypeMap::getBest() const
{
  if(contains(FsPaths::MSFS) && value(FsPaths::MSFS).hasDatabase)
    return FsPaths::MSFS;
  else if(contains(FsPaths::XPLANE_12) && value(FsPaths::XPLANE_12).hasDatabase)
    return FsPaths::XPLANE_12;
  else if(contains(FsPaths::XPLANE_11) && value(FsPaths::XPLANE_11).hasDatabase)
    return FsPaths::XPLANE_11;
  else if(contains(FsPaths::P3D_V6) && value(FsPaths::P3D_V6).hasDatabase)
    return FsPaths::P3D_V6;
  else if(contains(FsPaths::P3D_V5) && value(FsPaths::P3D_V5).hasDatabase)
    return FsPaths::P3D_V5;
  else if(contains(FsPaths::P3D_V4) && value(FsPaths::P3D_V4).hasDatabase)
    return FsPaths::P3D_V4;
  else if(contains(FsPaths::P3D_V3) && value(FsPaths::P3D_V3).hasDatabase)
    return FsPaths::P3D_V3;
  else if(contains(FsPaths::FSX_SE) && value(FsPaths::FSX_SE).hasDatabase)
    return FsPaths::FSX_SE;
  else if(contains(FsPaths::FSX) && value(FsPaths::FSX).hasDatabase)
    return FsPaths::FSX;

  return FsPaths::NONE;
}

FsPaths::SimulatorType SimulatorTypeMap::getBestInstalled() const
{
  return getBestInstalled({FsPaths::MSFS, FsPaths::XPLANE_12, FsPaths::XPLANE_11, FsPaths::P3D_V6, FsPaths::P3D_V5, FsPaths::P3D_V4,
                           FsPaths::P3D_V3, FsPaths::FSX_SE, FsPaths::FSX});
}

FsPaths::SimulatorType SimulatorTypeMap::getBestInstalled(const FsPaths::SimulatorTypeVector& types) const
{
  for(FsPaths::SimulatorType type : types)
  {
    if(contains(type) && (*this)[type].isInstalled)
      return type;
  }

  return FsPaths::NONE;
}

QList<FsPaths::SimulatorType> SimulatorTypeMap::getAllInstalled() const
{
  QList<FsPaths::SimulatorType> retval;
  for(auto it = begin(); it != end(); ++it)
  {
    if(it.value().isInstalled)
      retval.append(it.key());
  }
  return retval;
}

QList<FsPaths::SimulatorType> SimulatorTypeMap::getAllHavingDatabase() const
{
  QList<FsPaths::SimulatorType> retval;
  for(auto it = begin(); it != end(); ++it)
  {
    if(it.value().hasDatabase)
      retval.append(it.key());
  }
  return retval;
}

void SimulatorTypeMap::fillOneDefault(FsPaths::SimulatorType type, navdb::Status navDatabaseStatus)
{
  // Simulator is installed - create a new entry or update the present one
  FsPathType& path = (*this)[type];
  if(path.basePath.isEmpty())
    path.basePath = FsPaths::getBasePath(type);

  if(path.sceneryCfg.isEmpty())
    path.sceneryCfg = FsPaths::getSceneryLibraryPath(type);

  // Assign status if passed to this function
  if(navDatabaseStatus != navdb::UNKNOWN)
    path.navDatabaseStatus = navDatabaseStatus;

  // Reset to mixed if invalid
  if(path.navDatabaseStatus == navdb::UNKNOWN)
    path.navDatabaseStatus = navdb::MIXED;

  // If already present or not - this one has a registry entry or an installation file for X-Plane
  path.isInstalled = FsPaths::hasSimulator(type);
}

QDebug operator<<(QDebug out, const FsPathType& record)
{
  QDebugStateSaver saver(out);
  out.nospace() << "FsPathType["
                << "isInstalled " << record.isInstalled
                << ", hasDatabase " << record.hasDatabase
                << ", basePath " << record.basePath
                << ", sceneryCfg " << record.sceneryCfg
                << "]";
  return out;
}

QDataStream& operator<<(QDataStream& out, const FsPathType& obj)
{
  out << obj.basePath << obj.sceneryCfg << static_cast<quint8>(obj.navDatabaseStatus);
  return out;
}

QDataStream& operator>>(QDataStream& in, FsPathType& obj)
{
  quint8 navStatus;
  in >> obj.basePath >> obj.sceneryCfg >> navStatus;

  obj.basePath = QDir::toNativeSeparators(obj.basePath);
  obj.sceneryCfg = QDir::toNativeSeparators(obj.sceneryCfg);
  obj.navDatabaseStatus = static_cast<navdb::Status>(navStatus);

  return in;
}

QDataStream& operator<<(QDataStream& out, const SimulatorTypeMap& obj)
{
  out << static_cast<QHash<FsPaths::SimulatorType, FsPathType> >(obj);
  return out;
}

QDataStream& operator>>(QDataStream& in, SimulatorTypeMap& obj)
{
  QHash<FsPaths::SimulatorType, FsPathType> hash;

  // Copy all entries to a basic type first
  in >> hash;
  obj.swap(hash);
  return in;
}
