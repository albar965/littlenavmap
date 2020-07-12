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

#include "mapgui/mapcontextmenu.h"
#include "mapgui/mapmarkhandler.h"
#include "mapgui/mapscreenindex.h"
#include "mapgui/mapwidget.h"
#include "common/mapcolors.h"
#include "common/vehicleicons.h"
#include "common/formatter.h"
#include "query/airportquery.h"
#include "query/mapquery.h"
#include "online/onlinedatacontroller.h"
#include "gui/mainwindow.h"
#include "gui/actionstatesaver.h"
#include "gui/actiontextsaver.h"
#include "geo/pos.h"
#include "options/optiondata.h"
#include "route/route.h"
#include "navapp.h"
#include "common/unit.h"
#include "atools.h"
#include "common/symbolpainter.h"
#include "userdata/userdataicons.h"
#include "common/unit.h"

#include <ui_mainwindow.h>

using map::MapSearchResultIndex;

// Maximum number of items in disambiguation sub-menus
const static int MAX_MENU_ITEMS = 10;

// Truncate map object texts in the middle
const static int TEXT_ELIDE = 30;

// Keeps information for each menu, action and sub-menu.
struct MapContextMenu::MenuData
{
  mc::MenuActionType actionType = mc::NONE;

  // Points to map object in "result"
  const map::MapBase *base;
};

// Default sort order for disambiguation sub-menus
const static QVector<map::MapObjectTypes> DEFAULT_TYPE_SORT(
{
  map::AIRCRAFT,
  map::AIRCRAFT_ONLINE,
  map::AIRCRAFT_AI,
  map::LOGBOOK,
  map::USERPOINT,
  map::AIRPORT,
  map::PARKING,
  map::HELIPAD,
  map::ILS,
  map::VOR,
  map::NDB,
  map::WAYPOINT,
  map::USERPOINTROUTE,
  map::AIRWAY,
  map::TRACK,
  map::AIRCRAFT_AI_SHIP,
  map::AIRSPACE,
});

MapContextMenu::MapContextMenu(QMainWindow *mainWindowParam, MapWidget *mapWidgetParam)
  : mapWidget(mapWidgetParam), mainWindow(mainWindowParam), menu(mainWindowParam)
{
  result = new map::MapSearchResult;
  mapBasePos = new map::MapBase(map::NONE, -1, atools::geo::EMPTY_POS);

  ui = NavApp::getMainUi();
  QList<QAction *> actions(
  {
    // Save state since widgets are shared with others
    ui->actionMapHold,
    ui->actionMapNavaidRange,
    ui->actionMapRangeRings,
    ui->actionMapTrafficPattern,
    ui->actionRouteAddPos,
    ui->actionRouteAirportAlternate,
    ui->actionRouteAirportDest,
    ui->actionRouteAirportStart,
    ui->actionRouteAppendPos,
    ui->actionMapCopyCoordinates
  });

  // Texts with % will be replaced save them and let the ActionTextSaver restore them on return
  textSaver = new atools::gui::ActionTextSaver(actions);

  // Re-enable actions on exit to allow keystrokes
  stateSaver = new atools::gui::ActionStateSaver(actions);
}

MapContextMenu::~MapContextMenu()
{
  clear();
  delete textSaver;
  delete stateSaver;
  delete result;
  delete mapBasePos;
}

void MapContextMenu::clear()
{
  selectedBase = nullptr;
  selectedAction = nullptr;
  selectedActionType = mc::NONE;
  dataIndex.clear();
  *mapBasePos = map::MapBase(map::NONE, -1, atools::geo::EMPTY_POS);
  menu.clear();

  // Delete all generated action
  qDeleteAll(actionsAndMenus);
  actionsAndMenus.clear();

  result->clear();
}

void MapContextMenu::buildMainMenu()
{
  Ui::MainWindow *ui = NavApp::getMainUi();
  menu.clear();

  // Inherit tool tip status from well know menu
  menu.setToolTipsVisible(NavApp::isMenuToolTipsVisible());

  insertInformationMenu(menu);
  insertProcedureMenu(menu);
  insertCustomProcedureMenu(menu);
  menu.addSeparator();

  insertMeasureMenu(menu);
  menu.addAction(ui->actionMapHideDistanceMarker);
  menu.addSeparator();

  menu.addAction(ui->actionMapRangeRings);
  insertNavaidRangeMenu(menu);
  menu.addAction(ui->actionMapHideOneRangeRing);
  menu.addSeparator();

  insertPatternMenu(menu);
  menu.addAction(ui->actionMapHideTrafficPattern);
  menu.addSeparator();

  insertHoldMenu(menu);
  menu.addAction(ui->actionMapHideHold);
  menu.addSeparator();

  insertDepartureMenu(menu);
  insertDestinationMenu(menu);
  insertAlternateMenu(menu);
  menu.addSeparator();

  insertAddRouteMenu(menu);
  insertAppendRouteMenu(menu);
  insertDeleteRouteWaypointMenu(menu);
  insertEditRouteUserpointMenu(menu);
  menu.addSeparator();

  QMenu *sub = menu.addMenu(QIcon(":/littlenavmap/resources/icons/userdata.svg"), tr("&Userpoints"));
  sub->setToolTipsVisible(menu.toolTipsVisible());
  if(visibleOnMap)
  {
    insertUserpointAddMenu(*sub);
    insertUserpointEditMenu(*sub);
    insertUserpointMoveMenu(*sub);
    insertUserpointDeleteMenu(*sub);
  }
  else
    // No position - no sub-menu
    sub->setDisabled(true);
  menu.addSeparator();

  insertLogEntryEdit(menu);
  menu.addSeparator();

  insertShowInSearchMenu(menu);
  menu.addSeparator();

  if(NavApp::isFullScreen())
  {
    // Add menu to exit full screen
    menu.addAction(ui->actionShowFullscreenMap); // connected otherwise
    menu.addSeparator();
  }

  sub = menu.addMenu(tr("&More"));
  if(visibleOnMap)
  {
    // More rarely used menu items
    sub->addAction(ui->actionMapCopyCoordinates);
    sub->addAction(ui->actionMapSetMark);
    sub->addAction(ui->actionMapSetHome);
  }
  else
    // No position - no sub-menu
    sub->setDisabled(true);
}

