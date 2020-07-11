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

#ifndef LNM_MAPCONTEXTMENU_H
#define LNM_MAPCONTEXTMENU_H

#include <QApplication>
#include <QMenu>

class MapWidget;
class QAction;
class QMenu;
class QMainWindow;

namespace Ui {
class MainWindow;
}

namespace atools {
namespace geo {
class Pos;
}
namespace gui {
class ActionTextSaver;
class ActionStateSaver;
}
namespace fs {
namespace sc {
class SimConnectAircraft;
class SimConnectAircraft;
class SimConnectUserAircraft;
}
}
}

namespace map {
struct MapBase;
struct MapAirport;
struct MapVor;
struct MapNdb;
struct MapWaypoint;
struct MapIls;
struct MapUserpointRoute;
struct MapAirway;
struct MapParking;
struct MapHelipad;
struct MapAirspace;
struct MapUserpoint;
struct MapLogbookEntry;
struct MapSearchResult;
struct MapSearchResultIndex;
}

namespace mc {

/*
 * Describes the selected action for generated actions and the respective sub-menus.
 * Each value corresponds to an action or menu containing actions for disambiguation.
 * A "More" sub-menu is added if needed. */
enum MenuActionType
{
  NONE, /* Nothing selected - default value */
  INFORMATION, /* Show Information */
  PROCEDURE, /* Show airport procedures */
  CUSTOMPROCEDURE, /* Create custom procedure */
  MEASURE, /* GC measmurement line */
  NAVAIDRANGE, /* Show range ring for radio navaid */
  PATTERN, /* Airport traffic pattern */
  HOLD, /* Holding */
  DEPARTURE, /* Set departure in flight plan */
  DESTINATION, /* Set destination in flight plan */
  ALTERNATE, /* Add alternate airport to flight plan */
  ADDROUTE, /* Add airport, navid or position to next flight plan leg */
  APPENDROUTE, /* Append airport, navid or position to end of flight plan */
  DELETEROUTEWAYPOINT, /* Remove flight plan leg */
  EDITROUTEUSERPOINT, /* Edit user defined route waypoint or remarks for any flight plan point */
  USERPOINTADD, /* Add userpoint (in sub-menu) */
  USERPOINTEDIT, /* Edit userpoint (in sub-menu) */
  USERPOINTMOVE, /* Move userpoint on map (in sub-menu) */
  USERPOINTDELETE, /* Remove userpoint (in sub-menu) */
  LOGENTRYEDIT, /* Edit logbook entry on preview */
  SHOWINSEARCH /* Show objects in search window with filter and selection */
};

}

/*
 * Builds a context menu for the map dynamically with sub-menus for disambiguation, enabled/disabled
 * items and modified action texts depending on map objects at or near the click position.
 */
class MapContextMenu
{
  Q_DECLARE_TR_FUNCTIONS(MapContextMenu)

public:
  explicit MapContextMenu(QMainWindow *mainWindowParam, MapWidget *mapWidgetParam);
  ~MapContextMenu();

  /* Do not allow copying */
  MapContextMenu(MapContextMenu const&) = delete;
  MapContextMenu& operator=(MapContextMenu const&) = delete;

  /* Execute the menu for map objects at point and opens the menu at menuPos.
   *  Returns true if something was selected. */
  bool exec(QPoint menuPos, QPoint point);

  /* Resets all back to default */
  void clear();

  /* Get the action which was selected by the user.
   * Only used for pre-defined actions instead of the dynamically generated ones.*/
  QAction *getSelectedAction() const
  {
    return selectedAction;
  }

  /* Get the map object which was selected */
  const map::MapBase *getSelectedBase() const
  {
    return selectedBase;
  }

  /* Get selected action type like "Show information" */
  mc::MenuActionType getSelectedActionType() const
  {
    return selectedActionType;
  }

  /* true if the click position is in a valid map area, i.e. not outside the globe */
  bool isVisibleOnMap() const
  {
    return visibleOnMap;
  }

  /* Global click position */
  const atools::geo::Pos& getPos() const;

  /* Index of selected/nearest distance marker or -1 if none */
  int getDistMarkerIndex() const
  {
    return distMarkerIndex;
  }

  /* Index of selected/nearest traffic pattern or -1 if none */
  int getTrafficPatternIndex() const
  {
    return trafficPatternIndex;
  }

  /* Index of selected/nearest hold or -1 if none */
  int getHoldIndex() const
  {
    return holdIndex;
  }

  /* Index of selected/nearest range rings or -1 if none */
  int getRangeMarkerIndex() const
  {
    return rangeMarkerIndex;
  }

  /* Index of selected/nearest route leg or -1 if none */
  int getSelectedRouteIndex() const
  {
    return selectedRouteIndex;
  }

private:
  /* Build the whole menu structure */
  void buildMainMenu();

  /* Methods below insert actions, callbacks, texts and icons for the respective menu actions.
   * Sub-menus for disambiguation are created if needed */
  // ----
  void insertInformationMenu(QMenu& menu);
  void insertProcedureMenu(QMenu& menu);
  void insertCustomProcedureMenu(QMenu& menu);

