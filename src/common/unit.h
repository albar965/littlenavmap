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

#ifndef LITTLENAVMAP_UNIT_H
#define LITTLENAVMAP_UNIT_H

#include "options/optiondata.h"

#include <QCoreApplication>
#include <QLocale>
#include <functional>

namespace atools {
namespace geo {
class Pos;
}
}

/*
 * Converts arbitrary values and units into local user selected units.
 *
 * All functions have names based on the input unit. Output is always user selected unit.
 *
 * Used parameters:
 * @param addUnit Add unit to string. Based on user selected unit.
 * @param narrow Omit space between unit and value and use C locale. Mostly for map display.
 * @param minValPrec Add one decimal if value is below.
 * @param round Round resulting value to closest multiple of given round value.
 */
class Unit
{
  Q_DECLARE_TR_FUNCTIONS(Unit)

public:
  static void init();
  static void deInit();

  /* Initialize all text that are translateable after loading the translation files */
  static void initTranslateableTexts();

  Unit(const Unit& other) = delete;
  Unit& operator=(const Unit& other) = delete;

  /* Reverse functions. Convert local unit to known unit
   * Example: Unit::rev(OptionData::instance().getSimZoomOnLandingDistance(), Unit::distMeterF);
   * Converts the getSimZoomOnLandingDistance in user selected units to meter */
  static inline float rev(float value, std::function<float(float value)> func)
  {
    return value / func(1.f);
  }

  static inline float rev(float value, std::function<float(float value, bool fuelAsVolume)> func, bool fuelAsVolume)
  {
    return value / func(1.f, fuelAsVolume);
  }

  /* Replace values in text like "%dist%" or "%fuel% and returns new string. Saves original test in origtext. */
  static QString replacePlaceholders(const QString& text, QString& origtext, bool fuelAsVolume = false)
  {
    if(origtext.isEmpty())
      origtext = text;

    return replacePlaceholders(origtext, fuelAsVolume);
  }

  static QString replacePlaceholders(const QString& text, bool fuelAsVolume = false)
  {
    return replacePlaceholders(text, fuelAsVolume, unitFuelWeight);
  }

  static QString replacePlaceholders(const QString& text, bool fuelAsVolume, opts::UnitFuelAndWeight unit);

  /* Read all unit names (Meter, Nm, ...) in the methods as "from" values */

  /* Distances: Returns either nautical miles, kilometer or miles */
  static QString distMeter(float meter, bool addUnit = true, int minValPrec = 20, bool narrow = false, int precision = 0);
  static QString distNm(float nm, bool addUnit = true, int minValPrec = 20, bool narrow = false, int precision = 0);

  static float distMeterF(float meter);
  static float distNmF(float nm);

  /* Short distances: Returns either feet or meter */
  static QString distShortMeter(float meter, bool addUnit = true, bool narrow = false, int precision = 0)
  {
    return u(distShortMeterF(meter), unitShortDistStr, addUnit, narrow, precision);
  }

  static QString distShortNm(float nm, bool addUnit = true, bool narrow = false)
  {
    return u(distShortNmF(nm), unitShortDistStr, addUnit, narrow);
  }

  static QString distShortFeet(float ft, bool addUnit = true, bool narrow = false)
  {
    return u(distShortFeetF(ft), unitShortDistStr, addUnit, narrow);
  }

  /* Short and long distance. Meter or km / NM and ft */
  static QString distLongShortMeter(float ft, const QString& separator, bool addUnit = true, bool narrow = false, int precision = 0);

  static float distShortMeterF(float meter);
  static float distShortNmF(float nm);
  static float distShortFeetF(float ft);

  /* Speed: Returns either kts, km/h or m/h */
  static QString speedKts(float kts, bool addUnit = true, bool narrow = false)
  {
    return u(speedKtsF(kts), unitSpeedStr, addUnit, narrow);
  }

  /* Get the other available two units except the currently selected one. */
  static QStringList speedKtsOther(float kts, bool addUnit = true, bool narrow = false);

  static float speedKtsF(float kts);
  static QString speedMeterPerSec(float mPerSec, bool addUnit = true, bool narrow = false);
  static float speedMeterPerSecF(float mPerSec = true);

  static QString speedVertFpm(float fpm, bool addUnit = true);
  static float speedVertFpmF(float fpm);

  /* Speed: Returns either ft/m or m/s */
  static QString speedVertFpmOther(float fpm, bool addUnit = true);

  /* Altitude: Returns either meter or feet */
  static QString altMeter(float meter, bool addUnit = true, bool narrow = false, float round = 0.f);
  static QString altFeet(float ft, bool addUnit = true, bool narrow = false, float round = 0.f);

  /* Get the other unit ft vs. meter and vice versa */
  static QString altFeetOther(float ft, bool addUnit = true, bool narrow = false, float round = 0.f);

  static float altMeterF(float meter);
  static float altFeetF(float ft);
  static int altFeetI(int ft);

  /* Volume: Returns either gal or l for fuel*/
  static QString volGallon(float gal, bool addUnit = true)
  {
    return u(volGallonF(gal), unitVolStr, addUnit);
  }

  static float volGallonF(float gal);

  static QString volLiter(float liter, bool addUnit = true);
  static float volLiterF(float liter);

  /* Weight: Returns either lbs or kg for fuel */
  static QString weightLbs(float lbs, bool addUnit = true)
  {
    return u(weightLbsF(lbs), unitWeightStr, addUnit);
  }

  static float weightLbsF(float lbs);

  static QString weightKg(float kg, bool addUnit = true)
  {
    return u(weightKgF(kg), unitWeightStr, addUnit);
  }

