/*****************************************************************************
* Copyright 2015-2018 Alexander Barthel albar965@mailbox.org
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

  /* Reverse function. Convert local unit to known unit */
  static inline float rev(float value, std::function<float(float value)> func)
  {
    return value / func(1.f);
  }

  static QString replacePlaceholders(const QString& text, QString& origtext);
  static QString replacePlaceholders(const QString& text);

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
  static QString speedKts(float value, bool addUnit = true);
  static float speedKtsF(float value);
  static QString speedMeterPerSec(float value, bool addUnit = true);
  static float speedMeterPerSecF(float value = true);

  static QString speedVertFpm(float value, bool addUnit = true);
  static float speedVertFpmF(float value);

  /* Altitude: Returns either meter or feet */
  static QString altMeter(float value, bool addUnit = true, bool narrow = false);
  static QString altFeet(float value, bool addUnit = true, bool narrow = false);

  static float altMeterF(float value);
  static float altFeetF(float value);

  /* Volume: Returns either gal or l */
  static QString volGallon(float value, bool addUnit = true);
  static float volGallonF(float value);

  /* Weight: Returns either lbs or kg */
  static QString weightLbs(float value, bool addUnit = true);
  static float weightLbsF(float value);

  static QString ffGallon(float value, bool addUnit = true);
  static float ffGallonF(float value);
  static QString ffLbs(float value, bool addUnit = true);
  static float ffLbsF(float value);

  /* Coordinates: Returns either decimal or sexagesimal notation */
  static QString coords(const atools::geo::Pos& pos);
  static QString coordsLatY(const atools::geo::Pos& pos);
  static QString coordsLonX(const atools::geo::Pos& pos);

  static void optionsChanged();

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

private:
  Unit();
  static QString u(const QString& num, const QString& un, bool addUnit, bool narrow);
  static QString u(float num, const QString& un, bool addUnit, bool narrow = false);

  static const OptionData *opts;
  static QLocale *locale, *clocale;

  static opts::UnitDist unitDist;
  static opts::UnitShortDist unitShortDist;
  static opts::UnitAlt unitAlt;
  static opts::UnitSpeed unitSpeed;
  static opts::UnitVertSpeed unitVertSpeed;
  static opts::UnitCoords unitCoords;
  static opts::UnitFuelAndWeight unitFuelWeight;

  static QString unitDistStr;
  static QString unitShortDistStr;
  static QString unitAltStr;
  static QString unitSpeedStr;
  static QString unitVertSpeedStr;

  static QString unitVolStr;
  static QString unitWeightStr;

  static QString unitFfVolStr;
  static QString unitFfWeightStr;
};

#endif // LITTLENAVMAP_UNIT_H
