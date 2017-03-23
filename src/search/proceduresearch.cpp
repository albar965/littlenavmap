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

#include "search/proceduresearch.h"

#include "common/unit.h"
#include "common/mapcolors.h"
#include "gui/mainwindow.h"
#include "common/infoquery.h"
#include "common/procedurequery.h"
#include "sql/sqlrecord.h"
#include "ui_mainwindow.h"
#include "gui/actiontextsaver.h"
#include "common/constants.h"
#include "settings/settings.h"
#include "gui/widgetstate.h"
#include "mapgui/mapquery.h"
#include "common/htmlinfobuilder.h"
#include "util/htmlbuilder.h"
#include "common/symbolpainter.h"
#include "gui/itemviewzoomhandler.h"
#include "route/route.h"
#include "gui/griddelegate.h"
#include "geo/calculations.h"
#include "gui/dialog.h"

#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QTreeWidget>
#include <QUrlQuery>

enum TreeColumnIndex
{
  /* Column order for approch overview (tree view) */
  COL_DESCRIPTION = 0,
  COL_IDENT = 1,
  COL_ALTITUDE = 2,
  COL_COURSE = 3,
  COL_DISTANCE = 4,
  COL_REMARKS = 5,
  INVALID = -1
};

using atools::sql::SqlRecord;
using atools::sql::SqlRecordVector;
using maptypes::MapProcedureLeg;
using maptypes::MapProcedureLegs;
using maptypes::MapProcedureRef;
using atools::gui::WidgetState;
using atools::gui::ActionTextSaver;

class TreeEventFilter :
  public QObject
{

public:
  TreeEventFilter(QTreeWidget *parent)
    : QObject(parent), tree(parent)
  {
  }

  virtual ~TreeEventFilter()
  {

  }

private:
  bool eventFilter(QObject *object, QEvent *event)
  {
    if(event->type() == QEvent::MouseButtonPress)
    {
      QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
      if(mouseEvent != nullptr && mouseEvent->button() == Qt::LeftButton)
      {
        QTreeWidgetItem *item = tree->itemAt(mouseEvent->pos());
        if(item == nullptr)
          tree->clearSelection();
      }
    }

    return QObject::eventFilter(object, event);
  }

  QTreeWidget *tree;
};

ProcedureSearch::ProcedureSearch(MainWindow *main, QTreeWidget *treeWidgetParam, int tabWidgetIndex)
  : AbstractSearch(main,
                   tabWidgetIndex), infoQuery(main->getInfoQuery()), procedureQuery(main->getApproachQuery()),
    treeWidget(treeWidgetParam), mainWindow(main)
{
  zoomHandler = new atools::gui::ItemViewZoomHandler(treeWidget);
  gridDelegate = new atools::gui::GridDelegate(treeWidget);

  treeWidget->setItemDelegate(gridDelegate);

  Ui::MainWindow *ui = mainWindow->getUi();
  connect(treeWidget, &QTreeWidget::itemSelectionChanged, this, &ProcedureSearch::itemSelectionChanged);
  connect(treeWidget, &QTreeWidget::itemDoubleClicked, this, &ProcedureSearch::itemDoubleClicked);
  connect(treeWidget, &QTreeWidget::itemExpanded, this, &ProcedureSearch::itemExpanded);
  connect(treeWidget, &QTreeWidget::customContextMenuRequested, this, &ProcedureSearch::contextMenu);
  connect(ui->comboBoxProcedureSearchFilter, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
          this, &ProcedureSearch::filterIndexChanged);
  connect(ui->comboBoxProcedureRunwayFilter, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
          this, &ProcedureSearch::filterIndexRunwayChanged);

  connect(ui->dockWidgetSearch, &QDockWidget::visibilityChanged, this, &ProcedureSearch::dockVisibilityChanged);

  // Load text size from options
  zoomHandler->zoomPercent(OptionData::instance().getGuiSearchTableTextSize());

  createFonts();

  ui->labelProcedureSearch->setText(tr("No Airport selected."));

  treeEventFilter = new TreeEventFilter(treeWidget);
  treeWidget->viewport()->installEventFilter(treeEventFilter);

  connect(ui->actionSearchResetSearch, &QAction::triggered, this, &ProcedureSearch::resetSearch);

}

ProcedureSearch::~ProcedureSearch()
{
  delete zoomHandler;
  treeWidget->setItemDelegate(nullptr);
  treeWidget->viewport()->removeEventFilter(treeEventFilter);
  delete treeEventFilter;
  delete gridDelegate;
}

