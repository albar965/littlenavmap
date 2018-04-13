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

#include "print/printsupport.h"

#include "navapp.h"
#include "common/constants.h"
#include "mapgui/mapwidget.h"
#include "settings/settings.h"
#include "route/routecontroller.h"
#include "util/htmlbuilder.h"
#include "print/printdialog.h"
#include "common/htmlinfobuilder.h"
#include "options/optiondata.h"
#include "weather/weatherreporter.h"
#include "connect/connectclient.h"
#include "gui/mainwindow.h"

#include <QPainter>
#include <QtPrintSupport/QPrintPreviewDialog>
#include <QtPrintSupport/QPrinter>
#include <QStandardPaths>
#include <QDir>
#include <QDesktopServices>
#include <QTextDocument>
#include <QPrintDialog>
#include <QTextBlockFormat>
#include <QTextCursor>
#include <QTextDocumentWriter>
#include <QThread>
#include <QMainWindow>

using atools::settings::Settings;
using atools::util::HtmlBuilder;

PrintSupport::PrintSupport(MainWindow *parent)
  : mainWindow(parent), mapQuery(NavApp::getMapQuery())
{
  printFlightplanDialog = new PrintDialog(mainWindow);

  connect(printFlightplanDialog, &PrintDialog::printPreviewClicked,
          this, &PrintSupport::printPreviewFlightplanClicked);
  connect(printFlightplanDialog, &PrintDialog::printClicked,
          this, &PrintSupport::printFlightplanClicked);
}

PrintSupport::~PrintSupport()
{
  delete printFlightplanDialog;
  delete mapScreenPrintPixmap;
  delete flightPlanPrintDocument;
}

void PrintSupport::printMap()
{
  qDebug() << Q_FUNC_INFO;

  NavApp::getMapWidget()->showOverlays(false);
  delete mapScreenPrintPixmap;
  mapScreenPrintPixmap = new QPixmap(NavApp::getMapWidget()->mapScreenShot());
  NavApp::getMapWidget()->showOverlays(true);

  QPrintPreviewDialog *print = buildPreviewDialog(mainWindow);
  connect(print, &QPrintPreviewDialog::paintRequested, this, &PrintSupport::paintRequestedMap);
  print->exec();
  disconnect(print, &QPrintPreviewDialog::paintRequested, this, &PrintSupport::paintRequestedMap);
  deletePreviewDialog(print);
}

void PrintSupport::printFlightplan()
{
  qDebug() << Q_FUNC_INFO;

  fillWeatherCache();

  printFlightplanDialog->exec();
}

void PrintSupport::fillWeatherCache()
{
  qDebug() << Q_FUNC_INFO;

  const Route& route = NavApp::getRouteConst();

  if(!route.isEmpty())
  {
    map::WeatherContext currentWeatherContext;
    mainWindow->buildWeatherContext(currentWeatherContext, route.first().getAirport());
    mainWindow->buildWeatherContext(currentWeatherContext, route.last().getAirport());
  }
}

/* Open print preview dialog for flight plan. Called from PrintDialog */
void PrintSupport::printPreviewFlightplanClicked()
{
  qDebug() << Q_FUNC_INFO;

  QString cssContent;
  // QString css = atools::settings::Settings::getOverloadedPath(":/littlenavmap/resources/css/print.css");
  // QFile f(css);
  // if(f.open(QIODevice::ReadOnly | QIODevice::Text))
  // {
  // QTextStream in(&f);
  // cssContent = in.readAll();
  // }

  createFlightplanDocuments();

  QPrintPreviewDialog *print = buildPreviewDialog(printFlightplanDialog);
  connect(print, &QPrintPreviewDialog::paintRequested, this, &PrintSupport::paintRequestedFlightplan);
  print->exec();
  disconnect(print, &QPrintPreviewDialog::paintRequested, this, &PrintSupport::paintRequestedFlightplan);
  deletePreviewDialog(print);

  // QFile f2("/home/alex/Temp/lnm_route_export.html");
  // if(f2.open(QIODevice::Text | QIODevice::WriteOnly))
  // {
  // QTextStream ds(&f2);
  // ds << html.getHtml();
  // f2.close();
  // QDesktopServices::openUrl(QUrl::fromLocalFile("/home/alex/Temp/lnm_route_export.html"));
  // }

  deleteFlightplanDocuments();
}

