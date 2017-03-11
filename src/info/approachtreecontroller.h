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

#ifndef LITTLENAVMAP_APPROACHTREECONTROLLER_H
#define LITTLENAVMAP_APPROACHTREECONTROLLER_H

#include "common/maptypes.h"

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

class InfoQuery;
class QTreeWidget;
class QTreeWidgetItem;
class MainWindow;
class ApproachQuery;
class HtmlInfoBuilder;

/* Takes care of the tree widget in approach tab on the informtaion window. */
class ApproachTreeController :
  public QObject
{
  Q_OBJECT

public:
  ApproachTreeController(MainWindow *main);
  virtual ~ApproachTreeController();

  /* Fill tree widget and index with all approaches and transitions of an airport */
  void showApproaches(maptypes::MapAirport airport);

  /* Save tree view state */
  void saveState();
  void restoreState();

  /* Update fonts units, etc. */
  void optionsChanged();

  void preDatabaseLoad();
  void postDatabaseLoad();

  /* Highlight approach waypoint in the list */
  void highlightNextWaypoint(int leg);

  const maptypes::MapApproachLegs& getApproachSelectedLegs() const
  {
    return approachSelectedLegs;
  }

  void disconnectedFromSimulator();

signals:
  /* Show approaches and highlight circles on the map */
  void approachSelected(maptypes::MapApproachRef);
  void approachLegSelected(maptypes::MapApproachRef);

  /* Zoom to approaches/transitions or waypoints */
  void showPos(const atools::geo::Pos& pos, float zoom, bool doubleClick);
  void showRect(const atools::geo::Rect& rect, bool doubleClick);

  /* Add the initial fix to the route */
  void routeAttachApproach(const maptypes::MapApproachLegs& legs);

  /* Show information info window on navaid on double click */
  void showInformation(maptypes::MapSearchResult result);

private:
  void itemSelectionChanged();
  void itemDoubleClicked(QTreeWidgetItem *item, int column);

  /* Load legs dynamically as approaches or transitions are expanded */
  void itemExpanded(QTreeWidgetItem *item);

  void contextMenu(const QPoint& pos);

  // Save and restore expanded and selected item state
  QBitArray saveTreeViewState();
  void restoreTreeViewState(const QBitArray& state);

  /* Build full approach or transition items for the tree view */
  QTreeWidgetItem *buildApproachItem(QTreeWidgetItem *runwayItem, const atools::sql::SqlRecord& recApp);
  QTreeWidgetItem *buildTransitionItem(QTreeWidgetItem *apprItem, const atools::sql::SqlRecord& recTrans);

  /* Build an leg for the selected/table or tree view */
  void buildLegItem(QTreeWidgetItem *parentItem, const maptypes::MapApproachLeg& leg, float& remainingDistance);
  QString buildRemarkStr(const maptypes::MapApproachLeg& leg);
  QString buildCourseStr(const maptypes::MapApproachLeg& leg);
  QString buildDistanceStr(const maptypes::MapApproachLeg& leg);
  QString buildRemDistanceStr(const maptypes::MapApproachLeg& leg, float& remainingDistance);

  /* Highlight missing navaids red */
  void setItemStyle(QTreeWidgetItem *item, const maptypes::MapApproachLeg& leg);

  /* Show transition, approach or waypoint on map */
  void showEntry(QTreeWidgetItem *item, bool doubleClick);

  /* Update course and distances in the approach legs when a preceding transition is selected */
  void updateApproachItem(QTreeWidgetItem *apprItem, int transitionId);

  void addApproachLegs(const maptypes::MapApproachLegs *legs, QTreeWidgetItem *item, int transitionId,
                       float& remainingDistance);
  void addTransitionLegs(const maptypes::MapApproachLegs *legs, QTreeWidgetItem *item, float& remainingDistance);
  void fillApproachTreeWidget();
  void fillApproachInformation(const maptypes::MapAirport& airport, const maptypes::MapApproachRef& ref);

  /* Anchor clicked in the text browser */
  void anchorClicked(const QUrl& url);

  /* Focus on an approach and transition */
  void enableSelectedMode(const maptypes::MapApproachRef& ref);

  /* Unfocus and go back to tree  view */
  void disableSelectedMode();

  /* Fill header for tree or selected/table view */
  void updateTreeHeader();
  void createFonts();

  QTreeWidgetItem *parentApproachItem(QTreeWidgetItem *item) const;
  QTreeWidgetItem *parentTransitionItem(QTreeWidgetItem *item) const;

  maptypes::MapObjectTypes buildType(const atools::sql::SqlRecord& recApp);

  // item's types are the indexes into this array with approach, transition and leg ids
  QVector<maptypes::MapApproachRef> itemIndex;

  // Item type is the index into this array
  // Approach or transition legs are already loaded in tree if bit is set
  // Fist bit in pair: expanded or not, Second bit: selection state
  QBitArray itemLoadedIndex;

  InfoQuery *infoQuery = nullptr;
  ApproachQuery *approachQuery = nullptr;
  HtmlInfoBuilder *infoBuilder = nullptr;
  QTreeWidget *treeWidget = nullptr;
  MainWindow *mainWindow = nullptr;
  QFont transitionFont, approachFont, legFont, missedLegFont, invalidLegFont, activeLegFont, identFont;
  maptypes::MapAirport currentAirport;

  // Maps airport ID to expanded state of the tree widget items - bit array is same content as itemLoadedIndex
  QHash<int, QBitArray> recentTreeState;

  /* View mode: True if only one selected approach is shown */
  bool approachSelectedMode = false;
  maptypes::MapApproachLegs approachSelectedLegs;

  /* Used to make the table rows smaller and also used to adjust font size */
  atools::gui::ItemViewZoomHandler *zoomHandler = nullptr;
  atools::gui::GridDelegate *gridDelegate = nullptr;

};

#endif // LITTLENAVMAP_APPROACHTREECONTROLLER_H
