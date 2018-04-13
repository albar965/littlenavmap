/*****************************************************************************
* Copyright 2015-2018 Alexander Barthel albar965@mailbox.org
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
#include "navapp.h"
#include "route/route.h"
#include "common/mapcolors.h"
#include "query/infoquery.h"
#include "query/procedurequery.h"
#include "sql/sqlrecord.h"
#include "ui_mainwindow.h"
#include "gui/actiontextsaver.h"
#include "gui/actionstatesaver.h"
#include "common/constants.h"
#include "settings/settings.h"
#include "gui/widgetstate.h"
#include "query/mapquery.h"
#include "query/airportquery.h"
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
  COL_RESTR = 2,
  COL_COURSE = 3,
  COL_DISTANCE = 4,
  COL_REMARKS = 5,
  INVALID = -1
};

using atools::sql::SqlRecord;

using atools::sql::SqlRecordVector;
using proc::MapProcedureLeg;
using proc::MapProcedureLegs;
using proc::MapProcedureRef;
using atools::gui::WidgetState;
using atools::gui::ActionTextSaver;
using atools::gui::ActionStateSaver;

/* Use event filter to catch mouse click in white area and deselect all entries */
class TreeEventFilter :
  public QObject
{

public:
  TreeEventFilter(QTreeWidget *parent)
    : QObject(parent), tree(parent)
  {
  }

  virtual ~TreeEventFilter();

private:
  bool eventFilter(QObject *object, QEvent *event)
  {
    if(event->type() == QEvent::MouseButtonPress)
    {
      QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
      if(mouseEvent != nullptr && mouseEvent->button() == Qt::LeftButton)
      {
        QTreeWidgetItem *item = tree->itemAt(mouseEvent->pos());
        if(item == nullptr || object == NavApp::getMainUi()->labelProcedureSearch)
          tree->clearSelection();
      }

    }
    return QObject::eventFilter(object, event);
  }

  QTreeWidget *tree;
};

TreeEventFilter::~TreeEventFilter()
{

}

