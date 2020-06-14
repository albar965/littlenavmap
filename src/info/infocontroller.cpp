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

#include "infocontroller.h"

#include "atools.h"
#include "navapp.h"
#include "common/constants.h"
#include "common/maptools.h"
#include "common/maptypes.h"
#include "common/htmlinfobuilder.h"
#include "common/tabindexes.h"
#include "online/onlinedatacontroller.h"
#include "airspace/airspacecontroller.h"
#include "gui/tools.h"
#include "gui/mainwindow.h"
#include "gui/widgetutil.h"
#include "gui/widgetstate.h"
#include "gui/dialog.h"
#include "query/mapquery.h"
#include "query/airwaytrackquery.h"
#include "query/airportquery.h"
#include "route/route.h"
#include "common/mapcolors.h"
#include "settings/settings.h"
#include "ui_mainwindow.h"
#include "util/htmlbuilder.h"
#include "options/optiondata.h"
#include "sql/sqlrecord.h"
#include "mapgui/mapwidget.h"
#include "gui/helphandler.h"
#include "gui/tabwidgethandler.h"

#include <QUrlQuery>

using atools::util::HtmlBuilder;
using atools::fs::sc::SimConnectAircraft;
using atools::fs::sc::SimConnectUserAircraft;

namespace ahtml = atools::util::html;

InfoController::InfoController(MainWindow *parent)
  : QObject(parent), mainWindow(parent)
{
  mapQuery = NavApp::getMapQuery();
  airportQuery = NavApp::getAirportQuerySim();

  airspaceController = NavApp::getAirspaceController();

  infoBuilder = new HtmlInfoBuilder(mainWindow, true);

  // Get base font size for widgets
  Ui::MainWindow *ui = NavApp::getMainUi();

  tabHandlerInfo = new atools::gui::TabWidgetHandler(ui->tabWidgetInformation,
                                                     QIcon(":/littlenavmap/resources/icons/tabbutton.svg"),
                                                     tr("Open or close tabs"));
  tabHandlerInfo->init(ic::TabInfoIds, lnm::INFOWINDOW_WIDGET_TABS);

  tabHandlerAirportInfo = new atools::gui::TabWidgetHandler(ui->tabWidgetAirport,
                                                            QIcon(":/littlenavmap/resources/icons/tabbutton.svg"),
                                                            tr("Open or close tabs"));
  tabHandlerAirportInfo->init(ic::TabAirportInfoIds, lnm::INFOWINDOW_WIDGET_AIRPORT_TABS);

  tabHandlerAircraft = new atools::gui::TabWidgetHandler(ui->tabWidgetAircraft,
                                                         QIcon(":/littlenavmap/resources/icons/tabbutton.svg"),
                                                         tr("Open or close tabs"));
  tabHandlerAircraft->init(ic::TabAircraftIds, lnm::INFOWINDOW_WIDGET_AIRCRAFT_TABS);

  infoFontPtSize = static_cast<float>(ui->textBrowserAirportInfo->font().pointSizeF());
  simInfoFontPtSize = static_cast<float>(ui->textBrowserAircraftInfo->font().pointSizeF());

  // Set search path to silence text browser warnings
  QStringList paths({QApplication::applicationDirPath()});
  ui->textBrowserAirportInfo->setSearchPaths(paths);
  ui->textBrowserRunwayInfo->setSearchPaths(paths);
  ui->textBrowserComInfo->setSearchPaths(paths);
  ui->textBrowserApproachInfo->setSearchPaths(paths);
  ui->textBrowserNearestInfo->setSearchPaths(paths);
  ui->textBrowserWeatherInfo->setSearchPaths(paths);
  ui->textBrowserNavaidInfo->setSearchPaths(paths);
  ui->textBrowserUserpointInfo->setSearchPaths(paths);
  ui->textBrowserAirspaceInfo->setSearchPaths(paths);
  ui->textBrowserLogbookInfo->setSearchPaths(paths);
  ui->textBrowserCenterInfo->setSearchPaths(paths);
  ui->textBrowserClientInfo->setSearchPaths(paths);

  ui->textBrowserAircraftInfo->setSearchPaths(paths);
  ui->textBrowserAircraftProgressInfo->setSearchPaths(paths);
  ui->textBrowserAircraftAiInfo->setSearchPaths(paths);

  // Create connections for "Map" links in text browsers
  connect(ui->textBrowserAirportInfo, &QTextBrowser::anchorClicked, this, &InfoController::anchorClicked);
  connect(ui->textBrowserRunwayInfo, &QTextBrowser::anchorClicked, this, &InfoController::anchorClicked);
  connect(ui->textBrowserComInfo, &QTextBrowser::anchorClicked, this, &InfoController::anchorClicked);
  connect(ui->textBrowserApproachInfo, &QTextBrowser::anchorClicked, this, &InfoController::anchorClicked);
  connect(ui->textBrowserNearestInfo, &QTextBrowser::anchorClicked, this, &InfoController::anchorClicked);
  connect(ui->textBrowserWeatherInfo, &QTextBrowser::anchorClicked, this, &InfoController::anchorClicked);
  connect(ui->textBrowserNavaidInfo, &QTextBrowser::anchorClicked, this, &InfoController::anchorClicked);
  connect(ui->textBrowserUserpointInfo, &QTextBrowser::anchorClicked, this, &InfoController::anchorClicked);
  connect(ui->textBrowserAirspaceInfo, &QTextBrowser::anchorClicked, this, &InfoController::anchorClicked);
  connect(ui->textBrowserLogbookInfo, &QTextBrowser::anchorClicked, this, &InfoController::anchorClicked);

  connect(ui->textBrowserCenterInfo, &QTextBrowser::anchorClicked, this, &InfoController::anchorClicked);
  connect(ui->textBrowserClientInfo, &QTextBrowser::anchorClicked, this, &InfoController::anchorClicked);

  connect(ui->textBrowserAircraftInfo, &QTextBrowser::anchorClicked, this, &InfoController::anchorClicked);
  connect(ui->textBrowserAircraftProgressInfo, &QTextBrowser::anchorClicked, this, &InfoController::anchorClicked);
  connect(ui->textBrowserAircraftAiInfo, &QTextBrowser::anchorClicked, this, &InfoController::anchorClicked);

  connect(tabHandlerAircraft, &atools::gui::TabWidgetHandler::tabChanged,
          this, &InfoController::currentAircraftTabChanged);
  connect(tabHandlerInfo, &atools::gui::TabWidgetHandler::tabChanged,
          this, &InfoController::currentInfoTabChanged);
  connect(tabHandlerAirportInfo, &atools::gui::TabWidgetHandler::tabChanged,
          this, &InfoController::currentAirportInfoTabChanged);

  connect(ui->dockWidgetAircraft, &QDockWidget::visibilityChanged, this, &InfoController::visibilityChangedAircraft);
  connect(ui->dockWidgetInformation, &QDockWidget::visibilityChanged, this, &InfoController::visibilityChangedInfo);
}

