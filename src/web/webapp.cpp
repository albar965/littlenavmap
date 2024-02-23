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

#include "httpserver/httpsessionstore.h"
#include "templateengine/templatecache.h"
#include "httpserver/staticfilecontroller.h"

stefanfrings::TemplateCache *WebApp::templateCache = nullptr;
stefanfrings::HttpSessionStore *WebApp::sessionStore = nullptr;
stefanfrings::StaticFileController *WebApp::staticFileController = nullptr;

QString WebApp::documentRoot;
QString WebApp::htmlExtension = ".html";

void copyKeyValuesFromGroup(QSettings& settings, const QString& group, QHash<QString, QVariant>& toSettings)
{
  settings.beginGroup(group);
  const QStringList childKeys = settings.childKeys();
  for(const QString& childKey : childKeys)
  {
    QString key = childKey.trimmed();
    if(!key.startsWith('#') && !key.startsWith(';') && !key.startsWith("//"))
      toSettings.insert(key, settings.value(childKey));
  }
  settings.endGroup();
}

void WebApp::init(QObject *parent, const QString& configFileName, const QString& docrootParam)
{
  qDebug() << Q_FUNC_INFO << "configFileName" << configFileName << "docrootParam" << docrootParam;

  documentRoot = docrootParam;

  // Copy key from settings groups to hashmaps for modules
  QSettings settings(configFileName, QSettings::IniFormat);
  QHash<QString, QVariant> templateCacheSettings, sessionSettings, staticFileControllerSettings;

  // Configure template loader and cache - copy settings from group to hash
  copyKeyValuesFromGroup(settings, "templates", templateCacheSettings);
  if(!templateCacheSettings.contains("path"))
    templateCacheSettings.insert("path", documentRoot);

  templateCache = new stefanfrings::TemplateCache(templateCacheSettings, parent);
  htmlExtension = templateCacheSettings.value("suffix").toString();

  // Configure session store
  copyKeyValuesFromGroup(settings, "sessions", sessionSettings);
  sessionStore = new stefanfrings::HttpSessionStore(sessionSettings, parent);

  // Configure static file controller
  copyKeyValuesFromGroup(settings, "docroot", staticFileControllerSettings);
  if(!staticFileControllerSettings.contains("path"))
    staticFileControllerSettings.insert("path", docrootParam);

  staticFileController = new stefanfrings::StaticFileController(staticFileControllerSettings, parent);
}

void WebApp::deinit()
{
}