bool MapContextMenu::alphaSort(const map::MapBase *base1, const map::MapBase *base2)
{
  // Remove & before sorting
  return QString::localeAwareCompare(mapBaseText(base1).replace('&', QString()),
                                     mapBaseText(base2).replace('&', QString())) < 0;
}

QString MapContextMenu::mapBaseText(const map::MapBase *base)
{
  if(base != nullptr)
  {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch"
    switch(base->getTypeEnum())
#pragma GCC diagnostic pop
    {
      case map::AIRPORT:
        return map::airportText(*base->asPtr<map::MapAirport>());

      case map::VOR:
        return map::vorText(*base->asPtr<map::MapVor>());

      case map::NDB:
        return map::ndbText(*base->asPtr<map::MapNdb>());

      case map::ILS:
        return map::ilsTextShort(*base->asPtr<map::MapIls>());

      case map::WAYPOINT:
        return map::waypointText(*base->asPtr<map::MapWaypoint>());

      case map::AIRWAY:
        return map::airwayText(*base->asPtr<map::MapAirway>());

      case map::AIRCRAFT:
        return map::aircraftTextShort(base->asPtr<map::MapUserAircraft>()->getAircraft());

      case map::AIRCRAFT_AI:
        return map::aircraftTextShort(base->asPtr<map::MapAiAircraft>()->getAircraft());

      case map::AIRCRAFT_ONLINE:
        return map::aircraftTextShort(base->asPtr<map::MapOnlineAircraft>()->getAircraft());

      case map::USERPOINTROUTE:
        return map::userpointRouteText(*base->asPtr<map::MapUserpointRoute>());

      case map::AIRSPACE:
        return map::airspaceText(*base->asPtr<map::MapAirspace>());

      case map::PARKING:
        return map::parkingText(*base->asPtr<map::MapParking>());

      case map::HELIPAD:
        return map::helipadText(*base->asPtr<map::MapHelipad>());

      case map::USERPOINT:
        return map::userpointText(*base->asPtr<map::MapUserpoint>());

      case map::LOGBOOK:
        return map::logEntryText(*base->asPtr<map::MapLogbookEntry>());
    }
  }
  return QString();
}

QIcon MapContextMenu::mapBaseIcon(const map::MapBase *base)
{
  if(base != nullptr)
  {
    // Get size for icons
    int size = QFontMetrics(QApplication::font()).height();
    SymbolPainter painter;
    VehicleIcons *vehicleIcons = NavApp::getVehicleIcons();

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch"
    switch(base->getTypeEnum())
#pragma GCC diagnostic pop
    {
      case map::AIRPORT:
        return painter.createAirportIcon(*base->asPtr<map::MapAirport>(), size);

      case map::VOR:
        return painter.createVorIcon(*base->asPtr<map::MapVor>(), size);

      case map::NDB:
        return painter.createNdbIcon(size);

      case map::ILS:
        return QIcon(":/littlenavmap/resources/icons/ils.svg");

      case map::WAYPOINT:
        return painter.createWaypointIcon(size);

      case map::AIRWAY:
        return painter.createAirwayIcon(*base->asPtr<map::MapAirway>(), size);

      case map::AIRCRAFT:
        return vehicleIcons->iconFromCache(base->asPtr<map::MapUserAircraft>()->getAircraft(), size, 45 /* rotate */);

      case map::AIRCRAFT_AI:
        return vehicleIcons->iconFromCache(base->asPtr<map::MapAiAircraft>()->getAircraft(), size, 45 /* rotate */);

      case map::AIRCRAFT_ONLINE:
        return vehicleIcons->iconFromCache(base->asPtr<map::MapOnlineAircraft>()->getAircraft(), size, 45 /* rotate */);

      case map::USERPOINTROUTE:
        return painter.createUserpointIcon(size);

      case map::AIRSPACE:
        return painter.createAirspaceIcon(*base->asPtr<map::MapAirspace>(), size);

      case map::PARKING:
        return mapcolors::iconForParkingType(base->asPtr<map::MapParking>()->type);

      case map::HELIPAD:
        return painter.createHelipadIcon(*base->asPtr<map::MapHelipad>(), size);

      case map::USERPOINT:
        return QIcon(NavApp::getUserdataIcons()->getIconPath(base->asPtr<map::MapUserpoint>()->type));

      case map::LOGBOOK:
        return QIcon(":/littlenavmap/resources/icons/logbook.svg");
    }
  }
  return QIcon();
}