void ProcedureSearch::resetSearch()
{
  Ui::MainWindow *ui = mainWindow->getUi();
  if(ui->tabWidgetSearch->currentIndex() == tabIndex)
  {
    // Only reset if this tab is active
    ui->comboBoxProcedureRunwayFilter->setCurrentIndex(0);
    ui->comboBoxProcedureSearchFilter->setCurrentIndex(0);
  }
}

void ProcedureSearch::filterIndexChanged(int index)
{
  qDebug() << Q_FUNC_INFO;
  filterIndex = static_cast<FilterIndex>(index);

  treeWidget->clearSelection();
  fillApproachTreeWidget();
}

void ProcedureSearch::filterIndexRunwayChanged(int index)
{
  qDebug() << Q_FUNC_INFO;
  Q_UNUSED(index);

  treeWidget->clearSelection();
  fillApproachTreeWidget();
}

void ProcedureSearch::optionsChanged()
{
  // Adapt table view text size
  zoomHandler->zoomPercent(OptionData::instance().getGuiSearchTableTextSize());
  createFonts();
  updateTreeHeader();
  fillApproachTreeWidget();
}

void ProcedureSearch::preDatabaseLoad()
{
  // Clear display on map
  emit procedureSelected(maptypes::MapProcedureRef());
  emit procedureLegSelected(maptypes::MapProcedureRef());

  treeWidget->clear();

  itemIndex.clear();
  itemLoadedIndex.clear();
  currentAirport.position = atools::geo::Pos();
  recentTreeState.clear();
}

void ProcedureSearch::postDatabaseLoad()
{
  updateTreeHeader();

  WidgetState(lnm::APPROACHTREE_WIDGET).restore(treeWidget);
  procedureQuery->setCurrentSimulator(mainWindow->getCurrentSimulator());
}

void ProcedureSearch::showProcedures(maptypes::MapAirport airport)
{
  Ui::MainWindow *ui = mainWindow->getUi();
  ui->dockWidgetSearch->show();
  ui->dockWidgetSearch->raise();
  ui->tabWidgetSearch->setCurrentIndex(2);
  treeWidget->setFocus();

  if(currentAirport.isValid() && currentAirport.id == airport.id)
    // Ignore if noting has changed - or jump out of the view mode
    return;

  emit procedureLegSelected(maptypes::MapProcedureRef());
  emit procedureSelected(maptypes::MapProcedureRef());

  // Put state on stack and update tree
  if(currentAirport.isValid())
    recentTreeState.insert(currentAirport.id, saveTreeViewState());

  currentAirport = airport;

  fillRunwayFilter();
  fillApproachTreeWidget();

  restoreTreeViewState(recentTreeState.value(currentAirport.id));
  updateHeaderLabel();
}

void ProcedureSearch::updateHeaderLabel()
{
  QString procs;

  QList<QTreeWidgetItem *> items = treeWidget->selectedItems();
  for(QTreeWidgetItem *item : items)
  {
    MapProcedureRef ref = itemIndex.at(item->type());
    if(ref.isLeg())
    {
      item = item->parent();
      ref = itemIndex.at(item->type());
    }

    if(item != nullptr)
    {
      if(ref.isApproachAndTransition())
      {
        QTreeWidgetItem *appr = item->parent();
        if(appr != nullptr)
          procs.append(" " + appr->text(COL_DESCRIPTION) + " " + appr->text(COL_IDENT));
      }
      procs.append(" " + item->text(COL_DESCRIPTION) + " " + item->text(COL_IDENT));
    }
  }

  if(currentAirport.isValid())
    mainWindow->getUi()->labelProcedureSearch->setText(
      "<b>" + maptypes::airportTextShort(currentAirport) + "</b> " + procs);
  else
    mainWindow->getUi()->labelProcedureSearch->setText(tr("No Airport selected."));
}

void ProcedureSearch::clearRunwayFilter()
{
  Ui::MainWindow *ui = mainWindow->getUi();

  ui->comboBoxProcedureRunwayFilter->blockSignals(true);
  ui->comboBoxProcedureRunwayFilter->setCurrentIndex(0);
  ui->comboBoxProcedureRunwayFilter->clear();
  ui->comboBoxProcedureRunwayFilter->addItem(tr("All Runways"));
  ui->comboBoxProcedureRunwayFilter->blockSignals(false);
}

