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

#include "maphtmlinfobuilder.h"
#include "symbolpainter.h"
#include "common/maptypes.h"
#include "mapgui/mapquery.h"
#include "common/formatter.h"
#include "route/routemapobjectlist.h"
#include "geo/calculations.h"
#include "fs/bgl/ap/rw/runway.h"
#include <common/htmlbuilder.h>
#include <common/morsecode.h>
#include <common/weatherreporter.h>
#include "atools.h"

#include <QSize>

#include <sql/sqlrecord.h>

#include <info/infoquery.h>

using namespace maptypes;
using atools::sql::SqlRecord;
using atools::sql::SqlRecordVector;

const int SYMBOL_SIZE = 20;

Q_DECLARE_FLAGS(RunwayMarkingFlags, atools::fs::bgl::rw::RunwayMarkings);
Q_DECLARE_OPERATORS_FOR_FLAGS(RunwayMarkingFlags);

MapHtmlInfoBuilder::MapHtmlInfoBuilder(MapQuery *mapDbQuery, InfoQuery *infoDbQuery, bool formatInfo)
  : mapQuery(mapDbQuery), infoQuery(infoDbQuery), info(formatInfo)
{
  morse = new MorseCode("&nbsp;", "&nbsp;&nbsp;&nbsp;");
}

MapHtmlInfoBuilder::~MapHtmlInfoBuilder()
{
  delete morse;
}

void MapHtmlInfoBuilder::airportTitle(const MapAirport& airport, HtmlBuilder& html, QColor background)
{
  html.img(SymbolPainter(background).createAirportIcon(airport, SYMBOL_SIZE),
           QString(), QString(), QSize(SYMBOL_SIZE, SYMBOL_SIZE));
  html.nbsp().nbsp();

  html::Flags titleFlags = html::BOLD;
  if(airport.closed())
    titleFlags |= html::STRIKEOUT;
  if(airport.addon())
    titleFlags |= html::ITALIC;

  if(info)
    html.text(airport.name + " (" + airport.ident + ")", titleFlags | html::BIG);
  else
    html.text(airport.name + " (" + airport.ident + ")", titleFlags);
}

