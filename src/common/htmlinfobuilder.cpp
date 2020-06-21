/*****************************************************************************
* Copyright 2015-2020 Alexander Barthel alex@littlenavmap.org
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

#include "common/htmlinfobuilder.h"

#include "airspace/airspacecontroller.h"
#include "atools.h"
#include "common/airportfiles.h"
#include "common/formatter.h"
#include "common/fueltool.h"
#include "common/maptools.h"
#include "common/maptypes.h"
#include "common/symbolpainter.h"
#include "common/unit.h"
#include "common/vehicleicons.h"
#include "fs/bgl/ap/rw/runway.h"
#include "fs/online/onlinetypes.h"
#include "fs/sc/simconnectdata.h"
#include "fs/userdata/userdatamanager.h"
#include "fs/util/fsutil.h"
#include "fs/util/morsecode.h"
#include "fs/util/tacanfrequencies.h"
#include "fs/weather/metar.h"
#include "fs/weather/metarparser.h"
#include "geo/calculations.h"
#include "grib/windquery.h"
#include "logbook/logdatacontroller.h"
#include "navapp.h"
#include "options/optiondata.h"
#include "perf/aircraftperfcontroller.h"
#include "query/airportquery.h"
#include "query/airwaytrackquery.h"
#include "query/infoquery.h"
#include "query/mapquery.h"
#include "query/waypointtrackquery.h"
#include "route/route.h"
#include "route/routealtitudeleg.h"
#include "sql/sqlrecord.h"
#include "userdata/userdatacontroller.h"
#include "userdata/userdataicons.h"
#include "util/htmlbuilder.h"
#include "weather/windreporter.h"
#include "route/routealtitude.h"

#include <QSize>
#include <QFileInfo>

using namespace map;
using atools::sql::SqlRecord;
using atools::sql::SqlRecordVector;
using atools::geo::normalizeCourse;
using atools::geo::opposedCourseDeg;
using atools::capString;
using atools::fs::util::MorseCode;
using atools::fs::util::frequencyForTacanChannel;
using atools::util::HtmlBuilder;
using atools::util::html::Flags;
using atools::fs::sc::SimConnectAircraft;
using atools::fs::sc::SimConnectUserAircraft;
using atools::fs::weather::Metar;
using atools::geo::Pos;
using atools::fs::util::roundComFrequency;

using formatter::courseText;
using formatter::courseSuffix;
using formatter::courseTextFromMag;
using formatter::courseTextFromTrue;

namespace ahtml = atools::util::html;
namespace ageo = atools::geo;

const float HELIPAD_DISTANCE_KM = 0.200f;
const float STARTPOS_DISTANCE_KM = 0.500f;

const float NEAREST_MAX_DISTANCE_AIRPORT_NM = 75.f;
const float NEAREST_MAX_DISTANCE_NAVAID_NM = 50.f;
const int NEAREST_MAX_NUM_AIRPORT = 10;
const int NEAREST_MAX_NUM_NAVAID = 15;

// Print weather time in red if older than this
const int WEATHER_MAX_AGE_HOURS = 6;

// Maximum distance for bearing display
const int MAX_DISTANCE_FOR_BEARING_METER = ageo::nmToMeter(500);

HtmlInfoBuilder::HtmlInfoBuilder(QWidget *parent, bool formatInfo, bool formatPrint)
  : parentWidget(parent), info(formatInfo), print(formatPrint)
{
  mapQuery = NavApp::getMapQuery();
  waypointQuery = NavApp::getWaypointTrackQuery();
  infoQuery = NavApp::getInfoQuery();
  airportQuerySim = NavApp::getAirportQuerySim();
  airportQueryNav = NavApp::getAirportQueryNav();

  morse = new MorseCode("&nbsp;", "&nbsp;&nbsp;&nbsp;");
}

HtmlInfoBuilder::~HtmlInfoBuilder()
{
  delete morse;
}

void HtmlInfoBuilder::airportTitle(const MapAirport& airport, HtmlBuilder& html, int rating) const
{
  if(!print)
  {
    html.img(SymbolPainter().createAirportIcon(airport, symbolSizeTitle.height()),
             QString(), QString(), symbolSizeTitle);
    html.nbsp().nbsp();
  }

  // Adapt title to airport status
  Flags titleFlags = ahtml::BOLD;
  if(airport.closed())
    titleFlags |= ahtml::STRIKEOUT;
  if(airport.addon())
    titleFlags |= ahtml::ITALIC;

  if(info)
  {
    html.text(tr("%1 (%2)").arg(airport.name).arg(airport.ident), titleFlags | ahtml::BIG);
    html.nbsp().nbsp();
    if(rating != -1)
      html.text(atools::ratingString(rating, 5)).nbsp().nbsp();
  }

  if(info)
  {
    if(!print)
      // Add link to map
      html.a(tr("Map"),
             QString("lnm://show?id=%1&type=%2").arg(airport.id).arg(map::AIRPORT),
             ahtml::LINK_NO_UL);
  }
  else
  {
    html.text(tr("%1 (%2)").arg(airport.name).arg(airport.ident), titleFlags);
    if(rating != -1)
      html.nbsp().nbsp().text(atools::ratingString(rating, 5));
  }
}

void HtmlInfoBuilder::flightplanWaypointRemarks(HtmlBuilder& html, int index) const
{
  if(index >= 0)
    html.row2If(tr("Flight Plan Pos. Remarks:"),
                atools::elideTextLinesShort(NavApp::getRouteConst().getFlightplan().at(index).getComment(), 5, 40));
}

void HtmlInfoBuilder::airportText(const MapAirport& airport, const map::WeatherContext& weatherContext,
                                  HtmlBuilder& html, const Route *route) const
{
  const SqlRecord *rec = infoQuery->getAirportInformation(airport.id);
  int rating = -1;

  if(rec != nullptr)
    rating = rec->valueInt("rating");

  airportTitle(airport, html, rating);

  html.table();
  if(!info && route != nullptr && !route->isEmpty() && airport.routeIndex != -1)
  {
    // Add flight plan information if airport is a part of it
    if(airport.routeIndex == route->getDepartureAirportLegIndex())
      html.row2(tr("Departure Airport"), QString());
    else if(airport.routeIndex == route->getDestinationAirportLegIndex())
      html.row2(tr("Destination Airport"), QString());
    else if(airport.routeIndex >= route->getAlternateLegsOffset())
      html.row2(tr("Alternate Airport"), QString());
    else
      html.row2(tr("Flight Plan Position:"), locale.toString(airport.routeIndex + 1));
    flightplanWaypointRemarks(html, airport.routeIndex);
    routeWindText(html, *route, airport.routeIndex);
  }

  // Add bearing/distance to table
  if(!print)
    bearingText(airport.position, airport.magvar, html);

  // Administrative information ======================
  if(NavApp::getCurrentSimulatorDb() == atools::fs::FsPaths::XPLANE11)
  {
    if(airport.ident != airport.xpident)
      html.row2If(tr("X-Plane Ident:"), airport.xpident);
  }

  if(info)
  {
    if(airport.icao != airport.ident)
      html.row2If(tr("ICAO:"), airport.icao);
    html.row2If(tr("IATA:"), rec->valueStr("iata", QString()));
    html.row2If(tr("Region:"), airport.region);
  }

  QString city, state, country;
  airportQuerySim->getAirportAdminNamesById(airport.id, city, state, country);
  html.row2If(tr("City:"), city);
  html.row2If(tr("State or Province:"), state);
  html.row2If(tr("Country or Area Code:"), country);
  html.row2(tr("Elevation:"), Unit::altFeet(airport.getPosition().getAltitude()));
  html.row2(tr("Magnetic declination:"), map::magvarText(airport.magvar));

  // Get transition altitude from nav database
  map::MapAirport navAirport = airport;
  NavApp::getMapQuery()->getAirportNavReplace(navAirport);
  if(navAirport.isValid() && navAirport.transitionAltitude > 0)
    html.row2(tr("Transition altitude:"), Unit::altFeet(navAirport.transitionAltitude));

  if(info)
  {
    // Sunrise and sunset ===========================
    QDateTime datetime =
      NavApp::isConnectedAndAircraft() ? NavApp::getUserAircraft().getZuluTime() : QDateTime::currentDateTimeUtc();

    if(datetime.isValid())
    {
      QString timesource = NavApp::isConnectedAndAircraft() ? tr("simulator date") : tr("real date");

      Pos pos(rec->valueFloat("lonx"), rec->valueFloat("laty"));

      bool neverRises, neverSets;
      QTime sunrise = ageo::calculateSunriseSunset(neverRises, neverSets, pos,
                                                   datetime.date(), ageo::SUNRISE_CIVIL);
      QTime sunset = ageo::calculateSunriseSunset(neverRises, neverSets, pos,
                                                  datetime.date(), ageo::SUNSET_CIVIL);
      QString txt;

      if(neverRises)
        txt = tr("Sun never rises");
      else if(neverSets)
        txt = tr("Sun never sets");
      else
        txt = tr("%1, %2 UTC\n(civil twilight, %3)").
              arg(locale.toString(sunrise, QLocale::ShortFormat)).
              arg(locale.toString(sunset, QLocale::ShortFormat)).
              arg(timesource);

      html.row2(tr("Sunrise and sunset:"), txt);

      // Coordinates ===============================================
      addCoordinates(rec, html);
    }
  }

  html.tableEnd();
  // Create a list of facilities =============================
  if(info)
    head(html, tr("Facilities"));
  html.table();
  QStringList facilities;

  if(airport.closed())
    facilities.append(tr("Closed"));

  if(airport.addon())
    facilities.append(tr("Add-on"));
  if(airport.is3d())
    facilities.append(tr("3D"));
  if(airport.flags.testFlag(AP_MIL))
    facilities.append(tr("Military"));

  if(airport.apron())
    facilities.append(tr("Aprons"));
  if(airport.taxiway())
    facilities.append(tr("Taxiways"));
  if(airport.towerObject())
    facilities.append(tr("Tower Object"));
  if(airport.parking())
    facilities.append(tr("Parking"));

  if(airport.helipad())
    facilities.append(tr("Helipads"));
  if(airport.flags.testFlag(AP_AVGAS))
    facilities.append(tr("Avgas"));
  if(airport.flags.testFlag(AP_JETFUEL))
    facilities.append(tr("Jetfuel"));

  if(airportQueryNav->hasProcedures(airport.ident))
    facilities.append(tr("Procedures"));

  if(airport.flags.testFlag(AP_ILS))
    facilities.append(tr("ILS"));

  if(airport.vasi())
    facilities.append(tr("VASI"));
  if(airport.als())
    facilities.append(tr("ALS"));
  if(airport.fence())
    facilities.append(tr("Boundary Fence"));

  if(airport.flatten == 1)
    facilities.append(tr("Flatten"));
  if(airport.flatten == 0)
    facilities.append(tr("No Flatten"));

  if(facilities.isEmpty())
    facilities.append(tr("None"));

  html.row2(info ? QString() : tr("Facilities:"), facilities.join(tr(", ")));

  html.tableEnd();

  // Create a list of runway attributes
  if(info)
    head(html, tr("Runways"));
  html.table();
  QStringList runways;

  if(!airport.noRunways())
  {
    if(airport.hard())
      runways.append(tr("Hard"));
    if(airport.soft())
      runways.append(tr("Soft"));
    if(airport.water())
      runways.append(tr("Water"));
    if(airport.closedRunways())
      runways.append(tr("Closed"));
    if(airport.flags.testFlag(AP_LIGHT))
      runways.append(tr("Lighted"));
  }
  else
    runways.append(tr("None"));

  html.row2(info ? QString() : tr("Runways:"), runways.join(tr(", ")));
  html.tableEnd();

  if(!info && !airport.noRunways())
  {
    // Add longest for tooltip
    html.table();
    html.row2(tr("Longest Runway Length:"), Unit::distShortFeet(airport.longestRunwayLength));
    html.tableEnd();
  }

  if(!weatherContext.isEmpty())
  {
    if(info)
      head(html, tr("Weather"));
    html.table();

    // Source for map icon display
    MapWeatherSource src = NavApp::getMapWeatherSource();

    const atools::fs::weather::MetarResult& fsMetar = weatherContext.fsMetar;
    if(!fsMetar.isEmpty())
    {
      // Simulator weather =====================================================
      QString sim = tr("%1 ").arg(NavApp::getCurrentSimulatorShortName());
      addMetarLine(html, tr("%1Station").arg(sim), airport, fsMetar.metarForStation,
                   fsMetar.requestIdent, fsMetar.timestamp, true /* fs */, src == WEATHER_SOURCE_SIMULATOR);
      addMetarLine(html, tr("%1Nearest").arg(sim), airport,
                   fsMetar.metarForNearest, fsMetar.requestIdent, fsMetar.timestamp,
                   true /* fs */, src == WEATHER_SOURCE_SIMULATOR);
      addMetarLine(html, tr("%1Interpolated").arg(sim), airport,
                   fsMetar.metarForInterpolated, fsMetar.requestIdent, fsMetar.timestamp,
                   true /* fs */, src == WEATHER_SOURCE_SIMULATOR);
    }

    // Active Sky weather =====================================================
    addMetarLine(html, weatherContext.asType, airport, weatherContext.asMetar, QString(), QDateTime(),
                 false /* fs */, src == WEATHER_SOURCE_ACTIVE_SKY);

    // NOAA weather =====================================================
    addMetarLine(html, tr("NOAA Station"), airport, weatherContext.noaaMetar.metarForStation,
                 weatherContext.noaaMetar.requestIdent, weatherContext.noaaMetar.timestamp,
                 false /* fs */, src == WEATHER_SOURCE_NOAA);
    addMetarLine(html, tr("NOAA Nearest"), airport, weatherContext.noaaMetar.metarForNearest,
                 weatherContext.noaaMetar.requestIdent, weatherContext.noaaMetar.timestamp,
                 false /* fs */, src == WEATHER_SOURCE_NOAA);

    // VATSIM weather =====================================================
    addMetarLine(html, tr("VATSIM Station"), airport, weatherContext.vatsimMetar.metarForStation,
                 weatherContext.vatsimMetar.requestIdent, weatherContext.vatsimMetar.timestamp,
                 false /* fs */, src == WEATHER_SOURCE_VATSIM);
    addMetarLine(html, tr("VATSIM Nearest"), airport, weatherContext.vatsimMetar.metarForNearest,
                 weatherContext.vatsimMetar.requestIdent, weatherContext.vatsimMetar.timestamp,
                 false /* fs */, src == WEATHER_SOURCE_VATSIM);

    // IVAO weather =====================================================
    addMetarLine(html, tr("IVAO Station"), airport, weatherContext.ivaoMetar.metarForStation,
                 weatherContext.ivaoMetar.requestIdent, weatherContext.ivaoMetar.timestamp,
                 false /* fs */, src == WEATHER_SOURCE_IVAO);
    addMetarLine(html, tr("IVAO Nearest"), airport, weatherContext.ivaoMetar.metarForNearest,
                 weatherContext.ivaoMetar.requestIdent, weatherContext.ivaoMetar.timestamp,
                 false /* fs */, src == WEATHER_SOURCE_IVAO);
    html.tableEnd();
  }

  if(info && !airport.noRunways())
  {
    head(html, tr("Longest Runway"));
    html.table();
    html.row2(tr("Length:"), Unit::distShortFeet(airport.longestRunwayLength));
    if(rec != nullptr)
    {
      html.row2(tr("Width:"),
                Unit::distShortFeet(rec->valueInt("longest_runway_width")));

      float hdg = rec->valueFloat("longest_runway_heading");
      html.row2(tr("Heading:"), courseTextFromTrue(hdg, airport.magvar) + tr(", ") +
                courseTextFromTrue(opposedCourseDeg(hdg), airport.magvar), ahtml::NO_ENTITIES);
      html.row2(tr("Surface:"),
                map::surfaceName(rec->valueStr("longest_runway_surface")));
    }
    html.tableEnd();
  }

  // Add most important COM frequencies
  if(airport.towerFrequency > 0 || airport.atisFrequency > 0 || airport.awosFrequency > 0 ||
     airport.asosFrequency > 0 || airport.unicomFrequency > 0)
  {
    if(info)
      head(html, tr("COM Frequencies"));
    html.table();
    if(airport.towerFrequency > 0)
      html.row2(tr("Tower:"),
                locale.toString(roundComFrequency(airport.towerFrequency), 'f', 3) + tr(" MHz"));
    if(airport.atisFrequency > 0)
      html.row2(tr("ATIS:"), locale.toString(roundComFrequency(airport.atisFrequency), 'f', 3) + tr(" MHz"));
    if(airport.awosFrequency > 0)
      html.row2(tr("AWOS:"), locale.toString(roundComFrequency(airport.awosFrequency), 'f', 3) + tr(" MHz"));
    if(airport.asosFrequency > 0)
      html.row2(tr("ASOS:"), locale.toString(roundComFrequency(airport.asosFrequency), 'f', 3) + tr(" MHz"));
    if(airport.unicomFrequency > 0)
      html.row2(tr("Unicom:"), locale.toString(roundComFrequency(airport.unicomFrequency), 'f', 3) + tr(" MHz"));
    html.tableEnd();
  }

  if(info)
  {
    // Parking overview
    int numParkingGate = rec->valueInt("num_parking_gate");
    int numJetway = rec->valueInt("num_jetway");
    int numParkingGaRamp = rec->valueInt("num_parking_ga_ramp");
    int numParkingCargo = rec->valueInt("num_parking_cargo");
    int numParkingMilCargo = rec->valueInt("num_parking_mil_cargo");
    int numParkingMilCombat = rec->valueInt("num_parking_mil_combat");
    int numHelipad = rec->valueInt("num_helipad");

    head(html, tr("Parking"));
    html.table();
    if(numParkingGate > 0 || numJetway > 0 || numParkingGaRamp > 0 || numParkingCargo > 0 ||
       numParkingMilCargo > 0 || numParkingMilCombat > 0 ||
       !rec->isNull("largest_parking_ramp") || !rec->isNull("largest_parking_gate"))
    {
      if(numParkingGate > 0)
        html.row2(tr("Gates:"), numParkingGate);
      if(numJetway > 0)
        html.row2(tr("Jetways:"), numJetway);
      if(numParkingGaRamp > 0)
        html.row2(tr("GA Ramp:"), numParkingGaRamp);
      if(numParkingCargo > 0)
        html.row2(tr("Cargo:"), numParkingCargo);
      if(numParkingMilCargo > 0)
        html.row2(tr("Military Cargo:"), numParkingMilCargo);
      if(numParkingMilCombat > 0)
        html.row2(tr("Military Combat:"), numParkingMilCombat);

      if(!rec->isNull("largest_parking_ramp"))
        html.row2(tr("Largest Ramp:"), map::parkingRampName(rec->valueStr("largest_parking_ramp")));
      if(!rec->isNull("largest_parking_gate"))
        html.row2(tr("Largest Gate:"), map::parkingRampName(rec->valueStr("largest_parking_gate")));

      if(numHelipad > 0)
        html.row2(tr("Helipads:"), numHelipad);
    }
    else
      html.row2(QString(), tr("None"));
    html.tableEnd();
  } // if(info)

  // Add apt.dat or BGL file links
  if(info && !print)
    addAirportSceneryAndLinks(airport, html);

  // Add links to airport files directory "Documents/Airports/ICAO"
  if(info && !print)
    addAirportFolder(airport, html);

#ifdef DEBUG_INFORMATION
  MapAirport airportNav = mapQuery->getAirportNav(airport);

  html.small(QString("Database: airport_id = %1 (sim %2, %3)").arg(airport.getId()).
             arg(airportNav.getId()).arg(airportNav.ident)).br();

#endif
}

void HtmlInfoBuilder::nearestText(const MapAirport& airport, HtmlBuilder& html) const
{
  // Do not display as tooltip
  if(info && airport.isValid())
  {
    if(!print)
      airportTitle(airport, html, -1);

    // Get nearest airports that have procedures ====================================
    MapSearchResultIndex *nearestAirports = airportQueryNav->getNearestAirportsProc(airport,
                                                                                    NEAREST_MAX_DISTANCE_AIRPORT_NM);
    if(!nearestMapObjectsText(airport, html, nearestAirports, tr("Nearest Airports with Procedures"), false, true,
                              NEAREST_MAX_NUM_AIRPORT))
      html.p().b(tr("No airports with procedures within a radius of %1.").
                 arg(Unit::distNm(NEAREST_MAX_DISTANCE_AIRPORT_NM * 4.f))).pEnd();

    // Get nearest VOR and NDB ====================================
    MapSearchResultIndex *nearestNavaids = mapQuery->getNearestNavaids(airport.position,
                                                                       NEAREST_MAX_DISTANCE_NAVAID_NM,
                                                                       map::VOR | map::NDB | map::ILS,
                                                                       3 /* max ILS */, 4.f /* max ILS dist NM */);
    if(!nearestMapObjectsText(airport, html, nearestNavaids, tr("Nearest Radio Navaids"), true, false,
                              NEAREST_MAX_NUM_NAVAID))
      html.p().b(tr("No navaids within a radius of %1.").
                 arg(Unit::distNm(NEAREST_MAX_DISTANCE_NAVAID_NM * 4.f))).pEnd();
  }
}

void HtmlInfoBuilder::nearestMapObjectsTextRow(const MapAirport& airport, HtmlBuilder& html,
                                               const QString& type, const QString& ident, const QString& name,
                                               const QString& freq, const map::MapBase *base, float magVar,
                                               bool frequencyCol, bool airportCol) const
{
  float distance = airport.position.distanceMeterTo(base->position);
  float bearingTrue = normalizeCourse(airport.position.angleDegTo(base->position));

  QString url;
  if(base->objType == map::AIRPORT)
    // Show by id does only work for airport (needed for centering)
    url = QString("lnm://show?id=%1&type=%2").arg(base->id).arg(base->objType);
  else
    // Show all other navaids by coordinate
    url = QString("lnm://show?lonx=%1&laty=%2").arg(base->position.getLonX()).arg(base->position.getLatY());

  // Create table row ==========================
  html.tr(QColor());

  if(airportCol)
  {
    // Airports have no type and link is on name
    html.td(ident, ahtml::BOLD);
    html.tdF(ahtml::ALIGN_RIGHT).a(name, url, ahtml::LINK_NO_UL).tdEnd();
  }
  else
  {
    // Navaid type
    html.td(type, ahtml::BOLD);
    html.td(ident, ahtml::BOLD);
    html.tdF(ahtml::ALIGN_RIGHT).a(name, url, ahtml::LINK_NO_UL).tdEnd();
  }

  if(frequencyCol)
    html.td(freq, ahtml::ALIGN_RIGHT);

  html.td(courseTextFromTrue(bearingTrue, magVar), ahtml::ALIGN_RIGHT | ahtml::NO_ENTITIES).
  td(Unit::distMeter(distance, false), ahtml::ALIGN_RIGHT).
  trEnd();
}

