/*****************************************************************************
* Copyright 2015-2020 Alexander Barthel alex@littlenavmap.org
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

#include "search/searchbasetable.h"
#include "gui/itemviewzoomhandler.h"
#include "navapp.h"
#include "common/constants.h"
#include "search/sqlcontroller.h"
#include "logbook/logdatacontroller.h"
#include "search/column.h"
#include "search/searchcontroller.h"
#include "ui_mainwindow.h"
#include "common/mapcolors.h"
#include "search/columnlist.h"
#include "mapgui/mapwidget.h"
#include "route/route.h"
#include "atools.h"
#include "gui/actiontextsaver.h"
#include "gui/actionstatesaver.h"
#include "export/csvexporter.h"
#include "query/mapquery.h"
#include "query/airportquery.h"
#include "options/optiondata.h"
#include "common/unit.h"
#include "sql/sqlrecord.h"
#include "gui/dialog.h"
#include "geo/calculations.h"
#include "mapgui/mapmarkhandler.h"

#include <QTimer>
#include <QClipboard>
#include <QKeyEvent>
#include <QFileInfo>

/* When using distance search delay the update the table after 500 milliseconds */
const int DISTANCE_EDIT_UPDATE_TIMEOUT_MS = 500;

class ViewEventFilter :
  public QObject
{

public:
  ViewEventFilter(SearchBaseTable *parent)
    : QObject(parent), searchBase(parent)
  {
  }

private:
  virtual bool eventFilter(QObject *object, QEvent *event) override;

  SearchBaseTable *searchBase;
};

bool ViewEventFilter::eventFilter(QObject *object, QEvent *event)
{
  if(event->type() == QEvent::KeyPress)
  {
    QKeyEvent *pKeyEvent = dynamic_cast<QKeyEvent *>(event);
    if(pKeyEvent != nullptr)
    {
      switch(pKeyEvent->key())
      {
        case Qt::Key_Return:
          searchBase->showSelectedEntry();
          return true;
      }
    }
  }

  return QObject::eventFilter(object, event);
}

class SearchWidgetEventFilter :
  public QObject
{

public:
  SearchWidgetEventFilter(SearchBaseTable *parent)
    : QObject(parent), searchBase(parent)
  {
  }

private:
  virtual bool eventFilter(QObject *object, QEvent *event) override;

  SearchBaseTable *searchBase;
};

bool SearchWidgetEventFilter::eventFilter(QObject *object, QEvent *event)
{
  if(event->type() == QEvent::KeyPress)
  {
    QKeyEvent *pKeyEvent = dynamic_cast<QKeyEvent *>(event);
    if(pKeyEvent != nullptr)
    {
      switch(pKeyEvent->key())
      {
        case Qt::Key_Down:
          searchBase->activateView();
          return true;

        case Qt::Key_Return:
          searchBase->showFirstEntry();
          return true;
      }
    }
  }

  return QObject::eventFilter(object, event);
}

SearchBaseTable::SearchBaseTable(QMainWindow *parent, QTableView *tableView, ColumnList *columnList,
                                 si::TabSearchId tabWidgetIndex)
  : AbstractSearch(parent, tabWidgetIndex), columns(columnList), view(tableView)
{
  mapQuery = NavApp::getMapQuery();
  airportQuery = NavApp::getAirportQuerySim();

  zoomHandler = new atools::gui::ItemViewZoomHandler(view);
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
  connect(NavApp::navAppInstance(), &atools::gui::Application::fontChanged, this, &SearchBaseTable::fontChanged);
#endif

  Ui::MainWindow *ui = NavApp::getMainUi();

  // Avoid stealing of Ctrl-C from other default menus
  ui->actionSearchTableCopy->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionSearchResetSearch->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionSearchShowAll->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionSearchShowInformation->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionSearchShowApproaches->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionSearchShowApproachesCustom->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionSearchShowOnMap->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionSearchTableSelectNothing->setShortcutContext(Qt::WidgetWithChildrenShortcut);

  // Need extra action connected to catch the default Ctrl-C in the table view
  connect(ui->actionSearchTableCopy, &QAction::triggered, this, &SearchBaseTable::tableCopyClipboard);

  // Actions that cover the whole dock window
  ui->dockWidgetSearch->addActions({ui->actionSearchResetSearch, ui->actionSearchShowAll});

  tableView->addActions({ui->actionSearchTableCopy, ui->actionSearchTableSelectNothing});

  // Add actions to this tab
  ui->tabWidgetSearch->widget(tabWidgetIndex)->addActions({ui->actionSearchShowInformation,
                                                           ui->actionSearchShowApproaches,
                                                           ui->actionSearchShowApproachesCustom,
                                                           ui->actionSearchShowOnMap});

  // Update single shot timer
  updateTimer = new QTimer(this);
  updateTimer->setSingleShot(true);
  connect(updateTimer, &QTimer::timeout, this, &SearchBaseTable::editTimeout);
  connect(ui->actionSearchShowInformation, &QAction::triggered, this, &SearchBaseTable::showInformationTriggered);
  connect(ui->actionSearchShowApproaches, &QAction::triggered, this, &SearchBaseTable::showApproachesTriggered);
  connect(ui->actionSearchShowApproachesCustom, &QAction::triggered,
          this, &SearchBaseTable::showApproachesCustomTriggered);
  connect(ui->actionSearchShowOnMap, &QAction::triggered, this, &SearchBaseTable::showOnMapTriggered);
  connect(ui->actionSearchTableSelectNothing, &QAction::triggered, this, &SearchBaseTable::nothingSelectedTriggered);

  // Load text size from options
  zoomHandler->zoomPercent(OptionData::instance().getGuiSearchTableTextSize());

  viewEventFilter = new ViewEventFilter(this);
  widgetEventFilter = new SearchWidgetEventFilter(this);
  view->installEventFilter(viewEventFilter);
}

SearchBaseTable::~SearchBaseTable()
{
  view->removeEventFilter(viewEventFilter);
  delete controller;
  delete csvExporter;
  delete updateTimer;
  delete zoomHandler;
  delete columns;
  delete viewEventFilter;
  delete widgetEventFilter;
}

void SearchBaseTable::fontChanged()
{
  qDebug() << Q_FUNC_INFO;

  zoomHandler->fontChanged();
  optionsChanged();
}

/* Copy the selected rows of the table view as CSV into clipboard */
void SearchBaseTable::tableCopyClipboard()
{
  if(view->isVisible())
  {
    QString csv;
    SqlController *c = controller;

    int exported = 0;
    if(controller->hasColumn("lonx") && controller->hasColumn("laty"))
    {
      // Full CSV export including coordinates and full rows
      exported = CsvExporter::selectionAsCsv(view, true /* header */, true /* rows */, csv,
                                             {tr("Longitude"), tr("Latitude")},
                                             [c](int index) -> QStringList
      {
        return {QLocale().toString(c->getRawData(index, "lonx").toFloat(), 'f', 8),
                QLocale().toString(c->getRawData(index, "laty").toFloat(), 'f', 8)};
      });
    }
    else
      // Copy only selected cells
      exported = CsvExporter::selectionAsCsv(view, false /* header */, false /* rows */, csv);

    if(!csv.isEmpty())
      QApplication::clipboard()->setText(csv);

    NavApp::setStatusMessage(QString(tr("Copied %1 entries to clipboard.")).arg(exported));
  }
}

void SearchBaseTable::initViewAndController(atools::sql::SqlDatabase *db)
{
  view->horizontalHeader()->setSectionsMovable(true);
  view->verticalHeader()->setSectionsMovable(false);
  view->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);

  delete controller;
  controller = new SqlController(db, columns, view);
  controller->prepareModel();

  csvExporter = new CsvExporter(mainWindow, controller);
}

void SearchBaseTable::filterByRecord(const atools::sql::SqlRecord& record)
{
  controller->filterByRecord(record);
}

