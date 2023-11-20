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

#include "search/proceduresearch.h"

#include "app/navapp.h"
#include "atools.h"
#include "common/constants.h"
#include "common/formatter.h"
#include "common/mapcolors.h"
#include "common/unit.h"
#include "fs/util/fsutil.h"
#include "gui/actionstatesaver.h"
#include "gui/actiontextsaver.h"
#include "gui/dialog.h"
#include "gui/griddelegate.h"
#include "gui/itemviewzoomhandler.h"
#include "gui/tools.h"
#include "gui/widgetstate.h"
#include "query/airportquery.h"
#include "query/infoquery.h"
#include "query/mapquery.h"
#include "query/procedurequery.h"
#include "route/route.h"
#include "route/route.h"
#include "search/searchcontroller.h"
#include "settings/settings.h"
#include "sql/sqlrecord.h"
#include "ui_mainwindow.h"
#include "util/htmlbuilder.h"
#include "weather/weatherreporter.h"

#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QTreeWidget>
#include <QUrlQuery>
#include <QStringBuilder>

enum TreeColumnIndex
{
  /* Column order for approach overview (tree view) */
  COL_DESCRIPTION = 0,
  COL_IDENT = 1,
  COL_RESTR = 2,
  COL_COURSE = 3,
  COL_DISTANCE = 4,
  COL_WIND = 5,
  COL_REMARKS = 6,
  INVALID = -1
};

// Text for menu display of a procedure/transition without HTML. Attached to QTreeWidgetItem for procedures.
const static int TREEWIDGET_MENU_ROLE = Qt::UserRole;

// Text for header of a procedure/transition with HTML. Attached to QTreeWidgetItem for procedures.
const static int TREEWIDGET_HEADER_ROLE = Qt::UserRole + 1;

const static int COMBOBOX_RUNWAY_FILTER_ROLE = Qt::UserRole;
const static int COMBOBOX_PROCEDURE_FILTER_ROLE = Qt::UserRole;

using atools::sql::SqlRecord;

using atools::sql::SqlRecordList;
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
  TreeEventFilter(ProcedureSearch *parent)
    : QObject(parent), search(parent)
  {
  }

private:
  virtual bool eventFilter(QObject *object, QEvent *event) override;

  ProcedureSearch *search;
};

bool TreeEventFilter::eventFilter(QObject *object, QEvent *event)
{
  if(event->type() == QEvent::MouseButtonPress)
  {
    QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
    if(mouseEvent != nullptr && mouseEvent->button() == Qt::LeftButton)
    {
      QTreeWidgetItem *item = search->treeWidget->itemAt(mouseEvent->pos());
      if(item == nullptr || object == NavApp::getMainUi()->labelProcedureSearch)
        search->treeWidget->clearSelection();
    }

  }
  return QObject::eventFilter(object, event);
}

// =====================================================================================================

ProcedureSearch::ProcedureSearch(QMainWindow *main, QTreeWidget *treeWidgetParam, si::TabSearchId tabWidgetIndex)
  : AbstractSearch(main, tabWidgetIndex), treeWidget(treeWidgetParam)
{
  infoQuery = NavApp::getInfoQuery();
  procedureQuery = NavApp::getProcedureQuery();
  airportQueryNav = NavApp::getAirportQueryNav();

  currentAirportNav = new map::MapAirport;
  currentAirportSim = new map::MapAirport;

  zoomHandler = new atools::gui::ItemViewZoomHandler(treeWidget);
  connect(NavApp::navAppInstance(), &atools::gui::Application::fontChanged, this, &ProcedureSearch::fontChanged);
  gridDelegate = new atools::gui::GridDelegate(treeWidget);
  gridDelegate->setHeightIncrease(0);
  treeWidget->setItemDelegate(gridDelegate);
  atools::gui::adjustSelectionColors(treeWidget);

  transitionIndicator = tr(" (%L1 transitions)");
  transitionIndicatorOne = tr(" (one transition)");

  Ui::MainWindow *ui = NavApp::getMainUi();
  ui->comboBoxProcedureSearchFilter->insertSeparator(FILTER_SEPARATOR_1);

  ui->actionSearchProcedureSelectNothing->setShortcutContext(Qt::WidgetWithChildrenShortcut);

  ui->actionInfoApproachAttach->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionSearchProcedureInformation->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionSearchProcedureShowOnMap->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionSearchProcedureShowInSearch->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionInfoApproachShow->setShortcutContext(Qt::WidgetWithChildrenShortcut);

  treeWidget->addActions({ui->actionSearchProcedureSelectNothing, ui->actionInfoApproachAttach, ui->actionInfoApproachShow});

  ui->tabProcedureSearch->addActions({ui->actionSearchProcedureInformation, ui->actionSearchProcedureShowOnMap,
                                      ui->actionSearchProcedureShowInSearch});

  connect(ui->pushButtonProcedureSearchClearSelection, &QPushButton::clicked, this, &ProcedureSearch::clearSelectionClicked);
  connect(ui->pushButtonProcedureShowAll, &QPushButton::toggled, this, &ProcedureSearch::showAllToggled);

  connect(ui->pushButtonProcedureSearchReset, &QPushButton::clicked, this, &ProcedureSearch::resetSearch);
  connect(ui->actionSearchResetSearch, &QAction::triggered, this, &ProcedureSearch::resetSearch);

  connect(ui->actionSearchProcedureSelectNothing, &QAction::triggered, this, &ProcedureSearch::clearSelectionClicked);

  connect(ui->actionSearchProcedureInformation, &QAction::triggered, this, &ProcedureSearch::showInformationSelected);
  connect(ui->actionSearchProcedureShowOnMap, &QAction::triggered, this, &ProcedureSearch::showOnMapSelected);
  connect(ui->actionInfoApproachAttach, &QAction::triggered, this, &ProcedureSearch::procedureAttachSelected);
  connect(ui->actionInfoApproachClear, &QAction::triggered, this, &ProcedureSearch::clearSelectionClicked);
  connect(ui->actionInfoApproachShow, &QAction::triggered, this, &ProcedureSearch::showProcedureTriggered);

  connect(treeWidget, &QTreeWidget::itemSelectionChanged, this, &ProcedureSearch::itemSelectionChanged);
  connect(treeWidget, &QTreeWidget::itemDoubleClicked, this, &ProcedureSearch::itemDoubleClicked);
  connect(treeWidget, &QTreeWidget::itemExpanded, this, &ProcedureSearch::itemExpanded);
  connect(treeWidget, &QTreeWidget::customContextMenuRequested, this, &ProcedureSearch::contextMenu);
  connect(ui->comboBoxProcedureSearchFilter, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &ProcedureSearch::filterIndexChanged);
  connect(ui->comboBoxProcedureRunwayFilter, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &ProcedureSearch::filterIndexRunwayChanged);

  connect(ui->lineEditProcedureSearchIdentFilter, &QLineEdit::textChanged, this, &ProcedureSearch::filterChanged);

  connect(ui->dockWidgetSearch, &QDockWidget::visibilityChanged, this, &ProcedureSearch::dockVisibilityChanged);

  connect(ui->labelProcedureSearch, &QLabel::linkActivated, this, &ProcedureSearch::airportLabelLinkActivated);

  // Load text size from options
  zoomHandler->zoomPercent(OptionData::instance().getGuiSearchTableTextSize());

  createFonts();

  treeEventFilter = new TreeEventFilter(this);
  treeWidget->viewport()->installEventFilter(treeEventFilter);
}

ProcedureSearch::~ProcedureSearch()
{
  delete zoomHandler;
  treeWidget->setItemDelegate(nullptr);
  treeWidget->viewport()->removeEventFilter(treeEventFilter);
  delete treeEventFilter;
  delete gridDelegate;
  delete currentAirportNav;
  delete currentAirportSim;
}