InfoController::~InfoController()
{
  delete tabHandlerInfo;
  delete tabHandlerAirportInfo;
  delete tabHandlerAircraft;
  delete infoBuilder;
}

void InfoController::visibilityChangedAircraft(bool visible)
{
  if(visible)
    currentAircraftTabChanged(tabHandlerAircraft->getCurrentTabId());
}

void InfoController::visibilityChangedInfo(bool visible)
{
  if(visible)
  {
    currentInfoTabChanged(tabHandlerInfo->getCurrentTabId());
    currentAirportInfoTabChanged(tabHandlerAirportInfo->getCurrentTabId());
  }
}

void InfoController::currentAircraftTabChanged(int id)
{
  // Update new tab to avoid half a second delay or obsolete information
  switch(static_cast<ic::TabAircraftId>(id))
  {
    case ic::AIRCRAFT_USER:
      updateUserAircraftText();
      break;
    case ic::AIRCRAFT_USER_PROGRESS:
      updateAircraftProgressText();
      break;
    case ic::AIRCRAFT_AI:
      updateAiAircraftText();
      break;
  }
}

void InfoController::currentInfoTabChanged(int id)
{
  // Update new tab to avoid delay or obsolete information
  switch(static_cast<ic::TabInfoId>(id))
  {
    case ic::INFO_AIRPORT:
      // Tab of airport tabs - pass signal along
      currentAirportInfoTabChanged(tabHandlerAirportInfo->getCurrentTabId());
      break;

    case ic::INFO_NAVAID:
      updateNavaidInternal(currentSearchResult, true /* bearing changed */, false /* scroll to top */,
                           false /* force update */);
      break;

    case ic::INFO_USERPOINT:
      updateUserpointInternal(currentSearchResult, true /* bearing changed */, false /* scroll to top */);
      break;

    case ic::INFO_AIRSPACE:
    case ic::INFO_LOGBOOK:
    case ic::INFO_ONLINE_CLIENT:
    case ic::INFO_ONLINE_CENTER:
      break;
  }
}

void InfoController::currentAirportInfoTabChanged(int id)
{
  // Update new tab to avoid delay or obsolete information
  switch(static_cast<ic::TabAirportInfoId>(id))
  {
    case ic::INFO_AIRPORT_OVERVIEW:
      updateAirportInternal(false /* new */, true /* bearing changed */, false /* scroll to top */,
                            false /* force weather update */);
      break;

    case ic::INFO_AIRPORT_RUNWAYS:
    case ic::INFO_AIRPORT_COM:
    case ic::INFO_AIRPORT_APPROACHES:
    case ic::INFO_AIRPORT_NEAREST:
    case ic::INFO_AIRPORT_WEATHER:
      break;
  }
}

/* User clicked on "Map" link in text browsers */
void InfoController::anchorClicked(const QUrl& url)
{
  qDebug() << Q_FUNC_INFO << url;

  if(url.scheme() == "http" || url.scheme() == "https" || url.scheme() == "ftp" || url.scheme() == "file")
    // Open a normal link or file from the userpoint description
    atools::gui::anchorClicked(mainWindow, url);
  else if(url.scheme() == "lnm")
  {
    // Internal link like "show on map"
    QUrlQuery query(url);
    MapWidget *mapWidget = NavApp::getMapWidget();
    if(url.host() == "do")
    {
      if(query.hasQueryItem("hideairspaces"))
      {
        // Hide normal airspace highlights from information window =========================================
        mapWidget->clearAirspaceHighlights();
        mainWindow->updateHighlightActionStates();
      }
      else if(query.hasQueryItem("hideonlineairspaces"))
      {
        // Hide online airspaces from search or information window =========================================
        map::MapSearchResult searchHighlights = mapWidget->getSearchHighlights();
        searchHighlights.airspaces.clear();
        mapWidget->changeSearchHighlights(searchHighlights);
        mainWindow->updateHighlightActionStates();
      }
      else if(query.hasQueryItem("hideairways"))
      {
        // Hide airway highlights from information window =========================================
        mapWidget->clearAirwayHighlights();
        mainWindow->updateHighlightActionStates();
      }
      else if(query.hasQueryItem("lessprogress"))
      {
        // Handle more/less switch =====================================
        lessAircraftProgress = !lessAircraftProgress;
        NavApp::getMainUi()->textBrowserAircraftProgressInfo->setTextCursor(QTextCursor());
        updateAircraftProgressText();
      }
    }
    else if(url.host() == "show")
    {
      if(query.hasQueryItem("west") && query.hasQueryItem("north") &&
         query.hasQueryItem("east") && query.hasQueryItem("south"))
      {
        // Zoom to rect =========================================
        emit showRect(atools::geo::Rect(query.queryItemValue("west").toFloat(),
                                        query.queryItemValue("north").toFloat(),
                                        query.queryItemValue("east").toFloat(),
                                        query.queryItemValue("south").toFloat()), false /* doubleClick */);
      }
      else if(query.hasQueryItem("lonx") && query.hasQueryItem("laty"))
      {
        // Zoom to position =========================================
        float distanceKm = 0.f;
        if(query.hasQueryItem("distance"))
          distanceKm = query.queryItemValue("distance").toFloat();

        emit showPos(atools::geo::Pos(query.queryItemValue("lonx"), query.queryItemValue("laty")), distanceKm,
                     false /* doubleClick */);
      }
      else if(query.hasQueryItem("id") && query.hasQueryItem("type"))
      {
        // Zoom to an map object =========================================
        map::MapObjectTypes type(query.queryItemValue("type").toInt());
        int id = query.queryItemValue("id").toInt();

        if(type == map::AIRPORT)
          // Show airport by id ================================================
          emit showRect(airportQuery->getAirportById(id).bounding, false);
        else if(type == map::AIRSPACE)
        {
          // Show airspaces by id and source ================================================
          map::MapAirspaceSources src(query.queryItemValue("source").toInt());

          // Append airspace to current highlight list if not already present
          map::MapAirspace airspace = airspaceController->getAirspaceById({id, src});

          if(src & map::AIRSPACE_SRC_ONLINE)
          {
            // Append online center to current highlight list if not already present
            map::MapSearchResult searchHighlights = mapWidget->getSearchHighlights();
            if(!maptools::containsId(searchHighlights.airspaces, airspace.id))
              searchHighlights.airspaces.append(airspace);
            mapWidget->changeSearchHighlights(searchHighlights);
          }
          else
          {
            QList<map::MapAirspace> airspaceHighlights = mapWidget->getAirspaceHighlights();
            if(!maptools::containsId(airspaceHighlights, airspace.id))
              airspaceHighlights.append(airspace);
            mapWidget->changeAirspaceHighlights(airspaceHighlights);
          }
          mainWindow->updateHighlightActionStates();
          emit showRect(airspace.bounding, false);
        }
        else if(type == map::AIRWAY)
        {
          // Show full airways by id ================================================
          map::MapAirway airway = NavApp::getAirwayTrackQuery()->getAirwayById(id);

          // Get all airway segments and the bounding rectangle
          atools::geo::Rect bounding;
          QList<map::MapAirway> airways;
          NavApp::getAirwayTrackQuery()->getAirwayFull(airways, bounding, airway.name, airway.fragment);

          QList<QList<map::MapAirway> > airwayHighlights = mapWidget->getAirwayHighlights();
          airwayHighlights.append(airways);
          mapWidget->changeAirwayHighlights(airwayHighlights);
          mainWindow->updateHighlightActionStates();
          emit showRect(bounding, false);
        }
        else if(type == map::ILS)
          // Show ILS by bounding rectangle ================================================
          emit showRect(mapQuery->getIlsById(id).bounding, false);
        else
          qWarning() << Q_FUNC_INFO << "Unknwown type" << url << type;
      }
      else if(query.hasQueryItem("airport"))
      {
        // Airport ident from AI aircraft progress
        map::MapAirport airport;
        airportQuery->getAirportByIdent(airport, query.queryItemValue("airport"));
        emit showRect(airport.bounding, false);
      }
      else if(query.hasQueryItem("filepath"))
        // Show path in any OS dependent file manager. Selects the file in Windows Explorer.
        atools::gui::showInFileManager(query.queryItemValue("filepath"), mainWindow);
      else
        qWarning() << Q_FUNC_INFO << "Unknwown URL" << url;
    }
  }
}

