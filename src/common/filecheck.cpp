/*****************************************************************************
* Copyright 2015-2026 Alexander Barthel alex@littlenavmap.org
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

#include "common/filecheck.h"

#include "atools.h"
#include "common/constants.h"
#include "common/mapmarkers.h"
#include "fs/gpx/gpxio.h"
#include "fs/pln/flightplanio.h"
#include "gui/dockwidgethandler.h"
#include "perf/aircraftperfcontroller.h"
#include "util/properties.h"

FileCheck::FileCheck(const QString& filename)
{
  propertiesStartup = new atools::util::Properties;
  checkFileType(filename);
}

FileCheck::FileCheck(const atools::util::Properties& propertiesStartupParam)
{
  propertiesStartup = new atools::util::Properties(propertiesStartupParam);
  fromStartupProperties();
}

FileCheck::~FileCheck()
{
  delete propertiesStartup;
}

void FileCheck::checkFileType(const QString& filename)
{
  if(atools::checkFile(Q_FUNC_INFO, filename, true /* warn */))
  {
    if(flightplan.isEmpty() && atools::fs::pln::FlightplanIO::isFlightplanFile(filename))
      flightplan = filename;

    if(aircraftPerf.isEmpty() && AircraftPerfController::isPerformanceFile(filename))
      aircraftPerf = filename;

    if(layout.isEmpty() && atools::gui::DockWidgetHandler::isWindowLayoutFile(filename))
      layout = filename;

    if(gpx.isEmpty() && atools::fs::gpx::GpxIO::isGpxFile(filename))
      gpx = filename;

    if(markers.isEmpty() && MapMarkers::isMarkersFile(filename))
      markers = filename;
  }
}

void FileCheck::fromStartupProperties()
{
  flightPlanIsOther = false;

  // Get file names from properties
  QString planOption = propertiesStartup->getPropertyStr(lnm::STARTUP_FLIGHTPLAN);
  aircraftPerf = propertiesStartup->getPropertyStr(lnm::STARTUP_AIRCRAFT_PERF);
  layout = propertiesStartup->getPropertyStr(lnm::STARTUP_LAYOUT);
  gpx = propertiesStartup->getPropertyStr(lnm::STARTUP_GPX);
  markers = propertiesStartup->getPropertyStr(lnm::STARTUP_MARKER);
  flightplanDescription = propertiesStartup->getPropertyStr(lnm::STARTUP_FLIGHTPLAN_DESCR);
  forceLoading = propertiesStartup->contains(lnm::STARTUP_FORCE_LOADING);

  // Extract filenames from positional arguments without options ================================
  for(const QString& otherFile : propertiesStartup->getPropertyStrList(lnm::STARTUP_OTHER_ARGUMENTS))
  {
    // Update filenames from detected files but do not overwrite present filenames
    checkFileType(otherFile);

    // Replace plan name with other if option is not set
    if(planOption.isEmpty() && !flightplan.isEmpty())
    {
      planOption = flightplan;

      // true if passed in by double click on a plan file
      flightPlanIsOther = true;
    }
  }

  flightplan = planOption;
  cleanInvalidFiles();
}

void FileCheck::cleanInvalidFile(QString& file, bool warn) const
{
  // Do not log if file is empty
  if(!file.isEmpty() && !atools::checkFileMsg(file, warn).isEmpty())
    file.clear();
}

void FileCheck::cleanInvalidFiles(bool warn)
{
  cleanInvalidFile(flightplan, warn);
  cleanInvalidFile(aircraftPerf, warn);
  cleanInvalidFile(layout, warn);
  cleanInvalidFile(gpx, warn);
  cleanInvalidFile(markers, warn);
}