void ProcedureSearch::airportLabelLinkActivated(const QString& link)
{
  qDebug() << Q_FUNC_INFO << link;
  // lnm://showairport

  QUrl url(link);
  if(url.scheme() == "lnm" && url.host() == "showairport")
  {
    showOnMapSelected();
    showInformationSelected();
  }
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
  if(NavApp::getSearchController()->getCurrentSearchTabId() == tabIndex)
  {
    // Only reset if this tab is active
    ui->comboBoxProcedureRunwayFilter->setCurrentIndex(FILTER_ALL_RUNWAYS);
    ui->comboBoxProcedureSearchFilter->setCurrentIndex(FILTER_ALL_PROCEDURES);
    ui->lineEditProcedureSearchIdentFilter->clear();
  }
}

void ProcedureSearch::updateFilter()
{
  treeWidget->clearSelection();
  fillProcedureTreeWidget();

  if(NavApp::getMainUi()->pushButtonProcedureShowAll->isChecked())
    showAllToggled(true);

  updateWidgets();
}

void ProcedureSearch::filterIndexChanged(int index)
{
  qDebug() << Q_FUNC_INFO;
  filterIndex = static_cast<FilterIndex>(index);
  updateFilter();
}

void ProcedureSearch::filterIndexRunwayChanged(int)
{
  qDebug() << Q_FUNC_INFO;
  updateFilter();
}

void ProcedureSearch::filterChanged(const QString&)
{
  qDebug() << Q_FUNC_INFO;
  updateFilter();
}

void ProcedureSearch::optionsChanged()
{
  QBitArray state = saveTreeViewState();

  // Adapt table view text size
  gridDelegate->styleChanged();
  atools::gui::adjustSelectionColors(treeWidget);
  zoomHandler->zoomPercent(OptionData::instance().getGuiSearchTableTextSize());
  createFonts();
  updateHeaderLabel();
  updateTreeHeader();
  fillProcedureTreeWidget();

  restoreTreeViewState(state, true /* block signals */);
}

void ProcedureSearch::styleChanged()
{
  // Need to clear the labels to force style update - otherwise link colors remain the same
  NavApp::getMainUi()->labelRouteInfo->clear();

  optionsChanged();
}

void ProcedureSearch::preDatabaseLoad()
{
  // Clear display on map
  emit procedureSelected(proc::MapProcedureRef());
  emit proceduresSelected(QVector<proc::MapProcedureRef>());
  emit procedureLegSelected(proc::MapProcedureRef());

  treeWidget->clear();

  itemIndex.clear();
  itemLoadedIndex.clear();
  *currentAirportNav = *currentAirportSim = map::MapAirport();
  recentTreeState.clear();
}

void ProcedureSearch::postDatabaseLoad()
{
  resetSearch();
  updateFilterBoxes();
  updateHeaderLabel();
  updateProcedureWind();
  updateWidgets();
}

void ProcedureSearch::showProcedures(const map::MapAirport& airport, bool departureFilter, bool arrivalFilter)
{
  map::MapAirport navAirport = NavApp::getMapQueryGui()->getAirportNav(airport);

  Ui::MainWindow *ui = NavApp::getMainUi();
  ui->dockWidgetSearch->show();
  ui->dockWidgetSearch->raise();
  NavApp::getSearchController()->setCurrentSearchTabId(si::SEARCH_PROC);
  treeWidget->setFocus();

  if(departureFilter)
  {
    ui->comboBoxProcedureSearchFilter->setCurrentIndex(FILTER_SID_PROCEDURES);
    ui->comboBoxProcedureRunwayFilter->setCurrentIndex(FILTER_ALL_RUNWAYS);
  }
  else if(arrivalFilter)
  {
    ui->comboBoxProcedureSearchFilter->setCurrentIndex(FILTER_ARRIVAL_PROCEDURES);
    ui->comboBoxProcedureRunwayFilter->setCurrentIndex(FILTER_ALL_RUNWAYS);
  }
  else
    ui->comboBoxProcedureSearchFilter->setCurrentIndex(FILTER_ALL_PROCEDURES);

  ui->lineEditProcedureSearchIdentFilter->clear();
  ui->pushButtonProcedureShowAll->setChecked(false);

  if(currentAirportNav->isValid() && currentAirportNav->ident == navAirport.ident)
    // Ignore if noting has changed - or jump out of the view mode
    return;

  emit procedureSelected(proc::MapProcedureRef());
  emit proceduresSelected(QVector<proc::MapProcedureRef>());
  emit procedureLegSelected(proc::MapProcedureRef());

  *currentAirportSim = airport;

  // Put state on stack and update tree
  if(currentAirportNav->isValid())
    recentTreeState.insert(currentAirportNav->id, saveTreeViewState());

  airportQueryNav->getAirportByIdent(*currentAirportNav, navAirport.ident);

  updateFilterBoxes();

  fillProcedureTreeWidget();

  restoreTreeViewState(recentTreeState.value(currentAirportNav->id), false /* block signals */);
  updateHeaderLabel();
  updateWidgets();
}

void ProcedureSearch::updateWidgets()
{
  Ui::MainWindow *ui = NavApp::getMainUi();
  ui->pushButtonProcedureShowAll->setDisabled(itemIndex.isEmpty());
  ui->pushButtonProcedureSearchClearSelection->setEnabled(!treeWidget->selectedItems().isEmpty() ||
                                                          ui->pushButtonProcedureShowAll->isChecked());
}

void ProcedureSearch::updateHeaderLabel()
{
  QString procs;

  const QList<QTreeWidgetItem *> items = treeWidget->selectedItems();
  for(QTreeWidgetItem *item : items)
    procs.append(procedureAndTransitionText(item, true /* header */));

  QString tooltip, statusTip;
  Ui::MainWindow *ui = NavApp::getMainUi();
  atools::util::HtmlBuilder html;

  if(currentAirportSim->isValid())
  {
    html.a(map::airportTextShort(*currentAirportSim), "lnm://showairport", atools::util::html::BOLD | atools::util::html::LINK_NO_UL);
    if(currentAirportNav->procedure())
    {
      QString title, runwayText, sourceText;
      NavApp::getWeatherReporter()->getBestRunwaysTextShort(title, runwayText, sourceText, *currentAirportSim);
      if(!sourceText.isEmpty())
        html.br().text(atools::strJoin({title, runwayText, sourceText}, tr(" ")));
      else
        html.br();
      html.br().text(procs, atools::util::html::NO_ENTITIES).nbsp();
    }
    else
      html.br().text(tr("Airport has no procedure.")).nbsp();
  }
  else
  {
    html.warning(tr("No Airport selected.")).br().nbsp();
    ui->labelProcedureSearch->setText(html.getHtml());

    tooltip = tr("<p style='white-space:pre'>Use the right-click context menu in the map or<br/>"
                 "the airport search result table (<code>F4</code>)<br/>"
                 "and select \"Show Procedures\" for an airport.</p>");
    statusTip = tr("Select \"Show Procedures\" for an airport to fill this list");
  }

  ui->labelProcedureSearch->setText(html.getHtml());
  ui->labelProcedureSearch->setToolTip(tooltip);
  ui->labelProcedureSearch->setStatusTip(statusTip);
  treeWidget->setToolTip(tooltip);
  treeWidget->setStatusTip(statusTip);

#ifdef DEBUG_INFORMATION_PROC
  ui->labelProcedureSearch->setText(ui->labelProcedureSearch->text() + (errors ? " ***ERRORS*** " : " ---OK---"));

#endif
}

