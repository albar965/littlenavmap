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

const static QLatin1Literal DEFAULT_TYPE("Unknown");

static QHash<QString, QString> typeToTranslatedMap;
static QHash<QString, QString> translationToTypeMap;

void UserdataIcons::initTranslateableTexts()
{
  typeToTranslatedMap = QHash<QString, QString>(
  {
    {QLatin1Literal("Airport"), QObject::tr("Airport", "UserpointType")},
    {QLatin1Literal("Airstrip"), QObject::tr("Airstrip", "UserpointType")},
    {QLatin1Literal("Bookmark"), QObject::tr("Bookmark", "UserpointType")},
    {QLatin1Literal("Cabin"), QObject::tr("Cabin", "UserpointType")},
    {QLatin1Literal("Closed"), QObject::tr("Closed", "UserpointType")},
    {QLatin1Literal("Error"), QObject::tr("Error", "UserpointType")},
    {QLatin1Literal("Flag"), QObject::tr("Flag", "UserpointType")},
    {QLatin1Literal("Helipad"), QObject::tr("Helipad", "UserpointType")},
    {QLatin1Literal("Location"), QObject::tr("Location", "UserpointType")},
    {QLatin1Literal("Logbook"), QObject::tr("Logbook", "UserpointType")},
    {QLatin1Literal("Marker"), QObject::tr("Marker", "UserpointType")},
    {QLatin1Literal("Mountain"), QObject::tr("Mountain", "UserpointType")},
    {QLatin1Literal("Obstacle"), QObject::tr("Obstacle", "UserpointType")},
    {QLatin1Literal("Pin"), QObject::tr("Pin", "UserpointType")},
    {QLatin1Literal("POI"), QObject::tr("POI", "UserpointType")},
    {QLatin1Literal("Seaport"), QObject::tr("Seaport", "UserpointType")},
    {QLatin1Literal("Unknown"), QObject::tr("Unknown", "UserpointType")},
    {QLatin1Literal("VRP"), QObject::tr("VRP", "UserpointType")},
    {QLatin1Literal("Waypoint"), QObject::tr("Waypoint", "UserpointType")}
  });

  translationToTypeMap = QHash<QString, QString>(
  {
    {QObject::tr("Airport", "UserpointType"), QLatin1Literal("Airport")},
    {QObject::tr("Airstrip", "UserpointType"), QLatin1Literal("Airstrip")},
    {QObject::tr("Bookmark", "UserpointType"), QLatin1Literal("Bookmark")},
    {QObject::tr("Cabin", "UserpointType"), QLatin1Literal("Cabin")},
    {QObject::tr("Closed", "UserpointType"), QLatin1Literal("Closed")},
    {QObject::tr("Error", "UserpointType"), QLatin1Literal("Error")},
    {QObject::tr("Flag", "UserpointType"), QLatin1Literal("Flag")},
    {QObject::tr("Helipad", "UserpointType"), QLatin1Literal("Helipad")},
    {QObject::tr("Location", "UserpointType"), QLatin1Literal("Location")},
    {QObject::tr("Logbook", "UserpointType"), QLatin1Literal("Logbook")},
    {QObject::tr("Marker", "UserpointType"), QLatin1Literal("Marker")},
    {QObject::tr("Mountain", "UserpointType"), QLatin1Literal("Mountain")},
    {QObject::tr("Obstacle", "UserpointType"), QLatin1Literal("Obstacle")},
    {QObject::tr("Pin", "UserpointType"), QLatin1Literal("Pin")},
    {QObject::tr("POI", "UserpointType"), QLatin1Literal("POI")},
    {QObject::tr("Seaport", "UserpointType"), QLatin1Literal("Seaport")},
    {QObject::tr("Unknown", "UserpointType"), QLatin1Literal("Unknown")},
    {QObject::tr("VRP", "UserpointType"), QLatin1Literal("VRP")},
    {QObject::tr("Waypoint", "UserpointType"), QLatin1Literal("Waypoint")}
  });
}

QString UserdataIcons::typeToTranslated(const QString& type)
{
  return typeToTranslatedMap.value(type, type);
}

QString UserdataIcons::translatedToType(const QString& type)
{
  return translationToTypeMap.value(type, type);
}

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
  for(const QFileInfo& entry : configDir.entryInfoList({"userpoint_*.svg"}))
    loadIcon(entry);

  // Get default icons from resources if not already loaded before
  QDir resourceDir(":/littlenavmap/resources/icons");
  for(const QFileInfo& entry : resourceDir.entryInfoList({"userpoint_*.svg"}))
    loadIcon(entry);
}

QString UserdataIcons::getIconPath(const QString& type) const
{
  return typeMap.value(type, ":/littlenavmap/resources/icons/userpoint_" + DEFAULT_TYPE + ".svg");
}

QString UserdataIcons::getDefaultType(const QString& type)
{
  return typeMap.key(type, DEFAULT_TYPE);
}

void UserdataIcons::loadIcon(const QFileInfo& entry)
{
  static QRegularExpression typeRegexp("userpoint_(.+)\\.svg");
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