/* Open print dialog for printing (no preview). Called from PrintDialog  */
void PrintSupport::printFlightplanClicked()
{
  qDebug() << Q_FUNC_INFO;

  QPrinter printer;
  // printer.setOutputFormat(QPrinter::NativeFormat);
  // printer.setOutputFileName("LittleNavmapFlightplan");
  QPrintDialog dialog(&printer, printFlightplanDialog);

  dialog.setWindowTitle(tr("Print Flight Plan"));
  if(dialog.exec() == QDialog::Accepted)
  {
    createFlightplanDocuments();
    paintRequestedFlightplan(&printer);
    deleteFlightplanDocuments();

    // Close settings dialog if the user printed
    printFlightplanDialog->hide();
  }
}

/* Fill the flightPlanDocument with HTML code depending on selected options */
void PrintSupport::createFlightplanDocuments()
{
  deleteFlightplanDocuments();
  flightPlanPrintDocument = new QTextDocument();

  QFont font = flightPlanPrintDocument->defaultFont();
  qDebug() << "font pixel size" << font.pixelSize() << "font point size" << font.pointSizeF();

#ifdef Q_OS_MACOS
  int printTextSize = printFlightplanDialog->getPrintTextSize() / 2;
#else
  int printTextSize = printFlightplanDialog->getPrintTextSize();
#endif

  // Adjust font size according to dialog setting
  if(font.pointSize() != -1)
    font.setPointSizeF(font.pointSizeF() * printTextSize / 100.f);
  else if(font.pixelSize() != -1)
    font.setPixelSize(font.pixelSize() * static_cast<int>(printTextSize / 100.f));
  else
    qWarning() << "Unable to set font size";

  flightPlanPrintDocument->setDefaultFont(font);

  // Create a cursor to append html and page breaks
  QTextCursor cursor(flightPlanPrintDocument);

  // Create a block format inserting page breakss
  QTextBlockFormat pageBreakBlock;
  pageBreakBlock.setPageBreakPolicy(QTextFormat::PageBreak_AlwaysBefore);

  prt::PrintFlightPlanOpts opts = printFlightplanDialog->getPrintOptions();
  atools::util::HtmlBuilder html(false);

  const Route& route = NavApp::getRouteConst();

  bool printFlightplan = opts & prt::FLIGHTPLAN;
  bool printAnyDeparture = route.hasValidDeparture() && opts & prt::DEPARTURE_ANY;
  bool printAnyDestination = route.hasValidDestination() && opts & prt::DESTINATION_ANY;

  if(printFlightplan)
  {
    // Get font size in pixels to adjust icon size
    QFontMetricsF metrics(font);

    // Print the flight plan table
    html.append(NavApp::getRouteController()->flightplanTableAsHtml(metrics.height()));

    addHeader(cursor);
    cursor.movePosition(QTextCursor::End, QTextCursor::MoveAnchor);
    cursor.insertHtml(html.getHtml());
  }

  HtmlInfoBuilder builder(mainWindow, true /*info*/, true /*print*/);

  if(printAnyDeparture || printAnyDestination)
  {
    map::WeatherContext weatherContext;

    // print start and destination information if these are airports
    if(printAnyDeparture)
    {

      // Create HTML fragments
      html.clear().table();
      HtmlBuilder departureHtml(true);
      if(opts & prt::DEPARTURE_OVERVIEW)
      {
        mainWindow->buildWeatherContext(weatherContext, route.first().getAirport());
        builder.airportText(
          route.first().getAirport(), weatherContext, departureHtml, nullptr);
      }

      HtmlBuilder departureRunway(true);
      if(opts & prt::DEPARTURE_RUNWAYS)
        builder.runwayText(route.first().getAirport(), departureRunway,
                           opts & prt::DEPARTURE_RUNWAYS_DETAIL, opts & prt::DEPARTURE_RUNWAYS_SOFT);

      HtmlBuilder departureCom(true);
      if(opts & prt::DEPARTURE_COM)
        builder.comText(route.first().getAirport(), departureCom);

      HtmlBuilder departureWeather(true);
      if(opts & prt::DEPARTURE_WEATHER)
        builder.weatherText(weatherContext, route.first().getAirport(), departureCom);

      HtmlBuilder departureAppr(true);
      if(opts & prt::DEPARTURE_APPR)
        builder.procedureText(route.first().getAirport(), departureAppr);

      // Calculate the number of table columns - need to calculate column width in percent
      int numCols = 0;
      if(!departureHtml.isEmpty() || !departureCom.isEmpty() || !departureWeather.isEmpty())
        numCols++;
      if(!departureRunway.isEmpty())
        numCols++;
      if(!departureAppr.isEmpty())
        numCols++;
      numCols = std::max(1, numCols);

      // Merge all fragments into a table
      html.tr();
      if(!departureHtml.isEmpty() || !departureCom.isEmpty() || !departureWeather.isEmpty())
        html.td(100 / numCols).append(departureHtml).append(departureCom).append(departureWeather).tdEnd();
      if(!departureRunway.isEmpty())
        html.td(100 / numCols).append(departureRunway).tdEnd();
      if(!departureAppr.isEmpty())
        html.td(100 / numCols).append(departureAppr).tdEnd();
      html.trEnd();
      html.tableEnd();

      // Add to document and add a page feed, if needed
      cursor.movePosition(QTextCursor::End, QTextCursor::MoveAnchor);
      if(printFlightplan)
        cursor.insertBlock(pageBreakBlock);
      addHeader(cursor);
      cursor.insertHtml(html.getHtml());
    }

    if(printAnyDestination)
    {
      // Create HTML fragments
      html.clear().table();
      HtmlBuilder destinationHtml(true);
      if(opts & prt::DESTINATION_OVERVIEW)
      {
        mainWindow->buildWeatherContext(weatherContext, route.last().getAirport());
        builder.airportText(
          route.last().getAirport(), weatherContext, destinationHtml, nullptr);
      }

      HtmlBuilder destinationRunway(true);
      if(opts & prt::DESTINATION_RUNWAYS)
        builder.runwayText(route.last().getAirport(), destinationRunway,
                           opts & prt::DESTINATION_RUNWAYS_DETAIL, opts & prt::DESTINATION_RUNWAYS_SOFT);

      HtmlBuilder destinationCom(true);
      if(opts & prt::DESTINATION_COM)
        builder.comText(route.last().getAirport(), destinationCom);

      HtmlBuilder destinationWeather(true);
      if(opts & prt::DESTINATION_WEATHER)
        builder.weatherText(weatherContext, route.last().getAirport(), destinationCom);

      HtmlBuilder destinationAppr(true);
      if(opts & prt::DESTINATION_APPR)
        builder.procedureText(route.last().getAirport(), destinationAppr);

      // Calculate the number of table columns - need to calculate column width in percent
      int numCols = 0;
      if(!destinationHtml.isEmpty() || !destinationCom.isEmpty() || !destinationWeather.isEmpty())
        numCols++;
      if(!destinationRunway.isEmpty())
        numCols++;
      if(!destinationAppr.isEmpty())
        numCols++;

      numCols = std::max(1, numCols);

      // Merge all fragments into a table
      html.tr();
      if(!destinationHtml.isEmpty() || !destinationCom.isEmpty() || !destinationWeather.isEmpty())
        html.td(100 / numCols).append(destinationHtml).append(destinationCom).
        append(destinationWeather).tdEnd();
      if(!destinationRunway.isEmpty())
        html.td(100 / numCols).append(destinationRunway).tdEnd();
      if(!destinationAppr.isEmpty())
        html.td(100 / numCols).append(destinationAppr).tdEnd();
      html.trEnd();
      html.tableEnd();

      // Add to document and add a page feed, if needed
      cursor.movePosition(QTextCursor::End, QTextCursor::MoveAnchor);
      if(printAnyDeparture || printFlightplan)
        cursor.insertBlock(pageBreakBlock);
      addHeader(cursor);
      cursor.insertHtml(html.getHtml());
    }
  }

  flightPlanPrintDocument->adjustSize();

  // QFile file(QStandardPaths::standardLocations(
  // QStandardPaths::TempLocation).first() + QDir::separator() + "little_navmap_print.html");
  // if(file.open(QIODevice::WriteOnly))
  // {
  // qDebug() << "Writing print to" << file.fileName();
  // QTextDocumentWriter writer(&file, "HTML");
  // writer.write(flightPlanDocument);
  // QDesktopServices::openUrl(QUrl::fromLocalFile(file.fileName()));
  // file.close();
  // }
}