void MapHtmlInfoBuilder::airportText(const MapAirport& airport, HtmlBuilder& html,
                                     const RouteMapObjectList *routeMapObjects, WeatherReporter *weather,
                                     QColor background)
{
  const SqlRecord *rec = nullptr;
  if(info && infoQuery != nullptr)
    rec = infoQuery->getAirportInformation(airport.id);

  airportTitle(airport, html, background);

  QString city, state, country;
  mapQuery->getAirportAdminById(airport.id, city, state, country);

  html.table();
  html.row2("City:", city);
  if(!state.isEmpty())
    html.row2("State or Province:", state);
  html.row2("Country:", country);
  html.row2("Altitude:", locale.toString(airport.getPosition().getAltitude(), 'f', 0) + " ft");
  html.row2("Magvar:", locale.toString(airport.magvar, 'f', 1) + " °");
  if(rec != nullptr)
  {
    html.row2("Rating:", atools::ratingString(rec->valueInt("rating"), 5));
    addCoordinates(rec, html);
  }
  html.tableEnd();

  if(info)
    head(html, "Facilities");
  html.table();
  QStringList facilities;

  if(airport.closed())
    facilities.append("Closed");

  if(airport.addon())
    facilities.append("Add-on");
  if(airport.flags.testFlag(AP_MIL))
    facilities.append("Military");
  if(airport.scenery())
    facilities.append("Scenery");

  if(rec != nullptr)
  {
    if(rec->valueInt("num_apron") > 0)
      facilities.append("Aprons");
    if(rec->valueInt("num_taxi_path") > 0)
      facilities.append("Taxiways");
  }

  if(airport.helipad())
    facilities.append("Helipads");
  if(airport.flags.testFlag(AP_AVGAS))
    facilities.append("Avgas");
  if(airport.flags.testFlag(AP_JETFUEL))
    facilities.append("Jetfuel");

  if(airport.flags.testFlag(AP_APPR))
    facilities.append("Approaches");

  if(airport.flags.testFlag(AP_ILS))
    facilities.append("ILS");
  if(rec != nullptr)
  {
    if(rec->valueInt("num_runway_end_vasi") > 0)
      facilities.append("VASI");
    if(rec->valueInt("num_runway_end_als") > 0)
      facilities.append("ALS");
    if(rec->valueInt("num_boundary_fence") > 0)
      facilities.append("Boundary Fence");
    if(rec->valueBool("has_tower_object"))
      facilities.append("Tower Object");
  }

  if(facilities.isEmpty())
    facilities.append("None");

  html.row2(info ? QString() : "Facilities:", facilities.join(", "));

  html.tableEnd();

  if(!info)
  {
    html.table();
    html.row2("Longest Runway Length:", locale.toString(airport.longestRunwayLength) + " ft");
    html.tableEnd();
  }

  if(routeMapObjects != nullptr)
  {
    if(airport.routeIndex == 0)
      html.brText("Route start airport");
    else if(airport.routeIndex == routeMapObjects->size() - 1)
      html.brText("Route destination airport");
    else if(airport.routeIndex != -1)
      html.brText("Route position " + QString::number(airport.routeIndex + 1));
  }

  if(weather != nullptr)
  {
    QString asnMetar, noaaMetar, vatsimMetar;
    if(weather->hasAsnWeather())
      asnMetar = weather->getAsnMetar(airport.ident);

    if(!weather->hasAsnWeather() || info)
    {
      noaaMetar = weather->getNoaaMetar(airport.ident);

      if(noaaMetar.isEmpty() || info)
        vatsimMetar = weather->getVatsimMetar(airport.ident);
    }

    if(!asnMetar.isEmpty() || !noaaMetar.isEmpty() || !vatsimMetar.isEmpty())
    {
      if(info)
        head(html, "Weather");
      html.table();
      if(!asnMetar.isEmpty())
        html.row2("Metar (ASN):", asnMetar);
      if(!noaaMetar.isEmpty())
        html.row2("Metar (NOAA):", noaaMetar);
      if(!vatsimMetar.isEmpty())
        html.row2("Metar (Vatsim):", vatsimMetar);
      html.tableEnd();
    }
  }

  if(info)
  {
    head(html, "Longest Runway");
    html.table();
    html.row2("Length:", locale.toString(airport.longestRunwayLength) + " ft");
    if(rec != nullptr)
    {
      html.row2("Width:",
                locale.toString(rec->valueInt("longest_runway_width")) + " ft");

      float hdg = rec->valueFloat("longest_runway_heading") + airport.magvar;
      hdg = atools::geo::normalizeCourse(hdg);
      float otherHdg = atools::geo::normalizeCourse(atools::geo::opposedCourseDeg(hdg));

      html.row2("Heading:", locale.toString(hdg, 'f', 0) + " °M, " +
                locale.toString(otherHdg, 'f', 0) + " °M");
      html.row2("Surface:",
                maptypes::surfaceName(rec->valueStr("longest_runway_surface")));
    }
    html.tableEnd();
  }

  if(airport.towerFrequency > 0 || airport.atisFrequency > 0 || airport.awosFrequency > 0 ||
     airport.asosFrequency > 0 || airport.unicomFrequency > 0)
  {
    if(info)
      head(html, "COM Frequencies");
    html.table();
    if(airport.towerFrequency > 0)
      html.row2("Tower:", locale.toString(airport.towerFrequency / 1000., 'f', 2));
    if(airport.atisFrequency > 0)
      html.row2("ATIS:", locale.toString(airport.atisFrequency / 1000., 'f', 2));
    if(airport.awosFrequency > 0)
      html.row2("AWOS:", locale.toString(airport.awosFrequency / 1000., 'f', 2));
    if(airport.asosFrequency > 0)
      html.row2("ASOS:", locale.toString(airport.asosFrequency / 1000., 'f', 2));
    if(airport.unicomFrequency > 0)
      html.row2("Unicom:", locale.toString(airport.unicomFrequency / 1000., 'f', 2));
    html.tableEnd();
  }

  if(rec != nullptr)
  {
    int numParkingGate = rec->valueInt("num_parking_gate");
    int numJetway = rec->valueInt("num_jetway");
    int numParkingGaRamp = rec->valueInt("num_parking_ga_ramp");
    int numParkingCargo = rec->valueInt("num_parking_cargo");
    int numParkingMilCargo = rec->valueInt("num_parking_mil_cargo");
    int numParkingMilCombat = rec->valueInt("num_parking_mil_combat");
    int numHelipad = rec->valueInt("num_helipad");

    if(info)
      head(html, "Parking");
    html.table();
    if(numParkingGate > 0 || numJetway > 0 || numParkingGaRamp > 0 || numParkingCargo > 0 ||
       numParkingMilCargo > 0 || numParkingMilCombat > 0 ||
       !rec->isNull("largest_parking_ramp") || !rec->isNull("largest_parking_gate"))
    {

      if(numParkingGate > 0)
        html.row2("Gates:", numParkingGate);
      if(numJetway > 0)
        html.row2("Jetways:", numJetway);
      if(numParkingGaRamp > 0)
        html.row2("GA Ramp:", numParkingGaRamp);
      if(numParkingCargo > 0)
        html.row2("Cargo:", numParkingCargo);
      if(numParkingMilCargo > 0)
        html.row2("Military Cargo:", numParkingMilCargo);
      if(numParkingMilCombat > 0)
        html.row2("Military Combat:", numParkingMilCombat);

      if(!rec->isNull("largest_parking_ramp"))
        html.row2("Largest Ramp:", maptypes::parkingRampName(rec->valueStr("largest_parking_ramp")));
      if(!rec->isNull("largest_parking_gate"))
        html.row2("Largest Gate:", maptypes::parkingRampName(rec->valueStr("largest_parking_gate")));

      if(numHelipad > 0)
        html.row2("Helipads:", numHelipad);
    }
    else
      html.row2(QString(), "None");
    html.tableEnd();
  }

  if(info)
    head(html, "Runways");
  html.table();
  QStringList runways;

  if(airport.hard())
    runways.append("Hard");
  if(airport.soft())
    runways.append("Soft");
  if(airport.water())
    runways.append("Water");
  if(rec != nullptr)
  {
    if(rec->valueInt("num_runway_end_closed") > 0)
      runways.append("Closed");
  }
  if(airport.flags.testFlag(AP_LIGHT))
    runways.append("Lighted");

  html.row2(info ? QString() : "Runways:", runways.join(", "));
  html.tableEnd();

  if(rec != nullptr)
    addScenery(rec, html);
}

