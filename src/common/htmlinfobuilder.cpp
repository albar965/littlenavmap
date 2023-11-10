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

#include "common/htmlinfobuilder.h"

#include "airspace/airspacecontroller.h"
#include "atools.h"
#include "common/airportfiles.h"
#include "common/formatter.h"
#include "common/fueltool.h"
#include "common/htmlinfobuilderflags.h"
#include "common/mapcolors.h"
#include "common/maptools.h"
#include "common/maptypes.h"
#include "common/symbolpainter.h"
#include "common/unit.h"
#include "common/vehicleicons.h"
#include "fs/bgl/ap/rw/runway.h"
#include "fs/online/onlinetypes.h"
#include "fs/userdata/userdatamanager.h"
#include "fs/util/fsutil.h"
#include "fs/util/morsecode.h"
#include "fs/util/tacanfrequencies.h"
#include "fs/weather/metar.h"
#include "fs/weather/metarparser.h"
#include "geo/calculations.h"
#include "logbook/logdatacontroller.h"
#include "mapgui/mappaintwidget.h"
#include "app/navapp.h"
#include "online/onlinedatacontroller.h"
#include "perf/aircraftperfcontroller.h"
#include "query/airportquery.h"
#include "query/airwaytrackquery.h"
#include "query/infoquery.h"
#include "query/mapquery.h"
#include "query/waypointtrackquery.h"
#include "route/route.h"
#include "route/routealtitude.h"
#include "userdata/userdatacontroller.h"
#include "userdata/userdataicons.h"
#include "util/htmlbuilder.h"
#include "weather/windreporter.h"
#include "weather/weathercontext.h"

#include <QSize>
#include <QUrl>
#include <QFileInfo>
#include <QToolTip>
#include <QRegularExpression>
#include <QDir>

// Use % to concatenate strings faster than +
#include <QStringBuilder>

using namespace map;
using atools::sql::SqlRecord;
using atools::sql::SqlRecordList;
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
using formatter::courseTextFromTrue;

namespace ahtml = atools::util::html;
namespace ageo = atools::geo;

// Limits for nearest airports and navaids to airport
const float NEAREST_MAX_DISTANCE_AIRPORT_NM = 75.f;
const float NEAREST_MAX_DISTANCE_NAVAID_NM = 50.f;
const int NEAREST_MAX_NUM_AIRPORT = 10;
const int NEAREST_MAX_NUM_NAVAID = 15;

// Print weather time in red or orange if older than this
const qint64 WEATHER_MAX_AGE_HOURS_WARN = 3;
const qint64 WEATHER_MAX_AGE_HOURS_ERR = 6;

// Maximum distance for bearing display to user aircraft
const int MAX_DISTANCE_FOR_BEARING_METER = ageo::nmToMeter(8000);

HtmlInfoBuilder::HtmlInfoBuilder(QWidget *parent, MapPaintWidget *mapWidgetParam, bool infoParam, bool printParam, bool verboseParam)
  : parentWidget(parent), mapWidget(mapWidgetParam), info(infoParam), print(printParam), verbose(verboseParam)
{
  if(info)
    verbose = true;

  infoQuery = NavApp::getInfoQuery();
  airportQuerySim = NavApp::getAirportQuerySim();
  airportQueryNav = NavApp::getAirportQueryNav();

  morse = new MorseCode("&nbsp;", "&nbsp;&nbsp;&nbsp;");
}

HtmlInfoBuilder::~HtmlInfoBuilder()
{
  delete morse;
}

void HtmlInfoBuilder::airportTitle(const MapAirport& airport, HtmlBuilder& html, int rating, bool procedures) const
{
  if(!print)
  {
    html.img(SymbolPainter::createAirportIcon(airport, symbolSizeTitle.height()), QString(), QString(), symbolSizeTitle);
    html.nbsp().nbsp();
  }

  // Adapt title to airport status
  Flags titleFlags = ahtml::BOLD;
  if(airport.closed())
    titleFlags |= ahtml::STRIKEOUT;
  if(airport.addon())
    titleFlags |= ahtml::ITALIC | ahtml::UNDERLINE;

  if(info)
  {
    html.text(tr("%1 (%2)").arg(airport.name).arg(airport.displayIdent()), titleFlags | ahtml::BIG);
    html.nbsp().nbsp();
    if(rating != -1)
      html.text(atools::ratingString(rating, 5)).nbsp().nbsp();
  }

  if(info)
  {
    if(!print)
    {
      // Add link to map
      html.a(tr("Map"), QString("lnm://show?id=%1&type=%2").arg(airport.id).arg(map::AIRPORT), ahtml::BOLD | ahtml::LINK_NO_UL);

      if(procedures)
      {
        bool departure = false, destination = false, arrivalProc = false, departureProc = false;
        proc::procedureFlags(NavApp::getRouteConst(), &airport, &departure, &destination, nullptr, nullptr, &arrivalProc, &departureProc);

        QString linkText, link;
        if(departure && departureProc)
        {
          // Is departure airport and has procedures
          linkText = tr("Departure Procedures");
          link = "showprocsdepart";
        }
        else if(destination && arrivalProc)
        {
          // Is destination airport and has procedures
          linkText = tr("Arrival Procedures");
          link = "showprocsarrival";
        }
        else if(arrivalProc || departureProc)
        {
          // Neiter departure nor arrival and has any procedures
          linkText = tr("Procedures");
          link = "showprocs";
        }

        if(!link.isEmpty())
          html.text(tr(", ")).a(linkText, QString("lnm://%1?id=%2&type=%3").
                                arg(link).arg(airport.id).arg(map::AIRPORT), ahtml::BOLD | ahtml::LINK_NO_UL);
      }
    }
  }
  else
  {
    html.text(tr("%1 (%2)").arg(airport.name).arg(airport.displayIdent()), titleFlags);
    if(rating != -1)
      html.nbsp().nbsp().text(atools::ratingString(rating, 5));
  }
}

void HtmlInfoBuilder::flightplanWaypointRemarks(HtmlBuilder& html, int index) const
{
  if(index >= 0)
    html.row2If(tr("Flight Plan Pos. Remarks:"),
                atools::elideTextLinesShort(NavApp::getRouteConst().getFlightplanConst().at(index).getComment(), 5, 40));
}

void HtmlInfoBuilder::airportText(const MapAirport& airport, const map::WeatherContext& weatherContext,
                                  HtmlBuilder& html, const Route *route) const
{
  MapQuery *mapQuery = mapWidget->getMapQuery();
  const SqlRecord *rec = infoQuery->getAirportInformation(airport.id);
  int rating = -1;

  if(rec != nullptr)
    rating = rec->valueInt("rating");

  airportTitle(airport, html, rating, true /* procedures */);

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
  }

  if(!print && info)
    bearingToUserText(airport.position, airport.magvar, html);

  // Idents and codes ======================
  if(verbose)
  {
    const QString displayIdent = airport.displayIdent();
    if(info || airport.icao != displayIdent)
      html.row2If(tr("ICAO:"), airport.icao);

    if(info || airport.faa != displayIdent)
      html.row2If(tr("FAA:"), airport.faa);

    if(info || airport.iata != displayIdent)
      html.row2If(tr("IATA:"), airport.iata);

    if(info || airport.local != displayIdent)
      html.row2If(tr("Local Code:"), airport.local);

    if(airport.xplane && (info || airport.ident != displayIdent))
      html.row2If(tr("X-Plane Ident:"), airport.ident);
  }

  if(info)
    html.row2If(tr("Region:"), airport.region);

  // Administrative information ======================
  QString city, state, country;
  airportQuerySim->getAirportAdminNamesById(airport.id, city, state, country);
  if(verbose)
  {
    html.row2If(tr("City:"), city);
    html.row2If(tr("State or Province:"), state);
  }

  html.row2If(tr("Country or Area Code:"), country);
  html.row2(tr("Elevation:"), Unit::altFeet(airport.getAltitude()));

  if(verbose)
    html.row2(tr("Magnetic declination:"), map::magvarText(airport.magvar));

  if(info)
  {
    // Try transition altitude from nav database
    float transitionAltitude = 0.f, transitionLevel = 0.f;
    mapQuery->getAirportTransitionAltiudeAndLevel(airport, transitionAltitude, transitionLevel);
    if(transitionAltitude > 0.f)
      // Transition Altitude is the altitude when flying where you are required to change from a
      // local QNH to the standard of 1013.25 hectopascals or 29.92 inches of mercury
      html.row2(tr("Transition altitude:"), Unit::altFeet(transitionAltitude));
    if(transitionLevel > 0.f)
      // Transition Level is the altitude when flying where you are required to change from
      // standard of 1013 back to the local QNH. This is above the Transition Altitude.
      html.row2(tr("Transition level:"), tr("%1 (FL%2)").
                arg(Unit::altFeet(transitionLevel)).arg(transitionLevel / 100.f, 3, 'f', 0, QChar('0')));

    // Sunrise and sunset ===========================
    QDateTime datetime = NavApp::isConnectedAndAircraft() ? NavApp::getUserAircraft().getZuluTime() : QDateTime::currentDateTimeUtc();

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

  if(airport.xplane)
  {
    QString type;
    switch(airport.type)
    {
      case map::AP_TYPE_LAND:
        type = tr("Land airport");
        break;

      case map::AP_TYPE_SEAPLANE:
        type = tr("Seaplane base");
        break;

      case map::AP_TYPE_HELIPORT:
        type = tr("Heliport");
        break;

      case map::AP_TYPE_NONE:
        break;
    }
    facilities.append(tr("X-Plane %1").arg(type));
  }

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
    facilities.append(airport.xplane ? tr("Tower Viewpoint") : tr("Tower Object"));
  if(airport.parking())
    facilities.append(tr("Parking"));
  if(airport.helipad())
    facilities.append(tr("Helipads"));

  if(NavApp::getCurrentSimulatorDb() == atools::fs::FsPaths::MSFS && airport.flags.testFlag(AP_AVGAS) && airport.flags.testFlag(AP_JETFUEL))
    facilities.append(tr("Fuel"));
  else
  {
    if(airport.flags.testFlag(AP_AVGAS))
      facilities.append(tr("Avgas"));
    if(airport.flags.testFlag(AP_JETFUEL))
      facilities.append(tr("Jetfuel"));
  }

  if(mapQuery->hasProcedures(airport))
    facilities.append(tr("Procedures"));
  if(airport.flags.testFlag(AP_ILS))
    facilities.append(tr("ILS"));
  if(airport.vasi())
    facilities.append(tr("VASI"));
  if(airport.als())
    facilities.append(tr("ALS"));
  if(airport.flatten == 1)
    facilities.append(tr("Flatten"));
  if(airport.flatten == 0)
    facilities.append(tr("No Flatten"));
  if(facilities.isEmpty())
    facilities.append(tr("None"));

  facilities.sort();

  html.row2(info ? QString() : tr("Facilities:"), facilities.join(tr(", ")));

  html.tableEnd();

  // Runways ==================================================================================
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

  // Weather ==================================================================================
  if(!weatherContext.isEmpty())
  {
    if(info)
      head(html, tr("Weather"));
    html.table();

    // Source for map icon display
    MapWeatherSource src = NavApp::getMapWeatherSource();
    addMetarLines(html, weatherContext, src, airport);
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
      html.row2(tr("Heading:", "runway heading"), courseTextFromTrue(hdg, airport.magvar) % tr(", ") %
                courseTextFromTrue(opposedCourseDeg(hdg), airport.magvar), ahtml::NO_ENTITIES);
      html.row2If(tr("Surface:"), map::surfaceName(rec->valueStr("longest_runway_surface")));
    }
    html.tableEnd();
  }

  // Add most important COM frequencies ================================================================
  if((airport.towerFrequency > 0 || airport.atisFrequency > 0 || airport.awosFrequency > 0 ||
      airport.asosFrequency > 0 || airport.unicomFrequency > 0) && verbose)
  {
    if(info)
      head(html, tr("COM Frequencies"));
    html.table();
    if(airport.towerFrequency > 0)
      html.row2(tr("Tower:"), locale.toString(roundComFrequency(airport.towerFrequency), 'f', 3) % tr(" MHz"));
    if(airport.atisFrequency > 0)
      html.row2(tr("ATIS:"), locale.toString(roundComFrequency(airport.atisFrequency), 'f', 3) % tr(" MHz"));
    if(airport.awosFrequency > 0)
      html.row2(tr("AWOS:"), locale.toString(roundComFrequency(airport.awosFrequency), 'f', 3) % tr(" MHz"));
    if(airport.asosFrequency > 0)
      html.row2(tr("ASOS:"), locale.toString(roundComFrequency(airport.asosFrequency), 'f', 3) % tr(" MHz"));
    if(airport.unicomFrequency > 0)
      html.row2(tr("UNICOM:"), locale.toString(roundComFrequency(airport.unicomFrequency), 'f', 3) % tr(" MHz"));
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

  if(!info)
    routeWindText(html, *route, airport.routeIndex);

#ifdef DEBUG_INFORMATION
  if(info)
  {
    MapAirport airportNav = mapQuery->getAirportNav(airport);

    html.small(QString("Database: airport_id = %1, ident = %2, navdata %3, xp %4)").
               arg(airport.getId()).arg(airport.ident).arg(airport.navdata).arg(airport.xplane)).br();
    html.small(QString("Navdatabase: airport_id = %1, ident = %2, navdata %3, xp %4)").
               arg(airportNav.getId()).arg(airportNav.ident).arg(airportNav.navdata).arg(airportNav.xplane)).br();
  }
#endif
}

void HtmlInfoBuilder::nearestText(const MapAirport& airport, HtmlBuilder& html) const
{
  // Do not display as tooltip
  if(info && airport.isValid())
  {
    if(!print)
      airportTitle(airport, html, -1, true /* procedures */);

    // Get nearest airports that have procedures ====================================
    const MapResultIndex *nearestAirportsNav =
      airportQueryNav->getNearestProcAirports(airport.position, airport.ident, NEAREST_MAX_DISTANCE_AIRPORT_NM);

    if(!nearestMapObjectsText(airport, html, nearestAirportsNav, tr("Nearest Airports with Procedures"), false, true,
                              NEAREST_MAX_NUM_AIRPORT))
      html.p().b(tr("No airports with procedures within a radius of %1.").arg(Unit::distNm(NEAREST_MAX_DISTANCE_AIRPORT_NM * 4.f))).pEnd();

    // Get nearest VOR and NDB ====================================
    MapResultIndex *nearestNavaids =
      mapWidget->getMapQuery()->getNearestNavaids(airport.position, NEAREST_MAX_DISTANCE_NAVAID_NM,
                                                  map::VOR | map::NDB | map::ILS, 3 /* maxIls */, 4.f /* maxIlsDistNm */);

    if(!nearestMapObjectsText(airport, html, nearestNavaids, tr("Nearest Radio Navaids"), true, false, NEAREST_MAX_NUM_NAVAID))
      html.p().b(tr("No navaids within a radius of %1.").arg(Unit::distNm(NEAREST_MAX_DISTANCE_NAVAID_NM * 4.f))).pEnd();
  }
}

void HtmlInfoBuilder::nearestMapObjectsTextRow(const MapAirport& airport, HtmlBuilder& html,
                                               const QString& type, const QString& displayIdent, const QString& name,
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
    html.td(displayIdent, ahtml::BOLD);
    html.tdF(ahtml::ALIGN_RIGHT).a(name, url, ahtml::LINK_NO_UL).tdEnd();
  }
  else
  {
    // Navaid type
    html.td(type, ahtml::BOLD);
    html.td(displayIdent, ahtml::BOLD);
    html.tdF(ahtml::ALIGN_RIGHT).a(name, url, ahtml::LINK_NO_UL).tdEnd();
  }

  if(frequencyCol)
    html.td(freq, ahtml::ALIGN_RIGHT);

  html.td(courseTextFromTrue(bearingTrue, magVar), ahtml::ALIGN_RIGHT | ahtml::NO_ENTITIES).
  td(Unit::distMeter(distance, false), ahtml::ALIGN_RIGHT).
  trEnd();
}

bool HtmlInfoBuilder::nearestMapObjectsText(const MapAirport& airport, HtmlBuilder& html, const map::MapResultIndex *nearestNav,
                                            const QString& header, bool frequencyCol, bool airportCol, int maxRows) const
{
  if(nearestNav != nullptr && !nearestNav->isEmpty())
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
    for(const map::MapBase *baseNav : *nearestNav)
    {
      if(row++ > maxRows)
        // Stop at max
        break;

      // Airport ======================================
      const map::MapAirport *apNav = baseNav->asPtr<map::MapAirport>();
      if(apNav != nullptr)
      {
        // Airport comes from navdatabase having procedures - convert to simulator airport to get sim name
        // Convert navdatabase airport to simulator
        map::MapAirport apSim = mapWidget->getMapQuery()->getAirportSim(*apNav);

        // Omit center airport used as reference
        if(apSim.isValid() && apSim.id != airport.id)
          nearestMapObjectsTextRow(airport, html, QString(), apSim.displayIdent(), apSim.name, QString(), &apSim, apSim.magvar,
                                   frequencyCol, airportCol);
      }

      const map::MapVor *vor = baseNav->asPtr<map::MapVor>();
      if(vor != nullptr)
        nearestMapObjectsTextRow(airport, html, map::vorType(*vor), vor->ident, vor->name, locale.toString(vor->frequency / 1000., 'f', 2),
                                 vor, vor->magvar, frequencyCol, airportCol);

      const map::MapNdb *ndb = baseNav->asPtr<map::MapNdb>();
      if(ndb != nullptr)
        nearestMapObjectsTextRow(airport, html, tr("NDB"), ndb->ident, ndb->name, locale.toString(ndb->frequency / 100., 'f', 1),
                                 ndb, ndb->magvar, frequencyCol, airportCol);

      const map::MapWaypoint *waypoint = baseNav->asPtr<map::MapWaypoint>();
      if(waypoint != nullptr)
        nearestMapObjectsTextRow(airport, html, tr("Waypoint"), waypoint->ident, QString(), QString(), waypoint, waypoint->magvar,
                                 frequencyCol, airportCol);

      const map::MapIls *ils = baseNav->asPtr<map::MapIls>();
      if(ils != nullptr && !ils->isAnyGlsRnp())
        nearestMapObjectsTextRow(airport, html, map::ilsType(*ils, true /* gs */, true /* dme */, tr(", ")), ils->ident, ils->name,
                                 ils->freqMHzLocale(), ils, ils->magvar, frequencyCol, airportCol);
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
      airportTitle(airport, html, -1, true /* procedures */);

    const SqlRecordList *recVector = infoQuery->getComInformation(airport.id);
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
        html.td(locale.toString(roundComFrequency(rec.valueInt("frequency")), 'f', 3) % tr(" MHz")
#ifdef DEBUG_INFORMATION
                % " [" % QString::number(rec.valueInt("frequency")) % "]"
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

      if(info)
        // Add scenery indicator to clear source - either nav or sim
        addScenery(nullptr, html, DATASOURCE_COM);
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
    infoQuery->getRunwayEnds(ends, airport.id);

  if(!ends.isEmpty())
  {
    max = std::min(ends.size(), max);

    if(details)
    {
      // Table header for detailed view
      head(html, ends.getTotalNumber() == 1 ? tr("Best runway for wind") : tr("Best runways for wind"));
      html.table();

      html.tr().th(ends.getTotalNumber() == 1 ? tr("Runway") : tr("Runways")).
      th(tr("Surface")).th(tr("Length")).th(tr("Headwind")).th(tr("Crosswind")).trEnd();

      // Create runway table for details =====================================
      // Table for detailed view
      int num = 0;
      for(const maptools::RwEnd& end : qAsConst(ends))
      {
        // Stop at maximum number - tailwind is alread sorted out
        if(num > max)
          break;

        QString lengthTxt;
        if(end.minlength == end.maxlength)
          // Not grouped
          lengthTxt = Unit::distShortFeet(end.minlength);
        else
          // Grouped runways have a min and max length
          lengthTxt = tr("%1-%2").
                      arg(Unit::distShortFeet(end.minlength, false, false)).
                      arg(Unit::distShortFeet(end.maxlength));

        // Table entry ==================
        html.tr(QColor()).
        td(end.names.join(tr(", ")), ahtml::BOLD).
        td(end.softRunway ? tr("Soft") : tr("Hard")).
        td(lengthTxt, ahtml::ALIGN_RIGHT).
        td(formatter::windInformationTailHead(end.headWind), ahtml::ALIGN_RIGHT).
        td(Unit::speedKts(end.crossWind), ahtml::ALIGN_RIGHT).
        trEnd();
        num++;
      }
      html.tableEnd();
    }
    else
    {
      // Simple runway list for tooltips only with headwind > 2
      QStringList runways = ends.getSortedRunways(2);

      if(!runways.isEmpty())
      {
        html.br().b((ends.getTotalNumber() == 1 ? tr(" Wind prefers runway: ") : tr(" Wind prefers runways: "))).
        text(atools::strJoin(runways.mid(0, 4), tr(", "), tr(" and ")));
      }
    }
  }
  else if(details)
    // Either crosswind is equally strong and/or headwind too low or no directional wind
    head(html, tr("All runways good for takeoff and landing."));
}

void HtmlInfoBuilder::runwayText(const MapAirport& airport, HtmlBuilder& html, bool details, bool soft) const
{
  if(info && infoQuery != nullptr)
  {
    if(!print)
      airportTitle(airport, html, -1, true /* procedures */);
    html.br().br().b(tr("Elevation: ")).text(Unit::altFeet(airport.getAltitude())).br();

    const SqlRecordList *recVector = infoQuery->getRunwayInformation(airport.id);
    if(recVector != nullptr)
    {
      // Runways =========================================================================
      for(const SqlRecord& rec : *recVector)
      {
        if(!soft && !map::isHardSurface(rec.valueStr("surface")))
          continue;

        const SqlRecord *recPrim = infoQuery->getRunwayEndInformation(rec.valueInt("primary_end_id"));
        const SqlRecord *recSec = infoQuery->getRunwayEndInformation(rec.valueInt("secondary_end_id"));
        bool closedPrim = recPrim->valueBool("has_closed_markings");
        bool closedSec = recSec->valueBool("has_closed_markings");

        html.br().text(tr("Runway ") % recPrim->valueStr("name") % ", " % recSec->valueStr("name"),
                       ((closedPrim && closedSec) ? ahtml::STRIKEOUT : ahtml::NONE)
                       | ahtml::UNDERLINE | ahtml::BIG | ahtml::BOLD);
        html.table();

        html.row2(tr("Size:"), Unit::distShortFeet(rec.valueFloat("length"), false) %
                  tr(" x ") %
                  Unit::distShortFeet(rec.valueFloat("width")));

        html.row2If(tr("Surface:"), strJoinVal({map::surfaceName(rec.valueStr("surface")),
                                                map::smoothnessName(rec.value("smoothness", QVariant(QVariant::Double)))}));

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

          runwayEndText(html, airport, recPrim, rec.valueFloat("heading"), rec.valueFloat("length"), false /* secondary */);
          if(print)
            html.tdEnd().td();

          runwayEndText(html, airport, recSec, opposedCourseDeg(rec.valueFloat("heading")), rec.valueFloat("length"), true /* secondary */);

          if(print)
            html.tdEnd().trEnd().tableEnd();
        }
        if(info)
        {
#ifdef DEBUG_INFORMATION
          html.small(QString("Database: runway_id = %1").arg(rec.valueInt("runway_id"))).br();
          html.small(QString("Database: Primary runway_end_id = %1").arg(recPrim->valueInt("runway_end_id"))).br();
          html.small(QString("Database: Secondary runway_end_id = %1").arg(recSec->valueInt("runway_end_id"))).br();
#endif
        }
      }
    }
    else
      html.p(tr("Airport has no runway."));

    if(details)
    {
      // Helipads ==============================================================
      const SqlRecordList *heliVector = infoQuery->getHelipadInformation(airport.id);
      if(heliVector != nullptr)
      {
        for(const SqlRecord& heliRec : *heliVector)
        {
          bool closed = heliRec.valueBool("is_closed");
          bool hasStart = !heliRec.isNull("start_number");

          QString num = hasStart ? " " % heliRec.valueStr("runway_name") : tr(" (no Start Position)");

          html.h3(tr("Helipad%1").arg(num),
                  (closed ? ahtml::STRIKEOUT : ahtml::NONE)
                  | ahtml::UNDERLINE);
          html.nbsp().nbsp();

          Pos pos(heliRec.valueFloat("lonx"), heliRec.valueFloat("laty"));

          if(!print)
            html.a(tr("Map"), QString("lnm://show?lonx=%1&laty=%2").
                   arg(pos.getLonX()).arg(pos.getLatY()), ahtml::BOLD | ahtml::LINK_NO_UL).br();

          if(closed)
            html.text(tr("Is Closed"));
          html.table();

          html.row2(tr("Size:"), Unit::distShortFeet(heliRec.valueFloat("width"), false) %
                    tr(" x ") %
                    Unit::distShortFeet(heliRec.valueFloat("length")));
          html.row2If(tr("Surface:"), map::surfaceName(heliRec.valueStr("surface")) %
                      (heliRec.valueBool("is_transparent") ? tr(" (Transparent)") : QString()));
          html.row2If(tr("Type:"), atools::capString(heliRec.valueStr("type")));
          html.row2(tr("Heading:", "runway heading"), courseTextFromTrue(heliRec.valueFloat("heading"), airport.magvar),
                    ahtml::NO_ENTITIES);
          html.row2(tr("Elevation:"), Unit::altFeet(heliRec.valueFloat("altitude")));

          addCoordinates(&heliRec, html);
          html.tableEnd();
        }
      }
      else
        html.p(tr("Airport has no helipad."));

      // Start positions ==============================================================
      const SqlRecordList *startVector = infoQuery->getStartInformation(airport.id);

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
            html.a(startText, QString("lnm://show?lonx=%1&laty=%2").
                   arg(pos.getLonX()).arg(pos.getLatY()), ahtml::LINK_NO_UL);
          i++;
        }
      }
      else
        html.p(tr("Airport has no start position."));
    }
  }
}