void InfoController::saveState()
{
  Ui::MainWindow *ui = NavApp::getMainUi();
  atools::gui::WidgetState(lnm::INFOWINDOW_WIDGET).save(ui->tabWidgetLegend);

  // Store currently shown map objects in a string list containing id and type
  map::MapObjectRefVector refs;
  for(const map::MapAirport& airport  : currentSearchResult.airports)
    refs.append({airport.id, map::AIRPORT});

  for(const map::MapVor& vor : currentSearchResult.vors)
    refs.append({vor.id, map::VOR});

  for(const map::MapNdb& ndb : currentSearchResult.ndbs)
    refs.append({ndb.id, map::NDB});

  for(const map::MapWaypoint& waypoint : currentSearchResult.waypoints)
    refs.append({waypoint.id, map::WAYPOINT});

  for(const map::MapIls& ils : currentSearchResult.ils)
    refs.append({ils.id, map::ILS});

  for(const map::MapUserpoint& userpoint: currentSearchResult.userpoints)
    refs.append({userpoint.id, map::USERPOINT});

  for(const map::MapLogbookEntry& logEntry : currentSearchResult.logbookEntries)
    refs.append({logEntry.id, map::LOGBOOK});

  for(const map::MapAirway& airway : currentSearchResult.airways)
    refs.append({airway.id, map::AIRWAY});

  // Save list =====================================================
  atools::settings::Settings& settings = atools::settings::Settings::instance();
  QStringList refList;
  for(const map::MapObjectRef& ref : refs)
    refList.append(QString("%1;%2").arg(ref.id).arg(ref.objType));
  settings.setValue(lnm::INFOWINDOW_CURRENTMAPOBJECTS, refList.join(";"));

  // Save airspaces =====================================================
  QVector<map::MapAirspaceId> airspaces;
  refList.clear();
  for(const map::MapAirspace& airspace : currentSearchResult.airspaces)
  {
    // Do not save online airspace ids since they will change on next startup
    if(!airspace.isOnline())
      refList.append(QString("%1;%2").arg(airspace.id).arg(airspace.src));
  }
  settings.setValue(lnm::INFOWINDOW_CURRENTAIRSPACES, refList.join(";"));

  // More / less state ===================================
  settings.setValue(lnm::INFOWINDOW_MORE_LESS_PROGRESS, lessAircraftProgress);

  tabHandlerInfo->saveState();
  tabHandlerAirportInfo->saveState();
  tabHandlerAircraft->saveState();
}

void InfoController::restoreState()
{
  tabHandlerInfo->restoreState();
  tabHandlerAirportInfo->restoreState();
  tabHandlerAircraft->restoreState();

  if(OptionData::instance().getFlags() & opts::STARTUP_LOAD_INFO)
  {
    lessAircraftProgress =
      atools::settings::Settings::instance().valueBool(lnm::INFOWINDOW_MORE_LESS_PROGRESS, false);

    // Go through the string and collect all objects in the MapSearchResult
    map::MapSearchResult res;

    // All objects =================================
    QString refsStr = atools::settings::Settings::instance().valueStr(lnm::INFOWINDOW_CURRENTMAPOBJECTS);
    QStringList refsStrList = refsStr.split(";", QString::SkipEmptyParts);
    for(int i = 0; i < refsStrList.size(); i += 2)
      mapQuery->getMapObjectById(res, map::MapObjectTypes(refsStrList.value(i + 1).toInt()), map::AIRSPACE_SRC_NONE,
                                 refsStrList.value(i).toInt(), false /* airport from nav database */);

    // Airspaces =================================
    refsStr = atools::settings::Settings::instance().valueStr(lnm::INFOWINDOW_CURRENTAIRSPACES);
    refsStrList = refsStr.split(";", QString::SkipEmptyParts);
    for(int i = 0; i < refsStrList.size(); i += 2)
    {
      map::MapAirspaceId id;
      id.id = refsStrList.value(i).toInt();
      id.src = static_cast<map::MapAirspaceSources>(refsStrList.value(i + 1).toInt());
      res.airspaces.append(NavApp::getAirspaceController()->getAirspaceById(id));
    }

    showInformationInternal(res, map::NONE, false /* show windows */, false /* scroll to top */,
                            true /* forceUpdate */);

    Ui::MainWindow *ui = NavApp::getMainUi();
    atools::gui::WidgetState(lnm::INFOWINDOW_WIDGET).restore(ui->tabWidgetLegend);
  }
  updateTextEditFontSizes();
}

void InfoController::updateAirport()
{
  updateAirportInternal(false /* new */, false /* bearing change*/, false /* scroll to top */,
                        false /* force weather update */);
}

void InfoController::updateAirportWeather()
{
  updateAirportInternal(false /* new */, false /* bearing change*/, false /* scroll to top */,
                        true /* force weather update */);
}

void InfoController::updateProgress()
{
  HtmlBuilder html(true /* has background color */);
  Ui::MainWindow *ui = NavApp::getMainUi();

  if(atools::gui::util::canTextEditUpdate(ui->textBrowserAircraftProgressInfo))
  {
    // ok - scrollbars not pressed
    html.clear();
    infoBuilder->aircraftProgressText(lastSimData.getUserAircraftConst(), html, NavApp::getRouteConst(),
                                      true /* show more/less switch */, lessAircraftProgress);
    atools::gui::util::updateTextEdit(ui->textBrowserAircraftProgressInfo, html.getHtml(),
                                      false /* scroll to top*/, true /* keep selection */);
  }
}

