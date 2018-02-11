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

#include "export/htmlexporter.h"

#include "common/constants.h"
#include "search/column.h"
#include "gui/errorhandler.h"
#include "search/sqlmodel.h"
#include "gui/dialog.h"
#include "settings/settings.h"
#include "search/sqlcontroller.h"
#include "sql/sqlquery.h"

#include <cmath>
#include <algorithm>
#include <QDebug>
#include <QUrl>
#include <QFile>
#include <QDir>
#include <QApplication>
#include <QDateTime>
#include <QXmlStreamReader>
#include <QDesktopServices>
#include <QFileInfo>
#include <QSqlRecord>
#include <QItemSelectionModel>

using atools::gui::ErrorHandler;
using atools::gui::Dialog;
using atools::sql::SqlQuery;

HtmlExporter::HtmlExporter(QWidget *parent, SqlController *controller, int rowsPerPage)
  : Exporter(parent, controller), pageSize(rowsPerPage)
{
}

HtmlExporter::~HtmlExporter()
{
}

#ifdef ENABLE_HTML_EXPORT

QString HtmlExporter::saveHtmlFileDialog()
{
  return dialog->saveFileDialog(tr("Export HTML Document"),
                                tr("HTML Documents (*.htm *.html);;All Files (*)"),
                                "html", lnm::EXPORT_FILEDIALOG);
}

