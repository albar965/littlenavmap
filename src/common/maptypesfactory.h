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

#ifndef MAPTYPESFACTORY_H
#define MAPTYPESFACTORY_H

#include "maptypes.h"

class QSqlRecord;

class MapTypesFactory
{
public:
  MapTypesFactory();
  ~MapTypesFactory();

  void fillAirport(const QSqlRecord& record, maptypes::MapAirport& ap, bool complete);
  void fillVor(const QSqlRecord& record, maptypes::MapVor& vor);
  void fillNdb(const QSqlRecord& record, maptypes::MapNdb& ndb);
  void fillWaypoint(const QSqlRecord& record, maptypes::MapWaypoint& wp);
  void fillAirway(const QSqlRecord& record, maptypes::MapAirway& airway);
  void fillMarker(const QSqlRecord& record, maptypes::MapMarker& marker);
  void fillIls(const QSqlRecord& record, maptypes::MapIls& ils);
  void fillParking(const QSqlRecord& record, maptypes::MapParking& parking);
  maptypes::MapAirportFlags flag(const QSqlRecord& record, const QString& field,
                                 maptypes::MapAirportFlags flag);
  maptypes::MapAirportFlags getFlags(const QSqlRecord& record);

};

#endif // MAPTYPESFACTORY_H
