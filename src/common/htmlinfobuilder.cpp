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

#include "common/htmlinfobuilder.h"

#include "options/optiondata.h"
#include "navapp.h"
#include "atools.h"
#include "fs/online/onlinetypes.h"
#include "common/formatter.h"
#include "common/maptypes.h"
#include "fs/bgl/ap/rw/runway.h"
#include "fs/sc/simconnectdata.h"
#include "geo/calculations.h"
#include "query/infoquery.h"
#include "query/mapquery.h"
#include "query/airportquery.h"
#include "route/route.h"
#include "sql/sqlrecord.h"
#include "userdata/userdataicons.h"
#include "fs/userdata/userdatamanager.h"
#include "common/symbolpainter.h"
#include "util/htmlbuilder.h"
#include "fs/util/morsecode.h"
#include "fs/util/tacanfrequencies.h"
#include "fs/util/fsutil.h"
#include "common/unit.h"
#include "fs/weather/metar.h"
#include "fs/weather/metarparser.h"
#include "common/vehicleicons.h"

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

/* Airport, navaid and userpoint size */
const QSize SYMBOL_SIZE(20, 20);

/* Aircraft size */
const QSize SYMBOL_SIZE_VEHICLE(28, 28);

const float HELIPAD_ZOOM_METER = 200.f;
const float STARTPOS_ZOOM_METER = 500.f;

const float MIN_GROUND_SPEED = 30.f;

// Print weather time in red if older than this
const int WEATHER_MAX_AGE_HOURS = 6;

HtmlInfoBuilder::HtmlInfoBuilder(MainWindow *parentWindow, bool formatInfo,
                                 bool formatPrint)
  : mainWindow(parentWindow), info(formatInfo),
  print(formatPrint)
{
  mapQuery = NavApp::getMapQuery();
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
  html.img(SymbolPainter().createAirportIcon(airport, SYMBOL_SIZE.height()),
           QString(), QString(), SYMBOL_SIZE);
  html.nbsp().nbsp();

  // Adapt title to airport status
  Flags titleFlags = atools::util::html::BOLD;
  if(airport.closed())
    titleFlags |= atools::util::html::STRIKEOUT;
  if(airport.addon())
    titleFlags |= atools::util::html::ITALIC;

  if(info)
  {
    html.text(tr("%1 (%2)").arg(airport.name).arg(airport.ident), titleFlags | atools::util::html::BIG);
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
             atools::util::html::LINK_NO_UL);
  }
  else
  {
    html.text(tr("%1 (%2)").arg(airport.name).arg(airport.ident), titleFlags);
    if(rating != -1)
      html.nbsp().nbsp().text(atools::ratingString(rating, 5));
  }
}

void HtmlInfoBuilder::airportText(const MapAirport& airport, const map::WeatherContext& weatherContext,
                                  HtmlBuilder& html, const Route *route) const
{
  const SqlRecord *rec = infoQuery->getAirportInformation(airport.id);
  int rating = -1;

  if(rec != nullptr)
    rating = rec->valueInt("rating");

  airportTitle(airport, html, rating);
  html.br();

  QString city, state, country;
  airportQuerySim->getAirportAdminNamesById(airport.id, city, state, country);

  html.table();
  if(route != nullptr && !route->isEmpty() && airport.routeIndex != -1)
  {
    // Add flight plan information if airport is a part of it
    if(airport.routeIndex == 0)
      html.row2(tr("Departure Airport"), QString());
    else if(airport.routeIndex == route->size() - 1)
      html.row2(tr("Destination Airport"), QString());
    else
      html.row2(tr("Flight Plan position:"), locale.toString(airport.routeIndex + 1));
  }

  // Administrative information
  if(!city.isEmpty())
    html.row2(tr("City:"), city);
  if(!state.isEmpty())
    html.row2(tr("State or Province:"), state);
  if(!country.isEmpty())
    html.row2(tr("Country or Area Code:"), country);
  if(!airport.region.isEmpty())
    html.row2(tr("Region:"), airport.region);
  html.row2(tr("Elevation:"), Unit::altFeet(airport.getPosition().getAltitude()));
  html.row2(tr("Magnetic declination:"), map::magvarText(airport.magvar));

  // Get transition altitude from nav database
  map::MapAirport navAirport = airport;
  NavApp::getMapQuery()->getAirportNavReplace(navAirport);
  if(navAirport.transitionAltitude > 0)
    html.row2(tr("Transition altitude:"), Unit::altFeet(navAirport.transitionAltitude));

  if(info)
    addCoordinates(rec, html);

  html.tableEnd();

  // Create a list of facilities
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

    const atools::fs::weather::MetarResult& fsMetar = weatherContext.fsMetar;
    if(!fsMetar.isEmpty())
    {
      QString sim = tr("%1 ").arg(NavApp::getCurrentSimulatorShortName());
      addMetarLine(html, tr("%1Station").arg(
                     sim), fsMetar.metarForStation, fsMetar.requestIdent, fsMetar.timestamp, true);
      addMetarLine(html, tr("%1Nearest").arg(
                     sim), fsMetar.metarForNearest, fsMetar.requestIdent, fsMetar.timestamp, true);
      addMetarLine(html, tr("%1Interpolated").arg(sim),
                   fsMetar.metarForInterpolated, fsMetar.requestIdent, fsMetar.timestamp, true);
    }

    addMetarLine(html, weatherContext.asType, weatherContext.asMetar);

    addMetarLine(html, tr("NOAA Station"), weatherContext.noaaMetar.metarForStation,
                 weatherContext.noaaMetar.requestIdent, weatherContext.noaaMetar.timestamp);
    addMetarLine(html, tr("NOAA Nearest"), weatherContext.noaaMetar.metarForNearest,
                 weatherContext.noaaMetar.requestIdent, weatherContext.noaaMetar.timestamp);

    addMetarLine(html, tr("VATSIM"), weatherContext.vatsimMetar);

    addMetarLine(html, tr("IVAO Station"), weatherContext.ivaoMetar.metarForStation,
                 weatherContext.ivaoMetar.requestIdent, weatherContext.ivaoMetar.timestamp);
    addMetarLine(html, tr("IVAO Nearest"), weatherContext.ivaoMetar.metarForNearest,
                 weatherContext.ivaoMetar.requestIdent, weatherContext.ivaoMetar.timestamp);
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

      float hdg = rec->valueFloat("longest_runway_heading") - airport.magvar;
      hdg = normalizeCourse(hdg);
      float otherHdg = normalizeCourse(opposedCourseDeg(hdg));

      html.row2(tr("Heading:"), locale.toString(hdg, 'f', 0) + tr("°M, ") +
                locale.toString(otherHdg, 'f', 0) + tr("°M"));
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
      html.row2(tr("Tower:"), locale.toString(airport.towerFrequency / 1000., 'f', 3) + tr(" MHz"));
    if(airport.atisFrequency > 0)
      html.row2(tr("ATIS:"), locale.toString(airport.atisFrequency / 1000., 'f', 3) + tr(" MHz"));
    if(airport.awosFrequency > 0)
      html.row2(tr("AWOS:"), locale.toString(airport.awosFrequency / 1000., 'f', 3) + tr(" MHz"));
    if(airport.asosFrequency > 0)
      html.row2(tr("ASOS:"), locale.toString(airport.asosFrequency / 1000., 'f', 3) + tr(" MHz"));
    if(airport.unicomFrequency > 0)
      html.row2(tr("Unicom:"), locale.toString(airport.unicomFrequency / 1000., 'f', 3) + tr(" MHz"));
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
  }

  if(info && !print)
    addAirportScenery(airport, html);

#ifdef DEBUG_INFORMATION
  html.p().small(QString("Database: airport_id = %1").arg(airport.getId())).pEnd();
