/*****************************************************************************
* Copyright 2015-2024 Alexander Barthel alex@littlenavmap.org
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
#include "gui/clicktooltiphandler.h"
#include "gui/dialog.h"
#include "gui/griddelegate.h"
#include "gui/itemviewzoomhandler.h"
#include "gui/tools.h"
#include "gui/widgetstate.h"
#include "query/airportquery.h"
#include "query/infoquery.h"
#include "query/mapquery.h"
#include "query/procedurequery.h"
#include "query/querymanager.h"
#include "route/route.h"
#include "route/route.h"
#include "search/searchcontroller.h"
#include "search/searcheventfilter.h"
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
  COL_FROMTO = 3,
  COL_COURSE = 4,
  COL_DISTANCE = 5,
  COL_WIND = 6,
  COL_REMARKS = 7,
  INVALID = -1
};

enum TreeRole
{
  // Text for menu display of a procedure/transition without HTML. Attached to QTreeWidgetItem for procedures.
  TREEWIDGET_MENU_ROLE = Qt::UserRole,
  // Text for header of a procedure/transition with HTML. Attached to QTreeWidgetItem for procedures.
  TREEWIDGET_HEADER_ROLE = Qt::UserRole + 1,
  // QStringList with first and last waypoint
  TREEWIDGET_HEADER_FIRSTLAST_ROLE = Qt::UserRole + 2
};

const static int COMBOBOX_RUNWAY_FILTER_ROLE = Qt::UserRole;
const static int COMBOBOX_PROCEDURE_FILTER_ROLE = Qt::UserRole;

const static proc::MapProcedureRef EMPTY_PROC_REF;

using atools::sql::SqlRecord;

using atools::sql::SqlRecordList;
using proc::MapProcedureLeg;
using proc::MapProcedureLegs;
using proc::MapProcedureRef;
using atools::gui::WidgetState;
using atools::gui::ActionTextSaver;
using atools::gui::ActionStateSaver;

/* Hash entry that maps from int QTreeWidgetItem::type() to the MapProcedureRef */
class ProcIndexEntry
{
public:
  ProcIndexEntry()
  {
  }

  ProcIndexEntry(QTreeWidgetItem *itemParam, int airportIdParam, int runwayEndIdParam, int procIdParam, int transIdParam,
                 int legIdParam, proc::MapProcedureTypes type)
    : item(itemParam), ref(airportIdParam, runwayEndIdParam, procIdParam, transIdParam, legIdParam, type)
  {
  }

  bool isValid() const
  {
    return item != nullptr;
  }

  QTreeWidgetItem *getItem() const
  {
    return item;
  }

  const proc::MapProcedureRef& getRef() const
  {
    return ref;
  }

private:
  QTreeWidgetItem *item = nullptr;
  proc::MapProcedureRef ref;
};

Q_DECLARE_TYPEINFO(ProcIndexEntry, Q_PRIMITIVE_TYPE);

// =============================================================================================

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

// =============================================================================================

bool TreeEventFilter::eventFilter(QObject *object, QEvent *event)
{
  if(event->type() == QEvent::MouseButtonPress)
  {
    QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
    if(mouseEvent != nullptr && mouseEvent->button() == Qt::LeftButton)
    {
      QTreeWidgetItem *item = search->treeWidget->itemAt(mouseEvent->pos());
      if(item == nullptr || object == NavApp::getMainUi()->labelProcedureSearch || object == NavApp::getMainUi()->labelProcedureSearchWarn)
        search->treeWidget->clearSelection();
    }
  }
  return QObject::eventFilter(object, event);
}

// =====================================================================================================

ProcedureSearch::ProcedureSearch(QMainWindow *main, QTreeWidget *treeWidgetParam, si::TabSearchId tabWidgetIndex)
  : AbstractSearch(main, tabWidgetIndex), treeWidget(treeWidgetParam)
{
  const Queries *queries = QueryManager::instance()->getQueriesGui();
  infoQuery = queries->getInfoQuery();
  airportQueryNav = queries->getAirportQueryNav();
  airportQuerySim = queries->getAirportQuerySim();
  procedureQuery = queries->getProcedureQuery();

  currentAirportNav = new map::MapAirport;
  currentAirportSim = new map::MapAirport;
  savedAirportSim = new map::MapAirport;

  zoomHandler = new atools::gui::ItemViewZoomHandler(treeWidget);
  connect(NavApp::navAppInstance(), &QGuiApplication::fontChanged, this, &ProcedureSearch::fontChanged);
  gridDelegate = new atools::gui::GridDelegate(treeWidget);
  gridDelegate->setHeightIncrease(0);
  treeWidget->setItemDelegate(gridDelegate);
  atools::gui::adjustSelectionColors(treeWidget);

  transitionIndicator = tr(" (%L1 transitions)");
  transitionIndicatorOne = tr(" (one transition)");

  Ui::MainWindow *ui = NavApp::getMainUi();

  ui->labelProcedureSearchWarn->installEventFilter(new atools::gui::ClickToolTipHandler(ui->labelProcedureSearchWarn));
  ui->labelProcedureSearchWarn->hide();

  ui->comboBoxProcedureSearchFilter->insertSeparator(FILTER_SEPARATOR);

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
  connect(ui->actionSearchProcedureShowOnMap, &QAction::triggered, this, &ProcedureSearch::showAirportOnMapSelected);
  connect(ui->actionInfoApproachAttach, &QAction::triggered, this, &ProcedureSearch::procedureAttachSelected);
  connect(ui->actionInfoApproachClear, &QAction::triggered, this, &ProcedureSearch::clearSelectionClicked);
  connect(ui->actionInfoApproachShow, &QAction::triggered, this, &ProcedureSearch::showProcedureSelected);

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

  lineInputEventFilter = new SearchWidgetEventFilter(this);
  ui->lineEditProcedureSearchIdentFilter->installEventFilter(lineInputEventFilter);
}

ProcedureSearch::~ProcedureSearch()
{
  ui->lineEditProcedureSearchIdentFilter->removeEventFilter(lineInputEventFilter);
  ATOOLS_DELETE_LOG(lineInputEventFilter);

  ATOOLS_DELETE_LOG(zoomHandler);
  treeWidget->setItemDelegate(nullptr);
  treeWidget->viewport()->removeEventFilter(treeEventFilter);
  ATOOLS_DELETE_LOG(treeEventFilter);
  ATOOLS_DELETE_LOG(gridDelegate);
  ATOOLS_DELETE_LOG(currentAirportNav);
  ATOOLS_DELETE_LOG(currentAirportSim);
  ATOOLS_DELETE_LOG(savedAirportSim);
}

