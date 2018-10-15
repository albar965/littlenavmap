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

#ifndef LNM_AIRCRAFTPERFCONTROLLER_H
#define LNM_AIRCRAFTPERFCONTROLLER_H

#include <QTimer>

namespace atools {
namespace gui {
class FileHistoryHandler;
}
namespace fs {
namespace perf {
class AircraftPerf;
}
}
}

class MainWindow;

/*
 * Takes care of aircraft performance managment, loading, saving, generating the report on the flight plan dock.
 */
class AircraftPerfController :
  public QObject
{
  Q_OBJECT

public:
  explicit AircraftPerfController(MainWindow *parent);
  virtual ~AircraftPerfController();

  /* Create a new performance sheet and opens the edit dialog after asking to save currently unchanged */
  void create();

  /* Opens the edit dialog. */
  void edit();

  /* Open file dialog and load a new performance file after asking to save currently unchanged */
  void load();

  /* Load a new performance file from file history after asking to save currently unchanged */
  void loadFile(const QString& perfFile);

  /* Save current performance file */
  bool save();

  /* Open save dialog and save current performance file */
  bool saveAs();

  /* TODO */
  void collectToggled();

  /* Open related help in browser */
  void helpClicked();

  /* Ask user if data can be deleted when quitting.
   * @return true continue with new, exit, etc. */
  bool checkForChanges();

  /* true if file has changed */
  bool hasChanged() const
  {
    return changed;
  }

  /* Currently loaded performance filepath */
  QString getCurrentFilepath() const
  {
    return currentFilepath;
  }

  void connectAllSlots();

  void saveState();
  void restoreState();

  /* Update background colors in report */
  void optionsChanged();

  /* Get currently loaded performance data */
  const atools::fs::perf::AircraftPerf *getAircraftPerformance() const
  {
    return perf;
  }

  /* true if aircraft performance is loaded */
  bool hasAircraftPerformance() const
  {
    return perf != nullptr;
  }

  /* Cruise speed knots TAS */
  float getRouteCruiseSpeedKts();

  /* Wind direction in degree as set in the flight plan dock on tab aircraft */
  float getWindDir() const;

  /* Wind speed in knots as set in the flight plan dock on tab aircraft */
  float getWindSpeed() const;

  /* Sent back after aircraftPerformanceChanged was sent from here */
  void routeChanged(bool geometryChanged, bool newFlightplan = false);
  void routeAltitudeChanged(float altitudeFeet);

signals:
  /* Sent if performance or wind has changed */
  void aircraftPerformanceChanged(const atools::fs::perf::AircraftPerf *perf);

private:
  /* Wind spin boxes changed */
  void windChanged();

  /* Update wind data after a delay to avoid laggy pointer */
  void windChangedDelayed();

  /* Create or update HTML report */
  void updateReport();

  void updateActionStates();

  /* Invalidate performance file after an error - state "none loaded" */
  void noPerfLoaded();

  /* URL or show file in performance report clicked */
  void anchorClicked(const QUrl& url);

  MainWindow *mainWindow;

  /* Default font size - can be changed in settings */
  float infoFontPtSize = 10.f;
  int symbolSize = 16;

  /* true if data was changed */
  bool changed = false;
  QString currentFilepath;
  atools::fs::perf::AircraftPerf *perf = nullptr;
  atools::gui::FileHistoryHandler *fileHistory = nullptr;

  /* Timer to delay wind updates */
  QTimer windChangeTimer;
};

#endif // LNM_AIRCRAFTPERFCONTROLLER_H
