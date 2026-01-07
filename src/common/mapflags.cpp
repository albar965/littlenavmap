/*****************************************************************************
* Copyright 2015-2026 Alexander Barthel alex@littlenavmap.org
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

const QList<map::MapAirspaceSource> MAP_AIRSPACE_SRC_VALUES({AIRSPACE_SRC_SIM, AIRSPACE_SRC_NAV, AIRSPACE_SRC_ONLINE, AIRSPACE_SRC_USER});
const QList<map::MapAirspaceSource> MAP_AIRSPACE_SRC_NO_ONLINE_VALUES({AIRSPACE_SRC_SIM, AIRSPACE_SRC_NAV, AIRSPACE_SRC_USER});

QDebug operator<<(QDebug out, const map::MapAirspaceSource& type)
{
  out << map::MapAirspaceSources(type);
  return out;
}

QDebug operator<<(QDebug out, const map::MapAirspaceSources& type)
{
  QDebugStateSaver saver(out);

  QStringList flags;
  if(type == AIRSPACE_SRC_NONE)
    flags.append("AIRSPACE_SRC_NONE");
  else
  {
    if(type.testFlag(AIRSPACE_SRC_SIM))
      flags.append("AIRSPACE_SRC_SIM");
    if(type.testFlag(AIRSPACE_SRC_NAV))
      flags.append("AIRSPACE_SRC_NAV");
    if(type.testFlag(AIRSPACE_SRC_ONLINE))
      flags.append("AIRSPACE_SRC_ONLINE");
    if(type.testFlag(AIRSPACE_SRC_USER))
      flags.append("AIRSPACE_SRC_USER");
  }
  out.nospace().noquote() << flags.join("|");

  return out;
}

QDebug operator<<(QDebug out, const map::MapType& type)
{
  out << map::MapTypes(type);
  return out;
}

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
    if(type.testFlag(RUNWAY))
      flags.append("RUNWAY");
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
    if(type.testFlag(START))
      flags.append("START");
    if(type.testFlag(RUNWAYEND))
      flags.append("RUNWAYEND");
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
    if(type.testFlag(AIRPORT_UNLIGHTED))
      flags.append("AIRPORT_UNLIGHTED");
    if(type.testFlag(AIRPORT_NO_PROCS))
      flags.append("AIRPORT_NO_PROCS");
    if(type.testFlag(AIRPORT_CLOSED))
      flags.append("AIRPORT_CLOSED");
    if(type.testFlag(AIRPORT_MILITARY))
      flags.append("AIRPORT_MILITARY");
    if(type.testFlag(PROCEDURE_POINT))
      flags.append("PROCEDURE_POINT");
    if(type.testFlag(AIRCRAFT_TRAIL))
      flags.append("AIRCRAFT_TRAIL");
    if(type.testFlag(AIRPORT_ADDON_ZOOM))
      flags.append("AIRPORT_ADDON_ZOOM");
    if(type.testFlag(AIRPORT_ADDON_ZOOM_FILTER))
      flags.append("AIRPORT_ADDON_ZOOM_FILTER");
  }
  out.nospace().noquote() << flags.join("|");

  return out;
}

QDebug operator<<(QDebug out, const map::MapDisplayTypes& type)
{
  QDebugStateSaver saver(out);

  QStringList flags;
  if(type == DISPLAY_TYPE_NONE)
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

QDebug operator<<(QDebug out, const map::MapWeatherSource& type)
{
  if(type == WEATHER_SOURCE_SIMULATOR)
    out << "WEATHER_SOURCE_SIMULATOR";
  if(type == WEATHER_SOURCE_ACTIVE_SKY)
    out << "WEATHER_SOURCE_ACTIVE_SKY";
  if(type == WEATHER_SOURCE_NOAA)
    out << "WEATHER_SOURCE_NOAA";
  if(type == WEATHER_SOURCE_VATSIM)
    out << "WEATHER_SOURCE_VATSIM";
  if(type == WEATHER_SOURCE_IVAO)
    out << "WEATHER_SOURCE_IVAO";
  if(type == WEATHER_SOURCE_DISABLED)
    out << "WEATHER_SOURCE_DISABLED";
  return out;
}

QDebug operator<<(QDebug out, const map::MapAirspaceTypes& type)
{
  QDebugStateSaver saver(out);

  QStringList flags;
  if(type == AIRSPACE_NONE)
    flags.append("AIRSPACE_NONE");
  else
  {
    if(type.testFlag(CENTER))
      flags.append("CENTER");
    if(type.testFlag(CLASS_A))
      flags.append("CLASS_A");
    if(type.testFlag(CLASS_B))
      flags.append("CLASS_B");
    if(type.testFlag(CLASS_C))
      flags.append("CLASS_C");
    if(type.testFlag(CLASS_D))
      flags.append("CLASS_D");
    if(type.testFlag(CLASS_E))
      flags.append("CLASS_E");
    if(type.testFlag(CLASS_F))
      flags.append("CLASS_F");
    if(type.testFlag(CLASS_G))
      flags.append("CLASS_G");
    if(type.testFlag(TOWER))
      flags.append("TOWER");
    if(type.testFlag(CLEARANCE))
      flags.append("CLEARANCE");
    if(type.testFlag(GROUND))
      flags.append("GROUND");
    if(type.testFlag(DEPARTURE))
      flags.append("DEPARTURE");
    if(type.testFlag(APPROACH))
      flags.append("APPROACH");
    if(type.testFlag(MOA))
      flags.append("MOA");
    if(type.testFlag(RESTRICTED))
      flags.append("RESTRICTED");
    if(type.testFlag(PROHIBITED))
      flags.append("PROHIBITED");
    if(type.testFlag(WARNING))
      flags.append("WARNING");
    if(type.testFlag(ALERT))
      flags.append("ALERT");
    if(type.testFlag(DANGER))
      flags.append("DANGER");
    if(type.testFlag(NATIONAL_PARK))
      flags.append("NATIONAL_PARK");
    if(type.testFlag(MODEC))
      flags.append("MODEC");
    if(type.testFlag(RADAR))
      flags.append("RADAR");
    if(type.testFlag(TRAINING))
      flags.append("TRAINING");
    if(type.testFlag(GLIDERPROHIBITED))
      flags.append("GLIDERPROHIBITED");
    if(type.testFlag(WAVEWINDOW))
      flags.append("WAVEWINDOW");
    if(type.testFlag(CAUTION))
      flags.append("CAUTION");
    if(type.testFlag(ONLINE_OBSERVER))
      flags.append("ONLINE_OBSERVER");
    if(type.testFlag(FIR))
      flags.append("FIR");
    if(type.testFlag(UIR))
      flags.append("UIR");
    if(type.testFlag(GCA))
      flags.append("GCA");
    if(type.testFlag(MCTR))
      flags.append("MCTR");
    if(type.testFlag(TRSA))
      flags.append("TRSA");
  }
  out.nospace().noquote() << flags.join("|");

  return out;
}

QDebug operator<<(QDebug out, const map::MapAirspaceFlags& type)
{
  QDebugStateSaver saver(out);

  QStringList flags;
  if(type == AIRSPACE_ALTITUDE_FLAG_NONE)
    flags.append("NONE");
  else
  {
    if(type.testFlag(AIRSPACE_ALTITUDE_ALL))
      flags.append("AIRSPACE_ALTITUDE_ALL");
    if(type.testFlag(AIRSPACE_ALTITUDE_FLIGHTPLAN))
      flags.append("AIRSPACE_ALTITUDE_FLIGHTPLAN");
    if(type.testFlag(AIRSPACE_ALTITUDE_SET))
      flags.append("AIRSPACE_ALTITUDE_SET");
    if(type.testFlag(AIRSPACE_ALL_ON))
      flags.append("AIRSPACE_ALL_ON");
    if(type.testFlag(AIRSPACE_ALL_OFF))
      flags.append("AIRSPACE_ALL_OFF");
    if(type.testFlag(AIRSPACE_NO_MULTIPLE_Z))
      flags.append("AIRSPACE_NO_MULTIPLE_Z");
  }
  out.nospace().noquote() << flags.join("|");

  return out;
}

QDebug operator<<(QDebug out, const map::MapAirspaceFilter& type)
{
  out << "MapAirspaceFilter[";
  out << type.flags;
  out << type.types;
  out << "minAltitudeFt" << type.minAltitudeFt;
  out << "maxAltitudeFt" << type.maxAltitudeFt;
  out << "]";
  return out;
}

QDataStream& operator>>(QDataStream& dataStream, MapAirspaceFilter& obj)
{
  quint32 flags;
  quint64 types;
  dataStream >> types >> flags >> obj.minAltitudeFt >> obj.maxAltitudeFt;
  obj.types = map::MapAirspaceTypes(types);
  obj.flags = map::MapAirspaceFlags(flags);

  return dataStream;
}

QDataStream& operator<<(QDataStream& dataStream, const MapAirspaceFilter& obj)
{
  dataStream << static_cast<quint64>(obj.types) << static_cast<quint32>(obj.flags) << obj.minAltitudeFt << obj.maxAltitudeFt;
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

QString mapWeatherSourceStringShort(MapWeatherSource source)
{
  switch(source)
  {
    case map::WEATHER_SOURCE_DISABLED:
      return QObject::tr("—");

    case map::WEATHER_SOURCE_SIMULATOR:
      return QObject::tr("Simulator");

    case map::WEATHER_SOURCE_ACTIVE_SKY:
      return QObject::tr("AS");

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