#endif
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
      html.h3(tr("COM Frequencies"));
      html.table();
      html.tr().td(tr("Type"), atools::util::html::BOLD).
      td(tr("Frequency"), atools::util::html::BOLD).
      td(tr("Name"), atools::util::html::BOLD).trEnd();

      for(const SqlRecord& rec : *recVector)
      {
        html.tr();
        html.td(map::comTypeName(rec.valueStr("type")));
        html.td(locale.toString(rec.valueInt("frequency") / 1000., 'f', 3) + tr(" MHz"));
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

void HtmlInfoBuilder::runwayText(const MapAirport& airport, HtmlBuilder& html, bool details, bool soft) const
{
  if(info && infoQuery != nullptr)
  {
    if(!print)
      airportTitle(airport, html, -1);

    const SqlRecordVector *recVector = infoQuery->getRunwayInformation(airport.id);
    if(recVector != nullptr)
    {
      for(const SqlRecord& rec : *recVector)
      {
        if(!soft && !map::isHardSurface(rec.valueStr("surface")))
          continue;

        const SqlRecord *recPrim = infoQuery->getRunwayEndInformation(rec.valueInt("primary_end_id"));
        const SqlRecord *recSec = infoQuery->getRunwayEndInformation(rec.valueInt("secondary_end_id"));
        float hdgPrim = normalizeCourse(rec.valueFloat("heading") - airport.magvar);
        float hdgSec = normalizeCourse(opposedCourseDeg(hdgPrim));
        bool closedPrim = recPrim->valueBool("has_closed_markings");
        bool closedSec = recSec->valueBool("has_closed_markings");

        html.h3(tr("Runway ") + recPrim->valueStr("name") + ", " + recSec->valueStr("name"),
                (closedPrim & closedSec ? atools::util::html::STRIKEOUT : atools::util::html::NONE)
                | atools::util::html::UNDERLINE);
        html.table();

        html.row2(tr("Size:"), Unit::distShortFeet(rec.valueFloat("length"), false) +
                  tr(" x ") +
                  Unit::distShortFeet(rec.valueFloat("width")));

        html.row2(tr("Surface:"), map::surfaceName(rec.valueStr("surface")));

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

#ifdef DEBUG_INFORMATION
        html.p().small(QString("Database: runway_id = %1").arg(rec.valueInt("runway_id"))).pEnd();
#endif

        if(details)
        {
          runwayEndText(html, airport, recPrim, hdgPrim, rec.valueFloat("length"));
#ifdef DEBUG_INFORMATION
          html.p().small(QString("Database: Primary runway_end_id = %1").arg(recPrim->valueInt("runway_end_id"))).pEnd();
#endif
          runwayEndText(html, airport, recSec, hdgSec, rec.valueFloat("length"));
#ifdef DEBUG_INFORMATION
          html.p().small(QString("Database: Secondary runway_end_id = %1").arg(recSec->valueInt("runway_end_id"))).pEnd();
#endif
        }
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
                  (closed ? atools::util::html::STRIKEOUT : atools::util::html::NONE)
                  | atools::util::html::UNDERLINE);
          html.nbsp().nbsp();

          atools::geo::Pos pos(heliRec.valueFloat("lonx"), heliRec.valueFloat("laty"));

          if(!print)
            html.a(tr("Map"), QString("lnm://show?lonx=%1&laty=%2&zoom=%3").
                   arg(pos.getLonX()).arg(pos.getLatY()).arg(Unit::distMeterF(HELIPAD_ZOOM_METER)),
                   atools::util::html::LINK_NO_UL).br();

          if(closed)
            html.text(tr("Is Closed"));
          html.table();

          html.row2(tr("Size:"), Unit::distShortFeet(heliRec.valueFloat("width"), false) +
                    tr(" x ") +
                    Unit::distShortFeet(heliRec.valueFloat("length")));
          html.row2(tr("Surface:"), map::surfaceName(heliRec.valueStr("surface")) +
                    (heliRec.valueBool("is_transparent") ? tr(" (Transparent)") : QString()));
          html.row2(tr("Type:"), atools::capString(heliRec.valueStr("type")));
          html.row2(tr("Heading:"), tr("%1°M").arg(locale.toString(heliRec.valueFloat("heading"), 'f', 0)));
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
        for(const SqlRecord& startRec: *startVector)
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

          atools::geo::Pos pos(startRec.valueFloat("lonx"), startRec.valueFloat("laty"));

          if(i > 0)
            html.text(tr(", "));
          if(print)
            html.text(startText);
          else
            html.a(startText, QString("lnm://show?lonx=%1&laty=%2&zoom=%3").
                   arg(pos.getLonX()).arg(pos.getLatY()).arg(Unit::distMeterF(STARTPOS_ZOOM_METER)),
                   atools::util::html::LINK_NO_UL);
          i++;
        }
      }
      else
        html.p(tr("Airport has no start position."));
    }
  }
}

void HtmlInfoBuilder::runwayEndText(HtmlBuilder& html, const MapAirport& airport, const SqlRecord *rec, float hdgPrim,
                                    float length) const
{
  bool closed = rec->valueBool("has_closed_markings");

  html.h3(rec->valueStr("name"), closed ? (atools::util::html::STRIKEOUT) : atools::util::html::NONE);
  html.table();
  if(closed)
    html.row2(tr("Closed"), QString());
  html.row2(tr("Heading:"), tr("%1°M").arg(locale.toString(hdgPrim, 'f', 0)));

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
      ilsText(&irec, html, false);
  }
}

void HtmlInfoBuilder::ilsText(const atools::sql::SqlRecord *ilsRec, HtmlBuilder& html, bool approach) const
{
  // Add ILS information
  bool dme = !ilsRec->isNull("dme_altitude");
  bool gs = !ilsRec->isNull("gs_altitude");
  float magvar = ilsRec->valueFloat("mag_var");

  QString name = tr("%1 (%2) - ILS %3%4").arg(ilsRec->valueStr("name")).arg(ilsRec->valueStr("ident")).
                 arg(gs ? tr(", GS") : "").arg(dme ? tr(", DME") : "");

  QString prefix;
  if(!approach)
  {
    html.br().h4(name);
    html.table();
  }
  else
  {
    prefix = tr("ILS ");
    html.row2(tr("ILS:"), name);
  }

  html.row2(prefix + tr("Frequency:"),
            locale.toString(ilsRec->valueFloat("frequency") / 1000., 'f', 2) + tr(" MHz"));

  if(!approach)
  {
    html.row2(tr("Range:"), Unit::distNm(ilsRec->valueFloat("range")));
    html.row2(tr("Magnetic declination:"), map::magvarText(magvar));
    rowForBool(html, ilsRec, "has_backcourse", tr("Has Backcourse"), false);
  }

  float hdg = ilsRec->valueFloat("loc_heading") - magvar;
  hdg = normalizeCourse(hdg);

  if(ilsRec->isNull("loc_width"))
    html.row2(prefix + tr("Localizer Heading:"), locale.toString(hdg, 'f', 0) + tr("°M"));
  else
    html.row2(prefix + tr("Localizer Heading and Width:"), locale.toString(hdg, 'f', 0) + tr("°M") +
              tr(", ") + locale.toString(ilsRec->valueFloat("loc_width"), 'f', 1) + tr("°"));

  if(gs)
    html.row2(prefix + tr("Glideslope Pitch:"),
              locale.toString(ilsRec->valueFloat("gs_pitch"), 'f', 1) + tr("°"));

  if(!approach)
    html.tableEnd();
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

void HtmlInfoBuilder::procedureText(const MapAirport& airport, HtmlBuilder& html) const
{
  if(info && infoQuery != nullptr && airport.isValid())
  {
    MapAirport navAirport = mapQuery->getAirportNav(airport);

    if(!print)
      airportTitle(airport, html, -1);

    html.p(tr("Approaches and Transitions"));

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

        html.h3(header, atools::util::html::UNDERLINE);

        // Fill table ==========================================================
        html.table();

        if(!(type& proc::PROCEDURE_SID) && !(type & proc::PROCEDURE_STAR))
        {
          rowForBool(html, &recApp, "has_gps_overlay", tr("Has GPS Overlay"), false);
          // html.row2(tr("Fix Ident and Region:"), recApp.valueStr("fix_ident") + tr(", ") +
          // recApp.valueStr("fix_region"));

          // float hdg = recApp.valueFloat("heading") - airport.magvar;
          // hdg = normalizeCourse(hdg);
          // html.row2(tr("Heading:"), tr("%1°M, %2°T").arg(locale.toString(hdg, 'f', 0)).
          // arg(locale.toString(recApp.valueFloat("heading"), 'f', 0)));
        }

        // html.row2(tr("Altitude:"), Unit::altFeet(recApp.valueFloat("altitude")));
        // html.row2(tr("Missed Altitude:"), Unit::altFeet(recApp.valueFloat("missed_altitude")));

        addRadionavFixType(html, recApp);

        if(procType == "ILS" || procType == "LOC")
        {
          // Display ILS information ===========================================
          const atools::sql::SqlRecordVector *ilsRec =
            infoQuery->getIlsInformationSimByName(airport.ident, runwayIdent);
          if(ilsRec != nullptr && !ilsRec->isEmpty())
          {
            for(const atools::sql::SqlRecord& irec : *ilsRec)
              ilsText(&irec, html, true);
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

            if(backcourseEndIdent != 0)
            {
              const atools::sql::SqlRecordVector *ilsRec =
                infoQuery->getIlsInformationSimByName(airport.ident, backcourseEndIdent);
              if(ilsRec != nullptr && !ilsRec->isEmpty())
              {
                for(const atools::sql::SqlRecord& irec : *ilsRec)
                  ilsText(&irec, html, true);
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
        html.p().small(QString("Database: approach_id = %1").arg(recApp.valueInt("approach_id"))).pEnd();
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
                html.row2(tr("DME Morse:"), morse->getCode(vorReg.valueStr("ident")),
                          atools::util::html::BOLD | atools::util::html::NO_ENTITIES);
              }
              else
                html.row2(tr("DME data not found for %1/%2.").
                          arg(recTrans.valueStr("dme_ident")).arg(recTrans.valueStr("dme_region")));
            }

            addRadionavFixType(html, recTrans);
            html.tableEnd();
#ifdef DEBUG_INFORMATION
            html.p().small(QString("Database: transition_id = %1").arg(recTrans.valueInt("transition_id"))).pEnd();
#endif
          }
        }
      }
    }
    else
      html.p(tr("Airport has no approach."));
  }
}

void HtmlInfoBuilder::addRadionavFixType(atools::util::HtmlBuilder& html, const SqlRecord& recApp) const
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
        html.row2(tr("VORTAC Morse:"), morse->getCode(
                    vor.ident), atools::util::html::BOLD | atools::util::html::NO_ENTITIES);
      }
      else
      {
        html.row2(tr("VOR Type:"), map::navTypeNameVorLong(vor.type));
        html.row2(tr("VOR Frequency:"), locale.toString(vor.frequency / 1000., 'f', 2) + tr(" MHz"));
        if(vor.range > 0)
          html.row2(tr("VOR Range:"), Unit::distNm(vor.range));
        html.row2(tr("VOR Morse:"), morse->getCode(
                    vor.ident), atools::util::html::BOLD | atools::util::html::NO_ENTITIES);
      }
    }
    else
      qWarning() << "VOR data not found";
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

      html.row2(tr("NDB Frequency:"), locale.toString(ndb.frequency / 100., 'f', 2) + tr(" MHz"));

      if(ndb.range > 0)
        html.row2(tr("NDB Range:"), Unit::distNm(ndb.range));

      html.row2(tr("NDB Morse:"), morse->getCode(ndb.ident),
                atools::util::html::BOLD | atools::util::html::NO_ENTITIES);
    }
    else
      qWarning() << "NDB data not found";
  }
}

static const Flags WEATHER_TITLE_FLAGS = atools::util::html::BOLD | atools::util::html::BIG;

