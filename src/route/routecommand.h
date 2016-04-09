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

#ifndef ROUTECOMMAND_H
#define ROUTECOMMAND_H

#include <QUndoCommand>

#include <fs/pln/flightplan.h>

class RouteController;

class RouteCommand :
  public QUndoCommand
{
public:
  RouteCommand(RouteController *routeController, const atools::fs::pln::Flightplan* flightplanBefore);
  virtual ~RouteCommand();

  virtual void undo() override;
  virtual void redo() override;

  /* Need to keep both versions in a redundant way since linking is not reliable */
  void setFlightplanAfter(const atools::fs::pln::Flightplan* flightplanAfter);

private:
  bool firstRedoExecuted = false;
  RouteController *controller;
  atools::fs::pln::Flightplan planBefore, planAfter;
};

#endif // ROUTECOMMAND_H
