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

#include "logbook/logstatisticsdialog.h"
#include "ui_logstatisticsdialog.h"

#include "logdatacontroller.h"
#include "util/htmlbuilder.h"
#include "geo/calculations.h"
#include "common/unit.h"
#include "common/formatter.h"
#include "common/constants.h"
#include "gui/widgetstate.h"
#include "sql/sqldatabase.h"
#include "gui/itemviewzoomhandler.h"
#include "gui/helphandler.h"
#include "navapp.h"
#include "export/csvexporter.h"

#include <QDateTime>
#include <QSqlError>
#include <QDebug>
#include <QSqlQueryModel>
#include <QPushButton>
#include <QMimeData>
#include <QClipboard>

LogStatsDelegate::LogStatsDelegate()
{
}

void LogStatsDelegate::paint(QPainter *painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
  QStyleOptionViewItem opt(option);
  // Assign aligment copy from Query struct
  if(!align.isEmpty())
    opt.displayAlignment = align.at(index.column());
  QStyledItemDelegate::paint(painter, opt, index);
}

// ============================================================================================

LogStatsSqlModel::LogStatsSqlModel()
{

}

QVariant LogStatsSqlModel::data(const QModelIndex& index, int role) const
{
  if(role == Qt::DisplayRole)
  {
    // Apply locale formatting for numeric values - not compatible with sorting
    QVariant dataValue = QSqlQueryModel::data(index, Qt::DisplayRole);
    QVariant::Type type = dataValue.type();

    if(type == QVariant::Int)
      return locale.toString(dataValue.toInt());
    else if(type == QVariant::UInt)
      return locale.toString(dataValue.toUInt());
    else if(type == QVariant::LongLong)
      return locale.toString(dataValue.toLongLong());
    else if(type == QVariant::ULongLong)
      return locale.toString(dataValue.toULongLong());
    else if(type == QVariant::Double)
      return locale.toString(dataValue.toDouble(), 'f', 1);
  }

  return QSqlQueryModel::data(index, role);
}

// ============================================================================================

LogStatisticsDialog::LogStatisticsDialog(QWidget *parent, LogdataController *logdataControllerParam)
  : QDialog(parent), ui(new Ui::LogStatisticsDialog), logdataController(logdataControllerParam)
{
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
  setWindowModality(Qt::NonModal);

  // Prefill query vector
  initQueries();

  ui->setupUi(this);

  // Copy to clipboard button in button bar ============================
  QPushButton *button = ui->buttonBoxLogStats->addButton(tr("&Copy to Clipboard"), QDialogButtonBox::NoRole);
  button->setToolTip(tr("Copies overview as formatted text or table as CSV to clipboard"));

  // Fill query labels into combo box ==============================
  for(const Query& q : QUERIES)
    ui->comboBoxLogStatsGrouped->addItem(q.label, q.query);

  // Create and set delegate ==============================
  delegate = new LogStatsDelegate;
  ui->tableViewLogStatsGrouped->setItemDelegate(delegate);

  // Resize widget to get rid of the too large default margins
  zoomHandler = new atools::gui::ItemViewZoomHandler(ui->tableViewLogStatsGrouped);

  connect(ui->comboBoxLogStatsGrouped, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
          this, &LogStatisticsDialog::groupChanged);
  connect(ui->buttonBoxLogStats, &QDialogButtonBox::clicked, this, &LogStatisticsDialog::buttonBoxClicked);

  restoreState();
}

LogStatisticsDialog::~LogStatisticsDialog()
{
  ui->tableViewLogStatsGrouped->setItemDelegate(nullptr);
  delete delegate;

  clearModel();

  saveState();
  delete zoomHandler;
  delete ui;
}

void LogStatisticsDialog::updateWidgets()
{
  groupChanged(ui->comboBoxLogStatsGrouped->currentIndex());
  updateStatisticsText();
}

