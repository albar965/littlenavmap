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

#ifndef LITTLENAVMAP_UNIT_H
#define LITTLENAVMAP_UNIT_H

#include "options/optiondata.h"

#include <QApplication>
#include <functional>

namespace atools {
namespace geo {
class Pos;
}
}

class OptionData;

class Unit
{
  Q_DECLARE_TR_FUNCTIONS(Unit)

public:
  static void init();
  static void deInit();

  /* Initialize all text that are translateable after loading the translation files */
  static void initTranslateableTexts();

  ~Unit();

  /* Reverse functions. Convert local unit to known unit */
  static inline float rev(float value, std::function<float(float value)> func)
  {
    return value / func(1.f);
  }

  static inline float rev(float value, std::function<float(float value, bool fuelAsVolume)> func, bool fuelAsVolume)
  {
    return value / func(1.f, fuelAsVolume);
  }

  /* Replace values in text like "%dist%" or "%fuel% and returns new string. Saves original test in origtext. */
  static QString replacePlaceholders(const QString& text, QString& origtext, bool fuelAsVolume = false);
  static QString replacePlaceholders(const QString& text, bool fuelAsVolume = false);
  static QString replacePlaceholders(const QString& text, bool fuelAsVolume, opts::UnitFuelAndWeight unit);

  /* Read all unit names (Meter, Nm, ...) in the methods as "from" values */

  /* Distances: Returns either nautical miles, kilometer or miles */
  static QString distMeter(float value, bool addUnit = true, int minValPrec = 20, bool narrow = false);
  static QString distNm(float value, bool addUnit = true, int minValPrec = 20, bool narrow = false);

  static float distMeterF(float value);
  static float distNmF(float value);

  /* Short distances: Returns either feet or meter */
  static QString distShortMeter(float value, bool addUnit = true, bool narrow = false);
  static QString distShortNm(float value, bool addUnit = true, bool narrow = false);
  static QString distShortFeet(float value, bool addUnit = true, bool narrow = false);

  static float distShortMeterF(float value);
  static float distShortNmF(float value);
  static float distShortFeetF(float value);

  /* Speed: Returns either kts, km/h or m/h */
  static QString speedKts(float value, bool addUnit = true, bool narrow = false);
  static float speedKtsF(float value);
  static QString speedMeterPerSec(float value, bool addUnit = true, bool narrow = false);
  static float speedMeterPerSecF(float value = true);

  static QString speedVertFpm(float value, bool addUnit = true);
  static float speedVertFpmF(float value);

  /* Altitude: Returns either meter or feet */
  static QString altMeter(float value, bool addUnit = true, bool narrow = false);
  static QString altFeet(float value, bool addUnit = true, bool narrow = false);

  static float altMeterF(float value);
  static float altFeetF(float value);
  static int altFeetI(int value);

  /* Volume: Returns either gal or l for fuel*/
  static QString volGallon(float value, bool addUnit = true);
  static float volGallonF(float value);

  static QString volLiter(float value, bool addUnit = true);
  static float volLiterF(float value);

  /* Weight: Returns either lbs or kg for fuel */
  static QString weightLbs(float value, bool addUnit = true);
  static float weightLbsF(float value);
  static QString weightKg(float value, bool addUnit = true);
  static float weightKgF(float value);

  /* Get unit string with unit as selected in options and other unit in brackets */
  static QString weightLbsLocalOther(float value, bool localBold = false, bool otherSmall = true);
  static QString fuelLbsAndGalLocalOther(float valueLbs, float valueGal, bool localBold = false,
                                         bool otherSmall = true);

  /* Fuel flow US Gallon and lbs */
  static QString ffGallon(float value, bool addUnit = true);
  static float ffGallonF(float value);
  static QString ffLbs(float value, bool addUnit = true);
  static float ffLbsF(float value);

  /* Return string containing both units - weight and volume */
  static QString ffLbsAndGal(float valueLbs, float valueGal, bool addUnit = true);
  static QString fuelLbsAndGal(float valueLbs, float valueGal, bool addUnit = true);

  static QString fuelLbsGallon(float value, bool addUnit = true, bool fuelAsVolume = false);

  /* Converts either volume or weight to current unit */
  static float fuelLbsGallonF(float value, bool fuelAsVolume = false);

  static QString ffLbsGallon(float value, bool addUnit = true, bool fuelAsVolume = false);
  static float ffLbsGallonF(float value, bool fuelAsVolume = false);

  /* Fuel flow metric */
  static QString ffLiter(float value, bool addUnit = true);
  static float ffLiterF(float value);
  static QString ffKg(float value, bool addUnit = true);
  static float ffKgF(float value);

  /* Return string containing both units - weight and volume */
  static QString ffKgAndLiter(float valueKg, float valueLiter, bool addUnit = true);
  static QString fuelKgAndLiter(float valueKg, float valueLiter, bool addUnit = true);

  static QString fuelKgLiter(float value, bool addUnit = true, bool fuelAsVolume = false);
  static float fuelKgLiterF(float value, bool fuelAsVolume = false);

  static QString ffKgLiter(float value, bool addUnit = true, bool fuelAsVolume = false);
  static float ffKgLiterF(float value, bool fuelAsVolume = false);

  /* Can be used as function pointers for conversion. fromCopy is a dummy returning the same value */
  static float fromUsToMetric(float value, bool fuelAsVolume);
  static float fromMetricToUs(float value, bool fuelAsVolume);
  static float fromCopy(float value, bool fuelAsVolume);

  /* Coordinates: Returns either decimal or sexagesimal notation */
  static QString coords(const atools::geo::Pos& pos);
  static QString coords(const atools::geo::Pos& pos, opts::UnitCoords coordUnit);

