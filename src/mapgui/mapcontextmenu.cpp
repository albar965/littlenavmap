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
#include "atools.h"

#include <ui_mainwindow.h>

MapContextMenu::MapContextMenu(MapWidget *mapWidgetParam)
  : QObject(mapWidgetParam), mapWidget(mapWidgetParam)
{
  result = new map::MapSearchResult;

  Ui::MainWindow *ui = NavApp::getMainUi();
  QList<QAction *> actions(
  {
    ui->actionMapEditUserWaypoint,
    ui->actionMapHold,
    ui->actionMapLogdataEdit,
    ui->actionMapMeasureDistance,
    ui->actionMapNavaidRange,
    ui->actionMapRangeRings,
    ui->actionMapShowApproaches,
    ui->actionMapShowApproachesCustom,
    ui->actionMapShowInformation,
    ui->actionMapTrafficPattern,
    ui->actionMapUserdataAdd,
    ui->actionMapUserdataDelete,
    ui->actionMapUserdataEdit,
    ui->actionMapUserdataMove,
    ui->actionRouteAddPos,
    ui->actionRouteAirportAlternate,
    ui->actionRouteAirportDest,
    ui->actionRouteAirportStart,
    ui->actionRouteAppendPos,
    ui->actionRouteDeleteWaypoint,
    ui->actionShowInSearch
  });

  // Texts with % will be replaced save them and let the ActionTextSaver restore them on return
  textSaver = new atools::gui::ActionTextSaver(actions);

  // Re-enable actions on exit to allow keystrokes
  stateSaver = new atools::gui::ActionStateSaver(actions);
}

MapContextMenu::~MapContextMenu()
{
  delete result;
  delete textSaver;
  delete stateSaver;
}

void MapContextMenu::buildMenu(QMenu& menu)
{
  Ui::MainWindow *ui = NavApp::getMainUi();

  menu.addAction(ui->actionMapShowInformation);
  menu.addAction(ui->actionMapShowApproaches);
  menu.addAction(ui->actionMapShowApproachesCustom);
  menu.addSeparator();

  menu.addAction(ui->actionMapMeasureDistance);
  menu.addAction(ui->actionMapHideDistanceMarker);
  menu.addSeparator();

  menu.addAction(ui->actionMapRangeRings);
  menu.addAction(ui->actionMapNavaidRange);
  menu.addAction(ui->actionMapHideOneRangeRing);
  menu.addSeparator();

  menu.addAction(ui->actionMapTrafficPattern);
  menu.addAction(ui->actionMapHideTrafficPattern);
  menu.addAction(ui->actionMapHold);
  menu.addAction(ui->actionMapHideHold);
  menu.addSeparator();

  menu.addAction(ui->actionRouteAirportStart);
  menu.addAction(ui->actionRouteAirportDest);
  menu.addAction(ui->actionRouteAirportAlternate);
  menu.addSeparator();

  menu.addAction(ui->actionRouteAddPos);
  menu.addAction(ui->actionRouteAppendPos);
  menu.addAction(ui->actionRouteDeleteWaypoint);
  menu.addAction(ui->actionMapEditUserWaypoint);
  menu.addSeparator();

  QMenu *sub = menu.addMenu( /*QIcon(":/littlenavmap/resources/icons/about.svg"), */ tr("&Userpoints"));
  sub->addAction(ui->actionMapUserdataAdd);
  sub->addAction(ui->actionMapUserdataEdit);
  sub->addAction(ui->actionMapUserdataMove);
  sub->addAction(ui->actionMapUserdataDelete);
  menu.addSeparator();

  menu.addAction(ui->actionMapLogdataEdit);
  menu.addSeparator();

  menu.addAction(ui->actionShowInSearch);
  menu.addSeparator();

  menu.addAction(ui->actionMapSetMark);
  menu.addAction(ui->actionMapSetHome);

  if(NavApp::isFullScreen())
  {
    menu.addSeparator();
    menu.addAction(ui->actionShowFullscreenMap); // connected otherwise
  }
}

