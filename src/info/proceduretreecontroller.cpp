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

#include "info/proceduretreecontroller.h"

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
#include "route/routecontroller.h"
#include "gui/griddelegate.h"
#include "geo/calculations.h"

#include <QMenu>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QTreeWidget>
#include <QUrlQuery>

enum TreeColumnIndex
{
  /* Column order for approch overview (tree view) */
  OVERVIEW_DESCRIPTION = 0,
  OVERVIEW_IDENT = 1,
  OVERVIEW_ALTITUDE = 2,
  OVERVIEW_COURSE = 3,
  OVERVIEW_DISTANCE = 4,
  OVERVIEW_REMARKS = 5,

  /* Column order for selected/focused approch (table view) */
  SELECTED_IDENT = 0,
  SELECTED_IDENT_TYPE = 1,
  SELECTED_IDENT_FREQUENCY = 2,
  SELECTED_LEGTYPE = 3,
  SELECTED_ALTITUDE = 4,
  SELECTED_COURSE = 5,
  SELECTED_DISTANCE = 6,
  SELECTED_REMAINING_DISTANCE = 7,
  SELECTED_REMARKS = 8,

  INVALID = -1
};

using atools::sql::SqlRecord;
using atools::sql::SqlRecordVector;
using maptypes::MapProcedureLeg;
using maptypes::MapProcedureLegs;
using maptypes::MapProcedureRef;
using atools::gui::WidgetState;
using atools::gui::ActionTextSaver;

ProcedureTreeController::ProcedureTreeController(MainWindow *main)
  : infoQuery(main->getInfoQuery()), approachQuery(main->getApproachQuery()),
    treeWidget(main->getUi()->treeWidgetApproachInfo), mainWindow(main)
{
  zoomHandler = new atools::gui::ItemViewZoomHandler(treeWidget);
  gridDelegate = new atools::gui::GridDelegate(treeWidget);

  infoBuilder = new HtmlInfoBuilder(mainWindow, true);

  treeWidget->setItemDelegate(gridDelegate);

  connect(treeWidget, &QTreeWidget::itemSelectionChanged, this, &ProcedureTreeController::itemSelectionChanged);
  connect(treeWidget, &QTreeWidget::itemDoubleClicked, this, &ProcedureTreeController::itemDoubleClicked);
  connect(treeWidget, &QTreeWidget::itemExpanded, this, &ProcedureTreeController::itemExpanded);
  connect(treeWidget, &QTreeWidget::customContextMenuRequested, this, &ProcedureTreeController::contextMenu);
  connect(mainWindow->getUi()->textBrowserApproachInfo,
          &QTextBrowser::anchorClicked, this, &ProcedureTreeController::anchorClicked);

  currentAirport.id = -1;

  // Load text size from options
  zoomHandler->zoomPercent(OptionData::instance().getGuiRouteTableTextSize());

  createFonts();

  Ui::MainWindow *ui = mainWindow->getUi();
  ui->textBrowserApproachInfo->setSearchPaths({QApplication::applicationDirPath()});

  QFont f = ui->textBrowserApproachInfo->font();
  float newSize = static_cast<float>(ui->textBrowserApproachInfo->font().pointSizeF()) *
                  OptionData::instance().getGuiInfoTextSize() / 100.f;
  if(newSize > 0.1f)
  {
    f.setPointSizeF(newSize);
    ui->textBrowserApproachInfo->setFont(f);
  }
}

ProcedureTreeController::~ProcedureTreeController()
{
  delete infoBuilder;
  delete zoomHandler;
  treeWidget->setItemDelegate(nullptr);
  delete gridDelegate;
}

void ProcedureTreeController::optionsChanged()
{
  // Adapt table view text size
  zoomHandler->zoomPercent(OptionData::instance().getGuiRouteTableTextSize());
  createFonts();
  updateTreeHeader();
  fillApproachTreeWidget();
}

void ProcedureTreeController::preDatabaseLoad()
{
  Ui::MainWindow *ui = mainWindow->getUi();
  if(approachSelectedMode)
    WidgetState(lnm::APPROACHTREE_SELECTED_WIDGET).save(ui->treeWidgetApproachInfo);

  // Clear display on map
  emit procedureSelected(maptypes::MapProcedureRef());
  emit procedureLegSelected(maptypes::MapProcedureRef());

  ui->textBrowserApproachInfo->clear();
  treeWidget->clear();

  itemIndex.clear();
  itemLoadedIndex.clear();
  currentAirport.id = -1;
  currentAirport.position = atools::geo::Pos();
  recentTreeState.clear();
  approachSelectedMode = false;
  approachSelectedLegs = maptypes::MapProcedureLegs();
}

void ProcedureTreeController::postDatabaseLoad()
{
  updateTreeHeader();

  Ui::MainWindow *ui = mainWindow->getUi();
  WidgetState(lnm::APPROACHTREE_WIDGET).restore(ui->treeWidgetApproachInfo);
  approachQuery->setCurrentSimulator(mainWindow->getCurrentSimulator());
}