void SearchBaseTable::optionsChanged()
{
  // Need to reset model for "treat empty icons special"
  preDatabaseLoad();
  postDatabaseLoad();

  // Adapt table view text size
  zoomHandler->zoomPercent(OptionData::instance().getGuiSearchTableTextSize());

  // Update the unit strings in the table header
  updateUnits();

  // Run searches again to reflect unit changes
  updateDistanceSearch();

  for(const Column *col : columns->getColumns())
  {
    if(col->getSpinBoxWidget() != nullptr)
      updateFromSpinBox(col->getSpinBoxWidget()->value(), col);

    if(col->getMaxSpinBoxWidget() != nullptr)
      updateFromMaxSpinBox(col->getMaxSpinBoxWidget()->value(), col);

    if(col->getMinSpinBoxWidget() != nullptr)
      updateFromMinSpinBox(col->getMinSpinBoxWidget()->value(), col);
  }
  view->update();
}

void SearchBaseTable::styleChanged()
{
  view->update();
}

void SearchBaseTable::updateTableSelection(bool noFollow)
{
  tableSelectionChangedInternal(noFollow);
}

void SearchBaseTable::searchMarkChanged(const atools::geo::Pos& mark)
{
  qDebug() << "new mark" << mark;
  if(columns->isDistanceCheckBoxChecked() && mark.isValid())
    updateDistanceSearch();
}

void SearchBaseTable::updateDistanceSearch()
{
  if(columns->isDistanceCheckBoxChecked() &&
     NavApp::getMapWidget()->getSearchMarkPos().isValid())
  {
    // Currently running distance search - update result
    QSpinBox *minDistanceWidget = columns->getMinDistanceWidget();
    QSpinBox *maxDistanceWidget = columns->getMaxDistanceWidget();
    QComboBox *distanceDirWidget = columns->getDistanceDirectionWidget();

    controller->filterByDistance(NavApp::getMapWidget()->getSearchMarkPos(),
                                 static_cast<sqlproxymodel::SearchDirection>(distanceDirWidget->currentIndex()),
                                 Unit::rev(minDistanceWidget->value(), Unit::distNmF),
                                 Unit::rev(maxDistanceWidget->value(), Unit::distNmF));

    controller->loadAllRowsForDistanceSearch();
  }
}

void SearchBaseTable::connectSearchWidgets()
{
  void (QComboBox::*curIndexChangedPtr)(int) = &QComboBox::currentIndexChanged;
  void (QSpinBox::*valueChangedPtr)(int) = &QSpinBox::valueChanged;

  // Connect query builder callback to lambda ======================================
  if(columns->getQueryBuilder().isValid())
  {
    controller->setBuilder(columns->getQueryBuilder());

    QWidget *widget = columns->getQueryBuilder().getWidget();

    if(widget != nullptr)
    {
      QLineEdit *lineEdit = dynamic_cast<QLineEdit *>(widget);

      // Only line edit allowed for now
      if(lineEdit != nullptr)
      {
        connect(lineEdit, &QLineEdit::textChanged, this, [ = ](const QString& text)
        {
          Q_UNUSED(text)
          controller->filterByBuilder();
          updateButtonMenu();
          editStartTimer();
        });
      }
    }
  }

  // Connect all column assigned widgets to lambda ======================================
  for(const Column *col : columns->getColumns())
  {
    if(col->getLineEditWidget() != nullptr)
    {
      connect(col->getLineEditWidget(), &QLineEdit::textChanged, this, [ = ](const QString& text)
      {
        controller->filterByLineEdit(col, text);
        updateButtonMenu();
        editStartTimer();
      });
    }
    else if(col->getComboBoxWidget() != nullptr)
    {
      if(col->getComboBoxWidget()->isEditable())
      {
        // Treat editable combo boxes like line edits
        connect(col->getComboBoxWidget(), &QComboBox::editTextChanged, this, [ = ](const QString& text)
        {
          QComboBox *box = col->getComboBoxWidget();

          {
            QSignalBlocker blocker(box);

            // Reset index if entered word does not match
            QString txt = box->currentText();
            box->setCurrentIndex(box->findText(text, Qt::MatchExactly));
            box->setCurrentText(txt);
          }

          controller->filterByLineEdit(col, text);
          updateButtonMenu();
          editStartTimer();
        });
      }
      else
      {
        connect(col->getComboBoxWidget(), curIndexChangedPtr, this, [ = ](int index)
        {
          controller->filterByComboBox(col, index, index == 0);
          updateButtonMenu();
          editStartTimer();
        });
      }
    }
    else if(col->getCheckBoxWidget() != nullptr)
    {
      connect(col->getCheckBoxWidget(), &QCheckBox::stateChanged, this, [ = ](int state)
      {
        controller->filterByCheckbox(col, state, col->getCheckBoxWidget()->isTristate());
        updateButtonMenu();
        editStartTimer();
      });
    }
    else if(col->getSpinBoxWidget() != nullptr)
    {
      connect(col->getSpinBoxWidget(), valueChangedPtr, this, [ = ](int value)
      {
        updateFromSpinBox(value, col);
        updateButtonMenu();
        editStartTimer();
      });
    }
    else if(col->getMinSpinBoxWidget() != nullptr && col->getMaxSpinBoxWidget() != nullptr)
    {
      connect(col->getMinSpinBoxWidget(), valueChangedPtr, this, [ = ](int value)
      {
        updateFromMinSpinBox(value, col);
        updateButtonMenu();
        editStartTimer();
      });

      connect(col->getMaxSpinBoxWidget(), valueChangedPtr, this, [ = ](int value)
      {
        updateFromMaxSpinBox(value, col);
        updateButtonMenu();
        editStartTimer();
      });
    }
  }

  QSpinBox *minDistanceWidget = columns->getMinDistanceWidget();
  QSpinBox *maxDistanceWidget = columns->getMaxDistanceWidget();
  QComboBox *distanceDirWidget = columns->getDistanceDirectionWidget();
  QCheckBox *distanceCheckBox = columns->getDistanceCheckBox();

  if(minDistanceWidget != nullptr && maxDistanceWidget != nullptr &&
     distanceDirWidget != nullptr && distanceCheckBox != nullptr)
  {
    // If all distance widgets are present connect them
    connect(distanceCheckBox, &QCheckBox::stateChanged, this, &SearchBaseTable::distanceSearchStateChanged);

    connect(minDistanceWidget, valueChangedPtr, this, [ = ](int value)
    {
      controller->filterByDistanceUpdate(
        static_cast<sqlproxymodel::SearchDirection>(distanceDirWidget->currentIndex()),
        Unit::rev(value, Unit::distNmF),
        Unit::rev(maxDistanceWidget->value(), Unit::distNmF));

      maxDistanceWidget->setMinimum(value > 10 ? value : 10);
      updateButtonMenu();
      editStartTimer();
    });

    connect(maxDistanceWidget, valueChangedPtr, this, [ = ](int value)
    {
      controller->filterByDistanceUpdate(
        static_cast<sqlproxymodel::SearchDirection>(distanceDirWidget->currentIndex()),
        Unit::rev(minDistanceWidget->value(), Unit::distNmF),
        Unit::rev(value, Unit::distNmF));
      minDistanceWidget->setMaximum(value);
      updateButtonMenu();
      editStartTimer();
    });

    connect(distanceDirWidget, curIndexChangedPtr, this, [ = ](int index)
    {
      controller->filterByDistanceUpdate(static_cast<sqlproxymodel::SearchDirection>(index),
                                         Unit::rev(minDistanceWidget->value(), Unit::distNmF),
                                         Unit::rev(maxDistanceWidget->value(), Unit::distNmF));
      updateButtonMenu();
      editStartTimer();
    });
  }
}

void SearchBaseTable::updateFromSpinBox(int value, const Column *col)
{
  if(col->getUnitConvert() != nullptr)
    // Convert widget units to internal units using the given function pointer
    controller->filterBySpinBox(
      col, atools::roundToInt(Unit::rev(value, col->getUnitConvert())));
  else
    controller->filterBySpinBox(col, value);
}

void SearchBaseTable::updateFromMinSpinBox(int value, const Column *col)
{
  float valMin = value, valMax = col->getMaxSpinBoxWidget()->value();

  if(col->getUnitConvert() != nullptr)
  {
    // Convert widget units to internal units using the given function pointer
    valMin = atools::roundToInt(Unit::rev(valMin, col->getUnitConvert()));
    valMax = atools::roundToInt(Unit::rev(valMax, col->getUnitConvert()));
  }

  controller->filterByMinMaxSpinBox(col, atools::roundToInt(valMin),
                                    atools::roundToInt(valMax));
  col->getMaxSpinBoxWidget()->setMinimum(value);
}