bool HtmlExporter::askOverwriteDialog(const QString& basename, int totalPages)
{
  if(totalPages == 1)
    return true;

  QStringList existingFiles;

  for(int i = 1; i < std::min(totalPages, 10); i++)
  {
    QString fn = filenameForPage(basename, i);
    if(QFile::exists(fn))
    {
      if(existingFiles.isEmpty())
        existingFiles.push_back("<i>" + QDir::toNativeSeparators(fn) + "</i>");
      else
        existingFiles.push_back("<br/><i>" + QDir::toNativeSeparators(fn) + "</i>");
    }
  }
  if(existingFiles.size() == 9 && totalPages > 9)
    existingFiles.push_back("<br/><i>...</i>");

  if(!existingFiles.isEmpty())
  {
    int retval = QMessageBox::question(parentWidget, tr("Overwrite Files"),
                                       tr("<p>One or more additional files already exist.</p>"
                                            "<p>%1</p><p>Overwrite?</p>").
                                       arg(existingFiles.join("")),
                                       QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    return retval == QMessageBox::Yes;
  }
  else
    return true;
}

int HtmlExporter::exportAll(bool open)
{
  int exported = 0, totalToExport = 0, currentPage = 0, totalPages = 0, exportedPage = 0;

  QString filename = saveHtmlFileDialog();
  QString basename = QFileInfo(filename).fileName();
  qDebug() << "exportAllHtml" << filename;

  if(filename.isEmpty())
    return 0;

  // Run the current query to get all results - not only the visible
  atools::sql::SqlDatabase *db = controller->getSqlDatabase();
  SqlQuery query(db);
  query.exec(controller->getCurrentSqlQuery());
  totalToExport = controller->getTotalRowCount();
  totalPages = static_cast<int>(std::ceil(static_cast<float>(totalToExport) / static_cast<float>(pageSize)));

  if(!askOverwriteDialog(filename, totalPages))
    return exported;

  QFile file(filename);
  QXmlStreamWriter stream;
  if(!startFile(file, basename, stream, currentPage, totalPages))
    return exported;

  QVector<int> visualColumnIndex;
  while(query.next())
  {
    atools::sql::SqlRecord rec = query.record();

    if(exportedPage == 0)
    {
      // Create an index that maps the (probably reordered) columns of the
      // view to the model
      createVisualColumnIndex(rec.count(), visualColumnIndex);
      writeHtmlTableHeader(stream, headerNames(rec.count(), visualColumnIndex));
    }
    exportedPage++;

    stream.writeStartElement("tr");
    if((exportedPage % 2) == 1)
      // Use alternating color CSS class to row
      stream.writeAttribute("class", "alt");

    for(int col = 0; col < rec.count(); ++col)
    {
      // Get data formatted as shown in the table
      int physIndex = visualColumnIndex[col];
      if(physIndex != -1)
        writeHtmlTableCellFormatted(stream, rec.fieldName(physIndex), rec.value(physIndex), exportedPage);
    }
    stream.writeEndElement(); // tr

    exported++;
    if((exported % pageSize) == 0)
    {
      endFile(file, basename, stream, currentPage, totalPages);
      currentPage++;
      exportedPage = 0;

      file.setFileName(filenameForPage(filename, currentPage));
      if(!startFile(file, basename, stream, currentPage, totalPages))
        return exported;
    }
  }

  endFile(file, basename, stream, currentPage, totalPages);

  if(open)
    openDocument(filename);

  return exported;
}

int HtmlExporter::exportSelected(bool open)
{
  int exported = 0, totalToExport = 0, currentPage = 0, totalPages = 0, exportedPage = 0;

  QString filename = saveHtmlFileDialog();
  QString basename = QFileInfo(filename).fileName();
  qDebug() << "exportSelectedHtml" << filename;

  if(filename.isEmpty())
    return 0;

  const QItemSelection sel = controller->getSelection();
  for(QItemSelectionRange rng : sel)
    totalToExport += rng.height();
  totalPages = static_cast<int>(std::ceil(static_cast<float>(totalToExport) / static_cast<float>(pageSize)));

  if(!askOverwriteDialog(filename, totalPages))
    return exported;

  QFile file(filename);
  QXmlStreamWriter stream;
  if(!startFile(file, basename, stream, currentPage, totalPages))
    return exported;

  QVector<const Column *> columnList = controller->getCurrentColumns();

  QVector<int> visualColumnIndex;

  for(QItemSelectionRange rng : sel)
    for(int row = rng.top(); row <= rng.bottom(); ++row)
    {
      if(exportedPage == 0)
      {
        // Create an index that maps the (probably reordered) columns of the
        // view to the model
        createVisualColumnIndex(columnList.size(), visualColumnIndex);
        writeHtmlTableHeader(stream, headerNames(columnList.size(), visualColumnIndex));
      }
      exportedPage++;

      stream.writeStartElement("tr");
      if((exported % 2) == 1)
        // Write alternating color class
        stream.writeAttribute("class", "alt");

      QVariantList values = controller->getFormattedModelData(row);

      for(int col = 0; col < values.size(); ++col)
      {
        int physIndex = visualColumnIndex[col];
        if(physIndex != -1)
          writeHtmlTableCellRaw(stream, columnList.at(physIndex)->getColumnName(),
                                values.at(physIndex).toString(), exported);
      }
      stream.writeEndElement(); // tr

      exported++;
      if((exported % pageSize) == 0)
      {
        endFile(file, basename, stream, currentPage, totalPages);
        currentPage++;
        exportedPage = 0;

        file.setFileName(filenameForPage(filename, currentPage));
        if(!startFile(file, basename, stream, currentPage, totalPages))
          return exported;
      }
    }

  endFile(file, basename, stream, currentPage, totalPages);

  if(open)
    openDocument(filename);

  return exported;
}

bool HtmlExporter::startFile(QFile& file,
                             const QString& basename,
                             QXmlStreamWriter& stream,
                             int currentPage,
                             int totalPages)
{
  if(!file.open(QIODevice::WriteOnly | QIODevice::Text))
  {
    errorHandler->handleIOError(file);
    return false;
  }

  stream.setDevice(&file);
  stream.setAutoFormatting(true);
  stream.setAutoFormattingIndent(2);
  stream.setCodec("utf-8");

  // Get the CSS either from the resources or from the settings directory
  writeHtmlStart(stream,
                 atools::settings::Settings::getOverloadedPath(":/littlenavmap/resources/css/export.css"));
  writeHtmlHeader(stream);
  writeHtmlNav(stream, basename, currentPage, totalPages);

  stream.writeStartElement("table");
  stream.writeStartElement("tbody");
  return true;
}

void HtmlExporter::endFile(QFile& file,
                           const QString& basename,
                           QXmlStreamWriter& stream,
                           int currentPage,
                           int totalPages)
{
  stream.writeEndElement(); // tbody
  stream.writeEndElement(); // table

  writeHtmlNav(stream, basename, currentPage, totalPages);
  writeHtmlFooter(stream);
  writeHtmlEnd(stream);

  file.close();
}

void HtmlExporter::writeHtmlStart(QXmlStreamWriter& stream, const QString& cssFilename)
{
  // Read CSS into a string
  QString css;
  QFile cssFile(cssFilename);
  if(cssFile.open(QIODevice::ReadOnly))
  {
    QTextStream is(&cssFile);
    css = "\n" + is.readAll();
    cssFile.close();
  }
  else
    errorHandler->handleIOError(cssFile);

  stream.writeStartDocument();
  stream.writeStartElement("html");
  stream.writeStartElement("head");

  stream.writeStartElement("meta");
  stream.writeAttribute("http-equiv", "content-type");
  stream.writeAttribute("content", QString("text/html; charset=%1").arg("utf-8"));
  stream.writeEndElement(); // meta

  stream.writeTextElement("title", QApplication::applicationName());

  stream.writeTextElement("style", css);
  stream.writeEndElement(); // head

  stream.writeStartElement("body");
}

void HtmlExporter::writeHtmlEnd(QXmlStreamWriter& stream)
{
  stream.writeEndElement(); // body
  stream.writeEndElement(); // html
  stream.writeEndDocument();
}

void HtmlExporter::writeHtmlFooter(QXmlStreamWriter& stream)
{
  stream.writeStartElement("p");
  stream.writeAttribute("class", "footer");
  stream.writeCharacters(QString(tr("%1 Version %2 (revision %3) by Alexander Barthel. Exported on %4.")).
                         arg(QApplication::applicationName()).
                         arg(QApplication::applicationVersion()).
                         arg(GIT_REVISION).
                         arg(QDateTime::currentDateTime().toString(Qt::DefaultLocaleLongDate)));
  stream.writeEndElement(); // p
}

void HtmlExporter::writeHtmlHeader(QXmlStreamWriter& stream)
{
  stream.writeTextElement("h1", tr("FSX Logbook"));
}

QString HtmlExporter::filenameForPage(const QString& filename, int currentPage)
{
  if(currentPage == 0)
    return filename;
  else
  {
    QString retval = filename;
    if(filename.lastIndexOf(".") == -1)
      retval = filename + "_" + QString::number(currentPage);
    else
      retval.insert(filename.lastIndexOf("."), "_" + QString::number(currentPage));
    return retval;
  }
}

void HtmlExporter::writeHtmlNav(QXmlStreamWriter& stream,
                                const QString& basename,
                                int currentPage,
                                int totalPages)
{
  if(totalPages == 1)
    return;

  stream.writeStartElement("p");
  stream.writeCharacters(tr("Page %1 of %2 - ").arg(currentPage + 1).arg(totalPages));

  QString firstLink = basename;
  QString nextLink = basename;
  nextLink.insert(basename.lastIndexOf("."), "_" + QString::number(currentPage + 1));

  QString prevLink = basename;
  if(currentPage != 1)
    prevLink.insert(basename.lastIndexOf("."), "_" + QString::number(currentPage - 1));

  QString lastLink = basename;
  lastLink.insert(basename.lastIndexOf("."), "_" + QString::number(totalPages - 1));

  if(currentPage == 0)
  {
    stream.writeCharacters("First Page - Previous Page - ");
    writeHtmlLink(stream, nextLink, tr("Next Page"));
    stream.writeCharacters(" - ");
    writeHtmlLink(stream, lastLink, tr("Last Page"));
  }
  else if(currentPage == totalPages - 1)
  {
    writeHtmlLink(stream, firstLink, tr("First Page"));
    stream.writeCharacters(" - ");
    writeHtmlLink(stream, prevLink, tr("Previous Page"));
    stream.writeCharacters(" - Next Page - Last Page");
  }
  else
  {
    writeHtmlLink(stream, firstLink, tr("First Page"));
    stream.writeCharacters(" - ");
    writeHtmlLink(stream, prevLink, tr("Previous  Page"));
    stream.writeCharacters(" - ");
    writeHtmlLink(stream, nextLink, tr("Next Page"));
    stream.writeCharacters(" - ");
    writeHtmlLink(stream, lastLink, tr("Last Page"));
  }

  stream.writeEndElement(); // p
}

void HtmlExporter::writeHtmlLink(QXmlStreamWriter& stream, const QString& url, const QString& text)
{
  stream.writeStartElement("a");
  stream.writeAttribute("href", url);
  stream.writeCharacters(text);
  stream.writeEndElement(); // a
}

void HtmlExporter::writeHtmlTableHeader(QXmlStreamWriter& stream, const QStringList& names)
{
  stream.writeStartElement("tr");
  for(QString name : names)
    stream.writeTextElement("th", name.replace("\n", " "));
  stream.writeEndElement(); // tr
}

void HtmlExporter::writeHtmlTableCellFormatted(QXmlStreamWriter& stream,
                                               const QString& fieldName,
                                               QVariant value,
                                               int row)
{
  QString fmtVal = controller->formatModelData(fieldName, value);
  writeHtmlTableCellRaw(stream, fieldName, fmtVal, row);
}

void HtmlExporter::writeHtmlTableCellRaw(QXmlStreamWriter& stream,
                                         const QString& fieldName,
                                         const QString& value,
                                         int row)
{
  stream.writeStartElement("td");
  if(fieldName == controller->getSortColumn())
  {
    // Change table field background color to darker if it is the sorting
    // columns
    if((row % 2) == 1)
      stream.writeAttribute("class", "sort");
    else
      stream.writeAttribute("class", "sortalt");
  }
  stream.writeCharacters(value);
  stream.writeEndElement();
}

#endif
