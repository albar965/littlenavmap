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

#ifndef LITTLENAVMAP_CONSTANTS_H
#define LITTLENAVMAP_CONSTANTS_H

#include <QLatin1Literal>
#include <QSize>

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
extern QString helpOnlineUrl;
extern QString helpOnlineTutorialsUrl;
extern QString helpOnlineLegendUrl;
extern QString helpOnlineInstallRedistUrl;
extern QString helpOnlineInstallGlobeUrl;
extern QString helpOnlineNavdatabasesUrl;
extern QString helpLegendLocalFile;
extern QString helpOfflineFile;
extern QString helpDonateUrl;
extern QString helpFaqUrl;
extern QString updateDefaultUrl;

/* Load help URLs for above variables from file urls.cfg */
void loadHelpUrls();

// ======== Options ================================================================

/* State of "do not show again" dialog buttons */
const QLatin1Literal ACTIONS_SHOW_DISCONNECT_INFO("Actions/ShowDisconnectInfo");
const QLatin1Literal ACTIONS_SHOW_LOAD_FLP_WARN("Actions/ShowLoadFlpWarn");
const QLatin1Literal ACTIONS_SHOW_LOAD_FMS_ALT_WARN("Actions/ShowLoadFmsAltitudeWarn");
const QLatin1Literal ACTIONS_SHOW_QUIT("Actions/ShowQuit");
const QLatin1Literal ACTIONS_SHOW_INVALID_PROC_WARNING("Actions/ShowInvalidProcedure");
const QLatin1Literal ACTIONS_SHOW_RESET_VIEW("Actions/ShowResetView");
const QLatin1Literal ACTIONS_SHOWROUTE_PARKING_WARNING("Actions/ShowRouteParkingWarning");
const QLatin1Literal ACTIONS_SHOWROUTE_WARNING("Actions/ShowRouteWarning");
const QLatin1Literal ACTIONS_SHOWROUTE_WARNING_MULTI("Actions/ShowRouteWarningMulti");
const QLatin1Literal ACTIONS_SHOWROUTE_NO_CYCLE_WARNING("Actions/ShowRouteNoCycleWarning");
const QLatin1Literal ACTIONS_SHOWROUTE_ERROR("Actions/ShowRouteError");
const QLatin1Literal ACTIONS_SHOWROUTE_ALTERNATE_ERROR("Actions/ShowRouteAlternateError");
const QLatin1Literal ACTIONS_SHOWROUTE_START_CHANGED("Actions/ShowRouteStartChanged");
const QLatin1Literal ACTIONS_SHOW_UPDATE_FAILED("Actions/ShowUpdateFailed");
const QLatin1Literal ACTIONS_SHOW_SSL_FAILED("Actions/ShowSslFailed");
const QLatin1Literal ACTIONS_SHOW_INSTALL_GLOBE("Actions/ShowInstallGlobe");
const QLatin1Literal ACTIONS_SHOW_OVERWRITE_DATABASE("Actions/ShowOverwriteDatabase");
const QLatin1Literal ACTIONS_SHOW_START_PERF_COLLECTION("Actions/ShowPerfCollection");
const QLatin1Literal ACTIONS_SHOW_DELETE_TRAIL("Actions/DeleteTrail");
const QLatin1Literal ACTIONS_SHOW_DELETE_MARKS("Actions/DeleteMarks");
const QLatin1Literal ACTIONS_SHOW_RESET_PERF("Actions/ResetPerformanceColl");
const QLatin1Literal ACTIONS_SHOW_SEARCH_CENTER_NULL("Actions/SearchCenterNull");
const QLatin1Literal ACTIONS_SHOW_WEATHER_DOWNLOAD_FAIL("Actions/DownloadFailed");
const QLatin1Literal ACTIONS_SHOW_TRACK_DOWNLOAD_FAIL("Actions/TrackDownloadFailed");
const QLatin1Literal ACTIONS_SHOW_TRACK_DOWNLOAD_SUCCESS("Actions/TrackDownloadSuccess");
const QLatin1Literal ACTIONS_SHOW_LOGBOOK_CONVERSION("Actions/LogbookConversion");
const QLatin1Literal ACTIONS_SHOW_USER_AIRSPACE_NOTE("Actions/UserAirspaceNote");

