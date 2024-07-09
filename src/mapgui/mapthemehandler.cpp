/*****************************************************************************
* Copyright 2015-2024 Alexander Barthel alex@littlenavmap.org
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
#include "gui/messagebox.h"
#include "settings/settings.h"
#include "util/htmlbuilder.h"
#include "util/simplecrypt.h"
#include "util/xmlstream.h"
#include "mapgui/mapwidget.h"
#include "app/navapp.h"
#include "ui_mainwindow.h"
#include "gui/dialog.h"
#include "gui/widgetstate.h"
#include "options/optiondata.h"

#include <QCoreApplication>
#include <QDebug>
#include <QFileInfo>
#include <QXmlStreamReader>
#include <QRegularExpression>
#include <QDataStream>
#include <QActionGroup>
#include <QStringBuilder>
#include <QSettings>

const static quint64 KEY = 0x19CB0467EBD391CC;
const static QLatin1String FILENAME("mapthemekeys.bin");
const static int MAX_ERRORS = 5;

QString MapTheme::dgmlDisplayPath() const
{
  return atools::nativeCleanPath(dgmlFilepath);
}

QString MapTheme::displayPath() const
{
  return atools::nativeCleanPath(QFileInfo(dgmlFilepath).path());
}

// ==================================================================================
MapThemeHandler::MapThemeHandler(QWidget *mainWindowParam)
  : QObject(mainWindowParam), mainWindow(mainWindowParam)
{
  // Load list of themes to reject from configuration file
  QSettings settings(atools::settings::Settings::getOverloadedPath(":/littlenavmap/resources/config/mapthemes.cfg"), QSettings::IniFormat);
  settings.beginGroup("RejectDownloadUrl");
  const QStringList keys = settings.childKeys();
  for(const QString& key : keys)
  {
    QString regexpStr = settings.value(key).toString();
    if(!regexpStr.isEmpty())
    {
#ifdef DEBUG_INFORMATION
      qDebug().noquote().nospace() << Q_FUNC_INFO << " key " << key << " regexpStr " << regexpStr;
#endif

      QRegularExpression regexp(regexpStr);

      if(regexp.isValid())
        rejectDownloadUrlList.insert(regexp);
      else
        qWarning() << Q_FUNC_INFO << "Invalid regular expression" << regexpStr << "for key" << key;
    }
    else
      qWarning() << Q_FUNC_INFO << "Empty value for key" << key;
  }
  settings.endGroup();
}

MapThemeHandler::~MapThemeHandler()
{
  ATOOLS_DELETE_LOG(actionGroupMapTheme);
  ATOOLS_DELETE_LOG(toolButtonMapTheme);
}

void MapThemeHandler::loadThemes()
{
  using atools::util::HtmlBuilder;
  const atools::util::html::Flags FLAGS = atools::util::html::NOBR_WHITESPACE;
  const static QRegularExpression SHORTCUT_REGEXP("^Ctrl\\+Alt\\+[0-9]?$");

  // Load all these from folder
  themes.clear();
  themeIdToIndexMap.clear();
  errors.clear();

  QHash<QString, MapTheme> ids, sourceDirs;
  QSet<QString> shortcuts;
  for(const QFileInfo& dgml : findMapThemes({getMapThemeDefaultDir(), getMapThemeUserDir()}))
  {
    MapTheme theme = loadTheme(dgml);

    if(theme.visible)
    {
      if(ids.contains(theme.theme))
      {
        MapTheme otherTheme = ids.value(theme.theme);
        errors.append(tr("Duplicate map theme id \"%1\" in element \"&lt;theme&gt;\". Theme with first occurrence"
                         "%2&nbsp;(click to show).<br/>"
                         "Theme with second occurrence being ignored"
                         "%3&nbsp;(click to show).<br/>"
                         "Theme ids have to be unique across all map themes.<br/>"
                         "<b>Remove one of these two map themes to avoid this message.</b><br/>").
                      arg(theme.theme).
                      arg(HtmlBuilder::aFilePath(otherTheme.displayPath(), FLAGS)).
                      arg(HtmlBuilder::aFilePath(theme.displayPath(), FLAGS)));
        continue;
      }

      // Present source dirs loaded so far
      QSet<QString> sourceDirKeys(sourceDirs.keyBegin(), sourceDirs.keyEnd());

      // Test for overlapping source dirs with this theme
      QSet<QString> conflictSourceDirKeys = sourceDirKeys.intersect(QSet<QString>(theme.sourceDirs.begin(), theme.sourceDirs.end()));

      if(theme.online && !conflictSourceDirKeys.isEmpty())
      {
        // Get a list of themes using the same source dirs
        QList<MapTheme> otherThemes;
        for(auto it = conflictSourceDirKeys.begin(); it != conflictSourceDirKeys.end(); ++it)
          otherThemes.append(sourceDirs.values(*it));

        // Get file paths for DGML
        QStringList otherDgmlFilepaths;
        for(const MapTheme& t : otherThemes)
          otherDgmlFilepaths.append(t.displayPath());
        otherDgmlFilepaths.removeAll(QString());
        otherDgmlFilepaths.removeDuplicates();

        QStringList otherDgmlFilepathsText;
        for(const QString& otherDgmlFilepath : otherDgmlFilepaths)
          otherDgmlFilepathsText.append(HtmlBuilder::aFilePath(otherDgmlFilepath, FLAGS));

        errors.append(tr("Duplicate source directory or directories \"%1\" in element \"&lt;sourcedir&gt;\".<br/>"
                         "Map theme with first occurrence"
                         "%2&nbsp;(click to show).<br/>"
                         "Theme(s) with second occurrence being ignored"
                         "%3&nbsp;(click to show).<br/>"
                         "Source directories are used to cache map tiles and have to be unique across all map themes.<br/>"
                         "<b>Remove one or more of these map themes to avoid this message.</b><br/>").
                      arg(theme.sourceDirs.join(tr("\", \""))).
                      arg(HtmlBuilder::aFilePath(theme.displayPath(), FLAGS)).
                      arg(otherDgmlFilepathsText.join(tr("<br/>"))));
        continue;
      }

      if(theme.theme.isEmpty())
      {
        errors.append(tr("Empty id in in element \"&lt;theme&gt;\" in map theme"
                         "%1&nbsp;(click to show).<br/>"
                         "<b>Remove or repair this map theme to avoid this message.</b><br/>").
                      arg(HtmlBuilder::aFilePath(theme.displayPath(), FLAGS)));
        continue;
      }

      if(theme.online && theme.sourceDirs.isEmpty())
      {
        errors.append(tr("Empty source directory in in element \"&lt;sourcedir&gt;\" in map theme"
                         "%1&nbsp;(click to show).<br/>"
                         "<b>Remove or repair this map theme to avoid this message.</b><br/>").
                      arg(HtmlBuilder::aFilePath(theme.displayPath(), FLAGS)));
        continue;
      }

      if(theme.target != "earth")
      {
        errors.append(tr("Invalid target \"%1\" in element \"&lt;target&gt;\" in map theme "
                         "%2&nbsp;(click to show).<br/>"
                         "Element must contain text \"earth\".<br/>"
                         "<b>Remove or repair this map theme to avoid this message.</b><br/>").
                      arg(theme.target).
                      arg(HtmlBuilder::aFilePath(theme.displayPath(), FLAGS)));
        continue;
      }

      bool rejected = false;
      for(const QString& host : qAsConst(theme.downloadHosts))
      {
        for(const QRegularExpression& rejectExpr : qAsConst(rejectDownloadUrlList))
        {
          if(rejectExpr.match(host).hasMatch())
          {
            rejected = true;
            break;
          }
        }
      }

      if(rejected)
      {
        errors.append(tr("Map theme"
                         "%1&nbsp;(click to show)<br/>"
                         "was rejected since the service is discontinued.<br/>"
                         "<b>Remove this map theme to avoid this message.</b><br/>").
                      arg(HtmlBuilder::aFilePath(theme.displayPath(), FLAGS)));
        continue;
      }

      // Check shortcut ============================
      if(!theme.getShortcut().isEmpty())
      {
        if(!SHORTCUT_REGEXP.match(theme.getShortcut()).hasMatch())
        {
          errors.append(tr("Map theme"
                           "%1&nbsp;(click to show)<br/>"
                           "has an invalid shortcut \"%2\". Only \"Ctrl+Alt+NUMBER\" allowed.<br/>"
                           "<b>Remove this map theme or adjust element \"&lt;shortcut&gt;\" avoid this message.</b><br/>").
                        arg(HtmlBuilder::aFilePath(theme.displayPath(), FLAGS)).arg(theme.getShortcut()));
          continue;
        }
        else if(!shortcuts.contains(theme.getShortcut().toLower()))
          shortcuts.insert(theme.getShortcut().toLower());
        else
        {
          errors.append(tr("Map theme"
                           "%1&nbsp;(click to show)<br/>"
                           "has a duplicate shortcut \"%2\".<br/>"
                           "<b>Remove this map theme or adjust element \"&lt;shortcut&gt;\" avoid this message.</b><br/>").
                        arg(HtmlBuilder::aFilePath(theme.displayPath(), FLAGS)).arg(theme.getShortcut()));
          continue;
        }
      }

      ids.insert(theme.theme, theme);
      for(const QString& dir : qAsConst(theme.sourceDirs))
        sourceDirs.insert(dir, theme);

      qInfo() << Q_FUNC_INFO << "Found" << theme.theme << theme.name;

      // Add only visible themes for Earth
      themes.append(theme);

      // Collect all keys and insert with empty value
      for(const QString& key : qAsConst(theme.keys))
        mapThemeKeys.insert(key, QString());
    }
    else
      qInfo() << Q_FUNC_INFO << "Theme" << theme.theme << "not visible";
  }

  // Sort themes first by online/offline status and then case insensitive by name
  std::sort(themes.begin(), themes.end(), [](const MapTheme& t1, const MapTheme& t2) {
    if(t1.online == t2.online)
      return t1.name.compare(t2.name, Qt::CaseInsensitive) < 0;
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

void MapThemeHandler::showThemeLoadingErrors()
{
  if(!errors.isEmpty())
  {
    if(errors.size() > MAX_ERRORS)
    {
      int numMore = errors.size() - MAX_ERRORS;
      errors = errors.mid(0, MAX_ERRORS);
      errors.append(tr("<b>%1 more %2 found ...</b>").arg(numMore).arg(numMore == 1 ? tr("error") : tr("errors")));
    }

    atools::gui::MessageBox box(mainWindow);
    box.setIcon(QMessageBox::Warning);
    box.setMessage(tr("<p>Found errors in map %2:</p>"
                        "<ul><li>%1</li></ul>"
                          "<p>Ignoring duplicate, incorrect or rejected %2.</p>"
                            "<p>Note that all other valid map themes are loaded and can be used despite this message.</p>"
                              "<p>Restart Little Navmap after fixing the issues.</p>").
                   arg(errors.join("</li><li>")).arg(errors.size() == 1 ? tr("map theme") : tr("map themes")));
    box.setHelpUrl(lnm::helpOnlineUrl + "MAPTHEMES.html", lnm::helpLanguageOnline());
    box.setShowInFileManager();
    box.exec();
  }
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
  if(actionGroupMapTheme->checkedAction() == nullptr)
  {
    qWarning() << Q_FUNC_INFO << "checkedAction is null";
    return QString();
  }
  else
    return actionGroupMapTheme->checkedAction()->data().toString();
}

QHash<QString, QString> MapThemeHandler::getMapThemeKeysHash() const
{
  QHash<QString, QString> hash;
  for(auto it = mapThemeKeys.constBegin(); it != mapThemeKeys.constEnd(); ++it)
    hash.insert(it.key(), it.value());
  return hash;
}

void MapThemeHandler::setMapThemeKeys(const QMap<QString, QString>& keys)
{
  qDebug() << Q_FUNC_INFO << keys.keys();
  mapThemeKeys = keys;
}

void MapThemeHandler::saveKeyfile() const
{
  // Save keys ===================================================
  QFile keyFile(atools::settings::Settings::getPath() % atools::SEP % FILENAME);
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
    {
      it.value() = crypt.encryptToString(it.value());
      if(crypt.lastError() != atools::util::SimpleCrypt::ErrorNoError)
      {
        qWarning() << Q_FUNC_INFO << "Failed encrypting key to" << keyFile.fileName() << "error" << crypt.lastError();
        throw atools::Exception(tr("Failed encrypting key for %1. Reason: %2").arg(keyFile.fileName()).arg(crypt.lastError()));
      }
    }

    // Save to binary file
    QDataStream stream(&keyFile);
    stream << keys;

    // Call flush to catch potential errors
    if(keyFile.flush())
      qDebug() << Q_FUNC_INFO << "Wrote" << keys.size() << "keys";
    else
    {
      qWarning() << Q_FUNC_INFO << "Failed writing" << keys.size() << "keys to" << keyFile.fileName() << "error" << keyFile.errorString();
      throw atools::Exception(tr("Failed writing to %1. Reason: %2").arg(keyFile.fileName()).arg(keyFile.errorString()));
    }

    keyFile.close();
  }
  else
  {
    qWarning() << Q_FUNC_INFO << "Cannot open for writing" << keyFile.fileName() << "error" << keyFile.errorString();
    throw atools::Exception(tr("Cannot open file for writing %1. Reason: %2").arg(keyFile.fileName()).arg(keyFile.errorString()));
  }
}

void MapThemeHandler::saveState() const
{
  qDebug() << Q_FUNC_INFO;

  saveKeyfile();

  // Save current theme ===================================================
  atools::settings::Settings& settings = atools::settings::Settings::instance();
  settings.setValue(lnm::MAP_THEME, getCurrentThemeId());

  atools::gui::WidgetState widgetState(lnm::MAINWINDOW_WIDGET_MAPTHEME);
  widgetState.save(mapProjectionActionGroup);
}

void MapThemeHandler::restoreState()
{
  qDebug() << Q_FUNC_INFO;

  restoreKeyfile();

  atools::settings::Settings& settings = atools::settings::Settings::instance();

  // Load Current theme ===================================================
  QString themeId = defaultTheme.getThemeId();
  if(settings.contains(lnm::MAP_THEME) && getTheme(settings.valueStr(lnm::MAP_THEME)).isValid())
    // Restore map theme selection
    themeId = settings.valueStr(lnm::MAP_THEME);

  // Check related action
  changeMapThemeActions(themeId);

  atools::gui::WidgetState widgetState(lnm::MAINWINDOW_WIDGET_MAPTHEME);
  widgetState.restore(mapProjectionActionGroup);
}

void MapThemeHandler::restoreKeyfile()
{
  // Load keys ===================================================
  QFile keyFile(atools::settings::Settings::getPath() % atools::SEP % FILENAME);

  if(keyFile.exists())
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
      {
        mapThemeKeys.insert(it.key(), crypt.decryptToString(it.value()));
        if(crypt.lastError() != atools::util::SimpleCrypt::ErrorNoError)
        {
          qWarning() << Q_FUNC_INFO << "Failed decrypting key from" << keyFile.fileName() << crypt.lastError();
          throw atools::Exception(tr("Failed decrypting key from %1. Reason: %2").arg(keyFile.fileName()).arg(crypt.lastError()));
        }
      }

      if(keyFile.error() != QFileDevice::NoError)
      {
        qWarning() << Q_FUNC_INFO << "Failed reading" << keys.size() << "keys to" << keyFile.fileName() << "error" << keyFile.errorString();
        throw atools::Exception(tr("Failed reading from %1. Reason: %2").arg(keyFile.fileName()).arg(keyFile.errorString()));
      }

      keyFile.close();
    }
    else
    {
      qWarning() << Q_FUNC_INFO << "Cannot open for reading" << keyFile.fileName() << "error" << keyFile.errorString();
      throw atools::Exception(tr("Cannot open file for reading %1. Reason: %2").arg(keyFile.fileName()).arg(keyFile.errorString()));
    }
  }
  else
    qDebug() << Q_FUNC_INFO << "File does not exist" << keyFile.fileName();
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
          else if(reader.name() == "shortcut")
            theme.shortcut = reader.readElementText().simplified();
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

        theme.dgmlFilepath = dgml.canonicalFilePath();
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
                    QString host = reader.attributes().value("host").toString();

                    // Put all attributes of the download URL into one string
                    QString atts = reader.attributes().value("protocol").toString() % host % reader.attributes().value("path").toString();

                    // Extract keywords from download URL
                    QRegularExpressionMatchIterator regexpIter = KEYSREGEXP.globalMatch(atts);
                    while(regexpIter.hasNext())
                    {
                      QString key = regexpIter.next().captured(1);

                      // Ignore default keys
                      if(key != "x" && key != "y" && key != "z" && key != "zoomLevel" && key != "quadIndex" && key != "language" &&
                         key != "west" && key != "south" && key != "east" && key != "north")
                        theme.keys.append(key);
                    }

                    theme.downloadHosts.append(host);

                    // Online theme of download URL is given
                    theme.online = true;
                  }
                  else if(reader.name() == "sourcedir")
                    theme.sourceDirs.append(atools::nativeCleanPath(reader.readElementText().trimmed()));
                  else
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

  theme.sourceDirs.removeAll(QString());
  theme.sourceDirs.sort();
  return theme;
}

const QList<QFileInfo> MapThemeHandler::findMapThemes(const QStringList& paths)
{
  QList<QFileInfo> dgmlFileInfos;

  for(const QString& path : paths)
  {
    if(path.isEmpty())
      continue;

    QDir dir(path);
    if(dir.exists())
    {
      // Get all folders from "earth"
      const QFileInfoList themeDirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
      for(const QFileInfo& themeDirInfo : themeDirs)
      {
        // Check if folder is accessible
        if(atools::checkDir(Q_FUNC_INFO, themeDirInfo, true /* warn */))
        {
          // Theme folder
          QDir themeDir(themeDirInfo.absoluteFilePath());

          // Get all DGML files in folder - should be only one
          int found = 0;
          const QFileInfoList themeFiles = themeDir.entryInfoList({"*.dgml"}, QDir::Files | QDir::NoDotAndDotDot);
          for(const QFileInfo& themeFile : themeFiles)
          {
            if(atools::checkFile(Q_FUNC_INFO, themeFile, true /* warn */))
            {
              qInfo() << "MapThemeHandler::findMapThemes(): Found map theme file" << themeFile.absoluteFilePath();
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
    } // if(dir.exists())
  } // for(const QString& path : paths)
  return dgmlFileInfos;
}

