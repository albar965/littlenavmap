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

#include "search/searchbase.h"
#include "gui/itemviewzoomhandler.h"
#include "search/sqlcontroller.h"
#include "gui/mainwindow.h"
#include "search/column.h"
#include "ui_mainwindow.h"
#include "search/columnlist.h"
#include "mapgui/mapwidget.h"
#include "atools.h"
#include "gui/actiontextsaver.h"
#include "export/csvexporter.h"
#include "mapgui/mapquery.h"
#include "options/optiondata.h"
#include "common/unit.h"

#include <QTimer>
#include <QClipboard>
#include <QKeyEvent>

/* When using distance search delay the update the table after 500 milliseconds */
const int DISTANCE_EDIT_UPDATE_TIMEOUT_MS = 500;

class ViewEventFilter :
  public QObject
{

public:
  ViewEventFilter(SearchBase *parent)
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

  SearchBase *searchBase;
};

class LineEditEventFilter :
  public QObject
{

public:
  LineEditEventFilter(SearchBase *parent)
    : QObject(parent), searchBase(parent)
  {
  }

  virtual ~LineEditEventFilter()
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
      }
    }

    return QObject::eventFilter(object, event);
  }

  SearchBase *searchBase;
};

SearchBase::SearchBase(MainWindow *parent, QTableView *tableView, ColumnList *columnList,
                       MapQuery *mapQuery, int tabWidgetIndex)
  : AbstractSearch(parent), columns(columnList), view(tableView), mainWindow(parent), query(mapQuery),
    tabIndex(tabWidgetIndex)
{
  zoomHandler = new atools::gui::ItemViewZoomHandler(view);

  Ui::MainWindow *ui = mainWindow->getUi();

  // Avoid stealing of Ctrl-C from other default menus
  ui->actionSearchTableCopy->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionSearchResetSearch->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionSearchShowAll->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionSearchShowInformation->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionSearchShowApproaches->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionSearchShowOnMap->setShortcutContext(Qt::WidgetWithChildrenShortcut);

  // Need extra action connected to catch the default Ctrl-C in the table view
  connect(ui->actionSearchTableCopy, &QAction::triggered, this, &SearchBase::tableCopyClipboard);

  // Actions that cover the whole dock window
  ui->dockWidgetSearch->addActions({ui->actionSearchResetSearch, ui->actionSearchShowAll});

  tableView->addActions({ui->actionSearchTableCopy, ui->actionSearchShowInformation, ui->actionSearchShowApproaches,
                         ui->actionSearchShowOnMap});

  // Update single shot timer
  updateTimer = new QTimer(this);
  updateTimer->setSingleShot(true);
  connect(updateTimer, &QTimer::timeout, this, &SearchBase::editTimeout);
  connect(ui->actionSearchShowInformation, &QAction::triggered, this, &SearchBase::showInformationTriggered);
  connect(ui->actionSearchShowApproaches, &QAction::triggered, this, &SearchBase::showApproachesTriggered);
  connect(ui->actionSearchShowOnMap, &QAction::triggered, this, &SearchBase::showOnMapTriggered);

  // Load text size from options
  zoomHandler->zoomPercent(OptionData::instance().getGuiSearchTableTextSize());

  viewEventFilter = new ViewEventFilter(this);
  lineEditEventFilter = new LineEditEventFilter(this);
  view->installEventFilter(viewEventFilter);
}

SearchBase::~SearchBase()
{
  delete csvExporter;
  delete updateTimer;
  delete zoomHandler;
  delete columns;
  delete viewEventFilter;
  delete lineEditEventFilter;
}

/* Copy the selected rows of the table view as CSV into clipboard */
void SearchBase::tableCopyClipboard()
{
  if(view->isVisible())
  {
    QString csv;
    int exported = CsvExporter::selectionAsCsv(view, true, csv);

    if(!csv.isEmpty())
      QApplication::clipboard()->setText(csv);

    mainWindow->setStatusMessage(QString(tr("Copied %1 entries to clipboard.")).arg(exported));
  }
}