void HtmlInfoBuilder::weatherText(const map::WeatherContext& context, const MapAirport& airport,
                                  atools::util::HtmlBuilder& html) const
{

  if(info)
  {
    if(!print)
      airportTitle(airport, html, -1);

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
        decodedMetar(html, airport, map::MapAirport(), met, false /* interpolated */, fsxP3d);
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
                 atools::util::html::LINK_NO_UL);
        }

        decodedMetar(html, airport, reportAirport, met, false /* interpolated */, fsxP3d);
      }

      if(!metar.metarForInterpolated.isEmpty())
      {
        Metar met(metar.metarForInterpolated, metar.requestIdent, metar.timestamp, fsxP3d);
        html.p(tr("%2Interpolated Weather - %1").arg(met.getStation()).arg(sim), WEATHER_TITLE_FLAGS);
        decodedMetar(html, airport, map::MapAirport(), met, true /* interpolated */, fsxP3d);
      }
    }
    else if(!print && OptionData::instance().getFlags() & opts::WEATHER_INFO_FS)
      html.p(tr("Not connected to simulator."), atools::util::html::BOLD);

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
                   false /* FSX/P3D */);
    }

    // NOAA or nearest
    decodedMetars(html, context.noaaMetar, airport, tr("NOAA"));

    // Vatsim metar ===========================
    if(!context.vatsimMetar.isEmpty())
    {
      html.p(tr("VATSIM Weather"), WEATHER_TITLE_FLAGS);
      decodedMetar(html, airport, map::MapAirport(), Metar(context.vatsimMetar),
                   false /* interpolated */, false /* FSX/P3D */);
    }

    // IVAO or nearest
    decodedMetars(html, context.ivaoMetar, airport, tr("IVAO"));
  }
}

void HtmlInfoBuilder::decodedMetars(HtmlBuilder& html, const atools::fs::weather::MetarResult& metar,
                                    const map::MapAirport& airport, const QString& name) const
{
  if(metar.isValid())
  {
    if(!metar.metarForStation.isEmpty())
    {
      html.p(tr("%1 Station Weather").arg(name), WEATHER_TITLE_FLAGS);
      decodedMetar(html, airport, map::MapAirport(),
                   Metar(metar.metarForStation, metar.requestIdent, metar.timestamp, true), false, false);
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
               atools::util::html::LINK_NO_UL);
      }

      decodedMetar(html, airport, reportAirport, met, false, false);
    }
  }
}

void HtmlInfoBuilder::decodedMetar(HtmlBuilder& html, const map::MapAirport& airport,
                                   const map::MapAirport& reportAirport,
                                   const atools::fs::weather::Metar& metar, bool isInterpolated, bool isFsxP3d) const
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
    atools::util::html::Flags flags = atools::util::html::NONE;
    QColor color;
    if(time < oldUtc)
    {
      flags |= atools::util::html::BOLD;
      color = Qt::red;
    }
    html.row2(tr("Time: "), locale.toString(time, QLocale::ShortFormat) + " " + time.timeZoneAbbreviation(),
              flags, color);
  }

  if(!parsed.getReportTypeString().isEmpty())
    html.row2(tr("Report type: "), parsed.getReportTypeString());

  bool hasWind = false;
  // Wind =============================================================
  if(parsed.getWindSpeedMeterPerSec() > 0.f &&
     parsed.getWindSpeedMeterPerSec() < atools::fs::weather::INVALID_METAR_VALUE)
  {
    QString windDir, windVar;

    if(parsed.getWindDir() >= 0)
      // Wind direction given
      windDir = locale.toString(normalizeCourse(parsed.getWindDir() - airport.magvar), 'f', 0) + tr("°M") + tr(", ");

    if(parsed.getWindRangeFrom() != -1 && parsed.getWindRangeTo() != -1)
      // Wind direction range given (additionally to dir in some cases)
      windVar = tr(", variable ") +
                locale.toString(normalizeCourse(parsed.getWindRangeFrom() - airport.magvar), 'f', 0) +
                tr(" to ") +
                locale.toString(normalizeCourse(parsed.getWindRangeTo() - airport.magvar), 'f', 0) + tr("°M");
    else if(parsed.getWindDir() == -1)
      windDir = tr("Variable, ");

    float windMeterPerSec = parsed.getWindSpeedMeterPerSec();
    QString windSpeedStr;
    if(windMeterPerSec < INVALID_METAR_VALUE)
      windSpeedStr = Unit::speedMeterPerSec(windMeterPerSec);

    html.row2(tr("Wind:"), windDir + windSpeedStr + windVar);
    hasWind = true;
  }

  if(parsed.getGustSpeedMeterPerSec() < INVALID_METAR_VALUE)
  {
    hasWind = true;
    html.row2(tr("Wind gusts:"), Unit::speedMeterPerSec(parsed.getGustSpeedMeterPerSec()));
  }

  // Temperature  =============================================================
  float temp = parsed.getTemperatureC();
  if(temp < INVALID_METAR_VALUE)
    html.row2(tr("Temperature:"), locale.toString(atools::roundToInt(temp)) + tr("°C, ") +
              locale.toString(atools::roundToInt(atools::geo::degCToDegF(temp))) + tr("°F"));

  temp = parsed.getDewpointDegC();
  if(temp < INVALID_METAR_VALUE)
    html.row2(tr("Dewpoint:"), locale.toString(atools::roundToInt(temp)) + tr("°C, ") +
              locale.toString(atools::roundToInt(atools::geo::degCToDegF(temp))) + tr("°F"));

  // Pressure  =============================================================
  float slp = parsed.getPressureMbar();
  if(slp < INVALID_METAR_VALUE)
    html.row2(tr("Pressure:"), locale.toString(slp, 'f', 0) + tr(" hPa, ") +
              locale.toString(atools::geo::mbarToInHg(slp), 'f', 2) + tr(" inHg"));

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
    QString wtr = weather.join(", ").toLower().replace(" of in ", " in ");

    if(!wtr.isEmpty())
      wtr[0] = wtr.at(0).toUpper();

    html.row2(tr("Conditions:"), wtr);
  }

  html.tableEnd();

  if(hasClouds)
    html.p(tr("Clouds"), atools::util::html::BOLD);

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

  // QString getData()
  // bool getProxy()
  // QString getId()
  // MetarVisibility& getVertVisibility()
  // MetarVisibility *getDirVisibility()
  // double getRelHumidity() ;
  // QHash<QString, MetarRunway> getRunways() ;

  if(parsed.getCavok())
    html.p().text(tr("CAVOK:"), atools::util::html::BOLD).br().
    text(tr("No cloud below 5,000 ft (1,500 m), visibility of 10 km (6 nm) or more")).pEnd();

  if(!metar.getParsedMetar().getRemark().isEmpty())
    html.p().text(tr("Remarks:"), atools::util::html::BOLD).br().
    text(metar.getParsedMetar().getRemark()).pEnd();

  if(!parsed.isValid())
  {
    // Print error report if invalid
    html.p(tr("Report is not valid. Raw and clean METAR were:"), atools::util::html::BOLD, Qt::red);
    html.pre(metar.getMetar());
    if(metar.getMetar() != metar.getCleanMetar())
      html.pre(metar.getCleanMetar());
  }

  if(!parsed.getUnusedData().isEmpty())
    html.p().text(tr("Additional information:"), atools::util::html::BOLD).br().text(parsed.getUnusedData()).pEnd();

#ifdef DEBUG_INFORMATION
  html.p().small(tr("Source: %1").arg(metar.getMetar())).pEnd();
#endif
}

void HtmlInfoBuilder::vorText(const MapVor& vor, HtmlBuilder& html) const
{
  const SqlRecord *rec = nullptr;
  if(info && infoQuery != nullptr)
    rec = infoQuery->getVorInformation(vor.id);

  QIcon icon = SymbolPainter().createVorIcon(vor, SYMBOL_SIZE.height());
  html.img(icon, QString(), QString(), SYMBOL_SIZE);
  html.nbsp().nbsp();

  QString type = map::vorType(vor);
  navaidTitle(html, type + ": " + capString(vor.name) + " (" + vor.ident + ")");

  if(info)
  {
    // Add map link if not tooltip
    html.nbsp().nbsp();
    html.a(tr("Map"), QString("lnm://show?lonx=%1&laty=%2").
           arg(vor.position.getLonX()).arg(vor.position.getLatY()), atools::util::html::LINK_NO_UL);
    html.br();
  }

  html.table();
  if(vor.routeIndex >= 0)
    html.row2(tr("Flight Plan position:"), locale.toString(vor.routeIndex + 1));

  if(vor.tacan)
  {
    if(vor.dmeOnly)
      html.row2(tr("Type:"), tr("DME only"));
  }
  else
    html.row2(tr("Type:"), map::navTypeNameVorLong(vor.type));

  html.row2(tr("Region:"), vor.region);

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
  html.row2(tr("Morse:"), morse->getCode(vor.ident),
            atools::util::html::BOLD | atools::util::html::NO_ENTITIES);
  addCoordinates(rec, html);
  html.tableEnd();

  if(rec != nullptr)
    addScenery(rec, html);

#ifdef DEBUG_INFORMATION
  html.p().small(QString("Database: vor_id = %1").arg(vor.getId())).pEnd();
#endif
}

