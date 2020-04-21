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

#include "print/printsupport.h"

#include "navapp.h"
#include "common/constants.h"
#include "mapgui/mapwidget.h"
#include "common/mapcolors.h"
#include "settings/settings.h"
#include "route/routecontroller.h"
#include "perf/aircraftperfcontroller.h"
#include "util/htmlbuilder.h"
#include "print/printdialog.h"
#include "common/htmlinfobuilder.h"
#include "weather/weatherreporter.h"
#include "gui/mainwindow.h"

#include <QPainter>
#include <QtPrintSupport/QPrintPreviewDialog>
#include <QtPrintSupport/QPrinter>
#include <QTextDocument>
#include <QPrintDialog>
#include <QTextCursor>
#include <QBitArray>

using atools::settings::Settings;
using atools::util::HtmlBuilder;

PrintSupport::PrintSupport(MainWindow *parent)
  : mainWindow(parent), mapQuery(NavApp::getMapQuery())
{
  printDialog = new PrintDialog(mainWindow);

  connect(printDialog, &PrintDialog::printPreviewClicked, this, &PrintSupport::printPreviewFlightplanClicked);
  connect(printDialog, &PrintDialog::printClicked, this, &PrintSupport::printFlightplanClicked);
}

PrintSupport::~PrintSupport()
{
  delete printDialog;
  delete printer;
  delete printDocument;
}

void PrintSupport::printMap()
{
  qDebug() << Q_FUNC_INFO;

  buildPrinter();

  NavApp::getMapWidget()->showOverlays(false, true /* show scale */);
  QPrintPreviewDialog *print = buildPreviewDialog();
  connect(print, &QPrintPreviewDialog::paintRequested, this, &PrintSupport::paintRequestedMap);
  print->exec();
  disconnect(print, &QPrintPreviewDialog::paintRequested, this, &PrintSupport::paintRequestedMap);

  NavApp::getMapWidget()->showOverlays(true, true /* show scale */);
  deletePreviewDialog(print);
}

void PrintSupport::printFlightplan()
{
  qDebug() << Q_FUNC_INFO;

  // Prime weather cache
  fillWeatherCache();

  printDialog->exec();
}

void PrintSupport::fillWeatherCache()
{
  qDebug() << Q_FUNC_INFO;

  const Route& route = NavApp::getRouteConst();

  if(!route.isEmpty())
  {
    map::WeatherContext currentWeatherContext;
    mainWindow->buildWeatherContext(currentWeatherContext, route.getDepartureAirportLeg().getAirport());
    mainWindow->buildWeatherContext(currentWeatherContext, route.getDestinationAirportLeg().getAirport());
  }
}

/* Open print preview dialog for flight plan. Called from PrintDialog */
void PrintSupport::printPreviewFlightplanClicked()
{
  qDebug() << Q_FUNC_INFO;

  // Create and fill document to print
  createFlightplanDocuments();

  QPrintPreviewDialog *print = buildPreviewDialog();
  connect(print, &QPrintPreviewDialog::paintRequested, this, &PrintSupport::paintRequestedFlightplan);
  print->exec();
  disconnect(print, &QPrintPreviewDialog::paintRequested, this, &PrintSupport::paintRequestedFlightplan);
  deletePreviewDialog(print);
  deleteFlightplanDocuments();
}

/* Open print dialog for printing (no preview). Called from PrintDialog  */
void PrintSupport::printFlightplanClicked()
{
  qDebug() << Q_FUNC_INFO;

  QPrintDialog dialog(printer, printDialog);

  dialog.setWindowTitle(tr("Print Flight Plan"));
  if(dialog.exec() == QDialog::Accepted)
  {
    createFlightplanDocuments();
    paintRequestedFlightplan(printer);
    deleteFlightplanDocuments();

    // Close settings dialog if the user printed
    printDialog->hide();
  }
}

void PrintSupport::setPrintTextSize(int percent)
{
  if(printDocument == nullptr)
  {
    qWarning() << Q_FUNC_INFO << "printDocument==nullptr";
    return;
  }

  QFont font = printDocumentFont;
  // Adjust font size according to dialog setting
  if(font.pointSize() != -1)
    font.setPointSizeF(font.pointSizeF() * percent / 100.f);
  else if(font.pixelSize() != -1)
    font.setPixelSize(font.pixelSize() * static_cast<int>(percent / 100.f));
  else
    qWarning() << "Unable to set font size";

  printDocument->setDefaultFont(font);
}

