/*****************************************************************************
* Copyright 2015-2023 Alexander Barthel alex@littlenavmap.org
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
#include "query/airportquery.h"
#include "query/mapquery.h"
#include "online/onlinedatacontroller.h"
#include "gui/actionstatesaver.h"
#include "gui/actiontextsaver.h"
#include "options/optiondata.h"
#include "route/route.h"
#include "app/navapp.h"
#include "atools.h"
#include "common/unit.h"
#include "common/symbolpainter.h"

#include <ui_mainwindow.h>

#include <QStringBuilder>

using map::MapResultIndex;

// Maximum number of items in disambiguation sub-menus
const static int MAX_MENU_ITEMS = 10;

// Truncate map object texts in the middle
const static int TEXT_ELIDE = 30;
const static int TEXT_ELIDE_AIRPORT_NAME = 15;

// Keeps information for each menu, action and sub-menu.
struct MapContextMenu::MenuData
{
  mc::MenuActionType actionType = mc::NONE;

  // Points to map object in "result"
  const map::MapBase *base;
};

// Default sort order for disambiguation sub-menus
const static QVector<map::MapTypes> DEFAULT_TYPE_SORT(
{
  map::MARK_RANGE,
  map::MARK_DISTANCE,
  map::MARK_PATTERNS,
  map::MARK_HOLDING,
  map::MARK_MSA,
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
  map::PROCEDURE_POINT,
  map::HOLDING,
  map::AIRPORT_MSA,
  map::AIRWAY,
  map::TRACK,
  map::AIRCRAFT_AI_SHIP,
  map::AIRSPACE,
});

MapContextMenu::MapContextMenu(QMainWindow *mainWindowParam, MapWidget *mapWidgetParam, const Route& routeParam)
  : mapWidget(mapWidgetParam), mainWindow(mainWindowParam), route(routeParam), mapMenu(mainWindowParam)
{
  // Do not copy and modify result set since this can break the index
  // Modify only a referencing MapResultIndex
  result = new map::MapResult;
  mapBasePos = new map::MapPos(atools::geo::EMPTY_POS);

  ui = NavApp::getMainUi();
  QList<QAction *> actions(
  {
    // Save state since widgets are shared with others
    ui->actionMapCopyCoordinates,
    ui->actionMapHold,
    ui->actionMapAirportMsa,
    ui->actionMapNavaidRange,
    ui->actionMapRangeRings,
    ui->actionMapTrafficPattern,
    ui->actionRouteAddPos,
    ui->actionMapRouteAirportAlternate,
    ui->actionMapRouteAirportDest,
    ui->actionMapRouteAirportStart,
    ui->actionRouteAppendPos
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
  *mapBasePos = map::MapPos(atools::geo::EMPTY_POS);
  mapMenu.clear();

  // Delete all generated action
  qDeleteAll(actionsAndMenus);
  actionsAndMenus.clear();

  result->clear();
}

int MapContextMenu::getSelectedId() const
{
  return selectedBase != nullptr ? selectedBase->id : -1;
}

const atools::geo::Pos& MapContextMenu::getPos() const
{
  return mapBasePos->position;
}

void MapContextMenu::buildMainMenu()
{
  mapMenu.clear();

  // Inherit tool tip status from well know menu
  mapMenu.setToolTipsVisible(NavApp::isMenuToolTipsVisible());

  insertInformationMenu(mapMenu);
  mapMenu.addSeparator();

  insertDepartureMenu(mapMenu);
  insertDestinationMenu(mapMenu);
  insertAlternateMenu(mapMenu);
  mapMenu.addSeparator();

  insertCustomDepartureMenu(mapMenu);
  insertCustomApproachMenu(mapMenu);
  mapMenu.addSeparator();

  insertProcedureMenu(mapMenu);
  insertProcedureAddMenu(mapMenu);
  mapMenu.addSeparator();

  insertAddRouteMenu(mapMenu);
  insertAppendRouteMenu(mapMenu);
  insertDeleteRouteWaypointMenu(mapMenu);
  insertEditRouteUserpointMenu(mapMenu);
  mapMenu.addSeparator();

  insertDirectToMenu(mapMenu);
  mapMenu.addSeparator();

  insertMeasureMenu(mapMenu);
  ui->actionMapRangeRings->setText(ui->actionMapRangeRings->text() + tr("\tShift+Click"));
  mapMenu.addAction(ui->actionMapRangeRings);
  insertNavaidRangeMenu(mapMenu);
  insertPatternMenu(mapMenu);
  insertHoldMenu(mapMenu);
  insertAirportMsaMenu(mapMenu);
  insertRemoveMarkMenu(mapMenu);
  mapMenu.addSeparator();

  insertMarkAddonAirportMenu(mapMenu);
  mapMenu.addSeparator();

  QMenu *sub = mapMenu.addMenu(QIcon(":/littlenavmap/resources/icons/userdata.svg"), tr("&Userpoints"));
  sub->setToolTipsVisible(mapMenu.toolTipsVisible());
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
  mapMenu.addSeparator();

  insertLogEntryEdit(mapMenu);
  mapMenu.addSeparator();

  if(NavApp::isFullScreen())
  {
    // Add menu to exit full screen
    mapMenu.addAction(ui->actionShowFullscreenMap); // connected otherwise
    mapMenu.addSeparator();
  }

  sub = mapMenu.addMenu(tr("&More"));
  if(visibleOnMap)
  {
    // More rarely used menu items
    sub->addAction(ui->actionMapJumpCoordinates); // Used in MapWidget::contextMenuEvent()
    sub->addSeparator();
    insertShowInSearchMenu(*sub);
    insertShowInRouteMenu(*sub);
    sub->addSeparator();

    // Used directly in MapWidget::contextMenuEvent()
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
  return QString::localeAwareCompare(mapBaseText(base1, TEXT_ELIDE_AIRPORT_NAME).replace('&', QString()),
                                     mapBaseText(base2, TEXT_ELIDE_AIRPORT_NAME).replace('&', QString())) < 0;
}

QAction *MapContextMenu::insertAction(QMenu& menu, mc::MenuActionType actionType, const QString& text,
                                      const QString& tip, const QString& key, const QIcon& icon,
                                      const map::MapBase *base, bool submenu, bool allowNoMapObject,
                                      const ActionCallback& callback)
{
  QString actionText = text;
  QIcon callbackIcon;
  QAction *action = nullptr;
  bool disable = false;

  if(callback)
    // Execute callback if not null
    callback(base, actionText, callbackIcon, disable, submenu);

  if(!submenu)
  {
    // Add mouse button hints only for top level menus
    if(!key.isEmpty())
      actionText += "\t" + key;
  }

  // Prepare associated menu data
  MenuData data;
  data.actionType = actionType;
  data.base = base; // pointer to object in "result" field

  if(base != nullptr)
  {
    // Has underlying map object(s) ==========================================
    if(actionText.contains("%1"))
      // Replace %1 with map object text
      actionText = actionText.arg(atools::elideTextShortMiddle(mapBaseText(base, TEXT_ELIDE_AIRPORT_NAME), TEXT_ELIDE));

    QIcon actionIcon;
    if(callbackIcon.isNull())
      // Use icon from function call or generate one
      actionIcon = icon.isNull() ? mapBaseIcon(base, QFontMetrics(QApplication::font()).height()) : icon;
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
                                        const map::MapResultIndex& index, const QString& text,
                                        const QString& tip, const QString& key, const QIcon& icon,
                                        bool allowNoMapObject, const ActionCallback& callback)
{
  if(index.isEmpty())
    // Insert an disabled menu with base text - %1 replaced with empty string
    insertAction(menu, actionType, text, tip, key, icon, nullptr, false, allowNoMapObject, callback);
  else if(index.size() == 1)
    // Insert a single menu item with text
    insertAction(menu, actionType, text, tip, key, icon, index.constFirst(), false, allowNoMapObject, callback);
  else
  {
    QString subText = text.arg(QString());
    if(!key.isEmpty())
      subText += "\t" + key;

    // Add a sub-menu with all found entries - cut off and add "more..." if size exceeded
    QMenu *subMenu = new QMenu(subText, mainWindow);

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
      insertAction(*subMenu, actionType, tr("%1"), tip, QString(), QIcon(), index.at(i), true, allowNoMapObject, callback);
    }

    if(allowNoMapObject)
      // Add a coordinate menu item if no map object is allowed
      insertAction(*subMenu, actionType, tr("&Position %1").arg(Unit::coords(mapBasePos->position)), tip, QString(),
                   QIcon(":/littlenavmap/resources/icons/coordinate.svg"), mapBasePos, true, allowNoMapObject, callback);
  }
}

void MapContextMenu::insertInformationMenu(QMenu& menu)
{
  MapResultIndex index;
  index.addRef(*result, map::AIRPORT | map::VOR | map::NDB | map::ILS | map::HOLDING | map::AIRPORT_MSA | map::WAYPOINT | map::AIRWAY |
               map::TRACK | map::USERPOINT | map::AIRSPACE | map::AIRCRAFT | map::AIRCRAFT_AI | map::AIRCRAFT_ONLINE | map::LOGBOOK).
  sort(DEFAULT_TYPE_SORT, alphaSort);

  // Remove all online aircraft having a simulator shadow from the index for this menu item only
  // Collect shadowed online aircraft first
  OnlinedataController *onlineDataController = NavApp::getOnlinedataController();
  QSet<map::MapRef> refs;
  for(const map::MapBase *base : index)
  {
    // Check shadowed AI aircraft
    const map::MapAiAircraft *ai = base->asPtr<map::MapAiAircraft>();
    if(ai != nullptr && ai->getAircraft().isOnlineShadow())
      refs.insert(map::MapRef(onlineDataController->getShadowedOnlineAircraft(ai->getAircraft()).getId(), map::AIRCRAFT_ONLINE));
    else
    {
      // Check shadowed user aircraft
      const map::MapUserAircraft *user = base->asPtr<map::MapUserAircraft>();
      if(user != nullptr && user->getAircraft().isOnlineShadow())
        refs.insert(map::MapRef(onlineDataController->getShadowedOnlineAircraft(user->getAircraft()).getId(), map::AIRCRAFT_ONLINE));
    }
  }

  // Remove shadowed online aircraft from index
  index.erase(std::remove_if(index.begin(), index.end(), [refs](const map::MapBase *base) -> bool {
    return base != nullptr && refs.contains(base->getRef());
  }), index.end());

  insertMenuOrAction(menu, mc::INFORMATION, index, tr("&Show Information for %1"), tr("Show information for airport or navaid"),
                     tr("Click"), QIcon(":/littlenavmap/resources/icons/globals.svg"));
}

void MapContextMenu::insertProcedureMenu(QMenu& menu)
{
  // Callback to enable/disable and change text depending on state
  ActionCallback callback =
    [this](const map::MapBase *base, QString& text, QIcon&, bool& disable, bool submenu) -> void {
      if(base != nullptr && base->objType == map::AIRPORT)
      {
        bool departure = false, destination = false, arrivalProc = false, departureProc = false, roundtrip = false;
        disable = false;
        proc::procedureFlags(route, base, &departure, &destination, nullptr, &roundtrip, &arrivalProc, &departureProc);
        if(!arrivalProc && !departureProc)
        {
          // No procedures - disable and add remark
          text.append(tr(" (no procedure)"));
          disable = true;
        }
        else
        {
          if(destination && !roundtrip)
          {
            if(arrivalProc)
              // Airport is destination and has approaches/STAR
              text = submenu ? tr("%1 - Arrival Procedures") : tr("Show Arrival &Procedures for %1");
            else
            {
              // Airport is destination and has no approaches/STAR - disable
              text = submenu ? tr("%1 (no arrival)") : tr("Show Arrival &Procedures for %1 (no arrival)");
              disable = true;
            }
          }

          if(departure && !roundtrip)
          {
            if(departureProc)
              // Airport is departure and has SID
              text = submenu ? tr("%1 - Departure &Procedures") : tr("Show Departure &Procedures for %1");
            else
            {
              // Airport is departure and has no SID - disable
              text = submenu ? tr("%1 (no departure)") : tr("Show Departure &Procedures for %1 (no departure)");
              disable = true;
            }
          }
        }

        // Do our own text substitution for the airport to use shorter name
        if(text.contains("%1"))
          text = text.arg(map::airportTextShort(*base->asPtr<map::MapAirport>(), TEXT_ELIDE_AIRPORT_NAME));
      }
      else
        // No object or not an airport
        disable = true;
    };

  insertMenuOrAction(menu, mc::PROCEDURE,
                     MapResultIndex().addRef(*result, map::AIRPORT).sort(alphaSort),
                     tr("Show &Procedures for %1"), tr("Show procedures for this airport"),
                     QString(), QIcon(":/littlenavmap/resources/icons/approach.svg"), false /* allowNoMapObject */, callback);
}