void HtmlInfoBuilder::ndbText(const MapNdb& ndb, HtmlBuilder& html) const
{
  const SqlRecord *rec = nullptr;
  if(info && infoQuery != nullptr)
    rec = infoQuery->getNdbInformation(ndb.id);

  QIcon icon = SymbolPainter().createNdbIcon(SYMBOL_SIZE.height());
  html.img(icon, QString(), QString(), SYMBOL_SIZE);
  html.nbsp().nbsp();

  navaidTitle(html, tr("NDB: ") + capString(ndb.name) + " (" + ndb.ident + ")");

  if(info)
  {
    // Add map link if not tooltip
    html.nbsp().nbsp();
    html.a(tr("Map"), QString("lnm://show?lonx=%1&laty=%2").
           arg(ndb.position.getLonX()).arg(ndb.position.getLatY()), atools::util::html::LINK_NO_UL);
    html.br();
  }

  html.table();
  if(ndb.routeIndex >= 0)
    html.row2(tr("Flight Plan position "), locale.toString(ndb.routeIndex + 1));
  if(!ndb.type.isEmpty())
    html.row2(tr("Type:"), map::navTypeNameNdb(ndb.type));
  html.row2(tr("Region:"), ndb.region);
  html.row2(tr("Frequency:"), locale.toString(ndb.frequency / 100., 'f', 1) + tr(" kHz"));
  html.row2(tr("Magnetic declination:"), map::magvarText(ndb.magvar));
  if(ndb.getPosition().getAltitude() < INVALID_ALTITUDE_VALUE)
    html.row2(tr("Elevation:"), Unit::altFeet(ndb.getPosition().getAltitude()));
  if(ndb.range > 0)
    html.row2(tr("Range:"), Unit::distNm(ndb.range));
  html.row2(tr("Morse:"), morse->getCode(ndb.ident), atools::util::html::BOLD | atools::util::html::NO_ENTITIES);
  addCoordinates(rec, html);
  html.tableEnd();

  if(rec != nullptr)
    addScenery(rec, html);

#ifdef DEBUG_INFORMATION
  html.p().small(QString("Database: ndb_id = %1").arg(ndb.getId())).pEnd();
#endif
}

void HtmlInfoBuilder::userpointText(const MapUserpoint& userpoint, HtmlBuilder& html) const
{
  atools::sql::SqlRecord rec = NavApp::getUserdataManager()->getRecord(userpoint.id);

  // Check if userpoint still exists since it can be deleted in the background
  if(!rec.isEmpty())
  {
    QIcon icon(NavApp::getUserdataIcons()->getIconPath(userpoint.type));
    html.img(icon, QString(), QString(), SYMBOL_SIZE);
    html.nbsp().nbsp();

    navaidTitle(html, tr("Userpoint%1").arg(userpoint.temp ? tr(" (Temporary)") : QString()));

    if(info)
    {
      // Add map link if not tooltip
      html.nbsp().nbsp();
      html.a(tr("Map"), QString("lnm://show?lonx=%1&laty=%2").
             arg(userpoint.position.getLonX()).arg(userpoint.position.getLatY()), atools::util::html::LINK_NO_UL);
      html.br();
    }

    html.table();
    // Be cautious with user defined data and adapt it for HTML display
    html.row2If(tr("Type:"), adjustText(userpoint.type), atools::util::html::NO_ENTITIES);
    html.row2If(tr("Ident:"), adjustText(userpoint.ident), atools::util::html::NO_ENTITIES);
    html.row2If(tr("Region:"), adjustText(userpoint.region), atools::util::html::NO_ENTITIES);
    html.row2If(tr("Name:"), adjustText(userpoint.name), atools::util::html::NO_ENTITIES);

    QString description = adjustText(atools::elideTextLinesShort(userpoint.description, info ? 40 : 4));
    html.row2If(tr("Description:"), description, atools::util::html::NO_ENTITIES);

    html.row2If(tr("Tags:"), adjustText(userpoint.tags), atools::util::html::NO_ENTITIES);
    if(!rec.isNull("altitude"))
      html.row2If(tr("Elevation:"), Unit::altFeet(rec.valueFloat("altitude")));

    if(info)
      addCoordinates(userpoint.position, html);
    html.tableEnd();

    if(info)
    {
      html.br();
      html.table();
      if(!rec.isNull("visible_from"))
        html.row2If(tr("Visible from :"), Unit::distNm(rec.valueFloat("visible_from")));

      html.row2(tr("Last Change:"), rec.value("last_edit_timestamp").toDateTime().toString());
      html.tableEnd();
    }

    if(info && !rec.isNull("import_file_path"))
    {
      head(html, tr("File"));
      html.table();
      html.row2(tr("Imported from:"), filepathText(rec.valueStr("import_file_path")),
                atools::util::html::NO_ENTITIES | atools::util::html::SMALL);
      html.tableEnd();
    }

#ifdef DEBUG_INFORMATION
    html.p().small(QString("Database: uerpoint_id = %1").arg(userpoint.getId())).pEnd();
#endif
  }
  else
    qWarning() << Q_FUNC_INFO << "Empty record";
}

void HtmlInfoBuilder::waypointText(const MapWaypoint& waypoint, HtmlBuilder& html) const
{
  const SqlRecord *rec = nullptr;

  if(info && infoQuery != nullptr)
    rec = infoQuery->getWaypointInformation(waypoint.id);

  QIcon icon = SymbolPainter().createWaypointIcon(SYMBOL_SIZE.height());
  html.img(icon, QString(), QString(), SYMBOL_SIZE);
  html.nbsp().nbsp();

  navaidTitle(html, tr("Waypoint: ") + waypoint.ident);

  if(info)
  {
    // Add map link if not tooltip
    html.nbsp().nbsp();
    html.a(tr("Map"), QString("lnm://show?lonx=%1&laty=%2").
           arg(waypoint.position.getLonX()).arg(waypoint.position.getLatY()), atools::util::html::LINK_NO_UL);
    html.br();
  }

  html.table();
  if(waypoint.routeIndex >= 0)
    html.row2(tr("Flight Plan position:"), locale.toString(waypoint.routeIndex + 1));
  html.row2(tr("Type:"), map::navTypeNameWaypoint(waypoint.type));
  html.row2(tr("Region:"), waypoint.region);
  // html.row2(tr("Airport:"), waypoint.airportIdent);
  html.row2(tr("Magnetic declination:"), map::magvarText(waypoint.magvar));
  addCoordinates(rec, html);

  html.tableEnd();

  QList<MapAirway> airways;
  mapQuery->getAirwaysForWaypoint(airways, waypoint.id);

  if(!airways.isEmpty())
  {
    // Add airway information

    // Add airway name/text to vector
    QVector<std::pair<QString, QString> > airwayTexts;
    for(const MapAirway& aw : airways)
    {
      QString txt(map::airwayTypeToString(aw.type));

      QString altTxt = map::airwayAltText(aw);

      if(!altTxt.isEmpty())
        txt.append(tr(", ") + altTxt);

      airwayTexts.append(std::make_pair(aw.name, txt));
    }

    if(!airwayTexts.isEmpty())
    {
      // Sort airways by name
      std::sort(airwayTexts.begin(), airwayTexts.end(),
                [](const std::pair<QString, QString>& item1, const std::pair<QString, QString>& item2)
      {
        return item1.first < item2.first;
      });

      // Remove duplicates
      airwayTexts.erase(std::unique(airwayTexts.begin(), airwayTexts.end()), airwayTexts.end());

      if(info)
        head(html, tr("Airways:"));
      else
        html.br().b(tr("Airways: "));

      html.table();
      for(const std::pair<QString, QString>& aw : airwayTexts)
        html.row2(aw.first, aw.second);
      html.tableEnd();
    }
  }

  if(rec != nullptr)
    addScenery(rec, html);

#ifdef DEBUG_INFORMATION
  html.p().small(QString("Database: waypoint_id = %1").arg(waypoint.getId())).pEnd();
#endif
}

