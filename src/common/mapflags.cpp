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

map::MapTypes mapTypeFromString(const QStringList& flags)
{
  map::MapTypes type = NONE;

  for(const QString& str : flags)
  {
    if(str == QStringLiteral("AIRPORT"))
      type.setFlag(AIRPORT);
    if(str == QStringLiteral("RUNWAY"))
      type.setFlag(RUNWAY);
    if(str == QStringLiteral("VOR"))
      type.setFlag(VOR);
    if(str == QStringLiteral("NDB"))
      type.setFlag(NDB);
    if(str == QStringLiteral("ILS"))
      type.setFlag(ILS);
    if(str == QStringLiteral("MARKER"))
      type.setFlag(MARKER);
    if(str == QStringLiteral("WAYPOINT"))
      type.setFlag(WAYPOINT);
    if(str == QStringLiteral("AIRWAY"))
      type.setFlag(AIRWAY);
    if(str == QStringLiteral("AIRWAYV"))
      type.setFlag(AIRWAYV);
    if(str == QStringLiteral("AIRWAYJ"))
      type.setFlag(AIRWAYJ);
    if(str == QStringLiteral("USER_FEATURE"))
      type.setFlag(USER_FEATURE);
    if(str == QStringLiteral("AIRCRAFT"))
      type.setFlag(AIRCRAFT);
    if(str == QStringLiteral("AIRCRAFT_AI"))
      type.setFlag(AIRCRAFT_AI);
    if(str == QStringLiteral("AIRCRAFT_AI_SHIP"))
      type.setFlag(AIRCRAFT_AI_SHIP);
    if(str == QStringLiteral("AIRPORT_MSA"))
      type.setFlag(AIRPORT_MSA);
    if(str == QStringLiteral("USERPOINTROUTE"))
      type.setFlag(USERPOINTROUTE);
    if(str == QStringLiteral("PARKING"))
      type.setFlag(PARKING);
    if(str == QStringLiteral("START"))
      type.setFlag(START);
    if(str == QStringLiteral("RUNWAYEND"))
      type.setFlag(RUNWAYEND);
    if(str == QStringLiteral("INVALID"))
      type.setFlag(INVALID);
    if(str == QStringLiteral("MISSED_APPROACH"))
      type.setFlag(MISSED_APPROACH);
    if(str == QStringLiteral("PROCEDURE"))
      type.setFlag(PROCEDURE);
    if(str == QStringLiteral("AIRSPACE"))
      type.setFlag(AIRSPACE);
    if(str == QStringLiteral("HELIPAD"))
      type.setFlag(HELIPAD);
    if(str == QStringLiteral("HOLDING"))
      type.setFlag(HOLDING);
    if(str == QStringLiteral("USERPOINT"))
      type.setFlag(USERPOINT);
    if(str == QStringLiteral("TRACK"))
      type.setFlag(TRACK);
    if(str == QStringLiteral("AIRCRAFT_ONLINE"))
      type.setFlag(AIRCRAFT_ONLINE);
    if(str == QStringLiteral("LOGBOOK"))
      type.setFlag(LOGBOOK);
    if(str == QStringLiteral("MARK_RANGE"))
      type.setFlag(MARK_RANGE);
    if(str == QStringLiteral("MARK_DISTANCE"))
      type.setFlag(MARK_DISTANCE);
    if(str == QStringLiteral("MARK_HOLDING"))
      type.setFlag(MARK_HOLDING);
    if(str == QStringLiteral("MARK_PATTERNS"))
      type.setFlag(MARK_PATTERNS);
    if(str == QStringLiteral("MARK_MSA"))
      type.setFlag(MARK_MSA);
    if(str == QStringLiteral("AIRPORT_HARD"))
      type.setFlag(AIRPORT_HARD);
    if(str == QStringLiteral("AIRPORT_SOFT"))
      type.setFlag(AIRPORT_SOFT);
    if(str == QStringLiteral("AIRPORT_WATER"))
      type.setFlag(AIRPORT_WATER);
    if(str == QStringLiteral("AIRPORT_HELIPAD"))
      type.setFlag(AIRPORT_HELIPAD);
    if(str == QStringLiteral("AIRPORT_EMPTY"))
      type.setFlag(AIRPORT_EMPTY);
    if(str == QStringLiteral("AIRPORT_UNLIGHTED"))
      type.setFlag(AIRPORT_UNLIGHTED);
    if(str == QStringLiteral("AIRPORT_NO_PROCS"))
      type.setFlag(AIRPORT_NO_PROCS);
    if(str == QStringLiteral("AIRPORT_CLOSED"))
      type.setFlag(AIRPORT_CLOSED);
    if(str == QStringLiteral("AIRPORT_MILITARY"))
      type.setFlag(AIRPORT_MILITARY);
    if(str == QStringLiteral("PROCEDURE_POINT"))
      type.setFlag(PROCEDURE_POINT);
    if(str == QStringLiteral("AIRCRAFT_TRAIL"))
      type.setFlag(AIRCRAFT_TRAIL);
    if(str == QStringLiteral("AIRPORT_ADDON_ZOOM"))
      type.setFlag(AIRPORT_ADDON_ZOOM);
    if(str == QStringLiteral("AIRPORT_ADDON_ZOOM_FILTER"))
      type.setFlag(AIRPORT_ADDON_ZOOM_FILTER);
  }
  return type;
}

