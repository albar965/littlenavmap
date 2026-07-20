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

#include "optiondata.h"

#include "common/constants.h"
#include "exception.h"
#include "gui/application.h"
#include "settings/settings.h"

#include <QDebug>
#include <QFont>
#include <QFontDatabase>
#include <QRandomGenerator>
#include <QSettings>
#include <QSize>

QString OptionData::userAgentRandom;

OptionData *OptionData::optionData = nullptr;

const QString OptionData::WEATHER_NOAA_DEFAULT_URL = "https://tgftp.nws.noaa.gov/data/observations/metar/cycles/%1Z.TXT";
const QString OptionData::WEATHER_VATSIM_DEFAULT_URL = "https://metar.vatsim.net/metar.php?id=ALL";
const QString OptionData::WEATHER_IVAO_DEFAULT_URL = "https://api.ivao.aero/v2/airports/all/metar";
const QString OptionData::WEATHER_NOAA_WIND_BASE_DEFAULT_URL = "https://nomads.ncep.noaa.gov/cgi-bin/filter_gfs_1p00.pl";

/* Default values for well known networks */

OptionData::OptionData()
{
  userAgentDefault = QStringLiteral("%1/%2 (+https://www.littlenavmap.org; contact: %3)").
                     arg(QCoreApplication::applicationName(), QCoreApplication::applicationVersion(),
                         atools::gui::Application::getEmailAddresses().constFirst());

  QRandomGenerator random(QDateTime::currentSecsSinceEpoch());
  static QStringList userAgentList({
    QStringLiteral("Mozilla/5.0 (compatible; Marble/%1; DesktopDevice; Browser; QNamNetworkPlugin; %2)"),
    QStringLiteral("Lynx/2.8.1pre.9 libwww-FM/2.14"),
    QStringLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:146.0) Gecko/20100101 Thunderbird/146.0.1"),
    QStringLiteral("Opera/9.80 (Macintosh; Intel Mac OS X 10.14.1) Presto/2.12.388 Version/12.16"),
    QStringLiteral("Java 1.6.0_26"),
    QStringLiteral("curl/7.9.8 (i686-pc-linux-gnu) libcurl 7.9.8 (OpenSSL 0.9.6b) (ipv6 enabled)"),
    QStringLiteral("wii libnup/1.0")
  });

  int index = random.bounded(0, userAgentList.size() - 1);

  if(index == 0)
  {
    QStringList name({"Marble Virtual Globe", "marble"});
    userAgentRandom = userAgentList.at(0).arg(QStringLiteral("%1.%2.%3").
                                              arg(QString::number(random.bounded(22, 25)), QString::number(random.bounded(0, 9)),
                                                  QString::number(random.bounded(0, 9))), name.at(random.bounded(0, name.size() - 1)));
  }
  else
    userAgentRandom = userAgentList.at(index);
}

QString OptionData::getLanguageFromConfigFile()
{
  return atools::settings::Settings::instance().valueStr(lnm::OPTIONS_DIALOG_LANGUAGE);
}

void OptionData::saveLanguageToConfigFile(const QString& language)
{
  atools::settings::Settings::instance().setValue(lnm::OPTIONS_DIALOG_LANGUAGE, language);
}

const QString& OptionData::getUserAgent() const
{
  if(userAgent == QStringLiteral("RANDOM"))
    return userAgentRandom;
  else if(userAgent.isEmpty())
    return userAgentDefault;

  return userAgent;
}

opts::OnlineFormat OptionData::getOnlineFormat() const
{
  switch(onlineNetwork)
  {
    case opts::ONLINE_CUSTOM:
    case opts::ONLINE_CUSTOM_STATUS:
    case opts::ONLINE_NONE:
      return onlineFormat;

    case opts::ONLINE_VATSIM:
      return opts::ONLINE_FORMAT_VATSIM_JSON;

    case opts::ONLINE_IVAO:
      return opts::ONLINE_FORMAT_IVAO_JSON;

    case opts::ONLINE_PILOTEDGE:
      return opts::ONLINE_FORMAT_VATSIM;
  }
  return opts::ONLINE_FORMAT_VATSIM;
}