bool HtmlInfoBuilder::nearestMapObjectsText(const MapAirport& airport, HtmlBuilder& html,
                                            const map::MapSearchResultIndex *nearest, const QString& header,
                                            bool frequencyCol, bool airportCol, int maxRows) const
{
  if(nearest != nullptr && !nearest->isEmpty())
  {
    html.br().br().text(header, ahtml::BOLD | ahtml::BIG);
    html.table();

    // Create table header ==========================
    html.tr(QColor());

    if(!airportCol)
      html.th(tr("Type"));

    html.th(tr("Ident")).
    th(tr("Name"));

    if(frequencyCol)
      // Only for navaids
      html.th(tr("Frequency\nkHz/MHz"));

    html.th(tr("Bearing\n%1").arg(courseSuffix())).
    th(tr("Distance\n%1").arg(Unit::getUnitDistStr())).
    trEnd();

    // Go through mixed list of map objects ============================================
    int row = 1;
    for(const map::MapBase *base : *nearest)
    {
      if(row++ > maxRows)
        // Stop at max
        break;

      // Airport ======================================
      const map::MapAirport *ap = base->asType<map::MapAirport>(map::AIRPORT);
      if(ap != nullptr)
      {
        // Convert navdatabase airport to simulator
        map::MapAirport simAp = mapQuery->getAirportSim(*ap);

        // Omit center airport used as reference
        if(simAp.isValid() && simAp.id != airport.id)
          nearestMapObjectsTextRow(airport, html, QString(), simAp.ident, simAp.name,
                                   QString(), &simAp, simAp.magvar, frequencyCol, airportCol);
      }

      const map::MapVor *vor = base->asType<map::MapVor>(map::VOR);
      if(vor != nullptr)
        nearestMapObjectsTextRow(airport, html, map::vorType(*vor), vor->ident, vor->name,
                                 locale.toString(vor->frequency / 1000., 'f',
                                                 2), vor, vor->magvar, frequencyCol, airportCol);

      const map::MapNdb *ndb = base->asType<map::MapNdb>(map::NDB);
      if(ndb != nullptr)
        nearestMapObjectsTextRow(airport, html, tr("NDB"), ndb->ident, ndb->name,
                                 locale.toString(ndb->frequency / 100., 'f',
                                                 1), ndb, ndb->magvar, frequencyCol, airportCol);

      const map::MapWaypoint *waypoint = base->asType<map::MapWaypoint>(map::WAYPOINT);
      if(waypoint != nullptr)
        nearestMapObjectsTextRow(airport, html, tr("Waypoint"), waypoint->ident, QString(),
                                 QString(), waypoint, waypoint->magvar, frequencyCol, airportCol);

      const map::MapIls *ils = base->asType<map::MapIls>(map::ILS);
      if(ils != nullptr)
        nearestMapObjectsTextRow(airport, html, map::ilsType(*ils), ils->ident, ils->name,
                                 QString::number(ils->frequency / 1000., 'f',
                                                 2), ils, ils->magvar, frequencyCol, airportCol);
    }
    html.tableEnd();
    return true;
  }
  return false;
}

void HtmlInfoBuilder::comText(const MapAirport& airport, HtmlBuilder& html) const
{
  if(info && infoQuery != nullptr)
  {
    if(!print)
      airportTitle(airport, html, -1);

    const SqlRecordVector *recVector = infoQuery->getComInformation(airport.id);
    if(recVector != nullptr)
    {
      html.table();
      html.tr(QColor()).td(tr("Type"), ahtml::BOLD).
      td(tr("Frequency"), ahtml::BOLD).
      td(tr("Name"), ahtml::BOLD).trEnd();

      for(const SqlRecord& rec : *recVector)
      {
        html.tr(QColor());
        html.td(map::comTypeName(rec.valueStr("type")));
        // Round frequencies to nearest valid value to workaround for a compiler rounding bug
        html.td(locale.toString(roundComFrequency(rec.valueInt("frequency")), 'f', 3) + tr(" MHz")
#ifdef DEBUG_INFORMATION
                + " [" + QString::number(rec.valueInt("frequency")) + "]"
#endif
                );
        if(rec.valueStr("type") != tr("ATIS"))
          html.td(capString(rec.valueStr("name")));
        else
          // ATIS contains the airport code - do not capitalize this
          html.td(rec.valueStr("name"));
        html.trEnd();
      }
      html.tableEnd();
    }
    else
      html.p(tr("Airport has no COM Frequency."));
  }
}

void HtmlInfoBuilder::bestRunwaysText(const MapAirport& airport, HtmlBuilder& html,
                                      const atools::fs::weather::MetarParser& parsed, int max, bool details) const
{
  int windDirectionDeg = parsed.getWindDir();
  float windSpeedKts = parsed.getWindSpeedKts();
  maptools::RwVector ends(windSpeedKts, windDirectionDeg);

  // Need wind direction and speed - otherwise all runways are good =======================
  if(windDirectionDeg != -1 && windSpeedKts < atools::fs::weather::INVALID_METAR_VALUE)
  {
    const SqlRecordVector *recVector = infoQuery->getRunwayInformation(airport.id);
    if(recVector != nullptr)
    {

      // Collect runway ends and wind conditions =======================================
      for(const SqlRecord& rec : *recVector)
      {
        int length = rec.valueInt("length");
        QString surface = rec.valueStr("surface");
        const SqlRecord *recPrim = infoQuery->getRunwayEndInformation(rec.valueInt("primary_end_id"));
        if(!recPrim->valueBool("has_closed_markings"))
          ends.appendRwEnd(recPrim->valueStr("name"), surface, length, recPrim->valueFloat("heading"));

        const SqlRecord *recSec = infoQuery->getRunwayEndInformation(rec.valueInt("secondary_end_id"));
        if(!recSec->valueBool("has_closed_markings"))
          ends.appendRwEnd(recSec->valueStr("name"), surface, length, recSec->valueFloat("heading"));
      }
    }
  }

  if(!ends.isEmpty())
  {
    // Sort by headwind =======================================
    ends.sortRunwayEnds();

    max = std::min(ends.size(), max);
    QString rwTxt = ends.getTotalNumber() == 1 ? tr("Runway") : tr("Runways");

    if(details)
    {
      // Table header for detailed view
      head(html, tr("Best %1 for wind").arg(rwTxt.toLower()));
      html.table();
      html.tr(QColor()).th(rwTxt).th(tr("Surface")).th(tr("Length")).th(tr("Headwind")).th(tr("Crosswind")).trEnd();
    }

    // Create runway table for details =====================================

    if(details)
    {
      // Table for detailed view
      int num = 0;
      for(const maptools::RwEnd& end : ends)
      {
        // Stop at maximum number - tailwind is alread sorted out
        if(num > max)
          break;

        QString lengthTxt;
        if(end.minlength == end.maxlength)
          lengthTxt = Unit::distShortFeet(end.minlength);
        else
          // Grouped runways have a min and max length
          lengthTxt = tr("%1-%2").
                      arg(Unit::distShortFeet(end.minlength, false, false)).
                      arg(Unit::distShortFeet(end.maxlength));

        // Table entry ==================
        html.tr(QColor()).
        td(end.names.join(tr(", ")), ahtml::BOLD).
        td(end.soft ? tr("Soft") : tr("Hard")).
        td(lengthTxt, ahtml::ALIGN_RIGHT).
        td(formatter::windInformationHead(end.head), ahtml::ALIGN_RIGHT).
        td(Unit::speedKts(end.cross), ahtml::ALIGN_RIGHT).
        trEnd();
        num++;
      }
      html.tableEnd();
    }
    else
    {
      // Simple runway list for tooltips only with headwind > 2
      QStringList runways;
      for(const maptools::RwEnd& end : ends)
      {
        if(end.head <= 2)
          break;
        runways.append(end.names);
      }

      if(!runways.isEmpty())
        html.br().b(tr(" Prefers %1: ").arg(rwTxt)).text(runways.mid(0, 4).join(tr(", ")));
    }
  }
  else if(details)
    // Either crosswind is equally strong and/or headwind too low or no directional wind
    head(html, tr("All runways good for landing or takeoff."));
}

void HtmlInfoBuilder::runwayText(const MapAirport& airport, HtmlBuilder& html, bool details, bool soft) const
{
  if(info && infoQuery != nullptr)
  {
    if(!print)
      airportTitle(airport, html, -1);
    html.br();

    const SqlRecordVector *recVector = infoQuery->getRunwayInformation(airport.id);
    if(recVector != nullptr)
    {
      for(const SqlRecord& rec : *recVector)
      {
        if(!soft && !map::isHardSurface(rec.valueStr("surface")))
          continue;

        const SqlRecord *recPrim = infoQuery->getRunwayEndInformation(rec.valueInt("primary_end_id"));
        const SqlRecord *recSec = infoQuery->getRunwayEndInformation(rec.valueInt("secondary_end_id"));
        bool closedPrim = recPrim->valueBool("has_closed_markings");
        bool closedSec = recSec->valueBool("has_closed_markings");

        html.br().text(tr("Runway ") + recPrim->valueStr("name") + ", " + recSec->valueStr("name"),
                       (closedPrim & closedSec ? ahtml::STRIKEOUT : ahtml::NONE)
                       | ahtml::UNDERLINE | ahtml::BIG | ahtml::BOLD);
        html.table();

        html.row2(tr("Size:"), Unit::distShortFeet(rec.valueFloat("length"), false) +
                  tr(" x ") +
                  Unit::distShortFeet(rec.valueFloat("width")));

        html.row2(tr("Surface:"), strJoinVal({map::surfaceName(rec.valueStr("surface")),
                                              map::smoothnessName(rec.valueFloat("smoothness", -1.f))}));

        if(rec.valueFloat("pattern_altitude") > 0)
          html.row2(tr("Pattern Altitude:"), Unit::altFeet(rec.valueFloat("pattern_altitude")));

        // Lights information
        if(!rec.isNull("edge_light"))
          html.row2(tr("Edge Lights:"), map::edgeLights(rec.valueStr("edge_light")));
        if(!rec.isNull("center_light"))
          html.row2(tr("Center Lights:"), map::edgeLights(rec.valueStr("center_light")));

        rowForBool(html, &rec, "has_center_red", tr("Has red Center Lights"), false);

        // Add a list of runway markings
        atools::fs::bgl::rw::RunwayMarkingFlags flags(rec.valueInt("marking_flags"));
        QStringList markings;
        if(flags & atools::fs::bgl::rw::EDGES)
          markings.append(tr("Edges"));
        if(flags & atools::fs::bgl::rw::THRESHOLD)
          markings.append(tr("Threshold"));
        if(flags & atools::fs::bgl::rw::FIXED_DISTANCE)
          markings.append(tr("Fixed Distance"));
        if(flags & atools::fs::bgl::rw::TOUCHDOWN)
          markings.append(tr("Touchdown"));
        if(flags & atools::fs::bgl::rw::DASHES)
          markings.append(tr("Dashes"));
        if(flags & atools::fs::bgl::rw::IDENT)
          markings.append(tr("Ident"));
        if(flags & atools::fs::bgl::rw::PRECISION)
          markings.append(tr("Precision"));
        if(flags & atools::fs::bgl::rw::EDGE_PAVEMENT)
          markings.append(tr("Edge Pavement"));
        if(flags & atools::fs::bgl::rw::SINGLE_END)
          markings.append(tr("Single End"));

        if(flags & atools::fs::bgl::rw::ALTERNATE_THRESHOLD)
          markings.append(tr("Alternate Threshold"));
        if(flags & atools::fs::bgl::rw::ALTERNATE_FIXEDDISTANCE)
          markings.append(tr("Alternate Fixed Distance"));
        if(flags & atools::fs::bgl::rw::ALTERNATE_TOUCHDOWN)
          markings.append(tr("Alternate Touchdown"));
        if(flags & atools::fs::bgl::rw::ALTERNATE_PRECISION)
          markings.append(tr("Alternate Precision"));

        if(flags & atools::fs::bgl::rw::LEADING_ZERO_IDENT)
          markings.append(tr("Leading Zero Ident"));

        if(flags & atools::fs::bgl::rw::NO_THRESHOLD_END_ARROWS)
          markings.append(tr("No Threshold End Arrows"));

        if(markings.isEmpty())
          markings.append(tr("None"));

        html.row2(tr("Runway Markings:"), markings.join(tr(", ")));

        html.tableEnd();

        if(details)
        {
          if(print)
            html.table().tr().td();

          runwayEndText(html, airport, recPrim, rec.valueFloat("heading"), rec.valueFloat("length"));
          if(print)
            html.tdEnd().td();

          runwayEndText(html, airport, recSec, opposedCourseDeg(rec.valueFloat("heading")), rec.valueFloat("length"));

          if(print)
            html.tdEnd().trEnd().tableEnd();
        }
#ifdef DEBUG_INFORMATION
        html.small(QString("Database: runway_id = %1").arg(rec.valueInt("runway_id"))).br();
        html.small(QString("Database: Primary runway_end_id = %1").arg(recPrim->valueInt("runway_end_id"))).br();
        html.small(QString("Database: Secondary runway_end_id = %1").arg(recSec->valueInt("runway_end_id"))).br();
#endif
      }
    }
    else
      html.p(tr("Airport has no runway."));

    if(details)
    {
      // Helipads ==============================================================
      const SqlRecordVector *heliVector = infoQuery->getHelipadInformation(airport.id);

      if(heliVector != nullptr)
      {
        for(const SqlRecord& heliRec : *heliVector)
        {
          bool closed = heliRec.valueBool("is_closed");
          bool hasStart = !heliRec.isNull("start_number");

          QString num = hasStart ? " " + heliRec.valueStr("runway_name") : tr(" (No Start Position)");

          html.h3(tr("Helipad%1").arg(num),
                  (closed ? ahtml::STRIKEOUT : ahtml::NONE)
                  | ahtml::UNDERLINE);
          html.nbsp().nbsp();

          Pos pos(heliRec.valueFloat("lonx"), heliRec.valueFloat("laty"));

          if(!print)
            html.a(tr("Map"), QString("lnm://show?lonx=%1&laty=%2&distance=%3").
                   arg(pos.getLonX()).arg(pos.getLatY()).arg(HELIPAD_DISTANCE_KM),
                   ahtml::LINK_NO_UL).br();

          if(closed)
            html.text(tr("Is Closed"));
          html.table();

          html.row2(tr("Size:"), Unit::distShortFeet(heliRec.valueFloat("width"), false) +
                    tr(" x ") +
                    Unit::distShortFeet(heliRec.valueFloat("length")));
          html.row2(tr("Surface:"), map::surfaceName(heliRec.valueStr("surface")) +
                    (heliRec.valueBool("is_transparent") ? tr(" (Transparent)") : QString()));
          html.row2(tr("Type:"), atools::capString(heliRec.valueStr("type")));
          html.row2(tr("Heading:"), courseTextFromTrue(heliRec.valueFloat("heading"), airport.magvar),
                    ahtml::NO_ENTITIES);
          html.row2(tr("Elevation:"), Unit::altFeet(heliRec.valueFloat("altitude")));

          addCoordinates(&heliRec, html);
          html.tableEnd();
        }
      }
      else
        html.p(tr("Airport has no helipad."));

      // Start positions ==============================================================
      const SqlRecordVector *startVector = infoQuery->getStartInformation(airport.id);

      if(startVector != nullptr && !startVector->isEmpty())
      {
        html.h3(tr("Start Positions"));

        int i = 0;
        for(const SqlRecord& startRec : *startVector)
        {
          QString type = startRec.valueStr("type");
          QString name = startRec.valueStr("runway_name");
          QString startText;
          if(type == "R")
            startText = tr("Runway %1").arg(name);
          else if(type == "H")
            startText = tr("Helipad %1").arg(name);
          else if(type == "W")
            startText = tr("Water %1").arg(name);

          Pos pos(startRec.valueFloat("lonx"), startRec.valueFloat("laty"));

          if(i > 0)
            html.text(tr(", "));
          if(print)
            html.text(startText);
          else
            html.a(startText, QString("lnm://show?lonx=%1&laty=%2&distance=%3").
                   arg(pos.getLonX()).arg(pos.getLatY()).arg(STARTPOS_DISTANCE_KM),
                   ahtml::LINK_NO_UL);
          i++;
        }
      }
      else
        html.p(tr("Airport has no start position."));
    }
  }
}

void HtmlInfoBuilder::runwayEndText(HtmlBuilder& html, const MapAirport& airport, const SqlRecord *rec,
                                    float hdgPrimTrue, float length) const
{
  bool closed = rec->valueBool("has_closed_markings");

  html.br().br().text(rec->valueStr("name"), (closed ? (ahtml::STRIKEOUT) : ahtml::NONE) |
                      ahtml::BOLD | ahtml::BIG);
  html.table();
  if(closed)
    html.row2(tr("Closed"), QString());
  html.row2(tr("Heading:"), courseTextFromTrue(hdgPrimTrue, airport.magvar, true), ahtml::NO_ENTITIES);

  float threshold = rec->valueFloat("offset_threshold");
  if(threshold > 1.f)
  {
    html.row2(tr("Offset Threshold:"), Unit::distShortFeet(threshold));
    html.row2(tr("Effective Landing Distance:"), Unit::distShortFeet(length - threshold));
  }

  float blastpad = rec->valueFloat("blast_pad");
  if(blastpad > 1.f)
    html.row2(tr("Blast Pad:"), Unit::distShortFeet(blastpad));

  float overrrun = rec->valueFloat("overrun");
  if(overrrun > 1.f)
    html.row2(tr("Overrun:"), Unit::distShortFeet(overrrun));

  rowForBool(html, rec, "has_stol_markings", tr("Has STOL Markings"), false);

  // Two flags below only are probably only for AI
  // rowForBool(html, rec, "is_takeoff", tr("Closed for Takeoff"), true);
  // rowForBool(html, rec, "is_landing", tr("Closed for Landing"), true);

  if(!rec->isNull("is_pattern") && rec->valueStr("is_pattern") != "N" /* none X-Plane */)
    html.row2(tr("Pattern:"), map::patternDirection(rec->valueStr("is_pattern")));

  // Approach indicators
  if(rec->valueStr("right_vasi_type") == "UNKN")
  {
    // X-Plane - side is unknown
    rowForStr(html, rec, "left_vasi_type", tr("VASI Type:"), tr("%1"));
    rowForFloat(html, rec, "left_vasi_pitch", tr("VASI Pitch:"), tr("%1°"), 1);
  }
  else
  {
    rowForStr(html, rec, "left_vasi_type", tr("Left VASI Type:"), tr("%1"));
    rowForFloat(html, rec, "left_vasi_pitch", tr("Left VASI Pitch:"), tr("%1°"), 1);
    rowForStr(html, rec, "right_vasi_type", tr("Right VASI Type:"), tr("%1"));
    rowForFloat(html, rec, "right_vasi_pitch", tr("Right VASI Pitch:"), tr("%1°"), 1);
  }

  rowForStr(html, rec, "app_light_system_type", tr("ALS Type:"), tr("%1"));

  // End lights
  QStringList lights;
  if(rec->valueBool("has_end_lights"))
    lights.append(tr("Lights"));
  if(rec->valueBool("has_reils"))
    lights.append(tr("Strobes"));
  if(rec->valueBool("has_touchdown_lights"))
    lights.append(tr("Touchdown"));
  if(!lights.isEmpty())
    html.row2(tr("Runway End Lights:"), lights.join(tr(", ")));
  html.tableEnd();

  // Show none, one or more ILS
  const atools::sql::SqlRecordVector *ilsRec =
    infoQuery->getIlsInformationSimByName(airport.ident, rec->valueStr("name"));
  if(ilsRec != nullptr)
  {
    for(const atools::sql::SqlRecord& irec : *ilsRec)
      ilsText(&irec, html, false /* approach */, false /* standalone */);
  }
}

void HtmlInfoBuilder::ilsText(const map::MapIls& ils, HtmlBuilder& html) const
{
  atools::sql::SqlRecord ilsRec = infoQuery->getIlsInformationSimById(ils.id);
  ilsText(&ilsRec, html, false /* approach */, true /* standalone */);
}