void ProcedureSearch::fillRunwayFilter()
{
  clearRunwayFilter();

  if(currentAirport.isValid())
  {
    Ui::MainWindow *ui = mainWindow->getUi();

    // Add a tree of transitions and approaches
    const SqlRecordVector *recAppVector = infoQuery->getApproachInformation(currentAirport.id);

    if(recAppVector != nullptr) // Deduplicate runways
    {
      QSet<QString> runways;
      for(const SqlRecord& recApp : *recAppVector)
        runways.insert(recApp.valueStr("runway_name"));

      // Sort list of runways
      QList<QString> runwaylist = runways.toList();
      std::sort(runwaylist.begin(), runwaylist.end());

      for(const QString& rw : runwaylist)
      {
        if(rw.isEmpty())
          ui->comboBoxProcedureRunwayFilter->addItem(tr("No Runway"), rw);
        else
          ui->comboBoxProcedureRunwayFilter->addItem(tr("Runway %1").arg(rw), rw);
      }
    }
    else
      qWarning() << Q_FUNC_INFO << "nothing found for airport id" << currentAirport.id;
  }
}

void ProcedureSearch::fillApproachTreeWidget()
{
  treeWidget->blockSignals(true);
  treeWidget->clear();
  itemIndex.clear();
  itemLoadedIndex.clear();

  bool foundItems = false;
  if(currentAirport.isValid())
  {
    // Add a tree of transitions and approaches
    const SqlRecordVector *recAppVector = infoQuery->getApproachInformation(currentAirport.id);

    if(recAppVector != nullptr)
    {
      Ui::MainWindow *ui = mainWindow->getUi();
      QTreeWidgetItem *root = treeWidget->invisibleRootItem();

      for(const SqlRecord& recApp : *recAppVector)
      {
        foundItems = true;
        int runwayEndId = recApp.valueInt("runway_end_id");
        int apprId = recApp.valueInt("approach_id");
        QString rwname(recApp.valueStr("runway_name"));

        const SqlRecordVector *recTransVector = infoQuery->getTransitionInformation(recApp.valueInt("approach_id"));
        maptypes::MapObjectTypes type = buildType(recApp);

        bool filterOk = false;
        switch(filterIndex)
        {
          case ProcedureSearch::FILTER_ALL_PROCEDURES:
            filterOk = true;
            break;
          case ProcedureSearch::FILTER_DEPARTURE_PROCEDURES:
            filterOk = type & maptypes::PROCEDURE_DEPARTURE;
            break;
          case ProcedureSearch::FILTER_ARRIVAL_PROCEDURES:
            filterOk = type & maptypes::PROCEDURE_ARRIVAL_ALL;
            break;
          case ProcedureSearch::FILTER_APPROACH_AND_TRANSITIONS:
            filterOk = type & maptypes::PROCEDURE_ARRIVAL;
            break;
        }

        QString rwnamefilter = ui->comboBoxProcedureRunwayFilter->currentData(Qt::UserRole).toString();
        filterOk &= rwname == rwnamefilter || ui->comboBoxProcedureRunwayFilter->currentIndex() == 0;

        if(filterOk)
        {
          int transitionId = -1;
          if(type == maptypes::PROCEDURE_SID)
            transitionId = recTransVector->first().valueInt("transition_id");

          itemIndex.append(MapProcedureRef(currentAirport.id, runwayEndId, apprId, transitionId, -1, type));

          QTreeWidgetItem *apprItem = buildApproachItem(root, recApp);

          if(type != maptypes::PROCEDURE_SID)
          {
            if(recTransVector != nullptr)
            {
              // Transitions for this approach
              for(const SqlRecord& recTrans : *recTransVector)
              {
                itemIndex.append(MapProcedureRef(currentAirport.id, runwayEndId, apprId,
                                                 recTrans.valueInt("transition_id"), -1, type));
                buildTransitionItem(apprItem, recTrans);
              }
            }
          }
        }
      }
    }
    itemLoadedIndex.resize(itemIndex.size());
  }

  if(itemIndex.isEmpty())
  {
    QString message;
    if(!currentAirport.isValid())
      message = tr("No airport selected.");
    else
    {
      if(foundItems)
        message = tr("No procedures found.");
      else
        message = tr("%1 has no procedures.").arg(maptypes::airportText(currentAirport));
    }

    QTreeWidgetItem *item = new QTreeWidgetItem(treeWidget->invisibleRootItem(), {message});
    item->setDisabled(true);
    item->setFirstColumnSpanned(true);
  }
  treeWidget->blockSignals(false);

}

