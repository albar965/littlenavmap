/*****************************************************************************
* Copyright 2015-2022 Alexander Barthel alex@littlenavmap.org
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

#include "routeexport/fetchroutedialog.h"

#include "ui_fetchroutedialog.h"

#include "atools.h"
#include "common/constants.h"
#include "fs/pln/flightplan.h"
#include "gui/dialog.h"
#include "gui/helphandler.h"
#include "gui/widgetstate.h"
#include "navapp.h"
#include "route/routecontroller.h"
#include "routestring/routestringreader.h"
#include "util/htmlbuilder.h"
#include "util/httpdownloader.h"
#include "util/xmlstream.h"
#include "zip/gzip.h"
#include "settings/settings.h"

#include <QPushButton>
#include <QTimer>
#include <QUrlQuery>
#include <QXmlStreamReader>
#include <QStringBuilder>

using atools::util::HttpDownloader;

FetchRouteDialog::FetchRouteDialog(QWidget *parent) :
  QDialog(parent),
  ui(new Ui::FetchRouteDialog)
{
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
  setWindowModality(Qt::ApplicationModal);
  ui->setupUi(this);

  // Allow user to override URL
  fetcherUrl = atools::settings::Settings::instance().valueStr(lnm::ROUTE_EXPORT_SIMBRIEF_FETCHER_URL,
                                                               "https://www.simbrief.com/api/xml.fetcher.php");

  downloader = new HttpDownloader(this);
  flightplan = new atools::fs::pln::Flightplan;

  // Connect signals ============================================
  connect(downloader, &HttpDownloader::downloadFailed, this, &FetchRouteDialog::downloadFailed);
  connect(downloader, &HttpDownloader::downloadFinished, this, &FetchRouteDialog::downloadFinished);
  connect(downloader, &HttpDownloader::downloadSslErrors, this, &FetchRouteDialog::downloadSslErrors);
  connect(ui->buttonBox, &QDialogButtonBox::clicked, this, &FetchRouteDialog::buttonBoxClicked);
  connect(ui->lineEditLogin, &QLineEdit::textChanged, this, &FetchRouteDialog::updateButtonStates);

  // Change button texts and tooltips ============================================
  ui->buttonBox->button(QDialogButtonBox::Ok)->setText(tr("&Download Flight Plan"));
  ui->buttonBox->button(QDialogButtonBox::Ok)->setToolTip(tr("Connect to SimBrief and download flight plan information"));

  ui->buttonBox->button(QDialogButtonBox::Yes)->setText(tr("Create &Flight Plan"));
  ui->buttonBox->button(QDialogButtonBox::Yes)->setToolTip(tr("Create flight plan from downloaded information"));

  ui->buttonBox->button(QDialogButtonBox::YesToAll)->setText(tr("Open in &Route Description"));
  ui->buttonBox->button(QDialogButtonBox::YesToAll)->setToolTip(tr("Open flight plan in route description dialog for\n"
                                                                   "refinement or corrections."));

  restoreState();
}

FetchRouteDialog::~FetchRouteDialog()
{
  saveState();
  delete downloader;
  delete ui;
  delete flightplan;
}

void FetchRouteDialog::buttonBoxClicked(QAbstractButton *button)
{
  if(button == ui->buttonBox->button(QDialogButtonBox::Ok))
  {
    // Start download but do not close dialog
    startDownload();
    updateButtonStates();
  }
  else if(button == ui->buttonBox->button(QDialogButtonBox::Yes))
  {
    // Close dialog and use flight plan
    saveState();
    QDialog::accept();

    // Use always IFR for SimBrief plans
    flightplan->setFlightplanType(atools::fs::pln::IFR);

    emit routeNewFromFlightplan(*flightplan, false /* adjustAltitude */, true /* changed */, false /* undo */);
  }
  else if(button == ui->buttonBox->button(QDialogButtonBox::YesToAll))
  {
    // Close dialog and show route description dialog
    saveState();
    QDialog::accept();
    emit routeNewFromString(routeString);
  }
  else if(button == ui->buttonBox->button(QDialogButtonBox::Cancel))
  {
    // Stop all and close
    saveState();
    downloader->cancelDownload();
    updateButtonStates();
    QDialog::reject();
  }
  else if(button == ui->buttonBox->button(QDialogButtonBox::Help))
    // Show help but do not close
    atools::gui::HelpHandler::openHelpUrlWeb(parentWidget(), lnm::helpOnlineUrl % "SENDSIMBRIEF.html", lnm::helpLanguageOnline());
}