QStringList mapTypeToString(const map::MapTypes& flags)
{
  ATOOLS_FLAGS_TO_STR_BEGIN(NONE);
  ATOOLS_FLAGS_TO_STR(AIRPORT);
  ATOOLS_FLAGS_TO_STR(RUNWAY);
  ATOOLS_FLAGS_TO_STR(VOR);
  ATOOLS_FLAGS_TO_STR(NDB);
  ATOOLS_FLAGS_TO_STR(ILS);
  ATOOLS_FLAGS_TO_STR(MARKER);
  ATOOLS_FLAGS_TO_STR(WAYPOINT);
  ATOOLS_FLAGS_TO_STR(AIRWAY);
  ATOOLS_FLAGS_TO_STR(AIRWAYV);
  ATOOLS_FLAGS_TO_STR(AIRWAYJ);
  ATOOLS_FLAGS_TO_STR(USER_FEATURE);
  ATOOLS_FLAGS_TO_STR(AIRCRAFT);
  ATOOLS_FLAGS_TO_STR(AIRCRAFT_AI);
  ATOOLS_FLAGS_TO_STR(AIRCRAFT_AI_SHIP);
  ATOOLS_FLAGS_TO_STR(AIRPORT_MSA);
  ATOOLS_FLAGS_TO_STR(USERPOINTROUTE);
  ATOOLS_FLAGS_TO_STR(PARKING);
  ATOOLS_FLAGS_TO_STR(START);
  ATOOLS_FLAGS_TO_STR(RUNWAYEND);
  ATOOLS_FLAGS_TO_STR(INVALID);
  ATOOLS_FLAGS_TO_STR(MISSED_APPROACH);
  ATOOLS_FLAGS_TO_STR(PROCEDURE);
  ATOOLS_FLAGS_TO_STR(AIRSPACE);
  ATOOLS_FLAGS_TO_STR(HELIPAD);
  ATOOLS_FLAGS_TO_STR(HOLDING);
  ATOOLS_FLAGS_TO_STR(USERPOINT);
  ATOOLS_FLAGS_TO_STR(TRACK);
  ATOOLS_FLAGS_TO_STR(AIRCRAFT_ONLINE);
  ATOOLS_FLAGS_TO_STR(LOGBOOK);
  ATOOLS_FLAGS_TO_STR(MARK_RANGE);
  ATOOLS_FLAGS_TO_STR(MARK_DISTANCE);
  ATOOLS_FLAGS_TO_STR(MARK_HOLDING);
  ATOOLS_FLAGS_TO_STR(MARK_PATTERNS);
  ATOOLS_FLAGS_TO_STR(MARK_MSA);
  ATOOLS_FLAGS_TO_STR(AIRPORT_HARD);
  ATOOLS_FLAGS_TO_STR(AIRPORT_SOFT);
  ATOOLS_FLAGS_TO_STR(AIRPORT_WATER);
  ATOOLS_FLAGS_TO_STR(AIRPORT_HELIPAD);
  ATOOLS_FLAGS_TO_STR(AIRPORT_EMPTY);
  ATOOLS_FLAGS_TO_STR(AIRPORT_UNLIGHTED);
  ATOOLS_FLAGS_TO_STR(AIRPORT_NO_PROCS);
  ATOOLS_FLAGS_TO_STR(AIRPORT_CLOSED);
  ATOOLS_FLAGS_TO_STR(AIRPORT_MILITARY);
  ATOOLS_FLAGS_TO_STR(PROCEDURE_POINT);
  ATOOLS_FLAGS_TO_STR(AIRCRAFT_TRAIL);
  ATOOLS_FLAGS_TO_STR(AIRPORT_ADDON_ZOOM);
  ATOOLS_FLAGS_TO_STR(AIRPORT_ADDON_ZOOM_FILTER);
  ATOOLS_FLAGS_TO_STR_END;
}

QDebug operator<<(QDebug out, const map::MapTypes& type)
{
  QDebugStateSaver saver(out);
  out.nospace().noquote() << mapTypeToString(type).join("|");
  return out;
}

QStringList mapDisplayTypeToString(const map::MapDisplayTypes& flags)
{
  ATOOLS_FLAGS_TO_STR_BEGIN(DISPLAY_TYPE_NONE);
  ATOOLS_FLAGS_TO_STR(AIRPORT_WEATHER);
  ATOOLS_FLAGS_TO_STR(MORA);
  ATOOLS_FLAGS_TO_STR(WIND_BARBS);
  ATOOLS_FLAGS_TO_STR(WIND_BARBS_ROUTE);
  ATOOLS_FLAGS_TO_STR(LOGBOOK_DIRECT);
  ATOOLS_FLAGS_TO_STR(LOGBOOK_ROUTE);
  ATOOLS_FLAGS_TO_STR(LOGBOOK_TRACK);
  ATOOLS_FLAGS_TO_STR(COMPASS_ROSE);
  ATOOLS_FLAGS_TO_STR(COMPASS_ROSE_ATTACH);
  ATOOLS_FLAGS_TO_STR(FLIGHTPLAN);
  ATOOLS_FLAGS_TO_STR(FLIGHTPLAN_TOC_TOD);
  ATOOLS_FLAGS_TO_STR_END;
}

