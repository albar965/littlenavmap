/*****************************************************************************
* Copyright 2015-2017 Alexander Barthel albar965@mailbox.org
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
#include "common/htmlinfobuilder.h"
#include "gui/mainwindow.h"
#include "gui/widgetutil.h"
#include "gui/widgetstate.h"
#include "mapgui/mapquery.h"
#include "route/route.h"
#include "settings/settings.h"
#include "ui_mainwindow.h"
#include "util/htmlbuilder.h"
#include "options/optiondata.h"

#include <QDebug>
#include <QScrollBar>
#include <QUrlQuery>
#include <QDesktopServices>
#include <QProcess>
#include <QMessageBox>
#include <QDir>
#include <QTabWidget>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

using atools::util::HtmlBuilder;
using atools::fs::sc::SimConnectAircraft;
using atools::fs::sc::SimConnectUserAircraft;

InfoController::InfoController(MainWindow *parent)
  : QObject(parent), mainWindow(parent), mapQuery(NavApp::getMapQuery())
{
  infoBuilder = new HtmlInfoBuilder(mainWindow, true);

  Ui::MainWindow *ui = NavApp::getMainUi();
  infoFontPtSize = static_cast<float>(ui->textBrowserAirportInfo->font().pointSizeF());
  simInfoFontPtSize = static_cast<float>(ui->textBrowserAircraftInfo->font().pointSizeF());

  // Set search path to silence text browser warnings
  QStringList paths({QApplication::applicationDirPath()});
  ui->textBrowserAirportInfo->setSearchPaths(paths);
  ui->textBrowserRunwayInfo->setSearchPaths(paths);
  ui->textBrowserComInfo->setSearchPaths(paths);
  ui->textBrowserApproachInfo->setSearchPaths(paths);
  ui->textBrowserWeatherInfo->setSearchPaths(paths);
  ui->textBrowserNavaidInfo->setSearchPaths(paths);
  ui->textBrowserAirspaceInfo->setSearchPaths(paths);
  ui->textBrowserAircraftInfo->setSearchPaths(paths);
  ui->textBrowserAircraftProgressInfo->setSearchPaths(paths);
  ui->textBrowserAircraftAiInfo->setSearchPaths(paths);

  // Create connections for "Map" links in text browsers
  connect(ui->textBrowserAirportInfo, &QTextBrowser::anchorClicked, this, &InfoController::anchorClicked);
  connect(ui->textBrowserRunwayInfo, &QTextBrowser::anchorClicked, this, &InfoController::anchorClicked);
  connect(ui->textBrowserComInfo, &QTextBrowser::anchorClicked, this, &InfoController::anchorClicked);
  connect(ui->textBrowserApproachInfo, &QTextBrowser::anchorClicked, this, &InfoController::anchorClicked);
  connect(ui->textBrowserWeatherInfo, &QTextBrowser::anchorClicked, this, &InfoController::anchorClicked);
  connect(ui->textBrowserNavaidInfo, &QTextBrowser::anchorClicked, this, &InfoController::anchorClicked);
  connect(ui->textBrowserAirspaceInfo, &QTextBrowser::anchorClicked, this, &InfoController::anchorClicked);

  connect(ui->textBrowserAircraftInfo, &QTextBrowser::anchorClicked, this, &InfoController::anchorClicked);
  connect(ui->textBrowserAircraftProgressInfo, &QTextBrowser::anchorClicked, this, &InfoController::anchorClicked);

  connect(ui->textBrowserAircraftAiInfo, &QTextBrowser::anchorClicked, this, &InfoController::anchorClicked);

  connect(ui->tabWidgetAircraft, &QTabWidget::currentChanged, this, &InfoController::currentTabChanged);
}

InfoController::~InfoController()
{
  delete infoBuilder;
}

void InfoController::currentTabChanged(int index)
{
  // Update new tab to avoid half a second delay or obsolete information
  switch(static_cast<ic::TabIndexAircraft>(index))
  {
    case ic::AIRCRAFT_USER:
      updateAircraftText();
      break;
    case ic::AIRCRAFT_USER_PROGRESS:
      updateAircraftProgressText();
      break;
    case ic::AIRCRAFT_AI:
      updateAiAircraftText();
      break;
  }
}

/* User clicked on "Map" link in text browsers */
void InfoController::anchorClicked(const QUrl& url)
{
  qDebug() << Q_FUNC_INFO << url;

  if(url.scheme() == "lnm")
  {
    QUrlQuery query(url);

    if(url.host() == "show")
    {
      if(query.hasQueryItem("lonx") && query.hasQueryItem("laty"))
      {
        float zoom = 0.f;
        if(query.hasQueryItem("zoom"))
          zoom = query.queryItemValue("zoom").toFloat();

        emit showPos(atools::geo::Pos(query.queryItemValue("lonx").toFloat(),
                                      query.queryItemValue("laty").toFloat()), zoom, false);
      }
      else if(query.hasQueryItem("id") && query.hasQueryItem("type"))
      {
        map::MapObjectTypes type(query.queryItemValue("type").toInt());
        int id = query.queryItemValue("id").toInt();

        if(type & map::AIRPORT)
          emit showRect(mapQuery->getAirportById(id).bounding, false);
        if(type & map::AIRSPACE)
          emit showRect(mapQuery->getAirspaceById(id).bounding, false);
      }
      else if(query.hasQueryItem("airport"))
      {
        // Airport ident from AI aircraft progress
        map::MapAirport airport;
        mapQuery->getAirportByIdent(airport, query.queryItemValue("airport"));
        emit showRect(airport.bounding, false);
      }
      else if(query.hasQueryItem("filepath"))
      {
#ifdef Q_OS_WIN
        QFileInfo fp(query.queryItemValue("filepath"));
        fp.makeAbsolute();

        // if(!QProcess::startDetached("explorer.exe", {"/select", QDir::toNativeSeparators(fp.filePath())},
        // QDir::homePath()))
        // QMessageBox::warning(mainWindow, QApplication::applicationName(), QString(
        // tr("Error starting explorer.exe with path \"%1\"")).
        // arg(query.queryItemValue("filepath")));

        if(fp.exists())
        {
          // Syntax is: explorer /select, "C:\Folder1\Folder2\file_to_select"
          // Dir separators MUST be win-style slashes

          // QProcess::startDetached() has an obscure bug. If the path has
          // no spaces and a comma(and maybe other special characters) it doesn't
          // get wrapped in quotes. So explorer.exe can't find the correct path and
          // displays the default one. If we wrap the path in quotes and pass it to
          // QProcess::startDetached() explorer.exe still shows the default path. In
          // this case QProcess::startDetached() probably puts its own quotes around ours.

          STARTUPINFO startupInfo;
          ::ZeroMemory(&startupInfo, sizeof(startupInfo));
          startupInfo.cb = sizeof(startupInfo);

          PROCESS_INFORMATION processInfo;
          ::ZeroMemory(&processInfo, sizeof(processInfo));

          QString cmd = QString("explorer.exe /select,\"%1\"").arg(QDir::toNativeSeparators(fp.filePath()));
          LPWSTR lpCmd = new WCHAR[cmd.size() + 1];
          cmd.toWCharArray(lpCmd);
          lpCmd[cmd.size()] = 0;

          bool ret = ::CreateProcessW(NULL, lpCmd, NULL, NULL, FALSE, 0, NULL, NULL, &startupInfo,
                                      &processInfo);
          delete[] lpCmd;

          if(ret)
          {
            ::CloseHandle(processInfo.hProcess);
            ::CloseHandle(processInfo.hThread);
          }
        }
        else
        {
          // If the item to select doesn't exist, try to open its parent
          QUrl fileUrl = QUrl::fromLocalFile(fp.path());
          if(!QDesktopServices::openUrl(fileUrl))
            QMessageBox::warning(mainWindow, QApplication::applicationName(), QString(
                                   tr("Error opening path \"%1\"")).arg(url.toDisplayString()));
        }
#else
        // if(!QProcess::startDetached("nautilus", {query.queryItemValue("filepath")}, QDir::homePath()))
        // {
        QUrl fileUrl = QUrl::fromLocalFile(QFileInfo(query.queryItemValue("filepath")).path());

        if(!QDesktopServices::openUrl(fileUrl))
          QMessageBox::warning(mainWindow, QApplication::applicationName(), QString(
                                 tr("Error opening path \"%1\"")).arg(url.toDisplayString()));
        // }
#endif
      }
    }
  }
}