void HtmlInfoBuilder::airspaceText(const MapAirspace& airspace, const atools::sql::SqlRecord& onlineRec,
                                   HtmlBuilder& html) const
{
  QIcon icon = SymbolPainter().createAirspaceIcon(airspace, SYMBOL_SIZE.height());
  html.img(icon, QString(), QString(), SYMBOL_SIZE);
  html.nbsp().nbsp();

  QString network;
  if(airspace.online)
  {
    if(!NavApp::getOnlineNetwork().isEmpty())
      network = tr(" (%1)").arg(NavApp::getOnlineNetwork());
  }

  if(airspace.name.isEmpty())
    navaidTitle(html, tr("Airspace") + network);
  else
  {
    // Do not capitalize online network center names
    QString name = airspace.online ? airspace.name : formatter::capNavString(airspace.name);
    if(!info)
      name = atools::elideTextShort(name, 40);

    navaidTitle(html, ((info && !airspace.online) ? tr("Airspace: ") : QString()) + name + network);
  }

  if(info)
  {
    // Add map link if not tooltip
    html.nbsp().nbsp();
    html.a(tr("Map"),
           QString("lnm://show?id=%1&type=%2").
           arg(airspace.id).
           arg(airspace.online ? map::AIRSPACE_ONLINE : map::AIRSPACE),
           atools::util::html::LINK_NO_UL);
  }

  if(info)
    html.p(map::airspaceRemark(airspace.type));

  html.table();
  html.row2(tr("Type:"), map::airspaceTypeToString(airspace.type));

  if(!airspace.online)
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
      freqTxt.append(locale.toString(freq / 1000., 'f', 3));

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
    html.row2If(tr("ATIS:"), onlineRec.valueStr("atis"));
    html.row2If(tr("ATIS Time:"), locale.toString(onlineRec.valueDateTime("atis_time")));

    float qnh = onlineRec.valueFloat("qnh_mb");
    if(qnh > 0.f)
      html.row2(tr("Sea Level Pressure:"), locale.toString(qnh, 'f', 0) + tr(" hPa, ") +
                locale.toString(atools::geo::mbarToInHg(qnh), 'f', 2) + tr(" inHg"));

    if(onlineRec.isNull("administrative_rating") || onlineRec.isNull("atc_pilot_rating"))
      html.row2If(tr("Combined Rating:"), onlineRec.valueStr("combined_rating"));

    if(!onlineRec.isNull("administrative_rating"))
      html.row2If(tr("Aministrative Rating:"),
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

  if(info && !airspace.online)
  {
    const atools::sql::SqlRecord *rec = infoQuery->getAirspaceInformation(airspace.id);

    if(rec != nullptr)
      addScenery(rec, html);
  }

#ifdef DEBUG_INFORMATION
  html.p().small(QString("Database: online = %1, boundary_id = %2").arg(airspace.online).arg(airspace.getId())).pEnd();
#endif

}

void HtmlInfoBuilder::airwayText(const MapAirway& airway, HtmlBuilder& html) const
{
  navaidTitle(html, tr("Airway: ") + airway.name);
  html.table();
  html.row2(tr("Segment type:"), map::airwayTypeToString(airway.type));

  map::MapWaypoint from = mapQuery->getWaypointById(airway.fromWaypointId);
  map::MapWaypoint to = mapQuery->getWaypointById(airway.toWaypointId);

  // Show from/to waypoints if one-way and include links ==================================
  HtmlBuilder tempHtml(true);

  if(airway.direction == map::DIR_BACKWARD)
    // Reverse if one-way is backward
    std::swap(from, to);

  QString connector(airway.direction == map::DIR_BOTH ? tr(", ") : tr(" ► "));
  if(info)
  {
    tempHtml.a(tr("%1/%2").arg(from.ident).arg(from.region), QString("lnm://show?lonx=%1&laty=%2").
               arg(from.position.getLonX()).arg(from.position.getLatY()), atools::util::html::LINK_NO_UL);
    tempHtml.text(connector);
    tempHtml.a(tr("%1/%2").arg(to.ident).arg(to.region), QString("lnm://show?lonx=%1&laty=%2").
               arg(to.position.getLonX()).arg(to.position.getLatY()), atools::util::html::LINK_NO_UL);
  }
  else
    tempHtml.text(tr("%1/%2%3%4/%5").arg(from.ident).arg(from.region).arg(connector).arg(to.ident).arg(to.region));

  if(airway.direction != map::DIR_BOTH)
    html.row2(tr("Segment One-way:"), tempHtml.getHtml(), atools::util::html::NO_ENTITIES);
  else
    html.row2(tr("Segment:"), tempHtml.getHtml(), atools::util::html::NO_ENTITIES);

  QString altTxt = map::airwayAltText(airway);

  html.row2If(tr("Altitude for this segment:"), altTxt);

  html.row2(tr("Segment length:"), Unit::distMeter(airway.from.distanceMeterTo(airway.to)));

  if(infoQuery != nullptr && info)
  {
    // Show list of waypoints =================================================================
    atools::sql::SqlRecordVector waypoints =
      infoQuery->getAirwayWaypointInformation(airway.name, airway.fragment);

    if(!waypoints.isEmpty())
    {
      HtmlBuilder tempLinkHtml(true);
      for(const SqlRecord& wprec : waypoints)
      {
        if(!tempLinkHtml.isEmpty())
          tempLinkHtml.text(", ");
        tempLinkHtml.a(tr("%1/%2").
                       arg(wprec.valueStr("from_ident")).
                       arg(wprec.valueStr("from_region")),
                       QString("lnm://show?lonx=%1&laty=%2").
                       arg(wprec.valueFloat("from_lonx")).
                       arg(wprec.valueFloat("from_laty")), atools::util::html::LINK_NO_UL);
      }
      tempLinkHtml.text(", ");
      tempLinkHtml.a(tr("%1/%2").
                     arg(waypoints.last().valueStr("to_ident")).
                     arg(waypoints.last().valueStr("to_region")),
                     QString("lnm://show?lonx=%1&laty=%2").
                     arg(waypoints.last().valueFloat("to_lonx")).
                     arg(waypoints.last().valueFloat("to_laty")), atools::util::html::LINK_NO_UL);

      html.row2(tr("Waypoints Ident/Region:"), tempLinkHtml.getHtml(), atools::util::html::NO_ENTITIES);
    }
  }
  html.tableEnd();

#ifdef DEBUG_INFORMATION
  html.p().small(QString("Database: airway_id = %1").arg(airway.getId())).pEnd();
#endif
}

void HtmlInfoBuilder::markerText(const MapMarker& marker, HtmlBuilder& html) const
{
  if(marker.ident.isEmpty())
    head(html, tr("Marker: %1").arg(atools::capString(marker.type)));
  else
    head(html, tr("Marker: %1 (%2)").arg(marker.ident).arg(atools::capString(marker.type)));
}

void HtmlInfoBuilder::towerText(const MapAirport& airport, HtmlBuilder& html) const
{
  if(airport.towerFrequency > 0)
  {
    head(html, tr("Tower:"));
    html.br();
    head(html, locale.toString(airport.towerFrequency / 1000., 'f', 3) + tr(" MHz"));
  }
  else
    head(html, tr("Tower"));
}

void HtmlInfoBuilder::parkingText(const MapParking& parking, HtmlBuilder& html) const
{
  head(html, map::parkingName(parking.name) +
       (parking.number != -1 ? " " + locale.toString(parking.number) : QString()));
  html.brText(map::parkingTypeName(parking.type));
  html.brText(Unit::distShortFeet(parking.radius * 2.f));
  if(parking.jetway)
    html.brText(tr("Has Jetway"));
  if(!parking.airlineCodes.isEmpty())
    html.brText(tr("Airline Codes: ") + parking.airlineCodes);
}

void HtmlInfoBuilder::userpointTextRoute(const MapUserpointRoute& userpoint, HtmlBuilder& html) const
{
  head(html, tr("Position: ") + userpoint.name);
  if(userpoint.routeIndex >= 0)
    html.p().b(tr("Flight Plan position: ") + QString::number(userpoint.routeIndex + 1)).pEnd();
}

void HtmlInfoBuilder::procedurePointText(const proc::MapProcedurePoint& ap, HtmlBuilder& html) const
{
  QString header = proc::procedureTypeText(ap.mapType);

  head(html, header);

  QStringList atts;

  if(ap.flyover)
    atts += tr("Fly over");

  html.table();

  html.row2(tr("Leg Type:"), proc::procedureLegTypeStr(ap.type));
  html.row2(tr("Fix:"), ap.fixIdent);

  if(!atts.isEmpty())
    html.row2(atts.join(", "));

  if(!ap.remarks.isEmpty())
    html.row2(ap.remarks.join(", "));

  if(ap.altRestriction.isValid())
    html.row2(tr("Altitude Restriction:"), proc::altRestrictionText(ap.altRestriction));

  if(ap.speedRestriction.isValid())
    html.row2(tr("Speed Restriction:"), proc::speedRestrictionText(ap.speedRestriction));

  if(ap.calculatedDistance > 0.f)
    html.row2(tr("Distance:"), Unit::distNm(ap.calculatedDistance /*, true, 20, true*/));
  if(ap.time > 0.f)
    html.row2(tr("Time:"), QLocale().toString(ap.time, 'f', 0) + tr(" min"));
  if(ap.calculatedTrueCourse < map::INVALID_COURSE_VALUE)
    html.row2(tr("Course:"), QLocale().toString(normalizeCourse(ap.calculatedTrueCourse - ap.magvar), 'f', 0) +
              tr("°M"));

  if(!ap.turnDirection.isEmpty())
  {
    if(ap.turnDirection == "L")
      html.row2(tr("Turn:"), tr("Left"));
    else if(ap.turnDirection == "R")
      html.row2(tr("Turn:"), tr("Right"));
    else if(ap.turnDirection == "B")
      html.row2(tr("Turn:"), tr("Left or right"));
  }

  if(!ap.recFixIdent.isEmpty())
  {
    if(ap.rho > 0.f)
      html.row2(tr("Related Navaid:"),
                tr("%1 / %2 / %3").arg(ap.recFixIdent).
                arg(Unit::distNm(ap.rho /*, true, 20, true*/)).
                arg(QLocale().toString(ap.theta) + tr("°M")));
    else
      html.row2(tr("Related Navaid:"), tr("%1").arg(ap.recFixIdent));
  }

  html.tableEnd();
}

void HtmlInfoBuilder::aircraftText(const atools::fs::sc::SimConnectAircraft& aircraft, HtmlBuilder& html, int num,
                                   int total)
{
  if(!aircraft.getPosition().isValid())
    return;

  aircraftTitle(aircraft, html);

  html.nbsp().nbsp();
  QString aircraftText;
  if(aircraft.isUser())
  {
    aircraftText = tr("User Aircraft");
    if(info && !(NavApp::getShownMapFeatures() & map::AIRCRAFT))
      html.p(tr("User aircraft is not shown on map."), atools::util::html::BOLD);
  }
  else
  {
    QString type(tr(" Unknown"));
    switch(aircraft.getCategory())
    {
      case atools::fs::sc::AIRPLANE:
        type = tr(" Aircraft");
        break;
      case atools::fs::sc::HELICOPTER:
        type = tr(" Helicopter");
        break;
      case atools::fs::sc::BOAT:
        type = tr(" Ship");
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
      html.p(tr("No %2 shown on map.").arg(typeText), atools::util::html::BOLD);
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
    html.row2(tr("Model:"), aircraft.getAirplaneModel());

  if(!aircraft.getAirplaneRegistration().isEmpty())
    html.row2(tr("Registration:"), aircraft.getAirplaneRegistration());

  QString type = airplaneType(aircraft);
  if(!type.isEmpty())
    html.row2(tr("Type:"), type);

  if(aircraft.getCategory() == atools::fs::sc::BOAT)
  {
    if(info && aircraft.getModelRadius() > 0)
      html.row2(tr("Size:"), Unit::distShortFeet(aircraft.getModelRadius() * 2));
  }
  else if(info && aircraft.getWingSpan() > 0)
    html.row2(tr("Wingspan:"), Unit::distShortFeet(aircraft.getWingSpan()));
  html.tableEnd();

#ifdef DEBUG_INFORMATION
  html.p().small(QString("Object ID: %1").arg(aircraft.getId())).pEnd();
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
    html.row2If(tr("Visual Range:"), Unit::distNm(onlineRec.valueInt("visual_range")));

    float qnh = onlineRec.valueFloat("qnh_mb");
    if(qnh > 0.f)
      html.row2(tr("Sea Level Pressure:"), locale.toString(qnh, 'f', 0) + tr(" hPa, ") +
                locale.toString(atools::geo::mbarToInHg(qnh), 'f', 2) + tr(" inHg"));

    if(onlineRec.isNull("administrative_rating") || onlineRec.isNull("atc_pilot_rating"))
      html.row2If(tr("Combined Rating:"), onlineRec.valueStr("combined_rating"));

    if(!onlineRec.isNull("administrative_rating"))
      html.row2If(tr("Aministrative Rating:"),
                  atools::fs::online::admRatingText(onlineRec.valueInt("administrative_rating")));
    if(!onlineRec.isNull("atc_pilot_rating"))
      html.row2If(tr("ATC Rating:"), atools::fs::online::pilotRatingText(onlineRec.valueInt("atc_pilot_rating")));

    html.row2If(tr("Connection Time:"), locale.toString(onlineRec.valueDateTime("connection_time")));

    if(!onlineRec.valueStr("software_name").isEmpty())
      html.row2If(tr("Software:"), tr("%1 %2").
                  arg(onlineRec.valueStr("software_name")).
                  arg(onlineRec.valueStr("software_version")));
    html.tableEnd();

    // Always unknown
    // html.row2If(tr("Simulator:"), atools::fs::online::simulatorText(onlineRec.valueInt("simulator")));

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
    html.row2If(tr("Visual Range"), Unit::distNm(onlineRec.valueFloat("visual_range")));
    html.row2If(tr("Flight Rules:"), onlineRec.valueStr("flightplan_flight_rules"));
    html.row2If(tr("Type of Flight:"), onlineRec.valueStr("flightplan_type_of_flight"));
    html.row2If(tr("Departure Time:"), onlineRec.valueStr("flightplan_departure_time"));
    html.row2If(tr("Actual Departure Time:"), onlineRec.valueStr("flightplan_actual_departure_time"));

    // Display times
    double enrouteMin = onlineRec.valueDouble("flightplan_enroute_minutes");
    if(enrouteMin > 0.)
      html.row2(tr("Enroute hh:mm"), formatter::formatMinutesHours(enrouteMin / 60.));
    double enduranceMin = onlineRec.valueDouble("flightplan_endurance_minutes");
    if(enduranceMin > 0.)
      html.row2If(tr("Endurance hh:mm"), formatter::formatMinutesHours(enduranceMin / 60.));

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
      html.row2(tr("Coordinates:"), Unit::coords(aircraft.getPosition()));
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
    html.table();
    html.row2(tr("Max Gross Weight:"), Unit::weightLbs(userAircraft.getAirplaneMaxGrossWeightLbs()));
    html.row2(tr("Gross Weight:"), Unit::weightLbs(userAircraft.getAirplaneTotalWeightLbs()));

    html.row2(QString());
    html.row2(tr("Empty Weight:"), Unit::weightLbs(userAircraft.getAirplaneEmptyWeightLbs()));
    html.row2(tr("Zero Fuel Weight:"), Unit::weightLbs(userAircraft.getAirplaneTotalWeightLbs() -
                                                       userAircraft.getFuelTotalWeightLbs()));

    html.row2(tr("Total Payload:"), Unit::weightLbs(userAircraft.getAirplaneTotalWeightLbs() -
                                                    userAircraft.getAirplaneEmptyWeightLbs() -
                                                    userAircraft.getFuelTotalWeightLbs()));

    html.row2(tr("Fuel:"), Unit::weightLbs(userAircraft.getFuelTotalWeightLbs()) + ", " +
              Unit::volGallon(userAircraft.getFuelTotalQuantityGallons()));
    html.tableEnd();
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
                                           HtmlBuilder& html, const Route& route)
{
  if(!aircraft.getPosition().isValid())
    return;

  const SimConnectUserAircraft *userAircaft = dynamic_cast<const SimConnectUserAircraft *>(&aircraft);

  if(info && userAircaft != nullptr)
  {
    aircraftTitle(aircraft, html);
    if(!(NavApp::getShownMapFeatures() & map::AIRCRAFT))
      html.p(tr("User aircraft is not shown on map."), atools::util::html::BOLD);
  }

  float distFromStartNm = 0.f, distToDestNm = 0.f, nearestLegDistance = 0.f, crossTrackDistance = 0.f;
  float neededFuelWeight = INVALID_WEIGHT_VALUE, neededFuelVol = INVALID_VOLUME_VALUE;

  float toTod = map::INVALID_DISTANCE_VALUE;
  if(!route.isEmpty() && userAircaft != nullptr && info)
  {
    // The corrected leg will point to an approach leg if we head to the start of a procedure
    bool isCorrected = false;
    int activeLegCorrected = route.getActiveLegIndexCorrected(&isCorrected);
    int activeLeg = route.getActiveLegIndex();

    if(activeLegCorrected != map::INVALID_INDEX_VALUE &&
       route.getRouteDistances(&distFromStartNm, &distToDestNm, &nearestLegDistance, &crossTrackDistance))
    {
      if(distToDestNm > 0.01f && userAircaft->getFuelFlowPPH() > 0.01f &&
         userAircaft->getGroundSpeedKts() > MIN_GROUND_SPEED)
      {
        neededFuelWeight = distToDestNm / aircraft.getGroundSpeedKts() * userAircaft->getFuelFlowPPH();
        neededFuelVol = distToDestNm / aircraft.getGroundSpeedKts() * userAircaft->getFuelFlowGPH();
      }

      html.table();
      dateAndTime(userAircaft, html);
      html.tableEnd();

      // Route distances ===============================================================
      if(distToDestNm < map::INVALID_DISTANCE_VALUE)
      {
        head(html, tr("Destination"));
        html.table();

        bool timeAvailable = aircraft.getGroundSpeedKts() > MIN_GROUND_SPEED;
        float timeToDestination = 0.f;
        if(timeAvailable)
          timeToDestination = distToDestNm / aircraft.getGroundSpeedKts();

        QString destinationText = timeAvailable ? tr("Distance and Time:") : tr("Distance:");

        if(route.isActiveMissed())
          destinationText = tr("To End of Missed Approach:");

        html.row2(destinationText, Unit::distNm(distToDestNm) +
                  (timeAvailable ? tr(", ") + formatter::formatMinutesHoursLong(timeToDestination) : QString()));

        if(timeAvailable)
        {
          QDateTime arrival = userAircaft->getZuluTime().addSecs(static_cast<int>(timeToDestination * 3600.f));
          html.row2(tr("Arrival Time:"), locale.toString(arrival.time(), QLocale::ShortFormat) + " " +
                    arrival.timeZoneAbbreviation());

          if(userAircaft->getGroundSpeedKts() < atools::fs::sc::SC_INVALID_FLOAT)
          {
            if(distToDestNm > 0.01f && userAircaft->getFuelFlowPPH() > 0.01f &&
               userAircaft->getGroundSpeedKts() > MIN_GROUND_SPEED &&
               neededFuelVol < INVALID_VOLUME_VALUE && neededFuelWeight < INVALID_WEIGHT_VALUE)
            {
              html.row2(tr("Gross Weight:"),
                        Unit::weightLbs(userAircaft->getAirplaneTotalWeightLbs() - neededFuelWeight));
              html.row2(tr("Fuel:"),
                        Unit::weightLbs(userAircaft->getFuelTotalWeightLbs() - neededFuelWeight) + tr(", ") +
                        Unit::volGallon(userAircaft->getFuelTotalQuantityGallons() - neededFuelVol));
            }
          }

        }
        html.tableEnd();
      }

      if(route.size() > 1)
      {
        head(html, tr("Top of Descent"));
        html.table();
        // if(OptionData::instance().getFlags() & opts::FLIGHT_PLAN_SHOW_TOD)
        // Top of descent  ===============================================================
        html.row2(tr("To Destination:"), Unit::distNm(route.getTopOfDescentFromDestination()));

        if(distFromStartNm < map::INVALID_DISTANCE_VALUE)
          toTod = route.getTopOfDescentFromStart() - distFromStartNm;

        if(toTod > 0 && toTod < map::INVALID_DISTANCE_VALUE)
        {
          QStringList todTexts;
          todTexts.append(Unit::distNm(toTod));

          float fuelAtTod = INVALID_WEIGHT_VALUE, fuelVolAtTod = INVALID_VOLUME_VALUE;
          if(aircraft.getGroundSpeedKts() > MIN_GROUND_SPEED &&
             aircraft.getGroundSpeedKts() < atools::fs::sc::SC_INVALID_FLOAT)
          {
            float timeToTod = toTod / aircraft.getGroundSpeedKts();
            todTexts.append(formatter::formatMinutesHoursLong(timeToTod));

            if(userAircaft != nullptr)
            {
              fuelAtTod = userAircaft->getFuelTotalWeightLbs() - userAircaft->getFuelFlowPPH() * timeToTod;
              fuelVolAtTod = userAircaft->getFuelTotalQuantityGallons() - userAircaft->getFuelFlowGPH() * timeToTod;
            }
          }

          if(todTexts.size() == 1)
            html.row2(tr("Distance:"), todTexts.first());
          else
            html.row2(tr("Distance and Time:"), todTexts.join(tr(", ")));

          if(fuelAtTod < INVALID_WEIGHT_VALUE && fuelVolAtTod < INVALID_VOLUME_VALUE)
            html.row2(tr("Fuel:"),
                      Unit::weightLbs(fuelAtTod) + tr(", ") + Unit::volGallon(fuelVolAtTod));
        }
        html.tableEnd();
      }

      // Next leg ====================================================
      QString apprText;
      if(activeLegCorrected != map::INVALID_INDEX_VALUE)
      {
        const RouteLeg& routeLeg = route.at(activeLegCorrected);

        if(routeLeg.getProcedureLeg().isApproach())
          apprText = tr(" - Approach");
        else if(routeLeg.getProcedureLeg().isTransition())
          apprText = tr(" - Transition");
        else if(routeLeg.getProcedureLeg().isMissed())
          apprText = tr(" - Missed Approach");
      }

      head(html, tr("Next Waypoint") + apprText);
      html.table();

      if(activeLegCorrected != map::INVALID_INDEX_VALUE)
      {
        // If approaching an initial fix use corrected version
        const RouteLeg& routeLegCorrected = route.at(activeLegCorrected);

        // For course and distance use not corrected leg
        const RouteLeg& routeLeg = activeLeg != map::INVALID_INDEX_VALUE && isCorrected ?
                                   route.at(activeLeg) : routeLegCorrected;

        const proc::MapProcedureLeg& leg = routeLegCorrected.getProcedureLeg();

        // Next leg - approach data ====================================================
        if(routeLegCorrected.isAnyProcedure())
        {
          html.row2(tr("Leg Type:"), proc::procedureLegTypeStr(leg.type));

          QStringList instructions;
          if(leg.flyover)
            instructions += tr("Fly over");

          if(!leg.turnDirection.isEmpty())
          {
            if(leg.turnDirection == "L")
              instructions += tr("Turn Left");
            else if(leg.turnDirection == "R")
              instructions += tr("Turn Right");
            else if(leg.turnDirection == "B")
              instructions += tr("Turn Left or right");
          }
          if(!instructions.isEmpty())
            html.row2(tr("Instructions:"), instructions.join(", "));
        }

        // Next leg - waypoint data ====================================================
        if(!routeLegCorrected.getIdent().isEmpty())
          html.row2(tr("Name and Type:"), routeLegCorrected.getIdent() +
                    (routeLegCorrected.getMapObjectTypeName().isEmpty() ? QString() : tr(", ") +
                     routeLegCorrected.getMapObjectTypeName()));

        // Next leg - approach related navaid ====================================================
        if(!leg.recFixIdent.isEmpty())
        {
          if(leg.rho > 0.f)
            html.row2(tr("Related Navaid:"), tr("%1, %2, %3").arg(leg.recFixIdent).
                      arg(Unit::distNm(leg.rho /*, true, 20, true*/)).
                      arg(QLocale().toString(leg.theta, 'f', 0) + "°M"));
          else
            html.row2(tr("Related Navaid:"), tr("%1").arg(leg.recFixIdent));
        }

        if(routeLegCorrected.isAnyProcedure())
        {
          QStringList restrictions;

          if(leg.altRestriction.isValid())
            restrictions.append(proc::altRestrictionText(leg.altRestriction));
          if(leg.speedRestriction.isValid())
            restrictions.append(proc::speedRestrictionText(leg.speedRestriction));

          if(restrictions.size() > 1)
            html.row2(tr("Restrictions:"), restrictions.join(tr(", ")));
          else if(restrictions.size() == 1)
            html.row2(tr("Restriction:"), restrictions.first());
        }

        if(nearestLegDistance < map::INVALID_DISTANCE_VALUE)
        {
          QString timeStr;
          if(aircraft.getGroundSpeedKts() > MIN_GROUND_SPEED &&
             aircraft.getGroundSpeedKts() < atools::fs::sc::SC_INVALID_FLOAT)
            timeStr = formatter::formatMinutesHoursLong(nearestLegDistance / aircraft.getGroundSpeedKts());

          // Not for arc legs
          if((routeLeg.isRoute() || !leg.isCircular()) && routeLeg.getPosition().isValid())
          {
            float crs = normalizeCourse(aircraft.getPosition().angleDegToRhumb(
                                          routeLeg.getPosition()) - routeLeg.getMagvar());
            html.row2(tr("Distance, Course and Time:"), Unit::distNm(nearestLegDistance) + ", " +
                      locale.toString(crs, 'f', 0) + tr("°M, ") + timeStr);
          }
          else // if(!leg.noDistanceDisplay())
               // Only distance and time for arc legs
            html.row2(tr("Distance and Time:"), Unit::distNm(nearestLegDistance) + ", " + timeStr);
        }

        // No cross track and course for holds
        if(route.size() > 1)
        {
          // No course for arcs
          if(routeLeg.isRoute() || !routeLeg.getProcedureLeg().isCircular())
            html.row2(tr("Leg Course:"), locale.toString(routeLeg.getCourseToRhumbMag(), 'f', 0) + tr("°M"));

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

              html.row2(tr("Cross Track Distance:"),
                        Unit::distNm(std::abs(
                                       crossTrackDistance)) + " " + crossDirection, atools::util::html::NO_ENTITIES);
            }
            else
              html.row2(tr("Cross Track Distance:"), tr("Not along Track"));
          }
        }
      }
      else
        qWarning() << "Invalid route leg index" << activeLegCorrected;

      html.tableEnd();
    }
    else
    {
      head(html, tr("No Active Flight Plan Leg"));
      html.table();
      dateAndTime(userAircaft, html);
      html.tableEnd();
    }
  }
  else if(info && userAircaft != nullptr)
  {
    head(html, tr("No Flight Plan"));
    html.table();
    dateAndTime(userAircaft, html);
    html.tableEnd();
  }

  // Add departure and destination for AI ==================================================
  if(userAircaft == nullptr && (!aircraft.getFromIdent().isEmpty() || !aircraft.getToIdent().isEmpty()))
  {
    if(info && userAircaft != nullptr)
      head(html, tr("Flight Plan"));

    if(!aircraft.getFromIdent().isEmpty())
    {
      html.p();
      html.b(tr("Departure: "));
      if(info)
        html.a(aircraft.getFromIdent(), QString("lnm://show?airport=%1").arg(aircraft.getFromIdent()),
               atools::util::html::LINK_NO_UL);
      else
        html.text(aircraft.getFromIdent());
      html.text(tr(". "));
    }

    if(!aircraft.getToIdent().isEmpty())
    {
      html.b(tr("Destination: "));
      if(info)
        html.a(aircraft.getToIdent(), QString("lnm://show?airport=%1").arg(aircraft.getToIdent()),
               atools::util::html::LINK_NO_UL);
      else
        html.text(aircraft.getToIdent());
      html.text(tr("."));
      html.pEnd();
    }
  }

  if(info && userAircaft != nullptr)
    head(html, tr("Aircraft"));
  html.table();

  QStringList hdg;
  if(aircraft.getHeadingDegMag() < atools::fs::sc::SC_INVALID_FLOAT)
    hdg.append(locale.toString(aircraft.getHeadingDegMag(), 'f', 0) + tr("°M"));
  if(aircraft.getHeadingDegTrue() < atools::fs::sc::SC_INVALID_FLOAT)
    hdg.append(locale.toString(aircraft.getHeadingDegTrue(), 'f', 0) + tr("°T"));

  if(!hdg.isEmpty())
    html.row2(tr("Heading:"), hdg.join(", "));

  if(userAircaft != nullptr && info)
  {
    if(userAircaft != nullptr)
      html.row2(tr("Track:"), locale.toString(userAircaft->getTrackDegMag(), 'f', 0) + tr("°M, ") +
                locale.toString(userAircaft->getTrackDegTrue(), 'f', 0) + tr("°T"));

    html.row2(tr("Fuel Flow:"), Unit::ffLbs(userAircaft->getFuelFlowPPH()) + ", " +
              Unit::ffGallon(userAircaft->getFuelFlowGPH()));

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

    QString ice;

    if(userAircaft->getPitotIcePercent() >= 1.f)
      ice += tr("Pitot ") + locale.toString(userAircaft->getPitotIcePercent(), 'f', 0) + tr(" %");
    if(userAircaft->getStructuralIcePercent() >= 1.f)
    {
      if(!ice.isEmpty())
        ice += tr(", ");
      ice += tr("Structure ") + locale.toString(userAircaft->getStructuralIcePercent(), 'f', 0) + tr(" %");
    }
    if(ice.isEmpty())
      ice = tr("None");

    html.row2(tr("Ice:"), ice);
  }
  html.tableEnd();

  // Display more text for information display if not online aircraft
  bool longDisplay = info && !aircraft.isOnline();

  if(longDisplay)
    head(html, tr("Altitude"));
  html.table();

  if(aircraft.getCategory() != atools::fs::sc::BOAT)
  {
    if(longDisplay && aircraft.getIndicatedAltitudeFt() < atools::fs::sc::SC_INVALID_FLOAT)
      html.row2(tr("Indicated:"), Unit::altFeet(aircraft.getIndicatedAltitudeFt()));
  }
  html.row2(longDisplay ? tr("Actual:") : tr("Altitude:"), Unit::altFeet(aircraft.getPosition().getAltitude()));

  if(userAircaft != nullptr && longDisplay && aircraft.getCategory() != atools::fs::sc::BOAT)
  {
    if(userAircaft->getAltitudeAboveGroundFt() < atools::fs::sc::SC_INVALID_FLOAT)
      html.row2(tr("Above Ground:"), Unit::altFeet(userAircaft->getAltitudeAboveGroundFt()));
    if(userAircaft->getGroundAltitudeFt() < atools::fs::sc::SC_INVALID_FLOAT)
      html.row2(tr("Ground Elevation:"), Unit::altFeet(userAircaft->getGroundAltitudeFt()));
  }

  if(toTod <= 0 && userAircaft != nullptr /*&& OptionData::instance().getFlags() & opts::FLIGHT_PLAN_SHOW_TOD*/)
  {
    // Display vertical path deviation when after TOD
    float vertAlt = route.getDescentVerticalAltitude(distToDestNm);

    if(vertAlt < map::INVALID_ALTITUDE_VALUE)
    {
      float diff = aircraft.getPosition().getAltitude() - vertAlt;
      QString upDown;
      if(diff >= 100.f)
        upDown = tr(", above <b>▼</b>");
      else if(diff <= -100)
        upDown = tr(", below <b>▲</b>");

      html.row2(tr("Vertical Path Dev.:"), Unit::altFeet(diff) + upDown, atools::util::html::NO_ENTITIES);
    }
  }
  html.tableEnd();

  if(aircraft.getIndicatedSpeedKts() < atools::fs::sc::SC_INVALID_FLOAT ||
     aircraft.getGroundSpeedKts() < atools::fs::sc::SC_INVALID_FLOAT ||
     aircraft.getTrueAirspeedKts() < atools::fs::sc::SC_INVALID_FLOAT ||
     aircraft.getVerticalSpeedFeetPerMin() < atools::fs::sc::SC_INVALID_FLOAT)
  {

    if(longDisplay)
      head(html, tr("Speed"));
    html.table();
    if(longDisplay && aircraft.getCategory() != atools::fs::sc::BOAT &&
       aircraft.getIndicatedSpeedKts() < atools::fs::sc::SC_INVALID_FLOAT)
      html.row2(tr("Indicated:"), Unit::speedKts(aircraft.getIndicatedSpeedKts()));

    if(aircraft.getGroundSpeedKts() < atools::fs::sc::SC_INVALID_FLOAT)
      html.row2(longDisplay ? tr("Ground:") : tr("Groundspeed:"), Unit::speedKts(aircraft.getGroundSpeedKts()));

    if(longDisplay && aircraft.getCategory() != atools::fs::sc::BOAT)
      if(aircraft.getTrueAirspeedKts() < atools::fs::sc::SC_INVALID_FLOAT)
        html.row2(tr("True Airspeed:"), Unit::speedKts(aircraft.getTrueAirspeedKts()));

    if(aircraft.getCategory() != atools::fs::sc::BOAT)
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
                  atools::util::html::NO_ENTITIES);
      }
    }
    html.tableEnd();
  }

  if(userAircaft != nullptr && longDisplay)
  {
    head(html, tr("Environment"));
    html.table();
    float windSpeed = userAircaft->getWindSpeedKts();
    float windDir = normalizeCourse(userAircaft->getWindDirectionDegT() - userAircaft->getMagVarDeg());
    if(windSpeed >= 1.f)
      html.row2(tr("Wind Direction and Speed:"), locale.toString(windDir, 'f', 0) + tr("°M, ") +
                Unit::speedKts(windSpeed));
    else
      html.row2(tr("Wind Direction and Speed:"), tr("None"));

    float diffRad = atools::geo::toRadians(windDir - userAircaft->getHeadingDegMag());
    float headWind = windSpeed * std::cos(diffRad);
    float crossWind = windSpeed * std::sin(diffRad);

    QString value;
    if(std::abs(headWind) >= 1.0f)
    {
      value += Unit::speedKts(std::abs(headWind));

      if(headWind <= -1.f)
        value += tr("▲"); // Tailwind
      else
        value += tr("▼"); // Headwind
    }

    if(std::abs(crossWind) >= 1.0f)
    {
      if(!value.isEmpty())
        value += tr(", ");

      value += Unit::speedKts(std::abs(crossWind));

      if(crossWind >= 1.f)
        value += tr("◄");
      else if(crossWind <= -1.f)
        value += tr("►");
    }

    // if(!value.isEmpty())
    // Keep an empty line to avoid flickering
    html.row2(QString(), value, atools::util::html::NO_ENTITIES);

    // Total air temperature (TAT) is also called: indicated air temperature (IAT) or ram air temperature (RAT)
    float tat = userAircaft->getTotalAirTemperatureCelsius();
    if(tat < 0.f && tat > -0.5f)
      tat = 0.f;
    html.row2(tr("Total Air Temperature:"), locale.toString(tat, 'f', 0) + tr("°C, ") +
              locale.toString(atools::geo::degCToDegF(tat), 'f', 0) + tr("°F"));

    // Static air temperature (SAT) is also called: outside air temperature (OAT) or true air temperature
    float sat = userAircaft->getAmbientTemperatureCelsius();
    if(sat < 0.f && sat > -0.5f)
      sat = 0.f;
    html.row2(tr("Static Air Temperature:"), locale.toString(sat, 'f', 0) + tr("°C, ") +
              locale.toString(atools::geo::degCToDegF(sat), 'f', 0) + tr("°F"));

    float isaDeviation = sat - atools::geo::isaTemperature(userAircaft->getPosition().getAltitude());
    if(isaDeviation < 0.f && isaDeviation > -0.5f)
      isaDeviation = 0.f;
    html.row2(tr("ISA Deviation:"), locale.toString(isaDeviation, 'f', 0) + tr("°C"));

    float slp = userAircaft->getSeaLevelPressureMbar();
    html.row2(tr("Sea Level Pressure:"), locale.toString(slp, 'f', 0) + tr(" hPa, ") +
              locale.toString(atools::geo::mbarToInHg(slp), 'f', 2) + tr(" inHg"));

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

    html.tableEnd();
  }

  if(longDisplay)
  {
    head(html, tr("Position"));
    html.row2(tr("Coordinates:"), Unit::coords(aircraft.getPosition()));
    html.tableEnd();
  }
}