void SearchBaseTable::updateFromMaxSpinBox(int value, const Column *col)
{
  float valMin = col->getMinSpinBoxWidget()->value(), valMax = value;

  if(col->getUnitConvert() != nullptr)
  {
    // Convert widget units to internal units using the given function pointer
    valMin = atools::roundToInt(Unit::rev(valMin, col->getUnitConvert()));
    valMax = atools::roundToInt(Unit::rev(valMax, col->getUnitConvert()));
  }

  controller->filterByMinMaxSpinBox(col, atools::roundToInt(valMin), atools::roundToInt(valMax));
  col->getMinSpinBoxWidget()->setMaximum(value);

}

void SearchBaseTable::distanceSearchStateChanged(int state)
{
  distanceSearchChanged(state == Qt::Checked, true);
}

void SearchBaseTable::distanceSearchChanged(bool checked, bool changeViewState)
{
  if((NavApp::getMapWidget()->getSearchMarkPos().isNull() ||
      !NavApp::getMapWidget()->getSearchMarkPos().isValid()) && checked)
  {
    atools::gui::Dialog(mainWindow).showInfoMsgBox(lnm::ACTIONS_SHOW_SEARCH_CENTER_NULL,
                                                   tr("The search center is not set.\n"
                                                      "Right-click into the map and select\n"
                                                      "\"Set Center for Distance Search\"."),
                                                   tr("Do not &show this dialog again."));
  }

  QSpinBox *minDistanceWidget = columns->getMinDistanceWidget();
  QSpinBox *maxDistanceWidget = columns->getMaxDistanceWidget();
  QComboBox *distanceDirWidget = columns->getDistanceDirectionWidget();

  if(changeViewState)
    saveViewState(!checked);

  controller->filterByDistance(
    checked ? NavApp::getMapWidget()->getSearchMarkPos() : atools::geo::Pos(),
    static_cast<sqlproxymodel::SearchDirection>(distanceDirWidget->currentIndex()),
    Unit::rev(minDistanceWidget->value(), Unit::distNmF),
    Unit::rev(maxDistanceWidget->value(), Unit::distNmF));

  minDistanceWidget->setEnabled(checked);
  maxDistanceWidget->setEnabled(checked);
  distanceDirWidget->setEnabled(checked);
  if(checked)
    controller->loadAllRowsForDistanceSearch();
  restoreViewState(checked);
  updateButtonMenu();
}

void SearchBaseTable::installEventFilterForWidget(QWidget *widget)
{
  widget->installEventFilter(widgetEventFilter);
}

/* Search criteria editing has started. Start or restart the timer for a
 * delayed update if distance search is used */
void SearchBaseTable::editStartTimer()
{
  if(controller->isDistanceSearch())
  {
    qDebug() << "editStarted";
    updateTimer->start(DISTANCE_EDIT_UPDATE_TIMEOUT_MS);
  }
}

/* Delayed update timeout. Update result if distance search is active */
void SearchBaseTable::editTimeout()
{
  qDebug() << "editTimeout";
  controller->loadAllRowsForDistanceSearch();
}

void SearchBaseTable::connectSearchSlots()
{
  connect(view, &QTableView::doubleClicked, this, &SearchBaseTable::doubleClick);
  connect(view, &QTableView::customContextMenuRequested, this, &SearchBaseTable::contextMenu);

  Ui::MainWindow *ui = NavApp::getMainUi();

  connect(ui->actionSearchShowAll, &QAction::triggered, this, &SearchBaseTable::loadAllRowsIntoView);
  connect(ui->actionSearchResetSearch, &QAction::triggered, this, &SearchBaseTable::resetSearch);

  reconnectSelectionModel();

  connect(controller->getSqlModel(), &SqlModel::modelReset, this, &SearchBaseTable::reconnectSelectionModel);
  connect(controller->getSqlModel(), &SqlModel::fetchedMore, this, &SearchBaseTable::fetchedMore);

  connect(ui->dockWidgetSearch, &QDockWidget::visibilityChanged, this, &SearchBaseTable::dockVisibilityChanged);
}

void SearchBaseTable::updateUnits()
{
  columns->updateUnits();
  controller->updateHeaderData();
}

void SearchBaseTable::clearSelection()
{
  view->clearSelection();
}

bool SearchBaseTable::hasSelection() const
{
  return view->selectionModel() == nullptr ? false : view->selectionModel()->hasSelection();
}

/* Connect selection model again after a SQL model reset */
void SearchBaseTable::reconnectSelectionModel()
{
  if(view->selectionModel() != nullptr)
  {
    void (SearchBaseTable::*selChangedPtr)(const QItemSelection& selected, const QItemSelection& deselected) =
      &SearchBaseTable::tableSelectionChanged;

    connect(view->selectionModel(), &QItemSelectionModel::selectionChanged, this, selChangedPtr);
  }
}

/* Slot for table selection changed */
void SearchBaseTable::tableSelectionChanged(const QItemSelection&, const QItemSelection&)
{
  tableSelectionChangedInternal(false /* follow selection */);
}

/* Update highlights if dock is hidden or shown (does not change for dock tab stacks) */
void SearchBaseTable::dockVisibilityChanged(bool)
{
  tableSelectionChangedInternal(true /* do not follow selection */);
}

void SearchBaseTable::fetchedMore()
{
  tableSelectionChangedInternal(true /* do not follow selection */);
}

void SearchBaseTable::tableSelectionChangedInternal(bool noFollow)
{
  QItemSelectionModel *sm = view->selectionModel();

  int selectedRows = 0;
  if(sm != nullptr && sm->hasSelection())
    selectedRows = sm->selectedRows().size();

  updatePushButtons();

  emit selectionChanged(this, selectedRows, controller->getVisibleRowCount(), controller->getTotalRowCount());

  // Follow selection =======================
  if(!noFollow)
  {
    if(sm != nullptr &&
       sm->currentIndex().isValid() &&
       sm->isSelected(sm->currentIndex()) &&
       followModeAction() != nullptr &&
       followModeAction()->isChecked())
      showRow(sm->currentIndex().row(), false /* show info */);
  }
}

void SearchBaseTable::preDatabaseLoad()
{
  saveViewState(controller->isDistanceSearch());
  controller->preDatabaseLoad();
}

void SearchBaseTable::postDatabaseLoad()
{
  controller->postDatabaseLoad();
  restoreViewState(controller->isDistanceSearch());
}

/* Reset view sort order, column width and column order back to default values */
void SearchBaseTable::resetView()
{
  if(NavApp::getSearchController()->getCurrentSearchTabId() == tabIndex)
  {
    controller->resetView();
    updatePushButtons();
    NavApp::setStatusMessage(tr("Table view reset to defaults."));
  }
}

void SearchBaseTable::refreshData(bool loadAll, bool keepSelection)
{
  controller->refreshData(loadAll, keepSelection);

  tableSelectionChangedInternal(true /* do not follow selection */);
}

void SearchBaseTable::refreshView()
{
  controller->refreshView();

  tableSelectionChangedInternal(true /* do not follow selection */);
}

int SearchBaseTable::getVisibleRowCount() const
{
  return controller->getVisibleRowCount();
}

int SearchBaseTable::getTotalRowCount() const
{
  return controller->getTotalRowCount();
}

int SearchBaseTable::getSelectedRowCount() const
{
  QItemSelectionModel *sm = view->selectionModel();

  int selectedRows = 0;
  if(sm != nullptr && sm->hasSelection())
    selectedRows = sm->selectedRows().size();

  return selectedRows;
}

QVector<int> SearchBaseTable::getSelectedIds() const
{
  QVector<int> retval;

  const QItemSelection& selection = controller->getSelection();
  for(const QItemSelectionRange& rng :  selection)
  {
    for(int row = rng.top(); row <= rng.bottom(); ++row)
    {
      if(controller->hasRow(row))
        retval.append(controller->getRawData(row, columns->getIdColumnName()).toInt());
    }
  }
  return retval;
}

