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

#include "common/unit.h"

#include "geo/calculations.h"

#include "geo/pos.h"

const static QString COORDS_DEC_FORMAT_LONX("%L1° %L2");
const static QString COORDS_DEC_FORMAT_LATY("%L1° %L2");
const static QString COORDS_DMS_FORMAT_LONX("%L1° %L2' %L3\" %L4");
const static QString COORDS_DMS_FORMAT_LATY("%L1° %L2' %L3\" %L4");
const static QString COORDS_DM_FORMAT_LONX("%L1° %L2' %L3");
const static QString COORDS_DM_FORMAT_LATY("%L1° %L2' %L3");

QLocale *Unit::locale = nullptr;
QLocale *Unit::clocale = nullptr;
const OptionData *Unit::opts = nullptr;

opts::UnitDist Unit::unitDist = opts::DIST_NM;
opts::UnitShortDist Unit::unitShortDist = opts::DIST_SHORT_FT;
opts::UnitAlt Unit::unitAlt = opts::ALT_FT;
opts::UnitSpeed Unit::unitSpeed = opts::SPEED_KTS;
opts::UnitVertSpeed Unit::unitVertSpeed = opts::VERT_SPEED_FPM;
opts::UnitCoords Unit::unitCoords = opts::COORDS_DMS;
opts::UnitFuelAndWeight Unit::unitFuelWeight = opts::FUEL_WEIGHT_GAL_LBS;
bool Unit::showOtherFuel = true;

QString Unit::unitDistStr;
QString Unit::unitShortDistStr;
QString Unit::unitAltStr;
QString Unit::unitSpeedStr;
QString Unit::unitVertSpeedStr;

QString Unit::unitVolStr;
QString Unit::unitWeightStr;

QString Unit::unitFfVolStr;
QString Unit::unitFfWeightStr;

QString Unit::suffixDistNm;
QString Unit::suffixDistKm;
QString Unit::suffixDistMi;
QString Unit::suffixDistShortFt;
QString Unit::suffixDistShortMeter;
QString Unit::suffixAltFt;
QString Unit::suffixAltMeter;
QString Unit::suffixSpeedKts;
QString Unit::suffixSpeedKmH;
QString Unit::suffixSpeedMph;
QString Unit::suffixVertSpeedFpm;
QString Unit::suffixVertSpeedMs;
QString Unit::suffixFuelVolGal;
QString Unit::suffixFuelWeightLbs;
QString Unit::suffixFfWeightPpH;
QString Unit::suffixFfGalGpH;
QString Unit::suffixFuelVolLiter;
QString Unit::suffixFuelWeightKg;
QString Unit::suffixFfWeightKgH;
QString Unit::suffixFfVolLiterH;

Unit::Unit()
{

}

Unit::~Unit()
{

}

QString Unit::replacePlaceholders(const QString& text, QString& origtext, bool fuelAsVolume)
{
  if(origtext.isEmpty())
    origtext = text;

  return replacePlaceholders(origtext, fuelAsVolume);
}

QString Unit::replacePlaceholders(const QString& text, bool fuelAsVolume)
{
  return replacePlaceholders(text, fuelAsVolume, unitFuelWeight);
}

QString Unit::replacePlaceholders(const QString& text, bool fuelAsVolume, opts::UnitFuelAndWeight unit)
{
  QString retval(text);
  retval.replace("%distshort%", unitShortDistStr);
  retval.replace("%dist%", unitDistStr);
  retval.replace("%alt%", unitAltStr);
  retval.replace("%speed%", unitSpeedStr);
  retval.replace("%vspeed%", unitVertSpeedStr);

  switch(unit)
  {
    case opts::FUEL_WEIGHT_GAL_LBS:
      retval.replace("%fuel%", fuelAsVolume ? suffixFuelVolGal : suffixFuelWeightLbs);
      retval.replace("%weight%", suffixFuelWeightLbs);
      retval.replace("%volume%", suffixFuelVolGal);
      break;

    case opts::FUEL_WEIGHT_LITER_KG:
      retval.replace("%fuel%", fuelAsVolume ? suffixFuelVolLiter : suffixFuelWeightKg);
      retval.replace("%weight%", suffixFuelWeightKg);
      retval.replace("%volume%", suffixFuelVolLiter);
      break;
  }

  return retval;
}

void Unit::init()
{
  if(locale == nullptr)
  {
    locale = new QLocale();
    clocale = new QLocale(QLocale::C);
    opts = &OptionData::instance();
    optionsChanged();
  }
}

