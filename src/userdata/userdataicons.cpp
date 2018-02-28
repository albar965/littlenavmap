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

#include "userdata/userdataicons.h"

#include "settings/settings.h"

#include <QDir>
#include <QDebug>
#include <QRegularExpression>
#include <QPixmap>
#include <QIcon>

const static QLatin1Literal DEFAULT_TYPE("Flag");

UserdataIcons::UserdataIcons(QObject *parent) : QObject(parent)
{

}

UserdataIcons::~UserdataIcons()
{
  pixmapCache.clear();
}

QPixmap *UserdataIcons::getIconPixmap(const QString& type, int size)
{
  PixmapCacheKey key = std::make_pair(type, size);

  QPixmap *pixmap = pixmapCache.object(key);
  if(pixmap == nullptr)
  {
    QString file = typeMap.value(type);
    if(!file.isEmpty())
    {
      pixmap = new QPixmap(QIcon(file).pixmap(QSize(size, size)));
      pixmapCache.insert(key, pixmap);
    }
    else
      // Nothing found in map - use default
      pixmap = getIconPixmap(DEFAULT_TYPE, size);
  }

  return pixmap;
}

void UserdataIcons::loadIcons()
{
  // First get new and overloaded icons from the configuration directory
  QDir configDir(atools::settings::Settings::instance().getPath());
  for(const QFileInfo& entry : configDir.entryInfoList({"userdata_*.svg"}))
    loadIcon(entry);

  // Get default icons from resources if not already loaded before
  QDir resourceDir(":/littlenavmap/resources/icons");
  for(const QFileInfo& entry : resourceDir.entryInfoList({"userdata_*.svg"}))
    loadIcon(entry);
}

void UserdataIcons::loadIcon(const QFileInfo& entry)
{
  static QRegularExpression typeRegexp("userdata_(.+)\\.svg");
  QString name = atools::settings::Settings::instance().getOverloadedPath(entry.filePath());

  QRegularExpressionMatch match = typeRegexp.match(entry.fileName());
  if(match.hasMatch())
  {
    QString type = match.captured(1);

    if(!typeMap.contains(type))
    {
      typeMap.insert(type, entry.filePath());
      qDebug() << Q_FUNC_INFO << "Added icon" << entry.filePath() << "for type" << type;
    }
  }
  else
    qWarning() << Q_FUNC_INFO << "Could not find type in filename" << entry.filePath();
}