void SearchBaseTable::resetSearch()
{
  if(NavApp::getSearchController()->getCurrentSearchTabId() == tabIndex)
  {
    controller->resetSearch();
    updatePushButtons();
    NavApp::setStatusMessage(tr("Search filters cleared."));
  }
}

/* Loads all rows into the table view */
void SearchBaseTable::loadAllRowsIntoView()
{
  if(NavApp::getSearchController()->getCurrentSearchTabId() == tabIndex)
  {
    // bool allSelected = getSelectedRowCount() == getVisibleRowCount();

    // Clear selection since it can get invalid
    view->clearSelection();

    controller->loadAllRows();
    updatePushButtons();

    // if(allSelected)
    // view->selectAll();

    NavApp::setStatusMessage(tr("All entries read."));
  }
}

void SearchBaseTable::showFirstEntry()
{
  showRow(0, true /* show info */);
}

void SearchBaseTable::showSelectedEntry()
{
  QModelIndex idx = view->currentIndex();

  if(idx.isValid())
    showRow(idx.row(), true /* show info */);
}

void SearchBaseTable::activateView()
{
  // view->raise();
  // view->activateWindow();
  view->setFocus();
}

/* Double click into table view */
void SearchBaseTable::doubleClick(const QModelIndex& index)
{
  if(index.isValid())
    showRow(index.row(), true /* show info */);
}

void SearchBaseTable::showRow(int row, bool showInfo)
{
  qDebug() << Q_FUNC_INFO;

  // Show on information panel
  map::MapTypes navType = map::NONE;
  map::MapAirspaceSources airspaceSource = map::AIRSPACE_SRC_NONE;
  int id = -1;
  // get airport, VOR, NDB or waypoint id from model row
  getNavTypeAndId(row, navType, airspaceSource, id);

  if(id > 0 && navType != map::NONE)
  {
    // Check if the used table has bounding rectangle columns

    // Show on map
    if(columns->hasColumn("left_lonx") && columns->hasColumn("top_laty") &&
       columns->hasColumn("right_lonx") && columns->hasColumn("bottom_laty"))
    {
      // Rectangle at airports
      atools::geo::Pos topLeft(controller->getRawData(row, "left_lonx"),
                               controller->getRawData(row, "top_laty"));
      atools::geo::Pos bottomRight(controller->getRawData(row, "right_lonx"),
                                   controller->getRawData(row, "bottom_laty"));

      if(topLeft.isValid() && bottomRight.isValid())
        emit showRect(atools::geo::Rect(topLeft, bottomRight), true);
    }
    else if(columns->hasColumn("min_lonx") && columns->hasColumn("max_laty") &&
            columns->hasColumn("max_lonx") && columns->hasColumn("min_laty"))
    {
      // Different column names for airspaces and online centers
      atools::geo::Pos topLeft(controller->getRawData(row, "min_lonx"),
                               controller->getRawData(row, "max_laty"));
      atools::geo::Pos bottomRight(controller->getRawData(row, "max_lonx"),
                                   controller->getRawData(row, "min_laty"));

      if(topLeft.isValid() && bottomRight.isValid())
        emit showRect(atools::geo::Rect(topLeft, bottomRight), true);
    }
    else if(columns->hasColumn("departure_lonx") && columns->hasColumn("departure_laty") &&
            columns->hasColumn("destination_lonx") && columns->hasColumn("destination_laty"))
    {
      atools::geo::Pos depart(controller->getRawData(row, "departure_lonx"),
                              controller->getRawData(row, "departure_laty"));
      atools::geo::Pos dest(controller->getRawData(row, "destination_lonx"),
                            controller->getRawData(row, "destination_laty"));

      if(depart.isValid() && dest.isValid())
        emit showRect(atools::geo::boundingRect({depart, dest}), true);
      else if(depart.isValid())
        emit showPos(depart, 0.f, true);
      else if(dest.isValid())
        emit showPos(dest, 0.f, true);
    }
    else
    {
      atools::geo::Pos p(controller->getRawData(row, "lonx"), controller->getRawData(row, "laty"));
      if(p.isValid())
        emit showPos(p, 0.f, true);
    }

    if(showInfo)
    {
      map::MapResult result;
      mapQuery->getMapObjectById(result, navType, airspaceSource, id, false /* airport from nav database */);
      emit showInformation(result);
    }
  }
}

void SearchBaseTable::nothingSelectedTriggered()
{
  controller->selectNoRows();
}

