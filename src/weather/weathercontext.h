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

#ifndef LNM_WEATHERCONTEXT_H
#define LNM_WEATHERCONTEXT_H

#include "fs/weather/weathertypes.h"

namespace map {

/*
 * Stores last METARs to avoid unneeded updates in widget.
 * Can contain all METARs from all configured sources where each one has a station, nearest or interpolated METAR.
 */
struct WeatherContext
{
  /* Simulator and online sources. */
  atools::fs::weather::MetarResult fsMetar, ivaoMetar, noaaMetar, vatsimMetar;

  /* Active sky values */
  bool isAsDeparture = false, isAsDestination = false;
  QString asMetar, asType, ident;

  bool isEmpty() const
  {
    return fsMetar.isEmpty() && asMetar.isEmpty() && noaaMetar.isEmpty() && vatsimMetar.isEmpty() && ivaoMetar.isEmpty();
  }

};

QDebug operator<<(QDebug out, const map::WeatherContext& record);

} // namespace map

#endif // LNM_WEATHERCONTEXT_H