void Unit::deInit()
{
  delete locale;
  locale = nullptr;
  delete clocale;
  clocale = nullptr;
}

void Unit::initTranslateableTexts()
{
  suffixDistNm = Unit::tr("nm");
  suffixDistKm = Unit::tr("km");
  suffixDistMi = Unit::tr("mi");
  suffixDistShortFt = Unit::tr("ft");
  suffixDistShortMeter = Unit::tr("m");
  suffixAltFt = Unit::tr("ft");
  suffixAltMeter = Unit::tr("m");
  suffixSpeedKts = Unit::tr("kts");
  suffixSpeedKmH = Unit::tr("km/h");
  suffixSpeedMph = Unit::tr("mph");
  suffixVertSpeedFpm = Unit::tr("fpm");
  suffixVertSpeedMs = Unit::tr("m/s");
  suffixFuelVolGal = Unit::tr("gal");
  suffixFuelWeightLbs = Unit::tr("lbs");
  suffixFfWeightPpH = Unit::tr("pph");
  suffixFfGalGpH = Unit::tr("gph");
  suffixFuelVolLiter = Unit::tr("l");
  suffixFuelWeightKg = Unit::tr("kg");
  suffixFfWeightKgH = Unit::tr("kg/h");
  suffixFfVolLiterH = Unit::tr("l/h");

  // Defaults for changeable
  unitDistStr = suffixDistNm;
  unitShortDistStr = suffixDistShortFt;
  unitAltStr = suffixAltFt;
  unitSpeedStr = suffixSpeedKts;
  unitVertSpeedStr = suffixVertSpeedFpm;
  unitVolStr = suffixFuelVolGal;
  unitWeightStr = suffixFuelWeightLbs;
  unitFfVolStr = suffixFfGalGpH;
  unitFfWeightStr = suffixFfWeightPpH;
}