QAction *MapContextMenu::insertAction(QMenu& menu, mc::MenuActionType actionType, const QString& text,
                                      const QString& tip, const QIcon& icon, const map::MapBase *base, bool submenu,
                                      bool allowNoMapObject, const ActionCallback& callback)
{
  QString actionText = text;
  QIcon callbackIcon;
  QAction *action = nullptr;
  bool disable = false;

  if(callback)
    // Execute callback if not null
    callback(base, actionText, callbackIcon, disable, submenu);

  // Prepare associated menu data
  MenuData data;
  data.actionType = actionType;
  data.base = base; // pointer to object in "result" field

  if(base != nullptr)
  {
    // Has underlying map object(s) ==========================================
    if(actionText.contains("%1"))
      // Replace %1 with map object text
      actionText = actionText.arg(atools::elideTextShortMiddle(mapBaseText(base), TEXT_ELIDE));

    QIcon actionIcon;
    if(callbackIcon.isNull())
      // Use icon from function call or generate one
      actionIcon = icon.isNull() ? mapBaseIcon(base) : icon;
    else
      // Use icon from callback if it provided one
      actionIcon = callbackIcon;

    // Use either given or generated icon - needs main window as parent for status tip
    action = new QAction(actionIcon, actionText, mainWindow);
    action->setDisabled(disable);
  }
  else
  {
    // Has no underlying map object - click into map ==========================================
    if(actionText.contains("%1"))
    {
      if(allowNoMapObject)
        // No position is allowed - use pre-defined text instead of map feature text
        actionText = actionText.arg(tr("here"));
      else
        // Replace %1 since menu item will be disabled anyway
        actionText = actionText.arg(QString());
    }

    // needs main window as parent for status tip
    action = new QAction(icon, actionText, mainWindow);
    if(callback)
      // Overrides if callback is set
      action->setDisabled(disable);
    else
      // Disable if click without map object is not allowed
      action->setDisabled(!allowNoMapObject);
  }

  // Tooltip only if enabled for menus
  if(menu.toolTipsVisible())
    action->setToolTip(tip);
  action->setStatusTip(tip);

  // Action to menu
  menu.addAction(action);

  // Action to index for later deletion
  actionsAndMenus.append(action);

  // Insert reference data to index
  dataIndex.append(data);

  // Set index into action
  action->setData(dataIndex.size() - 1);
  return action;
}

void MapContextMenu::insertMenuOrAction(QMenu& menu, mc::MenuActionType actionType,
                                        const map::MapSearchResultIndex& index, const QString& text,
                                        const QString& tip, const QIcon& icon, bool allowNoMapObject,
                                        const ActionCallback& callback)
{
  if(index.isEmpty())
    // Insert an disabled menu with base text - %1 replaced with empty string
    insertAction(menu, actionType, text, tip, icon, nullptr, false, allowNoMapObject, callback);
  else if(index.size() == 1)
    // Insert a single menu item with text
    insertAction(menu, actionType, text, tip, icon, index.first(), false, allowNoMapObject, callback);
  else
  {
    // Add a sub-menu with all found entries - cut off and add "more..." if size exceeded
    QMenu *subMenu = new QMenu(text.arg(QString()), mainWindow);
    subMenu->setIcon(icon);
    if(menu.toolTipsVisible())
      subMenu->setToolTip(tip); // Inherit from parent menu
    subMenu->setStatusTip(tip);
    subMenu->setToolTipsVisible(menu.toolTipsVisible());
    menu.addMenu(subMenu);
    actionsAndMenus.append(subMenu);

    // Add menu items to sub. One for each map object in index
    for(int i = 0, idx = 0; i < index.size(); i++, idx++)
    {
      if(idx >= MAX_MENU_ITEMS)
      {
        // Overflow - create new sub-menu
        QMenu *sub = new QMenu(tr("&More"), mainWindow);
        subMenu->addMenu(sub);
        subMenu->setToolTipsVisible(menu.toolTipsVisible());

        // Add for later deletion
        actionsAndMenus.append(sub);
        subMenu = sub;
        idx = 0;
      }

      // Insert related menu item for map object
      insertAction(*subMenu, actionType, tr("%1"), tip, QIcon(), index.at(i), true, allowNoMapObject, callback);
    }

    if(allowNoMapObject)
      // Add a coordinate menu item if no map object is allowed
      insertAction(*subMenu, actionType, tr("&Position %1").arg(Unit::coords(mapBasePos->position)), tip,
                   QIcon(":/littlenavmap/resources/icons/coordinate.svg"), mapBasePos, true, allowNoMapObject,
                   callback);
  }
}

void MapContextMenu::insertInformationMenu(QMenu& menu)
{
  insertMenuOrAction(menu, mc::INFORMATION,
                     MapSearchResultIndex().
                     addRef(*result,
                            map::AIRPORT | map::VOR | map::NDB | map::ILS | map::WAYPOINT | map::AIRWAY | map::TRACK |
                            map::USERPOINT | map::AIRSPACE | map::AIRCRAFT | map::AIRCRAFT_AI | map::AIRCRAFT_ONLINE |
                            map::LOGBOOK).sort(DEFAULT_TYPE_SORT, alphaSort),
                     tr("&Show Information for %1"),
                     tr("Show information for airport or navaid"),
                     QIcon(":/littlenavmap/resources/icons/globals.svg"));
}