QString ProcedureSearch::procedureAndTransitionText(const QTreeWidgetItem *item, bool header) const
{
  const QString pattern(header ? tr("%1 <b>%2</b>") : tr("%1 %2")),
  patternSpace(header ? tr(" %1 <b>%2</b>") : tr(" %1 %2")), viaPattern(tr(" via "));

  QString text;
  if(item != nullptr)
  {
    int role = header ? TREEWIDGET_HEADER_ROLE : TREEWIDGET_MENU_ROLE;
    MapProcedureRef ref = itemIndex.at(item->type());
    if(ref.isLeg())
    {
      item = item->parent();
      ref = itemIndex.at(item->type());
    }

    if(ref.hasProcedureOnlyIds())
    {
      // Only approach
      text.append(pattern.arg(item->data(COL_DESCRIPTION, role).toString()).arg(item->text(COL_IDENT)));

      if(item->childCount() == 1 && ref.mapType & proc::PROCEDURE_SID)
      {
        // Special SID case that has only transition legs and only one transition
        QTreeWidgetItem *child = item->child(0);
        if(child != nullptr)
        {
          text.append(viaPattern);
          text.append(pattern.arg(child->data(COL_DESCRIPTION, role).toString()).arg(child->text(COL_IDENT)));
        }
      }
    }
    else
    {
      if(ref.hasProcedureAndTransitionIds())
      {
        QTreeWidgetItem *appr = item->parent();
        if(appr != nullptr)
        {
          text.append(pattern.arg(appr->data(COL_DESCRIPTION, role).toString()).arg(appr->text(COL_IDENT)));
          text.append(viaPattern);
        }
      }
      text.append(patternSpace.arg(item->data(COL_DESCRIPTION, role).toString()).arg(item->text(COL_IDENT)));
    }
  }
  return text.simplified();
}

void ProcedureSearch::clearRunwayFilter()
{
  Ui::MainWindow *ui = NavApp::getMainUi();

  ui->comboBoxProcedureRunwayFilter->blockSignals(true);
  ui->comboBoxProcedureRunwayFilter->setCurrentIndex(FILTER_ALL_RUNWAYS);
  ui->comboBoxProcedureRunwayFilter->clear();
  ui->comboBoxProcedureRunwayFilter->addItem(tr("All Runways"));
  ui->comboBoxProcedureRunwayFilter->blockSignals(false);
}

void ProcedureSearch::clearTypeFilter()
{
  Ui::MainWindow *ui = NavApp::getMainUi();

  // Remove all additional type filters below approach all
  ui->comboBoxProcedureSearchFilter->blockSignals(true);
  for(int i = ui->comboBoxProcedureSearchFilter->count() - 1; i > FILTER_APPROACH_ALL; i--)
    ui->comboBoxProcedureSearchFilter->removeItem(i);
  ui->comboBoxProcedureSearchFilter->blockSignals(false);
}

void ProcedureSearch::updateFilterBoxes()
{
  Ui::MainWindow *ui = NavApp::getMainUi();

  clearRunwayFilter();
  clearTypeFilter();

  if(currentAirportNav->isValid())
  {
    QStringList runwayNames = airportQueryNav->getRunwayNames(currentAirportNav->id);

    // Add a tree of transitions and approaches
    const SqlRecordList *recAppVector = infoQuery->getApproachInformation(currentAirportNav->id);

    if(recAppVector != nullptr) // Deduplicate runways
    {
      // Update runway name filter combo box ============================================
      QSet<QString> runways;
      for(const SqlRecord& recApp : *recAppVector)
        runways.insert(atools::fs::util::runwayBestFit(recApp.valueStr("runway_name"), runwayNames));

      // Sort list of runways
      QList<QString> runwaylist = runways.values();
      std::sort(runwaylist.begin(), runwaylist.end());

      for(const QString& rw : qAsConst(runwaylist))
      {
        if(rw.isEmpty())
          ui->comboBoxProcedureRunwayFilter->addItem(tr("No Runway"), rw);
        else
          ui->comboBoxProcedureRunwayFilter->addItem(tr("Runway %1").arg(rw), rw);
      }

      // Update type filter combo box ============================================
      QStringList types;
      for(const SqlRecord& recApp : *recAppVector)
      {
        // No SID/STAR GPS fake types
        if(!(buildTypeFromProcedureRec(recApp) & proc::PROCEDURE_SID_STAR_ALL))
          types.append(recApp.valueStr("type"));
      }

      types.removeAll(QString());
      types.removeDuplicates();
      if(types.size() > 1)
      {
        std::sort(types.begin(), types.end());

        // Add to combo box - old types were deleted in clearTypeFilter()
        for(const QString& type : types)
          ui->comboBoxProcedureSearchFilter->addItem(tr("%1 Approaches").arg(proc::MapProcedureLegs::displayType(type)), type);
      }
    }
    else
      qWarning() << Q_FUNC_INFO << "nothing found for airport id" << currentAirportNav->id;
  }

  bool enable = currentAirportNav->isValid() && currentAirportNav->procedure();
  ui->comboBoxProcedureSearchFilter->setEnabled(enable);
  ui->comboBoxProcedureRunwayFilter->setEnabled(enable);
  ui->lineEditProcedureSearchIdentFilter->setEnabled(enable);
}

