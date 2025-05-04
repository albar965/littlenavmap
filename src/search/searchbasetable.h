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

#ifndef LITTLENAVMAP_SEARCHBASE_H
#define LITTLENAVMAP_SEARCHBASE_H

#include "search/abstractsearch.h"

#include "common/mapflags.h"

class QTableView;
class SqlController;
class ColumnList;
class QMainWindow;
class QItemSelection;
class MapQuery;
class AirportQuery;
class QTimer;
class CsvExporter;
class Column;
class SearchViewEventFilter;
class SearchWidgetEventFilter;
class QLineEdit;
class QAction;
class QComboBox;

class QLayout;
class QueryWidget;
class QueryBuilderResult;

namespace atools {
namespace sql {
class SqlDatabase;
class SqlRecord;
}

namespace geo {
class Pos;
class Rect;
}
}

namespace map {
struct MapAirport;
struct MapAirportMsa;
struct MapResult;
}

/*
 * Base for all search classes which reside each in its own tab, contains a result table view and a list of
 * search widgets.
 */
class SearchBaseTable :
  public AbstractSearch
{
  Q_OBJECT

public:
  /* Class will take ownership of columnList */
  SearchBaseTable(QMainWindow *parent, QTableView *tableView, ColumnList *columnList,
                  si::TabSearchId tabWidgetIndex);
  virtual ~SearchBaseTable() override;

  SearchBaseTable(const SearchBaseTable& other) = delete;
  SearchBaseTable& operator=(const SearchBaseTable& other) = delete;

  /* Disconnect and reconnect queries on database change */
  virtual void preDatabaseLoad() override;
  virtual void postDatabaseLoad() override;

  /* Clear all search widgets */
  virtual void resetSearch();

  /* The center point of the distance search has changed. This will update the search result. */
  void searchMarkChanged(const atools::geo::Pos& mark);

  /* Set the search filter to ident, region, airport ident and update the search result.
   * "ignoreQueryBuilder" set to true will cause columns in record matching query builder
   * columns to be used as normal queries (not query builder). */
  void showInSearch(const atools::sql::SqlRecord& record, bool ignoreQueryBuilder = false);

  /* Options dialog has changed some options */
  virtual void optionsChanged() override;

  /* GUI style has changed */
  virtual void styleChanged() override;

  /* Causes a selectionChanged signal to be emitted so map highlights and status label can be updated */
  virtual void updateTableSelection(bool noFollow) override;

  /* Has to be called by the derived classes. Connects double click, context menu and some other actions */
  virtual void connectSearchSlots() override;

  virtual void updateUnits() override;

  virtual void clearSelection() override;
  virtual bool hasSelection() const override;

  virtual void showFirstEntry() override;

  void showSelectedEntry() override;
  void activateView() override;

  void nothingSelectedTriggered();

  /* Refresh table after updates in the database */
  void refreshData(bool loadAll, bool keepSelection, bool force);
  void refreshView();

  /* Number of rows currently loaded into the table view */
  int getVisibleRowCount() const;

  /* Total number of rows returned by the last query */
  int getTotalRowCount() const;

  /* Number of selected rows */
  int getSelectedRowCount() const;

  /* Get ids of all selected objects */
  QVector<int> getSelectedIds() const;

  /* Get selected rows in order from bottom to top */
  QVector<int> getSelectedRows() const;

  /* Default handler */
  QVariant modelDataHandler(int colIndex, int rowIndex, const Column *col, const QVariant&,
                            const QVariant& displayRoleValue, Qt::ItemDataRole role) const;
  QString formatModelData(const Column *, const QVariant& displayRoleValue) const;

  void selectAll();

  /* Callback for combined query on ident, icao, faa and local columns.
   * Internal. */
  QueryBuilderResult queryBuilderFunc(const QueryWidget& queryWidget);

signals:
  /* Show rectangle object (airport) on double click or menu selection */
  void showRect(const atools::geo::Rect& rect, bool doubleClick);

  /* Show point object (airport) on double click or menu selection */
  void showPos(const atools::geo::Pos& pos, float zoom, bool doubleClick);

  /* Search center changed in context menu */
  void changeSearchMark(const atools::geo::Pos& pos);

  /* Selection in table view has changed. Update label and map highlights */
  void selectionChanged(const SearchBaseTable *source, int selected, int visible, int total);

  /* Show information in context menu selected */
  void showInformation(const map::MapResult& result);

  /* Show approaches in context menu selected */
  void showProcedures(const map::MapAirport& airport, bool departureFilter, bool arrivalFilter);
  void showCustomApproach(const map::MapAirport& airport, const QString& suffix);
  void showCustomDeparture(const map::MapAirport& airport, const QString& suffix);

  /* Set airport as flight plan departure (from context menu) */
  void routeSetDeparture(const map::MapAirport& airport);

  /* Set airport as flight plan destination (from context menu) */
  void routeSetDestination(const map::MapAirport& airport);

  /* Add an alternate airport */
  void routeAddAlternate(const map::MapAirport& airport);

  /* Add airport or navaid to flight plan. Leg will be selected automatically */
  void routeAdd(int id, const atools::geo::Pos& userPos, map::MapTypes type, int legIndex);

  void routeDirectTo(int id, const atools::geo::Pos& userPos, map::MapTypes type, int legIndexDirectTo);

  /* Load flight plan or load aircraft performance file triggered from logbook */
  void loadRouteFile(const QString& filepath);
  void loadPerfFile(const QString& filepath);

  void addAirportMsa(const map::MapAirportMsa& airportMsa);

  void addUserpointFromMap(const map::MapResult& result, const atools::geo::Pos& pos, bool airportAddon);

protected:
  /* Update the hamburger menu button. Add * for change and check/uncheck actions */
  virtual void updateButtonMenu() = 0;
  virtual void updatePushButtons() = 0;

  /* Return the action that defines follow mode */
  virtual QAction *followModeAction() = 0;

  /* An option selection action in the dropdown button menu was checked. Update menu items and search. */
  void buttonMenuTriggered(QLayout *layouts, QWidget *otherWidgets, bool state, bool distanceSearch);

  /* Derived have to call this in constructor. Initializes table view, header, controller and CSV export. */
  void initViewAndController(atools::sql::SqlDatabase *db);

  /* Connect widgets to the controller */
  void connectSearchWidgets();

  void distanceSearchChanged(bool changeViewState);

  void installEventFilterForWidget(QWidget *widget);

  /* get airport at current position, airport at top of selection or airport on top of the list **/
  map::MapAirport currentAirport();

  /* Call at the end of restoring parameters */
  void finishRestore();

  /* Table/view controller */
  SqlController *controller = nullptr;

  /* Column definitions that will be used to create the SQL queries */
  ColumnList *columns;
  QTableView *view;
  MapQuery *mapQuery;
  AirportQuery *airportQuery;

  /* true = current view is distance search view and false = normal view */
  bool viewStateDistSearch = false;

private:
  virtual void saveViewState(bool distanceSearchState) = 0;
  virtual void restoreViewState(bool distanceSearchState) = 0;
  virtual void tabDeactivated() override;

  void tableSelectionChangedInternal(bool noFollow);
  void resetView();
  void editStartTimer();
  void doubleClick(const QModelIndex& index);
  void tableSelectionChanged(const QItemSelection&, const QItemSelection&);
  void reconnectSelectionModel();
  void getNavTypeAndId(int row, map::MapTypes& navType, int& id);
  void getNavTypeAndId(int row, map::MapTypes& navType, map::MapAirspaceSources& airspaceSource, int& id);
  void editTimeout();

  void loadAllRowsIntoView();
  void tableCopyClipboard();
  void showInformationTriggered();
  void showApproachesTriggered();
  void showApproachesCustomTriggered();
  void showDeparturesCustomTriggered();
  void showOnMapTriggered();
  void contextMenu(const QPoint& pos);
  void dockVisibilityChanged(bool);
  void distanceSearchStateChanged(int);
  void updateDistanceSearch();
  void updateFromSpinBox(int value, const Column *col);
  void updateFromMinSpinBox(int value, const Column *col);
  void updateFromMaxSpinBox(int value, const Column *col);
  void showRow(int row, bool showInfo);
  void fontChanged();
  void showApproaches(bool customApproach, bool customDeparture);
  void fetchedMore();

  /* Called by actions on airport search tab */
  void routeSetDepartureAction();
  void routeSetDestinationAction();
  void routeAddAlternateAction();
  void routeDirectToAction();

  /* Get selected index or index of first entry in the result table */
  QModelIndex selectedOrFirstIndex();

  QString joinQuery(const QStringList& texts, bool concatAnd);

  /* CSV export to clipboard */
  CsvExporter *csvExporter = nullptr;

  /* Used to delay search when using the time intensive distance search */
  QTimer *updateTimer;

  SearchViewEventFilter *viewEventFilter = nullptr;
  SearchWidgetEventFilter *widgetEventFilter = nullptr;

};

#endif // LITTLENAVMAP_SEARCHBASE_H