void SearchBase::initViewAndController()
{
  view->horizontalHeader()->setSectionsMovable(true);
  view->verticalHeader()->setSectionsMovable(false);
  view->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);

  controller = new SqlController(mainWindow->getDatabase(), columns, view);
  controller->prepareModel();

  csvExporter = new CsvExporter(mainWindow, controller);
}

void SearchBase::filterByIdent(const QString& ident, const QString& region, const QString& airportIdent)
{
  controller->filterByIdent(ident, region, airportIdent);
}

void SearchBase::optionsChanged()
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

void SearchBase::updateTableSelection()
{
  tableSelectionChanged();
}

void SearchBase::searchMarkChanged(const atools::geo::Pos& mark)
{
  qDebug() << "new mark" << mark;
  if(columns->getDistanceCheckBox()->isChecked() && mark.isValid())
    updateDistanceSearch();
}

void SearchBase::updateDistanceSearch()
{
  if(columns->getDistanceCheckBox()->isChecked() &&
     mainWindow->getMapWidget()->getSearchMarkPos().isValid())
  {
    // Currently running distance search - update result
    QSpinBox *minDistanceWidget = columns->getMinDistanceWidget();
    QSpinBox *maxDistanceWidget = columns->getMaxDistanceWidget();
    QComboBox *distanceDirWidget = columns->getDistanceDirectionWidget();

    controller->filterByDistance(mainWindow->getMapWidget()->getSearchMarkPos(),
                                 static_cast<sqlproxymodel::SearchDirection>(distanceDirWidget->currentIndex()),
                                 Unit::rev(minDistanceWidget->value(), Unit::distNmF),
                                 Unit::rev(maxDistanceWidget->value(), Unit::distNmF));

    controller->loadAllRowsForDistanceSearch();
  }
}