QDebug operator<<(QDebug out, const MapTheme& theme)
{
  out << "MapTheme("
      << "index" << theme.index
      << "urlName" << theme.urlName
      << "urlRef" << theme.urlRef
      << "sourceDirs" << theme.sourceDirs
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
      << "shortcut" << theme.shortcut
      << ")";
  return out;
}

void MapThemeHandler::setupMapThemesUi()
{
  // Map projection =========================================
  Ui::MainWindow *ui = NavApp::getMainUi();
  delete mapProjectionActionGroup;
  mapProjectionActionGroup = new QActionGroup(this);
  mapProjectionActionGroup->setObjectName("mapProjectionActionGroup");
  mapProjectionActionGroup->addAction(ui->actionMapProjectionMercator);
  mapProjectionActionGroup->addAction(ui->actionMapProjectionSpherical);
  connect(mapProjectionActionGroup, &QActionGroup::triggered, this, &MapThemeHandler::changeMapProjection);

  // Map themes =========================================
  delete toolButtonMapTheme;
  toolButtonMapTheme = new QToolButton(ui->toolBarMapOptions);

  // Create and add toolbar button =====================================
  toolButtonMapTheme->setIcon(QIcon(":/littlenavmap/resources/icons/map.svg"));
  toolButtonMapTheme->setPopupMode(QToolButton::InstantPopup);
  toolButtonMapTheme->setToolTip(tr("Select map theme and map projection"));
  toolButtonMapTheme->setStatusTip(toolButtonMapTheme->toolTip());
  toolButtonMapTheme->setCheckable(true);

  // Replace dummy action with tool button
  ui->toolBarMap->insertWidget(ui->actionThemeHandlerDummy, toolButtonMapTheme);
  ui->toolBarMap->removeAction(ui->actionThemeHandlerDummy);

  // Add tear off menu to button =======
  toolButtonMapTheme->setMenu(new QMenu(toolButtonMapTheme));
  QMenu *buttonMenu = toolButtonMapTheme->menu();
  buttonMenu->setToolTipsVisible(true);
  buttonMenu->setTearOffEnabled(true);

  // Theme menu items ===============================
  if(actionGroupMapTheme != nullptr)
  {
    // Delete all actions from the menu
    const QList<QAction *> actions = actionGroupMapTheme->actions();
    for(QAction *action : actions)
    {
      actionGroupMapTheme->removeAction(action);
      delete action;
    }
  }

  delete actionGroupMapTheme;
  actionGroupMapTheme = new QActionGroup(ui->menuViewTheme);
  actionGroupMapTheme->setObjectName("actionGroupMapTheme");
  connect(actionGroupMapTheme, &QActionGroup::triggered, this, &MapThemeHandler::changeMapTheme);

  // Add Projection on top of menu ============
  buttonMenu->addAction(ui->actionMapProjectionMercator);
  buttonMenu->addAction(ui->actionMapProjectionSpherical);
  buttonMenu->addSeparator();

  // Add all found map themes ====================================
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
      buttonMenu->addSeparator();
    }

    // Add item to menu in toolbar
    QString shortName = atools::elideTextShortMiddle(theme.getName(), 24);
    QString name = theme.isOnline() ? shortName : tr("%1 (offline)").arg(shortName);
    if(theme.hasKeys())
      // Add star to maps which require an API key or token
      name += tr(" *");

    // Build tooltip for entries
    QStringList tip;
    tip.append(theme.getName());
    tip.append(theme.isOnline() ? tr("online") : tr("offline"));
    tip.append(theme.hasKeys() ? tr("* requires registration") : tr("free"));

    // Create action for map/theme submenu
    QAction *action = ui->menuViewTheme->addAction(name);
    action->setCheckable(true);
    action->setToolTip(tip.join(tr(", ")));
    action->setStatusTip(action->toolTip());
    action->setActionGroup(actionGroupMapTheme);

    // Attach theme name for theme in MapThemeHandler
    action->setData(theme.getThemeId());
    action->setShortcut(theme.getShortcut());

    buttonMenu->addAction(action);

