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

#ifndef LITTLENAVMAP_UNIT_H
#define LITTLENAVMAP_UNIT_H

#include "options/optiondata.h"

#include <QApplication>

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
  static const Unit& instance();
  static void delInstance();

  ~Unit();

  /* Distances: Returns either nautical miles, kilometer or miles */
  static QString distMeter(float value, bool addUnit = true);
  static QString distNm(float value, bool addUnit = true);

  static float distMeterF(float value);
  static float distNmF(float value);

  /* Short distances: Returns either feet or meter */
  static QString distShortMeter(float value, bool addUnit = true);
  static QString distShortNm(float value, bool addUnit = true);
  static QString distShortFeet(float value, bool addUnit = true);

  static float distShortMeterF(float value);
  static float distShortNmF(float value);
  static float distShortFeetF(float value);

  /* Speed: Returns either kts, km/h or m/h */
  static QString speedKts(float value, bool addUnit = true);

  static float speedKtsF(float value);

  static QString speedVertFpm(float value, bool addUnit = true);

  static float speedVertFpmF(float value);

  /* Altitude: Returns either meter or feet */
  static QString altMeter(float value, bool addUnit = true);
  static QString altFeet(float value, bool addUnit = true);

  static float altMeterF(float value);
  static float altFeetF(float value);

  /* Coordinates: Returns either decimal or sexagesimal notation */
  static QString coords(const atools::geo::Pos& pos);

  static void optionsChanged();

private:
  Unit();
  static QString u(const QString& num, const QString& un, bool addUnit);

  static Unit *unit;
  static const OptionData *opts;
  static QLocale *locale;

  static opts::UnitDist unitDist;
  static opts::UnitShortDist unitShortDist;
  static opts::UnitAlt unitAlt;
  static opts::UnitSpeed unitSpeed;
  static opts::UnitVertSpeed unitVertSpeed;
  static opts::UnitCoords unitCoords;

  static QString unitDistStr;
  static QString unitShortDistStr;
  static QString unitAltStr;
  static QString unitSpeedStr;
  static QString unitVertSpeedStr;
};

#endif // LITTLENAVMAP_UNIT_H