void HtmlInfoBuilder::runwayEndText(HtmlBuilder& html, const MapAirport& airport, const SqlRecord *rec,
                                    float hdgPrimTrue, float length, bool secondary) const
{
  bool closed = rec->valueBool("has_closed_markings");

  QString name = rec->valueStr("name");
  html.br().br().text(name, (closed ? (ahtml::STRIKEOUT) : ahtml::NONE) | ahtml::BOLD | ahtml::BIG);
  html.table();

  if(name.endsWith('T'))
    html.row2(tr("Uses true course"), QString());

  if(closed)
    html.row2(tr("Closed"), QString());

  bool forceBoth = std::abs(airport.magvar) > 90.f;
  html.row2(tr("Heading:", "runway heading"),
            courseTextFromTrue(hdgPrimTrue, airport.magvar, true /* magBold */, false /* magBig */, true /* trueSmall */,
                               false /* narrow */, forceBoth),
            ahtml::NO_ENTITIES);

  float threshold = rec->valueFloat("offset_threshold");
  if(threshold > 1.f)
  {
    html.row2(tr("Offset Threshold:"), Unit::distShortFeet(threshold));
    html.row2(tr("Available Distance for Landing:"), Unit::distShortFeet(length - threshold));
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

#ifdef DEBUG_INFORMATION
  html.row2("[secondary]", secondary);
#else
  Q_UNUSED(secondary)
#endif
  html.tableEnd();

  // Show none, one or more ILS
  const QVector<map::MapIls> ilsVector = mapWidget->getMapQuery()->getIlsByAirportAndRunway(airport.ident, rec->valueStr("name"));
  for(const map::MapIls& ils : ilsVector)
    ilsTextRunwayInfo(ils, html);
}

void HtmlInfoBuilder::ilsTextProcInfo(const map::MapIls& ils, atools::util::HtmlBuilder& html) const
{
  ilsTextInternal(ils, html, true /* procInfo */, false /* runwayInfo */, false /* infoOrTooltip */);
}

void HtmlInfoBuilder::ilsTextRunwayInfo(const map::MapIls& ils, atools::util::HtmlBuilder& html) const
{
  ilsTextInternal(ils, html, false /* procInfo */, true /* runwayInfo */, false /* infoOrTooltip */);
}

void HtmlInfoBuilder::ilsTextInfo(const map::MapIls& ils, atools::util::HtmlBuilder& html) const
{
  ilsTextInternal(ils, html, false /* procInfo */, false /* runwayInfo */, true /* infoOrTooltip */);
}

void HtmlInfoBuilder::ilsTextInternal(const map::MapIls& ils, atools::util::HtmlBuilder& html, bool procInfo,
                                      bool runwayInfo, bool infoOrTooltip) const
{
  Q_UNUSED(runwayInfo)

  // Add ILS information

  // Prefix for procedure information
  QString prefix = procInfo ? map::ilsTypeShort(ils) % tr(" ") : QString();
  QString text = map::ilsTextShort(ils);

  // Check if text is to be generated for navaid or procedures tab
  if(infoOrTooltip)
  {
    html.img(map::ilsIcon(ils), QString(), QString(), symbolSizeTitle);
    html.nbsp().nbsp();

    navaidTitle(html, text);

    if(info)
    {
      // Add map link if not tooltip ==========================================
      html.nbsp().nbsp();
      html.a(tr("Map"), QString("lnm://show?id=%1&type=%2").arg(ils.id).arg(map::ILS),
             ahtml::BOLD | ahtml::LINK_NO_UL);
    }
    html.table();
  }
  else
  {
    if(procInfo)
      html.row2(prefix % tr("Type: "), map::ilsType(ils, true /* gs */, true /* dme */, tr(", ")));
    else
    {
      html.br().br().text(text, ahtml::BOLD | ahtml::BIG);
      html.table();
    }
  }

  if(infoOrTooltip)
  {
    if(!print && info)
      bearingToUserText(ageo::Pos(ils.position.getLonX(), ils.position.getLatY()), ils.magvar, html);

    // ILS information ==================================================
    QString runway = ils.runwayName;
    QString airportIdent = ils.airportIdent;

    if(runway.isEmpty())
      html.row2If(tr("Airport:"), airportIdent, ahtml::BOLD);
    else
      html.row2If(tr("Airport and runway:"), tr("%1, %2").arg(airportIdent).arg(runway), ahtml::BOLD);

    if(info)
      html.row2If(tr("Region:"), ils.region);
  }

  if(ils.isAnyGlsRnp())
    html.row2(prefix % tr("Channel:"), ils.freqMHzOrChannelLocale());
  else
    html.row2(prefix % tr("Frequency:"), ils.freqMHzLocale() % tr(" MHz"));

  if(!procInfo && verbose)
  {
    html.row2(tr("Magnetic declination:"), map::magvarText(ils.magvar));

    if(!ils.isAnyGlsRnp())
    {
      if(ils.range > 0)
        html.row2(tr("Range:"), Unit::distNm(ils.range));

      addMorse(html, tr("Morse:"), ils.ident);
      if(ils.hasBackcourse)
        html.row2(tr("Has Backcourse"), QString());
    }
  }

  // Localizer information ==================================================
  float hdgTrue = ils.heading;

  html.row2(prefix % (ils.isAnyGlsRnp() ? tr("Heading:", "localizer heading") : tr("Localizer Heading:")),
            courseTextFromTrue(hdgTrue, ils.magvar), ahtml::NO_ENTITIES);

  // Check for offset localizer ==================================================
  map::MapRunwayEnd end;
  if(!ils.isAnyGlsRnp())
  {
    if(ils.width > 0.1f && info)
      html.row2(tr("Width:"), locale.toString(ils.localizerWidth(), 'f', 2) % tr("°"));

    int endId = ils.runwayEndId;
    if(endId > 0)
    {
      // Get assigned runway end =====================
      end = airportQueryNav->getRunwayEndById(endId); // ILS are sourced from navdatabase
      if(end.isValid())
      {
        // The maximum angular offset for a LOC is 3° for FAA and 5° for ICAO.
        // Everything else is named either LDA (FAA) or IGS (ICAO).
        if(ageo::angleAbsDiff(end.heading, ageo::normalizeCourse(ils.heading)) > 2.f)
        {
          // Get airport for consistent magnetic variation =====================
          map::MapAirport airport = airportQuerySim->getAirportByIdent(ils.airportIdent);

          // Prefer airport variation to have the same runway heading as in the runway information
          float rwMagvar = airport.isValid() ? airport.magvar : ils.magvar;
          html.row2(tr("Offset localizer."));
          html.row2(tr("Runway heading:"), courseTextFromTrue(end.heading, rwMagvar), ahtml::NO_ENTITIES);
        }
      }
    }
  }

  if(ils.hasGlideslope())
  {
    html.row2(prefix % (ils.isAnyGlsRnp() ? tr("Glidepath:") : tr("Glideslope:")),
              locale.toString(ils.slope, 'f', 1) % tr("°"));
  }

  if(info && ils.isAnyGlsRnp())
  {
    html.row2If(tr("Performance:"), ils.perfIndicator);
    html.row2If(tr("Provider:"), ils.provider);
  }

  if(!procInfo)
    html.tableEnd();

  if(info && infoOrTooltip && !procInfo)
    // Add scenery indicator to clear source - either nav or sim
    addScenery(nullptr, html, DATASOURCE_NAV);

#ifdef DEBUG_INFORMATION
  if(info && !procInfo)
  {
    html.small(QString("Database: ils_id = %1, corrected %2").arg(ils.id).arg(ils.corrected)).br();
    if(end.isValid())
      html.small(QString("Database: runway_end_id = %1").arg(end.id)).br();
  }
#endif

  if(info && !procInfo)
    html.br();
}

void HtmlInfoBuilder::helipadText(const MapHelipad& helipad, HtmlBuilder& html) const
{
  QString num = helipad.start != -1 ? (" " % helipad.runwayName) : QString();

  head(html, tr("Helipad%1:").arg(num));
  html.brText(tr("Surface: ") % map::surfaceName(helipad.surface));
  html.brText(tr("Type: ") % atools::capString(helipad.type));
  html.brText(Unit::distShortFeet(std::max(helipad.width, helipad.length)));
  if(helipad.closed)
    html.brText(tr("Is Closed"));
}

void HtmlInfoBuilder::windText(const atools::grib::WindPosList& windStack, HtmlBuilder& html, float waypointAltitude, bool table) const
{
  const WindReporter *windReporter = NavApp::getWindReporter();

  float windbarbAltitude = windReporter->getDisplayAltitudeFt();
  float manualAltitude = windReporter->getManualAltitudeFt();
  float cruiseAltitude = NavApp::getRouteCruiseAltitudeFt();
  QString source = windReporter->getSourceText();

  const static int WIND_LAYERS_AT_WAYPOINT = 4;
#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO;
#endif

  if(!windStack.isEmpty())
  {
    float magVar = NavApp::getMagVar(windStack.constFirst().pos);
    if(windStack.size() == 1)
    {
      // Single line wind report for flight plan waypoints =============================================
      const atools::grib::WindPos& wind = windStack.constFirst();
      if(wind.isValid())
      {
        html.table();
        html.row2(tr("Flight Plan wind (%1)").arg(source), tr("%2, %3, %4").
                  arg(Unit::altFeet(wind.pos.getAltitude())).
                  arg(courseTextFromTrue(wind.wind.dir, magVar)).
                  arg(Unit::speedKts(wind.wind.speed)), ahtml::NO_ENTITIES | ahtml::NOBR);
        html.tableEnd();
      }
    }
    else if(!windStack.isEmpty())
    {
      // Several wind layers report for wind barbs =============================================
      if(!table)
      {
        head(html, tr("Wind (%1)").arg(source));
        html.table();
      }
      else
      {
        html.table();
        html.tr().th(tr("Wind (%1):").arg(source), ahtml::ALIGN_LEFT, QColor(), 3 /* colspan */).trEnd();
      }

      html.tr().th(Unit::getUnitAltStr()).th(tr("Direction")).th(Unit::getUnitSpeedStr()).trEnd();

      // Find index for given layer altitudes - these have to be included in the stack already
      int windbarbLayerIndex = -1, waypointLayerIndex = -1, manualLayerIndex = -1, cruiseLayerIndex = -1;
      for(int i = 0; i < windStack.size(); i++)
      {
        float alt = windStack.at(i).pos.getAltitude();
        if(atools::almostEqual(alt, windbarbAltitude, 10.f))
          windbarbLayerIndex = i;
        if(atools::almostEqual(alt, waypointAltitude, 10.f))
          waypointLayerIndex = i;
        if(atools::almostEqual(alt, manualAltitude, 10.f))
          manualLayerIndex = i;
        if(atools::almostEqual(alt, cruiseAltitude, 10.f))
          cruiseLayerIndex = i;
      }

      // Wind reports are all at the same position
      for(int i = 0; i < windStack.size(); i++)
      {
        const atools::grib::WindPos& wind = windStack.at(i);

        if(!wind.wind.isValid())
          continue;

        float alt = wind.pos.getAltitude();

        if(waypointAltitude < map::INVALID_ALTITUDE_VALUE)
        {
          // Show only two layers below and two layers above
          if(waypointLayerIndex != -1 && atools::almostNotEqual(waypointLayerIndex, i, WIND_LAYERS_AT_WAYPOINT / 2))
            continue;
        }
        else if(!verbose && windbarbLayerIndex != -1 && atools::almostNotEqual(windbarbLayerIndex, i, 1))
          continue;

        Flags flags = windbarbLayerIndex == i || waypointLayerIndex == i || manualLayerIndex == i || cruiseLayerIndex == i ?
                      ahtml::BOLD | ahtml::ALIGN_RIGHT : ahtml::ALIGN_RIGHT;
        QStringList suffixList;

        if(waypointLayerIndex == i && cruiseLayerIndex == i)
          suffixList.append(tr("flight plan waypoint and cruise"));
        else if(waypointLayerIndex == i)
          suffixList.append(tr("flight plan waypoint"));
        else if(cruiseLayerIndex == i)
          suffixList.append(tr("flight plan cruise"));

        if(windbarbLayerIndex == i)
          suffixList.append(tr("wind barbs"));
        if(manualLayerIndex == i)
          suffixList.append(tr("manual layer"));

        QString suffix;
        if(!suffixList.isEmpty())
          suffix = tr(" (%1)").arg(suffixList.join(tr(", ")));

        // One table row with three data fields
        QString courseTxt;
        if(wind.wind.isNull())
          courseTxt = tr("-");
        else
          courseTxt = courseTextFromTrue(wind.wind.dir, magVar);

        QString speedTxt;
        if(wind.wind.isNull())
          speedTxt = tr("0");
        else
          speedTxt = tr("%L1").arg(Unit::speedKtsF(wind.wind.speed), 0, 'f', 0);

        html.tr().
        td(atools::almostEqual(alt, 260.f) ? tr("Ground") : Unit::altFeet(alt, false), flags).
        td(courseTxt, flags | ahtml::NO_ENTITIES).
        td(speedTxt, flags).
        td(suffix).
        trEnd();
      }

#ifdef DEBUG_INFORMATION
      html.row2(tr("Pos:"), QString("Pos(%1, %2)").arg(windStack.first().pos.getLonX()).arg(windStack.first().pos.getLatY()), ahtml::PRE);
#endif

      html.tableEnd();
    }
  }
}

void HtmlInfoBuilder::procedureText(const MapAirport& airport, HtmlBuilder& html) const
{
  if(info && infoQuery != nullptr && airport.isValid())
  {
    MapQuery *mapQuery = mapWidget->getMapQuery();
    MapAirport navAirport = mapQuery->getAirportNav(airport);

    if(!print)
      airportTitle(airport, html, -1, true /* procedures */);

    const SqlRecordList *recAppVector = infoQuery->getApproachInformation(navAirport.id);
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

        QString runwayIdent = atools::fs::util::runwayBestFit(recApp.valueStr("runway_name"), runwayNames);
        QString runwayTxt = runwayIdent;

        if(!runwayTxt.isEmpty())
          runwayTxt = tr(" - Runway ") % runwayTxt;

        QString fix = recApp.valueStr("fix_ident");
        QString header;

        if(type & proc::PROCEDURE_SID)
          header = tr("SID %1 %2").arg(fix).arg(runwayTxt);
        else if(type & proc::PROCEDURE_STAR)
          header = tr("STAR %1 %2").arg(fix).arg(runwayTxt);
        else
          header = tr("Approach %1 %2 %3 %4").arg(proc::procedureType(procType)).
                   arg(recApp.valueStr("suffix")).arg(fix).arg(runwayTxt);

        html.hr();
        html.h3(header, ahtml::UNDERLINE);

        // Fill table ==========================================================
        html.table();

        if(!(type & proc::PROCEDURE_SID) && !(type & proc::PROCEDURE_STAR))
          rowForBool(html, &recApp, "has_gps_overlay", tr("Has GPS Overlay"), false);

        addRadionavFixType(html, recApp);

        if(procType == "LOCB")
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
              QVector<map::MapIls> ilsVector = mapQuery->getIlsByAirportAndRunway(airport.ident, backcourseEndIdent);
              if(!ilsVector.isEmpty())
              {
                for(int i = 0; i < ilsVector.size(); i++)
                {
                  const map::MapIls& ils = ilsVector.at(i);
                  ilsTextProcInfo(ils, html);
                  html.row2(QString(), QString());
                }
              }
              else
                html.row2(tr("ILS data not found"));
            }
            else
              html.row2(tr("ILS data runway not found"));
          }
        }
        else if(proc::MapProcedureLegs::hasFrequency(procType))
        {
          // Display ILS information ===========================================
          QVector<map::MapIls> ilsVector = mapQuery->getIlsByAirportAndRunway(airport.ident, runwayIdent);
          if(!ilsVector.isEmpty())
          {
            for(int i = 0; i < ilsVector.size(); i++)
            {
              const map::MapIls& ils = ilsVector.at(i);
              if(!ils.isAnyGlsRnp())
              {
                ilsTextProcInfo(ils, html);
                html.row2(QString(), QString());
              }
            }
          }
          else
            html.row2(tr("ILS data not found"));
        }
        else if(proc::MapProcedureLegs::hasChannel(procType))
        {
          // Display GLS information ===========================================
          const QVector<map::MapIls> ilsVector = mapQuery->getIlsByAirportAndRunway(airport.ident, runwayIdent);
          if(!ilsVector.isEmpty())
          {
            for(const map::MapIls& ils : ilsVector)
            {
              if(ils.isAnyGlsRnp())
              {
                ilsTextProcInfo(ils, html);
                html.row2(QString(), QString());
              }
            }
          }
          else
            html.row2(tr("GLS data not found"));
        }
        html.tableEnd();
#ifdef DEBUG_INFORMATION
        if(info)
          html.small(QString("Database: approach_id = %1").arg(recApp.valueInt("approach_id"))).br();
