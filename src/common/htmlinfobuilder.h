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

#ifndef LITTLENAVMAP_MAPHTMLINFOBUILDER_H
#define LITTLENAVMAP_MAPHTMLINFOBUILDER_H

#include "util/htmlbuilder.h"
#include "fs/weather/metar.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QLocale>

class RouteLeg;
class MapQuery;
class AirportQuery;
class InfoQuery;
class WeatherReporter;
class Route;
class MainWindow;

namespace map {
struct MapAirport;

struct MapVor;

struct MapNdb;

struct MapWaypoint;

struct MapAirway;

struct MapAirspace;

struct MapMarker;

struct MapAirport;

struct MapParking;

struct MapHelipad;

struct MapStart;

struct MapUserpointRoute;

struct MapProcedurePoint;

struct MapProcedureRef;

struct MapUserpoint;

}

namespace atools {

namespace geo {
class Pos;
}

namespace fs {
namespace util {
class MorseCode;
}

namespace weather {
struct MetarResult;

}

namespace sc {
class SimConnectUserAircraft;
class SimConnectAircraft;
}
}
namespace sql {
class SqlRecord;
}
}

namespace map {
struct WeatherContext;

}

namespace proc {
struct MapProcedurePoint;

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
   * @param route
   * @param weather
   * @param background Background color for icons
   */
  void airportText(const map::MapAirport& airport, const map::WeatherContext& weatherContext,
                   atools::util::HtmlBuilder& html, const Route *route) const;

  /*
   * Creates a HTML description for all runways of an airport.
   * @param airport
   * @param html Result containing HTML snippet
   * @param background Background color for icons
   */
  void runwayText(const map::MapAirport& airport, atools::util::HtmlBuilder& html,
                  bool details = true, bool soft = true) const;

  /*
   * Creates a HTML description for all COM frequencies of an airport.
   * @param airport
   * @param html Result containing HTML snippet
   * @param background Background color for icons
   */
  void comText(const map::MapAirport& airport, atools::util::HtmlBuilder& html) const;

  /*
   * Creates a HTML description for all approaches of an airport.
   * @param airport
   * @param html Result containing HTML snippet
   * @param background Background color for icons
   */
  void procedureText(const map::MapAirport& airport, atools::util::HtmlBuilder& html) const;

  void weatherText(const map::WeatherContext& context, const map::MapAirport& airport,
                   atools::util::HtmlBuilder& html) const;

  /*
   * Creates a HTML description for a VOR station.
   * @param vor
   * @param html Result containing HTML snippet
   * @param background Background color for icons
   */
  void vorText(const map::MapVor& vor, atools::util::HtmlBuilder& html) const;

  /*
   * Creates a HTML description for a NDB station.
   * @param ndb
   * @param html Result containing HTML snippet
   * @param background Background color for icons
   */
  void ndbText(const map::MapNdb& ndb, atools::util::HtmlBuilder& html) const;

  /*
   * Creates a HTML description for a waypoint including all attached airways.
   * @param waypoint
   * @param html Result containing HTML snippet
   * @param background Background color for icons
   */
  void waypointText(const map::MapWaypoint& waypoint, atools::util::HtmlBuilder& html) const;

  /* Description for user defined points */
  void userpointText(const map::MapUserpoint& userpoint, atools::util::HtmlBuilder& html) const;

  /*
   * Creates a HTML description of an airway. For info this includes all waypoints.
   * @param airway
   * @param html Result containing HTML snippet
   */
  void airwayText(const map::MapAirway& airway, atools::util::HtmlBuilder& html) const;
  void airspaceText(const map::MapAirspace& airspace, const atools::sql::SqlRecord& onlineRec,
                    atools::util::HtmlBuilder& html) const;

  /*
   * Creates a HTML description for a marker.
   * @param marker
   * @param html Result containing HTML snippet
   */
  void markerText(const map::MapMarker& marker, atools::util::HtmlBuilder& html) const;

  /*
   * Creates a HTML description for an airport's tower including frequency if available.
   * @param airport
   * @param html Result containing HTML snippet
   */
  void towerText(const map::MapAirport& airport, atools::util::HtmlBuilder& html) const;

  /*
   * Creates a HTML description for a parking spot (ramp GA, gate, etc.).
   * @param parking
   * @param html Result containing HTML snippet
   */
  void parkingText(const map::MapParking& parking, atools::util::HtmlBuilder& html) const;

  /*
   * Creates a HTML description for a helipad. Only called for tooltip
   * @param helipad
   * @param html Result containing HTML snippet
   */
  void helipadText(const map::MapHelipad& helipad,
                   atools::util::HtmlBuilder& html) const;

  /*
   * Creates a HTML description for a user defined flight plan point.
   * @param userpoint
   * @param html Result containing HTML snippet
   */
  void userpointTextRoute(const map::MapUserpointRoute& userpoint, atools::util::HtmlBuilder& html) const;

