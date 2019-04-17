/*****************************************************************************
* Copyright 2015-2019 Alexander Barthel alex@littlenavmap.org
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

QSettings *WebApp::templateCacheSettings = nullptr;
QSettings *WebApp::sessionSettings = nullptr;
QSettings *WebApp::staticFileControllerSettings = nullptr;

QString WebApp::documentRoot;
QString WebApp::htmlExtension = ".html";

void WebApp::init(QObject *parent, const QString& configFileName, const QString& docrootParam)
{
  qDebug() << Q_FUNC_INFO;
  documentRoot = docrootParam;

  // Configure template loader and cache
  templateCacheSettings = new QSettings(configFileName, QSettings::IniFormat, parent);
  templateCacheSettings->beginGroup("templates");
  templateCacheSettings->setValue("path", docrootParam);
  templateCache = new stefanfrings::TemplateCache(templateCacheSettings, parent);
  htmlExtension = templateCacheSettings->value("suffix").toString();

  // Configure session store
  sessionSettings = new QSettings(configFileName, QSettings::IniFormat, parent);
  sessionSettings->beginGroup("sessions");
  sessionStore = new stefanfrings::HttpSessionStore(sessionSettings, parent);

  // Configure static file controller
  staticFileControllerSettings = new QSettings(configFileName, QSettings::IniFormat, parent);
  staticFileControllerSettings->beginGroup("docroot");
  staticFileControllerSettings->setValue("path", docrootParam);
  staticFileController = new stefanfrings::StaticFileController(staticFileControllerSettings, parent);

}

void WebApp::deinit()
{
  qDebug() << Q_FUNC_INFO;

  delete templateCacheSettings;
  templateCacheSettings = nullptr;

  delete sessionSettings;
  sessionSettings = nullptr;

  delete staticFileControllerSettings;
  staticFileControllerSettings = nullptr;
}