void MapHtmlInfoBuilder::comText(const MapAirport& airport, HtmlBuilder& html, QColor background)
{
  if(info && infoQuery != nullptr)
  {
    airportTitle(airport, html, background);
    html.h3("COM Frequencies");

    const SqlRecordVector *recVector = infoQuery->getComInformation(airport.id);
    if(recVector != nullptr)
    {

      html.table();
      html.tr(QColor(Qt::lightGray));
      html.td("Type", html::BOLD).td("Frequency", html::BOLD).td("Name", html::BOLD);
      html.trEnd();

      for(const SqlRecord& rec : *recVector)
      {
        html.tr();
        html.td(maptypes::comTypeName(rec.valueStr("type")));
        html.td(locale.toString(rec.valueInt("frequency") / 1000., 'f', 2) + " MHz");
        html.td(rec.valueStr("name"));
        html.trEnd();
      }
      html.tableEnd();
    }
    else
      html.text("None");
  }
}

void MapHtmlInfoBuilder::runwayText(const MapAirport& airport, HtmlBuilder& html, QColor background)
{
  if(info && infoQuery != nullptr)
  {
    airportTitle(airport, html, background);

    const SqlRecordVector *recVector = infoQuery->getRunwayInformation(airport.id);
    if(recVector != nullptr)
    {
      for(const SqlRecord& rec : *recVector)
      {
        const SqlRecord *recPrim = infoQuery->getRunwayEndInformation(rec.valueInt("primary_end_id"));
        const SqlRecord *recSec = infoQuery->getRunwayEndInformation(rec.valueInt("secondary_end_id"));
        float hdgPrim = atools::geo::normalizeCourse(rec.valueFloat("heading") + airport.magvar);
        float hdgSec = atools::geo::normalizeCourse(atools::geo::opposedCourseDeg(hdgPrim));
        bool closedPrim = recPrim->valueBool("has_closed_markings");
        bool closedSec = recSec->valueBool("has_closed_markings");

        html.h3("Runway " + recPrim->valueStr("name") + ", " + recSec->valueStr("name"),
                (closedPrim & closedSec ? html::STRIKEOUT : html::NONE));
        html.table();

        html.row2("Size:", locale.toString(rec.valueInt("length")) + " x " + locale
                  .toString(rec.valueInt("width")) + " ft");
        html.row2("Surface:", maptypes::surfaceName(rec.valueStr("surface")));
        html.row2("Pattern Altitude:", locale.toString(rec.valueInt("pattern_altitude")) + " ft");

        rowForStr(html, &rec, "edge_light", "Edge Lights:", "%1");
        rowForStr(html, &rec, "center_light", "Center Lights:", "%1");
        rowForBool(html, &rec, "has_center_red", "Has red Center Lights", false);

        {
          RunwayMarkingFlags flags(rec.valueInt("marking_flags"));
          QStringList markings;
          if(flags & atools::fs::bgl::rw::EDGES)
            markings.append("Edges");
          if(flags & atools::fs::bgl::rw::THRESHOLD)
            markings.append("Threshold");
          if(flags & atools::fs::bgl::rw::FIXED_DISTANCE)
            markings.append("Fixed Distance");
          if(flags & atools::fs::bgl::rw::TOUCHDOWN)
            markings.append("Touchdown");
          if(flags & atools::fs::bgl::rw::DASHES)
            markings.append("Dashes");
          if(flags & atools::fs::bgl::rw::IDENT)
            markings.append("Ident");
          if(flags & atools::fs::bgl::rw::PRECISION)
            markings.append("Precision");
          if(flags & atools::fs::bgl::rw::EDGE_PAVEMENT)
            markings.append("Edge Pavement");
          if(flags & atools::fs::bgl::rw::SINGLE_END)
            markings.append("Single End");

          if(flags & atools::fs::bgl::rw::ALTERNATE_THRESHOLD)
            markings.append("Alternate Threshold");
          if(flags & atools::fs::bgl::rw::ALTERNATE_FIXEDDISTANCE)
            markings.append("Alternate Fixed Distance");
          if(flags & atools::fs::bgl::rw::ALTERNATE_TOUCHDOWN)
            markings.append("Alternate Touchdown");
          if(flags & atools::fs::bgl::rw::ALTERNATE_PRECISION)
            markings.append("Alternate Precision");

          if(flags & atools::fs::bgl::rw::LEADING_ZERO_IDENT)
            markings.append("Leading Zero Ident");

          if(flags & atools::fs::bgl::rw::NO_THRESHOLD_END_ARROWS)
            markings.append("No Threshold End Arrows");

          if(markings.isEmpty())
            markings.append("None");

          html.row2("Runway Markings:", markings.join(", "));
        }

        html.tableEnd();

        runwayEndText(html, recPrim, hdgPrim);
        runwayEndText(html, recSec, hdgSec);
      }
    }
  }
}

