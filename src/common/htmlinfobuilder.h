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
class MainWindow;

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
class SimConnectUserAircraft;
class SimConnectAircraft;
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

/*
 * Builds HTML snippets (no <html> and no <body> tags) for QTextEdits or tooltips.
 */
class HtmlInfoBuilder
{
  Q_DECLARE_TR_FUNCTIONS(MapHtmlInfoBuilder)

public:
  /*
   * @param mapDbQuery Initialized database query object
   * @param infoDbQuery Initialized database query object
   * @param formatInfo true if this should generate HTML for QTextEdits or QWebBrowser
   * (i.e. generate alternating background color for tables)
   */
  HtmlInfoBuilder(MainWindow *parentWindow, bool formatInfo, bool formatPrint = false);

  virtual ~HtmlInfoBuilder();

  /*
   * Creates a HTML description for an airport.
   * @param airport
   * @param html Result containing HTML snippet
   * @param routeMapObjects
   * @param weather
   * @param background Background color for icons
   */
  void airportText(const maptypes::MapAirport& airport, atools::util::HtmlBuilder& html,
                   const RouteMapObjectList *routeMapObjects,
                   WeatherReporter *weather, QColor background) const;

  /*
   * Creates a HTML description for all runways of an airport.
   * @param airport
   * @param html Result containing HTML snippet
   * @param background Background color for icons
   */
  void runwayText(const maptypes::MapAirport& airport, atools::util::HtmlBuilder& html,
                  QColor background, bool details = true, bool soft = true) const;

  /*
   * Creates a HTML description for all COM frequencies of an airport.
   * @param airport
   * @param html Result containing HTML snippet
   * @param background Background color for icons
   */
  void comText(const maptypes::MapAirport& airport, atools::util::HtmlBuilder& html, QColor background) const;

  /*
   * Creates a HTML description for all approaches of an airport.
   * @param airport
   * @param html Result containing HTML snippet
   * @param background Background color for icons
   */
  void approachText(const maptypes::MapAirport& airport, atools::util::HtmlBuilder& html,
                    QColor background) const;

  /*
   * Creates a HTML description for a VOR station.
   * @param vor
   * @param html Result containing HTML snippet
   * @param background Background color for icons
   */
  void vorText(const maptypes::MapVor& vor, atools::util::HtmlBuilder& html, QColor background) const;

  /*
   * Creates a HTML description for a NDB station.
   * @param ndb
   * @param html Result containing HTML snippet
   * @param background Background color for icons
   */
  void ndbText(const maptypes::MapNdb& ndb, atools::util::HtmlBuilder& html, QColor background) const;

  /*
   * Creates a HTML description for a waypoint including all attached airways.
   * @param waypoint
   * @param html Result containing HTML snippet
   * @param background Background color for icons
   */
  void waypointText(const maptypes::MapWaypoint& waypoint, atools::util::HtmlBuilder& html,
                    QColor background) const;

  /*
   * Creates a HTML description of an airway. For info this includes all waypoints.
   * @param airway
   * @param html Result containing HTML snippet
   */
  void airwayText(const maptypes::MapAirway& airway, atools::util::HtmlBuilder& html) const;

  /*
   * Creates a HTML description for a marker.
   * @param marker
   * @param html Result containing HTML snippet
   */
  void markerText(const maptypes::MapMarker& marker, atools::util::HtmlBuilder& html) const;

  /*
   * Creates a HTML description for an airport's tower including frequency if available.
   * @param airport
   * @param html Result containing HTML snippet
   */
  void towerText(const maptypes::MapAirport& airport, atools::util::HtmlBuilder& html) const;

  /*
   * Creates a HTML description for a parking spot (ramp GA, gate, etc.).
   * @param parking
   * @param html Result containing HTML snippet
   */
  void parkingText(const maptypes::MapParking& parking, atools::util::HtmlBuilder& html) const;

  /*
   * Creates a HTML description for a helipad.
   * @param helipad
   * @param html Result containing HTML snippet
   */
  void helipadText(const maptypes::MapHelipad& helipad, atools::util::HtmlBuilder& html) const;

  /*
   * Creates a HTML description for a user defined flight plan point.
   * @param userpoint
   * @param html Result containing HTML snippet
   */
  void userpointText(const maptypes::MapUserpoint& userpoint, atools::util::HtmlBuilder& html) const;

  /*
   * Creates an overview HTML description for the user aircraft in the simulator.
   * @param data
   * @param html Result containing HTML snippet
   */
  void aircraftText(const atools::fs::sc::SimConnectAircraft& userAircraft,
                    atools::util::HtmlBuilder& html, int num = -1, int total = -1) const;
  void aircraftTextWeightAndFuel(const atools::fs::sc::SimConnectUserAircraft& userAircraft,
                                 atools::util::HtmlBuilder& html) const;

  /*
   * Creates a HTML description for simulator user aircraft progress and ambient values.
   * @param html
   * @param html Result containing HTML snippet
   */
  void aircraftProgressText(const atools::fs::sc::SimConnectAircraft& data,
                            atools::util::HtmlBuilder& html,
                            const RouteMapObjectList& rmoList) const;

private:
  void addScenery(const atools::sql::SqlRecord *rec, atools::util::HtmlBuilder& html) const;
  void addCoordinates(const atools::sql::SqlRecord *rec, atools::util::HtmlBuilder& html) const;
  void head(atools::util::HtmlBuilder& html, const QString& text) const;

  void title(atools::util::HtmlBuilder& html, const QString& text) const;

  void airportTitle(const maptypes::MapAirport& airport, atools::util::HtmlBuilder& html,
                    QColor background) const;

  void rowForInt(atools::util::HtmlBuilder& html, const atools::sql::SqlRecord *rec, const QString& colName,
                 const QString& msg, const QString& val) const;

  void rowForBool(atools::util::HtmlBuilder& html, const atools::sql::SqlRecord *rec, const QString& colName,
                  const QString& msg, bool expected = false) const;

  void runwayEndText(atools::util::HtmlBuilder& html, const atools::sql::SqlRecord *rec, float hdgPrim,
                     float length) const;

  void rowForStr(atools::util::HtmlBuilder& html, const atools::sql::SqlRecord *rec, const QString& colName,
                 const QString& msg, const QString& val) const;

  void rowForFloat(atools::util::HtmlBuilder& html, const atools::sql::SqlRecord *rec, const QString& colName,
                   const QString& msg, const QString& val, int precision = 0) const;

  void rowForStrCap(atools::util::HtmlBuilder& html, const atools::sql::SqlRecord *rec,
                    const QString& colName, const QString& msg, const QString& val) const;

  void aircraftTitle(const atools::fs::sc::SimConnectAircraft& aircraft,
                     atools::util::HtmlBuilder& html) const;

  void timeAndDate(const atools::fs::sc::SimConnectUserAircraft *userAircaft,
                   atools::util::HtmlBuilder& html) const;

  MainWindow *mainWindow = nullptr;
  MapQuery *mapQuery;
  InfoQuery *infoQuery;
  atools::util::MorseCode *morse;
  bool info, print;
  QLocale locale;
  QString aircraftGroundEncodedIcon, aircraftEncodedIcon, aircraftAiGroundEncodedIcon, aircraftAiEncodedIcon;

};

#endif // MAPHTMLINFOBUILDER
