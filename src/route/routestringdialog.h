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

#ifndef LITTLENAVMAP_ROUTESTRINGDIALOG_H
#define LITTLENAVMAP_ROUTESTRINGDIALOG_H

#include <QDialog>

namespace Ui {
class RouteStringDialog;
}

namespace atools {
namespace fs {
namespace pln {
class Flightplan;
}
}
}

class MapQuery;
class QAbstractButton;
class RouteController;
class RouteString;

class RouteStringDialog :
  public QDialog
{
  Q_OBJECT

public:
  RouteStringDialog(QWidget *parent, RouteController *routeController);
  virtual ~RouteStringDialog();

  const atools::fs::pln::Flightplan& getFlightplan() const;

  /* Saves and restores all values */
  void saveState();
  void restoreState();

private:
  void readClicked();
  void fromClipboardClicked();
  void toClipboardClicked();
  void buttonBoxClicked(QAbstractButton *button);
  void updateButtonState();

  Ui::RouteStringDialog *ui;
  atools::fs::pln::Flightplan *flightplan = nullptr;
  MapQuery *query = nullptr;
  RouteController *controller = nullptr;
  RouteString *routeString;
};

#endif // LITTLENAVMAP_ROUTESTRINGDIALOG_H