void HtmlInfoBuilder::aircraftTitle(const atools::fs::sc::SimConnectAircraft& aircraft, HtmlBuilder& html)
{
  // const QString *iconFile;

  // updateAircraftIcons(false);

  // if(aircraft.isUser())
  // iconFile = aircraft.isOnGround() ? &aircraftGroundEncodedIcon : &aircraftEncodedIcon;
  // else if(aircraft.getCategory() == atools::fs::sc::BOAT)
  // iconFile = aircraft.isOnGround() ? &boatAiGroundEncodedIcon : &boatAiEncodedIcon;
  // else
  // iconFile = aircraft.isOnGround() ? &aircraftAiGroundEncodedIcon : &aircraftAiEncodedIcon;

  QIcon icon = NavApp::getVehicleIcons()->iconFromCache(aircraft, SYMBOL_SIZE_VEHICLE.height(), 45);

  if(aircraft.isUser())
    html.img(icon, tr("User Vehicle"), QString(), SYMBOL_SIZE_VEHICLE);
  else if(aircraft.isOnline())
    html.img(icon, tr("Online Client (%1)").arg(NavApp::getOnlineNetwork()), QString(), SYMBOL_SIZE_VEHICLE);
  else
    html.img(icon, tr("AI / Multiplayer Vehicle"), QString(), SYMBOL_SIZE_VEHICLE);
  html.nbsp().nbsp();

  QString title(aircraft.getAirplaneRegistration());
  QString title2 = airplaneType(aircraft);
  QString title3 = aircraft.isOnline() ? NavApp::getOnlineNetwork() : QString();

  if(!aircraft.getAirplaneModel().isEmpty())
    title2 += (title2.isEmpty() ? "" : ", ") + aircraft.getAirplaneModel();

  if(!title2.isEmpty())
    title += " - " + title2;

  if(!title3.isEmpty())
    title += " (" + title3 + ")";

  html.text(title, atools::util::html::BOLD | atools::util::html::BIG);

  if(info)
  {
    html.nbsp().nbsp();
    html.a(tr("Map"), QString("lnm://show?lonx=%1&laty=%2").
           arg(aircraft.getPosition().getLonX()).arg(aircraft.getPosition().getLatY()),
           atools::util::html::LINK_NO_UL);
  }
}

