/*****************************************************************************
* Copyright 2015-2016 Alexander Barthel albar965@mailbox.org
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

#ifndef LITTLENAVMAP_COORDINATES_H
#define LITTLENAVMAP_COORDINATES_H

#include <QString>

namespace atools {
namespace geo {
class Pos;
}
}

namespace coords {

QString toGfpFormat(const atools::geo::Pos& pos);
QString toDegMinFormat(const atools::geo::Pos& pos);

atools::geo::Pos fromAnyWaypointFormat(const QString& str);

/* N44124W122451 or N14544W017479 or S31240E136502 */
atools::geo::Pos fromGfpFormat(const QString& str);

/* Degrees only 46N078W */
atools::geo::Pos fromDegFormat(const QString& str);

/* Degrees and minutes 4620N07805W */
atools::geo::Pos fromDegMinFormat(const QString& str);

/* Degrees, minutes and seconds 481200N0112842E (Skyvector) */
atools::geo::Pos fromDegMinSecFormat(const QString& str);

/* Degrees and minutes in pair "N6500 W08000" or "N6500/W08000" */
atools::geo::Pos fromDegMinPairFormat(const QString& str);

/* NAT type 5020N */
atools::geo::Pos fromNatFormat(const QString& str);

} // namespace coords

#endif // LITTLENAVMAP_COORDINATES_H
