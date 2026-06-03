/*****************************************************************************
* Copyright 2015-2026 Alexander Barthel alex@littlenavmap.org
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
    {QStringLiteral("Addon"), tr("Addon", "UserpointType")},
    {QStringLiteral("Airport"), tr("Airport", "UserpointType")},
    {QStringLiteral("Airstrip"), tr("Airstrip", "UserpointType")},
    {QStringLiteral("Bookmark"), tr("Bookmark", "UserpointType")},
    {QStringLiteral("Building"), tr("Building", "UserpointType")},
    {QStringLiteral("Cabin"), tr("Cabin", "UserpointType")},
    {QStringLiteral("Closed"), tr("Closed", "UserpointType")},
    {QStringLiteral("DME"), tr("DME", "UserpointType")},
    {QStringLiteral("Error"), tr("Error", "UserpointType")},
    {QStringLiteral("Flag"), tr("Flag", "UserpointType")},
    {QStringLiteral("Helipad"), tr("Helipad", "UserpointType")},
    {QStringLiteral("History"), tr("History", "UserpointType")},
    {QStringLiteral("Landform"), tr("Landform", "UserpointType")},
    {QStringLiteral("Lighthouse"), tr("Lighthouse", "UserpointType")},
    {QStringLiteral("Location"), tr("Location", "UserpointType")},
    {QStringLiteral("Logbook"), tr("Logbook", "UserpointType")},
    {QStringLiteral("Marker"), tr("Marker", "UserpointType")},
    {QStringLiteral("Mountain"), tr("Mountain", "UserpointType")},
    {QStringLiteral("NDB"), tr("NDB", "UserpointType")},
    {QStringLiteral("Obstacle"), tr("Obstacle", "UserpointType")},
    {QStringLiteral("Oil Platform"), tr("Oil Platform", "UserpointType")},
    {QStringLiteral("Other"), tr("Other", "UserpointType")},
    {QStringLiteral("Park"), tr("Park", "UserpointType")},
    {QStringLiteral("Pin"), tr("Pin", "UserpointType")},
    {QStringLiteral("POI"), tr("POI", "UserpointType")},
    {QStringLiteral("Radio Range"), tr("Radio Range", "UserpointType")},
    {QStringLiteral("Seaport"), tr("Seaport", "UserpointType")},
    {QStringLiteral("Settlement"), tr("Settlement", "UserpointType")},
    {QStringLiteral("TACAN"), tr("TACAN", "UserpointType")},
    {QStringLiteral("Unknown"), tr("Unknown", "UserpointType")},
    {QStringLiteral("VOR"), tr("VOR", "UserpointType")},
    {QStringLiteral("VORDME"), tr("VORDME", "UserpointType")},
    {QStringLiteral("VORTAC"), tr("VORTAC", "UserpointType")},
    {QStringLiteral("VRP"), tr("VRP", "UserpointType")},
    {QStringLiteral("Water"), tr("Water", "UserpointType")},
    {QStringLiteral("Waypoint"), tr("Waypoint", "UserpointType")}
  });

  for(auto it = typeToTranslatedMap.constBegin(); it != typeToTranslatedMap.constEnd(); ++it)
    translationToTypeMap.insert(it.value(), it.key());
}

QString UserdataIcons::typeToTranslated(const QString& type)
{
  return typeToTranslatedMap.value(type, type);
}

QString UserdataIcons::translatedToType(const QString& type)
{
  return translationToTypeMap.value(type, type);
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
  for(const QFileInfo& entry : configDir.entryInfoList({"userpoint_*.svg", "userpoint_*.png", "userpoint_*.jpg", "userpoint_*.gif"}))
    loadIcon(entry);

  // Get default icons from resources if not already loaded before
  QDir resourceDir(":/littlenavmap/resources/icons");
  for(const QFileInfo& entry : resourceDir.entryInfoList({"userpoint_*.svg"}))
    loadIcon(entry);
}

const QList<QIcon> UserdataIcons::getAllIcons(float size)
{
  QList<QIcon> icons;
  for(auto it = getAllTypesMap().constBegin(); it != getAllTypesMap().constEnd(); ++it)
    icons.append(QIcon(*getIconPixmap(it.key(), size)));
  return icons;
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
