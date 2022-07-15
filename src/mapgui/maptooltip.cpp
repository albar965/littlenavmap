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

#include "mapgui/maptooltip.h"

#include "common/maptypes.h"
#include "navapp.h"
#include "util/htmlbuilder.h"
#include "common/htmlinfobuilder.h"
#include "gui/mainwindow.h"
#include "route/route.h"
#include "airspace/airspacecontroller.h"
#include "options/optiondata.h"
#include "sql/sqlrecord.h"
#include "weather/windreporter.h"
#include "grib/windquery.h"
#include "route/routealtitudeleg.h"
#include "mapgui/mappaintwidget.h"

#include <QPalette>
#include <QToolTip>

using namespace map;
using atools::util::HtmlBuilder;
using atools::fs::sc::SimConnectAircraft;
using atools::fs::sc::SimConnectUserAircraft;

/* Number of characters in the divider */
static const int TEXT_BAR_LENGTH = 20;

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
                                 const HtmlInfoBuilder& info,
                                 void (HtmlInfoBuilder::*func)(const TYPE&, atools::util::HtmlBuilder&) const) const
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
        html.textBar(TEXT_BAR_LENGTH);

      (info.*func)(type, html);

      numEntries++;
    }
  }
}

template<typename TYPE>
void MapTooltip::buildOneTooltipRt(atools::util::HtmlBuilder& html, bool& overflow, bool& distance, int& numEntries,
                                   const QList<TYPE>& list, const HtmlInfoBuilder& info,
                                   void (HtmlInfoBuilder::*func)(const TYPE&, atools::util::HtmlBuilder&) const) const
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
        html.textBar(TEXT_BAR_LENGTH);

      (info.*func)(type, html);

      if(type.routeIndex >= 0)
        distance = false;

      numEntries++;
    }
  }
}