void ProcedureSearch::saveState()
{
  Ui::MainWindow *ui = mainWindow->getUi();
  WidgetState(lnm::APPROACHTREE_WIDGET).save({ui->comboBoxProcedureSearchFilter,
                                              ui->comboBoxProcedureRunwayFilter});

  atools::settings::Settings& settings = atools::settings::Settings::instance();

  // Use current state and update the map too
  QBitArray state = saveTreeViewState();
  recentTreeState.insert(currentAirport.id, state);
  settings.setValueVar(lnm::APPROACHTREE_STATE, state);

  // Save column order and width
  WidgetState(lnm::APPROACHTREE_WIDGET).save(treeWidget);
  settings.setValue(lnm::APPROACHTREE_AIRPORT, currentAirport.id);
}

void ProcedureSearch::restoreState()
{
  atools::settings::Settings& settings = atools::settings::Settings::instance();
  mainWindow->getMapQuery()->getAirportById(currentAirport, settings.valueInt(lnm::APPROACHTREE_AIRPORT, -1));
  fillRunwayFilter();

  Ui::MainWindow *ui = mainWindow->getUi();
  WidgetState(lnm::APPROACHTREE_WIDGET).restore({ui->comboBoxProcedureSearchFilter,
                                                 ui->comboBoxProcedureRunwayFilter});

  fillApproachTreeWidget();

  QBitArray state = settings.valueVar(lnm::APPROACHTREE_STATE).toBitArray();
  recentTreeState.insert(currentAirport.id, state);

  updateTreeHeader();
  WidgetState(lnm::APPROACHTREE_WIDGET).restore(treeWidget);

  // Restoring state will emit above signal
  restoreTreeViewState(state);
  updateHeaderLabel();
}

void ProcedureSearch::updateTreeHeader()
{
  QTreeWidgetItem *header = new QTreeWidgetItem();

  header->setText(COL_DESCRIPTION, tr("Description"));
  header->setText(COL_IDENT, tr("Ident"));
  header->setText(COL_ALTITUDE, tr("Restriction\n%1").arg(Unit::getUnitAltStr()));
  header->setText(COL_COURSE, tr("Course\n°M"));
  header->setText(COL_DISTANCE, tr("Dist./Time\n%1/min").arg(Unit::getUnitDistStr()));
  header->setText(COL_REMARKS, tr("Remarks"));

  for(int col = COL_DESCRIPTION; col <= COL_REMARKS; col++)
    header->setTextAlignment(col, Qt::AlignCenter);

  treeWidget->setHeaderItem(header);
}

void ProcedureSearch::itemSelectionChanged()
{
  QList<QTreeWidgetItem *> items = treeWidget->selectedItems();
  if(items.isEmpty() || mainWindow->getUi()->tabWidgetSearch->currentIndex() != tabIndex)
  {
    emit procedureSelected(maptypes::MapProcedureRef());

    emit procedureLegSelected(maptypes::MapProcedureRef());
  }
  else
  {
    for(QTreeWidgetItem *item : items)
    {
      const MapProcedureRef& ref = itemIndex.at(item->type());

      qDebug() << Q_FUNC_INFO << ref.runwayEndId << ref.approachId << ref.transitionId << ref.legId;

      if(ref.isApproachOrTransition())
        emit procedureSelected(ref);

      if(ref.isLeg())
        emit procedureLegSelected(ref);
      else
        emit procedureLegSelected(maptypes::MapProcedureRef());

      if(ref.isApproachAndTransition())
        updateApproachItem(parentApproachItem(item), ref.transitionId);
    }
  }

  updateHeaderLabel();
}

/* Update course and distance for the parent approach of this leg item */
void ProcedureSearch::updateApproachItem(QTreeWidgetItem *apprItem, int transitionId)
{
  if(apprItem != nullptr)
  {
    for(int i = 0; i < apprItem->childCount(); i++)
    {
      QTreeWidgetItem *child = apprItem->child(i);
      const MapProcedureRef& childref = itemIndex.at(child->type());
      if(childref.isLeg())
      {
        const maptypes::MapProcedureLegs *legs = procedureQuery->getTransitionLegs(currentAirport, transitionId);
        if(legs != nullptr)
        {
          const maptypes::MapProcedureLeg *aleg = legs->approachLegById(childref.legId);

          if(aleg != nullptr)
          {
            child->setText(COL_COURSE, maptypes::procedureLegCourse(*aleg));
            child->setText(COL_DISTANCE, maptypes::procedureLegDistance(*aleg));
          }
          else
            qWarning() << "Approach legs not found" << childref.legId;
        }
        else
          qWarning() << "Transition not found" << transitionId;
      }
    }
  }
}

