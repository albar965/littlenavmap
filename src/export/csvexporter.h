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

#ifndef LITTLELOGBOOK_CSVEXPORTER_H
#define LITTLELOGBOOK_CSVEXPORTER_H

#include "export/exporter.h"

#include <QObject>

class SqlController;
class QWidget;
class QTextStream;
class QTableView;

namespace atools {
namespace sql {
class SqlExport;
}
}

/*
 * Allows to export the table content or the selected table content from the
 * given controller into CSV files.
 * Separator is ';' and quotation is '"'.
 *
 * All columns are exported in default order regardless what is shown or ordered
 * in the table view.
 */
class CsvExporter :
  public Exporter
{
  Q_DECLARE_TR_FUNCTIONS(CsvExporter)

public:
  explicit CsvExporter(QWidget *parentWidget, SqlController *controllerParam);
  virtual ~CsvExporter() override;

  /* Copies selection in table as CSV. */
  static int selectionAsCsv(QTableView *view, bool header, bool rows, QString& result,
                            const QStringList& additionalHeader = QStringList(),
                            std::function<QStringList(int)> additionalFields = nullptr,
                            std::function<QVariant(int, int)> dataCallback = nullptr);

  /* Copies full table content as CSV. */
  static int tableAsCsv(QTableView *view, bool header, QString& result,
                        const QStringList& additionalHeader = QStringList(),
                        std::function<QStringList(int)> additionalFields = nullptr);

private:
  static QString buildHeader(QTableView *view, atools::sql::SqlExport& exporter,
                             const QStringList& additionalHeader, std::function<QStringList(int)> additionalFields);

  /* Get file from save dialog */
  QString saveCsvFileDialog();

};

#endif // LITTLELOGBOOK_CSVEXPORTER_H
