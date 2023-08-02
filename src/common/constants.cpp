/*****************************************************************************
* Copyright 2015-2023 Alexander Barthel alex@littlenavmap.org
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
QString helpOnlineLegendUrl;
QString helpOnlineInstallRedistUrl;
QString helpOnlineInstallGlobeUrl;
QString helpOnlineInstallDirUrl;
QString helpOnlineNavdatabasesUrl;
QString helpOnlineStartUrl;
QString helpLegendLocalFile;
QString helpOfflineFile;
QString helpDonateUrl;
QString helpFaqUrl;
QString updateDefaultUrl;

const QSize DEFAULT_MAINWINDOW_SIZE(1280, 800);

const QString helpLanguageOnline()
{
  const static QRegularExpression INDICATOR_FILE("little-navmap-user-manual-(.+)\\.online",
                                                 QRegularExpression::CaseInsensitiveOption);

  if(supportedLanguageOnline.isEmpty())
  {
    // Get the online indicator file
    QString onlineFlagFile = atools::gui::HelpHandler::getHelpFile(
      QLatin1String("help") % atools::SEP % "little-navmap-user-manual-${LANG}.online",
      OptionData::instance().getLanguage());

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

  QSettings optionsettings(atools::settings::Settings::getFilename(), QSettings::IniFormat);

  QString guiLanguage = optionsettings.value(lnm::OPTIONS_DIALOG_LANGUAGE, QLocale().name()).toString();
  guiLanguage = guiLanguage.section('_', 0, 0);

  // .../help/en/index.html
  QFileInfo localFile(QCoreApplication::applicationDirPath() % atools::SEP % "help" % atools::SEP %
                      guiLanguage % atools::SEP % "index.html");

  if(!localFile.exists())
    // Try English manual
    localFile.setFile(QCoreApplication::applicationDirPath() % atools::SEP % "help" % atools::SEP %
                      "en" % atools::SEP % "index.html");

  // Check if local index.html exists
  if(localFile.exists())
  {
    // Use local files for manual
    QString base = QUrl::fromLocalFile(localFile.path()).toString() % "/";
    helpOnlineUrl = base;
    helpOnlineMainUrl = base % "index.html";

    helpOnlineTutorialsUrl = base % "TUTORIALS.html";
    helpOnlineLegendUrl = base % "LEGEND.html";
    helpOnlineInstallRedistUrl = base % "INSTALLATION.html#windows";
    helpOnlineInstallGlobeUrl = base % "OPTIONS.html#cache-elevation";
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
    helpOnlineLegendUrl = settings.value("help/legend", base + "LEGEND.html").toString();
    helpOnlineInstallRedistUrl = settings.value("help/installredist", base + "INSTALLATION.html#windows").toString();
    helpOnlineInstallGlobeUrl = settings.value("help/installglobe", base + "OPTIONS.html#cache-elevation").toString();
    helpOnlineInstallDirUrl = settings.value("help/installdir", base + "FOLDERS.html").toString();
    helpOnlineNavdatabasesUrl = settings.value("help/navdata", base + "NAVDATA.html").toString();
    helpOnlineStartUrl = settings.value("help/start", base + "START.html").toString();
  }

  qDebug() << Q_FUNC_INFO << "Help URL" << helpOnlineUrl;

  helpOnlineDownloadsUrl = settings.value("help/downloads", "https://www.littlenavmap.org/downloads/").toString();

  // [help] - Other URLs
  helpDonateUrl = settings.value("help/donate", "https://www.littlenavmap.org/donate.html").toString();
  helpFaqUrl = settings.value("help/faq", "https://www.littlenavmap.org/littlenavmap-faq.html").toString();

  // [local] - local files
  helpLegendLocalFile = settings.value("local/legend", "help/legend-${LANG}.html").toString();
  helpOfflineFile = settings.value("local/help", "help/little-navmap-user-manual-${LANG}.pdf").toString();

  // [update] - Update control file
  updateDefaultUrl = settings.value("update/url", "https://www.littlenavmap.org/littlenavmap-version").toString();
}

} // namespace lnm
