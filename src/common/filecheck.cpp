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

#include "common/filecheck.h"

#include "atools.h"
#include "common/constants.h"
#include "fs/pln/flightplanio.h"
#include "gui/dockwidgethandler.h"
#include "perf/aircraftperfcontroller.h"
#include "util/properties.h"
#include "fs/gpx/gpxio.h"

namespace fc {

void checkFileType(const QString& filename, QString *flightplan, QString *perf, QString *layout, QString *gpx)
{
  if(atools::checkFile(Q_FUNC_INFO, filename, true /* warn */))
  {
    if(flightplan != nullptr && flightplan->isEmpty() && atools::fs::pln::FlightplanIO::isFlightplanFile(filename))
      *flightplan = filename;

    if(perf != nullptr && perf->isEmpty() && AircraftPerfController::isPerformanceFile(filename))
      *perf = filename;

    if(layout != nullptr && layout->isEmpty() && atools::gui::DockWidgetHandler::isWindowLayoutFile(filename))
      *layout = filename;

    if(gpx != nullptr && gpx->isEmpty() && atools::fs::gpx::GpxIO::isGpxFile(filename))
      *gpx = filename;
  }
}

void fromStartupProperties(const atools::util::Properties& properties, QString *flightplan, QString *flightplanDescr, QString *perf,
                           QString *layout)
{
  if(flightplan != nullptr)
    *flightplan = properties.getPropertyStr(lnm::STARTUP_FLIGHTPLAN);

  if(perf != nullptr)
    *perf = properties.getPropertyStr(lnm::STARTUP_AIRCRAFT_PERF);

  if(layout != nullptr)
    *layout = properties.getPropertyStr(lnm::STARTUP_LAYOUT);

  if(flightplanDescr != nullptr)
    *flightplanDescr = properties.getPropertyStr(lnm::STARTUP_FLIGHTPLAN_DESCR);

  // Extract filenames from positional arguments without options ================================
  const QStringList propertyStrList = properties.getPropertyStrList(lnm::STARTUP_OTHER_ARGUMENTS);
  for(const QString& otherFile : propertyStrList)
    fc::checkFileType(otherFile, flightplan, perf, layout);
}

} // namespace fc