void MapHtmlInfoBuilder::runwayEndText(HtmlBuilder& html, const SqlRecord *rec, float hdgPrim)
{
  bool closed = rec->valueBool("has_closed_markings");

  html.h3(rec->valueStr("name"), closed ? (html::STRIKEOUT) : html::NONE);
  html.table();
  if(closed)
    html.row2("Closed", QString());
  html.row2("Heading:", locale.toString(hdgPrim, 'f', 0) + " °M");
  rowForInt(html, rec, "offset_threshold", "Offset Threshold:", "%1 ft");
  rowForInt(html, rec, "blast_pad", "Blast Pad:", "%1 ft");
  rowForInt(html, rec, "overrun", "Overrun:", "%1 ft");

  rowForBool(html, rec, "has_stol_markings", "Has STOL Markings", false);
  // rowForBool(html, recPrim, "is_takeoff", "Closed for Takeoff", true);
  // rowForBool(html, recPrim, "is_landing", "Closed for Landing", true);
  rowForStr(html, rec, "is_pattern", "Pattern:", "%1");

  rowForStr(html, rec, "left_vasi_type", "Left VASI Type:", "%1");
  rowForFloat(html, rec, "left_vasi_pitch", "Left VASI Pitch:", "%1 °", 1);
  rowForStr(html, rec, "right_vasi_type", "Right VASI Type:", "%1");
  rowForFloat(html, rec, "right_vasi_pitch", "Right VASI Pitch:", "%1 °", 1);

  rowForStr(html, rec, "app_light_system_type", "ALS Type:", "%1");
  rowForBool(html, rec, "has_end_lights", "Has Runway End Lights", false);
  rowForBool(html, rec, "has_reils", "Has Runway End Strobes", false);
  rowForBool(html, rec, "has_touchdown_lights", "Has Touchdown lights", false);

  // num_strobes integer not null
  html.tableEnd();

  const atools::sql::SqlRecord *ilsRec = infoQuery->getIlsInformation(rec->valueInt("runway_end_id"));
  if(ilsRec != nullptr)
  {
    bool dme = !ilsRec->isNull("dme_altitude");
    bool gs = !ilsRec->isNull("gs_altitude");

    html.br().h4(ilsRec->valueStr("name") + " (" +
                 ilsRec->valueStr("ident") + ") - " +
                 QString("ILS") + (gs ? ", GS" : "") + (dme ? ", DME" : ""));

    html.table();
    html.row2("Frequency:", locale.toString(ilsRec->valueFloat("frequency") / 1000., 'f', 2) + " MHz");
    html.row2("Range:", locale.toString(ilsRec->valueInt("range")) + " nm");
    float magvar = ilsRec->valueFloat("mag_var");
    html.row2("Magvar:", locale.toString(magvar, 'f', 1) + " °");
    rowForBool(html, ilsRec, "has_backcourse", "Has Backcourse", false);

    float hdg = ilsRec->valueFloat("loc_heading") + magvar;
    hdg = atools::geo::normalizeCourse(hdg);

    html.row2("Localizer Heading:", locale.toString(hdg, 'f', 1) + " °M");
    html.row2("Localizer Width:", locale.toString(ilsRec->valueFloat("loc_width"), 'f', 1) + " °");
    if(gs)
      html.row2("Glideslope Pitch:", locale.toString(ilsRec->valueFloat("gs_pitch"), 'f', 1) + " °");

    html.tableEnd();
  }
}