void MapContextMenu::insertProcedureAddMenu(QMenu& menu)
{
  MapResultIndex index;
  index.addRef(*result, map::PROCEDURE_POINT);

  // Erase all points which are route legs since the cannot be added - only deleted ============================
  index.erase(std::remove_if(index.begin(), index.end(), [](const map::MapBase *base) -> bool {
    return map::routeIndex(base) != -1;
  }), index.end());

  // Sort points by compound id ignoring the leg id
  std::sort(index.begin(), index.end(), [](const map::MapBase *base1, const map::MapBase *base2) -> bool {
    // The index contains only one type of PROCEDURE_POINT now
    return base1->asPtr<map::MapProcedurePoint>()->compoundId() < base2->asPtr<map::MapProcedurePoint>()->compoundId();
  });

  // Erase duplicates by compound id ignoring the leg id
  index.erase(std::unique(index.begin(), index.end(), [](const map::MapBase *base1, const map::MapBase *base2)-> bool {
    // The index contains only one type of PROCEDURE_POINT
    return base1->asPtr<map::MapProcedurePoint>()->compoundId() == base2->asPtr<map::MapProcedurePoint>()->compoundId();
  }), index.end());

  index.sort(DEFAULT_TYPE_SORT, alphaSort);

  // ==================================================
  // Callback to enable/disable and change text depending on state
  int iconSize = menu.fontMetrics().height();
  ActionCallback callback =
    [this, iconSize](const map::MapBase *base, QString& text, QIcon& icon, bool& disable, bool submenu) -> void
    {
      disable = true;
      if(base != nullptr && base->objType == map::PROCEDURE_POINT)
      {
        const map::MapProcedurePoint *pt = base->asPtr<map::MapProcedurePoint>();
        if(pt != nullptr)
        {
          const proc::MapProcedureLeg& leg = pt->getLeg();
          const proc::MapProcedureLegs *legs = pt->legs;
          if(legs != nullptr && !legs->isAnyCustom())
          {
            map::MapAirport airport = NavApp::getAirportQueryNav()->getAirportById(leg.airportId);
            NavApp::getMapQueryGui()->getAirportSim(airport);

            bool departure = false, destination = false;
            proc::procedureFlags(route, &airport, &departure, &destination);

            if(submenu && pt->previewAll)
              // Override icon to show color indicating procedure in multi preview
              icon = SymbolPainter::createProcedurePreviewIcon(legs->previewColor, iconSize);

            if(leg.mapType & proc::PROCEDURE_SID_ALL)
            {
              if(departure)
                text = submenu ? tr("%1") : tr("&Insert %1 into Flight Plan");
              else
                text = submenu ?
                       tr("%1 and use ") % airport.ident % tr(" as Departure") :
                       tr("&Use ") % airport.ident % tr(" and %1 as Departure");
            }
            else if(leg.mapType & proc::PROCEDURE_ARRIVAL_ALL)
            {
              if(destination)
                text = submenu ? tr("%1") : tr("&Insert %1 into Flight Plan");
              else
                text = submenu ?
                       tr("%1 and use ") % airport.ident % tr(" as Destination") :
                       tr("&Use ") % airport.ident % tr(" and %1 as Destination");
            }
            disable = false;
          }
        }
      }
    };

  insertMenuOrAction(menu, mc::PROCEDUREADD, index,
                     tr("&Insert Procedure %1 into Flight Plan"), tr("Add procedure to flight plan"),
                     QString(), QIcon(":/littlenavmap/resources/icons/approachselect.svg"), false /* allowNoMapObject */, callback);
}