  void procedurePointText(const proc::MapProcedurePoint& ap, atools::util::HtmlBuilder& html) const;

  /*
   * Creates an overview HTML description for any AI or user aircraft in the simulator.
   * @param data
   * @param html Result containing HTML snippet
   */
  void aircraftText(const atools::fs::sc::SimConnectAircraft& aircraft,
                    atools::util::HtmlBuilder& html, int num = -1, int total = -1);
  void aircraftTextWeightAndFuel(const atools::fs::sc::SimConnectUserAircraft& userAircraft,
                                 atools::util::HtmlBuilder& html) const;

  /*
   * Creates a HTML description for simulator user aircraft progress and ambient values.
   * @param html
   * @param html Result containing HTML snippet
   */
  void aircraftProgressText(const atools::fs::sc::SimConnectAircraft& data,
                            atools::util::HtmlBuilder& html,
                            const Route& route);

  /*
   * Create HTML for online aircraft also showing position.
   */
  void aircraftOnlineText(const atools::fs::sc::SimConnectAircraft& aircraft, const atools::sql::SqlRecord& onlineRec,
                          atools::util::HtmlBuilder& html);

private:
  void addScenery(const atools::sql::SqlRecord *rec, atools::util::HtmlBuilder& html) const;
  void addAirportScenery(const map::MapAirport& airport, atools::util::HtmlBuilder& html) const;
  void addCoordinates(const atools::sql::SqlRecord *rec, atools::util::HtmlBuilder& html) const;
  void addCoordinates(const atools::geo::Pos& pos, atools::util::HtmlBuilder& html) const;
  void head(atools::util::HtmlBuilder& html, const QString& text) const;

  void navaidTitle(atools::util::HtmlBuilder& html, const QString& text) const;

  void airportTitle(const map::MapAirport& airport, atools::util::HtmlBuilder& html, int rating) const;

  void rowForInt(atools::util::HtmlBuilder& html, const atools::sql::SqlRecord *rec, const QString& colName,
                 const QString& msg, const QString& val) const;

  void rowForBool(atools::util::HtmlBuilder& html, const atools::sql::SqlRecord *rec, const QString& colName,
                  const QString& msg, bool expected = false) const;

  void runwayEndText(atools::util::HtmlBuilder& html, const map::MapAirport& airport, const atools::sql::SqlRecord *rec,
                     float hdgPrim,
                     float length) const;

  void rowForStr(atools::util::HtmlBuilder& html, const atools::sql::SqlRecord *rec, const QString& colName,
                 const QString& msg, const QString& val) const;

  void rowForFloat(atools::util::HtmlBuilder& html, const atools::sql::SqlRecord *rec, const QString& colName,
                   const QString& msg, const QString& val, int precision = 0) const;

  void rowForStrCap(atools::util::HtmlBuilder& html, const atools::sql::SqlRecord *rec,
                    const QString& colName, const QString& msg, const QString& val) const;

  void aircraftTitle(const atools::fs::sc::SimConnectAircraft& aircraft,
                     atools::util::HtmlBuilder& html);

  void dateAndTime(const atools::fs::sc::SimConnectUserAircraft *userAircraft,
                   atools::util::HtmlBuilder& html) const;
  void addMetarLine(atools::util::HtmlBuilder& html, const QString& heading, const QString& metar,
                    const QString& station = QString(),
                    const QDateTime& timestamp = QDateTime(), bool fsMetar = false) const;

  void decodedMetar(atools::util::HtmlBuilder& html, const map::MapAirport& airport,
                    const map::MapAirport& reportAirport, const atools::fs::weather::Metar& metar,
                    bool isInterpolated, bool isFsxP3d) const;
  void decodedMetars(atools::util::HtmlBuilder& html, const atools::fs::weather::MetarResult& metar,
                     const map::MapAirport& airport, const QString& name) const;

  bool buildWeatherContext(map::WeatherContext& lastContext, map::WeatherContext& newContext,
                           const map::MapAirport& airport);
  void addRadionavFixType(atools::util::HtmlBuilder& html, const atools::sql::SqlRecord& recApp) const;
  void ilsText(const atools::sql::SqlRecord *ilsRec, atools::util::HtmlBuilder& html, bool approach) const;
  QString filepathText(const QString& filepath) const;
  QString airplaneType(const atools::fs::sc::SimConnectAircraft& aircraft) const;

  /* Escape entities and replace linefeeds with <br/> */
  QString adjustText(const QString& text) const;

  MainWindow *mainWindow = nullptr;
  MapQuery *mapQuery;
  AirportQuery *airportQuerySim, *airportQueryNav;
  InfoQuery *infoQuery;
  atools::fs::util::MorseCode *morse;
  bool info, print;
  QLocale locale;

};

#endif // MAPHTMLINFOBUILDER
