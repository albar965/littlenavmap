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

#include "webapp.h"

#include <QSettings>

#include "atools.h"
#include "httpserver/httpsessionstore.h"
#include "templateengine/templatecache.h"
#include "httpserver/staticfilecontroller.h"

stefanfrings::TemplateCache *WebApp::templateCache = nullptr;
stefanfrings::HttpSessionStore *WebApp::sessionStore = nullptr;
stefanfrings::StaticFileController *WebApp::staticFileController = nullptr;

QSettings *WebApp::sessionSettings = nullptr;
QSettings *WebApp::staticFileControllerSettings = nullptr;

QString WebApp::documentRoot;
QString WebApp::htmlExtension = ".html";

void copyKeyValuesFromGroup(QSettings& settings, const QString& group, QSettings *toSettings)
{
  settings.beginGroup(group);
  const QStringList keys = settings.childKeys();
  for(const QString& key : keys)
    toSettings->setValue(key, settings.value(key));
  settings.endGroup();
}

void WebApp::init(QObject *parent, const QString& configFileName, const QString& docrootParam)
{
  qDebug() << Q_FUNC_INFO;
  documentRoot = docrootParam;

  QSettings settings(configFileName, QSettings::IniFormat);

  // Configure template loader and cache - copy settings from group to hash
  QHash<QString, QVariant> templateCacheSettings;
  settings.beginGroup("templates");
  const QStringList keys = settings.childKeys();
  for(const QString& key : keys)
    templateCacheSettings.insert(key, settings.value(key).toString());
  settings.endGroup();

  if(!templateCacheSettings.contains("path"))
    templateCacheSettings.insert("path", documentRoot);
  templateCacheSettings.insert("filename", configFileName);

  // Cache creates a copy of the settings
  templateCache = new stefanfrings::TemplateCache(templateCacheSettings, parent);
  htmlExtension = templateCacheSettings.value("suffix").toString();

  // Configure session store
  sessionSettings = new QSettings();
  copyKeyValuesFromGroup(settings, "sessions", sessionSettings);
  sessionSettings->setValue("filename", configFileName);

  // Session store gets a reference to the settings
  sessionStore = new stefanfrings::HttpSessionStore(sessionSettings, parent);

  // Configure static file controller
  staticFileControllerSettings = new QSettings();
  copyKeyValuesFromGroup(settings, "docroot", staticFileControllerSettings);
  if(!staticFileControllerSettings->contains("path"))
    staticFileControllerSettings->setValue("path", docrootParam);
  staticFileControllerSettings->setValue("filename", configFileName);

  // File controller gets a reference to the settings
  staticFileController = new stefanfrings::StaticFileController(staticFileControllerSettings, parent);
}

void WebApp::deinit()
{
  ATOOLS_DELETE_LOG(sessionSettings);
  ATOOLS_DELETE_LOG(staticFileControllerSettings);
}