void MapContextMenu::insertCustomApproachMenu(QMenu& menu)
{
  ActionCallback callback =
    [this](const map::MapBase *base, QString& text, QIcon&, bool& disable, bool submenu) -> void {
      if(base != nullptr && base->objType == map::AIRPORT)
      {
        const map::MapAirport *airport = base->asPtr<map::MapAirport>();
        if(airport->noRunways())
        {
          // Heliport or other without runway - disable with remark
          text.append(tr(" (no runway)"));
          disable = true;
        }
        else
        {
          bool departure, destination;
          proc::procedureFlags(route, base, &departure, &destination);

          if(destination)
            // Airport is destination - insert into plan
            text = submenu ? tr("%1 ...") : tr("Select Destination &Runway for %1 ....");
          else if(!departure)
            // Airport is not destination - insert into plan and use airport
            text = submenu ? tr("%1 and use as Destination ...") : tr("Select &Runway and use %1 as Destination ...");
          else
            disable = true;
        }

        // Do our own text substitution for the airport to use shorter name
        if(text.contains("%1") && base->objType == map::AIRPORT)
          text = text.arg(map::airportTextShort(*base->asPtr<map::MapAirport>(), TEXT_ELIDE_AIRPORT_NAME));
      }
      else
        // No object or not an airport
        disable = true;
    };

  insertMenuOrAction(menu, mc::CUSTOMAPPROACH,
                     MapResultIndex().addRef(*result, map::AIRPORT).sort(alphaSort),
                     tr("Select Destination &Runway for %1"), tr("Select destination runway for airport"),
                     QString(), QIcon(":/littlenavmap/resources/icons/runwaydest.svg"), false /* allowNoMapObject */, callback);
}

