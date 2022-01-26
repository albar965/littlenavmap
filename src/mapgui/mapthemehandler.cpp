/*****************************************************************************
* Copyright 2015-2022 Alexander Barthel alex@littlenavmap.org
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

#include "mapgui/mapthemehandler.h"

#include "atools.h"
#include "exception.h"
#include "settings/settings.h"
#include "common/constants.h"
#include "util/xmlstream.h"

#include <QCoreApplication>
#include <QDebug>
#include <QFileInfo>
#include <QXmlStreamReader>
#include <QRegularExpression>

void MapThemeHandler::loadThemes()
{
  earthDir = QDir(atools::buildPath({QCoreApplication::applicationDirPath(), "data", "maps", "earth"}));

  // Check if the base folder exists and throw an exception if not
  QString msg = atools::checkDirMsg(earthDir.path());
  if(!msg.isEmpty())
    throw atools::Exception(tr("Base path \"%1\" for map themes not found. %2").arg(earthDir.path()).arg(msg));

  // Load all these from folder
  themes.clear();
  for(const QFileInfo& dgml : findMapThemes())
  {
    MapTheme theme = loadTheme(dgml);
    if(theme.visible && theme.target == "earth")
    {
      // Add only visible themes for Earth
      themes.append(theme);

      // Collect all keys and insert with empty value
      for(const QString& key : theme.keys)
        mapThemeKeys.insert(key, QString());
    }
  }

  // Sort themes first by online/offline status and then by name
  std::sort(themes.begin(), themes.end(), [](const MapTheme& t1, const MapTheme& t2) {
    if(t1.online == t2.online)
      return t1.name.compare(t2.name) < 0;
    else
      return t1.online > t2.online;
  });

  // Insert index into key objects and detect default OSM theme
  for(int i = 0; i < themes.size(); i++)
  {
    themes[i].index = i;

    if(themes.at(i).theme == "openstreetmap")
      defaultTheme = themes.at(i);
  }

  // Fall back to first if default OSM was not found
  if(!defaultTheme.isValid() && !themes.isEmpty())
    defaultTheme = themes.constFirst();
}

const MapTheme& MapThemeHandler::getTheme(int themeIndex) const
{
  const static MapTheme INVALID;

  if(themeIndex >= 0 && themeIndex < themes.size())
    return themes.at(themeIndex);
  else
  {
    // Return static default constructed value
    qWarning() << Q_FUNC_INFO << "Invalid theme index" << themeIndex;
    return INVALID;
  }
}

QHash<QString, QString> MapThemeHandler::getMapThemeKeysHash() const
{
  QHash<QString, QString> hash;
  for(auto it = mapThemeKeys.constBegin(); it != mapThemeKeys.constEnd(); ++it)
    hash.insert(it.key(), it.value());
  return hash;
}

void MapThemeHandler::clearMapThemeKeyValues()
{
  for(auto it = mapThemeKeys.begin(); it != mapThemeKeys.end(); ++it)
    it.value().clear();
}

void MapThemeHandler::restoreState()
{
  QStringList keys = atools::settings::Settings::instance().valueStrList(lnm::MAP_THEME_KEYS);

  for(int i = 0; i < keys.size(); i += 2)
  {
    QString key = keys.value(i);

    // Add only keys which are available from map configuration - others are ignored
    if(mapThemeKeys.contains(key))
      mapThemeKeys.insert(key, keys.value(i + 1));
  }
}

void MapThemeHandler::saveState()
{
  QStringList list;

  for(auto it = mapThemeKeys.begin(); it != mapThemeKeys.end(); ++it)
  {
    list.append(it.key());
    list.append(it.value());
  }

  atools::settings::Settings::instance().setValue(lnm::MAP_THEME_KEYS, list);
}

MapTheme MapThemeHandler::loadTheme(const QFileInfo& dgml)
{
  // Regexp to detect keywords like {API Key}
  static const QRegularExpression KEYSREGEXP("\\{([^\\}]+)\\}");

  MapTheme theme;
  QFile dgmlFile(dgml.filePath());
  if(dgmlFile.open(QIODevice::ReadOnly | QIODevice::Text))
  {
    atools::util::XmlStream xmlStream(&dgmlFile);
    QXmlStreamReader& reader = xmlStream.getReader();

    // Skip to more important parts
    xmlStream.readUntilElement("dgml");
    xmlStream.readUntilElement("document");

    while(xmlStream.readNextStartElement())
    {
      // head ====================================================================
      if(reader.name() == "head")
      {
        while(xmlStream.readNextStartElement())
        {
          if(reader.name() == "license")
          {
            theme.copyright = reader.attributes().value("short").toString().simplified();
            xmlStream.skipCurrentElement();
          }
          else if(reader.name() == "name")
            theme.name = reader.readElementText().simplified();
          else if(reader.name() == "target")
            theme.target = reader.readElementText().simplified();
          else if(reader.name() == "theme")
            theme.theme = reader.readElementText().simplified();
          else if(reader.name() == "visible")
            theme.visible = reader.readElementText().simplified().toLower() == "true";
          else if(reader.name() == "url")
          {
            theme.urlRef = reader.attributes().value("href").toString();
            theme.urlName = reader.readElementText().simplified();
          }
          // head/zoom ============================
          else if(reader.name() == "zoom")
          {
            while(xmlStream.readNextStartElement())
            {
              if(reader.name() == "discrete")
                theme.discrete = reader.readElementText().simplified().toLower() == "true";
              else
                xmlStream.skipCurrentElement();
            }
          }
          else
            xmlStream.skipCurrentElement();
        }

        theme.dgmlFilepath = dgml.filePath();

        // Create a new entry with path relative to "earth"
        theme.id = QString("earth") + QDir::separator() + earthDir.relativeFilePath(dgml.absoluteFilePath());
      }
      // map ====================================================================
      else if(reader.name() == "map")
      {
        while(xmlStream.readNextStartElement())
        {
          if(reader.name() == "layer")
          {
            while(xmlStream.readNextStartElement())
            {
              // map/layer/texture ===================
              if(reader.name() == "texture")
              {
                theme.textureLayer = true;

                while(xmlStream.readNextStartElement())
                {
                  if(reader.name() == "downloadUrl")
                  {
                    // Put all attributes of the download URL into one string
                    QString atts = reader.attributes().value("protocol").toString() + reader.attributes().value("host").toString() +
                                   reader.attributes().value("path").toString();

                    // Extract keywords from download URL
                    QRegularExpressionMatchIterator regexpIter = KEYSREGEXP.globalMatch(atts);
                    while(regexpIter.hasNext())
                    {
                      QString key = regexpIter.next().captured(1);

                      // Ignore default keys
                      if(key != "x" && key != "y" && key != "z" && key != "zoomLevel" &&
                         key != "west" && key != "south" && key != "east" && key != "north")
                        theme.keys.append(key);
                    }

                    // Online theme of download URL is given
                    theme.online = true;
                  }

                  xmlStream.skipCurrentElement();
                }
              }
              // map/layer/geodata ===================
              else if(reader.name() == "geodata")
              {
                // Offline theme of geodata is given
                theme.geodataLayer = true;
                xmlStream.skipCurrentElement();
              }
              else
                xmlStream.skipCurrentElement();
            } // while(xmlStream.readNextStartElement())
          } // if(reader.name() == "layer")
          else
            xmlStream.skipCurrentElement();
        }
      } // if(reader.name() == "head") ... else if(reader.name() == "map")
      else
        xmlStream.skipCurrentElement();
    } // while(xmlStream.readNextStartElement())

    dgmlFile.close();
  } // if(dgmlFile.open(QIODevice::ReadOnly | QIODevice::Text))
  else
    throw atools::Exception(tr("Cannot open file %1. Reason: %2").arg(dgmlFile.fileName()).arg(dgmlFile.errorString()));

#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << theme;
#endif

  return theme;
}

QList<QFileInfo> MapThemeHandler::findMapThemes()
{
  QList<QFileInfo> dgmlFileInfos;

  // Get all folders from "earth"
  for(const QFileInfo& themeDirInfo : earthDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot))
  {
    // Check if folder is accessible
    if(atools::checkDir(themeDirInfo, true /* warn */))
    {
      // Theme folder
      QDir themeDir(themeDirInfo.absoluteFilePath());

      // Get all DGML files in folder - should be only one
      int found = 0;
      for(const QFileInfo& themeFile : themeDir.entryInfoList({"*.dgml"}, QDir::Files | QDir::NoDotAndDotDot))
      {
        if(atools::checkFile(themeFile, true /* warn */))
        {
          qInfo() << Q_FUNC_INFO << "Found map theme file <<" << themeFile.absoluteFilePath();
          dgmlFileInfos.append(themeFile);
          found++;
        }
      }

      if(found == 0)
        qWarning() << Q_FUNC_INFO << "No DGML file found in folder" << themeDirInfo.absoluteFilePath();
      else if(found > 1)
        qWarning() << Q_FUNC_INFO << "More than one DGML file found in folder" << themeDirInfo.absoluteFilePath();
    }
  }
  return dgmlFileInfos;
}

QDebug operator<<(QDebug out, const MapTheme& theme)
{
  out << "MapTheme("
      << "id" << theme.id
      << "index" << theme.index
      << "urlName" << theme.urlName
      << "urlRef" << theme.urlRef
      << "dgmlFilepath" << theme.dgmlFilepath
      << "name" << theme.name
      << "copyright" << theme.copyright
      << "theme" << theme.theme
      << "target" << theme.target
      << "keys" << theme.keys
      << "online" << theme.online
      << "textureLayer" << theme.textureLayer
      << "geodataLayer" << theme.geodataLayer
      << "discrete" << theme.discrete
      << ")";
  return out;
}