void InfoController::updateAirportInternal(bool newAirport, bool bearingChange, bool scrollToTop,
                                           bool forceWeatherUpdate)
{
  if(databaseLoadStatus)
    return;

  if(currentSearchResult.hasAirports())
  {
    map::WeatherContext currentWeatherContext;
    bool weatherChanged = mainWindow->buildWeatherContextForInfo(currentWeatherContext,
                                                                 currentSearchResult.airports.first());

    // qDebug() << Q_FUNC_INFO << "newAirport" << newAirport << "weatherChanged" << weatherChanged
    // << "ident" << currentWeatherContext.ident;

    if(newAirport || weatherChanged || bearingChange || forceWeatherUpdate)
    {
      HtmlBuilder html(true);
      map::MapAirport airport;
      airportQuery->getAirportById(airport, currentSearchResult.airports.first().id);

      // qDebug() << Q_FUNC_INFO << "Updating html" << airport.ident << airport.id;

      infoBuilder->airportText(airport, currentWeatherContext, html, &NavApp::getRouteConst());

      Ui::MainWindow *ui = NavApp::getMainUi();
      // Leave position for weather or bearing updates
      atools::gui::util::updateTextEdit(ui->textBrowserAirportInfo, html.getHtml(),
                                        scrollToTop, !scrollToTop /* keep selection */);

      if(newAirport || weatherChanged || forceWeatherUpdate)
      {
        html.clear();
        infoBuilder->weatherText(currentWeatherContext, airport, html);

        // Leave position for weather updates
        atools::gui::util::updateTextEdit(ui->textBrowserWeatherInfo, html.getHtml(),
                                          scrollToTop, !scrollToTop /* keep selection */);
      }
    }
  }
}

void InfoController::clearInfoTextBrowsers()
{
  Ui::MainWindow *ui = NavApp::getMainUi();

  ui->textBrowserAirportInfo->clear();
  ui->textBrowserRunwayInfo->clear();
  ui->textBrowserComInfo->clear();
  ui->textBrowserApproachInfo->clear();
  ui->textBrowserNearestInfo->clear();
  ui->textBrowserWeatherInfo->clear();
  ui->textBrowserNavaidInfo->clear();
  ui->textBrowserUserpointInfo->clear();
  ui->textBrowserLogbookInfo->clear();
  ui->textBrowserAirspaceInfo->clear();

  ui->textBrowserClientInfo->clear();
  ui->textBrowserCenterInfo->clear();
}

void InfoController::showInformation(map::MapSearchResult result, map::MapObjectTypes preferredType)
{
  showInformationInternal(result, preferredType, true /* Show windows */, true /* scroll to top */,
                          false /* forceUpdate */);
}

void InfoController::updateAllInformation()
{
  showInformationInternal(currentSearchResult, map::NONE, false /* Show windows */, false /* scroll to top */,
                          false /* forceUpdate */);
}

void InfoController::onlineNetworkChanged()
{
  // Clear display
  NavApp::getMainUi()->textBrowserCenterInfo->clear();

  // Remove all online network airspaces from current result
  QList<map::MapAirspace> airspaces;
  for(const map::MapAirspace& airspace : currentSearchResult.airspaces)
    if(!airspace.isOnline())
      airspaces.append(airspace);
  currentSearchResult.airspaces = airspaces;

  showInformationInternal(currentSearchResult, map::NONE, false /* show windows */, false /* scroll to top */,
                          true /* forceUpdate */);
}

void InfoController::onlineClientAndAtcUpdated()
{
  showInformationInternal(currentSearchResult, map::NONE, false /* show windows */,
                          false /* scroll to top */, true /* forceUpdate */);
}

/* Show information in all tabs but do not show dock
 *  @return true if information was updated */
