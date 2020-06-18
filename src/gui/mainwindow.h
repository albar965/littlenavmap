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

#ifndef LITTLENAVMAP_MAINWINDOW_H
#define LITTLENAVMAP_MAINWINDOW_H

#include "fs/fspaths.h"
#include "common/mapflags.h"

#include <QMainWindow>
#include <QFileInfoList>
#include <QTimer>
#include <marble/MarbleGlobal.h>

class SearchController;
class RouteController;
class QComboBox;
class QLabel;
class QToolButton;
class SearchBaseTable;
class DatabaseManager;
class WeatherReporter;
class WindReporter;
class ConnectClient;
class ProfileWidget;
class InfoController;
class OptionsDialog;
class QActionGroup;
class PrintSupport;
class ProcedureSearch;
class Route;
class RouteExport;

namespace Marble {
class LegendWidget;
class MarbleAboutDialog;
class RenderState;
class ElevationModel;
}

namespace atools {
namespace geo {
class Pos;
}
namespace sql {
class SqlDatabase;
}

namespace gui {
class Dialog;
class ErrorHandler;
class HelpHandler;
class FileHistoryHandler;
class DockWidgetHandler;
}

}

namespace Ui {
class MainWindow;
}

class MapWidget;
class MapQuery;
class InfoQuery;
class ProcedureQuery;

namespace proc {
struct MapProcedureRef;

}

namespace map {
struct WeatherContext;

struct MapAirport;

}

/*
 * Main window contains all instances of controllers, widgets and managment classes.
 */
