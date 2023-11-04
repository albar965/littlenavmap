/*****************************************************************************
* Copyright 2015-2023 Alexander Barthel alex@littlenavmap.org
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

#include "grib/windtypes.h"
#include "common/mapflags.h"

#include <QCoreApplication>
#include <QLocale>
#include <QSize>

class RouteLeg;
class AirportQuery;
class InfoQuery;
class WeatherReporter;
class Route;
class MainWindow;
class MapPaintWidget;

class QFileInfo;

namespace atools {
namespace fs {
namespace weather {
class Metar;
class MetarParser;
}
}
namespace util {
class HtmlBuilder;
}
}

namespace map {

class AircraftTrailSegment;
struct MapAirport;
struct MapAirportMsa;
struct MapVor;
struct MapNdb;
struct MapWaypoint;
struct MapAirway;
struct MapAirspace;
struct MapIls;
struct MapMarker;
struct MapAirport;
struct MapParking;
struct MapHelipad;
struct MapStart;
struct MapUserpointRoute;
struct MapProcedurePoint;
struct MapProcedureRef;
struct MapUserpoint;
struct MapLogbookEntry;
struct MapBase;
struct MapResultIndex;
struct MapHolding;
struct PatternMarker;
struct RangeMarker;
struct HoldingMarker;
struct MsaMarker;
struct DistanceMarker;
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
 *
 * This is indepent of the main GUI main map widget.
 */
class HtmlInfoBuilder
{
  Q_DECLARE_TR_FUNCTIONS(MapHtmlInfoBuilder)

public:
  /*
   * @param infoParam Generate text for information displays. Implies verbose.
   * @param printParam Print text without links.
   * @param verboseParam Detailed tooltip text with info set to false
   * (i.e. generate alternating background color for tables)
   */
  HtmlInfoBuilder(QWidget *parent, MapPaintWidget *mapWidgetParam, bool infoParam, bool printParam = false,
                  bool verboseParam = true);

  virtual ~HtmlInfoBuilder();

  HtmlInfoBuilder(const HtmlInfoBuilder& other) = delete;
  HtmlInfoBuilder& operator=(const HtmlInfoBuilder& other) = delete;

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

  /* Adds text for preferred runways */
  void bestRunwaysText(const map::MapAirport& airport, atools::util::HtmlBuilder& html,
                       const atools::fs::weather::MetarParser& parsed, int max, bool details) const;

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

  /* Get information for navaids and airports near the currently displayed airport */
  void nearestText(const map::MapAirport& airport, atools::util::HtmlBuilder& html) const;

  /* Create HTML for decoded weather report for current airport */
  void weatherText(const map::WeatherContext& context, const map::MapAirport& airport,
                   atools::util::HtmlBuilder& html) const;

  /* Database nav features */
  void airportMsaText(const map::MapAirportMsa& msa, atools::util::HtmlBuilder& html) const;
  void holdingText(const map::MapHolding& holding, atools::util::HtmlBuilder& html) const;

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
   * Creates a HTML description for ILS.
   */
  void ilsTextInfo(const map::MapIls& ils, atools::util::HtmlBuilder& html) const;

  /*
   * Creates a HTML description for a waypoint including all attached airways.
   * @param waypoint
   * @param html Result containing HTML snippet
   * @param background Background color for icons
   */
  void waypointText(const map::MapWaypoint& waypoint, atools::util::HtmlBuilder& html) const;

  /* Description for user defined points */
  bool userpointText(map::MapUserpoint userpoint, atools::util::HtmlBuilder& html) const;
  void userpointTextInfo(const map::MapUserpoint& userpoint, atools::util::HtmlBuilder& html) const;

  /* Description for logbook entries */
  bool logEntryText(map::MapLogbookEntry logEntry, atools::util::HtmlBuilder& html) const;
  void logEntryTextInfo(const map::MapLogbookEntry& logEntry, atools::util::HtmlBuilder& html) const;

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
  void helipadText(const map::MapHelipad& helipad, atools::util::HtmlBuilder& html) const;

  /*
   * Creates a HTML description for a all upper layer winds at position
   * @param html Result containing HTML snippet
   */
  void windText(const atools::grib::WindPosList& windStack, atools::util::HtmlBuilder& html, float waypointAltitude, bool table) const;

  /*
   * Creates a HTML description for a user defined flight plan point.
   * @param userpoint
   * @param html Result containing HTML snippet
   */
  void userpointTextRoute(const map::MapUserpointRoute& userpoint, atools::util::HtmlBuilder& html) const;

  void procedurePointText(const map::MapProcedurePoint& procPoint, atools::util::HtmlBuilder& html, const Route *route) const;