void MapHtmlInfoBuilder::approachText(const MapAirport& airport, HtmlBuilder& html, QColor background)
{
  if(info && infoQuery != nullptr)
  {
    airportTitle(airport, html, background);

    const SqlRecordVector *recAppVector = infoQuery->getApproachInformation(airport.id);
    if(recAppVector != nullptr)
    {
      for(const SqlRecord& recApp : *recAppVector)
      {
        QString runway;
        if(!recApp.isNull("runway_name"))
          runway = " - Runway " + recApp.valueStr("runway_name");

        html.h4("Approach " + recApp.valueStr("type") + runway);
        html.table();
        rowForBool(html, &recApp, "has_gps_overlay", "Has GPS Overlay", false);
        html.row2("Fix Ident and Region:", recApp.valueStr("fix_ident") + ", " + recApp.valueStr("fix_region"));
        html.row2("Fix Type:", recApp.valueStr("fix_type"));

        float hdg = recApp.valueFloat("heading") + airport.magvar;
        hdg = atools::geo::normalizeCourse(hdg);
        html.row2("Heading:", locale.toString(hdg, 'f', 1) + " °M, " +
                  locale.toString(recApp.valueFloat("heading"), 'f', 1) + " °T");

        html.row2("Altitude:", locale.toString(recApp.valueFloat("altitude"), 'f', 0) + " ft");
        html.row2("Missed Altitude:", locale.toString(recApp.valueFloat("missed_altitude"), 'f', 0) + " ft");
        html.tableEnd();

        const SqlRecordVector *recTransVector =
          infoQuery->getTransitionInformation(recApp.valueInt("approach_id"));
        if(recTransVector != nullptr)
        {
          for(const SqlRecord& recTrans : *recTransVector)
          {
            html.h4("Transition " + recTrans.valueStr("fix_ident") + runway);
            html.table();
            html.row2("Type:", recTrans.valueStr("type"));
            html.row2("Fix Ident and Region:", recTrans.valueStr("fix_ident") + ", " +
                      recTrans.valueStr("fix_region"));
            html.row2("Fix Type:", recTrans.valueStr("fix_type"));
            html.row2("Altitude:", locale.toString(recTrans.valueFloat("altitude"), 'f', 0) + " ft");

            if(!recTrans.isNull("dme_ident"))
              html.row2("DME Ident and Region:", recTrans.valueStr("dme_ident") + ", " +
                        recTrans.valueStr("dme_region"));

            rowForFloat(html, &recTrans, "dme_radial", "DME Radial:", "%1", 0);
            rowForFloat(html, &recTrans, "dme_distance", "DME Distance:", "%1 nm", 0);
            html.tableEnd();
          }
        }
      }
    }
  }
}

