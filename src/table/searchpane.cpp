/*****************************************************************************
* Copyright 2015-2016 Alexander Barthel albar965@mailbox.org
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

#include "searchpane.h"
#include "logging/loggingdefs.h"
#include <QWidget>
#include <QTableView>
#include <QHeaderView>
#include <QMenu>
#include <QLineEdit>
#include "gui/tablezoomhandler.h"
#include "sql/sqldatabase.h"
#include "table/controller.h"
#include <gui/dialog.h>
#include <gui/mainwindow.h>
#include "column.h"
#include "ui_mainwindow.h"
#include <QMessageBox>
#include "columnlist.h"
#include <geo/pos.h>

SearchPane::SearchPane(MainWindow *parent, QTableView *view, ColumnList *columns,
                       atools::sql::SqlDatabase *sqlDb)
  : QObject(parent), db(sqlDb), airportColumns(columns), tableViewAirportSearch(view), parentWidget(parent)
{
  tableViewAirportSearch->horizontalHeader()->setSectionsMovable(true);

  atools::gui::TableZoomHandler zoomHandler(tableViewAirportSearch);
  Q_UNUSED(zoomHandler);
  airportController = new Controller(parentWidget, db, columns, tableViewAirportSearch);
  airportController->prepareModel();

  connectSlots();
}

SearchPane::~SearchPane()
{

}

void SearchPane::markChanged(const atools::geo::Pos& mark)
{
  mapMark = mark;
  qDebug() << "new mark" << mark;
  if(airportColumns->getDistanceCheckBox()->isChecked() && mark.isValid())
  {
    QSpinBox *minDistanceWidget = airportColumns->getMinDistanceWidget();
    QSpinBox *maxDistanceWidget = airportColumns->getMaxDistanceWidget();
    QComboBox *distanceDirWidget = airportColumns->getDistanceDirectionWidget();

    airportController->filterByDistance(mapMark,
                                        static_cast<sqlproxymodel::SearchDirection>(distanceDirWidget->
                                                                                    currentIndex()),
                                        minDistanceWidget->value(), maxDistanceWidget->value());
    airportController->loadAllRowsForRectQuery();
  }
}

void SearchPane::connectSearchWidgets()
{
  void (QComboBox::*curIndexChangedPtr)(int) = &QComboBox::currentIndexChanged;
  void (QSpinBox::*valueChangedPtr)(int) = &QSpinBox::valueChanged;

  for(const Column *col : airportColumns->getColumns())
  {
    /* *INDENT-OFF* */
    if(col->getLineEditWidget() != nullptr)
    {
      connect(col->getLineEditWidget(), &QLineEdit::textChanged, [=](const QString &text)
      {airportController->filterByLineEdit(col, text); });

      connect(col->getLineEditWidget(), &QLineEdit::editingFinished,
              airportController, &Controller::loadAllRowsForRectQuery);
    }
    else if(col->getComboBoxWidget() != nullptr)
    {
      connect(col->getComboBoxWidget(), curIndexChangedPtr, [=](int index)
      {airportController->filterByComboBox(col, index, index == 0); });

      connect(col->getComboBoxWidget(), curIndexChangedPtr,
              airportController, &Controller::loadAllRowsForRectQuery);
    }
    else if(col->getCheckBoxWidget() != nullptr)
    {
      connect(col->getCheckBoxWidget(), &QCheckBox::stateChanged, [=](int state)
      {airportController->filterByCheckbox(col, state, col->getCheckBoxWidget()->isTristate()); });
    }
    else if(col->getSpinBoxWidget() != nullptr)
    {
      connect(col->getSpinBoxWidget(), valueChangedPtr, [=](int value)
      {airportController->filterBySpinBox(col, value); });

      connect(col->getSpinBoxWidget(), &QSpinBox::editingFinished,
              airportController, &Controller::loadAllRowsForRectQuery);
    }
    else if(col->getMinSpinBoxWidget() != nullptr && col->getMaxSpinBoxWidget() != nullptr)
    {
      connect(col->getMinSpinBoxWidget(), valueChangedPtr, [=](int value)
      {airportController->filterByMinMaxSpinBox(col, value, col->getMaxSpinBoxWidget()->value()); });

      connect(col->getMinSpinBoxWidget(), &QSpinBox::editingFinished,
              airportController, &Controller::loadAllRowsForRectQuery);

      connect(col->getMaxSpinBoxWidget(), valueChangedPtr, [=](int value)
      {airportController->filterByMinMaxSpinBox(col, col->getMinSpinBoxWidget()->value(), value); });

      connect(col->getMaxSpinBoxWidget(), &QSpinBox::editingFinished,
              airportController, &Controller::loadAllRowsForRectQuery);
    }
    /* *INDENT-ON* */
  }

  QSpinBox *minDistanceWidget = airportColumns->getMinDistanceWidget();
  QSpinBox *maxDistanceWidget = airportColumns->getMaxDistanceWidget();
  QComboBox *distanceDirWidget = airportColumns->getDistanceDirectionWidget();
  QCheckBox *distanceCheckBox = airportColumns->getDistanceCheckBox();
  QPushButton *distanceUpdate = airportColumns->getDistanceUpdateButton();

  if(minDistanceWidget != nullptr && maxDistanceWidget != nullptr &&
     distanceDirWidget != nullptr && distanceCheckBox != nullptr && distanceUpdate != nullptr)
  {
    /* *INDENT-OFF* */
    connect(distanceCheckBox, &QCheckBox::stateChanged, [=](int state)
    {
      airportController->filterByDistance(
            state == Qt::Checked ? mapMark : atools::geo::Pos(),
            static_cast<sqlproxymodel::SearchDirection>(distanceDirWidget->currentIndex()),
            minDistanceWidget->value(), maxDistanceWidget->value());

      minDistanceWidget->setEnabled(state == Qt::Checked);
      maxDistanceWidget->setEnabled(state == Qt::Checked);
      distanceDirWidget->setEnabled(state == Qt::Checked);
      distanceUpdate->setEnabled(state == Qt::Checked);
      if(state == Qt::Checked)
        airportController->loadAllRowsForRectQuery();
    });

    connect(minDistanceWidget, valueChangedPtr, [=](int value)
    {
      airportController->filterByDistanceUpdate(
            static_cast<sqlproxymodel::SearchDirection>(distanceDirWidget->currentIndex()),
            value, maxDistanceWidget->value());
      maxDistanceWidget->setMinimum(value > 10 ? value : 10);
    });

    connect(maxDistanceWidget, valueChangedPtr, [=](int value)
    {
      airportController->filterByDistanceUpdate(
            static_cast<sqlproxymodel::SearchDirection>(distanceDirWidget->currentIndex()),
            minDistanceWidget->value(), value);
      minDistanceWidget->setMaximum(value);
    });

    connect(distanceDirWidget, curIndexChangedPtr, [=](int index)
    {
      airportController->filterByDistanceUpdate(static_cast<sqlproxymodel::SearchDirection>(index),
                                                minDistanceWidget->value(),
                                                maxDistanceWidget->value());
      airportController->loadAllRowsForRectQuery();
    });

    connect(distanceUpdate, &QPushButton::clicked, airportController, &Controller::loadAllRowsForRectQuery);
    connect(minDistanceWidget, &QSpinBox::editingFinished, airportController, &Controller::loadAllRowsForRectQuery);
    connect(maxDistanceWidget, &QSpinBox::editingFinished, airportController, &Controller::loadAllRowsForRectQuery);
    /* *INDENT-ON* */
  }
}