void LogStatisticsDialog::logDataChanged()
{
  if(isVisible())
  {
    setModel();
    updateWidgets();
    updateStatisticsText();
  }
}

void LogStatisticsDialog::optionsChanged()
{
  logDataChanged();
}

void LogStatisticsDialog::buttonBoxClicked(QAbstractButton *button)
{
  QDialogButtonBox::StandardButton buttonType = ui->buttonBoxLogStats->standardButton(button);

  qDebug() << Q_FUNC_INFO << buttonType;

  if(buttonType == QDialogButtonBox::Ok || buttonType == QDialogButtonBox::Close)
  {
    saveState();
    accept();
  }
  else if(buttonType == QDialogButtonBox::NoButton)
  {
    // Only non-standard button is copy to clipboard
    if(ui->tabWidget->currentIndex() == 0)
    {
      // Copy formatted and plain text from text browser to clipboard
      QMimeData *data = new QMimeData;
      data->setHtml(ui->textBrowserLogStatsOverview->toHtml());
      data->setText(ui->textBrowserLogStatsOverview->toPlainText());
      QGuiApplication::clipboard()->setMimeData(data);
      NavApp::setStatusMessage(QString(tr("Copied text to clipboard.")));
    }
    else
    {
      // Copy CSV from table to clipboard
      QString csv;
      int exported = CsvExporter::tableAsCsv(ui->tableViewLogStatsGrouped, true /* header */, csv);
      if(!csv.isEmpty())
        QApplication::clipboard()->setText(csv);
      NavApp::setStatusMessage(QString(tr("Copied %1 rows from table as CSV to clipboard.").arg(exported)));
    }
  }
  else if(buttonType == QDialogButtonBox::Help)
    atools::gui::HelpHandler::openHelpUrlWeb(
      this, lnm::helpOnlineUrl + "LOGBOOK.html#statistics", lnm::helpLanguageOnline());
}

void LogStatisticsDialog::clearModel()
{
  QItemSelectionModel *m = ui->tableViewLogStatsGrouped->selectionModel();
  ui->tableViewLogStatsGrouped->setModel(nullptr);
  delete m;

  delete model;
  model = nullptr;
}

void LogStatisticsDialog::setModel()
{
  clearModel();

  model = new LogStatsSqlModel;

  QItemSelectionModel *m = ui->tableViewLogStatsGrouped->selectionModel();
  ui->tableViewLogStatsGrouped->setModel(model);
  delete m;
}

void LogStatisticsDialog::saveState()
{
  atools::gui::WidgetState(lnm::LOGDATA_STATS_DIALOG).save({this, ui->tabWidget, ui->comboBoxLogStatsGrouped});
}

void LogStatisticsDialog::restoreState()
{
  atools::gui::WidgetState(lnm::LOGDATA_STATS_DIALOG, true, true).
  restore({this, ui->tabWidget, ui->comboBoxLogStatsGrouped});
}

void LogStatisticsDialog::groupChanged(int index)
{
  qDebug() << Q_FUNC_INFO << index;

  if(index == -1)
    return;

  if(index > QUERIES.size() - 1)
    index = QUERIES.size() - 1;

  // Calculate the conversion factor for distances which will be set into the SQL query
  float nmToUnit = 1.f;
  opts::UnitDist unitDist = Unit::getUnitDist();
  switch(unitDist)
  {
    case opts::DIST_NM:
      nmToUnit = 1.f;
      break;

    case opts::DIST_KM:
      nmToUnit = atools::geo::nmToKm(1.f);
      break;

    case opts::DIST_MILES:
      nmToUnit = atools::geo::nmToMi(1.f);
      break;
  }

  Query query = QUERIES.at(index);
  QString queryText = Unit::replacePlaceholders(query.query).arg(nmToUnit);
  model->setQuery(queryText, logdataController->getDatabase()->getQSqlDatabase());

  if(model->lastError().type() != QSqlError::NoError)
    qWarning() << Q_FUNC_INFO << "Error executing" << queryText << model->lastError().text();

  // Set headers
  for(int i = 0; i < query.header.size(); i++)
    model->setHeaderData(i, Qt::Horizontal, Unit::replacePlaceholders(query.header.at(i)));

  // Copy alignment data to delegate
  delegate->align = query.align;

  ui->tableViewLogStatsGrouped->resizeColumnsToContents();
}

