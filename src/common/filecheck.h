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

#ifndef LNM_FILECHECK_H
#define LNM_FILECHECK_H

class QString;

namespace atools {
namespace util {
class Properties;
}
}
namespace fc {

/* Read all properties from startup or data exchange for compatible file types. Also checks non positional arguments.
 * Checks the filenames for compatible file types and fills the given strings with file names in case of matches. */
void fromStartupProperties(const atools::util::Properties& properties, QString *flightplan, QString *flightplanDescr = nullptr,
                           QString *perf = nullptr, QString *layout = nullptr);

/* Checks the given filename for compatible file types and fills the given strings with file names in case of matches.
 * The file types are checked by content.
 * All compatible flight plan types are checked. */
void checkFileType(const QString& filename, QString *flightplan, QString *perf = nullptr, QString *layout = nullptr,
                   QString *gpx = nullptr);

} // namespace fc

#endif // LNM_FILECHECK_H
