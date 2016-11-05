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

#include "print/printsupport.h"
#include "common/constants.h"
#include "gui/mainwindow.h"
#include "mapgui/mapwidget.h"
#include "settings/settings.h"
#include "route/routecontroller.h"
#include "util/htmlbuilder.h"

#include <QPainter>
#include <QtPrintSupport/QPrintPreviewDialog>
#include <QtPrintSupport/QPrinter>
#include <QStandardPaths>
#include <QDir>
#include <QDesktopServices>
#include <QTextDocument>

using atools::settings::Settings;
using atools::util::HtmlBuilder;

PrintSupport::PrintSupport(MainWindow *parent)
  : mainWindow(parent)
{

}

PrintSupport::~PrintSupport()
{
  delete mapScreen;
  delete flightPlanDocument;
}

void PrintSupport::printMap()
{
  mainWindow->getMapWidget()->showOverlays(false);
  delete mapScreen;
  mapScreen = new QPixmap(mainWindow->getMapWidget()->mapScreenShot());
  mainWindow->getMapWidget()->showOverlays(true);

  QPrintPreviewDialog *print = buildPreviewDialog("Little Navmap Map.pdf");
  connect(print, &QPrintPreviewDialog::paintRequested, this, &PrintSupport::paintRequestedMap);
  print->exec();
  disconnect(print, &QPrintPreviewDialog::paintRequested, this, &PrintSupport::paintRequestedMap);
  deletePreviewDialog(print);
}

QPrintPreviewDialog *PrintSupport::buildPreviewDialog(const QString& printFileName)
{
  QPrintPreviewDialog *print = new QPrintPreviewDialog(mainWindow);
  print->resize(Settings::instance().valueVar(lnm::MAINWINDOW_PRINT_SIZE, QSize(640, 480)).toSize());
  print->printer()->setOutputFileName(
    QStandardPaths::standardLocations(QStandardPaths::DocumentsLocation).first() +
    QDir::separator() + printFileName);

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
  if(flightPlanDocument != nullptr)
    flightPlanDocument->print(printer);
}

void PrintSupport::paintRequestedMap(QPrinter *printer)
{
  QPixmap printScreen(mapScreen->scaled(printer->pageRect().size(),
                                        Qt::KeepAspectRatio, Qt::SmoothTransformation));

  QPainter painter;
  painter.begin(printer);

  qDebug() << "font.pointSize" << painter.font().pointSize()
           << "page" << printer->pageRect() << "paper" << printer->paperRect()
           << "screen" << mapScreen->rect() << "printScreen" << printScreen.rect();

  // Center image
  int x = (printer->pageRect().width() - printScreen.width()) / 2;
  int y = (printer->pageRect().height() - printScreen.height()) / 2;

  painter.drawPixmap(QPoint(x, y), printScreen);

  drawWatermark(QPoint(printer->pageRect().left(), printer->pageRect().height()), &painter);

  painter.end();
}

void PrintSupport::printFlightplan()
{
  qDebug() << "printFlightplan";

  QString cssContent;
//  QString css = atools::settings::Settings::getOverloadedPath(":/littlenavmap/resources/css/print.css");
//  QFile f(css);
//  if(f.open(QIODevice::ReadOnly | QIODevice::Text))
//  {
//    QTextStream in(&f);
//    cssContent = in.readAll();
//  }

  atools::util::HtmlBuilder html(false);
  html.doc(QString(), cssContent);
  html.append(mainWindow->getRouteController()->tableAsHtml());
  html.p(tr("%1 Version %2 (revision %3) on %4 ").
         arg(QApplication::applicationName()).
         arg(QApplication::applicationVersion()).
         arg(GIT_REVISION).
         arg(QLocale().toString(QDateTime::currentDateTime())));
  html.docEnd();

  delete flightPlanDocument;
  flightPlanDocument = new QTextDocument();
  flightPlanDocument->setHtml(html.getHtml());

  QPrintPreviewDialog *print = buildPreviewDialog("Little Navmap Map.pdf");
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

  delete flightPlanDocument;
  flightPlanDocument = nullptr;
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