QString MapTooltip::buildTooltip(const map::MapResult& mapSearchResult, const atools::geo::Pos& pos, const Route& route,
                                 bool airportDiagram)
{
  optsd::DisplayTooltipOptions opts = OptionData::instance().getDisplayTooltipOptions();

#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << mapSearchResult;
#endif

  HtmlBuilder html(false);
  HtmlInfoBuilder info(mainWindow, NavApp::getMapPaintWidgetGui(),
                       false /* infoParam */, false /* infoParam */, opts.testFlag(optsd::TOOLTIP_VERBOSE));
  int numEntries = 0;
  bool bearing = true, // Suppress bearing for user aircraft
       distance = true, // No distance to last flight plan leg for route legs
       overflow = false;

  // Append HTML text for all objects found in order of importance (airports first, etc.)
  // Objects are separated by a horizontal ruler
  // If max number of entries or lines is exceeded return the html

  // User Aircraft ===========================================================================
  if(!overflow && opts.testFlag(optsd::TOOLTIP_AIRCRAFT_USER))
  {
    if(mapSearchResult.userAircraft.isValid())
    {
      if(checkText(html))
      {
        overflow = true;
      }
      else
      {
        if(!html.isEmpty())
          html.textBar(TEXT_BAR_LENGTH);

        info.aircraftText(mapSearchResult.userAircraft.getAircraft(), html);
        info.aircraftProgressText(mapSearchResult.userAircraft.getAircraft(), html, route);
        bearing = false;
        numEntries++;
      }
    }
  }

  if(opts.testFlag(optsd::TOOLTIP_AIRCRAFT_AI))
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
          html.textBar(TEXT_BAR_LENGTH);

        info.aircraftText(aircraft.getAircraft(), html);
        info.aircraftProgressText(aircraft.getAircraft(), html, Route());

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
          html.textBar(TEXT_BAR_LENGTH);

        info.aircraftText(aircraft.getAircraft(), html);
        info.aircraftProgressText(aircraft.getAircraft(), html, Route());

        numEntries++;
      }
    }
  }

  // Navaids from procedure points ===========================================================================
  if(!overflow)
  {
    if(opts.testFlag(optsd::TOOLTIP_NAVAID))
    {
      for(const map::MapProcedurePoint& pt : mapSearchResult.procPoints)
      {
        if(checkText(html))
        {
          overflow = true;
          break;
        }

        if(!html.isEmpty())
          html.textBar(TEXT_BAR_LENGTH);

        info.procedurePointText(pt, html, &route);

        distance = false; // do not show distance to last leg

        numEntries++;
      }
    }
  }

  // User features / marks ===================================================================

  if(opts.testFlag(optsd::TOOLTIP_MARKS))
  {
    // Traffic pattern
    buildOneTooltip(html, overflow, numEntries, mapSearchResult.patternMarks, info, &HtmlInfoBuilder::patternMarkerText);

    // Range rings
    buildOneTooltip(html, overflow, numEntries, mapSearchResult.rangeMarks, info, &HtmlInfoBuilder::rangeMarkerText);

    // User Holds
    buildOneTooltip(html, overflow, numEntries, mapSearchResult.holdingMarks, info, &HtmlInfoBuilder::holdingMarkerText);

    // MSA diagrams
    buildOneTooltip(html, overflow, numEntries, mapSearchResult.msaMarks, info, &HtmlInfoBuilder::msaMarkerText);

    // MSA diagrams
    buildOneTooltip(html, overflow, numEntries, mapSearchResult.distanceMarks, info, &HtmlInfoBuilder::distanceMarkerText);
  }

  if(opts.testFlag(optsd::TOOLTIP_NAVAID))
  {
    // Logbook entries
    buildOneTooltip(html, overflow, numEntries, mapSearchResult.logbookEntries, info, &HtmlInfoBuilder::logEntryTextInfo);

    // Userpoints
    buildOneTooltip(html, overflow, numEntries, mapSearchResult.userpoints, info, &HtmlInfoBuilder::userpointTextInfo);
  }

  // Airports ===========================================================================
  if(!overflow)
  {
    if(opts.testFlag(optsd::TOOLTIP_AIRPORT))
    {
      for(const MapAirport& airport : mapSearchResult.airports)
      {
        if(checkText(html))
        {
          overflow = true;
          break;
        }

        if(!html.isEmpty())
          html.textBar(TEXT_BAR_LENGTH);

        map::WeatherContext currentWeatherContext;

        mainWindow->buildWeatherContextForTooltip(currentWeatherContext, airport);
        info.airportText(airport, currentWeatherContext, html, &route);

        if(airport.routeIndex >= 0)
          distance = false; // do not show distance to last leg

        numEntries++;
      }
    }
  }

  // Navaids ===========================================================================
  if(opts.testFlag(optsd::TOOLTIP_NAVAID))
  {
    buildOneTooltipRt(html, overflow, distance, numEntries, mapSearchResult.vors, info, &HtmlInfoBuilder::vorText);
    buildOneTooltipRt(html, overflow, distance, numEntries, mapSearchResult.ndbs, info, &HtmlInfoBuilder::ndbText);
    buildOneTooltipRt(html, overflow, distance, numEntries, mapSearchResult.waypoints, info, &HtmlInfoBuilder::waypointText);
    buildOneTooltip(html, overflow, numEntries, mapSearchResult.markers, info, &HtmlInfoBuilder::markerText);
    buildOneTooltip(html, overflow, numEntries, mapSearchResult.ils, info, &HtmlInfoBuilder::ilsTextInfo);

    // Database Holds ===========================================================================
    buildOneTooltip(html, overflow, numEntries, mapSearchResult.holdings, info, &HtmlInfoBuilder::holdingText);

    // Database MSA ===========================================================================
    buildOneTooltip(html, overflow, numEntries, mapSearchResult.airportMsa, info, &HtmlInfoBuilder::airportMsaText);
  }

  // Airport stuff ===========================================================================
  if(airportDiagram && opts.testFlag(optsd::TOOLTIP_AIRPORT))
  {
    buildOneTooltip(html, overflow, numEntries, mapSearchResult.towers, info, &HtmlInfoBuilder::towerText);
    buildOneTooltip(html, overflow, numEntries, mapSearchResult.parkings, info, &HtmlInfoBuilder::parkingText);
    buildOneTooltip(html, overflow, numEntries, mapSearchResult.helipads, info, &HtmlInfoBuilder::helipadText);
  }

  if(opts.testFlag(optsd::TOOLTIP_NAVAID))
  {
    buildOneTooltipRt(html, overflow, distance, numEntries, mapSearchResult.userpointsRoute, info, &HtmlInfoBuilder::userpointTextRoute);
    buildOneTooltip(html, overflow, numEntries, mapSearchResult.airways, info, &HtmlInfoBuilder::airwayText);
  }

  // High altitude winds ===========================================================================
  if(!overflow)
  {
    if(opts.testFlag(optsd::TOOLTIP_WIND) && mapSearchResult.windPos.isValid())
    {
      WindReporter *windReporter = NavApp::getWindReporter();
      atools::grib::WindPosVector winds = windReporter->getWindStackForPos(mapSearchResult.windPos);
      if(!winds.isEmpty())
      {
        if(checkText(html))
        {
          overflow = true;
        }
        else
        {
          if(!html.isEmpty())
            html.textBar(TEXT_BAR_LENGTH);

          info.windText(winds, html, windReporter->getAltitudeFt(), windReporter->getSourceText());

#ifdef DEBUG_INFORMATION
          html.hr().small(QString("Pos(%1, %2), alt(%3)").
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
    if(opts.testFlag(optsd::TOOLTIP_AIRSPACE))
    {
      // Put all online airspace on top of the list to have consistent ordering with menus and info windows
      MapResult res = mapSearchResult.moveOnlineAirspacesToFront();

      for(const MapAirspace& airspace : res.airspaces)
      {
        if(checkText(html))
        {
          overflow = true;
          break;
        }

        if(!html.isEmpty())
          html.textBar(TEXT_BAR_LENGTH);

        atools::sql::SqlRecord onlineRec;
        if(airspace.isOnline())
          onlineRec = NavApp::getAirspaceController()->getOnlineAirspaceRecordById(airspace.id);

        info.airspaceText(airspace, onlineRec, html);

        numEntries++;
      }
    }
  }

  QString str;

  // Prepend distance and bearing information ================================
  if(!html.isEmpty())
  {
    HtmlBuilder temp = html.cleared();
    info.bearingAndDistanceTexts(pos, NavApp::getMagVar(pos), temp, bearing, distance);
    if(!temp.isEmpty())
      temp.textBar(TEXT_BAR_LENGTH);
    str = temp.getHtml() + html.getHtml();

    if(str.endsWith("<br/>"))
      str.chop(5);
  }

#ifdef DEBUG_INFORMATION_TOOLTIP

  qDebug().noquote().nospace() << Q_FUNC_INFO << html.getHtml();

#endif

  return str;

}

/* Check if the result HTML has more than the allowed number of lines and add a "more" text */
bool MapTooltip::checkText(HtmlBuilder& html) const
{
  return html.checklengthTextBar(MAX_LINES, tr("More ..."), TEXT_BAR_LENGTH);
}