  static float weightKgF(float kg);

  /* Get unit string with unit as selected in options and other unit in brackets */
  static QString weightLbsLocalOther(float lbs, bool localBold = false, bool otherSmall = true);
  static QString fuelLbsAndGalLocalOther(float lbs, float gal, bool localBold = false, bool otherSmall = true);

  /* Fuel flow US Gallon and lbs */
  static QString ffGallon(float gal, bool addUnit = true)
  {
    return u(volGallonF(gal), unitFfVolStr, addUnit);
  }

  static float ffGallonF(float gal)
  {
    return volGallonF(gal);
  }

  static QString ffLbs(float lbs, bool addUnit = true)
  {
    return u(weightLbsF(lbs), unitFfWeightStr, addUnit);
  }

  static float ffLbsF(float lbs)
  {
    return weightLbsF(lbs);
  }

  /* Return string containing both units - weight and volume */
  static QString ffLbsAndGal(float lbs, float gal, bool addUnit = true);
  static QString fuelLbsAndGal(float lbs, float gal, bool addUnit = true);

  static QString fuelLbsGallon(float gal, bool addUnit = true, bool fuelAsVolume = false)
  {
    return fuelAsVolume ? volGallon(gal, addUnit) : weightLbs(gal, addUnit);
  }

  /* Converts either volume or weight to current unit */
  static float fuelLbsGallonF(float gal, bool fuelAsVolume = false)
  {
    return fuelAsVolume ? volGallonF(gal) : weightLbsF(gal);
  }

  static QString ffLbsGallon(float gal, bool addUnit = true, bool fuelAsVolume = false)
  {
    return fuelAsVolume ? ffGallon(gal, addUnit) : ffLbs(gal, addUnit);
  }

  static float ffLbsGallonF(float gal, bool fuelAsVolume = false)
  {
    return fuelAsVolume ? ffGallonF(gal) : ffLbsF(gal);
  }

  /* Fuel flow metric */
  static QString ffLiter(float liter, bool addUnit = true)
  {
    return u(volLiterF(liter), unitFfVolStr, addUnit);
  }

  static float ffLiterF(float liter);

  static QString ffKg(float kg, bool addUnit = true)
  {
    return u(weightKgF(kg), unitFfWeightStr, addUnit);
  }

  static float ffKgF(float kg)
  {
    return weightKgF(kg);
  }

  /* Return string containing both units - weight and volume */
  static QString ffKgAndLiter(float kg, float liter, bool addUnit = true);
  static QString fuelKgAndLiter(float kg, float liter, bool addUnit = true);

  static QString fuelKgLiter(float kgLiter, bool addUnit = true, bool fuelAsVolume = false)
  {
    return fuelAsVolume ? volLiter(kgLiter, addUnit) : weightKg(kgLiter, addUnit);
  }

  static float fuelKgLiterF(float kgLiter, bool fuelAsVolume = false)
  {
    return fuelAsVolume ? volLiterF(kgLiter) : weightKgF(kgLiter);
  }

  static QString ffKgLiter(float kgLiter, bool addUnit = true, bool fuelAsVolume = false)
  {
    return fuelAsVolume ? ffLiter(kgLiter, addUnit) : ffKg(kgLiter, addUnit);
  }

  static float ffKgLiterF(float kgLiter, bool fuelAsVolume = false)
  {
    return fuelAsVolume ? ffLiterF(kgLiter) : ffKgF(kgLiter);
  }

  /* Can be used as function pointers for conversion. fromCopy is a dummy returning the same value */
  static float fromUsToMetric(float value, bool fuelAsVolume);
  static float fromMetricToUs(float value, bool fuelAsVolume);

  static float fromCopy(float value, bool)
  {
    return value;
  }

  /* Coordinates: Returns either decimal or sexagesimal notation */
  static QString coords(const atools::geo::Pos& pos)
  {
    return coords(pos, unitCoords);
  }

  static QString coords(const atools::geo::Pos& pos, opts::UnitCoords coordUnit);

  static QString coordsLatY(const atools::geo::Pos& pos)
  {
    return coordsLatY(pos, unitCoords);
  }

  static QString coordsLatY(const atools::geo::Pos& pos, opts::UnitCoords coordUnit);

  static QString coordsLonX(const atools::geo::Pos& pos)
  {
    return coordsLonX(pos, unitCoords);
  }

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

  static opts::UnitAlt getUnitAlt()
  {
    return unitAlt;
  }

  static opts::UnitDist getUnitDist()
  {
    return unitDist;
  }

  static opts::UnitSpeed getUnitSpeed()
  {
    return unitSpeed;
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

  /* Remove trailing zeroes if the number has a decimal point */
  static QString adjustNum(QString num);

private:
  /* Singleton - make private */
  Unit()
  {

  }

  /* Merges numbers and units depending on flags */
  static QString u(const QString& num, const QString& un, bool addUnit, bool narrow);
  static QString u(float num, const QString& unitStr, bool addUnit, bool narrow = false, int precision = 0);
  static QString localOtherText(bool localBold, bool otherSmall);
  static QString localOtherText2(bool localBold, bool otherSmall);

  static QChar ns(float latY)
  {
    return latY > 0.f ? 'N' : 'S';
  }

  static QChar ew(float lonX)
  {
    return lonX > 0.f ? 'E' : 'W';
  }

  /* Negative sign or empty */
  static QString s(float ord)
  {
    return ord < 0.f ? locale->negativeSign() : QString();
  }

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
  static bool enhancedAccuracy;

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
