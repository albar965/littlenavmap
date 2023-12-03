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

#ifndef LITTLENAVMAP_MAINWINDOW_H
#define LITTLENAVMAP_MAINWINDOW_H

#include "fs/fspaths.h"
#include "common/mapflags.h"

#include <QMainWindow>
#include <QFileInfoList>
#include <QTimer>
#include <QDateTime>
#include <marble/MarbleGlobal.h>

class ConnectClient;
class DatabaseManager;
class InfoController;
class MapThemeHandler;
class OptionsDialog;
class PrintSupport;
class ProcedureSearch;
class ProfileWidget;
class QActionGroup;
class QLabel;
class QPushButton;
class QToolButton;
class Route;
class RouteController;
class RouteExport;
class RouteStringDialog;
class SearchBaseTable;
class SearchController;
class SimBriefHandler;
class WeatherReporter;
class WindReporter;

class WeatherContextHandler;
namespace Marble {
class MarbleAboutDialog;
class RenderState;
class ElevationModel;
}

namespace atools {
namespace fs {
namespace pln {
class Flightplan;
}
}
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

  ProfileWidget *getProfileWidget() const
  {
    return profileWidget;
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

  WeatherContextHandler *getWeatherContextHandler() const
  {
    return weatherContextHandler;
  }

  WindReporter *getWindReporter() const
  {
    return windReporter;
  }

  /* Update the window title after switching simulators, flight plan name or change status. */
  void updateWindowTitle();

  /* Update tooltip state in recent menus */
  void setToolTipsEnabledMainMenu(bool enabled);

  /* Update coordinate display in status bar */
  void updateMapPosLabel(const atools::geo::Pos& pos, int x, int y);

  /* Sets the text and tooltip of the statusbar label that indicates what objects are shown on the map */
  void setMapObjectsShownMessageText(const QString& text = QString(), const QString& tooltipText = QString());
  void setConnectionStatusMessageText(const QString& text, const QString& tooltipText);
  void setOnlineConnectionStatusMessageText(const QString& text, const QString& tooltipText);

  /* Sets a general status bar message which is shared with all widgets/actions status text */
  void setStatusMessage(const QString& message, bool addToLog = false);
  void statusMessageChanged(const QString& text);

  void setDetailLabelText(const QString& text);

  /* Render state from marble widget. Get the more detailed state since it updates more often */
  void renderStatusChanged(const Marble::RenderState& state);

  /* Clear render status if no updates appear */
  void renderStatusReset();

  /* Update label */
  void renderStatusUpdateLabel(Marble::RenderStatus status, bool forceUpdate);

  /* Show "Too many objects" label if number of map features was truncated */
  void resultTruncated();

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

  /* create a new flightplan from passed airports */
  void routeNewFromAirports(map::MapAirport departure, map::MapAirport destination);

  /* Load a flight plan in LNMPLN format from a string */
  void routeOpenFileLnmStr(const QString& string);

  /* Open file dialog for saving a LNMPLN flight plan. Filename will be built if empty. */
  QString routeSaveFileDialogLnm(const QString& filename = QString());

  /* Show file dialog for opening a flight plan with all supported formats */
  QString routeOpenFileDialog();

  /* Called from the export if LNMPLN was bulk exported */
  void routeSaveLnmExported(const QString& filename);

  /* Load or append trail from GPX file */
  void trailLoadGpx();
  void trailAppendGpx();

  /* true if map window is maximized */
  bool isFullScreen() const;

  /* Push button in map pressed */
  void exitFullScreenPressed();

  /* Called from SimBrief handler to open route string dialog */
  void routeFromStringSimBrief(const QString& routeString);

  /* Called from SimBrief handler or non-modal route string dialog to create new plan */
  void routeFromFlightplan(const atools::fs::pln::Flightplan& flightplan, bool adjustAltitude, bool changed, bool undo);

  MapThemeHandler *getMapThemeHandler() const
  {
    return mapThemeHandler;
  }

  /* Clear route */
  void routeNew();

  /* Question dialog and then delete map and profile trail */
  void deleteAircraftTrail(bool quiet);

  /* Silently deletes track on takeoff */
  void deleteProfileAircraftTrail();

  atools::gui::DockWidgetHandler *getDockHandler() const
  {
    return dockHandler;
  }

  /* Register and unregister dialogs for autoraise */
  void addDialogToDockHandler(QDialog *dialogWidget);
  void removeDialogFromDockHandler(QDialog *dialogWidget);

  /* Get all actions from main menu which have a text and a shortcut */
  QList<QAction *> getMainWindowActions();

  /* Set on top status according main window top status */
  void setStayOnTop(QWidget *widget);

  /* Set current and default path for the LNMPLN export */
  void setLnmplnExportDir(const QString& dir);

  /* Called by QApplication::fontChanged() */
  void fontChanged(const QFont& font);

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
  void mainWindowShownDelayed();

  /* Dock window functions */
  void raiseFloatingWindows();
  void hideTitleBar();
  void allowDockingWindows();
  void allowMovingWindows();

  /* Signal from action */
  void stayOnTop();

  /* Map dock widget floating state changed */
  void mapDockTopLevelChanged(bool topLevel);

  /* Called by action */
  void fullScreenMapToggle();

  /* Switches fs on or off */
  void fullScreenOn();
  void fullScreenOff();

  void saveStateMain();
  void saveActionStates();
  void saveMainWindowStates();
  void saveFileHistoryStates();

  void restoreStateMain();
  void updateActionStates();

  void updateOnlineActionStates();

  void runDirToolManual();
  void runDirTool(bool manual = true);

  /* Update status bar section for online status */
  void updateConnectionStatusMessageText();

  /* Set up own UI elements that cannot be created in designer */
  void setupUi();

  void updateStatusBarStyle();

  void preDatabaseLoad();
  void postDatabaseLoad(atools::fs::FsPaths::SimulatorType type);

  void updateMapHistoryActions(int minIndex, int curIndex, int maxIndex);

  void updateMapObjectsShown();

  /* Reset drawing settings */
  void resetMapObjectsShown();

  /* Selection in approach view has changed */
  void procedureLegSelected(const proc::MapProcedureRef& ref);

  /* Selection in approach view has changed */
  void procedureSelected(const proc::MapProcedureRef& ref); /* Single selection */
  void proceduresSelected(const QVector<proc::MapProcedureRef>& refs); /* Multi preview for all procedures */
  void proceduresSelectedInternal(const QVector<proc::MapProcedureRef>& refs, bool previewAll);

  void routeSelectionChanged(int selected, int total);

  /* New flight plan from opening route string dialog using current plan for prefill */
  void routeFromStringCurrent();

  void routeOpen();
  void routeOpenFile(QString filepath);
  void routeAppend();
  bool routeSaveSelection();
  void routeInsert(int insertBefore);
  void routeOpenRecent(const QString& routeFile);
  void routeOpenDescr(const QString& routeString);

  /* Flight plan save functions */
  bool routeSaveLnm();
  bool routeSaveAsLnm();

  void routeCenter();
  bool routeCheckForChanges();

  /* Reset all "do not show this again" message box status values */
  void resetMessages();
  void resetAllSettings();

  /* Manual issue report triggered from the menu */
  void createIssueReport();
  void showDatabaseFiles();
  void showShowMapCache();
  void showMapInstallation();

  /* Save map as images */
  void mapSaveImage();
  void mapSaveImageAviTab();
  void mapCopyToClipboard();

  /* Opens dialog for image resolution and returns pixmap and optionally AviTab JSON */
  bool createMapImage(QPixmap& pixmap, const QString& dialogTitle, const QString& optionPrefx, QString *json = nullptr);

  void distanceChanged();
  void showDonationPage();
  void showFaqPage();

  /* Loading of KML files */
  void kmlOpenRecent(const QString& kmlFile);
  void kmlOpen();
  void kmlClear();

  /* Loading and saving of window layout files */
  void layoutOpen();
  void layoutSaveAs();
  void layoutOpenRecent(const QString& layoutFile);
  void layoutOpenDrag(const QString& layoutFile); /* Open from drag and drop event */
  bool layoutOpenInternal(const QString& layoutFile);
  void loadLayoutDelayed(const QString& filename);

  void legendAnchorClicked(const QUrl& url);

  void trailLoadGpxFile(const QString& file);

  void showOfflineHelp();
  void showOnlineDownloads();
  void showChangelog();
  void showNavmapLegend();
  bool openInSkyVector();
  void clearProcedureCache();

  void openLogFile();
  void openConfigFile();

  /* Emit a signal windowShown after first appearance */
  virtual void showEvent(QShowEvent *event) override;
  void weatherUpdateTimeout();
  void updateAirspaceTypes(const map::MapAirspaceFilter& filter);
  void updateAirspaceSources();
  void resetWindowLayout();
  void resetTabLayout();

  void checkForUpdates();
  void updateClock() const;

  /* Actions that define the time source call this*/
  void sunShadingTimeChanged();

  /* Set user defined time for sun shading */
  void sunShadingTimeSet();

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

  /* Update API keys or tokens in GUI map widget and web API map widget */
  void updateMapKeys();

  void openOptionsDialog();

  void applyToolBarSize();

  /* Print the size of all container classes to detect overflow or memory leak conditions */
  void debugDumpContainerSizes() const;

  /* Reduce status bar size if no mouse movement */
  void shrinkStatusBar();

#ifdef DEBUG_INFORMATION
  void debugActionTriggered1();
  void debugActionTriggered2();
  void debugActionTriggered3();
  void debugActionTriggered4();
  void debugActionTriggered5();
  void debugActionTriggered6();
  void debugActionTriggered7();
  void debugActionTriggered8();

#endif

#ifdef DEBUG_DUMP_SHORTCUTS
  void printShortcuts();

#endif

#ifdef DEBUG_SIZE_INFORMATION
  virtual void resizeEvent(QResizeEvent *event) override;

#endif

  /* Original unchanged window title */
  QString mainWindowTitle;
  SearchController *searchController = nullptr;
  RouteController *routeController = nullptr;
  atools::gui::FileHistoryHandler *routeFileHistory = nullptr, *kmlFileHistory = nullptr, *layoutFileHistory = nullptr;

  /* Filepath of the inline nav map legend */
  QString legendFile;

  Ui::MainWindow *ui;
  MapWidget *mapWidget = nullptr;
  ProfileWidget *profileWidget = nullptr;
  PrintSupport *printSupport = nullptr;

  /* Status bar labels */
  QLabel *mapDistanceLabel = nullptr, *mapPositionLabel = nullptr, *mapMagvarLabel = nullptr, *mapRenderStatusLabel = nullptr,
         *mapDetailLabel = nullptr, *mapVisibleLabel = nullptr, *connectStatusLabel = nullptr, *timeLabel = nullptr;

  /* Connection field and tooltip in statusbar */
  QString connectionStatus, connectionStatusTooltip, onlineConnectionStatus, onlineConnectionStatusTooltip;

  /* List of status bar messages. First is shown and others are shown in tooltip. */
  struct StatusMessage
  {
    QDateTime timestamp;
    QString message;
  };

  QVector<StatusMessage> statusMessages;

  /* true if database is currently switched off (i.e. the scenery library loading is open) */
  bool hasDatabaseLoadStatus = false;

  /* Dialog classes and helper classes */
  Marble::MarbleAboutDialog *marbleAboutDialog = nullptr;
  OptionsDialog *optionsDialog = nullptr;
  atools::gui::Dialog *dialog = nullptr;
  atools::gui::ErrorHandler *errorHandler = nullptr;
  atools::gui::HelpHandler *helpHandler = nullptr;
  atools::gui::DockWidgetHandler *dockHandler = nullptr;

  /* Managment and controller classes */
  WeatherReporter *weatherReporter = nullptr;
  WindReporter *windReporter = nullptr;
  InfoController *infoController = nullptr;
  RouteExport *routeExport = nullptr;
  SimBriefHandler *simbriefHandler = nullptr;
  MapThemeHandler *mapThemeHandler = nullptr;
  RouteStringDialog *routeStringDialog = nullptr;

  /* Action  groups for main menu */
  QActionGroup *actionGroupMapProjection = nullptr, *actionGroupMapSunShading = nullptr,
               *actionGroupMapWeatherSource = nullptr, *actionGroupMapWeatherWindSource = nullptr;

  /* Reload weather all 15 seconds */
  QTimer weatherUpdateTimer;

  bool firstStart = true; /* emit window shown only once after startup */

  WeatherContextHandler *weatherContextHandler;
  QAction *emptyAirportSeparator = nullptr;

  QList<QToolBar *> toolbars;

  /* Show database dialog after cleanup of obsolete databases if true */
  bool databasesErased = false;
  QSize defaultToolbarIconSize;
  QString aboutMessage, layoutWarnText;
  QTimer clockTimer /* MainWindow::updateClock() every second */,
         renderStatusTimer /* MainWindow::renderStatusReset() if render status is stalled */,
         shrinkStatusBarTimer /* calls MainWindow::shrinkStatusBar() once map pos and magvar are "-" */;
  Marble::RenderStatus lastRenderStatus = Marble::Incomplete;

  /* Show hint dialog only once per session */
  bool backgroundHintRouteStringShown = false;

  /* Call debugDumpContainerSizes() every 30 seconds */
  QTimer debugDumpContainerSizesTimer;
};

#endif // LITTLENAVMAP_MAINWINDOW_H
