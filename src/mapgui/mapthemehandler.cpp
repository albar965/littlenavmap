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
#include "common/constants.h"
#include "exception.h"
#include "settings/settings.h"
#include "util/simplecrypt.h"
#include "util/xmlstream.h"
#include "mapgui/mapwidget.h"
#include "navapp.h"
#include "gui/mainwindow.h"
#include "ui_mainwindow.h"

#include <QCoreApplication>
#include <QDebug>
#include <QFileInfo>
#include <QXmlStreamReader>
#include <QRegularExpression>
#include <QDataStream>
#include <QActionGroup>
#include <QComboBox>

#include <gui/dialog.h>

#include <marble/LegendWidget.h>

const static quint64 KEY = 0x19CB0467EBD391CC;
const static QLatin1String FILENAME("mapthemekeys.bin");

MapThemeHandler::MapThemeHandler(QWidget *mainWindowParam)
  : QObject(mainWindowParam), mainWindow(mainWindowParam)
{
}

MapThemeHandler::~MapThemeHandler()
{
  qDebug() << Q_FUNC_INFO << "delete actionGroupMapTheme";
  delete actionGroupMapTheme;

  qDebug() << Q_FUNC_INFO << "delete comboBoxMapTheme";
  delete comboBoxMapTheme;
}

void MapThemeHandler::loadThemes()
{
  earthDir = QDir(atools::buildPath({QCoreApplication::applicationDirPath(), "data", "maps", "earth"}));

  // Check if the base folder exists and throw an exception if not
  QString msg = atools::checkDirMsg(earthDir.path());
  if(!msg.isEmpty())
    throw atools::Exception(tr("Base path \"%1\" for map themes not found. %2").arg(earthDir.path()).arg(msg));

  // Load all these from folder
  themes.clear();
  themeIdToIndexMap.clear();
  QSet<QString> ids;
  QStringList errors;
  for(const QFileInfo& dgml : findMapThemes())
  {
    MapTheme theme = loadTheme(dgml);

    if(ids.contains(theme.theme))
    {
      errors.append(tr("Duplicate theme id \"%1\" in element \"&lt;theme&gt;\"."
                       "<br/>File \"%2\".").arg(theme.theme).arg(theme.dgmlFilepath));
      qWarning() << Q_FUNC_INFO << "Duplicate theme id" << theme.theme << "in theme file" << theme.dgmlFilepath;
      continue;
    }

    if(theme.theme.isEmpty())
    {
      errors.append(tr("Empty theme id in in element \"&lt;theme&gt;\"."
                       "<br/>File \"%1\".").arg(theme.dgmlFilepath));
      qWarning() << Q_FUNC_INFO << "Empty theme id in theme file" << theme.dgmlFilepath;
      continue;
    }

    if(theme.target != "earth")
    {
      errors.append(tr("Invalid target \"%1\" in element \"&lt;target&gt;\"."
                       "<br/>File \"%2\".<br/>"
                       "Element must contain text \"earth\".").arg(theme.target).arg(theme.dgmlFilepath));
      qWarning() << Q_FUNC_INFO << "Invalid target" << theme.target << "in theme file" << theme.dgmlFilepath;
      continue;
    }

    ids.insert(theme.theme);

    if(theme.visible)
    {
      // Add only visible themes for Earth
      themes.append(theme);

      // Collect all keys and insert with empty value
      for(const QString& key : theme.keys)
        mapThemeKeys.insert(key, QString());
    }
    else
      qInfo() << Q_FUNC_INFO << "Theme" << theme.theme << "not visible";
  }

  if(!errors.isEmpty())
  {
    NavApp::closeSplashScreen();
    QMessageBox::warning(mainWindow, QApplication::applicationName(),
                         tr("<p>Found errors in map %2:</p>"
                              "<ul><li>%1</li></ul>"
                                "<p>Ignoring %2.</p>").
                         arg(errors.join("</li><li>")).arg(errors.size() == 1 ? tr("this map theme") : tr("these map themes")));
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

    // Add to index
    themeIdToIndexMap.insert(themes.at(i).theme, i);
  }

  // Fall back to first if default OSM was not found
  if(!defaultTheme.isValid() && !themes.isEmpty())
    defaultTheme = themes.constFirst();
}

const MapTheme& MapThemeHandler::themeByIndex(int themeIndex) const
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

const MapTheme& MapThemeHandler::getTheme(const QString& themeId) const
{
  return themeByIndex(themeIdToIndexMap.value(themeId, -1));
}