  // ----
  void insertMeasureMenu(QMenu& menu);

  // ui->actionMapHideDistanceMarker

  // ----
  // ui->actionMapRangeRings
  void insertNavaidRangeMenu(QMenu& menu);

  // ui->actionMapHideOneRangeRing

  // ----
  void insertPatternMenu(QMenu& menu);

  // ui->actionMapHideTrafficPattern
  void insertHoldMenu(QMenu& menu);

  // ui->actionMapHideHold

  // ----
  void insertDepartureMenu(QMenu& menu);
  void insertDestinationMenu(QMenu& menu);
  void insertAlternateMenu(QMenu& menu);

  // ----
  void insertAddRouteMenu(QMenu& menu);
  void insertAppendRouteMenu(QMenu& menu);
  void insertDeleteRouteWaypointMenu(QMenu& menu);
  void insertEditRouteUserpointMenu(QMenu& menu);

  // ---- sub-menu
  void insertUserpointAddMenu(QMenu& menu);
  void insertUserpointEditMenu(QMenu& menu);
  void insertUserpointMoveMenu(QMenu& menu);
  void insertUserpointDeleteMenu(QMenu& menu);

  // ----
  void insertLogEntryEdit(QMenu& menu);

  // ----
  void insertShowInSearchMenu(QMenu& menu);

  // ---- if full screen
  // ui->actionShowFullscreenMap // connected by main window

  // ---- sub-menu "More"
  // ui->actionMapCopyCoordinates
  // ui->actionMapSetMark
  // ui->actionMapSetHome

  /*
   * Callback which is used to define menu behavior.
   * @param text Fill to override menu text
   * @param disable Has to be set when passing a callback to insertMenuOrAction. Disables/enables the menu item
   * @param submenu true if the current item is added to a generated sub-menu
   */
  typedef  std::function<void (const map::MapBase *base, QString& text, QIcon& icon, bool& disable,
                               bool submenu)> ActionCallback;

  /* Insert menu for given action and given map objects from index. Adds a sub-menu if needed.
   * tip is added as status tip and as tooltip if menu tooltips are enabled.
   * allowNoMapObject Enables the menu for an empty index (click without map object) and also inserts a coordinates menu.  */
  void insertMenuOrAction(QMenu& menu, mc::MenuActionType actionType, const map::MapSearchResultIndex& index,
                          const QString& text, const QString& tip, const QIcon& icon,
                          bool allowNoMapObject = false, const ActionCallback& callback = nullptr);

  /* Insert a single action. */
  QAction *insertAction(QMenu& menu, mc::MenuActionType actionType, const QString& text, const QString& tip,
                        const QIcon& icon, const map::MapBase *base, bool submenu,
                        bool allowNoMapObject, const ActionCallback& callback);

  /* Get corresponding icon for map object. Can be dynamically generated or resource depending on map object type */
  QIcon mapBaseIcon(const map::MapBase *base);

  /* Gets text for menu item */
  static QString mapBaseText(const map::MapBase *base);

  /* Sort callback comparator for a locale-aware sorting of menu items*/
  static bool alphaSort(const map::MapBase *base1, const map::MapBase *base2);

  /* Determine various route and procedure related states for the given map object */
  void procedureFlags(const map::MapBase *base, bool *departure = nullptr, bool *destination = nullptr,
                      bool *alternate = nullptr, bool *arrivalProc = nullptr, bool *departureProc = nullptr) const;

  /* true if route leg can have comment edited */
  bool canEditRouteComment(const map::MapBase *base) const;

  /* true if route leg can be removed or modified */
  bool canEditRoutePoint(const map::MapBase *base) const;

  /* Name of underlying procedure or empty if route leg */
  QString procedureName(const map::MapBase *base) const;

  // Selections
  QAction *selectedAction = nullptr;
  mc::MenuActionType selectedActionType = mc::NONE;
  const map::MapBase *selectedBase = nullptr;

  // Result keeps all objects in the menu near the click position
  // Dynamically created indexes which point into these objects are used
  map::MapSearchResult *result;

  struct MenuData;
  // Index mapped to menu data which also contains a pointer to result above
  // Id is stored as action data
  QVector<MenuData> dataIndex;

  // Actions have to be generated with parent main window and therefore menu does not take ownership
  // Actions are kept here for later deletion
  QVector<QObject *> actionsAndMenus;

  // true if clicked position is within globe
  bool visibleOnMap = false;

  // Nearest indexes
  int distMarkerIndex = -1, trafficPatternIndex = -1, holdIndex = -1, rangeMarkerIndex = -1, selectedRouteIndex = -1;

  MapWidget *mapWidget;
  QMainWindow *mainWindow;

  // Keep global click position in a map base which allows to referece it by map base pointer
  map::MapBase *mapBasePos;

  // Keep state and text for some pre-defined action
  atools::gui::ActionTextSaver *textSaver;
  atools::gui::ActionStateSaver *stateSaver;

  Ui::MainWindow *ui;

  // Context menu
  QMenu menu;
};

#endif // LNM_MAPCONTEXTMENU_H
