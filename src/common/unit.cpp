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

#include "common/unit.h"

#include "geo/calculations.h"

#include "geo/pos.h"

const static QString COORDS_DEC_FORMAT("%1° %2 %3° %4");
const static QString COORDS_DMS_FORMAT("%1° %2' %3\" %4 %5° %6' %7\" %8");

Unit *Unit::unit = nullptr;
QLocale *Unit::locale = nullptr;
const OptionData *Unit::opts = nullptr;

opts::UnitDist Unit::unitDist = opts::DIST_NM;
opts::UnitShortDist Unit::unitShortDist = opts::DIST_SHORT_FT;
opts::UnitAlt Unit::unitAlt = opts::ALT_FT;
opts::UnitSpeed Unit::unitSpeed = opts::SPEED_KTS;
opts::UnitVertSpeed Unit::unitVertSpeed = opts::VERT_SPEED_FPM;
opts::UnitCoords Unit::unitCoords = opts::COORDS_DMS;

QString Unit::unitDistStr(Unit::tr("nm"));
QString Unit::unitShortDistStr(Unit::tr("ft"));
QString Unit::unitAltStr(Unit::tr("ft"));
QString Unit::unitSpeedStr(Unit::tr("kts"));
QString Unit::unitVertSpeedStr(Unit::tr("fpm"));

Unit::Unit()
{

}

Unit::~Unit()
{

}

const Unit& Unit::instance()
{
  if(unit == nullptr)
  {
    unit = new Unit();
    opts = &OptionData::instance();
    locale = new QLocale();
    unit->optionsChanged();
  }
  return *unit;
}

void Unit::delInstance()
{
  delete unit;
  unit = nullptr;

  delete locale;
  locale = nullptr;
}

QString Unit::distMeter(float value, bool addUnit)
{
  return u(locale->toString(distMeterF(value), 'f', 0), unitDistStr, addUnit);
}

QString Unit::distNm(float value, bool addUnit)
{
  return distMeter(atools::geo::nmToMeter(value), addUnit);
}

float Unit::distMeterF(float value)
{
  switch(unitDist)
  {
    case opts::DIST_NM:
      return atools::geo::meterToNm(value);

    case opts::DIST_KM:
      return value * 1000.f;

    case opts::DIST_MILES:
      return atools::geo::meterToMi(value);
  }
  return 0.f;
}

float Unit::distNmF(float value)
{
  return distMeterF(atools::geo::nmToMeter(value));
}

QString Unit::distShortMeter(float value, bool addUnit)
{
  return u(locale->toString(distShortMeterF(value), 'f', 0), unitShortDistStr, addUnit);
}

QString Unit::distShortNm(float value, bool addUnit)
{
  return distShortMeter(atools::geo::nmToMeter(value), addUnit);
}

QString Unit::distShortFeet(float value, bool addUnit)
{
  return distShortMeter(atools::geo::feetToMeter(value), addUnit);
}

float Unit::distShortMeterF(float value)
{
  switch(unitShortDist)
  {
    case opts::DIST_SHORT_FT:
      return atools::geo::meterToFeet(value);

    case opts::DIST_SHORT_METER:
      return value;
  }
  return 0.f;
}

float Unit::distShortNmF(float value)
{
  return distShortMeterF(atools::geo::nmToMeter(value));
}

float Unit::distShortFeetF(float value)
{
  return distShortMeterF(atools::geo::feetToMeter(value));
}

QString Unit::speedKts(float value, bool addUnit)
{
  return u(locale->toString(speedKtsF(value), 'f', 0), unitShortDistStr, addUnit);
}

float Unit::speedKtsF(float value)
{
  switch(unitSpeed)
  {
    case opts::SPEED_KTS:
      return value;

    case opts::SPEED_KMH:
      return atools::geo::nmToKm(value);

    case opts::SPEED_MPH:
      return atools::geo::nmToMi(value);
  }
  return 0.f;
}

