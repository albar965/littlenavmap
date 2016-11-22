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

QLocale *Unit::locale = nullptr;
const OptionData *Unit::opts = nullptr;

opts::UnitDist Unit::unitDist = opts::DIST_NM;
opts::UnitShortDist Unit::unitShortDist = opts::DIST_SHORT_FT;
opts::UnitAlt Unit::unitAlt = opts::ALT_FT;
opts::UnitSpeed Unit::unitSpeed = opts::SPEED_KTS;
opts::UnitVertSpeed Unit::unitVertSpeed = opts::VERT_SPEED_FPM;
opts::UnitCoords Unit::unitCoords = opts::COORDS_DMS;
opts::UnitFuelAndWeight Unit::unitFuelWeight = opts::FUEL_WEIGHT_GAL_LBS;

QString Unit::unitDistStr(Unit::tr("nm"));
QString Unit::unitShortDistStr(Unit::tr("ft"));
QString Unit::unitAltStr(Unit::tr("ft"));
QString Unit::unitSpeedStr(Unit::tr("kts"));
QString Unit::unitVertSpeedStr(Unit::tr("fpm"));

QString Unit::unitVolStr(Unit::tr("gal"));
QString Unit::unitWeightStr(Unit::tr("lbs"));

QString Unit::unitFfVolStr(Unit::tr("gph"));
QString Unit::unitFfWeightStr(Unit::tr("pph"));

Unit::Unit()
{

}

Unit::~Unit()
{

}

QString Unit::replacePlaceholders(const QString& text, QString& origtext)
{
  if(origtext.isEmpty())
    origtext = text;

  return replacePlaceholders(origtext);
}

QString Unit::replacePlaceholders(const QString& text)
{
  QString retval(text);
  if(retval.contains("%distshort%"))
    retval = QString(retval).replace("%distshort%", unitShortDistStr);
  if(retval.contains("%dist%"))
    retval = QString(retval).replace("%dist%", unitDistStr);
  if(retval.contains("%alt%"))
    retval = QString(retval).replace("%alt%", unitAltStr);
  if(retval.contains("%speed%"))
    retval = QString(retval).replace("%speed%", unitSpeedStr);
  if(retval.contains("%vspeed%"))
    retval = QString(retval).replace("%vspeed%", unitVertSpeedStr);
  return retval;
}

void Unit::init()
{
  if(locale == nullptr)
  {
    locale = new QLocale();
    opts = &OptionData::instance();
    optionsChanged();
  }
}

void Unit::deInit()
{
  delete locale;
  locale = nullptr;
}

QString Unit::distMeter(float value, bool addUnit, int minValPrec, bool narrow)
{
  float val = distMeterF(value);
  return u(locale->toString(val, 'f', val < minValPrec ? 1 : 0), unitDistStr, addUnit, narrow);
}

QString Unit::distNm(float value, bool addUnit, int minValPrec, bool narrow)
{
  return distMeter(atools::geo::nmToMeter(value), addUnit, minValPrec, narrow);
}

float Unit::distMeterF(float value)
{
  switch(unitDist)
  {
    case opts::DIST_NM:
      return atools::geo::meterToNm(value);

    case opts::DIST_KM:
      return value / 1000.f;

    case opts::DIST_MILES:
      return atools::geo::meterToMi(value);
  }
  return 0.f;
}

float Unit::distNmF(float value)
{
  return distMeterF(atools::geo::nmToMeter(value));
}

QString Unit::distShortMeter(float value, bool addUnit, bool narrow)
{
  return u(distShortMeterF(value), unitShortDistStr, addUnit, narrow);
}

QString Unit::distShortNm(float value, bool addUnit, bool narrow)
{
  return distShortMeter(atools::geo::nmToMeter(value), addUnit, narrow);
}

