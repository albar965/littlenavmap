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

#ifndef LNM_AIRCRAFTPERFCONTROLLER_H
#define LNM_AIRCRAFTPERFCONTROLLER_H

#include "fs/perf/aircraftperfconstants.h"

#include <QTimer>

namespace atools {
namespace util {
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

  /* Load a new performance file after asking to save currently unchanged */
  void loadFile(const QString& perfFile);

  /* Load into current from an XML string */
  void loadStr(const QString& string);

  /* Opens save as dialog to save the content in the string into a performance file
   * Does not affect current */
  bool saveAsStr(const QString& string);

  /* Default file dialogs for opening and saving */
  QString saveAsFileDialog() const;
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
  float getWindDir() const;

  /* Wind speed in knots as set in the flight plan dock on tab aircraft */
  float getWindSpeed() const;

  /* Checkbox above flight plan table */
  bool isWindManual() const;

  /* Sent back after aircraftPerformanceChanged was sent from here */
  void routeChanged(bool geometryChanged, bool newFlightplan = false);
  void updateReports();
  void routeAltitudeChanged(float altitudeFeet);

  void flightSegmentChanged(const atools::fs::perf::FlightSegment& flightSegment);

  /* true if the performance segments are valid, i.e. speeds are > 0 */
  bool isClimbValid() const;
  bool isDescentValid() const;

  /* Called from print support to get a HTML report */
  void fuelReport(atools::util::HtmlBuilder& html, bool print = false);
  void fuelReportFilepath(atools::util::HtmlBuilder& html, bool print);

  /* Detect format by reading the first few lines */
  static bool isPerformanceFile(const QString& file);

  /* Required reserve at destination. Reserve + alternate fuel */
  float getFuelReserveAtDestinationLbs() const;
  float getFuelReserveAtDestinationGal() const;

  /* Restart performance collection  */
  void restartCollection(bool quiet = false);

  /* Calculates values based on performance profile if valid - otherwise estimated by aircraft fuel flow and speed */
  bool calculateFuelAndTimeTo(float& fuelLbsToDest, float& fuelGalToDest, float& fuelLbsToTod, float& fuelGalToTod,
                              float& timeToDest, float& timeToTod,
                              float distanceToDest, int activeLeg) const;

signals:
  /* Sent if performance or wind has changed */
  void aircraftPerformanceChanged(const atools::fs::perf::AircraftPerf *perf);

private:
  /* Create a new performance sheet and opens the edit dialog after asking to save currently unchanged */
  void create();

  /* Opens the edit dialog. */
  void edit();
  bool editInternal(atools::fs::perf::AircraftPerf& perf, const QString& modeText);

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

  void manualWindToggled();

  /* Wind spin boxes changed */
  void windBoxesChanged();

  /* Update wind data after a delay to avoid laggy pointer */
  void windChangedDelayed();

  /* Create or update HTML report for fuel plan */
  void updateReport();

  /* Create or update HTML report for performance collection */
  void updateReportCurrent();

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
  qint64 reportLastSampleTimeMs = 0L;

  /* Timer to delay wind updates */
  QTimer windChangeTimer;

};

#endif // LNM_AIRCRAFTPERFCONTROLLER_H