#ifdef DEBUG_INFORMATION
    qDebug() << Q_FUNC_INFO << name << index;
#endif

    index++;

    // Remember theme online status
    online = theme.isOnline();
  }
}

void MapThemeHandler::changeMapThemeActions(const QString& themeId)
{
  if(!themeId.isEmpty())
  {
    // Search for actions entry with index for MapThemeHandler
    const QList<QAction *> actions = toolButtonMapTheme->menu()->actions();
    for(QAction *action : actions)
    {
      QVariant data = action->data();
      if(data.isValid() && data.toString() == themeId)
        action->setChecked(true);
    }
  }
}

void MapThemeHandler::changeMapTheme()
{
  MapWidget *mapWidget = NavApp::getMapWidgetGui();
  mapWidget->cancelDragAll();

  if(actionGroupMapTheme->checkedAction() == nullptr)
  {
    qWarning() << Q_FUNC_INFO << "checkedAction is null";
    return;
  }

  QString themeId = actionGroupMapTheme->checkedAction()->data().toString();
  MapTheme theme = getTheme(themeId);

  if(!theme.isValid())
  {
    qDebug() << Q_FUNC_INFO << "Falling back to default theme due to invalid index" << themeId;
    // No theme for index found - use default OSM
    theme = getDefaultTheme();
    themeId = theme.getThemeId();
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
                          "<p>Then go to menu \"Tools\" -> \"Options\" and to page \"Map Keys\" in Little Navmap and "
                            "enter the information for the key(s) below:</p>"
                            "<ul><li>%2</li></ul>"
                              "<p>The map will not show correctly until this is done.</p>%3").
                     arg(theme.getName()).arg(theme.getKeys().join(tr("</li><li>"))).arg(url),
                     tr("Do not &show this dialog again."));
    }
  }

  qDebug() << Q_FUNC_INFO << themeId << theme;

  mapWidget->setTheme(theme);
  if(NavApp::getMapPaintWidgetWeb() != nullptr)
    NavApp::getMapPaintWidgetWeb()->setTheme(theme);

  NavApp::setStatusMessage(tr("Map theme changed to %1.").arg(actionGroupMapTheme->checkedAction()->text()));
}

