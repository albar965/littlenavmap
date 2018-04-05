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

#ifndef LITTLENAVMAP_ROUTECOMMAND_H
#define LITTLENAVMAP_ROUTECOMMAND_H

#include "fs/pln/flightplan.h"

#include <QUndoCommand>

class RouteController;

namespace rctype {
/* Command type used to merge undo actions */
enum RouteCmdType
{
  EDIT = -1, /* Unmergeable edit */
  DELETE = 0, /* Waypoint(s) deleted in table */
  MOVE = 1, /* Waypoint(s) moved in table */
  ALTITUDE = 2, /* Altitude changed in spin box */
  SPEED = 3, /* Speed changed in spin box */
  REVERSE = 4 /* Route reverse action */
};

}

/*
 * Flight plan undo command including a few workaround for QUndoCommand inflexibilities.
 * Keeps a copy of the flight plan before and after the change.
 */
class RouteCommand :
  public QUndoCommand
{
public:
  RouteCommand(RouteController *routeController, const atools::fs::pln::Flightplan& flightplanBefore,
               const QString& text = QString(), rctype::RouteCmdType rcType = rctype::EDIT);
  virtual ~RouteCommand();

  virtual void undo() override;
  virtual void redo() override;

  /* Need to keep both versions in a redundant way since linking between commands is not reliable */
  void setFlightplanAfter(const atools::fs::pln::Flightplan& flightplanAfter);

private:
  virtual int id() const override;
  virtual bool mergeWith(const QUndoCommand *other) override;

  /* Avoid the first redo action when inserting the command. This not usable for complex interactions. */
  bool firstRedoExecuted = false;
  RouteController *controller;
  rctype::RouteCmdType type;
  atools::fs::pln::Flightplan planBeforeChange, planAfterChange;
};

#endif // LITTLENAVMAP_ROUTECOMMAND_H
