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

#ifndef TYPES_H
#define TYPES_H

#include "geo/pos.h"

#include <QString>

namespace maptypes {

enum MapObjectType
{
  NONE = 0x00,
  AIRPORT = 0x01,
  VOR = 0x02,
  NDB = 0x04,
  ILS = 0x08,
  MARKER = 0x10,
  WAYPOINT = 0x20,
  ALL_NAV = VOR | NDB | ILS | WAYPOINT,
  ALL = 0xff
};

Q_DECLARE_FLAGS(ObjectTypes, MapObjectType);
Q_DECLARE_OPERATORS_FOR_FLAGS(maptypes::ObjectTypes);

struct MapObject
{
  int id = -1;
  ObjectTypes type;
  atools::geo::Pos position;
};

QString navTypeName(const QString& type);
QString navName(const QString& type);

QString surfaceName(const QString& surface);
QString parkingGateName(const QString& gate);
QString parkingRampName(const QString& ramp);

} // namespace types

#endif // TYPES_H
