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

#include <common/htmlbuilder.h>
#include <common/morsecode.h>
#include <common/weatherreporter.h>

#include <QSize>

using namespace maptypes;

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
  QIcon icon = SymbolPainter(background).createAirportIcon(airport, SYMBOL_SIZE);
  html.img(icon, QString(), QString(), QSize(SYMBOL_SIZE, SYMBOL_SIZE));

  tableHeader(html, airport.name + " (" + airport.ident + ")");
  QString city, state, country;
  mapQuery->getAirportAdminById(airport.id, city, state, country);

  tableStart(html);
  tableRow(html, "City:", city);
  if(!state.isEmpty())
    tableRow(html, "State/Province:", state);
  tableRow(html, "Country:", country);
  tableEnd(html);

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
    if(info)
      tableHeader(html, "Weather");
    tableStart(html);
    QString metar;
    if(weather->hasAsnWeather())
    {
      metar = weather->getAsnMetar(airport.ident);
      if(!metar.isEmpty())
        tableRow(html, "Metar (ASN):", metar);
    }
    // else
    {
      metar = weather->getNoaaMetar(airport.ident);
      if(!metar.isEmpty())
        tableRow(html, "Metar (NOAA):", metar);
      metar = weather->getVatsimMetar(airport.ident);
      if(!metar.isEmpty())
        tableRow(html, "Metar (Vatsim):", metar);
    }
    tableEnd(html);
  }

  if(info)
    tableHeader(html, "Data");
  tableStart(html);
  tableRow(html, "Longest Runway:", locale.toString(airport.longestRunwayLength) + " ft");
  tableRow(html, "Altitude:", locale.toString(airport.getPosition().getAltitude(), 'f', 0) + " ft");
  tableRow(html, "Magvar:", locale.toString(airport.magvar, 'f', 1) + " °");
  tableEnd(html);

  if(info)
    tableHeader(html, "COM Frequencies");
  tableStart(html);
  if(airport.towerFrequency > 0)
    tableRow(html, "Tower:", locale.toString(airport.towerFrequency / 1000., 'f', 2));
  if(airport.atisFrequency > 0)
    tableRow(html, "ATIS:", locale.toString(airport.atisFrequency / 1000., 'f', 2));
  if(airport.awosFrequency > 0)
    tableRow(html, "AWOS:", locale.toString(airport.awosFrequency / 1000., 'f', 2));
  if(airport.asosFrequency > 0)
    tableRow(html, "ASOS:", locale.toString(airport.asosFrequency / 1000., 'f', 2));
  if(airport.unicomFrequency > 0)
    tableRow(html, "Unicom:", locale.toString(airport.unicomFrequency / 1000., 'f', 2));
  tableEnd(html);

  if(info)
    tableHeader(html, "Facilities");
  tableStart(html);
  QStringList facilities;

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

  tableRow(html, QString(), facilities.join(", "));

  tableEnd(html);
}

void MapHtmlInfoBuilder::vorText(const MapVor& vor, HtmlBuilder& html, QColor background)
{
  QIcon icon = SymbolPainter(background).createVorIcon(vor, SYMBOL_SIZE);
  html.img(icon, QString(), QString(), QSize(SYMBOL_SIZE, SYMBOL_SIZE));

  QString type = maptypes::vorType(vor);
  tableHeader(html, type + ": " + vor.name + " (" + vor.ident + ")");

  tableStart(html);
  if(vor.routeIndex >= 0)
    tableRow(html, "Route position:", QString::number(vor.routeIndex + 1));

  tableRow(html, "Type:", maptypes::navTypeName(vor.type));
  tableRow(html, "Region:", vor.region);
  tableRow(html, "Freq:", locale.toString(vor.frequency / 1000., 'f', 2) + " MHz");
  if(!vor.dmeOnly)
    tableRow(html, "Magvar:", locale.toString(vor.magvar, 'f', 1) + " °");
  tableRow(html, "Altitude:", locale.toString(vor.getPosition().getAltitude(), 'f', 0) + " ft");
  tableRow(html, "Range:", QString::number(vor.range) + " nm");
  tableRow(html, "Morse:", "<b>" + morse->getCode(vor.ident) + "</b>");
  tableEnd(html);
}