class MainWindow :
  public QMainWindow
{
  Q_OBJECT

public:
  MainWindow();
  virtual ~MainWindow() override;

  MapWidget *getMapWidget() const
  {
    return mapWidget;
  }

  void updateMap() const;

  RouteController *getRouteController() const
  {
    return routeController;
  }

  const Marble::ElevationModel *getElevationModel();

  /* Get main user interface instance */
  Ui::MainWindow *getUi() const
  {
    return ui;
  }

  WeatherReporter *getWeatherReporter() const
  {
    return weatherReporter;
  }

  WindReporter *getWindReporter() const
  {
    return windReporter;
  }

  /* Update the window title after switching simulators, flight plan name or change status. */
  void updateWindowTitle();

  /* Update coordinate display in status bar */
  void updateMapPosLabel(const atools::geo::Pos& pos, int x, int y);

  /* Sets the text and tooltip of the statusbar label that indicates what objects are shown on the map */
  void setMapObjectsShownMessageText(const QString& text = QString(), const QString& tooltipText = QString());
  void setConnectionStatusMessageText(const QString& text, const QString& tooltipText);
  void setOnlineConnectionStatusMessageText(const QString& text, const QString& tooltipText);

  /* Sets a general status bar message which is shared with all widgets/actions status text */
  void setStatusMessage(const QString& message, bool addToLog = false);

  void setDetailLabelText(const QString& text);

  bool buildWeatherContextForInfo(map::WeatherContext& weatherContext, const map::MapAirport& airport);
  void buildWeatherContext(map::WeatherContext& weatherContext, const map::MapAirport& airport) const;
  void buildWeatherContextForTooltip(map::WeatherContext& weatherContext, const map::MapAirport& airport) const;

  /* Render state from marble widget. Get the more detailed state since it updates more often */
  void renderStatusChanged(const Marble::RenderState& state);

  /* Clear render status if no updates appear */
  void renderStatusReset();

  /* Update label */
  void renderStatusUpdateLabel(Marble::RenderStatus status, bool forceUpdate);

  /* Show "Too many objects" label if number of map features was truncated */
  void resultTruncated(int truncatedTo);

  void setDatabaseErased(bool value)
  {
    databasesErased = value;
  }

  SearchController *getSearchController() const
  {
    return searchController;
  }

  void updateMarkActionStates();
  void updateHighlightActionStates();

  /* Update red messages at bottom of route dock window and profile dock window if altitude calculation has errors*/
  void updateErrorLabels();

  map::MapThemeComboIndex getMapThemeIndex() const;

  const InfoController *getInfoController() const
  {
    return infoController;
  }

  OptionsDialog *getOptionsDialog() const
  {
    return optionsDialog;
  }

  /* Show and raise windows after loading or creating new files */
  void showFlightPlan();
  void showAircraftPerformance();
  void showLogbookSearch();
  void showUserpointSearch();
  void showRouteCalc();

  /* Load a flight plan in LNMPLN format from a string */
  void routeOpenFileLnmStr(const QString& string);

  /* Open file dialog for saving a LNMPLN flight plan. Filename will be built if empty. */
  QString routeSaveFileDialogLnm(const QString& filename = QString());

  /* Show file dialog for opening a flight plan with all supported formats */
  QString routeOpenFileDialog();

signals:
  /* Emitted when window is shown the first time */
  void windowShown();

private:
  /* Work on the close event that also catches clicking the close button
   * in the window frame */
  virtual void closeEvent(QCloseEvent *event) override;

  /* Drag and drop support for loading flight plans from file manager */
  virtual void dragEnterEvent(QDragEnterEvent *event) override;
  virtual void dropEvent(QDropEvent *event) override;

  void connectAllSlots();
  void mainWindowShown();
  void raiseFloatingWindows();

  void saveStateMain();
  void saveActionStates();
  void saveMainWindowStates();
  void saveFileHistoryStates();

  void restoreStateMain();
  void updateActionStates();

  void updateOnlineActionStates();

  /* Update status bar section for online status */
  void updateConnectionStatusMessageText();

  void setupUi();

  void preDatabaseLoad();
  void postDatabaseLoad(atools::fs::FsPaths::SimulatorType type);

  void updateMapHistoryActions(int minIndex, int curIndex, int maxIndex);

  void updateMapObjectsShown();

  /* Reset drawing settings */
  void resetMapObjectsShown();

  void searchSelectionChanged(const SearchBaseTable *source, int selected, int visible, int total);
  void procedureLegSelected(const proc::MapProcedureRef& ref);
  void procedureSelected(const proc::MapProcedureRef& ref);

  void routeSelectionChanged(int selected, int total);

  void routeNewFromString();
  void routeNew();
  void routeOpen();
  void routeOpenFile(QString filepath);
  void routeAppend();
  void routeInsert(int insertBefore);
  void routeOpenRecent(const QString& routeFile);

  /* Flight plan save functions */
  bool routeSaveLnm();
  bool routeSaveAsLnm();

  void routeCenter();
  bool routeCheckForChanges();
  void showMapLegend();
  void resetMessages();
  void resetAllSettings();
  void showDatabaseFiles();

  /* Save map as images */
  void mapSaveImage();
  void mapSaveImageAviTab();
  void mapCopyToClipboard();

  /* Opens dialog for image resolution and returns pixmap and optionally AviTab JSON */
  bool createMapImage(QPixmap& pixmap, const QString& dialogTitle, const QString& optionPrefx, QString *json = nullptr);

  void distanceChanged();
  void showDonationPage();
  void showFaqPage();

  void kmlOpenRecent(const QString& kmlFile);
  void kmlOpen();
  void kmlClear();

  void legendAnchorClicked(const QUrl& url);

  void changeMapProjection(int index);
  void changeMapTheme();
  void scaleToolbar(QToolBar *toolbar, float scale);
  void findCustomMaps(QFileInfoList& customDgmlFiles);
  void themeMenuTriggered(bool checked);
  void updateLegend();
  void clearWeatherContext();
  void showOnlineHelp();
  void showOnlineTutorials();
  void showOfflineHelp();
  void showNavmapLegend();
  void loadNavmapLegend();
  bool openInSkyVector();
  void clearProcedureCache();

  /* Emit a signal windowShown after first appearance */
  virtual void showEvent(QShowEvent *event) override;
  void weatherUpdateTimeout();
  void fillActiveSkyType(map::WeatherContext& weatherContext, const QString& airportIdent) const;
  void updateAirspaceTypes(map::MapAirspaceFilter types);
  void resetWindowLayout();

  /* Question dialog and then delete map and profile trail */
  void deleteAircraftTrack(bool quiet = false);

  void checkForUpdates();
  void updateClock() const;

  /* Actions that define the time source call this*/
  void sunShadingTimeChanged();

  /* Set user defined time for sun shading */
  void sunShadingTimeSet();

  /* From menu action - remove all measurement lines, patterns, holds, etc. */
  void clearRangeRingsAndDistanceMarkers(bool quiet = false);

  /* Reset flight plan and other for new flight */
  void routeResetAll();

  /* Action from shortcut menu triggered */
  void actionShortcutMapTriggered();
  void actionShortcutProfileTriggered();
  void actionShortcutAirportSearchTriggered();
  void actionShortcutNavaidSearchTriggered();
  void actionShortcutUserpointSearchTriggered();
  void actionShortcutLogbookSearchTriggered();
  void actionShortcutFlightPlanTriggered();
  void actionShortcutCalcRouteTriggered();
  void actionShortcutAircraftPerformanceTriggered();
  void actionShortcutAirportInformationTriggered();
  void actionShortcutAirportWeatherTriggered();
  void actionShortcutNavaidInformationTriggered();
  void actionShortcutAircraftProgressTriggered();

  /* Internal web server actions */
  void toggleWebserver(bool checked);
  void webserverStatusChanged(bool running);
  void openWebserver();
  void saveStateNow();
  void optionsChanged();

#ifdef DEBUG_INFORMATION
  void debugActionTriggered1();
  void debugActionTriggered2();
  void debugActionTriggered3();

#endif

  /* Original unchanged window title */
  QString mainWindowTitle;
  SearchController *searchController = nullptr;
  RouteController *routeController = nullptr;
  atools::gui::FileHistoryHandler *routeFileHistory = nullptr, *kmlFileHistory = nullptr;

  /* Filepath of the inline nav map legend */
  QString legendFile;

  /* Combo boxes that are added to the toolbar */
  QComboBox *mapThemeComboBox = nullptr, *mapProjectionComboBox = nullptr;

  Ui::MainWindow *ui;
  MapWidget *mapWidget = nullptr;
  ProfileWidget *profileWidget = nullptr;
  PrintSupport *printSupport = nullptr;

  /* Status bar labels */
  QLabel *mapDistanceLabel = nullptr, *mapPosLabel = nullptr, *magvarLabel = nullptr, *renderStatusLabel = nullptr,
         *detailLabel = nullptr, *messageLabel = nullptr, *connectStatusLabel = nullptr, *timeLabel = nullptr;

  /* Connection field and tooltip in statusbar */
  QString connectionStatus, connectionStatusTooltip, onlineConnectionStatus, onlineConnectionStatusTooltip;

  /* List of status bar messages. First is shown and others are shown in tooltip. */
  QVector<std::pair<QTime, QString> > statusMessages;

  /* true if database is currently switched off (i.e. the scenery library loading is open) */
  bool hasDatabaseLoadStatus = false;

  /* Dialog classes and helper classes */
  Marble::MarbleAboutDialog *marbleAbout = nullptr;
  OptionsDialog *optionsDialog = nullptr;
  atools::gui::Dialog *dialog = nullptr;
  atools::gui::ErrorHandler *errorHandler = nullptr;
  atools::gui::HelpHandler *helpHandler = nullptr;
  atools::gui::DockWidgetHandler *dockHandler = nullptr;

  /* Map theme submenu actions */
  QList<QAction *> customMapThemeMenuActions;

  /* Managment and controller classes */
  WeatherReporter *weatherReporter = nullptr;
  WindReporter *windReporter = nullptr;
  InfoController *infoController = nullptr;
  RouteExport *routeExport = nullptr;

  /* Action  groups for main menu */
  QActionGroup *actionGroupMapProjection = nullptr, *actionGroupMapTheme = nullptr, *actionGroupMapSunShading = nullptr,
               *actionGroupMapWeatherSource = nullptr, *actionGroupMapWeatherWindSource = nullptr;

  QTimer weatherUpdateTimer;

  bool firstStart = true /* emit window shown only once after startup */,
       firstApplicationStart = false /* first starup on a system after installation */;

  map::WeatherContext *currentWeatherContext = nullptr;

  QAction *emptyAirportSeparator = nullptr;

  /* Show database dialog after cleanup of obsolete databases if true */
  bool databasesErased = false;

  QString aboutMessage;
  QTimer clockTimer, renderStatusTimer;
  Marble::RenderStatus lastRenderStatus = Marble::Incomplete;
};

#endif // LITTLENAVMAP_MAINWINDOW_H