void HtmlInfoBuilder::ilsText(const atools::sql::SqlRecord *ilsRec, HtmlBuilder& html, bool approach,
                              bool standalone) const
{
  // Add ILS information
  bool dme = !ilsRec->isNull("dme_altitude");
  bool gs = !ilsRec->isNull("gs_altitude");
  float ilsMagvar = ilsRec->valueFloat("mag_var");
  QString ident = ilsRec->valueStr("ident");
  QString name = ilsRec->valueStr("name");

  // Prefix for procedure information
  QString prefix;
  QString text = map::ilsTextShort(ident, name, gs, dme);

  if(gs)
    prefix = approach ? tr("ILS ") : QString();
  else if(!name.startsWith(QObject::tr("LOC")))
    prefix = approach ? tr("LOC ") : QString();

  // Check if text is to be generated for navaid or procedures tab
  if(standalone)
  {
    html.img(QIcon(":/littlenavmap/resources/icons/ils.svg"), QString(), QString(), symbolSizeTitle);
    html.nbsp().nbsp();

    navaidTitle(html, text);

    if(info)
    {
      // Add map link if not tooltip ==========================================
      html.nbsp().nbsp();
      html.a(tr("Map"),
             QString("lnm://show?id=%1&type=%2").arg(ilsRec->valueInt("ils_id")).arg(map::ILS), ahtml::LINK_NO_UL);
    }
    html.table();
  }
  else
  {
    if(approach)
      html.row2(prefix + tr(":"), name);
    else
    {
      html.br().br().text(text, ahtml::BOLD | ahtml::BIG);
      html.table();
    }
  }

  if(standalone)
  {
    // Add bearing/distance to table ==========================
    bearingText(ageo::Pos(ilsRec->valueFloat("lonx"), ilsRec->valueFloat("laty")), ilsMagvar, html);

    // ILS information ==================================================
    QString runway = ilsRec->valueStr("loc_runway_name");
    QString airportIdent = ilsRec->valueStr("loc_airport_ident");

    if(runway.isEmpty())
      html.row2If(tr("Airport:"), airportIdent, ahtml::BOLD);
    else
      html.row2If(tr("Airport and runway:"), tr("%1, %2").arg(airportIdent).arg(runway), ahtml::BOLD);

    if(info)
    {
      html.row2If(tr("Region:"), ilsRec->valueStr("region"));
    }
  }

  html.row2(prefix + tr("Frequency:"),
            locale.toString(ilsRec->valueFloat("frequency") / 1000., 'f', 2) + tr(" MHz"));
  if(!approach)
  {
    html.row2(tr("Magnetic declination:"), map::magvarText(ilsMagvar));
    html.row2(tr("Elevation:"), Unit::altFeet(ilsRec->valueFloat("altitude")));
  }

  if(!approach)
  {
    html.row2(tr("Range:"), Unit::distNm(ilsRec->valueFloat("range")));
    addMorse(html, tr("Morse:"), ilsRec->valueStr("ident"));
    rowForBool(html, ilsRec, "has_backcourse", tr("Has Backcourse"), false);
  }

  // Localizer information ==================================================
  float hdgTrue = ilsRec->valueFloat("loc_heading");

  if(ilsRec->isNull("loc_width"))
    html.row2(prefix + tr("Localizer Heading:"), courseTextFromTrue(hdgTrue, ilsMagvar), ahtml::NO_ENTITIES);
  else
    html.row2(prefix + tr("Localizer Heading and Width:"), courseTextFromTrue(hdgTrue, ilsMagvar) +
              tr(", ") + locale.toString(ilsRec->valueFloat("loc_width"), 'f', 1) + tr("°"), ahtml::NO_ENTITIES);

  // Check for offset localizer ==================================================
  map::MapRunwayEnd end;
  int endId = ilsRec->valueInt("loc_runway_end_id", 0);
  if(endId > 0)
  {
    // Get assigned runway end =====================
    end = airportQuerySim->getRunwayEndById(endId);
    if(end.isValid())
    {
      // The maximum angular offset for a LOC is 3° for FAA and 5° for ICAO.
      // Everything else is named either LDA (FAA) or IGS (ICAO).
      if(ageo::angleAbsDiff(end.heading, ilsRec->valueFloat("loc_heading")) > 2.5f)
      {
        // Get airport for consistent magnetic variation =====================
        map::MapAirport airport = airportQuerySim->getAirportByIdent(ilsRec->valueStr("loc_airport_ident"));

        // Prefer airport variation to have the same runway heading as in the runway information
        float rwMagvar = airport.isValid() ? airport.magvar : ilsMagvar;
        html.row2(tr("Offset localizer."));
        html.row2(tr("Runway heading:"), courseTextFromTrue(end.heading, rwMagvar), ahtml::NO_ENTITIES);
      }
    }
  }

  if(gs)
    html.row2(prefix + tr("Glideslope Pitch:"),
              locale.toString(ilsRec->valueFloat("gs_pitch"), 'f', 1) + tr("°"));

  if(!approach)
    html.tableEnd();

  if(info && standalone && !approach)
    // Add scenery indicator to clear source - either nav or sim
    addScenery(nullptr, html, true /* ILS */);

#ifdef DEBUG_INFORMATION
  html.small(QString("Database: ils_id = %1").arg(ilsRec->valueInt("ils_id"))).br();
  if(end.isValid())
    html.small(QString("Database: runway_end_id = %1").arg(end.id)).br();
#endif

  if(info)
    html.br();
}

void HtmlInfoBuilder::helipadText(const MapHelipad& helipad, HtmlBuilder& html) const
{
  QString num = helipad.start != -1 ? (" " + helipad.runwayName) : QString();

  head(html, tr("Helipad%1:").arg(num));
  html.brText(tr("Surface: ") + map::surfaceName(helipad.surface));
  html.brText(tr("Type: ") + atools::capString(helipad.type));
  html.brText(Unit::distShortFeet(std::max(helipad.width, helipad.length)));
  if(helipad.closed)
    html.brText(tr("Is Closed"));
}

void HtmlInfoBuilder::windText(const atools::grib::WindPosVector& windStack, HtmlBuilder& html,
                               float currentAltitude, const QString& source) const
{
  if(!windStack.isEmpty())
  {
    float magVar = NavApp::getMagVar(windStack.first().pos);
    if(windStack.size() == 1)
    {
      // Single line wind report for flight plan waypoints =============================================
      const atools::grib::WindPos& wind = windStack.first();
      if(wind.isValid())
      {
        html.row2(tr("Flight Plan wind (%1)").arg(source), tr("%2, %3, %4").
                  arg(Unit::altFeet(wind.pos.getAltitude())).
                  arg(courseTextFromTrue(wind.wind.dir, magVar, false /* no bold */, true /* no small */)).
                  arg(Unit::speedKts(wind.wind.speed)), ahtml::NO_ENTITIES);
      }
    }
    else
    {
      // Several wind layers report for wind barbs =============================================
      head(html, tr("Wind (%1)").arg(source));
      html.table();
      html.tr().th(Unit::getUnitAltStr()).th(courseSuffix()).th(Unit::getUnitSpeedStr()).trEnd();

      // Wind reports are all at the same position
      for(const atools::grib::WindPos& wind : windStack)
      {
        if(!wind.wind.isValid())
          continue;

        Flags flags = ahtml::ALIGN_RIGHT;

        float alt = wind.pos.getAltitude();
        flags |= atools::almostEqual(alt, currentAltitude, 10.f) ? ahtml::BOLD : ahtml::NONE;

        // One table row with three data fields
        QString courseTxt;
        if(wind.wind.isNull())
          courseTxt = tr("-");
        else
          courseTxt = courseTextFromTrue(wind.wind.dir, magVar, false /* no bold */, true /* no small */);

        QString speedTxt;
        if(wind.wind.isNull())
          speedTxt = tr("0");
        else
          speedTxt = tr("%1").arg(Unit::speedKtsF(wind.wind.speed), 0, 'f', 0);

        html.tr().
        td(atools::almostEqual(alt, 260.f) ? tr("Ground") : Unit::altFeet(alt, false), flags).
        td(courseTxt, flags | ahtml::NO_ENTITIES).
        td(speedTxt, flags).
        trEnd();
      }
      html.tableEnd();
    }
  }
}

void HtmlInfoBuilder::procedureText(const MapAirport& airport, HtmlBuilder& html) const
{
  if(info && infoQuery != nullptr && airport.isValid())
  {
    MapAirport navAirport = mapQuery->getAirportNav(airport);

    if(!print)
      airportTitle(airport, html, -1);

    const SqlRecordVector *recAppVector = infoQuery->getApproachInformation(navAirport.id);
    if(recAppVector != nullptr)
    {
      QStringList runwayNames = airportQueryNav->getRunwayNames(navAirport.id);

      for(const SqlRecord& recApp : *recAppVector)
      {
        // Approach information ==============================

        // Build header ===============================================
        QString procType = recApp.valueStr("type");
        proc::MapProcedureTypes type = proc::procedureType(NavApp::hasSidStarInDatabase(),
                                                           procType,
                                                           recApp.valueStr("suffix"),
                                                           recApp.valueBool("has_gps_overlay"));

        if(type & proc::PROCEDURE_STAR_ALL || type & proc::PROCEDURE_SID_ALL)
          continue;

        int rwEndId = recApp.valueInt("runway_end_id");

        QString runwayIdent = map::runwayBestFit(recApp.valueStr("runway_name"), runwayNames);
        QString runwayTxt = runwayIdent;

        if(!runwayTxt.isEmpty())
          runwayTxt = tr(" - Runway ") + runwayTxt;

        QString fix = recApp.valueStr("fix_ident");
        QString header;

        if(type & proc::PROCEDURE_SID)
          header = tr("SID %1 %2").arg(fix).arg(runwayTxt);
        else if(type & proc::PROCEDURE_STAR)
          header = tr("STAR %1 %2").arg(fix).arg(runwayTxt);
        else
          header = tr("Approach %1 %2 %3 %4").arg(proc::procedureType(procType)).
                   arg(recApp.valueStr("suffix")).arg(fix).arg(runwayTxt);

        html.h3(header, ahtml::UNDERLINE);

        // Fill table ==========================================================
        html.table();

        if(!(type& proc::PROCEDURE_SID) && !(type & proc::PROCEDURE_STAR))
          rowForBool(html, &recApp, "has_gps_overlay", tr("Has GPS Overlay"), false);

        addRadionavFixType(html, recApp);

        if(procType == "ILS" || procType == "LOC")
        {
          // Display ILS information ===========================================
          const atools::sql::SqlRecordVector *ilsRec =
            infoQuery->getIlsInformationSimByName(airport.ident, runwayIdent);
          if(ilsRec != nullptr && !ilsRec->isEmpty())
          {
            for(const atools::sql::SqlRecord& irec : *ilsRec)
              ilsText(&irec, html, true /* approach */, false /* standalone */);
          }
          else
            html.row2(tr("ILS data not found"));
        }
        else if(procType == "LOCB")
        {
          // Display backcourse ILS information ===========================================
          const QList<MapRunway> *runways = airportQueryNav->getRunways(airport.id);

          if(runways != nullptr)
          {
            // Find the opposite end
            QString backcourseEndIdent;
            for(const MapRunway& rw : *runways)
            {
              if(rw.primaryEndId == rwEndId)
              {
                backcourseEndIdent = rw.secondaryName;
                break;
              }
              else if(rw.secondaryEndId == rwEndId)
              {
                backcourseEndIdent = rw.primaryName;
                break;
              }
            }

            if(!backcourseEndIdent.isEmpty())
            {
              const atools::sql::SqlRecordVector *ilsRec =
                infoQuery->getIlsInformationSimByName(airport.ident, backcourseEndIdent);
              if(ilsRec != nullptr && !ilsRec->isEmpty())
              {
                for(const atools::sql::SqlRecord& irec : *ilsRec)
                  ilsText(&irec, html, true /* approach */, false /* standalone */);
              }
              else
                html.row2(tr("ILS data not found"));
            }
            else
              html.row2(tr("ILS data runway not found"));
          }
        }
        html.tableEnd();
#ifdef DEBUG_INFORMATION
        html.small(QString("Database: approach_id = %1").arg(recApp.valueInt("approach_id"))).br();
#endif

        const SqlRecordVector *recTransVector = infoQuery->getTransitionInformation(recApp.valueInt("approach_id"));
        if(recTransVector != nullptr)
        {
          // Transitions for this approach
          for(const SqlRecord& recTrans : *recTransVector)
          {
            if(!(type & proc::PROCEDURE_SID))
              html.h3(tr("Transition ") + recTrans.valueStr("fix_ident"));

            html.table();

            if(!(type & proc::PROCEDURE_SID))
            {
              if(recTrans.valueStr("type") == "F")
                html.row2(tr("Type:"), tr("Full"));
              else if(recTrans.valueStr("type") == "D")
                html.row2(tr("Type:"), tr("DME"));
            }

            // html.row2(tr("Fix Ident and Region:"), recTrans.valueStr("fix_ident") + tr(", ") +
            // recTrans.valueStr("fix_region"));

            // html.row2(tr("Altitude:"), Unit::altFeet(recTrans.valueFloat("altitude")));

            if(!recTrans.isNull("dme_ident"))
            {
              html.row2(tr("DME Ident and Region:"), recTrans.valueStr("dme_ident") + tr(", ") +
                        recTrans.valueStr("dme_region"));

              // rowForFloat(html, &recTrans, "dme_radial", tr("DME Radial:"), tr("%1"), 0);
              float dist = recTrans.valueFloat("dme_distance");
              if(dist > 1.f)
                html.row2(tr("DME Distance:"), Unit::distNm(dist, true /*addunit*/, 5));

              const atools::sql::SqlRecord vorReg =
                infoQuery->getVorByIdentAndRegion(recTrans.valueStr("dme_ident"),
                                                  recTrans.valueStr("dme_region"));

              if(!vorReg.isEmpty())
              {
                html.row2(tr("DME Type:"), map::navTypeNameVorLong(vorReg.valueStr("type")));
                if(vorReg.valueInt("frequency") > 0)
                  html.row2(tr("DME Frequency:"),
                            locale.toString(vorReg.valueInt("frequency") / 1000., 'f', 2) + tr(" MHz"));

                if(!vorReg.valueStr("channel").isEmpty())
                  html.row2(tr("DME Channel:"), vorReg.valueStr("channel"));
                html.row2(tr("DME Range:"), Unit::distNm(vorReg.valueInt("range")));
                addMorse(html, tr("DME Morse:"), vorReg.valueStr("ident"));
              }
              else
                html.row2(tr("DME data not found for %1/%2.").
                          arg(recTrans.valueStr("dme_ident")).arg(recTrans.valueStr("dme_region")));
            }

            addRadionavFixType(html, recTrans);
            html.tableEnd();
#ifdef DEBUG_INFORMATION
            html.small(QString("Database: transition_id = %1").arg(recTrans.valueInt("transition_id"))).br();
#endif
          }
        }
      }
    }
    else if(!print)
      html.p(tr("Airport has no approach."));
  }
}

void HtmlInfoBuilder::addRadionavFixType(HtmlBuilder& html, const SqlRecord& recApp) const
{
  QString fixType = recApp.valueStr("fix_type");

  if(fixType == "V" || fixType == "TV")
  {
    if(fixType == "V")
      html.row2(tr("Fix Type:"), tr("VOR"));
    else if(fixType == "TV")
      html.row2(tr("Fix Type:"), tr("Terminal VOR"));

    map::MapSearchResult result;

    mapQuery->getMapObjectByIdent(result, map::VOR, recApp.valueStr("fix_ident"), recApp.valueStr("fix_region"));

    if(result.hasVor())
    {
      const MapVor& vor = result.vors.first();

      if(vor.tacan)
      {
        html.row2(tr("TACAN Channel:"), QString(tr("%1 (%2 MHz)")).
                  arg(vor.channel).
                  arg(locale.toString(frequencyForTacanChannel(vor.channel) / 100.f, 'f', 2)));
        if(vor.range > 0)
          html.row2(tr("TACAN Range:"), Unit::distNm(vor.range));
      }
      else if(vor.vortac)
      {
        html.row2(tr("VORTAC Type:"), map::navTypeNameVorLong(vor.type));
        html.row2(tr("VORTAC Frequency:"), locale.toString(vor.frequency / 1000., 'f', 2) + tr(" MHz"));
        if(!vor.channel.isEmpty())
          html.row2(tr("VORTAC Channel:"), vor.channel);
        if(vor.range > 0)
          html.row2(tr("VORTAC Range:"), Unit::distNm(vor.range));
        addMorse(html, tr("VORTAC Morse:"), vor.ident);
      }
      else
      {
        html.row2(tr("VOR Type:"), map::navTypeNameVorLong(vor.type));
        html.row2(tr("VOR Frequency:"), locale.toString(vor.frequency / 1000., 'f', 2) + tr(" MHz"));
        if(vor.range > 0)
          html.row2(tr("VOR Range:"), Unit::distNm(vor.range));
        addMorse(html, tr("VOR Morse:"), vor.ident);
      }
    }
#ifdef DEBUG_INFORMATION
    else
      qWarning() << "VOR data not found";
#endif
  }
  else if(fixType == "N" || fixType == "TN")
  {
    if(fixType == "N")
      html.row2(tr("Fix Type:"), tr("NDB"));
    else if(fixType == "TN")
      html.row2(tr("Fix Type:"), tr("Terminal NDB"));

    map::MapSearchResult result;
    mapQuery->getMapObjectByIdent(result, map::NDB, recApp.valueStr("fix_ident"), recApp.valueStr("fix_region"));

    if(result.hasNdb())
    {
      const MapNdb& ndb = result.ndbs.first();

      if(!ndb.type.isEmpty())
        html.row2(tr("NDB Type:"), map::navTypeNameNdb(ndb.type));

      html.row2(tr("NDB Frequency:"), locale.toString(ndb.frequency / 100., 'f', 2) + tr(" kHz"));

      if(ndb.range > 0)
        html.row2(tr("NDB Range:"), Unit::distNm(ndb.range));

      addMorse(html, tr("NDB Morse:"), ndb.ident);
    }
#ifdef DEBUG_INFORMATION
    else
      qWarning() << "NDB data not found";
#endif
  }
}

static const Flags WEATHER_TITLE_FLAGS = ahtml::BOLD | ahtml::BIG;

void HtmlInfoBuilder::weatherText(const map::WeatherContext& context, const MapAirport& airport,
                                  HtmlBuilder& html) const
{
  if(info)
  {
    if(!print)
      airportTitle(airport, html, -1);

    optsw::FlagsWeather flags = OptionData::instance().getFlagsWeather();

    if(flags & optsw::WEATHER_INFO_ALL)
    {
      // Source for map icon display
      MapWeatherSource src = NavApp::getMapWeatherSource();
      bool weatherShown = NavApp::isMapWeatherShown();

      // Simconnect or X-Plane weather file metar ===========================
      if(context.fsMetar.isValid())
      {
        // Only FSX/P3D allow remote requests for now
        bool fsxP3d = context.fsMetar.simulator;

        const atools::fs::weather::MetarResult& metar = context.fsMetar;
        QString sim = tr("%1 ").arg(NavApp::getCurrentSimulatorShortName());

        if(!metar.metarForStation.isEmpty())
        {
          Metar met(metar.metarForStation, metar.requestIdent, metar.timestamp, true);

          html.p(tr("%1Station Weather").arg(sim), WEATHER_TITLE_FLAGS);
          decodedMetar(html, airport, map::MapAirport(), met, false /* interpolated */, fsxP3d,
                       src == WEATHER_SOURCE_SIMULATOR && weatherShown);
        }

        if(!metar.metarForNearest.isEmpty())
        {
          Metar met(metar.metarForNearest, metar.requestIdent, metar.timestamp, true);
          QString reportIcao = met.getParsedMetar().isValid() ? met.getParsedMetar().getId() : met.getStation();

          html.p(tr("%2Nearest Weather - %1").arg(reportIcao).arg(sim), WEATHER_TITLE_FLAGS);

          // Check if the station is an airport
          map::MapAirport reportAirport;
          airportQuerySim->getAirportByIdent(reportAirport, reportIcao);
          if(!print && reportAirport.isValid())
          {
            // Add link to airport
            html.nbsp().nbsp();
            html.a(tr("Map"), QString("lnm://show?id=%1&type=%2").arg(reportAirport.id).arg(map::AIRPORT),
                   ahtml::LINK_NO_UL);
          }

          decodedMetar(html, airport, reportAirport, met, false /* interpolated */, fsxP3d,
                       src == WEATHER_SOURCE_SIMULATOR && weatherShown);
        }

        if(!metar.metarForInterpolated.isEmpty())
        {
          Metar met(metar.metarForInterpolated, metar.requestIdent, metar.timestamp, fsxP3d);
          html.p(tr("%2Interpolated Weather - %1").arg(met.getStation()).arg(sim), WEATHER_TITLE_FLAGS);
          decodedMetar(html, airport, map::MapAirport(), met, true /* interpolated */, fsxP3d, false /* map src */);
        }
      } // if(context.fsMetar.isValid())
      else if(!print && OptionData::instance().getFlags() & optsw::WEATHER_INFO_FS)
        html.p(tr("Not connected to simulator."), ahtml::BOLD);

      // Active Sky metar ===========================
      if(!context.asMetar.isEmpty())
      {
        if(context.isAsDeparture && context.isAsDestination)
          html.p(context.asType + tr(" - Departure and Destination"), WEATHER_TITLE_FLAGS);
        else if(context.isAsDeparture)
          html.p(context.asType + tr(" - Departure"), WEATHER_TITLE_FLAGS);
        else if(context.isAsDestination)
          html.p(context.asType + tr(" - Destination"), WEATHER_TITLE_FLAGS);
        else
          html.p(context.asType, WEATHER_TITLE_FLAGS);

        decodedMetar(html, airport, map::MapAirport(), Metar(context.asMetar), false /* interpolated */,
                     false /* FSX/P3D */, src == WEATHER_SOURCE_ACTIVE_SKY && weatherShown);
      }

      // NOAA or nearest ===========================
      decodedMetars(html, context.noaaMetar, airport, tr("NOAA"), src == WEATHER_SOURCE_NOAA && weatherShown);

      // Vatsim metar ===========================
      decodedMetars(html, context.vatsimMetar, airport, tr("VATSIM"), src == WEATHER_SOURCE_VATSIM && weatherShown);

      // IVAO or nearest ===========================
      decodedMetars(html, context.ivaoMetar, airport, tr("IVAO"), src == WEATHER_SOURCE_IVAO && weatherShown);
    } // if(flags & optsw::WEATHER_INFO_ALL)
    else
      html.p().b(tr("No weather display selected in options dialog in page \"Weather\"."));
  } // if(info)
}

void HtmlInfoBuilder::decodedMetars(HtmlBuilder& html, const atools::fs::weather::MetarResult& metar,
                                    const map::MapAirport& airport, const QString& name, bool mapDisplay) const
{
  if(metar.isValid())
  {
    if(!metar.metarForStation.isEmpty())
    {
      html.p(tr("%1 Station Weather").arg(name), WEATHER_TITLE_FLAGS);
      decodedMetar(html, airport, map::MapAirport(),
                   Metar(metar.metarForStation, metar.requestIdent, metar.timestamp, true), false, false, mapDisplay);
    }

    if(!metar.metarForNearest.isEmpty())
    {
      Metar met(metar.metarForNearest, metar.requestIdent, metar.timestamp, true);
      QString reportIcao = met.getParsedMetar().isValid() ? met.getParsedMetar().getId() : met.getStation();

      html.p(tr("%1 Nearest Weather - %2").arg(name).arg(reportIcao), WEATHER_TITLE_FLAGS);

      // Check if the station is an airport
      map::MapAirport reportAirport;
      airportQuerySim->getAirportByIdent(reportAirport, reportIcao);
      if(!print && reportAirport.isValid())
      {
        // Add link to airport
        html.nbsp().nbsp();
        html.a(tr("Map"),
               QString("lnm://show?id=%1&type=%2").arg(reportAirport.id).arg(map::AIRPORT),
               ahtml::LINK_NO_UL);
      }

      decodedMetar(html, airport, reportAirport, met, false, false, mapDisplay);
    }
  }
}