void MapHtmlInfoBuilder::vorText(const MapVor& vor, HtmlBuilder& html, QColor background)
{
  const SqlRecord *rec = nullptr;
  if(info && infoQuery != nullptr)
    rec = infoQuery->getVorInformation(vor.id);

  QIcon icon = SymbolPainter(background).createVorIcon(vor, SYMBOL_SIZE);
  html.img(icon, QString(), QString(), QSize(SYMBOL_SIZE, SYMBOL_SIZE));
  html.nbsp().nbsp();

  QString type = maptypes::vorType(vor);
  title(html, type + ": " + vor.name + " (" + vor.ident + ")");

  html.table();
  if(vor.routeIndex >= 0)
    html.row2("Route position:", QString::number(vor.routeIndex + 1));

  html.row2("Type:", maptypes::navTypeName(vor.type));
  html.row2("Region:", vor.region);
  html.row2("Frequency:", locale.toString(vor.frequency / 1000., 'f', 2) + " MHz");
  if(!vor.dmeOnly)
    html.row2("Magvar:", locale.toString(vor.magvar, 'f', 1) + " °");
  html.row2("Altitude:", locale.toString(vor.getPosition().getAltitude(), 'f', 0) + " ft");
  html.row2("Range:", QString::number(vor.range) + " nm");
  html.row2("Morse:", "<b>" + morse->getCode(vor.ident) + "</b>");
  addCoordinates(rec, html);
  html.tableEnd();

  if(rec != nullptr)
    addScenery(rec, html);
}

void MapHtmlInfoBuilder::ndbText(const MapNdb& ndb, HtmlBuilder& html, QColor background)
{
  const SqlRecord *rec = nullptr;
  if(info && infoQuery != nullptr)
    rec = infoQuery->getNdbInformation(ndb.id);

  QIcon icon = SymbolPainter(background).createNdbIcon(ndb, SYMBOL_SIZE);
  html.img(icon, QString(), QString(), QSize(SYMBOL_SIZE, SYMBOL_SIZE));
  html.nbsp().nbsp();

  title(html, "NDB: " + ndb.name + " (" + ndb.ident + ")");
  html.table();
  if(ndb.routeIndex >= 0)
    html.row2("Route position ", QString::number(ndb.routeIndex + 1));
  html.row2("Type:", maptypes::navTypeName(ndb.type));
  html.row2("Region:", ndb.region);
  html.row2("Frequency:", locale.toString(ndb.frequency / 100., 'f', 2) + " kHz");
  html.row2("Magvar:", locale.toString(ndb.magvar, 'f', 1) + " °");
  html.row2("Altitude:", locale.toString(ndb.getPosition().getAltitude(), 'f', 0) + " ft");
  html.row2("Range:", QString::number(ndb.range) + " nm");
  html.row2("Morse:", "<b>" + morse->getCode(ndb.ident) + "</b>");
  addCoordinates(rec, html);
  html.tableEnd();

  if(rec != nullptr)
    addScenery(rec, html);
}