void MapContextMenu::insertCustomDepartureMenu(QMenu& menu)
{
  ActionCallback callback =
    [this](const map::MapBase *base, QString& text, QIcon&, bool& disable, bool submenu) -> void {
      if(base != nullptr && base->objType == map::AIRPORT)
      {
        const map::MapAirport *airport = base->asPtr<map::MapAirport>();
        if(airport->noRunways())
        {
          // Heliport or other without runway - disable with remark
          text.append(tr(" (no runway)"));
          disable = true;
        }
        else
        {
          bool departure, destination;
          proc::procedureFlags(route, base, &departure, &destination);

          if(departure)
            // Airport is departure - insert into plan
            text = submenu ? tr("%1 ...") : tr("Select Departure &Runway for %1 ....");
          else if(!destination)
            // Airport is not destination - insert into plan and use airport
            text = submenu ? tr("%1 and use as Departure ...") : tr("Select &Runway and use %1 as Departure ...");
          else
            disable = true;
        }

        // Do our own text substitution for the airport to use shorter name
        if(text.contains("%1") && base->objType == map::AIRPORT)
          text = text.arg(map::airportTextShort(*base->asPtr<map::MapAirport>(), TEXT_ELIDE_AIRPORT_NAME));
      }
      else
        // No object or not an airport
        disable = true;
    };

  insertMenuOrAction(menu, mc::CUSTOMDEPARTURE,
                     MapResultIndex().addRef(*result, map::AIRPORT).sort(alphaSort),
                     tr("Select Departure &Runway for %1"), tr("Select departure runway for airport"),
                     QString(), QIcon(":/littlenavmap/resources/icons/runwaydepart.svg"), false /* allowNoMapObject */, callback);
}

void MapContextMenu::insertDirectToMenu(QMenu& menu)
{
  ActionCallback callback =
    [this](const map::MapBase *base, QString& text, QIcon&, bool& disable, bool) -> void {
      disable = !visibleOnMap;
      if(base == nullptr)
        // Any position
        text = text.arg(tr("here"));
      else
        text.append(proc::procedureTextSuffixDirectTo(disable, route, map::routeIndex(base), base->asPtr<map::MapAirport>()));
    };

  insertMenuOrAction(menu, mc::DIRECT, MapResultIndex().
                     addRef(*result, map::AIRPORT | map::VOR | map::NDB | map::WAYPOINT | map::USERPOINT | map::USERPOINTROUTE).
                     sort(DEFAULT_TYPE_SORT, alphaSort),
                     tr("&Direct to %1"), tr("Change flight plan to fly direct to navaid, flight plan leg or position"),
                     QString(), QIcon(":/littlenavmap/resources/icons/directto.svg"), true /* allowNoMapObject */, callback);
}

void MapContextMenu::insertMeasureMenu(QMenu& menu)
{
  ActionCallback callback =
    [this](const map::MapBase *base, QString& text, QIcon&, bool& disable, bool) -> void {
      disable = !visibleOnMap;
      if(base == nullptr)
        // Any position
        text = text.arg(tr("here"));
    };

  insertMenuOrAction(menu, mc::MEASURE, MapResultIndex().
                     addRef(*result, map::AIRPORT | map::VOR | map::NDB | map::WAYPOINT | map::USERPOINT).
                     sort(DEFAULT_TYPE_SORT, alphaSort),
                     tr("&Measure Distance from %1"), tr("Measure great circle distance on the map"),
                     tr("Ctrl+Click"), QIcon(":/littlenavmap/resources/icons/distancemeasure.svg"), true /* allowNoMapObject */, callback);
}

void MapContextMenu::insertNavaidRangeMenu(QMenu& menu)
{
  ActionCallback callback =
    [](const map::MapBase *base, QString& text, QIcon&, bool& disable, bool) -> void {
      disable = base == nullptr;

      if(base != nullptr)
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
                     MapResultIndex().addRef(*result, map::VOR | map::NDB).sort(DEFAULT_TYPE_SORT, alphaSort),
                     tr("Add &Navaid Range Ring for %1"), tr("Show a ring for the radio navaid range on the map"),
                     tr("Shift+Click"), QIcon(":/littlenavmap/resources/icons/navrange.svg"), false /* allowNoMapObject */, callback);
}

