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

QStringList mapTypeToString(const map::MapTypes& type)
{
  QStringList flags;
  if(type == NONE)
    flags.append(QStringLiteral("NONE"));
  else
  {
    if(type.testFlag(AIRPORT))
      flags.append(QStringLiteral("AIRPORT"));
    if(type.testFlag(RUNWAY))
      flags.append(QStringLiteral("RUNWAY"));
    if(type.testFlag(VOR))
      flags.append(QStringLiteral("VOR"));
    if(type.testFlag(NDB))
      flags.append(QStringLiteral("NDB"));
    if(type.testFlag(ILS))
      flags.append(QStringLiteral("ILS"));
    if(type.testFlag(MARKER))
      flags.append(QStringLiteral("MARKER"));
    if(type.testFlag(WAYPOINT))
      flags.append(QStringLiteral("WAYPOINT"));
    if(type.testFlag(AIRWAY))
      flags.append(QStringLiteral("AIRWAY"));
    if(type.testFlag(AIRWAYV))
      flags.append(QStringLiteral("AIRWAYV"));
    if(type.testFlag(AIRWAYJ))
      flags.append(QStringLiteral("AIRWAYJ"));
    if(type.testFlag(USER_FEATURE))
      flags.append(QStringLiteral("USER_FEATURE"));
    if(type.testFlag(AIRCRAFT))
      flags.append(QStringLiteral("AIRCRAFT"));
    if(type.testFlag(AIRCRAFT_AI))
      flags.append(QStringLiteral("AIRCRAFT_AI"));
    if(type.testFlag(AIRCRAFT_AI_SHIP))
      flags.append(QStringLiteral("AIRCRAFT_AI_SHIP"));
    if(type.testFlag(AIRPORT_MSA))
      flags.append(QStringLiteral("AIRPORT_MSA"));
    if(type.testFlag(USERPOINTROUTE))
      flags.append(QStringLiteral("USERPOINTROUTE"));
    if(type.testFlag(PARKING))
      flags.append(QStringLiteral("PARKING"));
    if(type.testFlag(START))
      flags.append(QStringLiteral("START"));
    if(type.testFlag(RUNWAYEND))
      flags.append(QStringLiteral("RUNWAYEND"));
    if(type.testFlag(INVALID))
      flags.append(QStringLiteral("INVALID"));
    if(type.testFlag(MISSED_APPROACH))
      flags.append(QStringLiteral("MISSED_APPROACH"));
    if(type.testFlag(PROCEDURE))
      flags.append(QStringLiteral("PROCEDURE"));
    if(type.testFlag(AIRSPACE))
      flags.append(QStringLiteral("AIRSPACE"));
    if(type.testFlag(HELIPAD))
      flags.append(QStringLiteral("HELIPAD"));
    if(type.testFlag(HOLDING))
      flags.append(QStringLiteral("HOLDING"));
    if(type.testFlag(USERPOINT))
      flags.append(QStringLiteral("USERPOINT"));
    if(type.testFlag(TRACK))
      flags.append(QStringLiteral("TRACK"));
    if(type.testFlag(AIRCRAFT_ONLINE))
      flags.append(QStringLiteral("AIRCRAFT_ONLINE"));
    if(type.testFlag(LOGBOOK))
      flags.append(QStringLiteral("LOGBOOK"));
    if(type.testFlag(MARK_RANGE))
      flags.append(QStringLiteral("MARK_RANGE"));
    if(type.testFlag(MARK_DISTANCE))
      flags.append(QStringLiteral("MARK_DISTANCE"));
    if(type.testFlag(MARK_HOLDING))
      flags.append(QStringLiteral("MARK_HOLDING"));
    if(type.testFlag(MARK_PATTERNS))
      flags.append(QStringLiteral("MARK_PATTERNS"));
    if(type.testFlag(MARK_MSA))
      flags.append(QStringLiteral("MARK_MSA"));
    if(type.testFlag(AIRPORT_HARD))
      flags.append(QStringLiteral("AIRPORT_HARD"));
    if(type.testFlag(AIRPORT_SOFT))
      flags.append(QStringLiteral("AIRPORT_SOFT"));
    if(type.testFlag(AIRPORT_WATER))
      flags.append(QStringLiteral("AIRPORT_WATER"));
    if(type.testFlag(AIRPORT_HELIPAD))
      flags.append(QStringLiteral("AIRPORT_HELIPAD"));
    if(type.testFlag(AIRPORT_EMPTY))
      flags.append(QStringLiteral("AIRPORT_EMPTY"));
    if(type.testFlag(AIRPORT_UNLIGHTED))
      flags.append(QStringLiteral("AIRPORT_UNLIGHTED"));
    if(type.testFlag(AIRPORT_NO_PROCS))
      flags.append(QStringLiteral("AIRPORT_NO_PROCS"));
    if(type.testFlag(AIRPORT_CLOSED))
      flags.append(QStringLiteral("AIRPORT_CLOSED"));
    if(type.testFlag(AIRPORT_MILITARY))
      flags.append(QStringLiteral("AIRPORT_MILITARY"));
    if(type.testFlag(PROCEDURE_POINT))
      flags.append(QStringLiteral("PROCEDURE_POINT"));
    if(type.testFlag(AIRCRAFT_TRAIL))
      flags.append(QStringLiteral("AIRCRAFT_TRAIL"));
    if(type.testFlag(AIRPORT_ADDON_ZOOM))
      flags.append(QStringLiteral("AIRPORT_ADDON_ZOOM"));
    if(type.testFlag(AIRPORT_ADDON_ZOOM_FILTER))
      flags.append(QStringLiteral("AIRPORT_ADDON_ZOOM_FILTER"));
  }
  return flags;
}

QDebug operator<<(QDebug out, const map::MapTypes& type)
{
  QDebugStateSaver saver(out);

  out.nospace().noquote() << mapTypeToString(type).join("|");

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