void MapContextMenu::insertProcedureMenu(QMenu& menu)
{
  // Callback to enable/disable and change text depending on state
  ActionCallback callback =
    [ = ](const map::MapBase *base, QString& text, QIcon&, bool& disable, bool submenu) -> void
    {
      if(base != nullptr)
      {
        bool departure = false, destination = false, arrivalProc = false, departureProc = false;
        disable = false;
        procedureFlags(base, &departure, &destination, nullptr, &arrivalProc, &departureProc);
        if(!arrivalProc && !departureProc)
        {
          // No procedures - disable and add remark
          text.append(tr(" (has no procedure)"));
          disable = true;
        }
        else
        {
          if(destination)
          {
            if(arrivalProc)
              // Airport is destination and has approaches/STAR
              text = submenu ? tr("%1 - arrival procedures") : tr("Show arrival &procedures for %1");
            else
            {
              // Airport is destination and has no approaches/STAR - disable
              text = submenu ? tr("%1 (no arrival)") : tr("Show arrival &procedures for %1 (no arrival)");
              disable = true;
            }
          }
          if(departure)
          {
            if(departureProc)
              // Airport is departure and has SID
              text = submenu ? tr("%1 - departure &procedures") : tr("Show departure &procedures for %1");
            else
            {
              // Airport is departure and has no SID - disable
              text = submenu ? tr("%1 (no departure)") : tr("Show departure &procedures for %1 (no departure)");
              disable = true;
            }
          }
        }
      }
      else
        // No object - should not happen
        disable = true;
    };

  insertMenuOrAction(menu, mc::PROCEDURE,
                     MapSearchResultIndex().addRef(*result, map::AIRPORT).sort(alphaSort),
                     tr("Show &procedures for %1"),
                     tr("Show procedures for this airport"),
                     QIcon(":/littlenavmap/resources/icons/approach.svg"), false /* allowNoMapObject */, callback);
}

void MapContextMenu::insertCustomProcedureMenu(QMenu& menu)
{
  ActionCallback callback =
    [ = ](const map::MapBase *base, QString& text, QIcon&, bool& disable, bool submenu) -> void
    {
      bool departure = false, destination = false;
      procedureFlags(base, &departure, &destination);
      disable = base == nullptr;
      if(destination)
        // Airport is destination - insert into plan
        text = submenu ? tr("%1 - insert into Flight Plan") : tr("&Create Approach to %1 and insert into Flight Plan");
      else
        // Airport is not destination - insert into plan and use airport
        text = submenu ? tr("%1 - use as destination") : tr("&Create Approach and use %1 as Destination");
    };

  insertMenuOrAction(menu, mc::CUSTOMPROCEDURE,
                     MapSearchResultIndex().addRef(*result, map::AIRPORT).sort(alphaSort),
                     tr("Create &custom procedure for %1"),
                     tr("Add a custom approach to this airport to the flight plan"),
                     QIcon(":/littlenavmap/resources/icons/approachcustom.svg"),
                     false /* allowNoMapObject */,
                     callback);
}

void MapContextMenu::insertMeasureMenu(QMenu& menu)
{
  ActionCallback callback =
    [ = ](const map::MapBase *base, QString& text, QIcon&, bool& disable, bool) -> void
    {
      disable = !visibleOnMap || !NavApp::getMapMarkHandler()->isShown(map::MARK_MEASUREMENT);
      if(base == nullptr)
        // Any position
        text = tr("&Measure Distance from here");

      if(!NavApp::getMapMarkHandler()->isShown(map::MARK_MEASUREMENT))
        // Hidden - add remark and disable
        text.append(tr(" (hidden on map)"));
    };

  insertMenuOrAction(menu, mc::MEASURE, MapSearchResultIndex().
                     addRef(*result, map::AIRPORT | map::VOR | map::NDB | map::WAYPOINT).
                     sort(DEFAULT_TYPE_SORT, alphaSort),
                     tr("&Measure Distance from %1"),
                     tr("Measure great circle distance on the map"),
                     QIcon(":/littlenavmap/resources/icons/distancemeasure.svg"),
                     true /* allowNoMapObject */,
                     callback);
}

void MapContextMenu::insertNavaidRangeMenu(QMenu& menu)
{
  ActionCallback callback =
    [ = ](const map::MapBase *base, QString& text, QIcon&, bool& disable, bool) -> void
    {
      disable = base == nullptr || !NavApp::getMapMarkHandler()->isShown(map::MARK_RANGE_RINGS);
      if(!NavApp::getMapMarkHandler()->isShown(map::MARK_RANGE_RINGS))
        // Hidden - add remark and disable
        text.append(tr(" (hidden on map)"));
      else if(base != nullptr)
      {
        int range = 0;
        if(base->objType == map::VOR)
          range = base->asObj<map::MapVor>().range;
        else if(base->objType == map::NDB)
          range = base->asObj<map::MapNdb>().range;

        if(range < 1)
        {
          // VOR or NDB without valid range
          text.append(tr(" (no range)"));
          disable = true;
        }
      }
    };

  insertMenuOrAction(menu, mc::NAVAIDRANGE,
                     MapSearchResultIndex().addRef(*result, map::VOR | map::NDB).sort(DEFAULT_TYPE_SORT, alphaSort),
                     tr("Show &Navaid Range for %1"),
                     tr("Show a ring for the radio navaid range on the map"),
                     QIcon(":/littlenavmap/resources/icons/navrange.svg"),
                     false /* allowNoMapObject */,
                     callback);
}