void MapContextMenu::insertPatternMenu(QMenu& menu)
{
  ActionCallback callback =
    [](const map::MapBase *base, QString& text, QIcon&, bool& disable, bool) -> void {
      if(base != nullptr && base->objType == map::AIRPORT)
      {
        const map::MapAirport *airport = base->asPtr<map::MapAirport>();
        if(airport->noRunways())
        {
          text.append(tr(" (no runway)"));
          disable = true;
        }
        else
          disable = false;

        // Do our own text substitution for the airport to use shorter name
        if(text.contains("%1"))
          text = text.arg(map::airportTextShort(*base->asPtr<map::MapAirport>(), TEXT_ELIDE_AIRPORT_NAME));
      }
      else
        // No object or not an airport
        disable = true;
    };

  insertMenuOrAction(menu, mc::PATTERN, MapResultIndex().addRef(*result, map::AIRPORT).sort(alphaSort),
                     tr("Add &Traffic Pattern at %1 ..."), tr("Show a traffic pattern to a runway for this airport"),
                     QString(), QIcon(":/littlenavmap/resources/icons/trafficpattern.svg"), false /* allowNoMapObject */, callback);
}

void MapContextMenu::insertHoldMenu(QMenu& menu)
{
  ActionCallback callback =
    [this](const map::MapBase *base, QString& text, QIcon&, bool& disable, bool) -> void {
      disable = !visibleOnMap;
      if(base == nullptr)
        text = tr("Add &Holding here ...");
    };

  insertMenuOrAction(menu, mc::HOLDING, MapResultIndex().
                     addRef(*result, map::AIRPORT | map::VOR | map::NDB | map::WAYPOINT | map::USERPOINT | map::USERPOINTROUTE).
                     sort(DEFAULT_TYPE_SORT, alphaSort),
                     tr("Add &Holding at %1 ..."), tr("Show a holding pattern on the map at a position or a navaid"),
                     QString(), QIcon(":/littlenavmap/resources/icons/enroutehold.svg"), true /* allowNoMapObject */, callback);
}

void MapContextMenu::insertAirportMsaMenu(QMenu& menu)
{
  ActionCallback callback =
    [this](const map::MapBase *base, QString&, QIcon&, bool& disable, bool) -> void {
      disable = !visibleOnMap || base == nullptr;
    };

  insertMenuOrAction(menu, mc::AIRPORT_MSA, MapResultIndex().addRef(*result, map::AIRPORT_MSA).
                     sort(DEFAULT_TYPE_SORT, alphaSort),
                     tr("Add &MSA Diagram at %1"), tr("Show a MSA sector diagram on the map at an airport or a navaid"),
                     QString(), QIcon(":/littlenavmap/resources/icons/msa.svg"), false /* allowNoMapObject */, callback);
}

void MapContextMenu::insertDepartureMenu(QMenu& menu)
{
  MapResultIndex index;
  index.addRef(*result, map::AIRPORT | map::PARKING | map::HELIPAD).sort(DEFAULT_TYPE_SORT, alphaSort);

  // Erase all helipads without start position
  index.erase(std::remove_if(index.begin(), index.end(), [](const map::MapBase *base) -> bool {
    return base != nullptr && base->getType() == map::HELIPAD &&
    base->asPtr<map::MapHelipad>()->startId == -1;
  }), index.end());

  ActionCallback callback =
    [this](const map::MapBase *base, QString& text, QIcon&, bool& disable, bool) -> void
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

          text = tr("Set %1 at %2 as &Departure").
                 arg(atools::elideTextShortMiddle(map::helipadText(*helipad), TEXT_ELIDE)).
                 arg(atools::elideTextShortMiddle(map::airportText(airport), TEXT_ELIDE));
        }
        else if(base->getType() == map::PARKING)
        {
          // User clicked on parking ================================
          const map::MapParking *parking = base->asPtr<map::MapParking>();

          // Get related airport
          airport = NavApp::getAirportQuerySim()->getAirportById(parking->airportId);

          text = tr("Set %1 at %2 as &Departure").
                 arg(atools::elideTextShortMiddle(map::parkingText(*parking), TEXT_ELIDE)).
                 arg(atools::elideTextShortMiddle(map::airportText(airport), TEXT_ELIDE));
        }
        else if(base->getType() == map::AIRPORT)
          // Clicked on airport
          airport = base->asObj<map::MapAirport>();

        bool departure = false, destination = false, alternate = false;
        proc::procedureFlags(route, &airport, &departure, &destination, &alternate);

        if(departure)
        {
          if(base->getType() != map::HELIPAD && base->getType() != map::PARKING)
            // Is already departure airport and no parking clicked
            text.append(tr(" (is departure)"));
          // else user clicked parking spot
        }
        else if(destination)
          text.append(tr(" (is destination)"));
        else if(alternate)
          text.append(tr(" (is alternate)"));
      }
    };

  insertMenuOrAction(menu, mc::DEPARTURE, index, tr("Set %1 as &Departure"), tr("Set airport as departure in the flight plan"),
                     QString(), QIcon(":/littlenavmap/resources/icons/airportroutedest.svg"), false /* allowNoMapObject */, callback);
}

void MapContextMenu::insertDestinationMenu(QMenu& menu)
{
  ActionCallback callback =
    [this](const map::MapBase *base, QString& text, QIcon&, bool& disable, bool) -> void {
      disable = base == nullptr;
      if(base != nullptr)
        text.append(proc::procedureTextSuffixDepartDest(route, base->asObj<map::MapAirport>(), disable));
    };

  insertMenuOrAction(menu, mc::DESTINATION, MapResultIndex().addRef(*result, map::AIRPORT).sort(alphaSort),
                     tr("Set %1 as &Destination"), tr("Set airport as destination in the flight plan"),
                     QString(), QIcon(":/littlenavmap/resources/icons/airportroutestart.svg"), false /* allowNoMapObject */, callback);
}

