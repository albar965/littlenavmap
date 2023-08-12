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

#include "weather/weathercontext.h"

namespace map {

QDebug operator<<(QDebug out, const WeatherContext& record)
{
  QDebugStateSaver saver(out);

  out << "WeatherContext["
      << "Sim METAR" << record.fsMetar
      << "IVAO METAR" << record.ivaoMetar
      << "NOAA METAR" << record.noaaMetar
      << "VATSIM METAR" << record.vatsimMetar
      << "AS departure" << record.isAsDeparture
      << "AS destination" << record.isAsDestination
      << "AS METAR" << record.asMetar
      << "AS type" << record.asType
      << "ident" << record.ident << "]";
  return out;
}

} // namespace map