  static QString coordsLatY(const atools::geo::Pos& pos);
  static QString coordsLatY(const atools::geo::Pos& pos, opts::UnitCoords coordUnit);

  static QString coordsLonX(const atools::geo::Pos& pos);
  static QString coordsLonX(const atools::geo::Pos& pos, opts::UnitCoords coordUnit);

  /* Program options changed - update units */
  static void optionsChanged();

  /* ==================================================================
   * Getters for current units strings / suffixes from options */
  static const QString& getUnitDistStr()
  {
    return unitDistStr;
  }

  static const QString& getUnitShortDistStr()
  {
    return unitShortDistStr;
  }

  static const QString& getUnitAltStr()
  {
    return unitAltStr;
  }

  static const QString& getUnitSpeedStr()
  {
    return unitSpeedStr;
  }

  static const QString& getUnitVertSpeedStr()
  {
    return unitVertSpeedStr;
  }

  static const QString& getUnitVolStr()
  {
    return unitVolStr;
  }

  static const QString& getUnitWeightStr()
  {
    return unitWeightStr;
  }

  static const QString& getUnitFfVolStr()
  {
    return unitFfVolStr;
  }

  static const QString& getUnitFfWeightStr()
  {
    return unitFfWeightStr;
  }

  static opts::UnitDist getUnitDist()
  {
    return unitDist;
  }

  static opts::UnitShortDist getUnitShortDist()
  {
    return unitShortDist;
  }

  static opts::UnitVertSpeed getUnitVertSpeed()
  {
    return unitVertSpeed;
  }

  static opts::UnitCoords getUnitCoords()
  {
    return unitCoords;
  }

  /* ==================================================================
   * Getters for fixed Unit suffixes */
  static const QString& getSuffixDistNm()
  {
    return suffixDistNm;
  }

  static const QString& getSuffixDistKm()
  {
    return suffixDistKm;
  }

  static const QString& getSuffixDistMi()
  {
    return suffixDistMi;
  }

  static const QString& getSuffixDistShortFt()
  {
    return suffixDistShortFt;
  }

  static const QString& getSuffixDistShortMeter()
  {
    return suffixDistShortMeter;
  }

  static const QString& getSuffixAltFt()
  {
    return suffixAltFt;
  }

  static const QString& getSuffixAltMeter()
  {
    return suffixAltMeter;
  }

  static const QString& getSuffixSpeedKts()
  {
    return suffixSpeedKts;
  }

  static const QString& getSuffixSpeedKmH()
  {
    return suffixSpeedKmH;
  }

  static const QString& getSuffixSpeedMph()
  {
    return suffixSpeedMph;
  }

  static const QString& getSuffixVertSpeedFpm()
  {
    return suffixVertSpeedFpm;
  }

  static const QString& getSuffixVertSpeedMs()
  {
    return suffixVertSpeedMs;
  }

  static const QString& getSuffixFuelVolGal()
  {
    return suffixFuelVolGal;
  }

  static const QString& getSuffixFuelWeightLbs()
  {
    return suffixFuelWeightLbs;
  }

  static const QString& getSuffixFfWeightPpH()
  {
    return suffixFfWeightPpH;
  }

  static const QString& getSuffixFfGalGpH()
  {
    return suffixFfGalGpH;
  }

  static const QString& getSuffixFuelVolLiter()
  {
    return suffixFuelVolLiter;
  }

  static const QString& getSuffixFuelWeightKg()
  {
    return suffixFuelWeightKg;
  }

  static const QString& getSuffixFfWeightKgH()
  {
    return suffixFfWeightKgH;
  }

  static const QString& getSuffixFfVolLiterH()
  {
    return suffixFfVolLiterH;
  }

  static bool isShowOtherFuel()
  {
    return showOtherFuel;
  }

private:
  /* Singleton */
  Unit();

  /* Merges numbers and units depending on flags */
  static QString u(const QString& num, const QString& un, bool addUnit, bool narrow);
  static QString u(float num, const QString& un, bool addUnit, bool narrow = false);
  static QString localOtherText(bool localBold, bool otherSmall);
  static QString localOtherText2(bool localBold, bool otherSmall);

  static const OptionData *opts;
  static QLocale *locale, *clocale;

  static opts::UnitDist unitDist;
  static opts::UnitShortDist unitShortDist;
  static opts::UnitAlt unitAlt;
  static opts::UnitSpeed unitSpeed;
  static opts::UnitVertSpeed unitVertSpeed;
  static opts::UnitCoords unitCoords;
  static opts::UnitFuelAndWeight unitFuelWeight;
  static bool showOtherFuel;

  /* Currently selected unit suffixes */
  static QString unitDistStr;
  static QString unitShortDistStr;
  static QString unitAltStr;
  static QString unitSpeedStr;
  static QString unitVertSpeedStr;

  static QString unitVolStr;
  static QString unitWeightStr;

  static QString unitFfVolStr;
  static QString unitFfWeightStr;

  /* Fixed unit suffixes only updated for translations */
  static QString suffixDistNm;
  static QString suffixDistKm;
  static QString suffixDistMi;
  static QString suffixDistShortFt;
  static QString suffixDistShortMeter;
  static QString suffixAltFt;
  static QString suffixAltMeter;
  static QString suffixSpeedKts;
  static QString suffixSpeedKmH;
  static QString suffixSpeedMph;
  static QString suffixVertSpeedFpm;
  static QString suffixVertSpeedMs;
  static QString suffixFuelVolGal;
  static QString suffixFuelWeightLbs;
  static QString suffixFfWeightPpH;
  static QString suffixFfGalGpH;
  static QString suffixFuelVolLiter;
  static QString suffixFuelWeightKg;
  static QString suffixFfWeightKgH;
  static QString suffixFfVolLiterH;
};

#endif // LITTLENAVMAP_UNIT_H