void ProcedureSearch::fillProcedureTreeWidget()
{
  treeWidget->blockSignals(true);
  treeWidget->clear();
  itemIndex.clear();
  itemLoadedIndex.clear();

  if(currentAirportNav->isValid())
  {
    // Add a tree of transitions and approaches
    const SqlRecordList *procedureRecords = infoQuery->getApproachInformation(currentAirportNav->id);

    if(procedureRecords != nullptr)
    {
      QStringList runwayNames = airportQueryNav->getRunwayNames(currentAirportNav->id);
      Ui::MainWindow *ui = NavApp::getMainUi();
      QTreeWidgetItem *root = treeWidget->invisibleRootItem();
      QVector<SqlRecord> sortedProcedures;

      // Collect all procedures from the database
      for(SqlRecord procedureRec : *procedureRecords)
      {
#ifdef DEBUG_INFORMATION_PROCSEARCH
        qDebug() << Q_FUNC_INFO << "procedureRec " << procedureRec;
#endif

        proc::MapProcedureTypes type = buildTypeFromProcedureRec(procedureRec);

        bool filterOk = false;

        if(filterIndex == ProcedureSearch::FILTER_ALL_PROCEDURES)
          filterOk = true;
        else if(filterIndex == ProcedureSearch::FILTER_SID_PROCEDURES)
          filterOk = type & proc::PROCEDURE_SID_ALL;
        else if(filterIndex == ProcedureSearch::FILTER_STAR_PROCEDURES)
          filterOk = type & proc::PROCEDURE_STAR_ALL;
        else if(filterIndex == ProcedureSearch::FILTER_ARRIVAL_PROCEDURES)
          filterOk = type & proc::PROCEDURE_ARRIVAL_ALL;
        else if(filterIndex >= ProcedureSearch::FILTER_APPROACH_ALL) // More types after FILTER_APPROACH_ALL
          filterOk = type & proc::PROCEDURE_APPROACH_ALL_MISSED;

        // Resolve parallel runway assignments
        QStringList sidStarArincNames, sidStarRunways;
        QString allRunwayText(tr("All"));
        if(type & proc::PROCEDURE_SID_STAR_ALL)
          atools::fs::util::sidStarMultiRunways(runwayNames, procedureRec.valueStr("arinc_name", QString()), allRunwayText,
                                                &sidStarRunways, &sidStarArincNames);
        QString rwName;
        if(!procedureRec.valueStr("runway_name").isEmpty())
          rwName = atools::fs::util::runwayBestFit(procedureRec.valueStr("runway_name"), runwayNames);

        QString rwNamefilter = ui->comboBoxProcedureRunwayFilter->currentData(COMBOBOX_RUNWAY_FILTER_ROLE).toString();
        int rwNameIndex = ui->comboBoxProcedureRunwayFilter->currentIndex();

        if(rwNameIndex == 0)
          // All selected
          filterOk &= true;
        else if(rwNamefilter.isEmpty())
          // No rwy selected
          filterOk &= rwName.isEmpty() && sidStarArincNames.isEmpty();
        else
          filterOk &= rwName == rwNamefilter || // name equal
                      (!sidStarArincNames.isEmpty() && sidStarArincNames.contains(rwNamefilter)) ||
                      sidStarArincNames.contains(allRunwayText);

        if(filterOk)
        {
          // Add an extra field with the best airport runway name
          procedureRec.appendField("airport_runway_name", QVariant::String);
          procedureRec.setValue("airport_runway_name", rwName);

          // Keep the runways for the context menu
          procedureRec.appendField("sid_star_runways", QVariant::StringList);
          procedureRec.setValue("sid_star_runways", sidStarRunways);

          procedureRec.appendField("sid_star_arinc_name", QVariant::String);
          if(type & proc::PROCEDURE_SID_STAR_ALL && rwName.isEmpty())
            procedureRec.setValue("sid_star_arinc_name", sidStarArincNames.join(", "));

          sortedProcedures.append(procedureRec);
        }
      }

      std::sort(sortedProcedures.begin(), sortedProcedures.end(), procedureSortFunc);

      QString typefilter = ui->comboBoxProcedureSearchFilter->currentData(COMBOBOX_PROCEDURE_FILTER_ROLE).toString();
      int typeindex = ui->comboBoxProcedureSearchFilter->currentIndex();

      for(const SqlRecord& recApp : sortedProcedures)
      {
#ifdef DEBUG_INFORMATION_PROCSEARCH
        qDebug() << Q_FUNC_INFO << "recApp " << recApp;
#endif

        proc::MapProcedureTypes type = buildTypeFromProcedureRec(recApp);

        // Check ident filter ==========================================
        QString identFilter = ui->lineEditProcedureSearchIdentFilter->text();
        if(!identFilter.isEmpty())
        {
          if(!recApp.valueStr("fix_ident").startsWith(identFilter, Qt::CaseInsensitive) &&
             !(type & proc::PROCEDURE_APPROACH && recApp.valueStr("arinc_name").startsWith(identFilter, Qt::CaseInsensitive)))
            continue;
        }

        // Check type name filter ==========================================
        if(!(type & proc::PROCEDURE_SID_STAR_ALL) && typeindex > FILTER_APPROACH_ALL && recApp.valueStr("type") != typefilter)
          continue;

        int runwayEndId = recApp.valueInt("runway_end_id");
        int apprId = recApp.valueInt("approach_id");
        itemIndex.append(MapProcedureRef(currentAirportNav->id, runwayEndId, apprId, -1, -1, type));

        // Get transitions ======================================================
        const SqlRecordList *transitionRecords = infoQuery->getTransitionInformation(recApp.valueInt("approach_id"));

        QString prefix;
        if(type & proc::PROCEDURE_APPROACH)
          prefix = tr("Approach "); // SID and STAR prefix is already a part of the text

        QString procTypeText, // "RNAV 32-Y" or "ILS 16 (one transition)"
                headerText, // "RNAV 32-Y" or "ILS 16" in HTML
                menuText, // "RNAV 32-Y" or "ILS 16" in plain text
                ident; // Fix ident and ARINC name in () for approaches
        QStringList attText; // "GPS Overlay", etc.
        procedureDisplayText(procTypeText, headerText, menuText, attText, recApp, type,
                             transitionRecords != nullptr ? transitionRecords->size() : 0);

        ident = recApp.valueStr("fix_ident");
        if(type & proc::PROCEDURE_APPROACH && !recApp.valueStr("arinc_name").isEmpty())
          ident.append(tr(" (%1)").arg(recApp.valueStr("arinc_name")));

        QTreeWidgetItem *apprItem = buildProcedureItem(root, ident, prefix % procTypeText, prefix % headerText,
                                                       prefix % menuText, attText);
        if(transitionRecords != nullptr)
        {
          // Transitions for this approach
          for(const SqlRecord& transitionRec : *transitionRecords)
          {
            // Also add runway from parent approach to transition
            itemIndex.append({MapProcedureRef(currentAirportNav->id, runwayEndId, apprId,
                                              transitionRec.valueInt("transition_id"), -1, type)});
            buildTransitionItem(apprItem, transitionRec, type & proc::PROCEDURE_DEPARTURE || type & proc::PROCEDURE_STAR_ALL);
          }
        }
      }
    }
    itemLoadedIndex.resize(itemIndex.size());
  }

  updateProcedureWind();
  treeWidget->blockSignals(false);
}

void ProcedureSearch::saveState()
{
  Ui::MainWindow *ui = NavApp::getMainUi();
  WidgetState(lnm::APPROACHTREE_WIDGET).save({ui->comboBoxProcedureSearchFilter, ui->comboBoxProcedureRunwayFilter,
                                              ui->actionSearchProcedureFollowSelection, ui->lineEditProcedureSearchIdentFilter});

  atools::settings::Settings& settings = atools::settings::Settings::instance();

  // Use current state and update the map too
  QBitArray state = saveTreeViewState();
  if(currentAirportNav->isValid())
    recentTreeState.insert(currentAirportNav->id, state);
  settings.setValueVar(lnm::APPROACHTREE_STATE, state);

  // Save column order and width
  WidgetState(lnm::APPROACHTREE_WIDGET).save(treeWidget);
  if(currentAirportNav->isValid())
    settings.setValue(lnm::APPROACHTREE_AIRPORT_NAV, currentAirportNav->id);
  if(currentAirportSim->isValid())
    settings.setValue(lnm::APPROACHTREE_AIRPORT_SIM, currentAirportSim->id);
}

void ProcedureSearch::restoreState()
{
  atools::settings::Settings& settings = atools::settings::Settings::instance();
  if(OptionData::instance().getFlags() & opts::STARTUP_LOAD_SEARCH && !NavApp::isSafeMode())
  {
    if(NavApp::hasDataInDatabase())
    {
      airportQueryNav->getAirportById(*currentAirportNav, settings.valueInt(lnm::APPROACHTREE_AIRPORT_NAV, -1));
      NavApp::getAirportQuerySim()->getAirportById(*currentAirportSim, settings.valueInt(lnm::APPROACHTREE_AIRPORT_SIM, -1));
    }
  }

  updateFilterBoxes();

  QBitArray state;
  if(OptionData::instance().getFlags() & opts::STARTUP_LOAD_SEARCH && !NavApp::isSafeMode())
  {
    Ui::MainWindow *ui = NavApp::getMainUi();
    WidgetState(lnm::APPROACHTREE_WIDGET).restore({ui->comboBoxProcedureSearchFilter, ui->comboBoxProcedureRunwayFilter,
                                                   ui->actionSearchProcedureFollowSelection, ui->lineEditProcedureSearchIdentFilter});

    fillProcedureTreeWidget();
    if(currentAirportNav->isValid() && currentAirportNav->procedure())
    {
      state = settings.valueVar(lnm::APPROACHTREE_STATE).toBitArray();
      recentTreeState.insert(currentAirportNav->id, state);
    }
  }

  updateTreeHeader();
  WidgetState(lnm::APPROACHTREE_WIDGET).restore(treeWidget);

  if(OptionData::instance().getFlags() & opts::STARTUP_LOAD_SEARCH && !NavApp::isSafeMode())
  {
    // Restoring state will emit above signal
    if(currentAirportNav->isValid() && currentAirportNav->procedure())
      restoreTreeViewState(state, true /* block signals */);
  }

  updateHeaderLabel();
  updateWidgets();
}

void ProcedureSearch::updateTreeHeader()
{
  QTreeWidgetItem *header = new QTreeWidgetItem();

  header->setText(COL_DESCRIPTION, tr("Description"));
  header->setToolTip(COL_DESCRIPTION, tr("Procedure instruction."));

  header->setText(COL_IDENT, tr("Ident"));
  header->setToolTip(COL_IDENT, tr("ICAO ident of the navaid,"));

  header->setText(COL_RESTR, tr("Restriction\n%1/%2/angle").arg(Unit::getUnitAltStr()).arg(Unit::getUnitSpeedStr()));
  header->setToolTip(COL_RESTR, tr("Altitude restriction, speed limit or\nrequired descent flight path angle."));

  header->setText(COL_COURSE, tr("Course\n°M"));
  header->setToolTip(COL_COURSE, tr("Magnetic course to fly."));

  header->setText(COL_DISTANCE, tr("Dist./Time\n%1/min").arg(Unit::getUnitDistStr()));
  header->setToolTip(COL_DISTANCE, tr("Distance to fly in %1 or flying time in minutes.").arg(Unit::getUnitDistStr()));

  header->setText(COL_WIND, tr("Head- and Crosswind\n%1").arg(Unit::getUnitSpeedStr()));
  header->setToolTip(COL_WIND, tr("Head- and crosswind components in %1 for departure or arrival runway.\n"
                                  "Weather source is selected in menu \"Weather\" -> \"Airport Weather Source\".\n"
                                  "Tailwinds are omitted.").arg(Unit::getUnitSpeedStr()));

  header->setText(COL_REMARKS, tr("Remarks"));
  header->setToolTip(COL_REMARKS, tr("Turn instructions, flyover or related navaid for procedure legs."));

  for(int col = COL_DESCRIPTION; col <= COL_REMARKS; col++)
    header->setTextAlignment(col, Qt::AlignCenter);

  treeWidget->setHeaderItem(header);
}