void ProcedureSearch::itemDoubleClicked(QTreeWidgetItem *item, int column)
{
  Q_UNUSED(column);
  showEntry(item, true);
}

void ProcedureSearch::itemExpanded(QTreeWidgetItem *item)
{
  if(item != nullptr)
  {
    if(itemLoadedIndex.at(item->type()))
      return;

    // Get a copy since vector is rebuilt underneath
    const MapProcedureRef ref = itemIndex.at(item->type());

    if(ref.legId == -1)
    {
      if(ref.approachId != -1 && ref.transitionId == -1)
      {
        const MapProcedureLegs *legs = procedureQuery->getApproachLegs(currentAirport, ref.approachId);
        if(legs != nullptr)
        {
          addApproachLegs(legs, item, -1);
          itemLoadedIndex.setBit(item->type());
        }
        else
          qWarning() << Q_FUNC_INFO << "no legs found for" << currentAirport.id << ref.approachId;
      }
      else if(ref.approachId != -1 && ref.transitionId != -1)
      {
        const MapProcedureLegs *legs = procedureQuery->getTransitionLegs(currentAirport, ref.transitionId);
        if(legs != nullptr)
        {
          addTransitionLegs(legs, item);
          itemLoadedIndex.setBit(item->type());
        }
        else
          qWarning() << Q_FUNC_INFO << "no legs found for" << currentAirport.id << ref.transitionId;
      }
      itemLoadedIndex.resize(itemIndex.size());
    }
  }
}

void ProcedureSearch::addApproachLegs(const MapProcedureLegs *legs, QTreeWidgetItem *item, int transitionId)
{
  if(legs != nullptr)
  {
    for(const MapProcedureLeg& leg : legs->approachLegs)
    {
      itemIndex.append(MapProcedureRef(legs->ref.airportId, legs->ref.runwayEndId, legs->ref.approachId, transitionId,
                                       leg.legId, legs->mapType));
      buildLegItem(item, leg);
    }
  }
}

void ProcedureSearch::addTransitionLegs(const MapProcedureLegs *legs, QTreeWidgetItem *item)
{
  if(legs != nullptr)
  {
    for(const MapProcedureLeg& leg : legs->transitionLegs)
    {
      itemIndex.append(MapProcedureRef(legs->ref.airportId, legs->ref.runwayEndId, legs->ref.approachId,
                                       legs->ref.transitionId, leg.legId, legs->mapType));
      buildLegItem(item, leg);
    }
  }
}

