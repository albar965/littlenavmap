/*****************************************************************************
* Copyright 2015-2023 Alexander Barthel alex@littlenavmap.org
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
#include "gui/tools.h"
#include "ui_logstatisticsdialog.h"

#include "common/constants.h"
#include "common/formatter.h"
#include "common/unit.h"
#include "export/csvexporter.h"
#include "geo/calculations.h"
#include "gui/helphandler.h"
#include "gui/itemviewzoomhandler.h"
#include "gui/widgetstate.h"
#include "logdatacontroller.h"
#include "app/navapp.h"
#include "sql/sqldatabase.h"
#include "util/htmlbuilder.h"

#include <QSqlError>
#include <QPushButton>
#include <QMimeData>
#include <QClipboard>
#include <QStringBuilder>

// ============================================================================================

/* Contains information about selected query in combo box */
class Query
{
public:
  Query(const QString& labelParam, const QStringList& headerParam, const QVector<Qt::Alignment>& alignParam,
        const QStringList& colParam, int sortColumnParam, Qt::SortOrder sortCrderParam, const QString& queryParam)
    : label(labelParam), query(queryParam), cols(colParam), header(headerParam), align(alignParam),
    defaultSortColumn(sortColumnParam), defaultSortCrder(sortCrderParam)
  {
    Q_ASSERT_X(headerParam.size() == alignParam.size(), labelParam.toLatin1().constData(), query.toLatin1().constData());
    Q_ASSERT_X(headerParam.size() == colParam.size(), labelParam.toLatin1().constData(), query.toLatin1().constData());
    Q_ASSERT_X(defaultSortColumn < colParam.size(), labelParam.toLatin1().constData(), query.toLatin1().constData());
  }

  QString label, /* Combo box label */ query; /* SQL query */
  QStringList cols, header; /* SQL columns and Result table headers - must be equal to query columns */
  QVector<Qt::Alignment> align; /* Column alignment - must be equal to query columns */

  int defaultSortColumn; /* Index into list cols */
  Qt::SortOrder defaultSortCrder;
};

// ============================================================================================

