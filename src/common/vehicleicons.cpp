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

#include "common/vehicleicons.h"

#include "atools.h"
#include "fs/sc/simconnectaircraft.h"
#include "settings/settings.h"

#include <QIcon>
#include <QPainter>

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

uint qHash(const VehicleIcons::PixmapKey& key)
{
  return static_cast<uint>(key.size | (key.type << 8) | (key.ground << 10) | (key.user << 11) | (key.rotate << 11));
}

bool VehicleIcons::PixmapKey::operator==(const VehicleIcons::PixmapKey& other) const
{
  return type == other.type && ground == other.ground && user == other.user && size == other.size &&
         rotate == other.rotate;
}

const QPixmap *VehicleIcons::pixmapFromCache(const PixmapKey& key, int rotate)
{
  if(aircraftPixmaps.contains(key))
    return aircraftPixmaps.object(key);
  else
  {
    int size = key.size;
    QString name = ":/littlenavmap/resources/icons/aircraft";
    switch(key.type)
    {
      case AC_ONLINE:
        name += "_online";
        break;
      case AC_SMALL:
        name += "_small";
        break;
      case AC_JET:
        name += "_jet";
        break;
      case AC_HELICOPTER:
        name += "_helicopter";
        // Make helicopter a bit bigger due to image
        size = atools::roundToInt(size * 1.2f);
        break;
      case AC_SHIP:
        name += "_boat";
        break;
    }

    if(key.ground)
      name += "_ground";

    if(key.type != AC_ONLINE && key.user)
      // No user key for online
      name += "_user";

    name = atools::settings::Settings::instance().getOverloadedPath(name + ".svg");
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
      painter.drawPixmap(QPointF(-size / 2.f, -size / 2.f), pixmap, QRectF(0, 0, size, size));
      painter.resetTransform();
      newPx = new QPixmap(painterPixmap);
    }
    aircraftPixmaps.insert(key, newPx);
    return newPx;
  }
}

const QPixmap *VehicleIcons::pixmapFromCache(const atools::fs::sc::SimConnectAircraft& ac, int size, int rotate)
{
  PixmapKey key;

  if(ac.isOnline())
    key.type = AC_ONLINE;
  else if(ac.getCategory() == atools::fs::sc::HELICOPTER)
    key.type = AC_HELICOPTER;
  else if(ac.getCategory() == atools::fs::sc::BOAT)
    key.type = AC_SHIP;
  else if(ac.getEngineType() == atools::fs::sc::JET)
    key.type = AC_JET;
  else
    key.type = AC_SMALL;

  key.ground = ac.isOnGround();
  key.user = ac.isUser();
  key.size = size;
  key.rotate = rotate;
  return pixmapFromCache(key, rotate);
}
