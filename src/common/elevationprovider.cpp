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

#include "common/elevationprovider.h"

#include "common/constants.h"
#include "geo/calculations.h"
#include "app/navapp.h"
#include "fs/common/globereader.h"
#include "gui/helphandler.h"
#include "options/optiondata.h"
#include "geo/line.h"
#include "geo/linestring.h"
#include "geo/pos.h"
#include "gui/dialog.h"
#include "atools.h"

#include <marble/GeoDataCoordinates.h>
#include <marble/ElevationModel.h>

#include <QUrl>

/* Limt altitude to this value */
static Q_DECL_CONSTEXPR float ALTITUDE_LIMIT_METER = 8800.f;
/* Point removal equality tolerance in meter */
static Q_DECL_CONSTEXPR float SAME_ONLINE_ELEVATION_EPSILON = 1.f;

using atools::geo::Pos;
using atools::geo::Line;
using atools::geo::LineString;
using atools::fs::common::GlobeReader;

using namespace Marble;

ElevationProvider::ElevationProvider(QObject *parent)
  : QObject(parent)
{
}

ElevationProvider::~ElevationProvider()
{
  delete globeReader;
}

void ElevationProvider::marbleUpdateAvailable()
{
  if(!isGlobeOfflineProvider())
    emit updateAvailable();
}

float ElevationProvider::getElevationMeter(const atools::geo::Pos& pos, float sampleRadiusMeter)
{
  if(isGlobeOfflineProvider())
  {
    QMutexLocker locker(&mutex);
    float elevation = globeReader->getElevation(pos, sampleRadiusMeter);
    if(!(elevation > atools::fs::common::OCEAN && elevation < atools::fs::common::INVALID))
      return 0.f;
    else
      return elevation;
  }
  else
    return 0.f;
}

float ElevationProvider::getElevationFt(const atools::geo::Pos& pos, float sampleRadiusMeter)
{
  return atools::geo::meterToFeet(getElevationMeter(pos, sampleRadiusMeter));
}

void ElevationProvider::getElevations(atools::geo::LineString& elevations, const atools::geo::Line& line, float sampleRadiusMeter)
{
  if(!line.isValid())
    return;

  QMutexLocker locker(&mutex);

  if(isGlobeOfflineProvider())
  {
    globeReader->getElevations(elevations, LineString(line.getPos1(), line.getPos2()), sampleRadiusMeter);
    for(Pos& pos : elevations)
    {
      float alt = pos.getAltitude();
      if(!(alt > atools::fs::common::OCEAN && alt < atools::fs::common::INVALID))
        // Reset all invalid and ocean indicators to 0
        pos.setAltitude(0.f);
    }
  }
  else if(marbleModel != nullptr)
  {
    // Get altitude points for the line segment
    // The might not be complete and will be more complete on further iterations when we get a signal
    // from the elevation model
    const QVector<GeoDataCoordinates> temp = marbleModel->heightProfile(line.getPos1().getLonX(), line.getPos1().getLatY(),
                                                                        line.getPos2().getLonX(), line.getPos2().getLatY());

    // Limit long legs to a maximum of 2000 points - minimum of 1000 points
    int divisor = 1;
    while(temp.size() / divisor > 2000)
      divisor++;

    int i = 0;
    Pos lastDropped;
    for(const GeoDataCoordinates& c : temp)
    {
      if((i++ % divisor) != 0)
        continue;

      Pos pos(c.longitude(), c.latitude(), c.altitude());
      pos.toDeg();

      if(!elevations.isEmpty())
      {
        if(atools::almostEqual(elevations.constLast().getAltitude(), pos.getAltitude(), SAME_ONLINE_ELEVATION_EPSILON))
        {
          // Drop points with similar altitude
          lastDropped = pos;
          continue;
        }
        else if(lastDropped.isValid())
        {
          // Add last point of a stretch with similar altitude
          elevations.append(lastDropped);
          lastDropped = Pos();
        }
      }
      elevations.append(pos);
    }

    if(elevations.isEmpty())
    {
      // Workaround for invalid geometry data - add void
      elevations.append(line.getPos1());
      elevations.append(line.getPos2());
    }
  }

  for(Pos& pos : elevations)
    // Limit ground altitude
    pos.setAltitude(std::min(pos.getAltitude(), ALTITUDE_LIMIT_METER));
}

