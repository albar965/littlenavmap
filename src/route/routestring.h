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

#ifndef LITTLENAVMAP_ROUTESTRING_H
#define LITTLENAVMAP_ROUTESTRING_H

#include "common/maptypes.h"

#include <QStringList>
#include <QApplication>

namespace atools {
namespace fs {
namespace pln {
class Flightplan;
class FlightplanEntry;
}
}
}

class MapQuery;
class FlightplanEntryBuilder;
class RouteMapObjectList;

class RouteString
{
  Q_DECLARE_TR_FUNCTIONS(RouteString)

public:
  RouteString(FlightplanEntryBuilder *flightplanEntryBuilder = nullptr);
  virtual ~RouteString();

  /*
   * Create a route string like
   * LOWI DCT NORIN UT23 ALGOI UN871 BAMUR Z2 KUDES UN871 BERSU Z55 ROTOS
   * UZ669 MILPA UL612 MOU UM129 LMG UN460 CNA DCT LFCY
   */
  QString createStringForRoute(const RouteMapObjectList& route);

  /*
   * Create a route string in garming flight plan format (GFP):
   * FPN/RI:F:KTEB:F:LGA.J70.JFK.J79.HOFFI.J121.HTO.J150.OFTUR:F:KMVY
   */
  QString createGfpStringForRoute(const RouteMapObjectList& route);

  bool createRouteFromString(const QString& routeString, atools::fs::pln::Flightplan& flightplan);

  const QStringList& getErrors() const
  {
    return errors;
  }

  static QString cleanRouteString(const QString& string);

private:
  void appendError(const QString& err);
  bool addDeparture(atools::fs::pln::Flightplan& flightplan,
                    const QString& airportIdent);
  bool addDestination(atools::fs::pln::Flightplan& flightplan, const QString& airportIdent);
  void findIndexesInAirway(const QList<maptypes::MapAirwayWaypoint>& allAirwayWaypoints,
                           int curId, int lastId, int& startIndex, int& endIndex);
  void extractWaypoints(const QList<maptypes::MapAirwayWaypoint>& allAirwayWaypoints,
                        int startIndex, int endIndex,
                        QList<maptypes::MapWaypoint>& airwayWaypoints);
  void findBestNavaid(const QString& strItem, const atools::geo::Pos& lastPos, float maxDistance,
                      maptypes::MapSearchResult& result);
  QStringList createStringForRouteInternal(const RouteMapObjectList& route, bool gfpWaypoints);

  MapQuery *query = nullptr;
  FlightplanEntryBuilder *entryBuilder = nullptr;
  QStringList errors;

};

#endif // LITTLENAVMAP_ROUTESTRING_H