void SearchPane::connectSlots()
{
  Ui::MainWindow *ui = parentWidget->getUi();
  connect(tableViewAirportSearch, &QTableView::doubleClicked, this,
          &SearchPane::doubleClick);
  connect(tableViewAirportSearch, &QTableView::customContextMenuRequested, this,
          &SearchPane::tableContextMenu);
  connect(ui->actionAirportSearchResetSearch, &QAction::triggered, this, &SearchPane::resetSearch);
  connect(ui->actionAirportSearchResetView, &QAction::triggered, this, &SearchPane::resetView);
  connect(ui->actionAirportSearchTableCopy, &QAction::triggered, this, &SearchPane::tableCopyCipboard);
  connect(ui->actionAirportSearchShowAll, &QAction::triggered, this, &SearchPane::loadAllRowsIntoView);
}

void SearchPane::doubleClick(const QModelIndex& index)
{
  qDebug() << "double click";

  qDebug() << "total" << airportController->getTotalRowCount() << "visible"
           << airportController->getVisibleRowCount();

  if(index.isValid())
  {
    int id = airportController->getIdForRow(index);

    atools::sql::SqlQuery query(db);
    query.prepare("select lonx, laty from airport where airport_id = :id");
    query.bindValue(":id", id);
    query.exec();
    if(query.next())
      emit showPoint(query.value("lonx").toDouble(), query.value("laty").toDouble(), 2700);
    qDebug() << "id" << id;
  }
}