void HtmlInfoBuilder::decodedMetar(HtmlBuilder& html, const map::MapAirport& airport,
                                   const map::MapAirport& reportAirport,
                                   const atools::fs::weather::Metar& metar, bool isInterpolated, bool isFsxP3d,
                                   bool mapDisplay) const
{
  using atools::fs::weather::INVALID_METAR_VALUE;

  const atools::fs::weather::MetarParser& parsed = metar.getParsedMetar();

  bool hasClouds = !parsed.getClouds().isEmpty() &&
                   parsed.getClouds().first().getCoverage() != atools::fs::weather::MetarCloud::COVERAGE_CLEAR;

  html.table();

  if(reportAirport.isValid())
  {
    html.row2(tr("Reporting airport: "), tr("%1 (%2), %3, %4").
              arg(reportAirport.name).
              arg(reportAirport.ident).
              arg(Unit::altFeet(reportAirport.position.getAltitude())).
              arg(Unit::distMeter(reportAirport.position.distanceMeterTo(airport.position))));
  }

  // Time and date =============================================================
  if(!isInterpolated && !isFsxP3d)
  {
    QDateTime time;
    time.setOffsetFromUtc(0);
    time.setDate(QDate(parsed.getYear(), parsed.getMonth(), parsed.getDay()));
    time.setTime(QTime(parsed.getHour(), parsed.getMinute()));

    QDateTime oldUtc = QDateTime::currentDateTimeUtc().addSecs(-3600 * WEATHER_MAX_AGE_HOURS);
    ahtml::Flags flags = ahtml::NONE;
    QColor color;
    if(time < oldUtc)
    {
      flags |= ahtml::BOLD;
      color = Qt::red;
    }
    html.row2(tr("Time: "), locale.toString(time, QLocale::ShortFormat) + " " + time.timeZoneAbbreviation(),
              flags, color);
  }

  if(!parsed.getReportTypeString().isEmpty())
    html.row2(tr("Report type: "), parsed.getReportTypeString());

  HtmlBuilder flightRulesHtml = html.cleared();
  if(mapDisplay)
    flightRulesHtml.b();
  addFlightRulesSuffix(flightRulesHtml, metar, mapDisplay);
  if(mapDisplay)
    flightRulesHtml.bEnd();
  if(!flightRulesHtml.isEmpty())
    html.row2(tr("Flight Rules:"), flightRulesHtml);

  bool hasWind = false;
  float windSpeedKts = parsed.getWindSpeedKts();
  // Wind =============================================================
  if(windSpeedKts > 0.f)
  {
    QString windDir, windVar;

    if(parsed.getWindDir() >= 0)
      // Wind direction given
      windDir = courseTextFromTrue(parsed.getWindDir(), airport.magvar, false) + tr(", ");

    if(parsed.getWindRangeFrom() != -1 && parsed.getWindRangeTo() != -1)
      // Wind direction range given (additionally to dir in some cases)
      windVar = tr(", variable ") + courseTextFromTrue(parsed.getWindRangeFrom(), airport.magvar) +
                tr(" to ") + courseTextFromTrue(parsed.getWindRangeTo(), airport.magvar);
    else if(parsed.getWindDir() == -1)
      windDir = tr("Variable, ");

    if(windSpeedKts < INVALID_METAR_VALUE)
    {
      QString windSpeedStr;
      if(windSpeedKts < INVALID_METAR_VALUE)
        windSpeedStr = Unit::speedKts(windSpeedKts);

      html.row2(tr("Wind:"), windDir + windSpeedStr + windVar, ahtml::NO_ENTITIES);
    }
    else
      html.row2(tr("Wind:"), windDir + tr("Speed not valid") + windVar, ahtml::NO_ENTITIES);

    hasWind = true;
  }

  if(parsed.getGustSpeedKts() < INVALID_METAR_VALUE)
  {
    hasWind = true;
    html.row2(tr("Wind gusts:"), Unit::speedKts(parsed.getGustSpeedKts()));
  }

  // Temperature  =============================================================
  float temp = parsed.getTemperatureC();
  if(temp < INVALID_METAR_VALUE)
    html.row2(tr("Temperature:"), locale.toString(atools::roundToInt(temp)) + tr(" °C, ") +
              locale.toString(atools::roundToInt(ageo::degCToDegF(temp))) + tr(" °F"));

  temp = parsed.getDewpointDegC();
  if(temp < INVALID_METAR_VALUE)
    html.row2(tr("Dewpoint:"), locale.toString(atools::roundToInt(temp)) + tr(" °C, ") +
              locale.toString(atools::roundToInt(ageo::degCToDegF(temp))) + tr(" °F"));

  // Pressure  =============================================================
  float slp = parsed.getPressureMbar();
  if(slp < INVALID_METAR_VALUE)
    html.row2(tr("Pressure:"), locale.toString(slp, 'f', 0) + tr(" hPa, ") +
              locale.toString(ageo::mbarToInHg(slp), 'f', 2) + tr(" inHg"));

  // Visibility =============================================================
  const atools::fs::weather::MetarVisibility& minVis = parsed.getMinVisibility();
  QString visiblity;
  if(minVis.getVisibilityMeter() < INVALID_METAR_VALUE)
    visiblity.append(tr("%1 %2").
                     arg(minVis.getModifierString()).
                     arg(Unit::distMeter(minVis.getVisibilityMeter())));

  const atools::fs::weather::MetarVisibility& maxVis = parsed.getMaxVisibility();
  if(maxVis.getVisibilityMeter() < INVALID_METAR_VALUE)
    visiblity.append(tr(" %1 %2").
                     arg(maxVis.getModifierString()).
                     arg(Unit::distMeter(maxVis.getVisibilityMeter())));

  if(!visiblity.isEmpty())
    html.row2(tr("Visibility: "), visiblity);
  else
    html.row2(tr("No visibility report"));

  if(!hasClouds)
    html.row2(tr("No clouds"));

  if(!hasWind)
    html.row2(tr("No wind"));

  // Other conditions =============================================================
  const QStringList& weather = parsed.getWeather();
  if(!weather.isEmpty())
  {
    // Workaround for goofy texts
    QString wtr = weather.join(", ").replace(tr(" of in "), tr(" in "), Qt::CaseInsensitive);

    if(!wtr.isEmpty())
      wtr[0] = wtr.at(0).toUpper();

    html.row2(tr("Conditions:"), wtr);
  }

  html.tableEnd();

  if(hasClouds)
    html.p(tr("Clouds"), ahtml::BOLD);

  html.table();
  if(hasClouds)
  {
    for(const atools::fs::weather::MetarCloud& cloud : parsed.getClouds())
    {
      float altMeter = cloud.getAltitudeMeter();
      QString altStr;

      if(altMeter < INVALID_METAR_VALUE && cloud.getCoverage() != atools::fs::weather::MetarCloud::COVERAGE_CLEAR)
        altStr = Unit::altMeter(altMeter);

      html.row2(cloud.getCoverageString(), altStr);
    }
  }
  html.tableEnd();

  if(parsed.getCavok())
    html.p().text(tr("CAVOK:"), ahtml::BOLD).br().
    text(tr("No cloud below 5,000 ft (1,500 m), visibility of 10 km (6 nm) or more")).pEnd();

  if(!metar.getParsedMetar().getRemark().isEmpty())
    html.p().text(tr("Remarks:"), ahtml::BOLD).br().
    text(metar.getParsedMetar().getRemark()).pEnd();

  if(!parsed.isValid())
  {
    // Print error report if invalid
    html.p(tr("Report is not valid. Raw and clean METAR were:"), ahtml::BOLD, Qt::red);
    html.pre(metar.getMetar());
    if(metar.getMetar() != metar.getCleanMetar())
      html.pre(metar.getCleanMetar());
  }

  if(!parsed.getUnusedData().isEmpty())
    html.p().text(tr("Additional information:"), ahtml::BOLD).br().text(parsed.getUnusedData()).pEnd();

  // bestRunwaysText(airport, html, windSpeedKts, parsed.getWindDir(), 8 /* max entries */, true /* details */);
  bestRunwaysText(airport, html, parsed, 8 /* max entries */, true /* details */);

#ifdef DEBUG_INFORMATION
  html.p().small(tr("Source: %1").arg(metar.getMetar())).br();
#endif
}

void HtmlInfoBuilder::vorText(const MapVor& vor, HtmlBuilder& html) const
{
  const SqlRecord *rec = nullptr;
  if(info && infoQuery != nullptr)
    rec = infoQuery->getVorInformation(vor.id);

  QIcon icon = SymbolPainter().createVorIcon(vor, symbolSizeTitle.height());
  html.img(icon, QString(), QString(), symbolSizeTitle);
  html.nbsp().nbsp();

  QString type = map::vorType(vor);
  navaidTitle(html, type + ": " + capString(vor.name) + " (" + vor.ident + ")");

  if(info)
  {
    // Add map link if not tooltip
    html.nbsp().nbsp();
    html.a(tr("Map"), QString("lnm://show?lonx=%1&laty=%2").
           arg(vor.position.getLonX()).arg(vor.position.getLatY()), ahtml::LINK_NO_UL);
  }

  html.table();
  if(!info && vor.routeIndex >= 0)
  {
    html.row2(tr("Flight Plan Position:"), locale.toString(vor.routeIndex + 1));
    flightplanWaypointRemarks(html, vor.routeIndex);
    routeWindText(html, NavApp::getRouteConst(), vor.routeIndex);
  }

  // Add bearing/distance to table
  bearingText(vor.position, vor.magvar, html);

  if(vor.tacan)
  {
    if(vor.dmeOnly)
      html.row2(tr("Type:"), tr("DME only"));
  }
  else
    html.row2(tr("Type:"), map::navTypeNameVorLong(vor.type));

  if(rec != nullptr && !rec->isNull("airport_id"))
    airportRow(airportQueryNav->getAirportById(rec->valueInt("airport_id")), html);

  html.row2If(tr("Region:"), vor.region);

  if(!vor.tacan)
    html.row2(tr("Frequency:"), locale.toString(vor.frequency / 1000., 'f', 2) + tr(" MHz"));

  if(vor.vortac && !vor.channel.isEmpty())
    html.row2(tr("Channel:"), vor.channel);
  else if(vor.tacan)
    html.row2(tr("Channel:"), QString(tr("%1 (%2 MHz)")).
              arg(vor.channel).
              arg(locale.toString(frequencyForTacanChannel(vor.channel) / 100.f, 'f', 2)));

  if(!vor.tacan && !vor.dmeOnly)
    html.row2(tr("Magnetic declination:"), map::magvarText(vor.magvar));

  if(vor.getPosition().getAltitude() < INVALID_ALTITUDE_VALUE)
    html.row2(tr("Elevation:"), Unit::altFeet(vor.getPosition().getAltitude()));
  html.row2(tr("Range:"), Unit::distNm(vor.range));
  addMorse(html, tr("Morse:"), vor.ident);
  addCoordinates(rec, html);
  html.tableEnd();

  if(rec != nullptr)
    addScenery(rec, html);

#ifdef DEBUG_INFORMATION
  html.small(QString("Database: vor_id = %1").arg(vor.getId())).br();
#endif

  if(info)
    html.br();
}

void HtmlInfoBuilder::ndbText(const MapNdb& ndb, HtmlBuilder& html) const
{
  const SqlRecord *rec = nullptr;
  if(info && infoQuery != nullptr)
    rec = infoQuery->getNdbInformation(ndb.id);

  QIcon icon = SymbolPainter().createNdbIcon(symbolSizeTitle.height());
  html.img(icon, QString(), QString(), symbolSizeTitle);
  html.nbsp().nbsp();

  navaidTitle(html, tr("NDB: ") + capString(ndb.name) + " (" + ndb.ident + ")");

  if(info)
  {
    // Add map link if not tooltip
    html.nbsp().nbsp();
    html.a(tr("Map"), QString("lnm://show?lonx=%1&laty=%2").
           arg(ndb.position.getLonX()).arg(ndb.position.getLatY()), ahtml::LINK_NO_UL);
  }

  html.table();
  if(!info && ndb.routeIndex >= 0)
  {
    html.row2(tr("Flight Plan Position "), locale.toString(ndb.routeIndex + 1));
    flightplanWaypointRemarks(html, ndb.routeIndex);
    routeWindText(html, NavApp::getRouteConst(), ndb.routeIndex);
  }

  // Add bearing/distance to table
  bearingText(ndb.position, ndb.magvar, html);

  if(!ndb.type.isEmpty())
    html.row2(tr("Type:"), map::navTypeNameNdb(ndb.type));

  if(rec != nullptr && !rec->isNull("airport_id"))
    airportRow(airportQueryNav->getAirportById(rec->valueInt("airport_id")), html);

  html.row2If(tr("Region:"), ndb.region);
  html.row2(tr("Frequency:"), locale.toString(ndb.frequency / 100., 'f', 1) + tr(" kHz"));
  html.row2(tr("Magnetic declination:"), map::magvarText(ndb.magvar));
  if(ndb.getPosition().getAltitude() < INVALID_ALTITUDE_VALUE)
    html.row2(tr("Elevation:"), Unit::altFeet(ndb.getPosition().getAltitude()));
  if(ndb.range > 0)
    html.row2(tr("Range:"), Unit::distNm(ndb.range));
  addMorse(html, tr("Morse:"), ndb.ident);
  addCoordinates(rec, html);
  html.tableEnd();

  if(rec != nullptr)
    addScenery(rec, html);

#ifdef DEBUG_INFORMATION
  html.small(QString("Database: ndb_id = %1").arg(ndb.getId())).br();
#endif

  if(info)
    html.br();
}

void HtmlInfoBuilder::holdText(const Hold& hold, HtmlBuilder& html) const
{
  html.b(tr("Holding"));

  QString navType;
  if(hold.navType == map::AIRPORT)
    navType = tr("Airport");
  else if(hold.navType == map::VOR)
    navType = map::vorType(hold.vorDmeOnly, hold.vorHasDme, hold.vorTacan, hold.vorVortac);
  else if(hold.navType == map::NDB)
    navType = tr("NDB");
  else if(hold.navType == map::WAYPOINT)
    navType = tr("Waypoint");

  if(!navType.isEmpty())
    // Hold has a navaid
    html.brText(tr("%1 %2, %3 at %4").
                arg(navType).
                arg(hold.navIdent).
                arg(courseTextFromTrue(hold.courseTrue, hold.magvar)).
                arg(Unit::altFeet(hold.position.getAltitude())), ahtml::NO_ENTITIES);
  else
    // Hold at a position
    html.brText(tr("%1 at %2").
                arg(courseTextFromTrue(hold.courseTrue, hold.magvar)).
                arg(Unit::altFeet(hold.position.getAltitude())), ahtml::NO_ENTITIES);

  QString minTxt = locale.toString(hold.minutes);
  html.brText(tr("%1, %2, %3 %4").
              arg(Unit::distNm(hold.distance())).
              arg(Unit::speedKts(hold.speedKts)).
              arg(minTxt).
              arg(minTxt == "1" ? tr("minute") : tr("minutes")));

  html.br();
}

void HtmlInfoBuilder::rangeMarkerText(const RangeMarker& marker, atools::util::HtmlBuilder& html) const
{
  html.b(marker.ranges.size() > 1 ? tr("Range Rings") : tr("Range Ring"));
  if(!marker.text.isEmpty())
    html.brText(marker.text);

  if(marker.ranges.isEmpty() || (marker.ranges.size() == 1 && marker.ranges.first() == 0))
    html.brText(tr("No distance"));
  else
  {
    QStringList distStr;
    for(int dist : marker.ranges)
      distStr.append(Unit::distNm(dist, false));

    html.brText((marker.ranges.size() > 1 ? tr("Distances: %1 %2") : tr("Distance: %1 %2")).
                arg(distStr.join(tr(", "))).arg(Unit::getUnitDistStr()));
  }

  html.br();
}

void HtmlInfoBuilder::trafficPatternText(const TrafficPattern& pattern, atools::util::HtmlBuilder& html) const
{
  html.b(tr("Traffic Pattern"));
  html.brText(tr("Airport %1, runway %2").arg(pattern.airportIcao).arg(pattern.runwayName));
  html.brText(tr("Heading at final %1").arg(courseTextFromTrue(pattern.courseTrue, pattern.magvar)),
              ahtml::NO_ENTITIES);
  html.brText(tr("Pattern altitude %1").arg(Unit::altFeet(pattern.position.getAltitude())));
  html.br();
}

bool HtmlInfoBuilder::userpointText(MapUserpoint userpoint, HtmlBuilder& html) const
{
  if(!userpoint.isValid())
    return false;

  atools::sql::SqlRecord rec = NavApp::getUserdataManager()->getRecord(userpoint.id);

  // Update the structure since it might not have the latest changes
  userpoint = NavApp::getUserdataController()->getUserpointById(userpoint.id);

  // Check if userpoint still exists since it can be deleted in the background
  if(!rec.isEmpty() && userpoint.isValid())
  {
    QIcon icon(NavApp::getUserdataIcons()->getIconPath(userpoint.type));
    html.img(icon, QString(), QString(), symbolSizeTitle);
    html.nbsp().nbsp();

    navaidTitle(html, tr("Userpoint%1").arg(userpoint.temp ? tr(" (Temporary)") : QString()));

    if(info)
    {
      // Add map link if not tooltip
      html.nbsp().nbsp();
      html.a(tr("Map"), QString("lnm://show?lonx=%1&laty=%2").
             arg(userpoint.position.getLonX()).arg(userpoint.position.getLatY()), ahtml::LINK_NO_UL);
    }

    html.table();
    // Add bearing/distance to table
    bearingText(userpoint.position, NavApp::getMagVar(userpoint.position), html);

    // Be cautious with user defined data and adapt it for HTML display
    html.row2If(tr("Type:"), userpoint.type, ahtml::REPLACE_CRLF);
    html.row2If(tr("Ident:"), userpoint.ident, ahtml::REPLACE_CRLF);
    html.row2If(tr("Region:"), userpoint.region, ahtml::REPLACE_CRLF);
    html.row2If(tr("Name:"), userpoint.name, ahtml::REPLACE_CRLF);

    html.row2If(tr("Tags:"), userpoint.tags, ahtml::REPLACE_CRLF);
    if(!rec.isNull("altitude"))
      html.row2If(tr("Elevation:"), Unit::altFeet(rec.valueFloat("altitude")));

    if(info)
    {
      html.row2(tr("Magnetic declination:"), map::magvarText(NavApp::getMagVar(userpoint.position)));

      if(!rec.isNull("visible_from"))
        html.row2If(tr("Visible from:"), Unit::distNm(rec.valueFloat("visible_from")));

      html.row2(tr("Last Change:"), locale.toString(rec.value("last_edit_timestamp").toDateTime()));
    }
    if(info)
      addCoordinates(userpoint.position, html);
    html.tableEnd();

    descriptionText(userpoint.description, html);

    if(info && !rec.isNull("import_file_path"))
    {
      head(html, tr("File"));
      html.table();
      html.row2If(tr("Imported from:"), filepathTextShow(rec.valueStr("import_file_path")),
                  ahtml::NO_ENTITIES | ahtml::SMALL);
      html.tableEnd();
    }

#ifdef DEBUG_INFORMATION
    html.small(QString("Database: uerpoint_id = %1").arg(userpoint.getId())).br();
#endif
    return true;
  }
  else
  {
    qWarning() << Q_FUNC_INFO << "Empty record";
    return false;
  }
}