/* If approach has no legs and a single transition: SID special case. get transition id from cache */
void ProcedureSearch::fetchSingleTransitionId(MapProcedureRef& ref) const
{
  if(ref.hasProcedureOnlyIds())
  {
    // No transition
    const proc::MapProcedureLegs *legs = procedureQuery->getProcedureLegs(*currentAirportNav, ref.procedureId);
    if(legs != nullptr && legs->procedureLegs.isEmpty())
    {
      // Special case for SID which consists only of transition legs
      QVector<int> transitionIds = procedureQuery->getTransitionIdsForProcedure(ref.procedureId);
      if(!transitionIds.isEmpty())
        ref.transitionId = transitionIds.constFirst();
    }
  }
}

void ProcedureSearch::itemSelectionChangedInternal(bool noFollow)
{
#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO;
#endif

  Ui::MainWindow *ui = NavApp::getMainUi();
  const QList<QTreeWidgetItem *> selectedItems = treeWidget->selectedItems();
  if(selectedItems.isEmpty() || NavApp::getSearchController()->getCurrentSearchTabId() != tabIndex)
  {
    emit procedureSelected(proc::MapProcedureRef());
    emit procedureLegSelected(proc::MapProcedureRef());
  }
  else
  {
    for(QTreeWidgetItem *item : selectedItems)
    {
      MapProcedureRef ref = itemIndex.at(item->type());

      qDebug() << Q_FUNC_INFO << ref.runwayEndId << ref.procedureId << ref.transitionId << ref.legId;

      if(ref.hasProcedureOrTransitionIds())
      {
        fetchSingleTransitionId(ref);
        emit procedureSelected(ref);

        if(!noFollow && !ref.isLeg() && ui->actionSearchProcedureFollowSelection->isChecked())
          showEntry(item, false /* double click*/, true /* zoom */);
      }

      if(ref.isLeg())
      {
        // Highlight legs
        emit procedureLegSelected(ref);
        if(!noFollow && ui->actionSearchProcedureFollowSelection->isChecked())
          showEntry(item, false /* double click*/, false /* zoom */);
      }
      else
        // Remove leg highlight
        emit procedureLegSelected(proc::MapProcedureRef());

      if(ref.hasProcedureAndTransitionIds())
        updateProcedureItem(item, ref.transitionId);
    }
  }

  if(NavApp::getSearchController()->getCurrentSearchTabId() == tabIndex && ui->pushButtonProcedureShowAll->isChecked())
    emit proceduresSelected(itemIndex);
  else
    emit proceduresSelected(QVector<proc::MapProcedureRef>());

  updateHeaderLabel();
  updateWidgets();
}

/* Update course and distance for the parent approach of this leg item */
void ProcedureSearch::updateProcedureItem(QTreeWidgetItem *apprItem, int transitionId)
{
  if(apprItem != nullptr)
  {
    for(int i = 0; i < apprItem->childCount(); i++)
    {
      QTreeWidgetItem *child = apprItem->child(i);
      const MapProcedureRef& childref = itemIndex.at(child->type());
      if(childref.isLeg())
      {
        const proc::MapProcedureLegs *legs = procedureQuery->getTransitionLegs(*currentAirportNav, transitionId);
        if(legs != nullptr)
        {
          const proc::MapProcedureLeg *aleg = legs->transitionLegById(childref.legId);

          if(aleg != nullptr)
          {
            child->setText(COL_COURSE, proc::procedureLegCourse(*aleg));
            child->setText(COL_DISTANCE, proc::procedureLegDistance(*aleg));
          }
          else
            qWarning() << Q_FUNC_INFO << "Transition legs not found" << childref.legId;
        }
        else
          qWarning() << Q_FUNC_INFO << "Transition not found" << transitionId;
      }
    }
  }
}

void ProcedureSearch::itemDoubleClicked(QTreeWidgetItem *item, int)
{
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
    const MapProcedureRef ref = itemIndex.at(item->type());

    if(ref.legId == -1)
    {
      if(ref.procedureId != -1 && ref.transitionId == -1)
      {
        const MapProcedureLegs *legs = procedureQuery->getProcedureLegs(*currentAirportNav, ref.procedureId);
        if(legs != nullptr)
        {
          QList<QTreeWidgetItem *> items = buildProcedureLegItems(legs, -1);
          itemLoadedIndex.setBit(item->type());

          if(legs->mapType & proc::PROCEDURE_DEPARTURE)
            item->insertChildren(0, items);
          else
            item->addChildren(items);
        }
        else
          qWarning() << Q_FUNC_INFO << "no legs found for" << currentAirportNav->id << ref.procedureId;
      }
      else if(ref.procedureId != -1 && ref.transitionId != -1)
      {
        const MapProcedureLegs *legs = procedureQuery->getTransitionLegs(*currentAirportNav, ref.transitionId);
        if(legs != nullptr)
        {
          QList<QTreeWidgetItem *> items = buildTransitionLegItems(legs);
          item->addChildren(items);
          itemLoadedIndex.setBit(item->type());
        }
        else
          qWarning() << Q_FUNC_INFO << "no legs found for" << currentAirportNav->id << ref.transitionId;
      }
      itemLoadedIndex.resize(itemIndex.size());
    }
  }
}

QList<QTreeWidgetItem *> ProcedureSearch::buildProcedureLegItems(const MapProcedureLegs *legs, int transitionId)
{
  QList<QTreeWidgetItem *> items;
  if(legs != nullptr)
  {
    for(const MapProcedureLeg& leg : legs->procedureLegs)
    {
      itemIndex.append(MapProcedureRef(legs->ref.airportId, legs->ref.runwayEndId, legs->ref.procedureId, transitionId, leg.legId,
                                       legs->mapType));
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
      itemIndex.append(MapProcedureRef(legs->ref.airportId, legs->ref.runwayEndId, legs->ref.procedureId, legs->ref.transitionId, leg.legId,
                                       legs->mapType));
      items.append(buildLegItem(leg));
    }
  }
  return items;
}

void ProcedureSearch::showAllToggled(bool checked)
{
  qDebug() << Q_FUNC_INFO;
  if(!checked)
    emit proceduresSelected(QVector<proc::MapProcedureRef>());
  else
    emit proceduresSelected(itemIndex);

  updateWidgets();
}

void ProcedureSearch::clearSelectionClicked()
{
  NavApp::getMainUi()->pushButtonProcedureShowAll->setChecked(false);
  treeWidget->clearSelection();

  emit procedureSelected(proc::MapProcedureRef());
  emit procedureLegSelected(proc::MapProcedureRef());
}