ProcedureSearch::ProcedureSearch(QMainWindow *main, QTreeWidget *treeWidgetParam, si::SearchTabIndex tabWidgetIndex)
  : AbstractSearch(main, tabWidgetIndex), treeWidget(treeWidgetParam), mainWindow(main)
{
  infoQuery = NavApp::getInfoQuery();
  procedureQuery = NavApp::getProcedureQuery();
  airportQuery = NavApp::getAirportQueryNav();

  zoomHandler = new atools::gui::ItemViewZoomHandler(treeWidget);
  connect(NavApp::navAppInstance(), &atools::gui::Application::fontChanged, this, &ProcedureSearch::fontChanged);
  gridDelegate = new atools::gui::GridDelegate(treeWidget);

  treeWidget->setItemDelegate(gridDelegate);

  Ui::MainWindow *ui = NavApp::getMainUi();
  ui->actionSearchProcedureSelectNothing->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  treeWidget->addActions({ui->actionSearchProcedureSelectNothing});

  connect(ui->pushButtonProcedureSearchClearSelection, &QPushButton::clicked,
          this, &ProcedureSearch::clearSelectionTriggered);
  connect(ui->actionSearchProcedureSelectNothing, &QAction::triggered, this, &ProcedureSearch::clearSelectionTriggered);
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

void ProcedureSearch::fontChanged()
{
  qDebug() << Q_FUNC_INFO;

  zoomHandler->fontChanged();
  optionsChanged();
}

void ProcedureSearch::resetSearch()
{
  Ui::MainWindow *ui = NavApp::getMainUi();
  if(ui->tabWidgetSearch->currentIndex() == tabIndex)
  {
    // Only reset if this tab is active
    ui->comboBoxProcedureRunwayFilter->setCurrentIndex(0);
    ui->comboBoxProcedureSearchFilter->setCurrentIndex(FILTER_ALL_PROCEDURES);
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
  emit procedureSelected(proc::MapProcedureRef());
  emit procedureLegSelected(proc::MapProcedureRef());

}

void ProcedureSearch::preDatabaseLoad()
{
  // Clear display on map
  emit procedureSelected(proc::MapProcedureRef());
  emit procedureLegSelected(proc::MapProcedureRef());

  treeWidget->clear();

  itemIndex.clear();
  itemLoadedIndex.clear();
  currentAirportNav = map::MapAirport();
  recentTreeState.clear();
}

void ProcedureSearch::postDatabaseLoad()
{
  resetSearch();
  updateFilterBoxes();
  updateHeaderLabel();
}

void ProcedureSearch::showProcedures(map::MapAirport airport)
{
  NavApp::getMapQuery()->getAirportNavReplace(airport);

  Ui::MainWindow *ui = NavApp::getMainUi();
  ui->dockWidgetSearch->show();
  ui->dockWidgetSearch->raise();
  ui->tabWidgetSearch->setCurrentIndex(2);
  treeWidget->setFocus();

  if(NavApp::getRouteConst().isAirportDeparture(airport.ident))
    ui->comboBoxProcedureSearchFilter->setCurrentIndex(FILTER_DEPARTURE_PROCEDURES);
  else if(NavApp::getRouteConst().isAirportDestination(airport.ident))
    ui->comboBoxProcedureSearchFilter->setCurrentIndex(FILTER_ARRIVAL_PROCEDURES);
  else
    ui->comboBoxProcedureSearchFilter->setCurrentIndex(FILTER_ALL_PROCEDURES);

  if(currentAirportNav.isValid() && currentAirportNav.ident == airport.ident)
    // Ignore if noting has changed - or jump out of the view mode
    return;

  emit procedureLegSelected(proc::MapProcedureRef());
  emit procedureSelected(proc::MapProcedureRef());

  // Put state on stack and update tree
  if(currentAirportNav.isValid())
    recentTreeState.insert(currentAirportNav.id, saveTreeViewState());

  airportQuery->getAirportByIdent(currentAirportNav, airport.ident);

  updateFilterBoxes();

  fillApproachTreeWidget();

  restoreTreeViewState(recentTreeState.value(currentAirportNav.id));
  updateHeaderLabel();
}

void ProcedureSearch::updateHeaderLabel()
{
  QString procs;

  map::MapAirport airportSim = NavApp::getMapQuery()->getAirportSim(currentAirportNav);

  QList<QTreeWidgetItem *> items = treeWidget->selectedItems();
  for(QTreeWidgetItem *item : items)
    procs.append(approachAndTransitionText(item));

  if(airportSim.isValid())
    NavApp::getMainUi()->labelProcedureSearch->setText(
      "<b>" + map::airportTextShort(airportSim) + "</b> " + procs);
  else
    NavApp::getMainUi()->labelProcedureSearch->setText(tr("No Airport selected."));
}

QString ProcedureSearch::approachAndTransitionText(const QTreeWidgetItem *item)
{
  QString procs;
  if(item != nullptr)
  {
    MapProcedureRef ref = itemIndex.at(item->type()).procedureRef;
    if(ref.isLeg())
    {
      item = item->parent();
      ref = itemIndex.at(item->type()).procedureRef;
    }

    if(ref.hasApproachOnlyIds())
    {
      // Only approach
      procs.append(" " + item->text(COL_DESCRIPTION) + " " + item->text(COL_IDENT));
      if(item->childCount() == 1 && ref.mapType & proc::PROCEDURE_SID)
      {
        // Special SID case that has only transition legs and only one transition
        QTreeWidgetItem *child = item->child(0);
        if(child != nullptr)
          procs.append(" " + child->text(COL_DESCRIPTION) + " " + child->text(COL_IDENT));
      }
    }
    else
    {
      if(ref.hasApproachAndTransitionIds())
      {
        QTreeWidgetItem *appr = item->parent();
        if(appr != nullptr)
          procs.append(" " + appr->text(COL_DESCRIPTION) + " " + appr->text(COL_IDENT));
      }
      procs.append(" " + item->text(COL_DESCRIPTION) + " " + item->text(COL_IDENT));
    }
  }
  return procs;
}

void ProcedureSearch::clearRunwayFilter()
{
  Ui::MainWindow *ui = NavApp::getMainUi();

  ui->comboBoxProcedureRunwayFilter->blockSignals(true);
  ui->comboBoxProcedureRunwayFilter->setCurrentIndex(0);
  ui->comboBoxProcedureRunwayFilter->clear();
  ui->comboBoxProcedureRunwayFilter->addItem(tr("All Runways"));
  ui->comboBoxProcedureRunwayFilter->blockSignals(false);
}

void ProcedureSearch::updateFilterBoxes()
{
  Ui::MainWindow *ui = NavApp::getMainUi();
  ui->comboBoxProcedureSearchFilter->setHidden(!NavApp::hasSidStarInDatabase());

  clearRunwayFilter();

  if(currentAirportNav.isValid())
  {
    QStringList runwayNames = airportQuery->getRunwayNames(currentAirportNav.id);

    // Add a tree of transitions and approaches
    const SqlRecordVector *recAppVector = infoQuery->getApproachInformation(currentAirportNav.id);

    if(recAppVector != nullptr) // Deduplicate runways
    {
      QSet<QString> runways;
      for(const SqlRecord& recApp : *recAppVector)
        runways.insert(map::runwayBestFit(recApp.valueStr("runway_name"), runwayNames));

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
      qWarning() << Q_FUNC_INFO << "nothing found for airport id" << currentAirportNav.id;
  }
}

void ProcedureSearch::fillApproachTreeWidget()
{
  treeWidget->blockSignals(true);
  treeWidget->clear();
  itemIndex.clear();
  itemLoadedIndex.clear();

  if(currentAirportNav.isValid())
  {
    // Add a tree of transitions and approaches
    const SqlRecordVector *recAppVector = infoQuery->getApproachInformation(currentAirportNav.id);

    if(recAppVector != nullptr)
    {
      QStringList runwayNames = airportQuery->getRunwayNames(currentAirportNav.id);
      Ui::MainWindow *ui = NavApp::getMainUi();
      QTreeWidgetItem *root = treeWidget->invisibleRootItem();
      SqlRecordVector sorted;

      // Collect all procedures from the database
      for(SqlRecord recApp : *recAppVector)
      {
        QString rwname = map::runwayBestFit(recApp.valueStr("runway_name"), runwayNames);

        proc::MapProcedureTypes type = buildTypeFromApproachRec(recApp);

        bool filterOk = false;
        switch(filterIndex)
        {
          case ProcedureSearch::FILTER_ALL_PROCEDURES:
            filterOk = true;
            break;
          case ProcedureSearch::FILTER_DEPARTURE_PROCEDURES:
            filterOk = type & proc::PROCEDURE_DEPARTURE;
            break;
          case ProcedureSearch::FILTER_ARRIVAL_PROCEDURES:
            filterOk = type & proc::PROCEDURE_ARRIVAL_ALL;
            break;
          case ProcedureSearch::FILTER_APPROACH_AND_TRANSITIONS:
            filterOk = type & proc::PROCEDURE_ARRIVAL;
            break;
        }

        // Resolve parallel runway assignments
        QString allRunwayText = tr("All");
        QStringList sidStarArincNames, sidStarRunways;
        if((type& proc::PROCEDURE_SID) || (type & proc::PROCEDURE_STAR))
        {
          // arinc_name - added with database minor version 8
          QString arincName = recApp.valueStr("arinc_name", QString());
          if(proc::hasSidStarAllRunways(arincName))
          {
            sidStarArincNames.append(allRunwayText);
            sidStarRunways.append(runwayNames);
          }
          else if(proc::hasSidStarParallelRunways(arincName))
          {
            // Check which runways are assigned from values like "RW12B"
            arincName = arincName.mid(2, 2);
            if(runwayNames.contains(arincName + "L"))
            {
              sidStarArincNames.append(arincName + "L");
              sidStarRunways.append(arincName + "L");
            }
            if(runwayNames.contains(arincName + "R"))
            {
              sidStarArincNames.append(arincName + "R");
              sidStarRunways.append(arincName + "R");
            }
            if(runwayNames.contains(arincName + "C"))
            {
              sidStarArincNames.append(arincName + "C");
              sidStarRunways.append(arincName + "C");
            }
          }
#ifdef DEBUG_INFORMATION
          else if(!arincName.isEmpty())
            sidStarArincNames.append("(" + arincName + ")");
#endif
        }

        QString rwnamefilter = ui->comboBoxProcedureRunwayFilter->currentData(Qt::UserRole).toString();
        int rwnameindex = ui->comboBoxProcedureRunwayFilter->currentIndex();

        if(rwnameindex == 0)
          // All selected
          filterOk &= true;
        else if(rwnamefilter.isEmpty())
          // No rwy selected
          filterOk &= rwname.isEmpty() && sidStarArincNames.isEmpty();
        else
        {
          filterOk &= rwname == rwnamefilter || // name equal
                      (!sidStarArincNames.isEmpty() && sidStarArincNames.contains(rwnamefilter)) ||
                      sidStarArincNames.contains(allRunwayText);
        }

        if(filterOk)
        {
          // Add an extra field with the best airport runway name
          recApp.appendField("airport_runway_name", QVariant::String);
          recApp.setValue("airport_runway_name", rwname);

          // Keep the runways for the context menu
          recApp.appendField("sid_star_runways", QVariant::StringList);
          recApp.setValue("sid_star_runways", sidStarRunways);

          recApp.appendField("sid_star_arinc_name", QVariant::String);
          if(((type& proc::PROCEDURE_SID) || (type & proc::PROCEDURE_STAR)) && rwname.isEmpty())
            recApp.setValue("sid_star_arinc_name", sidStarArincNames.join("/"));

          sorted.append(recApp);
        }
      }

      std::sort(sorted.begin(), sorted.end(), procedureSortFunc);

      for(const SqlRecord& recApp : sorted)
      {
        proc::MapProcedureTypes type = buildTypeFromApproachRec(recApp);

        int runwayEndId = recApp.valueInt("runway_end_id");
        QStringList sidStarRunways = recApp.value("sid_star_runways").toStringList();

        int apprId = recApp.valueInt("approach_id");
        itemIndex.append({MapProcedureRef(currentAirportNav.id, runwayEndId, apprId, -1, -1, type), sidStarRunways});
        const SqlRecordVector *recTransVector = infoQuery->getTransitionInformation(recApp.valueInt("approach_id"));

        QTreeWidgetItem *apprItem = buildApproachItem(root, recApp, type);

        if(recTransVector != nullptr)
        {
          // Transitions for this approach
          for(const SqlRecord& recTrans : *recTransVector)
          {
            // Also add runway from parent approach to transition
            itemIndex.append({MapProcedureRef(currentAirportNav.id, runwayEndId, apprId,
                                              recTrans.valueInt("transition_id"), -1, type), sidStarRunways});
            buildTransitionItem(apprItem, recTrans,
                                type & proc::PROCEDURE_DEPARTURE || type & proc::PROCEDURE_STAR_ALL);
          }
        }
      }
    }
    itemLoadedIndex.resize(itemIndex.size());
  }

  treeWidget->blockSignals(false);
}

void ProcedureSearch::saveState()
{
  Ui::MainWindow *ui = NavApp::getMainUi();
  WidgetState(lnm::APPROACHTREE_WIDGET).save({ui->comboBoxProcedureSearchFilter,
                                              ui->comboBoxProcedureRunwayFilter,
                                              ui->actionSearchProcedureFollowSelection});

  atools::settings::Settings& settings = atools::settings::Settings::instance();

  // Use current state and update the map too
  QBitArray state = saveTreeViewState();
  recentTreeState.insert(currentAirportNav.id, state);
  settings.setValueVar(lnm::APPROACHTREE_STATE, state);

  // Save column order and width
  WidgetState(lnm::APPROACHTREE_WIDGET).save(treeWidget);
  settings.setValue(lnm::APPROACHTREE_AIRPORT, currentAirportNav.id);
}

void ProcedureSearch::restoreState()
{
  atools::settings::Settings& settings = atools::settings::Settings::instance();
  if(OptionData::instance().getFlags() & opts::STARTUP_LOAD_SEARCH)
  {
    if(NavApp::hasDataInDatabase())
      airportQuery->getAirportById(currentAirportNav, settings.valueInt(lnm::APPROACHTREE_AIRPORT, -1));
  }

  updateFilterBoxes();

  QBitArray state;
  if(OptionData::instance().getFlags() & opts::STARTUP_LOAD_SEARCH)
  {
    Ui::MainWindow *ui = NavApp::getMainUi();
    WidgetState(lnm::APPROACHTREE_WIDGET).restore({ui->comboBoxProcedureSearchFilter,
                                                   ui->comboBoxProcedureRunwayFilter,
                                                   ui->actionSearchProcedureFollowSelection});

    fillApproachTreeWidget();
    if(currentAirportNav.isValid() && currentAirportNav.procedure())
    {
      state = settings.valueVar(lnm::APPROACHTREE_STATE).toBitArray();
      recentTreeState.insert(currentAirportNav.id, state);
    }
  }

  updateTreeHeader();
  WidgetState(lnm::APPROACHTREE_WIDGET).restore(treeWidget);

  if(OptionData::instance().getFlags() & opts::STARTUP_LOAD_SEARCH)
  {
    // Restoring state will emit above signal
    if(currentAirportNav.isValid() && currentAirportNav.procedure())
      restoreTreeViewState(state);
  }

  updateHeaderLabel();
}

void ProcedureSearch::updateTreeHeader()
{
  QTreeWidgetItem *header = new QTreeWidgetItem();

  header->setText(COL_DESCRIPTION, tr("Description"));
  header->setText(COL_IDENT, tr("Ident"));
  header->setText(COL_RESTR, tr("Restriction\n%1/%2").arg(Unit::getUnitAltStr()).arg(Unit::getUnitSpeedStr()));
  header->setText(COL_COURSE, tr("Course\n°M"));
  header->setText(COL_DISTANCE, tr("Dist./Time\n%1/min").arg(Unit::getUnitDistStr()));
  header->setText(COL_REMARKS, tr("Remarks"));

  for(int col = COL_DESCRIPTION; col <= COL_REMARKS; col++)
    header->setTextAlignment(col, Qt::AlignCenter);

  treeWidget->setHeaderItem(header);
}

/* If approach has no legs and a single transition: SID special case. get transition id from cache */
void ProcedureSearch::fetchSingleTransitionId(MapProcedureRef& ref)
{
  if(ref.hasApproachOnlyIds())
  {
    // No transition
    const proc::MapProcedureLegs *legs = procedureQuery->getApproachLegs(currentAirportNav, ref.approachId);
    if(legs != nullptr && legs->approachLegs.isEmpty())
    {
      // Special case for SID which consists only of transition legs
      QVector<int> transitionIds = procedureQuery->getTransitionIdsForApproach(ref.approachId);
      if(!transitionIds.isEmpty())
        ref.transitionId = transitionIds.first();
    }
  }
}

void ProcedureSearch::itemSelectionChanged()
{
  QList<QTreeWidgetItem *> items = treeWidget->selectedItems();
  if(items.isEmpty() || NavApp::getMainUi()->tabWidgetSearch->currentIndex() != tabIndex)
  {
    NavApp::getMainUi()->pushButtonProcedureSearchClearSelection->setEnabled(false);

    emit procedureSelected(proc::MapProcedureRef());

    emit procedureLegSelected(proc::MapProcedureRef());
  }
  else
  {
    NavApp::getMainUi()->pushButtonProcedureSearchClearSelection->setEnabled(true);
    for(QTreeWidgetItem *item : items)
    {
      MapProcedureRef ref = itemIndex.at(item->type()).procedureRef;
      QStringList sidStarRunways = itemIndex.at(item->type()).sidStarRunways;

      qDebug() << Q_FUNC_INFO << ref.runwayEndId << ref.approachId << ref.transitionId << ref.legId << sidStarRunways;

      if(ref.hasApproachOrTransitionIds())
      {
        fetchSingleTransitionId(ref);
        emit procedureSelected(ref);

        if(!ref.isLeg() && NavApp::getMainUi()->actionSearchProcedureFollowSelection->isChecked())
          showEntry(item, false /* double click*/, true /* zoom */);
      }

      if(ref.isLeg())
      {
        // Highlight legs
        emit procedureLegSelected(ref);
        if(NavApp::getMainUi()->actionSearchProcedureFollowSelection->isChecked())
          showEntry(item, false /* double click*/, false /* zoom */);
      }
      else
        // Remove leg highlight
        emit procedureLegSelected(proc::MapProcedureRef());

      if(ref.hasApproachAndTransitionIds())
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
      const MapProcedureRef& childref = itemIndex.at(child->type()).procedureRef;
      if(childref.isLeg())
      {
        const proc::MapProcedureLegs *legs = procedureQuery->getTransitionLegs(currentAirportNav, transitionId);
        if(legs != nullptr)
        {
          const proc::MapProcedureLeg *aleg = legs->approachLegById(childref.legId);

          if(aleg != nullptr)
          {
            child->setText(COL_COURSE, proc::procedureLegCourse(*aleg));
            child->setText(COL_DISTANCE, proc::procedureLegDistance(*aleg));
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
  showEntry(item, true /* double click*/, true /* zoom */);
}

/* Load all approach or transition legs on demand - approaches and transitions are loaded after selecting the airport */
void ProcedureSearch::itemExpanded(QTreeWidgetItem *item)
{
  if(item != nullptr)
  {
    if(item->type() >= itemLoadedIndex.size())
      return;

    if(itemLoadedIndex.at(item->type()))
      return;

    // Get a copy since vector is rebuilt underneath
    const MapProcedureRef ref = itemIndex.at(item->type()).procedureRef;

    if(ref.legId == -1)
    {
      if(ref.approachId != -1 && ref.transitionId == -1)
      {
        const MapProcedureLegs *legs = procedureQuery->getApproachLegs(currentAirportNav, ref.approachId);
        if(legs != nullptr)
        {
          QList<QTreeWidgetItem *> items = buildApproachLegItems(legs, -1);
          itemLoadedIndex.setBit(item->type());

          if(legs->mapType & proc::PROCEDURE_DEPARTURE)
            item->insertChildren(0, items);
          else
            item->addChildren(items);
        }
        else
          qWarning() << Q_FUNC_INFO << "no legs found for" << currentAirportNav.id << ref.approachId;
      }
      else if(ref.approachId != -1 && ref.transitionId != -1)
      {
        const MapProcedureLegs *legs = procedureQuery->getTransitionLegs(currentAirportNav, ref.transitionId);
        if(legs != nullptr)
        {
          QList<QTreeWidgetItem *> items = buildTransitionLegItems(legs);
          item->addChildren(items);
          itemLoadedIndex.setBit(item->type());
        }
        else
          qWarning() << Q_FUNC_INFO << "no legs found for" << currentAirportNav.id << ref.transitionId;
      }
      itemLoadedIndex.resize(itemIndex.size());
    }
  }
}

QList<QTreeWidgetItem *> ProcedureSearch::buildApproachLegItems(const MapProcedureLegs *legs, int transitionId)
{
  QList<QTreeWidgetItem *> items;
  if(legs != nullptr)
  {
    for(const MapProcedureLeg& leg : legs->approachLegs)
    {
      itemIndex.append({MapProcedureRef(legs->ref.airportId, legs->ref.runwayEndId, legs->ref.approachId, transitionId,
                                        leg.legId, legs->mapType), {}
                       });
      items.append(buildLegItem(leg));
    }
  }
  return items;
}

QList<QTreeWidgetItem *> ProcedureSearch::buildTransitionLegItems(const MapProcedureLegs *legs)
{
  QList<QTreeWidgetItem *> items;
  if(legs != nullptr)
  {
    for(const MapProcedureLeg& leg : legs->transitionLegs)
    {
      itemIndex.append({MapProcedureRef(legs->ref.airportId, legs->ref.runwayEndId, legs->ref.approachId,
                                        legs->ref.transitionId, leg.legId, legs->mapType), {}
                       });
      items.append(buildLegItem(leg));
    }
  }
  return items;
}

void ProcedureSearch::clearSelectionTriggered()
{
  treeWidget->clearSelection();
  emit procedureLegSelected(proc::MapProcedureRef());
  emit procedureSelected(proc::MapProcedureRef());
}

void ProcedureSearch::contextMenu(const QPoint& pos)
{
  qDebug() << Q_FUNC_INFO;

  QPoint menuPos = QCursor::pos();
  // Use widget center if position is not inside widget
  if(!treeWidget->rect().contains(treeWidget->mapFromGlobal(QCursor::pos())))
    menuPos = treeWidget->mapToGlobal(treeWidget->rect().center());

  // Save text which will be changed below
  Ui::MainWindow *ui = NavApp::getMainUi();
  ActionTextSaver saver({ui->actionInfoApproachShow, ui->actionInfoApproachAttach});
  Q_UNUSED(saver);

  ActionStateSaver stateSaver({
    ui->actionInfoApproachShow, ui->actionInfoApproachAttach, ui->actionInfoApproachExpandAll,
    ui->actionInfoApproachCollapseAll, ui->actionSearchResetSearch, ui->actionInfoApproachClear,
    ui->actionSearchResetView
  });
  Q_UNUSED(stateSaver);

  QTreeWidgetItem *item = treeWidget->itemAt(pos);
  ProcData procData;
  MapProcedureRef ref;
  if(item != nullptr && !itemIndex.isEmpty())
  {
    procData = itemIndex.at(item->type());
    ref = procData.procedureRef;
    // Get transition id too if SID with only transition legs is selected
    fetchSingleTransitionId(ref);
  }

  ui->actionInfoApproachClear->setEnabled(treeWidget->selectionModel()->hasSelection());
  ui->actionInfoApproachShow->setDisabled(item == nullptr);

  const Route& route = NavApp::getRouteConst();

  ui->actionInfoApproachAttach->setDisabled(item == nullptr);

  QString text, showText;
  const proc::MapProcedureLegs *procedureLegs = nullptr;

  if(item != nullptr)
  {
    // Get the aproach legs for the initial fix
    if(ref.hasApproachOnlyIds())
      procedureLegs = procedureQuery->getApproachLegs(currentAirportNav, ref.approachId);
    else if(ref.hasApproachAndTransitionIds())
      procedureLegs = procedureQuery->getTransitionLegs(currentAirportNav, ref.transitionId);

    if(procedureLegs != nullptr && !procedureLegs->isEmpty())
    {
      QTreeWidgetItem *parentAppr = parentApproachItem(item);
      QTreeWidgetItem *parentTrans = parentTransitionItem(item);

      if(ref.hasApproachOrTransitionIds())
        text = approachAndTransitionText(parentTrans == nullptr ? parentAppr : parentTrans);

      if(!text.isEmpty())
        ui->actionInfoApproachShow->setEnabled(true);

      if(ref.isLeg())
        showText = item->text(COL_IDENT).isEmpty() ? tr("Position") : item->text(COL_IDENT);
      else
        showText = text;

      ui->actionInfoApproachShow->setText(ui->actionInfoApproachShow->text().arg(showText));

      if((route.hasValidDeparture() &&
          route.first().getId() == currentAirportNav.id && ref.mapType & proc::PROCEDURE_DEPARTURE) ||
         (route.hasValidDestination() && route.last().getId() == currentAirportNav.id &&
          ref.mapType & proc::PROCEDURE_ARRIVAL_ALL))
        ui->actionInfoApproachAttach->setText(tr("&Insert %1 into Flight Plan").arg(text));
      else
      {
        if(ref.mapType & proc::PROCEDURE_ARRIVAL_ALL)
          ui->actionInfoApproachAttach->setText(tr("&Use %1 and %2 as Destination").arg(currentAirportNav.ident).arg(
                                                  text));

        else if(ref.mapType & proc::PROCEDURE_DEPARTURE)
          ui->actionInfoApproachAttach->setText(tr("&Use %1 and %2 as Departure").arg(currentAirportNav.ident).arg(text));
      }
    }
  }

  if(procedureLegs == nullptr || procedureLegs->isEmpty())
  {
    ui->actionInfoApproachAttach->setEnabled(false);
    ui->actionInfoApproachShow->setEnabled(false);
    ui->actionInfoApproachAttach->setText(ui->actionInfoApproachAttach->text().arg(tr("Procedure")));
    ui->actionInfoApproachShow->setText(ui->actionInfoApproachShow->text().arg(tr("Procedure")));
  }

  QMenu menu;
  menu.addAction(ui->actionInfoApproachShow);
  menu.addSeparator();

  menu.addAction(ui->actionSearchProcedureFollowSelection);
  menu.addSeparator();

  QVector<QAction *> runwayActions;
  if(procData.sidStarRunways.isEmpty())
    menu.addAction(ui->actionInfoApproachAttach);
  else
    runwayActions = buildRunwaySubmenu(menu, procData);

  ui->actionInfoApproachExpandAll->setEnabled(!itemIndex.isEmpty());
  ui->actionInfoApproachCollapseAll->setEnabled(!itemIndex.isEmpty());

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
    // Reorder columns to match model order
    QHeaderView *header = treeWidget->header();
    for(int i = 0; i < header->count(); i++)
      header->moveSection(header->visualIndex(i), i);
    treeWidget->collapseAll();
    for(int i = 0; i < treeWidget->columnCount(); i++)
      treeWidget->resizeColumnToContents(i);
    NavApp::setStatusMessage(tr("Tree view reset to defaults."));
  }
  else if(action == ui->actionInfoApproachCollapseAll)
    treeWidget->collapseAll();
  else if(action == ui->actionInfoApproachClear)
    clearSelectionTriggered();
  else if(action == ui->actionInfoApproachShow)
    showEntry(item, false /* double click*/, true /* zoom */);
  else if(action == ui->actionInfoApproachAttach || runwayActions.contains(action))
  {
    if(procedureLegs != nullptr)
    {
      if(procedureLegs->hasError)
      {
        int result = atools::gui::Dialog(mainWindow).
                     showQuestionMsgBox(lnm::ACTIONS_SHOW_INVALID_PROC_WARNING,
                                        tr("Procedure has errors and will not display correctly.\n"
                                           "Really use it?"),
                                        tr("Do not &show this dialog again."),
                                        QMessageBox::Yes | QMessageBox::No,
                                        QMessageBox::No, QMessageBox::Yes);

        if(result == QMessageBox::Yes)
        {
          treeWidget->clearSelection();
          emit routeInsertProcedure(*procedureLegs, action->data().toString());
        }
      }
      else
      {
        treeWidget->clearSelection();
        emit routeInsertProcedure(*procedureLegs, action->data().toString());
      }
    }
    else
      qDebug() << Q_FUNC_INFO << "legs not found";
  }

  // Done by the actions themselves
  // else if(action == ui->actionInfoApproachShowAppr ||
  // action == ui->actionInfoApproachShowMissedAppr ||
  // action == ui->actionInfoApproachShowTrans)
}

QVector<QAction *> ProcedureSearch::buildRunwaySubmenu(QMenu& menu, const ProcData& procData)
{
  QVector<QAction *> actions;
  Ui::MainWindow *ui = NavApp::getMainUi();
  QMenu *sub = new QMenu(ui->actionInfoApproachAttach->text(), mainWindow);
  sub->setIcon(ui->actionInfoApproachAttach->icon());
  sub->setStatusTip(ui->actionInfoApproachAttach->statusTip());
  menu.addMenu(sub);

  int index = 1;
  for(const QString& runway : procData.sidStarRunways)
  {
    QString runwayText, runwayTip;
    if(procData.procedureRef.mapType & proc::PROCEDURE_SID_ALL)
    {
      runwayText = tr("Departure from %1");
      runwayTip = ui->actionInfoApproachAttach->statusTip() + " " + tr("Select departure runway for this SID");
    }
    else if(procData.procedureRef.mapType & proc::PROCEDURE_STAR_ALL)
    {
      runwayText = tr("Arrival at %1");
      runwayTip = ui->actionInfoApproachAttach->statusTip() + " " + tr("Select arrival runway for this STAR");
    }

    QString actionText = runway;
    if(procData.sidStarRunways.size() <= 3)
      // Use mnemonic on designator for short lists
      actionText = runwayText.arg(actionText.replace(QRegularExpression("([LRC])"), "&\\1"));
    else
      // Menmonic on first digit for long lists
      actionText = runwayText.arg(actionText.replace(QRegularExpression("([\\d]{2})"), "&\\1"));

    QAction *rwAction = new QAction(actionText, mainWindow);
    rwAction->setStatusTip(runwayTip);
    rwAction->setData(runway);
    sub->addAction(rwAction);
    actions.append(rwAction);
    index++;
  }
  return actions;
}

void ProcedureSearch::showEntry(QTreeWidgetItem *item, bool doubleClick, bool zoom)
{
  qDebug() << Q_FUNC_INFO;

  if(item == nullptr)
    return;

  MapProcedureRef ref = itemIndex.at(item->type()).procedureRef;
  fetchSingleTransitionId(ref);

  if(ref.legId != -1)
  {
    const proc::MapProcedureLeg *leg = nullptr;

    if(ref.transitionId != -1)
      leg = procedureQuery->getTransitionLeg(currentAirportNav, ref.legId);
    else if(ref.approachId != -1)
      leg = procedureQuery->getApproachLeg(currentAirportNav, ref.approachId, ref.legId);

    if(leg != nullptr)
    {
      emit showPos(leg->line.getPos2(), zoom ? 0.f : map::INVALID_DISTANCE_VALUE, doubleClick);

      if(doubleClick && (leg->navaids.hasNdb() || leg->navaids.hasVor() || leg->navaids.hasWaypoints()))
        emit showInformation(leg->navaids);
    }
  }
  else if(ref.transitionId != -1 && !doubleClick)
  {
    const proc::MapProcedureLegs *legs = procedureQuery->getTransitionLegs(currentAirportNav, ref.transitionId);
    if(legs != nullptr)
      emit showRect(legs->bounding, doubleClick);
  }
  else if(ref.approachId != -1 && !doubleClick)
  {
    const proc::MapProcedureLegs *legs = procedureQuery->getApproachLegs(currentAirportNav, ref.approachId);
    if(legs != nullptr)
      emit showRect(legs->bounding, doubleClick);
  }

}

QTreeWidgetItem *ProcedureSearch::buildApproachItem(QTreeWidgetItem *runwayItem, const SqlRecord& recApp,
                                                    proc::MapProcedureTypes maptype)
{
  QString suffix(recApp.valueStr("suffix"));
  QString type(recApp.valueStr("type"));
  int gpsOverlay = recApp.valueBool("has_gps_overlay");

  QString approachType;

  if(maptype == proc::PROCEDURE_SID)
    approachType += tr("SID");
  else if(maptype == proc::PROCEDURE_STAR)
    approachType += tr("STAR");
  else if(maptype == proc::PROCEDURE_APPROACH)
  {
    approachType = tr("Approach ") + proc::procedureType(type);

    if(!suffix.isEmpty())
      approachType += tr("-%1").arg(suffix);

    if(gpsOverlay)
      approachType += tr(" (GPS Overlay)");
  }

  approachType += " " + recApp.valueStr("airport_runway_name");
  approachType += " " + recApp.valueStr("sid_star_arinc_name", QString());

  QString altStr;
  // if(recApp.valueFloat("altitude") > 0.f)
  // altStr = Unit::altFeet(recApp.valueFloat("altitude"), false);

  QTreeWidgetItem *item = new QTreeWidgetItem({
    approachType,
    recApp.valueStr("fix_ident"),
    altStr
  }, itemIndex.size() - 1);
  item->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
  item->setTextAlignment(COL_RESTR, Qt::AlignRight);
  item->setTextAlignment(COL_COURSE, Qt::AlignRight);
  item->setTextAlignment(COL_DISTANCE, Qt::AlignRight);

  for(int i = 0; i < item->columnCount(); i++)
    item->setFont(i, approachFont);

  runwayItem->addChild(item);

  return item;
}

QTreeWidgetItem *ProcedureSearch::buildTransitionItem(QTreeWidgetItem *apprItem, const SqlRecord& recTrans,
                                                      bool sidOrStar)
{
  QString altStr;
  // if(recTrans.valueFloat("altitude") > 0.f)
  // altStr = Unit::altFeet(recTrans.valueFloat("altitude"), false);

  QString name(tr("Transition"));

  if(!sidOrStar)
  {
    if(recTrans.valueStr("type") == "F")
      name.append(tr(" (Full)"));
    else if(recTrans.valueStr("type") == "D")
      name.append(tr(" (DME)"));
  }

  QTreeWidgetItem *item = new QTreeWidgetItem({
    name,
    recTrans.valueStr("fix_ident"),
    altStr
  },
                                              itemIndex.size() - 1);
  item->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
  item->setTextAlignment(COL_RESTR, Qt::AlignRight);
  item->setTextAlignment(COL_COURSE, Qt::AlignRight);
  item->setTextAlignment(COL_DISTANCE, Qt::AlignRight);

  for(int i = 0; i < item->columnCount(); i++)
    item->setFont(i, transitionFont);

  apprItem->addChild(item);

  return item;
}

QTreeWidgetItem *ProcedureSearch::buildLegItem(const MapProcedureLeg& leg)
{
  QStringList texts;
  QIcon icon;
  int fontHeight = treeWidget->fontMetrics().height();
  texts << proc::procedureLegTypeStr(leg.type) << leg.fixIdent;

  QString restrictions;
  if(leg.altRestriction.isValid())
    restrictions.append(proc::altRestrictionTextShort(leg.altRestriction));
  if(leg.speedRestriction.isValid())
    restrictions.append("/" + proc::speedRestrictionTextShort(leg.speedRestriction));

  QString remarkStr = proc::procedureLegRemark(leg);

#ifdef DEBUG_INFORMATION
  remarkStr += QString(" | leg_id = %1 approach_id = %2 transition_id = %3 nav_id = %4").
               arg(leg.legId).arg(leg.approachId).arg(leg.transitionId).arg(leg.navId);
#endif

  texts << restrictions << proc::procedureLegCourse(leg) << proc::procedureLegDistance(leg);

  texts << remarkStr;

  QTreeWidgetItem *item = new QTreeWidgetItem(texts, itemIndex.size() - 1);
  if(!icon.isNull())
  {
    item->setIcon(0, icon);
    item->setSizeHint(0, QSize(fontHeight - 3, fontHeight - 3));
  }

  item->setToolTip(COL_REMARKS, remarkStr);

  item->setTextAlignment(COL_RESTR, Qt::AlignRight);
  item->setTextAlignment(COL_COURSE, Qt::AlignRight);
  item->setTextAlignment(COL_DISTANCE, Qt::AlignRight);

  setItemStyle(item, leg);
  return item;
}

void ProcedureSearch::setItemStyle(QTreeWidgetItem *item, const MapProcedureLeg& leg)
{
  bool invalid = leg.hasErrorRef();

  for(int i = 0; i < item->columnCount(); i++)
  {
    if(!invalid)
    {
      item->setFont(i, leg.missed ? missedLegFont : legFont);
      if(leg.missed)
        item->setForeground(i, OptionData::instance().isGuiStyleDark() ?
                            mapcolors::routeProcedureMissedTableColorDark :
                            mapcolors::routeProcedureMissedTableColor);
      else
        item->setForeground(i, OptionData::instance().isGuiStyleDark() ?
                            mapcolors::routeProcedureTableColorDark :
                            mapcolors::routeProcedureTableColor);
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

      if(item->type() < itemIndex.size() && itemIndex.at(item->type()).procedureRef.legId != -1)
        // Do not save legs
        continue;

      bool selected = item->isSelected();

      // Check if a leg is selected and push selection status down to the approach or transition
      // This avoids the need of expanding during loading which messes up the order
      for(int i = 0; i < item->childCount(); i++)
      {
        if(itemIndex.at(item->child(i)->type()).procedureRef.legId != -1 && item->child(i)->isSelected())
        {
          selected = true;
          break;
        }
      }

      state.resize(itemIdx + 2);
      state.setBit(itemIdx, item->isExpanded()); // Fist bit in triple: expanded or not
      state.setBit(itemIdx + 1, selected); // Second bit: selection state

      // qDebug() << item->text(COL_DESCRIPTION) << item->text(COL_IDENT)
      // << "expanded" << item->isExpanded() << "selected" << item->isSelected() << "child" << item->childCount();

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
    const MapProcedureRef& ref = itemIndex.at(current->type()).procedureRef;
    if(ref.hasApproachOnlyIds() && !ref.isLeg())
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
    const MapProcedureRef& ref = itemIndex.at(current->type()).procedureRef;
    if(ref.hasApproachAndTransitionIds() && !ref.isLeg())
      return current;

    current = current->parent();
  }
  return current != nullptr ? current : item;
}

void ProcedureSearch::getSelectedMapObjects(map::MapSearchResult& result) const
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
  if(NavApp::getMainUi()->tabWidgetSearch->currentIndex() != tabIndex)
  {
    // Hide preview if another tab is activated
    emit procedureSelected(proc::MapProcedureRef());
    emit procedureLegSelected(proc::MapProcedureRef());
  }
  else
    itemSelectionChanged();
}

/* Update highlights if dock is hidden or shown (does not change for dock tab stacks) */
void ProcedureSearch::dockVisibilityChanged(bool visible)
{
  if(!visible)
  {
    // Hide preview if dock is closed
    emit procedureSelected(proc::MapProcedureRef());
    emit procedureLegSelected(proc::MapProcedureRef());
  }
  else
    itemSelectionChanged();
}

void ProcedureSearch::tabDeactivated()
{
  emit procedureSelected(proc::MapProcedureRef());
  emit procedureLegSelected(proc::MapProcedureRef());
}

proc::MapProcedureTypes ProcedureSearch::buildTypeFromApproachRec(const SqlRecord& recApp)
{
  return proc::procedureType(NavApp::hasSidStarInDatabase(),
                             recApp.valueStr("type"), recApp.valueStr("suffix"),
                             recApp.valueBool("has_gps_overlay"));
}

bool ProcedureSearch::procedureSortFunc(const atools::sql::SqlRecord& rec1, const atools::sql::SqlRecord& rec2)
{
  static QHash<proc::MapProcedureTypes, int> priority(
  {
    {proc::PROCEDURE_SID, 0},
    {proc::PROCEDURE_STAR, 1},
    {proc::PROCEDURE_APPROACH, 2},
  });

  int priority1 = priority.value(buildTypeFromApproachRec(rec1));
  int priority2 = priority.value(buildTypeFromApproachRec(rec2));
  // First SID, then STAR and then approaches
  if(priority1 == priority2)
  {
    QString rwname1(rec1.valueStr("airport_runway_name"));
    QString rwname2(rec2.valueStr("airport_runway_name"));

    // Order by runway name
    if(rwname1 == rwname2)
    {
      QString ident1(rec1.valueStr("fix_ident"));
      QString ident2(rec2.valueStr("fix_ident"));

      if(ident1 == ident2)
        // Order by name + runway name
        return rec1.valueStr("sid_star_arinc_name") < rec2.valueStr("sid_star_arinc_name");
      else
        // Order by fix_ident
        return ident1 < ident2;
    }
    else
      return rwname1 < rwname2;
  }
  else
    return priority1 < priority2;
}
