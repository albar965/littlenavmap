/*****************************************************************************
* Copyright 2015-2017 Alexander Barthel albar965@mailbox.org
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

#include "common/mapcolors.h"

#include "mapgui/mapquery.h"
#include "options/optiondata.h"

#include <QPen>
#include <QString>
#include <QApplication>
#include <QPalette>

namespace mapcolors {

const QColor& colorForAirport(const maptypes::MapAirport& ap)
{
  static const QColor airportEmptyColor = QColor::fromRgb(110, 110, 110);
  static const QColor toweredAirportColor = QColor::fromRgb(15, 70, 130);
  static const QColor unToweredAirportColor = QColor::fromRgb(126, 58, 91);

  if(ap.empty() && !ap.waterOnly() && OptionData::instance().getFlags() & opts::MAP_EMPTY_AIRPORTS)
    return airportEmptyColor;
  else if(ap.tower())
    return toweredAirportColor;
  else
    return unToweredAirportColor;
}

const QColor& alternatingRowColor(int row, bool isSort)
{
  /* Alternating colors */
  static const QColor rowBgColor = QApplication::palette().color(QPalette::Active, QPalette::Base);
  static const QColor rowAltBgColor = QApplication::palette().color(QPalette::Active, QPalette::AlternateBase);

  /* Slightly darker background for sort column */
  static const QColor rowSortBgColor = rowBgColor.darker(106);
  static const QColor rowSortAltBgColor = rowAltBgColor.darker(106);

  if((row % 2) == 0)
    return isSort ? rowSortBgColor : rowBgColor;
  else
    return isSort ? rowSortAltBgColor : rowAltBgColor;
}

const QColor& colorForParkingType(const QString& type)
{
  static const QColor rampMilCargo(180, 0, 0);
  static const QColor rampMilCombat(Qt::red);
  static const QColor gate(100, 100, 255);
  static const QColor rampGa(0, 200, 0);
  static const QColor rampCargo(Qt::darkGreen);
  static const QColor fuel(Qt::yellow);
  static const QColor unknown;

  if(type == "RMCB")
    return rampMilCombat;
  else if(type == "RMC")
    return rampMilCargo;
  else if(type.startsWith("G"))
    return gate;
  else if(type.startsWith("RGA") || type.startsWith("DGA"))
    return rampGa;
  else if(type.startsWith("RC"))
    return rampCargo;
  else if(type.startsWith("FUEL"))
    return fuel;
  else
    return unknown;
}

const QIcon& iconForStartType(const QString& type)
{
  static const QIcon runway(":/littlenavmap/resources/icons/startrunway.svg");
  static const QIcon helipad(":/littlenavmap/resources/icons/starthelipad.svg");
  static const QIcon water(":/littlenavmap/resources/icons/startwater.svg");

  static const QIcon empty;
  if(type == "R")
    return runway;
  else if(type == "R")
    return helipad;
  else if(type == "R")
    return water;
  else
    return empty;
}

const QIcon& iconForParkingType(const QString& type)
{
  static const QIcon cargo(":/littlenavmap/resources/icons/parkingrampcargo.svg");
  static const QIcon ga(":/littlenavmap/resources/icons/parkingrampga.svg");
  static const QIcon mil(":/littlenavmap/resources/icons/parkingrampmil.svg");
  static const QIcon gate(":/littlenavmap/resources/icons/parkinggate.svg");
  static const QIcon fuel(":/littlenavmap/resources/icons/parkingfuel.svg");
  static const QIcon empty;

  if(type.startsWith("RM"))
    return mil;
  else if(type.startsWith("G"))
    return gate;
  else if(type.startsWith("RGA") || type.startsWith("DGA"))
    return ga;
  else if(type.startsWith("RC"))
    return cargo;
  else if(type.startsWith("FUEL"))
    return fuel;

  return empty;
}

const QColor& colorForSurface(const QString& surface)
{
  static const QColor concrete("#888888");
  static const QColor grass("#00a000");
  static const QColor water("#808585ff");
  static const QColor asphalt("#707070");
  static const QColor cement("#d0d0d0");
  static const QColor clay("#DEB887");
  static const QColor snow("#dbdbdb");
  static const QColor ice("#d0d0ff");
  static const QColor dirt("#CD853F");
  static const QColor coral("#FFE4C4");
  static const QColor gravel("#c0c0c0");
  static const QColor oilTreated("#2F4F4F");
  static const QColor steelMats("#a0f0ff");
  static const QColor bituminous("#808080");
  static const QColor brick("#A0522D");
  static const QColor macadam("#c8c8c8");
  static const QColor planks("#8B4513");
  static const QColor sand("#F4A460");
  static const QColor shale("#F5DEB3");
  static const QColor tarmac("#909090");
  static const QColor unknown("#ffffff");

  if(surface == "C")
    return concrete;
  else if(surface == "G")
    return grass;
  else if(surface == "W")
    return water;
  else if(surface == "A")
    return asphalt;
  else if(surface == "CE")
    return cement;
  else if(surface == "CL")
    return clay;
  else if(surface == "SN")
    return snow;
  else if(surface == "I")
    return ice;
  else if(surface == "D")
    return dirt;
  else if(surface == "CR")
    return coral;
  else if(surface == "GR")
    return gravel;
  else if(surface == "OT")
    return oilTreated;
  else if(surface == "SM")
    return steelMats;
  else if(surface == "B")
    return bituminous;
  else if(surface == "BR")
    return brick;
  else if(surface == "M")
    return macadam;
  else if(surface == "PL")
    return planks;
  else if(surface == "S")
    return sand;
  else if(surface == "SH")
    return shale;
  else if(surface == "T")
    return tarmac;

  // else if(surface == "NONE" || surface == "UNKNOWN" || surface == "INVALID")
  return unknown;
}

const QPen aircraftTrailPen(float size)
{
  opts::DisplayTrailType type = OptionData::instance().getDisplayTrailType();

  switch(type)
  {
    case opts::DASHED:
      return QPen(OptionData::instance().getTrailColor(), size, Qt::DashLine, Qt::FlatCap, Qt::BevelJoin);

    case opts::DOTTED:
      return QPen(OptionData::instance().getTrailColor(), size, Qt::DotLine, Qt::FlatCap, Qt::BevelJoin);

    case opts::SOLID:
      return QPen(OptionData::instance().getTrailColor(), size, Qt::SolidLine, Qt::FlatCap, Qt::BevelJoin);
  }
  return QPen();
}

} // namespace mapcolors
