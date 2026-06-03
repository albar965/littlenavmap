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

#include "gui/contextmenutool.h"

#include "app/navapp.h"
#include "gui/actiontool.h"
#include "route/route.h"

using atools::gui::ActionTool;

void ContextMenuTool::initAirportActions(const map::MapAirport& airport, const Route& route, int routeLegIndex, const QString& objectText)
{
  actionShowProcedures->setDisabled(true);
  actionShowApproach->setDisabled(true);
  actionShowDeparture->setDisabled(true);

  actionShowApproach->setText(tr("Select as &Destination{hint}"));
  actionShowDeparture->setText(tr("&Select as Departure{hint}"));

  if(airport.isValid())
  {
    bool departureFilter, arrivalFilter, hasDeparture, hasAnyArrival, airportDeparture, airportDestination, airportAlternate,
         airportRoundTrip;
    route.getAirportProcedureFlags(airport, routeLegIndex, departureFilter, arrivalFilter, hasDeparture, hasAnyArrival,
                                   airportDeparture, airportDestination, airportAlternate, airportRoundTrip);
    bool noRunways = airport.noRunways();

    // Procedure menu item ================================================================
    if(hasAnyArrival || hasDeparture)
    {
      if(airportDeparture && !airportRoundTrip)
      {
        if(hasDeparture)
        {
          actionShowProcedures->setText(tr("Show Departure &Procedures for %1"));
          ActionTool::setText(actionShowProcedures, true /* enabled */, objectText);
        }
        else
          actionShowProcedures->setText(tr("Show Procedures (no departure procedure)"));
      }
      else if(airportDestination && !airportRoundTrip)
      {
        if(hasAnyArrival)
        {
          actionShowProcedures->setText(tr("Show Arrival and Approach &Procedures for %1"));
          ActionTool::setText(actionShowProcedures, true /* enabled */, objectText);
        }
        else
          actionShowProcedures->setText(tr("Show &Procedures (no arrival and no approch procedure)"));
      }
      else
      {
        actionShowProcedures->setText(tr("Show &Procedures for %1"));
        ActionTool::setText(actionShowProcedures, true /* enabled */, objectText);
      }
    }
    else
      actionShowProcedures->setText(tr("Show Procedures (no procedure)"));

    // Departure and destination menu items ================================================================
    if(noRunways)
    {
      if(!airportDestination && !airportDeparture)
      {
        actionShowDeparture->setText(tr("&Select %1 as Departure{hint}"));
        actionShowDeparture->setEnabled(true);
        actionShowApproach->setText(tr("Select %1 as &Destination{hint}"));
        actionShowApproach->setEnabled(true);
      }
    }
    else
    {
      if(airportDeparture)
      {
        actionShowDeparture->setText(tr("&Select Departure Runway for %1{hint}"));
        actionShowDeparture->setEnabled(true);
      }
      else if(!airportDestination)
      {
        actionShowDeparture->setText(tr("&Select %1 as Departure{hint}"));
        actionShowDeparture->setEnabled(true);
      }

      if(airportDestination)
      {
        actionShowApproach->setText(tr("Select &Destination Runway for %1{hint}"));
        actionShowApproach->setEnabled(true);
      }
      else if(!airportDeparture)
      {
        actionShowApproach->setText(tr("Select %1 as &Destination{hint}"));
        actionShowApproach->setEnabled(true);
      }

      ActionTool::setText(actionShowDeparture, objectText);
      ActionTool::setText(actionShowApproach, objectText);
    }

    QString hintStr = airportItemSuffix(airportDeparture, airportDestination, airportAlternate, airportRoundTrip, noRunways);
    actionShowDeparture->setText(actionShowDeparture->text().replace("{hint}", hintStr));
    actionShowApproach->setText(actionShowApproach->text().replace("{hint}", hintStr));
  }
  else
  {
    actionShowProcedures->setText(tr("Show &Procedures"));
    actionShowDeparture->setText(tr("&Select Departure %1"));
    actionShowApproach->setText(tr("Select &Destination %1"));
  }
}

QString ContextMenuTool::airportItemSuffix(bool airportDeparture, bool airportDestination, bool airportAlternate,
                                           bool airportRoundTrip, bool noRunways, const QStringList& otherSuffixes)
{
  QStringList hints;

  // Append either one
  if(airportRoundTrip)
    hints.append(tr("is round trip"));
  else if(airportDeparture)
    hints.append(tr("is departure"));
  else if(airportDestination)
    hints.append(tr("is destination"));
  else if(airportAlternate)
    hints.append(tr("is alternate"));

  if(noRunways)
    hints.append(tr("no runway"));

  // Add suffixes from caller
  hints.append(otherSuffixes);
  hints.removeDuplicates();

  QString hintStr = atools::strJoin(tr(" ("), hints, tr(", "), tr(", "), tr(")"));

  // Having ruwnays triggers the selection dialog */
  if(!noRunways)
    hintStr.append(tr(" ..."));
  return hintStr;
}