void MapHtmlInfoBuilder::ndbText(const MapNdb& ndb, HtmlBuilder& html, QColor background)
{
  QIcon icon = SymbolPainter(background).createNdbIcon(ndb, SYMBOL_SIZE);
  html.img(icon, QString(), QString(), QSize(SYMBOL_SIZE, SYMBOL_SIZE));

  tableHeader(html, "NDB: " + ndb.name + " (" + ndb.ident + ")");
  tableStart(html);
  if(ndb.routeIndex >= 0)
    tableRow(html, "Route position ", QString::number(ndb.routeIndex + 1));
  tableRow(html, "Type:", maptypes::navTypeName(ndb.type));
  tableRow(html, "Region:", ndb.region);
  tableRow(html, "Freq:", locale.toString(ndb.frequency / 100., 'f', 2) + " kHz");
  tableRow(html, "Magvar:", locale.toString(ndb.magvar, 'f', 1) + " °");
  tableRow(html, "Altitude:", locale.toString(ndb.getPosition().getAltitude(), 'f', 0) + " ft");
  tableRow(html, "Range:", QString::number(ndb.range) + " nm");
  tableRow(html, "Morse:", "<b>" + morse->getCode(ndb.ident) + "</b>");
  tableEnd(html);
}

void MapHtmlInfoBuilder::waypointText(const MapWaypoint& waypoint, HtmlBuilder& html, QColor background)
{
  QIcon icon = SymbolPainter(background).createWaypointIcon(waypoint, SYMBOL_SIZE);
  html.img(icon, QString(), QString(), QSize(SYMBOL_SIZE, SYMBOL_SIZE));

  tableHeader(html, "Waypoint: " + waypoint.ident);
  tableStart(html);
  if(waypoint.routeIndex >= 0)
    tableRow(html, "Route position:", QString::number(waypoint.routeIndex + 1));
  tableRow(html, "Type:", maptypes::navTypeName(waypoint.type));
  tableRow(html, "Region:", waypoint.region);
  tableRow(html, "Magvar:", locale.toString(waypoint.magvar, 'f', 1) + " °");
  tableEnd(html);

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
        html.h4("Airways:");
      else
        html.br().b("Airways: ");

      tableStart(html);
      for(const std::pair<QString, QString>& aw : airwayTexts)
        tableRow(html, aw.first, aw.second);
      tableEnd(html);
    }
  }
}

void MapHtmlInfoBuilder::airwayText(const MapAirway& airway, HtmlBuilder& html)
{
  tableHeader(html, "Airway: " + airway.name);
  tableStart(html);
  tableRow(html, "Type:", maptypes::airwayTypeToString(airway.type));

  if(airway.minalt > 0)
    tableRow(html, "Min altitude:", QString::number(airway.minalt) + " ft");
  tableEnd(html);
}

void MapHtmlInfoBuilder::markerText(const MapMarker& m, HtmlBuilder& html)
{
  if(!html.isEmpty())
    html.hr();
  html.b("Marker: " + m.type);
}

void MapHtmlInfoBuilder::towerText(const MapAirport& airport, HtmlBuilder& html)
{
  if(airport.towerFrequency > 0)
    html.b("Tower: " + locale.toString(airport.towerFrequency / 1000., 'f', 2));
  else
    html.b("Tower");
}

void MapHtmlInfoBuilder::parkingText(const MapParking& parking, HtmlBuilder& html)
{
  if(parking.type != "FUEL")
  {
    html.b(maptypes::parkingName(parking.name) + " " + QString::number(parking.number));
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
  html.b("Helipad:");
  html.brText("Surface: " + maptypes::surfaceName(helipad.surface));
  html.brText("Type: " + helipad.type);
  html.brText(QString::number(helipad.width) + " ft");
  if(helipad.closed)
    html.brText("Is Closed");
}

void MapHtmlInfoBuilder::userpointText(const MapUserpoint& userpoint, HtmlBuilder& html)
{
  html.b("Routepoint:").brText(userpoint.name);
}

void MapHtmlInfoBuilder::tableHeader(HtmlBuilder& html, const QString& text)
{
  if(info)
    html.h4(text);
  else
    html.b(text);
}

void MapHtmlInfoBuilder::tableRow(HtmlBuilder& html, const QString& caption, const QString& value)
{
  if(info)
    html.row(caption, value);
  else
    html.brText(caption + " " + value);
}

void MapHtmlInfoBuilder::tableStart(HtmlBuilder& html)
{
  if(info)
    html.table();
}

void MapHtmlInfoBuilder::tableEnd(HtmlBuilder& html)
{
  if(info)
    html.tableEnd();
}
