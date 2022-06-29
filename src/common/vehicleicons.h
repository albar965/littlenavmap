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

#ifndef LNM_VEHICLEICONS_H
#define LNM_VEHICLEICONS_H

#include <QCache>

namespace atools {
namespace fs {
namespace sc {
class SimConnectAircraft;
}
}
}

class QIcon;
class QPixmap;

namespace internal {
struct PixmapKey;
}

/*
 * Caches pixmaps generated from SVG graphic files for aircraft, boat, helicopter, etc.
 */
class VehicleIcons
{

public:
  VehicleIcons();
  ~VehicleIcons();

  VehicleIcons(const VehicleIcons& other) = delete;
  VehicleIcons& operator=(const VehicleIcons& other) = delete;

  QIcon iconFromCache(const atools::fs::sc::SimConnectAircraft& ac, int size, int rotate);
  const QPixmap *pixmapFromCache(const atools::fs::sc::SimConnectAircraft& ac, int size, int rotate);

private:
  const QPixmap *pixmapFromCache(const internal::PixmapKey& key, int rotate);

  QCache<internal::PixmapKey, QPixmap> aircraftPixmaps;
};

#endif // LNM_VEHICLEICONS_H
