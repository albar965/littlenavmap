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

#ifndef LITTLENAVMAP_PRINTSUPPORT_H
#define LITTLENAVMAP_PRINTSUPPORT_H

#include <QCoreApplication>
#include <QFont>

class MainWindow;
class QPrinter;
class QPixmap;
class QPainter;
class QPrintPreviewDialog;
class QTextDocument;
class PrintDialog;
class MapQuery;
class InfoQuery;
class QTextCursor;

namespace map {
struct MapAirport;

}

namespace atools {
namespace util {
class HtmlBuilder;
}
}

/*
 * Functionality for printing maps, flight plans and airport information
 */
class PrintSupport
  : public QObject
{
  Q_DECLARE_TR_FUNCTIONS(PrintSupport)

public:
  PrintSupport(MainWindow *parent);
  virtual ~PrintSupport();

  /* Open preview for the current map view */
  void printMap();

  /* Show dialog for options and print flight plan */
  void printFlightplan();

  /* Save and restore print dialog settings and size */
  void saveState();
  void restoreState();

  /* Draw program name, version and date into an image */
  static void drawWatermark(const QPoint& pos, QPainter *painter);
  static void drawWatermark(const QPoint& pos, QPixmap *pixmap);

private:
  /* Draw map */
  void paintRequestedMap(QPrinter *);

  /* Draw flight plan document into the printer */
  void paintRequestedFlightplan(QPrinter *);

  QPrintPreviewDialog *buildPreviewDialog();
  void deletePreviewDialog(QPrintPreviewDialog *print);

  /* Button box in print dialog clicked */
  void printPreviewFlightplanClicked();
  void printFlightplanClicked();

  /* Create and file the document for the flight plan print job */
  void createFlightplanDocuments();
  void deleteFlightplanDocuments();

  void addHeader(atools::util::HtmlBuilder& html);

  /* Load weather so it is available for printing */
  void fillWeatherCache();

  void buildPrinter();
  void deletePrinter();

  /* Add airport information at cursor */
  void addAirport(QTextCursor& cursor, const map::MapAirport& airport, const QString& prefix, bool departure);

  static void drawWatermarkInternal(const QPoint& pos, QPainter *painter);
  void setPrintTextSize(int percent);

  QPrinter *printer = nullptr;
  MainWindow *mainWindow;
  PrintDialog *printDialog = nullptr;
  MapQuery *mapQuery = nullptr;

  /* Document that will contain the flight plan print */
  QTextDocument *printDocument = nullptr;

  /* Default font */
  QFont printDocumentFont;

};

#endif // LITTLENAVMAP_PRINTSUPPORT_H