void MapContextMenu::insertPatternMenu(QMenu& menu)
{
  ActionCallback callback =
    [ = ](const map::MapBase *base, QString& text, QIcon&, bool& disable, bool) -> void
    {
      disable = base == nullptr || !NavApp::getMapMarkHandler()->isShown(map::MARK_PATTERNS);
      if(!NavApp::getMapMarkHandler()->isShown(map::MARK_PATTERNS))
        // Hidden - add remark and disable
        text.append(tr(" (hidden on map)"));
    };

  insertMenuOrAction(menu, mc::PATTERN, MapSearchResultIndex().addRef(*result, map::AIRPORT).sort(alphaSort),
                     tr("Display Airport &Traffic Pattern at %1 ..."),
                     tr("Show a traffic pattern to a runway for this airport"),
                     QIcon(":/littlenavmap/resources/icons/trafficpattern.svg"),
                     false /* allowNoMapObject */,
                     callback);
}

void MapContextMenu::insertHoldMenu(QMenu& menu)
{
  ActionCallback callback =
    [ = ](const map::MapBase *base, QString& text, QIcon&, bool& disable, bool) -> void
    {
      disable = !visibleOnMap || !NavApp::getMapMarkHandler()->isShown(map::MARK_HOLDS);
      if(base == nullptr)
        text = tr("Display &Holding here ...");
      if(!NavApp::getMapMarkHandler()->isShown(map::MARK_HOLDS))
        // Hidden - add remark and disable
        text.append(tr(" (hidden on map)"));
    };

  insertMenuOrAction(menu, mc::HOLD, MapSearchResultIndex().
                     addRef(*result,
                            map::AIRPORT | map::VOR | map::NDB | map::WAYPOINT | map::USERPOINT | map::USERPOINTROUTE).
                     sort(DEFAULT_TYPE_SORT, alphaSort),
                     tr("Display &Holding at %1 ..."),
                     tr("Show a holding pattern on the map at a position or a navaid"),
                     QIcon(":/littlenavmap/resources/icons/hold.svg"),
                     true /* allowNoMapObject */,
                     callback);
}

void MapContextMenu::insertDepartureMenu(QMenu& menu)
{
  MapSearchResultIndex index;
  index.addRef(*result, map::AIRPORT | map::PARKING | map::HELIPAD).sort(DEFAULT_TYPE_SORT, alphaSort);

  // Erase all helipads without start position
  index.erase(std::remove_if(index.begin(), index.end(), [](const map::MapBase *base) -> bool {
    return base != nullptr && base->getType() == map::HELIPAD &&
    base->asPtr<map::MapHelipad>()->startId == -1;
  }), index.end());

  ActionCallback callback =
    [ = ](const map::MapBase *base, QString& text, QIcon&, bool& disable, bool) -> void
    {
      disable = base == nullptr;

      if(base != nullptr)
      {
        map::MapAirport airport;
        if(base->getType() == map::HELIPAD)
        {
          // User clicked on helipad ================================
          const map::MapHelipad *helipad = base->asPtr<map::MapHelipad>();

          // Get related airport
          airport = NavApp::getAirportQuerySim()->getAirportById(helipad->airportId);

          text = tr("Set %1 at %2 as &Flight Plan Departure").
                 arg(atools::elideTextShortMiddle(map::helipadText(*helipad), TEXT_ELIDE)).
                 arg(atools::elideTextShortMiddle(map::airportText(airport), TEXT_ELIDE));
        }
        else if(base->getType() == map::PARKING)
        {
          // User clicked on parking ================================
          const map::MapParking *parking = base->asPtr<map::MapParking>();

          // Get related airport
          airport = NavApp::getAirportQuerySim()->getAirportById(parking->airportId);

          text = tr("Set %1 at %2 as &Flight Plan Departure").
                 arg(atools::elideTextShortMiddle(map::parkingText(*parking), TEXT_ELIDE)).
                 arg(atools::elideTextShortMiddle(map::airportText(airport), TEXT_ELIDE));
        }
        else if(base->getType() == map::AIRPORT)
          // Clicked on airport
          airport = base->asObj<map::MapAirport>();

        bool departure = false, destination = false;
        procedureFlags(&airport, &departure, &destination);
        if(destination)
        {
          // Is already destination - disable
          disable = true;
          text.append(tr(" (is destination)"));
        }
        if(departure && base->getType() != map::HELIPAD && base->getType() != map::PARKING)
        {
          // Is already departure airport - disable
          disable = true;
          text.append(tr(" (is departure)"));
        }
      }
    };

  insertMenuOrAction(menu, mc::DEPARTURE, index,
                     tr("Set %1 as &Flight Plan Departure"),
                     tr("Set airport as departure in the flight plan"),
                     QIcon(":/littlenavmap/resources/icons/airportroutedest.svg"),
                     false /* allowNoMapObject */,
                     callback);
}

void MapContextMenu::insertDestinationMenu(QMenu& menu)
{
  ActionCallback callback =
    [ = ](const map::MapBase *base, QString& text, QIcon&, bool& disable, bool) -> void
    {
      disable = base == nullptr;
      if(base != nullptr)
      {
        bool departure = false, destination = false;
        procedureFlags(base, &departure, &destination);
        if(destination)
        {
          // Is already destination - disable
          disable = true;
          text.append(tr(" (is destination)"));
        }
        if(departure)
        {
          // Is already departure airport - disable
          disable = true;
          text.append(tr(" (is departure)"));
        }
      }
    };

  insertMenuOrAction(menu, mc::DESTINATION, MapSearchResultIndex().addRef(*result, map::AIRPORT).sort(alphaSort),
                     tr("Set %1 as Flight Plan &Destination"),
                     tr("Set airport as destination in the flight plan"),
                     QIcon(":/littlenavmap/resources/icons/airportroutestart.svg"),
                     false /* allowNoMapObject */,
                     callback);
}

