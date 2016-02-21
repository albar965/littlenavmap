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

void SearchPane::addSearchWidget(const QString& field, QWidget *widget)
{
  airportColumns->assignWidget(field, widget);
}

void SearchPane::connectSearchWidgets()
{
  void (QComboBox::*activatedPtr)(int) = &QComboBox::activated;
  void (QSpinBox::*valueChangedPtr)(int) = &QSpinBox::valueChanged;

  for(const Column *col : airportColumns->getColumns())
  {
    /* *INDENT-OFF* */

    if(col->getLineEditWidget() != nullptr)
      connect(col->getLineEditWidget(), &QLineEdit::textChanged,
              [=](const QString &text) {airportController->filterByLineEdit(col, text); });
    else if(col->getComboBoxWidget() != nullptr)
      connect(col->getComboBoxWidget(), activatedPtr,
              [=](int index) {airportController->filterByComboBox(col, index, index == 0); });
    else if(col->getCheckBoxWidget() != nullptr)
      connect(col->getCheckBoxWidget(), &QCheckBox::stateChanged,
              [=](int state) {airportController->filterByCheckbox(col, state, true); });
    else if(col->getSpinBoxWidget() != nullptr)
      connect(col->getSpinBoxWidget(), valueChangedPtr,
              [=](int value) {airportController->filterBySpinBox(col, value); });
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
  connect(ui->actionResetSearch, &QAction::triggered, this, &SearchPane::resetSearch);
  connect(ui->actionResetView, &QAction::triggered, this, &SearchPane::resetView);
  connect(ui->actionTableCopy, &QAction::triggered, this, &SearchPane::tableCopyCipboard);
  connect(ui->actionShowAll, &QAction::triggered, this, &SearchPane::loadAllRowsIntoView);
}

void SearchPane::doubleClick(const QModelIndex& index)
{
  qDebug() << "double click";
  if(index.isValid())
  {

    int id = airportController->getIdForRow(index);
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

  menu.addAction(ui->actionTableCopy);
  ui->actionTableCopy->setEnabled(index.isValid());

  menu.addAction(ui->actionTableSelectAll);
  ui->actionTableSelectAll->setEnabled(airportController->getTotalRowCount() > 0);

  menu.addSeparator();
  menu.addAction(ui->actionResetView);
  menu.addAction(ui->actionResetSearch);
  menu.addAction(ui->actionShowAll);

  QString actionFilterIncludingText, actionFilterExcludingText;
  actionFilterIncludingText = ui->actionFilterIncluding->text();
  actionFilterExcludingText = ui->actionFilterExcluding->text();

  // Add data to menu item text
  ui->actionFilterIncluding->setText(ui->actionFilterIncluding->text().arg(fieldData));
  ui->actionFilterIncluding->setEnabled(index.isValid() && columnCanFilter);

  ui->actionFilterExcluding->setText(ui->actionFilterExcluding->text().arg(fieldData));
  ui->actionFilterExcluding->setEnabled(index.isValid() && columnCanFilter);

  menu.addSeparator();
  menu.addAction(ui->actionFilterIncluding);
  menu.addAction(ui->actionFilterExcluding);
  menu.addSeparator();

  QAction *a = menu.exec(QCursor::pos());
  if(a != nullptr)
  {
    // A menu item was selected
    if(a == ui->actionFilterIncluding)
      airportController->filterIncluding(index);
    else if(a == ui->actionFilterExcluding)
      airportController->filterExcluding(index);
    else if(a == ui->actionTableSelectAll)
      airportController->selectAll();
    // else if(a == ui->actionTableCopy) this is alread covered by the connected action
  }

  // Restore old menu texts
  ui->actionFilterIncluding->setText(actionFilterIncludingText);
  ui->actionFilterIncluding->setEnabled(true);

  ui->actionFilterExcluding->setText(actionFilterExcludingText);
  ui->actionFilterExcluding->setEnabled(true);

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