void ProcedureSearch::airportLabelLinkActivated(const QString& link)
{
  qDebug() << Q_FUNC_INFO << link;
  // lnm://showairport

  QUrl url(link);
  if(url.scheme() == "lnm" && url.host() == "showairport")
  {
    showAirportOnMapSelected();
    showInformationSelected();
  }
}

void ProcedureSearch::fontChanged(const QFont&)
{
  qDebug() << Q_FUNC_INFO;

  optionsChanged();
  zoomHandler->zoomPercent(OptionData::instance().getGuiSearchTableTextSize());
}

void ProcedureSearch::resetSearch()
{
  Ui::MainWindow *ui = NavApp::getMainUi();
  if(NavApp::getSearchController()->getCurrentSearchTabId() == tabIndex)
  {
    // Only reset if this tab is active
    ui->comboBoxProcedureSearchFilter->setCurrentIndex(FILTER_ALL_PROCEDURES);
    ui->comboBoxProcedureRunwayFilter->setCurrentIndex(FILTER_ALL_RUNWAYS);
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
  filterIndex = static_cast<FilterIndex>(index);
  updateFilter();
}

void ProcedureSearch::filterIndexRunwayChanged(int)
{
  updateFilter();
}

void ProcedureSearch::filterChanged(const QString&)
{
  updateFilter();
}

void ProcedureSearch::optionsChanged()
{
  QSet<int> state = treeViewStateSave();

  // Adapt table view text size
  gridDelegate->styleChanged();
  atools::gui::adjustSelectionColors(treeWidget);
  zoomHandler->zoomPercent(OptionData::instance().getGuiSearchTableTextSize());
  createFonts();
  updateHeaderLabel();
  updateTreeHeader();
  fillProcedureTreeWidget();

  treeViewStateRestore(state);
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
  nextIndexId = 1;
  itemExpandedIndex.clear();
  *currentAirportNav = *currentAirportSim = map::MapAirport();
  recentTreeState.clear();
}

void ProcedureSearch::postDatabaseLoad()
{
  resetSearch();
  updateFilterBoxes();
  updateHeaderLabel();
  updateProcedureWind();
  clearSelection();

  showProceduresInternal(airportQuerySim->getAirportFuzzy(*savedAirportSim), savedDepartureFilter, savedArrivalFilter, true /* silent */);
}

void ProcedureSearch::showProcedures(const map::MapAirport& airport, bool departureFilter, bool arrivalFilter)
{
  showProceduresInternal(airport, departureFilter, arrivalFilter, false /* silent */);
}

void ProcedureSearch::showProceduresInternal(const map::MapAirport& airportSim, bool departureFilter, bool arrivalFilter, bool silent)
{
  map::MapAirport navAirport = QueryManager::instance()->getQueriesGui()->getMapQuery()->getAirportNav(airportSim);
  qDebug() << Q_FUNC_INFO << "airport" << airportSim << "navAirport" << navAirport
           << "departureFilter" << departureFilter << "arrivalFilter" << arrivalFilter;

  savedDepartureFilter = departureFilter;
  savedArrivalFilter = arrivalFilter;
  *savedAirportSim = airportSim;

  if(!silent)
  {
    Ui::MainWindow *ui = NavApp::getMainUi();
    ui->dockWidgetSearch->show();
    ui->dockWidgetSearch->raise();
    NavApp::getSearchController()->setCurrentSearchTabId(si::SEARCH_PROC);
    treeWidget->setFocus();
  }

  qDebug() << Q_FUNC_INFO << airportSim << navAirport;

#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << recentTreeState.keys();
#endif

  // Check if airport or settings have changed ==============================
  FilterIndex searchFilterIndex = FILTER_ALL_PROCEDURES;
  RunwayFilterIndex runwayFilterIndex = FILTER_ALL_RUNWAYS;

  if(departureFilter)
  {
    searchFilterIndex = FILTER_SID_PROCEDURES;
    runwayFilterIndex = FILTER_ALL_RUNWAYS;
  }
  else if(arrivalFilter)
  {
    searchFilterIndex = FILTER_ARRIVAL_PROCEDURES;
    runwayFilterIndex = FILTER_ALL_RUNWAYS;
  }
  else
    searchFilterIndex = FILTER_ALL_PROCEDURES;

  // Ignore request if noting has changed ========================================
  if(currentAirportNav->isValid() && navAirport.isValid() && currentAirportNav->id == navAirport.id &&
     searchFilterIndex == ui->comboBoxProcedureSearchFilter->currentIndex() &&
     runwayFilterIndex == ui->comboBoxProcedureRunwayFilter->currentIndex())
    return;

  // Save current state on stack =========================================================
  recentTreeState.insert(currentAirportNav->id, treeViewStateSave());

  // Remove all previews ===============
  treeWidget->clearSelection();
  emit procedureSelected(proc::MapProcedureRef());
  emit proceduresSelected(QVector<proc::MapProcedureRef>());
  emit procedureLegSelected(proc::MapProcedureRef());

  // Update fields with new data ===================
  *currentAirportSim = airportSim;
  *currentAirportNav = navAirport;
  ui->comboBoxProcedureSearchFilter->setCurrentIndex(searchFilterIndex);
  ui->comboBoxProcedureRunwayFilter->setCurrentIndex(runwayFilterIndex);
  ui->lineEditProcedureSearchIdentFilter->clear();
  ui->pushButtonProcedureShowAll->setChecked(false);

  updateFilterBoxes();

  fillProcedureTreeWidget();

  if(departureFilter || arrivalFilter)
  {
    // Expand procedure elements found in route ========
    treeViewStateFromRoute();

    // Save this state
    recentTreeState.insert(currentAirportNav->id, treeViewStateSave());
  }
  else
    treeViewStateRestore(recentTreeState.value(currentAirportNav->id));

  updateHeaderLabel();
  updateWidgets();
}

void ProcedureSearch::treeViewStateFromRoute()
{
#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO;
#endif

  const Route& route = NavApp::getRouteConst();
  const map::MapAirport& departureAirport = route.getDepartureAirportLeg().getAirport();
  const map::MapAirport& destAirport = route.getDestinationAirportLeg().getAirport();
  QVector<proc::MapProcedureRef> procRefs;

  // Get real SID procedure reference ===================================================
  if(departureAirport.id == currentAirportSim->id && route.hasAnySidProcedure() && !route.hasCustomDeparture())
    procRefs.append(route.getSidLegs().ref);

  // Get real arrival procedure references (probably STAR and approach) ===================================================
  if(destAirport.id == currentAirportSim->id)
  {
    // Select approach or STAR if no approach set
    if(route.hasAnyApproachProcedure() && !route.hasCustomApproach())
      procRefs.append(route.getApproachLegs().ref);

    if(route.hasAnyStarProcedure())
      procRefs.append(route.getStarLegs().ref);
  }

  for(const proc::MapProcedureRef& procRef :  procRefs)
  {
    if(procRef.hasProcedureId())
    {
      ProcIndexEntry entry;

      // Find matching procedure ================================
      for(auto it = itemIndex.begin(); it != itemIndex.end(); ++it)
      {
        const MapProcedureRef& ref = it->getRef();
        if(ref.hasProcedureOnlyIds() && procRef.procedureId == ref.procedureId)
          entry = *it;
      }

      if(entry.isValid())
      {
        // Expand procedure always ============================
        QTreeWidgetItem *procedureItem = entry.getItem();
        procedureItem->setExpanded(true);

        // Find matching transition ================================
        if(procRef.hasTransitionId())
        {
          for(int i = 0; i < procedureItem->childCount(); i++)
          {
            QTreeWidgetItem *transitionItem = procedureItem->child(i);
            const MapProcedureRef& ref = refFromItem(transitionItem);
            if(ref.hasTransitionId() && procRef.transitionId == ref.transitionId)
            {
              transitionItem->setExpanded(true);
              break;
            }
          }
        }
      }
    }
  }
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
  using atools::util::HtmlBuilder;
  QString procs;

  const QList<QTreeWidgetItem *> items = treeWidget->selectedItems();
  for(const QTreeWidgetItem *item : items)
    procs.append(procedureAndTransitionText(item, true /* header */));

  QString tooltip, statusTip;
  Ui::MainWindow *ui = NavApp::getMainUi();
  HtmlBuilder html;

  if(currentAirportSim->isValid())
  {
    html.a(map::airportTextShort(*currentAirportSim), "lnm://showairport", atools::util::html::BOLD | atools::util::html::LINK_NO_UL);
    if(currentAirportNav->isValid() && currentAirportNav->procedure())
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

  if(!runwayMismatches.isEmpty())
  {
    QString warningTooltip = tr("<p style='white-space:pre'>Procedure %1 %2 not found for simulator airport.<br/>"
                                "This means that runways from navigation data do not match runways of the simulator airport data.<br/>"
                                "Update your navigation data or update or install an add-on airport to fix this.</p>"
                                "<p style='white-space:pre'>You can still use procedures for this airport since %3<br/>"
                                "uses a best guess to cross reference simulator runways.</p>").
                             arg(runwayMismatches.size() == 1 ? tr("runway") : tr("runways")).
                             arg(atools::strJoin(runwayMismatches, tr(", "), tr(" and "))).
                             arg(QCoreApplication::applicationName());

    ui->labelProcedureSearchWarn->setText(HtmlBuilder::warningMessage(tr("Runway mismatches found. Click here for details.")));
    ui->labelProcedureSearchWarn->setToolTip(warningTooltip);
    ui->labelProcedureSearchWarn->show();
  }
  else
  {
    ui->labelProcedureSearchWarn->clear();
    ui->labelProcedureSearchWarn->hide();
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

  // Get parent reference if this is a leg item
  MapProcedureRef ref = refFromItem(item);
  if(ref.isLeg() && item != nullptr)
  {
    item = item->parent();
    ref = refFromItem(item);
  }

  QString text;
  // Generate HTML text for header or menu text
  const TreeRole role = header ? TREEWIDGET_HEADER_ROLE : TREEWIDGET_MENU_ROLE;

  if(item != nullptr && ref.hasProcedureId())
  {
    // Get first and last waypoint for the procedure or transition
    QStringList firstLastWp = item->data(COL_DESCRIPTION, TREEWIDGET_HEADER_FIRSTLAST_ROLE).toStringList();

    if(ref.hasProcedureOnlyIds())
    {
      // Procedure ==============================
      text.append(pattern.arg(item->data(COL_DESCRIPTION, role).toString()).arg(item->text(COL_IDENT)));

      if(header)
        text.append(atools::strJoin(tr(". From "), firstLastWp, tr(", "), tr(" to "), tr(".")));

      if(item->childCount() == 1 && ref.mapType.testFlag(proc::PROCEDURE_SID))
      {
        // Special SID case that has only transition legs and only one transition
        const QTreeWidgetItem *child = item->child(0);
        if(child != nullptr)
        {
          text.append(viaPattern);
          text.append(pattern.arg(child->data(COL_DESCRIPTION, role).toString()).arg(child->text(COL_IDENT)));
        }
      }

      // Text here: "Approach ILS 10L BLAZR (I10L), from TRAYL to 10L"
    }
    else if(ref.hasProcedureAndTransitionIds())
    {
      // Transition ==============================
      const QTreeWidgetItem *procedure = item->parent();
      if(procedure != nullptr)
      {
        // Clear unneded intermediate waypoint to prepare for merge
        if(firstLastWp.size() >= 2)
        {
          if(ref.mapType.testFlag(proc::PROCEDURE_SID))
            firstLastWp.removeFirst();
          else
            firstLastWp.removeLast();
        }

        // Merge first and last to have path from transition entry to approach last, for example
        QStringList firstLastWpProc = procedure->data(COL_DESCRIPTION, TREEWIDGET_HEADER_FIRSTLAST_ROLE).toStringList();
        if(!firstLastWpProc.isEmpty())
        {
          if(ref.mapType.testFlag(proc::PROCEDURE_SID))
            firstLastWp.prepend(firstLastWpProc.constFirst());
          else
            firstLastWp.append(firstLastWpProc.constLast());
        }

        text.append(pattern.arg(procedure->data(COL_DESCRIPTION, role).toString()).arg(procedure->text(COL_IDENT)));
        text.append(viaPattern);
      }

      text.append(patternSpace.arg(item->data(COL_DESCRIPTION, role).toString()).arg(item->text(COL_IDENT)));

      if(header)
        text.append(atools::strJoin(tr(". From "), firstLastWp, tr(", "), tr(" to "), tr(".")));

      // Text here: "Approach ILS 10L BLAZR (I10L) via transition BUXOM, from BUXOM to 10L"
    }
  } // if(item != nullptr)
  return text.simplified();
}

void ProcedureSearch::clearRunwayFilter()
{
  Ui::MainWindow *ui = NavApp::getMainUi();

  ui->comboBoxProcedureRunwayFilter->blockSignals(true);
  ui->comboBoxProcedureRunwayFilter->setCurrentIndex(FILTER_ALL_RUNWAYS);
  ui->comboBoxProcedureRunwayFilter->clear();
  ui->comboBoxProcedureRunwayFilter->addItem(tr("All Runways"));
  // "No runways" is maybe inserted here inbetween
  ui->comboBoxProcedureRunwayFilter->insertSeparator(FILTER_RUNWAYS_SEPARATOR);
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
  runwayMismatches.clear();

  clearRunwayFilter();
  clearTypeFilter();

  if(currentAirportNav->isValid())
  {
    // Get runway list from navdata
    const QStringList runwayNamesNav = airportQueryNav->getRunwayNames(currentAirportNav->id);

    // Get runway list from simulator
    const QStringList runwayNamesSim = airportQuerySim->getRunwayNames(currentAirportSim->id);

    // Add a tree of transitions and approaches
    const SqlRecordList *recProcList = infoQuery->getProcedureInformation(currentAirportNav->id);

    if(recProcList != nullptr) // Deduplicate runways
    {
      QStringList runways;
      // Update runway name filter combo box ============================================
      for(const SqlRecord& recProc : *recProcList)
      {
        proc::MapProcedureTypes type = proc::procedureType(NavApp::hasSidStarInDatabase(), recProc);

        if(type & proc::PROCEDURE_STAR_ALL || type & proc::PROCEDURE_SID_ALL)
        {
          // SID or STAR - have to resolve for ALL and parallel runways
          QStringList sidStarRunways;
          atools::fs::util::sidStarMultiRunways(runwayNamesNav, recProc.valueStr("arinc_name"), &sidStarRunways);

          if(sidStarRunways.isEmpty())
            runways.append(recProc.valueStr("runway_name"));
          else
            runways.append(sidStarRunways);
        }
        else
          // Approach with or without runway (circle-to-land)
          runways.append(recProc.valueStr("runway_name"));
      }

      // Sort list of runways and erase duplicates
      std::sort(runways.begin(), runways.end());
      runways.erase(std::unique(runways.begin(), runways.end()), runways.end());

      // Now collect all procedure runways which do not exist in simulator
      for(const QString& runway : runways)
      {
        if(!runway.isEmpty() && !atools::fs::util::runwayContains(runwayNamesSim, runway, false /* fuzzy */))
          runwayMismatches.append(runway);
      }

      for(const QString& rw : qAsConst(runways))
      {
        if(rw.isEmpty())
          // Insert item before separator
          ui->comboBoxProcedureRunwayFilter->insertItem(FILTER_NO_RUNWAYS, tr("No Runway"), rw);
        else
          // Append item after separator
          ui->comboBoxProcedureRunwayFilter->addItem(tr("Runway %1").arg(rw), rw);
      }

      // Update type filter combo box ============================================
      QStringList types;
      for(const SqlRecord& recApp : *recProcList)
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
  nextIndexId = 1;
  itemExpandedIndex.clear();
  QSet<QTreeWidgetItem *> itemsToExpand;

  if(currentAirportNav->isValid())
  {
    // Add a tree of transitions and approaches
    const SqlRecordList *procedureRecords = infoQuery->getProcedureInformation(currentAirportNav->id);

    if(procedureRecords != nullptr)
    {
      QStringList runwayNames = airportQueryNav->getRunwayNames(currentAirportNav->id);
      Ui::MainWindow *ui = NavApp::getMainUi();
      QTreeWidgetItem *root = treeWidget->invisibleRootItem();
      QVector<SqlRecord> filteredProcedures;

      // Collect all procedures from the database to filteredProcedures ===============================================
      for(SqlRecord procedureRec : *procedureRecords)
      {
#ifdef DEBUG_INFORMATION_PROCSEARCH
        qDebug() << Q_FUNC_INFO << "PROCEDURE ==========================================================";
        qDebug() << Q_FUNC_INFO << "procedureRec " << procedureRec;
#endif

        // Filter out by type from the combo box ================================================
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

        // Resolve parallel runway assignments for runway filter
        QStringList sidStarArincDispNames, sidStarRunways;
        QString allRunwayDispText(tr("All"));
        if(type & proc::PROCEDURE_SID_STAR_ALL)
          atools::fs::util::sidStarMultiRunways(runwayNames, procedureRec.valueStr("arinc_name", QString()),
                                                &sidStarRunways, allRunwayDispText, &sidStarArincDispNames);

        QString runwayName;
        if(!procedureRec.valueStr("runway_name").isEmpty())
          runwayName = atools::fs::util::runwayBestFit(procedureRec.valueStr("runway_name"), runwayNames);

        QString runwayNamefilter = ui->comboBoxProcedureRunwayFilter->currentData(COMBOBOX_RUNWAY_FILTER_ROLE).toString();
        int rwNameIndex = ui->comboBoxProcedureRunwayFilter->currentIndex();

        // Filter out by runway from the combo box ================================================
        if(rwNameIndex == FILTER_ALL_RUNWAYS)
          // All selected - always first position
          filterOk &= true;
        else if(runwayNamefilter.isEmpty())
          // No rwy selected - second position
          filterOk &= runwayName.isEmpty() && sidStarArincDispNames.isEmpty();
        else
          filterOk &= runwayName == runwayNamefilter || // name equal
                      (!sidStarArincDispNames.isEmpty() && sidStarArincDispNames.contains(runwayNamefilter)) ||
                      sidStarArincDispNames.contains(allRunwayDispText);

        if(filterOk)
        {
          // Add an extra field with the best airport runway name
          procedureRec.appendFieldAndValue("airport_runway_name", runwayName);

          // Keep the runway list for the context menu
          procedureRec.appendFieldAndValue("sid_star_runways", sidStarRunways);

          procedureRec.appendField("sid_star_arinc_name", QVariant::String);
          if(type & proc::PROCEDURE_SID_STAR_ALL && runwayName.isEmpty())
            procedureRec.setValue("sid_star_arinc_name", sidStarArincDispNames.join(", "));

          filteredProcedures.append(procedureRec);
        }
      }

      // Sort all procedures by type, name and runways ===================================================
      std::sort(filteredProcedures.begin(), filteredProcedures.end(), procedureSortFunc);

      QString typefilter = ui->comboBoxProcedureSearchFilter->currentData(COMBOBOX_PROCEDURE_FILTER_ROLE).toString();
      int typeindex = ui->comboBoxProcedureSearchFilter->currentIndex();

      // Now load sorted and filtered procedures ==============================================================
      for(SqlRecord& procedureRecFiltered : filteredProcedures)
      {
        // Check type filter ==========================================
        proc::MapProcedureTypes type = buildTypeFromProcedureRec(procedureRecFiltered);
        if(!(type & proc::PROCEDURE_SID_STAR_ALL) && typeindex > FILTER_APPROACH_ALL && procedureRecFiltered.valueStr("type") != typefilter)
          continue;

        // Get first and last waypoint ==========================================
        int procedureId = procedureRecFiltered.valueInt("approach_id");
        QString firstFix, lastFix;
        procedureQuery->getProcedureFirstLastWp(firstFix, lastFix, procedureId);

        // Add first and last waypoint to record ======================
        bool approach = type.testFlag(proc::PROCEDURE_APPROACH);
        bool sid = type.testFlag(proc::PROCEDURE_SID);
        int runwayEndId = procedureRecFiltered.valueInt("runway_end_id");
        if(approach)
        {
          // Approach ends usually with runway
          QString rw = airportQueryNav->getRunwayEndById(runwayEndId).name;
          procedureRecFiltered.appendFieldAndValue("fix_first", firstFix);
          procedureRecFiltered.appendFieldAndValue("fix_last", QVariant(rw.isEmpty() ? currentAirportNav->displayIdent() : rw));
          lastFix = rw;
        }
        else if(sid)
        {
          // SID starts with runway
          // Try to get multi runways list
          QString rw = procedureRecFiltered.value("sid_star_runways").toStringList().join(tr(", "));

          // Otherwise single or no runway
          if(rw.isEmpty())
            rw = procedureRecFiltered.value("arinc_name").toString();

          firstFix = rw.startsWith("RW") ? rw.mid(2) : rw;
          procedureRecFiltered.appendFieldAndValue("fix_first", firstFix);
          procedureRecFiltered.appendFieldAndValue("fix_last", lastFix);
        }
        else
        {
          // STAR
          procedureRecFiltered.appendFieldAndValue("fix_first", firstFix);
          procedureRecFiltered.appendFieldAndValue("fix_last", lastFix);
        }

        // Check text filter for procedure ===========================================================
        QString filter = ui->lineEditProcedureSearchIdentFilter->text();
        QString ident = procedureRecFiltered.valueStr("fix_ident");
        QString arinc = procedureRecFiltered.valueStr("arinc_name");
        bool procFilterMatch = filter.isEmpty() || ident.startsWith(filter, Qt::CaseInsensitive) ||
                               firstFix.startsWith(filter, Qt::CaseInsensitive) || lastFix.startsWith(filter, Qt::CaseInsensitive) ||
                               (approach && arinc.startsWith(filter, Qt::CaseInsensitive));

#ifdef DEBUG_INFORMATION_PROCSEARCH
        qDebug() << Q_FUNC_INFO << "PROCEDURE ==========================================================";
        qDebug() << Q_FUNC_INFO << "procedureRecSorted " << procedureRecFiltered << "procFilterMatch" << procFilterMatch;
#endif

        // Load transitions for this procedure ==============================================================
        const SqlRecordList *transitionRecords = infoQuery->getTransitionInformation(procedureId);
        QVector<SqlRecord> filteredTransitions;
        bool foundProceduresToExpand = false;
        int numTransitions = 0;
        if(transitionRecords != nullptr)
        {
          // Keep number and do not consider filtered out transitions ======================================
          numTransitions = transitionRecords->size();

          // Transitions for this procedure
          for(SqlRecord transitionRec : *transitionRecords)
          {
            QString firstTransFix, lastTransFix;
            procedureQuery->getTransitionFirstLastWp(firstTransFix, lastTransFix, transitionRec.valueInt("transition_id"));

            // Check text filter for transitions ==========================================
            if(filter.isEmpty() || procFilterMatch || transitionRec.valueStr("fix_ident").startsWith(filter, Qt::CaseInsensitive) ||
               firstTransFix.startsWith(filter, Qt::CaseInsensitive) || firstTransFix.startsWith(filter, Qt::CaseInsensitive))
            {
              transitionRec.appendFieldAndValue("fix_first", firstTransFix);
              transitionRec.appendFieldAndValue("fix_last", lastTransFix);
              filteredTransitions.append(transitionRec);

              // Expand procedure if procedure was not selected but transition
              foundProceduresToExpand |= !filter.isEmpty() && !procFilterMatch;

#ifdef DEBUG_INFORMATION_PROCSEARCH
              qDebug() << Q_FUNC_INFO << "TRANSITION ==========================================================";
              qDebug() << Q_FUNC_INFO << "transitionRec " << transitionRec << "foundMatchingTransitions" << foundProceduresToExpand;
#endif
            }
          }
        }

        // Sort transitions by name
        std::sort(filteredTransitions.begin(), filteredTransitions.end(), transitionSortFunc);

        // Add procedures and transitons to tree ============================================================
        // Procedure has to be added if its filter matched or if one of its transitions matched
        if(foundProceduresToExpand || procFilterMatch)
        {
          QString prefix;
          if(approach)
            prefix = tr("Approach "); // SID and STAR prefix is already a part of the text

          QString procTypeText, // "RNAV 32-Y" or "ILS 16 (one transition)"
                  headerText, // "RNAV 32-Y" or "ILS 16" in HTML
                  menuText; // "RNAV 32-Y" or "ILS 16" in plain text
          QStringList attText; // "GPS Overlay", etc.
          procedureDisplayText(procTypeText, headerText, menuText, attText, procedureRecFiltered, type, numTransitions);

          // Fix ident and ARINC name in () for approaches
          if(approach && !arinc.isEmpty())
            ident.append(tr(" (%1)").arg(arinc));

          // also creates transition_id in type()
          QTreeWidgetItem *procItem = buildProcedureItem(root, ident, procedureRecFiltered, prefix % procTypeText,
                                                         prefix % headerText, prefix % menuText, attText);

          itemIndex.insert(procItem->type(), ProcIndexEntry(procItem, currentAirportNav->id, runwayEndId, procedureId, -1, -1, type));

          // Transitions for this approach ===============================
          for(const SqlRecord& transitionRecFiltered : filteredTransitions)
          {
            // Also add runway from parent approach to transition - also creates transition_id in type()
            QTreeWidgetItem *transItem = buildTransitionItem(procItem, transitionRecFiltered,
                                                             type & proc::PROCEDURE_DEPARTURE || type & proc::PROCEDURE_STAR_ALL);
            itemIndex.insert(transItem->type(), ProcIndexEntry(transItem, currentAirportNav->id, runwayEndId, procedureId,
                                                               transitionRecFiltered.valueInt("transition_id"), -1, type));
          }

          if(foundProceduresToExpand)
            itemsToExpand.insert(procItem);
        }
      } // for(SqlRecord procedureRecSorted : sortedProcedures)
    } // if(procedureRecords != nullptr)
  } // if(currentAirportNav->isValid())

  updateProcedureWind();
  treeWidget->blockSignals(false);

  // Now expand any items which were collected from route procedures
  // Use signals for updating and loading expanded items
  for(QTreeWidgetItem *item : itemsToExpand)
    item->setExpanded(true);
}

void ProcedureSearch::saveState()
{
  Ui::MainWindow *ui = NavApp::getMainUi();
  WidgetState(lnm::APPROACHTREE_WIDGET).save(QList<const QObject *>({ui->comboBoxProcedureSearchFilter, ui->comboBoxProcedureRunwayFilter,
                                                                     ui->actionSearchProcedureFollowSelection,
                                                                     ui->lineEditProcedureSearchIdentFilter}));

  atools::settings::Settings& settings = atools::settings::Settings::instance();

  // Use current state and update the map too
  QSet<int> savedState = treeViewStateSave();
  recentTreeState.insert(currentAirportNav->id, savedState);
  settings.setValueVar(lnm::APPROACHTREE_STATE, atools::numSetToStrList(savedState));

  // Save column order and width
  WidgetState(lnm::APPROACHTREE_WIDGET).save(treeWidget);

  if(currentAirportSim->isValid() && currentAirportNav->isValid())
  {
    settings.setValue(lnm::APPROACHTREE_AIRPORT_NAV, currentAirportNav->id);
    settings.setValue(lnm::APPROACHTREE_AIRPORT_SIM, currentAirportSim->id);
  }
  else
  {
    settings.setValue(lnm::APPROACHTREE_AIRPORT_NAV, -1);
    settings.setValue(lnm::APPROACHTREE_AIRPORT_SIM, -1);
  }
}

void ProcedureSearch::restoreState()
{
  atools::settings::Settings& settings = atools::settings::Settings::instance();
  if(OptionData::instance().getFlags() & opts::STARTUP_LOAD_SEARCH && !atools::gui::Application::isSafeMode())
  {
    if(NavApp::hasDataInDatabase())
    {
      airportQueryNav->getAirportById(*currentAirportNav, settings.valueInt(lnm::APPROACHTREE_AIRPORT_NAV, -1));
      airportQuerySim->getAirportById(*currentAirportSim, settings.valueInt(lnm::APPROACHTREE_AIRPORT_SIM, -1));
    }

    if(!currentAirportSim->isValid() || !currentAirportNav->isValid())
      *currentAirportNav = *currentAirportSim = map::MapAirport();
  }

  updateFilterBoxes();

  QSet<int> state;
  if(OptionData::instance().getFlags() & opts::STARTUP_LOAD_SEARCH && !atools::gui::Application::isSafeMode())
  {
    Ui::MainWindow *ui = NavApp::getMainUi();
    WidgetState(lnm::APPROACHTREE_WIDGET).restore({ui->comboBoxProcedureSearchFilter, ui->comboBoxProcedureRunwayFilter,
                                                   ui->actionSearchProcedureFollowSelection, ui->lineEditProcedureSearchIdentFilter});

    fillProcedureTreeWidget();
    if(currentAirportNav->isValid() && currentAirportNav->procedure())
    {
      state = atools::strListToNumSet<int>(settings.valueStrList(lnm::APPROACHTREE_STATE));
      recentTreeState.insert(currentAirportNav->id, state);
    }
  }

  updateTreeHeader();
  WidgetState(lnm::APPROACHTREE_WIDGET).restore(treeWidget);

  if(OptionData::instance().getFlags() & opts::STARTUP_LOAD_SEARCH && !atools::gui::Application::isSafeMode())
  {
    // Restoring state will emit above signal
    if(currentAirportNav->isValid() && currentAirportNav->procedure())
    {
      treeViewStateRestore(state);
      *savedAirportSim = *currentAirportSim;
    }
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

  header->setText(COL_FROMTO, tr("First and last\nWaypoint"));
  header->setToolTip(COL_FROMTO, tr("First and last waypoint of the procedure, transition or both."));

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
      const QVector<int> transitionIds = procedureQuery->getTransitionIdsForProcedure(ref.procedureId);
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
      MapProcedureRef ref = refFromItem(item);

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
        updateProcedureItemCourseDist(item, ref.transitionId);
    }
  }

  if(NavApp::getSearchController()->getCurrentSearchTabId() == tabIndex && ui->pushButtonProcedureShowAll->isChecked())
  {
    QVector<proc::MapProcedureRef> refs;
    for(auto it = itemIndex.begin(); it != itemIndex.end(); ++it)
      refs.append(it->getRef());
    emit proceduresSelected(refs);
  }
  else
    emit proceduresSelected(QVector<proc::MapProcedureRef>());

  updateHeaderLabel();
  updateWidgets();
}

void ProcedureSearch::updateProcedureItemCourseDist(QTreeWidgetItem *procedureItem, int transitionId)
{
  if(procedureItem != nullptr)
  {
    for(int i = 0; i < procedureItem->childCount(); i++)
    {
      QTreeWidgetItem *child = procedureItem->child(i);
      const MapProcedureRef& childRef = refFromItem(child);
      if(childRef.isLeg())
      {
        const proc::MapProcedureLegs *legs = procedureQuery->getTransitionLegs(*currentAirportNav, transitionId);
        if(legs != nullptr)
        {
          const proc::MapProcedureLeg *procLeg = legs->transitionLegById(childRef.legId);

          if(procLeg != nullptr)
          {
            child->setText(COL_COURSE, proc::procedureLegCourse(*procLeg));
            child->setText(COL_DISTANCE, proc::procedureLegDistance(*procLeg));
          }
          else
            qWarning() << Q_FUNC_INFO << "Transition legs not found" << childRef.legId;
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

void ProcedureSearch::itemExpanded(QTreeWidgetItem *item)
{
  if(item != nullptr)
  {
    int type = item->type();

    if(itemExpandedIndex.contains(type))
      return;

    // Get a copy since vector is rebuilt underneath
    const MapProcedureRef ref = refFromItem(item);

    if(ref.legId == -1)
    {
      if(ref.procedureId != -1 && ref.transitionId == -1)
      {
        const MapProcedureLegs *legs = procedureQuery->getProcedureLegs(*currentAirportNav, ref.procedureId);
        if(legs != nullptr)
        {
          QList<QTreeWidgetItem *> items = buildProcedureLegItems(legs, -1);
          itemExpandedIndex.insert(type);

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
          itemExpandedIndex.insert(type);
        }
        else
          qWarning() << Q_FUNC_INFO << "no legs found for" << currentAirportNav->id << ref.transitionId;
      }
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
      QTreeWidgetItem *item = buildLegItem(leg);
      itemIndex.insert(item->type(),
                       ProcIndexEntry(item, legs->ref.airportId, legs->ref.runwayEndId, legs->ref.procedureId, transitionId, leg.legId,
                                      legs->mapType));
      items.append(item);
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
      QTreeWidgetItem *item = buildLegItem(leg);
      itemIndex.insert(item->type(),
                       ProcIndexEntry(item, legs->ref.airportId, legs->ref.runwayEndId, legs->ref.procedureId, legs->ref.transitionId,
                                      leg.legId, legs->mapType));
      items.append(item);
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
  {
    QVector<proc::MapProcedureRef> refs;
    for(auto it = itemIndex.begin(); it != itemIndex.end(); ++it)
      refs.append(it->getRef());

    emit proceduresSelected(refs);
  }

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
    if(currentAirportSim->isValid())
    {
      NavApp::getSearchController()->setCurrentSearchTabId(si::SEARCH_AIRPORT);
      emit showInSearch(map::AIRPORT, SqlRecord().appendFieldAndValue("ident", currentAirportSim->ident), true /* select */);
    }
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

void ProcedureSearch::showAirportOnMapSelected()
{
  // ui->actionSearchProcedureShowOnMap
  if(currentAirportSim->isValid())
    emit showRect(currentAirportSim->bounding, false /* doubleClick */);
}

const MapProcedureRef& ProcedureSearch::fetchProcRef(const QTreeWidgetItem *item) const
{
  if(item != nullptr && !itemIndex.isEmpty())
  {
    const MapProcedureRef& procData = refFromItem(item);
    if(procData.isLeg() && item->parent() != nullptr)
      // Get parent approach data if approach is a leg
      return refFromItem(item->parent());
    else
      return procData;
  }
  return EMPTY_PROC_REF;
}

const proc::MapProcedureLegs *ProcedureSearch::fetchProcData(MapProcedureRef& ref, const QTreeWidgetItem *item) const
{
  if(!currentAirportNav->isValid())
    return nullptr;

  ref = fetchProcRef(item);

  if(!ref.isEmpty())
    // Get transition id too if SID with only transition leg is selected
    fetchSingleTransitionId(ref);

  const proc::MapProcedureLegs *procedureLegs = nullptr;
  // Get the aproach legs for the initial fix
  if(ref.hasProcedureOnlyIds())
    procedureLegs = procedureQuery->getProcedureLegs(*currentAirportNav, ref.procedureId);
  else if(ref.hasProcedureAndTransitionIds())
    procedureLegs = procedureQuery->getTransitionLegs(*currentAirportNav, ref.transitionId);
  return procedureLegs;
}

void ProcedureSearch::showProcedureSelected()
{
  if(!treeWidget->selectedItems().isEmpty())
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
      atools::gui::Dialog::warning(mainWindow, tr("Procedure has errors and cannot be added to the flight plan.\n"
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

  if(item == nullptr || !currentAirportNav->isValid())
    return;

  MapProcedureRef ref = refFromItem(item);
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
      emit showRect(NavApp::getShownMapTypes().testFlag(map::MISSED_APPROACH) ? legs->boundingWithMissed : legs->bounding, doubleClick);
  }
  else if(ref.procedureId != -1 && !doubleClick)
  {
    const proc::MapProcedureLegs *legs = procedureQuery->getProcedureLegs(*currentAirportNav, ref.procedureId);
    if(legs != nullptr)
      emit showRect(NavApp::getShownMapTypes().testFlag(map::MISSED_APPROACH) ? legs->boundingWithMissed : legs->bounding, doubleClick);
  }
}

void ProcedureSearch::procedureDisplayText(QString& procTypeText, QString& headerText, QString& menuText, QStringList& attText,
                                           const SqlRecord& recProcedure, proc::MapProcedureTypes maptype, int numTransitions)
{
  QString suffix = recProcedure.valueStr("suffix");
  QString type = recProcedure.valueStr("type");

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

    if(recProcedure.valueBool("has_gps_overlay"))
      attText.append(tr("GPS Overlay"));
  }

  if(!recProcedure.valueStr("airport_runway_name", QString()).isEmpty())
  {
    procTypeText.append(tr(" %1").arg(recProcedure.valueStr("airport_runway_name")));
    headerText.append(tr(" <b>%1</b>").arg(recProcedure.valueStr("airport_runway_name")));
  }

  if(!recProcedure.valueStr("sid_star_arinc_name", QString()).isEmpty())
  {
    procTypeText.append(tr(" %1").arg(recProcedure.valueStr("sid_star_arinc_name", QString())));
    headerText.append(tr(" <b>%1</b>").arg(recProcedure.valueStr("sid_star_arinc_name", QString())));
  }

  // Menu text is no HTML and same as row text
  menuText = procTypeText;

  // Appears for almost all approaches
  // if(recApp.valueBool("has_vertical_angle", false))
  // attText.append(tr("VNAV"));

  QString cat = proc::aircraftCategoryText(recProcedure.valueStr("aircraft_category", QString()));
  if(!cat.isEmpty())
    attText.append(cat);

  if(recProcedure.valueBool("has_rnp", false))
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
      float windSpeedKts, windDirectionDeg;
      NavApp::getAirportMetarWind(windDirectionDeg, windSpeedKts, *currentAirportSim, false /* stationOnly */);

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

QTreeWidgetItem *ProcedureSearch::buildProcedureItem(QTreeWidgetItem *rootItem, const QString& ident,
                                                     const atools::sql::SqlRecord& recProcedure,
                                                     const QString& procTypeText, const QString& headerText, const QString& menuText,
                                                     const QStringList& attStr)
{
  QStringList firstLastWp = firstLastWaypoint(recProcedure);
  QTreeWidgetItem *item = new QTreeWidgetItem({procTypeText, // COL_DESCRIPTION
                                               ident, // COL_IDENT
                                               QString(), // COL_RESTR
                                               atools::strJoin(tr(" "), firstLastWp, tr(", "), tr(" to ")), // COL_FROMTO
                                               QString(), // COL_COURSE
                                               QString(), // COL_DISTANCE
                                               QString(), // COL_WIND
                                               attStr.join(tr(", ")) // COL_REMARKS
                                              }, nextIndexId++);

  item->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
  item->setTextAlignment(COL_RESTR, Qt::AlignRight);
  item->setTextAlignment(COL_COURSE, Qt::AlignRight);
  item->setTextAlignment(COL_DISTANCE, Qt::AlignRight);
  item->setData(COL_DESCRIPTION, TREEWIDGET_HEADER_ROLE, headerText);
  item->setData(COL_DESCRIPTION, TREEWIDGET_HEADER_FIRSTLAST_ROLE, firstLastWp);
  item->setData(COL_DESCRIPTION, TREEWIDGET_MENU_ROLE, menuText);

  // First columns bold
  for(int i = COL_DESCRIPTION; i <= COL_IDENT; i++)
    item->setFont(i, procedureBoldFont);

  // Rest in normal font
  for(int i = COL_RESTR; i <= COL_REMARKS; i++)
    item->setFont(i, procedureNormalFont);

  rootItem->addChild(item);

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

  QStringList firstLastWp = firstLastWaypoint(recTrans);
  QTreeWidgetItem *item = new QTreeWidgetItem({tr("Transition"), // COL_DESCRIPTION
                                               recTrans.valueStr("fix_ident"), // COL_IDENT
                                               QString(), // COL_RESTR
                                               atools::strJoin(tr(" "), firstLastWp, tr(", "), tr(" to ")), // COL_FROMTO
                                               QString(), // COL_COURSE
                                               QString(), // COL_DISTANCE
                                               QString(), // COL_WIND
                                               attStr.join(tr(", ")) // COL_REMARKS
                                              }, nextIndexId++);

  item->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
  item->setTextAlignment(COL_RESTR, Qt::AlignRight);
  item->setTextAlignment(COL_COURSE, Qt::AlignRight);
  item->setTextAlignment(COL_DISTANCE, Qt::AlignRight);
  item->setData(COL_DESCRIPTION, TREEWIDGET_HEADER_ROLE, tr("transition"));
  item->setData(COL_DESCRIPTION, TREEWIDGET_HEADER_FIRSTLAST_ROLE, firstLastWp);
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
  texts.append(proc::procedureLegTypeStr(leg.type));
  texts.append(proc::procedureLegFixStr(leg));
  texts.append(proc::restrictionText(leg).join(tr(", ")));
  texts.append(QString()); // From to not shown
  texts.append(proc::procedureLegCourse(leg));
  texts.append(proc::procedureLegDistance(leg));
  texts.append(QString()); // Wind filled in updateProcedureWind()

  QStringList remarkStr = proc::procedureLegRemark(leg);

  QStringList related = procedureLegRecommended(leg);
  if(!related.isEmpty())
    remarkStr.append(QObject::tr("Related: %1").arg(related.join(QObject::tr(", "))));

#ifdef DEBUG_INFORMATION
  remarkStr.append(QString(" | leg_id = %1 approach_id = %2 transition_id = %3").
                   arg(leg.legId).arg(leg.procedureId).arg(leg.transitionId));
#endif
  texts << remarkStr.join(tr(", "));

  QTreeWidgetItem *item = new QTreeWidgetItem(texts, nextIndexId++);
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
        item->setForeground(i, NavApp::isGuiStyleDark() ?
                            mapcolors::routeProcedureMissedTableColorDark :
                            mapcolors::routeProcedureMissedTableColor);
      else
        item->setForeground(i, NavApp::isGuiStyleDark() ?
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

QSet<int> ProcedureSearch::treeViewStateSave() const
{
#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO;
#endif

  QSet<int> state;

  if(!itemIndex.isEmpty() && currentAirportNav->isValid() && currentAirportSim->isValid())
  {
    QList<const QTreeWidgetItem *> itemStack;
    const QTreeWidgetItem *root = treeWidget->invisibleRootItem();

    for(int i = 0; i < root->childCount(); ++i)
      itemStack.append(root->child(i));

    while(!itemStack.isEmpty())
    {
      const QTreeWidgetItem *item = itemStack.takeFirst();

      if(item->type() < itemIndex.size() && refFromItem(item).legId != -1)
        // Do not save legs
        continue;

      if(item->isExpanded())
        state.insert(item->type());

      for(int i = 0; i < item->childCount(); ++i)
        itemStack.append(item->child(i));
    }
  }
  return state;
}

void ProcedureSearch::treeViewStateRestore(const QSet<int>& state)
{
#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO;
#endif

  if(state.isEmpty())
    return;

  QList<QTreeWidgetItem *> itemStack;
  const QTreeWidgetItem *root = treeWidget->invisibleRootItem();

  // Find selected and expanded items first without tree modification to keep order
  for(int i = 0; i < root->childCount(); ++i)
    itemStack.append(root->child(i));
  QVector<QTreeWidgetItem *> itemsToExpand;
  while(!itemStack.isEmpty())
  {
    QTreeWidgetItem *item = itemStack.takeFirst();
    if(item != nullptr)
    {
      if(state.contains(item->type()))
        itemsToExpand.append(item);

      for(int i = 0; i < item->childCount(); ++i)
        itemStack.append(item->child(i));
    }
  }

  // Expand and possibly reload
  for(QTreeWidgetItem *item : itemsToExpand)
    item->setExpanded(true);
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
  // Avoid spurious events that appear on shutdown and cause crashes
  if(!atools::gui::Application::isShuttingDown())
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
}

void ProcedureSearch::tabDeactivated()
{
  emit procedureSelected(proc::MapProcedureRef());
  emit proceduresSelected(QVector<proc::MapProcedureRef>());
  emit procedureLegSelected(proc::MapProcedureRef());
}

void ProcedureSearch::showSelectedEntry()
{
}

void ProcedureSearch::activateView()
{
  treeWidget->setFocus();
}

void ProcedureSearch::showFirstEntry()
{
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

const MapProcedureRef& ProcedureSearch::refFromItem(const QTreeWidgetItem *item) const
{
  if(item != nullptr)
  {
    // Get iterator which is the only way to get a const reference
    auto it = itemIndex.constFind(item->type());

    if(it != itemIndex.constEnd())
      return it->getRef();
  }

  return EMPTY_PROC_REF;
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

bool ProcedureSearch::transitionSortFunc(const atools::sql::SqlRecord& rec1, const atools::sql::SqlRecord& rec2)
{
  return rec1.valueStr("fix_ident") < rec2.valueStr("fix_ident");
}

QStringList ProcedureSearch::firstLastWaypoint(const atools::sql::SqlRecord& record) const
{
  QStringList fromToString;
  if(record.contains("fix_first"))
    fromToString.append(record.valueStr("fix_first"));

  if(record.contains("fix_last"))
    fromToString.append(record.valueStr("fix_last"));
  fromToString.removeAll(QString());

  return fromToString;
}