const QLatin1Literal ACTIONS_SHOW_SSL_WARNING_ONLINE("Actions/SslWarningOnline");
const QLatin1Literal ACTIONS_SHOW_SSL_WARNING_WIND("Actions/SslWarningWind");
const QLatin1Literal ACTIONS_SHOW_SSL_WARNING_TRACK("Actions/SslWarningTrack");
const QLatin1Literal ACTIONS_SHOW_SSL_WARNING_WEATHER("Actions/SslWarningWeather");

const QLatin1Literal ACTIONS_SHOW_NAVDATA_WARNING("Actions/ShowNavdataWarning");
const QLatin1Literal ACTIONS_SHOW_CRUISE_ZERO_WARNING("Actions/ShowCruiseZeroWarning");

const QLatin1Literal ACTIONS_SHOW_SAVE_WARNING("Actions/ShowSaveWarning");
const QLatin1Literal ACTIONS_SHOW_ZOOM_WARNING("Actions/ShowZoomsWarning");

/* Other setting key names */
const QLatin1Literal DATABASE_BASEPATH("Database/BasePath");
const QLatin1Literal DATABASE_LOADINGSIMULATOR("Database/LoadingSimulator");
const QLatin1Literal DATABASE_PATHS("Database/Paths");
const QLatin1Literal DATABASE_USE_NAV("Database/UseNav");
const QLatin1Literal DATABASE_SCENERYCONFIG("Database/SceneryConfig");
const QLatin1Literal DATABASE_SIMULATOR("Database/Simulator");
const QLatin1Literal DATABASE_LOAD_INACTIVE("Database/LoadInactive");
const QLatin1Literal DATABASE_LOAD_ADDONXML("Database/LoadAddOnXml");
const QLatin1Literal DATABASE_USER_AIRSPACE_PATH("Database/UserAirspacePath");

