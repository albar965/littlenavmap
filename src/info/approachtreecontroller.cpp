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
#include "common/infoquery.h"
#include "common/approachquery.h"
#include "sql/sqlrecord.h"
#include "ui_mainwindow.h"
#include "gui/actiontextsaver.h"
#include "common/constants.h"
#include "settings/settings.h"
#include "gui/widgetstate.h"

#include <QMenu>
#include <QTreeWidget>

enum TreeColumn
{
  DESCRIPTION,
  IDENT,
  ALTITUDE,
  COURSE,
  DISTANCE,
  REMARKS
};

using atools::sql::SqlRecord;
using atools::sql::SqlRecordVector;
using maptypes::MapApproachLeg;
using maptypes::MapApproachLegs;
using maptypes::MapApproachRef;

ApproachTreeController::ApproachTreeController(MainWindow *main)
  : infoQuery(main->getInfoQuery()), approachQuery(main->getApproachQuery()),
    treeWidget(main->getUi()->treeWidgetApproachInfo), mainWindow(main)
{
  connect(treeWidget, &QTreeWidget::itemSelectionChanged, this, &ApproachTreeController::itemSelectionChanged);
  connect(treeWidget, &QTreeWidget::itemDoubleClicked, this, &ApproachTreeController::itemDoubleClicked);
  connect(treeWidget, &QTreeWidget::itemExpanded, this, &ApproachTreeController::itemExpanded);
  connect(treeWidget, &QTreeWidget::customContextMenuRequested, this, &ApproachTreeController::contextMenu);

  QTreeWidgetItem *root = treeWidget->invisibleRootItem();
  runwayFont = root->font(0);
  runwayFont.setWeight(QFont::Bold);

  approachFont = root->font(0);
  approachFont.setWeight(QFont::Bold);

  transitionFont = root->font(0);
  transitionFont.setWeight(QFont::Bold);

  legFont = root->font(0);
  // legFont.setPointSizeF(legFont.pointSizeF() * 0.85f);

  missedLegFont = root->font(0);
  // missedLegFont.setItalic(true);

  invalidLegFont = legFont;
  invalidLegFont.setBold(true);
}

ApproachTreeController::~ApproachTreeController()
{

}

void ApproachTreeController::fillApproachTreeWidget(const maptypes::MapAirport& airport)
{
  if(lastAirportId == airport.id)
    return;

  if(lastAirportId != -1)
    recentTreeState.insert(lastAirportId, saveTreeViewState());

  treeWidget->clear();
  itemIndex.clear();
  itemLoadedIndex.clear();
  currentAirport = airport;

  const SqlRecordVector *recAppVector = infoQuery->getApproachInformation(airport.id);
  if(recAppVector != nullptr)
  {
    QTreeWidgetItem *root = treeWidget->invisibleRootItem();

    for(const SqlRecord& recApp : *recAppVector)
    {
      int runwayEndId = recApp.valueInt("runway_end_id");
      int apprId = recApp.valueInt("approach_id");

      itemIndex.append(MapApproachRef(airport.id, runwayEndId, apprId));

      QTreeWidgetItem *apprItem = buildApprItem(root, recApp);

      const SqlRecordVector *recTransVector = infoQuery->getTransitionInformation(recApp.valueInt("approach_id"));
      if(recTransVector != nullptr)
      {
        // Transitions for this approach
        for(const SqlRecord& recTrans : *recTransVector)
        {
          itemIndex.append(MapApproachRef(airport.id, runwayEndId, apprId, recTrans.valueInt("transition_id")));
          buildTransItem(apprItem, recTrans);
        }
      }
    }
  }
  else
  {
    QTreeWidgetItem *item = new QTreeWidgetItem(treeWidget->invisibleRootItem(),
                                                {tr("%1 has no approaches.").
                                                 arg(maptypes::airportText(airport))}, -1);
    item->setDisabled(true);
    item->setFirstColumnSpanned(true);
  }
  itemLoadedIndex.resize(itemIndex.size());

  if(recentTreeState.contains(airport.id))
    restoreTreeViewState(recentTreeState.value(airport.id));

  lastAirportId = airport.id;
}

void ApproachTreeController::clear()
{
  treeWidget->clearSelection();
  treeWidget->clear();
  itemIndex.clear();
  itemLoadedIndex.clear();
  recentTreeState.clear();
  lastAirportId = -1;

  QTreeWidgetItem *item = new QTreeWidgetItem(treeWidget->invisibleRootItem(),
                                              {tr("No airport selected.")});
  item->setDisabled(true);
}

