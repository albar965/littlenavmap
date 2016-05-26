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

#include <common/htmlbuilder.h>
#include <common/morsecode.h>
#include <common/weatherreporter.h>

#include <QSize>

#include <sql/sqlrecord.h>

#include <info/infoquery.h>

using namespace maptypes;
using atools::sql::SqlRecord;

const int SYMBOL_SIZE = 20;

MapHtmlInfoBuilder::MapHtmlInfoBuilder(MapQuery *mapDbQuery, InfoQuery *infoDbQuery, bool formatInfo)
  : mapQuery(mapDbQuery), infoQuery(infoDbQuery), info(formatInfo)
{
  morse = new MorseCode("&nbsp;", "&nbsp;&nbsp;&nbsp;");
}

MapHtmlInfoBuilder::~MapHtmlInfoBuilder()
{
  delete morse;
}

void MapHtmlInfoBuilder::airportText(const MapAirport& airport, HtmlBuilder& html,
                                     const RouteMapObjectList *routeMapObjects, WeatherReporter *weather,
                                     QColor background)
{
  const SqlRecord *rec = nullptr;
  if(info && infoQuery != nullptr)
    rec = infoQuery->getAirportInformation(airport.id);

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

  QString city, state, country;
  mapQuery->getAirportAdminById(airport.id, city, state, country);

  html.table();
  html.row("City:", city);
  if(!state.isEmpty())
    html.row("State/Province:", state);
  html.row("Country:", country);
  html.row("Altitude:", locale.toString(airport.getPosition().getAltitude(), 'f', 0) + " ft");
  html.row("Magvar:", locale.toString(airport.magvar, 'f', 1) + " °");
  if(rec != nullptr)
  {
    html.row("Rating (0-5):", rec->value("rating").toInt());
    addCoordinates(rec, html);
  }
  html.tableEnd();

  if(!info)
  {
    html.table();
    html.row("Longest Runway Length:", locale.toString(airport.longestRunwayLength) + " ft");
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
    {
      asnMetar = weather->getAsnMetar(airport.ident);
    }
    noaaMetar = weather->getNoaaMetar(airport.ident);
    vatsimMetar = weather->getVatsimMetar(airport.ident);

    if(!asnMetar.isEmpty() || !noaaMetar.isEmpty() || !vatsimMetar.isEmpty())
    {
      if(info)
        head(html, "Weather");
      html.table();
      if(!asnMetar.isEmpty())
        html.row("Metar (ASN):", asnMetar);
      if(!noaaMetar.isEmpty())
        html.row("Metar (NOAA):", noaaMetar);
      if(!vatsimMetar.isEmpty())
        html.row("Metar (Vatsim):", vatsimMetar);

      html.tableEnd();

    }
  }

  if(info)
  {
    head(html, "Longest Runway");
    html.table();
    html.row("Length:", locale.toString(airport.longestRunwayLength) + " ft");
    if(rec != nullptr)
    {
      html.row("Width:",
               locale.toString(rec->value("longest_runway_width").toInt()) + " ft");

      float hdg = rec->value("longest_runway_heading").toFloat() + rec->value("mag_var").toFloat();
      hdg = atools::geo::normalizeCourse(hdg);
      float otherHdg = atools::geo::normalizeCourse(atools::geo::opposedCourseDeg(hdg));

      html.row("Heading:", locale.toString(hdg, 'f', 0) + " ° / " + locale.toString(otherHdg, 'f', 0) + " °");
      html.row("Surface:",
               maptypes::surfaceName(rec->value("longest_runway_surface").toString()));
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
      html.row("Tower:", locale.toString(airport.towerFrequency / 1000., 'f', 2));
    if(airport.atisFrequency > 0)
      html.row("ATIS:", locale.toString(airport.atisFrequency / 1000., 'f', 2));
    if(airport.awosFrequency > 0)
      html.row("AWOS:", locale.toString(airport.awosFrequency / 1000., 'f', 2));
    if(airport.asosFrequency > 0)
      html.row("ASOS:", locale.toString(airport.asosFrequency / 1000., 'f', 2));
    if(airport.unicomFrequency > 0)
      html.row("Unicom:", locale.toString(airport.unicomFrequency / 1000., 'f', 2));
    html.tableEnd();
  }

  if(rec != nullptr)
  {
  }

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

  if(airport.hard())
    facilities.append("Hard Runways");
  if(airport.soft())
    facilities.append("Soft Runways");
  if(airport.water())
    facilities.append("Water Runways");
  if(airport.flags.testFlag(AP_LIGHT))
    facilities.append("Lighted Runways");
  if(airport.helipad())
    facilities.append("Helipads");
  if(airport.flags.testFlag(AP_AVGAS))
    facilities.append("Avgas");
  if(airport.flags.testFlag(AP_JETFUEL))
    facilities.append("Jetfuel");

  if(airport.flags.testFlag(AP_ILS))
    facilities.append("ILS");
  if(airport.flags.testFlag(AP_APPR))
    facilities.append("Approaches");

  html.row(QString(), facilities.join(", "));

  html.tableEnd();

  if(rec != nullptr)
    addScenery(rec, html);
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
    html.row("Route position:", QString::number(vor.routeIndex + 1));

  html.row("Type:", maptypes::navTypeName(vor.type));
  html.row("Region:", vor.region);
  html.row("Freq:", locale.toString(vor.frequency / 1000., 'f', 2) + " MHz");
  if(!vor.dmeOnly)
    html.row("Magvar:", locale.toString(vor.magvar, 'f', 1) + " °");
  html.row("Altitude:", locale.toString(vor.getPosition().getAltitude(), 'f', 0) + " ft");
  html.row("Range:", QString::number(vor.range) + " nm");
  html.row("Morse:", "<b>" + morse->getCode(vor.ident) + "</b>");
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
    html.row("Route position ", QString::number(ndb.routeIndex + 1));
  html.row("Type:", maptypes::navTypeName(ndb.type));
  html.row("Region:", ndb.region);
  html.row("Freq:", locale.toString(ndb.frequency / 100., 'f', 2) + " kHz");
  html.row("Magvar:", locale.toString(ndb.magvar, 'f', 1) + " °");
  html.row("Altitude:", locale.toString(ndb.getPosition().getAltitude(), 'f', 0) + " ft");
  html.row("Range:", QString::number(ndb.range) + " nm");
  html.row("Morse:", "<b>" + morse->getCode(ndb.ident) + "</b>");
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
    html.row("Route position:", QString::number(waypoint.routeIndex + 1));
  html.row("Type:", maptypes::navTypeName(waypoint.type));
  html.row("Region:", waypoint.region);
  html.row("Magvar:", locale.toString(waypoint.magvar, 'f', 1) + " °");
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
        html.row(aw.first, aw.second);
      html.tableEnd();
    }
  }

  if(rec != nullptr)
    addScenery(rec, html);
}

void MapHtmlInfoBuilder::airwayText(const MapAirway& airway, HtmlBuilder& html)
{
  const SqlRecord *rec = nullptr;
  if(info && infoQuery != nullptr)
    rec = infoQuery->getAirwayInformation(airway.id);

  title(html, "Airway: " + airway.name);
  html.table();
  html.row("Type:", maptypes::airwayTypeToString(airway.type));

  if(airway.minalt > 0)
    html.row("Min altitude:", QString::number(airway.minalt) + " ft");
  html.tableEnd();

  // TODO add all waypoints

  // if(rec != nullptr)
  // addScenery(rec, html);
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
  html.row("Title:", rec->value("title").toString());
  html.row("BGL Filepath:", rec->value("filepath").toString());
  html.tableEnd();
}

void MapHtmlInfoBuilder::addCoordinates(const atools::sql::SqlRecord *rec, HtmlBuilder& html)
{
  if(rec != nullptr)
  {
    atools::geo::Pos pos(rec->value("lonx").toFloat(), rec->value("laty").toFloat(),
                         rec->value("altitude").toFloat());

    html.row("Coordinates:", pos.toHumanReadableString());
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