void ProcedureSearch::contextMenu(const QPoint& pos)
{
  qDebug() << Q_FUNC_INFO;

  QPoint menuPos = QCursor::pos();
  // Use widget center if position is not inside widget
  if(!treeWidget->rect().contains(treeWidget->mapFromGlobal(QCursor::pos())))
    menuPos = treeWidget->mapToGlobal(treeWidget->rect().center());

  // Save text which will be changed below
  Ui::MainWindow *ui = mainWindow->getUi();
  ActionTextSaver saver({ui->actionInfoApproachShow, ui->actionInfoApproachAttach});
  Q_UNUSED(saver);

  QTreeWidgetItem *item = treeWidget->itemAt(pos);
  MapProcedureRef ref;
  if(item != nullptr)
    ref = itemIndex.at(item->type());

  ui->actionInfoApproachClear->setEnabled(treeWidget->selectionModel()->hasSelection());
  ui->actionInfoApproachShow->setDisabled(item == nullptr);

  const Route& route = mainWindow->getRoute();

  ui->actionInfoApproachAttach->setDisabled(item == nullptr);

  QString text, showText;

  if(item != nullptr)
  {
    QTreeWidgetItem *parentAppr = parentApproachItem(item);
    QTreeWidgetItem *parentTrans = parentTransitionItem(item);

    if(ref.isSid())
      text = parentTrans->text(COL_DESCRIPTION) + " " + parentTrans->text(COL_IDENT);
    else if(ref.isApproachAndTransition())
    {
      text = parentAppr->text(COL_DESCRIPTION) + " " + parentAppr->text(COL_IDENT) +
             tr(" and ") +
             parentTrans->text(COL_DESCRIPTION) + " " + parentTrans->text(COL_IDENT);
    }
    else if(ref.isApproachOnly())
      text = parentAppr->text(COL_DESCRIPTION) + " " + parentAppr->text(COL_IDENT);

    if(!text.isEmpty())
      ui->actionInfoApproachShow->setEnabled(true);

    if(ref.isLeg())
      showText = item->text(COL_IDENT).isEmpty() ? tr("Position") : item->text(COL_IDENT);
    else
      showText = text;

    ui->actionInfoApproachShow->setText(ui->actionInfoApproachShow->text().arg(showText));

    if((route.hasValidDeparture() && route.first().getId() == currentAirport.id) ||
       (route.hasValidDestination() && route.last().getId() == currentAirport.id))
      ui->actionInfoApproachAttach->setText(tr("Insert %1 into Flight Plan").arg(text));
    else
    {
      if(ref.mapType & maptypes::PROCEDURE_ARRIVAL_ALL)
        ui->actionInfoApproachAttach->setText(tr("Use %1 and %2 as Destination").arg(currentAirport.ident).arg(text));
      else if(ref.mapType & maptypes::PROCEDURE_DEPARTURE)
        ui->actionInfoApproachAttach->setText(tr("Use %1 and %2 as Departure").arg(currentAirport.ident).arg(text));
    }
  }
  else
  {
    ui->actionInfoApproachAttach->setText(ui->actionInfoApproachAttach->text().arg(tr("Procedure")));
    ui->actionInfoApproachShow->setText(ui->actionInfoApproachShow->text().arg(tr("Procedure")));
  }

  QMenu menu;
  menu.addAction(ui->actionInfoApproachShow);
  menu.addSeparator();
  menu.addAction(ui->actionInfoApproachAttach);
  menu.addSeparator();
  menu.addAction(ui->actionInfoApproachExpandAll);
  menu.addAction(ui->actionInfoApproachCollapseAll);
  menu.addSeparator();
  menu.addAction(ui->actionSearchResetSearch);
  menu.addAction(ui->actionInfoApproachClear);
  menu.addAction(ui->actionSearchResetView);

  QAction *action = menu.exec(menuPos);
  if(action == ui->actionInfoApproachExpandAll)
  {
    const QTreeWidgetItem *root = treeWidget->invisibleRootItem();
    for(int i = 0; i < root->childCount(); ++i)
      root->child(i)->setExpanded(true);
  }
  else if(action == ui->actionSearchResetView)
  {
    resetSearch();
    for(int i = 0; i < treeWidget->columnCount(); i++)
      treeWidget->resizeColumnToContents(i);
    mainWindow->setStatusMessage(tr("Table view reset to defaults."));
  }
  else if(action == ui->actionInfoApproachCollapseAll)
    treeWidget->collapseAll();
  else if(action == ui->actionInfoApproachClear)
  {
    treeWidget->clearSelection();
    emit procedureLegSelected(maptypes::MapProcedureRef());
    emit procedureSelected(maptypes::MapProcedureRef());
  }
  else if(action == ui->actionInfoApproachShow)
    showEntry(item, false);
  else if(action == ui->actionInfoApproachAttach)
  {
    // Get the aproach legs for the initial fix
    const maptypes::MapProcedureLegs *legs = nullptr;
    if(ref.isApproachOnly())
      legs = procedureQuery->getApproachLegs(currentAirport, ref.approachId);
    else if(ref.isApproachAndTransition())
      legs = procedureQuery->getTransitionLegs(currentAirport, ref.transitionId);

    if(legs != nullptr)
    {
      if(legs->hasError)
      {
        int result = atools::gui::Dialog(mainWindow).
                     showQuestionMsgBox(lnm::ACTIONS_SHOW_INVALID_PROC_WARNING,
                                        tr("Procedure has errors and will not display correctly.\n"
                                           "Really use it?"),
                                        tr("Do not &show this dialog again."),
                                        QMessageBox::Yes | QMessageBox::No,
                                        QMessageBox::No, QMessageBox::Yes);

        if(result == QMessageBox::Yes)
          emit routeInsertProcedure(*legs);
      }
      else
        emit routeInsertProcedure(*legs);
    }
    else
      qDebug() << Q_FUNC_INFO << "legs not found";
  }

  // Done by the actions themselves
  // else if(action == ui->actionInfoApproachShowAppr ||
  // action == ui->actionInfoApproachShowMissedAppr ||
  // action == ui->actionInfoApproachShowTrans)
}