/* Context menu in table view selected */
void SearchBaseTable::contextMenu(const QPoint& pos)
{
  qDebug() << Q_FUNC_INFO << "pos" << pos;

  Ui::MainWindow *ui = NavApp::getMainUi();

  QPoint menuPos = QCursor::pos();
  // Use widget center if position is not inside widget
  if(!view->rect().contains(view->mapFromGlobal(QCursor::pos())))
    menuPos = view->mapToGlobal(view->rect().center());

  // Move menu position off the cursor to avoid accidental selection on touchpads
  menuPos += QPoint(3, 3);

  QString fieldData;

  // Save and restore action texts on return
  atools::gui::ActionTextSaver saver({ui->actionSearchShowInformation, ui->actionSearchShowApproaches,
                                      ui->actionSearchShowApproachesCustom, ui->actionSearchShowOnMap,
                                      ui->actionSearchFilterIncluding, ui->actionSearchFilterExcluding,
                                      ui->actionRouteAirportDest, ui->actionRouteAirportStart,
                                      ui->actionRouteAirportAlternate, ui->actionRouteAddPos, ui->actionRouteAppendPos,
                                      ui->actionMapRangeRings, ui->actionMapNavaidRange, ui->actionMapTrafficPattern,
                                      ui->actionMapHold, ui->actionUserdataAdd, ui->actionUserdataDelete,
                                      ui->actionUserdataEdit, ui->actionLogdataAdd, ui->actionLogdataDelete,
                                      ui->actionLogdataEdit, ui->actionLogdataPerfLoad, ui->actionLogdataRouteOpen,
                                      ui->actionSearchShowOnMapAirport, ui->actionSearchShowInformationAirport,
                                      ui->actionSearchLogdataOpenPlan, ui->actionSearchLogdataSavePlanAs,
                                      ui->actionSearchLogdataOpenPerf, ui->actionSearchLogdataSavePerfAs,
                                      ui->actionSearchLogdataSaveGpxAs});

  // Re-enable actions on exit to allow keystrokes
  atools::gui::ActionStateSaver stateSaver(
  {
    ui->actionSearchShowInformation, ui->actionSearchShowApproaches, ui->actionSearchShowApproachesCustom,
    ui->actionSearchShowOnMap, ui->actionSearchFilterIncluding, ui->actionSearchFilterExcluding,
    ui->actionSearchResetSearch, ui->actionSearchShowAll, ui->actionMapTrafficPattern, ui->actionMapHold,
    ui->actionMapRangeRings, ui->actionMapNavaidRange, ui->actionRouteAirportStart, ui->actionRouteAirportDest,
    ui->actionRouteAirportAlternate, ui->actionRouteAddPos, ui->actionRouteAppendPos, ui->actionSearchTableCopy,
    ui->actionSearchTableSelectAll, ui->actionSearchTableSelectNothing, ui->actionSearchResetView,
    ui->actionSearchSetMark, ui->actionLogdataPerfLoad, ui->actionLogdataRouteOpen,
    ui->actionSearchShowOnMapAirport, ui->actionSearchShowInformationAirport, ui->actionSearchLogdataOpenPlan,
    ui->actionSearchLogdataSavePlanAs, ui->actionSearchLogdataOpenPerf, ui->actionSearchLogdataSavePerfAs,
    ui->actionSearchLogdataSaveGpxAs});

  bool columnCanFilter = false;
  atools::geo::Pos position;
  QModelIndex index = controller->getModelIndexAt(pos);
  map::MapTypes navType = map::NONE;

  // Airport in airport search also used for airport below cursor in logbook
  map::MapAirport airport;
  map::MapLogbookEntry logEntry;
  atools::sql::SqlRecord logRecord;

  // True if airport below cursor in logbook and ident not empty
  bool logAirport = false;

  int id = -1;
  if(index.isValid())
  {
    const Column *columnDescriptor = columns->getColumn(index.column());
    Q_ASSERT(columnDescriptor != nullptr);
    columnCanFilter = columnDescriptor->isFilter();

    if(columnCanFilter)
      // Disabled menu items don't need any content
      fieldData = atools::elideTextShort(controller->getFieldDataAt(index), 30);

    if(controller->hasColumn("lonx") && controller->hasColumn("laty"))
      // Get position to display range rings
      position = atools::geo::Pos(controller->getRawData(index.row(), "lonx"),
                                  controller->getRawData(index.row(), "laty"));

    // get airport, VOR, NDB or waypoint id from model row
    getNavTypeAndId(index.row(), navType, id);
    if(navType == map::AIRPORT)
      airportQuery->getAirportById(airport, id);
    else if(navType == map::LOGBOOK)
    {
      // Prepare airport for menu items in logbook
      logEntry = NavApp::getLogdataController()->getLogEntryById(id);
      logRecord = NavApp::getLogdataController()->getLogEntryRecordById(id);

      QString name = columnDescriptor->getColumnName();
      QList<map::MapAirport> airports;
      if(name == "destination_ident" || name == "destination_name")
        airports = airportQuery->getAirportsByOfficialIdent(logEntry.destinationIdent, &logEntry.destinationPos);
      else if(name == "departure_ident" || name == "departure_name")
        airports = airportQuery->getAirportsByOfficialIdent(logEntry.departureIdent, &logEntry.departurePos);

      if(!airports.isEmpty())
      {
        airport = airports.first();
        logAirport = true;
      }
      else
        qWarning() << Q_FUNC_INFO << "No airport found";
    }
  }
  else
    qDebug() << "Invalid index at" << pos;

  // Add data to menu item text ======================================================================
  QString filter = fieldData.isEmpty() ? QString() : tr("\"%1\"").arg(fieldData);
  ui->actionSearchFilterIncluding->setText(ui->actionSearchFilterIncluding->text().arg(filter));
  ui->actionSearchFilterIncluding->setEnabled(!fieldData.isEmpty() && index.isValid() && columnCanFilter);

  ui->actionSearchFilterExcluding->setText(ui->actionSearchFilterExcluding->text().arg(filter));
  ui->actionSearchFilterExcluding->setEnabled(!fieldData.isEmpty() && index.isValid() && columnCanFilter);

  int range = controller->hasColumn("range") ? controller->getRawData(index.row(), "range").toInt() : 0;
  ui->actionMapNavaidRange->setEnabled(range > 0 && (navType == map::VOR || navType == map::NDB));

  ui->actionRouteAddPos->setEnabled(navType == map::VOR || navType == map::NDB ||
                                    navType == map::WAYPOINT || navType == map::AIRPORT || navType == map::USERPOINT);
  ui->actionRouteAppendPos->setEnabled(navType == map::VOR || navType == map::NDB ||
                                       navType == map::WAYPOINT || navType == map::AIRPORT ||
                                       navType == map::USERPOINT);

  // Airport actions ==============================================================
  ui->actionRouteAirportStart->setEnabled(airport.isValid());
  ui->actionRouteAirportDest->setEnabled(airport.isValid());
  ui->actionSearchShowOnMapAirport->setEnabled(airport.isValid());
  ui->actionSearchShowInformationAirport->setEnabled(airport.isValid());

  const Route& route = NavApp::getRouteConst();
  ui->actionRouteAirportAlternate->setEnabled(airport.isValid() && route.getSizeWithoutAlternates() > 0);
  ui->actionMapTrafficPattern->setEnabled(navType == map::AIRPORT && !airport.noRunways());
  ui->actionMapHold->setEnabled(navType == map::VOR || navType == map::NDB || navType == map::WAYPOINT ||
                                navType == map::USERPOINT || navType == map::AIRPORT);

  // Airport procedure actions ==============================================================
  ui->actionSearchShowApproaches->setEnabled(false);
  ui->actionSearchShowApproachesCustom->setEnabled(false);
  if(navType == map::AIRPORT && airport.isValid())
  {
    bool departureFilter, arrivalFilter, hasDeparture, hasAnyArrival, airportDeparture, airportDestination,
         airportRoundTrip;

    route.getAirportProcedureFlags(airport, -1, departureFilter, arrivalFilter, hasDeparture,
                                   hasAnyArrival, airportDeparture, airportDestination, airportRoundTrip);

    if(hasAnyArrival || hasDeparture)
    {
      if(airportDeparture && !airportRoundTrip)
      {
        if(hasDeparture)
        {
          ui->actionSearchShowApproaches->setEnabled(true);
          ui->actionSearchShowApproaches->setText(ui->actionSearchShowApproaches->text().arg(tr("Departure ")));
        }
        else
          ui->actionSearchShowApproaches->setText(tr("Show procedures (airport has no departure procedure)"));
      }
      else if(airportDestination && !airportRoundTrip)
      {
        if(hasAnyArrival)
        {
          ui->actionSearchShowApproaches->setEnabled(true);
          ui->actionSearchShowApproaches->setText(ui->actionSearchShowApproaches->text().arg(tr("Arrival ")));
        }
        else
          ui->actionSearchShowApproaches->setText(tr("Show procedures (airport has no arrival procedure)"));
      }
      else
      {
        ui->actionSearchShowApproaches->setEnabled(true);
        ui->actionSearchShowApproaches->setText(ui->actionSearchShowApproaches->text().arg(tr("all ")));
      }
    }
    else
      ui->actionSearchShowApproaches->setText(tr("Show procedures (airport has no procedure)"));

    ui->actionSearchShowApproachesCustom->setEnabled(true);
    if(airportDestination)
      ui->actionSearchShowApproachesCustom->setText(tr("Create &Approach to Airport and insert into Flight Plan"));
    else
      ui->actionSearchShowApproachesCustom->setText(tr("Create &Approach and use Airport as Destination"));
  }
  else
    ui->actionSearchShowApproaches->setText(tr("Show &procedures"));

  ui->actionMapRangeRings->setEnabled(index.isValid() && position.isValid());
  ui->actionSearchSetMark->setEnabled(index.isValid() && position.isValid());

  ui->actionMapNavaidRange->setText(tr("Add &Navaid Range Ring"));
  ui->actionRouteAddPos->setText(tr("&Add to Flight Plan"));
  ui->actionRouteAppendPos->setText(tr("Append to &Flight Plan"));

  if(tabIndex == si::SEARCH_AIRPORT || tabIndex == si::SEARCH_LOG)
  {
    ui->actionRouteAirportStart->setText(tr("Set as &Flight Plan Departure"));
    ui->actionRouteAirportDest->setText(tr("Set as Flight Plan &Destination"));
    ui->actionRouteAirportAlternate->setText(tr("Add as Flight Plan &Alternate"));
  }

  if(tabIndex == si::SEARCH_LOG)
  {
    QString airportText;
    if(airport.isValid())
      airportText = map::airportText(airport, 20);
    else if(logAirport)
      airportText = tr("(Airport not found)");

    ui->actionSearchShowInformationAirport->setText(tr("Show &Information for %1").arg(airportText));
    ui->actionSearchShowOnMapAirport->setText(tr("Show %1 on &Map").arg(airportText));

    ui->actionRouteAirportStart->setText(tr("Set %1 as &Flight Plan Departure").arg(airportText));
    ui->actionRouteAirportDest->setText(tr("Set %1 as Flight Plan &Destination").arg(airportText));
    ui->actionRouteAirportAlternate->setText(tr("Add %1 as Flight Plan &Alternate").arg(airportText));
  }

  ui->actionMapTrafficPattern->setText(tr("Add Airport &Traffic Pattern ..."));
  ui->actionMapHold->setText(tr("Add &Holding ..."));

  ui->actionSearchTableCopy->setEnabled(index.isValid());
  ui->actionSearchTableSelectAll->setEnabled(controller->getTotalRowCount() > 0);
  ui->actionSearchTableSelectNothing->setEnabled(
    controller->getTotalRowCount() > 0 &&
    (view->selectionModel() == nullptr ? false : view->selectionModel()->hasSelection()));

  // Update texts to give user a hint for hidden user features in the disabled menu items =====================
  QString notShown(tr(" (hidden on map)"));
  if(!NavApp::getMapMarkHandler()->isShown(map::MARK_RANGE_RINGS))
  {
    ui->actionMapRangeRings->setDisabled(true);
    ui->actionMapNavaidRange->setDisabled(true);
    ui->actionMapRangeRings->setText(ui->actionMapRangeRings->text() + notShown);
    ui->actionMapNavaidRange->setText(ui->actionMapNavaidRange->text() + notShown);
  }
  if(!NavApp::getMapMarkHandler()->isShown(map::MARK_HOLDS))
  {
    ui->actionMapHold->setDisabled(true);
    ui->actionMapHold->setText(ui->actionMapHold->text() + notShown);
  }
  if(!NavApp::getMapMarkHandler()->isShown(map::MARK_PATTERNS))
  {
    ui->actionMapTrafficPattern->setDisabled(true);
    ui->actionMapTrafficPattern->setText(ui->actionMapTrafficPattern->text() + notShown);
  }

  // Build the menu depending on tab =========================================================================
  int selectedRows = getSelectedRowCount();
  QMenu menu;
  menu.setToolTipsVisible(NavApp::isMenuToolTipsVisible());
  if(atools::contains(tabIndex,
                      {si::SEARCH_AIRPORT, si::SEARCH_NAV, si::SEARCH_USER, si::SEARCH_LOG, si::SEARCH_ONLINE_CENTER,
                       si::SEARCH_ONLINE_CLIENT}))
  {
    menu.addAction(ui->actionSearchShowInformation);
    ui->actionSearchShowInformation->setEnabled(selectedRows > 0);
    if(navType == map::AIRPORT)
    {
      menu.addAction(ui->actionSearchShowApproaches);
      menu.addAction(ui->actionSearchShowApproachesCustom);
    }
    menu.addAction(ui->actionSearchShowOnMap);

    if(tabIndex == si::SEARCH_ONLINE_CENTER || tabIndex == si::SEARCH_ONLINE_CLIENT)
      ui->actionSearchShowOnMap->setEnabled(selectedRows > 0 && position.isValid());
    else
      ui->actionSearchShowOnMap->setEnabled(selectedRows > 0);
    menu.addSeparator();
  }

  if(atools::contains(tabIndex, {si::SEARCH_LOG}))
  {
    // Logbook airport menu if clicked on departure or destination =======================
    // menu takes ownership
    QMenu *sub = menu.addMenu(tr("&Airport"));
    sub->setToolTipsVisible(NavApp::isMenuToolTipsVisible());
    sub->addAction(ui->actionSearchShowInformationAirport);
    sub->addAction(ui->actionSearchShowOnMapAirport);
    sub->addSeparator();
    sub->addAction(ui->actionRouteAirportStart);
    sub->addAction(ui->actionRouteAirportDest);
    sub->addAction(ui->actionRouteAirportAlternate);
    menu.addSeparator();
  }

  // Add extra menu items in the user defined waypoint table - these are already connected
  if(tabIndex == si::SEARCH_USER)
  {
    if(selectedRows > 1)
    {
      ui->actionUserdataEdit->setText(tr("&Edit Userpoints ..."));
      ui->actionUserdataDelete->setText(tr("&Delete Userpoints"));
    }
    else
    {
      ui->actionUserdataEdit->setText(tr("&Edit Userpoint ..."));
      ui->actionUserdataDelete->setText(tr("&Delete Userpoint"));
    }

    menu.addAction(ui->actionUserdataAdd);
    menu.addAction(ui->actionUserdataEdit);
    menu.addAction(ui->actionUserdataDelete);
    menu.addSeparator();
  } // if(tabIndex == si::SEARCH_USER)

  if(tabIndex == si::SEARCH_LOG)
  {
    if(selectedRows > 1)
    {
      ui->actionLogdataEdit->setText(tr("&Edit Logbook Entries ..."));
      ui->actionLogdataDelete->setText(tr("&Delete Logbook Entries"));
    }
    else
    {
      ui->actionLogdataEdit->setText(tr("&Edit Logbook Entry ..."));
      ui->actionLogdataDelete->setText(tr("&Delete Logbook Entry"));
    }

    menu.addAction(ui->actionLogdataAdd);
    menu.addAction(ui->actionLogdataEdit);
    menu.addAction(ui->actionLogdataDelete);
    menu.addSeparator();

    // Logbook open or save attached files sub menu =====================================
    bool route = NavApp::getLogdataController()->hasRouteAttached(logEntry.id);
    ui->actionSearchLogdataOpenPlan->setEnabled(route);
    ui->actionSearchLogdataSavePlanAs->setEnabled(route);

    if(!route)
    {
      ui->actionSearchLogdataOpenPlan->setText(ui->actionSearchLogdataOpenPlan->text() + tr(" (no attachment)"));
      ui->actionSearchLogdataSavePlanAs->setText(ui->actionSearchLogdataSavePlanAs->text() + tr(" (no attachment)"));
    }

    bool perf = NavApp::getLogdataController()->hasPerfAttached(logEntry.id);
    ui->actionSearchLogdataOpenPerf->setEnabled(perf);
    ui->actionSearchLogdataSavePerfAs->setEnabled(perf);

    if(!perf)
    {
      ui->actionSearchLogdataOpenPerf->setText(ui->actionSearchLogdataOpenPerf->text() + tr(" (no attachment)"));
      ui->actionSearchLogdataSavePerfAs->setText(ui->actionSearchLogdataSavePerfAs->text() + tr(" (no attachment)"));
    }

    bool gpx = NavApp::getLogdataController()->hasTrackAttached(logEntry.id);
    ui->actionSearchLogdataSaveGpxAs->setEnabled(gpx);
    if(!gpx)
      ui->actionSearchLogdataSaveGpxAs->setText(ui->actionSearchLogdataSaveGpxAs->text() + tr(" (no attachment)"));

    // Logbook open referenced files menu =======================
    ui->actionLogdataRouteOpen->setEnabled(false);
    if(!logEntry.routeFile.isEmpty())
    {
      if(QFileInfo::exists(logEntry.routeFile))
      {
        ui->actionLogdataRouteOpen->setEnabled(true);
        ui->actionLogdataRouteOpen->setText(ui->actionLogdataRouteOpen->text().
                                            arg(atools::elideTextShort(QFileInfo(logEntry.routeFile).fileName(), 20)));
      }
      else
        ui->actionLogdataRouteOpen->setText(ui->actionLogdataRouteOpen->text().arg(tr("(file not found)")));
    }
    else
      ui->actionLogdataRouteOpen->setText(ui->actionLogdataRouteOpen->text().arg(QString()));

    ui->actionLogdataPerfLoad->setEnabled(false);
    if(!logEntry.perfFile.isEmpty())
    {
      if(QFileInfo::exists(logEntry.perfFile))
      {
        ui->actionLogdataPerfLoad->setEnabled(true);
        ui->actionLogdataPerfLoad->setText(ui->actionLogdataPerfLoad->text().
                                           arg(atools::elideTextShort(QFileInfo(logEntry.perfFile).fileName(), 20)));
      }
      else
        ui->actionLogdataPerfLoad->setText(ui->actionLogdataPerfLoad->text().arg(tr("(file not found)")));
    }
    else
      ui->actionLogdataPerfLoad->setText(ui->actionLogdataPerfLoad->text().arg(QString()));

    // menu takes ownership of sub
    QMenu *sub = menu.addMenu(tr("&Files"));
    sub->setToolTipsVisible(NavApp::isMenuToolTipsVisible());
    sub->addAction(ui->actionLogdataRouteOpen);
    sub->addAction(ui->actionLogdataPerfLoad);
    sub->addSeparator();
    sub->addAction(ui->actionSearchLogdataOpenPlan);
    sub->addAction(ui->actionSearchLogdataSavePlanAs);
    sub->addSeparator();
    sub->addAction(ui->actionSearchLogdataOpenPerf);
    sub->addAction(ui->actionSearchLogdataSavePerfAs);
    sub->addSeparator();
    sub->addAction(ui->actionSearchLogdataSaveGpxAs);
    menu.addSeparator();
  } // if(tabIndex == si::SEARCH_LOG)

  if(atools::contains(tabIndex,
                      {si::SEARCH_AIRPORT, si::SEARCH_NAV, si::SEARCH_USER, si::SEARCH_LOG, si::SEARCH_ONLINE_CENTER,
                       si::SEARCH_ONLINE_CLIENT}))
  {
    menu.addAction(followModeAction());
    menu.addSeparator();
    if(atools::contains(tabIndex, {si::SEARCH_LOG}))
    {
      // Logbook display options menu =======================
      QMenu *sub = menu.addMenu(tr("&View Options"));
      sub->setToolTipsVisible(NavApp::isMenuToolTipsVisible());
      sub->addAction(ui->actionSearchLogdataShowDirect);
      sub->addAction(ui->actionSearchLogdataShowRoute);
      sub->addAction(ui->actionSearchLogdataShowTrack);
      menu.addSeparator();
    }

    menu.addAction(ui->actionSearchFilterIncluding);
    menu.addAction(ui->actionSearchFilterExcluding);
    menu.addSeparator();

    menu.addAction(ui->actionSearchResetSearch);
    menu.addAction(ui->actionSearchShowAll);
    menu.addSeparator();
  }

  if(atools::contains(tabIndex,
                      {si::SEARCH_AIRPORT, si::SEARCH_NAV, si::SEARCH_USER, si::SEARCH_ONLINE_CENTER,
                       si::SEARCH_ONLINE_CLIENT}))
  {
    menu.addAction(ui->actionMapRangeRings);
    if(atools::contains(tabIndex, {si::SEARCH_NAV}))
      menu.addAction(ui->actionMapNavaidRange);
    menu.addSeparator();
  }

  if(atools::contains(tabIndex, {si::SEARCH_AIRPORT, si::SEARCH_NAV, si::SEARCH_USER}))
  {
    if(atools::contains(tabIndex, {si::SEARCH_AIRPORT}))
      menu.addAction(ui->actionMapTrafficPattern);
    menu.addAction(ui->actionMapHold);
    menu.addSeparator();
  }

  if(atools::contains(tabIndex, {si::SEARCH_AIRPORT}))
  {
    menu.addAction(ui->actionRouteAirportStart);
    menu.addAction(ui->actionRouteAirportDest);
    menu.addAction(ui->actionRouteAirportAlternate);
    menu.addSeparator();
  }

  if(atools::contains(tabIndex, {si::SEARCH_AIRPORT, si::SEARCH_NAV, si::SEARCH_USER}))
  {
    menu.addAction(ui->actionRouteAddPos);
    menu.addAction(ui->actionRouteAppendPos);
    menu.addSeparator();
  }

  menu.addAction(ui->actionSearchTableCopy);
  menu.addAction(ui->actionSearchTableSelectAll);
  menu.addAction(ui->actionSearchTableSelectNothing);
  menu.addSeparator();

  menu.addAction(ui->actionSearchResetView);
  menu.addSeparator();

  if(atools::contains(tabIndex,
                      {si::SEARCH_AIRPORT, si::SEARCH_NAV, si::SEARCH_USER, si::SEARCH_ONLINE_CENTER,
                       si::SEARCH_ONLINE_CLIENT}))
    menu.addAction(ui->actionSearchSetMark);

  QAction *action = menu.exec(menuPos);

  if(action != nullptr)
    qDebug() << Q_FUNC_INFO << "selected" << action->text();
  else
    qDebug() << Q_FUNC_INFO << "no action selected";

  if(action != nullptr)
  {
    // A menu item was selected
    // Other actions with shortcuts are connected directly to methods/signals
    if(action == ui->actionSearchResetView)
      resetView();
    else if(action == ui->actionSearchTableCopy)
      tableCopyClipboard();
    else if(action == ui->actionSearchFilterIncluding)
      controller->filterIncluding(index);
    else if(action == ui->actionSearchFilterExcluding)
      controller->filterExcluding(index);
    else if(action == ui->actionSearchTableSelectAll)
      controller->selectAllRows();
    else if(action == ui->actionSearchTableSelectNothing)
      controller->selectNoRows();
    else if(action == ui->actionSearchSetMark)
      emit changeSearchMark(position);
    else if(action == ui->actionMapRangeRings)
      NavApp::getMapWidget()->addRangeRing(position);
    else if(action == ui->actionMapTrafficPattern)
      NavApp::getMapWidget()->addTrafficPattern(airport);
    else if(action == ui->actionMapHold)
    {
      map::MapResult result;
      if(navType == map::USERPOINT)
        NavApp::getMapWidget()->addHold(result, position);
      else
      {
        mapQuery->getMapObjectById(result, navType, map::AIRSPACE_SRC_NONE, id, false /* airport from nav*/);
        NavApp::getMapWidget()->addHold(result, atools::geo::EMPTY_POS);
      }
    }
    else if(action == ui->actionMapNavaidRange)
    {
      QString freqChannelStr;
      if(navType == map::VOR)
      {
        int frequency = controller->getRawData(index.row(), "frequency").toInt();
        if(frequency > 0)
        {
          // Use frequency for VOR and VORTAC
          frequency /= 10;
          freqChannelStr = QString::number(frequency);
        }
        else
          // Use channel for TACAN
          freqChannelStr = controller->getRawData(index.row(), "channel").toString();
      }
      else if(navType == map::NDB)
        freqChannelStr = controller->getRawData(index.row(), "frequency").toString();

      NavApp::getMapWidget()->addNavRangeRing(position, navType,
                                              controller->getRawData(index.row(), "ident").toString(),
                                              freqChannelStr, controller->getRawData(index.row(), "range").toInt());
    }
    // else if(action == ui->actionMapHideRangeRings)
    // NavApp::getMapWidget()->clearRangeRingsAndDistanceMarkers(); // Connected directly
    else if(action == ui->actionRouteAddPos)
      emit routeAdd(id, atools::geo::EMPTY_POS, navType, -1);
    else if(action == ui->actionRouteAppendPos)
      emit routeAdd(id, atools::geo::EMPTY_POS, navType, map::INVALID_INDEX_VALUE);
    else if(action == ui->actionSearchShowInformationAirport)
    {
      map::MapResult result;
      result.airports.append(airport);
      emit showInformation(result);
    }
    else if(action == ui->actionSearchShowOnMapAirport)
    {
      emit showRect(airport.bounding, false);
      NavApp::setStatusMessage(tr("Showing airport on map."));
    }
    else if(action == ui->actionRouteAirportStart)
      emit routeSetDeparture(airport);
    else if(action == ui->actionRouteAirportDest)
      emit routeSetDestination(airport);
    else if(action == ui->actionRouteAirportAlternate)
      emit routeAddAlternate(airport);
    else if(action == ui->actionLogdataRouteOpen)
      emit loadRouteFile(logEntry.routeFile);
    else if(action == ui->actionLogdataPerfLoad)
      emit loadPerfFile(logEntry.perfFile);
    // Logbook actions are not connected to anything - execute here =========
    else if(action == ui->actionSearchLogdataOpenPlan)
      NavApp::getLogdataController()->planOpen(&logRecord, mainWindow);
    else if(action == ui->actionSearchLogdataSavePlanAs)
      NavApp::getLogdataController()->planSaveAs(&logRecord, mainWindow);
    else if(action == ui->actionSearchLogdataOpenPerf)
      NavApp::getLogdataController()->perfOpen(&logRecord, mainWindow);
    else if(action == ui->actionSearchLogdataSavePerfAs)
      NavApp::getLogdataController()->perfSaveAs(&logRecord, mainWindow);
    else if(action == ui->actionSearchLogdataSaveGpxAs)
      NavApp::getLogdataController()->gpxSaveAs(&logRecord, mainWindow);
    // Other actions are connected
  }
}

