/*****************************************************************************
* Copyright 2015-2025 Alexander Barthel alex@littlenavmap.org
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

#ifndef LNM_FILECHECK_H
#define LNM_FILECHECK_H

#include <QString>

namespace atools {
namespace util {
class Properties;
}
}

/* Class checks file types from a given filename, startup properties or data exchange properties.
 * Invalid files or not accessible files are logged. */
class FileCheck
{
public:
  /* Checks the given filename for compatible file types and fills the respective filenames in case of matches.
   * The file types are checked by content.
   * All compatible flight plan types are checked. */
  FileCheck(const QString& filename);

  /* Read all properties from startup or data exchange for compatible file types. Also checks non-positional arguments.
   * Checks the filenames for compatible file types and fills the respective filenames in case of matches. */
  explicit FileCheck(const atools::util::Properties& propertiesStartupParam);
  ~FileCheck();

  /* .lnmpln and all compatible */
  const QString& getFlightplanFile() const
  {
    return flightplan;
  }

  /* Flight plan route description */
  const QString& getFlightplanDescription() const
  {
    return flightplanDescription;
  }

  /* .lnmperf aircraft performance files */
  const QString& getAircraftPerfFile() const
  {
    return aircraftPerf;
  }

  /* .lnmlayout window layout files */
  const QString& getLayoutFile() const
  {
    return layout;
  }

  /* .gpx files loaded as trail */
  const QString& getGpxFile() const
  {
    return gpx;
  }

  /* .lnmuserfeat files loaded as markers */
  const QString& getMarkersFile() const
  {
    return markers;
  }

  /* true if passed in by double click on a plan file */
  bool isFlightPlanIsOther() const
  {
    return flightPlanIsOther;
  }

  /* true if any filename is set */
  bool hasAnyFile() const
  {
    return !flightplan.isEmpty() || !flightplanDescription.isEmpty() || !aircraftPerf.isEmpty() || !layout.isEmpty() ||
           !gpx.isEmpty() || !markers.isEmpty();
  }

  /* force overwrite of files when loading */
  bool isForceLoading() const
  {
    return forceLoading;
  }

private:
  /* Update filenames from detected files but do not overwrite present filenames */
  void checkFileType(const QString& filename);
  void fromStartupProperties();

  void cleanInvalidFiles(bool warn = true);
  void cleanInvalidFile(QString& file, bool warn) const;

  const atools::util::Properties *propertiesStartup = nullptr;
  QString flightplan, flightplanDescription, aircraftPerf, layout, gpx, markers;
  bool flightPlanIsOther = false, forceLoading = false;
};

#endif // LNM_FILECHECK_H
