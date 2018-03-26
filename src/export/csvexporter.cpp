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

#include "export/csvexporter.h"

#include "common/constants.h"
#include "gui/errorhandler.h"
#include "search/sqlmodel.h"
#include "gui/dialog.h"
#include "sql/sqlexport.h"
#include "search/sqlcontroller.h"

#include "sql/sqlrecord.h"

#include <QDebug>
#include <QFile>
#include <QTextCodec>
#include <QIODevice>
#include <QHeaderView>
#include <QTableView>

using atools::gui::ErrorHandler;
using atools::gui::Dialog;
using atools::sql::SqlQuery;
using atools::sql::SqlExport;

CsvExporter::CsvExporter(QWidget *parent, SqlController *controller) :
  Exporter(parent, controller)
{
}

CsvExporter::~CsvExporter()
{
}

QString CsvExporter::saveCsvFileDialog()
{
  return dialog->saveFileDialog(tr("Export CSV Document"),
                                tr("CSV Documents (*.csv);;All Files (*)"),
                                "csv", lnm::EXPORT_FILEDIALOG);
}

#ifdef ENABLE_CSV_EXPORT
int CsvExporter::exportAll(bool open)
{
  int exported = 0;
  QString filename = saveCsvFileDialog();

  if(!filename.isEmpty())
  {
    qDebug() << "exportAllCsv" << filename;
    QFile file(filename);

    if(file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
      QTextStream stream(&file);
      qDebug() << "Used codec" << stream.codec()->name();

      // Run the current query to get all results - not only the visible
      atools::sql::SqlDatabase *db = controller->getSqlDatabase();
      SqlQuery query(db);
      query.exec(controller->getCurrentSqlQuery());

      SqlExport sqlExport;
      sqlExport.setSeparatorChar(';');
      QVariantList values;

      int row = 0;
      while(query.next())
      {
        atools::sql::SqlRecord rec = query.record();
        if(row == 0)
          stream << sqlExport.getResultSetHeader(headerNames(rec.count()));

        // Write all columns
        values.clear();
        for(int col = 0; col < rec.count(); ++col)
          // Get data formatted as shown in the table
          values.append(controller->formatModelData(rec.fieldName(col), rec.value(col)));
        stream << sqlExport.getResultSetRow(values);
        row++;
        exported++;
      }

      stream.flush();
      file.close();

      if(open)
        openDocument(filename);
    }
    else
      errorHandler->handleIOError(file);
  }
  return exported;
}

int CsvExporter::exportSelected(bool open)
{
  int exported = 0;
  QString filename = saveCsvFileDialog();

  if(!filename.isEmpty())
  {
    qDebug() << "exportSelectedCsv" << filename;

    QFile file(filename);
    if(file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
      // Get the view selection
      const QItemSelection sel = controller->getSelection();
      QTextStream stream(&file);
      qDebug() << "Used codec" << stream.codec()->name();

      SqlExport sqlExport;
      sqlExport.setSeparatorChar(';');

      QVector<const Column *> columnList = controller->getCurrentColumns();

      stream << sqlExport.getResultSetHeader(headerNames(columnList.size()));

      for(QItemSelectionRange rng : sel)
        for(int row = rng.top(); row <= rng.bottom(); ++row)
        {
          // Export all fields
          stream << sqlExport.getResultSetRow(controller->getFormattedModelData(row));
          exported++;
        }

      stream.flush();
      file.close();

      if(open)
        openDocument(filename);
    }
    else
      errorHandler->handleIOError(file);
  }
  return exported;
}

#endif

int CsvExporter::selectionAsCsv(QTableView *view, bool header, bool rows, QString& result,
                                const QStringList& additionalHeader,
                                std::function<QStringList(int index)> additionalFields)
{
  int exported = 0;
  atools::sql::SqlExport exporter;
  exporter.setSeparatorChar(';');
  exporter.setEndline(false);

  QAbstractItemModel *model = view->model();

  QItemSelectionModel *selection = view->selectionModel();
  if(selection != nullptr && selection->hasSelection())
  {
    if(rows)
    {
      // Copy full rows
      QTextStream stream(&result, QIODevice::WriteOnly);
      QHeaderView *headerView = view->horizontalHeader();
      if(header)
      {
        // Build CSV header
        QStringList headers;
        for(int i = 0; i < model->columnCount(); i++)
          if(!view->isColumnHidden(i))
            headers.append(model->headerData(headerView->logicalIndex(i), Qt::Horizontal).
                           toString().replace("-\n", "").replace("\n", " "));

        stream << exporter.getResultSetHeader(headers) +
        (additionalHeader.isEmpty() || !additionalFields ? QString() : ";" + additionalHeader.join(";")) << endl;
      }

      for(QItemSelectionRange rng : selection->selection())
      {
        // Add data
        for(int row = rng.top(); row <= rng.bottom(); ++row)
        {
          QVariantList vars;
          for(int i = 0; i < model->columnCount(); i++)
            if(!view->isColumnHidden(i))
              vars.append(model->data(model->index(row, headerView->logicalIndex(i))));
          stream << exporter.getResultSetRow(vars) +
          (additionalHeader.isEmpty() || !additionalFields ? QString() : ";" + additionalFields(row).join(";")) << endl;

          exported++;
        }
      }
      stream.flush();
    }
    else
    {
      // Copy selected fields only
      QStringList resultList;
      QModelIndexList indexes = selection->selectedIndexes();
      for(const QModelIndex& index : indexes)
      {
        resultList.append(model->data(index).toString());
        exported++;
      }
      result = resultList.join(";");
    }
  }
  return exported;
}
