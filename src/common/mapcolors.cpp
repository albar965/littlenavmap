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

#include "mapcolors.h"

#include <QPen>
#include <QString>
#include <QApplication>
#include <QPalette>

#include <mapgui/mapquery.h>

namespace mapcolors {

const QPen textBackgroundPen = QPen(QBrush(QColor(Qt::lightGray)), 1, Qt::SolidLine, Qt::FlatCap);
const QPen textPen = QPen(QBrush(QColor::fromRgb(0, 0, 0)), 1, Qt::SolidLine, Qt::FlatCap);

const QColor airportDetailBackColor = QColor(Qt::white);

const QColor taxiwayNameColor = QColor(Qt::black);
const QColor runwayOutlineColor = QColor(Qt::black);
const QColor runwayOffsetColor = QColor(Qt::white);
const QColor parkingOutlineColor = QColor::fromRgb(80, 80, 80);
const QColor helipadOutlineColor = QColor(Qt::black);
const QColor activeTowerColor = QColor(Qt::red);
const QColor activeTowerOutlineColor = QColor(Qt::black);
const QColor inactiveTowerColor = QColor(Qt::lightGray);
const QColor inactiveTowerOutlineColor = QColor(Qt::darkGray);
const QColor darkParkingTextColor = QColor(Qt::black);
const QColor brightParkingTextColor = QColor(Qt::white);
const QColor towerTextColor = QColor(Qt::black);
const QColor runwayDimsTextColor = QColor(Qt::black);

const QColor textBoxColor = QColor(Qt::white);

const QColor airportSymbolFillColor = QColor(Qt::white);

const QPen markBackPen = QPen(QBrush(QColor::fromRgb(0, 0, 0)), 6, Qt::SolidLine, Qt::FlatCap);
const QPen markFillPen = QPen(QBrush(QColor::fromRgb(255, 255, 0)), 2, Qt::SolidLine, Qt::FlatCap);

const QPen homeBackPen = QPen(QBrush(QColor::fromRgb(0, 0, 0)), 2, Qt::SolidLine, Qt::FlatCap);
const QColor homeFillColor = QColor(Qt::yellow);

const QColor highlightBackColor = QColor(Qt::black);
const QColor highlightColor = QColor(Qt::yellow);
const QColor highlightColorFast = QColor(Qt::darkYellow);

const QColor vorSymbolColor = QColor(Qt::darkBlue);
const QColor ndbSymbolColor = QColor(Qt::darkRed);
const QColor markerSymbolColor = QColor(Qt::darkMagenta);
const QColor ilsSymbolColor = QColor(Qt::darkGreen);
const QColor waypointSymbolColor = QColor(Qt::magenta);

const QColor& colorForAirport(const MapAirport& ap)
{
  static QColor airportEmptyColor = QColor::fromRgb(150, 150, 150);
  static QColor toweredAirportColor = QColor::fromRgb(15, 70, 130);
  static QColor unToweredAirportColor = QColor::fromRgb(126, 58, 91);

  if(!ap.flags.testFlag(SCENERY) && !ap.waterOnly())
    return airportEmptyColor;
  else if(ap.flags.testFlag(TOWER))
    return toweredAirportColor;
  else
    return unToweredAirportColor;
}

const QColor& alternatingRowColor(int row, bool isSort, QStyle::State state)
{
  /* Alternating colors */
  static QColor rowBgColorHighlight = QApplication::palette().color(QPalette::Active, QPalette::Highlight);
  static QColor rowBgColor = QApplication::palette().color(QPalette::Active, QPalette::Base);
  static QColor rowAltBgColor = QApplication::palette().color(QPalette::Active, QPalette::AlternateBase);

  /* Slightly darker background for sort column */
  static QColor rowSortBgColor = rowBgColor.darker(106);
  static QColor rowSortAltBgColor = rowAltBgColor.darker(106);

  if(state.testFlag(QStyle::State_Selected) && state.testFlag(QStyle::State_Active))
    return rowBgColorHighlight;
  else
  {
    if((row % 2) == 0)
      return isSort ? rowSortBgColor : rowBgColor;
    else
      return isSort ? rowSortAltBgColor : rowAltBgColor;
  }
}

const QColor& colorForParkingType(const QString& type)
{
  static QColor rampMil(Qt::red);
  static QColor gate(100, 100, 255);
  static QColor rampGa(Qt::green);
  static QColor rampCargo(Qt::darkGreen);
  static QColor fuel(Qt::yellow);
  static QColor unknown;

  if(type.startsWith("RAMP_MIL"))
    return rampMil;
  else if(type.startsWith("GATE"))
    return gate;
  else if(type.startsWith("RAMP_GA") || type.startsWith("DOCK_GA"))
    return rampGa;
  else if(type.startsWith("RAMP_CARGO"))
    return rampCargo;
  else if(type.startsWith("FUEL"))
    return fuel;
  else
    return unknown;
}

const QColor& colorForSurface(const QString& surface)
{
  static QColor concrete(Qt::gray);
  static QColor grass("#00a000");
  static QColor water(133, 133, 255);
  static QColor asphalt(Qt::darkGray);
  static QColor cement(Qt::lightGray);
  static QColor clay("#DEB887");
  static QColor snow("#f0f0f0");
  static QColor ice("#f0f0ff");
  static QColor dirt("#CD853F");
  static QColor coral("#FFE4C4");
  static QColor gravel(Qt::lightGray);
  static QColor oilTreated("#2F4F4F");
  static QColor steelMats(Qt::lightGray);
  static QColor bituminous(Qt::darkGray);
  static QColor brick("#A0522D");
  static QColor macadam(Qt::lightGray);
  static QColor planks("#8B4513");
  static QColor sand("#F4A460");
  static QColor shale("#F5DEB3");
  static QColor tarmac(Qt::gray);
  static QColor unknown(Qt::black);

  if(surface == "CONCRETE")
    return concrete;
  else if(surface == "GRASS")
    return grass;
  else if(surface == "WATER")
    return water;
  else if(surface == "ASPHALT")
    return asphalt;
  else if(surface == "CEMENT")
    return cement;
  else if(surface == "CLAY")
    return clay;
  else if(surface == "SNOW")
    return snow;
  else if(surface == "ICE")
    return ice;
  else if(surface == "DIRT")
    return dirt;
  else if(surface == "CORAL")
    return coral;
  else if(surface == "GRAVEL")
    return gravel;
  else if(surface == "OIL_TREATED")
    return oilTreated;
  else if(surface == "STEEL_MATS")
    return steelMats;
  else if(surface == "BITUMINOUS")
    return bituminous;
  else if(surface == "BRICK")
    return brick;
  else if(surface == "MACADAM")
    return macadam;
  else if(surface == "PLANKS")
    return planks;
  else if(surface == "SAND")
    return sand;
  else if(surface == "SHALE")
    return shale;
  else if(surface == "TARMAC")
    return tarmac;

  // else if(rw.surface == "UNKNOWN")
  return unknown;
}

} // namespace mapcolors
