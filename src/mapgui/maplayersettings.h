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

#ifndef LITTLENAVMAP_MAPLAYERSETTINGS_H
#define LITTLENAVMAP_MAPLAYERSETTINGS_H

#include "mapgui/maplayer.h"

#include <QList>
#include <QCoreApplication>

namespace atools {
namespace  util {
class XmlStream;
class FileSystemWatcher;
}
}

/*
 * A list of map layers that defines what is painted at what zoom distance.
 * The configuration is loaded from a XML file.
 */
class MapLayerSettings
  : public QObject
{
  Q_OBJECT

public:
  MapLayerSettings(bool verbose);
  virtual ~MapLayerSettings() override;

  /* Add a map layer. Call finishAppend when done. */
  MapLayerSettings& append(const MapLayer& layer);

  /* Take the last added layer and create a clone with a new zoom distance */
  MapLayer cloneLast(float maximumRangeKm) const;

  /* Call when done appending layers. Sorts all layers by zoom distance. */
  void finishAppend();

  /* Get a layer for current zoom distance and detail factor */
  const MapLayer *getLayer(float distanceKm, int detailLevel = MAP_DEFAULT_DETAIL_LEVEL) const;

  /* Load from mapsettings.xml in resources or overloaded file in settings folder. */
  void loadFromFile();

  /* Connect a widget which is updated on file change */
  void connectMapSettingsUpdated(QWidget *mapWidget);

  static Q_DECL_CONSTEXPR int MAP_DEFAULT_DETAIL_LEVEL = 10;
  static Q_DECL_CONSTEXPR int MAP_MAX_DETAIL_LEVEL = 15;
  static Q_DECL_CONSTEXPR int MAP_MIN_DETAIL_LEVEL = 8;

signals:
  /* Sent if overloading configuration file changes */
  void mapSettingsChanged();

private:
  friend QDebug operator<<(QDebug out, const MapLayerSettings& record);

  void loadXmlInternal(atools::util::XmlStream& xmlStream);
  bool compare(const MapLayer& layer, float distance) const;
  void reloadFromFile();
  void reloadFromUpdate(const QStringList&);

  QList<MapLayer> layers;
  atools::util::FileSystemWatcher *fileWatcher = nullptr;
  QString filename;
};

#endif // LITTLENAVMAP_MAPLAYERSETTINGS_H