void ProcedureTreeController::disconnectedFromSimulator()
{
  qDebug() << Q_FUNC_INFO;

  highlightNextWaypoint(-1);
}

void ProcedureTreeController::highlightNextWaypoint(int leg)
{
  if(approachSelectedMode)
  {
    static const QColor rowBgColor = QApplication::palette().color(QPalette::Active, QPalette::Base);
    static const QColor rowAltBgColor = QApplication::palette().color(QPalette::Active, QPalette::AlternateBase);

    QColor color = OptionData::instance().isGuiStyleDark() ?
                   mapcolors::nextWaypointColorDark : mapcolors::nextWaypointColor;

    for(int i = 0; i < treeWidget->invisibleRootItem()->childCount(); i++)
    {
      QTreeWidgetItem *item = treeWidget->invisibleRootItem()->child(i);

      for(int col = SELECTED_IDENT; col <= SELECTED_REMARKS; col++)
      {
        if(i == leg)
        {
          item->setBackground(col, color);
          item->setFont(col, activeLegFont);
        }
        else
        {
          item->setBackground(col, (i % 2) == 0 ? rowBgColor : rowAltBgColor);

          if(col == SELECTED_IDENT)
            item->setFont(col, identFont);
          else
            item->setFont(col, legFont);
        }
      }
    }
  }
}

void ProcedureTreeController::showProcedures(maptypes::MapAirport airport)
{
  Ui::MainWindow *ui = mainWindow->getUi();
  ui->dockWidgetRoute->show();
  ui->dockWidgetRoute->raise();
  ui->tabWidgetRoute->setCurrentIndex(1);

  if(currentAirport.id == airport.id && !approachSelectedMode)
    // Ignore if noting has changed - or jump out of the view mode
    return;

  emit procedureLegSelected(maptypes::MapProcedureRef());
  emit procedureSelected(maptypes::MapProcedureRef());

  if(approachSelectedMode)
  {
    currentAirport = airport;

    // Change mode
    disableSelectedMode();
  }
  else
  {
    // Put state on stack and update tree
    if(currentAirport.id != -1)
      recentTreeState.insert(currentAirport.id, saveTreeViewState());

    currentAirport = airport;

    fillApproachTreeWidget();

    restoreTreeViewState(recentTreeState.value(currentAirport.id));
  }
}

void ProcedureTreeController::anchorClicked(const QUrl& url)
{
  if(url.scheme() == "lnm")
    emit showRect(currentAirport.bounding, false);
}

void ProcedureTreeController::fillApproachInformation(const maptypes::MapAirport& airport,
                                                     const maptypes::MapProcedureRef& ref)
{
  atools::util::HtmlBuilder html(true);
  infoBuilder->approachText(airport, html, QApplication::palette().color(QPalette::Active, QPalette::Base), ref);

  Ui::MainWindow *ui = mainWindow->getUi();
  ui->textBrowserApproachInfo->clear();

  if(!ref.isEmpty())
    ui->textBrowserApproachInfo->setText(html.getHtml());
}