void InfoController::showInformationInternal(map::MapSearchResult result, map::MapObjectTypes preferredType,
                                             bool showWindows, bool scrollToTop, bool forceUpdate)
{
  qDebug() << Q_FUNC_INFO;

  if(preferredType != map::NONE)
    // Clear all except preferred - used by context menu "Show information for ..." to update only one topic
    result.clear(~preferredType);

  bool foundAirport = false, foundNavaid = false, foundUserpoint = false,
       foundUserAircraft = false, foundUserAircraftShadow = false,
       foundAiAircraft = false, foundOnlineClient = false,
       foundAirspace = false, foundLogbookEntry = false, foundOnlineCenter = false;
  HtmlBuilder html(true);

  Ui::MainWindow *ui = NavApp::getMainUi();

  currentSearchResult.userAircraft = result.userAircraft;
  foundUserAircraft = currentSearchResult.userAircraft.getPosition().isValid();
  if(foundUserAircraft)
    foundUserAircraftShadow = currentSearchResult.userAircraft.isOnlineShadow();

  // Remember the clicked AI for the next update
  if(!result.aiAircraft.isEmpty())
  {
    qDebug() << "Found AI";
    currentSearchResult.aiAircraft = result.aiAircraft;
    foundAiAircraft = true;
  }

  // AI aircraft ================================================================
  for(const SimConnectAircraft& ac : currentSearchResult.aiAircraft)
    qDebug() << "Show AI" << ac.getAirplaneRegistration() << "id" << ac.getObjectId();

  if(foundUserAircraftShadow || !result.onlineAircraft.isEmpty())
  {
    OnlinedataController *odc = NavApp::getOnlinedataController();
    int num = 1;
    if(foundUserAircraftShadow)
    {
      SimConnectAircraft ac;
      odc->getShadowAircraft(ac, currentSearchResult.userAircraft);
      infoBuilder->aircraftText(ac, html, num++, odc->getNumClients());
      infoBuilder->aircraftProgressText(ac, html, Route(),
                                        false /* show more/less switch */, false /* true if less info mode */);
      infoBuilder->aircraftOnlineText(ac, odc->getClientRecordById(ac.getId()), html);
    }

    // Online aircraft
    if(!result.onlineAircraft.isEmpty())
    {
      // Clear current result and add only shown aircraft
      currentSearchResult.onlineAircraft.clear();
      currentSearchResult.onlineAircraftIds.clear();

      for(const SimConnectAircraft& ac : result.onlineAircraft)
      {
        infoBuilder->aircraftText(ac, html, num++, odc->getNumClients());
        infoBuilder->aircraftProgressText(ac, html, Route(),
                                          false /* show more/less switch */, false /* true if less info mode */);
        infoBuilder->aircraftOnlineText(ac, odc->getClientRecordById(ac.getId()), html);

        currentSearchResult.onlineAircraft.append(ac);
      }
    }
    atools::gui::util::updateTextEdit(ui->textBrowserClientInfo, html.getHtml(),
                                      false /* scroll to top*/, true /* keep selection */);
    foundOnlineClient = true;
  }

  // Airport ================================================================
  if(!result.airports.isEmpty())
  {
#ifdef DEBUG_INFORMATION
    qDebug() << "Found airport" << result.airports.first().ident;
#endif

    // Only one airport shown - have to make a copy here since currentSearchResult might be equal to result
    // when updating
    map::MapAirport airport = result.airports.first();

    bool changed = !currentSearchResult.hasAirports() || currentSearchResult.airports.first().id != airport.id;

    currentSearchResult.airports.clear();
    currentSearchResult.airportIds.clear();
    // Remember one airport
    currentSearchResult.airports.append(airport);

    // Update parts that are variable because of weather or bearing changes ==================
    updateAirportInternal(true /* new */, false /* bearing change*/, scrollToTop, false /* force weather update */);

    if(changed || forceUpdate)
    {
      // Update parts that have now weather or bearing depenedency =====================
      html.clear();
      infoBuilder->runwayText(airport, html);
      atools::gui::util::updateTextEdit(ui->textBrowserRunwayInfo, html.getHtml(),
                                        scrollToTop, !scrollToTop /* keep selection */);

      html.clear();
      infoBuilder->comText(airport, html);
      atools::gui::util::updateTextEdit(ui->textBrowserComInfo, html.getHtml(),
                                        scrollToTop, !scrollToTop /* keep selection */);

      html.clear();
      infoBuilder->procedureText(airport, html);
      atools::gui::util::updateTextEdit(ui->textBrowserApproachInfo, html.getHtml(),
                                        scrollToTop, !scrollToTop /* keep selection */);

      html.clear();
      infoBuilder->nearestText(airport, html);
      atools::gui::util::updateTextEdit(ui->textBrowserNearestInfo, html.getHtml(),
                                        scrollToTop, !scrollToTop /* keep selection */);
    }

    foundAirport = true;
  }

  // Airspaces ================================================================
  if(!result.airspaces.isEmpty())
  {
    atools::sql::SqlRecord onlineRec;

    if(result.hasSimNavUserAirspaces())
      currentSearchResult.clearNavdataAirspaces();

    html.clear();

    // Remove header link ==============================
    html.tableAtts({
      {"width", "100%"}
    }).tr();
    html.tdAtts({
      {"align", "right"}, {"valign", "top"}
    });
    html.b().a(tr("Remove Airspace Highlights"), QString("lnm://do?hideairspaces"),
               ahtml::LINK_NO_UL).bEnd().tdEnd().trEnd().tableEnd();

    for(const map::MapAirspace& airspace : result.getSimNavUserAirspaces())
    {
      qDebug() << "Found airspace" << airspace.id;

      currentSearchResult.airspaces.append(airspace);
      infoBuilder->airspaceText(airspace, onlineRec, html);
      html.br();
      foundAirspace = true;
    }

    if(foundAirspace)
      // Update and keep scroll position
      atools::gui::util::updateTextEdit(ui->textBrowserAirspaceInfo, html.getHtml(),
                                        false /* scroll to top*/, true /* keep selection */);

    // Online Center ==================================
    if(result.hasOnlineAirspaces())
      currentSearchResult.clearOnlineAirspaces();

    html.clear();

    // Delete all airspaces that were removed from the database inbetween
    QList<map::MapAirspace> onlineAirspaces = result.getOnlineAirspaces();
    QList<map::MapAirspace>::iterator it = std::remove_if(onlineAirspaces.begin(), onlineAirspaces.end(),
                                                          [ = ](const map::MapAirspace& airspace) -> bool
    {
      return !airspaceController->hasAirspaceById({airspace.id, map::AIRSPACE_SRC_ONLINE});
    });
    if(it != onlineAirspaces.end())
      onlineAirspaces.erase(it, onlineAirspaces.end());

    html.tableAtts({
      {"width", "100%"}
    }).tr();
    html.tdAtts({
      {"align", "right"}, {"valign", "top"}
    });
    html.b().a(tr("Remove Center Highlights"), QString("lnm://do?hideonlineairspaces"),
               ahtml::LINK_NO_UL).bEnd().tdEnd().trEnd().tableEnd();

    for(const map::MapAirspace& airspace : onlineAirspaces)
    {
      qDebug() << "Found airspace" << airspace.id;

      currentSearchResult.airspaces.append(airspace);

      // Get extra information for online network ATC
      if(airspace.isOnline())
        onlineRec = airspaceController->getOnlineAirspaceRecordById(airspace.id);

      infoBuilder->airspaceText(airspace, onlineRec, html);
      html.br();
      foundOnlineCenter = true;
    }

    if(foundOnlineCenter)
      // Update and keep scroll position
      atools::gui::util::updateTextEdit(ui->textBrowserCenterInfo, html.getHtml(),
                                        false /* scroll to top*/, true /* keep selection */);
  }

  // Logbook Entries ================================================================
  if(!result.logbookEntries.isEmpty())
  {
    html.clear();

    currentSearchResult.logbookEntries.clear();

    for(const map::MapLogbookEntry& logEntry : result.logbookEntries)
    {
      qDebug() << "Found log entry" << logEntry.id;

      currentSearchResult.logbookEntries.append(logEntry);
      foundLogbookEntry |= infoBuilder->logEntryText(logEntry, html);
      html.br();
    }

    if(foundLogbookEntry)
      // Update and keep scroll position
      atools::gui::util::updateTextEdit(ui->textBrowserLogbookInfo, html.getHtml(),
                                        false /* scroll to top*/, true /* keep selection */);
    else
      ui->textBrowserLogbookInfo->clear();

  }

  // Navaids ================================================================
  if(result.hasVor() || result.hasNdb() || result.hasWaypoints() || result.hasIls() || result.hasAirways())
    // if any navaids are to be shown clear search result before
    currentSearchResult.clear(map::NAV_ALL | map::ILS | map::AIRWAY | map::RUNWAYEND);

  if(!result.userpoints.isEmpty())
    // if any userpoints are to be shown clear search result before
    currentSearchResult.clear(map::USERPOINT);

  foundNavaid = updateNavaidInternal(result, false /* bearing changed */, scrollToTop, forceUpdate);
  foundUserpoint = updateUserpointInternal(result, false /* bearing changed */, scrollToTop);

  // Show dock windows if needed
  if(showWindows)
  {
    if(foundNavaid || foundUserpoint || foundAirport || foundAirspace || foundLogbookEntry || foundOnlineCenter ||
       foundOnlineClient || foundUserAircraftShadow)
    {
      NavApp::getMainUi()->dockWidgetInformation->show();
      NavApp::getMainUi()->dockWidgetInformation->raise();
    }

    if(foundUserAircraft || foundAiAircraft)
    {
      NavApp::getMainUi()->dockWidgetAircraft->show();
      NavApp::getMainUi()->dockWidgetAircraft->raise();
    }
  }

  if(showWindows)
  {
    // Select tab to activate ==========================================================
    ic::TabInfoId idx = tabHandlerInfo->getCurrentTabId<ic::TabInfoId>();

    ic::TabInfoId newId = idx;

    if(preferredType != map::NONE)
    {
      // Select the tab to activate by preferred type so that it matches the selected menu item
      if(preferredType & map::AIRPORT)
        newId = ic::INFO_AIRPORT;
      else if(preferredType & map::USERPOINT)
        newId = ic::INFO_USERPOINT;
      else if(preferredType & map::NAV_ALL || preferredType & map::AIRWAY)
        newId = ic::INFO_NAVAID;
      else if(preferredType & map::AIRSPACE && foundOnlineCenter)
        newId = ic::INFO_ONLINE_CENTER;
      else if(preferredType & map::AIRCRAFT_ONLINE)
        newId = ic::INFO_ONLINE_CLIENT;
      else if(preferredType & map::AIRSPACE)
        newId = ic::INFO_AIRSPACE;
      else if(preferredType & map::LOGBOOK)
        newId = ic::INFO_LOGBOOK;
    }
    else
    {
      bool onlineClient = foundOnlineClient || foundUserAircraftShadow;

      //// Decide which tab to activate automatically so that it tries to keep the current tab in front
      if(onlineClient)
        newId = ic::INFO_ONLINE_CLIENT;
      else if(foundLogbookEntry)
        newId = ic::INFO_LOGBOOK;
      else if(foundUserpoint)
        newId = ic::INFO_USERPOINT;
      else if(foundAirport)
        newId = ic::INFO_AIRPORT;
      else if(foundNavaid)
        newId = ic::INFO_NAVAID;
      else if(foundOnlineCenter)
        newId = ic::INFO_ONLINE_CENTER;
      else if(foundAirspace)
        newId = ic::INFO_AIRSPACE;
    }

    tabHandlerInfo->setCurrentTab(newId);

    // Select message ==========================================================
    QString objType;
    switch(newId)
    {
      case ic::INFO_AIRPORT:
        objType = tr("airport");
        break;
      case ic::INFO_NAVAID:
        objType = tr("navaid");
        break;
      case ic::INFO_USERPOINT:
        objType = tr("userpoint");
        break;
      case ic::INFO_AIRSPACE:
        objType = tr("airspace");
        break;
      case ic::INFO_LOGBOOK:
        objType = tr("logbook entry");
        break;
      case ic::INFO_ONLINE_CLIENT:
        objType = tr("online clients");
        break;
      case ic::INFO_ONLINE_CENTER:
        objType = tr("online center");
        break;
    }
    mainWindow->setStatusMessage(tr("Showing information for %1.").arg(objType));

    // Switch to a tab in aircraft window
    ic::TabAircraftId acidx = static_cast<ic::TabAircraftId>(tabHandlerAircraft->getCurrentTabId());
    if(foundUserAircraft && !foundAiAircraft)
    {
      // If no user aircraft related tabs is shown bring user tab to front
      if(acidx != ic::AIRCRAFT_USER && acidx != ic::AIRCRAFT_USER_PROGRESS)
      {
        // Open aircraft overview
        tabHandlerAircraft->openTab(ic::AIRCRAFT_USER);

        // Open and activate progress
        tabHandlerAircraft->setCurrentTab(ic::AIRCRAFT_USER_PROGRESS);
      }
    }
    if(!foundUserAircraft && foundAiAircraft)
      tabHandlerAircraft->setCurrentTab(ic::AIRCRAFT_AI);
  }
}