/* Add header paragraph at cursor position */
void PrintSupport::addHeader(QTextCursor& cursor)
{
  atools::util::HtmlBuilder html(true);
  html.p(tr("%1 Version %2 (revision %3) on %4 ").
         arg(QApplication::applicationName()).
         arg(QApplication::applicationVersion()).
         arg(GIT_REVISION).
         arg(QLocale().toString(QDateTime::currentDateTime()))).br().br();
  cursor.insertHtml(html.getHtml());
}

void PrintSupport::deleteFlightplanDocuments()
{
  delete flightPlanPrintDocument;
  flightPlanPrintDocument = nullptr;
}

QPrintPreviewDialog *PrintSupport::buildPreviewDialog(QWidget *parent)
{
  QPrintPreviewDialog *print = new QPrintPreviewDialog(parent);
  print->setWindowFlags(print->windowFlags() & ~Qt::WindowContextHelpButtonHint);
  print->setWindowModality(Qt::ApplicationModal);

  print->resize(Settings::instance().valueVar(lnm::MAINWINDOW_PRINT_SIZE, QSize(640, 480)).toSize());
  return print;
}

void PrintSupport::deletePreviewDialog(QPrintPreviewDialog *print)
{
  if(print != nullptr)
  {
    atools::settings::Settings::instance().setValueVar(lnm::MAINWINDOW_PRINT_SIZE, print->size());
    delete print;
  }
}

