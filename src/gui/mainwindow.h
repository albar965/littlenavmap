/*****************************************************************************
* Copyright 2015-2019 Alexander Barthel alex@littlenavmap.org
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
#include "fs/pln/flightplanconstants.h"

#include <QMainWindow>
#include <QUrl>
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
class ConnectClient;
class ProfileWidget;
class InfoController;
class OptionsDialog;
class QActionGroup;
class PrintSupport;
class ProcedureSearch;
class Route;
class AirspaceToolBarHandler;
class RouteExport;

namespace Marble {
class LegendWidget;
class MarbleAboutDialog;
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
  virtual ~MainWindow();

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

  /* Update the window title after switching simulators, flight plan name or change status. */
  void updateWindowTitle();

  /* Update coordinate display in status bar */
  void updateMapPosLabel(const atools::geo::Pos& pos, int x, int y);

  /* Sets the text and tooltip of the statusbar label that indicates what objects are shown on the map */
  void setMapObjectsShownMessageText(const QString& text = QString(), const QString& tooltipText = QString());
  void setConnectionStatusMessageText(const QString& text, const QString& tooltipText);
  void setOnlineConnectionStatusMessageText(const QString& text, const QString& tooltipText);

  /* Sets a general status bar message which is shared with all widgets/actions status text */
  void setStatusMessage(const QString& message);

  void setDetailLabelText(const QString& text);

  bool buildWeatherContextForInfo(map::WeatherContext& weatherContext, const map::MapAirport& airport);
  void buildWeatherContext(map::WeatherContext& weatherContext, const map::MapAirport& airport) const;
  void buildWeatherContextForTooltip(map::WeatherContext& weatherContext, const map::MapAirport& airport) const;

  /* Render status from marble widget */
  void renderStatusChanged(Marble::RenderStatus status);

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
  void raiseFloatingWindow(QDockWidget *dockWidget);

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

  void options();
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
  bool routeSave();
  bool routeSaveAsPln();
  bool routeSaveAsFlp();
  bool routeSaveAsFlightGear();
  bool routeSaveAsFms(atools::fs::pln::FileFormat format);
  bool routeSaveAsFms3();
  bool routeSaveAsFms11();
  bool routeExportClean();

  void routeCenter();
  bool routeCheckForChanges();
  void showMapLegend();
  void resetMessages();
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

  /* Emit a signal windowShown after first appearance */
  virtual void showEvent(QShowEvent *event) override;
  void weatherUpdateTimeout();
  void fillActiveSkyType(map::WeatherContext& weatherContext, const QString& airportIdent) const;
  void updateAirspaceTypes(map::MapAirspaceFilter types);
  void resetWindowLayout();

  bool routeSaveCheckWarnings(bool& saveAs, atools::fs::pln::FileFormat fileFormat);
  bool routeSaveCheckFMS11Warnings();

  /* Question dialog and then delete map and profile trail */
  void deleteAircraftTrack();

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
  void actionShortcutFlightPlanTriggered();
  void actionShortcutAircraftPerformanceTriggered();
  void actionShortcutAirportInformationTriggered();
  void actionShortcutAirportWeatherTriggered();
  void actionShortcutNavaidInformationTriggered();
  void actionShortcutAircraftProgressTriggered();

  /* Internal web server actions */
  void toggleWebserver(bool checked);
  void webserverStatusChanged(bool running);
  void openWebserver();

#ifdef DEBUG_INFORMATION
  void debugActionTriggered();

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

  /* List of status bar messages (currently only one) */
  QStringList statusMessages;

  /* true if database is currently switched off (i.e. the scenery library loading is open) */
  bool hasDatabaseLoadStatus = false;

  /* Dialog classes and helper classes */
  Marble::MarbleAboutDialog *marbleAbout = nullptr;
  OptionsDialog *optionsDialog = nullptr;
  atools::gui::Dialog *dialog = nullptr;
  atools::gui::ErrorHandler *errorHandler = nullptr;
  atools::gui::HelpHandler *helpHandler = nullptr;

  /* Map theme submenu actions */
  QList<QAction *> customMapThemeMenuActions;

  /* Managment and controller classes */
  WeatherReporter *weatherReporter = nullptr;
  InfoController *infoController = nullptr;
  AirspaceToolBarHandler *airspaceHandler = nullptr;
  RouteExport *routeExport = nullptr;

  /* Action  groups for main menu */
  QActionGroup *actionGroupMapProjection = nullptr, *actionGroupMapTheme = nullptr, *actionGroupMapSunShading = nullptr,
               *actionGroupMapWeatherSource = nullptr;

  QTimer weatherUpdateTimer;

  bool firstStart = true /* emit window shown only once after startup */,
       firstApplicationStart = false /* first starup on a system after installation */;

  map::WeatherContext *currentWeatherContext = nullptr;

  QAction *emptyAirportSeparator = nullptr;

  /* Show database dialog after cleanup of obsolete databases if true */
  bool databasesErased = false;

  QString aboutMessage;
  QTimer clockTimer;

};

#endif // LITTLENAVMAP_MAINWINDOW_H
