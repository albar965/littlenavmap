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

#ifndef LITTLENAVMAP_CONSTANTS_H
#define LITTLENAVMAP_CONSTANTS_H

#include <QLatin1Literal>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-const-variable"

/* Define to skip caching of approaches when loading */
// #define DEBUG_APPROACH_NO_CACHE

/* Paint additional information useful for debugging for approaches */
// #define DEBUG_APPROACH_PAINT
// #define DEBUG_ROUTE_PAINT

/* Use Shift+Ctrl-Mousemove to simulate an aircraft */
// #define DEBUG_MOVING_AIRPLANE

/* Print the main window state as a hex dump into the log */
// #define DEBUG_CREATE_WINDOW_STATE

/* Show database IDs in the information window and tooltips */
// #define DEBUG_INFORMATION

/* Force updates to always show a notification for testing */
// #define DEBUG_UPDATE

/* Use local URL for update checks for testing */
// #define DEBUG_UPDATE_URL

namespace lnm {

// ======== URLs ================================================================
// "master" or "release/1.4"
const QLatin1Literal HELP_BRANCH("release/2.0"); // VERSION_NUMBER

/* Important: keep slash at the end. Otherwise Gitbook will not display the page properly */
const QString HELP_ONLINE_URL(
  "https://albar965.gitbooks.io/little-navmap-user-manual/content/v/" + HELP_BRANCH + "/${LANG}/");

const QString HELP_ONLINE_TUTORIALS_URL(
  "https://albar965.gitbooks.io/little-navmap-user-manual/content/v/" + HELP_BRANCH + "/${LANG}/TUTORIALS.html");

const QString HELP_ONLINE_LEGEND_URL(
  "https://albar965.gitbooks.io/little-navmap-user-manual/content/v/" + HELP_BRANCH + "/${LANG}/LEGEND.html");

const QLatin1Literal HELP_LEGEND_INLINE_FILE("help/legend-${LANG}.html");

const QLatin1Literal HELP_OFFLINE_FILE("help/little-navmap-user-manual-${LANG}.pdf");

const QLatin1Literal HELP_DONTATE_URL("https://albar965.github.io/donate.html");

const QLatin1Literal OPTIONS_UPDATE_DEFAULT_URL("https://albar965.github.io/littlenavmap-version");

// ======== Options ================================================================

/* State of "do not show again" dialog buttons */
const QLatin1Literal ACTIONS_SHOW_DISCONNECT_INFO("Actions/ShowDisconnectInfo");
const QLatin1Literal ACTIONS_SHOW_LOAD_FLP_WARN("Actions/ShowLoadFlpWarn");
const QLatin1Literal ACTIONS_SHOW_QUIT("Actions/ShowQuit");
const QLatin1Literal ACTIONS_SHOW_INVALID_PROC_WARNING("Actions/ShowInvalidProcedure");
const QLatin1Literal ACTIONS_SHOW_RESET_VIEW("Actions/ShowResetView");
const QLatin1Literal ACTIONS_SHOWROUTE_PARKING_WARNING("Actions/ShowRouteParkingWarning");
const QLatin1Literal ACTIONS_SHOWROUTE_WARNING("Actions/ShowRouteWarning");
const QLatin1Literal ACTIONS_SHOWROUTE_NO_CYCLE_WARNING("Actions/ShowRouteNoCycleWarning");
const QLatin1Literal ACTIONS_SHOWROUTE_ERROR("Actions/ShowRouteError");
const QLatin1Literal ACTIONS_SHOWROUTE_PROC_ERROR("Actions/ShowRouteProcedureError");
const QLatin1Literal ACTIONS_SHOWROUTE_START_CHANGED("Actions/ShowRouteStartChanged");
const QLatin1Literal ACTIONS_SHOW_UPDATEFAILED("Actions/ShowUpdateFailed");
const QLatin1Literal ACTIONS_SHOW_OVERWRITE_DATABASE("Actions/ShowOverwriteDatabase");

const QLatin1Literal ACTIONS_SHOW_FS9_FSC_WARNING("Actions/ShowFs9Warning");
const QLatin1Literal ACTIONS_SHOW_FLP_WARNING("Actions/ShowFlpWarning");
const QLatin1Literal ACTIONS_SHOW_FMS3_WARNING("Actions/ShowFms3Warning");
const QLatin1Literal ACTIONS_SHOW_FMS11_WARNING("Actions/ShowFms11Warning");

/* Other setting key names */
const QLatin1Literal DATABASE_BASEPATH("Database/BasePath");
const QLatin1Literal DATABASE_LOADINGSIMULATOR("Database/LoadingSimulator");
const QLatin1Literal DATABASE_PATHS("Database/Paths");
const QLatin1Literal DATABASE_USE_NAV("Database/UseNav");
const QLatin1Literal DATABASE_SCENERYCONFIG("Database/SceneryConfig");
const QLatin1Literal DATABASE_SIMULATOR("Database/Simulator");
const QLatin1Literal DATABASE_LOAD_INACTIVE("Database/LoadInactive");
const QLatin1Literal DATABASE_LOAD_ADDONXML("Database/LoadAddOnXml");

const QLatin1Literal EXPORT_FILEDIALOG("Export/FileDialog");
const QLatin1Literal INFOWINDOW_CURRENTMAPOBJECTS("InfoWindow/CurrentMapObjects");
const QLatin1Literal INFOWINDOW_WIDGET("InfoWindow/Widget");
const QLatin1Literal MAINWINDOW_FIRSTAPPLICATIONSTART("MainWindow/FirstApplicationStart");
const QLatin1Literal MAINWINDOW_WIDGET("MainWindow/Widget");
const QLatin1Literal MAINWINDOW_WIDGET_STATE("MainWindow/WidgetState");
const QLatin1Literal MAINWINDOW_WIDGET_STATE_POS("MainWindow/WidgetStatePosition");
const QLatin1Literal MAINWINDOW_WIDGET_STATE_SIZE("MainWindow/WidgetStateSize");
const QLatin1Literal MAINWINDOW_WIDGET_STATE_MAXIMIZED("MainWindow/WidgetStateMaximized");
const QLatin1Literal MAINWINDOW_PRINT_SIZE("MainWindow/PrintPreviewSize");
const QLatin1Literal MAP_DETAILFACTOR("Map/DetailFactor");
const QLatin1Literal MAP_DISTANCEMARKERS("Map/DistanceMarkers");
const QLatin1Literal MAP_AIRSPACES("Map/AirspaceFilter");
const QLatin1Literal MAP_USERDATA("Map/Userdata");
const QLatin1Literal MAP_USERDATA_UNKNOWN("Map/UserdataUnknown");
const QLatin1Literal MAP_HOMEDISTANCE("Map/HomeDistance");
const QLatin1Literal MAP_HOMELATY("Map/HomeLatY");
const QLatin1Literal MAP_HOMELONX("Map/HomeLonX");
const QLatin1Literal MAP_KMLFILES("Map/KmlFiles");
const QLatin1Literal MAP_MARKLATY("Map/MarkLatY");
const QLatin1Literal MAP_MARKLONX("Map/MarkLonX");
const QLatin1Literal MAP_RANGEMARKERS("Map/RangeMarkers");
const QLatin1Literal MAP_OVERLAY_VISIBLE("Map/OverlayVisible");
const QLatin1Literal NAVCONNECT_REMOTEHOSTS("NavConnect/RemoteHosts");
const QLatin1Literal NAVCONNECT_REMOTE("NavConnect/Remote");
const QLatin1Literal ROUTE_FILENAME("Route/Filename");
const QLatin1Literal ROUTE_FILENAMESRECENT("Route/FilenamesRecent");
const QLatin1Literal ROUTE_FILENAMESKMLRECENT("Route/FilenamesKmlRecent");
const QLatin1Literal ROUTE_VIEW("Route/View");
const QLatin1Literal ROUTE_PRINT_DIALOG("Route/PrintWidget");
const QLatin1Literal ROUTE_STRING_DIALOG_SIZE("Route/StringDialogSize");
const QLatin1Literal ROUTE_STRING_DIALOG_SPLITTER("Route/StringDialogSplitter");
const QLatin1Literal ROUTE_STRING_DIALOG_OPTIONS("Route/StringDialogOptions");
const QLatin1Literal SEARCHTAB_AIRPORT_WIDGET("SearchPaneAirport/Widget");
const QLatin1Literal SEARCHTAB_NAV_WIDGET("SearchPaneNav/Widget");
const QLatin1Literal SEARCHTAB_AIRPORT_VIEW_WIDGET("SearchPaneAirport/WidgetView");
const QLatin1Literal SEARCHTAB_AIRPORT_VIEW_DIST_WIDGET("SearchPaneAirport/WidgetDistView");
const QLatin1Literal SEARCHTAB_NAV_VIEW_WIDGET("SearchPaneNav/WidgetView");
const QLatin1Literal SEARCHTAB_NAV_VIEW_DIST_WIDGET("SearchPaneNav/WidgetDistView");
const QLatin1Literal SEARCHTAB_USERDATA_VIEW_WIDGET("SearchPaneUserdata/WidgetView");

const QLatin1Literal SEARCHTAB_ONLINE_CLIENT_VIEW_WIDGET("SearchPaneOnlineClient/WidgetView");
const QLatin1Literal SEARCHTAB_ONLINE_CENTER_VIEW_WIDGET("SearchPaneOnlineCenter/WidgetView");
const QLatin1Literal SEARCHTAB_ONLINE_SERVER_VIEW_WIDGET("SearchPaneOnlineServer/WidgetView");

/* General settings in the configuration file not covered by any GUI elements */
const QLatin1Literal SETTINGS_INFOQUERY("Settings/InfoQuery");
const QLatin1Literal SETTINGS_MAPQUERY("Settings/MapQuery");
const QLatin1Literal SETTINGS_DATABASE("Settings/Database");

const QLatin1Literal APPROACHTREE_WIDGET("ApproachTree/Widget");
const QLatin1Literal APPROACHTREE_SELECTED_WIDGET("ApproachTree/WidgetSelected");
const QLatin1Literal APPROACHTREE_STATE("ApproachTree/TreeState");
const QLatin1Literal APPROACHTREE_AIRPORT("ApproachTree/Airport");
const QLatin1Literal APPROACHTREE_SELECTED_APPR("ApproachTree/SeletedApproach");

/* Export settings dialog */
const QLatin1Literal USERDATA_EXPORT_DIALOG("UserdataExport/Widget");
/* Edit dialog */
const QLatin1Literal USERDATA_EDIT_ADD_DIALOG("UserdataDialog/Widget");

/* Options dialog */
const QLatin1Literal OPTIONS_DIALOG_WIDGET("OptionsDialog/Widget");
const QLatin1Literal OPTIONS_DIALOG_AS_FILE_DLG("OptionsDialog/WeatherFileDialogAsn");
const QLatin1Literal OPTIONS_DIALOG_GLOBE_FILE_DLG("OptionsDialog/CacheFileDialogGlobe");
const QLatin1Literal OPTIONS_DIALOG_DB_FILE_DLG("OptionsDialog/DatabaseFileDialog");
const QLatin1Literal OPTIONS_DIALOG_DB_EXCLUDE("OptionsDialog/DatabaseExclude");
const QLatin1Literal OPTIONS_DIALOG_DB_ADDON_EXCLUDE("OptionsDialog/DatabaseAddonExclude");
const QLatin1Literal OPTIONS_DIALOG_FLIGHTPLAN_COLOR("OptionsDialog/FlightplanColor");
const QLatin1Literal OPTIONS_DIALOG_FLIGHTPLAN_PROCEDURE_COLOR("OptionsDialog/FlightplanProcedureColor");
const QLatin1Literal OPTIONS_DIALOG_FLIGHTPLAN_ACTIVE_COLOR("OptionsDialog/FlightplanActiveColor");
const QLatin1Literal OPTIONS_DIALOG_FLIGHTPLAN_PASSED_COLOR("OptionsDialog/FlightplanPassedColor");
const QLatin1Literal OPTIONS_DIALOG_TRAIL_COLOR("OptionsDialog/TrailColor");
const QLatin1Literal OPTIONS_DIALOG_DISPLAY_OPTIONS("OptionsDialog/DisplayOptions");
const QLatin1Literal OPTIONS_DIALOG_GUI_STYLE_INDEX("OptionsDialog/GuiStyleIndex");
const QLatin1Literal OPTIONS_DIALOG_WARN_STYLE("OptionsDialog/StyleWarning");

/* Other options that are only accessible in the configuration file */
const QLatin1Literal OPTIONS_LANGUAGE("Options/Language");
const QLatin1Literal OPTIONS_MARBLE_DEBUG("Options/MarbleDebug");
const QLatin1Literal OPTIONS_CONNECTCLIENT_DEBUG("Options/ConnectClientDebug");
const QLatin1Literal OPTIONS_DATAREADER_DEBUG("Options/DataReaderDebug");
const QLatin1Literal OPTIONS_VERSION("Options/Version");
const QLatin1Literal OPTIONS_NO_USER_AGENT("Options/NoUserAgent");
const QLatin1Literal OPTIONS_WEATHER_UPDATE("Options/WeatherUpdate");

/* Used to override  default URL */
const QLatin1Literal OPTIONS_UPDATE_URL("Update/Url");

const QLatin1Literal OPTIONS_UPDATE_CHECKED("Update/AlreadyChecked");
const QLatin1Literal OPTIONS_UPDATE_LAST_CHECKED("Update/LastCheckTimestamp");

/* Need to update these according to program version */
const QLatin1Literal OPTIONS_UPDATE_CHANNELS("OptionsDialog/Widget_comboBoxOptionsStartupUpdateChannels");
const QLatin1Literal OPTIONS_UPDATE_RATE("OptionsDialog/Widget_comboBoxOptionsStartupUpdateRate");

/* These have to be loaded before the options dialog instantiation */
const QLatin1Literal OPTIONS_GUI_OVERRIDE_LANGUAGE("OptionsDialog/Widget_checkBoxOptionsGuiOverrideLanguage");
const QLatin1Literal OPTIONS_GUI_OVERRIDE_LOCALE("OptionsDialog/Widget_checkBoxOptionsGuiOverrideLocale");

/* File dialog patterns */
#if defined(Q_OS_WIN32)
const QLatin1Literal FILE_PATTERN_SCENERYCONFIG("(*.cfg)");
const QLatin1Literal FILE_PATTERN_FLIGHTPLAN_LOAD("(*.pln *.flp *.fms)");
const QLatin1Literal FILE_PATTERN_FLIGHTPLAN_SAVE("(*.pln)");
const QLatin1Literal FILE_PATTERN_GFP("(*.gfp)");
const QLatin1Literal FILE_PATTERN_TXT("(*.txt)");
const QLatin1Literal FILE_PATTERN_RTE("(*.rte)");
const QLatin1Literal FILE_PATTERN_FPR("(*.fpr)");
const QLatin1Literal FILE_PATTERN_FPL("(*.fpl)");
const QLatin1Literal FILE_PATTERN_CORTEIN("(corte.in)");
const QLatin1Literal FILE_PATTERN_FLP("(*.flp)");
const QLatin1Literal FILE_PATTERN_FMS("(*.fms)");
const QLatin1Literal FILE_PATTERN_UFMC("(*.ufmc)");
const QLatin1Literal FILE_PATTERN_COMPANYROUTES_XML("(companyroutes.xml)");
const QLatin1Literal FILE_PATTERN_FLTPLAN("(*.fltplan)");
const QLatin1Literal FILE_PATTERN_BBS_PLN("(*.pln)");

const QLatin1Literal FILE_PATTERN_GPX("(*.gpx)");
const QLatin1Literal FILE_PATTERN_KML("(*.kml *.kmz)");

const QLatin1Literal FILE_PATTERN_USERDATA_CSV("(*.csv)");
const QLatin1Literal FILE_PATTERN_USER_FIX_DAT("(user_fix.dat)");
const QLatin1Literal FILE_PATTERN_USER_WPT("(user.wpt)");
const QLatin1Literal FILE_PATTERN_BGL_XML("(*.xml)");
#else
/* Use more or less case insensitive patterns for Linux */
const QLatin1Literal FILE_PATTERN_SCENERYCONFIG("(*.cfg *.Cfg *.CFG)");
const QLatin1Literal FILE_PATTERN_FLIGHTPLAN_LOAD("(*.pln *.Pln *.PLN *.flp *.Flp *.FLP *.fms *.Fms *.FMS)");
const QLatin1Literal FILE_PATTERN_FLIGHTPLAN_SAVE("(*.pln *.Pln *.PLN)");
const QLatin1Literal FILE_PATTERN_GFP("(*.gfp *.Gfp *.GFP)");
const QLatin1Literal FILE_PATTERN_TXT("(*.txt *.Txt *.TXT)");
const QLatin1Literal FILE_PATTERN_RTE("(*.rte *.Rte *.RTE)");
const QLatin1Literal FILE_PATTERN_FPR("(*.fpr *.Fpr *.FPR)");
const QLatin1Literal FILE_PATTERN_FPL("(*.fpl *.Fpl *.FPL)");
const QLatin1Literal FILE_PATTERN_CORTEIN("(corte.in Corte.in CORTE.IN)");
const QLatin1Literal FILE_PATTERN_FLP("(*.flp *.Flp *.FLP)");
const QLatin1Literal FILE_PATTERN_FMS("(*.fms *.Fms *.FMS)");
const QLatin1Literal FILE_PATTERN_UFMC("(*.ufmc *.Ufmc *.UFMC)");
const QLatin1Literal FILE_PATTERN_COMPANYROUTES_XML("(companyroutes.xml Companyroutes.xml COMPANYROUTES.XML)");
const QLatin1Literal FILE_PATTERN_FLTPLAN("(*.fltplan *.Fltplan *.FLTPLAN)");
const QLatin1Literal FILE_PATTERN_BBS_PLN("(*.pln *.Pln *.PLN)");

const QLatin1Literal FILE_PATTERN_GPX("(*.gpx *.Gpx *.GPX)");
const QLatin1Literal FILE_PATTERN_KML("(*.kml *.KML *.kmz *.KMZ)");

const QLatin1Literal FILE_PATTERN_USERDATA_CSV("(*.csv *.Csv *.CSV)");
const QLatin1Literal FILE_PATTERN_USER_FIX_DAT("(user_fix.dat)");
const QLatin1Literal FILE_PATTERN_USER_WPT("(user.wpt)");
const QLatin1Literal FILE_PATTERN_BGL_XML("(*.xml *.Xml *.XML)");
#endif
const QString FILE_PATTERN_AS_SNAPSHOT("(current_wx_snapshot.txt)");

const QString FILE_PATTERN_IMAGE("(*.jpg *.jpeg *.png *.bmp)");

/* Sqlite database names */
const QLatin1Literal DATABASE_DIR("little_navmap_db");
const QLatin1Literal DATABASE_PREFIX("little_navmap_");
const QLatin1Literal DATABASE_SUFFIX(".sqlite");
const QLatin1Literal DATABASE_BACKUP_SUFFIX("-backup");

/* This is the default configuration file for reading the scenery library.
 * It can be overridden by placing a  file with the same name into
 * the configuration directory. */
const QString DATABASE_NAVDATAREADER_CONFIG = ":/littlenavmap/resources/config/navdatareader.cfg";

const int MAINWINDOW_STATE_VERSION = 0;

/* Main window state for first startup. Generated in MainWindow::writeSettings() */
const unsigned char DEFAULT_MAINWINDOW_STATE[841] =
{
  0x0, 0x0, 0x0, 0xff, 0x0, 0x0, 0x0, 0x0, 0xfd, 0x0, 0x0, 0x0, 0x2, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1, 0x84, 0x0, 0x0,
  0x3, 0x10, 0xfc, 0x2, 0x0, 0x0, 0x0, 0x2, 0xfb, 0x0, 0x0, 0x0, 0x1e, 0x0, 0x64, 0x0, 0x6f, 0x0, 0x63, 0x0, 0x6b, 0x0,
  0x57, 0x0, 0x69, 0x0, 0x64, 0x0, 0x67, 0x0, 0x65, 0x0, 0x74, 0x0, 0x52, 0x0, 0x6f, 0x0, 0x75, 0x0, 0x74, 0x0, 0x65,
  0x1, 0x0, 0x0, 0x0, 0x61, 0x0, 0x0, 0x1, 0x81, 0x0, 0x0, 0x0, 0x83, 0x0, 0xff, 0xff, 0xff, 0xfb, 0x0, 0x0, 0x0, 0x2a,
  0x0, 0x64, 0x0, 0x6f, 0x0, 0x63, 0x0, 0x6b, 0x0, 0x57, 0x0, 0x69, 0x0, 0x64, 0x0, 0x67, 0x0, 0x65, 0x0, 0x74, 0x0,
  0x49, 0x0, 0x6e, 0x0, 0x66, 0x0, 0x6f, 0x0, 0x72, 0x0, 0x6d, 0x0, 0x61, 0x0, 0x74, 0x0, 0x69, 0x0, 0x6f, 0x0, 0x6e,
  0x1, 0x0, 0x0, 0x1, 0xe8, 0x0, 0x0, 0x1, 0x89, 0x0, 0x0, 0x0, 0x7b, 0x0, 0xff, 0xff, 0xff, 0x0, 0x0, 0x0, 0x1, 0x0,
  0x0, 0x3, 0x76, 0x0, 0x0, 0x3, 0x10, 0xfc, 0x2, 0x0, 0x0, 0x0, 0x2, 0xfc, 0x0, 0x0, 0x0, 0x61, 0x0, 0x0, 0x3, 0x10,
  0x0, 0x0, 0x2, 0x70, 0x0, 0xff, 0xff, 0xff, 0xfc, 0x2, 0x0, 0x0, 0x0, 0x2, 0xfc, 0x0, 0x0, 0x0, 0x61, 0x0, 0x0, 0x2,
  0x6e, 0x0, 0x0, 0x2, 0x1f, 0x0, 0xff, 0xff, 0xff, 0xfc, 0x1, 0x0, 0x0, 0x0, 0x2, 0xfb, 0x0, 0x0, 0x0, 0x1a, 0x0,
  0x64, 0x0, 0x6f, 0x0, 0x63, 0x0, 0x6b, 0x0, 0x57, 0x0, 0x69, 0x0, 0x64, 0x0, 0x67, 0x0, 0x65, 0x0, 0x74, 0x0, 0x4d,
  0x0, 0x61, 0x0, 0x70, 0x1, 0x0, 0x0, 0x1, 0x8a, 0x0, 0x0, 0x1, 0x43, 0x0, 0x0, 0x0, 0x38, 0x0, 0xff, 0xff, 0xff,
  0xfc, 0x0, 0x0, 0x2, 0xd3, 0x0, 0x0, 0x2, 0x2d, 0x0, 0x0, 0x2, 0x2d, 0x0, 0xff, 0xff, 0xff, 0xfc, 0x2, 0x0, 0x0, 0x0,
  0x2, 0xfb, 0x0, 0x0, 0x0, 0x20, 0x0, 0x64, 0x0, 0x6f, 0x0, 0x63, 0x0, 0x6b, 0x0, 0x57, 0x0, 0x69, 0x0, 0x64, 0x0,
  0x67, 0x0, 0x65, 0x0, 0x74, 0x0, 0x53, 0x0, 0x65, 0x0, 0x61, 0x0, 0x72, 0x0, 0x63, 0x0, 0x68, 0x1, 0x0, 0x0, 0x0,
  0x61, 0x0, 0x0, 0x1, 0xdb, 0x0, 0x0, 0x1, 0x9e, 0x0, 0xff, 0xff, 0xff, 0xfb, 0x0, 0x0, 0x0, 0x24, 0x0, 0x64, 0x0,
  0x6f, 0x0, 0x63, 0x0, 0x6b, 0x0, 0x57, 0x0, 0x69, 0x0, 0x64, 0x0, 0x67, 0x0, 0x65, 0x0, 0x74, 0x0, 0x41, 0x0, 0x69,
  0x0, 0x72, 0x0, 0x63, 0x0, 0x72, 0x0, 0x61, 0x0, 0x66, 0x0, 0x74, 0x1, 0x0, 0x0, 0x2, 0x42, 0x0, 0x0, 0x0, 0x8d, 0x0,
  0x0, 0x0, 0x7b, 0x0, 0xff, 0xff, 0xff, 0xfb, 0x0, 0x0, 0x0, 0x26, 0x0, 0x64, 0x0, 0x6f, 0x0, 0x63, 0x0, 0x6b, 0x0,
  0x57, 0x0, 0x69, 0x0, 0x64, 0x0, 0x67, 0x0, 0x65, 0x0, 0x74, 0x0, 0x45, 0x0, 0x6c, 0x0, 0x65, 0x0, 0x76, 0x0, 0x61,
  0x0, 0x74, 0x0, 0x69, 0x0, 0x6f, 0x0, 0x6e, 0x1, 0x0, 0x0, 0x2, 0xd5, 0x0, 0x0, 0x0, 0x9c, 0x0, 0x0, 0x0, 0x4b, 0x0,
  0xff, 0xff, 0xff, 0xfb, 0x0, 0x0, 0x0, 0x20, 0x0, 0x64, 0x0, 0x6f, 0x0, 0x63, 0x0, 0x6b, 0x0, 0x57, 0x0, 0x69, 0x0,
  0x64, 0x0, 0x67, 0x0, 0x65, 0x0, 0x74, 0x0, 0x4c, 0x0, 0x65, 0x0, 0x67, 0x0, 0x65, 0x0, 0x6e, 0x0, 0x64, 0x2, 0x0,
  0x0, 0x2, 0x44, 0x0, 0x0, 0x0, 0x68, 0x0, 0x0, 0x2, 0x4e, 0x0, 0x0, 0x2, 0xcc, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x3,
  0x10, 0x0, 0x0, 0x0, 0x4, 0x0, 0x0, 0x0, 0x4, 0x0, 0x0, 0x0, 0x8, 0x0, 0x0, 0x0, 0x8, 0xfc, 0x0, 0x0, 0x0, 0x3, 0x0,
  0x0, 0x0, 0x2, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x2, 0x0, 0x0, 0x0, 0x3, 0x0, 0x0, 0x0, 0x16, 0x0, 0x74, 0x0, 0x6f,
  0x0, 0x6f, 0x0, 0x6c, 0x0, 0x42, 0x0, 0x61, 0x0, 0x72, 0x0, 0x4d, 0x0, 0x61, 0x0, 0x69, 0x0, 0x6e, 0x1, 0x0, 0x0,
  0x0, 0x0, 0xff, 0xff, 0xff, 0xff, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x14, 0x0, 0x74, 0x0, 0x6f,
  0x0, 0x6f, 0x0, 0x6c, 0x0, 0x42, 0x0, 0x61, 0x0, 0x72, 0x0, 0x4d, 0x0, 0x61, 0x0, 0x70, 0x1, 0x0, 0x0, 0x0, 0x73,
  0xff, 0xff, 0xff, 0xff, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x22, 0x0, 0x74, 0x0, 0x6f, 0x0, 0x6f,
  0x0, 0x6c, 0x0, 0x62, 0x0, 0x61, 0x0, 0x72, 0x0, 0x4d, 0x0, 0x61, 0x0, 0x70, 0x0, 0x4f, 0x0, 0x70, 0x0, 0x74, 0x0,
  0x69, 0x0, 0x6f, 0x0, 0x6e, 0x0, 0x73, 0x1, 0x0, 0x0, 0x1, 0x78, 0xff, 0xff, 0xff, 0xff, 0x0, 0x0, 0x0, 0x0, 0x0,
  0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x2, 0x0, 0x0, 0x0, 0x3, 0x0, 0x0, 0x0, 0x18, 0x0, 0x74, 0x0, 0x6f, 0x0, 0x6f, 0x0,
  0x6c, 0x0, 0x42, 0x0, 0x61, 0x0, 0x72, 0x0, 0x52, 0x0, 0x6f, 0x0, 0x75, 0x0, 0x74, 0x0, 0x65, 0x1, 0x0, 0x0, 0x0,
  0x0, 0xff, 0xff, 0xff, 0xff, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x20, 0x0, 0x74, 0x0, 0x6f, 0x0,
  0x6f, 0x0, 0x6c, 0x0, 0x42, 0x0, 0x61, 0x0, 0x72, 0x0, 0x41, 0x0, 0x69, 0x0, 0x72, 0x0, 0x73, 0x0, 0x70, 0x0, 0x61,
  0x0, 0x63, 0x0, 0x65, 0x0, 0x73, 0x1, 0x0, 0x0, 0x1, 0xb1, 0xff, 0xff, 0xff, 0xff, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
  0x0, 0x0, 0x0, 0x0, 0x16, 0x0, 0x74, 0x0, 0x6f, 0x0, 0x6f, 0x0, 0x6c, 0x0, 0x42, 0x0, 0x61, 0x0, 0x72, 0x0, 0x56,
  0x0, 0x69, 0x0, 0x65, 0x0, 0x77, 0x1, 0x0, 0x0, 0x2, 0xa8, 0x0, 0x0, 0x2, 0x58, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
  0x0
};

/* Supported languages for the online help system. Will be determined by the
 * installation of offline PDF manual. */
const QStringList helpLanguagesOnline();
const QStringList helpLanguagesOffline();

} // namespace lnm

#pragma GCC diagnostic pop

#endif // LITTLENAVMAP_CONSTANTS_H
