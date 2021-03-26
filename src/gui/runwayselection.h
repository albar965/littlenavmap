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

#ifndef LNM_RUNWAYSELECTION_H
#define LNM_RUNWAYSELECTION_H

#include "common/maptypes.h"

#include <QObject>

class QLabel;
class QTableWidget;

namespace atools {
namespace gui {
class ItemViewZoomHandler;
}
}

/*
 * Provides methods to fill a table widget with runway information from an airport and to select a runway.
 */
class RunwaySelection :
  public QObject
{
  Q_OBJECT

public:
  /* The table should allow only single row selection */
  explicit RunwaySelection(QObject *parent, const map::MapAirport& mapAirport, QTableWidget *runwayTableParam);
  virtual ~RunwaySelection() override;

  /* Call to restore settings and to load runway information into the widget */
  void restoreState();

  /* Optional label widget for airport information */
  void setAirportLabel(QLabel *value)
  {
    airportLabel = value;
  }

  /* Get currently selected runway and runway end */
  void getCurrentSelected(map::MapRunway& runway, map::MapRunwayEnd& end) const;

  /* Get airport as passed in into the constructor */
  const map::MapAirport& getAirport() const
  {
    return airport;
  }

  /* Set to true to show pattern altitude in the list. Otherwise omitted */
  void setShowPattern(bool value)
  {
    showPattern = value;
  }

signals:
  /* Selection in the table has changed. */
  void itemSelectionChanged();

  /* An entry was double clicked in the table */
  void doubleClicked();

private:
  void fillAirportLabel();
  void fillRunwayList();
  void addItem(const map::MapRunway& rw, int index, bool primary);

  /* List of runways. Index is attached to table item in the first column */
  QList<map::MapRunway> runways;
  map::MapAirport airport;
  bool showPattern = false;

  QLabel *airportLabel = nullptr;
  QTableWidget *runwayTable = nullptr;
  atools::gui::ItemViewZoomHandler *zoomHandler = nullptr;
};

#endif // LNM_RUNWAYSELECTION_H
