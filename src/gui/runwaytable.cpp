/*****************************************************************************
* Copyright 2015-2026 Alexander Barthel alex@littlenavmap.org
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

#include "runwaytable.h"

#include "app/navapp.h"
#include "atools.h"
#include "common/formatter.h"
#include "common/symbolpainter.h"
#include "common/unit.h"
#include "fs/util/fsutil.h"
#include "geo/calculations.h"
#include "gui/tools.h"
#include "gui/widgetzoomhandler.h"
#include "query/airportquery.h"
#include "query/mapquery.h"
#include "query/querymanager.h"
#include "weather/weatherreporter.h"

#include <QLabel>
#include <QTableWidget>
#include <QHeaderView>

// Index into runways list - set to DATA_AIRPORT if airport. Data is set at first icon column
const int DATA_ROLE = Qt::UserRole;

// Data is attached to first column
const int DATA_ROLE_COLUMN = 0;
const int DATA_AIRPORT = -1;

struct RunwayIdxEntry
{
  RunwayIdxEntry(map::MapRunway runwayParam, map::MapRunwayEnd endParam)
    : runway(runwayParam), end(endParam)
  {
  }

  RunwayIdxEntry()
  {
  }

  bool isPrimary() const
  {
    return end.id == runway.primaryEndId;
  }

  bool isSecondary() const
  {
    return end.id == runway.secondaryEndId;
  }

  map::MapRunway runway;
  map::MapRunwayEnd end;
};

RunwayTable::RunwayTable(QObject *parent, const map::MapAirport& mapAirport, QTableWidget *runwayTableWidgetParam,
                         bool navdataParam, bool addAirportParam)
  : QObject(parent), runwayTableWidget(runwayTableWidgetParam), navdata(navdataParam), addAirport(addAirportParam)
{
  airport = new map::MapAirport;
  *airport = mapAirport;

  mapQuery = QueryManager::instance()->getQueriesGui()->getMapQuery();

  if(navdata)
    mapQuery->getAirportNavReplace(*airport);
  else
    mapQuery->getAirportSimReplace(*airport);

  connect(runwayTableWidget, &QTableWidget::itemSelectionChanged, this, &RunwayTable::itemSelectionChanged);
  connect(runwayTableWidget, &QTableWidget::doubleClicked, this, &RunwayTable::doubleClicked);

  // Resize widget to get rid of the too large default margins
  zoomHandler = new atools::gui::WidgetZoomHandler(runwayTableWidget);
  atools::gui::adjustSelectionColors(runwayTableWidget);
}

RunwayTable::~RunwayTable()
{
  delete zoomHandler;
  delete airport;
}

void RunwayTable::init()
{
  fillRunwayList();
  fillAirportLabel();

  QItemSelectionModel *selection = runwayTableWidget->selectionModel();

  if(selection != nullptr && !runways.isEmpty())
  {
    selection->clearSelection();

    // Try to find runway end and select row =======================================
    int rowToSelect = 0;
    if(runwayEndIdPreSelected != -1)
    {
      for(int i = 0; i < runwayTableWidget->rowCount(); i++)
      {
        int row = runwayTableWidget->item(i, DATA_ROLE_COLUMN)->data(DATA_ROLE).toInt();
        if(row != -1 && runways.at(row).end.id == runwayEndIdPreSelected)
        {
          rowToSelect = row + (addAirport ? 1 : 0);
          break;
        }
      }
    }

    selection->select(runwayTableWidget->model()->index(rowToSelect, 0), QItemSelectionModel::SelectCurrent | QItemSelectionModel::Rows);
  }
}

void RunwayTable::setPreSelectedRunwayEnd(const QString& name, QString *error)
{
  QList<map::MapRunwayEnd> runwayEnds;
  QueryManager::instance()->getQueriesGui()->getMapQuery()->getRunwayEndByNameFuzzy(runwayEnds, name, *airport, navdata /* navdata */);

  if(runwayEnds.size() >= 1)
  {
    setPreSelectedRunwayEnd(runwayEnds.constFirst().id);

    if(runwayEnds.size() > 1 && error != nullptr)
      *error = tr("Found more than one runway end matching name \"%1\" for airport \"%2\"").arg(name).arg(airport->ident);
  }
  else if(runwayEnds.isEmpty() && error != nullptr)
    *error = tr("Found no runway end matching name \"%1\" for airport \"%2\"").arg(name).arg(airport->ident);
}

QString RunwayTable::getCurrentSelectedName(bool *airportSelected) const
{
  map::MapRunway runway;
  map::MapRunwayEnd end;
  getCurrentSelected(runway, end, airportSelected);
  return end.name;
}

