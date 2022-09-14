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

#include "mapgui/maplayersettings.h"

#include "common/constants.h"
#include "exception.h"
#include "settings/settings.h"
#include "util/filesystemwatcher.h"
#include "util/xmlstream.h"

#include <algorithm>
#include <functional>

#include <QFileInfo>
#include <QWidget>
#include <QXmlStreamReader>

MapLayerSettings::MapLayerSettings(bool verbose)
{
  fileWatcher = new atools::util::FileSystemWatcher(this, verbose);
  fileWatcher->setMinFileSize(100);
  connect(fileWatcher, &atools::util::FileSystemWatcher::filesUpdated, this, &MapLayerSettings::reloadFromUpdate);
}

MapLayerSettings::~MapLayerSettings()
{
  delete fileWatcher;
}

void MapLayerSettings::connectMapSettingsUpdated(QWidget *mapWidget)
{
  // Update widget on file change
  connect(this, &MapLayerSettings::mapSettingsChanged, mapWidget, QOverload<>::of(&QWidget::update));
}

MapLayerSettings& MapLayerSettings::append(const MapLayer& layer)
{
  layers.append(layer);
  return *this;
}

MapLayer MapLayerSettings::cloneLast(float maximumRangeKm) const
{
  if(layers.isEmpty())
    return MapLayer(maximumRangeKm);
  else
    return layers.constLast().clone(maximumRangeKm);
}

void MapLayerSettings::finishAppend()
{
  std::sort(layers.begin(), layers.end());
}

const MapLayer *MapLayerSettings::getLayer(float distanceKm, int detailLevel) const
{
  using namespace std::placeholders;

  // Get the layer with the next lowest zoom distance
  auto it = std::lower_bound(layers.constBegin(), layers.constEnd(), distanceKm,
                             std::bind(&MapLayerSettings::compare, this, _1, _2));

  // Adjust iterator for detail level changes
  if(detailLevel > MAP_DEFAULT_DETAIL_LEVEL)
    it -= detailLevel - MAP_DEFAULT_DETAIL_LEVEL;
  else if(detailLevel < MAP_DEFAULT_DETAIL_LEVEL)
    it += MAP_DEFAULT_DETAIL_LEVEL - detailLevel;

  if(it >= layers.end())
    return &layers.constLast();

  if(it < layers.begin())
    return &(*(layers.begin()));

  return &(*it);
}

void MapLayerSettings::reloadFromUpdate(const QStringList&)
{
  // Reload from file update
  reloadFromFile();

  // Update map widget
  emit mapSettingsChanged();
}

void MapLayerSettings::reloadFromFile()
{
  qDebug() << Q_FUNC_INFO << filename;

  QFile xmlFile(filename);

  if(xmlFile.open(QIODevice::ReadOnly))
  {
    atools::util::XmlStream xmlStream(&xmlFile, filename);
    loadXmlInternal(xmlStream);
    xmlFile.close();
  }
  else
    throw atools::Exception(tr("Cannot open file \"%1\". Reason: %2").arg(filename).arg(xmlFile.errorString()));
}

void MapLayerSettings::loadFromFile()
{
  filename = atools::settings::Settings::getOverloadedPath(lnm::MAP_LAYER_CONFIG);

  QFileInfo fi(filename);
  if(!fi.exists() || !fi.isReadable())
    throw atools::Exception(tr("Cannot open layer settings file \"%1\" for reading.").arg(filename));

  reloadFromFile();

  if(!filename.startsWith(":/"))
    fileWatcher->setFilenameAndStart(filename);
}

void MapLayerSettings::loadXmlInternal(atools::util::XmlStream& xmlStream)
{
  QXmlStreamReader& reader = xmlStream.getReader();

  layers.clear();
  xmlStream.readUntilElement("LittleNavmap");
  xmlStream.readUntilElement("MapLayerSettings");

  MapLayer defLayer = MapLayer(0);

  while(xmlStream.readNextStartElement())
  {
    // Read data from header =========================================
    if(reader.name() == "Layers")
    {
      while(xmlStream.readNextStartElement())
      {
        if(reader.name() == "Layer")
        {
          if(xmlStream.readAttributeBool("Base"))
          {
            // Base layer is default for first entry
            MapLayer layer(0);
            layer.loadFromXml(xmlStream);
            defLayer = layer;
          }
          else
          {
            float range = xmlStream.readAttributeFloat("MaximumRangeKm");

            // First is based on default and then each is based on its predecessor
            MapLayer layer = layers.isEmpty() ? defLayer.clone(range) : cloneLast(range);
            layer.loadFromXml(xmlStream);
            layers.append(layer);
          }
        }
        else
          xmlStream.skipCurrentElement(true /* warn */);
      }
    }
    else
      xmlStream.skipCurrentElement(true /* warn */);
  }
  finishAppend();
}

bool MapLayerSettings::compare(const MapLayer& layer, float distance) const
{
  // The value returned indicates whether the first argument is considered to go before the second.
  return layer.getMaxRange() < distance;
}

QDebug operator<<(QDebug out, const MapLayerSettings& record)
{
  QDebugStateSaver saver(out);

  out.nospace().noquote() << "LayerSettings[" << endl;

  out << "DEFAULT ------------------------------" << endl;
  out << MapLayer(0) << endl;

  for(const MapLayer& layer : record.layers)
  {
    out << "------------------------------" << endl;
    out << layer << endl;
  }

  out << "]";

  return out;
}
