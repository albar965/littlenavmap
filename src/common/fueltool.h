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

#ifndef LNM_FUELTOOL_H
#define LNM_FUELTOOL_H

#include "options/optiondata.h"
#include <QApplication>

namespace atools {
namespace fs {
namespace perf {
class AircraftPerf;
}
}
}

/*
 * Provides functions to conver fuel numbers to strings containing values for volume, gallons and
 * alternative units (lbs vs kg and gal vs. liter).
 *
 * All input units are gallons or lbs.
 */
class FuelTool
{
  Q_DECLARE_TR_FUNCTIONS(FuelTool)

public:
  explicit FuelTool(const atools::fs::perf::AircraftPerf *aircraftPerf);
  explicit FuelTool(const atools::fs::perf::AircraftPerf& aircraftPerf);
  explicit FuelTool(bool jetfuelParam, bool fuelAsVolumeParam);

  /* Make a string with fuel in lbs and gallons or kg and liter */
  /* To currently user selected fuel units */
  QString weightVolLocal(float valueLbsGal) const;

  /* To opposite of currently user selected fuel units */
  QString weightVolOther(float valueLbsGal) const;

  /* Fuel flow in currently selected units */
  QString flowWeightVolLocal(float valueLbsGal) const;

  /* Get a string with local optinally bold and other unit in brackets */
  QString weightVolLocalOther(float valueLbsGal, bool localBold = false, bool otherSmall = true) const;

  /* Jetfuel or avgas */
  QString getFuelTypeString() const;

  bool isJetfuel() const
  {
    return jetfuel;
  }

  bool isFuelAsVolume() const
  {
    return fuelAsVolume;
  }

private:
  QString fuelWeightVol(opts::UnitFuelAndWeight unitFuelAndWeight, float valueLbsGal) const;

  bool jetfuel, fuelAsVolume;
};

#endif // LNM_FUELTOOL_H
