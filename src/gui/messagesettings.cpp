/*****************************************************************************
* Copyright 2015-2025 Alexander Barthel alex@littlenavmap.org
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

#include "gui/messagesettings.h"

#include "settings/settings.h"
#include "common/constants.h"

#include <QDebug>

// egrep -o -r "(showInfoMsgBox|showWarnMsgBox|showQuestionMsgBox|showQuestionMsgBox).*lnm::(\w+)"
//
// common/elevationprovider.cpp:showInfoMsgBox(lnm::ACTIONS_SHOW_INSTALL_GLOBE
// common/updatehandler.cpp:showWarnMsgBox(lnm::ACTIONS_SHOW_UPDATE_FAILED
// routeexport/fetchroutedialog.cpp:showQuestionMsgBox(lnm::ACTIONS_SHOW_SSL_WARNING_SIMBRIEF
// routeexport/simbriefhandler.cpp:showQuestionMsgBox(lnm::ACTIONS_SHOW_SEND_SIMBRIEF
// routeexport/routeexport.cpp:showQuestionMsgBox(lnm::ACTIONS_SHOW_ROUTE_AIRPORT_WARNING
// routeexport/routeexport.cpp:showQuestionMsgBox(lnm::ACTIONS_SHOW_ROUTE_NAVDATA_ALL_WARNING
// routeexport/routeexport.cpp:showQuestionMsgBox(lnm::ACTIONS_SHOW_ROUTE_NO_CYCLE_WARNING
// routeexport/routeexport.cpp:showQuestionMsgBox(lnm::ACTIONS_SHOW_ROUTE_VFR_WARNING
// routeexport/routeexport.cpp:showQuestionMsgBox(lnm::ACTIONS_SHOW_ROUTE_PARKING_WARNING
// routeexport/routeexport.cpp:showWarnMsgBox(lnm::ACTIONS_SHOW_FILE_EXPORT_OPTIONS_WARN_EXP
// logbook/logdatacontroller.cpp:showQuestionMsgBox(lnm::ACTIONS_SHOW_LOGBOOK_CONVERSION
// online/onlinedatacontroller.cpp:showQuestionMsgBox(lnm::ACTIONS_SHOW_SSL_WARNING_ONLINE
// db/databasemanager.cpp:showQuestionMsgBox(lnm::ACTIONS_SHOW_NAVDATA_WARNING
// db/databasemanager.cpp:showQuestionMsgBox(manualCheck ? QString() : lnm::ACTIONS_SHOW_CORRECT_MSFS_HAS_NAVIGRAPH
// db/databasemanager.cpp:showQuestionMsgBox(manualCheck ? QString() : lnm::ACTIONS_SHOW_CORRECT_MSFS_NO_NAVIGRAPH
// db/databasemanager.cpp:showQuestionMsgBox(manualCheck ? QString() : lnm::ACTIONS_SHOW_CORRECT_MSFS_NO_NAVIGRAPH
// db/databasemanager.cpp:showQuestionMsgBox(manualCheck ? QString() : lnm::ACTIONS_SHOW_CORRECT_XP_CYCLE_NAV_EQUAL
// db/databasemanager.cpp:showQuestionMsgBox(manualCheck ? QString() : lnm::ACTIONS_SHOW_CORRECT_XP_CYCLE_NAV_SMALLER
// db/databasemanager.cpp:showQuestionMsgBox(manualCheck ? QString() : lnm::ACTIONS_SHOW_CORRECT_FSX_P3D_OUTDATED
// db/databasemanager.cpp:showQuestionMsgBox(manualCheck ? QString() : lnm::ACTIONS_SHOW_CORRECT_FSX_P3D_UPDATED
// db/databasemanager.cpp:showQuestionMsgBox(manualCheck ? QString() : lnm::ACTIONS_SHOW_DATABASE_MSFS_NAVIGRAPH_ALL
// db/databasemanager.cpp:showInfoMsgBox(lnm::ACTIONS_SHOW_DATABASE_SIMCONNECT
// db/databasemanager.cpp:showInfoMsgBox(lnm::ACTIONS_SHOW_DATABASE_BACKGROUND_HINT
// db/databasemanager.cpp:showQuestionMsgBox(lnm::ACTIONS_SHOW_DATABASE_OLD
// track/trackcontroller.cpp:showQuestionMsgBox(lnm::ACTIONS_SHOW_SSL_WARNING_TRACK
// track/trackcontroller.cpp:showWarnMsgBox(lnm::ACTIONS_SHOW_TRACK_DOWNLOAD_FAIL
// weather/weatherreporter.cpp:showQuestionMsgBox(lnm::ACTIONS_SHOW_SSL_WARNING_WEATHER
// weather/weatherreporter.cpp:showWarnMsgBox(lnm::ACTIONS_SHOW_WEATHER_DOWNLOAD_FAIL
// weather/windreporter.cpp:showQuestionMsgBox(lnm::ACTIONS_SHOW_SSL_WARNING_WIND
// gui/mainwindow.cpp:showQuestionMsgBox(lnm::ACTIONS_SHOW_DELETE_TRAIL
// gui/mainwindow.cpp:showInfoMsgBox(lnm::ROUTE_STRING_DIALOG_BACKGROUND_HINT
// gui/mainwindow.cpp:showQuestionMsgBox(lnm::ACTIONS_SHOW_REPLACE_TRAIL
// gui/mainwindow.cpp:showWarnMsgBox(lnm::ACTIONS_SHOW_TRAIL_POINTS
// gui/mainwindow.cpp:showInfoMsgBox(lnm::ACTIONS_SHOW_CRUISE_ZERO_WARNING
// gui/mainwindow.cpp:showQuestionMsgBox(lnm::ACTIONS_SHOW_SAVE_WARNING
// gui/mainwindow.cpp:showWarnMsgBox(lnm::ACTIONS_SHOW_SSL_FAILED
// gui/mainwindow.cpp:showInfoMsgBox(lnm::ACTIONS_SHOW_MISSING_SIMULATORS
// gui/mainwindow.cpp:showQuestionMsgBox(lnm::ACTIONS_SHOW_QUIT_LOADING
// gui/mainwindow.cpp:showQuestionMsgBox(lnm::ACTIONS_SHOW_QUIT
// connect/connectclient.cpp:showInfoMsgBox(lnm::ACTIONS_SHOW_DISCONNECT_INFO
// perf/aircraftperfcontroller.cpp:showQuestionMsgBox(lnm::ACTIONS_SHOW_RESET_PERF
// search/proceduresearch.cpp:showQuestionMsgBox(lnm::ACTIONS_SHOW_INVALID_PROC_WARNING
// search/searchbasetable.cpp:showInfoMsgBox(lnm::ACTIONS_SHOW_SEARCH_CENTER_NULL
// mapgui/mapwidget.cpp:showWarnMsgBox(lnm::ACTIONS_SHOW_ZOOM_WARNING
// mapgui/mapthemehandler.cpp:showInfoMsgBox(lnm::ACTIONS_SHOW_MAPTHEME_REQUIRES_KEY
// mapgui/mapmarkhandler.cpp:showQuestionMsgBox(lnm::ACTIONS_SHOW_DELETE_MARKS
// route/routecontroller.cpp:showInfoMsgBox(lnm::ACTIONS_SHOW_LOAD_FLP_WARN
// route/routecontroller.cpp:showInfoMsgBox(lnm::ACTIONS_SHOW_LOAD_ALT_WARN
// route/routecontroller.cpp:showQuestionMsgBox(lnm::ACTIONS_SHOW_SAVE_LNMPLN_WARNING
// route/routecontroller.cpp:showInfoMsgBox(lnm::ACTIONS_SHOW_ROUTE_ERROR
// route/routecontroller.cpp:showWarnMsgBox(lnm::ACTIONS_SHOW_FLIGHTPLAN_WARN_CONVERT

void messages::resetAllMessages()
{
  qDebug() << Q_FUNC_INFO;
  atools::settings::Settings& settings = atools::settings::Settings::instance();

  resetEssentialMessages();

  // Show all message dialogs again
  settings.setValue(lnm::ACTIONS_SHOW_CRUISE_ZERO_WARNING, true);
  settings.setValue(lnm::ACTIONS_SHOW_DELETE_MARKS, true);
  settings.setValue(lnm::ACTIONS_SHOW_DELETE_TRAIL, true);
  settings.setValue(lnm::ACTIONS_SHOW_DISCONNECT_INFO, true);
  settings.setValue(lnm::ACTIONS_SHOW_INSTALL_DIRS, true);
  settings.setValue(lnm::ACTIONS_SHOW_INSTALL_GLOBE, true);
  settings.setValue(lnm::ACTIONS_SHOW_LOAD_FLP_WARN, true);
  settings.setValue(lnm::ACTIONS_SHOW_LOAD_ALT_WARN, true);
  settings.setValue(lnm::ACTIONS_SHOW_LOAD_ALT_CORRECTED, true);
  settings.setValue(lnm::ACTIONS_SHOW_LOGBOOK_CONVERSION, true);
  settings.setValue(lnm::ACTIONS_SHOW_QUIT, true);
  settings.setValue(lnm::ACTIONS_SHOW_QUIT_LOADING, true);
  settings.setValue(lnm::ACTIONS_SHOW_RESET_PERF, true);
  settings.setValue(lnm::ACTIONS_SHOW_SAVE_LNMPLN_WARNING, true);
  settings.setValue(lnm::ACTIONS_SHOW_SAVE_WARNING, true);
  settings.setValue(lnm::ACTIONS_SHOW_SEARCH_CENTER_NULL, true);
  settings.setValue(lnm::ACTIONS_SHOW_SEND_SIMBRIEF, true);
  settings.setValue(lnm::ACTIONS_SHOW_UPDATE_FAILED, true);
  settings.setValue(lnm::ACTIONS_SHOW_ZOOM_WARNING, true);
  settings.setValue(lnm::ACTIONS_SHOW_REPLACE_TRAIL, true);

  settings.setValue(lnm::ACTIONS_SHOW_ROUTE_ALTERNATE_ERROR, true);
  settings.setValue(lnm::ACTIONS_SHOW_ROUTE_ERROR, true);
  settings.setValue(lnm::ACTIONS_SHOW_ROUTE_NO_CYCLE_WARNING, true);
  settings.setValue(lnm::ACTIONS_SHOW_ROUTE_NAVDATA_ALL_WARNING, true);
  settings.setValue(lnm::ACTIONS_SHOW_ROUTE_PARKING_WARNING, true);
  settings.setValue(lnm::ACTIONS_SHOW_ROUTE_START_CHANGED, true);
  settings.setValue(lnm::ACTIONS_SHOW_ROUTE_AIRPORT_WARNING, true);
  settings.setValue(lnm::ACTIONS_SHOW_ROUTE_VFR_WARNING, true);

  settings.setValue(lnm::ACTIONS_SHOW_SSL_FAILED, true);
  settings.setValue(lnm::ACTIONS_SHOW_SSL_WARNING_ONLINE, true);
  settings.setValue(lnm::ACTIONS_SHOW_SSL_WARNING_SIMBRIEF, true);
  settings.setValue(lnm::ACTIONS_SHOW_SSL_WARNING_TRACK, true);
  settings.setValue(lnm::ACTIONS_SHOW_SSL_WARNING_WEATHER, true);
  settings.setValue(lnm::ACTIONS_SHOW_SSL_WARNING_WIND, true);
  settings.setValue(lnm::ACTIONS_SHOW_FLIGHTPLAN_WARN_CONVERT, true);

  settings.setValue(lnm::ACTIONS_SHOW_DATABASE_BACKGROUND_HINT, true);
  settings.setValue(lnm::ROUTE_STRING_DIALOG_BACKGROUND_HINT, true);

  settings.setValue(lnm::ACTIONS_INSTALL_XPCONNECT_WARN_XPL, true);
  settings.setValue(lnm::ACTIONS_INSTALL_XPCONNECT_INFO, true);
}

void messages::resetEssentialMessages()
{
  qDebug() << Q_FUNC_INFO;
  atools::settings::Settings& settings = atools::settings::Settings::instance();

  settings.setValue(lnm::ACTIONS_SHOW_DATABASE_OLD, true);
  settings.setValue(lnm::ACTIONS_SHOW_INVALID_PROC_WARNING, true);
  settings.setValue(lnm::ACTIONS_SHOW_MAPTHEME_REQUIRES_KEY, true);
  settings.setValue(lnm::ACTIONS_SHOW_NAVDATA_WARNING, true);
  settings.setValue(lnm::ACTIONS_SHOW_OVERWRITE_DATABASE, true);
  settings.setValue(lnm::ACTIONS_SHOW_TRACK_DOWNLOAD_FAIL, true);
  settings.setValue(lnm::ACTIONS_SHOW_WEATHER_DOWNLOAD_FAIL, true);
  settings.setValue(lnm::ACTIONS_SHOW_MISSING_SIMULATORS, true);
  settings.setValue(lnm::ACTIONS_SHOW_CORRECT_MSFS_HAS_NAVIGRAPH, true);
  settings.setValue(lnm::ACTIONS_SHOW_CORRECT_MSFS_NO_NAVIGRAPH, true);
  settings.setValue(lnm::ACTIONS_SHOW_CORRECT_XP_CYCLE_NAV_SMALLER, true);
  settings.setValue(lnm::ACTIONS_SHOW_CORRECT_XP_CYCLE_NAV_EQUAL, true);
  settings.setValue(lnm::ACTIONS_SHOW_CORRECT_FSX_P3D_UPDATED, true);
  settings.setValue(lnm::ACTIONS_SHOW_CORRECT_FSX_P3D_OUTDATED, true);
  settings.setValue(lnm::ACTIONS_SHOW_DATABASE_MSFS_NAVIGRAPH_ALL, true);
  settings.setValue(lnm::ACTIONS_SHOW_FILE_EXPORT_OPTIONS_WARN, true);
  settings.setValue(lnm::ACTIONS_SHOW_FILE_EXPORT_OPTIONS_WARN_EXP, true);
  settings.setValue(lnm::ACTIONS_SHOW_XP11_WEATHER_FILE_INVALID, true);
  settings.setValue(lnm::ACTIONS_SHOW_XP12_WEATHER_FILE_INVALID, true);
  settings.setValue(lnm::ACTIONS_SHOW_XP11_WEATHER_FILE_NO_SIM, true);
  settings.setValue(lnm::ACTIONS_SHOW_XP12_WEATHER_FILE_NO_SIM, true);
  settings.setValue(lnm::ACTIONS_SHOW_DATABASE_HINTS, true);
  settings.setValue(lnm::ACTIONS_SHOW_TRAIL_POINTS, true);
  settings.setValue(lnm::ACTIONS_SHOW_DATABASE_SIMCONNECT, true);
}
