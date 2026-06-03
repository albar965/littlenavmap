/*****************************************************************************
* Copyright 2015-2026 Alexander Barthel alex@littlenavmap.org
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

#include "mapgui/maptooltip.h"

#include "app/navapp.h"
#include "common/htmlinfobuilder.h"
#include "common/maptypes.h"
#include "gui/mainwindow.h"
#include "mapgui/mappaintwidget.h"
#include "query/airspacequeries.h"
#include "route/route.h"
#include "sql/sqlrecord.h"
#include "util/htmlbuilder.h"
#include "weather/weathercontext.h"
#include "weather/weathercontexthandler.h"
#include "weather/windreporter.h"

#include <QPalette>
#include <QToolTip>

using namespace map;
using atools::util::HtmlBuilder;
using atools::fs::sc::SimConnectAircraft;
using atools::fs::sc::SimConnectUserAircraft;

MapTooltip::MapTooltip(MainWindow *parentWindow)
  : mainWindow(parentWindow), weather(NavApp::getWeatherReporter())
{
  qDebug() << Q_FUNC_INFO;
}

MapTooltip::~MapTooltip()
{
  qDebug() << Q_FUNC_INFO;
}

template<typename TYPE>
void MapTooltip::buildOneTooltip(atools::util::HtmlBuilder& html, bool& overflow, int& numEntries, const QList<TYPE>& list,
                                 const HtmlInfoBuilder& info, const Route *route,
                                 void (HtmlInfoBuilder::*func)(const TYPE&, atools::util::HtmlBuilder&, const Route *) const) const
{
  if(!overflow)
  {
    for(const TYPE& type : list)
    {
      if(checkText(html))
      {
        overflow = true;
        break;
      }

      if(!html.isEmpty())
        html.hr();

      (info.*func)(type, html, route);

      numEntries++;
    }
  }
}

template<typename TYPE>
void MapTooltip::buildOneTooltipRt(atools::util::HtmlBuilder& html, bool& overflow, bool& distance, int& numEntries,
                                   const QList<TYPE>& list, const HtmlInfoBuilder& info, const Route *route,
                                   void (HtmlInfoBuilder::*func)(const TYPE&, atools::util::HtmlBuilder&, const Route *) const) const
{
  if(!overflow)
  {
    for(const TYPE& type : list)
    {
      if(checkText(html))
      {
        overflow = true;
        break;
      }

      if(!html.isEmpty())
        html.hr();

      (info.*func)(type, html, route);

      if(type.routeIndex >= 0)
        distance = false;

      numEntries++;
    }
  }
}

QString MapTooltip::buildTooltip(const map::MapResult& mapSearchResult, const atools::geo::Pos& pos, const Route *route,
                                 bool airportDiagram, optsd::DisplayTooltipOptions options, const QString& prefix)
{
#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << mapSearchResult;
#endif

  HtmlBuilder html(false /* backgroundColorUsed */, NavApp::isGuiStyleDark());

  if(!prefix.isEmpty())
    html.small(prefix);

  HtmlInfoBuilder info(QueryManager::instance()->getQueriesGui(), false /* info */, false /* print */,
                       options.testFlag(optsd::TOOLTIP_VERBOSE));

  int fontPixelSize = atools::roundToInt(QFontMetricsF(QToolTip::font()).height() * 1.1);
  info.setSymbolSizeTitle(QSize(fontPixelSize, fontPixelSize));

  fontPixelSize = atools::roundToInt(QFontMetricsF(QToolTip::font()).height() * 1.3);
  info.setSymbolSizeVehicle(QSize(fontPixelSize, fontPixelSize));

  int numEntries = 0;
  bool bearing = true, // Suppress bearing for user aircraft
       distance = true, // No distance to last flight plan leg for route legs
       overflow = false;

  // Append HTML text for all objects found in order of importance (airports first, etc.)
  // Objects are separated by a horizontal ruler
  // If max number of entries or lines is exceeded return the html

  // User Aircraft ===========================================================================
  if(!overflow && options.testFlag(optsd::TOOLTIP_AIRCRAFT_USER))
  {
    if(mapSearchResult.userAircraft.isValid())
    {
      if(checkText(html))
        overflow = true;
      else
      {
        if(!html.isEmpty())
          html.hr();

        info.aircraftText(mapSearchResult.userAircraft.getAircraft(), html);
        info.aircraftProgressText(mapSearchResult.userAircraft.getAircraft(), html, route);
        bearing = false;
        numEntries++;
      }
    }
  }

  if(options.testFlag(optsd::TOOLTIP_AIRCRAFT_AI))
  {
    // Online Aircraft ===========================================================================
    if(!overflow)
    {
      for(const map::MapOnlineAircraft& aircraft : mapSearchResult.onlineAircraft)
      {
        if(checkText(html))
        {
          overflow = true;
          break;
        }

        if(!html.isEmpty())
          html.hr();

        info.aircraftText(aircraft.getAircraft(), html);
        info.aircraftProgressText(aircraft.getAircraft(), html, route);

        numEntries++;
      }
    }

    // AI Aircraft ===========================================================================
    if(!overflow)
    {
      for(const map::MapAiAircraft& aircraft : mapSearchResult.aiAircraft)
      {
        if(checkText(html))
        {
          overflow = true;
          break;
        }

        if(!html.isEmpty())
          html.hr();

        info.aircraftText(aircraft.getAircraft(), html);
        info.aircraftProgressText(aircraft.getAircraft(), html, route);

        numEntries++;
      }
    }
  }

  // Navaids from procedure points ===========================================================================
  if(!overflow)
  {
    if(options.testFlag(optsd::TOOLTIP_NAVAID))
    {
      for(const map::MapProcedurePoint& pt : mapSearchResult.procPoints)
      {
        if(checkText(html))
        {
          overflow = true;
          break;
        }

        if(!html.isEmpty())
          html.hr();

        info.procedurePointText(pt, html, route);

        distance = false; // do not show distance to last leg

        numEntries++;
      }
    }
  }

  // Map Markers ===================================================================
  if(options.testFlag(optsd::TOOLTIP_MARKS))
  {
    // Measurment lines
    buildOneTooltip(html, overflow, numEntries, mapSearchResult.distanceMarks, info, route, &HtmlInfoBuilder::distanceMarkerText);

    // Range rings
    buildOneTooltip(html, overflow, numEntries, mapSearchResult.rangeMarks, info, route, &HtmlInfoBuilder::rangeMarkerText);

    // Traffic pattern
    buildOneTooltip(html, overflow, numEntries, mapSearchResult.patternMarks, info, route, &HtmlInfoBuilder::patternMarkerText);

    // User Holds
    buildOneTooltip(html, overflow, numEntries, mapSearchResult.holdingMarks, info, route, &HtmlInfoBuilder::holdingMarkerText);

    // MSA diagrams
    buildOneTooltip(html, overflow, numEntries, mapSearchResult.msaMarks, info, route, &HtmlInfoBuilder::msaMarkerText);

  }

  if(options.testFlag(optsd::TOOLTIP_NAVAID))
  {
    // Logbook entries
    buildOneTooltip(html, overflow, numEntries, mapSearchResult.logbookEntries, info, route, &HtmlInfoBuilder::logEntryTextInfo);

    // Userpoints
    buildOneTooltip(html, overflow, numEntries, mapSearchResult.userpoints, info, route, &HtmlInfoBuilder::userpointTextInfo);
  }

  // Airports ===========================================================================
  if(!overflow)
  {
    if(options.testFlag(optsd::TOOLTIP_AIRPORT))
    {
      for(const MapAirport& airport : mapSearchResult.airports)
      {
        if(checkText(html))
        {
          overflow = true;
          break;
        }

        if(!html.isEmpty())
          html.hr();

        map::WeatherContext currentWeatherContext;
        mainWindow->getWeatherContextHandler()->buildWeatherContextTooltip(currentWeatherContext, airport);
        info.airportText(airport, currentWeatherContext, html, route);

        if(airport.routeIndex >= 0)
          distance = false; // do not show distance to last leg

        numEntries++;
      }
    }
  }

  // Departure parking ===========================================================================
  if(route != nullptr && !route->isEmpty() && !overflow && options.testFlag(optsd::TOOLTIP_AIRPORT) && mapSearchResult.parkings.size() == 1)
  {
    // Do not show distance if departure parking is the only one in the list
    if(mapSearchResult.parkings.constFirst().id == route->getDepartureParking().id)
      distance = false; // do not show distance to last leg
  }

  // Aircraft trail point ===========================================================================
  if(options.testFlag(optsd::TOOLTIP_AIRCRAFT_TRAIL))
  {
    if(!overflow && mapSearchResult.trailSegment.isValid())
    {
      if(checkText(html))
        overflow = true;
      else
      {
        if(!html.isEmpty())
          html.hr();

        info.aircraftTrailText(mapSearchResult.trailSegment, html, false /* logbook */);
        distance = false; // do not show distance to last leg
        numEntries++;
      }
    }

    // Aircraft trail point from logbook preview ===========================================================================
    if(!overflow && mapSearchResult.trailSegmentLog.isValid())
    {
      if(checkText(html))
        overflow = true;
      else
      {
        if(!html.isEmpty())
          html.hr();

        info.aircraftTrailText(mapSearchResult.trailSegmentLog, html, true /* logbook */);
        distance = false; // do not show distance to last leg
        numEntries++;
      }
    }
  }

  // Navaids ===========================================================================
  if(options.testFlag(optsd::TOOLTIP_NAVAID))
  {
    buildOneTooltipRt(html, overflow, distance, numEntries, mapSearchResult.vors, info, route, &HtmlInfoBuilder::vorText);
    buildOneTooltipRt(html, overflow, distance, numEntries, mapSearchResult.ndbs, info, route, &HtmlInfoBuilder::ndbText);
    buildOneTooltipRt(html, overflow, distance, numEntries, mapSearchResult.waypoints, info, route, &HtmlInfoBuilder::waypointText);
    buildOneTooltip(html, overflow, numEntries, mapSearchResult.markers, info, route, &HtmlInfoBuilder::markerText);
    buildOneTooltip(html, overflow, numEntries, mapSearchResult.ils, info, route, &HtmlInfoBuilder::ilsTextInfo);

    // Database Holds ===========================================================================
    buildOneTooltip(html, overflow, numEntries, mapSearchResult.holdings, info, route, &HtmlInfoBuilder::holdingText);

    // Database MSA ===========================================================================
    buildOneTooltip(html, overflow, numEntries, mapSearchResult.airportMsa, info, route, &HtmlInfoBuilder::airportMsaText);
  }

  // Airport stuff ===========================================================================
  if(airportDiagram && options.testFlag(optsd::TOOLTIP_AIRPORT))
  {
    buildOneTooltip(html, overflow, numEntries, mapSearchResult.towers, info, route, &HtmlInfoBuilder::towerText);
    buildOneTooltip(html, overflow, numEntries, mapSearchResult.parkings, info, route, &HtmlInfoBuilder::parkingText);
    buildOneTooltip(html, overflow, numEntries, mapSearchResult.helipads, info, route, &HtmlInfoBuilder::helipadText);
    buildOneTooltip(html, overflow, numEntries, mapSearchResult.starts, info, route, &HtmlInfoBuilder::startText);
  }

  if(options.testFlag(optsd::TOOLTIP_NAVAID))
  {
    buildOneTooltipRt(html, overflow, distance, numEntries, mapSearchResult.userpointsRoute, info, route,
                      &HtmlInfoBuilder::userpointTextRoute);
    buildOneTooltip(html, overflow, numEntries, mapSearchResult.airways, info, route, &HtmlInfoBuilder::airwayText);
  }

  // High altitude wind barbs - not flight plan ===========================================================================
  if(!overflow)
  {
    if(options.testFlag(optsd::TOOLTIP_WIND) && mapSearchResult.windPos.isValid())
    {
      WindReporter *windReporter = NavApp::getWindReporter();
      atools::grib::WindPosList winds = windReporter->getWindStackForPos(mapSearchResult.windPos);
      if(!winds.isEmpty())
      {
        if(checkText(html))
          overflow = true;
        else
        {
          if(!html.isEmpty())
            html.hr();

          // Show wind stack with barb notation for layer
          info.windText(winds, html, map::INVALID_ALTITUDE_VALUE, false /* table */, route);

#ifdef DEBUG_INFORMATION_WIND
          html.hr().small(QStringLiteral("Pos(%1, %2), alt(%3)").
                          arg(mapSearchResult.windPos.getLonX()).arg(mapSearchResult.windPos.getLatY()).
                          arg(windReporter->getAltitudeFt(), 0, 'f', 2)).br();

          html.small(windReporter->getDebug(mapSearchResult.windPos));
#endif
          numEntries++;
        }
      }
    }
  }

  // Airspaces ===========================================================================
  if(!overflow)
  {
    if(options.testFlag(optsd::TOOLTIP_AIRSPACE))
    {
      // Put all online airspace on top of the list to have consistent ordering with menus and info windows
      const MapResult res = mapSearchResult.moveOnlineAirspacesToFront();

      for(const MapAirspace& airspace : res.airspaces)
      {
        if(checkText(html))
        {
          overflow = true;
          break;
        }

        if(!html.isEmpty())
          html.hr();

        atools::sql::SqlRecord onlineRec;
        if(airspace.isOnline())
          onlineRec = QueryManager::instance()->getQueriesGui()->getAirspaceQueries()->getOnlineAirspaceRecordById(airspace.id);

        info.airspaceText(airspace, onlineRec, html);

        numEntries++;
      }
    }
  }

  // Prepend distance and bearing information ================================
  QString str;
  if(!html.isEmpty())
  {
    HtmlBuilder temp = html.cleared();
    info.bearingAndDistanceTexts(pos, NavApp::getMagVar(pos), temp, bearing, distance, route);
    if(!temp.isEmpty())
      temp.hr();
    str = temp.getHtml() + html.getHtml();

    if(str.endsWith("<br/>"))
      str.chop(5);
  }

#ifdef DEBUG_INFORMATION_TOOLTIP
  qDebug().noquote().nospace() << Q_FUNC_INFO << Qt::endl
                               << "-----------------------------------" << Qt::endl
                               << str << Qt::endl
                               << "-----------------------------------";
#endif

  return str;
}

/* Check if the result HTML has more than the allowed number of lines and add a "more" text */
bool MapTooltip::checkText(HtmlBuilder& html) const
{
  return html.checkLengthHr(MAX_LINES, tr("More ..."));
}