QString HtmlInfoBuilder::airplaneType(const atools::fs::sc::SimConnectAircraft& aircraft) const
{
  if(!aircraft.getAirplaneType().isEmpty())
    return aircraft.getAirplaneType();
  else
    // Convert model ICAO code to a name
    return atools::fs::util::aircraftTypeForCode(aircraft.getAirplaneModel());
}

void HtmlInfoBuilder::addScenery(const atools::sql::SqlRecord *rec, HtmlBuilder& html) const
{
  head(html, tr("Scenery"));
  html.table();

  html.row2(rec->valueStr("title"), filepathText(rec->valueStr("filepath")),
            atools::util::html::NO_ENTITIES | atools::util::html::SMALL);
  html.tableEnd();
}

void HtmlInfoBuilder::addAirportScenery(const MapAirport& airport, HtmlBuilder& html) const
{
  head(html, tr("Scenery"));
  html.table();
  const atools::sql::SqlRecordVector *sceneryInfo =
    infoQuery->getAirportSceneryInformation(airport.ident);

  if(sceneryInfo != nullptr)
  {
    for(const SqlRecord& rec : *sceneryInfo)
      html.row2(rec.valueStr("title"), filepathText(rec.valueStr("filepath")),
                atools::util::html::NO_ENTITIES | atools::util::html::SMALL);
  }

  html.tableEnd();
}