void ProcedureSearch::showEntry(QTreeWidgetItem *item, bool doubleClick)
{
  if(item == nullptr)
    return;

  const MapProcedureRef& ref = itemIndex.at(item->type());

  if(ref.legId != -1)
  {
    const maptypes::MapProcedureLeg *leg = nullptr;

    if(ref.transitionId != -1)
      leg = procedureQuery->getTransitionLeg(currentAirport, ref.legId);
    else if(ref.approachId != -1)
      leg = procedureQuery->getApproachLeg(currentAirport, ref.approachId, ref.legId);

    if(leg != nullptr)
    {
      emit showPos(leg->line.getPos2(), 0.f, doubleClick);

      if(doubleClick && (leg->navaids.hasNdb() || leg->navaids.hasVor() || leg->navaids.hasWaypoints()))
        emit showInformation(leg->navaids);
    }
  }
  else if(ref.transitionId != -1 && !doubleClick)
  {
    const maptypes::MapProcedureLegs *legs = procedureQuery->getTransitionLegs(currentAirport, ref.transitionId);
    if(legs != nullptr)
      emit showRect(legs->bounding, doubleClick);
  }
  else if(ref.approachId != -1 && !doubleClick)
  {
    const maptypes::MapProcedureLegs *legs = procedureQuery->getApproachLegs(currentAirport, ref.approachId);
    if(legs != nullptr)
      emit showRect(legs->bounding, doubleClick);
  }

}

maptypes::MapObjectTypes ProcedureSearch::buildType(const SqlRecord& recApp)
{
  return maptypes::procedureType(mainWindow->getCurrentSimulator(),
                                 recApp.valueStr("type"), recApp.valueStr("suffix"), recApp.valueBool("has_gps_overlay"));
}

QTreeWidgetItem *ProcedureSearch::buildApproachItem(QTreeWidgetItem *runwayItem, const SqlRecord& recApp)
{
  QString suffix(recApp.valueStr("suffix"));
  QString type(recApp.valueStr("type"));
  int gpsOverlay = recApp.valueBool("has_gps_overlay");

  QString approachType;

  maptypes::MapObjectTypes maptype = buildType(recApp);
  if(maptype == maptypes::PROCEDURE_SID)
    approachType += tr("SID");
  else if(maptype == maptypes::PROCEDURE_STAR)
    approachType += tr("STAR");
  else if(maptype == maptypes::PROCEDURE_APPROACH)
  {
    approachType = tr("Approach ") + maptypes::procedureType(type);

    if(!suffix.isEmpty())
      approachType += " " + suffix;

    if(gpsOverlay)
      approachType += tr(" (GPS Overlay)");
  }

  approachType += " " + recApp.valueStr("runway_name");

  QString altStr;
  if(recApp.valueFloat("altitude") > 0.f)
    altStr = Unit::altFeet(recApp.valueFloat("altitude"), false);

  QTreeWidgetItem *item = new QTreeWidgetItem({
                                                approachType,
                                                recApp.valueStr("fix_ident"),
                                                altStr
                                              }, itemIndex.size() - 1);
  item->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
  item->setTextAlignment(COL_ALTITUDE, Qt::AlignRight);
  item->setTextAlignment(COL_COURSE, Qt::AlignRight);
  item->setTextAlignment(COL_DISTANCE, Qt::AlignRight);

  for(int i = 0; i < item->columnCount(); i++)
    item->setFont(i, approachFont);

  runwayItem->addChild(item);

  return item;
}

QTreeWidgetItem *ProcedureSearch::buildTransitionItem(QTreeWidgetItem *apprItem, const SqlRecord& recTrans)
{
  QString altStr;
  if(recTrans.valueFloat("altitude") > 0.f)
    altStr = Unit::altFeet(recTrans.valueFloat("altitude"), false);

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
  item->setTextAlignment(COL_ALTITUDE, Qt::AlignRight);
  item->setTextAlignment(COL_COURSE, Qt::AlignRight);
  item->setTextAlignment(COL_DISTANCE, Qt::AlignRight);

  for(int i = 0; i < item->columnCount(); i++)
    item->setFont(i, transitionFont);

  apprItem->addChild(item);

  return item;
}

