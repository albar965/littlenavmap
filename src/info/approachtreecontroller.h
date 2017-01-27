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

/* Takes care of the tree widget in approach tab on the informtaion window. */
class ApproachTreeController :
  public QObject
{
  Q_OBJECT

public:
  ApproachTreeController(MainWindow *main);
  virtual ~ApproachTreeController();

  /* Fill tree widget and index with all approaches and transitions of an airport */
  void fillApproachTreeWidget(const maptypes::MapAirport& airport);

  /* Clear all if no airport is selected */
  void clear();

  /* Save tree view state */
  void saveState();
  void restoreState();

signals:
  void approachSelected(maptypes::MapApproachRef);
  void approachLegSelected(maptypes::MapApproachRef);
  void approachAddToFlightPlan(maptypes::MapApproachRef);
  void approachShowOnMap(maptypes::MapApproachRef);

private:
  void itemSelectionChanged();
  void itemDoubleClicked(QTreeWidgetItem *item, int column);
  void treeClicked(QTreeWidgetItem *item, int column);
  void itemExpanded(QTreeWidgetItem *item);
  void contextMenu(const QPoint& pos);

  QTreeWidgetItem *buildApprItem(QTreeWidgetItem *runwayItem, const atools::sql::SqlRecord& recApp);
  QTreeWidgetItem *buildTransItem(QTreeWidgetItem *apprItem, const atools::sql::SqlRecord& recTrans);
  void buildTransLegItem(QTreeWidgetItem *parentItem, const maptypes::MapApproachLeg& leg);
  void buildApprLegItem(QTreeWidgetItem *parentItem, const maptypes::MapApproachLeg& leg);
  void setItemStyle(QTreeWidgetItem *item, const maptypes::MapApproachLeg& leg);

  // Save and restore expande item state
  QBitArray saveTreeViewState();
  void restoreTreeViewState(const QBitArray& state);

  QString buildRemarkStr(const maptypes::MapApproachLeg& leg);
  QString buildCourseStr(const maptypes::MapApproachLeg& leg);
  QString buildDistanceStr(const maptypes::MapApproachLeg& leg);

  // item's types are the indexes into this array with approach, transition and leg ids
  QVector<maptypes::MapApproachRef> itemIndex;

  // Item type is the index into this array
  // Approach or transition legs are already loaded in tree if bit is set
  QBitArray itemLoadedIndex;

  InfoQuery *infoQuery = nullptr;
  ApproachQuery *approachQuery = nullptr;
  QTreeWidget *treeWidget = nullptr;
  MainWindow *mainWindow = nullptr;
  int lastAirportId = -1;
  QFont transitionFont, approachFont, runwayFont, legFont, missedLegFont, invalidLegFont;
  maptypes::MapAirport currentAirport;

  // Maps airport ID to expanded state of the tree widget items
  QHash<int, QBitArray> recentTreeState;

};

#endif // LITTLENAVMAP_APPROACHTREECONTROLLER_H