void ApproachTreeController::saveState()
{
  Ui::MainWindow *ui = mainWindow->getUi();
  atools::gui::WidgetState(lnm::INFOWINDOW_APPROACH).save({ui->actionInfoApproachShowAppr,
                                                           ui->actionInfoApproachShowMissedAppr,
                                                           ui->actionInfoApproachShowTrans});

  atools::settings::Settings& settings = atools::settings::Settings::instance();
  settings.setValueVar(lnm::INFOWINDOW_APPROACHTREESTATE, saveTreeViewState());
}

void ApproachTreeController::restoreState()
{
  Ui::MainWindow *ui = mainWindow->getUi();
  atools::gui::WidgetState(lnm::INFOWINDOW_APPROACH).restore({ui->actionInfoApproachShowAppr,
                                                              ui->actionInfoApproachShowMissedAppr,
                                                              ui->actionInfoApproachShowTrans});

  atools::settings::Settings& settings = atools::settings::Settings::instance();
  if(settings.contains(lnm::INFOWINDOW_APPROACHTREESTATE))
    restoreTreeViewState(settings.valueVar(lnm::INFOWINDOW_APPROACHTREESTATE).toBitArray());
}

void ApproachTreeController::itemSelectionChanged()
{
  QList<QTreeWidgetItem *> items = treeWidget->selectedItems();
  if(items.isEmpty())
  {
    emit approachSelected(maptypes::MapApproachRef());
    emit approachLegSelected(maptypes::MapApproachRef());
  }
  else
  {
    for(const QTreeWidgetItem *item : items)
    {
      const MapApproachRef& ref = itemIndex.at(item->type());

      qDebug() << Q_FUNC_INFO << ref.runwayEndId << ref.approachId << ref.transitionId << ref.legId;

      if(ref.isApproachOrTransition())
        emit approachSelected(ref);

      if(ref.isLeg())
        emit approachLegSelected(ref);
      else
        emit approachLegSelected(maptypes::MapApproachRef());

      if(ref.isApproachAndTransition())
      {
        QTreeWidgetItem *apprItem;
        if(ref.isLeg())
          apprItem = item->parent()->parent();
        else
          apprItem = item->parent();

        updateApproachItem(apprItem, ref.transitionId);
      }
    }
  }
}

/* Update course and distance for the parent approach of this leg item */
void ApproachTreeController::updateApproachItem(QTreeWidgetItem *apprItem, int transitionId)
{
  if(apprItem != nullptr)
  {
    for(int i = 0; i < apprItem->childCount(); i++)
    {
      QTreeWidgetItem *child = apprItem->child(i);
      const MapApproachRef& childref = itemIndex.at(child->type());
      if(childref.isLeg())
      {
        const maptypes::MapApproachLegs *legs = approachQuery->getTransitionLegs(currentAirport, transitionId);
        const maptypes::MapApproachLeg *aleg = legs->approachLegById(childref.legId);

        if(aleg != nullptr)
        {
          child->setText(COURSE, buildCourseStr(*aleg));
          child->setText(DISTANCE, buildDistanceStr(*aleg));
        }
      }
    }
  }
}

void ApproachTreeController::itemDoubleClicked(QTreeWidgetItem *item, int column)
{
  Q_UNUSED(column);
  showEntry(item, true);
}

