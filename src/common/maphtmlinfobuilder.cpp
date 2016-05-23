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
#include "common/maptypes.h"
#include "mapgui/mapquery.h"
#include "common/formatter.h"
#include "route/routemapobjectlist.h"

#include <common/htmlbuilder.h>
#include <common/morsecode.h>
#include <common/weatherreporter.h>

using namespace maptypes;

MapHtmlInfoBuilder::MapHtmlInfoBuilder(MapQuery *mapQuery, bool formatInfo)
  : query(mapQuery), info(formatInfo)
{
  morse = new MorseCode("&nbsp;", "&nbsp;&nbsp;&nbsp;");
}

MapHtmlInfoBuilder::~MapHtmlInfoBuilder()
{
  delete morse;
}

void MapHtmlInfoBuilder::airportText(const MapAirport& airport, HtmlBuilder& html,
                                     const RouteMapObjectList *routeMapObjects, WeatherReporter *weather)
{
  html.b(airport.name + " (" + airport.ident + ")");
  QString city, state, country;
  query->getAirportAdminById(airport.id, city, state, country);
  html.brText().b(city + (state.isEmpty() ? "" : ", " + state) + ", " + country);

  if(weather != nullptr)
  {
    QString metar;
    if(weather->hasAsnWeather())
    {
      metar = weather->getAsnMetar(airport.ident);
      if(!metar.isEmpty())
        html.brText("Metar (ASN): " + metar);
    }
    // else
    {
      metar = weather->getNoaaMetar(airport.ident);
      if(!metar.isEmpty())
        html.brText("Metar (NOAA): " + metar);
      metar = weather->getVatsimMetar(airport.ident);
      if(!metar.isEmpty())
        html.brText("Metar (Vatsim): " + metar);
    }
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

  html.brText("Longest Runway: " + QLocale().toString(airport.longestRunwayLength) + " ft");
  html.brText("Altitude: " + QLocale().toString(airport.getPosition().getAltitude(), 'f', 0) + " ft");
  html.brText("Magvar: " + QLocale().toString(airport.magvar, 'f', 1) + " °");

  if(airport.towerFrequency > 0)
    html.brText("Tower: " + QLocale().toString(airport.towerFrequency / 1000., 'f', 2));
  if(airport.atisFrequency > 0)
    html.brText("ATIS: " + QLocale().toString(airport.atisFrequency / 1000., 'f', 2));
  if(airport.awosFrequency > 0)
    html.brText("AWOS: " + QLocale().toString(airport.awosFrequency / 1000., 'f', 2));
  if(airport.asosFrequency > 0)
    html.brText("ASOS: " + QLocale().toString(airport.asosFrequency / 1000., 'f', 2));
  if(airport.unicomFrequency > 0)
    html.brText("Unicom: " + QLocale().toString(airport.unicomFrequency / 1000., 'f', 2));

  if(airport.addon())
    html.brText("Add-on Airport");
  if(airport.flags.testFlag(AP_MIL))
    html.brText("Military Airport");
  if(airport.scenery())
    html.brText("Has some Scenery Elements");

  if(airport.hard())
    html.brText("Has Hard Runways");
  if(airport.soft())
    html.brText("Has Soft Runways");
  if(airport.water())
    html.brText("Has Water Runways");
  if(airport.flags.testFlag(AP_LIGHT))
    html.brText("Has Lighted Runways");
  if(airport.helipad())
    html.brText("Has Helipads");
  if(airport.flags.testFlag(AP_AVGAS))
    html.brText("Has Avgas");
  if(airport.flags.testFlag(AP_JETFUEL))
    html.brText("Has Jetfuel");

  if(airport.flags.testFlag(AP_ILS))
    html.brText("Has ILS");
  if(airport.flags.testFlag(AP_APPR))
    html.brText("Has Approaches");
}

void MapHtmlInfoBuilder::vorText(const MapVor& vor, HtmlBuilder& html)
{
  QString type = maptypes::vorType(vor);
  html.b(type + ": " + vor.name + " (" + vor.ident + ")");

  if(vor.routeIndex >= 0)
    html.brText("Route position " + QString::number(vor.routeIndex + 1));

  html.brText("Type: " + maptypes::navTypeName(vor.type));
  html.brText("Region: " + vor.region);
  html.brText("Freq: " + QLocale().toString(vor.frequency / 1000., 'f', 2) + " MHz");
  if(!vor.dmeOnly)
    html.brText("Magvar: " + QLocale().toString(vor.magvar, 'f', 1) + " °");
  html.brText("Altitude: " + QLocale().toString(vor.getPosition().getAltitude(), 'f', 0) + " ft");
  html.brText("Range: " + QString::number(vor.range) + " nm");
  html.brText("Morse: ").b(morse->getCode(vor.ident));
}

void MapHtmlInfoBuilder::ndbText(const MapNdb& ndb, HtmlBuilder& html)
{
  html.b("NDB: " + ndb.name + " (" + ndb.ident + ")");
  if(ndb.routeIndex >= 0)
    html.brText("Route position " + QString::number(ndb.routeIndex + 1));
  html.brText("Type: " + maptypes::navTypeName(ndb.type));
  html.brText("Region: " + ndb.region);
  html.brText("Freq: " + QLocale().toString(ndb.frequency / 100., 'f', 2) + " kHz");
  html.brText("Magvar: " + QLocale().toString(ndb.magvar, 'f', 1) + " °");
  html.brText("Altitude: " + QLocale().toString(ndb.getPosition().getAltitude(), 'f', 0) + " ft");
  html.brText("Range: " + QString::number(ndb.range) + " nm");
  html.brText("Morse: ").b(morse->getCode(ndb.ident));
}

void MapHtmlInfoBuilder::waypointText(const MapWaypoint& waypoint, HtmlBuilder& html)
{
  html.b("Waypoint: " + waypoint.ident);
  if(waypoint.routeIndex >= 0)
    html.brText("Route position " + QString::number(waypoint.routeIndex + 1));
  html.brText("Type: " + maptypes::navTypeName(waypoint.type));
  html.brText("Region: " + waypoint.region);
  html.brText("Magvar: " + QLocale().toString(waypoint.magvar, 'f', 1) + " °");

  QList<MapAirway> airways;
  query->getAirwaysForWaypoint(airways, waypoint.id);

  if(!airways.isEmpty())
  {
    QStringList airwayTexts;
    for(const MapAirway& aw : airways)
    {
      QString txt(aw.name + ", " + maptypes::airwayTypeToString(aw.type));
      if(aw.minalt > 0)
        txt += ", " + QString::number(aw.minalt) + " ft";
      airwayTexts.append(txt);
    }

    if(!airwayTexts.isEmpty())
    {
      airwayTexts.sort();
      airwayTexts.removeDuplicates();

      html.brText().b("Airways:");

      for(const QString& aw : airwayTexts)
        html.brText(aw);
    }
  }
}

void MapHtmlInfoBuilder::airwayText(const MapAirway& airway, HtmlBuilder& html)
{
  html.b("Airway: " + airway.name);
  html.brText("Type: " + maptypes::airwayTypeToString(airway.type));

  if(airway.minalt > 0)
    html.brText("Min altitude: " + QString::number(airway.minalt) + " ft");
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
    html.text("Tower: " + QLocale().toString(airport.towerFrequency / 1000., 'f', 2));
  else
    html.text("Tower");
}

void MapHtmlInfoBuilder::parkingText(const MapParking& parking, HtmlBuilder& html)
{
  if(parking.type != "FUEL")
  {
    html.text(maptypes::parkingName(parking.name) + " " + QString::number(parking.number));
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
  html.text("Helipad:");
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
