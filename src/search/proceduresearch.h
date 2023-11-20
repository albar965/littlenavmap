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

#ifndef LITTLENAVMAP_PROCTREECONTROLLER_H
#define LITTLENAVMAP_PROCTREECONTROLLER_H

#include "common/procflags.h"
#include "common/mapflags.h"
#include "search/abstractsearch.h"

#include <QBitArray>
#include <QFont>
#include <QObject>
#include <QVector>

namespace atools {
namespace gui {
class ItemViewZoomHandler;
class GridDelegate;
}

namespace sql {
class SqlRecord;
}
namespace geo {
class Pos;
class Rect;
}
}

namespace map {
struct MapAirport;
}

namespace proc {
struct MapProcedureRef;
struct MapProcedureLeg;
struct MapProcedureLegs;
}

class InfoQuery;
class QTreeWidget;
class QTreeWidgetItem;
class QMainWindow;
class QMenu;
class QAction;
class ProcedureQuery;
class AirportQuery;
class HtmlInfoBuilder;
class TreeEventFilter;

/* Takes care of the tree widget in approach tab on the informtaion window. */
class ProcedureSearch :
  public AbstractSearch
{
  Q_OBJECT

public:
  ProcedureSearch(QMainWindow *main, QTreeWidget *treeWidgetParam, si::TabSearchId tabWidgetIndex);
  virtual ~ProcedureSearch() override;

  ProcedureSearch(const ProcedureSearch& other) = delete;
  ProcedureSearch& operator=(const ProcedureSearch& other) = delete;

  /* Fill tree widget and index with all approaches and transitions of an airport */
  void showProcedures(const map::MapAirport& airport, bool departureFilter, bool arrivalFilter);

  /* Save tree view state */
  virtual void saveState() override;
  virtual void restoreState() override;

  /* Update fonts units, etc. */
  virtual void optionsChanged() override;

  /* GUI style has changed */
  virtual void styleChanged() override;

  virtual void preDatabaseLoad() override;
  virtual void postDatabaseLoad() override;

  /* Overrides with implementation */
  virtual void updateTableSelection(bool noFollow) override;
  virtual void clearSelection() override;
  virtual bool hasSelection() const override;

  /* Update wind in tree and header */
  void weatherUpdated();

signals:
  /* Show approaches and highlight circles on the map */
  void procedureSelected(const proc::MapProcedureRef& refs);
  void proceduresSelected(const QVector<proc::MapProcedureRef>& refs);
  void procedureLegSelected(const proc::MapProcedureRef& refs);

  /* Zoom to approaches/transitions or waypoints */
  void showPos(const atools::geo::Pos& pos, float zoom, bool doubleClick);
  void showRect(const atools::geo::Rect& rect, bool doubleClick);

  /* Add the complete procedure to the route */
  void routeInsertProcedure(const proc::MapProcedureLegs& legs);

  /* Show information info window on navaid on double click */
  void showInformation(const map::MapResult& result);

  /* Show a map object in the search panel (context menu) */
  void showInSearch(map::MapTypes type, const atools::sql::SqlRecord& record, bool select);

private:
  friend class TreeEventFilter;

  /* comboBoxProcedureSearchFilter index */
  enum FilterIndex
  {
    FILTER_ALL_PROCEDURES,
    FILTER_SID_PROCEDURES,
    FILTER_STAR_PROCEDURES,
    FILTER_ARRIVAL_PROCEDURES,
    FILTER_SEPARATOR_1,
    FILTER_APPROACH_ALL
    /* Approach types follow */
  };

  enum RunwayFilterIndex
  {
    FILTER_ALL_RUNWAYS,
    FILTER_NO_RUNWAYS /* Only if empty runways exist */
    /* Runways follow */
  };

  /* No op overrides */
  virtual void getSelectedMapObjects(map::MapResult&) const override;
  virtual void connectSearchSlots() override;
  virtual void updateUnits() override;

  virtual void tabDeactivated() override;

  void itemSelectionChanged();
  void itemSelectionChangedInternal(bool noFollow);
  void itemDoubleClicked(QTreeWidgetItem *item, int);

  /* Load legs dynamically as approaches or transitions are expanded */
  void itemExpanded(QTreeWidgetItem *item);

  void contextMenu(const QPoint& pos);

  /* Called from menu actions */
  void showInformationSelected();
  void showOnMapSelected();
  void procedureAttachSelected();
  void attachProcedure();
  void showProcedureTriggered();

  // Save and restore expanded and selected item state
  QBitArray saveTreeViewState();
  void restoreTreeViewState(const QBitArray& state, bool blockSignals);

  /* Build full approach or transition items for the tree view */
  QTreeWidgetItem *buildProcedureItem(QTreeWidgetItem *runwayItem, const QString& ident, const QString& procTypeText,
                                      const QString& headerText, const QString& menuText, const QStringList& attStr);
  QTreeWidgetItem *buildTransitionItem(QTreeWidgetItem *procItem, const atools::sql::SqlRecord& recTrans, bool sidOrStar);

  /* Build an leg for the selected/table or tree view */
  QTreeWidgetItem *buildLegItem(const proc::MapProcedureLeg& leg);

  /* Highlight missing navaids red */
  void setItemStyle(QTreeWidgetItem *item, const proc::MapProcedureLeg& leg);

  /* Show transition, approach or waypoint on map */
  void showEntry(QTreeWidgetItem *item, bool doubleClick, bool zoom);

  /* Update course and distances in the approach legs when a preceding transition is selected */
  void updateProcedureItem(QTreeWidgetItem *apprItem, int transitionId);

  QList<QTreeWidgetItem *> buildProcedureLegItems(const proc::MapProcedureLegs *legs, int transitionId);

  QList<QTreeWidgetItem *> buildTransitionLegItems(const proc::MapProcedureLegs *legs);

  void fillProcedureTreeWidget();

  /* Fill header for tree or selected/table view */
  void updateTreeHeader();
  void createFonts();

  void updateHeaderLabel();
  void updateWidgets();
  void updateFilter();

  void filterIndexChanged(int index);
  void filterIndexRunwayChanged(int);
  void filterChanged(const QString&);
  void clearRunwayFilter();
  void clearTypeFilter();
  void updateFilterBoxes();
  void resetSearch();
  void dockVisibilityChanged(bool visible);
  void fontChanged();

  static proc::MapProcedureTypes buildTypeFromProcedureRec(const atools::sql::SqlRecord& recApp);
  static bool procedureSortFunc(const atools::sql::SqlRecord& rec1, const atools::sql::SqlRecord& rec2);

  void fetchSingleTransitionId(proc::MapProcedureRef& ref) const;

  /* For header and menu item */
  QString procedureAndTransitionText(const QTreeWidgetItem *item, bool header) const;

  void clearSelectionClicked();
  void showAllToggled(bool checked);

  /* Get procedure reference with ids only */
  const proc::MapProcedureRef& fetchProcRef(const QTreeWidgetItem *item) const;

  /* Load whole procedure */
  const proc::MapProcedureLegs *fetchProcData(proc::MapProcedureRef& ref, const QTreeWidgetItem *item) const;

  /* User click on top link */
  void airportLabelLinkActivated(const QString& link);

  /* Create display text for procedure column */
  void procedureDisplayText(QString& procTypeText, QString& headerText, QString& menuText, QStringList& attText,
                            const atools::sql::SqlRecord& recApp, proc::MapProcedureTypes maptype, int numTransitions);

  /* Update wind columns for procedures after weather change */
  void updateProcedureWind();

  QString transitionIndicator, transitionIndicatorOne;

  // item's types are the indexes into this array with approach, transition and leg ids
  QVector<proc::MapProcedureRef> itemIndex;

  // Item type is the index into this array
  // Approach or transition legs are already loaded in tree if bit is set
  // Fist bit in pair: expanded or not, Second bit: selection state
  QBitArray itemLoadedIndex;

  InfoQuery *infoQuery = nullptr;
  ProcedureQuery *procedureQuery = nullptr;
  AirportQuery *airportQueryNav = nullptr;
  QTreeWidget *treeWidget = nullptr;

  /* Fonts for tree elements */
  QFont procedureBoldFont, procedureNormalFont, legFont, missedLegFont, invalidLegFont, identFont;

  map::MapAirport *currentAirportNav, *currentAirportSim;

  // Maps airport ID to expanded state of the tree widget items - bit array is same content as itemLoadedIndex
  QHash<int, QBitArray> recentTreeState;

  atools::gui::GridDelegate *gridDelegate = nullptr;

  FilterIndex filterIndex = FILTER_ALL_PROCEDURES;
  TreeEventFilter *treeEventFilter = nullptr;
  bool errors = false;
};

#endif // LITTLENAVMAP_PROCTREECONTROLLER_H