void ApproachTreeController::itemExpanded(QTreeWidgetItem *item)
{
  if(item != nullptr)
  {
    if(itemLoadedIndex.at(item->type()))
      return;

    // Get a copy since vector is rebuilt underneath
    const MapApproachRef entry = itemIndex.at(item->type());

    if(entry.legId == -1)
    {
      if(entry.approachId != -1 && entry.transitionId == -1)
      {
        itemLoadedIndex.setBit(item->type());
        const MapApproachLegs *legs = approachQuery->getApproachLegs(currentAirport, entry.approachId);

        if(legs != nullptr)
        {
          for(const MapApproachLeg& e : legs->alegs)
          {
            itemIndex.append(MapApproachRef(entry.airportId, entry.runwayEndId, entry.approachId,
                                            entry.transitionId, e.legId));
            buildApprLegItem(item, e);
          }
        }
      }
      else if(entry.approachId != -1 && entry.transitionId != -1)
      {
        itemLoadedIndex.setBit(item->type());
        const MapApproachLegs *legs = approachQuery->getTransitionLegs(currentAirport, entry.transitionId);

        if(legs != nullptr)
        {
          for(const MapApproachLeg& e : legs->tlegs)
          {
            itemIndex.append(MapApproachRef(entry.airportId, entry.runwayEndId, entry.approachId,
                                            entry.transitionId, e.legId));
            buildTransLegItem(item, e);
          }
        }
      }
      itemLoadedIndex.resize(itemIndex.size());
    }
  }
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
  menu.addAction(ui->actionInfoApproachClear);
  menu.addSeparator();
  menu.addAction(ui->actionInfoApproachShow);
  menu.addSeparator();
  menu.addAction(ui->actionInfoApproachShowAppr);
  menu.addAction(ui->actionInfoApproachShowMissedAppr);
  menu.addAction(ui->actionInfoApproachShowTrans);
  // menu.addAction(ui->actionInfoApproachAddToFlightPlan);

  ui->actionInfoApproachClear->setDisabled(treeWidget->selectedItems().isEmpty());
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
    ui->actionInfoApproachShow->setText(ui->actionInfoApproachShow->text().arg(QString()));
    ui->actionInfoApproachAddToFlightPlan->setText(ui->actionInfoApproachAddToFlightPlan->text().arg(QString()));
  }

  QAction *action = menu.exec(menuPos);
  if(action == ui->actionInfoApproachExpandAll)
  {
    // treeWidget->expandAll();
    const QTreeWidgetItem *root = treeWidget->invisibleRootItem();

    // First load child nodes to get the same tree
    for(int i = 0; i < root->childCount(); ++i)
      root->child(i)->setExpanded(true);
  }
  else if(action == ui->actionInfoApproachCollapseAll)
    treeWidget->collapseAll();
  else if(action == ui->actionInfoApproachClear)
  {
    treeWidget->clearSelection();
    emit approachLegSelected(maptypes::MapApproachRef());
    emit approachSelected(maptypes::MapApproachRef());
  }
  else if(action == ui->actionInfoApproachShow)
    showEntry(item, false);
  // else if(action == ui->actionInfoApproachAddToFlightPlan)
  // emit approachAddToFlightPlan(itemIndex.at(item->type()));

  // Done by the actions themselves
  // else if(action == ui->actionInfoApproachShowAppr ||
  // action == ui->actionInfoApproachShowMissedAppr ||
  // action == ui->actionInfoApproachShowTrans)
}

void ApproachTreeController::showEntry(QTreeWidgetItem *item, bool doubleClick)
{
  const MapApproachRef& entry = itemIndex.at(item->type());

  if(entry.legId != -1)
  {
    const maptypes::MapApproachLeg *leg = nullptr;

    if(entry.approachId != -1)
      leg = approachQuery->getApproachLeg(currentAirport, entry.legId);
    else if(entry.transitionId != -1)
      leg = approachQuery->getTransitionLeg(currentAirport, entry.legId);

    if(leg != nullptr)
      emit showPos(leg->line.getPos2(), 0.f, doubleClick);
  }
  else if(entry.transitionId != -1 && !doubleClick)
  {
    const maptypes::MapApproachLegs *legs = approachQuery->getTransitionLegs(currentAirport, entry.transitionId);
    if(legs != nullptr)
      emit showRect(legs->bounding, doubleClick);
  }
  else if(entry.approachId != -1 && !doubleClick)
  {
    const maptypes::MapApproachLegs *legs = approachQuery->getApproachLegs(currentAirport, entry.approachId);
    if(legs != nullptr)
      emit showRect(legs->bounding, doubleClick);
  }

}

QTreeWidgetItem *ApproachTreeController::buildApprItem(QTreeWidgetItem *runwayItem, const SqlRecord& recApp)
{
  QString approachType = maptypes::approachType(recApp.valueStr("type")) + " " + recApp.valueStr("suffix") + " " +
                         recApp.valueStr("runway_name");
  if(recApp.valueBool("has_gps_overlay"))
    approachType += tr(" (GPS Overlay)");

  QString altStr;
  if(recApp.valueFloat("altitude") > 0.f)
    altStr = Unit::altFeet(recApp.valueFloat("altitude"));

  QTreeWidgetItem *item = new QTreeWidgetItem({
                                                tr("Approach ") + approachType,
                                                recApp.valueStr("fix_ident"),
                                                altStr
                                              }, itemIndex.size() - 1);
  item->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);

  for(int i = 0; i < item->columnCount(); i++)
    item->setFont(i, approachFont);

  runwayItem->addChild(item);

  return item;
}