bool InfoController::updateNavaidInternal(const map::MapSearchResult& result, bool bearingChanged, bool scrollToTop,
                                          bool forceUpdate)
{
  HtmlBuilder html(true);
  Ui::MainWindow *ui = NavApp::getMainUi();
  bool foundNavaid = false;

  // Remove header link ==============================
  html.tableAtts({
    {"width", "100%"}
  }).tr();
  html.tdAtts({
    {"align", "right"}, {"valign", "top"}
  });
  html.b().a(tr("Remove Airway and Track Highlights"), QString("lnm://do?hideairways"),
             ahtml::LINK_NO_UL).bEnd().tdEnd().trEnd().tableEnd();

  for(const map::MapVor& vor : result.vors)
  {
#ifdef DEBUG_INFORMATION
    qDebug() << "Found vor" << vor.ident;
#endif

    if(!bearingChanged)
      currentSearchResult.vors.append(vor);
    infoBuilder->vorText(vor, html);
    html.br();
    foundNavaid = true;
  }

  for(const map::MapNdb& ndb : result.ndbs)
  {
#ifdef DEBUG_INFORMATION
    qDebug() << "Found ndb" << ndb.ident;
#endif

    if(!bearingChanged)
      currentSearchResult.ndbs.append(ndb);
    infoBuilder->ndbText(ndb, html);
    html.br();
    foundNavaid = true;
  }

  for(const map::MapWaypoint& waypoint : result.waypoints)
  {
#ifdef DEBUG_INFORMATION
    qDebug() << "Found waypoint" << waypoint.ident;
#endif

    if(!bearingChanged)
      currentSearchResult.waypoints.append(waypoint);
    infoBuilder->waypointText(waypoint, html);
    html.br();
    foundNavaid = true;
  }

  for(const map::MapIls& ils: result.ils)
  {
#ifdef DEBUG_INFORMATION
    qDebug() << "Found ils" << ils.ident;
#endif

    if(!bearingChanged)
      currentSearchResult.ils.append(ils);
    infoBuilder->ilsText(ils, html);
    html.br();
    foundNavaid = true;
  }

  for(const map::MapAirway& airway : result.airways)
  {
#ifdef DEBUG_INFORMATION
    qDebug() << "Found airway" << airway.name;
#endif

    if(!bearingChanged)
      currentSearchResult.airways.append(airway);
    infoBuilder->airwayText(airway, html);
    html.br();
    foundNavaid = true;
  }

  if(foundNavaid || forceUpdate)
    atools::gui::util::updateTextEdit(ui->textBrowserNavaidInfo, html.getHtml(),
                                      scrollToTop, !scrollToTop /* keep selection */);

  return foundNavaid;
}

bool InfoController::updateUserpointInternal(const map::MapSearchResult& result, bool bearingChanged, bool scrollToTop)
{
  HtmlBuilder html(true);
  Ui::MainWindow *ui = NavApp::getMainUi();
  bool foundUserpoint = false;

  // Userpoints on top of the list
  for(map::MapUserpoint userpoint: result.userpoints)
  {
#ifdef DEBUG_INFORMATION
    qDebug() << "Found waypoint" << userpoint.ident;
#endif
    if(!bearingChanged)
      currentSearchResult.userpoints.append(userpoint);
    foundUserpoint |= infoBuilder->userpointText(userpoint, html);
    html.br();
  }

  if(foundUserpoint)
    atools::gui::util::updateTextEdit(ui->textBrowserUserpointInfo, html.getHtml(),
                                      scrollToTop, !scrollToTop /* keep selection */);
  else
    ui->textBrowserUserpointInfo->clear();

  return foundUserpoint;
}

void InfoController::preDatabaseLoad()
{
  // Clear current airport and navaids result
  currentSearchResult = map::MapSearchResult();
  databaseLoadStatus = true;
  clearInfoTextBrowsers();
}

