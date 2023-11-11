/*****************************************************************************
* Copyright 2015-2023 Alexander Barthel alex@littlenavmap.org
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

#include <QStringBuilder>

namespace ageo = atools::geo;

const static QString COORDS_DEC_FORMAT_LONX("%L1° %L2");
const static QString COORDS_DEC_FORMAT_LATY("%L1° %L2");
const static QString COORDS_DMS_FORMAT_LONX("%L1° %L2' %L3\" %L4");
const static QString COORDS_DMS_FORMAT_LATY("%L1° %L2' %L3\" %L4");
const static QString COORDS_DM_FORMAT_LONX("%L1° %L2' %L3");
const static QString COORDS_DM_FORMAT_LATY("%L1° %L2' %L3");
const static QString COORDS_DM_GOOGLE_FORMAT_LONX("%1 %2");
const static QString COORDS_DM_GOOGLE_FORMAT_LATY("%1 %2");

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
  retval.replace("%dists%", unitShortDistStr);
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
  ATOOLS_DELETE_LOG(locale);
  ATOOLS_DELETE_LOG(clocale);
}

void Unit::initTranslateableTexts()
{
  suffixDistNm = Unit::tr("NM");
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

QString Unit::distMeter(float meter, bool addUnit, int minValPrec, bool narrow)
{
  float localValue = distMeterF(meter);
  if(narrow)
    return u(clocale->toString(localValue, 'f', localValue < minValPrec ? 1 : 0), unitDistStr, addUnit, narrow);
  else
    return u(locale->toString(localValue, 'f', localValue < minValPrec ? 1 : 0), unitDistStr, addUnit, narrow);
}

QString Unit::distNm(float nm, bool addUnit, int minValPrec, bool narrow)
{
  float localValue = distNmF(nm);
  if(narrow)
    return u(clocale->toString(localValue, 'f', localValue < minValPrec ? 1 : 0), unitDistStr, addUnit, narrow);
  else
    return u(locale->toString(localValue, 'f', localValue < minValPrec ? 1 : 0), unitDistStr, addUnit, narrow);
}

float Unit::distMeterF(float meter)
{
  switch(unitDist)
  {
    case opts::DIST_NM:
      return ageo::meterToNm(meter);

    case opts::DIST_KM:
      return meter / 1000.f;

    case opts::DIST_MILES:
      return ageo::meterToMi(meter);
  }
  return 0.f;
}

float Unit::distNmF(float nm)
{
  switch(unitDist)
  {
    case opts::DIST_NM:
      return nm;

    case opts::DIST_KM:
      return ageo::nmToMeter(nm) / 1000.f;

    case opts::DIST_MILES:
      return ageo::nmToMi(nm);
  }
  return 0.f;
}

QString Unit::distShortMeter(float meter, bool addUnit, bool narrow)
{
  return u(distShortMeterF(meter), unitShortDistStr, addUnit, narrow);
}

QString Unit::distShortNm(float nm, bool addUnit, bool narrow)
{
  return u(distShortNmF(nm), unitShortDistStr, addUnit, narrow);
}

QString Unit::distShortFeet(float ft, bool addUnit, bool narrow)
{
  return u(distShortFeetF(ft), unitShortDistStr, addUnit, narrow);
}

QString Unit::distLongShortMeter(float distanceMeter, const QString& separator, bool addUnit, bool narrow)
{
  QString distStr;
  float localDist = Unit::distMeterF(distanceMeter);
  if(Unit::getUnitDist() == opts::DIST_KM && Unit::getUnitShortDist() == opts::DIST_SHORT_METER)
  {
    // Use either km or meter
    if(localDist < 6.f)
      distStr = Unit::distShortMeter(distanceMeter, addUnit, narrow);
    else
      distStr = Unit::distMeter(distanceMeter, addUnit, 20, narrow);
  }
  else
  {
    // Use NM/mi and feet
    distStr = Unit::distMeter(distanceMeter, addUnit, 20, narrow);

    if(localDist < 3.f)
      // Add feet or meter to text for short distances below three local units
      distStr.append(separator % Unit::distShortMeter(distanceMeter, addUnit, narrow));
  }
  return distStr;
}

float Unit::distShortMeterF(float meter)
{
  switch(unitShortDist)
  {
    case opts::DIST_SHORT_FT:
      return ageo::meterToFeet(meter);

    case opts::DIST_SHORT_METER:
      return meter;
  }
  return 0.f;
}

float Unit::distShortNmF(float nm)
{
  switch(unitShortDist)
  {
    case opts::DIST_SHORT_FT:
      return ageo::nmToFeet(nm);

    case opts::DIST_SHORT_METER:
      return ageo::nmToMeter(nm);
  }
  return 0.f;
}

float Unit::distShortFeetF(float ft)
{
  switch(unitShortDist)
  {
    case opts::DIST_SHORT_FT:
      return ft;

    case opts::DIST_SHORT_METER:
      return ageo::feetToMeter(ft);
  }
  return 0.f;
}

QString Unit::speedKts(float kts, bool addUnit, bool narrow)
{
  return u(speedKtsF(kts), unitSpeedStr, addUnit, narrow);
}

QStringList Unit::speedKtsOther(float kts, bool addUnit, bool narrow)
{
  switch(unitSpeed)
  {
    case opts::SPEED_KTS:
      // Default is kts and kts input - print km/h and mph
      return {u(ageo::nmToKm(kts), suffixSpeedKmH, addUnit, narrow),
              u(ageo::nmToMi(kts), suffixSpeedMph, addUnit, narrow)};

    case opts::SPEED_KMH:
      // Default is km/h and kts input - print kts and mph
      return {u(kts, suffixSpeedKts, addUnit, narrow),
              u(ageo::nmToMi(kts), suffixSpeedMph, addUnit, narrow)};

    case opts::SPEED_MPH:
      // Default is mph and kts input - print kts and km/h
      return {u(kts, suffixSpeedKts, addUnit, narrow),
              u(ageo::nmToKm(kts), suffixSpeedKmH, addUnit, narrow)};
  }
  return QStringList();
}

float Unit::speedKtsF(float kts)
{
  switch(unitSpeed)
  {
    case opts::SPEED_KTS:
      return kts;

    case opts::SPEED_KMH:
      return ageo::nmToKm(kts);

    case opts::SPEED_MPH:
      return ageo::nmToMi(kts);
  }
  return 0.f;
}

QString Unit::speedMeterPerSec(float mPerSec, bool addUnit, bool narrow)
{
  return u(speedMeterPerSecF(mPerSec), unitSpeedStr, addUnit, narrow);
}

float Unit::speedMeterPerSecF(float mPerSec)
{
  switch(unitSpeed)
  {
    case opts::SPEED_KTS:
      return ageo::meterToNm(mPerSec * 3600.f);

    case opts::SPEED_KMH:
      return mPerSec * 3.6f;

    case opts::SPEED_MPH:
      return ageo::meterToMi(mPerSec * 3600.f);
  }
  return 0.f;
}

QString Unit::speedVertFpm(float fpm, bool addUnit)
{
  switch(unitVertSpeed)
  {
    case opts::VERT_SPEED_FPM:
      return locale->toString(fpm, 'f', 0) % (addUnit ? " " % unitVertSpeedStr : QString());

    case opts::VERT_SPEED_MS:
      return locale->toString(ageo::feetToMeter(fpm) / 60.f, 'f', 2) % (addUnit ? " " % unitVertSpeedStr : QString());
  }
  return QString();
}

float Unit::speedVertFpmF(float fpm)
{
  switch(unitVertSpeed)
  {
    case opts::VERT_SPEED_FPM:
      return fpm;

    case opts::VERT_SPEED_MS:
      return ageo::feetToMeter(fpm) / 60.f;
  }
  return 0.f;
}

QString Unit::speedVertFpmOther(float fpm, bool addUnit)
{
  switch(unitVertSpeed)
  {
    case opts::VERT_SPEED_FPM:
      // Default is ft/m and ft/m input - print m/s
      return locale->toString(ageo::feetToMeter(fpm) / 60.f, 'f', 2) % (addUnit ? " " % suffixVertSpeedMs : QString());

    case opts::VERT_SPEED_MS:
      // Default is m/s and ft/m input - print ft/m
      return locale->toString(fpm, 'f', 0) % (addUnit ? " " % suffixVertSpeedFpm : QString());
  }
  return QString();

}

QString Unit::altMeter(float meter, bool addUnit, bool narrow, float round)
{
  return u(atools::roundToNearest(altMeterF(meter), round), unitAltStr, addUnit, narrow);
}

QString Unit::altFeet(float ft, bool addUnit, bool narrow, float round)
{
  return u(atools::roundToNearest(altFeetF(ft), round), unitAltStr, addUnit, narrow);
}

QString Unit::altFeetOther(float ft, bool addUnit, bool narrow, float round)
{
  switch(unitAlt)
  {
    case opts::ALT_FT:
      // Default is ft and ft input - print meter
      return u(atools::roundToNearest(ageo::feetToMeter(ft), round), suffixAltMeter, addUnit, narrow);

    case opts::ALT_METER:
      // Default is meter and ft input - print ft
      return u(atools::roundToNearest(ft, round), suffixAltFt, addUnit, narrow);
  }
  return QString();
}

float Unit::altMeterF(float meter)
{
  switch(unitAlt)
  {
    case opts::ALT_FT:
      return ageo::meterToFeet(meter);

    case opts::ALT_METER:
      return meter;
  }
  return 0.f;
}

float Unit::altFeetF(float ft)
{
  switch(unitAlt)
  {
    case opts::ALT_FT:
      return ft;

    case opts::ALT_METER:
      return ageo::feetToMeter(ft);
  }
  return 0.f;
}

int Unit::altFeetI(int ft)
{
  return atools::roundToInt(altMeterF(ageo::feetToMeter(static_cast<float>(ft))));
}

QString Unit::volGallon(float gal, bool addUnit)
{
  return u(volGallonF(gal), unitVolStr, addUnit);
}

QString Unit::weightLbs(float lbs, bool addUnit)
{
  return u(weightLbsF(lbs), unitWeightStr, addUnit);
}

float Unit::volGallonF(float gal)
{
  switch(unitFuelWeight)
  {
    case opts::FUEL_WEIGHT_GAL_LBS:
      return gal;

    case opts::FUEL_WEIGHT_LITER_KG:
      return gal * 3.785411784f;
  }
  return 0.f;
}

QString Unit::volLiter(float liter, bool addUnit)
{
  return u(volLiterF(liter), unitVolStr, addUnit);
}

float Unit::volLiterF(float liter)
{
  switch(unitFuelWeight)
  {
    case opts::FUEL_WEIGHT_GAL_LBS:
      return liter / 3.785411784f;

    case opts::FUEL_WEIGHT_LITER_KG:
      return liter;
  }
  return 0.f;
}

float Unit::weightLbsF(float lbs)
{
  switch(unitFuelWeight)
  {
    case opts::FUEL_WEIGHT_GAL_LBS:
      return lbs;

    case opts::FUEL_WEIGHT_LITER_KG:
      return lbs / 2.204622f;
  }
  return 0.f;
}

QString Unit::weightKg(float kg, bool addUnit)
{
  return u(weightKgF(kg), unitWeightStr, addUnit);
}

float Unit::weightKgF(float kg)
{
  switch(unitFuelWeight)
  {
    case opts::FUEL_WEIGHT_GAL_LBS:
      return kg * 2.204622f;

    case opts::FUEL_WEIGHT_LITER_KG:
      return kg;
  }
  return 0.f;
}

QString Unit::localOtherText(bool localBold, bool otherSmall)
{
  if(showOtherFuel)
    return (localBold ? tr("<b>%1</b>") : tr("%1")) %
           (otherSmall ? tr(" <span style=\"font-size: small;\">(%2)</span>") : tr(" (%2)"));
  else
    return localBold ? tr("<b>%1</b>") : tr("%1");
}

QString Unit::localOtherText2(bool localBold, bool otherSmall)
{
  if(showOtherFuel)
    return (localBold ? tr("<b>%1, %2</b>") : tr("%1, %2")) %
           (otherSmall ? tr(" <span style=\"font-size: small;\">(%3, %4)</span>") : tr(" (%3, %4)"));
  else
    return localBold ? tr("<b>%1, %2</b>") : tr("%1, %2");
}

QString Unit::weightLbsLocalOther(float lbs, bool localBold, bool otherSmall)
{
  switch(unitFuelWeight)
  {
    case opts::FUEL_WEIGHT_GAL_LBS:
      if(showOtherFuel)
        // lbs (kg)
        return localOtherText(localBold, otherSmall).
               arg(u(lbs, suffixFuelWeightLbs, true)).
               arg(u(ageo::lbsToKg(lbs), suffixFuelWeightKg, true));
      else
        // lbs only
        return localOtherText(localBold, otherSmall).
               arg(u(lbs, suffixFuelWeightLbs, true));

    case opts::FUEL_WEIGHT_LITER_KG:
      if(showOtherFuel)
        // kg (lbs)
        return localOtherText(localBold, otherSmall).
               arg(u(ageo::lbsToKg(lbs), suffixFuelWeightKg, true)).
               arg(u(lbs, suffixFuelWeightLbs, true));
      else
        // kg only
        return localOtherText(localBold, otherSmall).
               arg(u(ageo::lbsToKg(lbs), suffixFuelWeightKg, true));
  }
  return QString();
}

QString Unit::fuelLbsAndGalLocalOther(float lbs, float gal, bool localBold, bool otherSmall)
{
  switch(unitFuelWeight)
  {
    case opts::FUEL_WEIGHT_GAL_LBS:
      if(showOtherFuel)
        // lbs, gal (kg, liter)
        return localOtherText2(localBold, otherSmall).
               arg(u(lbs, suffixFuelWeightLbs, true)).
               arg(u(gal, suffixFuelVolGal, true)).
               arg(u(ageo::lbsToKg(lbs), suffixFuelWeightKg, true)).
               arg(u(ageo::gallonToLiter(gal), suffixFuelVolLiter, true));
      else
        // lbs, gal only
        return localOtherText2(localBold, otherSmall).
               arg(u(lbs, suffixFuelWeightLbs, true)).
               arg(u(gal, suffixFuelVolGal, true));

    case opts::FUEL_WEIGHT_LITER_KG:
      if(showOtherFuel)
        // kg, liter (lbs, gal)
        return localOtherText2(localBold, otherSmall).
               arg(u(ageo::lbsToKg(lbs), suffixFuelWeightKg, true)).
               arg(u(ageo::gallonToLiter(gal), suffixFuelVolLiter, true)).
               arg(u(lbs, suffixFuelWeightLbs, true)).
               arg(u(gal, suffixFuelVolGal, true));
      else
        // kg, liter only
        return localOtherText2(localBold, otherSmall).
               arg(u(ageo::lbsToKg(lbs), suffixFuelWeightKg, true)).
               arg(u(ageo::gallonToLiter(gal), suffixFuelVolLiter, true));
  }
  return QString();
}

QString Unit::ffGallon(float gal, bool addUnit)
{
  return u(volGallonF(gal), unitFfVolStr, addUnit);
}

float Unit::ffGallonF(float gal)
{
  return volGallonF(gal);
}

QString Unit::ffLbs(float lbs, bool addUnit)
{
  return u(weightLbsF(lbs), unitFfWeightStr, addUnit);
}

float Unit::ffLbsF(float lbs)
{
  return weightLbsF(lbs);
}

QString Unit::ffLbsAndGal(float lbs, float gal, bool addUnit)
{
  return tr("%1, %2").arg(ffLbs(lbs, addUnit)).arg(ffGallon(gal, addUnit));
}

QString Unit::fuelLbsAndGal(float lbs, float gal, bool addUnit)
{
  return tr("%1, %2").arg(weightLbs(lbs, addUnit)).arg(volGallon(gal, addUnit));
}

QString Unit::fuelLbsGallon(float gal, bool addUnit, bool fuelAsVolume)
{
  return fuelAsVolume ? volGallon(gal, addUnit) : weightLbs(gal, addUnit);
}

float Unit::fuelLbsGallonF(float gal, bool fuelAsVolume)
{
  return fuelAsVolume ? volGallonF(gal) : weightLbsF(gal);
}

QString Unit::ffLbsGallon(float gal, bool addUnit, bool fuelAsVolume)
{
  return fuelAsVolume ? ffGallon(gal, addUnit) : ffLbs(gal, addUnit);
}

float Unit::ffLbsGallonF(float gal, bool fuelAsVolume)
{
  return fuelAsVolume ? ffGallonF(gal) : ffLbsF(gal);
}

QString Unit::ffLiter(float liter, bool addUnit)
{
  return u(volLiterF(liter), unitFfVolStr, addUnit);
}

float Unit::ffLiterF(float liter)
{
  return volLiterF(liter);
}

QString Unit::ffKg(float kg, bool addUnit)
{
  return u(weightKgF(kg), unitFfWeightStr, addUnit);
}

float Unit::ffKgF(float kg)
{
  return weightKgF(kg);
}

QString Unit::ffKgAndLiter(float kg, float liter, bool addUnit)
{
  return tr("%1, %2").arg(ffKg(kg, addUnit)).arg(ffLiter(liter, addUnit));
}

QString Unit::fuelKgAndLiter(float kg, float liter, bool addUnit)
{
  return tr("%1, %2").arg(weightKg(kg, addUnit), volLiter(liter, addUnit));
}

QString Unit::fuelKgLiter(float kgLiter, bool addUnit, bool fuelAsVolume)
{
  return fuelAsVolume ? volLiter(kgLiter, addUnit) : weightKg(kgLiter, addUnit);
}

float Unit::fuelKgLiterF(float kgLiter, bool fuelAsVolume)
{
  return fuelAsVolume ? volLiterF(kgLiter) : weightKgF(kgLiter);
}

QString Unit::ffKgLiter(float kgLiter, bool addUnit, bool fuelAsVolume)
{
  return fuelAsVolume ? ffLiter(kgLiter, addUnit) : ffKg(kgLiter, addUnit);
}

float Unit::ffKgLiterF(float kgLiter, bool fuelAsVolume)
{
  return fuelAsVolume ? ffLiterF(kgLiter) : ffKgF(kgLiter);
}

QString Unit::adjustNum(QString num)
{
  if(!num.isEmpty())
  {
    if(num.contains(locale->decimalPoint()))
    {
      while(num.back() == locale->zeroDigit())
        num.chop(1);
    }

    if(num.back() == locale->decimalPoint())
      num.chop(1);
  }
  return num;
}

QString Unit::coords(const ageo::Pos& pos)
{
  return coords(pos, unitCoords);
}

QString Unit::coords(const ageo::Pos& pos, opts::UnitCoords coordUnit)
{
  if(!pos.isValid())
    return QObject::tr("Invalid");

  if(coordUnit == opts::COORDS_LATY_LONX)
    return tr("%1 %2").arg(adjustNum(locale->toString(pos.getLatY(), 'f', 5))).arg(adjustNum(locale->toString(pos.getLonX(), 'f', 5)));
  else if(coordUnit == opts::COORDS_LONX_LATY)
    return tr("%1 %2").arg(adjustNum(locale->toString(pos.getLonX(), 'f', 5))).arg(adjustNum(locale->toString(pos.getLatY(), 'f', 5)));
  else if(coordUnit == opts::COORDS_DECIMAL_GOOGLE)
    return tr("%1, %2").arg(coordsLatY(pos, coordUnit)).arg(coordsLonX(pos, coordUnit));
  else
    return tr("%1 %2").arg(coordsLatY(pos, coordUnit)).arg(coordsLonX(pos, coordUnit));
}

QString Unit::coordsLonX(const ageo::Pos& pos)
{
  return coordsLonX(pos, unitCoords);
}

QString Unit::coordsLonX(const ageo::Pos& pos, opts::UnitCoords coordUnit)
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

    case opts::COORDS_DECIMAL_GOOGLE:
      return COORDS_DM_GOOGLE_FORMAT_LONX.
             arg(pos.getLonXDeg()).
             arg(std::abs(pos.getLonXMin() + pos.getLonXSec() / 60.f), 0, 'f', 2);
  }
  return QString();
}

QString Unit::coordsLatY(const ageo::Pos& pos)
{
  return coordsLatY(pos, unitCoords);
}

QString Unit::coordsLatY(const ageo::Pos& pos, opts::UnitCoords coordUnit)
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

    case opts::COORDS_DECIMAL_GOOGLE:
      return COORDS_DM_GOOGLE_FORMAT_LATY.
             arg(pos.getLatYDeg()).
             arg(std::abs(pos.getLatYMin() + pos.getLatYSec() / 60.f), 0, 'f', 2);
  }
  return QString();
}

QString Unit::u(const QString& num, const QString& un, bool addUnit, bool narrow)
{
  if(narrow)
  {
    // Get rid of the trailing dot zeroes
    QString nm(num);
    if(nm.endsWith(QString(locale->decimalPoint()) + "0"))
      nm.chop(2);
    return nm % (addUnit ? un : QString());
  }
  else
    return num % (addUnit ? " " % un : QString());
}

QString Unit::u(float num, const QString& un, bool addUnit, bool narrow)
{
  if(narrow)
    return clocale->toString(num, 'f', 0) % (addUnit ? QString() % un : QString());
  else
    return locale->toString(num, 'f', 0) % (addUnit ? " " % un : QString());
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
      unitDistStr = suffixDistNm; // Unit::tr("NM");
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
  return fuelAsVolume ? ageo::gallonToLiter(value) : ageo::lbsToKg(value);
}

float Unit::fromMetricToUs(float value, bool fuelAsVolume)
{
  return fuelAsVolume ? ageo::literToGallon(value) : ageo::kgToLbs(value);
}

float Unit::fromCopy(float value, bool)
{
  return value;
}
