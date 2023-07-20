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

#include "userdata/userdataicons.h"

#include "settings/settings.h"
#include "atools.h"

#include <QDir>
#include <QDebug>
#include <QRegularExpression>
#include <QPixmap>
#include <QIcon>

const static QLatin1String DEFAULT_TYPE("Unknown");

static QHash<QString, QString> typeToTranslatedMap;
static QHash<QString, QString> translationToTypeMap;

void UserdataIcons::initTranslateableTexts()
{
  typeToTranslatedMap = QHash<QString, QString>(
  {
    {QLatin1String("Airport"), tr("Airport", "UserpointType")},
    {QLatin1String("Addon"), tr("Addon", "UserpointType")}, // Keep in syc with map::MapUserPoint.isAddon()
    {QLatin1String("Airstrip"), tr("Airstrip", "UserpointType")},
    {QLatin1String("Bookmark"), tr("Bookmark", "UserpointType")},
    {QLatin1String("Building"), tr("Building", "UserpointType")},
    {QLatin1String("Cabin"), tr("Cabin", "UserpointType")},
    {QLatin1String("Closed"), tr("Closed", "UserpointType")},
    {QLatin1String("DME"), tr("DME", "UserpointType")},
    {QLatin1String("Error"), tr("Error", "UserpointType")},
    {QLatin1String("Flag"), tr("Flag", "UserpointType")},
    {QLatin1String("Helipad"), tr("Helipad", "UserpointType")},
    {QLatin1String("History"), tr("History", "UserpointType")},
    {QLatin1String("Landform"), tr("Landform", "UserpointType")},
    {QLatin1String("Lighthouse"), tr("Lighthouse", "UserpointType")},
    {QLatin1String("Location"), tr("Location", "UserpointType")},
    {QLatin1String("Logbook"), tr("Logbook", "UserpointType")},
    {QLatin1String("Marker"), tr("Marker", "UserpointType")},
    {QLatin1String("Mountain"), tr("Mountain", "UserpointType")},
    {QLatin1String("NDB"), tr("NDB", "UserpointType")},
    {QLatin1String("Obstacle"), tr("Obstacle", "UserpointType")},
    {QLatin1String("Oil Platform"), tr("Oil Platform", "UserpointType")},
    {QLatin1String("Other"), tr("Other", "UserpointType")},
    {QLatin1String("Park"), tr("Park", "UserpointType")},
    {QLatin1String("Pin"), tr("Pin", "UserpointType")},
    {QLatin1String("POI"), tr("POI", "UserpointType")},
    {QLatin1String("Radio Range"), tr("Radio Range", "UserpointType")},
    {QLatin1String("Seaport"), tr("Seaport", "UserpointType")},
    {QLatin1String("Settlement"), tr("Settlement", "UserpointType")},
    {QLatin1String("TACAN"), tr("TACAN", "UserpointType")},
    {QLatin1String("Unknown"), tr("Unknown", "UserpointType")},
    {QLatin1String("VOR"), tr("VOR", "UserpointType")},
    {QLatin1String("VORDME"), tr("VORDME", "UserpointType")},
    {QLatin1String("VORTAC"), tr("VORTAC", "UserpointType")},
    {QLatin1String("VRP"), tr("VRP", "UserpointType")},
    {QLatin1String("Water"), tr("Water", "UserpointType")},
    {QLatin1String("Waypoint"), tr("Waypoint", "UserpointType")}
  });

  translationToTypeMap = QHash<QString, QString>(
  {
    {tr("Airport", "UserpointType"), QLatin1String("Airport")},
    {tr("Addon", "UserpointType"), QLatin1String("Addon")},
    {tr("Airstrip", "UserpointType"), QLatin1String("Airstrip")},
    {tr("Bookmark", "UserpointType"), QLatin1String("Bookmark")},
    {tr("Building", "UserpointType"), QLatin1String("Building")},
    {tr("Cabin", "UserpointType"), QLatin1String("Cabin")},
    {tr("Closed", "UserpointType"), QLatin1String("Closed")},
    {tr("DME", "UserpointType"), QLatin1String("DME")},
    {tr("Error", "UserpointType"), QLatin1String("Error")},
    {tr("Flag", "UserpointType"), QLatin1String("Flag")},
    {tr("Helipad", "UserpointType"), QLatin1String("Helipad")},
    {tr("History", "UserpointType"), QLatin1String("History")},
    {tr("Landform", "UserpointType"), QLatin1String("Landform")},
    {tr("Lighthouse", "UserpointType"), QLatin1String("Lighthouse")},
    {tr("Location", "UserpointType"), QLatin1String("Location")},
    {tr("Logbook", "UserpointType"), QLatin1String("Logbook")},
    {tr("Marker", "UserpointType"), QLatin1String("Marker")},
    {tr("Mountain", "UserpointType"), QLatin1String("Mountain")},
    {tr("NDB", "UserpointType"), QLatin1String("NDB")},
    {tr("Obstacle", "UserpointType"), QLatin1String("Obstacle")},
    {tr("Oil Platform", "UserpointType"), QLatin1String("Oil Platform")},
    {tr("Other", "UserpointType"), QLatin1String("Other")},
    {tr("Park", "UserpointType"), QLatin1String("Park")},
    {tr("Pin", "UserpointType"), QLatin1String("Pin")},
    {tr("POI", "UserpointType"), QLatin1String("POI")},
    {tr("Radio Range", "UserpointType"), QLatin1String("Radio Range")},
    {tr("Seaport", "UserpointType"), QLatin1String("Seaport")},
    {tr("Settlement", "UserpointType"), QLatin1String("Settlement")},
    {tr("TACAN", "UserpointType"), QLatin1String("TACAN")},
    {tr("Unknown", "UserpointType"), QLatin1String("Unknown")},
    {tr("VOR", "UserpointType"), QLatin1String("VOR")},
    {tr("VORDME", "UserpointType"), QLatin1String("VORDME")},
    {tr("VORTAC", "UserpointType"), QLatin1String("VORTAC")},
    {tr("VRP", "UserpointType"), QLatin1String("VRP")},
    {tr("Water", "UserpointType"), QLatin1String("Water")},
    {tr("Waypoint", "UserpointType"), QLatin1String("Waypoint")}
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

UserdataIcons::UserdataIcons()
{

}

UserdataIcons::~UserdataIcons()
{
  pixmapCache.clear();
}

QPixmap *UserdataIcons::getIconPixmap(const QString& type, int size, icon::TextPlacement *textplacement)
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

  if(textplacement != nullptr)
  {
    // Place label at the right per default
    *textplacement = icon::ICON_LABEL_RIGHT;

    // NDB navaids have their label at the bottom - place user NDB on top
    if(type == "NDB")
      *textplacement = icon::ICON_LABEL_TOP;
    else if(atools::contains(type, {"Airport", "Airstrip", "Closed", "Helipad", "Seaport", "Waypoint"}))
      // Airports and waypoints have their label at the right - place user at the left
      *textplacement = icon::ICON_LABEL_LEFT;
    // else if(atools::contains(type, {"VOR", "VORTAC", "VORDME", "TACAN", "DME"}))
    //// All VOR types have labels at the left - place user at the right
    // *textplacement = icon::ICON_LABEL_RIGHT;
  }

  return pixmap;
}

void UserdataIcons::loadIcons()
{
  // First get new and overloaded icons from the configuration directory
  QDir configDir(atools::settings::Settings::getPath());
  for(const QFileInfo& entry : configDir.entryInfoList({"userpoint_*.svg", "userpoint_*.png",
                                                        "userpoint_*.jpg", "userpoint_*.gif"}))
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
  static QRegularExpression typeRegexp("userpoint_(.+)\\.(svg|png|jpg|gif)");
  QRegularExpressionMatch match = typeRegexp.match(entry.fileName());
  if(match.hasMatch())
  {
    QString type = match.captured(1);

    if(!typeMap.contains(type))
    {
      typeMap.insert(type, entry.filePath());
#ifdef DEBUG_INFORMATION
      qDebug() << Q_FUNC_INFO << "Added icon" << entry.filePath() << "for type" << type;
#endif
    }
  }
  else
    qWarning() << Q_FUNC_INFO << "Could not find type in filename" << entry.filePath();
}