/* Called by actions */
void MapThemeHandler::changeMapProjection()
{
  Ui::MainWindow *ui = NavApp::getMainUi();
  MapWidget *mapWidget = NavApp::getMapWidgetGui();
  mapWidget->cancelDragAll();

  Marble::Projection projection = Marble::Mercator;
  QString projectionText;
  if(ui->actionMapProjectionMercator->isChecked())
  {
    projection = Marble::Mercator;
    projectionText = tr("Mercator");
  }
  else if(ui->actionMapProjectionSpherical->isChecked())
  {
    projection = Marble::Spherical;
    projectionText = tr("Spherical");
  }

  qDebug() << "Changing projection to" << projection;
  mapWidget->setProjection(projection);

  NavApp::setStatusMessage(tr("Map projection changed to %1.").arg(projectionText));
}

void MapThemeHandler::optionsChanged()
{
  qDebug() << Q_FUNC_INFO;

  // Remember current theme id like "openstreetmap"
  QString currentThemeId = getCurrentThemeId();

  // Save key to avoid deletion
  saveKeyfile();

  // Now load all themes from all available folders
  loadThemes();

  // Reload keys and apply them to the themes
  restoreKeyfile();

  // Rebuild menu
  setupMapThemesUi();

  if(!getTheme(currentThemeId).isValid())
  {
    // Assign the default theme if the current one was removed
    currentThemeId = defaultTheme.getThemeId();
    NavApp::getMapWidgetGui()->setTheme(defaultTheme);

    if(NavApp::getMapPaintWidgetWeb() != nullptr)
      NavApp::getMapPaintWidgetWeb()->setTheme(defaultTheme);
  }

  // Check the theme action
  changeMapThemeActions(currentThemeId);
}