void FetchRouteDialog::updateButtonStates()
{
  if(downloader->isDownloading())
  {
    ui->buttonBox->button(QDialogButtonBox::Ok)->setDisabled(true); // Download
    ui->buttonBox->button(QDialogButtonBox::Yes)->setDisabled(true); // Create plan
    ui->buttonBox->button(QDialogButtonBox::YesToAll)->setDisabled(true); // Pass to route description
  }
  else
  {
    ui->buttonBox->button(QDialogButtonBox::Ok)->setDisabled(ui->lineEditLogin->text().isEmpty()); // Download
    ui->buttonBox->button(QDialogButtonBox::Yes)->setDisabled(flightplan->isEmpty()); // Create plan
    ui->buttonBox->button(QDialogButtonBox::YesToAll)->setDisabled(routeString.isEmpty()); // Pass to route description
  }
}

void FetchRouteDialog::restoreState()
{
  atools::gui::WidgetState widgetState(lnm::FETCH_SIMBRIEF_DIALOG, false);
  widgetState.restore({this, ui->comboBoxLoginType, ui->lineEditLogin});
  updateButtonStates();
}

void FetchRouteDialog::saveState()
{
  atools::gui::WidgetState widgetState(lnm::FETCH_SIMBRIEF_DIALOG, false);
  widgetState.save({this, ui->comboBoxLoginType, ui->lineEditLogin});
}

void FetchRouteDialog::startDownload()
{
  ui->textEditResult->setText(tr("Downloading flight plan ..."));
  // Fetching OFP (for LNM: fetch flight plan):
  // https://forum.navigraph.com/t/fetching-a-users-latest-ofp-data/5297
  // Example with user id: https://www.simbrief.com/api/xml.fetcher.php?userid=99999
  // Example with username: https://www.simbrief.com/api/xml.fetcher.php?username=LOGINNAME
  // Example with username: https://www.simbrief.com/api/xml.fetcher.php?username=LOGINNAME

  QUrlQuery query;
  query.addQueryItem(ui->comboBoxLoginType->currentIndex() == 0 ? "username" : "userid", ui->lineEditLogin->text());

  QUrl url(fetcherUrl);
  url.setQuery(query);

  {
    QUrlQuery queryLog;
    queryLog.addQueryItem(ui->comboBoxLoginType->currentIndex() == 0 ? "username" : "userid", "XXXXX");

    QUrl urlLog(fetcherUrl);
    urlLog.setQuery(queryLog);

    qDebug() << Q_FUNC_INFO << urlLog.toDisplayString();
  }

  downloader->setUrl(url.toEncoded());
  downloader->startDownload();
}