void MapContextMenu::insertAlternateMenu(QMenu& menu)
{
  ActionCallback callback =
    [ = ](const map::MapBase *base, QString& text, QIcon&, bool& disable, bool) -> void
    {
      disable = base == nullptr;
      if(base != nullptr)
      {
        bool departure = false, destination = false, alternate = false;
        procedureFlags(base, &departure, &destination, &alternate);

        // Do not allow to add as alternate if already part of plane
        if(destination)
        {
          disable = true;
          text.append(tr(" (is destination)"));
        }
        if(departure)
        {
          disable = true;
          text.append(tr(" (is departure)"));
        }
        if(alternate)
        {
          disable = true;
          text.append(tr(" (is alternate)"));
        }
      }
    };

  insertMenuOrAction(menu, mc::ALTERNATE, MapSearchResultIndex().addRef(*result, map::AIRPORT).sort(alphaSort),
                     tr("&Add %1 as Flight Plan Alternate"),
                     tr("Add airport as alternate to the flight plan"),
                     QIcon(":/littlenavmap/resources/icons/airportroutealt.svg"),
                     false /* allowNoMapObject */,
                     callback);
}

void MapContextMenu::insertAddRouteMenu(QMenu& menu)
{
  ActionCallback callback =
    [ = ](const map::MapBase *base, QString& text, QIcon&, bool& disable, bool) -> void
    {
      if(base == nullptr)
        // Modify text only
        text = tr("Add Position to Flight &Plan");
      disable = !visibleOnMap;
    };

  insertMenuOrAction(menu, mc::ADDROUTE,
                     MapSearchResultIndex().
                     addRef(*result, map::AIRPORT | map::VOR | map::NDB | map::WAYPOINT | map::USERPOINT).
                     sort(DEFAULT_TYPE_SORT, alphaSort),
                     tr("Add %1 to Flight &Plan"),
                     tr("Add airport, navaid or position to nearest flight plan leg"),
                     QIcon(":/littlenavmap/resources/icons/routeadd.svg"),
                     true /* allowNoMapObject */,
                     callback);
}

void MapContextMenu::insertAppendRouteMenu(QMenu& menu)
{
  ActionCallback callback =
    [ = ](const map::MapBase *base, QString& text, QIcon&, bool& disable, bool) -> void
    {
      if(base == nullptr)
        // Modify text only
        text = tr("Append Position to &Flight Plan");
      disable = !visibleOnMap;
    };

  insertMenuOrAction(menu, mc::APPENDROUTE,
                     MapSearchResultIndex().
                     addRef(*result, map::AIRPORT | map::VOR | map::NDB | map::WAYPOINT | map::USERPOINT).
                     sort(DEFAULT_TYPE_SORT, alphaSort),
                     tr("Append %1 to &Flight Plan"),
                     tr("Append airport, navaid or position to the end of the flight plan"),
                     QIcon(":/littlenavmap/resources/icons/routeadd.svg"),
                     true /* allowNoMapObject */,
                     callback);
}

void MapContextMenu::insertDeleteRouteWaypointMenu(QMenu& menu)
{
  MapSearchResultIndex index;
  index.addRef(*result, map::AIRPORT | map::VOR | map::NDB | map::WAYPOINT | map::USERPOINTROUTE).
  sort(DEFAULT_TYPE_SORT, alphaSort);

  // Erase all points which are not route legs
  index.erase(std::remove_if(index.begin(), index.end(), [ = ](const map::MapBase *base) -> bool
  {
    return map::routeIndex(base) == -1;
  }), index.end());

  ActionCallback callback =
    [ = ](const map::MapBase *base, QString& text, QIcon& icon, bool& disable, bool) -> void
    {
      QString procName = procedureName(base);
      if(!procName.isEmpty())
      {
        // Leg is part of a procedure
        text = tr("&Delete %1 from Flight Plan").arg(procName);
        icon = QIcon(":/littlenavmap/resources/icons/approach.svg");
      }
      else
      {
        bool canEdit = canEditRoutePoint(base);
        if(base != nullptr)
        {
          if(canEdit)
            text = tr("&Delete %1 from Flight Plan");
          else
            text = tr("&Delete from Flight Plan (is procedure)");
        }
        disable = !visibleOnMap || !canEdit;
      }
    };

  insertMenuOrAction(menu, mc::DELETEROUTEWAYPOINT, index,
                     tr("&Delete %1 from Flight Plan"),
                     tr("Delete airport, navaid or position from the flight plan"),
                     QIcon(":/littlenavmap/resources/icons/routedeleteleg.svg"),
                     false /* allowNoMapObject */,
                     callback);
}

void MapContextMenu::insertEditRouteUserpointMenu(QMenu& menu)
{
  MapSearchResultIndex index;
  index.addRef(*result, map::AIRPORT | map::VOR | map::NDB | map::WAYPOINT | map::USERPOINTROUTE).
  sort(DEFAULT_TYPE_SORT, alphaSort);

  // Erase all points which are not route legs
  index.erase(std::remove_if(index.begin(), index.end(), [ = ](const map::MapBase *base) -> bool
  {
    return map::routeIndex(base) == -1;
  }), index.end());

  ActionCallback callback = [ = ](const map::MapBase *base, QString& text, QIcon&, bool& disable, bool) -> void
                            {
                              bool canEdit = canEditRouteComment(base);
                              if(base != nullptr)
                              {
                                if(canEdit)
                                {
                                  // Modify text depending on type
                                  if(base->getType() == map::USERPOINTROUTE)
                                    text = tr("Edit Flight Plan &Position %1 ...");
                                  else
                                    text = tr("Edit Flight Plan &Position Remarks for %1 ...");
                                }
                                else
                                  text = tr("Edit Flight Plan &Position (is procedure)");
                              }
                              disable = !visibleOnMap || !canEdit;
                            };

  insertMenuOrAction(menu, mc::EDITROUTEUSERPOINT, index,
                     tr("Edit Flight Plan &Position %1 ..."),
                     tr("Edit remark, name or coordinate of flight plan position"),
                     QIcon(":/littlenavmap/resources/icons/routestring.svg"),
                     false /* allowNoMapObject */,
                     callback);
}