void MapContextMenu::insertAlternateMenu(QMenu& menu)
{
  ActionCallback callback =
    [this](const map::MapBase *base, QString& text, QIcon&, bool& disable, bool) -> void {
      disable = base == nullptr;
      if(base != nullptr)
      {
        if(route.getSizeWithoutAlternates() < 1)
        {
          disable = true;
          text.append(tr(" (no destination)"));
        }
        else
          text.append(proc::procedureTextSuffixAlternate(route, base->asObj<map::MapAirport>(), disable));
      }
    };

  insertMenuOrAction(menu, mc::ALTERNATE, MapResultIndex().addRef(*result, map::AIRPORT).sort(alphaSort),
                     tr("&Add %1 as Alternate"), tr("Add airport as alternate to the flight plan"),
                     QString(), QIcon(":/littlenavmap/resources/icons/airportroutealt.svg"), false /* allowNoMapObject */, callback);
}

void MapContextMenu::insertAddRouteMenu(QMenu& menu)
{
  ActionCallback callback =
    [this](const map::MapBase *base, QString& text, QIcon&, bool& disable, bool) -> void {
      if(base == nullptr)
        // Modify text only
        text = tr("Add Position to Flight &Plan");
      disable = !visibleOnMap;
    };

  insertMenuOrAction(menu, mc::ADDROUTE,
                     MapResultIndex().
                     addRef(*result, map::AIRPORT | map::VOR | map::NDB | map::WAYPOINT | map::USERPOINT).
                     sort(DEFAULT_TYPE_SORT, alphaSort),
                     tr("Add %1 to Flight &Plan"), tr("Add airport, navaid or position to nearest flight plan leg"),
                     tr("Ctrl+Alt+Click"), QIcon(":/littlenavmap/resources/icons/routeadd.svg"), true /* allowNoMapObject */, callback);
}

void MapContextMenu::insertAppendRouteMenu(QMenu& menu)
{
  ActionCallback callback =
    [this](const map::MapBase *base, QString& text, QIcon&, bool& disable, bool) -> void {
      if(base == nullptr)
        // Modify text only
        text = tr("Append Position to &Flight Plan");
      disable = !visibleOnMap;
    };

  insertMenuOrAction(menu, mc::APPENDROUTE,
                     MapResultIndex().
                     addRef(*result, map::AIRPORT | map::VOR | map::NDB | map::WAYPOINT | map::USERPOINT).
                     sort(DEFAULT_TYPE_SORT, alphaSort),
                     tr("Append %1 to &Flight Plan"), tr("Append airport, navaid or position to the end of the flight plan"),
                     tr("Shift+Alt+Click"), QIcon(":/littlenavmap/resources/icons/routeadd.svg"), true /* allowNoMapObject */, callback);
}

