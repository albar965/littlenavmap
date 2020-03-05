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

#include "fueltool.h"
#include "common/unit.h"
#include "geo/calculations.h"
#include "fs/perf/aircraftperf.h"

FuelTool::FuelTool(const atools::fs::perf::AircraftPerf *aircraftPerf)
  : jetfuel(aircraftPerf->isJetFuel()), fuelAsVolume(aircraftPerf->useFuelAsVolume())
{

}

FuelTool::FuelTool(const atools::fs::perf::AircraftPerf& aircraftPerf)
  : jetfuel(aircraftPerf.isJetFuel()), fuelAsVolume(aircraftPerf.useFuelAsVolume())
{

}

FuelTool::FuelTool(bool jetfuelParam, bool fuelAsVolumeParam)
  : jetfuel(jetfuelParam), fuelAsVolume(fuelAsVolumeParam)
{

}

QString FuelTool::fuelWeightVol(opts::UnitFuelAndWeight unitFuelAndWeight, float valueLbsGal) const
{
  static const QString STR("%1 %2, %3 %4");

  // Source value is always lbs or gallon depending on setting in perf
  // convert according to unitFuelAndWeight or pass through
  using namespace atools::geo;
  switch(unitFuelAndWeight)
  {
    // Imperial units requested =================================
    case opts::FUEL_WEIGHT_GAL_LBS:
      if(fuelAsVolume)
        // Pass volume through and convert volume to weight - input its gallon
        return STR.
               // lbs
               arg(fromGalToLbs(jetfuel, valueLbsGal), 0, 'f', 0).arg(Unit::getSuffixFuelWeightLbs()).
               // gal
               arg(valueLbsGal, 0, 'f', 0).arg(Unit::getSuffixFuelVolGal());
      else
        // Pass weight through and convert weight to volume - input its lbs
        return STR.
               // lbs
               arg(valueLbsGal, 0, 'f', 0).arg(Unit::getSuffixFuelWeightLbs()).
               // gal
               arg(fromLbsToGal(jetfuel, valueLbsGal), 0, 'f', 0).arg(Unit::getSuffixFuelVolGal());

    // Metric units requested =================================
    case opts::FUEL_WEIGHT_LITER_KG:
      if(fuelAsVolume)
        // Convert to metric and pass volume  through and convert volume to weight - input its gallon
        return STR.
               // kg
               arg(lbsToKg(fromGalToLbs(jetfuel, valueLbsGal)), 0, 'f', 0).arg(Unit::getSuffixFuelWeightKg()).
               // liter
               arg(gallonToLiter(valueLbsGal), 0, 'f', 0).arg(Unit::getSuffixFuelVolLiter());
      else
        // Convert to metric and  pass weight through and convert weight to volume - input its lbs
        return STR.
               // kg
               arg(lbsToKg(valueLbsGal), 0, 'f', 0).arg(Unit::getSuffixFuelWeightKg()).
               // liter
               arg(gallonToLiter(fromLbsToGal(jetfuel, valueLbsGal)), 0, 'f', 0).arg(Unit::getSuffixFuelVolLiter());
  }
  return QString();
}

QString FuelTool::weightVolLocal(float valueLbsGal) const
{
  // Convert to locally selected unit
  return fuelWeightVol(OptionData::instance().getUnitFuelAndWeight(), valueLbsGal);
}

QString FuelTool::weightVolOther(float valueLbsGal) const
{
  if(Unit::isShowOtherFuel())
  {
    // Convert to opposite of locally selected unit (lbs/gal vs. kg/l and vice versa)
    switch(OptionData::instance().getUnitFuelAndWeight())
    {
      case opts::FUEL_WEIGHT_GAL_LBS:
        return fuelWeightVol(opts::FUEL_WEIGHT_LITER_KG, valueLbsGal);

      case opts::FUEL_WEIGHT_LITER_KG:
        return fuelWeightVol(opts::FUEL_WEIGHT_GAL_LBS, valueLbsGal);
    }
  }
  return QString();
}

QString FuelTool::flowWeightVolLocal(float valueLbsGal) const
{
  if(fuelAsVolume)
    return Unit::ffLbsAndGal(atools::geo::fromGalToLbs(jetfuel, valueLbsGal), valueLbsGal);
  else
    return Unit::ffLbsAndGal(valueLbsGal, atools::geo::fromLbsToGal(jetfuel, valueLbsGal));
}

QString FuelTool::weightVolLocalOther(float valueLbsGal, bool localBold, bool otherSmall) const
{
  QString local = localBold ? tr("<b>%1</b>") : tr("%1");
  if(Unit::isShowOtherFuel())
  {
    QString other = otherSmall ? tr(" <span style=\"font-size: small;\">(%2)</span>") : tr(" (%2)");
    return (local + other).arg(weightVolLocal(valueLbsGal)).arg(weightVolOther(valueLbsGal));
  }
  else
    return local.arg(weightVolLocal(valueLbsGal));
}

QString FuelTool::getFuelTypeString() const
{
  return jetfuel ? tr("Jetfuel") : tr("Avgas");
}
