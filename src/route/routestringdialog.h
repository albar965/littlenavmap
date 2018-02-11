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

#ifndef LITTLENAVMAP_ROUTESTRINGDIALOG_H
#define LITTLENAVMAP_ROUTESTRINGDIALOG_H

#include "route/routestring.h"

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

  /* > 0 if speed was included in the string */
  float getSpeedKts() const
  {
    return speedKts;
  }

  /* True if the altitude was included in the string */
  bool isAltitudeIncluded() const
  {
    return altitudeIncluded;
  }

  static rs::RouteStringOptions getOptionsFromSettings();

private:
  void readButtonClicked();
  void fromClipboardClicked();
  void toClipboardClicked();
  void buttonBoxClicked(QAbstractButton *button);
  void updateButtonState();
  void toolButtonOptionTriggered(QAction *action);
  void updateButtonClicked();

  Ui::RouteStringDialog *ui;
  atools::fs::pln::Flightplan *flightplan = nullptr;
  MapQuery *mapQuery = nullptr;
  RouteController *controller = nullptr;
  RouteString *routeString;
  float speedKts = 0.f;
  bool altitudeIncluded = false;
  rs::RouteStringOptions options = rs::DEFAULT_OPTIONS;

  void updateFlightplan();

};

#endif // LITTLENAVMAP_ROUTESTRINGDIALOG_H
