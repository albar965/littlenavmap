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

#ifndef LNM_WEBAPP_H
#define LNM_WEBAPP_H

#include "io/inireader.h"

namespace stefanfrings {
class TemplateCache;
class HttpSessionStore;
class HttpListener;
class StaticFileController;
}

class QSettings;
class QString;
class QObject;

/*
 * Keeps global settings and caches for listener and file controllers and handlers.
 * Call init before use.
 */
class WebApp
{
public:
  static void init(QObject *parent, const QString& configFileName, const QString& docrootParam);
  static void deinit();

  static stefanfrings::TemplateCache *getTemplateCache()
  {
    return templateCache;
  }

  static stefanfrings::HttpSessionStore *getSessionStore()
  {
    return sessionStore;
  }

  static stefanfrings::StaticFileController *getStaticFileController()
  {
    return staticFileController;
  }

  static const QString& getDocroot()
  {
    return documentRoot;
  }

  static const QString& getHtmlExtension()
  {
    return htmlExtension;
  }

private:
  WebApp()
  {
  }

  /* Cache for template files */
  static stefanfrings::TemplateCache *templateCache;

  /* Storage for session cookies */
  static stefanfrings::HttpSessionStore *sessionStore;

  /* Controller for static files */
  static stefanfrings::StaticFileController *staticFileController;

  static atools::io::IniKeyValues templateCacheSettings, sessionSettings, staticFileControllerSettings;

  static QString documentRoot, htmlExtension;
};

#endif // LNM_WEBAPP_H