void ProcedureSearch::buildLegItem(QTreeWidgetItem *parentItem, const MapProcedureLeg& leg)
{
  QStringList texts;
  QIcon icon;
  int fontHeight = treeWidget->fontMetrics().height();
  texts << maptypes::procedureLegTypeStr(leg.type) << leg.fixIdent;

  QString remarkStr = maptypes::procedureLegRemark(leg);
  texts << maptypes::altRestrictionTextShort(leg.altRestriction)
        << maptypes::procedureLegCourse(leg) << maptypes::procedureLegDistance(leg);

  texts << remarkStr;

  QTreeWidgetItem *item = new QTreeWidgetItem(texts, itemIndex.size() - 1);
  if(!icon.isNull())
  {
    item->setIcon(0, icon);
    item->setSizeHint(0, QSize(fontHeight - 3, fontHeight - 3));
  }

  item->setToolTip(COL_REMARKS, remarkStr);

  item->setTextAlignment(COL_ALTITUDE, Qt::AlignRight);
  item->setTextAlignment(COL_COURSE, Qt::AlignRight);
  item->setTextAlignment(COL_DISTANCE, Qt::AlignRight);

  setItemStyle(item, leg);

  parentItem->addChild(item);
}

void ProcedureSearch::setItemStyle(QTreeWidgetItem *item, const MapProcedureLeg& leg)
{
  bool invalid = leg.hasInvalidRef();

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

QBitArray ProcedureSearch::saveTreeViewState()
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

      qDebug() << item->text(COL_DESCRIPTION) << item->text(COL_IDENT)
               << "expanded" << item->isExpanded() << "selected" << item->isSelected() << "child" << item->childCount();

      for(int i = 0; i < item->childCount(); ++i)
        itemStack.append(item->child(i));
      itemIdx += 2;
    }
  }
  return state;
}

void ProcedureSearch::restoreTreeViewState(const QBitArray& state)
{
  if(state.isEmpty())
    return;

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
    treeWidget->scrollToItem(selectedItem, QAbstractItemView::PositionAtTop);
  }
}

void ProcedureSearch::createFonts()
{
  const QFont& font = treeWidget->font();
  identFont = font;
  identFont.setWeight(QFont::Bold);

  approachFont = font;
  approachFont.setWeight(QFont::Bold);

  transitionFont = font;
  transitionFont.setWeight(QFont::Bold);

  legFont = font;
  // legFont.setPointSizeF(legFont.pointSizeF() * 0.85f);

  missedLegFont = font;
  // missedLegFont.setItalic(true);

  invalidLegFont = legFont;
  invalidLegFont.setBold(true);
}

QTreeWidgetItem *ProcedureSearch::parentApproachItem(QTreeWidgetItem *item) const
{
  QTreeWidgetItem *current = item, *root = treeWidget->invisibleRootItem();
  while(current != nullptr && current != root)
  {
    const MapProcedureRef& ref = itemIndex.at(current->type());
    if(ref.isApproachOnly() && !ref.isLeg())
      return current;

    current = current->parent();
  }
  return current != nullptr ? current : item;
}

QTreeWidgetItem *ProcedureSearch::parentTransitionItem(QTreeWidgetItem *item) const
{
  QTreeWidgetItem *current = item, *root = treeWidget->invisibleRootItem();
  while(current != nullptr && current != root)
  {
    const MapProcedureRef& ref = itemIndex.at(current->type());
    if(ref.isApproachAndTransition() && !ref.isLeg())
      return current;

    current = current->parent();
  }
  return current != nullptr ? current : item;
}

void ProcedureSearch::getSelectedMapObjects(maptypes::MapSearchResult& result) const
{
  Q_UNUSED(result);
}

void ProcedureSearch::connectSearchSlots()
{
}

void ProcedureSearch::updateUnits()
{
}

void ProcedureSearch::updateTableSelection()
{
  if(mainWindow->getUi()->tabWidgetSearch->currentIndex() != tabIndex)
  {
    // Hide preview if another tab is activated
    emit procedureSelected(maptypes::MapProcedureRef());
    emit procedureLegSelected(maptypes::MapProcedureRef());
  }
  else
    itemSelectionChanged();
}

/* Update highlights if dock is hidden or shown (does not change for dock tab stacks) */
void ProcedureSearch::dockVisibilityChanged(bool visible)
{
  if(!visible)
  {
    // Hide preview of dock is closed
    emit procedureSelected(maptypes::MapProcedureRef());
    emit procedureLegSelected(maptypes::MapProcedureRef());
  }
  else
    itemSelectionChanged();
}

void ProcedureSearch::tabDeactivated()
{
  emit procedureSelected(maptypes::MapProcedureRef());
  emit procedureLegSelected(maptypes::MapProcedureRef());
}