bool MapContextMenu::exec(QPoint menuPos, QPoint point)
{
  selectedAction = nullptr;

  Ui::MainWindow *ui = NavApp::getMainUi();
  const MapScreenIndex *screenIndex = mapWidget->getScreenIndex();
  int screenSearchDistance = OptionData::instance().getMapClickSensitivity();
  MapMarkHandler *mapMarkHandler = NavApp::getMapMarkHandler();

  // ===================================================================================
  // Build menu - add actions

  QMenu menu;
  buildMenu(menu);

  if(!point.isNull())
  {
    qreal lon, lat;
    // Cursor can be outside of map region
    visibleOnMap = mapWidget->geoCoordinates(point.x(), point.y(), lon, lat);

    if(visibleOnMap)
    {
      pos = atools::geo::Pos(lon, lat);
      distMarkerIndex = screenIndex->getNearestDistanceMarkIndex(point.x(), point.y(), screenSearchDistance);
      rangeMarkerIndex = screenIndex->getNearestRangeMarkIndex(point.x(), point.y(), screenSearchDistance);
      trafficPatternIndex = screenIndex->getNearestTrafficPatternIndex(point.x(),
                                                                       point.y(), screenSearchDistance);
      holdIndex = screenIndex->getNearestHoldIndex(point.x(), point.y(), screenSearchDistance);
    }
  }

  // Disable all menu items that depend on position
  ui->actionMapSetMark->setEnabled(visibleOnMap);
  ui->actionMapSetHome->setEnabled(visibleOnMap);
  ui->actionMapMeasureDistance->setEnabled(visibleOnMap && mapMarkHandler->isShown(map::MARK_MEASUREMENT));
  ui->actionMapRangeRings->setEnabled(visibleOnMap && mapMarkHandler->isShown(map::MARK_RANGE_RINGS));

  ui->actionMapUserdataAdd->setEnabled(visibleOnMap);
  ui->actionMapUserdataEdit->setEnabled(false);
  ui->actionMapUserdataDelete->setEnabled(false);
  ui->actionMapUserdataMove->setEnabled(false);

  ui->actionMapLogdataEdit->setEnabled(false);

  ui->actionMapHideOneRangeRing->setEnabled(visibleOnMap && rangeMarkerIndex != -1);
  ui->actionMapHideDistanceMarker->setEnabled(visibleOnMap && distMarkerIndex != -1);
  ui->actionMapHideTrafficPattern->setEnabled(visibleOnMap && trafficPatternIndex != -1);
  ui->actionMapHideHold->setEnabled(visibleOnMap && holdIndex != -1);

  ui->actionMapShowInformation->setEnabled(false);
  ui->actionMapShowApproaches->setEnabled(false);
  ui->actionMapShowApproachesCustom->setEnabled(false);
  ui->actionMapTrafficPattern->setEnabled(false);
  ui->actionMapHold->setEnabled(false);
  ui->actionMapNavaidRange->setEnabled(false);
  ui->actionShowInSearch->setEnabled(false);
  ui->actionRouteAddPos->setEnabled(visibleOnMap);
  ui->actionRouteAppendPos->setEnabled(visibleOnMap);
  ui->actionRouteAirportStart->setEnabled(false);
  ui->actionRouteAirportDest->setEnabled(false);
  ui->actionRouteAirportAlternate->setEnabled(false);
  ui->actionRouteDeleteWaypoint->setEnabled(false);
  ui->actionMapEditUserWaypoint->setEnabled(false);

  // Get objects near position =============================================================
  screenIndex->getAllNearest(point.x(), point.y(), screenSearchDistance, *result, map::QUERY_NONE);
  result->moveOnlineAirspacesToFront();

  // ===================================================================================
  // Get only one object of each type
  if(result->userAircraft.isValid())
    userAircraft = &result->userAircraft;

  aiAircraft = atools::firstOrNull(result->aiAircraft);
  onlineAircraft = atools::firstOrNull(result->onlineAircraft);

  // Add shadow for "show in search"
  atools::fs::sc::SimConnectAircraft shadowAircraft;
  if(userAircraft != nullptr && NavApp::getOnlinedataController()->getShadowAircraft(shadowAircraft, *userAircraft))
    onlineAircraft = &shadowAircraft;

  if(!result->helipads.isEmpty() && result->helipads.first().startId != -1)
    // Only helipads with start position are allowed
    helipad = &result->helipads.first();

  airport = atools::firstOrNull(result->airports);
  parking = atools::firstOrNull(result->parkings);
  vor = atools::firstOrNull(result->vors);
  ndb = atools::firstOrNull(result->ndbs);
  waypoint = atools::firstOrNull(result->waypoints);
  ils = atools::firstOrNull(result->ils);
  userpointRoute = atools::firstOrNull(result->userpointsRoute);
  userpoint = atools::firstOrNull(result->userpoints);
  logEntry = atools::firstOrNull(result->logbookEntries);
  airway = atools::firstOrNull(result->airways);
  airspace = result->firstSimNavUserAirspace();
  onlineCenter = result->firstOnlineAirspace();

  airportDestination = false;
  airportDeparture = false;
  if(airport != nullptr)
  {
    airportDestination = NavApp::getRouteConst().isAirportDestination(airport->ident);
    airportDeparture = NavApp::getRouteConst().isAirportDeparture(airport->ident);
  }

  // ===================================================================================
  // Collect information from the search result - build text only for one object for several menu items
  // Order is important - first items have lowest priority
  QString informationText, procedureText, measureText, rangeRingText, departureText, departureParkingText,
          destinationText, addRouteText, searchText, editUserpointText, patternText, holdText, userpointAddText;

  if(onlineCenter != nullptr)
  {
    informationText = result->numOnlineAirspaces() > 1 ? tr("Online Centers") : tr("Online Center");
    searchText = tr("Online Center %1").arg(onlineCenter->name);
  }
  else if(airspace != nullptr)
    informationText = result->numSimNavUserAirspaces() > 1 ? tr("Airspaces") : tr("Airspace");

  if(ils != nullptr)
    informationText = map::ilsTextShort(*ils);

  // Fill texts in reverse order of priority
  if(airway != nullptr)
    informationText = map::airwayText(*airway);

  if(userpointRoute != nullptr)
    // No show information on user point
    informationText.clear();

  if(waypoint != nullptr)
    userpointAddText = informationText = measureText = addRouteText = searchText = holdText =
      map::waypointText(*waypoint);

  if(ndb != nullptr)
    userpointAddText = informationText = measureText = rangeRingText = addRouteText = searchText = holdText =
      map::ndbText(*ndb);

  if(vor != nullptr)
    userpointAddText = informationText = measureText = rangeRingText = addRouteText = searchText = holdText =
      map::vorText(*vor);

  if(airport != nullptr)
    userpointAddText = procedureText = informationText =
      measureText = departureText =
        destinationText = addRouteText =
          searchText = patternText = holdText = map::airportText(*airport, 20);

  // Userpoints are drawn on top of all features
  if(userpoint != nullptr)
    editUserpointText = informationText = addRouteText = searchText = map::userpointText(*userpoint);

  if(logEntry != nullptr)
    informationText = searchText = map::logEntryText(*logEntry);

  // Override airport if part of route and visible
  if((airportDeparture || airportDestination) && airport != nullptr &&
     mapWidget->getShownMapFeaturesDisplay().testFlag(map::FLIGHTPLAN))
    informationText = addRouteText = map::airportText(*airport, 20);

  int departureParkingAirportId = -1;
  // Parking or helipad only if no airport at cursor
  if(airport == nullptr)
  {
    if(helipad != nullptr)
    {
      departureParkingAirportId = helipad->airportId;
      departureParkingText = tr("Helipad %1").arg(helipad->runwayName);
    }

    if(parking != nullptr)
    {
      departureParkingAirportId = parking->airportId;
      if(parking->number == -1)
        departureParkingText = map::parkingName(parking->name);
      else
        departureParkingText = map::parkingName(parking->name) + " " + QLocale().toString(parking->number);
    }
  }

  if(departureParkingAirportId != -1)
  {
    // Clear texts which are not valid for parking positions
    informationText.clear();
    procedureText.clear();
    measureText.clear();
    rangeRingText.clear();
    destinationText.clear();
    addRouteText.clear();
    searchText.clear();
  }

  if(aiAircraft != nullptr)
  {
    QStringList info;
    if(!aiAircraft->getAirplaneRegistration().isEmpty())
      info.append(aiAircraft->getAirplaneRegistration());
    if(!aiAircraft->getAirplaneModel().isEmpty())
      info.append(aiAircraft->getAirplaneModel());

    if(info.isEmpty())
      // X-Plane does not give any useful information at all
      info.append(tr("AI / Multiplayer") + tr(" %1").arg(aiAircraft->getObjectId() + 1));

    informationText = info.join(tr(" / "));
    aircraft = true;
  }

  if(onlineAircraft != nullptr)
  {
    searchText = informationText = tr("Online Client Aircraft %1").arg(onlineAircraft->getAirplaneRegistration());
    aircraft = true;
  }

  if(userAircraft != nullptr)
  {
    informationText = tr("User Aircraft");
    aircraft = true;
  }

  // ===================================================================================
  // Build "delete from flight plan" text
  map::MapObjectTypes deleteType = map::NONE;
  QString routeText;
  if(airport != nullptr && airport->routeIndex != -1)
  {
    routeText = map::airportText(*airport, 20);
    routeIndex = airport->routeIndex;
    deleteType = map::AIRPORT;
  }
  else if(vor != nullptr && vor->routeIndex != -1)
  {
    routeText = map::vorText(*vor);
    routeIndex = vor->routeIndex;
    deleteType = map::VOR;
  }
  else if(ndb != nullptr && ndb->routeIndex != -1)
  {
    routeText = map::ndbText(*ndb);
    routeIndex = ndb->routeIndex;
    deleteType = map::NDB;
  }
  else if(waypoint != nullptr && waypoint->routeIndex != -1)
  {
    routeText = map::waypointText(*waypoint);
    routeIndex = waypoint->routeIndex;
    deleteType = map::WAYPOINT;
  }
  else if(userpointRoute != nullptr && userpointRoute->routeIndex != -1)
  {
    routeText = map::userpointRouteText(*userpointRoute);
    routeIndex = userpointRoute->routeIndex;
    deleteType = map::USERPOINTROUTE;
  }

  // ===================================================================================
  // Update "set airport as start/dest"
  if(airport != nullptr || departureParkingAirportId != -1)
  {
    QString airportText(departureText);

    if(departureParkingAirportId != -1)
    {
      // Get airport for parking
      map::MapAirport parkAp;
      NavApp::getAirportQuerySim()->getAirportById(parkAp, departureParkingAirportId);
      airportText = map::airportText(parkAp, 20) + " / ";
    }

    ui->actionRouteAirportStart->setEnabled(true);
    ui->actionRouteAirportStart->setText(ui->actionRouteAirportStart->text().arg(airportText + departureParkingText));

    if(airport != nullptr)
    {
      ui->actionRouteAirportDest->setEnabled(true);
      ui->actionRouteAirportDest->setText(ui->actionRouteAirportDest->text().arg(destinationText));
    }
    else
      ui->actionRouteAirportDest->setText(ui->actionRouteAirportDest->text().arg(QString()));

    if(airport != nullptr && NavApp::getRouteConst().getSizeWithoutAlternates() > 0)
    {
      ui->actionRouteAirportAlternate->setEnabled(true);
      ui->actionRouteAirportAlternate->setText(ui->actionRouteAirportAlternate->text().arg(destinationText));
    }
    else
      ui->actionRouteAirportAlternate->setText(ui->actionRouteAirportAlternate->text().arg(QString()));
  }
  else
  {
    // No airport or selected parking position
    ui->actionRouteAirportStart->setText(ui->actionRouteAirportStart->text().arg(QString()));
    ui->actionRouteAirportDest->setText(ui->actionRouteAirportDest->text().arg(QString()));
    ui->actionRouteAirportAlternate->setText(ui->actionRouteAirportAlternate->text().arg(QString()));
  }

  // ===================================================================================
  // Update "show information" for airports, navaids, airways and airspaces
  if(vor != nullptr || ndb != nullptr || waypoint != nullptr || ils != nullptr || airport != nullptr ||
     airway != nullptr || airspace != nullptr || onlineCenter != nullptr || userpoint != nullptr || logEntry != nullptr)
  {
    ui->actionMapShowInformation->setEnabled(true);
    ui->actionMapShowInformation->setText(ui->actionMapShowInformation->text().arg(informationText));
  }
  else
  {
    if(aircraft)
    {
      ui->actionMapShowInformation->setEnabled(true);
      ui->actionMapShowInformation->setText(ui->actionMapShowInformation->text().arg(informationText));
    }
    else
      ui->actionMapShowInformation->setText(ui->actionMapShowInformation->text().arg(QString()));
  }

  // ===================================================================================
  // Update "edit userpoint" and "add userpoint"
  if(vor != nullptr || ndb != nullptr || waypoint != nullptr || airport != nullptr)
    ui->actionMapUserdataAdd->setText(ui->actionMapUserdataAdd->text().arg(userpointAddText));
  else
    ui->actionMapUserdataAdd->setText(ui->actionMapUserdataAdd->text().arg(QString()));

  if(userpoint != nullptr)
  {
    ui->actionMapUserdataEdit->setEnabled(true);
    ui->actionMapUserdataEdit->setText(ui->actionMapUserdataEdit->text().arg(editUserpointText));
    ui->actionMapUserdataDelete->setEnabled(true);
    ui->actionMapUserdataDelete->setText(ui->actionMapUserdataDelete->text().arg(editUserpointText));
    ui->actionMapUserdataMove->setEnabled(true);
    ui->actionMapUserdataMove->setText(ui->actionMapUserdataMove->text().arg(editUserpointText));
  }
  else
  {
    ui->actionMapUserdataEdit->setText(ui->actionMapUserdataEdit->text().arg(QString()));
    ui->actionMapUserdataDelete->setText(ui->actionMapUserdataDelete->text().arg(QString()));
    ui->actionMapUserdataMove->setText(ui->actionMapUserdataMove->text().arg(QString()));
  }

  if(logEntry != nullptr)
  {
    ui->actionMapLogdataEdit->setEnabled(true);
    ui->actionMapLogdataEdit->setText(ui->actionMapLogdataEdit->text().arg(map::logEntryText(*logEntry)));
  }
  else
    ui->actionMapLogdataEdit->setText(ui->actionMapLogdataEdit->text().arg(QString()));

  // ===================================================================================
  // Update "show in search" and "add to route" only for airports an navaids
  if(vor != nullptr || ndb != nullptr || waypoint != nullptr || airport != nullptr ||
     userpoint != nullptr)
  {
    ui->actionRouteAddPos->setEnabled(true);
    ui->actionRouteAddPos->setText(ui->actionRouteAddPos->text().arg(addRouteText));
    ui->actionRouteAppendPos->setEnabled(true);
    ui->actionRouteAppendPos->setText(ui->actionRouteAppendPos->text().arg(addRouteText));
  }
  else
  {
    ui->actionRouteAddPos->setText(ui->actionRouteAddPos->text().arg(tr("Position")));
    ui->actionRouteAppendPos->setText(ui->actionRouteAppendPos->text().arg(tr("Position")));
  }

  if(vor != nullptr || ndb != nullptr || waypoint != nullptr || airport != nullptr ||
     userpoint != nullptr || logEntry != nullptr || onlineAircraft != nullptr || onlineCenter != nullptr)
  {
    ui->actionShowInSearch->setEnabled(true);
    ui->actionShowInSearch->setText(ui->actionShowInSearch->text().arg(searchText));
  }
  else
    ui->actionShowInSearch->setText(ui->actionShowInSearch->text().arg(QString()));

  if(airport != nullptr)
  {
    bool hasAnyArrival = NavApp::getMapQuery()->hasAnyArrivalProcedures(*airport);
    bool hasDeparture = NavApp::getMapQuery()->hasDepartureProcedures(*airport);

    if(hasAnyArrival || hasDeparture)
    {
      if(airportDeparture)
      {
        if(hasDeparture)
        {
          ui->actionMapShowApproaches->setEnabled(true);
          ui->actionMapShowApproaches->setText(ui->actionMapShowApproaches->text().arg(tr("Departure ")).
                                               arg(procedureText));
        }
        else
          ui->actionMapShowApproaches->setText(tr("Show procedures (%1 has no departure procedure)").arg(airport->ident));
      }
      else if(airportDestination)
      {
        if(hasAnyArrival)
        {
          ui->actionMapShowApproaches->setEnabled(true);
          ui->actionMapShowApproaches->setText(ui->actionMapShowApproaches->text().arg(tr("Arrival ")).
                                               arg(procedureText));
        }
        else
          ui->actionMapShowApproaches->setText(tr("Show procedures (%1 has no arrival procedure)").arg(airport->ident));
      }
      else
      {
        ui->actionMapShowApproaches->setEnabled(true);
        ui->actionMapShowApproaches->setText(ui->actionMapShowApproaches->text().arg(tr("all ")).arg(procedureText));
      }
    }
    else
      ui->actionMapShowApproaches->setText(tr("Show procedures (%1 has no procedure)").arg(airport->ident));

    ui->actionMapShowApproachesCustom->setEnabled(true);
    if(airportDestination)
      ui->actionMapShowApproachesCustom->setText(tr("Create &Approach to %1 and insert into Flight Plan").
                                                 arg(procedureText));
    else
      ui->actionMapShowApproachesCustom->setText(tr("Create &Approach and use %1 as Destination").
                                                 arg(procedureText));
  }
  else
  {
    ui->actionMapShowApproaches->setText(ui->actionMapShowApproaches->text().arg(QString()).arg(QString()));
    ui->actionMapShowApproachesCustom->setText(ui->actionMapShowApproachesCustom->text().arg(QString()));
  }

  // Traffic pattern ========================================
  if(airport != nullptr && !airport->noRunways() && NavApp::getMapMarkHandler()->isShown(map::MARK_PATTERNS))
  {
    ui->actionMapTrafficPattern->setEnabled(true);
    ui->actionMapTrafficPattern->setText(ui->actionMapTrafficPattern->text().arg(patternText));
  }
  else
    ui->actionMapTrafficPattern->setText(ui->actionMapTrafficPattern->text().arg(QString()));

  // Hold ========================================
  if(visibleOnMap && NavApp::getMapMarkHandler()->isShown(map::MARK_HOLDS))
  {
    ui->actionMapHold->setEnabled(true);
    ui->actionMapHold->setText(ui->actionMapHold->text().arg(holdText.isEmpty() ? tr("Position") : holdText));
  }
  else
    ui->actionMapHold->setText(ui->actionMapHold->text().arg(QString()));

  // Update "delete in route"
  if(routeIndex != -1 && NavApp::getRouteConst().canEditPoint(routeIndex))
  {
    ui->actionRouteDeleteWaypoint->setEnabled(true);
    ui->actionRouteDeleteWaypoint->setText(ui->actionRouteDeleteWaypoint->text().arg(routeText));
  }
  else
    ui->actionRouteDeleteWaypoint->setText(ui->actionRouteDeleteWaypoint->text().arg(tr("Position")));

  // Edit route position ==============================
  ui->actionMapEditUserWaypoint->setText(tr("Edit Flight Plan &Position or Remarks..."));
  if(routeIndex != -1)
  {
    if(userpointRoute != nullptr)
    {
      // Edit user waypoint ================
      ui->actionMapEditUserWaypoint->setEnabled(true);
      ui->actionMapEditUserWaypoint->setText(tr("Edit Flight Plan &Position %1 ...").arg(routeText));
      ui->actionMapEditUserWaypoint->setToolTip(tr("Edit name and coordinates of user defined flight plan position"));
      ui->actionMapEditUserWaypoint->setStatusTip(ui->actionMapEditUserWaypoint->toolTip());
    }
    else if(NavApp::getRouteConst().canEditComment(routeIndex))
    {
      // Edit remarks only ================
      ui->actionMapEditUserWaypoint->setEnabled(true);
      ui->actionMapEditUserWaypoint->setText(tr("Edit Flight Plan &Position Remarks for %1 ...").arg(routeText));
      ui->actionMapEditUserWaypoint->setToolTip(tr("Edit remarks for selected flight plan leg"));
      ui->actionMapEditUserWaypoint->setStatusTip(ui->actionMapEditUserWaypoint->toolTip());
    }
  }

  // Update "show range rings for Navaid" =====================
  if((vor != nullptr || ndb != nullptr) && NavApp::getMapMarkHandler()->isShown(map::MARK_RANGE_RINGS))
  {
    ui->actionMapNavaidRange->setEnabled(true);
    ui->actionMapNavaidRange->setText(ui->actionMapNavaidRange->text().arg(rangeRingText));
  }
  else
    ui->actionMapNavaidRange->setText(ui->actionMapNavaidRange->text().arg(QString()));

  if(parking == nullptr && helipad == nullptr && !measureText.isEmpty() &&
     NavApp::getMapMarkHandler()->isShown(map::MARK_MEASUREMENT))
  {
    // Set text to measure "from airport" etc.
    ui->actionMapMeasureDistance->setText(ui->actionMapMeasureDistance->text().arg(measureText));
  }
  else
  {
    // Noting found at cursor - use "measure from here"
    ui->actionMapMeasureDistance->setText(ui->actionMapMeasureDistance->text().arg(tr("here")));
  }

  // Update texts to give user a hint for hidden user features in the disabled menu items =====================
  QString notShown(tr(" (hidden on map)"));
  if(!NavApp::getMapMarkHandler()->isShown(map::MARK_MEASUREMENT))
  {
    ui->actionMapMeasureDistance->setText(ui->actionMapMeasureDistance->text() + notShown);
  }
  if(!NavApp::getMapMarkHandler()->isShown(map::MARK_RANGE_RINGS))
  {
    ui->actionMapRangeRings->setText(ui->actionMapRangeRings->text() + notShown);
    ui->actionMapNavaidRange->setText(ui->actionMapNavaidRange->text() + notShown);
  }
  if(!NavApp::getMapMarkHandler()->isShown(map::MARK_HOLDS))
    ui->actionMapHold->setText(ui->actionMapHold->text() + notShown);
  if(!NavApp::getMapMarkHandler()->isShown(map::MARK_PATTERNS))
    ui->actionMapTrafficPattern->setText(ui->actionMapTrafficPattern->text() + notShown);

  qDebug() << "departureParkingAirportId " << departureParkingAirportId;
  qDebug() << "airport " << airport;
  qDebug() << "vor " << vor;
  qDebug() << "ndb " << ndb;
  qDebug() << "waypoint " << waypoint;
  qDebug() << "ils " << ils;
  qDebug() << "parking " << parking;
  qDebug() << "helipad " << helipad;
  qDebug() << "routeIndex " << routeIndex;
  qDebug() << "userpointRoute " << userpointRoute;
  qDebug() << "informationText" << informationText;
  qDebug() << "procedureText" << procedureText;
  qDebug() << "measureText" << measureText;
  qDebug() << "departureText" << departureText;
  qDebug() << "destinationText" << destinationText;
  qDebug() << "addRouteText" << addRouteText;
  qDebug() << "searchText" << searchText;
  qDebug() << "patternText" << patternText;
  qDebug() << "holdText" << holdText;

  // Show the menu ------------------------------------------------
  selectedAction = menu.exec(menuPos);

  qDebug() << Q_FUNC_INFO << (selectedAction != nullptr ? selectedAction->text() : "null")
           << (selectedAction != nullptr ? selectedAction->objectName() : "null");

  return selectedAction;
}