void MapContextMenu::insertDeleteRouteWaypointMenu(QMenu& menu)
{
  // Create index ============================
  MapResultIndex index;
  index.addRef(*result, map::AIRPORT | map::VOR | map::NDB | map::WAYPOINT | map::USERPOINTROUTE | map::PROCEDURE_POINT);

  // Erase all points which are not route legs ============================
  index.erase(std::remove_if(index.begin(), index.end(), [this](const map::MapBase *base) -> bool {
    return map::routeIndex(base) == -1 || (base->objType != map::PROCEDURE_POINT && isProcedure(base));
  }), index.end());

  // Erase duplicate occasions of procedures which can appear in double used waypoints
  index.erase(std::unique(index.begin(), index.end(), [](const map::MapBase *base1, const map::MapBase *base2) -> bool {
    const map::MapProcedurePoint *procPt1 = base1->asPtr<map::MapProcedurePoint>();
    if(procPt1 != nullptr)
    {
      const map::MapProcedurePoint *procPt2 = base2->asPtr<map::MapProcedurePoint>();
      if(procPt2 != nullptr)
        return procPt1->compoundId() == procPt2->compoundId();
    }
    return false;
  }), index.end());

  index.sort(DEFAULT_TYPE_SORT, alphaSort);

  ActionCallback callback =
    [this](const map::MapBase *base, QString& text, QIcon& icon, bool& disable, bool) -> void {
      disable = true;
      if(base != nullptr)
      {
        if(base->objType == map::PROCEDURE_POINT)
        {
          const map::MapProcedurePoint *procPt = base->asPtr<map::MapProcedurePoint>();
          if(procPt != nullptr)
          {
            QString procName = route.getProcedureLegText(procPt->getLeg().mapType,
                                                         false /* includeRunway */, true /* missedAsApproach */,
                                                         false /* transitionAsProcedure */);
            text = tr("&Delete %1 from Flight Plan").arg(procName);
            icon = QIcon(":/littlenavmap/resources/icons/approach.svg");
            disable = false;
          }
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
      }
    };

  insertMenuOrAction(menu, mc::DELETEROUTEWAYPOINT, index,
                     tr("&Delete %1 from Flight Plan"), tr("Delete airport, navaid or position from the flight plan"),
                     tr("Ctrl+Alt+Click"), QIcon(":/littlenavmap/resources/icons/routedeleteleg.svg"), false /* allowNoMapObject */,
                     callback);
}

void MapContextMenu::insertEditRouteUserpointMenu(QMenu& menu)
{
  MapResultIndex index;
  index.addRef(*result, map::AIRPORT | map::VOR | map::NDB | map::WAYPOINT | map::USERPOINTROUTE).
  sort(DEFAULT_TYPE_SORT, alphaSort);

  // Erase all points which are not route legs
  index.erase(std::remove_if(index.begin(), index.end(), [](const map::MapBase *base) -> bool
  {
    return map::routeIndex(base) == -1;
  }), index.end());

  ActionCallback callback =
    [this](const map::MapBase *base, QString& text, QIcon&, bool& disable, bool) -> void {
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
                     tr("Edit Flight Plan &Position %1 ..."), tr("Edit remark, name or coordinate of flight plan position"),
                     QString(), QIcon(":/littlenavmap/resources/icons/routestring.svg"), false /* allowNoMapObject */, callback);
}

void MapContextMenu::insertMarkAddonAirportMenu(QMenu& menu)
{
  ActionCallback callback =
    [this](const map::MapBase *base, QString&, QIcon&, bool& disable, bool) -> void {
      disable = !visibleOnMap || base == nullptr;
    };

  insertMenuOrAction(menu, mc::MARKAIRPORTADDON,
                     MapResultIndex().
                     addRef(*result, map::AIRPORT).
                     sort(DEFAULT_TYPE_SORT, alphaSort),
                     tr("&Mark %1 as Add-on"), tr("Create a userpoint highlighting the airport as add-on"),
                     QString(), QIcon(":/littlenavmap/resources/icons/airportaddon.svg"), false /* allowNoMapObject */,
                     callback);
}

void MapContextMenu::insertUserpointAddMenu(QMenu& menu)
{
  ActionCallback callback =
    [this](const map::MapBase *base, QString& text, QIcon&, bool& disable, bool) -> void {
      if(base == nullptr)
        // Modify text only
        text = tr("Add &Userpoint here ...");
      disable = !visibleOnMap;
    };

  insertMenuOrAction(menu, mc::USERPOINTADD,
                     MapResultIndex().
                     addRef(*result, map::AIRPORT | map::VOR | map::NDB | map::WAYPOINT | map::USERPOINT).
                     sort(DEFAULT_TYPE_SORT, alphaSort),
                     tr("Add &Userpoint %1 ..."), tr("Add an userpoint at this position"),
                     tr("Ctrl+Shift+Click"), QIcon(":/littlenavmap/resources/icons/userdata_add.svg"), true /* allowNoMapObject */,
                     callback);
}

void MapContextMenu::insertRemoveMarkMenu(QMenu& menu)
{
  ActionCallback callback =
    [this](const map::MapBase *base, QString& text, QIcon& icon, bool& disable, bool submenu) -> void {
      bool shown = base != nullptr && NavApp::getMapMarkHandler()->isShown(base->objType);
      disable = !visibleOnMap || !shown || base == nullptr;

      if(disable)
        text = tr("&Remove User Feature");
      else if(!submenu)
        text = tr("&Remove %1");

      if(base != nullptr)
      {
        if(base->objType == map::MARK_RANGE)
          icon = QIcon(":/littlenavmap/resources/icons/rangeringoff.svg");
        else if(base->objType == map::MARK_DISTANCE)
          icon = QIcon(":/littlenavmap/resources/icons/distancemeasureoff.svg");
        else if(base->objType == map::MARK_HOLDING)
          icon = QIcon(":/littlenavmap/resources/icons/holdoff.svg");
        else if(base->objType == map::MARK_PATTERNS)
          icon = QIcon(":/littlenavmap/resources/icons/trafficpatternoff.svg");
        else if(base->objType == map::MARK_MSA)
          icon = QIcon(":/littlenavmap/resources/icons/msaoff.svg");
      }
    };

  insertMenuOrAction(menu, mc::REMOVEUSER, MapResultIndex().addRef(*result, map::MARK_ALL).sort(DEFAULT_TYPE_SORT, alphaSort),
                     tr("&Remove User Feature %1"), tr("Remove User Feature"),
                     QString(), QIcon(":/littlenavmap/resources/icons/marksoff.svg"), false /* allowNoMapObject */,
                     callback);
}

void MapContextMenu::insertUserpointEditMenu(QMenu& menu)
{
  insertMenuOrAction(menu, mc::USERPOINTEDIT, MapResultIndex().addRef(*result, map::USERPOINT).sort(alphaSort),
                     tr("&Edit Userpoint %1 ..."), tr("Edit the userpoint at this position"),
                     tr("Ctrl+Shift+Click"), QIcon(":/littlenavmap/resources/icons/userdata_edit.svg"));
}

void MapContextMenu::insertUserpointMoveMenu(QMenu& menu)
{
  insertMenuOrAction(menu, mc::USERPOINTMOVE, MapResultIndex().addRef(*result, map::USERPOINT).sort(alphaSort),
                     tr("&Move Userpoint %1"), tr("Move the userpoint to a new position on the map"),
                     QString(), QIcon(":/littlenavmap/resources/icons/userdata_move.svg"));
}

void MapContextMenu::insertUserpointDeleteMenu(QMenu& menu)
{
  insertMenuOrAction(menu, mc::USERPOINTDELETE, MapResultIndex().addRef(*result, map::USERPOINT).sort(alphaSort),
                     tr("&Delete Userpoint %1"), tr("Remove the userpoint at this position"),
                     QString(), QIcon(":/littlenavmap/resources/icons/userdata_delete.svg"));
}

void MapContextMenu::insertLogEntryEdit(QMenu& menu)
{
  insertMenuOrAction(menu, mc::LOGENTRYEDIT, MapResultIndex().addRef(*result, map::LOGBOOK).sort(alphaSort),
                     tr("Edit &Log Entry %1 ..."), tr("Edit the logbook entry at this position"),
                     QString(), QIcon(":/littlenavmap/resources/icons/logdata_edit.svg"));
}

void MapContextMenu::insertShowInRouteMenu(QMenu& menu)
{
  MapResultIndex index;
  index.addRef(*result, map::AIRPORT | map::VOR | map::NDB | map::WAYPOINT | map::USERPOINTROUTE).
  sort(DEFAULT_TYPE_SORT, alphaSort);

  // Erase all waypoints which are not part of the flight plan
  index.erase(std::remove_if(index.begin(), index.end(), [](const map::MapBase *base) -> bool {
    return map::routeIndex(base) == -1;
  }), index.end());

  insertMenuOrAction(menu, mc::SHOWINROUTE, index,
                     tr("Select Leg %1 in &Flight Plan"), tr("Select the related flight plan leg for a navaid or airport"),
                     QString(), QIcon(":/littlenavmap/resources/icons/routeselect.svg"), false);

}

void MapContextMenu::insertShowInSearchMenu(QMenu& menu)
{
  ActionCallback callback =
    [this](const map::MapBase *base, QString& text, QIcon&, bool& disable, bool) -> void {
      disable = !visibleOnMap || base == nullptr;

#ifdef DEBUG_INFORMATION
      if(base != nullptr)
        qDebug() << Q_FUNC_INFO << map::mapTypeToString(base->getType());
#endif

      if(base != nullptr && base->objType == map::AIRCRAFT)
      {
        // Add shadowed online aircraft for user
        const map::MapUserAircraft *userAircraft = base->asPtr<map::MapUserAircraft>();
        if(userAircraft != nullptr)
        {
          if(userAircraft->getAircraft().isOnlineShadow())
          {
            atools::fs::sc::SimConnectAircraft shadowedOnlineAircraft =
              NavApp::getOnlinedataController()->getShadowedOnlineAircraft(userAircraft->getAircraft());

            if(shadowedOnlineAircraft.isValid())
              text = tr("&Show %1 in Search").arg(map::aircraftTextShort(shadowedOnlineAircraft));
            else
              disable = true;
          }
          else
            disable = true;

          if(disable)
            text = tr("&Show in Search");
        }
      }
    };

  MapResultIndex index;
  index.addRef(*result, map::AIRPORT | map::VOR | map::NDB | map::WAYPOINT | map::USERPOINT | map::AIRSPACE |
               map::AIRCRAFT | map::AIRCRAFT_ONLINE | map::LOGBOOK).sort(DEFAULT_TYPE_SORT, alphaSort);

  // Erase all non-online airspaces and aircraft which are not online client shadows
  index.erase(std::remove_if(index.begin(), index.end(), [](const map::MapBase *base) -> bool {
    if(base->getType() == map::AIRSPACE)
      return !base->asPtr<map::MapAirspace>()->src.testFlag(map::AIRSPACE_SRC_ONLINE);

    return false;
  }), index.end());

  insertMenuOrAction(menu, mc::SHOWINSEARCH, index,
                     tr("&Show %1 in Search"), tr("Show the airport, navaid, userpoint or other object in the search window"),
                     QString(), QIcon(":/littlenavmap/resources/icons/search.svg"), false, callback);
}

bool MapContextMenu::exec(QPoint menuPos, QPoint point)
{
  clear();

  const MapScreenIndex *screenIndex = mapWidget->getScreenIndex();
  int screenSearchDist = OptionData::instance().getMapClickSensitivity();

  // ===================================================================================
  // Build menu - add actions

  if(!point.isNull() && !mapWidget->noRender())
  {
    qreal lon, lat;
    // Cursor can be outside of map region
    visibleOnMap = mapWidget->geoCoordinates(point.x(), point.y(), lon, lat);

    if(visibleOnMap)
      // Cursor is not off-globe
      mapBasePos->position = atools::geo::Pos(lon, lat);
  }

  // Get objects near position =============================================================
  map::MapObjectQueryTypes queryType = map::QUERY_MARK | map::QUERY_PREVIEW_PROC_POINTS | map::QUERY_PROC_RECOMMENDED;

  // Fetch alternates only if enabled on map
  if(mapWidget->getShownMapDisplayTypes().testFlag(map::FLIGHTPLAN_ALTERNATE))
    queryType |= map::QUERY_ALTERNATE;

  screenIndex->getAllNearest(point, screenSearchDist, *result, queryType);

  result->moveOnlineAirspacesToFront();

  // Disable all general menu items that depend on position ===========================
  ui->actionMapSetMark->setEnabled(visibleOnMap);
  ui->actionMapSetHome->setEnabled(visibleOnMap);

  // Copy coordinates ===================
  ui->actionMapCopyCoordinates->setEnabled(visibleOnMap);
  if(visibleOnMap)
    ui->actionMapCopyCoordinates->setText(ui->actionMapCopyCoordinates->text().arg(Unit::coords(mapBasePos->position)));

  // Enable or disable map marks ===========================
  ui->actionMapRangeRings->setEnabled(visibleOnMap);

  // Build the menu =============================================================
  // The result must not be modified after building the menu because objects are referenced via dataIndex by pointers
  buildMainMenu();

  // Show the menu ------------------------------------------------
  selectedAction = mapMenu.exec(menuPos);

  if(selectedAction != nullptr)
  {
    // A menu items was clicked ===============================
    qDebug() << Q_FUNC_INFO << "selectedAction text" << selectedAction->text() << "name" << selectedAction->objectName();

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

bool MapContextMenu::canEditRouteComment(const map::MapBase *base) const
{
  if(base != nullptr)
  {
    int routeIndex = map::routeIndex(base);
    return routeIndex != -1 && route.canEditComment(routeIndex);
  }
  return false;
}

bool MapContextMenu::canEditRoutePoint(const map::MapBase *base) const
{
  if(base != nullptr)
  {
    int routeIndex = map::routeIndex(base);
    return routeIndex != -1 && route.canEditPoint(routeIndex);
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
      const RouteLeg& leg = route.value(routeIndex);
      if(leg.isAnyProcedure())
        return route.getProcedureLegText(leg.getProcedureType(),
                                         false /* includeRunway */, true /* missedAsApproach */, false /* transitionAsProcedure */);
    }
  }
  return QString();
}

bool MapContextMenu::isProcedure(const map::MapBase *base) const
{
  if(base != nullptr)
  {
    int routeIndex = map::routeIndex(base);
    if(routeIndex != -1)
      return route.value(routeIndex).isAnyProcedure();
  }
  return false;
}