QDebug operator<<(QDebug out, const map::MapDisplayType& type)
{
  out << map::MapDisplayTypes(type);
  return out;
}

QDebug operator<<(QDebug out, const map::MapDisplayTypes& type)
{
  QDebugStateSaver saver(out);
  out.nospace().noquote() << mapDisplayTypeToString(type).join("|");
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

QStringList mapAirspaceTypeToString(const map::MapAirspaceTypes& flags)
{
  ATOOLS_FLAGS_TO_STR_BEGIN(AIRSPACE_NONE);
  ATOOLS_FLAGS_TO_STR(CENTER);
  ATOOLS_FLAGS_TO_STR(CLASS_A);
  ATOOLS_FLAGS_TO_STR(CLASS_B);
  ATOOLS_FLAGS_TO_STR(CLASS_C);
  ATOOLS_FLAGS_TO_STR(CLASS_D);
  ATOOLS_FLAGS_TO_STR(CLASS_E);
  ATOOLS_FLAGS_TO_STR(CLASS_F);
  ATOOLS_FLAGS_TO_STR(CLASS_G);
  ATOOLS_FLAGS_TO_STR(TOWER);
  ATOOLS_FLAGS_TO_STR(CLEARANCE);
  ATOOLS_FLAGS_TO_STR(GROUND);
  ATOOLS_FLAGS_TO_STR(DEPARTURE);
  ATOOLS_FLAGS_TO_STR(APPROACH);
  ATOOLS_FLAGS_TO_STR(MOA);
  ATOOLS_FLAGS_TO_STR(RESTRICTED);
  ATOOLS_FLAGS_TO_STR(PROHIBITED);
  ATOOLS_FLAGS_TO_STR(WARNING);
  ATOOLS_FLAGS_TO_STR(ALERT);
  ATOOLS_FLAGS_TO_STR(DANGER);
  ATOOLS_FLAGS_TO_STR(NATIONAL_PARK);
  ATOOLS_FLAGS_TO_STR(MODEC);
  ATOOLS_FLAGS_TO_STR(RADAR);
  ATOOLS_FLAGS_TO_STR(TRAINING);
  ATOOLS_FLAGS_TO_STR(GLIDERPROHIBITED);
  ATOOLS_FLAGS_TO_STR(WAVEWINDOW);
  ATOOLS_FLAGS_TO_STR(CAUTION);
  ATOOLS_FLAGS_TO_STR(ONLINE_OBSERVER);
  ATOOLS_FLAGS_TO_STR(FIR);
  ATOOLS_FLAGS_TO_STR(UIR);
  ATOOLS_FLAGS_TO_STR(GCA);
  ATOOLS_FLAGS_TO_STR(MCTR);
  ATOOLS_FLAGS_TO_STR(TRSA);
  ATOOLS_FLAGS_TO_STR_END;
}

QDebug operator<<(QDebug out, const map::MapAirspaceType& type)
{
  out << map::MapAirspaceTypes(type);
  return out;
}

QDebug operator<<(QDebug out, const map::MapAirspaceTypes& type)
{
  QDebugStateSaver saver(out);
  out.nospace().noquote() << mapAirspaceTypeToString(type).join("|");
  return out;
}

QStringList mapAirspaceFlagsToString(const map::MapAirspaceFlags& flags)
{
  ATOOLS_FLAGS_TO_STR_BEGIN(AIRSPACE_ALTITUDE_FLAG_NONE);
  ATOOLS_FLAGS_TO_STR(AIRSPACE_ALTITUDE_ALL);
  ATOOLS_FLAGS_TO_STR(AIRSPACE_ALTITUDE_FLIGHTPLAN);
  ATOOLS_FLAGS_TO_STR(AIRSPACE_ALTITUDE_SET);
  ATOOLS_FLAGS_TO_STR(AIRSPACE_ALL_ON);
  ATOOLS_FLAGS_TO_STR(AIRSPACE_ALL_OFF);
  ATOOLS_FLAGS_TO_STR(AIRSPACE_NO_MULTIPLE_Z);
  ATOOLS_FLAGS_TO_STR_END;
}

QDebug operator<<(QDebug out, const map::MapAirspaceFlag& type)
{
  out << map::MapAirspaceFlags(type);
  return out;
}

QDebug operator<<(QDebug out, const map::MapAirspaceFlags& type)
{
  QDebugStateSaver saver(out);
  out.nospace().noquote() << mapAirspaceFlagsToString(type).join("|");
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
  return QStringLiteral();
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
  return QStringLiteral();
}

} // namespace map