void MapContextMenu::insertUserpointAddMenu(QMenu& menu)
{
  ActionCallback callback =
    [ = ](const map::MapBase *base, QString& text, QIcon&, bool& disable, bool) -> void
    {
      if(base == nullptr)
        // Modify text only
        text = tr("Add &Userpoint here ...");
      disable = !visibleOnMap;
    };

  insertMenuOrAction(menu, mc::USERPOINTADD,
                     MapSearchResultIndex().
                     addRef(*result, map::AIRPORT | map::VOR | map::NDB | map::WAYPOINT | map::USERPOINT).
                     sort(DEFAULT_TYPE_SORT, alphaSort),
                     tr("Add &Userpoint %1 ..."),
                     tr("Add a userpoint at this position"),
                     QIcon(":/littlenavmap/resources/icons/userdata_add.svg"),
                     true /* allowNoMapObject */,
                     callback);
}

void MapContextMenu::insertUserpointEditMenu(QMenu& menu)
{
  insertMenuOrAction(menu, mc::USERPOINTEDIT, MapSearchResultIndex().addRef(*result, map::USERPOINT).sort(alphaSort),
                     tr("&Edit Userpoint %1 ..."),
                     tr("Edit the userpoint at this position"),
                     QIcon(":/littlenavmap/resources/icons/userdata_edit.svg"));
}

void MapContextMenu::insertUserpointMoveMenu(QMenu& menu)
{
  insertMenuOrAction(menu, mc::USERPOINTMOVE, MapSearchResultIndex().addRef(*result, map::USERPOINT).sort(alphaSort),
                     tr("&Move Userpoint %1"),
                     tr("Move the userpoint to a new position on the map"),
                     QIcon(":/littlenavmap/resources/icons/userdata_move.svg"));
}

void MapContextMenu::insertUserpointDeleteMenu(QMenu& menu)
{
  insertMenuOrAction(menu, mc::USERPOINTDELETE, MapSearchResultIndex().addRef(*result, map::USERPOINT).sort(alphaSort),
                     tr("&Delete Userpoint %1"),
                     tr("Remove the userpoint at this position"),
                     QIcon(":/littlenavmap/resources/icons/userdata_delete.svg"));
}

void MapContextMenu::insertLogEntryEdit(QMenu& menu)
{
  insertMenuOrAction(menu, mc::LOGENTRYEDIT, MapSearchResultIndex().addRef(*result, map::LOGBOOK).sort(alphaSort),
                     tr("Edit &Log Entry %1 ..."),
                     tr("Edit the logbook entry at this position"),
                     QIcon(":/littlenavmap/resources/icons/logdata_edit.svg"));
}

void MapContextMenu::insertShowInSearchMenu(QMenu& menu)
{
  MapSearchResultIndex index;
  index.addRef(*result, map::AIRPORT | map::VOR | map::NDB | map::WAYPOINT | map::USERPOINT | map::AIRSPACE |
               map::AIRCRAFT | map::AIRCRAFT_AI | map::AIRCRAFT_ONLINE | map::LOGBOOK).
  sort(DEFAULT_TYPE_SORT, alphaSort);

  // Erase all non-online airspaces and aircraft which are not online client shadows
  index.erase(std::remove_if(index.begin(), index.end(), [](const map::MapBase *base) -> bool
  {
    if(base->getType() == map::AIRSPACE)
      return !base->asPtr<map::MapAirspace>()->src.testFlag(map::AIRSPACE_SRC_ONLINE);

    if(base->objType == map::AIRCRAFT || base->objType == map::AIRCRAFT_AI)
      return !map::isAircraftShadow(base);

    return false;
  }), index.end());

  insertMenuOrAction(menu, mc::SHOWINSEARCH, index,
                     tr("&Show %1 in Search"),
                     tr("Show the airport, navaid, userpoint or other object in the search window"),
                     QIcon(":/littlenavmap/resources/icons/search.svg"));
}