void ProcedureSearch::contextMenu(const QPoint& pos)
{
  qDebug() << Q_FUNC_INFO;

  QPoint menuPos = QCursor::pos();
  // Use widget center if position is not inside widget
  if(!treeWidget->rect().contains(treeWidget->mapFromGlobal(QCursor::pos())))
    menuPos = treeWidget->mapToGlobal(treeWidget->rect().center());

  // Move menu position off the cursor to avoid accidental selection on touchpads
  menuPos += QPoint(3, 3);

  // Save text which will be changed below
  Ui::MainWindow *ui = NavApp::getMainUi();
  ActionTextSaver saver({ui->actionInfoApproachShow, ui->actionSearchProcedureFollowSelection,
                         ui->actionInfoApproachAttach, ui->actionInfoApproachExpandAll,
                         ui->actionInfoApproachCollapseAll, ui->actionSearchProcedureInformation,
                         ui->actionSearchProcedureShowOnMap, ui->actionSearchProcedureShowInSearch,
                         ui->actionInfoApproachExpandAll, ui->actionInfoApproachCollapseAll,
                         ui->actionSearchResetSearch, ui->actionInfoApproachClear, ui->actionSearchResetView});

  ActionStateSaver stateSaver({
    ui->actionInfoApproachShow, ui->actionSearchProcedureFollowSelection, ui->actionInfoApproachAttach,
    ui->actionInfoApproachExpandAll, ui->actionInfoApproachCollapseAll, ui->actionSearchProcedureInformation,
    ui->actionSearchProcedureShowOnMap, ui->actionSearchProcedureShowInSearch, ui->actionInfoApproachExpandAll,
    ui->actionInfoApproachCollapseAll, ui->actionSearchResetSearch, ui->actionInfoApproachClear,
    ui->actionSearchResetView
  });

  QTreeWidgetItem *item = treeWidget->itemAt(pos);

  ui->actionInfoApproachClear->setEnabled(hasSelection());
  ui->actionSearchResetSearch->setEnabled(treeWidget->topLevelItemCount() > 0);
  ui->actionInfoApproachShow->setDisabled(item == nullptr);

  const Route& route = NavApp::getRouteConst();

  ui->actionInfoApproachAttach->setDisabled(item == nullptr);

  MapProcedureRef ref;
  const proc::MapProcedureLegs *procedureLegs = fetchProcData(ref, item);

  if(item != nullptr)
  {
    if(procedureLegs != nullptr && !procedureLegs->isEmpty())
    {
      QString showText, text = procedureAndTransitionText(item, false /* header */);

      if(!text.isEmpty())
        ui->actionInfoApproachShow->setEnabled(true);

      if(ref.isLeg())
        showText = item->text(COL_IDENT).isEmpty() ? tr("Position") : item->text(COL_IDENT);
      else
        showText = text;

      ui->actionInfoApproachShow->setText(ui->actionInfoApproachShow->text().arg(showText));

      if((route.hasValidDeparture() &&
          route.getDepartureAirportLeg().getId() == currentAirportSim->id && ref.mapType & proc::PROCEDURE_DEPARTURE) ||
         (route.hasValidDestination() && route.getDestinationAirportLeg().getId() == currentAirportSim->id &&
          ref.mapType & proc::PROCEDURE_ARRIVAL_ALL))
        ui->actionInfoApproachAttach->setText(tr("&Insert %1 into Flight Plan").arg(text));
      else
      {
        if(ref.mapType & proc::PROCEDURE_ARRIVAL_ALL)
          ui->actionInfoApproachAttach->setText(tr("&Use %1 and %2 as Destination").arg(currentAirportSim->displayIdent()).arg(text));

        else if(ref.mapType & proc::PROCEDURE_DEPARTURE)
          ui->actionInfoApproachAttach->setText(tr("&Use %1 and %2 as Departure").arg(currentAirportSim->displayIdent()).arg(text));
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

  // Build airport context menu entries ====================================================
  ui->actionSearchProcedureInformation->setEnabled(currentAirportSim->isValid());
  ui->actionSearchProcedureShowOnMap->setEnabled(currentAirportSim->isValid());
  ui->actionSearchProcedureShowInSearch->setEnabled(currentAirportSim->isValid());

  QString airportText = currentAirportSim->isValid() ? map::airportTextShort(*currentAirportSim) : tr("Airport");
  ui->actionSearchProcedureInformation->setText(ui->actionSearchProcedureInformation->text().arg(airportText));
  ui->actionSearchProcedureShowOnMap->setText(ui->actionSearchProcedureShowOnMap->text().arg(airportText));
  ui->actionSearchProcedureShowInSearch->setText(ui->actionSearchProcedureShowInSearch->text().arg(airportText));

  // Show all procedures in preview =======================
  ui->actionSearchProcedureShowAll->setEnabled(!itemIndex.isEmpty());
  ui->actionSearchProcedureShowAll->setChecked(ui->pushButtonProcedureShowAll->isChecked());

  ui->actionInfoApproachExpandAll->setEnabled(!itemIndex.isEmpty());
  ui->actionInfoApproachCollapseAll->setEnabled(!itemIndex.isEmpty());

  // Create menu ===================================================================================
  QMenu menu;
  menu.setToolTipsVisible(NavApp::isMenuToolTipsVisible());

  // Add actions =====================
  menu.addAction(ui->actionInfoApproachShow);
  menu.addSeparator();

  menu.addAction(ui->actionInfoApproachAttach);
  menu.addSeparator();

  // Airport actions
  menu.addAction(ui->actionSearchProcedureInformation);
  menu.addAction(ui->actionSearchProcedureShowOnMap);
  menu.addAction(ui->actionSearchProcedureShowInSearch);
  menu.addSeparator();

  menu.addAction(ui->actionSearchProcedureShowAll);
  menu.addSeparator();

  menu.addAction(ui->actionSearchProcedureFollowSelection);
  menu.addSeparator();

  menu.addAction(ui->actionInfoApproachExpandAll);
  menu.addAction(ui->actionInfoApproachCollapseAll);
  menu.addSeparator();

  menu.addAction(ui->actionSearchResetSearch);
  menu.addAction(ui->actionInfoApproachClear);
  menu.addAction(ui->actionSearchResetView);

  // Execute menu =============================================================
  QAction *action = menu.exec(menuPos);

  if(action == ui->actionInfoApproachExpandAll)
  {
#ifdef DEBUG_INFORMATION
    const QTreeWidgetItem *root = treeWidget->invisibleRootItem();
    for(int i = 0; i < root->childCount(); ++i)
    {
      QTreeWidgetItem *itm = root->child(i);
      itm->setExpanded(true);
      for(int j = 0; j < itm->childCount(); ++j)
        itm->child(j)->setExpanded(true);
    }

    if(errors)
      updateHeaderLabel();

#else
    const QTreeWidgetItem *root = treeWidget->invisibleRootItem();
    for(int i = 0; i < root->childCount(); ++i)
      root->child(i)->setExpanded(true);
#endif
  }
  else if(action == ui->actionInfoApproachCollapseAll)
    treeWidget->collapseAll();
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
  else if(action == ui->actionSearchProcedureShowAll)
    // Button signal triggers procedure display
    ui->pushButtonProcedureShowAll->setChecked(ui->actionSearchProcedureShowAll->isChecked());
  else if(action == ui->actionSearchProcedureShowInSearch)
  {
    NavApp::getSearchController()->setCurrentSearchTabId(si::SEARCH_AIRPORT);
    emit showInSearch(map::AIRPORT, SqlRecord().appendFieldAndValue("ident", currentAirportSim->ident), true /* select */);
  }

  // else Other are done by the actions themselves
}

void ProcedureSearch::showInformationSelected()
{
  // ui->actionSearchProcedureInformation
  if(currentAirportSim->isValid())
  {
    map::MapResult result;
    result.airports.append(*currentAirportSim);
    emit showInformation(result);
  }
}

void ProcedureSearch::showOnMapSelected()
{
  // ui->actionSearchProcedureShowOnMap
  if(currentAirportSim->isValid())
    emit showRect(currentAirportSim->bounding, false /* doubleClick */);
}

const MapProcedureRef& ProcedureSearch::fetchProcRef(const QTreeWidgetItem *item) const
{
  const static MapProcedureRef EMPTY_REF;
  if(item != nullptr && !itemIndex.isEmpty())
  {
    const MapProcedureRef& procData = itemIndex.at(item->type());
    if(procData.isLeg() && item->parent() != nullptr)
      // Get parent approach data if approach is a leg
      return itemIndex.at(item->parent()->type());
    else
      return procData;
  }
  return EMPTY_REF;
}

const proc::MapProcedureLegs *ProcedureSearch::fetchProcData(MapProcedureRef& ref, const QTreeWidgetItem *item) const
{
  ref = fetchProcRef(item);

  if(!ref.isEmpty())
    // Get transition id too if SID with only transition legs is selected
    fetchSingleTransitionId(ref);

  const proc::MapProcedureLegs *procedureLegs = nullptr;
  // Get the aproach legs for the initial fix
  if(ref.hasProcedureOnlyIds())
    procedureLegs = procedureQuery->getProcedureLegs(*currentAirportNav, ref.procedureId);
  else if(ref.hasProcedureAndTransitionIds())
    procedureLegs = procedureQuery->getTransitionLegs(*currentAirportNav, ref.transitionId);
  return procedureLegs;
}

void ProcedureSearch::showProcedureTriggered()
{
  if(treeWidget->selectedItems().isEmpty())
    return;

  showEntry(treeWidget->selectedItems().constFirst(), false /* double click*/, true /* zoom */);
}

void ProcedureSearch::attachProcedure()
{
  if(treeWidget->selectedItems().isEmpty())
    return;

  MapProcedureRef ref;
  const proc::MapProcedureLegs *procedureLegs = fetchProcData(ref, treeWidget->selectedItems().constFirst());

  qDebug() << Q_FUNC_INFO << ref;

  if(procedureLegs != nullptr)
  {
    if(procedureLegs->hasHardError)
    {
      QMessageBox::warning(mainWindow, QApplication::applicationName(),
                           tr("Procedure has errors and cannot be added to the flight plan.\n"
                              "This can happen due to inconsistent navdata, missing waypoints or other reasons."));
    }
    else if(procedureLegs->hasError)
    {
      int result = atools::gui::Dialog(mainWindow).
                   showQuestionMsgBox(lnm::ACTIONS_SHOW_INVALID_PROC_WARNING,
                                      tr("Procedure has errors and will not display correctly.\n"
                                         "This can happen due to inconsistent navdata, "
                                         "missing waypoints or other reasons.\n\n"
                                         "Really use it?"),
                                      tr("Do not &show this dialog again."),
                                      QMessageBox::Yes | QMessageBox::No,
                                      QMessageBox::No, QMessageBox::Yes);

      if(result == QMessageBox::Yes)
      {
        treeWidget->clearSelection();
        emit routeInsertProcedure(*procedureLegs);
      }
    }
    else
    {
      treeWidget->clearSelection();
      emit routeInsertProcedure(*procedureLegs);
    }
  }
  else
    qDebug() << Q_FUNC_INFO << "legs not found";
}

void ProcedureSearch::procedureAttachSelected()
{
  // ui->actionInfoApproachAttach,
  attachProcedure();
}

void ProcedureSearch::showEntry(QTreeWidgetItem *item, bool doubleClick, bool zoom)
{
  qDebug() << Q_FUNC_INFO;

  if(item == nullptr)
    return;

  MapProcedureRef ref = itemIndex.at(item->type());
  fetchSingleTransitionId(ref);

  if(ref.legId != -1)
  {
    const proc::MapProcedureLeg *leg = nullptr;

    if(ref.transitionId != -1)
      leg = procedureQuery->getTransitionLeg(*currentAirportNav, ref.legId);
    else if(ref.procedureId != -1)
      leg = procedureQuery->getProcedureLeg(*currentAirportNav, ref.procedureId, ref.legId);

    if(leg != nullptr)
    {
      emit showPos(leg->line.getPos2(), zoom ? 0.f : map::INVALID_DISTANCE_VALUE, doubleClick);

      if(doubleClick && (leg->navaids.hasNdb() || leg->navaids.hasVor() || leg->navaids.hasWaypoints()))
        emit showInformation(leg->navaids);
    }
  }
  else if(ref.transitionId != -1 && !doubleClick)
  {
    const proc::MapProcedureLegs *legs = procedureQuery->getTransitionLegs(*currentAirportNav, ref.transitionId);
    if(legs != nullptr)
      emit showRect(legs->bounding, doubleClick);
  }
  else if(ref.procedureId != -1 && !doubleClick)
  {
    const proc::MapProcedureLegs *legs = procedureQuery->getProcedureLegs(*currentAirportNav, ref.procedureId);
    if(legs != nullptr)
      emit showRect(legs->bounding, doubleClick);
  }
}

void ProcedureSearch::procedureDisplayText(QString& procTypeText, QString& headerText, QString& menuText, QStringList& attText,
                                           const SqlRecord& recProc, proc::MapProcedureTypes maptype, int numTransitions)
{
  QString suffix = recProc.valueStr("suffix");
  QString type = recProc.valueStr("type");

  if(maptype == proc::PROCEDURE_SID)
    headerText = procTypeText = tr("SID");
  else if(maptype == proc::PROCEDURE_STAR)
    headerText = procTypeText = tr("STAR");
  else if(maptype == proc::PROCEDURE_APPROACH)
  {
    procTypeText = proc::procedureType(type);
    headerText = tr("<b>%1").arg(procTypeText);

    if(!suffix.isEmpty())
    {
      procTypeText += tr("-%1").arg(suffix);
      headerText += tr("-%1").arg(suffix);
    }
    headerText += tr("</b>");

    if(recProc.valueBool("has_gps_overlay"))
      attText.append(tr("GPS Overlay"));
  }

  if(!recProc.valueStr("airport_runway_name", QString()).isEmpty())
  {
    procTypeText.append(tr(" %1").arg(recProc.valueStr("airport_runway_name")));
    headerText.append(tr(" <b>%1</b>").arg(recProc.valueStr("airport_runway_name")));
  }

  if(!recProc.valueStr("sid_star_arinc_name", QString()).isEmpty())
  {
    procTypeText.append(tr(" %1").arg(recProc.valueStr("sid_star_arinc_name", QString())));
    headerText.append(tr(" <b>%1</b>").arg(recProc.valueStr("sid_star_arinc_name", QString())));
  }

  // Menu text is no HTML and same as row text
  menuText = procTypeText;

  // Appears for almost all approaches
  // if(recApp.valueBool("has_vertical_angle", false))
  // attText.append(tr("VNAV"));

  QString cat = proc::aircraftCategoryText(recProc.valueStr("aircraft_category", QString()));
  if(!cat.isEmpty())
    attText.append(cat);

  if(recProc.valueBool("has_rnp", false))
    attText.append(tr("RNP"));

  if(numTransitions == 1)
    procTypeText.append(transitionIndicatorOne);
  else if(numTransitions > 1)
    procTypeText.append(transitionIndicator.arg(numTransitions));
}

void ProcedureSearch::updateProcedureWind()
{
  if(currentAirportSim->isValid())
  {
    QTreeWidgetItem *root = treeWidget->invisibleRootItem();

    if(root->childCount() > 0)
    {
      int windDirectionDeg;
      float windSpeedKts;
      NavApp::getAirportWind(windDirectionDeg, windSpeedKts, *currentAirportSim, false /* stationOnly */);

      for(int i = 0; i < root->childCount(); i++)
      {
        QTreeWidgetItem *item = root->child(i);
        MapProcedureRef ref = fetchProcRef(item);

        QString windText;
        map::MapRunwayEnd runwayEnd = airportQueryNav->getRunwayEndById(ref.runwayEndId);
        if(runwayEnd.isValid())
          windText = formatter::windInformationShort(windDirectionDeg, windSpeedKts, runwayEnd.heading);
        item->setText(COL_WIND, windText);
      }
    }
  }
}

QTreeWidgetItem *ProcedureSearch::buildProcedureItem(QTreeWidgetItem *runwayItem, const QString& ident, const QString& procTypeText,
                                                     const QString& headerText, const QString& menuText, const QStringList& attStr)
{
  QTreeWidgetItem *item = new QTreeWidgetItem({procTypeText, ident, QString(), QString(), QString(), QString(), attStr.join(tr(", "))},
                                              itemIndex.size() - 1);
  item->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
  item->setTextAlignment(COL_RESTR, Qt::AlignRight);
  item->setTextAlignment(COL_COURSE, Qt::AlignRight);
  item->setTextAlignment(COL_DISTANCE, Qt::AlignRight);
  item->setData(COL_DESCRIPTION, TREEWIDGET_HEADER_ROLE, headerText);
  item->setData(COL_DESCRIPTION, TREEWIDGET_MENU_ROLE, menuText);

  // First columns bold
  for(int i = COL_DESCRIPTION; i <= COL_IDENT; i++)
    item->setFont(i, procedureBoldFont);

  // Rest in normal font
  for(int i = COL_RESTR; i <= COL_REMARKS; i++)
    item->setFont(i, procedureNormalFont);

  runwayItem->addChild(item);

  return item;
}

QTreeWidgetItem *ProcedureSearch::buildTransitionItem(QTreeWidgetItem *procItem, const SqlRecord& recTrans, bool sidOrStar)
{
  QStringList attStr;
  if(!sidOrStar)
  {
    if(recTrans.valueStr("type") == "F")
      attStr.append(tr("Full"));
    else if(recTrans.valueStr("type") == "D")
      attStr.append(tr("DME"));
  }

  QTreeWidgetItem *item = new QTreeWidgetItem({tr("Transition"), recTrans.valueStr("fix_ident"), QString(),
                                               QString(), QString(), QString(), attStr.join(tr(", "))}, itemIndex.size() - 1);
  item->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
  item->setTextAlignment(COL_RESTR, Qt::AlignRight);
  item->setTextAlignment(COL_COURSE, Qt::AlignRight);
  item->setTextAlignment(COL_DISTANCE, Qt::AlignRight);
  item->setData(COL_DESCRIPTION, TREEWIDGET_HEADER_ROLE, tr("transition"));
  item->setData(COL_DESCRIPTION, TREEWIDGET_MENU_ROLE, tr("transition"));

  // First columns bold
  for(int i = COL_DESCRIPTION; i <= COL_IDENT; i++)
    item->setFont(i, procedureBoldFont);

  // Rest normal
  for(int i = COL_RESTR; i <= COL_REMARKS; i++)
    item->setFont(i, procedureNormalFont);

  procItem->addChild(item);

  return item;
}

QTreeWidgetItem *ProcedureSearch::buildLegItem(const MapProcedureLeg& leg)
{
  QIcon icon;
  int fontHeight = treeWidget->fontMetrics().height();

  QStringList texts;
  texts << proc::procedureLegTypeStr(leg.type);
  texts << proc::procedureLegFixStr(leg);
  texts << proc::restrictionText(leg).join(tr(", ")) << proc::procedureLegCourse(leg) << proc::procedureLegDistance(leg);
  texts << QString(); // Wind filled in updateProcedureWind()

  QStringList remarkStr = proc::procedureLegRemark(leg);

  QStringList related = procedureLegRecommended(leg);
  if(!related.isEmpty())
    remarkStr.append(QObject::tr("Related: %1").arg(related.join(QObject::tr(", "))));

#ifdef DEBUG_INFORMATION
  remarkStr.append(QString(" | leg_id = %1 approach_id = %2 transition_id = %3").
                   arg(leg.legId).arg(leg.procedureId).arg(leg.transitionId));
#endif
  texts << remarkStr.join(tr(", "));

  QTreeWidgetItem *item = new QTreeWidgetItem(texts, itemIndex.size() - 1);
  if(!icon.isNull())
  {
    item->setIcon(0, icon);
    item->setSizeHint(0, QSize(fontHeight - 3, fontHeight - 3));
  }

  item->setToolTip(COL_REMARKS, remarkStr.join(tr(", ")));

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
        item->setForeground(i, NavApp::isCurrentGuiStyleNight() ?
                            mapcolors::routeProcedureMissedTableColorDark :
                            mapcolors::routeProcedureMissedTableColor);
      else
        item->setForeground(i, NavApp::isCurrentGuiStyleNight() ?
                            mapcolors::routeProcedureTableColorDark :
                            mapcolors::routeProcedureTableColor);
    }
    else
    {
      item->setFont(i, invalidLegFont);
      item->setForeground(i, Qt::red);
      errors = true;
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

      for(int i = 0; i < item->childCount(); ++i)
        itemStack.append(item->child(i));
      itemIdx += 2;
    }
  }
  return state;
}

void ProcedureSearch::restoreTreeViewState(const QBitArray& state, bool blockSignals)
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
    if(blockSignals)
      treeWidget->blockSignals(true);
    selectedItem->setSelected(true);
    if(blockSignals)
      treeWidget->blockSignals(false);

    treeWidget->scrollToItem(selectedItem, QAbstractItemView::PositionAtTop);
  }
}

