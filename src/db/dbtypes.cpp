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

#include "db/dbtypes.h"

#include <QDebug>
#include <QDataStream>
#include <QDir>

using atools::fs::FsPaths;

void SimulatorTypeMap::fillDefault()
{
  for(FsPaths::SimulatorType type : FsPaths::getAllSimulatorTypes())
    fillOneDefault(type);
}

atools::fs::FsPaths::SimulatorType SimulatorTypeMap::getBest() const
{
#if defined(Q_OS_WIN32) || defined(DEBUG_FS_PATHS)
  if(contains(FsPaths::MSFS) && value(FsPaths::MSFS).hasDatabase)
    return FsPaths::MSFS;
  else if(contains(FsPaths::P3D_V5) && value(FsPaths::P3D_V5).hasDatabase)
    return FsPaths::P3D_V5;
  else if(contains(FsPaths::P3D_V4) && value(FsPaths::P3D_V4).hasDatabase)
    return FsPaths::P3D_V4;
  else if(contains(FsPaths::P3D_V3) && value(FsPaths::P3D_V3).hasDatabase)
    return FsPaths::P3D_V3;
  else if(contains(FsPaths::P3D_V2) && value(FsPaths::P3D_V2).hasDatabase)
    return FsPaths::P3D_V2;
  else if(contains(FsPaths::FSX_SE) && value(FsPaths::FSX_SE).hasDatabase)
    return FsPaths::FSX_SE;
  else if(contains(FsPaths::FSX) && value(FsPaths::FSX).hasDatabase)
    return FsPaths::FSX;

  // else if(contains(FsPaths::XPLANE11) && value(FsPaths::XPLANE11).hasDatabase)
  // If all fails use X-Plane as default
  return FsPaths::XPLANE11;

#else
  // macOS and Linux - only X-Plane
  return FsPaths::XPLANE11;

#endif
}

FsPaths::SimulatorType SimulatorTypeMap::getBestInstalled() const
{
#if defined(Q_OS_WIN32) || defined(DEBUG_FS_PATHS)

  FsPaths::SimulatorType type = getBestInstalled({FsPaths::MSFS, FsPaths::P3D_V5, FsPaths::P3D_V4, FsPaths::P3D_V3,
                                                  FsPaths::P3D_V2, FsPaths::FSX_SE, FsPaths::FSX});

  if(type == FsPaths::UNKNOWN)
    return FsPaths::XPLANE11;
  else
    return type;

#else
  // macOS and Linux - only X-Plane
  return FsPaths::XPLANE11;

#endif
}

FsPaths::SimulatorType SimulatorTypeMap::getBestInstalled(const FsPaths::SimulatorTypeVector& types) const
{
#if defined(Q_OS_WIN32) || defined(DEBUG_FS_PATHS)
  for(FsPaths::SimulatorType type : types)
  {
    if(contains(type) && (*this)[type].isInstalled)
      return type;
  }

  return FsPaths::UNKNOWN;

#else
  return FsPaths::UNKNOWN;

#endif
}

QList<FsPaths::SimulatorType> SimulatorTypeMap::getAllInstalled() const
{
  QList<FsPaths::SimulatorType> retval;
  for(FsPaths::SimulatorType simType : keys())
    if(value(simType).isInstalled)
      retval.append(simType);
  return retval;
}

QList<FsPaths::SimulatorType> SimulatorTypeMap::getAllHavingDatabase() const
{
  QList<FsPaths::SimulatorType> retval;
  for(FsPaths::SimulatorType simType : keys())
    if(value(simType).hasDatabase)
      retval.append(simType);
  return retval;
}

void SimulatorTypeMap::fillOneDefault(FsPaths::SimulatorType type)
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
    path.isInstalled = FsPaths::hasSimulator(type);
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