/* Triggered by show information action in context menu. Populates map search result and emits show information */
void SearchBaseTable::showInformationTriggered()
{
  if(NavApp::getSearchController()->getCurrentSearchTabId() == tabIndex)
  {
    qDebug() << Q_FUNC_INFO;

    // Index covers a cell
    QModelIndex index = selectedOrFirstIndex();
    if(index.isValid())
    {
      map::MapTypes navType = map::NONE;
      map::MapAirspaceSources airspaceSource = map::AIRSPACE_SRC_NONE;
      int id = -1;
      getNavTypeAndId(index.row(), navType, airspaceSource, id);

      map::MapResult result;
      mapQuery->getMapObjectById(result, navType, airspaceSource, id, false /* airport from nav database */);
      emit showInformation(result);
    }
  }
}

/* Triggered by show approaches action in context menu. Populates map search result and emits show information */
void SearchBaseTable::showApproachesTriggered()
{
  showApproaches(false);
}

void SearchBaseTable::showApproachesCustomTriggered()
{
  showApproaches(true);
}

void SearchBaseTable::showApproaches(bool custom)
{
  if(NavApp::getSearchController()->getCurrentSearchTabId() == tabIndex)
  {
    qDebug() << Q_FUNC_INFO;

    // Index covers a cell
    QModelIndex index = selectedOrFirstIndex();
    if(index.isValid())
    {
      map::MapTypes navType = map::NONE;
      int id = -1;
      getNavTypeAndId(index.row(), navType, id);

      if(custom)
        emit showProceduresCustom(airportQuery->getAirportById(id));
      else
      {
        map::MapAirport airport = airportQuery->getAirportById(id);
        bool departureFilter, arrivalFilter;
        NavApp::getRouteConst().getAirportProcedureFlags(airport, -1, departureFilter, arrivalFilter);
        emit showProcedures(airport, departureFilter, arrivalFilter);
      }
    }
  }
}