/* Delegate to change table column data alignment */
class LogStatsDelegate
  : public QStyledItemDelegate
{
public:
  QVector<Qt::Alignment> align;

private:
  virtual void paint(QPainter *painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;

};

void LogStatsDelegate::paint(QPainter *painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
  QStyleOptionViewItem opt(option);
  // Assign alignment copy from Query struct
  if(!align.isEmpty())
    opt.displayAlignment = align.at(index.column());
  QStyledItemDelegate::paint(painter, opt, index);
}

// ============================================================================================

/* Overrides data method for local sensitive number formatting and sorting by SQL query */
class LogStatsSqlModel :
  public QSqlQueryModel
{
public:
  LogStatsSqlModel(QObject *parent, const QSqlDatabase *db)
    : QSqlQueryModel(parent), database(db)
  {

  }

  void setLogStatQuery(const Query *queryParam)
  {
    if(queryParam != query)
    {
      // Update all to defaults if different
      query = queryParam;
      sortColumn = query->defaultSortColumn;
      sortOrder = query->defaultSortCrder;
    }

    // Calculate the conversion factor for distances which will be set into the SQL query
    nmToUnitFactor = 1.f;
    switch(Unit::getUnitDist())
    {
      case opts::DIST_NM:
        nmToUnitFactor = 1.f;
        break;

      case opts::DIST_KM:
        nmToUnitFactor = atools::geo::nmToKm(1.f);
        break;

      case opts::DIST_MILES:
        nmToUnitFactor = atools::geo::nmToMi(1.f);
        break;
    }

    setQuery(buildQuery(), *database);
  }

private:
  virtual QVariant data(const QModelIndex& index, int role) const override;

  virtual void sort(int column, Qt::SortOrder order) override
  {
    if(query != nullptr)
    {
      // Reset with new non default sort order and update query
      sortColumn = column;
      sortOrder = order;
      setQuery(buildQuery(), *database);
    }
  }

  QString buildQuery()
  {
    // Build query with current ordering, unit placeholders and conversion factor
    QString str = query->query;
    if(str.contains("%1"))
      str = str.arg(nmToUnitFactor);
    return str % " order by " % query->cols.at(sortColumn) % (sortOrder == Qt::DescendingOrder ? " desc" : " asc");
  }

  QLocale locale;
  float nmToUnitFactor = 1.f;
  const QSqlDatabase *database = nullptr;
  const Query *query = nullptr;
  int sortColumn = 0;
  Qt::SortOrder sortOrder = Qt::DescendingOrder;
};

QVariant LogStatsSqlModel::data(const QModelIndex& index, int role) const
{
  if(role == Qt::DisplayRole)
  {
    // Apply locale formatting for numeric values
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
    else if(type == QVariant::DateTime)
      return locale.toString(dataValue.toDateTime());
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
  atools::gui::adjustSelectionColors(ui->tableViewLogStatsGrouped);

  // Copy main menu actions to allow using shortcuts in the non-modal dialog too
  addActions(NavApp::getMainWindowActions());

  // Copy to clipboard button in button bar ============================
  QPushButton *button = ui->buttonBoxLogStats->addButton(tr("&Copy to Clipboard"), QDialogButtonBox::NoRole);
  button->setToolTip(tr("Copies overview as formatted text or table as CSV to clipboard"));

  // Fill query labels into combo box ==============================
  for(const Query& q : qAsConst(queries))
    ui->comboBoxLogStatsGrouped->addItem(q.label, q.query);

  // Create and set delegate ==============================
  delegate = new LogStatsDelegate;
  ui->tableViewLogStatsGrouped->setItemDelegate(delegate);
  ui->tableViewLogStatsGrouped->setSortingEnabled(true);

  // Resize widget to get rid of the too large default margins
  zoomHandler = new atools::gui::ItemViewZoomHandler(ui->tableViewLogStatsGrouped);

  connect(ui->comboBoxLogStatsGrouped, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &LogStatisticsDialog::groupChanged);
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

void LogStatisticsDialog::styleChanged()
{
  atools::gui::adjustSelectionColors(ui->tableViewLogStatsGrouped);
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
      // Need to load whole table
      while(model->canFetchMore())
        model->fetchMore(QModelIndex());

      // Copy CSV from table to clipboard
      QString csv;
      int exported = CsvExporter::tableAsCsv(ui->tableViewLogStatsGrouped, true /* header */, csv);
      if(!csv.isEmpty())
        QApplication::clipboard()->setText(csv);
      NavApp::setStatusMessage(QString(tr("Copied %1 rows from table as CSV to clipboard.").arg(exported)));
    }
  }
  else if(buttonType == QDialogButtonBox::Help)
    atools::gui::HelpHandler::openHelpUrlWeb(this, lnm::helpOnlineUrl + "LOGBOOK.html#statistics", lnm::helpLanguageOnline());
}

void LogStatisticsDialog::clearModel()
{
  QItemSelectionModel *selectionModel = ui->tableViewLogStatsGrouped->selectionModel();
  ui->tableViewLogStatsGrouped->setModel(nullptr);
  delete selectionModel;

  delete model;
  model = nullptr;
}

void LogStatisticsDialog::setModel()
{
  clearModel();

  model = new LogStatsSqlModel(this, &logdataController->getDatabase()->getQSqlDatabase());

  QItemSelectionModel *selectionModel = ui->tableViewLogStatsGrouped->selectionModel();
  ui->tableViewLogStatsGrouped->setModel(model);
  delete selectionModel;

  groupChanged(ui->comboBoxLogStatsGrouped->currentIndex());
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

  if(index > queries.size() - 1)
    index = queries.size() - 1;

  const Query& query = queries.at(index);
  model->setLogStatQuery(&query);
  ui->tableViewLogStatsGrouped->sortByColumn(query.defaultSortColumn, query.defaultSortCrder);

  if(model->lastError().type() != QSqlError::NoError)
    qWarning() << Q_FUNC_INFO << "Error executing" << query.query << model->lastError().text();

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
  float timeMaximum, timeAverage, timeTotal, timeMaximumSim, timeAverageSim, timeTotalSim;
  logdataController->getFlightStatsTripTime(timeMaximum, timeAverage, timeTotal,
                                            timeMaximumSim, timeAverageSim, timeTotalSim);

  // Workaround to avoid translation changes
  html.p(tr("Flight Time Real"), header);
  html.table();
  html.row2(tr("Total:"), formatter::formatMinutesHoursLong(timeTotal), right);
  html.row2(tr("Average:"), formatter::formatMinutesHoursLong(timeAverage), right);
  html.row2(tr("Maximum:"), formatter::formatMinutesHoursLong(timeMaximum), right);
  html.tableEnd();

  html.p(tr("Flight Time Simulator"), header);
  html.table();
  html.row2(tr("Total:"), formatter::formatMinutesHoursLong(timeTotalSim), right);
  html.row2(tr("Average:"), formatter::formatMinutesHoursLong(timeAverageSim), right);
  html.row2(tr("Maximum:"), formatter::formatMinutesHoursLong(timeMaximumSim), right);
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
  for(const std::pair<int, QString>& sim : qAsConst(simulators))
    html.row2(tr("%1:").arg(sim.second.isEmpty() ? tr("Unknown") : sim.second),
              tr("%1 flights").arg(locale.toString(sim.first)), right);
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

  ui->textBrowserLogStatsOverview->setHtml(html.getHtml());
}

void LogStatisticsDialog::initQueries()
{
  const static Qt::AlignmentFlag RIGHT = Qt::AlignRight;
  const static Qt::AlignmentFlag LEFT = Qt::AlignLeft;

  queries = {

    Query(tr("Top airports"), // Combox box entry
          {tr("Number of\nvisits"), tr("ICAO"), tr("Name")}, // Tables headers - allows variables like %dist%
          {RIGHT, RIGHT, LEFT}, // Column alignment
          {"cnt", "ident", "name"}, 0, Qt::DescendingOrder, // Columns, default order column and default order direction
          // Query - allows variables like %dist% and %1 is replacement for distance factor for conversion
          "select count(1) as cnt, ident, name from ( "
          "select logbook_id, departure_ident as ident, departure_name as name from logbook where departure_ident is not null "
          "union "
          "select logbook_id, destination_ident as ident, destination_name as name from logbook where destination_ident is not null) "
          "group by ident, name"),

    Query(tr("Top departure airports"),
          {tr("Number of\ndepartures"), tr("Ident"), tr("Name")},
          {RIGHT, RIGHT, LEFT},
          {"cnt", "departure_ident", "departure_name"}, 0, Qt::DescendingOrder,
          "select count(1) as cnt, departure_ident, departure_name from logbook group by departure_ident, departure_name"),

    Query(tr("Top destination airports"),
          {tr("Number of\ndestinations"), tr("Ident"), tr("Name")},
          {RIGHT, RIGHT, LEFT},
          {"cnt", "destination_ident", "destination_name"}, 0, Qt::DescendingOrder,
          "select count(1) as cnt, destination_ident, destination_name from logbook group by destination_ident, destination_name"),

    Query(tr("Longest flights by distance"),
          {tr("Flight Plan\nDistance %dist%"), tr("From ICAO"), tr("From Name"), tr("To ICAO"), tr("To Name"),
           tr("Simulator"), tr("Aircraft\nModel"), tr("Aircraft\nType"), tr("Aircraft\nRegistration")},
          {RIGHT, RIGHT, LEFT, RIGHT, LEFT, LEFT, LEFT, LEFT, LEFT},
          {"dist", "departure_ident", "departure_name", "destination_ident", "destination_name", "simulator", "aircraft_name",
           "aircraft_type", "aircraft_registration"}, 0, Qt::DescendingOrder,
          "select cast(distance * %1 as int) as dist, departure_ident, departure_name, destination_ident, destination_name, "
          "simulator, aircraft_name, aircraft_type, aircraft_registration from logbook"),

    Query(tr("Longest flights by simulator time"),
          {tr("Simulator time\nhours"), tr("From Airport\nICAO"), tr("From Airport\nName"), tr("To Airport\nICAO"), tr("To Airport\nName"),
           tr("Simulator"), tr("Aircraft\nModel"), tr("Aircraft\nType"), tr("Aircraft\nRegistration")},
          {RIGHT, RIGHT, LEFT, RIGHT, LEFT, LEFT, LEFT, LEFT, LEFT},
          {"simtime", "departure_ident", "departure_name", "destination_ident", "destination_name", "simulator", "aircraft_name",
           "aircraft_type", "aircraft_registration"}, 0, Qt::DescendingOrder,
          "select cast((strftime('%s', destination_time_sim) - strftime('%s', departure_time_sim)) / 3600. as double) as simtime, "
          "departure_ident, departure_name, destination_ident, destination_name, "
          "simulator, aircraft_name, aircraft_type, aircraft_registration from logbook where simtime > 0 "),

    Query(tr("Longest flights by real time"),
          {tr("Real time\nhours"), tr("From Airport\nICAO"), tr("From Airport\nName"),
           tr("To Airport\nICAO"), tr("To Airport\nName"),
           tr("Simulator"), tr("Aircraft\nModel"), tr("Aircraft\nType"), tr("Aircraft\nRegistration")},
          {RIGHT, RIGHT, LEFT, RIGHT, LEFT, LEFT, LEFT, LEFT, LEFT},
          {"time", "departure_ident", "departure_name", "destination_ident", "destination_name", "simulator", "aircraft_name",
           "aircraft_type", "aircraft_registration"}, 0, Qt::DescendingOrder,
          "select cast((strftime('%s', destination_time) - strftime('%s', departure_time)) / 3600. as double) as time, "
          "departure_ident, departure_name, destination_ident, destination_name, "
          "simulator, aircraft_name, aircraft_type, aircraft_registration from logbook"),

    Query(tr("Aircraft usage"),
          {tr("Number of\nflights"), tr("Simulator"), tr("Total flight\nplan distance %dist%"),
           tr("Total simulator time\nhours"), tr("Total realtime\nhours"), tr("Model"), tr("Type"), tr(
             "Registration")},
          {RIGHT, LEFT, RIGHT, RIGHT, RIGHT, LEFT, LEFT, LEFT},
          {"cnt", "simulator", "dist", "time", "simtime", "aircraft_name", "aircraft_type", "aircraft_registration"}, 0, Qt::DescendingOrder,
          "select count(1) as cnt, simulator, cast(round(sum(distance) * %1) as int) as dist, "
          "cast(sum((strftime('%s', destination_time) - strftime('%s', departure_time)) / 3600.) as double) as time, "
          "cast(sum(max(strftime('%s', destination_time_sim) - strftime('%s', departure_time_sim), 0) / 3600.) as double) as simtime, "
          "aircraft_name, aircraft_type, aircraft_registration "
          "from logbook group by simulator, aircraft_name, aircraft_type, aircraft_registration"),

    Query(tr("Aircraft usage by type"),
          {tr("Number of\nflights"), tr("Simulator"), tr("Total flight\nplan distance %dist%"), tr("Total realtime\nhours"),
           tr("Total simulator time\nhours"), tr("Type")},
          {RIGHT, LEFT, RIGHT, RIGHT, RIGHT, LEFT},
          {"cnt", "simulator", "dist", "time", "simtime", "aircraft_type"}, 0, Qt::DescendingOrder,
          "select count(1) as cnt, simulator, cast(round(sum(distance) * %1) as int) as dist, "
          "cast(sum((strftime('%s', destination_time) - strftime('%s', departure_time)) / 3600.) as double) as time, "
          "cast(sum(max(strftime('%s', destination_time_sim) - strftime('%s', departure_time_sim), 0) / 3600.) as double) as simtime, "
          "aircraft_type from logbook group by simulator, aircraft_type"),

    Query(tr("Aircraft usage by registration"),
          {tr("Number of\nflights"), tr("Simulator"), tr("Total flight\nplan distance %dist%"), tr("Total realtime\nhours"),
           tr("Total simulator time\nhours"), tr("Type")},
          {RIGHT, LEFT, RIGHT, RIGHT, RIGHT, LEFT},
          {"cnt", "simulator", "dist", "time", "simtime", "aircraft_registration"}, 0, Qt::DescendingOrder,
          "select count(1) as cnt, simulator, cast(round(sum(distance) * %1) as int) as dist, "
          "cast(sum((strftime('%s', destination_time) - strftime('%s', departure_time)) / 3600.) as double) as time, "
          "cast(sum(max(strftime('%s', destination_time_sim) - strftime('%s', departure_time_sim), 0) / 3600.) as double) as simtime, "
          "aircraft_registration from logbook group by simulator, aircraft_registration"),

    Query(tr("Aircraft hours, distance, number of flights flown and more"),
          {tr("Aircraft name"), tr("Aircraft type"), tr("Total flights"), tr("Hours flown"), tr("Average hours flown"),
           tr("Total distance"), tr("Average distance"), tr("First flight"), tr("Last flight")},
          {LEFT, LEFT, RIGHT, RIGHT, RIGHT, RIGHT, RIGHT, RIGHT, RIGHT},
          {"aircraft_name", "aircraft_type", "total_flights", "total_hours", "avg_hours", "total_distance", "avg_distance",
           "last_flight", "first_flight"}, 0, Qt::DescendingOrder,
          "select aircraft_name, aircraft_type, count(*) as total_flights, "
          "sum(hours_flown) as total_hours, avg(hours_flown) as avg_hours, "
          "sum(distance_flown) as total_distance, avg(distance_flown) as avg_distance, "
          "datetime(max(departure_time)) as last_flight, datetime(min(departure_time)) as first_flight "
          "from (select logbook_id, aircraft_name, aircraft_type, departure_time, ifnull(distance_flown,0) as distance_flown, "
          "cast ((julianday(destination_time) - julianday(departure_time)) * 24 as real) as hours_flown "
          "from logbook where departure_time is not null and destination_time is not null) "
          "group by aircraft_name, aircraft_type")
  };
}

void LogStatisticsDialog::showEvent(QShowEvent *)
{
  if(!position.isNull())
    move(position);

  setModel();
  updateWidgets();
}

void LogStatisticsDialog::hideEvent(QHideEvent *)
{
  position = geometry().topLeft();

  // Disconnect from database if not shown
  clearModel();
}
