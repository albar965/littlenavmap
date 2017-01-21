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

#include "info/approachtreecontroller.h"

#include "common/unit.h"
#include "gui/mainwindow.h"
#include "info/infoquery.h"
#include "sql/sqlrecord.h"
#include "ui_mainwindow.h"
#include "gui/actiontextsaver.h"

#include <QMenu>
#include <QTreeWidget>

using atools::sql::SqlRecord;
using atools::sql::SqlRecordVector;
using maptypes::MapApproachRef;

ApproachTreeController::ApproachTreeController(MainWindow *main)
  : infoQuery(main->getInfoQuery()), treeWidget(main->getUi()->treeWidgetApproachInfo), mainWindow(main)
{
  connect(treeWidget, &QTreeWidget::itemSelectionChanged, this, &ApproachTreeController::itemSelectionChanged);
  connect(treeWidget, &QTreeWidget::itemDoubleClicked, this, &ApproachTreeController::itemDoubleClicked);
  connect(treeWidget, &QTreeWidget::itemActivated, this, &ApproachTreeController::itemActivated);
  connect(treeWidget, &QTreeWidget::customContextMenuRequested, this, &ApproachTreeController::contextMenu);
}

ApproachTreeController::~ApproachTreeController()
{

}

void ApproachTreeController::fillApproachTreeWidget(int airportId)
{
  if(lastAirportId == airportId)
    return;

  treeWidget->clear();
  itemIndex.clear();

  const SqlRecordVector *recAppVector = infoQuery->getApproachInformation(airportId);
  if(recAppVector != nullptr)
  {
    QTreeWidgetItem *root = treeWidget->invisibleRootItem();
    for(const SqlRecord& recApp : *recAppVector)
    {
      QString runwayName;
      if(recApp.valueStr("runway_name").isNull())
        runwayName = tr("No runway assignement");
      else
        runwayName = tr("Runway ") + recApp.valueStr("runway_name");

      QList<QTreeWidgetItem *> foundItems = treeWidget->findItems(runwayName, Qt::MatchExactly, 0);

      QTreeWidgetItem *runwayItem = nullptr;
      if(foundItems.isEmpty())
      {
        itemIndex.append(MapApproachRef(airportId, recApp.valueInt("runway_end_id")));
        runwayItem = new QTreeWidgetItem(root, {runwayName}, itemIndex.size() - 1);
      }
      else
        runwayItem = foundItems.first();

      int runwayEndId = itemIndex.at(runwayItem->type()).runwayEndId;

      int apprId = recApp.valueInt("approach_id");
      QString approachType = maptypes::approachType(recApp.valueStr("type")) + " " + recApp.valueStr("suffix");
      itemIndex.append(MapApproachRef(airportId, runwayEndId, apprId));
      QTreeWidgetItem *apprItem =
        new QTreeWidgetItem(runwayItem,
                            {
                              tr("Approach ") + approachType,
                              recApp.valueStr("fix_ident"),
                              Unit::altFeet(recApp.valueFloat("altitude"))
                            }, itemIndex.size() - 1);

      const SqlRecordVector *recTransVector =
        infoQuery->getTransitionInformation(recApp.valueInt("approach_id"));
      if(recTransVector != nullptr)
      {
        // Transitions for this approach
        for(const SqlRecord& recTrans : *recTransVector)
        {
          itemIndex.append(MapApproachRef(airportId, runwayEndId, apprId, recTrans.valueInt("transition_id")));
          new QTreeWidgetItem(apprItem,
                              {
                                tr("Transition") /* + recTrans.valueStr("fix_ident")*/,
                                recTrans.valueStr("fix_ident"),
                                Unit::altFeet(recTrans.valueFloat("altitude"))
                              },
                              itemIndex.size() - 1);
        }
      }
    }
    // treeWidget->expandAll();
  }
  else
  {
    QTreeWidgetItem *item = new QTreeWidgetItem(treeWidget->invisibleRootItem(),
                                                {tr("Airport has no approaches.")});
    item->setDisabled(true);
    return;
  }

  lastAirportId = airportId;
}

