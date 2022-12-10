/*****************************************************************************
* Copyright 2015-2022 Alexander Barthel alex@littlenavmap.org
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

#ifndef LNM_AIRCRAFTPERFCONTROLLER_H
#define LNM_AIRCRAFTPERFCONTROLLER_H

#include "fs/perf/aircraftperfconstants.h"

#include <QTimer>

namespace atools {
namespace util {
class MovingAverageTime;
class HtmlBuilder;
}

namespace gui {
class FileHistoryHandler;
}
namespace fs {
namespace sc {
class SimConnectData;
}

namespace perf {
class AircraftPerf;
class AircraftPerfHandler;
}
}
}

class MainWindow;
struct FuelTimeResult;

/*
 * Takes care of aircraft performance managment, loading, saving, generating the report on the flight plan dock.
 *
 * Handles all menus, actions and buttons.
 */
class AircraftPerfController
  : public QObject
{
  Q_OBJECT

public:
  explicit AircraftPerfController(MainWindow *parent);
  virtual ~AircraftPerfController() override;

  AircraftPerfController(const AircraftPerfController& other) = delete;
  AircraftPerfController& operator=(const AircraftPerfController& other) = delete;

  /* Load a new performance file after asking to save currently unchanged */
  void loadFile(const QString& perfFile);

  /* Load into current from an XML string */
  void loadStr(const QString& string);

  /* Opens save as dialog to save the content in the string into a performance file
   * Does not affect current */
  bool saveAsStr(const QString& string) const;

  /* Default file dialogs for opening and saving.
   *  Offers the INI format as alternative if oldFormat is not null. */
  QString saveAsFileDialog(const QString& filepath, bool *oldFormat = nullptr) const;
  QString openFileDialog() const;

  /* Ask user if data can be deleted when quitting.
   * @return true continue with new, exit, etc. */
  bool checkForChanges();

  /* true if file has changed */
  bool hasChanged() const
  {
    return changed;
  }

  /* Currently loaded performance filepath */
  const QString& getCurrentFilepath() const
  {
    return currentFilepath;
  }

  void connectAllSlots();

  void saveState();
  void restoreState();

  /* Update background colors in report */
  void optionsChanged();

  /* Connection was established */
  void connectedToSimulator();

  /* Disconnected manually or due to error */
  void disconnectedFromSimulator();

  /* Get currently loaded performance data */
  const atools::fs::perf::AircraftPerf& getAircraftPerformance() const
  {
    return *perf;
  }

  /* Updates for automatic performance calculation */
  void simDataChanged(const atools::fs::sc::SimConnectData& simulatorData);

  /* Cruise speed knots TAS */
  float getRouteCruiseSpeedKts();

  /* Wind direction in degree as set in the flight plan dock on tab aircraft */
  float getManualWindDirDeg() const;

  /* Wind speed in knots as set in the flight plan dock on tab aircraft */
  float getManualWindSpeedKts() const;

  /* Selected wind altitude */
  float getManualWindAltFt() const;

  /* Checkbox above flight plan table */
  bool isWindManual() const;

  /* Sent back after aircraftPerformanceChanged was sent from here */
  void routeChanged(bool, bool = false);
  void updateReports();
  void routeAltitudeChanged(float);
  void warningChanged();

  void flightSegmentChanged(const atools::fs::perf::FlightSegment& flightSegment);

  /* true if the performance segments are valid, i.e. speeds are > 0 */
  bool isClimbValid() const;
  bool isDescentValid() const;

  /* Called from print support to get a HTML report */
  void fuelReport(atools::util::HtmlBuilder& html, bool print, bool visible);
  void fuelReportFilepath(atools::util::HtmlBuilder& html, bool print);

  /* Detect format by reading the first few lines */
  static bool isPerformanceFile(const QString& file);

  /* Required reserve at destination. Reserve + alternate fuel */
  float getFuelReserveAtDestinationLbs() const;
  float getFuelReserveAtDestinationGal() const;

  /* Restart performance collection  */
  void restartCollection(bool quiet = false);

  /* Calculates values based on performance profile if valid - otherwise estimated by aircraft fuel flow and speed */
  void calculateFuelAndTimeTo(FuelTimeResult& result, float distanceToDest, float distanceToNext, int activeLeg) const;

  float getBlockFuel() const;
  float getTripFuel() const;

  /* Current aircraft endurance with full fuel load */
  void getEnduranceFull(float& enduranceHours, float& enduranceNm);

  /* Current aircraft endurance based on current fuel flow. This is the rolling average over ten seconds or current value. */
  void getEnduranceAverage(float& enduranceHours, float& enduranceNm);

  /* Get collected major errors */
  bool hasErrors() const;
  QStringList getErrorStrings() const;

  /* Update tooltips showing the file path. Set "setToolTipsVisible" for menu always to true */
  void updateMenuTooltips();

signals:
  /* Sent if performance or wind has changed */
  void aircraftPerformanceChanged(const atools::fs::perf::AircraftPerf *perf);
  void windChanged();

private:
  /* Create a new performance sheet and opens the edit dialog after asking to save currently unchanged */
  void create();

  /* Opens the edit dialog asks for changes and saves if needed. */
  void edit();

  /* Opens the edit dialog and returns true if accepted. */
  bool editInternal(atools::fs::perf::AircraftPerf& perf, const QString& modeText, bool newPerf, bool& saveClicked);

  /* Open file dialog and load a new performance file after asking to save currently unchanged */
  void load();

  /* Open file dialog and show merge dialog */
  void loadAndMerge();

  /* Show merge dialog for currently collected performance */
  void mergeCollected();

  /* Save current performance file */
  bool save();

  /* Open save dialog and save current performance file */
  bool saveAs();

  /* Open related help in browser */
  void helpClickedPerf() const;
  void helpClickedPerfCollect() const;

  /* Wind spin boxes changed */
  void windBoxesChanged();

  /* Update wind data after a delay to avoid laggy pointer */
  void windChangedDelayed();

  /* Create or update HTML report for fuel plan */
  void updateReport();

  /* Create or update HTML report for performance collection */
  void updateReportCurrent();

  /* Update buttons tab title change indication and main window filename and change indication */
  void updateActionStates();

  /* Invalidate performance file after an error - state "none loaded" */
  void noPerfLoaded();

  /* URL or show file in performance report clicked */
  void anchorClicked(const QUrl& url);

  void fuelReportRunway(atools::util::HtmlBuilder& html);

  /* Dock window or tab visibility changed */
  void tabVisibilityChanged();

  /* Cruise altitude either from flight plan or widget */
  float cruiseAlt();

  /* Update * for changed if tab is reopened */
  void updateTabTiltle();

  void windText(atools::util::HtmlBuilder& html, const QString& label, float windSpeed, float windDirection, float headWind) const;

  MainWindow *mainWindow;

  /* Default font size - can be changed in settings */
  float infoFontPtSize = 10.f;
  int symbolSize = 16;

  /* true if data was changed */
  bool changed = false;

  /* Filename or empty if not saved yet */
  QString currentFilepath;
  atools::fs::perf::AircraftPerf *perf = nullptr;

  /* Collects aircraft data */
  atools::fs::perf::AircraftPerfHandler *perfHandler = nullptr;

  atools::gui::FileHistoryHandler *fileHistory = nullptr;

  /* Last update of report when collecting data */
  qint64 currentReportLastSampleTimeMs = 0L, reportLastSampleTimeMs = 0L;

  /* Timer to delay wind updates */
  QTimer windChangeTimer;
  atools::fs::sc::SimConnectData *lastSimData;

  /* For a smooth endurance calculation - first value is fuel flow in PPH and second is groundspeed in KTS */
  atools::util::MovingAverageTime *fuelFlowGroundspeedAverage;

  QStringList errorTooltips;
};

#endif // LNM_AIRCRAFTPERFCONTROLLER_H