void SearchPane::tableContextMenu(const QPoint& pos)
{
  Ui::MainWindow *ui = parentWidget->getUi();
  QString header, fieldData;
  bool columnCanFilter = false, columnCanGroup = false;

  QModelIndex index = airportController->getModelIndexAt(pos);
  if(index.isValid())
  {
    const Column *columnDescriptor = airportController->getColumn(index.column());
    Q_ASSERT(columnDescriptor != nullptr);
    columnCanFilter = columnDescriptor->isFilter();
    columnCanGroup = columnDescriptor->isGroup();

    if(columnCanGroup)
    {
      header = airportController->getHeaderNameAt(index);
      Q_ASSERT(!header.isNull());
      // strip LF and other from header name
      header.replace("-\n", "").replace("\n", " ");
    }

    if(columnCanFilter)
      // Disabled menu items don't need any content
      fieldData = airportController->getFieldDataAt(index);
  }
  else
    qDebug() << "Invalid index at" << pos;

  // Build the menu
  QMenu menu;

  menu.addAction(ui->actionAirportSearchSetMark);
  menu.addSeparator();

  menu.addAction(ui->actionAirportSearchTableCopy);
  ui->actionAirportSearchTableCopy->setEnabled(index.isValid());

  menu.addAction(ui->actionAirportSearchTableSelectAll);
  ui->actionAirportSearchTableSelectAll->setEnabled(airportController->getTotalRowCount() > 0);

  menu.addSeparator();
  menu.addAction(ui->actionAirportSearchResetView);
  menu.addAction(ui->actionAirportSearchResetSearch);
  menu.addAction(ui->actionAirportSearchShowAll);

  QString actionFilterIncludingText, actionFilterExcludingText;
  actionFilterIncludingText = ui->actionAirportSearchFilterIncluding->text();
  actionFilterExcludingText = ui->actionAirportSearchFilterExcluding->text();

  // Add data to menu item text
  ui->actionAirportSearchFilterIncluding->setText(ui->actionAirportSearchFilterIncluding->text().arg(
                                                    fieldData));
  ui->actionAirportSearchFilterIncluding->setEnabled(index.isValid() && columnCanFilter);

  ui->actionAirportSearchFilterExcluding->setText(ui->actionAirportSearchFilterExcluding->text().arg(
                                                    fieldData));
  ui->actionAirportSearchFilterExcluding->setEnabled(index.isValid() && columnCanFilter);

  menu.addSeparator();
  menu.addAction(ui->actionAirportSearchFilterIncluding);
  menu.addAction(ui->actionAirportSearchFilterExcluding);
  menu.addSeparator();

  QAction *a = menu.exec(QCursor::pos());
  if(a != nullptr)
  {
    // A menu item was selected
    if(a == ui->actionAirportSearchFilterIncluding)
      airportController->filterIncluding(index);
    else if(a == ui->actionAirportSearchFilterExcluding)
      airportController->filterExcluding(index);
    else if(a == ui->actionAirportSearchTableSelectAll)
      airportController->selectAll();
    // else if(a == ui->actionTableCopy) this is alread covered by the connected action
  }

  // Restore old menu texts
  ui->actionAirportSearchFilterIncluding->setText(actionFilterIncludingText);
  ui->actionAirportSearchFilterIncluding->setEnabled(true);

  ui->actionAirportSearchFilterExcluding->setText(actionFilterExcludingText);
  ui->actionAirportSearchFilterExcluding->setEnabled(true);

}

void SearchPane::preDatabaseLoad()
{
  airportController->resetSearch();
  airportController->clearModel();
}

void SearchPane::postDatabaseLoad()
{
  airportController->prepareModel();
  // connectControllerSlots();
  // assignSearchFieldsToController();

  // updateWidgetsOnSelection();
  // updateWidgetStatus();
  // updateGlobalStats();
}

void SearchPane::resetView()
{
  atools::gui::Dialog dlg(parentWidget);
  int result = dlg.showQuestionMsgBox("Actions/ShowResetView",
                                      tr("Reset sort order, column order and column sizes to default?"),
                                      tr("Do not &show this dialog again."),
                                      QMessageBox::Yes | QMessageBox::No,
                                      QMessageBox::Yes, QMessageBox::Yes);

  if(result == QMessageBox::Yes)
  {
    airportController->resetView();
    parentWidget->getUi()->statusBar->showMessage(tr("View reset to default."));
  }
}

void SearchPane::resetSearch()
{
  airportController->resetSearch();
  parentWidget->getUi()->statusBar->showMessage(tr("Search filters cleared."));
}

void SearchPane::tableCopyCipboard()
{
  // QString rows;
  int exported = 0; // csvExporter->exportSelectedToString(&rows);
  // QApplication::clipboard()->setText(rows);
  parentWidget->getUi()->statusBar->showMessage(
    QString(tr("Copied %1 logbook entries to clipboard.")).arg(exported));
}

void SearchPane::loadAllRowsIntoView()
{
  airportController->loadAllRows();
  parentWidget->getUi()->statusBar->showMessage(tr("All logbook entries read."));
}