const QLatin1Literal EXPORT_FILEDIALOG("Export/FileDialog");
const QLatin1Literal INFOWINDOW_CURRENTMAPOBJECTS("InfoWindow/CurrentMapObjects");
const QLatin1Literal INFOWINDOW_CURRENTAIRSPACES("InfoWindow/CurrentMapAirspaces");
const QLatin1Literal INFOWINDOW_WIDGET("InfoWindow/Widget");
const QLatin1Literal INFOWINDOW_WIDGET_TABS("InfoWindow/WidgetTabs");
const QLatin1Literal INFOWINDOW_WIDGET_AIRPORT_TABS("InfoWindow/WidgetAirportTabs");
const QLatin1Literal INFOWINDOW_WIDGET_AIRCRAFT_TABS("InfoWindow/WidgetAircraftTabs");
const QLatin1Literal INFOWINDOW_MORE_LESS_PROGRESS("InfoWindow/MoreLessProgress");
const QLatin1Literal MAINWINDOW_FIRSTAPPLICATIONSTART("MainWindow/FirstApplicationStart");
const QLatin1Literal MAINWINDOW_WIDGET("MainWindow/Widget");
const QLatin1Literal MAINWINDOW_WIDGET_DOCKHANDLER("MainWindow/WidgetDockHandler");
const QLatin1Literal MAINWINDOW_PRINT_SIZE("MainWindow/PrintPreviewSize");
const QLatin1Literal MAP_DETAILFACTOR("Map/DetailFactor");
const QLatin1Literal MAP_DISTANCEMARKERS("Map/DistanceMarkers");
const QLatin1Literal MAP_TRAFFICPATTERNS("Map/TrafficPatterns");
const QLatin1Literal MAP_HOLDS("Map/Holds");
const QLatin1Literal MAP_AIRSPACES("Map/AirspaceFilter");
const QLatin1Literal MAP_USERDATA("Map/Userdata");
const QLatin1Literal MAP_USERDATA_UNKNOWN("Map/UserdataUnknown");
const QLatin1Literal MAP_WIND_LEVEL("Map/WindLevel");
const QLatin1Literal MAP_WIND_LEVEL_ROUTE("Map/WindLevelRoute");
const QLatin1Literal MAP_WIND_SOURCE("Map/WindSource");
const QLatin1Literal MAP_HOMEDISTANCE("Map/HomeDistance");
const QLatin1Literal MAP_HOMELATY("Map/HomeLatY");
const QLatin1Literal MAP_HOMELONX("Map/HomeLonX");
const QLatin1Literal MAP_KMLFILES("Map/KmlFiles");
const QLatin1Literal MAP_MARKLATY("Map/MarkLatY");
const QLatin1Literal MAP_MARKLONX("Map/MarkLonX");
const QLatin1Literal MAP_RANGEMARKERS("Map/RangeMarkers");
const QLatin1Literal MAP_OVERLAY_VISIBLE("Map/OverlayVisible");
const QLatin1Literal MAP_WEATHER_SOURCE("Map/WeatherSource");
const QLatin1Literal MAP_SUN_SHADING_TIME_OPTION("Map/SunShadingTimeOption");
const QLatin1Literal MAP_SUN_SHADING_TIME("Map/SunShadingTime");
const QLatin1Literal MAP_MARK_DISPLAY("Map/MarkDisplay");

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
const QLatin1Literal ROUTEWINDOW_WIDGET_TABS("Route/WidgetTabs");
const QLatin1Literal TRAFFIC_PATTERN_DIALOG("Route/TrafficPatternDialog");
const QLatin1Literal TRAFFIC_PATTERN_DIALOG_COLOR("Route/TrafficPatternDialogColor");
const QLatin1Literal HOLD_DIALOG("Route/HoldDialog");
const QLatin1Literal ROUTE_EXPORT_DIALOG("RouteExport/RouteExportDialog");
const QLatin1Literal ROUTE_EXPORT_DIALOG_ZOOM("RouteExport/RouteExportDialogZoom");
const QLatin1Literal ROUTE_EXPORT_FORMATS("RouteExport/RouteExportFormats");
const QLatin1Literal HOLD_DIALOG_COLOR("Route/HoldDialogColor");
const QLatin1Literal CUSTOM_PROCEDURE_DIALOG("Route/CustomProcedureDialog");
const QLatin1Literal ROUTE_CALC_DIALOG("Route/RouteCalcDialog");
const QLatin1Literal SEARCHTAB_AIRPORT_WIDGET("SearchPaneAirport/Widget");
const QLatin1Literal SEARCHTAB_WIDGET_TABS("SearchPaneAirport/WidgetTabs");
const QLatin1Literal SEARCHTAB_NAV_WIDGET("SearchPaneNav/Widget");
const QLatin1Literal SEARCHTAB_AIRPORT_VIEW_WIDGET("SearchPaneAirport/WidgetView");
const QLatin1Literal SEARCHTAB_AIRPORT_VIEW_DIST_WIDGET("SearchPaneAirport/WidgetDistView");
const QLatin1Literal SEARCHTAB_NAV_VIEW_WIDGET("SearchPaneNav/WidgetView");
const QLatin1Literal SEARCHTAB_NAV_VIEW_DIST_WIDGET("SearchPaneNav/WidgetDistView");
const QLatin1Literal SEARCHTAB_USERDATA_VIEW_WIDGET("SearchPaneUserdata/WidgetView");
const QLatin1Literal SEARCHTAB_LOGDATA_VIEW_WIDGET("SearchPaneLogdata/WidgetView");

const QLatin1Literal IMAGE_EXPORT_DIALOG("Map/ImageExportDialog");
const QLatin1Literal IMAGE_EXPORT_AVITAB_DIALOG("Map/ImageExportDialogAviTab");

const QLatin1Literal RESET_FOR_NEW_FLIGHT_DIALOG("Route/ResetAllDialog");
const QLatin1Literal ROUTE_FLIGHTPLAN_COLUMS_DIALOG("Route/FlightPlanTableColumns");

