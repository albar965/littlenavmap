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

#ifndef LITTLENAVMAP_AIRPORTSEARCH_H
#define LITTLENAVMAP_AIRPORTSEARCH_H

#include "search/searchbasetable.h"
#include "search/randomdepartureairportpickingbycriteria.h"

class Column;
class AirportIconDelegate;
class QAction;
class UnitStringTool;
class QueryWidget;

namespace atools {
namespace sql {
class SqlDatabase;
}
}

class QProgressDialog;

/*
 * Airport search tab including all search widgets and the result table view.
 */
class AirportSearch :
  public SearchBaseTable
{
  Q_OBJECT

public:
  explicit AirportSearch(QMainWindow *parent, QTableView *tableView, si::TabSearchId tabWidgetIndex);
  virtual ~AirportSearch() override;

  AirportSearch(const AirportSearch& other) = delete;
  AirportSearch& operator=(const AirportSearch& other) = delete;

  /* All state saving is done through the widget state */
  virtual void saveState() override;
  virtual void restoreState() override;

  virtual void getSelectedMapObjects(map::MapResult& result) const override;
  virtual void connectSearchSlots() override;
  virtual void postDatabaseLoad() override;
  virtual void resetSearch() override;

public slots:
  void randomFlightSearchProgressing();
  void dataRandomAirportsReceived(bool isSuccess, int indexDeparture, int indexDestination,
                                  QVector<std::pair<int, atools::geo::Pos> > *data);

private:
  virtual void updateButtonMenu() override;
  virtual void saveViewState(bool distanceSearchState) override;
  virtual void restoreViewState(bool distanceSearchState) override;
  virtual void updatePushButtons() override;
  QAction *followModeAction() override;

  /* Options dialog has changed some options */
  virtual void optionsChanged() override;

  void setCallbacks();
  QVariant modelDataHandler(int colIndex, int rowIndex, const Column *col, const QVariant&,
                            const QVariant& displayRoleValue, Qt::ItemDataRole role) const;
  QString formatModelData(const Column *col, const QVariant& displayRoleValue) const;
  void overrideMode(const QStringList& overrideColumnTitles);

  /*
   * Random Flight Generator (RFG)
   * (a generator for a flight plan of a single non-stop flight from
   *  a random* departure to a random* destination airport)
   * *either the departure or the destination airport can be non-random
   *  optionally
   * A range for the distance between departure and destination airport
   * can be given.
   */

  /* RFG push button clicked. showDialog is false if user clicked "Search again" */
  void randomFlightClicked(bool showDialog);

  /* Update min/max values in RFG spin boxes */
  void keepRandomFlightRangeSane();

  /* RFG departure search thread */
  RandomDepartureAirportPickingByCriteria *departurePicker;

  /* RFG search progress feedback */
  QProgressDialog *randomFlightSearchProgress = nullptr;

  // following indices as in controller->getSqlModel()->getFullResultSet()
  // ui must make sure only 1 is > -1
  /* RFG predefined departure airport index */
  int predefinedDeparture = -1;

  /* RFG predefined destination airport index */
  int predefinedDestination = -1;

  /* Remember user selection in case user clicks "Search again" */
  bool randomFixedDeparture = false, randomFixedDestination = false;

  /* Airports have to persist during search. List is cleared in AirportSearch::dataRandomAirportsReceived() */
  QVector<std::pair<int, atools::geo::Pos> > randomSearchAirports;

  /* All layouts, lines and drop down menu items */
  QList<QObject *> airportSearchWidgets;

  /* All drop down menu actions */
  QList<QAction *> airportSearchMenuActions;

  /* Draw airport icon into ident table column */
  AirportIconDelegate *iconDelegate = nullptr;
  UnitStringTool *unitStringTool;
};

#endif // LITTLENAVMAP_AIRPORTSEARCH_H