QString MapThemeHandler::getCurrentThemeId() const
{
  return comboBoxMapTheme->currentData().toString();
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

void MapThemeHandler::saveState()
{
  QFile keyFile(atools::settings::Settings::instance().getPath() + QDir::separator() + FILENAME);
  if(keyFile.open(QIODevice::WriteOnly))
  {
    // Apply simple encryption to the keys
    // Note that this is not safe since the keys can be decrypted by all having access to these sources and the KEY above.
    atools::util::SimpleCrypt crypt(KEY);
    crypt.setCompressionMode(atools::util::SimpleCrypt::CompressionNever);
    crypt.setIntegrityProtectionMode(atools::util::SimpleCrypt::ProtectionChecksum);

    // Create a copy and encrypt keys
    QMap<QString, QString> keys(mapThemeKeys);
    for(auto it = keys.begin(); it != keys.end(); ++it)
      it.value() = crypt.encryptToString(it.value());

    // Save to binary file
    QDataStream stream(&keyFile);
    stream << keys;
    keyFile.close();
  }
  else
    throw atools::Exception(tr("Cannot open file %1. Reason: %2").arg(keyFile.fileName()).arg(keyFile.errorString()));

  atools::settings::Settings& settings = atools::settings::Settings::instance();
  settings.setValue(lnm::MAP_THEME, getCurrentThemeId());
}

void MapThemeHandler::restoreState()
{
  QFile keyFile(atools::settings::Settings::instance().getPath() + QDir::separator() + FILENAME);

  if(atools::checkFile(keyFile, false /* warn */))
  {
    if(keyFile.open(QIODevice::ReadOnly))
    {
      // Load from binary file
      QMap<QString, QString> keys;
      QDataStream stream(&keyFile);
      stream >> keys;

      // Decrypt keys and merge into list fetched from DGML files
      atools::util::SimpleCrypt crypt(KEY);
      for(auto it = keys.begin(); it != keys.end(); ++it)
        mapThemeKeys.insert(it.key(), crypt.decryptToString(it.value()));

      keyFile.close();
    }
    else
      throw atools::Exception(tr("Cannot open file %1. Reason: %2").arg(keyFile.fileName()).arg(keyFile.errorString()));
  }

  atools::settings::Settings& settings = atools::settings::Settings::instance();

  QString themeId = getDefaultTheme().getThemeId();
  if(settings.contains(lnm::MAP_THEME) && getTheme(settings.valueStr(lnm::MAP_THEME)).isValid())
    // Restore map theme selection
    themeId = settings.valueStr(lnm::MAP_THEME);

  changeMapThemeComboBox(themeId, true /* blockSignals */);
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
                      if(key != "x" && key != "y" && key != "z" && key != "zoomLevel" && key != "language" &&
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

void MapThemeHandler::setupMapThemesUi()
{
  Ui::MainWindow *ui = NavApp::getMainUi();
  // Theme combo box ============================
  delete comboBoxMapTheme;
  comboBoxMapTheme = new QComboBox(mainWindow);
  comboBoxMapTheme->setObjectName("comboBoxMapTheme");
  comboBoxMapTheme->setToolTip(tr("Select map theme"));
  comboBoxMapTheme->setStatusTip(comboBoxMapTheme->toolTip());
  ui->toolBarMapThemeProjection->addWidget(comboBoxMapTheme);

  // Theme menu items ===============================
  delete actionGroupMapTheme;
  actionGroupMapTheme = new QActionGroup(ui->menuViewTheme);
  actionGroupMapTheme->setObjectName("actionGroupMapTheme");

  bool online = true;
  int index = 0;
  // Sort order is always online/offline and then alphabetical
  for(const MapTheme& theme : getThemes())
  {
    // Check if offline map come after online and add separators
    if(!theme.isOnline() && online)
    {
      // Add separator between online and offline maps
      ui->menuViewTheme->addSeparator();
      comboBoxMapTheme->insertSeparator(comboBoxMapTheme->count());
    }

    // Add item to combo box in toolbar
    QString name = theme.isOnline() ? theme.getName() : tr("%1 (offline)").arg(theme.getName());
    if(theme.hasKeys())
      // Add star to maps which require an API key or token
      name += tr(" *");

    // Build tooltip for entries
    QStringList tip;
    tip.append(theme.getName());
    tip.append(theme.isOnline() ? tr("online") : tr("offline"));
    tip.append(theme.hasKeys() ? tr("* requires registration") : tr("free"));

    // Add item and attach index for theme in MapThemeHandler
    comboBoxMapTheme->addItem(name, theme.getThemeId());
    comboBoxMapTheme->setItemData(index, tip.join(tr(", ")), Qt::ToolTipRole);

    // Create action for map/theme submenu
    QAction *action = ui->menuViewTheme->addAction(name);
    action->setCheckable(true);
    action->setToolTip(tip.join(tr(", ")));
    action->setStatusTip(action->toolTip());
    action->setActionGroup(actionGroupMapTheme);

    // Add keyboard shortcut for top 10 themes
    if(index < 10)
      action->setShortcut(tr("Ctrl+Alt+%1").arg(index));

    // Attach theme name for theme in MapThemeHandler
    action->setData(theme.getThemeId());

#ifdef DEBUG_INFORMATION
    qDebug() << Q_FUNC_INFO << name << index;
#endif

    index++;

    // Remember theme online status
    online = theme.isOnline();
  }

  connect(comboBoxMapTheme, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MapThemeHandler::changeMapTheme);
  // Let theme menus update combo boxes
  for(QAction *action : actionGroupMapTheme->actions())
    connect(action, &QAction::triggered, this, &MapThemeHandler::themeMenuTriggered);
}

void MapThemeHandler::themeMenuTriggered(bool checked)
{
  QAction *action = dynamic_cast<QAction *>(sender());
  if(action != nullptr && checked)
    changeMapThemeComboBox(action->data().toString(), false /* blockSignals */);
}

void MapThemeHandler::changeMapThemeComboBox(const QString& themeId, bool blockSignals)
{
  if(!themeId.isEmpty())
  {
    // Search for combo box entry with index for MapThemeHandler
    for(int i = 0; i < comboBoxMapTheme->count(); i++)
    {
      QVariant data = comboBoxMapTheme->itemData(i);
      if(data.isValid() && data.toString() == themeId)
      {
        // Avoid recursion by blocking signals
        if(blockSignals)
          comboBoxMapTheme->blockSignals(true);
        comboBoxMapTheme->setCurrentIndex(i);
        if(blockSignals)
          comboBoxMapTheme->blockSignals(false);
        break;
      }
    }
  }
}

void MapThemeHandler::changeMapTheme()
{
  MapWidget *mapWidget = NavApp::getMapWidgetGui();
  mapWidget->cancelDragAll();

  QString themeId = comboBoxMapTheme->currentData().toString();
  MapTheme theme = getTheme(themeId);

  if(!theme.isValid())
  {
    qDebug() << Q_FUNC_INFO << "Falling back to default theme due to invalid index" << themeId;
    // No theme for index found - use default OSM
    theme = getDefaultTheme();
    themeId = theme.getThemeId();

    changeMapThemeComboBox(themeId, true /* blockSignals */);
  }

  // Check if theme needs API keys, usernames or tokens ======================================
  if(theme.hasKeys())
  {
    // Check if all required values are set
    bool allValid = true;
    for(const QString& key : theme.getKeys())
    {
      if(mapThemeKeys.value(key).isEmpty())
      {
        allValid = false;
        break;
      }
    }

    if(!allValid)
    {
      // One or more keys are not present or empty - show info dialog =================================
      NavApp::closeSplashScreen();

      // Fetch all keys for map theme
      QString url;
      if(!theme.getUrlRef().isEmpty())
        url = tr("<p>Click here to create an account: <a href=\"%1\">%2</a></p>").
              arg(theme.getUrlRef()).arg(theme.getUrlName().isEmpty() ? tr("Link") : theme.getUrlName());

      atools::gui::Dialog(mainWindow).
      showInfoMsgBox(lnm::ACTIONS_SHOW_MAPTHEME_REQUIRES_KEY,
                     tr("<p>The map theme \"%1\" requires additional information.</p>"
                          "<p>You have to create an user account at the related website and then create an username, an access key or a token.<br/>"
                          "Most of these services offer a free plan for hobbyists.</p>"
                          "<p>Then go to menu \"Tools\" -> \"Options\" and to page \"Map Display Keys\" in Little Navmap and "
                            "enter the information for the key(s) below:</p>"
                            "<ul><li>%2</li></ul>"
                              "<p>The map will not show correctly until this is done.</p>%3").
                     arg(theme.getName()).arg(theme.getKeys().join(tr("</li><li>"))).arg(url),
                     tr("Do not &show this dialog again."));
    }
  }

  qDebug() << Q_FUNC_INFO << themeId << theme;

  mapWidget->setTheme(theme.getId(), themeId);

  // Update menu items in group
  for(QAction *action : actionGroupMapTheme->actions())
  {
    action->blockSignals(true);
    action->setChecked(action->data().toString() == themeId);
    action->blockSignals(false);
  }

  updateLegend();

  NavApp::setStatusMessage(tr("Map theme changed to %1.").arg(comboBoxMapTheme->currentText()));
}

void MapThemeHandler::updateLegend()
{
  Ui::MainWindow *ui = NavApp::getMainUi();

  Marble::LegendWidget *legendWidget = new Marble::LegendWidget(mainWindow);
  legendWidget->setMarbleModel(NavApp::getMapWidgetGui()->model());
  QString basePath;
  QString html = legendWidget->getHtml(basePath);
  ui->textBrowserLegendInfo->setSearchPaths({basePath});
  ui->textBrowserLegendInfo->setText(html);
  delete legendWidget;
}