QString Unit::distShortFeet(float value, bool addUnit, bool narrow)
{
  return distShortMeter(atools::geo::feetToMeter(value), addUnit, narrow);
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
  return u(speedKtsF(value), unitSpeedStr, addUnit);
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
  switch(unitVertSpeed)
  {
    case opts::VERT_SPEED_FPM:
      return locale->toString(value, 'f', 0) + (addUnit ? " " + unitVertSpeedStr : QString());

    case opts::VERT_SPEED_MS:
      return locale->toString(atools::geo::feetToMeter(value) / 60.f, 'f', 1) +
             (addUnit ? " " + unitVertSpeedStr : QString());
  }
  return QString();
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

QString Unit::altMeter(float value, bool addUnit, bool narrow)
{
  return u(altMeterF(value), unitAltStr, addUnit, narrow);
}

QString Unit::altFeet(float value, bool addUnit, bool narrow)
{
  return altMeter(atools::geo::feetToMeter(value), addUnit, narrow);
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

QString Unit::volGallon(float value, bool addUnit)
{
  return u(volGallonF(value), unitVolStr, addUnit);
}

QString Unit::weightLbs(float value, bool addUnit)
{
  return u(weightLbsF(value), unitWeightStr, addUnit);
}

float Unit::volGallonF(float value)
{
  switch(unitFuelWeight)
  {
    case opts::FUEL_WEIGHT_GAL_LBS:
      return value;

    case opts::FUEL_WEIGHT_LITER_KG:
      return value * 3.785411784f;
  }
  return 0.f;
}

float Unit::weightLbsF(float value)
{
  switch(unitFuelWeight)
  {
    case opts::FUEL_WEIGHT_GAL_LBS:
      return value;

    case opts::FUEL_WEIGHT_LITER_KG:
      return value / 2.204622f;
  }
  return 0.f;
}

QString Unit::ffGallon(float value, bool addUnit)
{
  return u(volGallonF(value), unitFfVolStr, addUnit);
}

float Unit::ffGallonF(float value)
{
  return volGallonF(value);
}

QString Unit::ffLbs(float value, bool addUnit)
{
  return u(weightLbsF(value), unitFfWeightStr, addUnit);
}

float Unit::ffLbsF(float value)
{
  return weightLbsF(value);
}

QString Unit::coords(const atools::geo::Pos& pos)
{
  switch(unitCoords)
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
             arg(std::abs(pos.getLatY()), 0, 'f', 4, QChar('0')).
             arg(pos.getLatY() > 0.f ? "N" : "S").
             arg(std::abs(pos.getLonX()), 0, 'f', 4, QChar('0')).
             arg(pos.getLonX() > 0.f ? "E" : "W");
  }
  return QString();
}

QString Unit::u(const QString& num, const QString& un, bool addUnit, bool narrow)
{
  if(narrow)
    return num + (addUnit ? un : QString());
  else
    return num + (addUnit ? " " + un : QString());
}

QString Unit::u(float num, const QString& un, bool addUnit, bool narrow)
{
  if(narrow)
    return QString::number(num, 'f', 0) + (addUnit ? QString() + un : QString());
  else
    return locale->toString(num, 'f', 0) + (addUnit ? " " + un : QString());
}

void Unit::optionsChanged()
{
  unitDist = opts->getUnitDist();
  unitShortDist = opts->getUnitShortDist();
  unitAlt = opts->getUnitAlt();
  unitSpeed = opts->getUnitSpeed();
  unitVertSpeed = opts->getUnitVertSpeed();
  unitCoords = opts->getUnitCoords();
  unitFuelWeight = opts->getUnitFuelWeight();

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

  switch(unitFuelWeight)
  {
    case opts::FUEL_WEIGHT_GAL_LBS:
      unitVolStr = Unit::tr("gal");
      unitWeightStr = Unit::tr("lbs");
      unitFfWeightStr = Unit::tr("pph");
      unitFfVolStr = Unit::tr("gph");
      break;
    case opts::FUEL_WEIGHT_LITER_KG:
      unitVolStr = Unit::tr("l");
      unitWeightStr = Unit::tr("kg");
      unitFfWeightStr = Unit::tr("kg/h");
      unitFfVolStr = Unit::tr("l/h");
      break;
  }
}
