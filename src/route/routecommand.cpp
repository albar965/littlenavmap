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

#include "route/routecommand.h"
#include "route/routecontroller.h"

RouteCommand::RouteCommand(RouteController *routeController,
                           const atools::fs::pln::Flightplan& flightplanBefore, const QString& text,
                           rctype::RouteCmdType rcType)
  : QUndoCommand(text), controller(routeController), type(rcType)
{
  planBeforeChange = flightplanBefore;
}

RouteCommand::~RouteCommand()
{

}

void RouteCommand::setFlightplanAfter(const atools::fs::pln::Flightplan& flightplanAfter)
{
  planAfterChange = flightplanAfter;
}

void RouteCommand::undo()
{
  controller->changeRouteUndo(planBeforeChange);
}

void RouteCommand::redo()
{
  if(!firstRedoExecuted)
    // Skip first redo - I need to do the initial changes myself
    firstRedoExecuted = true;
  else
    controller->changeRouteRedo(planAfterChange);
}