const QLatin1Literal PROFILE_WINDOW_OPTIONS("Profile/Options");

const QLatin1Literal SEARCHTAB_ONLINE_CLIENT_VIEW_WIDGET("SearchPaneOnlineClient/WidgetView");
const QLatin1Literal SEARCHTAB_ONLINE_CENTER_VIEW_WIDGET("SearchPaneOnlineCenter/WidgetView");
const QLatin1Literal SEARCHTAB_ONLINE_SERVER_VIEW_WIDGET("SearchPaneOnlineServer/WidgetView");

const QLatin1Literal AIRCRAFT_PERF_FILENAME("AircraftPerformance/Filename");
const QLatin1Literal AIRCRAFT_PERF_WIDGETS("AircraftPerformance/Widget");
const QLatin1Literal AIRCRAFT_PERF_FILENAMESRECENT("AircraftPerformance/FilenamesRecent");
const QLatin1Literal AIRCRAFT_PERF_EDIT_DIALOG("AircraftPerformance/EditDialog");
const QLatin1Literal AIRCRAFT_PERF_MERGE_DIALOG("AircraftPerformance/MergeDialog");

const QLatin1Literal AIRSPACE_CONTROLLER_WIDGETS("AirspaceController/Widget");

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
const QLatin1Literal USERDATA_EXPORT_CHOICE_DIALOG("UserdataExport/ChoiceDialog");
/* Edit dialog */
const QLatin1Literal USERDATA_EDIT_ADD_DIALOG("UserdataDialog/Widget");

const QLatin1Literal ROUTE_USERWAYPOINT_DIALOG("Route/UserWaypointDialog");

const QLatin1Literal TIME_DIALOG("Map/TimeDialog");

/* Flightplan export dialog for online formats */
const QLatin1Literal FLIGHTPLAN_ONLINE_EXPORT("Route/FlightplanOnlineExport");
const QLatin1Literal ROUTE_PARKING_DIALOG("Route/ParkingDialog");

const QLatin1Literal LOGDATA_EDIT_ADD_DIALOG("LogdataDialog/Widget");
const QLatin1Literal LOGDATA_STATS_DIALOG("LogdataStatsDialog/Widget");

const QLatin1Literal LOGDATA_EXPORT_CSV("Logdata/CsvExport");

/* Options dialog */
const QLatin1Literal OPTIONS_DIALOG_WIDGET("OptionsDialog/Widget");
const QLatin1Literal OPTIONS_DIALOG_AS_FILE_DLG("OptionsDialog/WeatherFileDialogAsn");
const QLatin1Literal OPTIONS_DIALOG_XPLANE_DLG("OptionsDialog/WeatherFileDialogXplane");
const QLatin1Literal OPTIONS_DIALOG_XPLANE_WIND_FILE_DLG("OptionsDialog/WeatherFileDialogXplaneWind");
const QLatin1Literal OPTIONS_DIALOG_GLOBE_FILE_DLG("OptionsDialog/CacheFileDialogGlobe");
const QLatin1Literal OPTIONS_DIALOG_DB_DIR_DLG("OptionsDialog/DatabaseDirDialog");
const QLatin1Literal OPTIONS_DIALOG_DB_FILE_DLG("OptionsDialog/DatabaseFilesDialog");
const QLatin1Literal OPTIONS_DIALOG_DB_EXCLUDE("OptionsDialog/DatabaseExclude");
const QLatin1Literal OPTIONS_DIALOG_DB_ADDON_EXCLUDE("OptionsDialog/DatabaseAddonExclude");
const QLatin1Literal OPTIONS_DIALOG_FLIGHTPLAN_COLOR("OptionsDialog/FlightplanColor");
const QLatin1Literal OPTIONS_DIALOG_FLIGHTPLAN_PROCEDURE_COLOR("OptionsDialog/FlightplanProcedureColor");
const QLatin1Literal OPTIONS_DIALOG_FLIGHTPLAN_ACTIVE_COLOR("OptionsDialog/FlightplanActiveColor");
const QLatin1Literal OPTIONS_DIALOG_FLIGHTPLAN_PASSED_COLOR("OptionsDialog/FlightplanPassedColor");
const QLatin1Literal OPTIONS_DIALOG_TRAIL_COLOR("OptionsDialog/TrailColor");
const QLatin1Literal OPTIONS_DIALOG_DISPLAY_OPTIONS("OptionsDialog/DisplayOptions");
const QLatin1Literal OPTIONS_DIALOG_DISPLAY_OPTIONS_COMPASS_ROSE("OptionsDialog/DisplayOptionsCompassRose");
const QLatin1Literal OPTIONS_DIALOG_DISPLAY_OPTIONS_MEASUREMENT("OptionsDialog/DisplayOptionsMeasurement");
const QLatin1Literal OPTIONS_DIALOG_DISPLAY_OPTIONS_ROUTE("OptionsDialog/DisplayOptionsRoute");
const QLatin1Literal OPTIONS_DIALOG_DISPLAY_OPTIONS_NAVAID("OptionsDialog/DisplayOptionsNavAid");
const QLatin1Literal OPTIONS_DIALOG_GUI_STYLE_INDEX("OptionsDialog/GuiStyleIndex");
const QLatin1Literal OPTIONS_DIALOG_WARN_STYLE("OptionsDialog/StyleWarning");
const QLatin1Literal OPTIONS_DIALOG_WEB_DOCROOT_DLG("OptionsDialog/WebDocroot");