bool HtmlInfoBuilder::logEntryText(MapLogbookEntry logEntry, HtmlBuilder& html) const
{
  if(!logEntry.isValid())
    return false;

  LogdataController *logdataController = NavApp::getLogdataController();
  atools::sql::SqlRecord rec = logdataController->getLogEntryRecordById(logEntry.id);

  // Update the structure since it might not have the latest changes
  logEntry = logdataController->getLogEntryById(logEntry.id);

  if(!rec.isEmpty() && logEntry.isValid())
  {
    html.img(QIcon(":/littlenavmap/resources/icons/logbook.svg"), QString(), QString(), symbolSizeTitle);
    html.nbsp().nbsp();

    navaidTitle(html, tr("Logbook Entry: %1 to %2").
                arg(logEntry.departureIdent.isEmpty() ? tr("-") : logEntry.departureIdent).
                arg(logEntry.destinationIdent.isEmpty() ? tr("-") : logEntry.destinationIdent));

    // From/to ================================================================
    html.table();
    html.row2(tr("From:"),
              airportLink(html, logEntry.departureIdent, logEntry.departureName) +
              (logEntry.departureIdent.isEmpty() ? QString() :
               tr(", %1").arg(Unit::altFeet(rec.valueFloat("departure_alt")))),
              ahtml::NO_ENTITIES | ahtml::BOLD);
    html.row2(tr("To:"),
              airportLink(html, logEntry.destinationIdent, logEntry.destinationName) +
              (logEntry.destinationIdent.isEmpty() ? QString() :
               tr(", %1").arg(Unit::altFeet(rec.valueFloat("destination_alt")))),
              ahtml::NO_ENTITIES | ahtml::BOLD);

    if(info)
    {
      html.tableEnd();
      html.br();

      // Aircraft ================================================================
      html.table();
    }
    if(rec.valueStr("aircraft_name").isEmpty())
      html.row2If(tr("Aircraft type:"), rec.valueStr("aircraft_type"));
    else if(rec.valueStr("aircraft_type").isEmpty())
      html.row2If(tr("Aircraft model:"), rec.valueStr("aircraft_name"));
    else
      html.row2If(tr("Aircraft model and type:"), tr("%1, %2").
                  arg(rec.valueStr("aircraft_name")).arg(rec.valueStr("aircraft_type")));
    html.row2If(tr("Aircraft registration:"), rec.valueStr("aircraft_registration"));
    html.row2If(tr("Flight number:"), rec.valueStr("flightplan_number"));
    html.row2If(tr("Simulator:"), rec.valueStr("simulator"));

    if(info)
    {
      html.row2If(tr("Route Description:"), rec.valueStr("route_string"));
      html.tableEnd();

      // Flight =======================================================
      html.p(tr("Flight"), ahtml::BOLD);
      html.table();
    }

    if(rec.valueFloat("flightplan_cruise_altitude") > 0.f)
      html.row2(tr("Flight plan cruise altitude:"), Unit::altFeet(rec.valueFloat("flightplan_cruise_altitude")));

    html.row2(tr("Flight plan distance:"), Unit::distNm(rec.valueFloat("distance")));

    if(info)
    {
      if(rec.valueFloat("distance_flown") > 0.f)
        html.row2(tr("Distance flown:"), Unit::distNm(rec.valueFloat("distance_flown")));

      ageo::LineString line = logEntry.lineString();
      if(line.isValid())
        html.row2(tr("Great circle distance:"), Unit::distMeter(line.lengthMeter()));
    }

    float travelTimeHours = (rec.valueDateTime("destination_time_sim").toSecsSinceEpoch() -
                             rec.valueDateTime("departure_time_sim").toSecsSinceEpoch()) / 3600.f;

    FuelTool ft(rec.valueBool("is_jetfuel"), false /* fuel is in lbs */);
    float usedFuel = rec.valueFloat("used_fuel");
    float grossWeight = rec.valueFloat("grossweight");
    if(travelTimeHours > 0.01)
    {
      html.row2(tr("Travel time:"), formatter::formatMinutesHoursLong(travelTimeHours));
      if(info)
      {
        if(rec.valueFloat("distance_flown") > 0.f)
          html.row2(tr("Average ground speed:"), Unit::speedKts(rec.valueFloat("distance_flown") / travelTimeHours));

        if(usedFuel > 0.f)
          html.row2(tr("Average fuel flow:"), ft.flowWeightVolLocal(usedFuel / travelTimeHours));
      }
    }

    if(info)
    {
      html.tableEnd();

      // Departure =======================================================
      html.p(tr("Departure (%1)").
             arg(logEntry.departureIdent.isEmpty() ? tr("-") : logEntry.departureIdent), ahtml::BOLD);
      html.table();

      QDateTime departTime = rec.valueDateTime("departure_time");
      if(departTime.isValid())
      {
        QString departTimeZone = departTime.timeZoneAbbreviation();
        html.row2If(tr("Runway:"), rec.valueStr("departure_runway"));
        html.row2If(tr("Real time:"),
                    tr("%1 %2").arg(locale.toString(departTime, QLocale::ShortFormat)).arg(departTimeZone));
      }
    }
    QDateTime departTimeSim = rec.valueDateTime("departure_time_sim");
    if(departTimeSim.isValid())
    {
      QString departTimeZoneSim = departTimeSim.timeZoneAbbreviation();
      html.row2If(info ? tr("Simulator time:") : tr("Departure time:"),
                  tr("%1 %2").arg(locale.toString(departTimeSim, QLocale::ShortFormat)).arg(departTimeZoneSim));
    }
    if(info)
    {
      if(grossWeight > 0.f)
        html.row2(tr("Gross Weight:"), Unit::weightLbs(grossWeight), ahtml::NO_ENTITIES);
      addCoordinates(Pos(rec.valueFloat("departure_lonx"),
                         rec.valueFloat("departure_laty"),
                         rec.valueFloat("departure_alt", 0.f)), html);
      html.tableEnd();

      // Destination =======================================================
      html.p(tr("Destination (%1)").
             arg(logEntry.destinationIdent.isEmpty() ? tr("-") : logEntry.destinationIdent), ahtml::BOLD);
      html.table();

      html.row2If(tr("Runway:"), rec.valueStr("destination_runway"));
      QDateTime destTime = rec.valueDateTime("destination_time");
      if(destTime.isValid())
      {
        QString destTimeZone = destTime.timeZoneAbbreviation();
        html.row2If(tr("Real time:"),
                    tr("%1 %2").arg(locale.toString(destTime, QLocale::ShortFormat)).arg(destTimeZone));
      }

      QDateTime destTimeSim = rec.valueDateTime("destination_time_sim");
      if(destTimeSim.isValid())
      {
        QString destTimeZoneSim = destTimeSim.timeZoneAbbreviation();
        html.row2If(tr("Simulator time:"),
                    tr("%1 %2").arg(locale.toString(destTimeSim, QLocale::ShortFormat)).arg(destTimeZoneSim));
      }

      if(grossWeight > 0.f)
        html.row2(tr("Gross Weight:"), Unit::weightLbs(grossWeight - usedFuel), ahtml::NO_ENTITIES);

      addCoordinates(Pos(rec.valueFloat("destination_lonx"),
                         rec.valueFloat("destination_laty"),
                         rec.valueFloat("destination_alt", 0.f)), html);
      html.tableEnd();

      // Fuel =======================================================
      if(rec.valueFloat("trip_fuel") > 0.f || rec.valueFloat("block_fuel") > 0.f || usedFuel > 0.f)
      {
        html.p(tr("Fuel"), ahtml::BOLD);
        html.table();
        ahtml::Flags flags = ahtml::ALIGN_RIGHT;
        html.row2(tr("Type:"), ft.getFuelTypeString());
        html.row2(tr("Trip from plan:"), ft.weightVolLocal(rec.valueFloat("trip_fuel")), flags);
        html.row2(tr("Block from plan:"), ft.weightVolLocal(rec.valueFloat("block_fuel")), flags);
        html.row2(tr("Used from takeoff to landing:"), ft.weightVolLocal(usedFuel), flags);
        html.tableEnd();
      }

      // Files =======================================================
      bool perf = logdataController->hasPerfAttached(logEntry.id);
      bool route = logdataController->hasRouteAttached(logEntry.id);
      bool track = logdataController->hasTrackAttached(logEntry.id);

      if(!rec.valueStr("flightplan_file").isEmpty() || !rec.valueStr("performance_file").isEmpty() ||
         perf || track || route)
      {
        html.p(tr("Files"), ahtml::BOLD);
        html.table();
        html.row2If(tr("Flight plan:"),
                    atools::strJoin({(route ? tr("Attached") : QString()),
                                     filepathTextShow(rec.valueStr("flightplan_file"), tr("Referenced: "))},
                                    tr("<br/>")),
                    ahtml::NO_ENTITIES | ahtml::SMALL);
        html.row2If(tr("Aircraft performance:"),
                    atools::strJoin({(route ? tr("Attached") : QString()),
                                     filepathTextShow(rec.valueStr("performance_file"), tr("Referenced: "))},
                                    tr("<br/>")),
                    ahtml::NO_ENTITIES | ahtml::SMALL);
        html.row2If(tr("Aircraft trail:"), route ? tr("Attached") : QString(), ahtml::NO_ENTITIES | ahtml::SMALL);
        html.tableEnd();
      }

      // Description =======================================================
      descriptionText(logEntry.description, html);
    } // if(info)
    html.tableEnd();

#ifdef DEBUG_INFORMATION
    html.small(QString("Database: logbook_id = %1").arg(logEntry.id)).br();
#endif

    return true;
  }
  else
  {
    qWarning() << Q_FUNC_INFO << "Empty record";
    return false;
  }
}

void HtmlInfoBuilder::descriptionText(const QString& description, HtmlBuilder& html) const
{
  if(!description.isEmpty())
  {
    html.p().b(tr("Description")).pEnd();
    if(info)
      html.table(1, 2, 0, 100, html.getRowBackColor());
    else
      html.table();
    html.tr().td(atools::elideTextLinesShort(description, info ? 200 : 4),
                 (info ? ahtml::AUTOLINK : ahtml::NONE)).trEnd();
    html.tableEnd();
  }
}

void HtmlInfoBuilder::airportRow(const map::MapAirport& ap, HtmlBuilder& html) const
{
  if(ap.isValid())
  {
    map::MapAirport apSim = mapQuery->getAirportSim(ap);
    if(apSim.isValid())
    {
      HtmlBuilder apHtml = html.cleared();
      apHtml.a(apSim.ident, QString("lnm://show?airport=%1").arg(apSim.ident), ahtml::LINK_NO_UL);
      html.row2(tr("Airport:"), apHtml.getHtml(), ahtml::NO_ENTITIES);
    }
  }
}

void HtmlInfoBuilder::waypointText(const MapWaypoint& waypoint, HtmlBuilder& html) const
{
  const SqlRecord *rec = nullptr;

  if(info && infoQuery != nullptr)
    rec = waypointQuery->getWaypointInformation(waypoint.id);

  QIcon icon = SymbolPainter().createWaypointIcon(symbolSizeTitle.height());
  html.img(icon, QString(), QString(), symbolSizeTitle);
  html.nbsp().nbsp();

  navaidTitle(html, tr("Waypoint: ") + waypoint.ident);

  if(info)
  {
    // Add map link if not tooltip
    html.nbsp().nbsp();
    html.a(tr("Map"), QString("lnm://show?lonx=%1&laty=%2").
           arg(waypoint.position.getLonX()).arg(waypoint.position.getLatY()), ahtml::LINK_NO_UL);
  }

  html.table();
  if(!info && waypoint.routeIndex >= 0)
  {
    html.row2(tr("Flight Plan Position:"), locale.toString(waypoint.routeIndex + 1));
    flightplanWaypointRemarks(html, waypoint.routeIndex);
    routeWindText(html, NavApp::getRouteConst(), waypoint.routeIndex);
  }

  // Add bearing/distance to table
  bearingText(waypoint.position, waypoint.magvar, html);

  if(info)
  {
    html.row2(tr("Type:"), map::navTypeNameWaypoint(waypoint.type));
    if(rec != nullptr && rec->contains("airport_id") && !rec->isNull("airport_id"))
      airportRow(airportQueryNav->getAirportById(rec->valueInt("airport_id")), html);
  }
  html.row2If(tr("Region:"), waypoint.region);
  html.row2(tr("Magnetic declination:"), map::magvarText(waypoint.magvar));
  addCoordinates(rec, html);

  html.tableEnd();

  QList<MapAirway> airways;
  NavApp::getAirwayTrackQuery()->getAirwaysForWaypoint(airways, waypoint.id);

  if(!airways.isEmpty())
  {
    // Add airway information

    // Add airway name/text to vector
    QVector<QStringList> airwayTexts;
    for(const MapAirway& aw : airways)
    {
      QString txt(map::airwayTrackTypeToString(aw.type));

      QString altTxt = map::airwayAltText(aw);

      if(!altTxt.isEmpty())
        txt.append(tr(", ") + altTxt);

      airwayTexts.append({aw.isAirway() ? tr("Airway") : tr("Track"), aw.name, txt});
    }

    if(!airwayTexts.isEmpty())
    {
      // Sort airways by name
      std::sort(airwayTexts.begin(), airwayTexts.end(),
                [](const QStringList& item1, const QStringList& item2)
      {
        return item1.first() == item2.first() ? item1.at(1) < item2.at(1) : item1.first() < item2.first();
      });

      // Remove duplicates
      airwayTexts.erase(std::unique(airwayTexts.begin(), airwayTexts.end()), airwayTexts.end());

      if(info)
        head(html, tr("Connections:"));
      else
        html.br().b(tr("Connections: "));

      html.table();
      for(const QStringList& aw : airwayTexts)
        html.row2(aw.at(0), aw.at(1) + " " + aw.at(2));
      html.tableEnd();
    }
  }

  if(rec != nullptr)
    addScenery(rec, html);

#ifdef DEBUG_INFORMATION
  html.small(QString("Database: waypoint_id = %1").arg(waypoint.getId())).br();
#endif

  if(info)
    html.br();

}

void HtmlInfoBuilder::bearingText(const ageo::Pos& pos, float magVar, HtmlBuilder& html) const
{
  const atools::fs::sc::SimConnectUserAircraft& userAircraft = NavApp::getUserAircraft();

  float distance = pos.distanceMeterTo(userAircraft.getPosition());
  if(NavApp::isConnectedAndAircraft() && distance < MAX_DISTANCE_FOR_BEARING_METER)
  {
    html.row2(tr("Bearing and distance to user:"),
              tr("%1, %2").
              arg(courseTextFromTrue(normalizeCourse(userAircraft.getPosition().angleDegTo(pos)), magVar)).
              arg(Unit::distMeter(distance)),
              ahtml::NO_ENTITIES);
  }
}

void HtmlInfoBuilder::airspaceText(const MapAirspace& airspace, const atools::sql::SqlRecord& onlineRec,
                                   HtmlBuilder& html) const
{
  QIcon icon = SymbolPainter().createAirspaceIcon(airspace, symbolSizeTitle.height());
  html.img(icon, QString(), QString(), symbolSizeTitle);
  html.nbsp().nbsp();

  QString suffix;
  if(airspace.isOnline())
  {
    if(!NavApp::getOnlineNetworkTranslated().isEmpty())
      suffix = tr(" (%1)").arg(NavApp::getOnlineNetworkTranslated());
  }

  if(airspace.name.isEmpty())
    navaidTitle(html, tr("Airspace") + suffix);
  else
  {
    // Do not capitalize online network center names
    QString name = airspace.isOnline() ? airspace.name : formatter::capNavString(airspace.name);
    if(!info)
      name = atools::elideTextShort(name, 40);

    if((NavApp::isNavdataAll() || NavApp::isNavdataMixed()) && !airspace.multipleCode.trimmed().isEmpty() &&
       !airspace.isOnline())
      // Add multiple code as suffix to indicate overlapping duplicates
      suffix = tr(" (%1)").arg(airspace.multipleCode);

    navaidTitle(html, ((info && !airspace.isOnline()) ? tr("Airspace: ") : QString()) + name + suffix);
  }

  if(info)
  {
    // Add map link if not tooltip
    html.nbsp().nbsp();
    html.a(tr("Map"),
           QString("lnm://show?id=%1&type=%2&source=%3").
           arg(airspace.id).
           arg(map::AIRSPACE).
           arg(airspace.src),
           ahtml::LINK_NO_UL);
  }

  QStringList header;

  if(info)
  {
    const QString remark = map::airspaceRemark(airspace.type);
    if(!remark.isEmpty())
      header.append(remark);

    if((NavApp::isNavdataAll() || NavApp::isNavdataMixed()) && airspace.timeCode != 'U' && !airspace.isOnline())
    {
      // Add comment about active times if navdata airspace
      QString msg;

      // if(airspace.timeCode == "C")
      // msg = tr("Active continuously, including holidays");
      if(airspace.timeCode == "H")
        msg = tr("Active continuously, excluding holidays");
      else if(airspace.timeCode == "N")
        msg = tr("Active not continuously");
      else if(airspace.timeCode.trimmed().isEmpty())
        msg = tr("Active times announced by NOTAM");
      if(!msg.isEmpty())
        header.append(msg);
    }
  }

  if(!header.isEmpty())
    html.p(header.join("\n"));

  html.table();

  if(!airspace.restrictiveDesignation.isEmpty())
  {
    QString restrictedName = airspace.restrictiveType + "-" + airspace.restrictiveDesignation;
    if(restrictedName != airspace.name)
      html.row2(tr("Designation:"), restrictedName);
  }

  html.row2(tr("Type:"), map::airspaceTypeToString(airspace.type));

  if(!airspace.isOnline())
  {
    if(airspace.minAltitudeType.isEmpty())
      html.row2(tr("Min altitude:"), tr("Unknown"));
    else
      html.row2(tr("Min altitude:"), Unit::altFeet(airspace.minAltitude) + " " + airspace.minAltitudeType);

    QString maxAlt;
    if(airspace.maxAltitudeType.isEmpty())
      maxAlt = tr("Unknown");
    else if(airspace.maxAltitudeType == "UL")
      maxAlt = tr("Unlimited");
    else
      maxAlt = Unit::altFeet(airspace.maxAltitude) + " " + airspace.maxAltitudeType;

    html.row2(tr("Max altitude:"), maxAlt);
  }

  html.row2If(tr("COM:"), formatter::capNavString(airspace.comName));

  html.row2If(tr("COM Type:"), map::comTypeName(airspace.comType));
  if(!airspace.comFrequencies.isEmpty())
  {
    QStringList freqTxt;
    for(int freq : airspace.comFrequencies)
    {
      // Round frequencies to nearest valid value to workaround for a compiler rounding bug

      if(airspace.isOnline())
        // Use online freqencies as is
        freqTxt.append(locale.toString(freq / 1000.f, 'f', 3));
      else
        freqTxt.append(locale.toString(roundComFrequency(freq), 'f', 3));

#ifdef DEBUG_INFORMATION
      freqTxt.last().append(QString(" [%1]").arg(freq));
#endif
    }

    html.row2(tr("COM Frequency:"), freqTxt.join(tr(", ") + tr(" MHz")));
  }

  if(!onlineRec.isEmpty() && info)
  {
    // Show online center information ====================================
    html.row2If(tr("VID:"), onlineRec.valueStr("vid"));
    html.row2If(tr("Name:"), onlineRec.valueStr("name"));
    html.row2If(tr("Server:"), onlineRec.valueStr("server"));
    html.row2If(tr("Facility Type:"), atools::fs::online::facilityTypeText(onlineRec.valueInt("facility_type")));
    html.row2If(tr("Visual Range:"), Unit::distNm(onlineRec.valueInt("visual_range")));
    html.row2If(tr("ATIS:"), onlineRec.valueStr("atis").replace("^\\A7", "\n"));
    html.row2If(tr("ATIS Time:"), locale.toString(onlineRec.valueDateTime("atis_time")));

    float qnh = onlineRec.valueFloat("qnh_mb");
    if(qnh > 0.f && qnh < 10000.f)
      html.row2(tr("Sea Level Pressure:"), locale.toString(qnh, 'f', 0) + tr(" hPa, ") +
                locale.toString(ageo::mbarToInHg(qnh), 'f', 2) + tr(" inHg"));

    if(onlineRec.isNull("administrative_rating") || onlineRec.isNull("atc_pilot_rating"))
      html.row2If(tr("Combined Rating:"), onlineRec.valueStr("combined_rating"));

    if(!onlineRec.isNull("administrative_rating"))
      html.row2If(tr("Administrative Rating:"),
                  atools::fs::online::admRatingText(onlineRec.valueInt("administrative_rating")));
    if(!onlineRec.isNull("atc_pilot_rating"))
      html.row2If(tr("ATC Rating:"), atools::fs::online::atcRatingText(onlineRec.valueInt("atc_pilot_rating")));

    html.row2If(tr("Connection Time:"), locale.toString(onlineRec.valueDateTime("connection_time")));

    if(!onlineRec.valueStr("software_name").isEmpty())
      html.row2If(tr("Software:"), tr("%1 %2").
                  arg(onlineRec.valueStr("software_name")).
                  arg(onlineRec.valueStr("software_version")));

    // Always unknown
    // html.row2If(tr("Simulator:"), atools::fs::online::simulatorText(onlineRec.valueInt("simulator")));
  }
  html.tableEnd();

  if(info && !airspace.isOnline())
  {
    atools::sql::SqlRecord rec = NavApp::getAirspaceController()->getAirspaceInfoRecordById(airspace.combinedId());

    if(!rec.isEmpty())
      addScenery(&rec, html);
  }

#ifdef DEBUG_INFORMATION
  html.small(QString("Database: source = %1, boundary_id = %2").
             arg(map::airspaceSourceText(airspace.src)).arg(airspace.getId())).br();
#endif

}

void HtmlInfoBuilder::airwayText(const MapAirway& airway, HtmlBuilder& html) const
{
  bool isAirway = airway.isAirway();

  if(!isAirway && !NavApp::hasTracks())
    return;

  if(isAirway)
    navaidTitle(html, tr("Airway: ") + airway.name);
  else
    navaidTitle(html, tr("Track: ") + airway.name);

  if(info)
  {
    // Add map link if not tooltip
    html.nbsp().nbsp();
    html.a(tr("Map"), QString("lnm://show?id=%1&fragment=%2&type=%3").
           arg(airway.id).arg(airway.fragment).arg(map::AIRWAY), ahtml::LINK_NO_UL);
  }

  html.table();

  if(isAirway)
  {
    html.row2If(tr("Segment type:"), map::airwayTrackTypeToString(airway.type));

    if(info)
      html.row2If(tr("Route type:"), map::airwayRouteTypeToString(airway.routeType));
  }
  else
    html.row2If(tr("Track type:"), map::airwayTrackTypeToString(airway.type));

  map::MapWaypoint from = waypointQuery->getWaypointById(airway.fromWaypointId);
  map::MapWaypoint to = waypointQuery->getWaypointById(airway.toWaypointId);

  // Show from/to waypoints if one-way and include links ==================================
  HtmlBuilder tempHtml = html.cleared();

  if(airway.direction == map::DIR_BACKWARD)
    // Reverse if one-way is backward
    std::swap(from, to);

  QString connector(airway.direction == map::DIR_BOTH ? tr(", ") : tr(" ► "));
  if(info)
  {
    tempHtml.a(identRegionText(from.ident, from.region), QString("lnm://show?lonx=%1&laty=%2").
               arg(from.position.getLonX()).arg(from.position.getLatY()), ahtml::LINK_NO_UL);
    tempHtml.text(connector);
    tempHtml.a(identRegionText(to.ident, to.region), QString("lnm://show?lonx=%1&laty=%2").
               arg(to.position.getLonX()).arg(to.position.getLatY()), ahtml::LINK_NO_UL);
  }
  else
    tempHtml.text(tr("%1%2%3").
                  arg(identRegionText(from.ident, from.region)).
                  arg(connector).
                  arg(identRegionText(to.ident, to.region)));

  if(airway.direction != map::DIR_BOTH)
    html.row2(tr("Segment One-way:"), tempHtml.getHtml(), ahtml::NO_ENTITIES);
  else
    html.row2(tr("Segment:"), tempHtml.getHtml(), ahtml::NO_ENTITIES);

  QString altTxt = map::airwayAltText(airway);
  html.row2If(tr("Altitude for this segment:"), altTxt);

  if(!airway.altitudeLevelsEast.isEmpty())
  {
    QStringList altLevels;
    for(int level : airway.altitudeLevelsEast)
      altLevels.append(QString("%1").arg(level));
    html.row2If(tr("Track levels East:"), altLevels.join(tr(", ")));
  }
  if(!airway.altitudeLevelsWest.isEmpty())
  {
    QStringList altLevels;
    for(int level : airway.altitudeLevelsWest)
      altLevels.append(QString("%1").arg(level));
    html.row2If(tr("Track levels West:"), altLevels.join(tr(", ")));
  }

  html.row2(tr("Segment length:"), Unit::distMeter(airway.from.distanceMeterTo(airway.to)));

  atools::sql::SqlRecord trackMeta;
  if(infoQuery != nullptr && info)
  {
    if(!isAirway)
    {
      trackMeta = infoQuery->getTrackMetadata(airway.id);
      if(!trackMeta.isEmpty())
      {
        QDateTime from = trackMeta.valueDateTime("valid_from");
        QDateTime to = trackMeta.valueDateTime("valid_to");
        QDateTime now = QDateTime::currentDateTimeUtc();

        html.row2(tr("Track valid:"), tr("%1 UTC to<br/>%2 UTC%3").
                  arg(locale.toString(from, QLocale::ShortFormat)).
                  arg(locale.toString(to, QLocale::ShortFormat)).
                  arg(now >= from && now <= to ? tr("<br/><b>Track is now valid.</b>") : QString()),
                  atools::util::html::NO_ENTITIES);
        html.row2(tr("Track downloaded:"),
                  locale.toString(trackMeta.valueDateTime("download_timestamp"), QLocale::ShortFormat));
      }
    }

    // Show list of waypoints =================================================================
    QList<map::MapAirwayWaypoint> waypointList;
    NavApp::getAirwayTrackQuery()->getWaypointListForAirwayName(waypointList, airway.name, airway.fragment);

    if(!waypointList.isEmpty())
    {
      HtmlBuilder tempLinkHtml = html.cleared();
      for(const map::MapAirwayWaypoint& airwayWaypoint : waypointList)
      {
        if(!tempLinkHtml.isEmpty())
          tempLinkHtml.text(", ");
        tempLinkHtml.a(identRegionText(airwayWaypoint.waypoint.ident, airwayWaypoint.waypoint.region),
                       QString("lnm://show?lonx=%1&laty=%2").
                       arg(airwayWaypoint.waypoint.position.getLonX()).
                       arg(airwayWaypoint.waypoint.position.getLatY()), ahtml::LINK_NO_UL);
      }

      html.row2(tr("Waypoints Ident/Region:"), tempLinkHtml.getHtml(), ahtml::NO_ENTITIES);
    }
  }
  html.tableEnd();

#ifdef DEBUG_INFORMATION
  if(!trackMeta.isEmpty())
    html.small(trackMeta.valueStr("route")).br();
  html.small(QString("Database: airway_id = %1").arg(airway.getId())).br();
#endif

  if(info)
    html.br();
}