QAction *MapContextMenu::getSelectedAction() const
{
  return selectedAction;
}

const map::MapSearchResult& MapContextMenu::getSelectedResult() const
{
  return *result;
}

bool MapContextMenu::isVisibleOnMap() const
{
  return visibleOnMap;
}

const map::MapLogbookEntry *MapContextMenu::getLogEntry() const
{
  return logEntry;
}

const map::MapVor *MapContextMenu::getVor() const
{
  return vor;
}

const map::MapNdb *MapContextMenu::getNdb() const
{
  return ndb;
}

const map::MapWaypoint *MapContextMenu::getWaypoint() const
{
  return waypoint;
}

bool MapContextMenu::isAircraft() const
{
  return aircraft;
}

const atools::fs::sc::SimConnectAircraft *MapContextMenu::getAiAircraft() const
{
  return aiAircraft;
}

const atools::fs::sc::SimConnectAircraft *MapContextMenu::getOnlineAircraft() const
{
  return onlineAircraft;
}

const atools::fs::sc::SimConnectUserAircraft *MapContextMenu::getUserAircraft() const
{
  return userAircraft;
}

const atools::geo::Pos& MapContextMenu::getPos() const
{
  return pos;
}

const map::MapParking *MapContextMenu::getParking() const
{
  return parking;
}