/* Other options that are only accessible in the configuration file */
const QLatin1Literal OPTIONS_DIALOG_LANGUAGE("OptionsDialog/Language");
const QLatin1Literal OPTIONS_DIALOG_FONT("OptionsDialog/Font");
const QLatin1Literal OPTIONS_DIALOG_MAP_FONT("OptionsDialog/MapFont");
const QLatin1Literal OPTIONS_PIXMAP_CACHE("Options/PixmapCache");
const QLatin1Literal OPTIONS_MARBLE_DEBUG("Options/MarbleDebug");
const QLatin1Literal OPTIONS_CONNECTCLIENT_DEBUG("Options/ConnectClientDebug");
const QLatin1Literal OPTIONS_DOCKHANDLER_DEBUG("Options/DockHandlerDebug");
const QLatin1Literal OPTIONS_WHAZZUP_PARSER_DEBUG("Options/WhazzupParserDebug");
const QLatin1Literal OPTIONS_DATAREADER_DEBUG("Options/DataReaderDebug");
const QLatin1Literal OPTIONS_WEATHER_DEBUG("Options/WeatherDebug");
const QLatin1Literal OPTIONS_TRACK_DEBUG("Options/TrackDebug");
const QLatin1Literal OPTIONS_WEATHER_LEVELS("Options/WeatherLevels");
const QLatin1Literal OPTIONS_WIND_DEBUG("Options/WindDebug");
const QLatin1Literal OPTIONS_WEBSERVER_DEBUG("Options/WebserverDebug");
const QLatin1Literal OPTIONS_VERSION("Options/Version");
const QLatin1Literal OPTIONS_NO_USER_AGENT("Options/NoUserAgent");
const QLatin1Literal OPTIONS_WEATHER_UPDATE("Options/WeatherUpdate");
const QLatin1Literal OPTIONS_PROFILE_SIMPLYFY("Options/SimplifyProfile");

/* Track download URLs */
const QLatin1Literal OPTIONS_TRACK_NAT_URL("Track/NatUrl");
const QLatin1Literal OPTIONS_TRACK_NAT_PARAM("Track/NatUrlParam");
const QLatin1Literal OPTIONS_TRACK_PACOTS_URL("Track/PacotsUrl");
const QLatin1Literal OPTIONS_TRACK_PACOTS_PARAM("Track/PacotsUrlParam");
const QLatin1Literal OPTIONS_TRACK_AUSOTS_URL("Track/AusotsUrl");
const QLatin1Literal OPTIONS_TRACK_AUSOTS_PARAM("Track/AusotsUrlParam");

/* Used to override  default URL */
const QLatin1Literal OPTIONS_UPDATE_URL("Update/Url");