void PrintSupport::paintRequestedFlightplan(QPrinter *printer)
{
  if(flightPlanPrintDocument != nullptr)
    flightPlanPrintDocument->print(printer);
}

void PrintSupport::paintRequestedMap(QPrinter *printer)
{
  if(mapScreenPrintPixmap != nullptr)
  {
    QPixmap printScreen(mapScreenPrintPixmap->scaled(printer->pageRect().size(),
                                                     Qt::KeepAspectRatio, Qt::SmoothTransformation));

    QPainter painter;
    painter.begin(printer);

    qDebug() << "font.pointSize" << painter.font().pointSize()
             << "page" << printer->pageRect() << "paper" << printer->paperRect()
             << "screen" << mapScreenPrintPixmap->rect() << "printScreen" << printScreen.rect();

    // Center image
    int x = (printer->pageRect().width() - printScreen.width()) / 2;
    int y = (printer->pageRect().height() - printScreen.height()) / 2;

    painter.drawPixmap(QPoint(x, y), printScreen);

    drawWatermark(QPoint(printer->pageRect().left(), printer->pageRect().height()), &painter);

    painter.end();
  }
}

void PrintSupport::saveState()
{
  printFlightplanDialog->saveState();
}

void PrintSupport::restoreState()
{
  printFlightplanDialog->restoreState();
}

void PrintSupport::drawWatermark(const QPoint& pos, QPixmap *pixmap)
{
  QPainter painter;
  painter.begin(pixmap);
  QFont font = painter.font();
  font.setPixelSize(9);
  painter.setFont(font);
  drawWatermarkInternal(pos, &painter);
  painter.end();
}

void PrintSupport::drawWatermark(const QPoint& pos, QPainter *painter)
{
  QFont font = painter->font();
  font.setPointSize(11);
  painter->setFont(font);
  drawWatermarkInternal(pos, painter);
}

void PrintSupport::drawWatermarkInternal(const QPoint& pos, QPainter *painter)
{
  // Draw text
  painter->setPen(Qt::black);
  painter->setBackground(QColor("#e0e0e0"));
  painter->setBackgroundMode(Qt::OpaqueMode);

  painter->drawText(pos.x(),
                    pos.y() - painter->fontMetrics().descent(),
                    tr("%1 Version %2 (revision %3) on %4 ").
                    arg(QApplication::applicationName()).
                    arg(QApplication::applicationVersion()).
                    arg(GIT_REVISION).
                    arg(QLocale().toString(QDateTime::currentDateTime())));

}