const map::MapHelipad *MapContextMenu::getHelipad() const
{
  return helipad;
}

const map::MapAirspace *MapContextMenu::getAirspace() const
{
  return airspace;
}

const map::MapIls *MapContextMenu::getIls() const
{
  return ils;
}

const map::MapUserpointRoute *MapContextMenu::getUserpointRoute() const
{
  return userpointRoute;
}

const map::MapAirspace *MapContextMenu::getOnlineCenter() const
{
  return onlineCenter;
}

const map::MapAirway *MapContextMenu::getAirway() const
{
  return airway;
}

bool MapContextMenu::isAirportDestination() const
{
  return airportDestination;
}

bool MapContextMenu::isAirportDeparture() const
{
  return airportDeparture;
}

int MapContextMenu::getDistMarkerIndex() const
{
  return distMarkerIndex;
}

int MapContextMenu::getTrafficPatternIndex() const
{
  return trafficPatternIndex;
}

int MapContextMenu::getHoldIndex() const
{
  return holdIndex;
}

int MapContextMenu::getRangeMarkerIndex() const
{
  return rangeMarkerIndex;
}

int MapContextMenu::getRouteIndex() const
{
  return routeIndex;
}

const map::MapAirport *MapContextMenu::getAirport() const
{
  return airport;
}

const map::MapUserpoint *MapContextMenu::getUserpoint() const
{
  return userpoint;
}