const QLatin1Literal OPTIONS_UPDATE_ALREADY_CHECKED("Update/AlreadyChecked");
const QLatin1Literal OPTIONS_UPDATE_LAST_CHECKED("Update/LastCheckTimestamp");

/* Need to update these according to program version */
const QLatin1Literal OPTIONS_UPDATE_CHANNELS("OptionsDialog/Widget_comboBoxOptionsStartupUpdateChannels");
const QLatin1Literal OPTIONS_UPDATE_RATE("OptionsDialog/Widget_comboBoxOptionsStartupUpdateRate");

/* These have to be loaded before the options dialog instantiation */
const QLatin1Literal OPTIONS_GUI_OVERRIDE_LOCALE("OptionsDialog/Widget_checkBoxOptionsGuiOverrideLocale");

/* File dialog patterns */
const QLatin1Literal FILE_PATTERN_SCENERYCONFIG("(*.cfg)");
const QLatin1Literal FILE_PATTERN_FLIGHTPLAN_LOAD("(*.lnmpln *.pln *.flp *.fms *.fgfp *.fpl)");
const QLatin1Literal FILE_PATTERN_LNMPLN("(*.lnmpln)");
const QLatin1Literal FILE_PATTERN_KML("(*.kml *.kmz)");
const QLatin1Literal FILE_PATTERN_GPX("(*.gpx)");

const QLatin1Literal FILE_PATTERN_USERDATA_CSV("(*.csv)");
const QLatin1Literal FILE_PATTERN_USER_FIX_DAT("(user_fix.dat)");
const QLatin1Literal FILE_PATTERN_USER_WPT("(user.wpt)");
const QLatin1Literal FILE_PATTERN_BGL_XML("(*.xml)");
const QLatin1Literal FILE_PATTERN_AIRCRAFT_PERF("(*.lnmperf)");
const QLatin1Literal FILE_PATTERN_AIRCRAFT_PERF_INI("(*.lnmperf)");
const QLatin1Literal FILE_PATTERN_GRIB("(*.grib)");

const QString FILE_PATTERN_AS_SNAPSHOT("(current_wx_snapshot.txt)");
const QString FILE_PATTERN_XPLANE_METAR("(METAR.rwx)");
const QString FILE_PATTERN_XPLANE_LOGBOOK("(X-Plane*Pilot.txt)"); /* Need * since file dialog fails on spaces */

const QString FILE_PATTERN_IMAGE("(*.jpg *.jpeg *.png *.bmp)");
const QString FILE_PATTERN_IMAGE_AVITAB("(*.png *.jpeg)");

/* Sqlite database names */
const QLatin1Literal DATABASE_DIR("little_navmap_db");
const QLatin1Literal DATABASE_PREFIX("little_navmap_");
const QLatin1Literal DATABASE_SUFFIX(".sqlite");
const QLatin1Literal DATABASE_BACKUP_SUFFIX("-backup");

/* This is the default configuration file for reading the scenery library.
 * It can be overridden by placing a  file with the same name into
 * the configuration directory. */
const QString DATABASE_NAVDATAREADER_CONFIG = ":/littlenavmap/resources/config/navdatareader.cfg";

/* Configuration for online networks */
const QString NETWORKS_CONFIG = ":/littlenavmap/resources/config/networks.cfg";

/* Configuration for online networks */
const QString URLS_CONFIG = ":/littlenavmap/resources/config/urls.cfg";

/* Main window state for first startup. Generated in MainWindow::writeSettings() */
extern const QSize DEFAULT_MAINWINDOW_SIZE;

/*
 * Supported language for the online help system. Will be determined by presence of the file
 * little-navmap-user-manual-${LANG}.online in folder help and the current language settings of the system
 * as well as any override settings in the options dialog.
 *
 * Falls back to English if indicator file is missing.
 *
 * This will consider region fallbacks in both directions like pt_BR -> pt or pt -> pt_BR
 *
 * Not thread safe.
 */
const QString helpLanguageOnline();

} // namespace lnm

#endif // LITTLENAVMAP_CONSTANTS_H
