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

#include "common/mapflags.h"

#include <QDataStream>
#include <QDebug>

namespace map {

QDebug operator<<(QDebug out, const map::MapObjectTypes& type)
{
  QDebugStateSaver saver(out);
  Q_UNUSED(saver);

  QStringList flags;
  if(type == NONE)
    flags.append("NONE");
  else
  {
    if(type & AIRPORT)
      flags.append("AIRPORT");
    if(type & AIRPORT_HARD)
      flags.append("AIRPORT_HARD");
    if(type & AIRPORT_SOFT)
      flags.append("AIRPORT_SOFT");
    if(type & AIRPORT_EMPTY)
      flags.append("AIRPORT_EMPTY");
    if(type & AIRPORT_ADDON)
      flags.append("AIRPORT_ADDON");
    if(type & VOR)
      flags.append("VOR");
    if(type & NDB)
      flags.append("NDB");
    if(type & ILS)
      flags.append("ILS");
    if(type & MARKER)
      flags.append("MARKER");
    if(type & WAYPOINT)
      flags.append("WAYPOINT");
    if(type & AIRWAY)
      flags.append("AIRWAY");
    if(type & AIRWAYV)
      flags.append("AIRWAYV");
    if(type & AIRWAYJ)
      flags.append("AIRWAYJ");
    if(type & FLIGHTPLAN)
      flags.append("ROUTE");
    if(type & AIRCRAFT)
      flags.append("AIRCRAFT");
    if(type & AIRCRAFT_AI)
      flags.append("AIRCRAFT_AI");
    if(type & AIRCRAFT_AI_SHIP)
      flags.append("AIRCRAFT_AI_BOAT");
    if(type & AIRCRAFT_TRACK)
      flags.append("AIRCRAFT_TRACK");
    if(type & USERPOINTROUTE)
      flags.append("USER");
    if(type & PARKING)
      flags.append("PARKING");
    if(type & RUNWAYEND)
      flags.append("RUNWAYEND");
    if(type & INVALID)
      flags.append("INVALID");
  }

  out.nospace().noquote() << flags.join("|");

  return out;
}

QDataStream& operator>>(QDataStream& dataStream, MapAirspaceFilter& obj)
{
  quint32 types, flags;
  dataStream >> types >> flags;
  obj.types = map::MapAirspaceTypes(types);
  obj.flags = map::MapAirspaceFlags(flags);
  return dataStream;
}

QDataStream& operator<<(QDataStream& dataStream, const MapAirspaceFilter& obj)
{
  dataStream << static_cast<quint32>(obj.types) << static_cast<quint32>(obj.flags);
  return dataStream;
}

} // namespace types