void InfoController::saveState()
{
  Ui::MainWindow *ui = NavApp::getMainUi();
  atools::gui::WidgetState(lnm::INFOWINDOW_WIDGET).save({ui->tabWidgetInformation, ui->tabWidgetAircraft,
                                                         ui->tabWidgetLegend});

  // Store currently shown map objects in a string list containing id and type
  map::MapObjectRefList refs;
  for(const map::MapAirport& airport  : currentSearchResult.airports)
    refs.append({airport.id, map::AIRPORT});
  for(const map::MapVor& vor : currentSearchResult.vors)
    refs.append({vor.id, map::VOR});
  for(const map::MapNdb& ndb : currentSearchResult.ndbs)
    refs.append({ndb.id, map::NDB});
  for(const map::MapWaypoint& waypoint : currentSearchResult.waypoints)
    refs.append({waypoint.id, map::WAYPOINT});
  for(const map::MapAirway& airway : currentSearchResult.airways)
    refs.append({airway.id, map::AIRWAY});
  for(const map::MapAirspace& airspace : currentSearchResult.airspaces)
    refs.append({airspace.id, map::AIRSPACE});

  atools::settings::Settings& settings = atools::settings::Settings::instance();
  QStringList refList;
  for(const map::MapObjectRef& ref : refs)
    refList.append(QString("%1;%2").arg(ref.id).arg(ref.type));
  settings.setValue(lnm::INFOWINDOW_CURRENTMAPOBJECTS, refList.join(";"));
}

