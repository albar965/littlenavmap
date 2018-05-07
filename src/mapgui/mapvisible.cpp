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

#include "mapgui/mapvisible.h"

#include "mapgui/maplayer.h"
#include "mapgui/mappaintlayer.h"
#include "navapp.h"
#include "util/htmlbuilder.h"
#include "gui/mainwindow.h"
#include "common/maptypes.h"

#include <common/unit.h>

MapVisible::MapVisible(MapPaintLayer *paintLayerParam)
  : paintLayer(paintLayerParam)
{

}

MapVisible::~MapVisible()
{

}

/* Update the visible objects indication in the status bar. */
void MapVisible::updateVisibleObjectsStatusBar()
{
  if(!NavApp::hasDataInDatabase())
  {
    NavApp::getMainWindow()->setMapObjectsShownMessageText(tr("<b style=\"color:red\">Database empty.</b>"),
                                                           tr("The currently selected Scenery Database is empty."));
  }
  else
  {

    const MapLayer *layer = paintLayer->getMapLayer();

    if(layer != nullptr)
    {
      map::MapObjectTypes shown = paintLayer->getShownMapObjects();

      QStringList airportLabel;
      atools::util::HtmlBuilder tooltip(false);
      tooltip.b(tr("Currently shown on map:"));
      tooltip.table();

      // Collect airport information ==========================================================
      if(layer->isAirport() && ((shown& map::AIRPORT_HARD) || (shown& map::AIRPORT_SOFT) ||
                                (shown & map::AIRPORT_ADDON)))
      {
        QString runway, runwayShort;
        QString apShort, apTooltip, apTooltipAddon;
        bool showAddon = shown & map::AIRPORT_ADDON;
        bool showEmpty = shown & map::AIRPORT_EMPTY;
        bool showStock = (shown& map::AIRPORT_HARD) || (shown & map::AIRPORT_SOFT);
        bool showHard = shown & map::AIRPORT_HARD;
        bool showAny = showStock | showAddon;

        // Prepare runway texts
        if(!(shown& map::AIRPORT_HARD) && (shown & map::AIRPORT_SOFT))
        {
          runway = tr(" soft runways (S)");
          runwayShort = tr(",S");
        }
        else if((shown& map::AIRPORT_HARD) && !(shown & map::AIRPORT_SOFT))
        {
          runway = tr(" hard runways (H)");
          runwayShort = tr(",H");
        }
        else if((shown& map::AIRPORT_HARD) && (shown & map::AIRPORT_SOFT))
        {
          runway = tr(" all runway types (H,S)");
          runwayShort = tr(",H,S");
        }
        else if(!(shown& map::AIRPORT_HARD) && !(shown & map::AIRPORT_SOFT))
        {
          runway.clear();
          runwayShort.clear();
        }

        // Prepare short airport indicator
        if(showAddon)
          apShort = showEmpty ? tr("AP,A,E") : tr("AP,A");
        else if(showStock)
          apShort = showEmpty ? tr("AP,E") : tr("AP");

        // Build the rest of the strings
        if(layer->getDataSource() == layer::ALL)
        {
          if(layer->getMinRunwayLength() > 0)
          {
            if(showAny)
              apShort.append(">").append(QLocale().toString(layer->getMinRunwayLength() / 100)).append(runwayShort);

            if(showStock)
              apTooltip = tr("Airports (AP) with runway length > %1 and%2").
                          arg(Unit::distShortFeet(layer->getMinRunwayLength())).
                          arg(runway);

            if(showAddon)
              apTooltipAddon = tr("Add-on airports with runway length > %1").
                               arg(Unit::distShortFeet(layer->getMinRunwayLength()));
          }
          else
          {
            if(showStock)
            {
              apShort.append(runwayShort);
              apTooltip = tr("Airports (AP) with%1").arg(runway);
            }
            if(showAddon)
              apTooltipAddon = tr("Add-on airports (A)");
          }
        }
        else if(layer->getDataSource() == layer::MEDIUM)
        {
          if(showAny)
            apShort.append(tr(">%1%2").arg(Unit::distShortFeet(layer::MAX_MEDIUM_RUNWAY_FT / 100, false)).
                           arg(runwayShort));

          if(showStock)
            apTooltip = tr("Airports (AP) with runway length > %1 and%2").
                        arg(Unit::distShortFeet(layer::MAX_MEDIUM_RUNWAY_FT)).
                        arg(runway);

          if(showAddon)
            apTooltipAddon.append(tr("Add-on airports (A) with runway length > %1").
                                  arg(Unit::distShortFeet(layer::MAX_MEDIUM_RUNWAY_FT)));
        }
        else if(layer->getDataSource() == layer::LARGE)
        {
          if(showAddon || showHard)
            apShort.append(tr(">%1,H").arg(Unit::distShortFeet(layer::MAX_LARGE_RUNWAY_FT / 100, false)));
          else
            apShort.clear();

          if(showStock && showHard)
            apTooltip = tr("Airports (AP) with runway length > %1 and hard runways (H)").
                        arg(Unit::distShortFeet(layer::MAX_LARGE_RUNWAY_FT));

          if(showAddon)
            apTooltipAddon.append(tr("Add-on airports (A) with runway length > %1").
                                  arg(Unit::distShortFeet(layer::MAX_LARGE_RUNWAY_FT)));
        }

        airportLabel.append(apShort);

        if(!apTooltip.isEmpty())
          tooltip.tr().td(apTooltip).trEnd();

        if(!apTooltipAddon.isEmpty())
          tooltip.tr().td(apTooltipAddon).trEnd();

        if(showEmpty)
          tooltip.tr().td(tr("Empty airports (E) with zero rating")).trEnd();
      }

      QStringList navaidLabel, navaidsTooltip;
      // Collect navaid information ==========================================================
      if(layer->isVor() && shown & map::VOR)
      {
        navaidLabel.append(tr("V"));
        navaidsTooltip.append(tr("VOR (V)"));
      }
      if(layer->isNdb() && shown & map::NDB)
      {
        navaidLabel.append(tr("N"));
        navaidsTooltip.append(tr("NDB (N)"));
      }
      if(layer->isIls() && shown & map::ILS)
      {
        navaidLabel.append(tr("I"));
        navaidsTooltip.append(tr("ILS (I)"));
      }
      if(layer->isWaypoint() && shown & map::WAYPOINT)
      {
        navaidLabel.append(tr("W"));
        navaidsTooltip.append(tr("Waypoints (W)"));
      }
      if(layer->isAirway() && shown & map::AIRWAYJ)
      {
        navaidLabel.append(tr("JA"));
        navaidsTooltip.append(tr("Jet Airways (JA)"));
      }
      if(layer->isAirway() && shown & map::AIRWAYV)
      {
        navaidLabel.append(tr("VA"));
        navaidsTooltip.append(tr("Victor Airways (VA)"));
      }

      QStringList airspacesTooltip, airspaceGroupLabel, airspaceGroupTooltip;
      if(shown & map::AIRSPACE || shown & map::AIRSPACE_ONLINE)
      {
        map::MapAirspaceFilter airspaceFilter = paintLayer->getShownAirspacesTypesByLayer();
        // Collect airspace information ==========================================================
        for(int i = 0; i < map::MAP_AIRSPACE_TYPE_BITS; i++)
        {
          map::MapAirspaceTypes type(1 << i);
          if(airspaceFilter.types & type)
            airspacesTooltip.append(map::airspaceTypeToString(type));
        }
        std::sort(airspacesTooltip.begin(), airspacesTooltip.end());

        if(airspaceFilter.types & map::AIRSPACE_ICAO)
        {
          airspaceGroupLabel.append(tr("ICAO"));
          airspaceGroupTooltip.append(tr("Class A-E (ICAO)"));
        }

        if(airspaceFilter.types & map::AIRSPACE_FIR)
        {
          airspaceGroupLabel.append(tr("FIR"));
          airspaceGroupTooltip.append(tr("Flight Information Region, class F and/or G (FIR)"));
        }

        if(airspaceFilter.types & map::AIRSPACE_RESTRICTED)
        {
          airspaceGroupLabel.append(tr("RSTR"));
          airspaceGroupTooltip.append(tr("Restricted (RSTR)"));
        }

        if(airspaceFilter.types & map::AIRSPACE_SPECIAL)
        {
          airspaceGroupLabel.append(tr("SPEC"));
          airspaceGroupTooltip.append(tr("Special (SPEC)"));
        }

        if(airspaceFilter.types & map::AIRSPACE_OTHER || airspaceFilter.types & map::AIRSPACE_CENTER)
        {
          airspaceGroupLabel.append(tr("OTR"));
          airspaceGroupTooltip.append(tr("Centers and others (OTR)"));
        }
      }

      if(!navaidsTooltip.isEmpty())
        tooltip.tr().td().b(tr("Navaids: ")).text(navaidsTooltip.join(", ")).tdEnd().trEnd();
      else
        tooltip.tr().td(tr("No navaids")).trEnd();

      QString asText, asGroupText;
      if(shown & map::AIRSPACE_ONLINE && shown & map::AIRSPACE)
      {
        asText = tr("Airspaces and Online Centers: ");
        asGroupText = tr("Airspace and Online Center Groups: ");
      }
      else if(shown & map::AIRSPACE_ONLINE)
      {
        asText = tr("Online Centers: ");
        asGroupText = tr("Online Center Groups: ");
      }
      else if(shown & map::AIRSPACE)
      {
        asText = tr("Airspaces: ");
        asGroupText = tr("Airspace Groups: ");
      }

      if(!airspacesTooltip.isEmpty())
      {
        tooltip.tr().td().b(asGroupText).text(airspaceGroupTooltip.join(", ")).tdEnd().trEnd();
        tooltip.tr().td().b(asText).text(airspacesTooltip.join(", ")).tdEnd().trEnd();
      }
      else
        tooltip.tr().td(tr("No airspaces")).trEnd();

      QStringList aiLabel;
      QStringList ai;
      if(NavApp::isConnected())
      {
        // AI vehicles
        if(shown & map::AIRCRAFT_AI && layer->isAiAircraftLarge())
        {
          QString ac;
          if(!layer->isAiAircraftSmall())
          {
            ac = tr("Aircraft > %1 ft").arg(layer::LARGE_AIRCRAFT_SIZE);
            aiLabel.append("A>");
          }
          else
          {
            ac = tr("Aircraft");
            aiLabel.append("A");
          }

          if(layer->isAiAircraftGround())
          {
            ac.append(tr(" on ground"));
            aiLabel.last().append("G");
          }
          ai.append(ac);
        }

        if(shown & map::AIRCRAFT_AI_SHIP && layer->isAiShipLarge())
        {
          if(!layer->isAiShipSmall())
          {
            ai.append(tr("Ships > %1 ft").arg(layer::LARGE_SHIP_SIZE));
            aiLabel.append("S>");
          }
          else
          {
            ai.append(tr("Ships"));
            aiLabel.append("S");
          }
        }
      }
      if(shown & map::AIRCRAFT_AI && NavApp::isOnlineNetworkActive())
        ai.append(tr("%1 Clients / Aircraft").arg(NavApp::getOnlineNetwork()));

      if(!ai.isEmpty())
        tooltip.tr().td().b(tr("AI / multiplayer / online client: ")).text(ai.join(", ")).tdEnd().trEnd();
      else
        tooltip.tr().td(tr("No AI / Multiplayer / online client")).trEnd();
      tooltip.tableEnd();

      QStringList label;
      if(!airportLabel.isEmpty())
        label.append(airportLabel.join(tr(",")));
      if(!navaidLabel.isEmpty())
        label.append(navaidLabel.join(tr(",")));
      if(!airspaceGroupLabel.isEmpty())
        label.append(airspaceGroupLabel.join(tr(",")));
      if(!aiLabel.isEmpty())
        label.append(aiLabel.join(tr(",")));

      // Update the statusbar label text and tooltip of the label
      NavApp::getMainWindow()->setMapObjectsShownMessageText(label.join(" / "), tooltip.getHtml());
    }
  }
}
