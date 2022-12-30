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

#include "common/vehicleicons.h"

#include "atools.h"
#include "fs/sc/simconnectaircraft.h"
#include "settings/settings.h"

#include <QIcon>
#include <QPainter>

namespace internal {

enum AircraftType
{
  AC_SMALL,
  AC_JET,
  AC_HELICOPTER,
  AC_SHIP,
  AC_CARRIER,
  AC_FRIGATE
};

/* Key built from all attributes */
struct PixmapKey
{
  bool operator==(const PixmapKey& other) const
  {
    return type == other.type && ground == other.ground && user == other.user && size == other.size && rotate == other.rotate &&
           online == other.online;
  }

  AircraftType type;
  bool ground, user, online;
  int size, rotate;
};

uint qHash(const PixmapKey& key)
{
  return static_cast<uint>(key.size ^ (key.type << 8) ^ (key.ground << 12) ^ (key.user << 13) ^ (key.rotate << 14) ^ (key.online << 15));
}

}

VehicleIcons::VehicleIcons()
{

}

VehicleIcons::~VehicleIcons()
{

}

QIcon VehicleIcons::iconFromCache(const atools::fs::sc::SimConnectAircraft& ac, int size, int rotate)
{
  return QIcon(*pixmapFromCache(ac, size, rotate));
}

const QPixmap *VehicleIcons::pixmapFromCache(const internal::PixmapKey& key, int rotate)
{
  if(aircraftPixmaps.contains(key))
    return aircraftPixmaps.object(key);
  else
  {
    int size = key.size;
    QString name = ":/littlenavmap/resources/icons/aircraft";
    switch(key.type)
    {
      case internal::AC_SMALL:
        name += "_small";
        break;
      case internal::AC_JET:
        name += "_jet";
        break;
      case internal::AC_HELICOPTER:
        name += "_helicopter";
        // Make helicopter a bit bigger due to image
        size = atools::roundToInt(size * 1.2f);
        break;
      case internal::AC_SHIP:
        name += "_boat";
        break;
      case internal::AC_CARRIER:
        name += "_carrier";
        break;
      case internal::AC_FRIGATE:
        name += "_frigate";
        break;
    }

    if(key.ground)
      name += "_ground";

    // User aircraft always yellow
    if(!key.online && key.user)
      // No user key for online
      name += "_user";

    // Dark online icon not for user aircraft
    if(key.online && !key.user)
      name += "_online";

    name = atools::settings::Settings::getOverloadedPath(name + ".svg", true /* ignoreMissing */);

    // Check if SVG exists
    if(name.isEmpty())
      name = ":/littlenavmap/resources/icons/aircraft_unknown.svg";

    QPixmap *newPx = nullptr;
    QPixmap pixmap = QIcon(name).pixmap(QSize(size, size));
    if(rotate == 0)
      newPx = new QPixmap(pixmap);
    else
    {
      QPixmap painterPixmap(size, size);
      painterPixmap.fill(QColor(Qt::transparent));
      QPainter painter(&painterPixmap);

      painter.setRenderHint(QPainter::Antialiasing, true);
      painter.setRenderHint(QPainter::TextAntialiasing, true);
      painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

      painter.translate(size / 2.f, size / 2.f);
      painter.rotate(rotate);
      painter.drawPixmap(QPointF(-size / 2.f, -size / 2.f), pixmap,
                         QRectF(0, 0, size * pixmap.devicePixelRatio(), size * pixmap.devicePixelRatio()));
      painter.resetTransform();
      newPx = new QPixmap(painterPixmap);
    }
    aircraftPixmaps.insert(key, newPx);
    return newPx;
  }
}

const QPixmap *VehicleIcons::pixmapFromCache(const atools::fs::sc::SimConnectAircraft& ac, int size, int rotate)
{
  internal::PixmapKey key;

  // Also use online icon for simulator shadows but not for the user aircraft
  if(ac.getCategory() == atools::fs::sc::HELICOPTER)
    key.type = internal::AC_HELICOPTER;
  else if(ac.isAnyBoat())
  {
    if(ac.getCategory() == atools::fs::sc::CARRIER)
      key.type = internal::AC_CARRIER;
    else if(ac.getCategory() == atools::fs::sc::FRIGATE)
      key.type = internal::AC_FRIGATE;
    else
      key.type = internal::AC_SHIP;
  }
  else if(ac.getEngineType() == atools::fs::sc::JET)
    key.type = internal::AC_JET;
  else
    key.type = internal::AC_SMALL;

  // Dark icon for online aircraft or simulator shadow aircraft but not user
  key.online = (ac.isOnline() || ac.isOnlineShadow()) && !ac.isUser();
  key.ground = ac.isOnGround();
  key.user = ac.isUser();
  key.size = size;
  key.rotate = rotate;
  return pixmapFromCache(key, rotate);
}
