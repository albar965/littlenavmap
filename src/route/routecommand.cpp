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

int RouteCommand::id() const
{
  return type;
}

bool RouteCommand::mergeWith(const QUndoCommand *other)
{
  if(other->id() != id())
    return false;

  const RouteCommand *newCmd = dynamic_cast<const RouteCommand *>(other);
  if(newCmd == nullptr)
    return false;

  switch(type)
  {
    case rctype::EDIT:
      return false;

    case rctype::REVERSE:
    case rctype::DELETE:
    case rctype::MOVE:
    case rctype::ALTITUDE:
    case rctype::SPEED:
      // Merge - overwrite the flight plan after the change
      planAfterChange = newCmd->planAfterChange;
      // Let controller know about the merge so the undo index can be adapted
      controller->undoMerge();
      return true;
  }
  return false;
}
