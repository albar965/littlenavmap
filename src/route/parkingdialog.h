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

#ifndef LITTLENAVMAP_PARKINGDIALOG_H
#define LITTLENAVMAP_PARKINGDIALOG_H

#include <QDialog>

#include "common/maptypes.h"

namespace Ui {
class ParkingDialog;
}

class QListWidget;
class MapQuery;

/*
 * Allows to select the flight plan departure parking or start position.
 * The latter one can be a runway or helipad.
 */
class ParkingDialog :
  public QDialog
{
  Q_OBJECT

public:
  ParkingDialog(QWidget *parent, const map::MapAirport& departureAirport);
  virtual ~ParkingDialog();

  /* Get selected parking spot
   * @return true if parking was selected */
  bool getSelectedParking(map::MapParking& parking) const;

  /* Get selected start position.
   * @return true if a start was selected */
  bool getSelectedStartPosition(map::MapStart& start) const;

private:
  void updateButtons();

  // Used to fill the list
  struct StartPosition
  {
    map::MapParking parking;
    map::MapStart start;
  };

  QList<StartPosition> entries;
  Ui::ParkingDialog *ui;
};

#endif // LITTLENAVMAP_PARKINGDIALOG_H
