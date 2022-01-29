/*****************************************************************************
* Copyright 2015-2022 Alexander Barthel alex@littlenavmap.org
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

#ifndef LNM_SIMBRIEFHANDLER_H
#define LNM_SIMBRIEFHANDLER_H

#include <QObject>

class Route;
class MainWindow;

/*
 * Send flight plan to SimBrief and fetch and parse flight plans from SimBrief.
 */
class SimBriefHandler :
  public QObject
{
  Q_OBJECT

public:
  explicit SimBriefHandler(MainWindow *parent);
  virtual ~SimBriefHandler() override;

  /* Send current flight plan as route description string to SimBrief in web browser after question dialog. */
  void sendRouteToSimBrief();

  /* Opens FetchRouteDialog to download plan from SimBrief */
  void fetchRouteFromSimBrief();

private:
  QUrl sendRouteUrl(const QString& departure, const QString& destination, const QString& alternate, const QString& route,
                    const QString& aircraftType, float cruisingAltitudeFeet);

  MainWindow *mainWindow;
  QString dispatchUrl; /* URL can be overridden in settings */
};

#endif // LNM_SIMBRIEFHANDLER_H
