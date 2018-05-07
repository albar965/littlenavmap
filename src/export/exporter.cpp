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

#include "export/exporter.h"
#include "search/sqlcontroller.h"
#include "gui/dialog.h"
#include "gui/errorhandler.h"
#include "search/column.h"

#include <QDebug>
#include <QUrl>
#include <QDesktopServices>
#include <QApplication>
#include <QSqlField>
#include <QSqlRecord>

using atools::gui::Dialog;
using atools::gui::ErrorHandler;

Exporter::Exporter(QWidget *parentWidget, SqlController *controllerObj)
  : controller(controllerObj)
{
  dialog = new Dialog(parentWidget);
  errorHandler = new ErrorHandler(parentWidget);
}

Exporter::~Exporter()
{
  delete dialog;
  delete errorHandler;
}

void Exporter::createVisualColumnIndex(int cnt, QVector<int>& visualToIndex)
{
  visualToIndex.clear();
  visualToIndex.fill(-1, cnt);

  for(int i = 0; i < cnt; i++)
  {
    int vindex = controller->getColumnVisualIndex(i);

    Q_ASSERT(vindex >= 0);

    if(controller->isColumnVisibleInView(i))
      visualToIndex[vindex] = i;
  }
}

QStringList Exporter::headerNames(int cnt, const QVector<int>& visualToIndex)
{
  QStringList columnNames;

  for(int i = 0; i < cnt; i++)
    if(visualToIndex[i] != -1)
    {
      QString cname = controller->getColumnDescriptor(visualToIndex[i])->getDisplayName();
      columnNames.append(cname.replace("-\n", "").replace("\n", " "));
    }
  return columnNames;
}

QStringList Exporter::headerNames(int cnt)
{
  QStringList columnNames;

  for(int i = 0; i < cnt; i++)
  {
    QString cname = controller->getColumnDescriptor(i)->getDisplayName();
    columnNames.append(cname.replace("-\n", "").replace("\n", " "));
  }
  return columnNames;
}

void Exporter::openDocument(const QString& file)
{
  QUrl url(QUrl::fromLocalFile(file));

  qDebug() << "HtmlExporter opening" << url;
  if(!QDesktopServices::openUrl(url))
  {
    qWarning() << "openUrl failed for" << url;
    QMessageBox::warning(parentWidget, QApplication::applicationName(),
                         QString(tr("Cannot open file \"%1\"")).arg(file),
                         QMessageBox::Close, QMessageBox::NoButton);
  }
}

void Exporter::fillRecord(const QVariantList& values, const QStringList& cols, QSqlRecord& rec)
{
  Q_ASSERT(values.size() == cols.size());

  if(rec.isEmpty())
    for(int i = 0; i < values.size(); i++)
      rec.append(QSqlField(cols.at(i), values.at(i).type()));

  for(int i = 0; i < values.size(); i++)
  {
    QVariant val = values.at(i);
    if(val.type() == QVariant::String && val.toString().isEmpty())
      rec.setNull(i);
    else
      rec.setValue(i, values.at(i));
  }
}
