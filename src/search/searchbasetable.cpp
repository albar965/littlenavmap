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

#include "search/searchbasetable.h"
#include "gui/itemviewzoomhandler.h"
#include "navapp.h"
#include "search/sqlcontroller.h"
#include "search/column.h"
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

#include <QTimer>
#include <QClipboard>
#include <QKeyEvent>

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

  virtual ~ViewEventFilter()
  {

  }

private:
  bool eventFilter(QObject *object, QEvent *event)
  {
    if(event->type() == QEvent::KeyPress)
    {
      QKeyEvent *pKeyEvent = static_cast<QKeyEvent *>(event);
      switch(pKeyEvent->key())
      {
        case Qt::Key_Return:
          searchBase->showSelectedEntry();
          return true;
      }
    }

    return QObject::eventFilter(object, event);
  }

  SearchBaseTable *searchBase;
};

class SearchWidgetEventFilter :
  public QObject
{

public:
  SearchWidgetEventFilter(SearchBaseTable *parent)
    : QObject(parent), searchBase(parent)
  {
  }

  virtual ~SearchWidgetEventFilter()
  {

  }

private:
  bool eventFilter(QObject *object, QEvent *event)
  {
    if(event->type() == QEvent::KeyPress)
    {
      QKeyEvent *pKeyEvent = static_cast<QKeyEvent *>(event);
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

    return QObject::eventFilter(object, event);
  }

  SearchBaseTable *searchBase;
};

SearchBaseTable::SearchBaseTable(QMainWindow *parent, QTableView *tableView, ColumnList *columnList,
                                 si::SearchTabIndex tabWidgetIndex)
  : AbstractSearch(parent, tabWidgetIndex), columns(columnList), view(tableView), mainWindow(parent)
{
  mapQuery = NavApp::getMapQuery();
  airportQuery = NavApp::getAirportQuerySim();

  zoomHandler = new atools::gui::ItemViewZoomHandler(view);
  connect(NavApp::navAppInstance(), &atools::gui::Application::fontChanged, this, &SearchBaseTable::fontChanged);

  Ui::MainWindow *ui = NavApp::getMainUi();

  // Avoid stealing of Ctrl-C from other default menus
  ui->actionSearchTableCopy->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionSearchResetSearch->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionSearchShowAll->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionSearchShowInformation->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionSearchShowApproaches->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionSearchShowOnMap->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionSearchTableSelectNothing->setShortcutContext(Qt::WidgetWithChildrenShortcut);

  // Need extra action connected to catch the default Ctrl-C in the table view
  connect(ui->actionSearchTableCopy, &QAction::triggered, this, &SearchBaseTable::tableCopyClipboard);

  // Actions that cover the whole dock window
  ui->dockWidgetSearch->addActions({ui->actionSearchResetSearch, ui->actionSearchShowAll});

  tableView->addActions({ui->actionSearchTableCopy, ui->actionSearchShowInformation, ui->actionSearchShowApproaches,
                         ui->actionSearchShowOnMap, ui->actionSearchTableSelectNothing});

  // Update single shot timer
  updateTimer = new QTimer(this);
  updateTimer->setSingleShot(true);
  connect(updateTimer, &QTimer::timeout, this, &SearchBaseTable::editTimeout);
  connect(ui->actionSearchShowInformation, &QAction::triggered, this, &SearchBaseTable::showInformationTriggered);
  connect(ui->actionSearchShowApproaches, &QAction::triggered, this, &SearchBaseTable::showApproachesTriggered);
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
      exported = CsvExporter::selectionAsCsv(view, true /* header */, true /* rows */, csv, {"longitude", "latitude"},
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

void SearchBaseTable::updateTableSelection()
{
  tableSelectionChanged();
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

  // Connect all column assigned widgets to lambda
  for(const Column *col : columns->getColumns())
  {
    if(col->getLineEditWidget() != nullptr)
    {
      connect(col->getLineEditWidget(), &QLineEdit::textChanged, [ = ](const QString& text)
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
        connect(col->getComboBoxWidget(), &QComboBox::editTextChanged, [ = ](const QString& text)
        {
          QComboBox *box = col->getComboBoxWidget();

          {
            QSignalBlocker blocker(box);
            Q_UNUSED(blocker);

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
        connect(col->getComboBoxWidget(), curIndexChangedPtr, [ = ](int index)
        {
          controller->filterByComboBox(col, index, index == 0);
          updateButtonMenu();
          editStartTimer();
        });
      }
    }
    else if(col->getCheckBoxWidget() != nullptr)
    {
      connect(col->getCheckBoxWidget(), &QCheckBox::stateChanged, [ = ](int state)
      {
        controller->filterByCheckbox(col, state, col->getCheckBoxWidget()->isTristate());
        updateButtonMenu();
        editStartTimer();
      });
    }
    else if(col->getSpinBoxWidget() != nullptr)
    {
      connect(col->getSpinBoxWidget(), valueChangedPtr, [ = ](int value)
      {
        updateFromSpinBox(value, col);
        updateButtonMenu();
        editStartTimer();
      });
    }
    else if(col->getMinSpinBoxWidget() != nullptr && col->getMaxSpinBoxWidget() != nullptr)
    {
      connect(col->getMinSpinBoxWidget(), valueChangedPtr, [ = ](int value)
      {
        updateFromMinSpinBox(value, col);
        updateButtonMenu();
        editStartTimer();
      });

      connect(col->getMaxSpinBoxWidget(), valueChangedPtr, [ = ](int value)
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

    connect(minDistanceWidget, valueChangedPtr, [ = ](int value)
    {
      controller->filterByDistanceUpdate(
        static_cast<sqlproxymodel::SearchDirection>(distanceDirWidget->currentIndex()),
        Unit::rev(value, Unit::distNmF),
        Unit::rev(maxDistanceWidget->value(), Unit::distNmF));

      maxDistanceWidget->setMinimum(value > 10 ? value : 10);
      updateButtonMenu();
      editStartTimer();
    });

    connect(maxDistanceWidget, valueChangedPtr, [ = ](int value)
    {
      controller->filterByDistanceUpdate(
        static_cast<sqlproxymodel::SearchDirection>(distanceDirWidget->currentIndex()),
        Unit::rev(minDistanceWidget->value(), Unit::distNmF),
        Unit::rev(value, Unit::distNmF));
      minDistanceWidget->setMaximum(value);
      updateButtonMenu();
      editStartTimer();
    });

    connect(distanceDirWidget, curIndexChangedPtr, [ = ](int index)
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
  void (SearchBaseTable::*selChangedPtr)() = &SearchBaseTable::tableSelectionChanged;
  connect(controller->getSqlModel(), &SqlModel::fetchedMore, this, selChangedPtr);

  connect(ui->dockWidgetSearch, &QDockWidget::visibilityChanged, this, &SearchBaseTable::dockVisibilityChanged);
}

void SearchBaseTable::updateUnits()
{
  columns->updateUnits();
  controller->updateHeaderData();
}

/* Connect selection model again after a SQL model reset */
void SearchBaseTable::reconnectSelectionModel()
{
  if(view->selectionModel() != nullptr)
  {
    void (SearchBaseTable::*selChangedPtr)(const QItemSelection &selected, const QItemSelection &deselected) =
      &SearchBaseTable::tableSelectionChanged;

    connect(view->selectionModel(), &QItemSelectionModel::selectionChanged, this, selChangedPtr);
  }
}

/* Slot for table selection changed */
void SearchBaseTable::tableSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected)
{
  Q_UNUSED(selected);
  Q_UNUSED(deselected);

  tableSelectionChanged();
}

/* Update highlights if dock is hidden or shown (does not change for dock tab stacks) */
void SearchBaseTable::dockVisibilityChanged(bool visible)
{
  Q_UNUSED(visible);

  tableSelectionChanged();
}

void SearchBaseTable::tableSelectionChanged()
{
  QItemSelectionModel *sm = view->selectionModel();

  int selectedRows = 0;
  if(sm != nullptr && sm->hasSelection())
    selectedRows = sm->selectedRows().size();

  updatePushButtons();

  emit selectionChanged(this, selectedRows, controller->getVisibleRowCount(), controller->getTotalRowCount());

  if(sm != nullptr &&
     sm->currentIndex().isValid() &&
     sm->isSelected(sm->currentIndex()) &&
     followModeAction() != nullptr &&
     followModeAction()->isChecked())
    emit showPos(controller->getGeoPos(sm->currentIndex()), map::INVALID_DISTANCE_VALUE, false);
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
  Ui::MainWindow *ui = NavApp::getMainUi();
  if(ui->tabWidgetSearch->currentIndex() == tabIndex)
  {
    controller->resetView();
    updatePushButtons();
    NavApp::setStatusMessage(tr("Table view reset to defaults."));
  }
}

void SearchBaseTable::refreshData(bool loadAll, bool keepSelection)
{
  controller->refreshData(loadAll, keepSelection);

  tableSelectionChanged();
}

void SearchBaseTable::refreshView()
{
  controller->refreshView();

  tableSelectionChanged();
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
  Ui::MainWindow *ui = NavApp::getMainUi();

  if(ui->tabWidgetSearch->currentIndex() == tabIndex)
  {
    controller->resetSearch();
    updatePushButtons();
    NavApp::setStatusMessage(tr("Search filters cleared."));
  }
}

/* Loads all rows into the table view */
void SearchBaseTable::loadAllRowsIntoView()
{
  Ui::MainWindow *ui = NavApp::getMainUi();
  if(ui->tabWidgetSearch->currentIndex() == tabIndex)
  {
    controller->loadAllRows();
    updatePushButtons();
    NavApp::setStatusMessage(tr("All entries read."));
  }
}

void SearchBaseTable::showFirstEntry()
{
  showRow(0);
}

void SearchBaseTable::showSelectedEntry()
{
  QModelIndex idx = view->currentIndex();

  if(idx.isValid())
    showRow(idx.row());
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
    showRow(index.row());
}

void SearchBaseTable::showRow(int row)
{
  qDebug() << Q_FUNC_INFO;

  // Show on information panel
  map::MapObjectTypes navType = map::NONE;
  int id = -1;
  // get airport, VOR, NDB or waypoint id from model row
  getNavTypeAndId(row, navType, id);

  if(id > 0 && navType != map::NONE)
  {
    // Check if the used table has bounding rectangle columns

    // Show on map
    if(columns->hasColumn("left_lonx") && columns->hasColumn("top_laty") &&
       columns->hasColumn("right_lonx") && columns->hasColumn("bottom_laty"))
    {
      // Rectangle at airports
      float leftLon = controller->getRawData(row, "left_lonx").toFloat();
      float topLat = controller->getRawData(row, "top_laty").toFloat();
      float rightLon = controller->getRawData(row, "right_lonx").toFloat();
      float bottomLat = controller->getRawData(row, "bottom_laty").toFloat();
      emit showRect(atools::geo::Rect(leftLon, topLat, rightLon, bottomLat), true);
    }
    else if(columns->hasColumn("min_lonx") && columns->hasColumn("max_laty") &&
            columns->hasColumn("max_lonx") && columns->hasColumn("min_laty"))
    {
      // Different column names for airspaces and online centers
      float leftLon = controller->getRawData(row, "min_lonx").toFloat();
      float topLat = controller->getRawData(row, "max_laty").toFloat();
      float rightLon = controller->getRawData(row, "max_lonx").toFloat();
      float bottomLat = controller->getRawData(row, "min_laty").toFloat();
      emit showRect(atools::geo::Rect(leftLon, topLat, rightLon, bottomLat), true);
    }
    else
    {
      atools::geo::Pos p(controller->getRawData(row, "lonx").toFloat(),
                         controller->getRawData(row, "laty").toFloat());
      if(p.isValid())
        emit showPos(p, 0.f, true);
    }

    map::MapSearchResult result;
    mapQuery->getMapObjectById(result, navType, id, false /* airport from nav database */);

    emit showInformation(result);
  }
}

void SearchBaseTable::nothingSelectedTriggered()
{
  controller->selectNoRows();
}

/* Context menu in table view selected */
void SearchBaseTable::contextMenu(const QPoint& pos)
{
  Ui::MainWindow *ui = NavApp::getMainUi();

  QPoint menuPos = QCursor::pos();
  // Use widget center if position is not inside widget
  if(!view->rect().contains(view->mapFromGlobal(QCursor::pos())))
    menuPos = view->mapToGlobal(view->rect().center());

  QString fieldData = "Data";

  // Save and restore action texts on return
  atools::gui::ActionTextSaver saver({ui->actionSearchFilterIncluding, ui->actionSearchFilterExcluding,
                                      ui->actionRouteAirportDest, ui->actionRouteAirportStart,
                                      ui->actionRouteAddPos, ui->actionRouteAppendPos, ui->actionMapNavaidRange,
                                      ui->actionSearchShowApproaches});
  Q_UNUSED(saver);

  // Re-enable actions on exit to allow keystrokes
  atools::gui::ActionStateSaver stateSaver(
  {
    ui->actionSearchShowInformation, ui->actionSearchShowApproaches, ui->actionSearchShowOnMap,
    ui->actionSearchFilterIncluding, ui->actionSearchFilterExcluding,
    ui->actionSearchResetSearch, ui->actionSearchShowAll,
    ui->actionMapRangeRings, ui->actionMapNavaidRange, ui->actionMapHideRangeRings,
    ui->actionRouteAirportStart, ui->actionRouteAirportDest, ui->actionRouteAddPos, ui->actionRouteAppendPos,
    ui->actionSearchTableCopy, ui->actionSearchTableSelectAll, ui->actionSearchTableSelectNothing,
    ui->actionSearchResetView, ui->actionSearchSetMark
  });
  Q_UNUSED(stateSaver);

  bool columnCanFilter = false;
  atools::geo::Pos position;
  QModelIndex index = controller->getModelIndexAt(pos);
  map::MapObjectTypes navType = map::NONE;
  map::MapAirport airport;
  int id = -1;
  if(index.isValid())
  {
    const Column *columnDescriptor = columns->getColumn(index.column());
    Q_ASSERT(columnDescriptor != nullptr);
    columnCanFilter = columnDescriptor->isFilter();

    if(columnCanFilter)
      // Disabled menu items don't need any content
      fieldData = controller->getFieldDataAt(index);

    if(controller->hasColumn("lonx") && controller->hasColumn("laty"))
      // Get position to display range rings
      position = atools::geo::Pos(controller->getRawData(index.row(), "lonx").toFloat(),
                                  controller->getRawData(index.row(), "laty").toFloat());

    // get airport, VOR, NDB or waypoint id from model row
    getNavTypeAndId(index.row(), navType, id);
    if(navType == map::AIRPORT)
      airportQuery->getAirportById(airport, id);
  }
  else
    qDebug() << "Invalid index at" << pos;

  // Add data to menu item text
  ui->actionSearchFilterIncluding->setText(ui->actionSearchFilterIncluding->text().arg("\"" + fieldData + "\""));
  ui->actionSearchFilterIncluding->setEnabled(index.isValid() && columnCanFilter);

  ui->actionSearchFilterExcluding->setText(ui->actionSearchFilterExcluding->text().arg("\"" + fieldData + "\""));
  ui->actionSearchFilterExcluding->setEnabled(index.isValid() && columnCanFilter);

  ui->actionMapNavaidRange->setEnabled(navType == map::VOR || navType == map::NDB);

  ui->actionRouteAddPos->setEnabled(navType == map::VOR || navType == map::NDB ||
                                    navType == map::WAYPOINT || navType == map::AIRPORT || navType == map::USERPOINT);
  ui->actionRouteAppendPos->setEnabled(navType == map::VOR || navType == map::NDB ||
                                       navType == map::WAYPOINT || navType == map::AIRPORT ||
                                       navType == map::USERPOINT);

  ui->actionRouteAirportDest->setEnabled(navType == map::AIRPORT);
  ui->actionRouteAirportStart->setEnabled(navType == map::AIRPORT);

  ui->actionSearchShowApproaches->setEnabled(false);
  if(navType == map::AIRPORT && airport.isValid())
  {
    bool hasAnyArrival = NavApp::getAirportQueryNav()->hasAnyArrivalProcedures(airport.ident);
    bool hasDeparture = NavApp::getAirportQueryNav()->hasDepartureProcedures(airport.ident);
    bool airportDestination = NavApp::getRouteConst().isAirportDestination(airport.ident);
    bool airportDeparture = NavApp::getRouteConst().isAirportDeparture(airport.ident);

    if(hasAnyArrival || hasDeparture)
    {
      if(airportDeparture)
      {
        if(hasDeparture)
        {
          ui->actionSearchShowApproaches->setEnabled(true);
          ui->actionSearchShowApproaches->setText(ui->actionSearchShowApproaches->text().arg(tr("Departure ")));
        }
        else
          ui->actionSearchShowApproaches->setText(tr("Show procedures (airport has no departure procedure)"));
      }
      else if(airportDestination)
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
  }
  else
    ui->actionSearchShowApproaches->setText(tr("Show procedures"));

  ui->actionMapRangeRings->setEnabled(index.isValid());
  ui->actionMapHideRangeRings->setEnabled(!NavApp::getMapWidget()->getDistanceMarkers().isEmpty() ||
                                          !NavApp::getMapWidget()->getRangeRings().isEmpty());

  ui->actionSearchSetMark->setEnabled(index.isValid());

  ui->actionMapNavaidRange->setText(tr("Show Navaid Range"));
  ui->actionRouteAddPos->setText(tr("Add to Flight Plan"));
  ui->actionRouteAppendPos->setText(tr("Append to Flight Plan"));
  ui->actionRouteAirportStart->setText(tr("Set as Flight Plan Departure"));
  ui->actionRouteAirportDest->setText(tr("Set as Flight Plan Destination"));

  ui->actionSearchTableCopy->setEnabled(index.isValid());
  ui->actionSearchTableSelectAll->setEnabled(controller->getTotalRowCount() > 0);
  ui->actionSearchTableSelectNothing->setEnabled(
    controller->getTotalRowCount() > 0 && view->selectionModel()->hasSelection());

  // Build the menu depending on tab =========================================================================
  QMenu menu;
  if(atools::contains(getTabIndex(),
                      {si::SEARCH_AIRPORT, si::SEARCH_NAV, si::SEARCH_USER, si::SEARCH_ONLINE_CENTER,
                       si::SEARCH_ONLINE_CLIENT}))
  {
    menu.addAction(ui->actionSearchShowInformation);
    if(navType == map::AIRPORT)
      menu.addAction(ui->actionSearchShowApproaches);
    menu.addAction(ui->actionSearchShowOnMap);
    menu.addSeparator();
  }

  // Add extra menu items in the user defined waypoint table - these are already connected
  if(getTabIndex() == si::SEARCH_USER)
  {
    menu.addAction(ui->actionUserdataAdd);
    menu.addAction(ui->actionUserdataEdit);
    menu.addAction(ui->actionUserdataDelete);
    menu.addSeparator();
  }

  if(atools::contains(getTabIndex(),
                      {si::SEARCH_AIRPORT, si::SEARCH_NAV, si::SEARCH_USER, si::SEARCH_ONLINE_CENTER,
                       si::SEARCH_ONLINE_CLIENT}))
  {
    menu.addAction(followModeAction());
    menu.addSeparator();

    menu.addAction(ui->actionSearchFilterIncluding);
    menu.addAction(ui->actionSearchFilterExcluding);
    menu.addSeparator();

    menu.addAction(ui->actionSearchResetSearch);
    menu.addAction(ui->actionSearchShowAll);
    menu.addSeparator();
  }

  if(atools::contains(getTabIndex(),
                      {si::SEARCH_AIRPORT, si::SEARCH_NAV, si::SEARCH_USER, si::SEARCH_ONLINE_CENTER, si::SEARCH_ONLINE_CLIENT}))
    menu.addAction(ui->actionMapRangeRings);

  if(atools::contains(getTabIndex(), {si::SEARCH_NAV}))
    menu.addAction(ui->actionMapNavaidRange);

  if(atools::contains(getTabIndex(), {si::SEARCH_AIRPORT, si::SEARCH_NAV, si::SEARCH_USER, si::SEARCH_ONLINE_CENTER, si::SEARCH_ONLINE_CLIENT}))
  {
    menu.addAction(ui->actionMapHideRangeRings);
    menu.addSeparator();
  }

  if(atools::contains(getTabIndex(), {si::SEARCH_AIRPORT}))
  {
    menu.addAction(ui->actionRouteAirportStart);
    menu.addAction(ui->actionRouteAirportDest);
    menu.addSeparator();
  }

  if(atools::contains(getTabIndex(), {si::SEARCH_AIRPORT, si::SEARCH_NAV, si::SEARCH_USER}))
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

  if(atools::contains(getTabIndex(), {si::SEARCH_AIRPORT, si::SEARCH_NAV, si::SEARCH_USER, si::SEARCH_ONLINE_CENTER, si::SEARCH_ONLINE_CLIENT}))
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
    else if(action == ui->actionMapNavaidRange)
    {
      QString freqChaStr;
      if(navType == map::VOR)
      {
        int frequency = controller->getRawData(index.row(), "frequency").toInt();
        if(frequency > 0)
        {
          // Use frequency for VOR and VORTAC
          frequency /= 10;
          freqChaStr = QString::number(frequency);
        }
        else
          // Use channel for TACAN
          freqChaStr = controller->getRawData(index.row(), "channel").toString();
      }
      else if(navType == map::NDB)
        freqChaStr = controller->getRawData(index.row(), "frequency").toString();

      NavApp::getMapWidget()->addNavRangeRing(position, navType,
                                              controller->getRawData(index.row(), "ident").toString(),
                                              freqChaStr,
                                              controller->getRawData(index.row(), "range").toInt());
    }
    else if(action == ui->actionMapHideRangeRings)
      NavApp::getMapWidget()->clearRangeRingsAndDistanceMarkers();
    else if(action == ui->actionRouteAddPos)
      emit routeAdd(id, atools::geo::EMPTY_POS, navType, -1);
    else if(action == ui->actionRouteAppendPos)
      emit routeAdd(id, atools::geo::EMPTY_POS, navType, map::INVALID_INDEX_VALUE);
    else if(action == ui->actionRouteAirportStart)
    {
      map::MapAirport ap;
      airportQuery->getAirportById(ap, controller->getIdForRow(index));
      emit routeSetDeparture(ap);
    }
    else if(action == ui->actionRouteAirportDest)
    {
      map::MapAirport ap;
      airportQuery->getAirportById(ap, controller->getIdForRow(index));
      emit routeSetDestination(ap);
    }
  }
}

/* Triggered by show information action in context menu. Populates map search result and emits show information */
void SearchBaseTable::showInformationTriggered()
{
  qDebug() << Q_FUNC_INFO;

  Ui::MainWindow *ui = NavApp::getMainUi();
  if(ui->tabWidgetSearch->currentIndex() == tabIndex)
  {
    // Index covers a cell
    QModelIndex index = view->currentIndex();
    if(index.isValid())
    {
      map::MapObjectTypes navType = map::NONE;
      int id = -1;
      getNavTypeAndId(index.row(), navType, id);

      map::MapSearchResult result;
      mapQuery->getMapObjectById(result, navType, id, false /* airport from nav database */);
      emit showInformation(result);
    }
  }
}

/* Triggered by show approaches action in context menu. Populates map search result and emits show information */
void SearchBaseTable::showApproachesTriggered()
{
  Ui::MainWindow *ui = NavApp::getMainUi();
  if(ui->tabWidgetSearch->currentIndex() == tabIndex)
  {
    // Index covers a cell
    QModelIndex index = view->currentIndex();
    if(index.isValid())
    {
      map::MapObjectTypes navType = map::NONE;
      int id = -1;
      getNavTypeAndId(index.row(), navType, id);
      emit showProcedures(airportQuery->getAirportById(id));
    }
  }
}

/* Show on map action in context menu */
void SearchBaseTable::showOnMapTriggered()
{
  Ui::MainWindow *ui = NavApp::getMainUi();
  if(ui->tabWidgetSearch->currentIndex() == tabIndex)
  {
    QModelIndex index = view->currentIndex();
    if(index.isValid())
    {
      map::MapObjectTypes navType = map::NONE;
      int id = -1;
      getNavTypeAndId(index.row(), navType, id);

      map::MapSearchResult result;
      mapQuery->getMapObjectById(result, navType, id, false /* airport from nav database */);

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
      else if(!result.onlineAircraft.isEmpty())
      {
        emit showPos(result.onlineAircraft.first().getPosition(), 0.f, false);
        NavApp::setStatusMessage(tr("Showing online client/aircraft on map."));
      }
    }
  }
}

/* Fetch nav type and database id from a model row */
void SearchBaseTable::getNavTypeAndId(int row, map::MapObjectTypes& navType, int& id)
{
  navType = map::NONE;
  id = -1;

  if(getTabIndex() == si::SEARCH_AIRPORT)
  {
    // Airport table
    navType = map::AIRPORT;
    id = controller->getRawData(row, columns->getIdColumn()->getIndex()).toInt();
  }
  else if(getTabIndex() == si::SEARCH_NAV)
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
  else if(getTabIndex() == si::SEARCH_USER)
  {
    // User data
    navType = map::USERPOINT;
    id = controller->getRawData(row, columns->getIdColumn()->getIndex()).toInt();
  }
  else if(getTabIndex() == si::SEARCH_ONLINE_CLIENT)
  {
    navType = map::AIRCRAFT_ONLINE;
    id = controller->getRawData(row, columns->getIdColumn()->getIndex()).toInt();
  }
  else if(getTabIndex() == si::SEARCH_ONLINE_CENTER)
  {
    navType = map::AIRSPACE_ONLINE;
    id = controller->getRawData(row, columns->getIdColumn()->getIndex()).toInt();
  }
  else if(getTabIndex() == si::SEARCH_ONLINE_SERVER)
  {
    navType = map::NONE;
  }
}

void SearchBaseTable::tabDeactivated()
{
  emit selectionChanged(this, 0, controller->getVisibleRowCount(), controller->getTotalRowCount());
}

/* Callback for the controller. Will be called for each table cell and should return a formatted value */
QVariant SearchBaseTable::modelDataHandler(int colIndex, int rowIndex, const Column *col, const QVariant& roleValue,
                                           const QVariant& displayRoleValue, Qt::ItemDataRole role) const
{
  Q_UNUSED(roleValue);

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
QString SearchBaseTable::formatModelData(const Column *col, const QVariant& displayRoleValue) const
{
  Q_UNUSED(col);

  // Called directly by the model for export functions
  if(displayRoleValue.type() == QVariant::Int || displayRoleValue.type() == QVariant::UInt)
    return QLocale().toString(displayRoleValue.toInt());
  else if(displayRoleValue.type() == QVariant::LongLong || displayRoleValue.type() == QVariant::ULongLong)
    return QLocale().toString(displayRoleValue.toLongLong());
  else if(displayRoleValue.type() == QVariant::Double)
    return QLocale().toString(displayRoleValue.toDouble());

  return displayRoleValue.toString();
}