void InfoController::postDatabaseLoad()
{
  databaseLoadStatus = false;
  updateAircraftInfo();
}

void InfoController::styleChanged()
{
  tabHandlerInfo->styleChanged();
  tabHandlerAirportInfo->styleChanged();
  tabHandlerAircraft->styleChanged();
  showInformationInternal(currentSearchResult, map::NONE, false /* Show windows */, false /* scroll to top */,
                          true /* forceUpdate */);
}

void InfoController::tracksChanged()
{
  // Remove tracks from current result since the ids might change
  currentSearchResult.airways.erase(std::remove_if(currentSearchResult.airways.begin(),
                                                   currentSearchResult.airways.end(),
                                                   [ = ](const map::MapAirway& airway) -> bool
  {
    return airway.isTrack();
  }), currentSearchResult.airways.end());

  // Update all tabs and force update
  showInformationInternal(currentSearchResult, map::NONE, false /* Show windows */, false /* scroll to top */,
                          true /* forceUpdate */);
}

void InfoController::updateUserAircraftText()
{
  Ui::MainWindow *ui = NavApp::getMainUi();
#ifdef DEBUG_MOVING_AIRPLANE
  if( /* DISABLES CODE */ (true))
#else
  if(NavApp::isConnected())
#endif
  {
    if(lastSimData.getUserAircraftConst().getPosition().isValid())
    {
      if(atools::gui::util::canTextEditUpdate(ui->textBrowserAircraftInfo))
      {
        // ok - scrollbars not pressed
        HtmlBuilder html(true /* has background color */);
        infoBuilder->aircraftText(lastSimData.getUserAircraftConst(), html);
        infoBuilder->aircraftTextWeightAndFuel(lastSimData.getUserAircraftConst(), html);
        atools::gui::util::updateTextEdit(ui->textBrowserAircraftInfo, html.getHtml(),
                                          false /* scroll to top*/, true /* keep selection */);
      }
    }
    else
      ui->textBrowserAircraftInfo->setPlainText(tr("Connected. Waiting for update."));
  }
  else
    ui->textBrowserAircraftInfo->clear();
}

void InfoController::updateAircraftProgressText()
{
  Ui::MainWindow *ui = NavApp::getMainUi();
#ifdef DEBUG_MOVING_AIRPLANE
  if( /* DISABLES CODE */ (true))
#else
  if(NavApp::isConnected())
#endif
  {
    if(lastSimData.getUserAircraftConst().isValid())
    {
      if(atools::gui::util::canTextEditUpdate(ui->textBrowserAircraftProgressInfo))
      {
        // ok - scrollbars not pressed
        HtmlBuilder html(true /* has background color */);
        infoBuilder->aircraftProgressText(lastSimData.getUserAircraftConst(), html, NavApp::getRouteConst(),
                                          true /* show more/less switch */, lessAircraftProgress);
        atools::gui::util::updateTextEdit(ui->textBrowserAircraftProgressInfo, html.getHtml(),
                                          false /* scroll to top*/, true /* keep selection */);
      }
    }
    else
      ui->textBrowserAircraftProgressInfo->setPlainText(tr("Connected. Waiting for update."));
  }
  else
    ui->textBrowserAircraftProgressInfo->clear();
}

void InfoController::updateAiAircraftText()
{
  Ui::MainWindow *ui = NavApp::getMainUi();
#ifdef DEBUG_MOVING_AIRPLANE
  if( /* DISABLES CODE */ (true))
#else
  if(NavApp::isConnected())
#endif
  {
    if(lastSimData.getUserAircraftConst().getPosition().isValid())
    {
      if(atools::gui::util::canTextEditUpdate(ui->textBrowserAircraftAiInfo))
      {
        // ok - scrollbars not pressed
        HtmlBuilder html(true /* has background color */);
        html.clear();
        if(!currentSearchResult.aiAircraft.isEmpty())
        {
          int num = 1;
          for(const SimConnectAircraft& aircraft : currentSearchResult.aiAircraft)
          {
            infoBuilder->aircraftText(aircraft, html, num, lastSimData.getAiAircraftConst().size());
            infoBuilder->aircraftProgressText(aircraft, html, Route(),
                                              false /* show more/less switch */, false /* true if less info mode */);
            num++;
          }

          atools::gui::util::updateTextEdit(ui->textBrowserAircraftAiInfo, html.getHtml(),
                                            false /* scroll to top*/, true /* keep selection */);
        }
        else
        {
          int numAi = lastSimData.getAiAircraftConst().size();
          QString text;

          if(!(NavApp::getShownMapFeatures() & map::AIRCRAFT_AI))
            text = tr("<b>AI and multiplayer aircraft are not shown on map.</b><br/>");

          text += tr("No AI or multiplayer aircraft selected.<br/>"
                     "Found %1 AI or multiplayer aircraft.").
                  arg(numAi > 0 ? QLocale().toString(numAi) : tr("no"));
          atools::gui::util::updateTextEdit(ui->textBrowserAircraftAiInfo, text,
                                            false /* scroll to top*/, true /* keep selection */);
        }
      }
    }
    else
      ui->textBrowserAircraftAiInfo->setPlainText(tr("Connected. Waiting for update."));
  }
  else
    ui->textBrowserAircraftAiInfo->clear();
}

void InfoController::simDataChanged(atools::fs::sc::SimConnectData data)
{
  if(databaseLoadStatus)
    return;

  Ui::MainWindow *ui = NavApp::getMainUi();

  if(atools::almostNotEqual(QDateTime::currentDateTime().toMSecsSinceEpoch(),
                            lastSimUpdate, static_cast<qint64>(MIN_SIM_UPDATE_TIME_MS)))
  {
    // Last update was more than 500 ms ago
    updateAiAirports(data);

    lastSimData = data;
    if(data.getUserAircraftConst().isValid() && ui->dockWidgetAircraft->isVisible())
    {
      if(tabHandlerAircraft->getCurrentTabId() == ic::AIRCRAFT_USER)
        updateUserAircraftText();

      if(tabHandlerAircraft->getCurrentTabId() == ic::AIRCRAFT_USER_PROGRESS)
        updateAircraftProgressText();

      if(tabHandlerAircraft->getCurrentTabId() == ic::AIRCRAFT_AI)
        updateAiAircraftText();
    }
    lastSimUpdate = QDateTime::currentDateTime().toMSecsSinceEpoch();
  }

  if(atools::almostNotEqual(QDateTime::currentDateTime().toMSecsSinceEpoch(),
                            lastSimBearingUpdate, static_cast<qint64>(MIN_SIM_UPDATE_BEARING_TIME_MS)))
  {
    // Last update was more than a second ago
    if(data.getUserAircraftConst().isValid() && ui->dockWidgetInformation->isVisible())
    {
      if(tabHandlerAirportInfo->getCurrentTabId() == ic::INFO_AIRPORT_OVERVIEW)
        updateAirportInternal(false /* new */, true /* bearing change*/, false /* scroll to top */,
                              false /* force weather update */);

      if(tabHandlerInfo->getCurrentTabId() == ic::INFO_NAVAID)
        updateNavaidInternal(currentSearchResult, true /* bearing changed */, false /* scroll to top */,
                             false /* force update */);

      if(tabHandlerInfo->getCurrentTabId() == ic::INFO_USERPOINT)
        updateUserpointInternal(currentSearchResult, true /* bearing changed */, false /* scroll to top */);
    }
    lastSimBearingUpdate = QDateTime::currentDateTime().toMSecsSinceEpoch();
  }
}

