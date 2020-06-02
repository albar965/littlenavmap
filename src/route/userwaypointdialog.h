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

#ifndef LITTLENAVMAP_USERWAYPOINTDIALOG_H
#define LITTLENAVMAP_USERWAYPOINTDIALOG_H

#include <QDialog>

class QRegularExpressionValidator;

namespace Ui {
class UserWaypointDialog;
}

namespace atools {
namespace fs {
namespace pln {
class FlightplanEntry;
}
}
namespace geo {
class Pos;
}
}

class QAbstractButton;

/*
 * Edit coordinates or the name of a user defined flight plan position.
 * Also used to edit remarks for a flight plan waypoint.
 */
class UserWaypointDialog :
  public QDialog
{
  Q_OBJECT

public:
  UserWaypointDialog(QWidget *parent, const atools::fs::pln::FlightplanEntry& entryParam);
  virtual ~UserWaypointDialog();

  /* Entry is copyied. Get changed copy here */
  const atools::fs::pln::FlightplanEntry& getEntry() const
  {
    return *entry;
  }

private:
  void coordsEdited(const QString& text);
  void buttonBoxClicked(QAbstractButton *button);

  Ui::UserWaypointDialog *ui;
  atools::fs::pln::FlightplanEntry *entry;
};

#endif // LITTLENAVMAP_USERWAYPOINTDIALOG_H