void MapHtmlInfoBuilder::waypointText(const MapWaypoint& waypoint, HtmlBuilder& html, QColor background)
{
  const SqlRecord *rec = nullptr;
  if(info && infoQuery != nullptr)
    rec = infoQuery->getWaypointInformation(waypoint.id);

  QIcon icon = SymbolPainter(background).createWaypointIcon(waypoint, SYMBOL_SIZE);
  html.img(icon, QString(), QString(), QSize(SYMBOL_SIZE, SYMBOL_SIZE));
  html.nbsp().nbsp();

  title(html, "Waypoint: " + waypoint.ident);
  html.table();
  if(waypoint.routeIndex >= 0)
    html.row2("Route position:", QString::number(waypoint.routeIndex + 1));
  html.row2("Type:", maptypes::navTypeName(waypoint.type));
  html.row2("Region:", waypoint.region);
  html.row2("Magvar:", locale.toString(waypoint.magvar, 'f', 1) + " °");
  addCoordinates(rec, html);

  html.tableEnd();

  QList<MapAirway> airways;
  mapQuery->getAirwaysForWaypoint(airways, waypoint.id);

  if(!airways.isEmpty())
  {
    QVector<std::pair<QString, QString> > airwayTexts;
    for(const MapAirway& aw : airways)
    {
      QString txt(maptypes::airwayTypeToString(aw.type));
      if(aw.minalt > 0)
        txt += ", " + QString::number(aw.minalt) + " ft";
      airwayTexts.append(std::make_pair(aw.name, txt));
    }

    if(!airwayTexts.isEmpty())
    {
      std::sort(airwayTexts.begin(), airwayTexts.end(),
                [ = ](const std::pair<QString, QString> &item1, const std::pair<QString, QString> &item2)
                {
                  return item1.first < item2.first;
                });
      airwayTexts.erase(std::unique(airwayTexts.begin(), airwayTexts.end()), airwayTexts.end());

      if(info)
        head(html, "Airways:");
      else
        html.br().b("Airways: ");

      html.table();
      for(const std::pair<QString, QString>& aw : airwayTexts)
        html.row2(aw.first, aw.second);
      html.tableEnd();
    }
  }

  if(rec != nullptr)
    addScenery(rec, html);
}

void MapHtmlInfoBuilder::airwayText(const MapAirway& airway, HtmlBuilder& html)
{
  title(html, "Airway: " + airway.name);
  html.table();
  html.row2("Type:", maptypes::airwayTypeToString(airway.type));

  if(airway.minalt > 0)
    html.row2("Min altitude:", QString::number(airway.minalt) + " ft");

  if(infoQuery != nullptr && info)
  {
    atools::sql::SqlRecordVector waypoints =
      infoQuery->getAirwayWaypointInformation(airway.name, airway.fragment);

    if(!waypoints.isEmpty())
    {
      QStringList waypointTexts;
      for(const SqlRecord& wprec : waypoints)
        waypointTexts.append(wprec.valueStr("from_ident") + ", " + wprec.valueStr("from_region"));
      waypointTexts.append(waypoints.last().valueStr("to_ident") + ", " +
                           waypoints.last().valueStr("to_region"));

      html.row2("Waypoints (Ident and Region):", waypointTexts.join(", "));
    }
  }
  html.tableEnd();
}

void MapHtmlInfoBuilder::markerText(const MapMarker& m, HtmlBuilder& html)
{
  if(!html.isEmpty())
    html.hr();
  head(html, "Marker: " + m.type);
}