QTreeWidgetItem *ApproachTreeController::buildTransItem(QTreeWidgetItem *apprItem, const SqlRecord& recTrans)
{
  QString altStr;
  if(recTrans.valueFloat("altitude") > 0.f)
    altStr = Unit::altFeet(recTrans.valueFloat("altitude"));

  QString name(tr("Transition"));
  if(recTrans.valueStr("type") == "F")
    name.append(tr(" (Full)"));
  else if(recTrans.valueStr("type") == "D")
    name.append(tr(" (DME)"));

  QTreeWidgetItem *item = new QTreeWidgetItem({
                                                name,
                                                recTrans.valueStr("fix_ident"),
                                                altStr
                                              },
                                              itemIndex.size() - 1);
  item->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);

  for(int i = 0; i < item->columnCount(); i++)
    item->setFont(i, transitionFont);

  apprItem->addChild(item);

  return item;
}

void ApproachTreeController::buildApprLegItem(QTreeWidgetItem *parentItem, const MapApproachLeg& leg)
{
  QString remarkStr = buildRemarkStr(leg);
  QTreeWidgetItem *item = new QTreeWidgetItem(
    {
      /*(leg.missed ? tr("Missed: ") : QString()) + */ maptypes::approachLegTypeStr(leg.type),
      leg.fixIdent,
      maptypes::altRestrictionText(leg.altRestriction),
      buildCourseStr(leg),
      buildDistanceStr(leg),
      remarkStr
    },
    itemIndex.size() - 1);

  item->setToolTip(REMARKS, remarkStr);

  setItemStyle(item, leg);

  parentItem->addChild(item);
}

void ApproachTreeController::buildTransLegItem(QTreeWidgetItem *parentItem, const MapApproachLeg& leg)
{
  QString remarkStr = buildRemarkStr(leg);
  QTreeWidgetItem *item = new QTreeWidgetItem(
    {
      maptypes::approachLegTypeStr(leg.type),
      leg.fixIdent,
      maptypes::altRestrictionText(leg.altRestriction),
      buildCourseStr(leg),
      buildDistanceStr(leg),
      remarkStr
    },
    itemIndex.size() - 1);

  item->setToolTip(REMARKS, remarkStr);

  setItemStyle(item, leg);

  parentItem->addChild(item);
}

void ApproachTreeController::setItemStyle(QTreeWidgetItem *item, const MapApproachLeg& leg)
{
  bool invalid = (!leg.fixIdent.isEmpty() && !leg.fixPos.isValid())
                 /* ||   (!leg.recFixIdent.isEmpty() && !leg.recFixPos.isValid())*/;

  for(int i = 0; i < item->columnCount(); i++)
  {
    if(!invalid)
    {
      item->setFont(i, leg.missed ? missedLegFont : legFont);
      if(leg.missed)
        item->setForeground(i, QColor(140, 140, 140));
    }
    else
    {
      item->setFont(i, invalidLegFont);
      item->setForeground(i, Qt::red);
    }
  }
}

QString ApproachTreeController::buildCourseStr(const MapApproachLeg& leg)
{
  QString courseStr;
  // if(leg.type != "IF" && leg.type != "DF" && !(leg.type == "TF" && leg.course == 0.f))
  if(leg.course != 0.f && leg.type != maptypes::INITIAL_FIX && leg.type != maptypes::CONSTANT_RADIUS_ARC &&
     leg.type != maptypes::ARC_TO_FIX)
    courseStr = QLocale().toString(leg.course, 'f', 0) + (leg.trueCourse ? tr("°T") : tr("°M"));
  return courseStr;
}

QString ApproachTreeController::buildDistanceStr(const MapApproachLeg& leg)
{
  if(leg.calculatedDistance > 0.f && leg.type != maptypes::INITIAL_FIX)
    return Unit::distNm(leg.calculatedDistance);
  else
    return QString();
}