QString HtmlInfoBuilder::filepathText(const QString& filepath) const
{
  HtmlBuilder link(true);

  if(QFileInfo::exists(filepath))
    link.a(filepath, QString("lnm://show?filepath=%1").arg(filepath), atools::util::html::LINK_NO_UL);
  else
    link.text(filepath);
  return link.getHtml();
}

void HtmlInfoBuilder::addCoordinates(const atools::sql::SqlRecord *rec, HtmlBuilder& html) const
{
  if(rec != nullptr)
    addCoordinates(atools::geo::Pos(rec->valueFloat("lonx"), rec->valueFloat("laty"), rec->valueFloat("altitude", 0.f)),
                   html);
}

void HtmlInfoBuilder::addCoordinates(const atools::geo::Pos& pos, HtmlBuilder& html) const
{
  html.row2(tr("Coordinates:"), Unit::coords(pos));

#ifdef DEBUG_INFORMATION
  html.row2(tr("Pos:"), QString("Pos(%1, %2)").arg(pos.getLonX()).arg(pos.getLatY()));
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
    html.text(text, atools::util::html::BOLD | atools::util::html::BIG | atools::util::html::UNDERLINE);
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

void HtmlInfoBuilder::addMetarLine(atools::util::HtmlBuilder& html, const QString& heading,
                                   const QString& metar, const QString& station,
                                   const QDateTime& timestamp, bool fsMetar) const
{
  if(!metar.isEmpty())
  {
    Metar m(metar, station, timestamp, fsMetar);
    const atools::fs::weather::MetarParser& pm = m.getParsedMetar();

    if(!pm.isValid())
      qWarning() << "Metar is not valid";

    // Add METAR suffix for tooltip
    html.row2(heading + (info ? tr(":") : tr(" METAR:")), fsMetar ? m.getCleanMetar() : metar);
  }
}

QString HtmlInfoBuilder::adjustText(const QString& text) const
{
  static QRegularExpression LINEFEED_REGEXP("[\\n\\r\\f]");
  return text.toHtmlEscaped().replace(LINEFEED_REGEXP, "<br/>");
}
