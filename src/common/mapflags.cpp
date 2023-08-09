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

#include "common/mapflags.h"

#include <QDataStream>
#include <QDebug>

namespace map {

const QVector<map::MapAirspaceSources> MAP_AIRSPACE_SRC_VALUES =
{AIRSPACE_SRC_SIM, AIRSPACE_SRC_NAV, AIRSPACE_SRC_ONLINE, AIRSPACE_SRC_USER};

const QVector<map::MapAirspaceSources> MAP_AIRSPACE_SRC_NO_ONLINE_VALUES =
{AIRSPACE_SRC_SIM, AIRSPACE_SRC_NAV, AIRSPACE_SRC_USER};

QDebug operator<<(QDebug out, const map::MapTypes& type)
{
  QDebugStateSaver saver(out);

  QStringList flags;
  if(type == NONE)
    flags.append("NONE");
  else
  {
    if(type.testFlag(AIRPORT))
      flags.append("AIRPORT");
    if(type.testFlag(VOR))
      flags.append("VOR");
    if(type.testFlag(NDB))
      flags.append("NDB");
    if(type.testFlag(ILS))
      flags.append("ILS");
    if(type.testFlag(MARKER))
      flags.append("MARKER");
    if(type.testFlag(WAYPOINT))
      flags.append("WAYPOINT");
    if(type.testFlag(AIRWAY))
      flags.append("AIRWAY");
    if(type.testFlag(AIRWAYV))
      flags.append("AIRWAYV");
    if(type.testFlag(AIRWAYJ))
      flags.append("AIRWAYJ");
    if(type.testFlag(USER_FEATURE))
      flags.append("USER_FEATURE");
    if(type.testFlag(AIRCRAFT))
      flags.append("AIRCRAFT");
    if(type.testFlag(AIRCRAFT_AI))
      flags.append("AIRCRAFT_AI");
    if(type.testFlag(AIRCRAFT_AI_SHIP))
      flags.append("AIRCRAFT_AI_SHIP");
    if(type.testFlag(AIRPORT_MSA))
      flags.append("AIRPORT_MSA");
    if(type.testFlag(USERPOINTROUTE))
      flags.append("USERPOINTROUTE");
    if(type.testFlag(PARKING))
      flags.append("PARKING");
    if(type.testFlag(RUNWAYEND))
      flags.append("RUNWAYEND");
    if(type.testFlag(RUNWAY))
      flags.append("RUNWAY");
    if(type.testFlag(INVALID))
      flags.append("INVALID");
    if(type.testFlag(MISSED_APPROACH))
      flags.append("MISSED_APPROACH");
    if(type.testFlag(PROCEDURE))
      flags.append("PROCEDURE");
    if(type.testFlag(AIRSPACE))
      flags.append("AIRSPACE");
    if(type.testFlag(HELIPAD))
      flags.append("HELIPAD");
    if(type.testFlag(HOLDING))
      flags.append("HOLDING");
    if(type.testFlag(USERPOINT))
      flags.append("USERPOINT");
    if(type.testFlag(TRACK))
      flags.append("TRACK");
    if(type.testFlag(AIRCRAFT_ONLINE))
      flags.append("AIRCRAFT_ONLINE");
    if(type.testFlag(LOGBOOK))
      flags.append("LOGBOOK");
    if(type.testFlag(MARK_RANGE))
      flags.append("MARK_RANGE");
    if(type.testFlag(MARK_DISTANCE))
      flags.append("MARK_DISTANCE");
    if(type.testFlag(MARK_HOLDING))
      flags.append("MARK_HOLDING");
    if(type.testFlag(MARK_PATTERNS))
      flags.append("MARK_PATTERNS");
    if(type.testFlag(MARK_MSA))
      flags.append("MARK_MSA");
    if(type.testFlag(AIRPORT_HARD))
      flags.append("AIRPORT_HARD");
    if(type.testFlag(AIRPORT_SOFT))
      flags.append("AIRPORT_SOFT");
    if(type.testFlag(AIRPORT_WATER))
      flags.append("AIRPORT_WATER");
    if(type.testFlag(AIRPORT_HELIPAD))
      flags.append("AIRPORT_HELIPAD");
    if(type.testFlag(AIRPORT_EMPTY))
      flags.append("AIRPORT_EMPTY");
    if(type.testFlag(AIRPORT_ADDON))
      flags.append("AIRPORT_ADDON");
    if(type.testFlag(AIRPORT_UNLIGHTED))
      flags.append("AIRPORT_UNLIGHTED");
    if(type.testFlag(AIRPORT_NO_PROCS))
      flags.append("AIRPORT_NO_PROCS");
    if(type.testFlag(AIRPORT_CLOSED))
      flags.append("AIRPORT_CLOSED");
    if(type.testFlag(AIRPORT_MILITARY))
      flags.append("AIRPORT_MILITARY");
  }

  out.nospace().noquote() << flags.join("|");

  return out;
}

QDebug operator<<(QDebug out, const map::MapDisplayTypes& type)
{
  QDebugStateSaver saver(out);

  QStringList flags;
  if(type == NONE)
    flags.append("NONE");
  else
  {
    if(type.testFlag(DISPLAY_TYPE_NONE))
      flags.append("DISPLAY_TYPE_NONE");
    if(type.testFlag(AIRPORT_WEATHER))
      flags.append("AIRPORT_WEATHER");
    if(type.testFlag(MORA))
      flags.append("MINIMUM_ALTITUDE");
    if(type.testFlag(WIND_BARBS))
      flags.append("WIND_BARBS");
    if(type.testFlag(WIND_BARBS_ROUTE))
      flags.append("WIND_BARBS_ROUTE");
    if(type.testFlag(LOGBOOK_DIRECT))
      flags.append("LOGBOOK_DIRECT");
    if(type.testFlag(LOGBOOK_ROUTE))
      flags.append("LOGBOOK_ROUTE");
    if(type.testFlag(LOGBOOK_TRACK))
      flags.append("LOGBOOK_TRACK");
    if(type.testFlag(COMPASS_ROSE))
      flags.append("COMPASS_ROSE");
    if(type.testFlag(COMPASS_ROSE_ATTACH))
      flags.append("COMPASS_ROSE_ATTACH");
    if(type.testFlag(FLIGHTPLAN))
      flags.append("FLIGHTPLAN");
    if(type.testFlag(FLIGHTPLAN_TOC_TOD))
      flags.append("FLIGHTPLAN_TOC_TOD");
  }

  out.nospace().noquote() << flags.join("|");

  return out;
}

QDataStream& operator>>(QDataStream& dataStream, MapAirspaceFilter& obj)
{
  quint32 types, flags;
  dataStream >> types >> flags >> obj.minAltitudeFt >> obj.maxAltitudeFt;
  obj.types = map::MapAirspaceTypes(types);
  obj.flags = map::MapAirspaceFlags(flags);

  return dataStream;
}

QDataStream& operator<<(QDataStream& dataStream, const MapAirspaceFilter& obj)
{
  dataStream << static_cast<quint32>(obj.types) << static_cast<quint32>(obj.flags) << obj.minAltitudeFt << obj.maxAltitudeFt;
  return dataStream;
}

QString mapWeatherSourceString(MapWeatherSource source)
{
  switch(source)
  {
    case map::WEATHER_SOURCE_DISABLED:
      return QObject::tr("Disabled");

    case map::WEATHER_SOURCE_SIMULATOR:
      return QObject::tr("Flight Simulator");

    case map::WEATHER_SOURCE_ACTIVE_SKY:
      return QObject::tr("Active Sky");

    case map::WEATHER_SOURCE_NOAA:
      return QObject::tr("NOAA");

    case map::WEATHER_SOURCE_VATSIM:
      return QObject::tr("VATSIM");

    case map::WEATHER_SOURCE_IVAO:
      return QObject::tr("IVAO");
  }
  return QString();
}

} // namespace map