QString Unit::speedVertFpm(float value, bool addUnit)
{
  return u(locale->toString(speedVertFpmF(value), 'f', 0), unitShortDistStr, addUnit);
}

float Unit::speedVertFpmF(float value)
{
  switch(unitVertSpeed)
  {
    case opts::VERT_SPEED_FPM:
      return value;

    case opts::VERT_SPEED_MS:
      return atools::geo::feetToMeter(value) / 60.f;
  }
  return 0.f;
}

QString Unit::altMeter(float value, bool addUnit)
{
  return u(locale->toString(altMeterF(value), 'f', 0), unitAltStr, addUnit);
}

QString Unit::altFeet(float value, bool addUnit)
{
  return altMeter(atools::geo::feetToMeter(value), addUnit);
}

float Unit::altMeterF(float value)
{
  switch(unitAlt)
  {
    case opts::ALT_FT:
      return atools::geo::meterToFeet(value);

    case opts::ALT_METER:
      return value;
  }
  return 0.f;
}

float Unit::altFeetF(float value)
{
  return altMeterF(atools::geo::feetToMeter(value));
}

QString Unit::coords(const atools::geo::Pos& pos)
{
  switch(Unit::instance().unitCoords)
  {
    case opts::COORDS_DMS:
      return COORDS_DMS_FORMAT.
             arg(atools::absInt(pos.getLatYDeg())).
             arg(atools::absInt(pos.getLatYMin())).
             arg(std::abs(pos.getLatYSec()), 0, 'f', 2).
             arg(pos.getLatY() > 0.f ? "N" : "S").
             arg(atools::absInt(pos.getLonXDeg())).
             arg(atools::absInt(pos.getLonXMin())).
             arg(std::abs(pos.getLonXSec()), 0, 'f', 2).
             arg(pos.getLonX() > 0.f ? "E" : "W");

    case opts::COORDS_DEC:
      return COORDS_DEC_FORMAT.
             arg(std::abs(pos.getLatY())).
             arg(pos.getLatY() > 0.f ? "N" : "S").
             arg(std::abs(pos.getLonX())).
             arg(pos.getLonX() > 0.f ? "E" : "W");
  }
  return QString();
}

QString Unit::u(const QString& num, const QString& un, bool addUnit)
{
  return num + (addUnit ? " " + un : QString());
}

void Unit::optionsChanged()
{
  unitDist = opts->getUnitDist();
  unitShortDist = opts->getUnitShortDist();
  unitAlt = opts->getUnitAlt();
  unitSpeed = opts->getUnitSpeed();
  unitCoords = opts->getUnitCoords();

  switch(unitDist)
  {
    case opts::DIST_NM:
      unitDistStr = Unit::tr("nm");
      break;
    case opts::DIST_KM:
      unitDistStr = Unit::tr("km");
      break;
    case opts::DIST_MILES:
      unitDistStr = Unit::tr("mi");
      break;
  }

  switch(unitShortDist)
  {
    case opts::DIST_SHORT_FT:
      unitShortDistStr = Unit::tr("ft");
      break;
    case opts::DIST_SHORT_METER:
      unitShortDistStr = Unit::tr("m");
      break;
  }

  switch(unitAlt)
  {
    case opts::ALT_FT:
      unitAltStr = Unit::tr("ft");
      break;
    case opts::ALT_METER:
      unitAltStr = Unit::tr("m");
      break;
  }

  switch(unitSpeed)
  {
    case opts::SPEED_KTS:
      unitSpeedStr = Unit::tr("kts");
      break;
    case opts::SPEED_KMH:
      unitSpeedStr = Unit::tr("km/h");
      break;
    case opts::SPEED_MPH:
      unitSpeedStr = Unit::tr("mph");
      break;
  }

  switch(unitVertSpeed)
  {
    case opts::VERT_SPEED_FPM:
      unitVertSpeedStr = Unit::tr("fpm");
      break;
    case opts::VERT_SPEED_MS:
      unitVertSpeedStr = Unit::tr("m/s");
      break;
  }
}