void ProcedureTreeController::fillApproachTreeWidget()
{
  treeWidget->blockSignals(true);
  treeWidget->clear();
  itemIndex.clear();
  itemLoadedIndex.clear();

  if(currentAirport.id != -1)
  {
    if(approachSelectedMode)
    {
      // Show information for the selected approach and/or transition
      fillApproachInformation(currentAirport, approachSelectedLegs.ref);
      treeWidget->setIndentation(0);

      // Add only legs in view mode
      QTreeWidgetItem *root = treeWidget->invisibleRootItem();
      if(approachSelectedLegs.ref.isApproachOnly())
      {
        const MapProcedureLegs *legs = approachQuery->getApproachLegs(currentAirport,
                                                                     approachSelectedLegs.ref.approachId);
        float remainingDistance = legs->approachDistance;
        if(legs != nullptr)
        {
          approachSelectedLegs = *legs;

          addApproachLegs(&approachSelectedLegs, root, -1, remainingDistance);
        }
      }
      else if(approachSelectedLegs.ref.isApproachAndTransition())
      {
        const MapProcedureLegs *legs = approachQuery->getTransitionLegs(currentAirport,
                                                                       approachSelectedLegs.ref.transitionId);
        float remainingDistance = legs->approachDistance + legs->transitionDistance;
        if(legs != nullptr)
        {
          approachSelectedLegs = *legs;
          addTransitionLegs(&approachSelectedLegs, root, remainingDistance);
          addApproachLegs(&approachSelectedLegs, root, approachSelectedLegs.ref.transitionId, remainingDistance);
        }
      }
    }
    else
    {
      // Show all approaches in the text information
      fillApproachInformation(currentAirport, maptypes::MapProcedureRef());

      // Reset table like style back to plain tree
      treeWidget->resetIndentation();

      // Add a tree of transitions and approaches
      const SqlRecordVector *recAppVector = infoQuery->getApproachInformation(currentAirport.id);
      if(recAppVector != nullptr)
      {
        QTreeWidgetItem *root = treeWidget->invisibleRootItem();

        for(const SqlRecord& recApp : *recAppVector)
        {
          int runwayEndId = recApp.valueInt("runway_end_id");
          int apprId = recApp.valueInt("approach_id");

          const SqlRecordVector *recTransVector = infoQuery->getTransitionInformation(recApp.valueInt("approach_id"));
          maptypes::MapObjectTypes type = buildType(recApp);

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
      itemLoadedIndex.resize(itemIndex.size());
    }
  }

  if(itemIndex.isEmpty())
  {
    if(currentAirport.id == -1)
    {
      QTreeWidgetItem *item = new QTreeWidgetItem(treeWidget->invisibleRootItem(), {tr("No airport selected.")});
      item->setDisabled(true);
      item->setFirstColumnSpanned(true);
    }
    else
    {
      QTreeWidgetItem *item = new QTreeWidgetItem(treeWidget->invisibleRootItem(),
                                                  {tr("%1 has no procedures.").
                                                   arg(maptypes::airportText(currentAirport))}, -1);
      item->setDisabled(true);
      item->setFirstColumnSpanned(true);
    }
  }
  treeWidget->blockSignals(false);

}

void ProcedureTreeController::saveState()
{
  Ui::MainWindow *ui = mainWindow->getUi();
  WidgetState(lnm::APPROACHTREE_WIDGET).save({ui->actionInfoApproachShowAppr,
                                              ui->actionInfoApproachShowMissedAppr,
                                              ui->splitterApproachInfo,
                                              ui->tabWidgetRoute});

  atools::settings::Settings& settings = atools::settings::Settings::instance();
  if(approachSelectedMode)
  {
    // Use last saved state before entering view mode
    settings.setValueVar(lnm::APPROACHTREE_STATE, recentTreeState.value(currentAirport.id));

    // Save column order and width
    WidgetState(lnm::APPROACHTREE_SELECTED_WIDGET).save(ui->treeWidgetApproachInfo);
  }
  else
  {
    // Use current state and update the map too
    QBitArray state = saveTreeViewState();
    recentTreeState.insert(currentAirport.id, state);
    settings.setValueVar(lnm::APPROACHTREE_STATE, state);

    // Save column order and width
    WidgetState(lnm::APPROACHTREE_WIDGET).save(ui->treeWidgetApproachInfo);
  }

  settings.setValueVar(lnm::APPROACHTREE_SELECTED_APPR + "_Mode", approachSelectedMode);
  settings.setValueVar(lnm::APPROACHTREE_SELECTED_APPR + "_AirportId", approachSelectedLegs.ref.airportId);
  settings.setValueVar(lnm::APPROACHTREE_SELECTED_APPR + "_RunwayEndId", approachSelectedLegs.ref.runwayEndId);
  settings.setValueVar(lnm::APPROACHTREE_SELECTED_APPR + "_ApproachId", approachSelectedLegs.ref.approachId);
  settings.setValueVar(lnm::APPROACHTREE_SELECTED_APPR + "_TransitionId", approachSelectedLegs.ref.transitionId);
  settings.setValueVar(lnm::APPROACHTREE_SELECTED_APPR + "_LegId", approachSelectedLegs.ref.legId);

  settings.setValue(lnm::APPROACHTREE_AIRPORT, currentAirport.id);
}

void ProcedureTreeController::restoreState()
{
  Ui::MainWindow *ui = mainWindow->getUi();
  WidgetState(lnm::APPROACHTREE_WIDGET).restore({ui->actionInfoApproachShowAppr,
                                                 ui->actionInfoApproachShowMissedAppr,
                                                 ui->splitterApproachInfo,
                                                 ui->tabWidgetRoute});

  atools::settings::Settings& settings = atools::settings::Settings::instance();

  mainWindow->getMapQuery()->getAirportById(currentAirport, settings.valueInt(lnm::APPROACHTREE_AIRPORT, -1));

  approachSelectedMode = settings.valueBool(lnm::APPROACHTREE_SELECTED_APPR + "_Mode", false);
  approachSelectedLegs.ref.airportId = settings.valueInt(lnm::APPROACHTREE_SELECTED_APPR + "_AirportId", -1);
  approachSelectedLegs.ref.runwayEndId = settings.valueInt(lnm::APPROACHTREE_SELECTED_APPR + "_RunwayEndId", -1);
  approachSelectedLegs.ref.approachId = settings.valueInt(lnm::APPROACHTREE_SELECTED_APPR + "_ApproachId", -1);
  approachSelectedLegs.ref.transitionId = settings.valueInt(lnm::APPROACHTREE_SELECTED_APPR + "_TransitionId", -1);
  approachSelectedLegs.ref.legId = settings.valueInt(lnm::APPROACHTREE_SELECTED_APPR + "_LegId", -1);

  fillApproachTreeWidget();

  QBitArray state = settings.valueVar(lnm::APPROACHTREE_STATE).toBitArray();
  recentTreeState.insert(currentAirport.id, state);

  updateTreeHeader();
  if(approachSelectedMode)
  {
    WidgetState(lnm::APPROACHTREE_SELECTED_WIDGET).restore(ui->treeWidgetApproachInfo);

    emit procedureSelected(approachSelectedLegs.ref);
  }
  else
  {
    WidgetState(lnm::APPROACHTREE_WIDGET).restore(ui->treeWidgetApproachInfo);

    // Restoring state will emit above signal
    restoreTreeViewState(state);
  }
}

void ProcedureTreeController::updateTreeHeader()
{
  QTreeWidgetItem *header = new QTreeWidgetItem();

  if(approachSelectedMode)
  {
    header->setText(SELECTED_IDENT, tr("Ident"));
    header->setText(SELECTED_IDENT_TYPE, tr("Navaid\nType"));
    header->setText(SELECTED_IDENT_FREQUENCY, tr("Freq.\nMHz/kHz"));
    header->setText(SELECTED_LEGTYPE, tr("Leg Type"));
    header->setText(SELECTED_ALTITUDE, tr("Altitude\n%1").arg(Unit::getUnitAltStr()));
    header->setText(SELECTED_COURSE, tr("Course\n°M"));
    header->setText(SELECTED_DISTANCE, tr("Dist./Time\n%1/min").arg(Unit::getUnitDistStr()));
    header->setText(SELECTED_REMAINING_DISTANCE, tr("Remaining\n%1").arg(Unit::getUnitDistStr()));
    header->setText(SELECTED_REMARKS, tr("Remarks"));

    for(int col = SELECTED_IDENT; col <= SELECTED_REMARKS; col++)
      header->setTextAlignment(col, Qt::AlignCenter);
  }
  else
  {
    header->setText(OVERVIEW_DESCRIPTION, tr("Description"));
    header->setText(OVERVIEW_IDENT, tr("Ident"));
    header->setText(OVERVIEW_ALTITUDE, tr("Altitude\n%1").arg(Unit::getUnitAltStr()));
    header->setText(OVERVIEW_COURSE, tr("Course\n°M"));
    header->setText(OVERVIEW_DISTANCE, tr("Dist./Time\n%1/min").arg(Unit::getUnitDistStr()));
    header->setText(OVERVIEW_REMARKS, tr("Remarks"));

    for(int col = OVERVIEW_DESCRIPTION; col <= OVERVIEW_REMARKS; col++)
      header->setTextAlignment(col, Qt::AlignCenter);
  }

  treeWidget->setHeaderItem(header);
}

void ProcedureTreeController::itemSelectionChanged()
{
  QList<QTreeWidgetItem *> items = treeWidget->selectedItems();
  if(items.isEmpty())
  {
    if(!approachSelectedMode)
      emit procedureSelected(maptypes::MapProcedureRef());

    emit procedureLegSelected(maptypes::MapProcedureRef());

    if(!approachSelectedMode)
      fillApproachInformation(currentAirport, maptypes::MapProcedureRef());
  }
  else
  {
    for(QTreeWidgetItem *item : items)
    {
      const MapProcedureRef& ref = itemIndex.at(item->type());

      qDebug() << Q_FUNC_INFO << ref.runwayEndId << ref.approachId << ref.transitionId << ref.legId;

      if(approachSelectedMode)
        emit procedureLegSelected(ref);
      else
      {
        if(ref.isApproachOrTransition())
          emit procedureSelected(ref);

        if(ref.isLeg())
          emit procedureLegSelected(ref);
        else
          emit procedureLegSelected(maptypes::MapProcedureRef());

        if(ref.isApproachAndTransition())
          updateApproachItem(parentApproachItem(item), ref.transitionId);
      }

      if(!approachSelectedMode)
        // Show information for selected approach/transition in the text view
        fillApproachInformation(currentAirport, ref);
    }
  }
}

/* Update course and distance for the parent approach of this leg item */
void ProcedureTreeController::updateApproachItem(QTreeWidgetItem *apprItem, int transitionId)
{
  if(apprItem != nullptr)
  {
    TreeColumnIndex courseIndex = approachSelectedMode ? SELECTED_COURSE : OVERVIEW_COURSE,
                    distanceIndex = approachSelectedMode ? SELECTED_DISTANCE : OVERVIEW_DISTANCE;

    for(int i = 0; i < apprItem->childCount(); i++)
    {
      QTreeWidgetItem *child = apprItem->child(i);
      const MapProcedureRef& childref = itemIndex.at(child->type());
      if(childref.isLeg())
      {
        const maptypes::MapProcedureLegs *legs = approachQuery->getTransitionLegs(currentAirport, transitionId);
        if(legs != nullptr)
        {
          const maptypes::MapProcedureLeg *aleg = legs->approachLegById(childref.legId);

          if(aleg != nullptr)
          {
            child->setText(courseIndex, maptypes::procedureLegCourse(*aleg));
            child->setText(distanceIndex, maptypes::procedureLegDistance(*aleg));
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

void ProcedureTreeController::itemDoubleClicked(QTreeWidgetItem *item, int column)
{
  Q_UNUSED(column);
  showEntry(item, true);
}

void ProcedureTreeController::itemExpanded(QTreeWidgetItem *item)
{
  if(approachSelectedMode)
    return;

  if(item != nullptr)
  {
    if(itemLoadedIndex.at(item->type()))
      return;

    // Get a copy since vector is rebuilt underneath
    const MapProcedureRef ref = itemIndex.at(item->type());

    if(ref.legId == -1)
    {
      float remainingDistanceDummy = 0.f;
      if(ref.approachId != -1 && ref.transitionId == -1)
      {
        const MapProcedureLegs *legs = approachQuery->getApproachLegs(currentAirport, ref.approachId);
        addApproachLegs(legs, item, -1, remainingDistanceDummy);
        itemLoadedIndex.setBit(item->type());
      }
      else if(ref.approachId != -1 && ref.transitionId != -1)
      {
        const MapProcedureLegs *legs = approachQuery->getTransitionLegs(currentAirport, ref.transitionId);
        addTransitionLegs(legs, item, remainingDistanceDummy);
        itemLoadedIndex.setBit(item->type());
      }
      itemLoadedIndex.resize(itemIndex.size());
    }
  }
}

void ProcedureTreeController::addApproachLegs(const MapProcedureLegs *legs, QTreeWidgetItem *item, int transitionId,
                                             float& remainingDistance)
{
  if(legs != nullptr)
  {
    for(const MapProcedureLeg& leg : legs->approachLegs)
    {
      itemIndex.append(MapProcedureRef(legs->ref.airportId, legs->ref.runwayEndId, legs->ref.approachId, transitionId,
                                      leg.legId, legs->mapType));
      buildLegItem(item, leg, remainingDistance);
    }
  }
}

void ProcedureTreeController::addTransitionLegs(const MapProcedureLegs *legs, QTreeWidgetItem *item,
                                               float& remainingDistance)
{
  if(legs != nullptr)
  {
    for(const MapProcedureLeg& leg : legs->transitionLegs)
    {
      itemIndex.append(MapProcedureRef(legs->ref.airportId, legs->ref.runwayEndId, legs->ref.approachId,
                                      legs->ref.transitionId, leg.legId, legs->mapType));
      buildLegItem(item, leg, remainingDistance);
    }
  }
}

void ProcedureTreeController::contextMenu(const QPoint& pos)
{
  qDebug() << Q_FUNC_INFO;

  QPoint menuPos = QCursor::pos();
  // Use widget center if position is not inside widget
  if(!treeWidget->rect().contains(treeWidget->mapFromGlobal(QCursor::pos())))
    menuPos = treeWidget->mapToGlobal(treeWidget->rect().center());

  // Save text which will be changed below
  Ui::MainWindow *ui = mainWindow->getUi();
  ActionTextSaver saver({ui->actionInfoApproachShow, ui->actionInfoApproachSelect, ui->actionInfoApproachAttach});
  Q_UNUSED(saver);

  QTreeWidgetItem *item = treeWidget->itemAt(pos);
  int itemRow = treeWidget->invisibleRootItem()->indexOfChild(item);
  MapProcedureRef ref;
  if(item != nullptr)
    ref = itemIndex.at(item->type());

  ui->actionInfoApproachExpandAll->setDisabled(approachSelectedMode);
  ui->actionInfoApproachCollapseAll->setDisabled(approachSelectedMode);
  ui->actionInfoApproachClear->setEnabled(treeWidget->selectionModel()->hasSelection());
  ui->actionInfoApproachUnselect->setDisabled(!approachSelectedMode);
  ui->actionInfoApproachSelect->setDisabled(item == nullptr || approachSelectedMode);
  ui->actionInfoApproachShow->setDisabled(item == nullptr);

  const Route& rmos = mainWindow->getRouteController()->getRoute();

#ifdef DEBUG_MOVING_AIRPLANE
  ui->actionInfoApproachActivateLeg->setDisabled(rmos.isEmpty());
#else
  ui->actionInfoApproachActivateLeg->setDisabled(rmos.isEmpty() || !rmos.isConnectedToApproach());
#endif

  if(approachSelectedMode)
    ui->actionInfoApproachAttach->setDisabled(mainWindow->getRouteController()->isFlightplanEmpty());
  else
    ui->actionInfoApproachAttach->setDisabled(item == nullptr || mainWindow->getRouteController()->isFlightplanEmpty());

  QMenu menu;
  menu.addAction(ui->actionInfoApproachExpandAll);
  menu.addAction(ui->actionInfoApproachCollapseAll);
  menu.addSeparator();
  menu.addAction(ui->actionInfoApproachClear);
  menu.addSeparator();
  if(approachSelectedMode)
  {
    menu.addAction(ui->actionInfoApproachUnselect);
    menu.addAction(ui->actionInfoApproachActivateLeg);
  }
  else
    menu.addAction(ui->actionInfoApproachSelect);
  menu.addAction(ui->actionInfoApproachAttach);
  menu.addSeparator();
  menu.addAction(ui->actionInfoApproachShow);
  menu.addSeparator();
  menu.addAction(ui->actionInfoApproachShowAppr);
  menu.addAction(ui->actionInfoApproachShowMissedAppr);

  QString text, showText;

  int descriptionIndex = approachSelectedMode ? SELECTED_LEGTYPE : OVERVIEW_DESCRIPTION;
  int identIndex = approachSelectedMode ? SELECTED_IDENT : OVERVIEW_IDENT;

  if(item != nullptr)
  {
    if(!approachSelectedMode)
    {
      QTreeWidgetItem *parentAppr = parentApproachItem(item);
      QTreeWidgetItem *parentTrans = parentTransitionItem(item);

      if(ref.isSid())
        text = parentTrans->text(descriptionIndex) + " " + parentTrans->text(identIndex);
      else if(ref.isApproachAndTransition())
      {
        text = parentAppr->text(descriptionIndex) + " " + parentAppr->text(identIndex) +
               tr(" and ") +
               parentTrans->text(descriptionIndex) + " " + parentTrans->text(identIndex);
      }
      else if(ref.isApproachOnly())
        text = parentAppr->text(descriptionIndex) + " " + parentAppr->text(identIndex);
    }
    else
      text = item->text(descriptionIndex);

    if(!text.isEmpty())
    {
      ui->actionInfoApproachShow->setEnabled(true);
      ui->actionInfoApproachSelect->setEnabled(true);
    }

    if(ref.isLeg())
      showText = item->text(identIndex).isEmpty() ? tr("Position") : item->text(identIndex);
    else
      showText = text;
  }

  ui->actionInfoApproachShow->setText(ui->actionInfoApproachShow->text().arg(showText));
  ui->actionInfoApproachSelect->setText(ui->actionInfoApproachSelect->text().arg(text));

  if(approachSelectedMode)
  {
    QString addText;
    if(approachSelectedLegs.ref.isApproachAndTransition())
      addText = tr("Approach and Transition");
    else if(approachSelectedLegs.ref.isApproachOnly())
      addText = tr("Approach");

    ui->actionInfoApproachAttach->setText(ui->actionInfoApproachAttach->text().arg(addText));
  }
  else
    ui->actionInfoApproachAttach->setText(ui->actionInfoApproachAttach->text().arg(text));

  QAction *action = menu.exec(menuPos);
  if(action == ui->actionInfoApproachExpandAll)
  {
    const QTreeWidgetItem *root = treeWidget->invisibleRootItem();
    for(int i = 0; i < root->childCount(); ++i)
      root->child(i)->setExpanded(true);
  }
  else if(action == ui->actionInfoApproachCollapseAll)
    treeWidget->collapseAll();
  else if(action == ui->actionInfoApproachClear)
  {
    treeWidget->clearSelection();
    emit procedureLegSelected(maptypes::MapProcedureRef());
    if(!approachSelectedMode)
      emit procedureSelected(maptypes::MapProcedureRef());
  }
  else if(action == ui->actionInfoApproachSelect)
    // Show only legs of currently selected approach and transition
    enableSelectedMode(ref);
  else if(action == ui->actionInfoApproachUnselect)
    disableSelectedMode();
  else if(action == ui->actionInfoApproachShow)
    showEntry(item, false);
  else if(action == ui->actionInfoApproachAttach)
  {
    const maptypes::MapProcedureRef& addRef = approachSelectedMode ? approachSelectedLegs.ref : ref;

    // Get the aproach legs for the initial fix
    const maptypes::MapProcedureLegs *legs = nullptr;
    if(addRef.isApproachOnly())
      legs = approachQuery->getApproachLegs(currentAirport, addRef.approachId);
    else if(addRef.isApproachAndTransition())
      legs = approachQuery->getTransitionLegs(currentAirport, addRef.transitionId);

    // enableSelectedMode(addRef);

    if(legs != nullptr)
      emit routeInsertProcedure(*legs);
  }
  else if(action == ui->actionInfoApproachActivateLeg)
    // Will call this class back to highlight row
    mainWindow->getRouteController()->activateLeg(itemRow);

  // Done by the actions themselves
  // else if(action == ui->actionInfoApproachShowAppr ||
  // action == ui->actionInfoApproachShowMissedAppr ||
  // action == ui->actionInfoApproachShowTrans)
}

void ProcedureTreeController::showEntry(QTreeWidgetItem *item, bool doubleClick)
{
  if(item == nullptr)
    return;

  const MapProcedureRef& ref = itemIndex.at(item->type());

  if(ref.legId != -1)
  {
    const maptypes::MapProcedureLeg *leg = nullptr;

    if(ref.transitionId != -1)
      leg = approachQuery->getTransitionLeg(currentAirport, ref.legId);
    else if(ref.approachId != -1)
      leg = approachQuery->getApproachLeg(currentAirport, ref.approachId, ref.legId);

    if(leg != nullptr)
    {
      emit showPos(leg->line.getPos2(), 0.f, doubleClick);

      if(doubleClick && (leg->navaids.hasNdb() || leg->navaids.hasVor() || leg->navaids.hasWaypoints()))
        emit showInformation(leg->navaids);
    }
  }
  else if(ref.transitionId != -1 && !doubleClick)
  {
    const maptypes::MapProcedureLegs *legs = approachQuery->getTransitionLegs(currentAirport, ref.transitionId);
    if(legs != nullptr)
      emit showRect(legs->bounding, doubleClick);
  }
  else if(ref.approachId != -1 && !doubleClick)
  {
    const maptypes::MapProcedureLegs *legs = approachQuery->getApproachLegs(currentAirport, ref.approachId);
    if(legs != nullptr)
      emit showRect(legs->bounding, doubleClick);
  }

}

maptypes::MapObjectTypes ProcedureTreeController::buildType(const SqlRecord& recApp)
{
  QString suffix(recApp.valueStr("suffix"));
  QString type(recApp.valueStr("type"));
  int gpsOverlay = recApp.valueBool("has_gps_overlay");

  if(mainWindow->getCurrentSimulator() == atools::fs::FsPaths::P3D_V3 && type == "GPS" &&
     (suffix == "A" || suffix == "D") && gpsOverlay)
  {
    if(suffix == "A")
      return maptypes::PROCEDURE_STAR;
    else if(suffix == "D")
      return maptypes::PROCEDURE_SID;
  }
  return maptypes::PROCEDURE_APPROACH;
}

QTreeWidgetItem *ProcedureTreeController::buildApproachItem(QTreeWidgetItem *runwayItem, const SqlRecord& recApp)
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
  item->setTextAlignment(OVERVIEW_ALTITUDE, Qt::AlignRight);
  item->setTextAlignment(OVERVIEW_COURSE, Qt::AlignRight);
  item->setTextAlignment(OVERVIEW_DISTANCE, Qt::AlignRight);

  for(int i = 0; i < item->columnCount(); i++)
    item->setFont(i, approachFont);

  runwayItem->addChild(item);

  return item;
}

QTreeWidgetItem *ProcedureTreeController::buildTransitionItem(QTreeWidgetItem *apprItem, const SqlRecord& recTrans)
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
  item->setTextAlignment(OVERVIEW_ALTITUDE, Qt::AlignRight);
  item->setTextAlignment(OVERVIEW_COURSE, Qt::AlignRight);
  item->setTextAlignment(OVERVIEW_DISTANCE, Qt::AlignRight);

  for(int i = 0; i < item->columnCount(); i++)
    item->setFont(i, transitionFont);

  apprItem->addChild(item);

  return item;
}

void ProcedureTreeController::buildLegItem(QTreeWidgetItem *parentItem, const MapProcedureLeg& leg,
                                          float& remainingDistance)
{
  QStringList texts;
  QIcon icon;
  int fontHeight = treeWidget->fontMetrics().height();
  int size = fontHeight + 1;
  if(approachSelectedMode)
  {
    texts << leg.fixIdent;
    if(leg.navaids.hasVor())
    {
      const maptypes::MapVor& vor = leg.navaids.vors.first();
      icon = SymbolPainter(Qt::transparent).createVorIcon(vor, size);
      texts << maptypes::vorFullShortText(vor);
      texts << QLocale().toString(vor.frequency / 1000.f, 'f', 2);
    }
    else if(leg.navaids.hasNdb())
    {
      const maptypes::MapNdb& ndb = leg.navaids.ndbs.first();
      icon = SymbolPainter(Qt::transparent).createNdbIcon(size);
      texts << maptypes::ndbFullShortText(ndb);
      texts << QLocale().toString(ndb.frequency / 100.f, 'f', 1);
    }
    else if(leg.navaids.hasWaypoints())
    {
      icon = SymbolPainter(Qt::transparent).createWaypointIcon(size + 2);
      texts << "" << "";
    }
    else
    {
      icon = SymbolPainter(Qt::transparent).createApproachPointIcon(size);
      texts << "" << "";
    }

    texts << maptypes::procedureLegTypeStr(leg.type);
  }
  else
    texts << maptypes::procedureLegTypeStr(leg.type) << leg.fixIdent;

  QString remarkStr = maptypes::procedureLegRemark(leg);
  texts << maptypes::altRestrictionTextShort(leg.altRestriction)
        << maptypes::procedureLegCourse(leg) << maptypes::procedureLegDistance(leg);

  if(approachSelectedMode)
    texts << maptypes::procedureLegRemDistance(leg, remainingDistance);

  texts << remarkStr;

  QTreeWidgetItem *item = new QTreeWidgetItem(texts, itemIndex.size() - 1);
  if(!icon.isNull())
  {
    item->setIcon(0, icon);
    item->setSizeHint(0, QSize(fontHeight - 3, fontHeight - 3));
  }

  item->setToolTip(approachSelectedMode ? SELECTED_REMARKS : OVERVIEW_REMARKS, remarkStr);

  if(approachSelectedMode)
  {
    item->setTextAlignment(SELECTED_IDENT_FREQUENCY, Qt::AlignRight);
    item->setTextAlignment(SELECTED_ALTITUDE, Qt::AlignRight);
    item->setTextAlignment(SELECTED_COURSE, Qt::AlignRight);
    item->setTextAlignment(SELECTED_DISTANCE, Qt::AlignRight);
    item->setTextAlignment(SELECTED_REMAINING_DISTANCE, Qt::AlignRight);
  }
  else
  {
    item->setTextAlignment(OVERVIEW_ALTITUDE, Qt::AlignRight);
    item->setTextAlignment(OVERVIEW_COURSE, Qt::AlignRight);
    item->setTextAlignment(OVERVIEW_DISTANCE, Qt::AlignRight);
  }

  setItemStyle(item, leg);

  parentItem->addChild(item);
}

void ProcedureTreeController::setItemStyle(QTreeWidgetItem *item, const MapProcedureLeg& leg)
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

  if(approachSelectedMode)
    item->setFont(0, identFont);
}

QBitArray ProcedureTreeController::saveTreeViewState()
{
  QList<const QTreeWidgetItem *> itemStack;
  const QTreeWidgetItem *root = treeWidget->invisibleRootItem();

  QBitArray state;

  if(!itemIndex.isEmpty() && !approachSelectedMode)
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

      int identIndex = approachSelectedMode ? SELECTED_IDENT : OVERVIEW_IDENT;
      int descriptionIndex = approachSelectedMode ? SELECTED_LEGTYPE : OVERVIEW_DESCRIPTION;

      qDebug() << item->text(descriptionIndex) << item->text(identIndex)
               << "expanded" << item->isExpanded() << "selected" << item->isSelected() << "child" << item->childCount();

      for(int i = 0; i < item->childCount(); ++i)
        itemStack.append(item->child(i));
      itemIdx += 2;
    }
  }
  return state;
}

void ProcedureTreeController::restoreTreeViewState(const QBitArray& state)
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

void ProcedureTreeController::enableSelectedMode(const maptypes::MapProcedureRef& ref)
{
  Ui::MainWindow *ui = mainWindow->getUi();

  if(!approachSelectedMode)
  {
    // Save tree state
    recentTreeState.insert(currentAirport.id, saveTreeViewState());
    WidgetState(lnm::APPROACHTREE_WIDGET).save(ui->treeWidgetApproachInfo);
  }

  approachSelectedLegs.ref = ref;
  approachSelectedMode = true;

  treeWidget->clear();
  updateTreeHeader();

  WidgetState(lnm::APPROACHTREE_SELECTED_WIDGET).restore(ui->treeWidgetApproachInfo);

  fillApproachTreeWidget();

  highlightNextWaypoint(mainWindow->getRouteController()->getRoute().getActiveLegIndexCorrected());

  emit procedureSelected(approachSelectedLegs.ref);
}

void ProcedureTreeController::disableSelectedMode()
{
  Ui::MainWindow *ui = mainWindow->getUi();
  if(approachSelectedMode)
    WidgetState(lnm::APPROACHTREE_SELECTED_WIDGET).save(ui->treeWidgetApproachInfo);

  approachSelectedLegs.ref = MapProcedureRef();
  approachSelectedMode = false;

  treeWidget->clear();
  updateTreeHeader();

  WidgetState(lnm::APPROACHTREE_WIDGET).restore(ui->treeWidgetApproachInfo);

  fillApproachTreeWidget();

  if(recentTreeState.contains(currentAirport.id))
    restoreTreeViewState(recentTreeState.value(currentAirport.id));

  // Selected signal will be emitted when selecting tree item
}

void ProcedureTreeController::createFonts()
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

  activeLegFont = legFont;
  activeLegFont.setBold(true);
}

QTreeWidgetItem *ProcedureTreeController::parentApproachItem(QTreeWidgetItem *item) const
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

QTreeWidgetItem *ProcedureTreeController::parentTransitionItem(QTreeWidgetItem *item) const
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