#endif

        const SqlRecordList *recTransVector = infoQuery->getTransitionInformation(recApp.valueInt("approach_id"));
        if(recTransVector != nullptr)
        {
          // Transitions for this approach
          for(const SqlRecord& recTrans : *recTransVector)
          {
            if(!(type & proc::PROCEDURE_SID))
              html.br().text(tr("Transition ") % recTrans.valueStr("fix_ident"), ahtml::BOLD | ahtml::BIG);

            html.table();

            if(!recTrans.isNull("dme_ident"))
            {
              html.row2(tr("DME Ident and Region:"), recTrans.valueStr("dme_ident") % tr(", ") %
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
                            locale.toString(vorReg.valueInt("frequency") / 1000., 'f', 2) % tr(" MHz"));

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
            if(info)
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

    map::MapResult result;

    mapWidget->getMapQuery()->getMapObjectByIdent(result, map::VOR, recApp.valueStr("fix_ident"),
                                                  recApp.valueStr("fix_region"));

    if(result.hasVor())
    {
      const MapVor& vor = result.vors.constFirst();

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
        html.row2If(tr("VORTAC Type:"), map::navTypeNameVorLong(vor.type));
        html.row2(tr("VORTAC Frequency:"), locale.toString(vor.frequency / 1000., 'f', 2) % tr(" MHz"));
        if(!vor.channel.isEmpty())
          html.row2(tr("VORTAC Channel:"), vor.channel);
        if(vor.range > 0)
          html.row2(tr("VORTAC Range:"), Unit::distNm(vor.range));
        addMorse(html, tr("VORTAC Morse:"), vor.ident);
      }
      else
      {
        html.row2If(tr("VOR Type:"), map::navTypeNameVorLong(vor.type));
        html.row2(tr("VOR Frequency:"), locale.toString(vor.frequency / 1000., 'f', 2) % tr(" MHz"));
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

    map::MapResult result;
    mapWidget->getMapQuery()->getMapObjectByIdent(result, map::NDB, recApp.valueStr("fix_ident"),
                                                  recApp.valueStr("fix_region"));

    if(result.hasNdb())
    {
      const MapNdb& ndb = result.ndbs.constFirst();

      html.row2If(tr("NDB Type:"), map::navTypeNameNdb(ndb.type));
      html.row2(tr("NDB Frequency:"), locale.toString(ndb.frequency / 100., 'f', 2) % tr(" kHz"));

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

void HtmlInfoBuilder::weatherText(const map::WeatherContext& context, const MapAirport& airport, HtmlBuilder& html) const
{
  if(info)
  {
    if(!print)
      airportTitle(airport, html, -1, true /* procedures */);

    MapQuery *mapQuery = mapWidget->getMapQuery();

    float transitionAltitude = 0.f, transitionLevel = 0.f;
    mapQuery->getAirportTransitionAltiudeAndLevel(airport, transitionAltitude, transitionLevel);

    QStringList transitionStr;
    if(transitionAltitude > 0.f)
      transitionStr.append(tr("Altitude %1").arg(Unit::altFeet(transitionAltitude)));
    if(transitionLevel > 0.f)
      transitionStr.append(tr("Level %1 (FL%2)").
                           arg(Unit::altFeet(transitionLevel)).arg(transitionLevel / 100.f, 3, 'f', 0, QChar('0')));

    if(!transitionStr.isEmpty())
      html.br().br().b(tr("Transition: ")).text(transitionStr.join(tr(", ")));

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
                   ahtml::BOLD | ahtml::LINK_NO_UL);
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
          html.p(context.asType % tr(" - Departure and Destination"), WEATHER_TITLE_FLAGS);
        else if(context.isAsDeparture)
          html.p(context.asType % tr(" - Departure"), WEATHER_TITLE_FLAGS);
        else if(context.isAsDestination)
          html.p(context.asType % tr(" - Destination"), WEATHER_TITLE_FLAGS);
        else
          html.p(context.asType, WEATHER_TITLE_FLAGS);

        decodedMetar(html, airport, map::MapAirport(), Metar(context.asMetar), false /* interpolated */,
                     false /* FSX/P3D */, src == WEATHER_SOURCE_ACTIVE_SKY && weatherShown);
      }

      // NOAA or nearest ===========================
      decodedMetars(html, context.noaaMetar, airport, tr("NOAA"), src == WEATHER_SOURCE_NOAA && weatherShown);

      // Vatsim metar or nearest ===========================
      decodedMetars(html, context.vatsimMetar, airport, tr("VATSIM"), src == WEATHER_SOURCE_VATSIM && weatherShown);

      // IVAO or nearest ===========================
      decodedMetars(html, context.ivaoMetar, airport, tr("IVAO"), src == WEATHER_SOURCE_IVAO && weatherShown);
    } // if(flags & optsw::WEATHER_INFO_ALL)
    else
      html.p().warning(tr("No weather display selected in options dialog on page \"Weather\"."));
  } // if(info)
}

void HtmlInfoBuilder::airportMsaText(const map::MapAirportMsa& msa, atools::util::HtmlBuilder& html) const
{
  airportMsaTextInternal(msa, html, false /* user */);
}

void HtmlInfoBuilder::msaMarkerText(const map::MsaMarker& msa, atools::util::HtmlBuilder& html) const
{
  airportMsaTextInternal(msa.msa, html, true /* user */);
}

void HtmlInfoBuilder::airportMsaTextInternal(const map::MapAirportMsa& msa, atools::util::HtmlBuilder& html, bool user) const
{
  html.img(QIcon(":/littlenavmap/resources/icons/msa.svg"), QString(), QString(), symbolSizeTitle);
  html.nbsp().nbsp();

  QString title;
  if(msa.navType == map::AIRPORT)
    title.append(tr("%1 %2").arg(tr("Airport")).arg(msa.navIdent));
  else
  {
    if(!msa.navIdent.isEmpty())
    {
      if(msa.navType == map::WAYPOINT)
        title.append(tr("%1 %2").arg(tr("Waypoint")).arg(msa.navIdent));
      else if(msa.navType == map::VOR)
        title.append(tr("%1 %2").arg(map::vorType(msa.vorDmeOnly, msa.vorHasDme, msa.vorTacan, msa.vorVortac)).arg(msa.navIdent));
      else if(msa.navType == map::NDB)
        title.append(tr("%1 %2").arg(tr("NDB")).arg(msa.navIdent));
      else if(msa.navType == map::RUNWAYEND)
        title.append(tr("%1 %2").arg(tr("Runway")).arg(msa.navIdent));
    }

    if(!msa.airportIdent.isEmpty())
      title.append(tr(" (%1 %2)").arg(tr("Airport")).arg(msa.airportIdent));
  }

  navaidTitle(html, tr("%1 %2at %3").arg(user ? tr("User MSA Diagram") : QString(tr("MSA"))).arg(msa.multipleCode % tr(" ")).arg(title));

  if(info)
  {
    // Add map link if not tooltip
    html.nbsp().nbsp();
    html.a(tr("Map"), QString("lnm://show?lonx=%1&laty=%2").
           arg(msa.position.getLonX()).arg(msa.position.getLatY()), ahtml::BOLD | ahtml::LINK_NO_UL);
  }
  html.table();
  html.row2(tr("Radius:"), Unit::distNm(msa.radius));

  if(verbose)
    html.row2(tr("Bearing and alt. units:"), msa.trueBearing ? tr("°T, ") : tr("°M, ") % Unit::getUnitAltStr());

  if(info)
    html.row2(tr("Magnetic declination:"), map::magvarText(msa.magvar));
  if(msa.altitudes.size() == 0)
    html.row2(tr("No altitude"));
  else if(msa.altitudes.size() == 1)
    html.row2(tr("Minimum altitude:"), Unit::altFeet(msa.altitudes.at(0)));

  if(user)
    html.row2(tr("Diagram added by user"), QString());
  html.tableEnd();

  if(verbose)
  {
    if(msa.altitudes.size() > 1)
    {
      QFont font;
      if(info)
        font = NavApp::getTextBrowserInfoFont();
      else
        font = QToolTip::font();

      int actualSize = 0;
      float sizeFactor = info ? (msa.altitudes.size() > 1 ? 5. : 2.) : (msa.altitudes.size() > 1 ? 4. : 2.);
      QIcon icon = SymbolPainter::createAirportMsaIcon(msa, QToolTip::font(), sizeFactor, &actualSize);
      html.p().img(icon, QString(), QString(), QSize(actualSize, actualSize)).pEnd();
    }
  }

  if(info && !user)
    addScenery(infoQuery->getMsaInformation(msa.id), html, DATASOURCE_MSA);

  if(info)
    html.br();
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
               ahtml::BOLD | ahtml::LINK_NO_UL);
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

  QList<atools::fs::weather::MetarCloud> clouds = parsed.getClouds();
  bool hasClouds = !clouds.isEmpty() && clouds.constFirst().getCoverage() != atools::fs::weather::MetarCloud::COVERAGE_CLEAR;

  html.table();

  if(reportAirport.isValid())
  {
    html.row2(tr("Reporting airport: "), tr("%1 (%2), %3, %4").
              arg(reportAirport.name).
              arg(reportAirport.displayIdent()).
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

    QString text = locale.toString(time, QLocale::ShortFormat) % " " % time.timeZoneAbbreviation();

    qint64 secsTo = time.secsTo(QDateTime::currentDateTimeUtc());
    if(secsTo > WEATHER_MAX_AGE_HOURS_ERR * 3600)
      html.row2Error(tr("Time: "), text + tr(" (%1 hours old)").arg(secsTo / 3600));
    else if(secsTo > WEATHER_MAX_AGE_HOURS_WARN * 3600)
      html.row2Warning(tr("Time: "), text + tr(" (%1 hours old)").arg(secsTo / 3600));
    else
      html.row2(tr("Time: "), text);
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
      windDir = courseTextFromTrue(parsed.getWindDir(), airport.magvar) % tr(", ");

    if(parsed.getWindRangeFrom() != -1 && parsed.getWindRangeTo() != -1)
      // Wind direction range given (additionally to dir in some cases)
      windVar = tr(", variable ") % courseTextFromTrue(parsed.getWindRangeFrom(), airport.magvar) %
                tr(" to ") % courseTextFromTrue(parsed.getWindRangeTo(), airport.magvar);
    else if(parsed.getWindDir() == -1)
      windDir = tr("Variable, ");

    if(windSpeedKts < INVALID_METAR_VALUE)
    {
      QString windSpeedStr;
      if(windSpeedKts < INVALID_METAR_VALUE)
        windSpeedStr = Unit::speedKts(windSpeedKts);

      html.row2(tr("Wind:"), windDir % windSpeedStr % windVar, ahtml::NO_ENTITIES);
    }
    else
      html.row2(tr("Wind:"), windDir % tr("Speed not valid") % windVar, ahtml::NO_ENTITIES);

    hasWind = true;
  }

  if(parsed.getGustSpeedKts() < INVALID_METAR_VALUE)
  {
    hasWind = true;
    html.row2(tr("Wind gusts:"), Unit::speedKts(parsed.getGustSpeedKts()));
  }

  // Temperature  =============================================================
  float temperature = parsed.getTemperatureC();
  if(temperature < INVALID_METAR_VALUE)
    html.row2(tr("Temperature:"), locale.toString(atools::roundToInt(temperature)) % tr("°C, ") %
              locale.toString(atools::roundToInt(ageo::degCToDegF(temperature))) % tr("°F"));

  float dewpoint = parsed.getDewpointDegC();
  if(dewpoint < INVALID_METAR_VALUE)
    html.row2(tr("Dewpoint:"), locale.toString(atools::roundToInt(dewpoint)) % tr("°C, ") %
              locale.toString(atools::roundToInt(ageo::degCToDegF(dewpoint))) % tr("°F"));

  // Pressure  =============================================================
  float seaLevelPressureMbar = parsed.getPressureMbar();
  if(seaLevelPressureMbar < INVALID_METAR_VALUE)
  {
    html.row2(tr("Pressure:"), locale.toString(seaLevelPressureMbar, 'f', 0) % tr(" hPa, ") %
              locale.toString(ageo::mbarToInHg(seaLevelPressureMbar), 'f', 2) % tr(" inHg"));

    if(temperature < INVALID_METAR_VALUE)
      html.row2(tr("Density Altitude:"), Unit::altFeet(ageo::densityAltitudeFt(temperature, airport.getAltitude(), seaLevelPressureMbar)));
  }

  // Visibility =============================================================
  const atools::fs::weather::MetarVisibility& minVis = parsed.getMinVisibility();
  QString visiblity;
  if(minVis.getVisibilityMeter() < INVALID_METAR_VALUE)
    visiblity.append(tr("%1 %2").
                     arg(minVis.getModifierString()).
                     arg(Unit::distMeter(minVis.getVisibilityMeter())));

  const atools::fs::weather::MetarVisibility& maxVis = parsed.getMaxVisibility();
  if(maxVis.getVisibilityMeter() < INVALID_METAR_VALUE)
    visiblity.append(tr(" %1 %2").arg(maxVis.getModifierString()).arg(Unit::distMeter(maxVis.getVisibilityMeter())));

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
    text(tr("No cloud below 5,000 ft (1,500 m), visibility of 10 km (6 NM) or more")).pEnd();

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

  bestRunwaysText(airport, html, parsed, 8 /* max entries */, true /* details */);

#ifdef DEBUG_INFORMATION
  if(info)
    html.p().small(tr("Source: %1").arg(metar.getMetar())).br();
#endif
}

void HtmlInfoBuilder::vorText(const MapVor& vor, HtmlBuilder& html) const
{
  const SqlRecord *rec = nullptr;
  if(info && infoQuery != nullptr)
    rec = infoQuery->getVorInformation(vor.id);

  QIcon icon = SymbolPainter::createVorIcon(vor, symbolSizeTitle.height());
  html.img(icon, QString(), QString(), symbolSizeTitle);
  html.nbsp().nbsp();

  QString type = map::vorType(vor);
  navaidTitle(html, type % ": " % capString(vor.name) % " (" % vor.ident % ")");

  if(info)
  {
    // Add map link if not tooltip
    html.nbsp().nbsp();
    html.a(tr("Map"), QString("lnm://show?lonx=%1&laty=%2").
           arg(vor.position.getLonX()).arg(vor.position.getLatY()), ahtml::BOLD | ahtml::LINK_NO_UL);
  }

  html.table();
  routeInfoText(html, vor.routeIndex, vor.recommended);

  if(!print) // Also tooltip due to magnetic variation
    bearingToUserText(vor.position, vor.magvar, html);

  if(verbose)
  {
    if(vor.tacan)
    {
      if(vor.dmeOnly)
        html.row2(tr("Type:"), tr("DME only"));
    }
    else
      html.row2If(tr("Type:"), map::navTypeNameVorLong(vor.type));

    if(rec != nullptr && !rec->isNull("airport_id"))
      airportRow(airportQueryNav->getAirportById(rec->valueInt("airport_id")), html);

  }
  if(info)
    html.row2If(tr("Region:"), vor.region);

  if(!vor.tacan)
    html.row2(tr("Frequency:"), locale.toString(vor.frequency / 1000., 'f', 2) % tr(" MHz"));

  if(vor.vortac && !vor.channel.isEmpty())
    html.row2(tr("Channel:"), vor.channel);
  else if(vor.tacan)
    html.row2(tr("Channel:"), QString(tr("%1 (%2 MHz)")).
              arg(vor.channel).
              arg(locale.toString(frequencyForTacanChannel(vor.channel) / 100.f, 'f', 2)));

  if(!vor.tacan && !vor.dmeOnly)
    html.row2(tr("Calibrated declination:"), map::magvarText(vor.magvar));

  if(info)
    html.row2(tr("Magnetic declination:"), map::magvarText(NavApp::getMagVar(vor.position)));

  if(vor.getAltitude() < INVALID_ALTITUDE_VALUE && verbose)
    html.row2(tr("Elevation:"), Unit::altFeet(vor.getAltitude()));
  html.row2(tr("Range:"), Unit::distNm(vor.range));

  if(verbose)
    addMorse(html, tr("Morse:"), vor.ident);

  if(info)
    addCoordinates(rec, html);
  html.tableEnd();

  MapWaypoint wp = NavApp::getWaypointTrackQueryGui()->getWaypointByNavId(vor.id, map::VOR);
  if(wp.artificial != map::WAYPOINT_ARTIFICIAL_NONE)
    // Artificial waypoints are not shown - display airway list here
    waypointAirwayText(wp, html);

  if(rec != nullptr)
    addScenery(rec, html, DATASOURCE_NAV);

  if(!info)
    routeWindText(html, NavApp::getRouteConst(), vor.routeIndex);

#ifdef DEBUG_INFORMATION
  if(info)
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

  QIcon icon = SymbolPainter::createNdbIcon(symbolSizeTitle.height());
  html.img(icon, QString(), QString(), symbolSizeTitle);
  html.nbsp().nbsp();

  navaidTitle(html, tr("NDB: ") % capString(ndb.name) % " (" % ndb.ident % ")");

  if(info)
  {
    // Add map link if not tooltip
    html.nbsp().nbsp();
    html.a(tr("Map"), QString("lnm://show?lonx=%1&laty=%2").
           arg(ndb.position.getLonX()).arg(ndb.position.getLatY()), ahtml::BOLD | ahtml::LINK_NO_UL);
  }

  html.table();
  routeInfoText(html, ndb.routeIndex, ndb.recommended);

  if(!print && info)
    bearingToUserText(ndb.position, ndb.magvar, html);

  if(verbose)
  {
    html.row2If(tr("Type:"), map::navTypeNameNdb(ndb.type));

    if(rec != nullptr && !rec->isNull("airport_id"))
      airportRow(airportQueryNav->getAirportById(rec->valueInt("airport_id")), html);

  }

  if(info)
    html.row2If(tr("Region:"), ndb.region);

  html.row2(tr("Frequency:"), locale.toString(ndb.frequency / 100., 'f', 1) % tr(" kHz"));
  if(info)
    html.row2(tr("Magnetic declination:"), map::magvarText(ndb.magvar));

  if(ndb.getAltitude() < INVALID_ALTITUDE_VALUE && verbose)
    html.row2(tr("Elevation:"), Unit::altFeet(ndb.getAltitude()));

  if(ndb.range > 0)
    html.row2(tr("Range:"), Unit::distNm(ndb.range));

  if(verbose)
    addMorse(html, tr("Morse:"), ndb.ident);

  if(info)
    addCoordinates(rec, html);
  html.tableEnd();

  MapWaypoint wp = NavApp::getWaypointTrackQueryGui()->getWaypointByNavId(ndb.id, map::NDB);
  if(wp.artificial != map::WAYPOINT_ARTIFICIAL_NONE)
    // Artificial waypoints are not shown - display airway list here
    waypointAirwayText(wp, html);

  if(rec != nullptr)
    addScenery(rec, html, DATASOURCE_NAV);

  if(!info)
    routeWindText(html, NavApp::getRouteConst(), ndb.routeIndex);

#ifdef DEBUG_INFORMATION
  if(info)
    html.small(QString("Database: ndb_id = %1 waypoint_id = %2").arg(ndb.getId()).arg(wp.getId())).br();
#endif

  if(info)
    html.br();
}

void HtmlInfoBuilder::holdingText(const MapHolding& holding, HtmlBuilder& html) const
{
  holdingTextInternal(holding, html, false /* user */);
}

void HtmlInfoBuilder::holdingMarkerText(const HoldingMarker& holding, HtmlBuilder& html) const
{
  holdingTextInternal(holding.holding, html, true /* user */);
}

void HtmlInfoBuilder::holdingTextInternal(const MapHolding& holding, HtmlBuilder& html, bool user) const
{
  html.img(QIcon(":/littlenavmap/resources/icons/enroutehold.svg"), QString(), QString(), symbolSizeTitle);
  html.nbsp().nbsp();

  QString navTypeStr;
  if(holding.navType == map::AIRPORT)
    navTypeStr = tr("Airport");
  else if(holding.navType == map::VOR)
    navTypeStr = map::vorType(holding.vorDmeOnly, holding.vorHasDme, holding.vorTacan, holding.vorVortac);
  else if(holding.navType == map::NDB)
    navTypeStr = tr("NDB");
  else if(holding.navType == map::WAYPOINT)
    navTypeStr = tr("Waypoint");
  else if(holding.navType == map::USERPOINT)
    navTypeStr = tr("Userpoint");

  QString title;

  if(user)
    title.append(tr("User "));

  if(!navTypeStr.isEmpty())
    // Hold has a navaid
    title.append(tr("Holding at %1 %2, %3").
                 arg(navTypeStr).arg(holding.navIdent).arg(courseTextFromTrue(holding.courseTrue, holding.magvar)));
  else
    // Hold at a position
    title.append(tr("Holding %1").arg(courseTextFromTrue(holding.courseTrue, holding.magvar)));

  navaidTitle(html, title, true /* noEntities */);

  html.table();
  // Add bearing/distance to table

  if(!print && info)
    bearingToUserText(holding.position, holding.magvar, html);

  if(info && !holding.airportIdent.isEmpty())
    airportRow(airportQuerySim->getAirportByIdent(holding.airportIdent), html);

  if(holding.time > 0.f)
  {
    QString minStr = locale.toString(holding.time);
    html.row2If(tr("Time:"), tr("%1 %2").arg(minStr).
                arg(atools::almostEqual(holding.time, 1.f) ? tr("Minute") : tr("Minutes")));

    if(info)
    {
      float complete = holding.time * 2.f + 2.f;
      QString completeStr = locale.toString(complete);
      html.row2If(tr("Total time to complete:"), tr("%1 %2").arg(completeStr).
                  arg(atools::almostEqual(complete, 1.f) ? tr("Minute") : tr("Minutes")));
    }
  }

  if(verbose || info)
    html.row2If(tr("Turn:"), holding.turnLeft ? tr("Left") : tr("Right"));

  bool estimated = false;
  float dist = holding.distance(&estimated);
  html.row2If(estimated ? tr("Estimated length:") : tr("Length:"), Unit::distNm(dist));

  if(user)
  {
    if(holding.position.getAltitude() > 0.f)
      html.row2If(tr("Altitude:"), Unit::altFeet(holding.position.getAltitude()));
    if(holding.speedKts > 0.f)
      html.row2If(tr("Speed:"), Unit::speedKts(holding.speedKts));
  }
  else
  {
    if(holding.speedLimit > 0.f)
      html.row2If(tr("Speed limit:"), Unit::speedKts(holding.speedLimit));

    if(holding.minAltititude > 0.f && holding.maxAltititude > 0.f)
      html.row2If(tr("Altitude:"), tr("%1 to %2").arg(Unit::altFeet(holding.minAltititude, false /* addUnit */)).
                  arg(Unit::altFeet(holding.maxAltititude)));
    else if(holding.minAltititude > 0.f)
      html.row2If(tr("Min altitude:"), Unit::altFeet(holding.minAltititude));
    else if(holding.maxAltititude > 0.f)
      html.row2If(tr("Max altitude:"), Unit::altFeet(holding.maxAltititude));
  }
  html.tableEnd();

  if(info && !user)
    addScenery(infoQuery->getHoldingInformation(holding.id), html, DATASOURCE_HOLD);

  if(info)
    html.br();
}

void HtmlInfoBuilder::rangeMarkerText(const RangeMarker& marker, atools::util::HtmlBuilder& html) const
{
  html.img(QIcon(":/littlenavmap/resources/icons/rangerings.svg"), QString(), QString(), symbolSizeTitle);
  html.nbsp().nbsp();

  navaidTitle(html, marker.ranges.size() > 1 ? tr("Range Rings") : tr("Range Ring"));

  html.table();
  html.row2If(tr("Label:"), marker.text);

  QStringList distStr;
  for(float dist : marker.ranges)
    distStr.append(Unit::distNm(dist, false));

  html.row2If(marker.ranges.size() > 1 ? tr("Radii:") : tr("Radius"),
              tr("%1 %2").arg(distStr.join(tr(", "))).arg(Unit::getUnitDistStr()));

  html.tableEnd();
}

void HtmlInfoBuilder::distanceMarkerText(const DistanceMarker& marker, atools::util::HtmlBuilder& html) const
{
  html.img(QIcon(":/littlenavmap/resources/icons/distancemeasure.svg"), QString(), QString(), symbolSizeTitle);
  html.nbsp().nbsp();

  navaidTitle(html, tr("Distance Measurement"));

  float initTrue = marker.from.initialBearing(marker.to);
  float finalTrue = marker.from.finalBearing(marker.to);

  QString initCourseStr = tr("%L1°M, %L2°T").arg(initTrue - marker.magvar, 0, 'f', 0).arg(initTrue, 0, 'f', 0);
  QString finalCourseStr = tr("%L1°M, %L2°T").arg(finalTrue - marker.magvar, 0, 'f', 0).arg(finalTrue, 0, 'f', 0);

  html.table();
  html.row2If(tr("Label:"), marker.text);
  float distMeter = marker.getDistanceMeter();
  if(distMeter < map::INVALID_DISTANCE_VALUE)
    html.row2If(tr("Distance:"), Unit::distLongShortMeter(distMeter, tr(", ")));

  if(initCourseStr != finalCourseStr)
  {
    html.row2If(tr("Start Course:"), initCourseStr);
    html.row2If(tr("End Course:"), finalCourseStr);
  }
  else
    html.row2If(tr("Course:"), initCourseStr);

  html.tableEnd();
}

void HtmlInfoBuilder::bearingAndDistanceTexts(const atools::geo::Pos& pos, float magvar, atools::util::HtmlBuilder& html, bool bearing,
                                              bool distance)
{
  if(pos.isValid())
  {
    bool added = false;
    atools::util::HtmlBuilder temp = html.cleared();
    temp.table();
    if(!print && !info && distance) // Only tooltip and not for flight plan positions
      added |= distanceToRouteText(pos, temp);

    if(!print && bearing) // Bearing only in tooltip
      added |= bearingToUserText(pos, magvar, temp);
    temp.tableEnd();

    if(added)
      html.append(temp);
  }
}

void HtmlInfoBuilder::patternMarkerText(const PatternMarker& pattern, atools::util::HtmlBuilder& html) const
{
  html.img(QIcon(":/littlenavmap/resources/icons/trafficpattern.svg"), QString(), QString(), symbolSizeTitle);
  html.nbsp().nbsp();

  navaidTitle(html, tr("Traffic Pattern"), true /* noEntities */);

  html.table();
  html.row2If(tr("Airport:"), pattern.airportIcao);
  html.row2If(tr("Runway:"), pattern.runwayName);
  html.row2If(tr("Turn:"), pattern.turnRight ? tr("Right") : tr("Left"));
  html.row2(tr("Heading at final:"), courseTextFromTrue(pattern.courseTrue, pattern.magvar), ahtml::NO_ENTITIES);
  html.row2(tr("Pattern altitude:"),
            Unit::altFeet(pattern.position.getAltitude(), true /* addUnit */, false /* narrow */, 10.f /* round to */));
  html.tableEnd();
}

void HtmlInfoBuilder::userpointTextInfo(const MapUserpoint& userpoint, HtmlBuilder& html) const
{
  userpointText(userpoint, html);
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
    html.img(icon, QString(), QString(), QSize(0, symbolSizeTitle.height())); // Pass only height over to avoid stretching the image
    html.nbsp().nbsp();

    navaidTitle(html, tr("Userpoint%1").arg(userpoint.temp ? tr(" (Temporary)") : QString()));

    if(info)
    {
      // Add map link if not tooltip
      html.nbsp().nbsp();
      html.a(tr("Map"), QString("lnm://show?lonx=%1&laty=%2").
             arg(userpoint.position.getLonX()).arg(userpoint.position.getLatY()), ahtml::BOLD | ahtml::LINK_NO_UL);
    }

    html.table();

    if(!print && info)
      bearingToUserText(userpoint.position, NavApp::getMagVar(userpoint.position), html);

    // Be cautious with user defined data and adapt it for HTML display
    QString transType = UserdataIcons::typeToTranslated(userpoint.type);
    if(transType == userpoint.type)
      html.row2If(tr("Type:"), userpoint.type, ahtml::REPLACE_CRLF);
    else
      html.row2If(tr("Type:"), tr("%1 (%2)").arg(userpoint.type).arg(UserdataIcons::typeToTranslated(userpoint.type)), ahtml::REPLACE_CRLF);

    html.row2If(tr("Ident:"), userpoint.ident, ahtml::REPLACE_CRLF);
    if(info)
      html.row2If(tr("Region:"), userpoint.region, ahtml::REPLACE_CRLF);
    html.row2If(tr("Name:"), userpoint.name, ahtml::REPLACE_CRLF);

    html.row2If(tr("Tags:"), userpoint.tags, ahtml::REPLACE_CRLF);

    if(!rec.isNull("altitude") && verbose)
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
      html.row2If(tr("Imported from:"), filepathTextShow(rec.valueStr("import_file_path")), ahtml::NO_ENTITIES | ahtml::SMALL);
      html.tableEnd();
    }

#ifdef DEBUG_INFORMATION
    if(info)
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

void HtmlInfoBuilder::logEntryTextInfo(const MapLogbookEntry& logEntry, HtmlBuilder& html) const
{
  logEntryText(logEntry, html);
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
              airportLink(html, logEntry.departureIdent, logEntry.departureName, logEntry.departurePos) %
              (logEntry.departureIdent.isEmpty() ? QString() :
               tr(", %1").arg(Unit::altFeet(rec.valueFloat("departure_alt")))),
              ahtml::NO_ENTITIES | ahtml::BOLD);
    html.row2(tr("To:"),
              airportLink(html, logEntry.destinationIdent, logEntry.destinationName, logEntry.destinationPos) %
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

    if(verbose)
    {
      html.row2If(tr("Aircraft registration:"), rec.valueStr("aircraft_registration"));
      html.row2If(tr("Flight number:"), rec.valueStr("flightplan_number"));
      html.row2If(tr("Simulator:"), rec.valueStr("simulator"));
    }

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

    float grossWeight = rec.valueFloat("grossweight");
    float usedFuel = rec.valueFloat("used_fuel");
    FuelTool ft(rec.valueBool("is_jetfuel"), false /* fuel is in lbs */);

    if(verbose)
    {
      if(info)
      {
        if(rec.valueFloat("distance_flown") > 0.f)
          html.row2(tr("Distance flown:"), Unit::distNm(rec.valueFloat("distance_flown")));

        const ageo::LineString line = logEntry.lineString();
        if(line.isValid())
          html.row2(tr("Great circle distance:"), Unit::distMeter(line.lengthMeter()));
      }

      float travelTimeHours = (rec.valueDateTime("destination_time_sim").toSecsSinceEpoch() -
                               rec.valueDateTime("departure_time_sim").toSecsSinceEpoch()) / 3600.f;

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
    }

    if(info)
    {
      html.tableEnd();

      // Departure =======================================================
      html.p(tr("Departure (%1)").arg(logEntry.departureIdent.isEmpty() ? tr("-") : logEntry.departureIdent), ahtml::BOLD);
      html.table();

      html.row2If(tr("Runway:"), rec.valueStr("departure_runway"));
    }

    if(verbose)
    {
      QDateTime departTime = rec.valueDateTime("departure_time");
      if(departTime.isValid())
      {
        QString departTimeZone = departTime.timeZoneAbbreviation();
        html.row2If(info ? tr("Real time:") : tr("Departure real time:"),
                    tr("%1 %2").arg(locale.toString(departTime, QLocale::ShortFormat)).arg(departTimeZone));
      }

      QDateTime departTimeSim = rec.valueDateTime("departure_time_sim");
      if(departTimeSim.isValid())
      {
        QString departTimeZoneSim = departTimeSim.timeZoneAbbreviation();
        html.row2If(info ? tr("Simulator time:") : tr("Departure simulator time:"),
                    tr("%1 %2").arg(locale.toString(departTimeSim, QLocale::ShortFormat)).arg(departTimeZoneSim));
      }
    }

    if(info)
    {
      if(grossWeight > 0.f)
        html.row2(tr("Gross Weight:"), Unit::weightLbs(grossWeight), ahtml::NO_ENTITIES);
      addCoordinates(Pos(rec.valueFloat("departure_lonx"), rec.valueFloat("departure_laty"), rec.valueFloat("departure_alt", 0.f)), html);
      html.tableEnd();

      // Destination =======================================================
      html.p(tr("Destination (%1)").arg(logEntry.destinationIdent.isEmpty() ? tr("-") : logEntry.destinationIdent), ahtml::BOLD);
      html.table();

      html.row2If(tr("Runway:"), rec.valueStr("destination_runway"));
      QDateTime destTime = rec.valueDateTime("destination_time");
      if(destTime.isValid())
      {
        QString destTimeZone = destTime.timeZoneAbbreviation();
        html.row2If(tr("Real time:"), tr("%1 %2").arg(locale.toString(destTime, QLocale::ShortFormat)).arg(destTimeZone));
      }

      QDateTime destTimeSim = rec.valueDateTime("destination_time_sim");
      if(destTimeSim.isValid())
      {
        QString destTimeZoneSim = destTimeSim.timeZoneAbbreviation();
        html.row2If(tr("Simulator time:"), tr("%1 %2").arg(locale.toString(destTimeSim, QLocale::ShortFormat)).arg(destTimeZoneSim));
      }

      if(grossWeight > 0.f)
        html.row2(tr("Gross Weight:"), Unit::weightLbs(grossWeight - usedFuel), ahtml::NO_ENTITIES);

      addCoordinates(Pos(rec.valueFloat("destination_lonx"), rec.valueFloat("destination_laty"),
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

      if(!rec.valueStr("flightplan_file").isEmpty() || !rec.valueStr("performance_file").isEmpty() || perf || track || route)
      {
        html.p(tr("Files"), ahtml::BOLD);
        html.table();
        html.row2If(tr("Flight plan:"), atools::strJoin({(route ? tr("Attached") : QString()),
                                                         filepathTextShow(rec.valueStr("flightplan_file"), tr("Referenced: "))},
                                                        tr("<br/>")), ahtml::NO_ENTITIES | ahtml::SMALL);
        html.row2If(tr("Aircraft performance:"), atools::strJoin({(route ? tr("Attached") : QString()),
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
    if(info)
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
    html.p().b(tr("Remarks")).pEnd();
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
    map::MapAirport apSim = mapWidget->getMapQuery()->getAirportSim(ap);
    if(apSim.isValid())
    {
      HtmlBuilder apHtml = html.cleared();
      apHtml.a(apSim.displayIdent(), QString("lnm://show?airport=%1").arg(apSim.ident), ahtml::LINK_NO_UL);
      html.row2(tr("Airport:"), apHtml.getHtml(), ahtml::NO_ENTITIES);
    }
  }
}

void HtmlInfoBuilder::waypointText(const MapWaypoint& waypoint, HtmlBuilder& html) const
{
  const SqlRecord *rec = nullptr;

  if(info && infoQuery != nullptr)
    rec = mapWidget->getWaypointTrackQuery()->getWaypointInformation(waypoint.id);

  QIcon icon = SymbolPainter::createWaypointIcon(symbolSizeTitle.height());
  html.img(icon, QString(), QString(), symbolSizeTitle);
  html.nbsp().nbsp();

  navaidTitle(html, tr("Waypoint: ") % waypoint.ident);

  if(info)
  {
    // Add map link if not tooltip
    html.nbsp().nbsp();
    html.a(tr("Map"), QString("lnm://show?lonx=%1&laty=%2").
           arg(waypoint.position.getLonX()).arg(waypoint.position.getLatY()), ahtml::BOLD | ahtml::LINK_NO_UL);
  }

  html.table();
  routeInfoText(html, waypoint.routeIndex, waypoint.recommended);

  if(!print && info)
    bearingToUserText(waypoint.position, waypoint.magvar, html);

  if(info)
  {
    if(waypoint.arincType.isEmpty())
      html.row2If(tr("Type:"), map::navTypeNameWaypoint(waypoint.type));
    else
      // Show detailed description instead
      html.row2If(tr("Type description:"), map::navTypeArincNamesWaypoint(waypoint.arincType));

    if(rec != nullptr && rec->contains("airport_id") && !rec->isNull("airport_id"))
      airportRow(airportQueryNav->getAirportById(rec->valueInt("airport_id")), html);
  }
  else
    html.row2If(tr("Type:"), map::navTypeNameWaypoint(waypoint.type));

  if(verbose)
  {
    const static QRegularExpression WP_NAME_RADIAL_DME("D([0-9]{3})([A-Z])");

    QRegularExpressionMatch match = WP_NAME_RADIAL_DME.match(waypoint.ident);
    if(match.hasMatch())
    {
      // D stands for DME arc waypoint. Aaa is the radial that the fix is on from the reference VOR. B will be a letter corresponding
      // to the distance from the reference VOR. For example, G is the seventh letter of the alphabet so D234G would be a point
      // on the 234° radial 7 nm from the reference VOR. DME arcs greater than 26 nm will have waypoints where the first two characters
      // are the first two letters of the DME identifier. The next three characters will be the radial that the arc waypoint is on.
      float radial = match.captured(1).toFloat();
      char dist = atools::latin1CharAt(match.captured(2), 0);
      html.row2If(tr("Radial and dist. to related:"),
                  strJoinVal({courseText(radial, map::INVALID_COURSE_VALUE), Unit::distNm(static_cast<float>(dist - 'A') + 1.f)}));
    }
  }

  if(info)
  {
    html.row2If(tr("Region:"), waypoint.region);
    html.row2(tr("Magnetic declination:"), map::magvarText(waypoint.magvar));
    addCoordinates(rec, html);
  }

  html.tableEnd();

  // Waypoints should normally not appear here if they are artificial except for FSX, MSFS and the like
  waypointAirwayText(waypoint, html);

  if(rec != nullptr)
    addScenery(rec, html, DATASOURCE_NAV);

  if(!info)
    routeWindText(html, NavApp::getRouteConst(), waypoint.routeIndex);

#ifdef DEBUG_INFORMATION
  if(info)
    html.small(QString("Database: waypoint_id = %1, artificial = %2").arg(waypoint.getId()).arg(waypoint.artificial)).br();
#endif

  if(info)
    html.br();
}

void HtmlInfoBuilder::waypointAirwayText(const MapWaypoint& waypoint, HtmlBuilder& html) const
{
  if(!waypoint.isValid())
    return;

  QList<MapAirway> airways;
  NavApp::getAirwayTrackQueryGui()->getAirwaysForWaypoint(airways, waypoint.id);

  if(!airways.isEmpty())
  {
    if(!verbose)
    {
      // One line with airway names
      QStringList texts;
      for(int i = 0; i < airways.size(); i++)
        texts.append(airways.at(i).name);
      std::sort(texts.begin(), texts.end());
      texts.removeDuplicates();

      QString text = texts.mid(0, 5).join(tr(", "));

      if(texts.size() > 5)
        text.append(tr(" ..."));

      html.table();
      html.row2If(tr("Airways:"), text);
      html.tableEnd();
    }
    else
    {
      // Add airway information

      // Add airway name/text to vector
      QVector<QStringList> airwayTexts;
      for(const MapAirway& aw : qAsConst(airways))
      {
        QString txt(map::airwayTrackTypeToString(aw.type));

        QString altTxt = map::airwayAltText(aw);

        if(!altTxt.isEmpty())
          txt.append(tr(", ") % altTxt);

        airwayTexts.append({aw.isAirway() ? tr("Airway") : tr("Track"), aw.name, txt});
      }

      if(!airwayTexts.isEmpty())
      {
        // Sort airways by name
        std::sort(airwayTexts.begin(), airwayTexts.end(),
                  [](const QStringList& item1, const QStringList& item2)
        {
          return item1.constFirst() == item2.constFirst() ? item1.at(1) < item2.at(1) : item1.constFirst() < item2.constFirst();
        });

        // Remove duplicates
        airwayTexts.erase(std::unique(airwayTexts.begin(), airwayTexts.end()), airwayTexts.end());

        if(info)
          head(html, tr("Connections:"));
        else
          html.br().b(tr("Connections: "));

        html.table();
        int size = info ? airwayTexts.size() : std::min(airwayTexts.size(), 5);
        for(int i = 0; i < size; i++)
        {
          const QStringList& aw = airwayTexts.at(i);
          html.row2(aw.at(0), aw.at(1) % " " % aw.at(2));
        }
        if(size < airwayTexts.size())
          html.row2(tr("More"), tr("..."));
        html.tableEnd();
      }
    }
  }
}

bool HtmlInfoBuilder::distanceToRouteText(const ageo::Pos& pos, HtmlBuilder& html) const
{
  const Route& route = NavApp::getRouteConst();
  if(!route.isEmpty())
  {
    const RouteLeg& lastLeg = route.getDestinationLeg();
    float distance = pos.distanceMeterTo(lastLeg.getPosition());
    html.row2(tr("To last flight plan leg:"), Unit::distMeter(distance), ahtml::NO_ENTITIES);
    return true;
  }
  return false;
}

bool HtmlInfoBuilder::bearingToUserText(const ageo::Pos& pos, float magVar, HtmlBuilder& html) const
{
  if(NavApp::isConnectedAndAircraft())
  {
    const atools::fs::sc::SimConnectUserAircraft& userAircraft = NavApp::getUserAircraft();

    float distance = pos.distanceMeterTo(userAircraft.getPosition());
    if(distance < MAX_DISTANCE_FOR_BEARING_METER)
    {
      html.row2(tr("Bearing and distance to user aircraft:"),
                tr("%1, %2").
                arg(courseTextFromTrue(normalizeCourse(userAircraft.getPosition().angleDegTo(pos)), magVar)).
                arg(Unit::distMeter(distance)),
                ahtml::NO_ENTITIES);

#ifdef DEBUG_INFORMATION
      static bool heartbeat = false;
      html.row2(heartbeat ? " X" : " 0");
      heartbeat = !heartbeat;
#endif
      return true;
    }
  }
  return false;
}

void HtmlInfoBuilder::airspaceText(const MapAirspace& airspace, const atools::sql::SqlRecord& onlineRec, HtmlBuilder& html) const
{
  const OptionData& optionData = OptionData::instance();
  QIcon icon = SymbolPainter::createAirspaceIcon(airspace, symbolSizeTitle.height(),
                                                 optionData.getDisplayThicknessAirspace(),
                                                 optionData.getDisplayTransparencyAirspace());

  html.img(icon, QString(), QString(), symbolSizeTitle);
  html.nbsp().nbsp();

  QString suffix;
  if(airspace.isOnline())
  {
    if(!NavApp::getOnlineNetworkTranslated().isEmpty())
      suffix = tr(" (%1)").arg(NavApp::getOnlineNetworkTranslated());
  }

  if(airspace.name.isEmpty())
    navaidTitle(html, tr("Airspace") % suffix);
  else
  {
    // Do not capitalize online network center names
    QString name = map::airspaceName(airspace);
    if(!info)
      name = atools::elideTextShort(name, 40);

    navaidTitle(html, ((info && !airspace.isOnline()) ? tr("Airspace: ") : QString()) % name % suffix);
  }

  if(info && airspace.hasValidGeometry())
  {
    // Add map link if not tooltip
    html.nbsp().nbsp();
    html.a(tr("Map"), QString("lnm://show?id=%1&type=%2&source=%3").arg(airspace.id).arg(map::AIRSPACE).arg(airspace.src),
           ahtml::BOLD | ahtml::LINK_NO_UL);
  }

  QStringList header;

  if(info)
  {
    const QString remark = map::airspaceRemark(airspace.type);
    if(!remark.isEmpty())
      header.append(remark);

    if(airspace.src == map::AIRSPACE_SRC_NAV && airspace.timeCode != 'U' && !airspace.isOnline())
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

  if(!airspace.hasValidGeometry())
    html.p().warning(tr("Airspace has no geometry and cannot be shown on the map.")).pEnd();

  html.table();

  html.row2If(tr("Designation:"), map::airspaceRestrictiveName(airspace));
  html.row2If(tr("Type:"), map::airspaceTypeToString(airspace.type));

  if(!airspace.isOnline())
  {
    if(airspace.minAltitudeType.isEmpty())
      html.row2(tr("Min altitude:"), tr("Unknown"));
    else
      html.row2(tr("Min altitude:"), Unit::altFeet(airspace.minAltitude) % tr(" ") % airspace.minAltitudeType);

    QString maxAlt;
    if(airspace.maxAltitudeType.isEmpty())
      maxAlt = tr("Unknown");
    else if(airspace.maxAltitudeType == "UL")
      maxAlt = tr("Unlimited");
    else
      maxAlt = Unit::altFeet(airspace.maxAltitude) % tr(" ") % airspace.maxAltitudeType;

    html.row2(tr("Max altitude:"), maxAlt);
  }

  html.row2If(tr("Multiple code:"), airspace.multipleCode);
  html.row2If(tr("COM:"), formatter::capNavString(airspace.comName));
  html.row2If(tr("COM Type:"), map::comTypeName(airspace.comType));
  if(!airspace.comFrequencies.isEmpty())
  {
    QStringList freqTxt;
    for(int freq : airspace.comFrequencies)
    {
      // Round frequencies to nearest valid value to workaround for a compiler rounding bug

      if(airspace.isOnline())
        // Use online freqencies as is - convert kHz to MHz
        freqTxt.append(locale.toString(freq / 1000.f, 'f', 3));
      else
        freqTxt.append(locale.toString(roundComFrequency(freq), 'f', 3));

#ifdef DEBUG_INFORMATION
      if(info)
        freqTxt.last().append(QString(" [%1]").arg(freq));
#endif
    }

    html.row2(freqTxt.size() > 1 ? tr("COM Frequencies:") : tr("COM Frequency:"), freqTxt.join(tr(", ")) % tr(" MHz"));
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
    html.row2If(tr("Connection Time:"), locale.toString(onlineRec.valueDateTime("connection_time")));

    // Always unknown
    // html.row2If(tr("Simulator:"), atools::fs::online::simulatorText(onlineRec.valueInt("simulator")));
  }
  html.tableEnd();

  if(info && !airspace.isOnline())
  {
    atools::sql::SqlRecord rec = NavApp::getAirspaceController()->getAirspaceInfoRecordById(airspace.combinedId());

    if(!rec.isEmpty())
      addScenery(&rec, html, DATASOURCE_NAV);
  }

#ifdef DEBUG_INFORMATION
  if(info)
  {
    html.small(QString("Database: source = %1, boundary_id = %2").
               arg(map::airspaceSourceText(airspace.src)).arg(airspace.getId())).br();
    html.small(QString("name = %1, comName = %2, comType = %3, minAltitudeType = %4, maxAltitudeType = %5, "
                       "multipleCode = %6, restrictiveDesignation = %7, restrictiveType = %8, timeCode = %9").
               arg(airspace.name).arg(airspace.comName).arg(airspace.comType).arg(airspace.minAltitudeType).
               arg(airspace.maxAltitudeType).arg(airspace.multipleCode).arg(airspace.restrictiveDesignation).
               arg(airspace.restrictiveType).arg(airspace.timeCode));
  }
#endif

}

void HtmlInfoBuilder::airwayText(const MapAirway& airway, HtmlBuilder& html) const
{
  bool isAirway = airway.isAirway();

  if(!isAirway && !NavApp::hasTracks())
    return;

  if(isAirway)
    navaidTitle(html, tr("Airway: ") % airway.name);
  else
    navaidTitle(html, tr("Track: ") % airway.name);

  if(info)
  {
    // Add map link if not tooltip
    html.nbsp().nbsp();
    html.a(tr("Map"), QString("lnm://show?id=%1&fragment=%2&type=%3").
           arg(airway.id).arg(airway.fragment).arg(map::AIRWAY), ahtml::BOLD | ahtml::LINK_NO_UL);
  }

  html.table();

  if(isAirway)
  {
    if(verbose)
      html.row2If(tr("Segment type:"), map::airwayTrackTypeToString(airway.type));

    if(info)
      html.row2If(tr("Route type:"), map::airwayRouteTypeToString(airway.routeType));
  }
  else
    html.row2If(tr("Track type:"), map::airwayTrackTypeToString(airway.type));

  WaypointTrackQuery *waypointQuery = mapWidget->getWaypointTrackQuery();
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
    tempHtml.text(tr("%1%2%3").arg(identRegionText(from.ident, from.region)).arg(connector).arg(identRegionText(to.ident, to.region)));

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

  if(verbose)
    html.row2(tr("Segment length:"), Unit::distMeter(airway.from.distanceMeterTo(airway.to)));

  atools::sql::SqlRecord trackMeta;
  if(infoQuery != nullptr && info)
  {
    if(!isAirway)
    {
      trackMeta = infoQuery->getTrackMetadata(airway.id);
      if(!trackMeta.isEmpty())
      {
        QDateTime validFrom = trackMeta.valueDateTime("valid_from");
        QDateTime validTo = trackMeta.valueDateTime("valid_to");
        QDateTime validNow = QDateTime::currentDateTimeUtc();

        if(!validFrom.isNull() && !validTo.isNull())
          html.row2(tr("Track valid:"), tr("%1 UTC to<br/>%2 UTC%3").
                    arg(locale.toString(validFrom, QLocale::ShortFormat)).
                    arg(locale.toString(validTo, QLocale::ShortFormat)).
                    arg(validNow >= validFrom && validNow <= validTo ?
                        tr("<br/><b>Track is now valid.</b>") : QString()),
                    ahtml::NO_ENTITIES);
        else
          html.row2(tr("Track valid:"), tr("No validity period"));

        html.row2(tr("Track downloaded:"),
                  locale.toString(trackMeta.valueDateTime("download_timestamp"), QLocale::ShortFormat));
      }
    }

    // Show list of waypoints =================================================================
    QList<map::MapAirwayWaypoint> waypointList;
    NavApp::getAirwayTrackQueryGui()->getWaypointListForAirwayName(waypointList, airway.name, airway.fragment);

    if(!waypointList.isEmpty())
    {
      HtmlBuilder tempLinkHtml = html.cleared();
      for(const map::MapAirwayWaypoint& airwayWaypoint : qAsConst(waypointList))
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
  if(info)
  {
    if(!trackMeta.isEmpty())
      html.small(trackMeta.valueStr("route")).br();
    html.small(ageo::Line(airway.from, airway.to).crossesAntiMeridian() ? "cross am" : "-").br();
    html.small(airway.westCourse ? "west" : "").br();
    html.small(airway.eastCourse ? "east" : "").br();
    html.small(QString("Database: airway_id = %1").arg(airway.getId())).br();
  }
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
    head(html, locale.toString(roundComFrequency(airport.towerFrequency), 'f', 3) % tr(" MHz"));
  }
  else
    head(html, airport.xplane ? tr("Tower Viewpoint") : tr("Tower"));
}

void HtmlInfoBuilder::parkingText(const MapParking& parking, HtmlBuilder& html) const
{
  head(html, map::parkingName(parking.name) % (parking.number != -1 ? " " % locale.toString(parking.number) % parking.suffix : QString()));

  if(NavApp::getRouteConst().getDepartureParking().id == parking.id)
    html.brText(tr("Flight plan departure parking"), ahtml::BOLD);

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
    html.br().text(tr("Airline Codes: ") % txt, ahtml::NOBR | ahtml::REPLACE_CRLF);
  }
  html.br();
}

void HtmlInfoBuilder::userpointTextRoute(const MapUserpointRoute& userpoint, HtmlBuilder& html) const
{
#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO;
#endif

  QIcon icon = SymbolPainter::createUserpointIcon(symbolSizeTitle.height());
  html.img(icon, QString(), QString(), symbolSizeTitle);
  html.nbsp().nbsp();

  navaidTitle(html, tr("Position: ") % userpoint.ident);

  if(!info && userpoint.routeIndex >= 0)
  {
    if(!userpoint.name.isEmpty() || !userpoint.comment.isEmpty() || !userpoint.region.isEmpty())
    {
      html.table();
      html.row2If(tr("Flight Plan Position:"), QString::number(userpoint.routeIndex + 1));
      if(verbose)
        html.row2If(tr("Region:"), userpoint.region);
      html.row2If(tr("Name:"), userpoint.name);
      html.row2If(tr("Remarks:"), atools::elideTextLinesShort(userpoint.comment, 20, 80));
      html.tableEnd();
    }
    else
      html.p().b(tr("Flight Plan Position: ") % QString::number(userpoint.routeIndex + 1)).pEnd();

    if(!info)
      routeWindText(html, NavApp::getRouteConst(), userpoint.routeIndex);
  }
}

void HtmlInfoBuilder::procedurePointText(const map::MapProcedurePoint& procPoint, HtmlBuilder& html, const Route *route) const
{
  QString header;
  const proc::MapProcedureLegs *legs = procPoint.legs;
  const proc::MapProcedureLeg& leg = procPoint.getLeg();

  if(!procPoint.preview && route != nullptr)
    // Part of the route
    header =
      route->getProcedureLegText(leg.mapType, true /* includeRunway */, false /* missedAsApproach */, false /* transitionAsProcedure */);
  else
    header = map::procedurePointText(procPoint);

  QIcon icon;

  if(legs->isAnyCustom())
  {
    if(legs->isCustomDeparture())
      icon = QIcon(":/littlenavmap/resources/icons/runwaydepart.svg");
    else
      icon = QIcon(":/littlenavmap/resources/icons/runwaydest.svg");
  }
  else
  {
    if(leg.mapType.testFlag(proc::PROCEDURE_MISSED))
      icon = QIcon(":/littlenavmap/resources/icons/missed.svg");
    else
      icon = QIcon(":/littlenavmap/resources/icons/approach.svg");
  }

  html.img(icon, QString(), QString(), symbolSizeTitle);
  html.nbsp().nbsp();

  if(procPoint.legs->previewColor.isValid() && procPoint.previewAll)
  {
    QIcon procIcon = SymbolPainter::createProcedurePreviewIcon(procPoint.legs->previewColor, symbolSizeTitle.height());
    html.img(procIcon, QString(), QString(), symbolSizeTitle);
    html.nbsp().nbsp();
  }

  navaidTitle(html, header);

  html.table();

  if(!info && procPoint.routeIndex >= 0)
    html.row2(tr("Flight Plan Position:"), locale.toString(procPoint.routeIndex + 1));

  if(!legs->isAnyCustom() && (procPoint.preview || procPoint.previewAll))
    html.row2(tr("First and last Fix:"), atools::strJoin(procedureTextFirstAndLastFix(*legs, leg.mapType), tr(", "), tr(" and ")));

  if(!procPoint.previewAll)
  {
    // Add IAF, MAP, ...
    QString typeStr, type = proc::proceduresLegSecialTypeLongStr(proc::specialType(leg.arincDescrCode));

    if(type.isEmpty())
      typeStr = tr("Fix:");
    else
      typeStr = tr("%1:").arg(type);

    if(!legs->isCustomDeparture())
    {
      html.row2If(typeStr, leg.fixIdent);

      if(verbose)
      {
        if(!legs->isAnyCustom())
          html.row2If(tr("Leg Type:"), proc::procedureLegTypeStr(leg.type));

        if(leg.calculatedDistance > 0.f)
          html.row2(tr("Distance:"), Unit::distNm(leg.calculatedDistance /*, true, 20, true*/));

        if(leg.time > 0.f)
          html.row2(tr("Time:"), locale.toString(leg.time, 'f', 0) % tr(" min"));

        if(leg.calculatedTrueCourse < map::INVALID_COURSE_VALUE)
          html.row2(tr("Course:"), courseTextFromTrue(leg.calculatedTrueCourse, leg.magvar), ahtml::NO_ENTITIES);

        if(leg.flyover)
          html.row2(tr("Fly over"));

        if(!leg.remarks.isEmpty())
          html.row2(leg.remarks.join(", "));
      }

      if(leg.altRestriction.isValid())
        html.row2(tr("Altitude Restriction:"), proc::altRestrictionText(leg.altRestriction));

      if(leg.speedRestriction.isValid())
        html.row2(tr("Speed Restriction:"), proc::speedRestrictionText(leg.speedRestriction));
    }

    if(!leg.turnDirection.isEmpty())
    {
      if(leg.turnDirection == "L")
        html.row2(tr("Turn:"), tr("Left"));
      else if(leg.turnDirection == "R")
        html.row2(tr("Turn:"), tr("Right"));
      else if(leg.turnDirection == "B")
        html.row2(tr("Turn:"), tr("Left or right"));
    }

    if(verbose)
      html.row2If(tr("Related Navaid:"), proc::procedureLegRecommended(leg).join(tr(", ")), ahtml::NO_ENTITIES);
  }
  html.tableEnd();
}

void HtmlInfoBuilder::aircraftText(const atools::fs::sc::SimConnectAircraft& aircraft, HtmlBuilder& html, int num, int total)
{
#ifdef DEBUG_INFORMATION
  html.textBr("[HtmlInfoBuilder::aircraftText()]").textBr(QString("[online %1 shadow %2 id %3]").
                                                          arg(aircraft.isOnline()).
                                                          arg(aircraft.isOnlineShadow()).
                                                          arg(aircraft.getObjectId()));
#endif

  // Icon and title
  aircraftTitle(aircraft, html);

  // Show note for online/simulator shadow relation ===========================
  if(aircraft.isOnlineShadow())
  {
    atools::fs::sc::SimConnectAircraft onlineAircraft = NavApp::getOnlinedataController()->getShadowedOnlineAircraft(aircraft);
    if(onlineAircraft.isValid())
      html.p(tr("Simulator aircraft for online client %1.").arg(onlineAircraft.getAirplaneRegistration()));
  }
  else if(aircraft.isOnline())
  {
    atools::fs::sc::SimConnectAircraft simAircraft = NavApp::getOnlinedataController()->getShadowSimAircraft(aircraft.getId());
    if(simAircraft.isValid())
      html.p(tr("Online client related to simulator aircraft %1.").arg(simAircraft.getAirplaneRegistration()));
  }

  if(verbose)
  {
    // Heading, altitude and speed ======================
    QStringList hdg;
    float heading = atools::fs::sc::SC_INVALID_FLOAT;
    if(aircraft.getHeadingDegMag() < atools::fs::sc::SC_INVALID_FLOAT)
      heading = aircraft.getHeadingDegMag();
    else if(aircraft.getHeadingDegTrue() < atools::fs::sc::SC_INVALID_FLOAT)
      heading = normalizeCourse(aircraft.getHeadingDegTrue() - NavApp::getMagVar(aircraft.getPosition()));

    if(heading < atools::fs::sc::SC_INVALID_FLOAT)
      hdg.append(courseText(heading, aircraft.getHeadingDegTrue()));

    QStringList texts;
    // Heading if available
    if(!hdg.isEmpty())
      texts.append(tr("<b>Heading</b>&nbsp;%1").arg(hdg.join(tr(", "))));

    // Actual or indicated altitude
    if(aircraft.getActualAltitudeFt() < atools::fs::sc::SC_INVALID_FLOAT)
      texts.append(tr("<b>Act. Altitude</b>&nbsp;%1").arg(Unit::altFeet(aircraft.getActualAltitudeFt())));
    else if(aircraft.getIndicatedAltitudeFt() < atools::fs::sc::SC_INVALID_FLOAT)
      texts.append(tr("<b>Ind. Altitude</b>&nbsp;%1").arg(Unit::altFeet(aircraft.getIndicatedAltitudeFt())));

    // Ground speed always
    if(aircraft.getGroundSpeedKts() > 1.f && aircraft.getGroundSpeedKts() < atools::fs::sc::SC_INVALID_FLOAT)
      texts.append(tr("<b>Groundspeed</b>&nbsp;%1").arg(Unit::speedKts(aircraft.getGroundSpeedKts())));

    // Indicated speed if flying
    if(!aircraft.isOnGround() && aircraft.getIndicatedSpeedKts() < atools::fs::sc::SC_INVALID_FLOAT)
      texts.append(tr("<b>Ind. Speed</b>&nbsp;%1").arg(Unit::speedKts(aircraft.getIndicatedSpeedKts())));

    html.p(atools::strJoin(texts, tr(", ")), ahtml::NO_ENTITIES);
  }

  QString aircraftText;
  QString typeText;
  QString type = map::aircraftTypeString(aircraft);
  if(aircraft.isUser() && aircraft.isOnlineShadow())
    typeText = tr("User %1 / Online Client").arg(type);
  else if(aircraft.isUser())
    typeText = tr("User %1").arg(type);
  else if(aircraft.isOnlineShadow())
    typeText = tr("AI / Multiplayer %1 / Online Client").arg(type);
  else if(aircraft.isOnline())
    typeText = tr("Online Client");
  else
    typeText = tr("AI / Multiplayer %1").arg(type);

  if(aircraft.isUser())
  {
    aircraftText = typeText;
    if(info && !(NavApp::getShownMapTypes().testFlag(map::AIRCRAFT)))
      html.p(tr("User aircraft is not shown on map."), ahtml::BOLD);
  }
  else
  {
    if(num != -1 && total != -1)
      aircraftText = tr("%1 - %2 of %3").arg(typeText).arg(num).arg(total);
    else
      aircraftText = typeText;

    if(info && num == 1 && !(NavApp::getShownMapTypes().testFlag(map::AIRCRAFT_AI)))
      html.p(tr("No %2 shown on map.").arg(typeText), ahtml::BOLD);

    if(info && num == 1 && !(NavApp::getShownMapTypes().testFlag(map::AIRCRAFT_ONLINE)))
      html.p(tr("No online aircraft shown on map."), ahtml::BOLD);
  }

  head(html, aircraftText);

  html.table();
  if(!aircraft.getAirplaneTitle().isEmpty())
    html.row2(tr("Title:"), aircraft.getAirplaneTitle()); // Asobo Baron G58
  else
    html.row2(tr("Number:"), locale.toString(aircraft.getObjectId() + 1));

  html.row2If(tr("Airline:"), aircraft.getAirplaneAirline());

  html.row2If(tr("Flight Number:"), aircraft.getAirplaneFlightnumber());

  html.row2If(tr("Transponder Code:"), aircraft.getTransponderCodeStr());

  html.row2If(tr("Type:"), aircraft.getAirplaneModel()); // BE58

  html.row2If(tr("Registration:"), aircraft.getAirplaneRegistration()); // ASXGS

  QString model = map::aircraftType(aircraft);
  html.row2If(tr("Model:"), model); // Beechcraft

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
  if(info)
  {
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
  }
#endif
}

void HtmlInfoBuilder::aircraftOnlineText(const atools::fs::sc::SimConnectAircraft& aircraft,
                                         const atools::sql::SqlRecord& onlineRec, HtmlBuilder& html)
{
#ifdef DEBUG_INFORMATION
  html.textBr("[HtmlInfoBuilder::aircraftOnlineText()]");
#endif

  if(!onlineRec.isEmpty() && info)
  {
#ifdef DEBUG_INFORMATION
    qDebug() << Q_FUNC_INFO << onlineRec;
#endif

    // General online information =================================================================
    head(html, tr("Online Information"));
    html.table();
    html.row2If(tr("VID:"), onlineRec.valueStr("vid"));
    html.row2If(tr("Name:"), onlineRec.valueStr("name"));
    html.row2If(tr("Server:"), onlineRec.valueStr("server"));
    html.row2If(tr("Connection Time:"), locale.toString(onlineRec.valueDateTime("connection_time")));
    html.row2If(tr("Simulator:"), atools::fs::online::simulatorText(onlineRec.valueInt("simulator")));
    html.tableEnd();

    // Flight plan information =================================================================
    head(html, tr("Flight Plan"));
    html.table();

    html.row2If(tr("State:"), onlineRec.valueStr("state"));

    QString spd = onlineRec.valueStr("flightplan_cruising_speed");
    QString alt = onlineRec.valueStr("flightplan_cruising_level");

    // Decode speed and altitude
    float speed, altitude;
    bool speedOk, altitudeOk;
    atools::fs::util::extractSpeedAndAltitude(spd % (alt == "VFR" ? QString() : alt), speed, altitude, &speedOk, &altitudeOk);

    if(speedOk)
      html.row2If(tr("Cruising Speed"), tr("%1 (%2)").arg(spd).arg(Unit::speedKts(speed)));
    else
      html.row2If(tr("Cruising Speed"), spd);

    if(altitudeOk)
      html.row2If(tr("Cruising Level:"), tr("%1 (%2)").arg(alt).arg(Unit::altFeet(altitude)));
    else
      html.row2If(tr("Cruising Level:"), alt);

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
      html.row2(tr("Estimated en-route time hh:mm:"), formatter::formatMinutesHours(enrouteMin / 60.));
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
      route.prepend(departure % " ");
    QString destination = aircraft.getToIdent();
    if(!route.endsWith(destination))
      route.append(" " % destination);

    html.row2If(tr("Route:"), route);
    html.row2If(tr("Other Information:"), onlineRec.valueStr("flightplan_other_info"));
    html.row2If(tr("Persons on Board:"), onlineRec.valueInt("flightplan_persons_on_board"));
    html.tableEnd();

    if(info && aircraft.isValid())
    {
      head(html, tr("Position"));
      html.table();
      addCoordinates(aircraft.getPosition(), html);
      html.tableEnd();
    }
  }
}

void HtmlInfoBuilder::aircraftTextWeightAndFuel(const atools::fs::sc::SimConnectUserAircraft& userAircraft,
                                                HtmlBuilder& html) const
{
  if(!userAircraft.isValid())
    return;

  if(info)
  {
    head(html, tr("Weight and Fuel"));
    html.table().row2AlignRight();

    float maxGrossWeight = userAircraft.getAirplaneMaxGrossWeightLbs();
    float grossWeight = userAircraft.getAirplaneTotalWeightLbs();

    html.row2(tr("Max Gross Weight:"), Unit::weightLbsLocalOther(maxGrossWeight), ahtml::NO_ENTITIES | ahtml::BOLD);

    if(grossWeight > maxGrossWeight)
      html.row2Error(tr("Gross Weight:"), Unit::weightLbsLocalOther(grossWeight, false, false));
    else
    {
      QString weight = HtmlBuilder::textStr(Unit::weightLbsLocalOther(grossWeight, true /* bold */), ahtml::NO_ENTITIES | ahtml::BOLD);
      QString percent = tr("<br/>%1 % of max gross weight").arg(100.f / maxGrossWeight * grossWeight, 0, 'f', 0);

      html.row2(tr("Gross Weight:"), weight % percent, ahtml::NO_ENTITIES);
    }

    html.row2(QString());
    html.row2(tr("Empty Weight:"), Unit::weightLbsLocalOther(userAircraft.getAirplaneEmptyWeightLbs()),
              ahtml::NO_ENTITIES);
    html.row2(tr("Zero Fuel Weight:"), Unit::weightLbsLocalOther(userAircraft.getAirplaneTotalWeightLbs() -
                                                                 userAircraft.getFuelTotalWeightLbs()), ahtml::NO_ENTITIES);

    html.row2(tr("Total Payload:"), Unit::weightLbsLocalOther(userAircraft.getAirplaneTotalWeightLbs() -
                                                              userAircraft.getAirplaneEmptyWeightLbs() -
                                                              userAircraft.getFuelTotalWeightLbs()), ahtml::NO_ENTITIES);

    html.row2(tr("Fuel:"), Unit::fuelLbsAndGalLocalOther(userAircraft.getFuelTotalWeightLbs(),
                                                         userAircraft.getFuelTotalQuantityGallons(), true /* bold */), ahtml::NO_ENTITIES);
    html.tableEnd().row2AlignRight(false);
  }
}

void HtmlInfoBuilder::aircraftTrailText(const AircraftTrailSegment& trailSegment, HtmlBuilder& html, bool logbook) const
{
  if(logbook)
    // Prefix with logbook icon for logbook trails ===========================
    html.img(QIcon(":/littlenavmap/resources/icons/logbook.svg"), QString(), QString(), symbolSizeTitle);

  if(OptionData::instance().getFlags().testFlag(opts::MAP_TRAIL_GRADIENT))
  {
    // Show color gradient SVG =================================
    switch(OptionData::instance().getDisplayTrailGradientType())
    {
      case opts::TRAIL_GRADIENT_COLOR_YELLOW_BLUE:
        html.img(QIcon(":/littlenavmap/resources/icons/icongradientcolors2.svg"), QString(), QString(), symbolSizeTitle);
        break;

      case opts::TRAIL_GRADIENT_COLOR_RAINBOW:
        html.img(QIcon(":/littlenavmap/resources/icons/icongradientcolors.svg"), QString(), QString(), symbolSizeTitle);
        break;

      case opts::TRAIL_GRADIENT_BLACKWHITE:
        html.img(QIcon(":/littlenavmap/resources/icons/icongradientblackwhite.svg"), QString(), QString(), symbolSizeTitle);
        break;
    }
  }
  else
  {
    // Create icon showing the same line style as configured by the user or logbook icon ==============
    float thicknessTrail = OptionData::instance().getDisplayThicknessTrail() / 100.f;
    html.img(SymbolPainter::createAircraftTrailIcon(symbolSizeTitle.height(), mapcolors::aircraftTrailPen(thicknessTrail * 2.f)),
             QString(), QString(), symbolSizeTitle);
  }

  html.nbsp().nbsp();

  if(logbook)
    html.text("Logbook Aircraft Trail", ahtml::BOLD);
  else
    html.text("User Aircraft Trail", ahtml::BOLD);

  html.table();

  // Simulator date and time with seconds - interpolated ========================
  QDateTime datetime = QDateTime::fromMSecsSinceEpoch(trailSegment.timestampPos, Qt::UTC);
  bool overrideLocale = OptionData::instance().getFlags().testFlag(opts::GUI_OVERRIDE_LOCALE);
  html.row2(tr("Simulator Date and Time:"),
            formatter::formatDateTimeSeconds(datetime, overrideLocale) % tr(" ") % datetime.timeZoneAbbreviation());

  // Speed through segment ==================================
  if(atools::geo::meterPerSecToKnots(trailSegment.speed) < 10000.f)
    html.row2(tr("Groundspeed:"), Unit::speedMeterPerSec(trailSegment.speed));

  // Course ==================================
  if(trailSegment.headingTrue < map::INVALID_HEADING_VALUE)
    html.row2(tr("Course:", "aircraft course"), courseTextFromTrue(trailSegment.headingTrue, NavApp::getMagVar(trailSegment.to)),
              ahtml::NO_ENTITIES);

  // Long line segments =========================================
  if(trailSegment.length > 5000.)
    html.row2(tr("Trail Segment Length (long jump):"), Unit::distMeter(static_cast<float>(trailSegment.length)));

  // Distance to current point ========================
  html.row2(tr("Distance Flown:"), Unit::distMeter(static_cast<float>(trailSegment.distanceFromStart)));

  // Interpolated altitude ====================================
  html.row2(tr("Actual Altitude:"), Unit::altFeet(trailSegment.getAltitude()));

  // On ground weighted/interpolated ====================================
  if(trailSegment.onGround)
    html.row2(tr("On ground"));
  html.tableEnd();

#ifdef DEBUG_INFORMATION
  html.small("[Index]" % QString::number(trailSegment.index) %
             " [length]" % Unit::distMeter(static_cast<float>(trailSegment.length)) %
             " [Time]" % QString::number(trailSegment.timestampPos) %
             " [alt diff]" % QString::number(trailSegment.from.getAltitude() - trailSegment.to.getAltitude())
             );
#endif

  if(info)
    html.br();
}

void HtmlInfoBuilder::dateTimeAndFlown(const SimConnectUserAircraft *userAircraft, HtmlBuilder& html) const
{
  html.id(pid::DATE_TIME_REAL).row2(tr("Real Date and Time:"),
                                    locale.toString(QDateTime::currentDateTimeUtc(), QLocale::ShortFormat) % tr(" ") %
                                    QDateTime::currentDateTimeUtc().timeZoneAbbreviation());

  html.id(pid::LOCAL_TIME_REAL).row2(tr("Real Local Time:"),
                                     locale.toString(QDateTime::currentDateTime(), QLocale::ShortFormat) % tr(" ") %
                                     QDateTime::currentDateTime().timeZoneAbbreviation());

  html.id(pid::DATE_TIME).row2(tr("Simulator Date and Time:"),
                               locale.toString(userAircraft->getZuluTime(), QLocale::ShortFormat) % tr(" ") %
                               userAircraft->getZuluTime().timeZoneAbbreviation());

  html.id(pid::LOCAL_TIME).row2(tr("Simulator Local Time:"),
                                locale.toString(userAircraft->getLocalTime().time(), QLocale::ShortFormat) % tr(" ") %
                                userAircraft->getLocalTime().timeZoneAbbreviation()

#ifdef DEBUG_INFORMATION
                                + "[" + locale.toString(userAircraft->getLocalTime(), QLocale::ShortFormat) + "]"
#endif
                                );

  // Flown distance ========================================
  // Stored in map widget
  float distanceFlownNm = NavApp::getTakeoffFlownDistanceNm();
  QDateTime takeoffDateTime = NavApp::getTakeoffDateTime();
  if(distanceFlownNm > 0.f && distanceFlownNm < map::INVALID_DISTANCE_VALUE && takeoffDateTime.isValid())
    html.id(pid::FLOWN).row2(tr("Flown:"), tr("%1 since takoff at %2").arg(Unit::distNm(distanceFlownNm)).
                             arg(locale.toString(takeoffDateTime.time(), QLocale::ShortFormat)) %
                             tr(" ") % takeoffDateTime.timeZoneAbbreviation());
}

void HtmlInfoBuilder::aircraftProgressText(const atools::fs::sc::SimConnectAircraft& aircraft, HtmlBuilder& html, const Route& route)
{
  if(!aircraft.isValid())
    return;

  const SimConnectUserAircraft *userAircraft = dynamic_cast<const SimConnectUserAircraft *>(&aircraft);
  AircraftPerfController *perfController = NavApp::getAircraftPerfController();

  // Fuel and time calculated or estimated
  FuelTimeResult fuelTime;

  if(info && userAircraft != nullptr)
  {
    aircraftTitle(aircraft, html);
    if(!(NavApp::getShownMapTypes() & map::AIRCRAFT))
      html.warning(tr("User aircraft is not shown on map."));
  }

  float distFromStartNm = 0.f, distToDestNm = 0.f, nextLegDistance = 0.f, crossTrackDistance = 0.f;

  html.row2AlignRight();

  // Flight plan legs =========================================================================
  float distanceToTod = map::INVALID_DISTANCE_VALUE, distanceToToc = map::INVALID_DISTANCE_VALUE;
  if(!route.isEmpty() && userAircraft != nullptr && info)
  {
    // The corrected leg will point to an approach leg if we head to the start of a procedure
    bool corrected = false;
    int activeLegIdxCorrected = route.getActiveLegIndexCorrected(&corrected);
    int activeLegIdx = route.getActiveLegIndex();
    bool alternate = route.isActiveAlternate();
    bool destination = route.isActiveDestinationAirport();

    if(activeLegIdxCorrected != map::INVALID_INDEX_VALUE &&
       route.getRouteDistances(&distFromStartNm, &distToDestNm, &nextLegDistance, &crossTrackDistance))
    {
      if(distFromStartNm < map::INVALID_DISTANCE_VALUE)
      {
        distanceToTod = route.getTopOfDescentDistance() - distFromStartNm;
        distanceToToc = route.getTopOfClimbDistance() - distFromStartNm;
      }

      if(alternate)
        // Use distance to alternate instead of destination
        distToDestNm = nextLegDistance;

      // Calculates values based on performance profile if valid - otherwise estimated by aircraft fuel flow and speed
      perfController->calculateFuelAndTimeTo(fuelTime, distToDestNm, nextLegDistance, activeLegIdx);

      // Print warning messages ===================================================================
      if(route.getSizeWithoutAlternates() < 2)
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

      // ================================================================================
      // Current date and time ========================================
      html.tableIf();
      dateTimeAndFlown(userAircraft, html);
      html.tableEndIf();

      // Route distances ===============================================================
      if(distToDestNm < map::INVALID_DISTANCE_VALUE)
      {
        // Add name and type to header text
        QString headText = alternate ? tr("Alternate") : tr("Destination");
        const RouteLeg& destLeg = alternate && route.getActiveLeg() != nullptr ? *route.getActiveLeg() : route.getDestinationAirportLeg();

        headText += tr(" - ") % destLeg.getDisplayIdent() % (destLeg.getMapTypeName().isEmpty() ? QString() : tr(", ") %
                                                             destLeg.getMapTypeName());

        html.mark();
        head(html, headText);
        html.table();

        // ================================================================================
        // Destination  ===============================================================
        bool timeToDestAvailable = fuelTime.isTimeToDestValid();
        bool fuelToDestAvailable = fuelTime.isFuelToDestValid();

        QString destinationText = timeToDestAvailable ? tr("Distance, Time and Arrival:") : tr("Distance:");
        if(route.isActiveMissed())
          // RouteAltitude legs does not provide values for missed - calculate them based on aircraft
          destinationText = tr("To End of Missed Approach:");

        if(timeToDestAvailable)
        {
          QDateTime arrival = userAircraft->getZuluTime().addSecs(static_cast<int>(fuelTime.timeToDest * 3600.f));

          html.id(pid::DEST_DIST_TIME_ARR).row2(destinationText, strJoinVal({
            Unit::distNm(distToDestNm),
            formatter::formatMinutesHoursLong(fuelTime.timeToDest),
            locale.toString(arrival.time(), QLocale::ShortFormat) % tr(" ") % arrival.timeZoneAbbreviation()}));
        }
        else
          html.id(pid::DEST_DIST_TIME_ARR).row2(destinationText, Unit::distNm(distToDestNm));

        if(fuelToDestAvailable)
        {
          // Remaining fuel estimate at destination
          float remainingFuelLbs = userAircraft->getFuelTotalWeightLbs() - fuelTime.fuelLbsToDest;
          float remainingFuelGal = userAircraft->getFuelTotalQuantityGallons() - fuelTime.fuelGalToDest;
          QString fuelValue = Unit::fuelLbsAndGalLocalOther(remainingFuelLbs, remainingFuelGal);
          QString grossWeight = Unit::weightLbsLocalOther(userAircraft->getAirplaneTotalWeightLbs() - fuelTime.fuelLbsToDest);

          if(!fuelTime.estimatedFuel && !fuelTime.estimatedTime)
          {
            // Display warnings for fuel at destination only if fuel was calculated from a valid performance and profile
            if(remainingFuelLbs <= 0.1f || remainingFuelGal <= 0.1f)
              html.id(pid::DEST_FUEL).row2Error(tr("Fuel:"), tr("Insufficient"));
            else if(remainingFuelLbs < perfController->getFuelReserveAtDestinationLbs() ||
                    remainingFuelGal < perfController->getFuelReserveAtDestinationGal())
              // Required reserves at destination
              html.id(pid::DEST_FUEL).row2Warning(tr("Fuel (low):"),
                                                  Unit::fuelLbsAndGalLocalOther(remainingFuelLbs, remainingFuelGal, false, false));
            else
              // No warning - display plain values
              html.id(pid::DEST_FUEL).row2(tr("Fuel:"), fuelValue, ahtml::NO_ENTITIES);
            html.id(pid::DEST_GROSS_WEIGHT).row2(tr("Gross Weight:"), grossWeight, ahtml::NO_ENTITIES);
          }
          else
          {
            // No warnings for estimated fuel
            html.id(pid::DEST_FUEL).row2(tr("Fuel (estimated):"), fuelValue, ahtml::NO_ENTITIES);
            html.id(pid::DEST_GROSS_WEIGHT).row2(tr("Gross Weight (estimated):"), grossWeight, ahtml::NO_ENTITIES);
          }
        } // if(!less && fuelAvailable)
        html.tableEndIf();
      } // if(distToDestNm < map::INVALID_DISTANCE_VALUE)

      if(route.getSizeWithoutAlternates() > 1 && !alternate) // No TOD display when flying an alternate leg
      {
        // ================================================================================
        // Top of descent  ===============================================================
        html.mark();
        head(html, tr("Top of Descent%1").arg(distanceToTod < 0.f ? tr(" (passed)") : QString()));
        html.table();

        if(!(distanceToTod < map::INVALID_DISTANCE_VALUE))
          html.id(pid::TOD_DIST_TIME_ARR).row2Error(QString(), tr("Not valid."));

        if(distanceToTod > 0.f && distanceToTod < map::INVALID_DISTANCE_VALUE)
        {
          QStringList todValues, todHeader;
          todValues.append(Unit::distNm(distanceToTod));
          todHeader.append(tr("Distance"));

          if(fuelTime.isTimeToTodValid())
          {
            todValues.append(formatter::formatMinutesHoursLong(fuelTime.timeToTod));
            todHeader.append(tr("Time"));

            QDateTime arrival = userAircraft->getZuluTime().addSecs(static_cast<int>(fuelTime.timeToTod * 3600.f));
            todValues.append(locale.toString(arrival.time(), QLocale::ShortFormat) % tr(" ") % arrival.timeZoneAbbreviation());
            todHeader.append(tr("Arrival"));
          }
          html.id(pid::TOD_DIST_TIME_ARR).row2(strJoinHdr(todHeader), strJoinVal(todValues));

          if(fuelTime.isFuelToTodValid())
          {
            // Calculate remaining fuel at TOD
            float remainingFuelTodLbs = userAircraft->getFuelTotalWeightLbs() - fuelTime.fuelLbsToTod;
            float remainingFuelTodGal = userAircraft->getFuelTotalQuantityGallons() - fuelTime.fuelGalToTod;

            html.id(pid::TOD_FUEL).row2(tr("Fuel:"), Unit::fuelLbsAndGal(remainingFuelTodLbs, remainingFuelTodGal), ahtml::NO_ENTITIES);
          }
        } // if(distanceToTod > 0 && distanceToTod < map::INVALID_DISTANCE_VALUE)

        if(route.getTopOfDescentFromDestination() < map::INVALID_DISTANCE_VALUE)
          html.id(pid::TOD_TO_DESTINATION).row2(tr("To Destination:"), Unit::distNm(route.getTopOfDescentFromDestination()));

        html.tableEndIf();

        // ================================================================================
        // Top of climb  ===============================================================
        html.mark();
        head(html, tr("Top of Climb%1").arg(distanceToToc < 0.f ? tr(" (passed)") : QString()));
        html.table();

        if(!(distanceToToc < map::INVALID_DISTANCE_VALUE))
          html.id(pid::TOC_DIST_TIME_ARR).row2Error(QString(), tr("Not valid."));

        if(distanceToToc > 0.f && distanceToToc < map::INVALID_DISTANCE_VALUE)
        {
          QStringList tocValues, tocHeader;
          tocValues.append(Unit::distNm(distanceToToc));
          tocHeader.append(tr("Distance"));

          if(fuelTime.isTimeToTocValid())
          {
            tocValues.append(formatter::formatMinutesHoursLong(fuelTime.timeToToc));
            tocHeader.append(tr("Time"));

            QDateTime arrival = userAircraft->getZuluTime().addSecs(static_cast<int>(fuelTime.timeToToc * 3600.f));
            tocValues.append(locale.toString(arrival.time(), QLocale::ShortFormat) % tr(" ") % arrival.timeZoneAbbreviation());
            tocHeader.append(tr("Arrival"));
          }
          html.id(pid::TOC_DIST_TIME_ARR).row2(strJoinHdr(tocHeader), strJoinVal(tocValues));

          if(fuelTime.isFuelToTocValid())
          {
            // Calculate remaining fuel at TOC
            float remainingFuelTocLbs = userAircraft->getFuelTotalWeightLbs() - fuelTime.fuelLbsToToc;
            float remainingFuelTocGal = userAircraft->getFuelTotalQuantityGallons() - fuelTime.fuelGalToToc;

            html.id(pid::TOC_FUEL).row2(tr("Fuel:"), Unit::fuelLbsAndGal(remainingFuelTocLbs, remainingFuelTocGal), ahtml::NO_ENTITIES);
          }
        } // if(distanceToToc > 0 && distanceToToc < map::INVALID_DISTANCE_VALUE)

        if(route.getTopOfClimbDistance() < map::INVALID_DISTANCE_VALUE)
          html.id(pid::TOC_FROM_DESTINATION).row2(tr("From Departure:"), Unit::distNm(route.getTopOfClimbDistance()));

        html.tableEndIf();
      } // if(route.getSizeWithoutAlternates() > 1 && !alternate) // No TOC display when flying an alternate leg

      // ================================================================================
      // Next waypoint ========================================================================
      QString wpText;
      if(alternate)
        wpText = tr(" - Alternate");
      else if(destination)
        wpText = tr(" - Destination");
      else if(activeLegIdxCorrected != map::INVALID_INDEX_VALUE)
      {
        const RouteLeg& routeLeg = route.value(activeLegIdxCorrected);

        if(routeLeg.getProcedureLeg().isApproach())
          wpText = tr(" - Approach");
        else if(routeLeg.getProcedureLeg().isTransition())
          wpText = tr(" - Transition");
        else if(routeLeg.getProcedureLeg().isMissed())
          wpText = tr(" - Missed Approach");
      }

      // Next leg - waypoint data ====================================================
      // If approaching an initial fix use corrected version
      const RouteLeg& routeLegCorrected = route.value(activeLegIdxCorrected);
      const RouteAltitudeLeg& routeAltLegCorrected = route.getAltitudeLegAt(activeLegIdxCorrected);
      QString nextName;
      if(activeLegIdxCorrected != map::INVALID_INDEX_VALUE)
      {
        if(!routeLegCorrected.getDisplayIdent().isEmpty())
          nextName = routeLegCorrected.getDisplayIdent() %
                     (routeLegCorrected.getMapTypeName().isEmpty() ? QString() : tr(", ") %
                      routeLegCorrected.getMapTypeName());
      }

      // From or to fix depending on leg type
      QString fromTo;
      if(!routeLegCorrected.getDisplayIdent().isEmpty())
      {
        fromTo = tr(" - to ");
        if(routeLegCorrected.isAnyProcedure() && proc::procedureLegFrom(routeLegCorrected.getProcedureLegType()))
          fromTo = tr(" - from ");
      }
      else
        fromTo = tr(" - ");

      html.mark();
      head(html, tr("Next Waypoint%1%2%3").arg(wpText).arg(fromTo).arg(nextName));
      html.table();

      if(activeLegIdxCorrected != map::INVALID_INDEX_VALUE)
      {
        // If approaching an initial fix use corrected version

        // For course and distance use not corrected leg
        int routeLegIndex = activeLegIdx != map::INVALID_INDEX_VALUE && corrected ? activeLegIdx : activeLegIdxCorrected;
        const RouteLeg& routeLeg = route.value(routeLegIndex);
        const RouteLeg& lastRouteLeg = route.value(routeLegIndex - 1);

        const proc::MapProcedureLeg& procLeg = routeLegCorrected.getProcedureLeg();

        // Next leg - approach data ====================================================
        if(routeLegCorrected.isAnyProcedure())
        {
          html.id(pid::NEXT_LEG_TYPE).row2(tr("Leg Type:"), proc::procedureLegTypeStr(procLeg.type));

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
            html.id(pid::NEXT_INSTRUCTIONS).row2(tr("Instructions:"), instructions.join(", "));
        }

        // Next leg - approach related navaid ====================================================
        html.id(pid::NEXT_RELATED).row2If(tr("Related Navaid:"), proc::procedureLegRecommended(procLeg).join(tr(", ")), ahtml::NO_ENTITIES);

        // Altitude restrictions for procedure legs ==============================================
        if(routeLegCorrected.isAnyProcedure())
        {
          QStringList restrictions;

          if(procLeg.altRestriction.isValid())
            restrictions.append(proc::altRestrictionText(procLeg.altRestriction));
          if(procLeg.speedRestriction.isValid())
            restrictions.append(proc::speedRestrictionText(procLeg.speedRestriction));

          if(restrictions.size() > 1)
            html.id(pid::NEXT_RESTRICTION).row2(tr("Restrictions:"), restrictions.join(tr(", ")));
          else if(restrictions.size() == 1)
            html.id(pid::NEXT_RESTRICTION).row2(tr("Restriction:"), restrictions.constFirst());
        }

        // Distance, time and ETA for next waypoint ==============================================
        float courseToWptTrue = map::INVALID_COURSE_VALUE;
        if(nextLegDistance < map::INVALID_DISTANCE_VALUE)
        {
          QStringList distTimeStr, distTimeHeader;

          // Distance
          distTimeStr.append(Unit::distNm(nextLegDistance));
          distTimeHeader.append(tr("Distance"));

          if(aircraft.getGroundSpeedKts() > MIN_GROUND_SPEED &&
             aircraft.getGroundSpeedKts() < atools::fs::sc::SC_INVALID_FLOAT && fuelTime.isTimeToNextValid())
          {
            // Travel time
            distTimeStr.append(formatter::formatMinutesHoursLong(fuelTime.timeToNext));
            distTimeHeader.append(tr("Time"));

            // ETA
            QDateTime arrivalNext = userAircraft->getZuluTime().addSecs(static_cast<int>(fuelTime.timeToNext * 3600.f));
            distTimeStr.append(locale.toString(arrivalNext.time(), QLocale::ShortFormat) % tr(" ") %
                               arrivalNext.timeZoneAbbreviation());
            distTimeHeader.append(tr("Arrival"));
          }

          if(!alternate && !destination)
            // Distance, time and arrival are already part of the
            // destination information when flying an alternate leg
            html.id(pid::NEXT_DIST_TIME_ARR).row2(strJoinHdr(distTimeHeader), strJoinVal(distTimeStr), ahtml::NO_ENTITIES);

          // Next leg - altitude ====================================================
          float altitude = map::INVALID_ALTITUDE_VALUE;
          if(NavApp::getAltitudeLegs().isValidProfile())
          {
            const RouteAltitudeLeg& routeAltLeg = activeLegIdx != map::INVALID_INDEX_VALUE &&
                                                  corrected ? route.getAltitudeLegAt(activeLegIdx) : routeAltLegCorrected;

            if(!routeAltLeg.isEmpty())
              // Get calculated altitude
              altitude = routeAltLeg.getWaypointAltitude();
          }
          else if(route.getSizeWithoutAlternates() == 1)
            // Get altitude for single point plan
            altitude = route.getFirstLeg().getAltitude();

          if(altitude < map::INVALID_ALTITUDE_VALUE)
            html.id(pid::NEXT_ALTITUDE).row2(tr("Altitude:"), Unit::altFeet(altitude));

          if(!alternate && !destination)
          {
            // Fuel
            if(fuelTime.isFuelToNextValid())
            {
              // Calculate remaining fuel at next
              float remainingFuelNextLbs = userAircraft->getFuelTotalWeightLbs() - fuelTime.fuelLbsToNext;
              float remainingFuelNextGal = userAircraft->getFuelTotalQuantityGallons() - fuelTime.fuelGalToNext;

              html.id(pid::NEXT_FUEL).row2(tr("Fuel:"), Unit::fuelLbsAndGal(remainingFuelNextLbs, remainingFuelNextGal),
                                           ahtml::NO_ENTITIES);
            }
          }

          // Course Not for arc legs
          if((routeLeg.isRoute() || !procLeg.isCircular()) && routeLeg.getPosition().isValid())
          {
            courseToWptTrue = aircraft.getPosition().angleDegTo(routeLeg.getPosition());

            html.id(pid::NEXT_COURSE_TO_WP).row2(tr("Course to waypoint:"),
                                                 courseTextFromTrue(courseToWptTrue, routeLeg.getMagvarEnd(), true /* magBold */),
                                                 ahtml::NO_ENTITIES);

            int lastDepartureLegIdx = route.getLastIndexOfDepartureProcedure();
            int lastRouteLegIdx = route.getDestinationIndexBeforeProcedure();

            if((routeLegIndex > lastDepartureLegIdx || routeLegIndex <= lastRouteLegIdx) &&
               (lastRouteLeg.isRoute() || routeLeg.isRoute()))
            {
              if(lastRouteLeg.isCalibratedVor())
              {
                // Last leg is FROM VOR and en-route - show course from calibrated VOR
                float outboundCourseMag, dummy;
                route.getOutboundCourse(routeLegIndex, outboundCourseMag, dummy);
                html.id(pid::NEXT_COURSE_FROM_VOR).
                row2(tr("Leg course from %1 %2:").arg(map::vorType(lastRouteLeg.getVor())).arg(lastRouteLeg.getIdent()),
                     courseText(outboundCourseMag, map::INVALID_COURSE_VALUE), ahtml::NO_ENTITIES);
              }

              if(routeLeg.isCalibratedVor())
              {
                // This leg is TO VOR and this or last leg is en-route - show course to calibrated VOR
                float inboundCourseMag, dummy;
                route.getInboundCourse(routeLegIndex, inboundCourseMag, dummy);
                html.id(pid::NEXT_COURSE_TO_VOR).
                row2(tr("Leg course to %1 %2:").arg(map::vorType(routeLeg.getVor())).arg(routeLeg.getIdent()),
                     courseText(inboundCourseMag, map::INVALID_COURSE_VALUE), ahtml::NO_ENTITIES);
              }
            }
          }
        } // if(nearestLegDistance < map::INVALID_DISTANCE_VALUE)

        // No cross track and course for holds
        if(route.getSizeWithoutAlternates() > 1)
        {
          // No course for arcs
          if(routeLeg.isRoute() || !routeLeg.getProcedureLeg().isCircular())
          {
            float courseMag = map::INVALID_COURSE_VALUE, courseTrue = map::INVALID_COURSE_VALUE;
            bool procedure = false;
            routeLeg.getMagTrueRealCourse(courseMag, courseTrue, &procedure);

            // TODO avoid breaking translation temporarily
            QString text = procedure ? tr("Leg Course") + tr(":") : tr("Leg Start Course:");
            html.id(pid::NEXT_LEG_COURSE).row2If(text, courseText(courseMag, courseTrue, true /* magBold */), ahtml::NO_ENTITIES);

            if(userAircraft != nullptr && userAircraft->isFlying() && courseToWptTrue < INVALID_COURSE_VALUE)
            {
              // Crab angle is the amount of correction an aircraft must be turned into the wind in order to maintain the desired course.
              float headingTrue = ageo::windCorrectedHeading(userAircraft->getWindSpeedKts(), userAircraft->getWindDirectionDegT(),
                                                             courseToWptTrue, userAircraft->getTrueAirspeedKts());
              if(headingTrue < INVALID_COURSE_VALUE)
                html.id(pid::NEXT_HEADING).row2(tr("Heading:", "heading to next"),
                                                courseTextFromTrue(headingTrue, userAircraft->getMagVarDeg()), ahtml::NO_ENTITIES);
            }
          }

          if(!routeLeg.getProcedureLeg().isHold())
          {
            if(crossTrackDistance < map::INVALID_DISTANCE_VALUE)
            {
              // Positive means right of course,  negative means left of course.
              QString crossDirection;
              if(crossTrackDistance >= 0.1f)
                crossDirection = tr("<b>◄</b>");
              else if(crossTrackDistance <= -0.1f)
                crossDirection = tr("<b>►</b>");

              html.id(pid::NEXT_CROSS_TRACK_DIST).row2(tr("Cross Track Distance:"),
                                                       tr("%1 %2").arg(Unit::distNm(std::abs(crossTrackDistance))).
                                                       arg(crossDirection), ahtml::NO_ENTITIES);
            }
            else
              html.id(pid::NEXT_CROSS_TRACK_DIST).row2(tr("Cross Track Distance:"), tr("Not along Track"));
          }
        } // if(route.getSizeWithoutAlternates() > 1)
      } // if(routeLegCorrected != nullptr)
      else
        qWarning() << "Invalid route leg index" << activeLegIdxCorrected;

      html.tableEndIf();

      if(!routeLegCorrected.getComment().isEmpty() && html.isIdSet(pid::NEXT_REMARKS))
      {
        html.p().b(tr("Waypoint Remarks")).pEnd();
        html.table(1, 2, 0, 0, html.getRowBackColor());
        html.tr().td().text(atools::elideTextLinesShort(routeLegCorrected.getComment(), 3, 100,
                                                        true /* compressEmpty */, false /* ellipseLastLine */),
                            ahtml::AUTOLINK | ahtml::SMALL).tdEnd().trEnd();
        html.tableEnd();
      }

    } // if(activeLegCorrected != map::INVALID_INDEX_VALUE && ...
    else
    {
      if(route.isTooFarToFlightPlan() && NavApp::isConnectedAndAircraftFlying())
        html.warning(tr("No Active Flight Plan Leg. Too far from flight plan."));
      else
        html.b(tr("No Active Flight Plan Leg."));
      html.tableIf();
      dateTimeAndFlown(userAircraft, html);
      html.tableEndIf();
    }
  } // if(!route.isEmpty() && userAircraft != nullptr && info)
  else if(info && userAircraft != nullptr)
  {
    html.warning(tr("No Flight Plan."));
    html.tableIf();
    dateTimeAndFlown(userAircraft, html);
    html.tableEndIf();
  }

  // Add departure and destination for AI ==================================================
  if(userAircraft == nullptr && (!aircraft.getFromIdent().isEmpty() || !aircraft.getToIdent().isEmpty()))
  {
    if(info && userAircraft != nullptr)
      head(html, tr("Flight Plan"));

    html.table();
    html.row2(tr("Departure:"), airportLink(html, aircraft.getFromIdent(), QString(), ageo::EMPTY_POS), ahtml::NO_ENTITIES);
    html.row2(tr("Destination:"), airportLink(html, aircraft.getToIdent(), QString(), ageo::EMPTY_POS), ahtml::NO_ENTITIES);
    html.tableEnd();
    html.br();
  }

  // ================================================================================
  // Aircraft =========================================================================
  html.mark();
  if(info && userAircraft != nullptr)
    head(html, tr("Aircraft"));
  html.table();

  float heading = atools::fs::sc::SC_INVALID_FLOAT;
  if(aircraft.getHeadingDegMag() < atools::fs::sc::SC_INVALID_FLOAT)
    heading = aircraft.getHeadingDegMag();
  else if(aircraft.getHeadingDegTrue() < atools::fs::sc::SC_INVALID_FLOAT)
    heading = normalizeCourse(aircraft.getHeadingDegTrue() - NavApp::getMagVar(aircraft.getPosition()));

  if(heading < atools::fs::sc::SC_INVALID_FLOAT)
    html.id(pid::AIRCRAFT_HEADING).row2(tr("Heading:", "aircraft heading"),
                                        courseText(heading, aircraft.getHeadingDegTrue(),
                                                   true /* magBold */, true /* magBig */, false /* trueSmall */), ahtml::NO_ENTITIES);

  if(userAircraft != nullptr && info)
  {
    if(userAircraft != nullptr)
      html.id(pid::AIRCRAFT_TRACK).row2(tr("Track:"),
                                        courseText(userAircraft->getTrackDegMag(), userAircraft->getTrackDegTrue()), ahtml::NO_ENTITIES);

    html.id(pid::AIRCRAFT_FUEL_FLOW).row2(tr("Fuel Flow:"),
                                          Unit::ffLbsAndGal(userAircraft->getFuelFlowPPH(), userAircraft->getFuelFlowGPH()));

    html.id(pid::AIRCRAFT_FUEL).row2(tr("Fuel:"),
                                     Unit::fuelLbsAndGalLocalOther(userAircraft->getFuelTotalWeightLbs(),
                                                                   userAircraft->getFuelTotalQuantityGallons()), ahtml::NO_ENTITIES);
    html.id(pid::AIRCRAFT_GROSS_WEIGHT).row2(tr("Gross Weight:"),
                                             Unit::weightLbsLocalOther(userAircraft->getAirplaneTotalWeightLbs()), ahtml::NO_ENTITIES);

    if(userAircraft->isFlying())
    {
      float hoursRemaining = 0.f, distanceRemaining = 0.f;
      perfController->getEnduranceAverage(hoursRemaining, distanceRemaining);

#ifdef DEBUG_INFORMATION
      qDebug() << Q_FUNC_INFO << "hoursRemaining" << hoursRemaining;
      qDebug() << Q_FUNC_INFO << "distanceRemaining" << distanceRemaining;
#endif

      if(hoursRemaining < map::INVALID_TIME_VALUE && distanceRemaining < map::INVALID_DISTANCE_VALUE)
      {
        QString text = formatter::formatMinutesHoursLong(hoursRemaining) % tr(", ") % Unit::distNm(distanceRemaining);

        // Show error colors only for free flight
        if(route.getSizeWithoutAlternates() <= 1 || route.getActiveLegIndexCorrected() == map::INVALID_INDEX_VALUE)
        {
          if(hoursRemaining < 0.5)
            html.id(pid::AIRCRAFT_ENDURANCE).row2Error(tr("Endurance (critical):"), text);
          else if(hoursRemaining < 0.75)
            html.id(pid::AIRCRAFT_ENDURANCE).row2Warning(tr("Endurance (low):"), text);
          else
            html.id(pid::AIRCRAFT_ENDURANCE).row2(tr("Endurance:"), text);
        }
        else
          html.id(pid::AIRCRAFT_ENDURANCE).row2(tr("Endurance:"), text);
      }
    }

    // ================================================================================
    // Ice ===============================================
    QStringList ice = map::aircraftIcing(*userAircraft, false /* narrow */);
    if(!ice.isEmpty())
      html.id(pid::AIRCRAFT_ICE).row2Error(tr("Ice:"), atools::strJoin(ice, tr(", "), tr(", "), tr("&nbsp;%")));
  } // if(userAircraft != nullptr && info)
  html.tableEndIf();

  // Display more text for information display if not online aircraft
  bool longDisplay = info && !aircraft.isOnline();

  // ================================================================================
  // Altitude =========================================================================
  if(aircraft.getCategory() != atools::fs::sc::CARRIER && aircraft.getCategory() != atools::fs::sc::FRIGATE)
  {
    html.mark();
    if(longDisplay)
      head(html, tr("Altitude"));
    html.table();

    if(!aircraft.isAnyBoat())
    {
      if(longDisplay && aircraft.getIndicatedAltitudeFt() < atools::fs::sc::SC_INVALID_FLOAT)
      {
        // Check for display of alternate units
        QString otherInd;
        if(html.isIdSet(pid::ALT_INDICATED_OTHER))
          otherInd = tr(", %1").arg(Unit::altFeetOther(aircraft.getIndicatedAltitudeFt()));

        html.id(pid::ALT_INDICATED).row2(tr("Indicated:"),
                                         highlightText(Unit::altFeet(aircraft.getIndicatedAltitudeFt())) % otherInd, ahtml::NO_ENTITIES);
      }
    }

    float speedActKts = aircraft.getActualAltitudeFt();
    if(speedActKts < atools::fs::sc::SC_INVALID_FLOAT)
    {
      // Check for display of alternate units
      QString otherAct;
      if(html.isIdSet(pid::ALT_ACTUAL_OTHER))
        otherAct = tr(", <small>%1</small>").arg(Unit::altFeetOther(speedActKts));

      html.id(pid::ALT_ACTUAL).row2(longDisplay ? tr("Actual:") : tr("Altitude:"), Unit::altFeet(speedActKts) % otherAct,
                                    ahtml::NO_ENTITIES);
    }

    if(userAircraft != nullptr && longDisplay && !aircraft.isAnyBoat())
    {
      if(userAircraft->getAltitudeAboveGroundFt() < atools::fs::sc::SC_INVALID_FLOAT)
        html.id(pid::ALT_ABOVE_GROUND).row2(tr("Above Ground:"), Unit::altFeet(userAircraft->getAltitudeAboveGroundFt()));

      if(userAircraft->getGroundAltitudeFt() < atools::fs::sc::SC_INVALID_FLOAT)
        html.id(pid::ALT_GROUND_ELEVATION).row2(tr("Ground Elevation:"), Unit::altFeet(userAircraft->getGroundAltitudeFt()));

      if(userAircraft->getAltitudeAutopilotFt() < atools::fs::sc::SC_INVALID_FLOAT)
        html.id(pid::ALT_AUTOPILOT_ALT).row2(tr("Autopilot Selected:"), Unit::altFeet(userAircraft->getAltitudeAutopilotFt()));
    }
    html.tableEndIf();

    // ================================================================================
    // Speed =========================================================================
    if(aircraft.getIndicatedSpeedKts() < atools::fs::sc::SC_INVALID_FLOAT ||
       aircraft.getGroundSpeedKts() < atools::fs::sc::SC_INVALID_FLOAT ||
       aircraft.getTrueAirspeedKts() < atools::fs::sc::SC_INVALID_FLOAT ||
       aircraft.getVerticalSpeedFeetPerMin() < atools::fs::sc::SC_INVALID_FLOAT)
    {
      html.mark();
      if(longDisplay)
        head(html, tr("Speed"));
      html.table();
      float speedIndKts = aircraft.getIndicatedSpeedKts();
      if(longDisplay && !aircraft.isAnyBoat() && speedIndKts < atools::fs::sc::SC_INVALID_FLOAT)
      {
        QString warn(tr("Indicated (speed limit):")), normal(tr("Indicated:"));

        QString otherInd;
        if(html.isIdSet(pid::SPEED_INDICATED_OTHER))
          otherInd = tr(", %1").arg(Unit::speedKtsOther(speedIndKts).join(tr(", ")));

        html.id(pid::SPEED_INDICATED);
        if(aircraft.getIndicatedAltitudeFt() < 10000.f && speedIndKts > 260.f)
          // Exceeding speed limit - show red message
          html.row2(warn, HtmlBuilder::errorMessage(Unit::speedKts(speedIndKts), HtmlBuilder::MSG_FLAGS | ahtml::BIG) % otherInd,
                    ahtml::NO_ENTITIES);
        else
        {
          if(Unit::getUnitSpeed() == opts::SPEED_KTS)
          {
            // Use int value to compare and display to avoid confusing warning display for 250 on rounding errors
            int speedIndKtsInt = static_cast<int>(speedIndKts);
            if(aircraft.getIndicatedAltitudeFt() < 10000.f && speedIndKtsInt > 250)
              // Exceeding speed limit slightly - orange warning
              html.row2(warn, HtmlBuilder::warningMessage(Unit::speedKts(speedIndKtsInt), HtmlBuilder::MSG_FLAGS | ahtml::BIG) % otherInd,
                        ahtml::NO_ENTITIES);
            else
              html.row2(normal, highlightText(Unit::speedKts(speedIndKtsInt)) % otherInd, ahtml::NO_ENTITIES);
          }
          else
          {
            if(aircraft.getIndicatedAltitudeFt() < 10000.f && speedIndKts > 251.f)
              // Exceeding speed limit slightly - orange warning
              html.row2(warn, HtmlBuilder::warningMessage(Unit::speedKts(speedIndKts), HtmlBuilder::MSG_FLAGS | ahtml::BIG) % otherInd,
                        ahtml::NO_ENTITIES);
            else
              html.row2(normal, highlightText(Unit::speedKts(speedIndKts)) % otherInd, ahtml::NO_ENTITIES);
          }
        }
      }

      if(aircraft.getGroundSpeedKts() < atools::fs::sc::SC_INVALID_FLOAT)
      {
        QString otherGnd;
        if(html.isIdSet(pid::SPEED_GROUND_OTHER))
          otherGnd = tr(", %1").arg(Unit::speedKtsOther(aircraft.getGroundSpeedKts()).join(tr(", ")));

        html.id(pid::SPEED_GROUND).row2(longDisplay ? tr("Ground:") : tr("Groundspeed:"),
                                        Unit::speedKts(aircraft.getGroundSpeedKts()) % otherGnd);
      }

      if(longDisplay && !aircraft.isAnyBoat())
        if(aircraft.getTrueAirspeedKts() < atools::fs::sc::SC_INVALID_FLOAT)
          html.id(pid::SPEED_TRUE).row2(tr("True Airspeed:"), Unit::speedKts(aircraft.getTrueAirspeedKts()));

      if(!aircraft.isAnyBoat())
      {
        if(longDisplay && aircraft.getMachSpeed() < atools::fs::sc::SC_INVALID_FLOAT)
        {
          float mach = aircraft.getMachSpeed();
          if(mach > 0.4f)
            html.id(pid::SPEED_MACH).row2(tr("Mach:"), locale.toString(mach, 'f', 3));
          else
            html.id(pid::SPEED_MACH).row2(tr("Mach:"), tr("-"));
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

          QString otherVert;
          if(html.isIdSet(pid::SPEED_VERTICAL_OTHER))
            otherVert = tr(", %1").arg(Unit::speedVertFpmOther(vspeed));

          html.id(pid::SPEED_VERTICAL).row2(longDisplay ? tr("Vertical:") : tr("Vertical Speed:"),
                                            Unit::speedVertFpm(vspeed) % otherVert % upDown, ahtml::NO_ENTITIES);
        }
      }
      html.tableEndIf();
    }

    // ================================================================================
    // Vertical Flight Path =========================================================================
    if(distanceToTod <= 0.f && userAircraft != nullptr && userAircraft->isFlying())
    {
      html.mark();
      if(longDisplay)
        head(html, tr("Descent Path"));
      html.table();

      QString descentDeviation, verticalAngle, verticalAngleNext;
      bool verticalRequired;
      route.getVerticalPathDeviationTexts(&descentDeviation, &verticalAngle, &verticalRequired, &verticalAngleNext);

      if(html.isIdSet(pid::DESCENT_DEVIATION))
        html.row2If(tr("Deviation:"), descentDeviation, ahtml::NO_ENTITIES);

      if(html.isIdSet(pid::DESCENT_ANGLE_SPEED))
        html.row2If(verticalRequired ? tr("Required Angle and Speed:") : tr("Angle and Speed:"), verticalAngle);

      if(html.isIdSet(pid::DESCENT_VERT_ANGLE_NEXT))
      {
        float vertAngleToNext = route.getVerticalAngleToNext(nextLegDistance);
        if(vertAngleToNext < map::INVALID_ANGLE_VALUE)
          html.row2(tr("Angle and Speed to Next:"), tr("%L1°, %L2").arg(vertAngleToNext, 0, 'g', 2).
                    arg(Unit::speedVertFpm(-ageo::descentSpeedForPathAngle(userAircraft->getGroundSpeedKts(), vertAngleToNext)) %
                        tr(" ▼")));
      }
      html.tableEndIf();
    }
  }

  // ================================================================================
  // Environment =========================================================================
  if(userAircraft != nullptr && longDisplay)
  {
    html.mark();
    head(html, tr("Environment"));
    html.table();
    float windSpeed = userAircraft->getWindSpeedKts();
    float windDir = normalizeCourse(userAircraft->getWindDirectionDegT() - userAircraft->getMagVarDeg());
    if(windSpeed >= 1.f)
    {
      QString windTxt = courseText(windDir, userAircraft->getWindDirectionDegT(),
                                   true /* magBold */, true /* magBig */, false /* trueSmall */) %
                        tr(", ") % highlightText(Unit::speedKts(windSpeed));
      html.id(pid::ENV_WIND_DIR_SPEED).row2(tr("Wind Direction and Speed:"), windTxt, ahtml::NO_ENTITIES);
    }
    else
      html.id(pid::ENV_WIND_DIR_SPEED).row2(tr("Wind Direction and Speed:"), tr("None"));

    // Head/tail and crosswind =================================================
    float headWind = 0.f, crossWind = 0.f;
    ageo::windForCourse(headWind, crossWind, windSpeed, windDir, userAircraft->getHeadingDegMag());

    QString windPtr = formatter::windInformation(headWind, crossWind, tr(", "));

    // Keep an empty line to avoid flickering
    html.id(pid::ENV_WIND_DIR_SPEED).row2(QString(), windPtr, ahtml::NO_ENTITIES);

    // Total air temperature (TAT) is also called: indicated air temperature (IAT) or ram air temperature (RAT)
    float tat = userAircraft->getTotalAirTemperatureCelsius();
    if(tat < 0.f && tat > -0.5f)
      tat = 0.f;
    html.id(pid::ENV_TAT).row2(tr("Total Air Temperature:"), locale.toString(tat, 'f', 0) % tr(" °C, ") %
                               locale.toString(ageo::degCToDegF(tat), 'f', 0) % tr("°F"));

    // Static air temperature (SAT) is also called: outside air temperature (OAT) or true air temperature
    float sat = userAircraft->getAmbientTemperatureCelsius();
    if(sat < 0.f && sat > -0.5f)
      sat = 0.f;
    html.id(pid::ENV_SAT).row2(tr("Static Air Temperature:"), locale.toString(sat, 'f', 0) % tr(" °C, ") %
                               locale.toString(ageo::degCToDegF(sat), 'f', 0) % tr("°F"));

    float isaDeviation = sat - ageo::isaTemperature(userAircraft->getActualAltitudeFt());
    if(isaDeviation < 0.f && isaDeviation > -0.5f)
      isaDeviation = 0.f;
    html.id(pid::ENV_ISA_DEV).row2(tr("ISA Deviation:"), locale.toString(isaDeviation, 'f', 0) % tr(" °C"));

    float seaLevelPressureMbar = userAircraft->getSeaLevelPressureMbar();
    QString pressureTxt = highlightText(locale.toString(seaLevelPressureMbar, 'f', 0) % tr(" hPa, ") %
                                        locale.toString(ageo::mbarToInHg(seaLevelPressureMbar), 'f', 2) % tr(" inHg"));
    html.id(pid::ENV_SEA_LEVEL_PRESS).row2(tr("Sea Level Pressure:"), pressureTxt, ahtml::NO_ENTITIES);

    // Not in air
    if(userAircraft->isOnGround())
      html.id(pid::ENV_DENSITY_ALTITUDE).
      row2(tr("Density Altitude:"), Unit::altFeet(ageo::densityAltitudeFt(sat, aircraft.getActualAltitudeFt(), seaLevelPressureMbar)));

    QStringList precip;
    // if(data.getFlags() & atools::fs::sc::IN_CLOUD) // too unreliable
    // precip.append(tr("Cloud"));

    if(NavApp::isXpConnect())
    {
      if(userAircraft->inRain())
      {
        // X-Plane does not deliver snow flag - show snow if temperature is below zero
        if(userAircraft->getAmbientTemperatureCelsius() < 0.f)
          precip.append(tr("Snow"));
        else if(userAircraft->getAmbientTemperatureCelsius() < 5.f)
          precip.append(tr("Rain or Snow"));
        else
          precip.append(tr("Rain"));
      }
    }
    else
    {
      if(userAircraft->inRain())
        precip.append(tr("Rain"));

      if(userAircraft->inSnow())
        precip.append(tr("Snow"));
    }

    precip.removeDuplicates();

    if(!precip.isEmpty())
      html.id(pid::ENV_CONDITIONS).row2(tr("Conditions:"), precip.join(tr(", ")));

    if(Unit::distMeterF(userAircraft->getAmbientVisibilityMeter()) > 20.f)
      html.id(pid::ENV_VISIBILITY).row2(tr("Visibility:"), tr("> 20 ") % Unit::getUnitDistStr());
    else
      html.id(pid::ENV_VISIBILITY).row2(tr("Visibility:"), Unit::distMeter(userAircraft->getAmbientVisibilityMeter(), true /*unit*/, 5));

    html.tableEndIf();
  }

  if(longDisplay && html.isIdSet(pid::POS_COORDINATES))
  {
    head(html, tr("Position"));
    addCoordinates(aircraft.getPosition(), html);
    html.tableEnd();
  }
  html.row2AlignRight(false);
  html.clearId();

#ifdef DEBUG_INFORMATION_INFO
  if(info)
  {
    html.hr().pre(
      "distanceToFlightPlan " + QString::number(route.getDistanceToFlightPlan(), 'f', 2) +
      " distFromStartNm " + QString::number(distFromStartNm, 'f', 2) +
      " distToDestNm " + QString::number(distToDestNm, 'f', 1) +
      "\nnearestLegDistance " + QString::number(nearestLegDistance, 'f', 2) +
      " crossTrackDistance " + QString::number(crossTrackDistance, 'f', 2) +
      "\nfuelToDestinationLbs " + QString::number(fuelTime.fuelLbsToDest, 'f', 2) +
      " fuelToDestinationGal " + QString::number(fuelTime.fuelGalToDest, 'f', 2) +
      (distanceToTod > 0.f ?
       ("\ndistanceToTod " + QString::number(distanceToTod, 'f', 2) +
        " fuelToTodLbs " + QString::number(fuelTime.fuelLbsToTod, 'f', 2) +
        " fuelToTodGal " + QString::number(fuelTime.fuelGalToTod, 'f', 2)) : QString()) +
      (nearestLegDistance > 0.f ?
       ("\ndistanceToNext " + QString::number(nearestLegDistance, 'f', 2) +
        " fuelToNextLbs " + QString::number(fuelTime.fuelLbsToNext, 'f', 2) +
        " fuelToNextGal " + QString::number(fuelTime.fuelGalToNext, 'f', 2)) : QString())
      );

    html.pre();
    html.textBr("TimeToDestValid " + QString::number(fuelTime.isTimeToDestValid()));
    html.textBr("TimeToTodValid " + QString::number(fuelTime.isTimeToTodValid()));
    html.textBr("TimeToNextValid " + QString::number(fuelTime.isTimeToNextValid()));
    html.textBr("FuelToDestValid " + QString::number(fuelTime.isFuelToDestValid()));
    html.textBr("FuelToTodValid " + QString::number(fuelTime.isFuelToTodValid()));
    html.textBr("FuelToNextValid " + QString::number(fuelTime.isFuelToNextValid()));
    html.textBr("EstimatedFuel " + QString::number(fuelTime.estimatedFuel));
    html.textBr("EstimatedTime " + QString::number(fuelTime.estimatedTime));
    html.textBr("OnlineShadow " + QString::number(aircraft.isOnlineShadow()));
    html.textBr("Online " + QString::number(aircraft.isOnline()));
    html.textBr("User " + QString::number(aircraft.isUser()));
    html.preEnd();
  }
#endif
}

QString HtmlInfoBuilder::airportLink(const HtmlBuilder& html, const QString& ident, const QString& name,
                                     const atools::geo::Pos& pos) const
{
  HtmlBuilder builder = html.cleared();
  if(!ident.isEmpty())
  {
    if(info)
    {
      QString link = QString("lnm://show?airport=%1").arg(ident);
      if(pos.isValid())
        link += QString("&aplonx=%1&aplaty=%2").arg(pos.getLonX()).arg(pos.getLatY());

      if(name.isEmpty())
        // Ident only
        builder.a(ident, link, ahtml::LINK_NO_UL);
      else
      {
        // Name and ident
        builder.text(tr("%1 (").arg(name));
        builder.a(ident, link, ahtml::LINK_NO_UL);
        builder.text(tr(")"));
      }
    }
    else
      builder.text(name.isEmpty() ? ident : tr("%1 (%2)").arg(name).arg(ident));
  }
  return builder.getHtml();
}

void HtmlInfoBuilder::aircraftTitle(const atools::fs::sc::SimConnectAircraft& aircraft, HtmlBuilder& html)
{
  QIcon icon = NavApp::getVehicleIcons()->iconFromCache(aircraft, symbolSizeVehicle.height(), 45);

  html.tableAtts({
    {"width", "100%"}
  });
  html.tr();
  html.td();
  if(aircraft.isUser() && aircraft.isOnlineShadow())
    html.img(icon, tr("User Vehicle / Online Client (%1)").arg(NavApp::getOnlineNetworkTranslated()),
             QString(), symbolSizeVehicle);
  else if(aircraft.isUser())
    html.img(icon, tr("User Vehicle"), QString(), symbolSizeVehicle);
  else if(aircraft.isOnlineShadow())
    html.img(icon, tr("AI / Multiplayer / Online Client (%1)").arg(NavApp::getOnlineNetworkTranslated()),
             QString(), symbolSizeVehicle);
  else if(aircraft.isOnline())
    html.img(icon, tr("Online Client (%1)").arg(NavApp::getOnlineNetworkTranslated()), QString(), symbolSizeVehicle);
  else
    html.img(icon, tr("AI / Multiplayer Vehicle"), QString(), symbolSizeVehicle);
  html.nbsp().nbsp();

  QString title(aircraft.getAirplaneRegistration());
  QString title2 = map::aircraftType(aircraft);
  QString title3 = aircraft.isOnline() ? NavApp::getOnlineNetworkTranslated() : QString();

  if(!aircraft.getAirplaneModel().isEmpty())
    title2 += (title2.isEmpty() ? QString() : tr(", ")) % aircraft.getAirplaneModel();

  if(!title2.isEmpty())
    title += tr(" - %1").arg(title2);

  if(!title3.isEmpty())
    title += tr(" (%1)").arg(title3);

#ifdef DEBUG_INFORMATION
  static bool heartbeat = false;

  title += (heartbeat ? " X" : " 0");
  heartbeat = !heartbeat;

#endif

  html.text(title, ahtml::BOLD | ahtml::BIG);

  if(info && !print)
  {
    if(aircraft.isValid())
    {
      html.nbsp().nbsp();
      html.a(tr("Map"), QString("lnm://show?lonx=%1&laty=%2").
             arg(aircraft.getPosition().getLonX()).arg(aircraft.getPosition().getLatY()),
             ahtml::BOLD | ahtml::LINK_NO_UL);
    }
  }
  html.tdEnd();

  html.trEnd();
  html.tableEnd();
}

void HtmlInfoBuilder::addScenery(const atools::sql::SqlRecord *rec, HtmlBuilder& html, SceneryType type) const
{
  head(html, tr("Data Source"));
  html.table();

  switch(type)
  {
    case DATASOURCE_COM:
      // COM is used from sim except in Navigraph all - no file given
      html.row2(NavApp::isNavdataAll() ? tr("Navigraph") : tr("Simulator"));
      break;

    case DATASOURCE_HOLD:
    case DATASOURCE_MSA:
    case DATASOURCE_NAV:
      if(rec != nullptr)
        html.row2(rec->valueStr("title", QString()), filepathTextShow(rec->valueStr("filepath", QString())), ahtml::NO_ENTITIES);
      else
        html.row2(!NavApp::isNavdataOff() ? tr("Navigraph") : tr("Simulator"));
      break;
  }

  html.tableEnd();
}

void HtmlInfoBuilder::addAirportFolder(const MapAirport& airport, HtmlBuilder& html) const
{
  const QFileInfoList airportFiles = AirportFiles::getAirportFiles(airport.displayIdent());

  if(!airportFiles.isEmpty())
  {
    head(html, tr("Files"));
    html.table();

    for(const QString& dir : AirportFiles::getAirportFilesBase(airport.displayIdent()))
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
  const atools::sql::SqlRecordList *sceneryInfo = infoQuery->getAirportSceneryInformation(airport.ident);

  if(sceneryInfo != nullptr)
  {
    head(html, sceneryInfo->size() > 1 ? tr("Data Sources") : tr("Data Source"));
    html.table();

    int i = 0;
    for(const SqlRecord& rec : *sceneryInfo)
    {
      QString title = rec.valueStr("title");
      if(airport.xplane)
        title = i == 0 ? tr("X-Plane") : QString();

      html.row2(title, filepathTextShow(rec.valueStr("filepath"), QString(), true /* canonical */), ahtml::NO_ENTITIES);
      i++;
    }
    html.tableEnd();
  }

  // Links ============================================

  // Check if airport is in navdata
  QStringList links;
  MapAirport airportNav = mapWidget->getMapQuery()->getAirportNav(airport);
  ahtml::Flags flags = ahtml::LINK_NO_UL | ahtml::NO_ENTITIES;

  if(airportNav.isValid() && airportNav.navdata)
  {
    links.append(html.cleared().a(tr("AirNav.com"),
                                  QString("https://www.airnav.com/airport/%1").arg(airportNav.displayIdent()), flags).getHtml());

    links.append(html.cleared().a(tr("ChartFox"), QString("https://chartfox.org/%1").arg(airportNav.displayIdent()), flags).
                 text(tr("&nbsp;(needs&nbsp;login)"), ahtml::SMALL | ahtml::NO_ENTITIES).getHtml());

    links.append(html.cleared().a(tr("FltPlan"),
                                  QString("https://fltplan.com/Airport.cgi?%1").arg(airportNav.displayIdent()), flags).getHtml());

    links.append(html.cleared().a(tr("FlightAware"),
                                  QString("https://www.flightaware.com/live/airport/%1").arg(airportNav.displayIdent()), flags).getHtml());

    links.append(html.cleared().a(tr("OpenNav"),
                                  QString("https://opennav.com/airport/%1").arg(airportNav.displayIdent()), flags).getHtml());

    // Removed Pilot Nav since it tricks you into a commercial service
    // links.append(html.cleared().a(tr("Pilot&nbsp;Nav"), QString("https://www.pilotnav.com/airport/%1").
    // arg(airportNav.displayIdent()), flags).getHtml());

    links.append(html.cleared().a(tr("SkyVector"),
                                  QString("https://skyvector.com/airport/%1").arg(airportNav.displayIdent()), flags).getHtml());
  }

  // Use internal id for X-Plane gateway since this includes the long internal idents
  if(airport.xplane)
    links.append(html.cleared().a(tr("X-Plane&nbsp;Scenery&nbsp;Gateway"),
                                  QString("https://gateway.x-plane.com/scenery/page/%1").
                                  arg(airport.ident), flags).getHtml());

  // Display link table ===============
  if(!links.isEmpty())
  {
    head(html, tr("Links"));
    html.table();
    html.row2(QString(), links.join(tr(",&nbsp;&nbsp; ")), ahtml::NO_ENTITIES);
    html.tableEnd();
  }
}

QString HtmlInfoBuilder::filepathTextShow(const QString& filepath, const QString& prefix, bool canonical) const
{
  HtmlBuilder link(true);

  if(filepath.isEmpty())
    return QString();

  QFileInfo fileinfo(filepath);

  if(fileinfo.exists())
  {
    QString filepathStr = atools::nativeCleanPath(canonical ? atools::canonicalFilePath(fileinfo) : fileinfo.absoluteFilePath());
    link.small(prefix).a(filepathStr, QString("lnm://show?filepath=%1").arg(filepathStr), ahtml::LINK_NO_UL | ahtml::SMALL);
  }
  else
    link.small(prefix).small(filepath).text(tr(" (file not found)"), ahtml::SMALL | ahtml::BOLD);
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
  if(pos.isValid())
    html.row2(tr("Coordinates:"), Unit::coords(pos));

#ifdef DEBUG_INFORMATION
  if(info)
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

void HtmlInfoBuilder::navaidTitle(HtmlBuilder& html, const QString& text, bool noEntities) const
{
  if(info)
    html.text(text, ahtml::BOLD | ahtml::BIG | (noEntities ? ahtml::NO_ENTITIES : ahtml::NONE));
  else
    html.text(text, ahtml::BOLD | (noEntities ? ahtml::NO_ENTITIES : ahtml::NONE));
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

void HtmlInfoBuilder::addMetarLines(HtmlBuilder& html, const map::WeatherContext& weatherContext, MapWeatherSource src,
                                    const map::MapAirport& airport) const
{
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
}

void HtmlInfoBuilder::addMetarLine(atools::util::HtmlBuilder& html, const QString& header,
                                   const map::MapAirport& airport, const QString& metar, const QString& station,
                                   const QDateTime& timestamp, bool fsMetar, bool mapDisplay) const
{
  if(!metar.isEmpty())
  {
    Metar m(metar, station, timestamp, fsMetar);
    const atools::fs::weather::MetarParser& parsed = m.getParsedMetar();
    QDateTime time;
    time.setOffsetFromUtc(0);
    time.setDate(QDate(parsed.getYear(), parsed.getMonth(), parsed.getDay()));
    time.setTime(QTime(parsed.getHour(), parsed.getMinute()));

    if(!parsed.isValid())
      qWarning() << "Metar is not valid";

    HtmlBuilder whtml = html.cleared();

    whtml.text(fsMetar ? m.getCleanMetar() : metar);

    qint64 secsTo = time.secsTo(QDateTime::currentDateTimeUtc());
    if(secsTo > WEATHER_MAX_AGE_HOURS_ERR * 3600)
      whtml.text(tr(" ")).error(tr("(%1 hours old)").arg(secsTo / 3600));
    else if(secsTo > WEATHER_MAX_AGE_HOURS_WARN * 3600)
      whtml.text(tr(" ")).warning(tr("(%1 hours old)").arg(secsTo / 3600));

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
    html.row2(header % (info ? tr(":") : tr(" METAR:")), whtml);

  }
}

void HtmlInfoBuilder::addFlightRulesSuffix(atools::util::HtmlBuilder& html,
                                           const atools::fs::weather::Metar& metar, bool mapDisplay) const
{
  if(!print)
  {
    html.img(SymbolPainter::createAirportWeatherIcon(metar, symbolSize.height()),
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
#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO;
#endif
  const WindReporter *windReporter = NavApp::getWindReporter();
  // Wind text is always shown independent of barb status at route
  if(index >= 0 && NavApp::getWindReporter()->hasAnyWindData() && route.isValidProfile() && windReporter->isRouteWindShown())
  {
    const RouteAltitudeLeg& altLeg = route.getAltitudeLegAt(index);
    if(altLeg.getLineString().getPos2().getAltitude() > MIN_WIND_BARB_ALTITUDE && !altLeg.isMissed() && !altLeg.isAlternate())
    {
      // Create entry for flight plan position which might be different from the cruise altitude
      atools::grib::WindPos routeWpWindPos;
      routeWpWindPos.pos = altLeg.getLineString().getPos2();
      routeWpWindPos.wind.dir = altLeg.getWindDirection();
      routeWpWindPos.wind.speed = altLeg.getWindSpeed();

      atools::grib::WindPosList winds;
      if(verbose)
        // Get full stack for all default altitudes - flight plan cruise altitude layer is already added
        winds = windReporter->getWindStackForPos(altLeg.getLineString().getPos2(), &routeWpWindPos);
      else
        // Show only one entry for flight plan pos if verbose tooltips not selected
        winds.append(routeWpWindPos);

      // Show wind text without header but using a table only
      windText(winds, html, routeWpWindPos.pos.getAltitude(), true /* table */);
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

QString HtmlInfoBuilder::highlightText(const QString& text) const
{
  return HtmlBuilder::textStr(text, ahtml::BOLD | ahtml::BIG);
}

void HtmlInfoBuilder::routeInfoText(HtmlBuilder& html, int routeIndex, bool recommended) const
{
#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO;
#endif

  if(!info && routeIndex >= 0)
  {
    if(recommended)
      html.row2(tr("Related navaid for procedure"), QString());
    html.row2(tr("Flight Plan Position:"), locale.toString(routeIndex + 1));
    flightplanWaypointRemarks(html, routeIndex);
  }
}