  /*
   * Creates an overview HTML description for any AI or user aircraft in the simulator.
   * @param data
   * @param html Result containing HTML snippet
   */
  void aircraftText(const atools::fs::sc::SimConnectAircraft& aircraft, atools::util::HtmlBuilder& html, int num = -1, int total = -1);
  void aircraftTextWeightAndFuel(const atools::fs::sc::SimConnectUserAircraft& userAircraft, atools::util::HtmlBuilder& html) const;

  /*
   * Creates a HTML description for simulator user aircraft progress and ambient values.
   * @param html
   * @param html Result containing HTML snippet
   */
  void aircraftProgressText(const atools::fs::sc::SimConnectAircraft& data, atools::util::HtmlBuilder& html, const Route& route);

  /*
   * Create HTML for online aircraft also showing position.
   */
  void aircraftOnlineText(const atools::fs::sc::SimConnectAircraft& aircraft, const atools::sql::SqlRecord& onlineRec,
                          atools::util::HtmlBuilder& html);

  void aircraftTrailText(const map::AircraftTrailSegment& trailSegment, atools::util::HtmlBuilder& html, bool logbook) const;

  /* User features / marks */
  void msaMarkerText(const map::MsaMarker& msa, atools::util::HtmlBuilder& html) const;
  void holdingMarkerText(const map::HoldingMarker& holding, atools::util::HtmlBuilder& html) const;
  void patternMarkerText(const map::PatternMarker& pattern, atools::util::HtmlBuilder& html) const;
  void rangeMarkerText(const map::RangeMarker& marker, atools::util::HtmlBuilder& html) const;
  void distanceMarkerText(const map::DistanceMarker& marker, atools::util::HtmlBuilder& html) const;

  void setSymbolSize(const QSize& value)
  {
    symbolSize = value;
  }

  void setSymbolSizeVehicle(const QSize& value)
  {
    symbolSizeVehicle = value;
  }

  void setSymbolSizeTitle(const QSize& value)
  {
    symbolSizeTitle = value;
  }

  /* Add bearing and distance to user and last flight plan leg in a table if pos is valid */
  void bearingAndDistanceTexts(const atools::geo::Pos& pos, float magvar, atools::util::HtmlBuilder& html, bool bearing, bool distance);

private:
  void head(atools::util::HtmlBuilder& html, const QString& text) const;

  bool nearestMapObjectsText(const map::MapAirport& airport, atools::util::HtmlBuilder& html,
                             const map::MapResultIndex *nearestNav, const QString& header, bool frequencyCol,
                             bool airportCol,
                             int maxRows) const;
  void nearestMapObjectsTextRow(const map::MapAirport& airport, atools::util::HtmlBuilder& html, const QString& type,
                                const QString& displayIdent, const QString& name, const QString& freq,
                                const map::MapBase *base,
                                float magVar, bool frequencyCol, bool airportCol) const;

  /* Add scenery entries and links into table */
  enum SceneryType
  {
    DATASOURCE_COM, DATASOURCE_HOLD, DATASOURCE_MSA, DATASOURCE_NAV
  };

  void addScenery(const atools::sql::SqlRecord *rec, atools::util::HtmlBuilder& html, SceneryType type) const;

  void addAirportSceneryAndLinks(const map::MapAirport& airport, atools::util::HtmlBuilder& html) const;
  void addAirportFolder(const map::MapAirport& airport, atools::util::HtmlBuilder& html) const;

  /* Add coordinates into table */
  void addCoordinates(const atools::sql::SqlRecord *rec, atools::util::HtmlBuilder& html) const;
  void addCoordinates(const atools::geo::Pos& pos, atools::util::HtmlBuilder& html) const;

  /* Bearing to simulator aircraft if connected. Returns true if text was addded. */
  bool bearingToUserText(const atools::geo::Pos& pos, float magVar, atools::util::HtmlBuilder& html) const;

  /* Distance to last flight plan waypoint. Returns true if text was addded. */
  bool distanceToRouteText(const atools::geo::Pos& pos, atools::util::HtmlBuilder& html) const;

  void navaidTitle(atools::util::HtmlBuilder& html, const QString& text, bool noEntities = false) const;

  void airportTitle(const map::MapAirport& airport, atools::util::HtmlBuilder& html, int rating, bool procedures) const;

  void airportMsaTextInternal(const map::MapAirportMsa& msa, atools::util::HtmlBuilder& html, bool user) const;
  void holdingTextInternal(const map::MapHolding& holding, atools::util::HtmlBuilder& html, bool user) const;

  void rowForInt(atools::util::HtmlBuilder& html, const atools::sql::SqlRecord *rec, const QString& colName,
                 const QString& msg, const QString& val) const;

  void rowForBool(atools::util::HtmlBuilder& html, const atools::sql::SqlRecord *rec, const QString& colName,
                  const QString& msg, bool expected = false) const;