QString MapThemeHandler::getStatusTextForDir(const QString& path, bool& error)
{
  QString message = atools::checkDirMsg(path);

  if(message.isEmpty())
  {
    QString stockPath = atools::canonicalFilePath(atools::buildPathNoCase({QApplication::applicationDirPath(), "data", "maps", "earth"}));
    if(atools::canonicalFilePath(path).compare(stockPath, Qt::CaseInsensitive) == 0)
    {
      message = tr("Directory is set to the included stock themes. You have to set a directory outside of the installation.");
      error = true;
    }
    else
    {
      int numThemes = 0;
      QDir dir(path);
      const QFileInfoList themeDirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
      for(const QFileInfo& themeDir : themeDirs)
      {
        if(atools::checkFile(Q_FUNC_INFO, themeDir.absoluteFilePath() % atools::SEP % themeDir.fileName() % ".dgml"))
          numThemes++;
      }

      if(numThemes == 0)
        message = tr("Directory is valid. No map themes found inside.");
      else
        message = tr("Directory is valid. %1 %2 found.").arg(numThemes).arg(numThemes > 1 ? tr("map themes") : tr("map theme"));
      error = false;
    }
  }
  else
    error = true;
  return message;
}

void MapThemeHandler::validateMapThemeDirectories(QWidget *parent)
{
  QStringList msg;

  // Default application dir is required
  msg.append(atools::checkDirMsg(getMapThemeDefaultDir()));

  if(!getMapThemeUserDir().isEmpty())
    // User defined dir is optional
    msg.append(atools::checkDirMsg(getMapThemeUserDir()));

  // Remove empty error messages
  msg.removeAll(QString());

  if(!msg.isEmpty())
    atools::gui::Dialog::warning(parent, tr("Base path(s) for map themes not found.\n%1").arg(msg.join(tr(",\n"))));
}

QString MapThemeHandler::getMapThemeDefaultDir()
{
  return QCoreApplication::applicationDirPath() % atools::SEP % "data" % atools::SEP % "maps" % atools::SEP % "earth";
}

QString MapThemeHandler::getMapThemeUserDir()
{
  return OptionData::instance().getCacheMapThemeDir();
}
