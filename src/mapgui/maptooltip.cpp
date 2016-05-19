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

#include "maplayer.h"
#include "maptooltip.h"
#include "common/maptypes.h"
#include "mapgui/mapquery.h"
#include "common/formatter.h"
#include "route/routemapobjectlist.h"

#include <common/morsecode.h>
#include <common/weatherreporter.h>

using namespace maptypes;

MapTooltip::MapTooltip(QObject *parent, MapQuery *mapQuery, WeatherReporter *weatherReporter)
  : QObject(parent), query(mapQuery), weather(weatherReporter)
{
  morse = new MorseCode("&nbsp;", "&nbsp;&nbsp;&nbsp;");
}

MapTooltip::~MapTooltip()
{
  delete morse;
}

QString MapTooltip::buildTooltip(const maptypes::MapSearchResult& mapSearchResult,
                                 const RouteMapObjectList& routeMapObjects,
                                 bool airportDiagram)
{
  QStringList text;
  for(const MapAirport& ap : mapSearchResult.airports)
  {
    qDebug() << "Airport" << ap.ident << "id" << ap.id;

    if(checkText(text))
      break;

    if(!text.isEmpty())
      text += "<hr/>";

    text += "<b>" + ap.name + " (" + ap.ident + ")</b>";
    QString city, state, country;
    query->getAirportAdminById(ap.id, city, state, country);
    text += "<br/><b>" + city;
    if(!state.isEmpty())
      text += ", " + state;
    text += ", " + country + "</b>";

    QString metar;
    if(weather->hasAsnWeather())
    {
      metar = weather->getAsnMetar(ap.ident);
      if(!metar.isEmpty())
        text += "<br/>Metar (ASN): " + metar;
    }
    // else
    {
      metar = weather->getNoaaMetar(ap.ident);
      if(!metar.isEmpty())
        text += "<br/>Metar (NOAA): " + metar;
      metar = weather->getVatsimMetar(ap.ident);
      if(!metar.isEmpty())
        text += "<br/>Metar (Vatsim): " + metar;
    }

    if(ap.routeIndex == 0)
      text += "<br/>Route start airport";
    else if(ap.routeIndex == routeMapObjects.size() - 1)
      text += "<br/>Route destination airport";
    else if(ap.routeIndex != -1)
      text += "<br/>Route position " + QString::number(ap.routeIndex + 1);

    text += "<br/>Longest Runway: " + QLocale().toString(ap.longestRunwayLength) + " ft";
    text += "<br/>Altitude: " + QLocale().toString(ap.getPosition().getAltitude(), 'f', 0) + " ft";
    text += "<br/>Magvar: " + formatter::formatDoubleUnit(ap.magvar, QString(), 1) + " °";

    if(ap.towerFrequency > 0)
      text += "<br/>Tower: " + formatter::formatDoubleUnit(ap.towerFrequency / 1000., QString(), 2);
    if(ap.atisFrequency > 0)
      text += "<br/>ATIS: " + formatter::formatDoubleUnit(ap.atisFrequency / 1000., QString(), 2);
    if(ap.awosFrequency > 0)
      text += "<br/>AWOS: " + formatter::formatDoubleUnit(ap.awosFrequency / 1000., QString(), 2);
    if(ap.asosFrequency > 0)
      text += "<br/>ASOS: " + formatter::formatDoubleUnit(ap.asosFrequency / 1000., QString(), 2);
    if(ap.unicomFrequency > 0)
      text += "<br/>Unicom: " + formatter::formatDoubleUnit(ap.unicomFrequency / 1000., QString(), 2);

    if(ap.addon())
      text += "<br/>Add-on Airport";
    if(ap.flags.testFlag(AP_MIL))
      text += "<br/>Military Airport";
    if(ap.scenery())
      text += "<br/>Has some Scenery Elements";

    if(ap.hard())
      text += "<br/>Has Hard Runways";
    if(ap.soft())
      text += "<br/>Has Soft Runways";
    if(ap.water())
      text += "<br/>Has Water Runways";
    if(ap.helipad())
      text += "<br/>Has Helipads";
    if(ap.flags.testFlag(AP_LIGHT))
      text += "<br/>Has Lighted Runways";
    if(ap.flags.testFlag(AP_AVGAS))
      text += "<br/>Has Avgas";
    if(ap.flags.testFlag(AP_JETFUEL))
      text += "<br/>Has Jetfuel";

    if(ap.flags.testFlag(AP_ILS))
      text += "<br/>Has ILS";
    if(ap.flags.testFlag(AP_APPR))
      text += "<br/>Has Approaches";
  }

  for(const MapVor& vor : mapSearchResult.vors)
  {
    qDebug() << "Vor" << vor.ident << "id" << vor.id;

    if(checkText(text))
      break;

    if(!text.isEmpty())
      text += "<hr/>";
    QString type = maptypes::vorType(vor);
    text += "<b>" + type + ": " + vor.name + " (" + vor.ident + ")</b>";

    if(vor.routeIndex >= 0)
      text += "<br/>Route position " + QString::number(vor.routeIndex + 1);

    text += "<br/>Type: " + maptypes::navTypeName(vor.type);
    text += "<br/>Region: " + vor.region;
    text += "<br/>Freq: " + formatter::formatDoubleUnit(vor.frequency / 1000., QString(), 2) + " MHz";
    if(!vor.dmeOnly)
      text += "<br/>Magvar: " + formatter::formatDoubleUnit(vor.magvar, QString(), 1) + " °";
    text += "<br/>Altitude: " + QLocale().toString(vor.getPosition().getAltitude(), 'f', 0) + " ft";
    text += "<br/>Range: " + QString::number(vor.range) + " nm";
    text += "<br/><b>" + morse->getCode(vor.ident) + "</b>";
  }

  for(const MapNdb& ndb : mapSearchResult.ndbs)
  {
    qDebug() << "Ndb" << ndb.ident << "id" << ndb.id;

    if(checkText(text))
      break;

    if(!text.isEmpty())
      text += "<hr/>";
    text += "<b>NDB: " + ndb.name + " (" + ndb.ident + ")</b>";
    if(ndb.routeIndex >= 0)
      text += "<br/>Route position " + QString::number(ndb.routeIndex + 1);
    text += "<br/>Type: " + maptypes::navTypeName(ndb.type);
    text += "<br/>Region: " + ndb.region;
    text += "<br/>Freq: " + formatter::formatDoubleUnit(ndb.frequency / 100., QString(), 2) + " kHz";
    text += "<br/>Magvar: " + formatter::formatDoubleUnit(ndb.magvar, QString(), 1) + " °";
    text += "<br/>Altitude: " + QLocale().toString(ndb.getPosition().getAltitude(), 'f', 0) + " ft";
    text += "<br/>Range: " + QString::number(ndb.range) + " nm";
    text += "<br/><b>" + morse->getCode(ndb.ident) + "</b>";
  }

  for(const MapWaypoint& wp : mapSearchResult.waypoints)
  {
    qDebug() << "Waypoint" << wp.ident << "id" << wp.id;

    if(checkText(text))
      break;

    if(!text.isEmpty())
      text += "<hr/>";
    text += "<b>Waypoint: " + wp.ident + "</b>";
    if(wp.routeIndex >= 0)
      text += "<br/>Route position " + QString::number(wp.routeIndex + 1);
    text += "<br/>Type: " + maptypes::navTypeName(wp.type);
    text += "<br/>Region: " + wp.region;
    text += "<br/>Magvar: " + formatter::formatDoubleUnit(wp.magvar, QString(), 1) + " °";

    QList<MapAirway> airways;
    query->getAirwaysForWaypoint(airways, wp.id);

    if(!airways.isEmpty())
    {
      QStringList airwayTexts;
      for(const MapAirway& aw : airways)
      {
        if(checkText(text))
          break;

        QString txt("<br/>" + aw.name + ", " + maptypes::airwayTypeToString(aw.type));
        if(aw.minalt > 0)
          txt += ", " + QString::number(aw.minalt) + " ft";
        airwayTexts.append(txt);
      }

      if(!airwayTexts.isEmpty())
      {
        airwayTexts.sort();
        airwayTexts.removeDuplicates();

        text += "<br/><b>Airways:</b>";
        text += airwayTexts.join("");
      }
    }
  }

  for(const MapAirway& airway : mapSearchResult.airways)
  {
    qDebug() << "Airway" << airway.name << "id" << airway.id;

    if(checkText(text))
      break;

    if(!text.isEmpty())
      text += "<hr/>";
    text += "<b>Airway: " + airway.name + "</b>";
    text += "<br/>Type: " + maptypes::airwayTypeToString(airway.type);

    if(airway.minalt > 0)
      text += "<br/>" + QString::number(airway.minalt) + " ft";
  }

  for(const MapMarker& m : mapSearchResult.markers)
  {
    if(checkText(text))
      break;

    if(!text.isEmpty())
      text += "<hr/>";
    text += "<b>Marker: " + m.type + "</b>";
  }

  if(airportDiagram)
  {
    for(const MapAirport& ap : mapSearchResult.towers)
    {
      if(checkText(text))
        break;

      if(!text.isEmpty())
        text += "<hr/>";

      if(ap.towerFrequency > 0)
        text += "Tower: " + formatter::formatDoubleUnit(ap.towerFrequency / 1000., QString(), 2);
      else
        text += "Tower";
    }
    for(const MapParking& p : mapSearchResult.parkings)
    {
      if(checkText(text))
        break;

      if(!text.isEmpty())
        text += "<hr/>";

      if(p.type != "FUEL")
        text += maptypes::parkingName(p.name) + " " + QString::number(p.number) +
                "<br/>" + maptypes::parkingTypeName(p.type) +
                "<br/>" + QString::number(p.radius * 2) + " ft" +
                (p.jetway ? "<br/>Has Jetway" : "");
      else
        text += "Fuel";

    }
    for(const MapHelipad& p : mapSearchResult.helipads)
    {
      if(!text.isEmpty())
        text += "<hr/>";

      text += QString("Helipad:") +
              "<br/>Surface: " + maptypes::surfaceName(p.surface) +
              "<br/>Type: " + p.type +
              "<br/>" + QString::number(p.width) + " ft" +
              (p.closed ? "<br/>Is Closed" : "");

    }
  }
  for(const MapUserpoint& up : mapSearchResult.userPoints)
  {
    if(checkText(text))
      break;

    if(!text.isEmpty())
      text += "<hr/>";

    text += QString("<b>Routepoint:</b>") +
            "<br/>" + up.name;
  }
  return text.join("");
}

bool MapTooltip::checkText(QStringList& text)
{
  static QString dotText(tr("<b>More ...</b>"));
  if(text.size() > MAXLINES)
  {
    if(text.last() != dotText)
    {
      text += "<hr/>";
      text += dotText;
    }
    return true;
  }
  return false;
}
