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

#ifndef LITTLENAVMAP_CONTEXTMENUTOOL_H
#define LITTLENAVMAP_CONTEXTMENUTOOL_H

#include <QCoreApplication>
#include <QAction>

namespace map {
class MapAirport;
}
class Route;

/*
 * Helps building context menus for adding and naming menu items for departure, destination and alternate.
 */
class ContextMenuTool
{
  Q_DECLARE_TR_FUNCTIONS(ContextMenuTool)

public:
  /* Set texts and enable/disable actions for given airport. Object text is airport name or other.
   * Actions have to be set using setActionShowApproaches() before. */
  void initAirportActions(const map::MapAirport& airport, const Route& route, int routeLegIndex, const QString& objectText);

  void setActions(QAction *actionShowDepartureParam, QAction *actionShowApproachParam, QAction *actionShowProceduresParam)
  {
    actionShowProcedures = actionShowProceduresParam;
    actionShowApproach = actionShowApproachParam;
    actionShowDeparture = actionShowDepartureParam;
  }

  /* Returns a list of all suffixes depending on flags. Example: " (is destination, no runway) ...".
   * Ellipse is added only if runways are present (shows dialog for selection. */
  static QString airportItemSuffix(bool airportDeparture, bool airportDestination, bool airportAlternate,
                                   bool airportRoundTrip, bool noRunways, const QStringList& otherSuffixes = QStringList());

private:
  QAction *actionShowProcedures = nullptr, *actionShowApproach = nullptr, *actionShowDeparture = nullptr;
};

#endif //