void ProcedureSearch::createFonts()
{

  identFont = procedureNormalFont = procedureBoldFont = legFont = missedLegFont = invalidLegFont = treeWidget->font();
  identFont.setWeight(QFont::Bold);
  procedureBoldFont.setWeight(QFont::Bold);
  invalidLegFont.setWeight(QFont::Bold);
}

void ProcedureSearch::getSelectedMapObjects(map::MapResult&) const
{
}

void ProcedureSearch::connectSearchSlots()
{
}

void ProcedureSearch::updateUnits()
{
}

void ProcedureSearch::updateTableSelection(bool noFollow)
{
  if(NavApp::getSearchController()->getCurrentSearchTabId() != tabIndex)
  {
    // Hide preview if another tab is activated
    emit procedureSelected(proc::MapProcedureRef());
    emit procedureLegSelected(proc::MapProcedureRef());
  }
  else
    itemSelectionChangedInternal(noFollow);
}

void ProcedureSearch::clearSelection()
{
  treeWidget->clearSelection();
  NavApp::getMainUi()->pushButtonProcedureShowAll->setChecked(false);
  updateWidgets();
}

bool ProcedureSearch::hasSelection() const
{
  return treeWidget->selectionModel()->hasSelection() || NavApp::getMainUi()->pushButtonProcedureShowAll->isChecked();
}