QString Unit::distMeter(float value, bool addUnit, int minValPrec, bool narrow)
{
  float val = distMeterF(value);
  if(narrow)
    return u(clocale->toString(val, 'f', val < minValPrec ? 1 : 0), unitDistStr, addUnit, narrow);
  else
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

QString Unit::speedKts(float value, bool addUnit, bool narrow)
{
  return u(speedKtsF(value), unitSpeedStr, addUnit, narrow);
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

QString Unit::speedMeterPerSec(float value, bool addUnit, bool narrow)
{
  return u(speedMeterPerSecF(value), unitSpeedStr, addUnit, narrow);
}

float Unit::speedMeterPerSecF(float value)
{
  switch(unitSpeed)
  {
    case opts::SPEED_KTS:
      return atools::geo::meterToNm(value * 3600.f);

    case opts::SPEED_KMH:
      return value * 3.6f;

    case opts::SPEED_MPH:
      return atools::geo::meterToMi(value * 3600.f);
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

int Unit::altFeetI(int value)
{
  return atools::roundToInt(altMeterF(atools::geo::feetToMeter(value)));
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

QString Unit::volLiter(float value, bool addUnit)
{
  return u(volLiterF(value), unitVolStr, addUnit);
}

float Unit::volLiterF(float value)
{
  switch(unitFuelWeight)
  {
    case opts::FUEL_WEIGHT_GAL_LBS:
      return value / 3.785411784f;

    case opts::FUEL_WEIGHT_LITER_KG:
      return value;
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

QString Unit::weightKg(float value, bool addUnit)
{
  return u(weightKgF(value), unitWeightStr, addUnit);
}

float Unit::weightKgF(float value)
{
  switch(unitFuelWeight)
  {
    case opts::FUEL_WEIGHT_GAL_LBS:
      return value * 2.204622f;

    case opts::FUEL_WEIGHT_LITER_KG:
      return value;
  }
  return 0.f;
}

QString Unit::localOtherText(bool localBold, bool otherSmall)
{
  if(showOtherFuel)
    return (localBold ? tr("<b>%1</b>") : tr("%1")) +
           (otherSmall ? tr(" <span style=\"font-size: small;\">(%2)</span>") : tr(" (%2)"));
  else
    return localBold ? tr("<b>%1</b>") : tr("%1");
}

QString Unit::localOtherText2(bool localBold, bool otherSmall)
{
  if(showOtherFuel)
    return (localBold ? tr("<b>%1, %2</b>") : tr("%1, %2")) +
           (otherSmall ? tr(" <span style=\"font-size: small;\">(%3, %4)</span>") : tr(" (%3, %4)"));
  else
    return localBold ? tr("<b>%1, %2</b>") : tr("%1, %2");
}

QString Unit::weightLbsLocalOther(float valueLbs, bool localBold, bool otherSmall)
{
  switch(unitFuelWeight)
  {
    case opts::FUEL_WEIGHT_GAL_LBS:
      if(showOtherFuel)
        // lbs (kg)
        return localOtherText(localBold, otherSmall).
               arg(u(valueLbs, suffixFuelWeightLbs, true)).
               arg(u(atools::geo::lbsToKg(valueLbs), suffixFuelWeightKg, true));
      else
        // lbs only
        return localOtherText(localBold, otherSmall).
               arg(u(valueLbs, suffixFuelWeightLbs, true));

    case opts::FUEL_WEIGHT_LITER_KG:
      if(showOtherFuel)
        // kg (lbs)
        return localOtherText(localBold, otherSmall).
               arg(u(atools::geo::lbsToKg(valueLbs), suffixFuelWeightKg, true)).
               arg(u(valueLbs, suffixFuelWeightLbs, true));
      else
        // kg only
        return localOtherText(localBold, otherSmall).
               arg(u(atools::geo::lbsToKg(valueLbs), suffixFuelWeightKg, true));
  }
  return QString();
}

QString Unit::fuelLbsAndGalLocalOther(float valueLbs, float valueGal, bool localBold, bool otherSmall)
{
  switch(unitFuelWeight)
  {
    case opts::FUEL_WEIGHT_GAL_LBS:
      if(showOtherFuel)
        // lbs, gal (kg, liter)
        return localOtherText2(localBold, otherSmall).
               arg(u(valueLbs, suffixFuelWeightLbs, true)).
               arg(u(valueGal, suffixFuelVolGal, true)).
               arg(u(atools::geo::lbsToKg(valueLbs), suffixFuelWeightKg, true)).
               arg(u(atools::geo::gallonToLiter(valueGal), suffixFuelVolLiter, true));
      else
        // lbs, gal only
        return localOtherText2(localBold, otherSmall).
               arg(u(valueLbs, suffixFuelWeightLbs, true)).
               arg(u(valueGal, suffixFuelVolGal, true));

    case opts::FUEL_WEIGHT_LITER_KG:
      if(showOtherFuel)
        // kg, liter (lbs, gal)
        return localOtherText2(localBold, otherSmall).
               arg(u(atools::geo::lbsToKg(valueLbs), suffixFuelWeightKg, true)).
               arg(u(atools::geo::gallonToLiter(valueGal), suffixFuelVolLiter, true)).
               arg(u(valueLbs, suffixFuelWeightLbs, true)).
               arg(u(valueGal, suffixFuelVolGal, true));
      else
        // kg, liter only
        return localOtherText2(localBold, otherSmall).
               arg(u(atools::geo::lbsToKg(valueLbs), suffixFuelWeightKg, true)).
               arg(u(atools::geo::gallonToLiter(valueGal), suffixFuelVolLiter, true));
  }
  return QString();
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

QString Unit::ffLbsAndGal(float valueLbs, float valueGal, bool addUnit)
{
  return tr("%1, %2").arg(ffLbs(valueLbs, addUnit)).arg(ffGallon(valueGal, addUnit));
}

QString Unit::fuelLbsAndGal(float valueLbs, float valueGal, bool addUnit)
{
  return tr("%1, %2").arg(weightLbs(valueLbs, addUnit)).arg(volGallon(valueGal, addUnit));
}

QString Unit::fuelLbsGallon(float value, bool addUnit, bool fuelAsVolume)
{
  return fuelAsVolume ? volGallon(value, addUnit) : weightLbs(value, addUnit);
}

float Unit::fuelLbsGallonF(float value, bool fuelAsVolume)
{
  return fuelAsVolume ? volGallonF(value) : weightLbsF(value);
}

QString Unit::ffLbsGallon(float value, bool addUnit, bool fuelAsVolume)
{
  return fuelAsVolume ? ffGallon(value, addUnit) : ffLbs(value, addUnit);
}

float Unit::ffLbsGallonF(float value, bool fuelAsVolume)
{
  return fuelAsVolume ? ffGallonF(value) : ffLbsF(value);
}

QString Unit::ffLiter(float value, bool addUnit)
{
  return u(volLiterF(value), unitFfVolStr, addUnit);
}

float Unit::ffLiterF(float value)
{
  return volLiterF(value);
}

QString Unit::ffKg(float value, bool addUnit)
{
  return u(weightKgF(value), unitFfWeightStr, addUnit);
}

float Unit::ffKgF(float value)
{
  return weightKgF(value);
}

QString Unit::ffKgAndLiter(float valueKg, float valueLiter, bool addUnit)
{
  return tr("%1, %2").arg(ffKg(valueKg, addUnit)).arg(ffLiter(valueLiter, addUnit));
}

QString Unit::fuelKgAndLiter(float valueKg, float valueLiter, bool addUnit)
{
  return tr("%1, %2").arg(weightKg(valueKg, addUnit), volLiter(valueLiter, addUnit));
}

QString Unit::fuelKgLiter(float value, bool addUnit, bool fuelAsVolume)
{
  return fuelAsVolume ? volLiter(value, addUnit) : weightKg(value, addUnit);
}

float Unit::fuelKgLiterF(float value, bool fuelAsVolume)
{
  return fuelAsVolume ? volLiterF(value) : weightKgF(value);
}

QString Unit::ffKgLiter(float value, bool addUnit, bool fuelAsVolume)
{
  return fuelAsVolume ? ffLiter(value, addUnit) : ffKg(value, addUnit);
}

float Unit::ffKgLiterF(float value, bool fuelAsVolume)
{
  return fuelAsVolume ? ffLiterF(value) : ffKgF(value);
}

QString Unit::coords(const atools::geo::Pos& pos)
{
  return coords(pos, unitCoords);
}

QString Unit::coords(const atools::geo::Pos& pos, opts::UnitCoords coordUnit)
{
  if(!pos.isValid())
    return QObject::tr("Invalid");

  if(coordUnit == opts::COORDS_LATY_LONX)
    return tr("%1 %2").arg(QLocale().toString(pos.getLatY(), 'f', 5)).arg(QLocale().toString(pos.getLonX(), 'f', 5));
  else if(coordUnit == opts::COORDS_LONX_LATY)
    return tr("%1 %2").arg(QLocale().toString(pos.getLonX(), 'f', 5)).arg(QLocale().toString(pos.getLatY(), 'f', 5));
  else
    return tr("%1 %2").arg(coordsLatY(pos, coordUnit)).arg(coordsLonX(pos, coordUnit));
}

QString Unit::coordsLonX(const atools::geo::Pos& pos)
{
  return coordsLonX(pos, unitCoords);
}

QString Unit::coordsLonX(const atools::geo::Pos& pos, opts::UnitCoords coordUnit)
{
  if(!pos.isValid())
    return QObject::tr("Invalid");

  switch(coordUnit)
  {
    case opts::COORDS_LATY_LONX:
    case opts::COORDS_LONX_LATY:
      return QLocale().toString(pos.getLonX(), 'f', 5);

    case opts::COORDS_DMS:
      return COORDS_DMS_FORMAT_LONX.
             arg(atools::absInt(pos.getLonXDeg())).
             arg(atools::absInt(pos.getLonXMin())).
             arg(std::abs(pos.getLonXSec()), 0, 'f', 2).
             arg(pos.getLonX() > 0.f ? "E" : "W");

    case opts::COORDS_DEC:
      return COORDS_DEC_FORMAT_LONX.
             arg(std::abs(pos.getLonX()), 0, 'f', 4, QChar('0')).
             arg(pos.getLonX() > 0.f ? "E" : "W");

    case opts::COORDS_DM:
      return COORDS_DM_FORMAT_LONX.
             arg(atools::absInt(pos.getLonXDeg())).
             arg(std::abs(pos.getLonXMin() + pos.getLonXSec() / 60.f), 0, 'f', 2).
             arg(pos.getLonX() > 0.f ? "E" : "W");
  }
  return QString();
}

QString Unit::coordsLatY(const atools::geo::Pos& pos)
{
  return coordsLatY(pos, unitCoords);
}

QString Unit::coordsLatY(const atools::geo::Pos& pos, opts::UnitCoords coordUnit)
{
  if(!pos.isValid())
    return QObject::tr("Invalid");

  switch(coordUnit)
  {
    case opts::COORDS_LATY_LONX:
    case opts::COORDS_LONX_LATY:
      return QLocale().toString(pos.getLatY(), 'f', 5);

    case opts::COORDS_DMS:
      return COORDS_DMS_FORMAT_LATY.
             arg(atools::absInt(pos.getLatYDeg())).
             arg(atools::absInt(pos.getLatYMin())).
             arg(std::abs(pos.getLatYSec()), 0, 'f', 2).
             arg(pos.getLatY() > 0.f ? "N" : "S");

    case opts::COORDS_DEC:
      return COORDS_DEC_FORMAT_LATY.
             arg(std::abs(pos.getLatY()), 0, 'f', 4, QChar('0')).
             arg(pos.getLatY() > 0.f ? "N" : "S");

    case opts::COORDS_DM:
      return COORDS_DM_FORMAT_LATY.
             arg(atools::absInt(pos.getLatYDeg())).
             arg(std::abs(pos.getLatYMin() + pos.getLatYSec() / 60.f), 0, 'f', 2).
             arg(pos.getLatY() > 0.f ? "N" : "S");
  }
  return QString();
}

QString Unit::u(const QString& num, const QString& un, bool addUnit, bool narrow)
{
  if(narrow)
  {
    // Get rid of the trailing dot zero
    QString nm(num);
    if(nm.endsWith(QString(locale->decimalPoint()) + "0"))
      nm.chop(2);

    return nm + (addUnit ? un : QString());
  }
  else
    return num + (addUnit ? " " + un : QString());
}

QString Unit::u(float num, const QString& un, bool addUnit, bool narrow)
{
  if(narrow)
    return clocale->toString(num, 'f', 0) + (addUnit ? QString() + un : QString());
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
  unitFuelWeight = opts->getUnitFuelAndWeight();
  showOtherFuel = opts->getFlags2() & opts2::UNIT_FUEL_SHOW_OTHER;

  switch(unitDist)
  {
    case opts::DIST_NM:
      unitDistStr = suffixDistNm; // Unit::tr("nm");
      break;
    case opts::DIST_KM:
      unitDistStr = suffixDistKm; // Unit::tr("km");
      break;
    case opts::DIST_MILES:
      unitDistStr = suffixDistMi; // Unit::tr("mi");
      break;
  }

  switch(unitShortDist)
  {
    case opts::DIST_SHORT_FT:
      unitShortDistStr = suffixDistShortFt; // Unit::tr("ft");
      break;
    case opts::DIST_SHORT_METER:
      unitShortDistStr = suffixDistShortMeter; // Unit::tr("m");
      break;
  }

  switch(unitAlt)
  {
    case opts::ALT_FT:
      unitAltStr = suffixAltFt; // Unit::tr("ft");
      break;
    case opts::ALT_METER:
      unitAltStr = suffixAltMeter; // Unit::tr("m");
      break;
  }

  switch(unitSpeed)
  {
    case opts::SPEED_KTS:
      unitSpeedStr = suffixSpeedKts; // Unit::tr("kts");
      break;
    case opts::SPEED_KMH:
      unitSpeedStr = suffixSpeedKmH; // Unit::tr("km/h");
      break;
    case opts::SPEED_MPH:
      unitSpeedStr = suffixSpeedMph; // Unit::tr("mph");
      break;
  }

  switch(unitVertSpeed)
  {
    case opts::VERT_SPEED_FPM:
      unitVertSpeedStr = suffixVertSpeedFpm; // Unit::tr("fpm");
      break;
    case opts::VERT_SPEED_MS:
      unitVertSpeedStr = suffixVertSpeedMs; // Unit::tr("m/s");
      break;
  }

  switch(unitFuelWeight)
  {
    case opts::FUEL_WEIGHT_GAL_LBS:
      unitVolStr = suffixFuelVolGal; // Unit::tr("gal");
      unitWeightStr = suffixFuelWeightLbs; // Unit::tr("lbs");
      unitFfWeightStr = suffixFfWeightPpH; // Unit::tr("pph");
      unitFfVolStr = suffixFfGalGpH; // Unit::tr("gph");
      break;

    case opts::FUEL_WEIGHT_LITER_KG:
      unitVolStr = suffixFuelVolLiter; // Unit::tr("l");
      unitWeightStr = suffixFuelWeightKg; // Unit::tr("kg");
      unitFfWeightStr = suffixFfWeightKgH; // Unit::tr("kg/h");
      unitFfVolStr = suffixFfVolLiterH; // Unit::tr("l/h");
      break;
  }
}

float Unit::fromUsToMetric(float value, bool fuelAsVolume)
{
  return fuelAsVolume ? atools::geo::gallonToLiter(value) : atools::geo::lbsToKg(value);
}

float Unit::fromMetricToUs(float value, bool fuelAsVolume)
{
  return fuelAsVolume ? atools::geo::literToGallon(value) : atools::geo::kgToLbs(value);
}

float Unit::fromCopy(float value, bool fuelAsVolume)
{
  Q_UNUSED(fuelAsVolume);
  return value;
}
