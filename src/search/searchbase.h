/*****************************************************************************
* Copyright 2015-2017 Alexander Barthel albar965@mailbox.org
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

#include "common/maptypes.h"

#include "search/abstractsearch.h"

#include <QObject>

namespace atools {
namespace gui {
class ItemViewZoomHandler;
}
}

class QTableView;
class SqlController;
class ColumnList;
class MainWindow;
class QItemSelection;
class MapQuery;
class QTimer;
class CsvExporter;
class Column;
class ViewEventFilter;
class LineEditEventFilter;
class QLineEdit;

/*
 * Base for all search classes which reside each in its own tab, contains a result table view and a list of
 * search widgets.
 */
class SearchBase :
  public AbstractSearch
{
  Q_OBJECT

public:
  /* Class will take ownership of columnList */
  SearchBase(MainWindow *parent, QTableView *tableView, ColumnList *columnList, MapQuery *mapQuery,
             int tabWidgetIndex);
  virtual ~SearchBase();

  /* Disconnect and reconnect queries on database change */
  virtual void preDatabaseLoad() override;
  virtual void postDatabaseLoad() override;

  /* Clear all search widgets */
  void resetSearch();

  /* The center point of the distance search has changed. This will update the search result. */
  void searchMarkChanged(const atools::geo::Pos& mark);

  /* Set the search filter to ident, region, airport ident and update the search result */
  void filterByIdent(const QString& ident, const QString& region = QString(),
                     const QString& airportIdent = QString());

  /* Options dialog has changed some options */
  virtual void optionsChanged() override;

  /* Causes a selectionChanged signal to be emitted so map hightlights and status label can be updated */
  virtual void updateTableSelection() override;

  /* Has to be called by the derived classes. Connects double click, context menu and some other actions */
  virtual void connectSearchSlots() override;

  virtual void updateUnits() override;

  void showFirstEntry();

  void showSelectedEntry();
  void activateView();

signals:
  /* Show rectangle object (airport) on double click or menu selection */
  void showRect(const atools::geo::Rect& rect, bool doubleClick);

  /* Show point object (airport) on double click or menu selection */
  void showPos(const atools::geo::Pos& pos, int zoom, bool doubleClick);

  /* Search center changed in context menu */
  void changeSearchMark(const atools::geo::Pos& pos);

  /* Selection in table view has changed. Update label and map highlights */
  void selectionChanged(const SearchBase *source, int selected, int visible, int total);

  /* Show information in context menu selected */
  void showInformation(maptypes::MapSearchResult result);

  /* Show approaches in context menu selected */
  void showApproaches(maptypes::MapAirport airport);

  /* Set airport as flight plan departure (from context menu) */
  void routeSetDeparture(maptypes::MapAirport airport);

  /* Set airport as flight plan destination (from context menu) */
  void routeSetDestination(maptypes::MapAirport airport);

  /* Add airport or navaid to flight plan. Leg will be selected automatically */
  void routeAdd(int id, atools::geo::Pos userPos, maptypes::MapObjectTypes type, int legIndex);

protected:
  /* Update the hamburger menu button. Add * for change and check/uncheck actions */
  virtual void updateButtonMenu() = 0;

  /* Derived have to call this in constructor. Initializes table view, header, controller and CSV export. */
  void initViewAndController();

  /* Connect widgets to the controller */
  void connectSearchWidgets();

  void distanceSearchChanged(bool checked, bool changeViewState);

  void connectLineEdit(QLineEdit *lineEdit);

  /* Table/view controller */
  SqlController *controller;

  /* Column definitions that will be used to create the SQL queries */
  ColumnList *columns;
  QTableView *view;
  MainWindow *mainWindow;

private:
  virtual void saveViewState(bool distSearchActive) = 0;
  virtual void restoreViewState(bool distSearchActive) = 0;

  void tableSelectionChanged();
  void resetView();
  void editStartTimer();
  void doubleClick(const QModelIndex& index);
  void tableSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected);
  void reconnectSelectionModel();
  void getNavTypeAndId(int row, maptypes::MapObjectTypes& navType, int& id);
  void editTimeout();

  void loadAllRowsIntoView();
  void tableCopyClipboard();
  void showInformationTriggered();
  void showApproachesTriggered();
  void showOnMapTriggered();
  void contextMenu(const QPoint& pos);
  void dockVisibilityChanged(bool visible);
  void distanceSearchStateChanged(int state);
  void updateDistanceSearch();
  void updateFromSpinBox(int value, const Column *col);
  void updateFromMinSpinBox(int value, const Column *col);
  void updateFromMaxSpinBox(int value, const Column *col);
  void showRow(int row);

  /* Used to make the table rows smaller and also used to adjust font size */
  atools::gui::ItemViewZoomHandler *zoomHandler = nullptr;

  /* CSV export to clipboard */
  CsvExporter *csvExporter = nullptr;
  MapQuery *query;

  /* Used to delay search when using the time intensive distance search */
  QTimer *updateTimer;

  /* Tab index of this search tab on the search dock window */
  int tabIndex;

  ViewEventFilter *viewEventFilter = nullptr;
  LineEditEventFilter *lineEditEventFilter = nullptr;

};

#endif // LITTLENAVMAP_SEARCHBASE_H