void ProcedureSearch::weatherUpdated()
{
  updateHeaderLabel();
  updateProcedureWind();
}

/* Update highlights if dock is hidden or shown (does not change for dock tab stacks) */
void ProcedureSearch::dockVisibilityChanged(bool visible)
{
  if(!visible)
  {
    // Hide preview if dock is closed
    emit procedureSelected(proc::MapProcedureRef());
    emit proceduresSelected(QVector<proc::MapProcedureRef>());
    emit procedureLegSelected(proc::MapProcedureRef());
  }
  else
    itemSelectionChangedInternal(true /* do not follow selection */);
}

void ProcedureSearch::tabDeactivated()
{
  emit procedureSelected(proc::MapProcedureRef());
  emit proceduresSelected(QVector<proc::MapProcedureRef>());
  emit procedureLegSelected(proc::MapProcedureRef());
}

void ProcedureSearch::itemSelectionChanged()
{
  itemSelectionChangedInternal(false /* follow selection */);
}

proc::MapProcedureTypes ProcedureSearch::buildTypeFromProcedureRec(const SqlRecord& recApp)
{
  return proc::procedureType(NavApp::hasSidStarInDatabase(), recApp.valueStr("type"), recApp.valueStr("suffix"),
                             recApp.valueBool("has_gps_overlay"));
}

bool ProcedureSearch::procedureSortFunc(const atools::sql::SqlRecord& rec1, const atools::sql::SqlRecord& rec2)
{
  static QHash<proc::MapProcedureTypes, int> priority({
    {proc::PROCEDURE_SID, 0}, {proc::PROCEDURE_STAR, 1}, {proc::PROCEDURE_APPROACH, 2}, });

  int priority1 = priority.value(buildTypeFromProcedureRec(rec1));
  int priority2 = priority.value(buildTypeFromProcedureRec(rec2));
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