/* Fill the flightPlanDocument with HTML code depending on selected options */
void PrintSupport::createFlightplanDocuments()
{
  deleteFlightplanDocuments();
  printDocument = new QTextDocument();
  printDocumentFont = printDocument->defaultFont();

  // Create a cursor to append html, text table and page breaks =================
  QTextCursor cursor(printDocument);

  // Create a block format inserting page breaks
  QTextBlockFormat pageBreakBlock;
  pageBreakBlock.setPageBreakPolicy(QTextFormat::PageBreak_AlwaysBefore);

  prt::PrintFlightPlanOpts opts = printDialog->getPrintOptions();
  const Route& route = NavApp::getRouteConst();
  bool newPage = opts & prt::NEW_PAGE;

  // Header =========================================================================
  HtmlBuilder html(mapcolors::mapPrintRowColor, mapcolors::mapPrintRowColorAlt);
  // Header line with program version
  addHeader(html);
  NavApp::getRouteController()->flightplanHeader(html, !(opts & prt::HEADER));

  if(opts & prt::HEADER)
  {
    html.p().b(tr("Flight Plan File:")).nbsp().nbsp().
    small(NavApp::getRouteController()->getCurrentRouteFilepath()).pEnd();
  }

  cursor.insertHtml(html.getHtml());
  if(newPage)
    cursor.insertBlock(pageBreakBlock);

  // Performance  =========================================================================
  if(opts & prt::FUEL_REPORT)
  {
    html.clear();
    if(!newPage)
      // Add a line to separate if page breaks are off
      html.hr();
    NavApp::getAircraftPerfController()->fuelReport(html, true /* print */);
    NavApp::getAircraftPerfController()->fuelReportFilepath(html, true /* print */);
    cursor.insertHtml(html.getHtml());
    if(newPage)
      cursor.insertBlock(pageBreakBlock);
  }

  // Flight plan  =========================================================================
  if(opts & prt::FLIGHTPLAN)
  {
    html.clear();
    if(!newPage)
      // Add a line to separate if page breaks are off
      html.hr();
    html.h2(tr("Flight Plan"));
    cursor.insertHtml(html.getHtml());

    float fontPointSize = static_cast<float>(printDocumentFont.pointSizeF()) *
                          printDialog->getPrintTextSizeFlightplan() / 100.f;
    NavApp::getRouteController()->flightplanTableAsTextTable(cursor, printDialog->getSelectedRouteTableColumns(),
                                                             fontPointSize);
    if(newPage)
      cursor.insertBlock(pageBreakBlock);
  }

  setPrintTextSize(printDialog->getPrintTextSize());

  // Start and destination  =========================================================================
  // print start and destination information if these are airports
  if(route.hasValidDeparture() && opts & prt::DEPARTURE_ANY)
    addAirport(cursor, route.getDepartureAirportLeg().getAirport(), tr("Departure"), true /* departure */);
  if(route.hasValidDestination() && opts & prt::DESTINATION_ANY)
    addAirport(cursor, route.getDestinationAirportLeg().getAirport(), tr("Destination"), false /* destination */);

  printDocument->adjustSize();
}

void PrintSupport::addAirport(QTextCursor& cursor, const map::MapAirport& airport, const QString& prefix,
                              bool departure)
{
  // Create a block format inserting page breaks
  QTextBlockFormat pageBreakBlock;
  pageBreakBlock.setPageBreakPolicy(QTextFormat::PageBreak_AlwaysBefore);

  HtmlInfoBuilder builder(mainWindow, true /*info*/, true /*print*/);
  map::WeatherContext weatherContext;

  prt::PrintFlightPlanOpts opts = printDialog->getPrintOptions();
  bool newPage = opts & prt::NEW_PAGE;

  HtmlBuilder html(mapcolors::mapPrintRowColor, mapcolors::mapPrintRowColorAlt);
  if(departure ? (opts& prt::DEPARTURE_OVERVIEW) : (opts & prt::DESTINATION_OVERVIEW))
  {
    mainWindow->buildWeatherContext(weatherContext, airport);
    if(!newPage)
      html.hr();
    html.h2(tr("%1 Airport").arg(prefix));
    builder.airportText(airport, weatherContext, html, nullptr);
    cursor.insertHtml(html.getHtml());
    if(newPage)
      cursor.insertBlock(pageBreakBlock);
  }

  if(departure ? (opts& prt::DEPARTURE_RUNWAYS) : (opts & prt::DESTINATION_RUNWAYS))
  {
    html.clear();
    if(!newPage)
      html.hr();
    html.h3(tr("%1 Airport Runways").arg(prefix));
    builder.runwayText(airport, html,
                       departure ? (opts& prt::DEPARTURE_RUNWAYS_DETAIL) : (opts & prt::DESTINATION_RUNWAYS_DETAIL),
                       departure ? (opts& prt::DEPARTURE_RUNWAYS_SOFT) : (opts & prt::DESTINATION_RUNWAYS_SOFT));
    cursor.insertHtml(html.getHtml());
    if(newPage)
      cursor.insertBlock(pageBreakBlock);
  }

  if(departure ? (opts& prt::DEPARTURE_COM) : (opts & prt::DESTINATION_COM))
  {
    html.clear();
    if(!newPage)
      html.hr();
    html.h3(tr("%1 Airport COM Frequencies").arg(prefix));
    builder.comText(airport, html);
    cursor.insertHtml(html.getHtml());
    if(newPage)
      cursor.insertBlock(pageBreakBlock);
  }

  if(departure ? (opts& prt::DEPARTURE_WEATHER) : (opts & prt::DESTINATION_WEATHER))
  {
    html.clear();
    if(!newPage)
      html.hr();
    html.h3(tr("%1 Airport Weather").arg(prefix));
    builder.weatherText(weatherContext, airport, html);
    cursor.insertHtml(html.getHtml());
    if(newPage)
      cursor.insertBlock(pageBreakBlock);
  }

  if(departure ? (opts& prt::DEPARTURE_APPR) : (opts & prt::DESTINATION_APPR))
  {
    html.clear();
    if(!newPage)
      html.hr();
    html.h3(tr("%1 Airport Procedures").arg(prefix));
    builder.procedureText(airport, html);
    cursor.insertHtml(html.getHtml());
    if(newPage)
      cursor.insertBlock(pageBreakBlock);
  }
}