void HtmlInfoBuilder::markerText(const MapMarker& marker, HtmlBuilder& html) const
{
  if(marker.ident.isEmpty())
    head(html, tr("Marker: %1").arg(atools::capString(marker.type)));
  else
    head(html, tr("Marker: %1 (%2)").arg(marker.ident).arg(atools::capString(marker.type)));
  html.br();
}

void HtmlInfoBuilder::towerText(const MapAirport& airport, HtmlBuilder& html) const
{
  if(airport.towerFrequency > 0)
  {
    head(html, tr("Tower:"));
    html.br();
    head(html, locale.toString(roundComFrequency(airport.towerFrequency), 'f', 3) + tr(" MHz"));
  }
  else
    head(html, tr("Tower"));
}

void HtmlInfoBuilder::parkingText(const MapParking& parking, HtmlBuilder& html) const
{
  head(html, map::parkingName(parking.name) +
       (parking.number != -1 ? " " + locale.toString(parking.number) : QString()));

  if(!parking.type.isEmpty())
    html.brText(map::parkingTypeName(parking.type));

  if(parking.radius > 0.f)
    html.brText(Unit::distShortFeet(parking.radius * 2.f));

  if(parking.jetway)
    html.brText(tr("Has Jetway"));

  if(!parking.airlineCodes.isEmpty())
  {
    // Turn text into blocks and strip number of lines and add ... if needed
    QString txt = atools::elideTextLinesShort(atools::blockText(parking.airlineCodes.split(","), 10, ",", "\n"), 8);

    // Do not allow tooltip to break line
    html.br().text(tr("Airline Codes: ") + txt, ahtml::NOBR | ahtml::REPLACE_CRLF);
  }
}

void HtmlInfoBuilder::userpointTextRoute(const MapUserpointRoute& userpoint, HtmlBuilder& html) const
{
  QIcon icon = SymbolPainter().createUserpointIcon(symbolSizeTitle.height());
  html.img(icon, QString(), QString(), symbolSizeTitle);
  html.nbsp().nbsp();

  navaidTitle(html, tr("Position: ") + userpoint.ident);

  if(!info && userpoint.routeIndex >= 0)
  {
    if(!userpoint.name.isEmpty() || !userpoint.comment.isEmpty() || !userpoint.region.isEmpty())
    {
      html.table();
      html.row2If(tr("Flight Plan Position:"), QString::number(userpoint.routeIndex + 1));
      html.row2If(tr("Region:"), userpoint.region);
      html.row2If(tr("Name:"), userpoint.name);
      html.row2If(tr("Remarks:"), atools::elideTextLinesShort(userpoint.comment, 20, 80));
      html.tableEnd();
    }
    else
      html.p().b(tr("Flight Plan Position: ") + QString::number(userpoint.routeIndex + 1)).pEnd();
    routeWindText(html, NavApp::getRouteConst(), userpoint.routeIndex);
  }
}

void HtmlInfoBuilder::procedurePointText(const proc::MapProcedurePoint& procPoint, HtmlBuilder& html,
                                         const Route *route) const
{
  QString header = procPoint.preview ? proc::procedureTypeText(procPoint.mapType) : route->getProcedureLegText(
    procPoint.mapType);

  head(html, header);

  QStringList atts;

  if(procPoint.flyover)
    atts += tr("Fly over");

  html.table();

  // Add IAF, MAP, ...
  QString typeStr = proc::proceduresLegSecialTypeLongStr(proc::specialType(procPoint.arincDescrCode));
  if(!typeStr.isEmpty())
    html.row2(typeStr);

  html.row2(tr("Leg Type:"), proc::procedureLegTypeStr(procPoint.type));
  html.row2(tr("Fix:"), procPoint.fixIdent);

  if(!atts.isEmpty())
    html.row2(atts.join(", "));

  if(!procPoint.remarks.isEmpty())
    html.row2(procPoint.remarks.join(", "));

  if(procPoint.altRestriction.isValid())
    html.row2(tr("Altitude Restriction:"), proc::altRestrictionText(procPoint.altRestriction));

  if(procPoint.speedRestriction.isValid())
    html.row2(tr("Speed Restriction:"), proc::speedRestrictionText(procPoint.speedRestriction));

  if(procPoint.calculatedDistance > 0.f)
    html.row2(tr("Distance:"), Unit::distNm(procPoint.calculatedDistance /*, true, 20, true*/));
  if(procPoint.time > 0.f)
    html.row2(tr("Time:"), locale.toString(procPoint.time, 'f', 0) + tr(" min"));
  if(procPoint.calculatedTrueCourse < map::INVALID_COURSE_VALUE)
    html.row2(tr("Course:"), courseTextFromTrue(procPoint.calculatedTrueCourse, procPoint.magvar), ahtml::NO_ENTITIES);

  if(!procPoint.turnDirection.isEmpty())
  {
    if(procPoint.turnDirection == "L")
      html.row2(tr("Turn:"), tr("Left"));
    else if(procPoint.turnDirection == "R")
      html.row2(tr("Turn:"), tr("Right"));
    else if(procPoint.turnDirection == "B")
      html.row2(tr("Turn:"), tr("Left or right"));
  }

  if(!procPoint.recFixIdent.isEmpty())
  {
    if(procPoint.rho > 0.f)
      html.row2(tr("Related Navaid:"),
                tr("%1 / %2 / %3").arg(procPoint.recFixIdent).
                arg(Unit::distNm(procPoint.rho /*, true, 20, true*/)).
                arg(courseTextFromMag(procPoint.theta, procPoint.magvar)), ahtml::NO_ENTITIES);
    else
      html.row2(tr("Related Navaid:"), tr("%1").arg(procPoint.recFixIdent));
  }

  html.tableEnd();
}

void HtmlInfoBuilder::aircraftText(const atools::fs::sc::SimConnectAircraft& aircraft, HtmlBuilder& html, int num,
                                   int total)
{
  if(!aircraft.getPosition().isValid())
    return;

  aircraftTitle(aircraft, html, false /* show more/less switch */, false /* true if less info mode */);

  QString aircraftText;
  if(aircraft.isUser())
  {
    aircraftText = tr("User Aircraft");
    if(info && !(NavApp::getShownMapFeatures() & map::AIRCRAFT))
      html.p(tr("User aircraft is not shown on map."), ahtml::BOLD);
  }
  else
  {
    QString type(tr(" Unknown"));
    switch(aircraft.getCategory())
    {
      case atools::fs::sc::BOAT:
        type = tr(" Ship");
        break;
      case atools::fs::sc::CARRIER:
        type = tr(" Carrier");
        break;
      case atools::fs::sc::FRIGATE:
        type = tr(" Frigate");
        break;
      case atools::fs::sc::AIRPLANE:
        type = tr(" Aircraft");
        break;
      case atools::fs::sc::HELICOPTER:
        type = tr(" Helicopter");
        break;
      case atools::fs::sc::UNKNOWN:
        type.clear();
        break;
      case atools::fs::sc::GROUNDVEHICLE:
      case atools::fs::sc::CONTROLTOWER:
      case atools::fs::sc::SIMPLEOBJECT:
      case atools::fs::sc::VIEWER:
        break;
    }

    QString typeText;
    if(aircraft.isOnline())
      typeText = tr("Online Client");
    else
      typeText = tr("AI / Multiplayer");

    if(num != -1 && total != -1)
      aircraftText = tr("%1%2 - %3 of %4 Vehicles").arg(typeText).arg(type).arg(num).arg(total);
    else
      aircraftText = tr("%1%2").arg(typeText).arg(type);

    if(info && num == 1 && !(NavApp::getShownMapFeatures() & map::AIRCRAFT_AI))
      html.p(tr("No %2 shown on map.").arg(typeText), ahtml::BOLD);
  }

  head(html, aircraftText);

  html.table();
  if(!aircraft.getAirplaneTitle().isEmpty())
    html.row2(tr("Title:"), aircraft.getAirplaneTitle());
  else
    html.row2(tr("Number:"), locale.toString(aircraft.getObjectId() + 1));

  if(!aircraft.getAirplaneAirline().isEmpty())
    html.row2(tr("Airline:"), aircraft.getAirplaneAirline());

  if(!aircraft.getAirplaneFlightnumber().isEmpty())
    html.row2(tr("Flight Number:"), aircraft.getAirplaneFlightnumber());

  if(!aircraft.getAirplaneModel().isEmpty())
    html.row2(tr("Type:"), aircraft.getAirplaneModel());

  if(!aircraft.getAirplaneRegistration().isEmpty())
    html.row2(tr("Registration:"), aircraft.getAirplaneRegistration());

  QString type = airplaneType(aircraft);
  if(!type.isEmpty())
    html.row2(tr("Model:"), type);

  if(aircraft.isAnyBoat())
  {
    if(info && aircraft.getModelRadius() > 0)
      html.row2(tr("Size:"), Unit::distShortFeet(aircraft.getModelRadius() * 2));
    if(info && aircraft.getDeckHeight() > 0)
      html.row2(tr("Deck height:"), Unit::distShortFeet(aircraft.getDeckHeight()));
  }
  else if(info && aircraft.getWingSpan() > 0)
    html.row2(tr("Wingspan:"), Unit::distShortFeet(aircraft.getWingSpan()));
  html.tableEnd();

#ifdef DEBUG_INFORMATION
  html.small(QString("Object ID: %1").arg(aircraft.getId())).br();
  QStringList flags;
  if(aircraft.isUser())
    flags << "User";
  if(aircraft.isOnGround())
    flags << "Ground";
  if(aircraft.isOnline())
    flags << "Online";
  if(aircraft.isOnlineShadow())
    flags << "Online Shadow";
  if(aircraft.isSimPaused())
    flags << "Pause";
  if(aircraft.isSimReplay())
    flags << "Replay";
  html.small(QString("Flags: %1").arg(flags.join(","))).br();
#endif
}

void HtmlInfoBuilder::aircraftOnlineText(const atools::fs::sc::SimConnectAircraft& aircraft,
                                         const atools::sql::SqlRecord& onlineRec, HtmlBuilder& html)
{
  if(!onlineRec.isEmpty() && info)
  {
    // General online information =================================================================
    head(html, tr("Online Information"));
    html.table();

    html.row2If(tr("VID:"), onlineRec.valueStr("vid"));
    html.row2If(tr("Name:"), onlineRec.valueStr("name"));
    html.row2If(tr("Server:"), onlineRec.valueStr("server"));

    float qnh = onlineRec.valueFloat("qnh_mb");
    if(qnh > 0.f && qnh < 10000.f)
      html.row2(tr("Sea Level Pressure:"), locale.toString(qnh, 'f', 0) + tr(" hPa, ") +
                locale.toString(ageo::mbarToInHg(qnh), 'f', 2) + tr(" inHg"));

    if(onlineRec.isNull("administrative_rating") || onlineRec.isNull("atc_pilot_rating"))
      html.row2If(tr("Combined Rating:"), onlineRec.valueStr("combined_rating"));

    if(!onlineRec.isNull("administrative_rating"))
      html.row2If(tr("Administrative Rating:"),
                  atools::fs::online::admRatingText(onlineRec.valueInt("administrative_rating")));
    if(!onlineRec.isNull("atc_pilot_rating"))
      html.row2If(tr("ATC Rating:"), atools::fs::online::pilotRatingText(onlineRec.valueInt("atc_pilot_rating")));

    html.row2If(tr("Connection Time:"), locale.toString(onlineRec.valueDateTime("connection_time")));

    if(!onlineRec.valueStr("software_name").isEmpty())
      html.row2If(tr("Software:"), tr("%1 %2").
                  arg(onlineRec.valueStr("software_name")).
                  arg(onlineRec.valueStr("software_version")));
    html.row2If(tr("Simulator:"), atools::fs::online::simulatorText(onlineRec.valueInt("simulator")));
    html.tableEnd();

    // Flight plan information =================================================================
    head(html, tr("Flight Plan"));
    html.table();

    if(onlineRec.valueBool("prefile"))
      html.row2(tr("Is Prefile"));

    QString spd = onlineRec.valueStr("flightplan_cruising_speed");
    QString alt = onlineRec.valueStr("flightplan_cruising_level");

    // Decode speed and altitude
    float speed, altitude;
    bool speedOk, altitudeOk;
    atools::fs::util::extractSpeedAndAltitude(
      spd + (alt == "VFR" ? QString() : alt), speed, altitude, &speedOk, &altitudeOk);

    if(speedOk)
      html.row2If(tr("Cruising Speed"), tr("%1 (%2)").arg(spd).arg(Unit::speedKts(speed)));
    else
      html.row2If(tr("Cruising Speed"), spd);

    if(altitudeOk)
      html.row2If(tr("Cruising Level:"), tr("%1 (%2)").arg(alt).arg(Unit::altFeet(altitude)));
    else
      html.row2If(tr("Cruising Level:"), alt);

    html.row2If(tr("Transponder Code:"), onlineRec.valueStr("transponder_code"));

    float range = onlineRec.valueFloat("visual_range");
    if(range > 0.f && range < map::INVALID_ALTITUDE_VALUE)
      html.row2If(tr("Visual Range:"), Unit::distNm(range));
    html.row2If(tr("Flight Rules:"), onlineRec.valueStr("flightplan_flight_rules"));
    html.row2If(tr("Type of Flight:"), onlineRec.valueStr("flightplan_type_of_flight"));

    // Display times =============================================================
    QString departureTime = onlineRec.valueStr("flightplan_departure_time");
    QTime depTime = atools::timeFromHourMinStr(departureTime);
    html.row2If(tr("Departure Time:"),
                depTime.isValid() ? depTime.toString("HH:mm UTC") : departureTime);

    QString actualDepartureTime = onlineRec.valueStr("flightplan_actual_departure_time");
    QTime actDepTime = atools::timeFromHourMinStr(actualDepartureTime);
    html.row2If(tr("Actual Departure Time:"),
                actDepTime.isValid() ? actDepTime.toString("HH:mm UTC") : actualDepartureTime);

    QTime estArrTime = atools::timeFromHourMinStr(onlineRec.valueStr("flightplan_estimated_arrival_time"));
    if(estArrTime.isValid())
      html.row2If(tr("Estimated Arrival Time:"), estArrTime.toString("HH:mm UTC"));

    double enrouteMin = onlineRec.valueDouble("flightplan_enroute_minutes");

    if(enrouteMin > 0.)
      html.row2(tr("Estimated Enroute time hh:mm:"), formatter::formatMinutesHours(enrouteMin / 60.));
    double enduranceMin = onlineRec.valueDouble("flightplan_endurance_minutes");
    if(enduranceMin > 0.)
      html.row2If(tr("Endurance hh:mm:"), formatter::formatMinutesHours(enduranceMin / 60.));

    QStringList alternates({onlineRec.valueStr("flightplan_alternate_aerodrome"),
                            onlineRec.valueStr("flightplan_2nd_alternate_aerodrome")});
    alternates.removeAll(QString());
    html.row2If(alternates.size() > 1 ? tr("Alternates:") : tr("Alternate:"), alternates.join(tr(", ")));

    QString route = onlineRec.valueStr("flightplan_route").trimmed();
    QString departure = aircraft.getFromIdent();
    if(!route.startsWith(departure))
      route.prepend(departure + " ");
    QString destination = aircraft.getToIdent();
    if(!route.endsWith(destination))
      route.append(" " + destination);

    html.row2If(tr("Route:"), route);
    html.row2If(tr("Other Information:"), onlineRec.valueStr("flightplan_other_info"));
    html.row2If(tr("Persons on Board:"), onlineRec.valueInt("flightplan_persons_on_board"));
    html.tableEnd();

    if(info)
    {
      head(html, tr("Position"));
      addCoordinates(aircraft.getPosition(), html);
      html.tableEnd();
    }
  }
}

void HtmlInfoBuilder::aircraftTextWeightAndFuel(const atools::fs::sc::SimConnectUserAircraft& userAircraft,
                                                HtmlBuilder& html) const
{
  if(!userAircraft.getPosition().isValid())
    return;

  if(info)
  {
    head(html, tr("Weight and Fuel"));
    html.table().row2AlignRight();

    float maxGrossWeight = userAircraft.getAirplaneMaxGrossWeightLbs();
    float grossWeight = userAircraft.getAirplaneTotalWeightLbs();

    html.row2(tr("Max Gross Weight:"), Unit::weightLbsLocalOther(maxGrossWeight), ahtml::NO_ENTITIES);

    if(grossWeight > maxGrossWeight)
      html.row2Error(tr("Gross Weight:"), Unit::weightLbsLocalOther(grossWeight, false, false));
    else
      html.row2(tr("Gross Weight:"), Unit::weightLbsLocalOther(grossWeight, true /* bold */), ahtml::NO_ENTITIES);

    html.row2(QString());
    html.row2(tr("Empty Weight:"), Unit::weightLbsLocalOther(userAircraft.getAirplaneEmptyWeightLbs()),
              ahtml::NO_ENTITIES);
    html.row2(tr("Zero Fuel Weight:"), Unit::weightLbsLocalOther(userAircraft.getAirplaneTotalWeightLbs() -
                                                                 userAircraft.getFuelTotalWeightLbs()),
              ahtml::NO_ENTITIES);

    html.row2(tr("Total Payload:"), Unit::weightLbsLocalOther(userAircraft.getAirplaneTotalWeightLbs() -
                                                              userAircraft.getAirplaneEmptyWeightLbs() -
                                                              userAircraft.getFuelTotalWeightLbs()),
              ahtml::NO_ENTITIES);

    html.row2(tr("Fuel:"), Unit::fuelLbsAndGalLocalOther(userAircraft.getFuelTotalWeightLbs(),
                                                         userAircraft.getFuelTotalQuantityGallons(), true /* bold */),
              ahtml::NO_ENTITIES);
    html.tableEnd().row2AlignRight(false);
  }
}

void HtmlInfoBuilder::dateAndTime(const SimConnectUserAircraft *userAircraft, HtmlBuilder& html) const
{
  html.row2(tr("Date and Time:"),
            locale.toString(userAircraft->getZuluTime(), QLocale::ShortFormat) +
            tr(" ") +
            userAircraft->getZuluTime().timeZoneAbbreviation());

  html.row2(tr("Local Time:"),
            locale.toString(userAircraft->getLocalTime().time(), QLocale::ShortFormat) +
            tr(" ") +
            userAircraft->getLocalTime().timeZoneAbbreviation());
}

