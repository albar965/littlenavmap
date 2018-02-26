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

#ifndef USERDATACONTROLLER_H
#define USERDATACONTROLLER_H

#include <QObject>

namespace atools {
namespace gui {
class Dialog;
class ErrorHandler;
class HelpHandler;
}
namespace fs {
namespace userdata {
class UserdataManager;
}
}
}

class MainWindow;

/*
 * Methods to edit, add, delete, import and export userdata.
 */
class UserdataController :
  public QObject
{
  Q_OBJECT

public:
  UserdataController(atools::fs::userdata::UserdataManager *userdataManager, MainWindow *parent);
  virtual ~UserdataController();

  /* Show add dialog and add to the database if accepted */
  void addUserpoint();

  /* Show edit dialog and save changes to the database if accepted for the given ids*/
  void editUserpoints(const QVector<int>& ids);

  /* Show message box and delete entries with the given ids */
  void deleteUserpoints(const QVector<int>& ids);

  /* Import and export from a predefined CSV format */
  void importCsv();
  void exportCsv();

  /* Import and export user_fix.dat file from X-Plane */
  void importXplane();
  void exportXplane();

  /* Import and export Garmin GTN user waypoint database */
  void importGarmin();
  void exportGarmin();

  /* Export waypoints into a XML file for BGL compilation */
  void exportBgl();

  /* Remove all data but keep the schema to avoid locks */
  void  clearDatabase();

  /* Show search tab and raise window */
  void  showSearch();

signals:
  /* Sent after database modification to update the search result table */
  void refreshUserdataSearch();

private:
  atools::fs::userdata::UserdataManager *manager;
  atools::gui::Dialog *dialog;
  MainWindow *mainWindow;
};

#endif // USERDATACONTROLLER_H