void SearchBase::connectSearchWidgets()
{
  void (QComboBox::*curIndexChangedPtr)(int) = &QComboBox::currentIndexChanged;
  void (QSpinBox::*valueChangedPtr)(int) = &QSpinBox::valueChanged;

  // Connect all column assigned widgets to lambda
  for(const Column *col : columns->getColumns())
  {
    if(col->getLineEditWidget() != nullptr)
    {
      connect(col->getLineEditWidget(), &QLineEdit::textChanged, [ = ](const QString &text)
              {
                controller->filterByLineEdit(col, text);
                updateButtonMenu();
                editStartTimer();
              });
    }
    else if(col->getComboBoxWidget() != nullptr)
    {
      connect(col->getComboBoxWidget(), curIndexChangedPtr, [ = ](int index)
              {
                controller->filterByComboBox(col, index, index == 0);
                updateButtonMenu();
                editStartTimer();
              });
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
    connect(distanceCheckBox, &QCheckBox::stateChanged, this, &SearchBase::distanceSearchStateChanged);

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

void SearchBase::updateFromSpinBox(int value, const Column *col)
{
  if(col->getUnitConvert() != nullptr)
    // Convert widget units to internal units using the given function pointer
    controller->filterBySpinBox(
      col, atools::roundToInt(Unit::rev(value, col->getUnitConvert())));
  else
    controller->filterBySpinBox(col, value);
}

void SearchBase::updateFromMinSpinBox(int value, const Column *col)
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

void SearchBase::updateFromMaxSpinBox(int value, const Column *col)
{
  float valMin = col->getMinSpinBoxWidget()->value(), valMax = value;

  if(col->getUnitConvert() != nullptr)
  {
    // Convert widget units to internal units using the given function pointer
    valMin = atools::roundToInt(Unit::rev(valMin, col->getUnitConvert()));
    valMax = atools::roundToInt(Unit::rev(valMax, col->getUnitConvert()));
  }

  controller->filterByMinMaxSpinBox(col, atools::roundToInt(valMin),
                                    atools::roundToInt(valMax));
  col->getMinSpinBoxWidget()->setMaximum(value);

}

void SearchBase::distanceSearchStateChanged(int state)
{
  distanceSearchChanged(state == Qt::Checked, true);
}

void SearchBase::distanceSearchChanged(bool checked, bool changeViewState)
{
  QSpinBox *minDistanceWidget = columns->getMinDistanceWidget();
  QSpinBox *maxDistanceWidget = columns->getMaxDistanceWidget();
  QComboBox *distanceDirWidget = columns->getDistanceDirectionWidget();

  if(changeViewState)
    saveViewState(!checked);

  controller->filterByDistance(
    checked ? mainWindow->getMapWidget()->getSearchMarkPos() : atools::geo::Pos(),
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

void SearchBase::connectLineEdit(QLineEdit *lineEdit)
{
  connect(lineEdit, &QLineEdit::returnPressed, this, &SearchBase::showFirstEntry);
  lineEdit->installEventFilter(lineEditEventFilter);
}

/* Search criteria editing has started. Start or restart the timer for a
 * delayed update if distance search is used */
void SearchBase::editStartTimer()
{
  if(controller->isDistanceSearch())
  {
    qDebug() << "editStarted";
    updateTimer->start(DISTANCE_EDIT_UPDATE_TIMEOUT_MS);
  }
}

/* Delayed update timeout. Update result if distance search is active */
void SearchBase::editTimeout()
{
  qDebug() << "editTimeout";
  controller->loadAllRowsForDistanceSearch();
}

void SearchBase::connectSearchSlots()
{
  connect(view, &QTableView::doubleClicked, this, &SearchBase::doubleClick);
  connect(view, &QTableView::customContextMenuRequested, this, &SearchBase::contextMenu);

  Ui::MainWindow *ui = mainWindow->getUi();

  connect(ui->actionSearchShowAll, &QAction::triggered, this, &SearchBase::loadAllRowsIntoView);
  connect(ui->actionSearchResetSearch, &QAction::triggered, this, &SearchBase::resetSearch);

  reconnectSelectionModel();

  connect(controller->getSqlModel(), &SqlModel::modelReset, this, &SearchBase::reconnectSelectionModel);
  void (SearchBase::*selChangedPtr)() = &SearchBase::tableSelectionChanged;
  connect(controller->getSqlModel(), &SqlModel::fetchedMore, this, selChangedPtr);

  connect(ui->dockWidgetSearch, &QDockWidget::visibilityChanged, this, &SearchBase::dockVisibilityChanged);
}

void SearchBase::updateUnits()
{
  columns->updateUnits();
  controller->updateHeaderData();
}

/* Connect selection model again after a SQL model reset */
void SearchBase::reconnectSelectionModel()
{
  if(view->selectionModel() != nullptr)
  {
    void (SearchBase::*selChangedPtr)(const QItemSelection &selected, const QItemSelection &deselected) =
      &SearchBase::tableSelectionChanged;

    connect(view->selectionModel(), &QItemSelectionModel::selectionChanged, this, selChangedPtr);
  }
}

/* Slot for table selection changed */
void SearchBase::tableSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected)
{
  Q_UNUSED(selected);
  Q_UNUSED(deselected);

  tableSelectionChanged();
}

/* Update highlights if dock is hidden or shown (does not change for dock tab stacks) */
void SearchBase::dockVisibilityChanged(bool visible)
{
  Q_UNUSED(visible);

  tableSelectionChanged();
}

void SearchBase::tableSelectionChanged()
{
  QItemSelectionModel *sm = view->selectionModel();

  int selectedRows = 0;
  if(sm != nullptr && sm->hasSelection())
    selectedRows = sm->selectedRows().size();

  emit selectionChanged(this, selectedRows, controller->getVisibleRowCount(), controller->getTotalRowCount());
}

void SearchBase::preDatabaseLoad()
{
  saveViewState(controller->isDistanceSearch());
  controller->preDatabaseLoad();
}

void SearchBase::postDatabaseLoad()
{
  controller->postDatabaseLoad();
  restoreViewState(controller->isDistanceSearch());
}

/* Reset view sort order, column width and column order back to default values */
void SearchBase::resetView()
{
  Ui::MainWindow *ui = mainWindow->getUi();
  if(ui->tabWidgetSearch->currentIndex() == tabIndex)
  {
    controller->resetView();
    mainWindow->setStatusMessage(tr("Table view reset to defaults."));
  }
}

void SearchBase::resetSearch()
{
  Ui::MainWindow *ui = mainWindow->getUi();
  if(ui->tabWidgetSearch->currentIndex() == tabIndex)
  {
    controller->resetSearch();
    mainWindow->setStatusMessage(tr("Search filters cleared."));
  }
}

/* Loads all rows into the table view */
void SearchBase::loadAllRowsIntoView()
{
  Ui::MainWindow *ui = mainWindow->getUi();
  if(ui->tabWidgetSearch->currentIndex() == tabIndex)
  {
    controller->loadAllRows();
    mainWindow->setStatusMessage(tr("All entries read."));
  }
}

void SearchBase::showFirstEntry()
{
  showRow(0);
}

void SearchBase::showSelectedEntry()
{
  QModelIndex idx = view->currentIndex();

  if(idx.isValid())
    showRow(idx.row());
}

void SearchBase::activateView()
{
  // view->raise();
  // view->activateWindow();
  view->setFocus();
}

/* Double click into table view */
void SearchBase::doubleClick(const QModelIndex& index)
{
  if(index.isValid())
    showRow(index.row());
}

void SearchBase::showRow(int row)
{
  // Show on information panel
  maptypes::MapObjectTypes navType = maptypes::NONE;
  int id = -1;
  // get airport, VOR, NDB or waypoint id from model row
  getNavTypeAndId(row, navType, id);

  if(id > 0)
  {
    // Check if the used table has bounding rectangle columns
    bool hasBounding = columns->hasColumn("left_lonx") && columns->hasColumn("top_laty") &&
                       columns->hasColumn("right_lonx") && columns->hasColumn("bottom_laty");

    // Show on map
    if(hasBounding)
    {
      float leftLon = controller->getRawData(row, "left_lonx").toFloat();
      float topLat = controller->getRawData(row, "top_laty").toFloat();
      float rightLon = controller->getRawData(row, "right_lonx").toFloat();
      float bottomLat = controller->getRawData(row, "bottom_laty").toFloat();
      emit showRect(atools::geo::Rect(leftLon, topLat, rightLon, bottomLat), true);
    }
    else
    {
      atools::geo::Pos p(controller->getRawData(row, "lonx").toFloat(),
                         controller->getRawData(row, "laty").toFloat());
      if(p.isValid())
        emit showPos(p, 0.f, true);
    }

    maptypes::MapSearchResult result;
    query->getMapObjectById(result, navType, id);

    emit showInformation(result);
  }
}

/* Context menu in table view selected */
void SearchBase::contextMenu(const QPoint& pos)
{
  Ui::MainWindow *ui = mainWindow->getUi();

  QPoint menuPos = QCursor::pos();
  // Use widget center if position is not inside widget
  if(!view->rect().contains(view->mapFromGlobal(QCursor::pos())))
    menuPos = view->mapToGlobal(view->rect().center());

  QString fieldData = "Data";

  // Save and restore action texts on return
  atools::gui::ActionTextSaver saver({ui->actionSearchFilterIncluding, ui->actionSearchFilterExcluding,
                                      ui->actionRouteAirportDest, ui->actionRouteAirportStart,
                                      ui->actionRouteAddPos, ui->actionRouteAppendPos, ui->actionMapNavaidRange});
  Q_UNUSED(saver);

  bool columnCanFilter = false;
  atools::geo::Pos position;
  QModelIndex index = controller->getModelIndexAt(pos);
  maptypes::MapObjectTypes navType = maptypes::NONE;
  maptypes::MapAirport airport;
  int id = -1;
  if(index.isValid())
  {
    const Column *columnDescriptor = columns->getColumn(index.column());
    Q_ASSERT(columnDescriptor != nullptr);
    columnCanFilter = columnDescriptor->isFilter();

    if(columnCanFilter)
      // Disabled menu items don't need any content
      fieldData = controller->getFieldDataAt(index);

    // Get position to display range rings
    position = atools::geo::Pos(controller->getRawData(index.row(), "lonx").toFloat(),
                                controller->getRawData(index.row(), "laty").toFloat());

    // get airport, VOR, NDB or waypoint id from model row
    getNavTypeAndId(index.row(), navType, id);
    if(navType == maptypes::AIRPORT)
      query->getAirportById(airport, id);
  }
  else
    qDebug() << "Invalid index at" << pos;

  // Add data to menu item text
  ui->actionSearchFilterIncluding->setText(ui->actionSearchFilterIncluding->text().arg("\"" + fieldData + "\""));
  ui->actionSearchFilterIncluding->setEnabled(index.isValid() && columnCanFilter);

  ui->actionSearchFilterExcluding->setText(ui->actionSearchFilterExcluding->text().arg("\"" + fieldData + "\""));
  ui->actionSearchFilterExcluding->setEnabled(index.isValid() && columnCanFilter);

  ui->actionMapNavaidRange->setEnabled(navType == maptypes::VOR || navType == maptypes::NDB);

  ui->actionRouteAddPos->setEnabled(navType == maptypes::VOR || navType == maptypes::NDB ||
                                    navType == maptypes::WAYPOINT || navType == maptypes::AIRPORT);
  ui->actionRouteAppendPos->setEnabled(navType == maptypes::VOR || navType == maptypes::NDB ||
                                       navType == maptypes::WAYPOINT || navType == maptypes::AIRPORT);

  ui->actionRouteAirportDest->setEnabled(navType == maptypes::AIRPORT);
  ui->actionRouteAirportStart->setEnabled(navType == maptypes::AIRPORT);
  ui->actionSearchShowApproaches->setEnabled(navType == maptypes::AIRPORT && airport.isValid() &&
                                             (airport.flags & maptypes::AP_PROCEDURE));

  ui->actionMapRangeRings->setEnabled(index.isValid());
  ui->actionMapHideRangeRings->setEnabled(!mainWindow->getMapWidget()->getDistanceMarkers().isEmpty() ||
                                          !mainWindow->getMapWidget()->getRangeRings().isEmpty());

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

  // Build the menu
  QMenu menu;
  menu.addAction(ui->actionSearchShowInformation);
  if(navType == maptypes::AIRPORT)
    menu.addAction(ui->actionSearchShowApproaches);
  menu.addAction(ui->actionSearchShowOnMap);
  menu.addSeparator();

  menu.addAction(ui->actionSearchFilterIncluding);
  menu.addAction(ui->actionSearchFilterExcluding);
  menu.addSeparator();

  menu.addAction(ui->actionSearchResetSearch);
  menu.addAction(ui->actionSearchShowAll);
  menu.addSeparator();

  menu.addAction(ui->actionMapRangeRings);
  menu.addAction(ui->actionMapNavaidRange);
  menu.addAction(ui->actionMapHideRangeRings);
  menu.addSeparator();

  menu.addAction(ui->actionRouteAirportStart);
  menu.addAction(ui->actionRouteAirportDest);
  menu.addSeparator();

  menu.addAction(ui->actionRouteAddPos);
  menu.addAction(ui->actionRouteAppendPos);
  menu.addSeparator();

  menu.addAction(ui->actionSearchTableCopy);
  menu.addAction(ui->actionSearchTableSelectAll);
  menu.addAction(ui->actionSearchTableSelectNothing);
  menu.addSeparator();

  menu.addAction(ui->actionSearchResetView);
  menu.addSeparator();

  menu.addAction(ui->actionSearchSetMark);

  QAction *action = menu.exec(menuPos);
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
      emit changeSearchMark(controller->getGeoPos(index));
    else if(action == ui->actionMapRangeRings)
      mainWindow->getMapWidget()->addRangeRing(position);
    else if(action == ui->actionMapNavaidRange)
    {
      // Radio navaid range ring
      int frequency = controller->getRawData(index.row(), "frequency").toInt();
      if(navType == maptypes::VOR)
        // Adapt scaled frequency from nav_search table to the scale used the VOR table
        frequency /= 10;

      mainWindow->getMapWidget()->addNavRangeRing(position, navType,
                                                  controller->getRawData(index.row(), "ident").toString(),
                                                  frequency,
                                                  controller->getRawData(index.row(), "range").toInt());
    }
    else if(action == ui->actionMapHideRangeRings)
      mainWindow->getMapWidget()->clearRangeRingsAndDistanceMarkers();
    else if(action == ui->actionRouteAddPos)
      emit routeAdd(id, atools::geo::EMPTY_POS, navType, -1);
    else if(action == ui->actionRouteAppendPos)
      emit routeAdd(id, atools::geo::EMPTY_POS, navType, maptypes::INVALID_INDEX_VALUE);
    else if(action == ui->actionRouteAirportStart)
    {
      maptypes::MapAirport ap;
      query->getAirportById(ap, controller->getIdForRow(index));
      emit routeSetDeparture(ap);
    }
    else if(action == ui->actionRouteAirportDest)
    {
      maptypes::MapAirport ap;
      query->getAirportById(ap, controller->getIdForRow(index));
      emit routeSetDestination(ap);
    }
  }
}

/* Triggered by show information action in context menu. Populates map search result and emits show information */
void SearchBase::showInformationTriggered()
{
  Ui::MainWindow *ui = mainWindow->getUi();
  if(ui->tabWidgetSearch->currentIndex() == tabIndex)
  {
    // Index covers a cell
    QModelIndex index = view->currentIndex();
    if(index.isValid())
    {
      maptypes::MapObjectTypes navType = maptypes::NONE;
      int id = -1;
      getNavTypeAndId(index.row(), navType, id);

      maptypes::MapSearchResult result;
      query->getMapObjectById(result, navType, id);
      emit showInformation(result);
    }
  }
}

/* Triggered by show approaches action in context menu. Populates map search result and emits show information */
void SearchBase::showApproachesTriggered()
{
  Ui::MainWindow *ui = mainWindow->getUi();
  if(ui->tabWidgetSearch->currentIndex() == tabIndex)
  {
    // Index covers a cell
    QModelIndex index = view->currentIndex();
    if(index.isValid())
    {
      maptypes::MapObjectTypes navType = maptypes::NONE;
      int id = -1;
      getNavTypeAndId(index.row(), navType, id);
      emit showApproaches(query->getAirportById(id));
    }
  }
}

/* Show on map action in context menu */
void SearchBase::showOnMapTriggered()
{
  Ui::MainWindow *ui = mainWindow->getUi();
  if(ui->tabWidgetSearch->currentIndex() == tabIndex)
  {
    QModelIndex index = view->currentIndex();
    if(index.isValid())
    {
      maptypes::MapObjectTypes navType = maptypes::NONE;
      int id = -1;
      getNavTypeAndId(index.row(), navType, id);

      maptypes::MapSearchResult result;
      query->getMapObjectById(result, navType, id);

      if(!result.airports.isEmpty())
      {
        emit showRect(result.airports.first().bounding, false);
        mainWindow->setStatusMessage(tr("Showing airport on map."));
      }
      else
      {
        if(!result.vors.isEmpty())
          emit showPos(result.vors.first().getPosition(), 0.f, false);
        else if(!result.ndbs.isEmpty())
          emit showPos(result.ndbs.first().getPosition(), 0.f, false);
        else if(!result.waypoints.isEmpty())
          emit showPos(result.waypoints.first().getPosition(), 0.f, false);
        mainWindow->setStatusMessage(tr("Showing navaid on map."));
      }
    }
  }
}

/* Fetch nav type and database id from a model row */
void SearchBase::getNavTypeAndId(int row, maptypes::MapObjectTypes& navType, int& id)
{
  navType = maptypes::NONE;
  id = -1;

  if(columns->getTablename() == "airport")
  {
    // Airport table
    navType = maptypes::AIRPORT;
    id = controller->getRawData(row, columns->getIdColumn()->getIndex()).toInt();
  }
  else
  {
    // Otherwise nav_search table
    navType = maptypes::navTypeToMapObjectType(controller->getRawData(row, "nav_type").toString());

    if(navType == maptypes::VOR)
      id = controller->getRawData(row, "vor_id").toInt();
    else if(navType == maptypes::NDB)
      id = controller->getRawData(row, "ndb_id").toInt();
    else if(navType == maptypes::WAYPOINT)
      id = controller->getRawData(row, "waypoint_id").toInt();
  }
}