void InfoController::restoreState()
{
  QString refsStr = atools::settings::Settings::instance().valueStr(lnm::INFOWINDOW_CURRENTMAPOBJECTS);
  QStringList refsStrList = refsStr.split(";", QString::SkipEmptyParts);

  // Go through the string and collect all objects in the MapSearchResult
  map::MapSearchResult res;
  for(int i = 0; i < refsStrList.size(); i += 2)
    mapQuery->getMapObjectById(res,
                               map::MapObjectTypes(refsStrList.at(i + 1).toInt()),
                               refsStrList.at(i).toInt());

  iconBackColor = QApplication::palette().color(QPalette::Active, QPalette::Base);
  updateTextEditFontSizes();
  infoBuilder->updateAircraftIcons(true);
  showInformationInternal(res, false);

  Ui::MainWindow *ui = NavApp::getMainUi();
  atools::gui::WidgetState(lnm::INFOWINDOW_WIDGET).restore({ui->tabWidgetInformation, ui->tabWidgetAircraft,
                                                            ui->tabWidgetLegend});
}

void InfoController::updateAirport()
{
  updateAirportInternal(false);
}

void InfoController::updateProgress()
{
  HtmlBuilder html(true /* has background color */);
  Ui::MainWindow *ui = NavApp::getMainUi();

  if(atools::gui::util::canTextEditUpdate(ui->textBrowserAircraftProgressInfo))
  {
    // ok - scrollbars not pressed
    html.clear();
    infoBuilder->aircraftProgressText(lastSimData.getUserAircraft(), html, NavApp::getRoute());
    atools::gui::util::updateTextEdit(ui->textBrowserAircraftProgressInfo, html.getHtml());
  }
}