void RunwayTable::getCurrentSelected(map::MapRunway& runway, map::MapRunwayEnd& end, bool *airportSelected) const
{
  QItemSelectionModel *selection = runwayTableWidget->selectionModel();

  if(airportSelected != nullptr)
    *airportSelected = false;

  if(selection != nullptr)
  {
    QModelIndexList rows = selection->selectedRows();
    if(!rows.isEmpty())
    {
      QTableWidgetItem *item = runwayTableWidget->item(rows.constFirst().row(), DATA_ROLE_COLUMN);
      if(item != nullptr)
      {
        // Get index and primary from user role data
        int index = item->data(DATA_ROLE).toInt();

#ifdef DEBUG_INFORMATION
        qDebug() << Q_FUNC_INFO << item->text() << index;
#endif

        if(index == DATA_AIRPORT)
        {
          if(airportSelected != nullptr)
            *airportSelected = true;
        }
        else
        {
          // Get runway
          runway = runways.at(index).runway;

          // Get runway end
          end = runways.at(index).end;
        }
      }
    }
  }
}

const map::MapAirport& RunwayTable::getAirport() const
{
  return *airport;
}

bool RunwayTable::hasRunways() const
{
  return !runways.isEmpty();
}

void RunwayTable::fillAirportLabel()
{
  QString label;
  if(airportLabel != nullptr)
  {
    label = tr("<p><b>%1, elevation %2</b></p>").arg(map::airportTextShort(*airport)).arg(Unit::altFeet(airport->position.getAltitude()));

    QString title, runwayText, sourceText;
    NavApp::getWeatherReporter()->getBestRunwaysTextShort(title, runwayText, sourceText, *airport);
    if(!sourceText.isEmpty())
      label.append(tr("<p>%1</p>").arg(atools::strJoin({title, runwayText, sourceText}, tr(" "))));
  }

  airportLabel->setText(label);
}

void RunwayTable::fillRunwayList()
{
  const Queries *queries = QueryManager::instance()->getQueriesGui();
  AirportQuery *airportQuery = queries->getAirportQuery(navdata);

  // Get all runways from airport ==================================
  const QList<map::MapRunway> *runwayList = airportQuery->getRunways(airport->id);
  if(runwayList != nullptr)
  {
    // Append each runway twice with primary and secondary ends and apply filter ==========
    for(const map::MapRunway& r : *runwayList)
    {
      map::MapRunwayEnd prim = airportQuery->getRunwayEndById(r.primaryEndId);
      if(includeRunway(prim.name))
        runways.append(RunwayIdxEntry(r, prim));

      map::MapRunwayEnd sec = airportQuery->getRunwayEndById(r.secondaryEndId);
      if(includeRunway(sec.name))
        runways.append(RunwayIdxEntry(r, sec));
    }

    // Sort by length and normalized name ===================
    std::sort(runways.begin(), runways.end(), [](const RunwayIdxEntry& rw1, const RunwayIdxEntry& rw2) -> bool {
      if(atools::almostEqual(rw1.runway.length, rw2.runway.length))
        return atools::fs::util::runwayNamePrefixZero(rw1.end.name) < atools::fs::util::runwayNamePrefixZero(rw2.end.name);
      else
        // Longest on top
        return rw1.runway.length > rw2.runway.length;
    });
  }

  runwayTableWidget->clear();
  runwayTableWidget->setDisabled(runways.isEmpty());

  if(runways.isEmpty())
  {
    if(addAirport)
    {
      // No runways but airport to be shown - should never appear
      runwayTableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
      runwayTableWidget->setRowCount(1);
      runwayTableWidget->setColumnCount(1);
      runwayTableWidget->setHorizontalHeaderLabels({tr("Airport Name and Ident")});
      addAirportItem(*airport, 0);
    }
    else
    {
      // No runways and no airport to be shown - should never appear
      runwayTableWidget->setSelectionMode(QAbstractItemView::NoSelection);
      runwayTableWidget->clearSelection();
      runwayTableWidget->setRowCount(1);
      runwayTableWidget->setColumnCount(1);
      runwayTableWidget->setItem(0, 0, new QTableWidgetItem(tr("Airport has no runway.")));
    }
  }
  else
  {
    // Build header information
    const QStringList header({QStringLiteral(), // Icon
                              addAirport ? tr(" Runway Number or\nAirport Name and Ident") : tr(" Runway Number"),
                              tr(" Length and Width\n%1").arg(Unit::getUnitShortDistStr()),
                              tr(" Heading\n°M", "Runway heading"),
                              tr(" Surface "),
                              tr(" Facilities "),
                              tr(" Head- and Crosswind\n%1").arg(Unit::getUnitSpeedStr())});

    const QStringList headerTooltips({QStringLiteral(), // Icon
                                      addAirport ? tr(" Runway Number or Airport Name and Ident. ") : tr(" Runway Number."),
                                      tr("Length and width of runway in %1.").arg(Unit::getUnitShortDistStr()),
                                      tr("Runway heading in degree magnetic."),
                                      QStringLiteral(),
                                      QStringLiteral(),
                                      tr("Head- and crosswind components for runway in %1.\n"
                                         "Weather source is selected in menu \"Weather\" -> \"Airport Weather Source\".\n"
                                         "Tailwinds are omitted.").arg(Unit::getUnitSpeedStr())});

    Q_ASSERT(header.size() == headerTooltips.size());

    runwayTableWidget->setSelectionMode(QAbstractItemView::SingleSelection);

    // Set table size ===================================================
    runwayTableWidget->setRowCount(runways.size() + (addAirport ? 1 : 0));
    runwayTableWidget->setColumnCount(header.size());

    // Fill header texts ===================================================
    runwayTableWidget->setHorizontalHeaderLabels(header);
    for(int i = 0; i < headerTooltips.size(); i++)
    {
      QTableWidgetItem *item = runwayTableWidget->horizontalHeaderItem(i);

      if(item != nullptr)
        item->setToolTip(headerTooltips.at(i));
      else
        qWarning() << Q_FUNC_INFO << "Item" << i << "is null";
    }

    // Fetch airport wind ===================================================
    float windSpeedKts, windDirectionDeg;
    NavApp::getAirportMetarWind(windDirectionDeg, windSpeedKts, *airport, false /* stationOnly */);

    int tableIndex = 0; // Index in runway table
    if(addAirport)
      addAirportItem(*airport, tableIndex++);

    // Fill items ===================================================
    int runwayListIndex = 0;
    for(const RunwayIdxEntry& runway : std::as_const(runways))
      addRunwayItem(runway, formatter::windInformationShort(windDirectionDeg, windSpeedKts, runway.end.heading),
                    tableIndex++, runwayListIndex++);

    runwayTableWidget->resizeColumnsToContents();
  } // if(runways.isEmpty()) ... else
}

