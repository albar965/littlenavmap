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

#include "routecommand.h"
#include "routecontroller.h"

RouteCommand::RouteCommand(RouteController *routeController,
                           const atools::fs::pln::Flightplan *flightplanBefore)
  : controller(routeController)
{
  if(flightplanBefore != nullptr)
    planBefore = *flightplanBefore;
}

RouteCommand::~RouteCommand()
{

}

void RouteCommand::setFlightplanAfter(const atools::fs::pln::Flightplan *flightplanAfter)
{
  if(flightplanAfter != nullptr)
    planAfter = *flightplanAfter;
}

void RouteCommand::undo()
{
  qDebug() << "undo";
  controller->changeRoute(planBefore);
}

void RouteCommand::redo()
{
  if(!firstRedoExecuted)
  {
    qDebug() << "first redo ignored";
    firstRedoExecuted = true;
  }
  else
  {
    qDebug() << "redo";
    controller->changeRoute(planAfter);
  }
}
