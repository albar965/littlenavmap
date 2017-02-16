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

  void optionsChanged();
  void preDatabaseLoad();
  void postDatabaseLoad();

  void highlightNextWaypoint(int leg);

  const maptypes::MapApproachLegs& getApproachSelectedLegs() const
  {
    return approachSelectedLegs;
  }

signals:
  /* Show approaches and highlight circles on the map */
  void approachSelected(maptypes::MapApproachRef);
  void approachLegSelected(maptypes::MapApproachRef);
  void approachAddToFlightPlan(maptypes::MapApproachRef);

  /* Zoom to approaches/transitions or waypoints */
  void showPos(const atools::geo::Pos& pos, float zoom, bool doubleClick);
  void showRect(const atools::geo::Rect& rect, bool doubleClick);

  /* Add, replace or delete object from flight plan from context menu or drag and drop */
  void routeAttachApproach(const maptypes::MapApproachLegs& legs);
  void routeClearApproach();

private:
  void itemSelectionChanged();
  void itemDoubleClicked(QTreeWidgetItem *item, int column);
  void treeClicked(QTreeWidgetItem *item, int column);
  void itemExpanded(QTreeWidgetItem *item);
  void contextMenu(const QPoint& pos);

  QTreeWidgetItem *buildApprItem(QTreeWidgetItem *runwayItem, const atools::sql::SqlRecord& recApp);
  QTreeWidgetItem *buildTransitionItem(QTreeWidgetItem *apprItem, const atools::sql::SqlRecord& recTrans);
  void buildLegItem(QTreeWidgetItem *parentItem, const maptypes::MapApproachLeg& leg);
  void setItemStyle(QTreeWidgetItem *item, const maptypes::MapApproachLeg& leg);

  // Save and restore expanded and selected item state
  QBitArray saveTreeViewState();
  void restoreTreeViewState(const QBitArray& state);

  QString buildRemarkStr(const maptypes::MapApproachLeg& leg);
  QString buildCourseStr(const maptypes::MapApproachLeg& leg);
  QString buildDistanceStr(const maptypes::MapApproachLeg& leg);
  void showEntry(QTreeWidgetItem *item, bool doubleClick);
  void updateApproachItem(QTreeWidgetItem *apprItem, int transitionId);
  void addApproachLegs(const maptypes::MapApproachLegs *legs, QTreeWidgetItem *item);
  void addTransitionLegs(const maptypes::MapApproachLegs *legs, QTreeWidgetItem *item);
  void fillApproachTreeWidget();
  void anchorClicked(const QUrl& url);
  void fillApproachInformation(const maptypes::MapAirport& airport, const maptypes::MapApproachRef& ref);
  void enableSelectedMode(const maptypes::MapApproachRef& ref);
  void disableSelectedMode();
  void updateTreeHeader();
  void createFonts();

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
