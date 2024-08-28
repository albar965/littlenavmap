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

#include "common/constants.h"

#include "atools.h"
#include "gui/helphandler.h"
#include "options/optiondata.h"
#include "settings/settings.h"

#include <QDir>
#include <QRegularExpression>
#include <QSettings>
#include <QDebug>
#include <QCoreApplication>
#include <QUrl>
#include <QStringBuilder>

namespace lnm {

static QString supportedLanguageOnline;

QString helpOnlineUrl;
QString helpOnlineMainUrl;
QString helpOnlineTutorialsUrl;
QString helpOnlineDownloadsUrl;
QString helpOnlineMainMenuUrl;
QString helpOnlineShortcutsUrl;
QString helpOnlineMapDisplayUrl;
QString helpOnlineLegendUrl;
QString helpOnlineFlightPlanningUrl;
QString helpOnlineAircraftPerfUrl;
QString helpOnlineUserInterfaceUrl;

QString helpOnlineInstallGlobeUrl;
QString helpOnlineInstallDirUrl;
QString helpOnlineNavdatabasesUrl;
QString helpOnlineStartUrl;
QString helpOfflineFile;
QString helpDonateUrl;
QString helpManualDownloadUrl;
QString helpFaqUrl;
QString updateDefaultUrl;

const QSize DEFAULT_MAINWINDOW_SIZE(1360, 800);

const QString helpLanguageOnline()
{
  const static QRegularExpression INDICATOR_FILE("little-navmap-user-manual-(.+)\\.online", QRegularExpression::CaseInsensitiveOption);

  if(supportedLanguageOnline.isEmpty())
  {
    // Get the online indicator file
    QString onlineFlagFile = atools::gui::HelpHandler::getHelpFile(
      QLatin1String("help") % atools::SEP % "little-navmap-user-manual-${LANG}.online", OptionData::getLanguageFromConfigFile());

    // Extract language from the file
    QRegularExpressionMatch match = INDICATOR_FILE.match(onlineFlagFile);
    if(match.hasMatch() && !match.captured(1).isEmpty())
      supportedLanguageOnline = match.captured(1);

    // Fall back to English
    if(supportedLanguageOnline.isEmpty())
      supportedLanguageOnline = "en";
  }

  return supportedLanguageOnline;
}

void loadHelpUrls()
{
  QString urlsPath = atools::settings::Settings::getOverloadedPath(lnm::URLS_CONFIG);
  qInfo() << Q_FUNC_INFO << "Loading urls.cfg from" << urlsPath;

  QSettings settings(urlsPath, QSettings::IniFormat);

  QString lang = QLocale().name();

  qDebug() << Q_FUNC_INFO << lang;

  if(!OptionData::getLanguageFromConfigFile().isEmpty())
    lang = OptionData::getLanguageFromConfigFile().section('_', 0, 0);

  // .../help/en/index.html
  QFileInfo localFile(QCoreApplication::applicationDirPath() % atools::SEP % "help" % atools::SEP % lang % atools::SEP % "index.html");

  if(!localFile.exists())
    // Try English manual
    localFile.setFile(QCoreApplication::applicationDirPath() % atools::SEP % "help" % atools::SEP % "en" % atools::SEP % "index.html");

  // Check if local index.html exists
  if(localFile.exists())
  {
    // Use local files for manual
    QString base = QUrl::fromLocalFile(localFile.path()).toString() % "/";
    helpOnlineUrl = base;
    helpOnlineMainUrl = base % "index.html";
    helpOnlineMainMenuUrl = base % "MENUS.html";
    helpOnlineShortcutsUrl = base % "SHORTCUTS.html";
    helpOnlineMapDisplayUrl = base % "MAPDISPLAY.html";
    helpOnlineTutorialsUrl = base % "TUTORIALS.html";
    helpOnlineLegendUrl = base % "LEGEND.html";
    helpOnlineFlightPlanningUrl = base % "FLIGHTPLAN.html";
    helpOnlineAircraftPerfUrl = base % "AIRCRAFTPERF.html";
    helpOnlineUserInterfaceUrl = base % "INTRO.html";
    helpOnlineInstallGlobeUrl = base % "GLOBE.html";
    helpOnlineInstallDirUrl = base % "FOLDERS.html";
    helpOnlineNavdatabasesUrl = base % "NAVDATA.html";
    helpOnlineStartUrl = base % "START.html";
  }
  else
  {
    // Use online help links from configuration
    // [help] - Online help URLs
    QString base = "https://www.littlenavmap.org/manuals/littlenavmap/release/latest/${LANG}/";
    helpOnlineUrl = settings.value("help/base", base).toString();
    helpOnlineMainUrl = helpOnlineUrl;
    helpOnlineTutorialsUrl = settings.value("help/tutorials", base + "TUTORIALS.html").toString();
    helpOnlineMainMenuUrl = settings.value("help/mainmenu", base + "MENUS.html").toString();
    helpOnlineShortcutsUrl = settings.value("help/shortcuts", base + "SHORTCUTS.html").toString();
    helpOnlineMapDisplayUrl = settings.value("help/mapdisplay", base + "MAPDISPLAY.html").toString();
    helpOnlineLegendUrl = settings.value("help/legend", base + "LEGEND.html").toString();
    helpOnlineFlightPlanningUrl = settings.value("help/flightplanning", base + "FLIGHTPLAN.html").toString();
    helpOnlineAircraftPerfUrl = settings.value("help/aircraftperf", base + "AIRCRAFTPERF.html").toString();
    helpOnlineUserInterfaceUrl = settings.value("help/userinterface", base + "INTRO.html").toString();
    helpOnlineInstallGlobeUrl = settings.value("help/installglobe", base + "GLOBE.html").toString();
    helpOnlineInstallDirUrl = settings.value("help/installdir", base + "FOLDERS.html").toString();
    helpOnlineNavdatabasesUrl = settings.value("help/navdata", base + "NAVDATA.html").toString();
    helpOnlineStartUrl = settings.value("help/start", base + "START.html").toString();
  }

  qDebug() << Q_FUNC_INFO << "Help URL" << helpOnlineUrl;

  helpOnlineDownloadsUrl = settings.value("help/downloads", "https://www.littlenavmap.org/downloads/").toString();

  // [help] - Other URLs
  helpDonateUrl = settings.value("help/donate", "https://www.littlenavmap.org/donate.html").toString();
  helpManualDownloadUrl = settings.value("help/downloadmanual", "https://albar965.github.io/manuals.html").toString();
  helpFaqUrl = settings.value("help/faq", "https://www.littlenavmap.org/littlenavmap-faq.html").toString();

  // [local] - local files
  helpOfflineFile = settings.value("local/help", "help/little-navmap-user-manual-${LANG}.pdf").toString();

  // [update] - Update control file
  updateDefaultUrl = settings.value("update/url", "https://www.littlenavmap.org/littlenavmap-version").toString();
}

} // namespace lnm
