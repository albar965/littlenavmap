/*****************************************************************************
* Copyright 2015-2024 Alexander Barthel alex@littlenavmap.org
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

#include "airspace/airspacecontroller.h"
#include "app/navapp.h"
#include "atools.h"
#include "common/constants.h"
#include "common/htmlinfobuilder.h"
#include "common/mapcolors.h"
#include "common/maptools.h"
#include "fs/sc/simconnectdata.h"
#include "gui/desktopservices.h"
#include "gui/helphandler.h"
#include "gui/mainwindow.h"
#include "gui/tabwidgethandler.h"
#include "gui/tools.h"
#include "gui/widgetutil.h"
#include "info/aircraftprogressconfig.h"
#include "mapgui/mapwidget.h"
#include "online/onlinedatacontroller.h"
#include "options/optiondata.h"
#include "query/airportquery.h"
#include "query/airwaytrackquery.h"
#include "query/mapquery.h"
#include "route/route.h"
#include "settings/settings.h"
#include "ui_mainwindow.h"
#include "util/htmlbuilder.h"
#include "weather/weathercontext.h"
#include "weather/weathercontexthandler.h"

#include <QUrlQuery>

using atools::util::HtmlBuilder;
using atools::fs::sc::SimConnectAircraft;
using atools::fs::sc::SimConnectUserAircraft;

namespace ahtml = atools::util::html;

InfoController::InfoController(MainWindow *parent)
  : QObject(parent), mainWindow(parent)
{
  lastSimData = new atools::fs::sc::SimConnectData;
  currentSearchResult = new map::MapResult;
  savedSearchResult = new map::MapResult;

  mapQuery = NavApp::getMapQueryGui();
  airportQuery = NavApp::getAirportQuerySim();

  airspaceController = NavApp::getAirspaceController();

  infoBuilder = new HtmlInfoBuilder(parent->getMapWidget(), true);

  // Get base font size for widgets
  Ui::MainWindow *ui = NavApp::getMainUi();

  QPushButton *pushButtonInfoHelp = new QPushButton(QIcon(":/littlenavmap/resources/icons/help.svg"), QString(), ui->tabWidgetInformation);
  pushButtonInfoHelp->setToolTip(tr("Show help for the information window"));
  pushButtonInfoHelp->setStatusTip(tr("Show help for the information window"));

  tabHandlerInfo = new atools::gui::TabWidgetHandler(ui->tabWidgetInformation, {pushButtonInfoHelp},
                                                     QIcon(":/littlenavmap/resources/icons/tabbutton.svg"),
                                                     tr("Open or close tabs"));
  tabHandlerInfo->init(ic::TabInfoIds, lnm::INFOWINDOW_WIDGET_TABS);

  tabHandlerAirportInfo = new atools::gui::TabWidgetHandler(ui->tabWidgetAirport, {},
                                                            QIcon(":/littlenavmap/resources/icons/tabbutton.svg"),
                                                            tr("Open or close tabs"));
  tabHandlerAirportInfo->init(ic::TabAirportInfoIds, lnm::INFOWINDOW_WIDGET_AIRPORT_TABS);

  QPushButton *pushButtonAircraftHelp = new QPushButton(QIcon(":/littlenavmap/resources/icons/help.svg"), QString(), ui->tabWidgetAircraft);
  pushButtonAircraftHelp->setToolTip(tr("Show help for the aircraft window"));
  pushButtonAircraftHelp->setStatusTip(tr("Show help for the aircraft window"));

  tabHandlerAircraft = new atools::gui::TabWidgetHandler(ui->tabWidgetAircraft, {pushButtonAircraftHelp},
                                                         QIcon(":/littlenavmap/resources/icons/tabbutton.svg"),
                                                         tr("Open or close tabs"));
  tabHandlerAircraft->init(ic::TabAircraftIds, lnm::INFOWINDOW_WIDGET_AIRCRAFT_TABS);

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

  // Inactive texts, tooltips and placeholders
  waitingForUpdateText = tr("Little Navmap is connected to %1.\n\n"
                            "Prepare the flight and load your aircraft in the simulator to see progress updates.");

  notConnectedText = tr("Not connected to simulator.\n\n"
                        "Go to the main menu -> \"Tools\" -> \"Connect to Flight Simulator\" "
                        "or press \"Ctrl+Shift+C\".\n"
                        "Then select the simulator and click \"Connect\".\n",
                        "Keep instructions in sync with translated menus and shortcuts");

  aircraftProgressConfig = new AircraftProgressConfig(mainWindow);

  // ==================================================================================
  // Create a configuration push button and place it into the aircraft progress info text browser
  QPushButton *button = new QPushButton(QIcon(":/littlenavmap/resources/icons/settingsroute.svg"),
                                        QString(), ui->textBrowserAircraftProgressInfo->viewport());
  button->setToolTip(tr("Select the fields to show in the aircraft progress tab."));
  button->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));

  // Create a layout to position the button automatically
  QVBoxLayout *layout = new QVBoxLayout(ui->textBrowserAircraftProgressInfo->viewport());
  layout->setMargin(5);
  layout->setSpacing(0);
  layout->addWidget(button, 0, Qt::AlignRight); // Add button to the right
  layout->addSpacerItem(new QSpacerItem(10, 10, QSizePolicy::Minimum, QSizePolicy::Expanding)); // Move button up with spacer
  connect(button, &QPushButton::clicked, this, &InfoController::progressConfigurationClicked);

  // Create context menu connections for progress text browser ===========================
  ui->textBrowserAircraftProgressInfo->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(ui->textBrowserAircraftProgressInfo, &QTextBrowser::customContextMenuRequested, this, &InfoController::showProgressContextMenu);
  connect(ui->actionInfoDisplayOptions, &QAction::triggered, this, &InfoController::progressConfigurationClicked);

  // ==================================================================================
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

  connect(tabHandlerAircraft, &atools::gui::TabWidgetHandler::tabChanged, this, &InfoController::currentAircraftTabChanged);
  connect(tabHandlerInfo, &atools::gui::TabWidgetHandler::tabChanged, this, &InfoController::currentInfoTabChanged);
  connect(tabHandlerAirportInfo, &atools::gui::TabWidgetHandler::tabChanged, this, &InfoController::currentAirportInfoTabChanged);

  connect(ui->dockWidgetAircraft, &QDockWidget::visibilityChanged, this, &InfoController::visibilityChangedAircraft);
  connect(ui->dockWidgetInformation, &QDockWidget::visibilityChanged, this, &InfoController::visibilityChangedInfo);

  connect(pushButtonInfoHelp, &QPushButton::clicked, this, &InfoController::helpInfoClicked);
  connect(pushButtonAircraftHelp, &QPushButton::clicked, this, &InfoController::helpAircraftClicked);
}

void InfoController::helpInfoClicked()
{
  // https://www.littlenavmap.org/manuals/littlenavmap/release/latest/en/INFO.html
  atools::gui::HelpHandler::openHelpUrlWeb(
    NavApp::getMainUi()->dockWidgetInformation, lnm::helpOnlineUrl + "INFO.html", lnm::helpLanguageOnline());
}

void InfoController::helpAircraftClicked()
{
  atools::gui::HelpHandler::openHelpUrlWeb(
    NavApp::getMainUi()->dockWidgetAircraft, lnm::helpOnlineUrl + "INFO.html#simulator-aircraft-dock-window", lnm::helpLanguageOnline());
}

InfoController::~InfoController()
{
  ATOOLS_DELETE_LOG(aircraftProgressConfig);
  ATOOLS_DELETE_LOG(tabHandlerInfo);
  ATOOLS_DELETE_LOG(tabHandlerAirportInfo);
  ATOOLS_DELETE_LOG(tabHandlerAircraft);
  ATOOLS_DELETE_LOG(infoBuilder);
  ATOOLS_DELETE_LOG(lastSimData);
  ATOOLS_DELETE_LOG(currentSearchResult);
  ATOOLS_DELETE_LOG(savedSearchResult);
}

QString InfoController::getConnectionTypeText()
{
  if(NavApp::isNetworkConnect())
    return tr("Little Navconnect");
  else if(NavApp::isXpConnect())
    return tr("X-Plane");
  else if(NavApp::isSimConnect())
#if defined(SIMCONNECT_BUILD_WIN64)
    return tr("MSFS");

#elif defined(SIMCONNECT_BUILD_WIN32)
    return tr("FSX or P3D");

#else
    return tr("FSX, P3D or MSFS");

#endif

  return QString();
}

void InfoController::showProgressContextMenu(const QPoint& point)
{
  Ui::MainWindow *ui = NavApp::getMainUi();

  QMenu *menu = ui->textBrowserAircraftProgressInfo->createStandardContextMenu();
  menu->addAction(ui->actionInfoDisplayOptions);
  menu->exec(ui->textBrowserAircraftProgressInfo->mapToGlobal(point));
  delete menu;
}

void InfoController::visibilityChangedAircraft(bool visible)
{
  // Avoid spurious events that appear on shutdown and cause crashes
  if(!atools::gui::Application::isShuttingDown() && visible)
    currentAircraftTabChanged(tabHandlerAircraft->getCurrentTabId());
}

void InfoController::visibilityChangedInfo(bool visible)
{
  // Avoid spurious events that appear on shutdown and cause crashes
  if(!atools::gui::Application::isShuttingDown() && visible)
  {
    currentInfoTabChanged(tabHandlerInfo->getCurrentTabId());
    currentAirportInfoTabChanged(tabHandlerAirportInfo->getCurrentTabId());
  }
}

void InfoController::currentAircraftTabChanged(int id)
{
  // Avoid spurious events that appear on shutdown and cause crashes
  if(!atools::gui::Application::isShuttingDown())
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
      updateNavaidInternal(*currentSearchResult, true /* bearing changed */, false /* scrollToTop */, false /* forceUpdate */);
      break;

    case ic::INFO_USERPOINT:
      updateUserpointInternal(*currentSearchResult, true /* bearing changed */, false /* scrollToTop */);
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
      updateAirportInternal(false /* new */, true /* bearing changed */, false /* scrollToTop */, false /* forceWeatherUpdate */);
      break;

    case ic::INFO_AIRPORT_RUNWAYS:
    case ic::INFO_AIRPORT_COM:
    case ic::INFO_AIRPORT_APPROACHES:
    case ic::INFO_AIRPORT_NEAREST:
    case ic::INFO_AIRPORT_WEATHER:
      break;
  }
}