void LogStatisticsDialog::updateStatisticsText()
{
  atools::util::HtmlBuilder html(true);

  QLocale locale;
  atools::util::html::Flags header = atools::util::html::BOLD | atools::util::html::BIG;
  atools::util::html::Flags right = atools::util::html::ALIGN_RIGHT;

  // ======================================================
  html.p(tr("Departure Times"), header);
  QDateTime earliest, latest, earliestSim, latestSim;
  logdataController->getFlightStatsTime(earliest, latest, earliestSim, latestSim);
  html.table();
  html.row2If(tr("Earliest:"), tr("%1 %2").
              arg(locale.toString(earliest, QLocale::ShortFormat)).
              arg(earliest.timeZoneAbbreviation()), right);
  html.row2If(tr("Earliest in Simulator:"), tr("%1 %2").
              arg(locale.toString(earliestSim, QLocale::ShortFormat)).
              arg(earliestSim.timeZoneAbbreviation()), right);
  html.row2If(tr("Latest:"), tr("%1 %2").
              arg(locale.toString(latest, QLocale::ShortFormat)).
              arg(latest.timeZoneAbbreviation()), right);
  html.row2If(tr("Latest in Simulator:"), tr("%1 %2").
              arg(locale.toString(latestSim, QLocale::ShortFormat)).
              arg(latestSim.timeZoneAbbreviation()), right);
  html.tableEnd();

  // ======================================================
  html.p(tr("Flight Plan Distances"), header);
  float distTotal, distMax, distAverage;
  logdataController->getFlightStatsDistance(distTotal, distMax, distAverage);
  html.table();
  html.row2(tr("Total:"), Unit::distNm(distTotal), right);
  html.row2(tr("Maximum:"), Unit::distNm(distMax), right);
  html.row2(tr("Average:"), Unit::distNm(distAverage), right);
  html.tableEnd();

  html.p(tr("Simulators"), header);
  QVector<std::pair<int, QString> > simulators;
  logdataController->getFlightStatsSimulator(simulators);
  html.table();
  for(const std::pair<int, QString>& sim : simulators)
    html.row2(tr("%1:").arg(sim.second), tr("%1 flights").arg(locale.toString(sim.first)), right);
  html.tableEnd();

  // ======================================================
  html.p(tr("Used Aircraft"), header);
  int numTypes, numRegistrations, numNames, numSimulators;
  logdataController->getFlightStatsAircraft(numTypes, numRegistrations, numNames, numSimulators);
  html.table();
  html.row2(tr("Number of distinct types:"), locale.toString(numTypes), right);
  html.row2(tr("Number of distinct registrations:"), locale.toString(numRegistrations), right);
  html.row2(tr("Number of distinct names:"), locale.toString(numNames), right);
  html.tableEnd();

  // ======================================================
  html.p(tr("Visited Airports"), header);
  int numDepartAirports, numDestAirports;
  logdataController->getFlightStatsAirports(numDepartAirports, numDestAirports);
  html.table();
  html.row2(tr("Distinct departures:"), locale.toString(numDepartAirports), right);
  html.row2(tr("Distinct destinations:"), locale.toString(numDestAirports), right);
  html.tableEnd();

  // ======================================================
  html.p(tr("Flight Time"), header);
  float timeMaximum, timeAverage, timeMaximumSim, timeAverageSim;
  logdataController->getFlightStatsTripTime(timeMaximum, timeAverage, timeMaximumSim, timeAverageSim);
  html.table();
  html.row2(tr("Maximum:"), formatter::formatMinutesHoursLong(timeMaximum), right);
  html.row2(tr("Maximum in simulator:"), formatter::formatMinutesHoursLong(timeMaximumSim), right);
  html.row2(tr("Average:"), formatter::formatMinutesHoursLong(timeAverage), right);
  html.row2(tr("Average in simulator:"), formatter::formatMinutesHoursLong(timeAverageSim), right);
  html.tableEnd();

  ui->textBrowserLogStatsOverview->setHtml(html.getHtml());
}