void InfoController::updateAirportInternal(bool newAirport)
{
  if(databaseLoadStatus)
    return;

  if(!currentSearchResult.airports.isEmpty())
  {
    map::WeatherContext currentWeatherContext;
    bool weatherChanged = mainWindow->buildWeatherContextForInfo(currentWeatherContext,
                                                                 currentSearchResult.airports.first());

    // qDebug() << Q_FUNC_INFO << "newAirport" << newAirport << "weatherChanged" << weatherChanged
    // << "ident" << currentWeatherContext.ident;

    if(newAirport || weatherChanged)
    {
      HtmlBuilder html(true);
      map::MapAirport airport;
      mapQuery->getAirportById(airport, currentSearchResult.airports.first().id);

      // qDebug() << Q_FUNC_INFO << "Updating html" << airport.ident << airport.id;

      infoBuilder->airportText(airport, currentWeatherContext, html, &NavApp::getRoute(), iconBackColor);

      Ui::MainWindow *ui = NavApp::getMainUi();
      if(newAirport)
        // scroll up for new airports
        ui->textBrowserAirportInfo->setText(html.getHtml());
      else
        // Leave position for weather updates
        atools::gui::util::updateTextEdit(ui->textBrowserAirportInfo, html.getHtml());

      html.clear();
      infoBuilder->weatherText(currentWeatherContext, airport, html, iconBackColor);

      if(newAirport)
        // scroll up for new airports
        ui->textBrowserWeatherInfo->setText(html.getHtml());
      else
        // Leave position for weather updates
        atools::gui::util::updateTextEdit(ui->textBrowserWeatherInfo, html.getHtml());
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
  ui->textBrowserWeatherInfo->clear();
  ui->textBrowserNavaidInfo->clear();
  ui->textBrowserAirspaceInfo->clear();
}

void InfoController::showInformation(map::MapSearchResult result)
{
  showInformationInternal(result, true);
}

void InfoController::updateAllInformation()
{
  showInformationInternal(currentSearchResult, false);
}

/* Show information in all tabs but do not show dock
 *  @return true if information was updated */
void InfoController::showInformationInternal(const map::MapSearchResult& result, bool showWindows)
{
  qDebug() << Q_FUNC_INFO;

  bool foundAirport = false, foundNavaid = false, foundUserAircraft = false, foundAiAircraft = false,
       foundAirspace = false;
  HtmlBuilder html(true);

  Ui::MainWindow *ui = NavApp::getMainUi();

  currentSearchResult.userAircraft = result.userAircraft;
  foundUserAircraft = currentSearchResult.userAircraft.getPosition().isValid();

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

  // Airport ================================================================
  if(!result.airports.isEmpty())
  {
    qDebug() << "Found airport" << result.airports.first().ident;

    // Only one airport shown - have to make a copy here since currentSearchResult might be equal to result
    // when updating
    map::MapAirport airport = result.airports.first();

    currentSearchResult.airports.clear();
    currentSearchResult.airportIds.clear();
    // Remember one airport
    currentSearchResult.airports.append(airport);

    updateAirportInternal(true);

    html.clear();
    infoBuilder->runwayText(airport, html, iconBackColor);
    ui->textBrowserRunwayInfo->setText(html.getHtml());

    html.clear();
    infoBuilder->comText(airport, html, iconBackColor);
    ui->textBrowserComInfo->setText(html.getHtml());

    html.clear();
    infoBuilder->procedureText(airport, html, iconBackColor);
    ui->textBrowserApproachInfo->setText(html.getHtml());

    html.clear();
    map::WeatherContext currentWeatherContext;
    mainWindow->buildWeatherContextForInfo(currentWeatherContext, airport);
    infoBuilder->weatherText(currentWeatherContext, airport, html, iconBackColor);
    ui->textBrowserWeatherInfo->setText(html.getHtml());

    foundAirport = true;
  }

  // Airspaces ================================================================
  if(!result.airspaces.isEmpty())
  {
    currentSearchResult.airspaces.clear();
    html.clear();
    for(const map::MapAirspace& airspace : result.airspaces)
    {
      qDebug() << "Found airspace" << airspace.id;

      currentSearchResult.airspaces.append(airspace);
      infoBuilder->airspaceText(airspace, html, iconBackColor);
      html.br();
    }
    foundAirspace = true;
    ui->textBrowserAirspaceInfo->setText(html.getHtml());
  }

  // Navaids ================================================================
  if(!result.vors.isEmpty() || !result.ndbs.isEmpty() || !result.waypoints.isEmpty() || !result.airways.isEmpty())
  {
    // if any navaids are to be shown clear search result before
    currentSearchResult.vors.clear();
    currentSearchResult.vorIds.clear();
    currentSearchResult.ndbs.clear();
    currentSearchResult.ndbIds.clear();
    currentSearchResult.ils.clear();
    currentSearchResult.runwayEnds.clear();
    currentSearchResult.waypoints.clear();
    currentSearchResult.waypointIds.clear();
    currentSearchResult.airways.clear();
  }

  html.clear();
  for(const map::MapVor& vor : result.vors)
  {
    qDebug() << "Found vor" << vor.ident;

    currentSearchResult.vors.append(vor);
    infoBuilder->vorText(vor, html, iconBackColor);
    html.br();
    foundNavaid = true;
  }

  for(const map::MapNdb& ndb : result.ndbs)
  {
    qDebug() << "Found ndb" << ndb.ident;

    currentSearchResult.ndbs.append(ndb);
    infoBuilder->ndbText(ndb, html, iconBackColor);
    html.br();
    foundNavaid = true;
  }

  for(const map::MapWaypoint& waypoint : result.waypoints)
  {
    qDebug() << "Found waypoint" << waypoint.ident;

    currentSearchResult.waypoints.append(waypoint);
    infoBuilder->waypointText(waypoint, html, iconBackColor);
    html.br();
    foundNavaid = true;
  }

  // Remove the worst airway duplicates as a workaround for buggy source data
  for(const map::MapAirway& airway : result.airways)
  {
    qDebug() << "Found airway" << airway.name;

    currentSearchResult.airways.append(airway);
    infoBuilder->airwayText(airway, html);
    html.br();
    foundNavaid = true;
  }

  if(foundNavaid)
    ui->textBrowserNavaidInfo->setText(html.getHtml());

  // Show dock windows if needed
  if(showWindows)
  {
    if(foundNavaid || foundAirport || foundAirspace)
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
    if(foundNavaid)
      mainWindow->setStatusMessage(tr("Showing information for navaid."));
    else if(foundAirport)
      mainWindow->setStatusMessage(tr("Showing information for airport."));
    else if(foundAirspace)
      mainWindow->setStatusMessage(tr("Showing information for airspace."));

    ic::TabIndex idx = static_cast<ic::TabIndex>(ui->tabWidgetInformation->currentIndex());
    bool airportActive = idx == ic::INFO_AIRPORT || idx == ic::INFO_RUNWAYS || idx == ic::INFO_COM ||
                         idx == ic::INFO_APPROACHES || idx == ic::INFO_WEATHER;

    bool navaidActive = idx == ic::INFO_NAVAID;

    ic::TabIndex newIdx = idx;

    if(foundAirspace && !foundNavaid && !foundAirport)
      newIdx = ic::INFO_AIRSPACE;
    else if(foundAirport && !foundNavaid)
    {
      if(!airportActive)
        newIdx = ic::INFO_AIRPORT;
    }
    else if(foundNavaid && !foundAirport)
      newIdx = ic::INFO_NAVAID;
    else if(foundNavaid && foundAirport)
    {
      if(!airportActive && !navaidActive)
        newIdx = ic::INFO_AIRPORT;
    }

    ui->tabWidgetInformation->setCurrentIndex(newIdx);

    // Switch to a tab in aircraft window
    ic::TabIndexAircraft acidx = static_cast<ic::TabIndexAircraft>(ui->tabWidgetAircraft->currentIndex());
    if(foundUserAircraft && !foundAiAircraft)
    {
      // If no user aircraft related tabs is shown bring user tab to front
      if(acidx != ic::AIRCRAFT_USER && acidx != ic::AIRCRAFT_USER_PROGRESS)
        ui->tabWidgetAircraft->setCurrentIndex(ic::AIRCRAFT_USER);
    }
    if(!foundUserAircraft && foundAiAircraft)
      ui->tabWidgetAircraft->setCurrentIndex(ic::AIRCRAFT_AI);
  }
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

void InfoController::updateAircraftText()
{
  Ui::MainWindow *ui = NavApp::getMainUi();
#ifdef DEBUG_MOVING_AIRPLANE
  if( /* DISABLES CODE */ (true))
#else
  if(NavApp::isConnected())
#endif
  {
    if(lastSimData.getUserAircraft().getPosition().isValid())
    {
      if(atools::gui::util::canTextEditUpdate(ui->textBrowserAircraftInfo))
      {
        // ok - scrollbars not pressed
        HtmlBuilder html(true /* has background color */);
        infoBuilder->aircraftText(lastSimData.getUserAircraft(), html);
        infoBuilder->aircraftTextWeightAndFuel(lastSimData.getUserAircraft(), html);
        atools::gui::util::updateTextEdit(ui->textBrowserAircraftInfo, html.getHtml());
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
    if(lastSimData.getUserAircraft().getPosition().isValid())
    {
      if(atools::gui::util::canTextEditUpdate(ui->textBrowserAircraftProgressInfo))
      {
        // ok - scrollbars not pressed
        HtmlBuilder html(true /* has background color */);
        infoBuilder->aircraftProgressText(lastSimData.getUserAircraft(), html, NavApp::getRoute());
        atools::gui::util::updateTextEdit(ui->textBrowserAircraftProgressInfo, html.getHtml());
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
    if(lastSimData.getUserAircraft().getPosition().isValid())
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
            infoBuilder->aircraftText(aircraft, html, num, lastSimData.getAiAircraft().size());
            infoBuilder->aircraftProgressText(aircraft, html, Route());
            num++;
          }

          atools::gui::util::updateTextEdit(ui->textBrowserAircraftAiInfo, html.getHtml());
        }
        else
        {
          int numAi = lastSimData.getAiAircraft().size();
          QString text;

          if(!(NavApp::getShownMapFeatures() & map::AIRCRAFT_AI))
            text = tr("<b>AI and multiplayer aircraft are not shown on map.</b><br/>");

          text += tr("No AI or multiplayer aircraft selected.<br/>"
                     "Found %1 AI or multiplayer aircraft.").
                  arg(numAi > 0 ? QLocale().toString(numAi) : tr("no"));
          atools::gui::util::updateTextEdit(ui->textBrowserAircraftAiInfo, text);
        }
      }
    }
    else
      ui->textBrowserAircraftAiInfo->setPlainText(tr("Connected. Waiting for update."));
  }
  else
    ui->textBrowserAircraftAiInfo->clear();
}

void InfoController::simulatorDataReceived(atools::fs::sc::SimConnectData data)
{
  if(databaseLoadStatus)
    return;

  if(atools::almostNotEqual(QDateTime::currentDateTime().toMSecsSinceEpoch(),
                            lastSimUpdate, static_cast<qint64>(MIN_SIM_UPDATE_TIME_MS)))
  {
    // Last update was more than 500 ms ago
    updateAiAirports(data);

    Ui::MainWindow *ui = NavApp::getMainUi();

    lastSimData = data;
    if(data.getUserAircraft().getPosition().isValid() && ui->dockWidgetAircraft->isVisible())
    {
      if(ui->tabWidgetAircraft->currentIndex() == ic::AIRCRAFT_USER)
        updateAircraftText();

      if(ui->tabWidgetAircraft->currentIndex() == ic::AIRCRAFT_USER_PROGRESS)
        updateAircraftProgressText();

      if(ui->tabWidgetAircraft->currentIndex() == ic::AIRCRAFT_AI)
        updateAiAircraftText();
    }
    lastSimUpdate = QDateTime::currentDateTime().toMSecsSinceEpoch();
  }
}

void InfoController::updateAiAirports(const atools::fs::sc::SimConnectData& data)
{
  if(data.getPacketId() > 0)
  {
    // Ignore weather updates
    const QVector<atools::fs::sc::SimConnectAircraft>& newAiAircraft = data.getAiAircraft();
    QVector<atools::fs::sc::SimConnectAircraft> newAiAircraftShown;

    // Find all aircraft currently shown on the page in the newly arrived ai list
    for(SimConnectAircraft& aircraft : currentSearchResult.aiAircraft)
    {
      QVector<atools::fs::sc::SimConnectAircraft>::const_iterator it =
        std::find_if(newAiAircraft.begin(), newAiAircraft.end(),
                     [ = ](const SimConnectAircraft &ac)->bool
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
  updateAircraftText();
  updateAircraftProgressText();
  updateAiAircraftText();
}

void InfoController::optionsChanged()
{
  iconBackColor = QApplication::palette().color(QPalette::Active, QPalette::Base);
  updateTextEditFontSizes();
  infoBuilder->updateAircraftIcons(true);
  showInformationInternal(currentSearchResult, false);
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
  setTextEditFontSize(ui->textBrowserWeatherInfo, infoFontPtSize, sizePercent);
  setTextEditFontSize(ui->textBrowserNavaidInfo, infoFontPtSize, sizePercent);
  setTextEditFontSize(ui->textBrowserAirspaceInfo, infoFontPtSize, sizePercent);

  sizePercent = OptionData::instance().getGuiInfoSimSize();
  setTextEditFontSize(ui->textBrowserAircraftInfo, simInfoFontPtSize, sizePercent);
  setTextEditFontSize(ui->textBrowserAircraftProgressInfo, simInfoFontPtSize, sizePercent);
  setTextEditFontSize(ui->textBrowserAircraftAiInfo, simInfoFontPtSize, sizePercent);
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