QString OptionData::getOnlineStatusUrl() const
{
  switch(onlineNetwork)
  {
    case opts::ONLINE_CUSTOM:
    case opts::ONLINE_NONE:
    case opts::ONLINE_IVAO:
      return QStringLiteral();

    case opts::ONLINE_VATSIM:
      return onlineVatsimStatusUrl;

    case opts::ONLINE_PILOTEDGE:
      return onlinePilotEdgeStatusUrl;

    case opts::ONLINE_CUSTOM_STATUS:
      return onlineStatusUrl;
  }
  return QStringLiteral();
}

QString OptionData::getOnlineTransceiverUrl() const
{
  switch(onlineNetwork)
  {
    case opts::ONLINE_CUSTOM:
    case opts::ONLINE_NONE:
    case opts::ONLINE_IVAO:
    case opts::ONLINE_PILOTEDGE:
    case opts::ONLINE_CUSTOM_STATUS:
      return QStringLiteral();

    case opts::ONLINE_VATSIM:
      return onlineVatsimTransceiverUrl;
  }
  return QStringLiteral();
}

QString OptionData::getOnlineWhazzupUrl() const
{
  switch(onlineNetwork)
  {
    case opts::ONLINE_CUSTOM:
      return onlineWhazzupUrl;

    case opts::ONLINE_IVAO:
      return onlineIvaoWhazzupUrl;

    case opts::ONLINE_NONE:
    case opts::ONLINE_VATSIM:
    case opts::ONLINE_PILOTEDGE:
    case opts::ONLINE_CUSTOM_STATUS:
      return QStringLiteral();
  }
  return QStringLiteral();
}

const QFont OptionData::getMapFont() const
{
  return bestFont(mapFont, guiFont, QFontDatabase::systemFont(QFontDatabase::GeneralFont));
}

const QFont OptionData::getProfileFont() const
{
  return bestFont(profileFont, guiFont, QFontDatabase::systemFont(QFontDatabase::GeneralFont));
}

const QFont OptionData::getGuiFont() const
{
  QFont font;
  if(!guiFont.isEmpty())
    font.fromString(guiFont);
  else
    font = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
  return font;
}

int OptionData::getOnlineReload(opts::OnlineNetwork network) const
{
  switch(network)
  {
    case opts::ONLINE_VATSIM:
      return onlineVatsimReload;

    case opts::ONLINE_IVAO:
      return onlineIvaoReload;

    case opts::ONLINE_PILOTEDGE:
      return onlinePilotEdgeReload;

    case opts::ONLINE_CUSTOM_STATUS:
    case opts::ONLINE_CUSTOM:
      return onlineCustomReload;

    case opts::ONLINE_NONE:
      break;
  }
  return 180;
}

const QSize OptionData::getGuiToolbarSize() const
{
  return QSize(guiToolbarSize, guiToolbarSize);
}

QFont OptionData::bestFont(const QString& fontStr, const QString& guiFontStr, const QFont& fallback)
{
  QFont font;
  if(!fontStr.isEmpty())
    // Use set font
    font.fromString(fontStr);
  else if(!guiFontStr.isEmpty())
    // Fall back to GUI font
    font.fromString(guiFontStr);
  else
    // Fall back to system font
    font = fallback;

  if(fallback.bold())
    font.setBold(true);

  if(fallback.italic())
    font.setItalic(true);

  return font;
}

const OptionData& OptionData::instance()
{
  OptionData& optData = instanceInternal();

  if(!optData.valid)
  {
    qCritical() << "OptionData not initialized yet";
    throw atools::Exception("OptionData not initialized yet");
  }

  return optData;
}

OptionData& OptionData::instanceInternal()
{
  if(optionData == nullptr)
  {
    qDebug() << "Creating new OptionData";
    optionData = new OptionData();
  }

  return *optionData;
}
