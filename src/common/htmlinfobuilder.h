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

#ifndef LITTLENAVMAP_MAPHTMLINFOBUILDER_H
#define LITTLENAVMAP_MAPHTMLINFOBUILDER_H

#include "util/htmlbuilder.h"

#include <QCoreApplication>
#include <QLocale>

class RouteMapObject;
class MapQuery;
class InfoQuery;
class WeatherReporter;
class RouteMapObjectList;

namespace maptypes {
struct MapAirport;

struct MapVor;

struct MapNdb;

struct MapWaypoint;

struct MapAirway;

struct MapMarker;

struct MapAirport;

struct MapParking;

struct MapHelipad;

struct MapUserpoint;

}

namespace atools {
namespace fs {
namespace sc {
class SimConnectData;
}
}
namespace sql {
class SqlRecord;
}
namespace util {
// class HtmlBuilder;
class MorseCode;
}
}

class HtmlInfoBuilder
{
  Q_DECLARE_TR_FUNCTIONS(MapHtmlInfoBuilder)

public:
  HtmlInfoBuilder(MapQuery *mapQuery, InfoQuery *infoDbQuery, bool formatInfo);

  virtual ~HtmlInfoBuilder();

  void airportText(const maptypes::MapAirport& airport, atools::util::HtmlBuilder& html,
                   const RouteMapObjectList *routeMapObjects,
                   WeatherReporter *weather, QColor background);
  void runwayText(const maptypes::MapAirport& airport, atools::util::HtmlBuilder& html, QColor background);
  void comText(const maptypes::MapAirport& airport, atools::util::HtmlBuilder& html, QColor background);
  void approachText(const maptypes::MapAirport& airport, atools::util::HtmlBuilder& html, QColor background);

  void vorText(const maptypes::MapVor& vor, atools::util::HtmlBuilder& html, QColor background);
  void ndbText(const maptypes::MapNdb& ndb, atools::util::HtmlBuilder& html, QColor background);
  void waypointText(const maptypes::MapWaypoint& waypoint, atools::util::HtmlBuilder& html, QColor background);

  void airwayText(const maptypes::MapAirway& airway, atools::util::HtmlBuilder& html);
  void markerText(const maptypes::MapMarker& m, atools::util::HtmlBuilder& html);
  void towerText(const maptypes::MapAirport& airport, atools::util::HtmlBuilder& html);
  void parkingText(const maptypes::MapParking& parking, atools::util::HtmlBuilder& html);
  void helipadText(const maptypes::MapHelipad& helipad, atools::util::HtmlBuilder& html);
  void userpointText(const maptypes::MapUserpoint& userpoint, atools::util::HtmlBuilder& html);

  void aircraftText(const atools::fs::sc::SimConnectData& data, atools::util::HtmlBuilder& html);

  void aircraftProgressText(const atools::fs::sc::SimConnectData& data, atools::util::HtmlBuilder& html,
                            const RouteMapObjectList& rmoList);

private:
  MapQuery *mapQuery;
  InfoQuery *infoQuery;
  atools::util::MorseCode *morse;
  bool info;
  QLocale locale;
  QString aircraftGroundEncodedIcon, aircraftEncodedIcon;

  void addScenery(const atools::sql::SqlRecord *rec, atools::util::HtmlBuilder& html);
  void addCoordinates(const atools::sql::SqlRecord *rec, atools::util::HtmlBuilder& html);
  void head(atools::util::HtmlBuilder& html, const QString& text);

  void title(atools::util::HtmlBuilder& html, const QString& text);

  void airportTitle(const maptypes::MapAirport& airport, atools::util::HtmlBuilder& html, QColor background);

  void rowForInt(atools::util::HtmlBuilder& html, const atools::sql::SqlRecord *rec, const QString& colName,
                 const QString& msg, const QString& val);

  void rowForBool(atools::util::HtmlBuilder& html, const atools::sql::SqlRecord *rec, const QString& colName,
                  const QString& msg, bool expected = false);

  void runwayEndText(atools::util::HtmlBuilder& html, const atools::sql::SqlRecord *rec, float hdgPrim,
                     int length);

  void rowForStr(atools::util::HtmlBuilder& html, const atools::sql::SqlRecord *rec, const QString& colName,
                 const QString& msg, const QString& val);

  void rowForFloat(atools::util::HtmlBuilder& html, const atools::sql::SqlRecord *rec, const QString& colName,
                   const QString& msg, const QString& val, int precision = 0);

  void rowForStrCap(atools::util::HtmlBuilder& html, const atools::sql::SqlRecord *rec,
                    const QString& colName,
                    const QString& msg,
                    const QString& val);

  void aircraftTitle(const atools::fs::sc::SimConnectData& data, atools::util::HtmlBuilder& html);

};

#endif // MAPHTMLINFOBUILDER