void FetchRouteDialog::downloadFinished(const QByteArray& data, QString)
{
  qDebug() << Q_FUNC_INFO;

  // Read downloaded XML ==================================================================
  atools::util::XmlStream xmlStream(atools::zip::gzipDecompressIf(data, Q_FUNC_INFO));
  QXmlStreamReader& reader = xmlStream.getReader();

  QString departure, destination, alternate, route;
  xmlStream.readUntilElement("OFP");

  // . ...
  // . <origin>
  // .   <icao_code>EDDF</icao_code>
  // .   <iata_code>FRA</iata_code>
  // .   <faa_code/>
  // . ...
  // . <destination>
  // .   <icao_code>LIRF</icao_code>
  // .   <iata_code>FCO</iata_code>
  // .   <faa_code/>
  // .   <elevation>14</elevation>
  // .   <pos_lat>41.800278</pos_lat>
  // .   <pos_long>12.238889</pos_long>
  // . ...
  // . <alternate>
  // .   <icao_code>LIRN</icao_code>
  // .   <iata_code>NAP</iata_code>
  // . ...
  // .   <atc>
  // .     <flightplan_text>(FPL-N275SB-IS ...
  // .  LIRR0101 PER/C RMK/TCAS)</flightplan_text>
  // .     <route>N0418F330 ANEK1F ANEKI Y163 NATOR N850 TRA UP131 RESIA DCT IDREK DCT BAGNO DCT GAVRA DCT RITEB RITE3R</route>
  // .     <route_ifps>N0418F330 ANEKI1F ANEKI Y163 NATOR N850 TRA UP131 RESIA DCT IDREK DCT BAGNO DCT GAVRA DCT RITEB RITEB3R</route_ifps>
  // .     <callsign>N275SB</callsign>
  // .     <initial_spd>0418</initial_spd>
  // .     <initial_spd_unit>N</initial_spd_unit>
  // .     <initial_alt>330</initial_alt>
  // .     <initial_alt_unit>F</initial_alt_unit>
  // . ...
  while(xmlStream.readNextStartElement())
  {
    // Read route elements if needed ======================================================
    if(reader.name() == "origin")
    {
      while(xmlStream.readNextStartElement())
      {
        if(reader.name() == "icao_code")
          departure = reader.readElementText();
        else
          xmlStream.skipCurrentElement(false /* warn */);
      }
    }
    else if(reader.name() == "destination")
    {
      while(xmlStream.readNextStartElement())
      {
        if(reader.name() == "icao_code")
          destination = reader.readElementText();
        else
          xmlStream.skipCurrentElement(false /* warn */);
      }
    }
    else if(reader.name() == "alternate")
    {
      while(xmlStream.readNextStartElement())
      {
        if(reader.name() == "icao_code")
          alternate = reader.readElementText();
        else
          xmlStream.skipCurrentElement(false /* warn */);
      }
    }
    else if(reader.name() == "atc")
    {
      while(xmlStream.readNextStartElement())
      {
        if(reader.name() == "route")
          route = reader.readElementText();
        else
          xmlStream.skipCurrentElement(false /* warn */);
      }
    }
    else
      xmlStream.skipCurrentElement(false /* warn */);
  }

  // Join plan elements to route string =============================
  routeString = departure % " " % route % " " % destination % " " % alternate;

  // Read string to flight plan
  RouteStringReader routeStringReader(NavApp::getRouteController()->getFlightplanEntryBuilder());
  bool ok = routeStringReader.createRouteFromString(routeString, rs::SIMBRIEF_READ_DEFAULTS, flightplan);

  QString message(tr("<p>Flight successfully downloaded. Reading of route description %1.").arg(ok ? tr("successful") : tr("failed")));

  // Add any information/warning/error messages from parsing
  message.append(tr("<p>"));
  if(!routeStringReader.getMessages().isEmpty())
    message.append(routeStringReader.getMessages().join("<br/>"));
  message.append(tr("</p>"));

  if(!ok)
    // Parsing failed clear plan
    flightplan->clear();

  ui->textEditResult->setText(message);

  qDebug() << Q_FUNC_INFO << "departure" << departure << "destination" << destination << "alternate" << alternate << "route" << route;

#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << *flightplan;
#endif

  // Have to update states in event queue since isDownloading is still set while in this method
  QTimer::singleShot(0, this, &FetchRouteDialog::updateButtonStates);
}

void FetchRouteDialog::downloadFailed(const QString& error, int errorCode, QString downloadUrl)
{
  qDebug() << Q_FUNC_INFO;
  QString message = atools::util::HtmlBuilder::errorMessage(tr("Download of SimBrief flight plan failed."));

  ui->textEditResult->setText(tr("<p>%1</p>"
                                   "<p>URL: &quot;%2&quot;</p>"
                                     "<p>Error: %3 (%4)</p>"
                                       "<p>Did you log into SimBrief and generate a flight plan?</p>"
                                         "<p>Is your Pilot ID or Username correct?</p>").
                              arg(message).arg(downloadUrl).arg(error).arg(errorCode));

  // Have to update states in event queue since isDownloading is still set while in this method
  QTimer::singleShot(0, this, &FetchRouteDialog::updateButtonStates);
}

void FetchRouteDialog::downloadSslErrors(const QStringList& errors, const QString& downloadUrl)
{
  qDebug() << Q_FUNC_INFO;
  NavApp::closeSplashScreen();

  int result = atools::gui::Dialog(this).
               showQuestionMsgBox(lnm::ACTIONS_SHOW_SSL_WARNING_SIMBRIEF,
                                  tr("<p>Errors while trying to establish an encrypted connection to SimBrief:</p>"
                                       "<p>URL: %1</p>"
                                         "<p>Error messages:<br/>%2</p>"
                                           "<p>Continue?</p>").
                                  arg(downloadUrl).
                                  arg(atools::strJoin(errors, tr("<br/>"))),
                                  tr("Do not &show this again and ignore errors."),
                                  QMessageBox::Cancel | QMessageBox::Yes,
                                  QMessageBox::Cancel, QMessageBox::Yes);

  downloader->setIgnoreSslErrors(result == QMessageBox::Yes);

  QTimer::singleShot(0, this, &FetchRouteDialog::updateButtonStates);
}