void LogStatisticsDialog::initQueries()
{
  QUERIES = {

    Query{
      // Combox box entry
      tr("Top airports"),

      // Tables headers - allows variables like %dist%
      {tr("Number of\nvisits"), tr("ICAO"), tr("Name")},

      // Column alignment
      {Qt::AlignRight, Qt::AlignRight, Qt::AlignLeft},

      // Query - allows variables like %dist% and %1 is replacement for distance factor for conversion
      "select count(1), ident, name from ( "
      "select logbook_id, departure_ident as ident, departure_name as name from logbook where departure_ident is not null "
      "union "
      "select logbook_id, destination_ident as ident, destination_name as name from logbook where destination_ident is not null) "
      "group by ident, name order by count(1) desc limit 250"
    },

    Query{
      tr("Top departure airports"),
      {tr("Number of\ndepartures"), tr("ICAO"), tr("Name")},
      {Qt::AlignRight, Qt::AlignRight, Qt::AlignLeft},
      "select count(1), departure_ident, departure_name "
      "from logbook group by departure_ident, departure_name order by count(1) desc limit 250"
    },

    Query{
      tr("Top destination airports"),
      {tr("Number of\ndestinations"), tr("ICAO"), tr("Name")},
      {Qt::AlignRight, Qt::AlignRight, Qt::AlignLeft},
      "select count(1), destination_ident, destination_name "
      "from logbook group by destination_ident, destination_name order by count(1) desc limit 250"
    },

    Query{
      tr("Longest flights by distance"),
      {tr("Flight Plan\nDistance %dist%"), tr("From ICAO"), tr("From Name"), tr("To ICAO"), tr("To Name"),
       tr("Simulator"), tr("Aircraft\nModel"), tr("Aircraft\nType"), tr("Aircraft\nRegistration")},
      {Qt::AlignRight, Qt::AlignRight, Qt::AlignLeft, Qt::AlignRight, Qt::AlignLeft, Qt::AlignLeft, Qt::AlignLeft,
       Qt::AlignLeft, Qt::AlignLeft},
      "select cast(distance * %1 as int), departure_ident, departure_name, destination_ident, destination_name, "
      "simulator, aircraft_name, aircraft_type, aircraft_registration "
      "from logbook order by distance desc limit 250"
    },

    Query{
      tr("Longest flights by simulator time"),
      {tr("Simulator time\nhours"), tr("From Airport\nICAO"), tr("From Airport\nName"),
       tr("To Airport\nICAO"), tr("To Airport\nName"),
       tr("Simulator"), tr("Aircraft\nModel"), tr("Aircraft\nType"), tr("Aircraft\nRegistration")},
      {Qt::AlignRight, Qt::AlignRight, Qt::AlignLeft, Qt::AlignRight, Qt::AlignLeft, Qt::AlignLeft, Qt::AlignLeft,
       Qt::AlignLeft, Qt::AlignLeft},
      "select cast((strftime('%s', destination_time_sim) - strftime('%s', departure_time_sim)) / 3600. as double), "
      "departure_ident, departure_name, destination_ident, destination_name, "
      "simulator, aircraft_name, aircraft_type, aircraft_registration "
      "from logbook order by strftime('%s', destination_time_sim) - strftime('%s', departure_time_sim) desc limit 250"
    },

    Query{
      tr("Longest flights by real time"),
      {tr("Real time\nhours"), tr("From Airport\nICAO"), tr("From Airport\nName"),
       tr("To Airport\nICAO"), tr("To Airport\nName"),
       tr("Simulator"), tr("Aircraft\nModel"), tr("Aircraft\nType"), tr("Aircraft\nRegistration")},
      {Qt::AlignRight, Qt::AlignRight, Qt::AlignLeft, Qt::AlignRight, Qt::AlignLeft, Qt::AlignLeft, Qt::AlignLeft,
       Qt::AlignLeft, Qt::AlignLeft},
      "select cast((strftime('%s', destination_time) - strftime('%s', departure_time)) / 3600. as double), "
      "departure_ident, departure_name, destination_ident, destination_name, "
      "simulator, aircraft_name, aircraft_type, aircraft_registration "
      "from logbook order by strftime('%s', destination_time) - strftime('%s', departure_time) desc limit 250"
    },

    Query{
      tr("Aircraft usage"),
      {tr("Number of\nflights"), tr("Simulator"), tr("Total flight\nplan distance %dist%"),
       tr("Total simulator time\nhours"), tr("Total realtime\nhours"), tr("Model"), tr("Type"), tr("Registration")},
      {Qt::AlignRight, Qt::AlignLeft, Qt::AlignRight, Qt::AlignRight, Qt::AlignRight, Qt::AlignLeft, Qt::AlignLeft,
       Qt::AlignLeft},
      "select count(1), simulator, cast(round(sum(distance) * %1) as int), "
      "cast(sum((strftime('%s', destination_time) - strftime('%s', departure_time)) / 3600.) as double), "
      "cast(sum((strftime('%s', destination_time_sim) - strftime('%s', departure_time_sim)) / 3600.) as double), "
      "aircraft_name, aircraft_type, aircraft_registration "
      "from logbook group by simulator, aircraft_name, aircraft_type, aircraft_registration order by count(1) desc limit 250"
    },

    Query{
      tr("Aircraft usage by type"),
      {tr("Number of\nflights"), tr("Simulator"), tr("Total flight\nplan distance %dist%"), tr("Total realtime\nhours"),
       tr("Total simulator time\nhours"), tr("Type")},
      {Qt::AlignRight, Qt::AlignLeft, Qt::AlignRight, Qt::AlignRight, Qt::AlignRight, Qt::AlignLeft},
      "select count(1), simulator, cast(round(sum(distance) * %1) as int), "
      "cast(sum((strftime('%s', destination_time) - strftime('%s', departure_time)) / 3600.) as double), "
      "cast(sum((strftime('%s', destination_time_sim) - strftime('%s', departure_time_sim)) / 3600.) as double), "
      "aircraft_type "
      "from logbook group by simulator, aircraft_type order by count(1) desc limit 250"
    },

    Query{
      tr("Aircraft usage by registration"),
      {tr("Number of\nflights"), tr("Simulator"), tr("Total flight\nplan distance %dist%"), tr("Total realtime\nhours"),
       tr("Total simulator time\nhours"), tr("Type")},
      {Qt::AlignRight, Qt::AlignLeft, Qt::AlignRight, Qt::AlignRight, Qt::AlignRight, Qt::AlignLeft},
      "select count(1), simulator, cast(round(sum(distance) * %1) as int), "
      "cast(sum((strftime('%s', destination_time) - strftime('%s', departure_time)) / 3600.) as double), "
      "cast(sum((strftime('%s', destination_time_sim) - strftime('%s', departure_time_sim)) / 3600.) as double), "
      "aircraft_registration "
      "from logbook group by simulator, aircraft_registration order by count(1) desc limit 250"
    }
  };
}

void LogStatisticsDialog::showEvent(QShowEvent *event)
{
  qDebug() << Q_FUNC_INFO;
  Q_UNUSED(event);
  setModel();
  updateWidgets();
}

void LogStatisticsDialog::hideEvent(QHideEvent *event)
{
  qDebug() << Q_FUNC_INFO;
  Q_UNUSED(event);

  // Disconnect from database if not shown
  clearModel();
}