void InfoController::updateAiAirports(const atools::fs::sc::SimConnectData& data)
{
  if(data.getPacketId() > 0)
  {
    // Ignore weather updates
    const QVector<atools::fs::sc::SimConnectAircraft>& newAiAircraft = data.getAiAircraftConst();
    QVector<atools::fs::sc::SimConnectAircraft> newAiAircraftShown;

    // Find all aircraft currently shown on the page in the newly arrived ai list
    for(SimConnectAircraft& aircraft : currentSearchResult.aiAircraft)
    {
      QVector<atools::fs::sc::SimConnectAircraft>::const_iterator it =
        std::find_if(newAiAircraft.begin(), newAiAircraft.end(),
                     [ = ](const SimConnectAircraft& ac) -> bool
      {
        return ac.getObjectId() == aircraft.getObjectId();
      });
      if(it != newAiAircraft.end())
        newAiAircraftShown.append(*it);
    }

    // Overwite old list
    currentSearchResult.aiAircraft = newAiAircraftShown.toList();
  }
}

void InfoController::connectedToSimulator()
{
  qDebug() << Q_FUNC_INFO;
  updateAircraftInfo();
}

void InfoController::disconnectedFromSimulator()
{
  qDebug() << Q_FUNC_INFO;
  lastSimData = atools::fs::sc::SimConnectData();
  lastSimUpdate = 0;
  updateAircraftInfo();
}

void InfoController::updateAircraftInfo()
{
  updateUserAircraftText();
  updateAircraftProgressText();
  updateAiAircraftText();
}

void InfoController::optionsChanged()
{
  updateTextEditFontSizes();
  showInformationInternal(currentSearchResult, map::NONE, false /* Show windows */, false /* scroll to top */,
                          true /* forceUpdate */);
  updateAircraftInfo();
}

/* Update font size in text browsers if options have changed */
void InfoController::updateTextEditFontSizes()
{
  Ui::MainWindow *ui = NavApp::getMainUi();

  int sizePercent = OptionData::instance().getGuiInfoTextSize();
  setTextEditFontSize(ui->textBrowserAirportInfo, infoFontPtSize, sizePercent);
  setTextEditFontSize(ui->textBrowserRunwayInfo, infoFontPtSize, sizePercent);
  setTextEditFontSize(ui->textBrowserComInfo, infoFontPtSize, sizePercent);
  setTextEditFontSize(ui->textBrowserApproachInfo, infoFontPtSize, sizePercent);
  setTextEditFontSize(ui->textBrowserNearestInfo, infoFontPtSize, sizePercent);
  setTextEditFontSize(ui->textBrowserWeatherInfo, infoFontPtSize, sizePercent);
  setTextEditFontSize(ui->textBrowserNavaidInfo, infoFontPtSize, sizePercent);
  setTextEditFontSize(ui->textBrowserUserpointInfo, infoFontPtSize, sizePercent);
  setTextEditFontSize(ui->textBrowserAirspaceInfo, infoFontPtSize, sizePercent);
  setTextEditFontSize(ui->textBrowserLogbookInfo, infoFontPtSize, sizePercent);

  setTextEditFontSize(ui->textBrowserCenterInfo, infoFontPtSize, sizePercent);
  setTextEditFontSize(ui->textBrowserClientInfo, infoFontPtSize, sizePercent);

  sizePercent = OptionData::instance().getGuiInfoSimSize();
  setTextEditFontSize(ui->textBrowserAircraftInfo, simInfoFontPtSize, sizePercent);
  setTextEditFontSize(ui->textBrowserAircraftProgressInfo, simInfoFontPtSize, sizePercent);
  setTextEditFontSize(ui->textBrowserAircraftAiInfo, simInfoFontPtSize, sizePercent);

  // Adjust symbol sizes
  int infoFontPixelSize = ui->textBrowserAirportInfo->fontMetrics().height();
  infoBuilder->setSymbolSize(QSize(infoFontPixelSize, infoFontPixelSize));
  infoBuilder->setSymbolSizeTitle(QSize(infoFontPixelSize, infoFontPixelSize) * 3 / 2);

  int simInfoFontPixelSize = ui->textBrowserAircraftInfo->fontMetrics().height();
  infoBuilder->setSymbolSizeVehicle(QSize(simInfoFontPixelSize, simInfoFontPixelSize) * 3 / 2);
}

/* Set font size in text edit based on percent of original size */
void InfoController::setTextEditFontSize(QTextEdit *textEdit, float origSize, int percent)
{
  QFont f = textEdit->font();
  float newSize = origSize * percent / 100.f;
  if(newSize > 0.1f)
  {
    f.setPointSizeF(newSize);
    textEdit->setFont(f);
  }
}

QStringList InfoController::getAirportTextFull(const QString& ident) const
{
  map::MapAirport airport;
  airportQuery->getAirportByIdent(airport, ident);

  QStringList retval;
  if(airport.isValid())
  {
    map::WeatherContext weatherContext;
    mainWindow->buildWeatherContext(weatherContext, airport);

    atools::util::HtmlBuilder html(mapcolors::webTableBackgroundColor, mapcolors::webTableAltBackgroundColor);
    HtmlInfoBuilder builder(mainWindow, true /*info*/, true /*print*/);
    builder.airportText(airport, weatherContext, html, nullptr);
    retval.append(html.getHtml());

    html.clear();
    builder.runwayText(airport, html);
    retval.append(html.getHtml());

    html.clear();
    builder.comText(airport, html);
    retval.append(html.getHtml());

    html.clear();
    builder.procedureText(airport, html);
    retval.append(html.getHtml());

    html.clear();
    builder.weatherText(weatherContext, airport, html);
    retval.append(html.getHtml());
  }

  return retval;
}

void InfoController::setCurrentInfoTabIndex(ic::TabInfoId tabId)
{
  tabHandlerInfo->setCurrentTab(tabId);
}

void InfoController::setCurrentAirportInfoTabIndex(ic::TabAirportInfoId tabId)
{
  tabHandlerAirportInfo->setCurrentTab(tabId);
}

void InfoController::setCurrentAircraftTabIndex(ic::TabAircraftId tabId)
{
  tabHandlerAircraft->setCurrentTab(tabId);
}

void InfoController::resetWindowLayout()
{
  tabHandlerInfo->reset();
  tabHandlerAirportInfo->reset();
  tabHandlerAircraft->reset();
}