  void runwayEndText(atools::util::HtmlBuilder& html, const map::MapAirport& airport, const atools::sql::SqlRecord *rec,
                     float hdgPrimTrue, float length, bool secondary) const;

  void rowForStr(atools::util::HtmlBuilder& html, const atools::sql::SqlRecord *rec, const QString& colName,
                 const QString& msg, const QString& val) const;

  void rowForFloat(atools::util::HtmlBuilder& html, const atools::sql::SqlRecord *rec, const QString& colName,
                   const QString& msg, const QString& val, int precision = 0) const;

  void rowForStrCap(atools::util::HtmlBuilder& html, const atools::sql::SqlRecord *rec,
                    const QString& colName, const QString& msg, const QString& val) const;

  void aircraftTitle(const atools::fs::sc::SimConnectAircraft& aircraft,
                     atools::util::HtmlBuilder& html);

  void dateTimeAndFlown(const atools::fs::sc::SimConnectUserAircraft *userAircraft, atools::util::HtmlBuilder& html) const;
  void addMetarLines(atools::util::HtmlBuilder& html, const map::WeatherContext& weatherContext, map::MapWeatherSource src,
                     const map::MapAirport& airport) const;
  void addMetarLine(atools::util::HtmlBuilder& html, const QString& header, const map::MapAirport& airport,
                    const QString& metar, const QString& station, const QDateTime& timestamp, bool fsMetar, bool mapDisplay) const;

  void decodedMetar(atools::util::HtmlBuilder& html, const map::MapAirport& airport,
                    const map::MapAirport& reportAirport, const atools::fs::weather::Metar& metar,
                    bool isInterpolated, bool isFsxP3d, bool mapDisplay) const;
  void decodedMetars(atools::util::HtmlBuilder& html, const atools::fs::weather::MetarResult& metar,
                     const map::MapAirport& airport, const QString& name, bool mapDisplay) const;

  void addRadionavFixType(atools::util::HtmlBuilder& html, const atools::sql::SqlRecord& recApp) const;

  QString filepathTextShow(const QString& filepath, const QString& prefix = QString(), bool canonical = false) const;
  QString filepathTextOpen(const QFileInfo& filepath, bool showPath) const;

  void airportRow(const map::MapAirport& ap, atools::util::HtmlBuilder& html) const;

  void addFlightRulesSuffix(atools::util::HtmlBuilder& html, const atools::fs::weather::Metar& metar,
                            bool mapDisplay) const;

  /* Insert airport link using ident and/or name */
  QString airportLink(const atools::util::HtmlBuilder& html, const QString& ident,
                      const QString& name, const atools::geo::Pos& pos) const;

  /* Adds text for remarks */
  void descriptionText(const QString& descriptionText, atools::util::HtmlBuilder& html) const;

  /* Add morse code row2line */
  void addMorse(atools::util::HtmlBuilder& html, const QString& name, const QString& code) const;

  void ilsTextProcInfo(const map::MapIls& ils, atools::util::HtmlBuilder& html) const;
  void ilsTextRunwayInfo(const map::MapIls& ils, atools::util::HtmlBuilder& html) const;
  void ilsTextInternal(const map::MapIls& ils, atools::util::HtmlBuilder& html, bool procInfo, bool runwayInfo, bool infoOrTooltip) const;

  /* Add wind text for flight plan waypoints */
  void routeWindText(atools::util::HtmlBuilder& html, const Route& route, int index) const;

  QString identRegionText(const QString& ident, const QString& region) const;

  /* Add remarks from a routeleg at the given index to a table */
  void flightplanWaypointRemarks(atools::util::HtmlBuilder& html, int index) const;

  /* Join values and header with default values */
  QString strJoinVal(const QStringList& list) const;
  QString strJoinHdr(const QStringList& list) const;

  QString highlightText(const QString& text) const;
  void routeInfoText(atools::util::HtmlBuilder& html, int routeIndex, bool recommended) const;

  void waypointAirwayText(const map::MapWaypoint& waypoint, atools::util::HtmlBuilder& html) const;

  /* Airport, navaid and userpoint icon size */
  QSize symbolSize = QSize(18, 18);

  /* Airport, navaid and userpoint icon title size */
  QSize symbolSizeTitle = QSize(24, 24);

  /* Aircraft size */
  QSize symbolSizeVehicle = QSize(28, 28);

  QWidget *parentWidget = nullptr;
  MapPaintWidget *mapWidget = nullptr;

  AirportQuery *airportQuerySim, *airportQueryNav;
  InfoQuery *infoQuery;
  atools::fs::util::MorseCode *morse;

  bool info, /* Shown in information panel - otherwise tooltip */
       print, /* Printing */
       verbose /* Verbose tooltip option in settings set */;

  QLocale locale;
};

#endif // MAPHTMLINFOBUILDER
