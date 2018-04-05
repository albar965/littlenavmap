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

#ifndef LITTLENAVMAP_PRINTSUPPORT_H
#define LITTLENAVMAP_PRINTSUPPORT_H

#include <QCoreApplication>

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

  /* Print the current map view */
  void printMap();

  /* Show dialog for options and print flight plan */
  void printFlightplan();

  void saveState();
  void restoreState();

  /* Draw program name, version and date into an image */
  static void drawWatermark(const QPoint& pos, QPainter *painter);
  static void drawWatermark(const QPoint& pos, QPixmap *pixmap);

private:
  void paintRequestedMap(QPrinter *printer);
  void paintRequestedFlightplan(QPrinter *printer);
  static void drawWatermarkInternal(const QPoint& pos, QPainter *painter);
  QPrintPreviewDialog *buildPreviewDialog(QWidget *parent);
  void deletePreviewDialog(QPrintPreviewDialog *print);
  void printPreviewFlightplanClicked();
  void printFlightplanClicked();
  void createFlightplanDocuments();
  void deleteFlightplanDocuments();
  void addHeader(QTextCursor& cursor);
  void fillWeatherCache();

  MainWindow *mainWindow;
  PrintDialog *printFlightplanDialog = nullptr;
  MapQuery *mapQuery = nullptr;

  QTextDocument *flightPlanPrintDocument = nullptr;
  QPixmap *mapScreenPrintPixmap = nullptr;

};

#endif // LITTLENAVMAP_PRINTSUPPORT_H