QString ApproachTreeController::buildRemarkStr(const MapApproachLeg& leg)
{
  QStringList remarks;
  if(leg.flyover)
    remarks.append(tr("Fly over"));

  if(leg.turnDirection == "R")
    remarks.append(tr("Turn right"));
  else if(leg.turnDirection == "L")
    remarks.append(tr("Turn left"));
  else if(leg.turnDirection == "B")
    remarks.append(tr("Turn both"));

  QString legremarks = maptypes::approachLegRemarks(leg.type);
  if(!legremarks.isEmpty())
    remarks.append(legremarks);

  if(!leg.recFixIdent.isEmpty())
  {
    if(leg.rho > 0.f)
      remarks.append(tr("%1 / %2 / %3").arg(leg.recFixIdent).
                     arg(Unit::distNm(leg.rho /*, true, 20, true*/)).
                     arg(QLocale().toString(leg.theta) + (leg.trueCourse ? tr("°T") : tr("°M"))));
    else
      remarks.append(tr("%1").arg(leg.recFixIdent));
  }

  if(!leg.remarks.isEmpty())
    remarks.append(leg.remarks);

  if(!leg.fixIdent.isEmpty() && !leg.fixPos.isValid())
    remarks.append(tr("Data error: Fix %1/%2 not found").
                   arg(leg.fixIdent).arg(leg.fixRegion));
  if(!leg.recFixIdent.isEmpty() && !leg.recFixPos.isValid())
    remarks.append(tr("Data error: Recommended fix %1/%2 not found").
                   arg(leg.recFixIdent).arg(leg.recFixRegion));

  return remarks.join(", ");
}

QBitArray ApproachTreeController::saveTreeViewState()
{
  QList<const QTreeWidgetItem *> itemStack;
  const QTreeWidgetItem *root = treeWidget->invisibleRootItem();

  QBitArray state;

  if(!itemIndex.isEmpty())
  {
    for(int i = 0; i < root->childCount(); ++i)
      itemStack.append(root->child(i));

    int itemIdx = 0;
    while(!itemStack.isEmpty())
    {
      const QTreeWidgetItem *item = itemStack.takeFirst();

      if(item->type() < itemIndex.size() && itemIndex.at(item->type()).legId != -1)
        // Do not save legs
        continue;

      bool selected = item->isSelected();

      // Check if a leg is selected and push selection status down to the approach or transition
      // This avoids the need of expanding during loading which messes up the order
      for(int i = 0; i < item->childCount(); i++)
      {
        if(itemIndex.at(item->child(i)->type()).legId != -1 && item->child(i)->isSelected())
        {
          selected = true;
          break;
        }
      }

      state.resize(itemIdx + 2);
      state.setBit(itemIdx, item->isExpanded()); // Fist bit in triple: expanded or not
      state.setBit(itemIdx + 1, selected); // Second bit: selection state

      qDebug() << item->text(0) << item->text(1) << "expanded" << item->isExpanded() << "selected" <<
      item->isSelected() << "child" << item->childCount();

      for(int i = 0; i < item->childCount(); ++i)
        itemStack.append(item->child(i));
      itemIdx += 2;
    }
  }
  return state;
}

void ApproachTreeController::restoreTreeViewState(const QBitArray& state)
{
  QList<QTreeWidgetItem *> itemStack;
  const QTreeWidgetItem *root = treeWidget->invisibleRootItem();

  // Find selected and expanded items first without tree modification to keep order
  for(int i = 0; i < root->childCount(); ++i)
    itemStack.append(root->child(i));
  int itemIdx = 0;
  QVector<QTreeWidgetItem *> itemsToExpand;
  QTreeWidgetItem *selectedItem = nullptr;
  while(!itemStack.isEmpty())
  {
    QTreeWidgetItem *item = itemStack.takeFirst();
    if(item != nullptr && itemIdx < state.size() - 1)
    {
      if(state.at(itemIdx))
        itemsToExpand.append(item);
      if(state.at(itemIdx + 1))
        selectedItem = item;

      for(int i = 0; i < item->childCount(); ++i)
        itemStack.append(item->child(i));
      itemIdx += 2;
    }
  }

  // Expand and possibly reload
  for(QTreeWidgetItem *item : itemsToExpand)
    item->setExpanded(true);

  // Center the selected item
  if(selectedItem != nullptr)
  {
    selectedItem->setSelected(true);
    treeWidget->scrollToItem(selectedItem, QAbstractItemView::PositionAtCenter);
  }
}