void MapHtmlInfoBuilder::towerText(const MapAirport& airport, HtmlBuilder& html)
{
  if(airport.towerFrequency > 0)
    head(html, "Tower: " + locale.toString(airport.towerFrequency / 1000., 'f', 2));
  else
    head(html, "Tower");
}

void MapHtmlInfoBuilder::parkingText(const MapParking& parking, HtmlBuilder& html)
{
  if(parking.type != "FUEL")
  {
    head(html, maptypes::parkingName(parking.name) + " " + QString::number(parking.number));
    html.brText(maptypes::parkingTypeName(parking.type));
    html.brText(QString::number(parking.radius * 2) + " ft");
    if(parking.jetway)
      html.brText("Has Jetway");
  }
  else
    html.text("Fuel");
}

void MapHtmlInfoBuilder::helipadText(const MapHelipad& helipad, HtmlBuilder& html)
{
  head(html, "Helipad:");
  html.brText("Surface: " + maptypes::surfaceName(helipad.surface));
  html.brText("Type: " + helipad.type);
  html.brText(QString::number(helipad.width) + " ft");
  if(helipad.closed)
    html.brText("Is Closed");
}

void MapHtmlInfoBuilder::userpointText(const MapUserpoint& userpoint, HtmlBuilder& html)
{
  head(html, "Routepoint:");
  html.brText(userpoint.name);
}

void MapHtmlInfoBuilder::addScenery(const atools::sql::SqlRecord *rec, HtmlBuilder& html)
{
  head(html, "Scenery");
  html.table();
  html.row2("Title:", rec->valueStr("title"));
  html.row2("BGL Filepath:", rec->valueStr("filepath"));
  html.tableEnd();
}

void MapHtmlInfoBuilder::addCoordinates(const atools::sql::SqlRecord *rec, HtmlBuilder& html)
{
  if(rec != nullptr)
  {
    float alt = 0;
    if(rec->contains("altitude"))
      alt = rec->valueFloat("altitude");
    atools::geo::Pos pos(rec->valueFloat("lonx"), rec->valueFloat("laty"), alt);
    html.row2("Coordinates:", pos.toHumanReadableString());
  }
}

void MapHtmlInfoBuilder::head(HtmlBuilder& html, const QString& text)
{
  if(info)
    html.h4(text);
  else
    html.b(text);
}

void MapHtmlInfoBuilder::title(HtmlBuilder& html, const QString& text)
{
  if(info)
    html.text(text, html::BOLD | html::BIG);
  else
    html.b(text);
}

void MapHtmlInfoBuilder::rowForFloat(HtmlBuilder& html, const SqlRecord *rec, const QString& colName,
                                     const QString& msg, const QString& val, int precision)
{
  if(!rec->isNull(colName))
  {
    float i = rec->valueFloat(colName);
    if(i > 0)
      html.row2(msg, val.arg(locale.toString(i, 'f', precision)));
  }
}

void MapHtmlInfoBuilder::rowForInt(HtmlBuilder& html, const SqlRecord *rec, const QString& colName,
                                   const QString& msg, const QString& val)
{
  if(!rec->isNull(colName))
  {
    int i = rec->valueInt(colName);
    if(i > 0)
      html.row2(msg, val.arg(locale.toString(i)));
  }
}

void MapHtmlInfoBuilder::rowForBool(HtmlBuilder& html, const SqlRecord *rec, const QString& colName,
                                    const QString& msg, bool expected)
{
  if(!rec->isNull(colName))
  {
    bool i = rec->valueBool(colName);
    if(i != expected)
      html.row2(msg, QString());
  }
}

void MapHtmlInfoBuilder::rowForStr(HtmlBuilder& html, const SqlRecord *rec, const QString& colName,
                                   const QString& msg, const QString& val)
{
  if(!rec->isNull(colName))
  {
    QString i = rec->valueStr(colName);
    if(!i.isEmpty())
      html.row2(msg, val.arg(i));
  }
}