void HtmlInfoBuilder::aircraftProgressText(const atools::fs::sc::SimConnectAircraft& aircraft,
                                           HtmlBuilder& html, const Route& route, bool moreLessSwitch, bool less)
{
  if(!aircraft.getPosition().isValid())
    return;

  const SimConnectUserAircraft *userAircaft = dynamic_cast<const SimConnectUserAircraft *>(&aircraft);
  AircraftPerfController *perfController = NavApp::getAircraftPerfController();

  // Fuel and time calculated or estimated
  FuelTimeResult fuelTime;

  if(info && userAircaft != nullptr)
  {
    aircraftTitle(aircraft, html, moreLessSwitch, less);
    if(!(NavApp::getShownMapFeatures() & map::AIRCRAFT))
      html.warning(tr("User aircraft is not shown on map."));
  }

  float distFromStartNm = 0.f, distToDestNm = 0.f, nearestLegDistance = 0.f, crossTrackDistance = 0.f;

  html.row2AlignRight();

  // Flight plan legs =========================================================================
  float distanceToTod = map::INVALID_DISTANCE_VALUE;
  if(!route.isEmpty() && userAircaft != nullptr && info)
  {
    // The corrected leg will point to an approach leg if we head to the start of a procedure
    bool isCorrected = false;
    int activeLegCorrected = route.getActiveLegIndexCorrected(&isCorrected);
    int activeLeg = route.getActiveLegIndex();
    bool alternate = route.isActiveAlternate();
    bool destination = route.isActiveDestinationAirport();

    if(activeLegCorrected != map::INVALID_INDEX_VALUE &&
       route.getRouteDistances(&distFromStartNm, &distToDestNm, &nearestLegDistance, &crossTrackDistance))
    {
      if(distFromStartNm < map::INVALID_DISTANCE_VALUE)
        distanceToTod = route.getTopOfDescentDistance() - distFromStartNm;

      if(alternate)
        // Use distance to alternate instead of destination
        distToDestNm = nearestLegDistance;

      // Calculates values based on performance profile if valid - otherwise estimated by aircraft fuel flow and speed
      perfController->calculateFuelAndTimeTo(fuelTime, distToDestNm, nearestLegDistance, activeLeg);

      // Print warning messages ===================================================================
      if(route.size() < 2)
        // Single point plan
        html.warning(tr("Flight plan not valid. Fuel and time estimated."));
      else
      {
        if(fuelTime.estimatedFuel && fuelTime.estimatedTime)
          html.warning(tr("Aircraft performance not valid. Fuel and time estimated."));
        else if(fuelTime.estimatedFuel)
          html.warning(tr("Aircraft performance not valid. Fuel estimated."));
        else if(fuelTime.estimatedTime)
          html.warning(tr("Aircraft performance not valid. Time estimated."));
      }

      html.table();
      dateAndTime(userAircaft, html);
      html.tableEnd();

      // Route distances ===============================================================
      if(distToDestNm < map::INVALID_DISTANCE_VALUE)
      {
        // Add name and type to header text
        QString headText = alternate ? tr("Alternate") : tr("Destination");
        const RouteLeg& destLeg = alternate && route.getActiveLeg() != nullptr ?
                                  *route.getActiveLeg() : route.getDestinationAirportLeg();

        headText += tr(" - ") + destLeg.getIdent() +
                    (destLeg.getMapObjectTypeName().isEmpty() ? QString() : tr(", ") +
                     destLeg.getMapObjectTypeName());

        head(html, headText);
        html.table();

        // Destination  ===============================================================
        bool timeToDestAvailable = fuelTime.isTimeToDestValid();
        bool fuelToDestAvailable = fuelTime.isFuelToDestValid();

        QString destinationText = timeToDestAvailable ? tr("Distance, Time and Arrival:") : tr("Distance:");
        if(route.isActiveMissed())
          // RouteAltitude legs does not provide values for missed - calculate them based on aircraft
          destinationText = tr("To End of Missed Approach:");

        if(timeToDestAvailable)
        {
          QDateTime arrival = userAircaft->getZuluTime().addSecs(static_cast<int>(fuelTime.timeToDest * 3600.f));

          html.row2(destinationText, strJoinVal({
            Unit::distNm(distToDestNm),
            formatter::formatMinutesHoursLong(fuelTime.timeToDest),
            locale.toString(arrival.time(), QLocale::ShortFormat) + tr(" ") + arrival.timeZoneAbbreviation()}));
        }
        else
          html.row2(destinationText, strJoinVal(
                      {Unit::distNm(distToDestNm), formatter::formatMinutesHoursLong(fuelTime.timeToDest)}));

        if(!less && fuelToDestAvailable)
        {
          // Remaining fuel estimate at destination
          float remainingFuelLbs = userAircaft->getFuelTotalWeightLbs() - fuelTime.fuelLbsToDest;
          float remainingFuelGal = userAircaft->getFuelTotalQuantityGallons() - fuelTime.fuelGalToDest;
          QString fuelValue = Unit::fuelLbsAndGalLocalOther(remainingFuelLbs, remainingFuelGal);

          QString fuelHdr = tr("Fuel:");
          if(!fuelTime.estimatedFuel && !fuelTime.estimatedTime)
          {
            // Display warnings for fuel at destination only if fuel was calculated from a valid performance and profile
            if(remainingFuelLbs < 0.f || remainingFuelGal < 0.f)
              html.row2Error(fuelHdr, Unit::fuelLbsAndGalLocalOther(remainingFuelLbs, remainingFuelGal, false, false));
            else if(remainingFuelLbs < perfController->getFuelReserveAtDestinationLbs() ||
                    remainingFuelGal < perfController->getFuelReserveAtDestinationGal())
              // Required reserves at destination
              html.row2Warning(fuelHdr,
                               Unit::fuelLbsAndGalLocalOther(remainingFuelLbs, remainingFuelGal, false, false));
            else
              // No warning - display plain values
              html.row2(fuelHdr, fuelValue, ahtml::NO_ENTITIES);
          }
          else
            // No warnings for estimated fuel
            html.row2(fuelHdr, fuelValue, ahtml::NO_ENTITIES);

          html.row2(tr("Gross Weight:"),
                    Unit::weightLbsLocalOther(userAircaft->getAirplaneTotalWeightLbs() - fuelTime.fuelLbsToDest),
                    ahtml::NO_ENTITIES);
        } // if(!less && fuelAvailable)
        html.tableEnd();
      } // if(distToDestNm < map::INVALID_DISTANCE_VALUE)

      // Top of descent  ===============================================================
      if(route.getSizeWithoutAlternates() > 1 && !alternate) // No TOD display when flying an alternate leg
      {
        // Avoid printing the head only in less information mode
        head(html, tr("Top of Descent%1").arg(distanceToTod < 0.f ? tr(" (passed)") : QString()));
        html.table();

        if(!(distanceToTod < map::INVALID_DISTANCE_VALUE))
          html.row2Error(QString(), tr("Not valid."));

        if(distanceToTod > 0.f && distanceToTod < map::INVALID_DISTANCE_VALUE)
        {
          QStringList todValues, todHeader;
          todValues.append(Unit::distNm(distanceToTod));
          todHeader.append(tr("Distance"));

          if(!less && fuelTime.isTimeToTodValid())
          {
            todValues.append(formatter::formatMinutesHoursLong(fuelTime.timeToTod));
            todHeader.append(tr("Time"));

            QDateTime arrival = userAircaft->getZuluTime().addSecs(static_cast<int>(fuelTime.timeToTod * 3600.f));
            todValues.append(locale.toString(arrival.time(), QLocale::ShortFormat) +
                             tr(" ") + arrival.timeZoneAbbreviation());
            todHeader.append(tr("Arrival"));
          }
          html.row2(strJoinHdr(todHeader), strJoinVal(todValues));

          if(!less && fuelTime.isFuelToTodValid())
          {
            // Calculate remaining fuel at TOD
            float remainingFuelTodLbs = userAircaft->getFuelTotalWeightLbs() - fuelTime.fuelLbsToTod;
            float remainingFuelTodGal = userAircaft->getFuelTotalQuantityGallons() - fuelTime.fuelGalToTod;

            html.row2(tr("Fuel:"), Unit::fuelLbsAndGal(remainingFuelTodLbs, remainingFuelTodGal), ahtml::NO_ENTITIES);
          }
        } // if(distanceToTod > 0 && distanceToTod < map::INVALID_DISTANCE_VALUE)

        if(!less && route.getTopOfDescentFromDestination() < map::INVALID_DISTANCE_VALUE)
          html.row2(tr("To Destination:"), Unit::distNm(route.getTopOfDescentFromDestination()));

        html.tableEnd();
      } // if(route.getSizeWithoutAlternates() > 1 && !alternate) // No TOD display when flying an alternate leg

      // Next leg ====================================================
      QString wpText;
      if(alternate)
        wpText = tr(" - Alternate");
      else if(destination)
        wpText = tr(" - Destination");
      else if(activeLegCorrected != map::INVALID_INDEX_VALUE)
      {
        const RouteLeg& routeLeg = route.value(activeLegCorrected);

        if(routeLeg.getProcedureLeg().isApproach())
          wpText = tr(" - Approach");
        else if(routeLeg.getProcedureLeg().isTransition())
          wpText = tr(" - Transition");
        else if(routeLeg.getProcedureLeg().isMissed())
          wpText = tr(" - Missed Approach");
      }

      // Next leg - waypoint data ====================================================
      // If approaching an initial fix use corrected version
      const RouteLeg& routeLegCorrected = route.value(activeLegCorrected);
      QString nextName;
      if(activeLegCorrected != map::INVALID_INDEX_VALUE)
      {
        if(!routeLegCorrected.getIdent().isEmpty())
          nextName = tr(" - ") + routeLegCorrected.getIdent() +
                     (routeLegCorrected.getMapObjectTypeName().isEmpty() ? QString() : tr(", ") +
                      routeLegCorrected.getMapObjectTypeName());
      }

      head(html, tr("Next Waypoint%1%2").arg(wpText).arg(nextName));
      html.table();

      if(activeLegCorrected != map::INVALID_INDEX_VALUE)
      {
        // If approaching an initial fix use corrected version

        // For course and distance use not corrected leg
        const RouteLeg& routeLeg = activeLeg != map::INVALID_INDEX_VALUE &&
                                   isCorrected ? route.value(activeLeg) : routeLegCorrected;

        const proc::MapProcedureLeg& procLeg = routeLegCorrected.getProcedureLeg();

        // Next leg - approach data ====================================================
        if(routeLegCorrected.isAnyProcedure())
        {
          if(!less)
          {
            html.row2(tr("Leg Type:"), proc::procedureLegTypeStr(procLeg.type));

            QStringList instructions;
            if(procLeg.flyover)
              instructions += tr("Fly over");

            if(!procLeg.turnDirection.isEmpty())
            {
              if(procLeg.turnDirection == "L")
                instructions += tr("Turn Left");
              else if(procLeg.turnDirection == "R")
                instructions += tr("Turn Right");
              else if(procLeg.turnDirection == "B")
                instructions += tr("Turn Left or right");
            }
            if(!instructions.isEmpty())
              html.row2(tr("Instructions:"), instructions.join(", "));
          }
        }

        // Next leg - approach related navaid ====================================================
        if(!less && !procLeg.recFixIdent.isEmpty())
        {
          if(procLeg.rho > 0.f)
            html.row2(tr("Related Navaid:"), tr("%1, %2, %3").arg(procLeg.recFixIdent).
                      arg(Unit::distNm(procLeg.rho /*, true, 20, true*/)).
                      arg(courseTextFromMag(procLeg.theta, procLeg.magvar)), ahtml::NO_ENTITIES);
          else
            html.row2(tr("Related Navaid:"), tr("%1").arg(procLeg.recFixIdent));
        }

        // Altitude restrictions for procedure legs ==============================================
        if(routeLegCorrected.isAnyProcedure())
        {
          QStringList restrictions;

          if(procLeg.altRestriction.isValid())
            restrictions.append(proc::altRestrictionText(procLeg.altRestriction));
          if(procLeg.speedRestriction.isValid())
            restrictions.append(proc::speedRestrictionText(procLeg.speedRestriction));

          if(restrictions.size() > 1)
            html.row2(tr("Restrictions:"), restrictions.join(tr(", ")));
          else if(restrictions.size() == 1)
            html.row2(tr("Restriction:"), restrictions.first());
        }

        // Distance, time and ETA for next waypoint ==============================================
        float courseToWptTrue = map::INVALID_COURSE_VALUE;
        if(nearestLegDistance < map::INVALID_DISTANCE_VALUE)
        {
          QStringList distTimeStr, distTimeHeader;

          // Distance
          distTimeStr.append(Unit::distNm(nearestLegDistance));
          distTimeHeader.append(tr("Distance"));

          if(aircraft.getGroundSpeedKts() > MIN_GROUND_SPEED &&
             aircraft.getGroundSpeedKts() < atools::fs::sc::SC_INVALID_FLOAT && fuelTime.isTimeToNextValid())
          {
            // Travel time
            distTimeStr.append(formatter::formatMinutesHoursLong(nearestLegDistance / aircraft.getGroundSpeedKts()));
            distTimeHeader.append(tr("Time"));

            // ETA
            QDateTime arrivalNext = userAircaft->getZuluTime().addSecs(static_cast<int>(fuelTime.timeToNext * 3600.f));
            distTimeStr.append(locale.toString(arrivalNext.time(), QLocale::ShortFormat) + tr(" ") +
                               arrivalNext.timeZoneAbbreviation());
            distTimeHeader.append(tr("Arrival"));
          }

          if(!alternate && !destination)
          {
            // Distance, time and arrival are already part of the
            // destination information when flying an alternate leg
            html.row2(strJoinHdr(distTimeHeader), strJoinVal(distTimeStr), ahtml::NO_ENTITIES);

            // Fuel
            if(!less && fuelTime.isFuelToNextValid())
            {
              // Calculate remaining fuel at next
              float remainingFuelNextLbs = userAircaft->getFuelTotalWeightLbs() - fuelTime.fuelLbsToNext;
              float remainingFuelNextGal = userAircaft->getFuelTotalQuantityGallons() - fuelTime.fuelGalToNext;

              html.row2(tr("Fuel:"), Unit::fuelLbsAndGal(remainingFuelNextLbs, remainingFuelNextGal),
                        ahtml::NO_ENTITIES);
            }
          }

          // Course Not for arc legs
          if((routeLeg.isRoute() || !procLeg.isCircular()) && routeLeg.getPosition().isValid())
          {
            courseToWptTrue = aircraft.getPosition().angleDegTo(routeLeg.getPosition());

            html.row2(tr("Course:"),
                      courseTextFromTrue(courseToWptTrue, routeLeg.getMagVarBySettings(), true /* bold */),
                      ahtml::NO_ENTITIES);
          }

        } // if(nearestLegDistance < map::INVALID_DISTANCE_VALUE)

        // No cross track and course for holds
        if(route.getSizeWithoutAlternates() > 1)
        {
          // No course for arcs
          if(routeLeg.isRoute() || !routeLeg.getProcedureLeg().isCircular())
          {
            html.row2If(tr("Leg Course:"), courseText(routeLeg.getCourseToMag(), routeLeg.getCourseToTrue(),
                                                      true), ahtml::NO_ENTITIES);

            if(!less && userAircaft != nullptr && userAircaft->isFlying() && courseToWptTrue < INVALID_COURSE_VALUE)
            {
              // Crab angle is the amount of correction an aircraft must be turned into the wind in order to maintain the desired course.
              float crabAngleTrue = ageo::windCorrectedHeading(userAircaft->getWindSpeedKts(),
                                                               userAircaft->getWindDirectionDegT(),
                                                               courseToWptTrue,
                                                               userAircaft->getTrueAirspeedKts());
              if(crabAngleTrue < INVALID_COURSE_VALUE)
                html.row2(tr("Crab angle:"), courseTextFromTrue(crabAngleTrue, userAircaft->getMagVarDeg()),
                          ahtml::NO_ENTITIES);
            }
          }

          if(!less && !routeLeg.getProcedureLeg().isHold())
          {
            if(crossTrackDistance < map::INVALID_DISTANCE_VALUE)
            {
              // Positive means right of course,  negative means left of course.
              QString crossDirection;
              if(crossTrackDistance >= 0.1f)
                crossDirection = tr("<b>◄</b>");
              else if(crossTrackDistance <= -0.1f)
                crossDirection = tr("<b>►</b>");

              html.row2(tr("Cross Track Distance:"),
                        Unit::distNm(std::abs(crossTrackDistance)) + " " + crossDirection, ahtml::NO_ENTITIES);
            }
            else
              html.row2(tr("Cross Track Distance:"), tr("Not along Track"));
          }
        }
      }
      else
        qWarning() << "Invalid route leg index" << activeLegCorrected;

      html.tableEnd();
    } // if(activeLegCorrected != map::INVALID_INDEX_VALUE && ...
    else
    {
      if(route.isTooFarToFlightPlan())
        html.warning(tr("No Active Flight Plan Leg. Too far from flight plan."));
      else
        html.b(tr("No Active Flight Plan Leg."));
      html.table();
      dateAndTime(userAircaft, html);
      html.tableEnd();
    }
  } // if(!route.isEmpty() && userAircaft != nullptr && info)
  else if(info && userAircaft != nullptr)
  {
    html.b(tr("No Flight Plan."));
    html.table();
    dateAndTime(userAircaft, html);
    html.tableEnd();
  }

  // Add departure and destination for AI ==================================================
  if(userAircaft == nullptr && (!aircraft.getFromIdent().isEmpty() || !aircraft.getToIdent().isEmpty()))
  {
    if(info && userAircaft != nullptr)
      head(html, tr("Flight Plan"));

    html.table();
    html.row2(tr("Departure:"), airportLink(html, aircraft.getFromIdent()), ahtml::NO_ENTITIES);
    html.row2(tr("Destination:"), airportLink(html, aircraft.getToIdent()), ahtml::NO_ENTITIES);
    html.tableEnd();
    html.br();
  }

  // Aircraft =========================================================================
  if(info && userAircaft != nullptr)
    head(html, tr("Aircraft"));
  html.table();

  QStringList hdg;
  float heading = atools::fs::sc::SC_INVALID_FLOAT;
  if(aircraft.getHeadingDegMag() < atools::fs::sc::SC_INVALID_FLOAT)
    heading = aircraft.getHeadingDegMag();
  else if(aircraft.getHeadingDegTrue() < atools::fs::sc::SC_INVALID_FLOAT)
    heading = normalizeCourse(aircraft.getHeadingDegTrue() - NavApp::getMagVar(aircraft.getPosition()));

  if(heading < atools::fs::sc::SC_INVALID_FLOAT)
    hdg.append(courseText(heading, aircraft.getHeadingDegTrue(), true));

  if(!hdg.isEmpty())
    html.row2(tr("Heading:"), hdg.join(tr(", ")), ahtml::NO_ENTITIES);

  if(userAircaft != nullptr && info)
  {
    if(userAircaft != nullptr)
      html.row2(tr("Track:"), courseText(userAircaft->getTrackDegMag(), userAircaft->getTrackDegTrue()),
                ahtml::NO_ENTITIES);

    if(!less)
      html.row2(tr("Fuel Flow:"), Unit::ffLbsAndGal(userAircaft->getFuelFlowPPH(), userAircaft->getFuelFlowGPH()));

    if(userAircaft->getGroundSpeedKts() < atools::fs::sc::SC_INVALID_FLOAT)
    {
      if(userAircaft->getFuelFlowPPH() > 1.0f && aircraft.getGroundSpeedKts() > MIN_GROUND_SPEED)
      {
        float hoursRemaining = userAircaft->getFuelTotalWeightLbs() / userAircaft->getFuelFlowPPH();
        float distanceRemaining = hoursRemaining * aircraft.getGroundSpeedKts();
        html.row2(tr("Endurance:"), formatter::formatMinutesHoursLong(hoursRemaining) + tr(", ") +
                  Unit::distNm(distanceRemaining));
      }
    }

    // Ice ===============================================
    QStringList ice;

    if(userAircaft->getPitotIcePercent() >= 1.f)
      ice.append(tr("Pitot ") + locale.toString(userAircaft->getPitotIcePercent(), 'f', 0) + tr(" %"));

    if(userAircaft->getStructuralIcePercent() >= 1.f)
      ice.append(tr("Structure ") + locale.toString(userAircaft->getStructuralIcePercent(), 'f', 0) + tr(" %"));

    if(userAircaft->getAoaIcePercent() >= 1.f)
      ice.append(tr("AOA ") + locale.toString(userAircaft->getAoaIcePercent(), 'f', 0) + tr(" %"));

    if(userAircaft->getInletIcePercent() >= 1.f)
      ice.append(tr("Inlet ") + locale.toString(userAircaft->getInletIcePercent(), 'f', 0) + tr(" %"));

    if(userAircaft->getPropIcePercent() >= 1.f)
      ice.append(tr("Prop ") + locale.toString(userAircaft->getPropIcePercent(), 'f', 0) + tr(" %"));

    if(userAircaft->getStatIcePercent() >= 1.f)
      ice.append(tr("Static ") + locale.toString(userAircaft->getStatIcePercent(), 'f', 0) + tr(" %"));

    if(userAircaft->getWindowIcePercent() >= 1.f)
      ice.append(tr("Window ") + locale.toString(userAircaft->getWindowIcePercent(), 'f', 0) + tr(" %"));

    if(userAircaft->getCarbIcePercent() >= 1.f)
      ice.append(tr("Carb. ") + locale.toString(userAircaft->getCarbIcePercent(), 'f', 0) + tr(" %"));

    if(ice.isEmpty())
      html.row2(tr("Ice:"), tr("None"));
    else
      html.row2Error(tr("Ice:"), ice.join(tr(", ")));
  } // if(userAircaft != nullptr && info)
  html.tableEnd();

  // Display more text for information display if not online aircraft
  bool longDisplay = info && !aircraft.isOnline();

  // Altitude =========================================================================
  if(aircraft.getCategory() != atools::fs::sc::CARRIER && aircraft.getCategory() != atools::fs::sc::FRIGATE)
  {
    if(longDisplay)
      head(html, tr("Altitude"));
    html.table();

    if(!aircraft.isAnyBoat())
    {
      if(longDisplay && aircraft.getIndicatedAltitudeFt() < atools::fs::sc::SC_INVALID_FLOAT)
        html.row2(tr("Indicated:"), Unit::altFeet(aircraft.getIndicatedAltitudeFt()), ahtml::BOLD);
    }

    if(aircraft.getPosition().getAltitude() < atools::fs::sc::SC_INVALID_FLOAT)
      html.row2(longDisplay ? tr("Actual:") : tr("Altitude:"), Unit::altFeet(aircraft.getPosition().getAltitude()));

    if(!less && userAircaft != nullptr && longDisplay && !aircraft.isAnyBoat())
    {
      if(userAircaft->getAltitudeAboveGroundFt() < atools::fs::sc::SC_INVALID_FLOAT)
        html.row2(tr("Above Ground:"), Unit::altFeet(userAircaft->getAltitudeAboveGroundFt()));
      if(userAircaft->getGroundAltitudeFt() < atools::fs::sc::SC_INVALID_FLOAT)
        html.row2(tr("Ground Elevation:"), Unit::altFeet(userAircaft->getGroundAltitudeFt()));
    }

    if(!less && distanceToTod <= 0 && userAircaft != nullptr)
    {
      // Display vertical path deviation when after TOD
      float vertAlt = route.getAltitudeForDistance(distToDestNm);

      if(vertAlt < map::INVALID_ALTITUDE_VALUE)
      {
        float diff = aircraft.getPosition().getAltitude() - vertAlt;
        QString upDown;
        if(diff >= 100.f)
          upDown = tr(", above <b>▼</b>");
        else if(diff <= -100)
          upDown = tr(", below <b>▲</b>");

        html.row2(tr("Vertical Path Dev.:"), Unit::altFeet(diff) + upDown, ahtml::NO_ENTITIES);
      }
    }
    html.tableEnd();
  }

  // Speed =========================================================================
  if(aircraft.getIndicatedSpeedKts() < atools::fs::sc::SC_INVALID_FLOAT ||
     aircraft.getGroundSpeedKts() < atools::fs::sc::SC_INVALID_FLOAT ||
     aircraft.getTrueAirspeedKts() < atools::fs::sc::SC_INVALID_FLOAT ||
     aircraft.getVerticalSpeedFeetPerMin() < atools::fs::sc::SC_INVALID_FLOAT)
  {

    if(longDisplay)
      head(html, tr("Speed"));
    html.table();
    if(longDisplay && !aircraft.isAnyBoat() && aircraft.getIndicatedSpeedKts() < atools::fs::sc::SC_INVALID_FLOAT)
    {
      if(aircraft.getIndicatedAltitudeFt() < 10000.f && aircraft.getIndicatedSpeedKts() > 251.)
        html.row2Warning(tr("Indicated:"), Unit::speedKts(aircraft.getIndicatedSpeedKts()));
      else
        html.row2(tr("Indicated:"), Unit::speedKts(aircraft.getIndicatedSpeedKts()), ahtml::BOLD);
    }

    if(aircraft.getGroundSpeedKts() < atools::fs::sc::SC_INVALID_FLOAT)
      html.row2(longDisplay ? tr("Ground:") : tr("Groundspeed:"), Unit::speedKts(aircraft.getGroundSpeedKts()));

    if(longDisplay && !aircraft.isAnyBoat())
      if(!less && aircraft.getTrueAirspeedKts() < atools::fs::sc::SC_INVALID_FLOAT)
        html.row2(tr("True Airspeed:"), Unit::speedKts(aircraft.getTrueAirspeedKts()));

    if(!less && !aircraft.isAnyBoat())
    {
      if(longDisplay && aircraft.getMachSpeed() < atools::fs::sc::SC_INVALID_FLOAT)
      {
        float mach = aircraft.getMachSpeed();
        if(mach > 0.4f)
          html.row2(tr("Mach:"), locale.toString(mach, 'f', 3));
        else
          html.row2(tr("Mach:"), tr("-"));
      }

      if(aircraft.getVerticalSpeedFeetPerMin() < atools::fs::sc::SC_INVALID_FLOAT)
      {
        int vspeed = atools::roundToInt(aircraft.getVerticalSpeedFeetPerMin());
        QString upDown;
        if(vspeed >= 100)
          upDown = tr(" <b>▲</b>");
        else if(vspeed <= -100)
          upDown = tr(" <b>▼</b>");

        if(vspeed < 10.f && vspeed > -10.f)
          vspeed = 0.f;

        html.row2(longDisplay ? tr("Vertical:") : tr("Vertical Speed:"), Unit::speedVertFpm(vspeed) + upDown,
                  ahtml::NO_ENTITIES);
      }
    }
    html.tableEnd();
  }

  // Environment =========================================================================
  if(userAircaft != nullptr && longDisplay)
  {
    head(html, tr("Environment"));
    html.table();
    float windSpeed = userAircaft->getWindSpeedKts();
    float windDir = normalizeCourse(userAircaft->getWindDirectionDegT() - userAircaft->getMagVarDeg());
    if(windSpeed >= 1.f)
      html.row2(tr("Wind Direction and Speed:"), courseText(windDir, userAircaft->getWindDirectionDegT()) +
                tr(", ") + Unit::speedKts(windSpeed), ahtml::NO_ENTITIES);
    else
      html.row2(tr("Wind Direction and Speed:"), tr("None"));

    // Head/tail and crosswind =================================================
    float headWind = 0.f, crossWind = 0.f;
    ageo::windForCourse(headWind, crossWind, windSpeed, windDir, userAircaft->getHeadingDegMag());

    if(!less)
    {
      QString windPtr = formatter::windInformation(headWind, crossWind);

      // if(!value.isEmpty())
      // Keep an empty line to avoid flickering
      html.row2(QString(), windPtr, ahtml::NO_ENTITIES);
    }

    // Total air temperature (TAT) is also called: indicated air temperature (IAT) or ram air temperature (RAT)
    float tat = userAircaft->getTotalAirTemperatureCelsius();
    if(tat < 0.f && tat > -0.5f)
      tat = 0.f;
    html.row2(tr("Total Air Temperature:"), locale.toString(tat, 'f', 0) + tr(" °C, ") +
              locale.toString(ageo::degCToDegF(tat), 'f', 0) + tr(" °F"));

    if(!less)
    {
      // Static air temperature (SAT) is also called: outside air temperature (OAT) or true air temperature
      float sat = userAircaft->getAmbientTemperatureCelsius();
      if(sat < 0.f && sat > -0.5f)
        sat = 0.f;
      html.row2(tr("Static Air Temperature:"), locale.toString(sat, 'f', 0) + tr(" °C, ") +
                locale.toString(ageo::degCToDegF(sat), 'f', 0) + tr(" °F"));

      float isaDeviation = sat - ageo::isaTemperature(userAircaft->getPosition().getAltitude());
      if(isaDeviation < 0.f && isaDeviation > -0.5f)
        isaDeviation = 0.f;
      html.row2(tr("ISA Deviation:"), locale.toString(isaDeviation, 'f', 0) + tr(" °C"));
    }

    float slp = userAircaft->getSeaLevelPressureMbar();
    html.row2(tr("Sea Level Pressure:"), locale.toString(slp, 'f', 0) + tr(" hPa, ") +
              locale.toString(ageo::mbarToInHg(slp), 'f', 2) + tr(" inHg"));

    if(!less)
    {
      QStringList precip;
      // if(data.getFlags() & atools::fs::sc::IN_CLOUD) // too unreliable
      // precip.append(tr("Cloud"));
      if(userAircaft->inRain())
        precip.append(tr("Rain"));
      if(userAircaft->inSnow())
        precip.append(tr("Snow"));
      if(precip.isEmpty())
        precip.append(tr("None"));
      html.row2(tr("Conditions:"), precip.join(tr(", ")));

      if(Unit::distMeterF(userAircaft->getAmbientVisibilityMeter()) > 20.f)
        html.row2(tr("Visibility:"), tr("> 20 ") + Unit::getUnitDistStr());
      else
        html.row2(tr("Visibility:"), Unit::distMeter(userAircaft->getAmbientVisibilityMeter(), true /*unit*/, 5));
    }

    html.tableEnd();
  }

  if(!less && longDisplay)
  {
    head(html, tr("Position"));
    addCoordinates(aircraft.getPosition(), html);
    html.tableEnd();
  }
  html.row2AlignRight(false);

#ifdef DEBUG_INFORMATION
  if(info)
  {
    html.hr().pre(
      "distanceToFlightPlan " + QString::number(route.getDistanceToFlightPlan(), 'f', 2) +
      "distFromStartNm " + QString::number(distFromStartNm, 'f', 2) +
      " distToDestNm " + QString::number(distToDestNm, 'f', 1) +
      "\nnearestLegDistance " + QString::number(nearestLegDistance, 'f', 2) +
      " crossTrackDistance " + QString::number(crossTrackDistance, 'f', 2) +
      "\nfuelToDestinationLbs " + QString::number(fuelTime.fuelLbsToDest, 'f', 2) +
      " fuelToDestinationGal " + QString::number(fuelTime.fuelGalToDest, 'f', 2) +
      (distanceToTod > 0.f ?
       ("\ndistanceToTod " + QString::number(distanceToTod, 'f', 2) +
        " fuelToTodLbs " + QString::number(fuelTime.fuelLbsToTod, 'f', 2) +
        " fuelToTodGal " + QString::number(fuelTime.fuelGalToTod, 'f', 2)) : QString())
      );

    html.pre().textBr("TimeToDestValid " + QString::number(fuelTime.isTimeToDestValid()));
    html.textBr("TimeToTodValid " + QString::number(fuelTime.isTimeToTodValid()));
    html.textBr("TimeToNextValid " + QString::number(fuelTime.isTimeToNextValid()));
    html.textBr("FuelToDestValid " + QString::number(fuelTime.isFuelToDestValid()));
    html.textBr("FuelToTodValid " + QString::number(fuelTime.isFuelToTodValid()));
    html.textBr("FuelToNextValid " + QString::number(fuelTime.isFuelToNextValid()));
    html.textBr("estimatedFuel " + QString::number(fuelTime.estimatedFuel));
    html.textBr("estimatedTime " + QString::number(fuelTime.estimatedTime)).preEnd();
  }
#endif
}

