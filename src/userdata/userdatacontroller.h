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
#include <QVector>

namespace atools {
namespace sql {
class SqlRecord;
}

namespace geo {
class Pos;
}
namespace gui {
class Dialog;
class ErrorHandler;
class HelpHandler;
}
namespace fs {
namespace sc {
class SimConnectUserAircraft;
}
namespace common {
class MagDecReader;
}
namespace userdata {
class UserdataManager;

}
}
}

namespace map {
struct MapSearchResult;

struct MapUserpoint;

}

class MainWindow;
class UserdataIcons;
class QToolButton;
class QAction;

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
  void addUserpoint(int id, const atools::geo::Pos& pos);

  /* Show edit dialog and save changes to the database if accepted for the given ids*/
  void editUserpoints(const QVector<int>& ids);

  /* Show message box and delete entries with the given ids */
  void deleteUserpoints(const QVector<int>& ids);

  /* Import and export from a predefined CSV format */
  void importCsv();
  void exportCsv();

  /* Import and export user_fix.dat file from X-Plane */
  void importXplaneUserFixDat();
  void exportXplaneUserFixDat();

  /* Import and export Garmin GTN user waypoint database */
  void importGarmin();
  void exportGarmin();

  /* Export waypoints into a XML file for BGL compilation */
  void exportBglXml();

  /* Remove all data but keep the schema to avoid locks */
  void  clearDatabase();

  /* Show search tab and raise window */
  void  showSearch();

  /* Get icon manager */
  UserdataIcons *getUserdataIcons() const
  {
    return icons;
  }

  /* Get the given type or the default type if unknown */
  QString getDefaultType(const QString& type);

  /* Add toolbar button and actions. Also adds actions to menu. Call this only once. */
  void addToolbarButton();

  void saveState();
  void restoreState();

  /* Reset display settings to show all */
  void resetSettingsToDefault();

  /* Get currently in menu selected types for display */
  const QStringList& getSelectedTypes() const
  {
    return selectedTypes;
  }

  /* Get all registered types as found by icon manager */
  QStringList getAllTypes() const;

  /* Show unknown types*/
  bool isSelectedUnknownType() const
  {
    return selectedUnknownType;
  }

  /* Add userpoint from map and prefill with result data */
  void addUserpointFromMap(const map::MapSearchResult& result, atools::geo::Pos pos);
  void editUserpointFromMap(const map::MapSearchResult& result);
  void deleteUserpointFromMap(int id);
  void moveUserpointFromMap(const map::MapUserpoint& userpoint);

  /* Needed for export */
  void setMagDecReader(atools::fs::common::MagDecReader *magDecReader);

  void aircraftTakeoff(const atools::fs::sc::SimConnectUserAircraft& aircraft);
  void aircraftLanding(const atools::fs::sc::SimConnectUserAircraft& aircraft, float flownDistanceNm,
                       float averageTasKts);

  /* Create CSV backup in the settings directory */
  void backup();

  /* Remove all data from the table which has the temporary flag set. */
  void clearTemporary();

signals:
  /* Sent after database modification to update the search result table */
  void refreshUserdataSearch(bool loadAll, bool keepSelection);

  /* Issue a redraw of the map */
  void userdataChanged();

private:
  /* Called by any action */
  void toolbarActionTriggered();

  /* Copy class state to actions and vice versa */
  void typesToActions();
  void actionsToTypes();
  void addUserpointInternal(int id, const atools::geo::Pos& pos, const atools::sql::SqlRecord& prefill);
  bool exportSelectedQuestion(bool& exportSelected, bool& append, bool appendAllowed);

  /* Get default X-Plane path to user_fix.dat file */
  QString xplaneUserWptDatPath();

  /* Get default Garmin GTN export path */
  QString garminGtnUserWptPath();

  void createTakoffLanding(const atools::fs::sc::SimConnectUserAircraft& aircraft, bool takeoff, float flownDistanceNm,
                           float averageTasKts);

  /* Remember last aircraft for fuel calculations */
  const atools::fs::sc::SimConnectUserAircraft *aircraftAtTakeoff = nullptr;

  /* Currently in actions selected types */
  QStringList selectedTypes;
  bool selectedUnknownType = false;

  atools::fs::userdata::UserdataManager *manager;
  atools::gui::Dialog *dialog;
  MainWindow *mainWindow;
  UserdataIcons *icons;

  // Buttons and actions for toolbar and menu
  QToolButton *userdataToolButton = nullptr;
  QAction *actionAll = nullptr, *actionNone = nullptr, *actionUnknown = nullptr;
  QVector<QAction *> actions;
  atools::sql::SqlRecord *lastAddedRecord = nullptr;

};

#endif // USERDATACONTROLLER_H