/* Add header paragraph at cursor position */
void PrintSupport::addHeader(atools::util::HtmlBuilder& html)
{
  html.p().small(tr("%1 Version %2 (revision %3) on %4 ").
                 arg(QApplication::applicationName()).
                 arg(QApplication::applicationVersion()).
                 arg(GIT_REVISION).
                 arg(QLocale().toString(QDateTime::currentDateTime()))).pEnd();
}

void PrintSupport::deleteFlightplanDocuments()
{
  delete printDocument;
  printDocument = nullptr;
}

void PrintSupport::deletePrinter()
{
  delete printer;
  printer = nullptr;
}

void PrintSupport::buildPrinter()
{
  if(printer == nullptr)
  {
    printer = new QPrinter(QPrinter::HighResolution);
    printer->setOrientation(QPrinter::Landscape);

    qDebug() << Q_FUNC_INFO << "printer resolution" << printer->resolution();
  }
}

QPrintPreviewDialog *PrintSupport::buildPreviewDialog()
{
  buildPrinter();
  QPrintPreviewDialog *print = new QPrintPreviewDialog(printer, mainWindow);
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

void PrintSupport::paintRequestedFlightplan(QPrinter *)
{
  if(printDocument != nullptr)
    printDocument->print(printer);
}

void PrintSupport::paintRequestedMap(QPrinter *)
{
  QGuiApplication::setOverrideCursor(Qt::WaitCursor);

  MapWidget *mapWidget = NavApp::getMapWidget();
  QPainter painter;

  // Calculate best ratio
  double xscale = printer->pageRect().width() / static_cast<double>(mapWidget->width());
  double yscale = printer->pageRect().height() / static_cast<double>(mapWidget->height());
  double scale = std::min(xscale, yscale);

  painter.begin(printer);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setRenderHint(QPainter::TextAntialiasing, true);
  painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
  painter.translate(printer->paperRect().x() + printer->pageRect().width() / 2,
                    printer->paperRect().y() + printer->pageRect().height() / 2);

  // Scale to printer resolution for better images
  painter.scale(scale, scale);
  painter.translate(-mapWidget->width() / 2, -mapWidget->height() / 2);

  // Print map and avoid dark map in night mode
  mapWidget->setPrinting(true);
  mapWidget->render(&painter);
  mapWidget->setPrinting(false);

  QFont font = painter.font();
  font.setPixelSize(10);
  painter.setFont(font);
  drawWatermarkInternal(QPoint(0, mapWidget->height()), &painter);

  painter.end();

  QGuiApplication::restoreOverrideCursor();
}

void PrintSupport::saveState()
{
  printDialog->saveState();
}

void PrintSupport::restoreState()
{
  printDialog->restoreState();
  printDialog->setRouteTableColumns(NavApp::getRouteController()->getRouteColumns());
}

void PrintSupport::drawWatermark(const QPoint& pos, QPixmap *pixmap)
{
  // Watermark for images
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
  painter->setBackground(QColor("#a0ffffff"));
  painter->setBrush(Qt::NoBrush);
  painter->setBackgroundMode(Qt::OpaqueMode);

  painter->drawText(pos.x(),
                    pos.y() - painter->fontMetrics().descent(),
                    tr("%1 Version %2 (revision %3) on %4 ").
                    arg(QApplication::applicationName()).
                    arg(QApplication::applicationVersion()).
                    arg(GIT_REVISION).
                    arg(QLocale().toString(QDateTime::currentDateTime())));
}