QString HtmlInfoBuilder::airportLink(const HtmlBuilder& html, const QString& ident, const QString& name) const
{
  HtmlBuilder builder = html.cleared();
  if(!ident.isEmpty())
  {
    if(info)
    {
      if(name.isEmpty())
        // Ident only
        builder.a(ident, QString("lnm://show?airport=%1").arg(ident), ahtml::LINK_NO_UL);
      else
      {
        // Name and ident
        builder.text(tr("%1 (").arg(name));
        builder.a(ident, QString("lnm://show?airport=%1").arg(ident), ahtml::LINK_NO_UL);
        builder.text(tr(")"));
      }
    }
    else
      builder.text(name.isEmpty() ? ident : tr("%1 (%2)").arg(name).arg(ident));
  }
  return builder.getHtml();
}

void HtmlInfoBuilder::aircraftTitle(const atools::fs::sc::SimConnectAircraft& aircraft, HtmlBuilder& html,
                                    bool moreLessSwitch, bool less)
{
  QIcon icon = NavApp::getVehicleIcons()->iconFromCache(aircraft, symbolSizeVehicle.height(), 45);

  html.tableAtts({
    {"width", "100%"}
  });
  html.tr();
  html.td();
  if(aircraft.isUser())
    html.img(icon, tr("User Vehicle"), QString(), symbolSizeVehicle);
  else if(aircraft.isOnline())
    html.img(icon, tr("Online Client (%1)").arg(NavApp::getOnlineNetworkTranslated()), QString(), symbolSizeVehicle);
  else
    html.img(icon, tr("AI / Multiplayer Vehicle"), QString(), symbolSizeVehicle);
  html.nbsp().nbsp();

  QString title(aircraft.getAirplaneRegistration());
  QString title2 = airplaneType(aircraft);
  QString title3 = aircraft.isOnline() ? NavApp::getOnlineNetworkTranslated() : QString();

  if(!aircraft.getAirplaneModel().isEmpty())
    title2 += (title2.isEmpty() ? "" : ", ") + aircraft.getAirplaneModel();

  if(!title2.isEmpty())
    title += " - " + title2;

  if(!title3.isEmpty())
    title += " (" + title3 + ")";

#ifdef DEBUG_INFORMATION
  static bool heartbeat = false;

  title += (heartbeat ? " X" : " 0");
  heartbeat = !heartbeat;

#endif

  html.text(title, ahtml::BOLD | ahtml::BIG);

  if(info && !print)
  {
    html.nbsp().nbsp();
    html.a(tr("Map"), QString("lnm://show?lonx=%1&laty=%2").
           arg(aircraft.getPosition().getLonX()).arg(aircraft.getPosition().getLatY()),
           ahtml::LINK_NO_UL);
    if(moreLessSwitch)
    {
      // Show more/less links =============================================
      html.tdEnd();

      html.tdAtts({
        {"align", "right"}, {"valign", "top"}
      });

      HtmlBuilder linkHtml(html.cleared());
      linkHtml.a(less ? tr("More") : tr("Less"), QString("lnm://do?lessprogress"),
                 ahtml::LINK_NO_UL);

      HtmlBuilder disabledLinkHtml(html.cleared());
      disabledLinkHtml.text(less ? tr("Less") : tr("More"), ahtml::BOLD);

      if(less)
        html.textHtml(linkHtml).nbsp().nbsp().textHtml(disabledLinkHtml);
      else
        html.textHtml(disabledLinkHtml).nbsp().nbsp().textHtml(linkHtml);
    }
  }
  html.tdEnd();

  html.trEnd();
  html.tableEnd();
}

QString HtmlInfoBuilder::airplaneType(const atools::fs::sc::SimConnectAircraft& aircraft) const
{
  if(!aircraft.getAirplaneType().isEmpty())
    return aircraft.getAirplaneType();
  else
    // Convert model ICAO code to a name
    return atools::fs::util::aircraftTypeForCode(aircraft.getAirplaneModel());
}

void HtmlInfoBuilder::addScenery(const atools::sql::SqlRecord *rec, HtmlBuilder& html, bool ils) const
{
  if(ils)
  {
    head(html, tr("Scenery"));
    html.table();
    html.row2(NavApp::isNavdataAll() ? tr("Navigraph") : tr("Simulator"));
    html.tableEnd();
  }
  else if(rec != nullptr && rec->contains("title") && rec->contains("filepath"))
  {
    head(html, tr("Scenery"));
    html.table();
    html.row2(rec->valueStr("title"), filepathTextShow(rec->valueStr("filepath")), ahtml::NO_ENTITIES);
    html.tableEnd();
  }
}

void HtmlInfoBuilder::addAirportFolder(const MapAirport& airport, HtmlBuilder& html) const
{
  QFileInfoList airportFiles = AirportFiles::getAirportFiles(airport.ident);

  if(!airportFiles.isEmpty())
  {
    head(html, tr("Files"));
    html.table();

    for(const QString& dir : AirportFiles::getAirportFilesBase(airport.ident))
      html.row2(tr("Path:"), filepathTextOpen(dir, true), ahtml::NO_ENTITIES | ahtml::SMALL);

    int i = 0;
    for(const QFileInfo& file : airportFiles)
      html.row2(i++ > 0 ? QString() : tr("Files:"), filepathTextOpen(file, false),
                ahtml::NO_ENTITIES | ahtml::SMALL);
    html.tableEnd();
  }
}

void HtmlInfoBuilder::addAirportSceneryAndLinks(const MapAirport& airport, HtmlBuilder& html) const
{
  // Scenery library entries ============================================
  const atools::sql::SqlRecordVector *sceneryInfo =
    infoQuery->getAirportSceneryInformation(airport.ident);

  if(sceneryInfo != nullptr)
  {
    head(html, tr("Scenery"));
    html.table();

    int i = 0;
    for(const SqlRecord& rec : *sceneryInfo)
    {
      QString title = rec.valueStr("title");
      if(NavApp::getCurrentSimulatorDb() == atools::fs::FsPaths::XPLANE11)
        title = i == 0 ? tr("X-Plane") : QString();

      html.row2If(title, filepathTextShow(rec.valueStr("filepath")), ahtml::NO_ENTITIES);
      i++;
    }
    html.tableEnd();
  }

  // Links ============================================
  QStringList links;
  if(NavApp::getCurrentSimulatorDb() == atools::fs::FsPaths::XPLANE11)
    links.append(html.cleared().a(tr("X-Plane Scenery Gateway"),
                                  QString("https://gateway.x-plane.com/scenery/page/%1").
                                  arg(airport.xpident), ahtml::LINK_NO_UL).getHtml());

  // Skip X-Plane's obscure long idents
  if(airport.ident.size() <= 4 && airport.ident.size() >= 3)
  {
    // Check if airport is in navdata
    MapAirport airportNav = mapQuery->getAirportNav(airport);

    if(airportNav.isValid() && airportNav.navdata)
    {
      links.append(html.cleared().a(tr("SkyVector"), QString("https://skyvector.com/airport/%1").
                                    arg(airportNav.icaoIdent()), ahtml::LINK_NO_UL).getHtml());
      links.append(html.cleared().a(tr("FlightAware"), QString("https://www.flightaware.com/live/airport/%1").
                                    arg(airportNav.icaoIdent()), ahtml::LINK_NO_UL).getHtml());
      links.append(html.cleared().a(tr("OpenNav"), QString("https://opennav.com/airport/%1").
                                    arg(airportNav.icaoIdent()), ahtml::LINK_NO_UL).getHtml());
    }
  }

  // Display link table ===============
  if(!links.isEmpty())
  {
    head(html, tr("Links"));
    html.table();
    for(const QString& link : links)
      html.row2(QString(), link, ahtml::NO_ENTITIES);
    html.tableEnd();
  }
}

QString HtmlInfoBuilder::filepathTextShow(const QString& filepath, const QString& prefix) const
{
  HtmlBuilder link(true);

  if(filepath.isEmpty())
    return QString();

  if(QFileInfo::exists(filepath))
    link.small(prefix).a(filepath, QString("lnm://show?filepath=%1").arg(filepath), ahtml::LINK_NO_UL | ahtml::SMALL);
  else
    link.small(prefix).small(filepath).
    text(tr(" (File not found)"), ahtml::SMALL | ahtml::BOLD);
  return link.getHtml();
}

QString HtmlInfoBuilder::filepathTextOpen(const QFileInfo& filepath, bool showPath) const
{
  HtmlBuilder link(true);

  if(filepath.exists())
    link.a(showPath ? filepath.filePath() : filepath.fileName(),
           QUrl::fromLocalFile(filepath.filePath()).toString(QUrl::EncodeSpaces), ahtml::LINK_NO_UL);
  return link.getHtml();
}

void HtmlInfoBuilder::addCoordinates(const atools::sql::SqlRecord *rec, HtmlBuilder& html) const
{
  if(rec != nullptr && rec->contains("lonx") && rec->contains("laty"))
    addCoordinates(Pos(rec->valueFloat("lonx"), rec->valueFloat("laty"), rec->valueFloat("altitude", 0.f)), html);
}

void HtmlInfoBuilder::addCoordinates(const Pos& pos, HtmlBuilder& html) const
{
  html.row2(tr("Coordinates:"), Unit::coords(pos));

#ifdef DEBUG_INFORMATION
  html.row2(tr("Pos:"), QString("Pos(%1, %2)").arg(pos.getLonX()).arg(pos.getLatY()), ahtml::PRE);
#endif
}

void HtmlInfoBuilder::head(HtmlBuilder& html, const QString& text) const
{
  if(info)
    html.h4(text);
  else
    html.b(text);
}

void HtmlInfoBuilder::navaidTitle(HtmlBuilder& html, const QString& text) const
{
  if(info)
    html.text(text, ahtml::BOLD | ahtml::BIG);
  else
    html.b(text);
}

void HtmlInfoBuilder::rowForFloat(HtmlBuilder& html, const SqlRecord *rec, const QString& colName,
                                  const QString& msg, const QString& val, int precision) const
{
  if(!rec->isNull(colName))
  {
    float i = rec->valueFloat(colName);
    if(i > 0)
      html.row2(msg, val.arg(locale.toString(i, 'f', precision)));
  }
}

void HtmlInfoBuilder::rowForInt(HtmlBuilder& html, const SqlRecord *rec, const QString& colName,
                                const QString& msg, const QString& val) const
{
  if(!rec->isNull(colName))
  {
    int i = rec->valueInt(colName);
    if(i > 0)
      html.row2(msg, val.arg(locale.toString(i)));
  }
}

void HtmlInfoBuilder::rowForBool(HtmlBuilder& html, const SqlRecord *rec, const QString& colName,
                                 const QString& msg, bool expected) const
{
  if(!rec->isNull(colName))
  {
    bool i = rec->valueBool(colName);
    if(i != expected)
      html.row2(msg, QString());
  }
}

void HtmlInfoBuilder::rowForStr(HtmlBuilder& html, const SqlRecord *rec, const QString& colName,
                                const QString& msg, const QString& val) const
{
  if(!rec->isNull(colName))
  {
    QString i = rec->valueStr(colName);
    if(!i.isEmpty())
      html.row2(msg, val.arg(i));
  }
}

void HtmlInfoBuilder::rowForStrCap(HtmlBuilder& html, const SqlRecord *rec, const QString& colName,
                                   const QString& msg, const QString& val) const
{
  if(!rec->isNull(colName))
  {
    QString i = rec->valueStr(colName);
    if(!i.isEmpty())
      html.row2(msg, val.arg(capString(i)));
  }
}

void HtmlInfoBuilder::addMetarLine(atools::util::HtmlBuilder& html, const QString& header,
                                   const map::MapAirport& airport, const QString& metar, const QString& station,
                                   const QDateTime& timestamp, bool fsMetar, bool mapDisplay) const
{
  if(!metar.isEmpty())
  {
    Metar m(metar, station, timestamp, fsMetar);
    const atools::fs::weather::MetarParser& parsed = m.getParsedMetar();

    if(!parsed.isValid())
      qWarning() << "Metar is not valid";

    HtmlBuilder whtml = html.cleared();
    whtml.text(fsMetar ? m.getCleanMetar() : metar);

    whtml.nbsp().nbsp();
    if(mapDisplay)
      whtml.b();
    else
      whtml.small();
    whtml.text(tr("("));
    addFlightRulesSuffix(whtml, m, mapDisplay);
    whtml.text(tr(")"));
    if(mapDisplay)
      whtml.bEnd();
    else
      whtml.smallEnd();

    // Add METAR suffix for tooltip
    bestRunwaysText(airport, whtml, parsed, 4 /* max entries */, false /* details */);
    html.row2(header + (info ? tr(":") : tr(" METAR:")), whtml);

  }
}

void HtmlInfoBuilder::addFlightRulesSuffix(atools::util::HtmlBuilder& html,
                                           const atools::fs::weather::Metar& metar, bool mapDisplay) const
{
  if(!print)
  {
    html.img(SymbolPainter().createAirportWeatherIcon(metar, symbolSize.height()),
             QString(), QString(), symbolSize);
    html.nbsp();
  }

  html.text(metar.getParsedMetar().getFlightRulesString());
  if(mapDisplay)
    html.nbsp().text(tr("-")).nbsp().text(tr("Map"));
}

void HtmlInfoBuilder::addMorse(atools::util::HtmlBuilder& html, const QString& name, const QString& code) const
{
  html.row2(name, morse->getCode(code), ahtml::BOLD | ahtml::NO_ENTITIES);
}

void HtmlInfoBuilder::routeWindText(HtmlBuilder& html, const Route& route, int index) const
{
  if(index >= 0)
  {
    // Wind text is always shown independent of barb status at route
    if(NavApp::getWindReporter()->hasWindData() && route.hasValidProfile())
    {
      const RouteAltitudeLeg& altLeg = route.getAltitudeLegAt(index);
      if(altLeg.getLineString().getPos2().getAltitude() > MIN_WIND_BARB_ALTITUDE && !altLeg.isMissed() &&
         !altLeg.isAlternate())
      {
        atools::grib::WindPos wp;
        wp.pos = altLeg.getLineString().getPos2();
        wp.wind.dir = altLeg.getWindDirection();
        wp.wind.speed = altLeg.getWindSpeed();

        windText({wp}, html, altLeg.getLineString().getPos2().getAltitude(),
                 NavApp::getWindReporter()->getSourceText());
      }
    }
  }
}

QString HtmlInfoBuilder::strJoinVal(const QStringList& list) const
{
  return atools::strJoin(list, tr(", "));
}

QString HtmlInfoBuilder::strJoinHdr(const QStringList& list) const
{
  return atools::strJoin(list, tr(", "), tr(" and "), tr(":"));
}

QString HtmlInfoBuilder::identRegionText(const QString& ident, const QString& region) const
{
  if(region.isEmpty())
    return ident;
  else
    return tr("%1/%2").arg(ident).arg(region);
}