void InfoController::progressConfigurationClicked()
{
  qDebug() << Q_FUNC_INFO;
  aircraftProgressConfig->progressConfiguration();
  aircraftProgressConfig->saveState();
  updateProgress();
}

void InfoController::anchorClicked(const QUrl& url)
{
  qDebug() << Q_FUNC_INFO << url;

  if(url.scheme() == "http" || url.scheme() == "https" || url.scheme() == "ftp" || url.scheme() == "file")
    // Open a normal link or file from the userpoint description
    atools::gui::DesktopServices::openUrl(mainWindow, url);
  else if(url.scheme() == "lnm")
  {
    // Internal link like "show on map"
    QUrlQuery query(url);
    MapWidget *mapWidget = NavApp::getMapWidgetGui();

    map::MapTypes type(map::NONE);
    int id = -1;
    if(query.hasQueryItem("type"))
      type = query.queryItemValue("type").toULongLong();
    if(query.hasQueryItem("id"))
      id = query.queryItemValue("id").toInt();

    if(url.host() == "do")
    {
      // "lnm://do?hideairspaces"
      if(query.hasQueryItem("hideairspaces") || query.hasQueryItem("hideonlineairspaces"))
      {
        // Hide normal airspace highlights from information window =========================================
        mapWidget->clearAirspaceHighlights();
        mainWindow->updateHighlightActionStates();
      }
      else if(query.hasQueryItem("hideairways"))
      {
        // Hide airway highlights from information window =========================================
        mapWidget->clearAirwayHighlights();
        mainWindow->updateHighlightActionStates();
      }
    }
    else if(url.host() == "info")
    {
      // "lnm://info?id=%1&type=%2"
      if(id != -1 && type != map::NONE)
      {
        map::MapResult result;
        mapQuery->getMapObjectById(result, type, map::MapAirspaceSources(), id, false /* airportFromNavDatabase */);
        showInformation(result);
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
        float distanceKm = 0.f; // Default is set by user for no doubleClick
        if(query.hasQueryItem("distance"))
          distanceKm = query.queryItemValue("distance").toFloat();

        emit showPos(atools::geo::Pos(query.queryItemValue("lonx"), query.queryItemValue("laty")), distanceKm, false /* doubleClick */);
      }
      else if(id != -1 && type != map::NONE)
      {
        // Zoom to an map object =========================================
        if(type == map::AIRPORT)
          // Show airport by id ================================================
          emit showRect(airportQuery->getAirportById(id).bounding, false);
        else if(type == map::AIRSPACE)
        {
          // Show airspaces by id and source ================================================
          map::MapAirspaceSources src(query.queryItemValue("source").toInt());

          // Append airspace to current highlight list if not already present
          map::MapAirspace airspace = airspaceController->getAirspaceById({id, src});

          QList<map::MapAirspace> airspaceHighlights = mapWidget->getAirspaceHighlights();
          if(!maptools::containsId(airspaceHighlights, airspace.id))
            airspaceHighlights.append(airspace);
          mapWidget->changeAirspaceHighlights(airspaceHighlights);

          mainWindow->updateHighlightActionStates();
          emit showRect(airspace.bounding, false);
        }
        else if(type == map::AIRWAY)
        {
          // Show full airways by id ================================================
          map::MapAirway airway = NavApp::getAirwayTrackQueryGui()->getAirwayById(id);

          // Get all airway segments and the bounding rectangle
          atools::geo::Rect bounding;
          QList<map::MapAirway> airways;
          NavApp::getAirwayTrackQueryGui()->getAirwayFull(airways, bounding, airway.name, airway.fragment);

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
        atools::geo::Pos pos;

        if(query.hasQueryItem("aplonx") && query.hasQueryItem("aplaty"))
          pos = atools::geo::Pos(query.queryItemValue("aplonx"), query.queryItemValue("aplaty"));

        QList<map::MapAirport> airports = airportQuery->getAirportsByOfficialIdent(query.queryItemValue("airport"),
                                                                                   pos.isValid() ? &pos : nullptr);

        if(!airports.isEmpty())
          emit showRect(airports.constFirst().bounding, false);
        else
          qWarning() << Q_FUNC_INFO << "No airport found for" << query.queryItemValue("airport");
      }
      else if(query.hasQueryItem("filepath"))
        // Show path in any OS dependent file manager. Selects the file in Windows Explorer.
        atools::gui::DesktopServices::openFile(mainWindow, query.queryItemValue("filepath"), true /* showInFileManager */);
      else
        qWarning() << Q_FUNC_INFO << "Unknwown URL" << url;
    }
    else if(url.host() == "showprocsdepart" && id != -1 && type != map::NONE)
      emit showProcedures(airportQuery->getAirportById(id), true /* departureFilter */, false /* arrivalFilter */);
    else if(url.host() == "showprocsarrival" && id != -1 && type != map::NONE)
      emit showProcedures(airportQuery->getAirportById(id), false /* departureFilter */, true /* arrivalFilter */);
    else if(url.host() == "showprocs" && id != -1 && type != map::NONE)
      emit showProcedures(airportQuery->getAirportById(id), false /* departureFilter */, false /* arrivalFilter */);
  }

  // Remember clicked text edit to clear the unwanted selection on next update when calling updateTextEdit()
  QTextEdit *textEdit = dynamic_cast<QTextEdit *>(sender());
  if(textEdit != nullptr)
    anchorsClicked.insert(textEdit);
}

void InfoController::saveState() const
{
  // Store currently shown map objects in a string list containing id and type
  map::MapRefVector refs;
  for(const map::MapAirport& airport  : qAsConst(currentSearchResult->airports))
    refs.append({airport.id, map::AIRPORT});

  for(const map::MapAirportMsa& msa  : qAsConst(currentSearchResult->airportMsa))
    refs.append({msa.id, map::AIRPORT_MSA});

  for(const map::MapVor& vor : qAsConst(currentSearchResult->vors))
    refs.append({vor.id, map::VOR});

  for(const map::MapNdb& ndb : qAsConst(currentSearchResult->ndbs))
    refs.append({ndb.id, map::NDB});

  for(const map::MapWaypoint& waypoint : qAsConst(currentSearchResult->waypoints))
    refs.append({waypoint.id, map::WAYPOINT});

  for(const map::MapIls& ils : qAsConst(currentSearchResult->ils))
    refs.append({ils.id, map::ILS});

  for(const map::MapHolding& holding : qAsConst(currentSearchResult->holdings))
    refs.append({holding.id, map::HOLDING});

  for(const map::MapUserpoint& userpoint: qAsConst(currentSearchResult->userpoints))
    refs.append({userpoint.id, map::USERPOINT});

  for(const map::MapLogbookEntry& logEntry : qAsConst(currentSearchResult->logbookEntries))
    refs.append({logEntry.id, map::LOGBOOK});

  for(const map::MapAirway& airway : qAsConst(currentSearchResult->airways))
    refs.append({airway.id, map::AIRWAY});

  // Save list =====================================================
  atools::settings::Settings& settings = atools::settings::Settings::instance();
  QStringList refList;
  for(const map::MapRef& ref : refs)
    refList.append(QString("%1;%2").arg(ref.id).arg(ref.objType));
  settings.setValue(lnm::INFOWINDOW_CURRENTMAPOBJECTS, refList.join(";"));

  // Save airspaces =====================================================
  refList.clear();
  for(const map::MapAirspace& airspace : qAsConst(currentSearchResult->airspaces))
  {
    // Do not save online airspace ids since they will change on next startup
    if(!airspace.isOnline())
      refList.append(QString("%1;%2").arg(airspace.id).arg(airspace.src));
  }
  settings.setValue(lnm::INFOWINDOW_CURRENTAIRSPACES, refList.join(";"));

  tabHandlerInfo->saveState();
  tabHandlerAirportInfo->saveState();
  tabHandlerAircraft->saveState();
  aircraftProgressConfig->saveState();
}

void InfoController::restoreState()
{
  tabHandlerInfo->restoreState();
  tabHandlerAirportInfo->restoreState();
  tabHandlerAircraft->restoreState();
  aircraftProgressConfig->restoreState();

  updateTextEditFontSizes();
  updateAircraftInfo();
}

void InfoController::restoreInformation()
{
  if(OptionData::instance().getFlags() & opts::STARTUP_LOAD_INFO && !NavApp::isSafeMode())
  {
    // Go through the string and collect all objects in the MapSearchResult
    map::MapResult res;

    // All objects =================================
    QString refsStr = atools::settings::Settings::instance().valueStr(lnm::INFOWINDOW_CURRENTMAPOBJECTS);
    QStringList refsStrList = refsStr.split(";", QString::SkipEmptyParts);
    for(int i = 0; i < refsStrList.size(); i += 2)
      mapQuery->getMapObjectById(res, map::MapTypes(refsStrList.value(i + 1).toULongLong()), map::AIRSPACE_SRC_NONE,
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

    // Remove wrong objects
    res.removeInvalid();

    showInformationInternal(res, false /* showWindows */, false /* scrollToTop */, true /* forceUpdate */);
  }
}

void InfoController::updateAirport()
{
  updateAirportInternal(false /* new */, false /* bearing change*/, false /* scrollToTop */, false /* forceWeatherUpdate */);
}

void InfoController::updateAirportWeather()
{
  updateAirportInternal(false /* new */, false /* bearing change*/, false /* scrollToTop */, true /* forceWeatherUpdate */);
}

void InfoController::routeChanged(bool, bool)
{
  updateAirportInternal(false /* new */, true /* bearing change*/, false /* scrollToTop */, false /* forceWeatherUpdate */);
}

void InfoController::updateProgress()
{
  HtmlBuilder html(true /* has background color */);
  Ui::MainWindow *ui = NavApp::getMainUi();

  if(atools::gui::util::canTextEditUpdate(ui->textBrowserAircraftProgressInfo))
  {
    // ok - scrollbars not pressed
    html.clear();
    html.setIdBits(aircraftProgressConfig->getEnabledBits());
    infoBuilder->aircraftProgressText(lastSimData->getUserAircraftConst(), html, NavApp::getRouteConst());
    updateTextEdit(ui->textBrowserAircraftProgressInfo, html.getHtml(), false /* scrollToTop*/, true /* keepSelection */);
  }
}

void InfoController::updateAirportInternal(bool newAirport, bool bearingChange, bool scrollToTop, bool forceWeatherUpdate)
{
  if(databaseLoadStatus)
    return;

  if(currentSearchResult->hasAirports())
  {
    map::WeatherContext currentWeatherContext;
    bool weatherChanged = NavApp::getWeatherContextHandler()->buildWeatherContextInfoFull(currentWeatherContext,
                                                                                          currentSearchResult->airports.constFirst());

    // qDebug() << Q_FUNC_INFO << "newAirport" << newAirport << "weatherChanged" << weatherChanged
    // << "ident" << currentWeatherContext.ident;

    if(newAirport || weatherChanged || bearingChange || forceWeatherUpdate)
    {
      HtmlBuilder html(true);
      map::MapAirport airport;
      airportQuery->getAirportById(airport, currentSearchResult->airports.constFirst().id);

      // qDebug() << Q_FUNC_INFO << "Updating html" << airport.ident << airport.id;

      infoBuilder->airportText(airport, currentWeatherContext, html, &NavApp::getRouteConst());

      Ui::MainWindow *ui = NavApp::getMainUi();
      // Leave position for weather or bearing updates
      updateTextEdit(ui->textBrowserAirportInfo, html.getHtml(), scrollToTop, !scrollToTop /* keepSelection */);

      if(newAirport || weatherChanged || forceWeatherUpdate)
      {
        html.clear();
        infoBuilder->weatherText(currentWeatherContext, airport, html);

        // Leave position for weather updates
        updateTextEdit(ui->textBrowserWeatherInfo, html.getHtml(), scrollToTop, !scrollToTop /* keepSelection */);
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

void InfoController::showInformation(map::MapResult result)
{
  showInformationInternal(result, true /* showWindows */, true /* scrollToTop */, false /* forceUpdate */);
}

void InfoController::updateAllInformation()
{
  showInformationInternal(*currentSearchResult, false /* showWindows */, false /* scrollToTop */, false /* forceUpdate */);
}

void InfoController::onlineNetworkChanged()
{
  // Clear display
  NavApp::getMainUi()->textBrowserCenterInfo->clear();

  // Remove all online network airspaces from current result
  QList<map::MapAirspace> airspaces;
  for(const map::MapAirspace& airspace : qAsConst(currentSearchResult->airspaces))
    if(!airspace.isOnline())
      airspaces.append(airspace);
  currentSearchResult->airspaces = airspaces;

  showInformationInternal(*currentSearchResult, false /* showWindows */, false /* scrollToTop */, true /* forceUpdate */);
}

void InfoController::onlineClientAndAtcUpdated()
{
  showInformationInternal(*currentSearchResult, false /* showWindows */, false /* scrollToTop */, true /* forceUpdate */);
}

/* Show information in all tabs but do not show dock */
void InfoController::showInformationInternal(map::MapResult result, bool showWindows, bool scrollToTop, bool forceUpdate)
{
#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << "======================================================";
  qDebug() << Q_FUNC_INFO << "result" << result;
  qDebug() << Q_FUNC_INFO << "currentSearchResult" << *currentSearchResult;
#endif

  // Flags used to decide which tab and window to raise
  bool foundAirport = false, foundNavaid = false, foundUserpoint = false, foundUserAircraft = false, foundUserAircraftShadow = false,
       foundAiAircraft = false, foundOnlineClient = false, foundAirspace = false, foundLogbookEntry = false, foundOnlineCenter = false;

  HtmlBuilder html(true);

  Ui::MainWindow *ui = NavApp::getMainUi();
  OnlinedataController *onlineDataController = NavApp::getOnlinedataController();

  // Check for shadowed user aircraft ======================================
  currentSearchResult->userAircraft = result.userAircraft;
  foundUserAircraft = currentSearchResult->userAircraft.isValid();
  if(foundUserAircraft)
    foundUserAircraftShadow = currentSearchResult->userAircraft.getAircraft().isOnlineShadow();

  // Filter online and AI aircraft with shadows ============================================================
  if(!result.aiAircraft.isEmpty() || !result.onlineAircraft.isEmpty())
  {
    QList<map::MapAiAircraft> aiAircraftList;
    QList<map::MapOnlineAircraft> onlineAircraftList;
    QSet<int> onlineIds;

    // Get shadowed online aircraft from AI shadows ====================
    for(const map::MapAiAircraft& mapAiAircraft : qAsConst(result.aiAircraft))
    {
      atools::fs::sc::SimConnectAircraft onlineAircraft = onlineDataController->getShadowedOnlineAircraft(mapAiAircraft.getAircraft());

      if(onlineAircraft.isValid())
      {
        onlineAircraftList.append(map::MapOnlineAircraft(onlineAircraft));
        onlineIds.insert(onlineAircraft.getId());
      }
      aiAircraftList.append(mapAiAircraft);
    }

    // Add present online aircraft which are not already added by shadow above ================
    for(const map::MapOnlineAircraft& mapOnlineAircraft : qAsConst(result.onlineAircraft))
    {
      if(!onlineIds.contains(mapOnlineAircraft.getId()))
        onlineAircraftList.append(map::MapOnlineAircraft(mapOnlineAircraft));
    }

    if(!onlineAircraftList.isEmpty())
      // Copy to result for online display below
      result.onlineAircraft = onlineAircraftList;

    if(!aiAircraftList.isEmpty())
    {
      // Copy to current result for updateAiAircraftText()
      currentSearchResult->aiAircraft = aiAircraftList;
      foundAiAircraft = true;
    }
  }

#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << "result" << result;
  qDebug() << Q_FUNC_INFO << "currentSearchResult" << *currentSearchResult;
#endif

  // Check showWindows which indicates info click - update only once here
  if(foundAiAircraft && showWindows)
    // Uses currentSearchResult.aiAircraft
    updateAiAircraftText();

  // Online aircraft and user aircraft shadow ================================================================
  if(foundUserAircraftShadow || !result.onlineAircraft.isEmpty())
  {
    // User aircraft shadow ================================
    int num = 1;
    if(foundUserAircraftShadow)
    {
      SimConnectAircraft ac = onlineDataController->getShadowedOnlineAircraft(currentSearchResult->userAircraft.getAircraft());
      if(ac.isValid())
      {
        infoBuilder->aircraftText(ac, html, num++, onlineDataController->getNumClients());
        infoBuilder->aircraftProgressText(ac, html, Route());
        infoBuilder->aircraftOnlineText(ac, onlineDataController->getClientRecordById(ac.getId()), html);

        // Clear online aircraft to avoid them returning to display
        result.onlineAircraft.clear();
        currentSearchResult->onlineAircraft.clear();
        currentSearchResult->onlineAircraftIds.clear();
      }
    }

    // Online aircraft ================================
    if(!result.onlineAircraft.isEmpty())
    {
      // Clear current result and add only shown aircraft
      currentSearchResult->onlineAircraft.clear();
      currentSearchResult->onlineAircraftIds.clear();

      for(const map::MapOnlineAircraft& mapOnlineAircraft : qAsConst(result.onlineAircraft))
      {
        atools::fs::sc::SimConnectAircraft ac = onlineDataController->getClientAircraftById(mapOnlineAircraft.getId());

        if(!ac.isValid())
        {
          // Not found - use aircraft from iterator
          ac = mapOnlineAircraft.getAircraft();
          qWarning() << Q_FUNC_INFO << "Online aircraft not found"
                     << mapOnlineAircraft.getId() << mapOnlineAircraft.getAircraft().getAirplaneRegistration();
        }

        infoBuilder->aircraftText(ac, html, num++, onlineDataController->getNumClients());
        infoBuilder->aircraftProgressText(ac, html, Route());
        infoBuilder->aircraftOnlineText(ac, onlineDataController->getClientRecordById(ac.getId()), html);
        currentSearchResult->onlineAircraft.append(map::MapOnlineAircraft(ac));
      }
    }
    updateTextEdit(ui->textBrowserClientInfo, html.getHtml(), false /* scrollToTop*/, true /* keepSelection */);

    // User or AI shadowed
    foundOnlineClient = true;
  }

  // Airport ================================================================
  if(!result.airports.isEmpty())
  {
#ifdef DEBUG_INFORMATION
    qDebug() << "Found airport" << result.airports.constFirst().ident;
#endif

    // Only one airport shown - have to make a copy here since currentSearchResult might be equal to result
    // when updating
    map::MapAirport airport = result.airports.constFirst();

    bool changed = !currentSearchResult->hasAirports() || currentSearchResult->airports.constFirst().id != airport.id;

    currentSearchResult->airports.clear();
    currentSearchResult->airportIds.clear();
    // Remember one airport
    currentSearchResult->airports.append(airport);

    // Update parts that are variable because of weather or bearing changes ==================
    updateAirportInternal(true /* new */, false /* bearing change*/, scrollToTop, false /* forceWeatherUpdate */);

    if(changed || forceUpdate)
    {
      // Update parts that have now weather or bearing depenedency =====================
      html.clear();
      infoBuilder->runwayText(airport, html);
      updateTextEdit(ui->textBrowserRunwayInfo, html.getHtml(), scrollToTop, !scrollToTop /* keepSelection */);

      html.clear();
      infoBuilder->comText(airport, html);
      updateTextEdit(ui->textBrowserComInfo, html.getHtml(), scrollToTop, !scrollToTop /* keepSelection */);

      html.clear();
      infoBuilder->procedureText(airport, html);
      updateTextEdit(ui->textBrowserApproachInfo, html.getHtml(), scrollToTop, !scrollToTop /* keepSelection */);

      html.clear();
      infoBuilder->nearestText(airport, html);
      updateTextEdit(ui->textBrowserNearestInfo, html.getHtml(), scrollToTop, !scrollToTop /* keepSelection */);
    }

    foundAirport = true;
  }

  // Airspaces ================================================================
  if(!result.airspaces.isEmpty())
  {
    atools::sql::SqlRecord onlineRec;

    if(result.hasSimNavUserAirspaces())
      currentSearchResult->clearNavdataAirspaces();

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
#ifdef DEBUG_INFORMATION
      qDebug() << "Found airspace" << airspace.id;
#endif
      currentSearchResult->airspaces.append(airspace);
      infoBuilder->airspaceText(airspace, onlineRec, html);
      html.br();
      foundAirspace = true;
    }

    if(foundAirspace)
      // Update and keep scroll position
      updateTextEdit(ui->textBrowserAirspaceInfo, html.getHtml(), false /* scrollToTop*/, true /* keepSelection */);

    // Online Center ==================================
    if(result.hasOnlineAirspaces())
      currentSearchResult->clearOnlineAirspaces();

    html.clear();

    // Delete all airspaces that were removed from the database inbetween
    QList<map::MapAirspace> onlineAirspaces = result.getOnlineAirspaces();
    QList<map::MapAirspace>::iterator it = std::remove_if(onlineAirspaces.begin(), onlineAirspaces.end(),
                                                          [this](const map::MapAirspace& airspace) -> bool {
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

    for(const map::MapAirspace& airspace : qAsConst(onlineAirspaces))
    {
#ifdef DEBUG_INFORMATION
      qDebug() << "Found airspace" << airspace.id;
#endif
      currentSearchResult->airspaces.append(airspace);

      // Get extra information for online network ATC
      if(airspace.isOnline())
        onlineRec = airspaceController->getOnlineAirspaceRecordById(airspace.id);

      infoBuilder->airspaceText(airspace, onlineRec, html);
      html.br();
      foundOnlineCenter = true;
    }

    if(foundOnlineCenter)
      // Update and keep scroll position
      updateTextEdit(ui->textBrowserCenterInfo, html.getHtml(), false /* scrollToTop*/, true /* keepSelection */);
  }

  // Logbook Entries ================================================================
  if(!result.logbookEntries.isEmpty())
  {
    html.clear();

    currentSearchResult->logbookEntries.clear();

    for(const map::MapLogbookEntry& logEntry : qAsConst(result.logbookEntries))
    {
      qDebug() << "Found log entry" << logEntry.id;

      currentSearchResult->logbookEntries.append(logEntry);
      foundLogbookEntry |= infoBuilder->logEntryText(logEntry, html);
      html.br();
    }

    if(foundLogbookEntry)
      // Update and keep scroll position
      updateTextEdit(ui->textBrowserLogbookInfo, html.getHtml(), false /* scrollToTop*/, true /* keepSelection */);
    else
      ui->textBrowserLogbookInfo->clear();

  }

  // Navaids tab ================================================================
  if(result.hasVor() || result.hasNdb() || result.hasWaypoints() || result.hasIls() || result.hasHoldings() || result.hasAirportMsa() ||
     result.hasAirways())
    // if any navaids are to be shown clear search result before
    currentSearchResult->clear(map::NAV_ALL | map::ILS | map::AIRWAY | map::RUNWAYEND | map::HOLDING | map::AIRPORT_MSA);

  if(!result.userpoints.isEmpty())
    // if any userpoints are to be shown clear search result before
    currentSearchResult->clear(map::USERPOINT);

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

    bool onlineClient = foundOnlineClient || foundUserAircraftShadow;

    // Decide which tab to activate automatically so that it tries to keep the current tab in front
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

template<typename TYPE>
void InfoController::buildOneNavaid(atools::util::HtmlBuilder& html, bool& bearingChanged, bool& foundNavaid, const QList<TYPE>& list,
                                    QList<TYPE>& currentList, const HtmlInfoBuilder *info,
                                    void (HtmlInfoBuilder::*func)(const TYPE&, atools::util::HtmlBuilder&) const) const
{
  for(const TYPE& type : list)
  {
#ifdef DEBUG_INFORMATION
    qDebug() << "Found" << type.getIdent();
#endif

    if(!bearingChanged)
      currentList.append(type);
    (info->*func)(type, html);
    html.br();
    foundNavaid = true;
  }
}

bool InfoController::updateNavaidInternal(const map::MapResult& result, bool bearingChanged, bool scrollToTop, bool forceUpdate)
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

  buildOneNavaid(html, bearingChanged, foundNavaid, result.vors, currentSearchResult->vors, infoBuilder, &HtmlInfoBuilder::vorText);
  buildOneNavaid(html, bearingChanged, foundNavaid, result.ndbs, currentSearchResult->ndbs, infoBuilder, &HtmlInfoBuilder::ndbText);
  buildOneNavaid(html, bearingChanged, foundNavaid, result.waypoints, currentSearchResult->waypoints, infoBuilder,
                 &HtmlInfoBuilder::waypointText);
  buildOneNavaid(html, bearingChanged, foundNavaid, result.ils, currentSearchResult->ils, infoBuilder, &HtmlInfoBuilder::ilsTextInfo);
  buildOneNavaid(html, bearingChanged, foundNavaid, result.holdings, currentSearchResult->holdings, infoBuilder,
                 &HtmlInfoBuilder::holdingText);
  buildOneNavaid(html, bearingChanged, foundNavaid, result.airportMsa, currentSearchResult->airportMsa, infoBuilder,
                 &HtmlInfoBuilder::airportMsaText);
  buildOneNavaid(html, bearingChanged, foundNavaid, result.airways, currentSearchResult->airways, infoBuilder,
                 &HtmlInfoBuilder::airwayText);

  if(!foundNavaid)
    html.clear();

  if(foundNavaid || forceUpdate)
    updateTextEdit(ui->textBrowserNavaidInfo, html.getHtml(), scrollToTop, !scrollToTop /* keepSelection */);

  return foundNavaid;
}

bool InfoController::updateUserpointInternal(const map::MapResult& result, bool bearingChanged, bool scrollToTop)
{
  HtmlBuilder html(true);
  Ui::MainWindow *ui = NavApp::getMainUi();
  bool foundUserpoint = false;

  // Userpoints on top of the list
  for(const map::MapUserpoint& userpoint : qAsConst(result.userpoints))
  {
#ifdef DEBUG_INFORMATION
    qDebug() << "Found waypoint" << userpoint.ident;
#endif
    if(!bearingChanged)
      currentSearchResult->userpoints.append(userpoint);
    foundUserpoint |= infoBuilder->userpointText(userpoint, html);
    html.br();
  }

  if(foundUserpoint)
    updateTextEdit(ui->textBrowserUserpointInfo, html.getHtml(), scrollToTop, !scrollToTop /* keepSelection */);
  else
    ui->textBrowserUserpointInfo->clear();

  return foundUserpoint;
}

void InfoController::preDatabaseLoad()
{
  // Save current result to be able to reload airport, VORs, NDBs and waypoints by name after switch
  *savedSearchResult = *currentSearchResult;

  // Clear current airport and navaids result
  *currentSearchResult = map::MapResult();
  databaseLoadStatus = true;
  clearInfoTextBrowsers();
}

void InfoController::postDatabaseLoad()
{
  databaseLoadStatus = false;
  updateAircraftInfo();

  // Reload airport by ident and position =====================================
  if(savedSearchResult->hasAirports())
    currentSearchResult->airports.append(NavApp::getAirportQuerySim()->getAirportFuzzy(savedSearchResult->airports.constFirst()));

  // Reload navaids by ident, region and position ===================================
  // Insert only the first one for each getMapObjectByIdent() query
  for(const map::MapWaypoint& waypoint : qAsConst(savedSearchResult->waypoints))
  {
    map::MapResult result;
    mapQuery->getMapObjectByIdent(result, map::WAYPOINT, waypoint.ident, waypoint.region, QString(), waypoint.position, 50);
    if(result.hasWaypoints())
      currentSearchResult->waypoints.append(result.waypoints.constFirst());
  }

  for(const map::MapVor& vor : qAsConst(savedSearchResult->vors))
  {
    map::MapResult result;
    mapQuery->getMapObjectByIdent(result, map::VOR, vor.ident, vor.region, QString(), vor.position, 50);
    if(result.hasVor())
      currentSearchResult->vors.append(result.vors.constFirst());
  }

  for(const map::MapNdb& ndb : qAsConst(savedSearchResult->ndbs))
  {
    map::MapResult result;
    mapQuery->getMapObjectByIdent(result, map::NDB, ndb.ident, ndb.region, QString(), ndb.position, 50);
    if(result.hasNdb())
      currentSearchResult->ndbs.append(result.ndbs.constFirst());
  }

  // Reload all features which are not bound to a database ID ==================================
  currentSearchResult->userpoints.append(savedSearchResult->userpoints);
  currentSearchResult->logbookEntries.append(savedSearchResult->logbookEntries);
  currentSearchResult->userAircraft = savedSearchResult->userAircraft;
  currentSearchResult->aiAircraft.append(savedSearchResult->aiAircraft);
  currentSearchResult->onlineAircraft.append(savedSearchResult->onlineAircraft);

  currentSearchResult->removeInvalid();

  savedSearchResult->clear();
  showInformationInternal(*currentSearchResult, false /* showWindows */, false /* scrollToTop */, true /* forceUpdate */);
}

void InfoController::styleChanged()
{
  tabHandlerInfo->styleChanged();
  tabHandlerAirportInfo->styleChanged();
  tabHandlerAircraft->styleChanged();
  showInformationInternal(*currentSearchResult, false /* showWindows */, false /* scrollToTop */, true /* forceUpdate */);
}

void InfoController::tracksChanged()
{
  // Remove tracks from current result since the ids might change
  currentSearchResult->airways.erase(std::remove_if(currentSearchResult->airways.begin(), currentSearchResult->airways.end(),
                                                    [](const map::MapAirway& airway) -> bool {
    return airway.isTrack();
  }), currentSearchResult->airways.end());

  // Update all tabs and force update
  showInformationInternal(*currentSearchResult, false /* showWindows */, false /* scrollToTop */, true /* forceUpdate */);
}

void InfoController::updateUserAircraftText()
{
  Ui::MainWindow *ui = NavApp::getMainUi();
#ifdef DEBUG_MOVING_AIRPLANE
  if(/* DISABLES CODE */ (true))
#else
  if(NavApp::isConnectedActive())
#endif
  {
    if(NavApp::isUserAircraftValid())
    {
      if(atools::gui::util::canTextEditUpdate(ui->textBrowserAircraftInfo))
      {
        // ok - scrollbars not pressed
        HtmlBuilder html(true /* has background color */);
        infoBuilder->aircraftText(lastSimData->getUserAircraftConst(), html);
        infoBuilder->aircraftTextWeightAndFuel(lastSimData->getUserAircraftConst(), html);
        updateTextEdit(ui->textBrowserAircraftInfo, html.getHtml(), false /* scrollToTop*/, true /* keepSelection */);
      }
      ui->textBrowserAircraftInfo->setToolTip(QString());
      ui->textBrowserAircraftInfo->setStatusTip(QString());
    }
    else
    {
      ui->textBrowserAircraftInfo->clear();
      ui->textBrowserAircraftInfo->setPlaceholderText(waitingForUpdateText.arg(getConnectionTypeText()));
    }
  }
  else
  {
    ui->textBrowserAircraftInfo->clear();
    ui->textBrowserAircraftInfo->setPlaceholderText(notConnectedText);
  }
}

void InfoController::updateAircraftProgressText()
{
  Ui::MainWindow *ui = NavApp::getMainUi();
#ifdef DEBUG_MOVING_AIRPLANE
  if(/* DISABLES CODE */ (true))
#else
  if(NavApp::isConnected())
#endif
  {
    if(NavApp::isUserAircraftValid())
    {
      if(atools::gui::util::canTextEditUpdate(ui->textBrowserAircraftProgressInfo))
      {
        // ok - scrollbars not pressed
        HtmlBuilder html(true /* has background color */);
        html.setIdBits(aircraftProgressConfig->getEnabledBits());
        infoBuilder->aircraftProgressText(lastSimData->getUserAircraftConst(), html, NavApp::getRouteConst());
        updateTextEdit(ui->textBrowserAircraftProgressInfo, html.getHtml(), false /* scrollToTop*/, true /* keepSelection */);
      }
      ui->textBrowserAircraftProgressInfo->setToolTip(QString());
      ui->textBrowserAircraftProgressInfo->setStatusTip(QString());
    }
    else
    {
      ui->textBrowserAircraftProgressInfo->clear();
      ui->textBrowserAircraftProgressInfo->setPlaceholderText(waitingForUpdateText.arg(getConnectionTypeText()));
    }
  }
  else
  {
    ui->textBrowserAircraftProgressInfo->clear();
    ui->textBrowserAircraftProgressInfo->setPlaceholderText(notConnectedText);
  }
}

void InfoController::updateAiAircraftText()
{
  Ui::MainWindow *ui = NavApp::getMainUi();
#ifdef DEBUG_MOVING_AIRPLANE
  if(/* DISABLES CODE */ (true))
#else
  if(NavApp::isConnected())
#endif
  {
    if(NavApp::isUserAircraftValid())
    {
      if(atools::gui::util::canTextEditUpdate(ui->textBrowserAircraftAiInfo))
      {
        // ok - scrollbars not pressed
        HtmlBuilder html(true /* has background color */);
        html.clear();
        if(!currentSearchResult->aiAircraft.isEmpty())
        {
          int num = 1;
          for(const map::MapAiAircraft& aircraft : qAsConst(currentSearchResult->aiAircraft))
          {
            infoBuilder->aircraftText(aircraft.getAircraft(), html, num, lastSimData->getAiAircraftConst().size());

            infoBuilder->aircraftProgressText(aircraft.getAircraft(), html, Route());
            num++;
          }

          updateTextEdit(ui->textBrowserAircraftAiInfo, html.getHtml(), false /* scrollToTop*/, true /* keepSelection */);
        }
        else
        {
          int numAi = lastSimData->getAiAircraftConst().size();
          QString text;

          if(!(NavApp::getShownMapTypes() & map::AIRCRAFT_AI))
            text = tr("<b>AI and multiplayer aircraft are not shown on map.</b><br/>");

          text += tr("No AI or multiplayer aircraft selected.<br/>"
                     "Found %1 AI or multiplayer aircraft.").
                  arg(numAi > 0 ? QLocale().toString(numAi) : tr("no"));
          updateTextEdit(ui->textBrowserAircraftAiInfo, text, false /* scrollToTop*/, true /* keepSelection */);
        }
      }
      ui->textBrowserAircraftAiInfo->setToolTip(QString());
      ui->textBrowserAircraftAiInfo->setStatusTip(QString());
    }
    else
    {
      ui->textBrowserAircraftAiInfo->clear();
      ui->textBrowserAircraftAiInfo->setPlaceholderText(waitingForUpdateText.arg(getConnectionTypeText()));
    }
  }
  else
  {
    ui->textBrowserAircraftAiInfo->clear();
    ui->textBrowserAircraftAiInfo->setPlaceholderText(notConnectedText);
  }
}

void InfoController::simDataChanged(const atools::fs::sc::SimConnectData& data)
{
  if(databaseLoadStatus)
    return;

  Ui::MainWindow *ui = NavApp::getMainUi();

  if(atools::almostNotEqual(QDateTime::currentDateTime().toMSecsSinceEpoch(),
                            lastSimUpdate, static_cast<qint64>(MIN_SIM_UPDATE_TIME_MS)))
  {
    // Last update was more than 500 ms ago
    updateAiAirports(data);

    *lastSimData = data;
    if(data.getUserAircraftConst().isFullyValid() && ui->dockWidgetAircraft->isVisible())
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
    if(data.getUserAircraftConst().isFullyValid() && ui->dockWidgetInformation->isVisible())
    {
      if(tabHandlerAirportInfo->getCurrentTabId() == ic::INFO_AIRPORT_OVERVIEW)
        updateAirportInternal(false /* new */, true /* bearing change*/, false /* scrollToTop */, false /* forceWeatherUpdate */);

      if(tabHandlerInfo->getCurrentTabId() == ic::INFO_NAVAID)
        updateNavaidInternal(*currentSearchResult, true /* bearing changed */, false /* scrollToTop */, false /* forceUpdate */);

      if(tabHandlerInfo->getCurrentTabId() == ic::INFO_USERPOINT)
        updateUserpointInternal(*currentSearchResult, true /* bearing changed */, false /* scrollToTop */);
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
    QList<map::MapAiAircraft> newAiAircraftShown;

    // Find all aircraft currently shown on the page in the newly arrived ai list
    for(const map::MapAiAircraft& aircraft : qAsConst(currentSearchResult->aiAircraft))
    {
      auto it = std::find_if(newAiAircraft.constBegin(), newAiAircraft.constEnd(), [&aircraft](const SimConnectAircraft& ac) -> bool {
        return ac.getObjectId() == aircraft.getAircraft().getObjectId();
      });

      if(it != newAiAircraft.end())
        newAiAircraftShown.append(map::MapAiAircraft(*it));
    }

    // Overwite old list
    currentSearchResult->aiAircraft = newAiAircraftShown;
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
  *lastSimData = atools::fs::sc::SimConnectData();
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
  showInformationInternal(*currentSearchResult, false /* showWindows */, false /* scrollToTop */, true /* forceUpdate */);
  updateAircraftInfo();
}

void InfoController::fontChanged(const QFont&)
{
  optionsChanged();
}

/* Update font size in text browsers if options have changed */
void InfoController::updateTextEditFontSizes()
{
  Ui::MainWindow *ui = NavApp::getMainUi();

  using atools::gui::setWidgetFontSize;

  int sizePercentInfo = OptionData::instance().getGuiInfoTextSize();
  setWidgetFontSize(ui->textBrowserAirportInfo, sizePercentInfo);
  setWidgetFontSize(ui->textBrowserRunwayInfo, sizePercentInfo);
  setWidgetFontSize(ui->textBrowserComInfo, sizePercentInfo);
  setWidgetFontSize(ui->textBrowserApproachInfo, sizePercentInfo);
  setWidgetFontSize(ui->textBrowserNearestInfo, sizePercentInfo);
  setWidgetFontSize(ui->textBrowserWeatherInfo, sizePercentInfo);
  setWidgetFontSize(ui->textBrowserNavaidInfo, sizePercentInfo);
  setWidgetFontSize(ui->textBrowserUserpointInfo, sizePercentInfo);
  setWidgetFontSize(ui->textBrowserAirspaceInfo, sizePercentInfo);
  setWidgetFontSize(ui->textBrowserLogbookInfo, sizePercentInfo);
  setWidgetFontSize(ui->textBrowserCenterInfo, sizePercentInfo);
  setWidgetFontSize(ui->textBrowserClientInfo, sizePercentInfo);

  int sizePercentSim = OptionData::instance().getGuiInfoSimSize();
  setWidgetFontSize(ui->textBrowserAircraftInfo, sizePercentSim);
  setWidgetFontSize(ui->textBrowserAircraftProgressInfo, sizePercentSim);
  setWidgetFontSize(ui->textBrowserAircraftAiInfo, sizePercentSim);

  // Adjust symbol sizes
  int fontPixelSize = atools::roundToInt(QFontMetricsF(ui->textBrowserAirportInfo->font()).height());
  infoBuilder->setSymbolSize(QSize(fontPixelSize, fontPixelSize));

  fontPixelSize = atools::roundToInt(QFontMetricsF(ui->textBrowserAirportInfo->font()).height() * 1.2);
  infoBuilder->setSymbolSizeTitle(QSize(fontPixelSize, fontPixelSize));

  fontPixelSize = atools::roundToInt(QFontMetricsF(ui->textBrowserAircraftInfo->font()).height() * 1.4);
  infoBuilder->setSymbolSizeVehicle(QSize(fontPixelSize, fontPixelSize));
}

QStringList InfoController::getAirportTextFull(const QString& ident) const
{
  map::MapAirport airport;
  airportQuery->getAirportByIdent(airport, ident);

  QStringList retval;
  if(airport.isValid())
  {
    map::WeatherContext weatherContext;
    NavApp::getWeatherContextHandler()->buildWeatherContextInfo(weatherContext, airport);

    atools::util::HtmlBuilder html(mapcolors::webTableBackgroundColor, mapcolors::webTableAltBackgroundColor);
    HtmlInfoBuilder builder(mainWindow->getMapWidget(), true /*info*/, true /*print*/);
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

void InfoController::resetTabLayout()
{
  tabHandlerInfo->reset();
  tabHandlerAirportInfo->reset();
  tabHandlerAircraft->reset();
}

const QBitArray& InfoController::getEnabledProgressBits() const
{
  return aircraftProgressConfig->getEnabledBits();
}

const QBitArray& InfoController::getEnabledProgressBitsWeb() const
{
  return aircraftProgressConfig->getEnabledBitsWeb();
}

void InfoController::updateTextEdit(QTextEdit *textEdit, const QString& text, bool scrollToTop, bool keepSelection)
{
  // Clear selection if textEdit was in anchorsClicked and removed
  atools::gui::util::updateTextEdit(textEdit, text, scrollToTop, keepSelection, anchorsClicked.remove(textEdit));
}