/* Show on map action in context menu */
void SearchBaseTable::showOnMapTriggered()
{
  if(NavApp::getSearchController()->getCurrentSearchTabId() == tabIndex)
  {
    qDebug() << Q_FUNC_INFO;

    QModelIndex index = selectedOrFirstIndex();
    if(index.isValid())
    {
      map::MapTypes navType = map::NONE;
      map::MapAirspaceSources airspaceSource = map::AIRSPACE_SRC_NONE;
      int id = -1;
      getNavTypeAndId(index.row(), navType, airspaceSource, id);

      map::MapResult result;
      mapQuery->getMapObjectById(result, navType, airspaceSource, id, false /* airport from nav database */);

      if(!result.airports.isEmpty())
      {
        emit showRect(result.airports.first().bounding, false);
        NavApp::setStatusMessage(tr("Showing airport on map."));
      }
      else if(!result.airspaces.isEmpty())
      {
        emit showRect(result.airspaces.first().bounding, false);
        NavApp::setStatusMessage(tr("Showing airspace on map."));
      }
      else if(!result.vors.isEmpty())
      {
        emit showPos(result.vors.first().getPosition(), 0.f, false);
        NavApp::setStatusMessage(tr("Showing VOR on map."));
      }
      else if(!result.ndbs.isEmpty())
      {
        emit showPos(result.ndbs.first().getPosition(), 0.f, false);
        NavApp::setStatusMessage(tr("Showing NDB on map."));
      }
      else if(!result.waypoints.isEmpty())
      {
        emit showPos(result.waypoints.first().getPosition(), 0.f, false);
        NavApp::setStatusMessage(tr("Showing waypoint on map."));
      }
      else if(!result.userpoints.isEmpty())
      {
        emit showPos(result.userpoints.first().getPosition(), 0.f, false);
        NavApp::setStatusMessage(tr("Showing userpoint on map."));
      }
      else if(!result.logbookEntries.isEmpty())
      {
        emit showRect(result.logbookEntries.first().bounding(), false);
        NavApp::setStatusMessage(tr("Showing logbook entry on map."));
      }
      else if(!result.onlineAircraft.isEmpty())
      {
        emit showPos(result.onlineAircraft.first().getPosition(), 0.f, false);
        NavApp::setStatusMessage(tr("Showing online client/aircraft on map."));
      }
    }
  }
}

