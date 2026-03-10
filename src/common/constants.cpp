/*****************************************************************************
* Copyright 2015-2026 Alexander Barthel alex@littlenavmap.org
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
  const static QRegularExpression INDICATOR_FILE(QStringLiteral("little-navmap-user-manual-(.+)\\.online"),
                                                 QRegularExpression::CaseInsensitiveOption);

  if(supportedLanguageOnline.isEmpty())
  {
    // Get the online indicator file
    QString onlineFlagFile = atools::gui::HelpHandler::getHelpFile(
      QStringLiteral("help") % atools::SEP % QStringLiteral("little-navmap-user-manual-${LANG}.online"),
      OptionData::getLanguageFromConfigFile());

    // Extract language from the file
    QRegularExpressionMatch match = INDICATOR_FILE.match(onlineFlagFile);
    if(match.hasMatch() && !match.captured(1).isEmpty())
      supportedLanguageOnline = match.captured(1);

    // Fall back to English
    if(supportedLanguageOnline.isEmpty())
      supportedLanguageOnline = QStringLiteral("en");
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
  QFileInfo localFile(QCoreApplication::applicationDirPath() % atools::SEP % QStringLiteral("help") % atools::SEP % lang %
                      atools::SEP % QStringLiteral("index.html"));

  if(!localFile.exists())
    // Try English manual
    localFile.setFile(QCoreApplication::applicationDirPath() % atools::SEP % QStringLiteral("help") % atools::SEP % QStringLiteral("en") %
                      atools::SEP % QStringLiteral("index.html"));

  // Check if local index.html exists
  if(localFile.exists())
  {
    // Use local files for manual
    QString base = QUrl::fromLocalFile(localFile.path()).toString() % QStringLiteral("/");
    helpOnlineUrl = base;
    helpOnlineMainUrl = base % QStringLiteral("index.html");
    helpOnlineMainMenuUrl = base % QStringLiteral("MENUS.html");
    helpOnlineShortcutsUrl = base % QStringLiteral("SHORTCUTS.html");
    helpOnlineMapDisplayUrl = base % QStringLiteral("MAPDISPLAY.html");
    helpOnlineTutorialsUrl = base % QStringLiteral("TUTORIALS.html");
    helpOnlineLegendUrl = base % QStringLiteral("LEGEND.html");
    helpOnlineFlightPlanningUrl = base % QStringLiteral("FLIGHTPLAN.html");
    helpOnlineAircraftPerfUrl = base % QStringLiteral("AIRCRAFTPERF.html");
    helpOnlineUserInterfaceUrl = base % QStringLiteral("INTRO.html");
    helpOnlineInstallGlobeUrl = base % QStringLiteral("GLOBE.html");
    helpOnlineInstallDirUrl = base % QStringLiteral("FOLDERS.html");
    helpOnlineNavdatabasesUrl = base % QStringLiteral("NAVDATA.html");
    helpOnlineStartUrl = base % QStringLiteral("START.html");
  }
  else
  {
    // Use online help links from configuration
    // [help] - Online help URLs
    const QString base = QStringLiteral("https://www.littlenavmap.org/manuals/littlenavmap/release/latest/${LANG}/");
    helpOnlineUrl = settings.value(QStringLiteral("help/base"), base).toString();
    helpOnlineMainUrl = helpOnlineUrl;
    helpOnlineTutorialsUrl = settings.value(QStringLiteral("help/tutorials"), base + QStringLiteral("TUTORIALS.html")).toString();
    helpOnlineMainMenuUrl = settings.value(QStringLiteral("help/mainmenu"), base + QStringLiteral("MENUS.html")).toString();
    helpOnlineShortcutsUrl = settings.value(QStringLiteral("help/shortcuts"), base + QStringLiteral("SHORTCUTS.html")).toString();
    helpOnlineMapDisplayUrl = settings.value(QStringLiteral("help/mapdisplay"), base + QStringLiteral("MAPDISPLAY.html")).toString();
    helpOnlineLegendUrl = settings.value(QStringLiteral("help/legend"), base + QStringLiteral("LEGEND.html")).toString();
    helpOnlineFlightPlanningUrl = settings.value(QStringLiteral("help/flightplanning"),
                                                 base + QStringLiteral("FLIGHTPLAN.html")).toString();
    helpOnlineAircraftPerfUrl = settings.value(QStringLiteral("help/aircraftperf"), base + QStringLiteral("AIRCRAFTPERF.html")).toString();
    helpOnlineUserInterfaceUrl = settings.value(QStringLiteral("help/userinterface"), base + QStringLiteral("INTRO.html")).toString();
    helpOnlineInstallGlobeUrl = settings.value(QStringLiteral("help/installglobe"), base + QStringLiteral("GLOBE.html")).toString();
    helpOnlineInstallDirUrl = settings.value(QStringLiteral("help/installdir"), base + QStringLiteral("FOLDERS.html")).toString();
    helpOnlineNavdatabasesUrl = settings.value(QStringLiteral("help/navdata"), base + QStringLiteral("NAVDATA.html")).toString();
    helpOnlineStartUrl = settings.value(QStringLiteral("help/start"), base + QStringLiteral("START.html")).toString();
  }

  qDebug() << Q_FUNC_INFO << "Help URL" << helpOnlineUrl;

  helpOnlineDownloadsUrl = settings.value(QStringLiteral("help/downloads"),
                                          QStringLiteral("https://www.littlenavmap.org/downloads/")).toString();

  // [help] - Other URLs
  helpDonateUrl = settings.value(QStringLiteral("help/donate"), QStringLiteral("https://www.littlenavmap.org/donate.html")).toString();
  helpManualDownloadUrl = settings.value(QStringLiteral("help/downloadmanual"),
                                         QStringLiteral("https://albar965.github.io/manuals.html")).toString();
  helpFaqUrl = settings.value(QStringLiteral("help/faq"), QStringLiteral("https://www.littlenavmap.org/littlenavmap-faq.html")).toString();

  // [local] - local files
  helpOfflineFile = settings.value(QStringLiteral("local/help"), QStringLiteral("help/little-navmap-user-manual-${LANG}.pdf")).toString();

  // [update] - Update control file
  updateDefaultUrl = settings.value(QStringLiteral("update/url"),
                                    QStringLiteral("https://www.littlenavmap.org/littlenavmap-version")).toString();
}

} // namespace lnm