void ApproachTreeController::clear()
{
  treeWidget->clear();
  itemIndex.clear();
  lastAirportId = -1;

  QTreeWidgetItem *item = new QTreeWidgetItem(treeWidget->invisibleRootItem(),
                                              {tr("No airport selected.")});
  item->setDisabled(true);
}

void ApproachTreeController::itemSelectionChanged()
{
  for(const QTreeWidgetItem *item : treeWidget->selectedItems())
  {
    const MapApproachRef& entry = itemIndex.at(item->type());

    qDebug() << Q_FUNC_INFO << entry.runwayEndId << entry.approachId << entry.transitionId;

    emit approachSelected(entry);
  }
}

void ApproachTreeController::itemDoubleClicked(QTreeWidgetItem *item, int column)
{
  qDebug() << Q_FUNC_INFO;
}

void ApproachTreeController::itemActivated(QTreeWidgetItem *item, int column)
{
  qDebug() << Q_FUNC_INFO;
}

void ApproachTreeController::contextMenu(const QPoint& pos)
{
  qDebug() << Q_FUNC_INFO;

  QPoint menuPos = QCursor::pos();
  // Use widget center if position is not inside widget
  if(!treeWidget->rect().contains(treeWidget->mapFromGlobal(QCursor::pos())))
    menuPos = treeWidget->mapToGlobal(treeWidget->rect().center());

  // Save text which will be changed below
  Ui::MainWindow *ui = mainWindow->getUi();
  atools::gui::ActionTextSaver saver({ui->actionInfoApproachShow, ui->actionInfoApproachAddToFlightPlan});
  Q_UNUSED(saver);

  QMenu menu;
  menu.addAction(ui->actionInfoApproachExpandAll);
  menu.addAction(ui->actionInfoApproachCollapseAll);
  menu.addSeparator();
  menu.addAction(ui->actionInfoApproachShow);
  menu.addAction(ui->actionInfoApproachAddToFlightPlan);

  ui->actionInfoApproachShow->setDisabled(true);
  ui->actionInfoApproachAddToFlightPlan->setDisabled(true);

  QTreeWidgetItem *item = treeWidget->itemAt(pos);
  if(item != nullptr)
  {
    const MapApproachRef& entry = itemIndex.at(item->type());
    if(entry.approachId == -1 && entry.transitionId == -1)
    {
      ui->actionInfoApproachShow->setText(
        ui->actionInfoApproachShow->text().arg(QString()));

      ui->actionInfoApproachAddToFlightPlan->setText(
        ui->actionInfoApproachAddToFlightPlan->text().arg(QString()));
    }
    else
    {
      ui->actionInfoApproachShow->setEnabled(true);
      ui->actionInfoApproachAddToFlightPlan->setEnabled(true);

      ui->actionInfoApproachShow->setText(
        ui->actionInfoApproachShow->text().arg(item->text(0)));
      ui->actionInfoApproachAddToFlightPlan->setText(
        ui->actionInfoApproachAddToFlightPlan->text().arg(item->text(0)));
    }
  }
  else
  {
    ui->actionInfoApproachShow->setText(
      ui->actionInfoApproachShow->text().arg(QString()));

    ui->actionInfoApproachAddToFlightPlan->setText(
      ui->actionInfoApproachAddToFlightPlan->text().arg(QString()));
  }

  QAction *action = menu.exec(menuPos);
  if(action == ui->actionInfoApproachExpandAll)
    treeWidget->expandAll();
  else if(action == ui->actionInfoApproachCollapseAll)
    treeWidget->collapseAll();
  else if(action == ui->actionInfoApproachShow)
    emit approachShowOnMap(itemIndex.at(item->type()));
  else if(action == ui->actionInfoApproachAddToFlightPlan)
    emit approachAddToFlightPlan(itemIndex.at(item->type()));
}