QModelIndex SearchBaseTable::selectedOrFirstIndex()
{
  QModelIndex idx = view->currentIndex();
  if(!idx.isValid())
    idx = view->model()->index(0, 0);
  return idx;
}

void SearchBaseTable::getNavTypeAndId(int row, map::MapTypes& navType, int& id)
{
  map::MapAirspaceSources airspaceSource;
  getNavTypeAndId(row, navType, airspaceSource, id);
}

/* Fetch nav type and database id from a model row */
void SearchBaseTable::getNavTypeAndId(int row, map::MapTypes& navType, map::MapAirspaceSources& airspaceSource,
                                      int& id)
{
  navType = map::NONE;
  id = -1;
  airspaceSource = map::AIRSPACE_SRC_NONE;

  if(tabIndex == si::SEARCH_AIRPORT)
  {
    // Airport table
    navType = map::AIRPORT;
    id = controller->getRawData(row, columns->getIdColumn()->getIndex()).toInt();
  }
  else if(tabIndex == si::SEARCH_NAV)
  {
    // Otherwise nav_search table
    navType = map::navTypeToMapObjectType(controller->getRawData(row, "nav_type").toString());

    if(navType == map::VOR)
      id = controller->getRawData(row, "vor_id").toInt();
    else if(navType == map::NDB)
      id = controller->getRawData(row, "ndb_id").toInt();
    else if(navType == map::WAYPOINT)
      id = controller->getRawData(row, "waypoint_id").toInt();
  }
  else if(tabIndex == si::SEARCH_USER)
  {
    // User data
    navType = map::USERPOINT;
    id = controller->getRawData(row, columns->getIdColumn()->getIndex()).toInt();
  }
  else if(tabIndex == si::SEARCH_LOG)
  {
    // Logbook
    navType = map::LOGBOOK;
    id = controller->getRawData(row, columns->getIdColumn()->getIndex()).toInt();
  }
  else if(tabIndex == si::SEARCH_ONLINE_CLIENT)
  {
    navType = map::AIRCRAFT_ONLINE;
    id = controller->getRawData(row, columns->getIdColumn()->getIndex()).toInt();
  }
  else if(tabIndex == si::SEARCH_ONLINE_CENTER)
  {
    navType = map::AIRSPACE;
    airspaceSource = map::AIRSPACE_SRC_ONLINE;
    id = controller->getRawData(row, columns->getIdColumn()->getIndex()).toInt();
  }
  else if(tabIndex == si::SEARCH_ONLINE_SERVER)
  {
    navType = map::NONE;
  }
}

void SearchBaseTable::tabDeactivated()
{
  emit selectionChanged(this, 0, controller->getVisibleRowCount(), controller->getTotalRowCount());
}

/* Callback for the controller. Will be called for each table cell and should return a formatted value */
QVariant SearchBaseTable::modelDataHandler(int colIndex, int rowIndex, const Column *col, const QVariant&,
                                           const QVariant& displayRoleValue, Qt::ItemDataRole role) const
{
  switch(role)
  {
    case Qt::DisplayRole:
      return formatModelData(col, displayRoleValue);

    case Qt::TextAlignmentRole:
      if(displayRoleValue.type() == QVariant::Int || displayRoleValue.type() == QVariant::UInt ||
         displayRoleValue.type() == QVariant::LongLong || displayRoleValue.type() == QVariant::ULongLong ||
         displayRoleValue.type() == QVariant::Double)
        // Align all numeric columns right
        return Qt::AlignRight;

      break;
    case Qt::BackgroundRole:
      if(colIndex == controller->getSortColumnIndex())
        // Use another alternating color if this is a field in the sort column
        return mapcolors::alternatingRowColor(rowIndex, true);

      break;
    default:
      break;
  }

  return QVariant();
}

/* Formats the QVariant to a QString depending on column name */
QString SearchBaseTable::formatModelData(const Column *, const QVariant& displayRoleValue) const
{
  // Called directly by the model for export functions
  if(displayRoleValue.type() == QVariant::Int || displayRoleValue.type() == QVariant::UInt)
    return QLocale().toString(displayRoleValue.toInt());
  else if(displayRoleValue.type() == QVariant::LongLong || displayRoleValue.type() == QVariant::ULongLong)
    return QLocale().toString(displayRoleValue.toLongLong());
  else if(displayRoleValue.type() == QVariant::Double)
    return QLocale().toString(displayRoleValue.toDouble());

  return displayRoleValue.toString();
}

void SearchBaseTable::selectAll()
{
  view->selectAll();
}
