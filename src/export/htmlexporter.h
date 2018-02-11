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

#ifndef LITTLELOGBOOK_HTMLEXPORTER_H
#define LITTLELOGBOOK_HTMLEXPORTER_H

#include "export/exporter.h"

#include <QObject>

class SqlController;
class QWidget;
class QXmlStreamWriter;
class QFile;

/*
 * Allows to export the table content or the selected table content from the
 * given controller into HTML files. Uses a CSS file from the resources to
 * format file.
 */
class HtmlExporter :
  public Exporter
{
public:
  HtmlExporter(QWidget *parentWidget, SqlController *controller, int rowsPerPage);
  virtual ~HtmlExporter();

  // Disabled unused export functionality since it is not compatible with other classes
#ifdef ENABLE_HTML_EXPORT
  /* Export all rows.
   *
   * @param open Open file in default browser after export.
   * @return number of rows exported.
   */
  virtual int exportAll(bool open) override;

  /* Export only selected rows.
   *
   * @param open Open file in default browser after export.
   * @return number of rows exported.
   */
  virtual int exportSelected(bool open) override;

private:
  /* Get filename from save dialog */
  QString saveHtmlFileDialog();

  /* Write head and start of body */
  void writeHtmlStart(QXmlStreamWriter& stream, const QString& cssFilename);

  /* End body and html */
  void writeHtmlEnd(QXmlStreamWriter& stream);

  /* Write heading */
  void writeHtmlHeader(QXmlStreamWriter& stream);

  /* Write table header tr and th */
  void writeHtmlTableHeader(QXmlStreamWriter& stream, const QStringList& names);

  /* Write footer (exported on, etc.) */
  void writeHtmlFooter(QXmlStreamWriter& stream);

  /* Write td as is */
  void writeHtmlTableCellRaw(QXmlStreamWriter& stream,
                             const QString& fieldName,
                             const QString& value,
                             int row);

  /* Write td and format it using the controller */
  void writeHtmlTableCellFormatted(QXmlStreamWriter& stream,
                                   const QString& fieldName,
                                   QVariant value,
                                   int row);

  /* Write next, previous, last and first page links */
  void writeHtmlNav(QXmlStreamWriter& stream, const QString& basename, int currentPage, int totalPages);

  /* Write an anchor and href */
  void writeHtmlLink(QXmlStreamWriter& stream, const QString& url, const QString& text);

  /* Close file and write all end of page footer, etc. including table end tags */
  void endFile(QFile& file,
               const QString& basename,
               QXmlStreamWriter& stream,
               int currentPage,
               int totalPages);

  /* Open file and write all HTML headers until including table begin tags */
  bool startFile(QFile& file,
                 const QString& basename,
                 QXmlStreamWriter& stream,
                 int currentPage,
                 int totalPages);

  /* Check if multiple files for paging already exist and ask user for overwrite or not */
  bool askOverwriteDialog(const QString& basename, int totalPages);

  /* Get the filename for the page including page number */
  QString filenameForPage(const QString& filename, int currentPage);

#endif

  int pageSize = 500;
};

#endif // LITTLELOGBOOK_HTMLEXPORTER_H