void RunwayTable::addAirportItem(const map::MapAirport& airport, int tableIndex)
{
  // Icon without text
  QTableWidgetItem *item =
    new QTableWidgetItem(SymbolPainter::createAirportIcon(airport, runwayTableWidget->verticalHeader()->defaultSectionSize()),
                         QStringLiteral());

  item->setData(DATA_ROLE, QVariant(DATA_AIRPORT));
  runwayTableWidget->setItem(tableIndex, 0, item);

  item = new QTableWidgetItem(map::airportTextShort(airport, 100, false /* includeIdent */));
  QFont font = item->font();
  font.setBold(true);
  item->setFont(font);
  runwayTableWidget->setItem(tableIndex, 1, item);
}

void RunwayTable::addRunwayItem(const RunwayIdxEntry& entry, const QString& windText, int tableIndex, int runwayListIndex)
{
  static const QIcon icon(QStringLiteral(":/littlenavmap/resources/icons/startrunway.svg"));

  // Collect attributes
  QStringList atts;
  const map::MapRunway& runway = entry.runway;

  if(runway.hasEdgeLight())
    atts.append(tr("Lighted"));

  if(entry.isPrimary() && runway.primaryClosed)
    atts.append(tr("Closed"));

  if(entry.isSecondary() && runway.secondaryClosed)
    atts.append(tr("Closed"));

  // Add ILS and similar approach aids
  for(const map::MapIls& ils : mapQuery->getIlsByAirportAndRunway(airport->ident, entry.end.name))
    atts.append(map::ilsTypeShort(ils));
  atts.removeAll(QStringLiteral());
  atts.removeDuplicates();

  if(showPattern && runway.patternAlt > 100.f)
    atts.append(tr("Pattern at %1").arg(Unit::altFeet(runway.patternAlt)));

  float heading = atools::geo::normalizeCourse(entry.end.heading - airport->magvar);

  // Column index in table
  int col = 0;
  // Icon =======================
  QTableWidgetItem *item = new QTableWidgetItem(icon, QStringLiteral());
  item->setData(DATA_ROLE, QVariant(runwayListIndex));
  runwayTableWidget->setItem(tableIndex, col++, item);

  // First item with index data attached ============================
  item = new QTableWidgetItem(entry.end.name);
  QFont font = item->font();
  font.setBold(true);
  item->setFont(font);
  runwayTableWidget->setItem(tableIndex, col++, item);

  // Dimensions
  item = new QTableWidgetItem(tr("%1 x %2").
                              arg(Unit::distShortFeet(runway.length, false /* addUnit */)).
                              arg(Unit::distShortFeet(runway.width, false /* addUnit */)));
  item->setTextAlignment(Qt::AlignRight);
  runwayTableWidget->setItem(tableIndex, col++, item);

  // Heading
  item = new QTableWidgetItem(QLocale().toString(heading, 'f', 0));
  item->setTextAlignment(Qt::AlignRight);
  runwayTableWidget->setItem(tableIndex, col++, item);

  // Surface
  item = new QTableWidgetItem(map::surfaceName(runway.surface));
  runwayTableWidget->setItem(tableIndex, col++, item);

  // Attributes
  item = new QTableWidgetItem(atts.join(", "));
  runwayTableWidget->setItem(tableIndex, col++, item);

  // Headwind and Crosswind
  item = new QTableWidgetItem(windText);
  runwayTableWidget->setItem(tableIndex, col++, item);
}

bool RunwayTable::includeRunway(const QString& runwayName)
{
  if(runwayNameFilter.isEmpty())
    // Include always
    return true;
  else
  {
    for(const QString& filter : std::as_const(runwayNameFilter))
    {
      if(atools::fs::util::runwayEqual(runwayName, filter, false /* fuzzy */))
        return true;
    }
  }
  return false;
}