bool ElevationProvider::isGlobeOfflineProvider() const
{
  return globeReader != nullptr && globeReader->isValid();
}

bool ElevationProvider::isGlobeDirValid()
{
  return isGlobeDirectoryValid(OptionData::instance().getOfflineElevationPath());
}

bool ElevationProvider::isGlobeDirectoryValid(const QString& path)
{
  // Checks for files and more
  return GlobeReader::isDirValid(path);
}

void ElevationProvider::optionsChanged()
{
  updateReader(false /* startup */);
}

void ElevationProvider::init(const Marble::ElevationModel *model)
{
  marbleModel = model;

  // Marble will let us know when updates are available
  if(marbleModel != nullptr)
    connect(marbleModel, &ElevationModel::updateAvailable, this, &ElevationProvider::marbleUpdateAvailable);
  updateReader(true /* startup */);
}

void ElevationProvider::updateReader(bool startup)
{
  bool warnWrongGlobePath = false, warnOpenFiles = false,
       useOffline = OptionData::instance().getFlags().testFlag(opts::CACHE_USE_OFFLINE_ELEVATION);
  const QString& path = OptionData::instance().getOfflineElevationPath();

  {
    // Make sure to wait for other methods to finish before changing the reader
    QMutexLocker locker(&mutex);

    if(useOffline)
    {
      if(!GlobeReader::isDirValid(path))
      {
        warnWrongGlobePath = true;
        delete globeReader;
        globeReader = nullptr;
      }
      else
      {
        delete globeReader;
        globeReader = new GlobeReader(path);

        qDebug() << Q_FUNC_INFO << "Opening GLOBE files";

        if(!globeReader->openFiles())
        {
          delete globeReader;
          globeReader = nullptr;
          warnOpenFiles = true;
        }
        else
          qDebug() << Q_FUNC_INFO << "Opening GLOBE done";
      }
    }
    else
    {
      delete globeReader;
      globeReader = nullptr;
    }
  }

  // Show this warning at startup and when changing options
  if(warnOpenFiles)
  {
    NavApp::closeSplashScreen();
    atools::gui::Dialog::warning(NavApp::getQMainWidget(), tr("Cannot open GLOBE data in directory<br/>\"%1\"").arg(path));
  }

  // Show this warning at startup and when changing options
  if(warnWrongGlobePath)
  {
    // Warn outside the mutex lock to avoid deadlocks
    NavApp::closeSplashScreen();
    atools::gui::Dialog::warning(NavApp::getQMainWidget(),
                                 tr("GLOBE elevation data directory is not valid:<br/>\"%1\"<br/><br/>"
                                    "Go to main menu -&gt; \"Tools\" -&gt; \"Options\" and then<br/>"
                                    "to page \"Cache and Files\". Then click \"Select GLOBE Directory\" and<br/>"
                                    "select the correct place with the GLOBE elevation files.",
                                    "Keep instructions in sync with translated menus").arg(path));
  }

  // Show this only on startup
  if(!useOffline && startup)
  {
    NavApp::closeSplashScreen();
    QUrl url = atools::gui::HelpHandler::getHelpUrlWeb(lnm::helpOnlineInstallGlobeUrl, lnm::helpLanguageOnline());
    QString message = tr(
      "<p>The online elevation data which is used by default for the elevation profile is limited and has a lot of errors.<br/>"
      "Therefore, it is recommended to download and use the offline GLOBE elevation data which provides world wide coverage.</p>"
      "<p>Go to the main menu -&gt; \"Tools\" -&gt; \"Options\" and then to page \"Cache and files\" to add the GLOBE data.</p>"
        "<p><a href=\"%1\">Click here for more information in the Little Navmap online manual</a></p>",
      "Keep instructions in sync with translated menus").arg(url.toString());

    atools::gui::Dialog(NavApp::getQMainWidget()).showInfoMsgBox(lnm::ACTIONS_SHOW_INSTALL_GLOBE, message,
                                                                 tr("Do not &show this dialog again."));
  }

  emit updateAvailable();
}