bool MapContextMenu::exec(QPoint menuPos, QPoint point)
{
  clear();

  const MapScreenIndex *screenIndex = mapWidget->getScreenIndex();
  int screenSearchDist = OptionData::instance().getMapClickSensitivity();

  // ===================================================================================
  // Build menu - add actions

  if(!point.isNull())
  {
    qreal lon, lat;
    // Cursor can be outside of map region
    visibleOnMap = mapWidget->geoCoordinates(point.x(), point.y(), lon, lat);

    if(visibleOnMap)
    {
      // Cursor is not off-globe
      mapBasePos->position = atools::geo::Pos(lon, lat);
      distMarkerIndex = screenIndex->getNearestDistanceMarkIndex(point.x(), point.y(), screenSearchDist);
      rangeMarkerIndex = screenIndex->getNearestRangeMarkIndex(point.x(), point.y(), screenSearchDist);
      trafficPatternIndex = screenIndex->getNearestTrafficPatternIndex(point.x(), point.y(), screenSearchDist);
      holdIndex = screenIndex->getNearestHoldIndex(point.x(), point.y(), screenSearchDist);
    }
  }

  // Get objects near position =============================================================
  screenIndex->getAllNearest(point.x(), point.y(), screenSearchDist, *result, map::QUERY_NONE);

  // Remove online aircraft from onlineAircraft which also have a simulator shadow in simAircraft
  // Menus should only show the online part
  NavApp::getOnlinedataController()->filterOnlineShadowAircraft(result->onlineAircraft, result->aiAircraft);
  result->moveOnlineAirspacesToFront();

  // Disable all general menu items that depend on position ===========================
  ui->actionMapSetMark->setEnabled(visibleOnMap);
  ui->actionMapSetHome->setEnabled(visibleOnMap);

  // Copy coordinates ===================
  ui->actionMapCopyCoordinates->setEnabled(visibleOnMap);
  if(visibleOnMap)
    ui->actionMapCopyCoordinates->setText(ui->actionMapCopyCoordinates->text().arg(Unit::coords(mapBasePos->position)));

  // Enable or disable map marks ===========================
  ui->actionMapRangeRings->setEnabled(visibleOnMap && NavApp::getMapMarkHandler()->isShown(map::MARK_RANGE_RINGS));

  // Hide marks
  ui->actionMapHideOneRangeRing->setEnabled(visibleOnMap && rangeMarkerIndex != -1);
  ui->actionMapHideDistanceMarker->setEnabled(visibleOnMap && distMarkerIndex != -1);
  ui->actionMapHideTrafficPattern->setEnabled(visibleOnMap && trafficPatternIndex != -1);
  ui->actionMapHideHold->setEnabled(visibleOnMap && holdIndex != -1);

  if(!NavApp::getMapMarkHandler()->isShown(map::MARK_RANGE_RINGS))
    ui->actionMapRangeRings->setText(ui->actionMapRangeRings->text() + tr(" (hidden on map)"));

  // Build the menu =============================================================
  // The result must not be modified after building the menu because objects are referenced via dataIndex by pointers
  buildMainMenu();

  // Show the menu ------------------------------------------------
  selectedAction = menu.exec(menuPos);

  if(selectedAction != nullptr)
  {
    // A menu items was clicked ===============================
    qDebug() << Q_FUNC_INFO << "selectedAction text" << selectedAction->text()
             << "name" << selectedAction->objectName();

    MenuData data = dataIndex.value(selectedAction->data().toInt());
    selectedActionType = data.actionType;
    selectedBase = data.base;
    selectedRouteIndex = map::routeIndex(data.base);

    qDebug() << Q_FUNC_INFO << "selectedActionType" << selectedActionType;
    if(selectedBase != nullptr)
      qDebug() << Q_FUNC_INFO << "selectedBase" << *selectedBase;

    return true;
  }
  else
    // Nothing clicked ==============================
    qDebug() << Q_FUNC_INFO << "null";

  return false;
}

const atools::geo::Pos& MapContextMenu::getPos() const
{
  return mapBasePos->position;
}

bool MapContextMenu::canEditRouteComment(const map::MapBase *base) const
{
  if(base != nullptr)
  {
    int routeIndex = map::routeIndex(base);
    return routeIndex != -1 && NavApp::getRouteConst().canEditComment(routeIndex);
  }
  return false;
}

bool MapContextMenu::canEditRoutePoint(const map::MapBase *base) const
{
  if(base != nullptr)
  {
    int routeIndex = map::routeIndex(base);
    return routeIndex != -1 && NavApp::getRouteConst().canEditPoint(routeIndex);
  }
  return false;
}

QString MapContextMenu::procedureName(const map::MapBase *base) const
{
  if(base != nullptr)
  {
    int routeIndex = map::routeIndex(base);
    if(routeIndex != -1)
    {
      const Route& route = NavApp::getRouteConst();
      const RouteLeg& leg = route.value(routeIndex);
      if(leg.isAnyProcedure())
        return route.getProcedureLegText(leg.getProcedureType());
    }
  }
  return QString();
}

void MapContextMenu::procedureFlags(const map::MapBase *base, bool *departure, bool *destination, bool *alternate,
                                    bool *arrivalProc, bool *departureProc) const
{
  if(departure != nullptr)
    *departure = false;
  if(destination != nullptr)
    *destination = false;
  if(alternate != nullptr)
    *alternate = false;
  if(arrivalProc != nullptr)
    *arrivalProc = false;
  if(departureProc != nullptr)
    *departureProc = false;

  if(base != nullptr && base->getType() == map::AIRPORT)
  {
    const map::MapAirport *airport = base->asPtr<map::MapAirport>();

    if(departure != nullptr)
      *departure = NavApp::getRouteConst().isAirportDeparture(airport->ident);
    if(destination != nullptr)
      *destination = NavApp::getRouteConst().isAirportDestination(airport->ident);
    if(alternate != nullptr)
      *alternate = NavApp::getRouteConst().isAirportAlternate(airport->ident);

    if(arrivalProc != nullptr)
      *arrivalProc = NavApp::getMapQuery()->hasAnyArrivalProcedures(*airport);
    if(departureProc != nullptr)
      *departureProc = NavApp::getMapQuery()->hasDepartureProcedures(*airport);
  }
}
