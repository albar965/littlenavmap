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

#include "optiondata.h"

#include "exception.h"

#include <QDebug>
#include <QFont>
#include <QFontDatabase>
#include <QSize>

OptionData *OptionData::optionData = nullptr;

const QString OptionData::WEATHER_NOAA_DEFAULT_URL = "https://tgftp.nws.noaa.gov/data/observations/metar/cycles/%1Z.TXT";
const QString OptionData::WEATHER_VATSIM_DEFAULT_URL = "https://metar.vatsim.net/metar.php?id=ALL";
const QString OptionData::WEATHER_IVAO_DEFAULT_URL = "https://api.ivao.aero/v2/airports/all/metar";
const QString OptionData::WEATHER_NOAA_WIND_BASE_DEFAULT_URL = "https://nomads.ncep.noaa.gov/cgi-bin/filter_gfs_1p00.pl";

/* Default values for well known networks */

OptionData::OptionData()
{

}

OptionData::~OptionData()
{

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
      return QString();

    case opts::ONLINE_VATSIM:
      return onlineVatsimStatusUrl;

    case opts::ONLINE_PILOTEDGE:
      return onlinePilotEdgeStatusUrl;

    case opts::ONLINE_CUSTOM_STATUS:
      return onlineStatusUrl;
  }
  return QString();
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
      return QString();

    case opts::ONLINE_VATSIM:
      return onlineVatsimTransceiverUrl;
  }
  return QString();
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
      return QString();
  }
  return QString();
}

const QFont OptionData::getMapFont() const
{
  QFont font;
  if(!mapFont.isEmpty())
    font.fromString(mapFont);
  else if(!guiFont.isEmpty())
    font.fromString(guiFont);
  else
    font = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
  return font;
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
