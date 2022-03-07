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

#include "runwayselection.h"

#include "navapp.h"
#include "common/unit.h"
#include "query/airportquery.h"
#include "geo/calculations.h"
#include "query/mapquery.h"
#include "fs/util/fsutil.h"
#include "gui/itemviewzoomhandler.h"

#include "atools.h"

#include <QLabel>
#include <QTableWidget>

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

RunwaySelection::RunwaySelection(QObject *parent, const map::MapAirport& mapAirport, QTableWidget *runwayTableWidgetParam)
  : QObject(parent), runwayTableWidget(runwayTableWidgetParam)
{
  airport = new map::MapAirport;
  *airport = mapAirport;

  connect(runwayTableWidget, &QTableWidget::itemSelectionChanged, this, &RunwaySelection::itemSelectionChanged);
  connect(runwayTableWidget, &QTableWidget::doubleClicked, this, &RunwaySelection::doubleClicked);

  // Resize widget to get rid of the too large default margins
  zoomHandler = new atools::gui::ItemViewZoomHandler(runwayTableWidget);
}

RunwaySelection::~RunwaySelection()
{
  delete zoomHandler;
  delete airport;
}

void RunwaySelection::restoreState()
{
  fillRunwayList();
  fillAirportLabel();

  QItemSelectionModel *selection = runwayTableWidget->selectionModel();

  if(selection != nullptr)
  {
    selection->clearSelection();

    if(!runways.isEmpty())
      // Select first row
      selection->select(runwayTableWidget->model()->index(0, 0), QItemSelectionModel::Select | QItemSelectionModel::Rows);
  }
}

QString RunwaySelection::getCurrentSelectedName() const
{
  map::MapRunway runway;
  map::MapRunwayEnd end;
  getCurrentSelected(runway, end);
  return end.name;
}

void RunwaySelection::getCurrentSelected(map::MapRunway& runway, map::MapRunwayEnd& end) const
{
  QItemSelectionModel *selection = runwayTableWidget->selectionModel();

  if(selection != nullptr)
  {
    QModelIndexList rows = selection->selectedRows();
    if(!rows.isEmpty())
    {
      QTableWidgetItem *item = runwayTableWidget->item(rows.constFirst().row(), 0);
      if(item != nullptr)
      {
        // Get index and primary from user role data
        int rwIndex = item->data(Qt::UserRole).toInt();

        // Get runway
        runway = runways.at(rwIndex).runway;

        // Get runway end
        end = runways.at(rwIndex).end;
      }
    }
  }
}

const map::MapAirport& RunwaySelection::getAirport() const
{
  return *airport;
}

void RunwaySelection::fillAirportLabel()
{
  if(airportLabel != nullptr)
    airportLabel->setText(tr("<b>%1, elevation %2</b>").
                          arg(map::airportTextShort(*airport)).
                          arg(Unit::altFeet(airport->position.getAltitude())));
}

void RunwaySelection::fillRunwayList()
{
  AirportQuery *airportQuerySim = NavApp::getAirportQuerySim();

  // Get all runways from airport ==================================
  const QList<map::MapRunway> *rw = airportQuerySim->getRunways(airport->id);
  if(rw != nullptr)
  {
    // Append each runway twice with primary and secondary ends and apply filter ==========
    for(const map::MapRunway& r : *rw)
    {
      map::MapRunwayEnd prim = airportQuerySim->getRunwayEndById(r.primaryEndId);
      if(includeRunway(prim.name))
        runways.append(RunwayIdxEntry(r, prim));

      map::MapRunwayEnd sec = airportQuerySim->getRunwayEndById(r.secondaryEndId);
      if(includeRunway(sec.name))
        runways.append(RunwayIdxEntry(r, sec));
    }

    // Sort by length and heading ===================
    std::sort(runways.begin(), runways.end(), [](const RunwayIdxEntry& rw1, const RunwayIdxEntry& rw2) -> bool {
      return atools::almostEqual(rw1.runway.length, rw2.runway.length) ?
      rw1.end.heading<rw2.end.heading : rw1.runway.length> rw2.runway.length;
    });
  }

  runwayTableWidget->clear();
  runwayTableWidget->setDisabled(runways.isEmpty());

  if(runways.isEmpty())
  {
    runwayTableWidget->setSelectionMode(QAbstractItemView::NoSelection);
    runwayTableWidget->clearSelection();

    runwayTableWidget->setRowCount(1);
    runwayTableWidget->setColumnCount(1);
    runwayTableWidget->setItem(0, 0, new QTableWidgetItem(tr("Airport has no runway.")));
  }
  else
  {
    runwayTableWidget->setSelectionMode(QAbstractItemView::SingleSelection);

    // Set table size
    runwayTableWidget->setRowCount(runways.size());
    runwayTableWidget->setColumnCount(5);

    runwayTableWidget->setHorizontalHeaderLabels({tr(" Number "), tr(" Length and Width "), tr(" Heading "), tr(" Surface "),
                                                  tr(" Facilities ")});

    // Index in runway table
    int index = 0;
    for(const RunwayIdxEntry& runway : runways)
      addItem(runway, index++);

    runwayTableWidget->resizeColumnsToContents();
  }
}

void RunwaySelection::addItem(const RunwayIdxEntry& entry, int index)
{
  // Collect attributes
  QStringList atts;

  if(entry.runway.isLighted())
    atts.append(tr("Lighted"));

  if(entry.isPrimary() && entry.runway.primaryClosed)
    atts.append(tr("Closed"));

  if(entry.isSecondary() && entry.runway.secondaryClosed)
    atts.append(tr("Closed"));

  // Add ILS and similar approach aids
  for(const map::MapIls& ils : NavApp::getMapQueryGui()->getIlsByAirportAndRunway(airport->ident, entry.end.name))
    atts.append(map::ilsTypeShort(ils));
  atts.removeAll(QString());
  atts.removeDuplicates();

  if(showPattern && entry.runway.patternAlt > 100.f)
    atts.append(tr("Pattern at %1").arg(Unit::altFeet(entry.runway.patternAlt)));

  float heading = atools::geo::normalizeCourse(entry.end.heading - airport->magvar);

  // Column index in table
  int col = 0;

  // First item with index data attached
  QTableWidgetItem *item = new QTableWidgetItem(entry.end.name);
  item->setData(Qt::UserRole, QVariant(index));
  QFont font = item->font();
  font.setBold(true);
  item->setFont(font);
  runwayTableWidget->setItem(index, col++, item);

  // Dimensions
  item = new QTableWidgetItem(tr("%1 x %2").
                              arg(Unit::distShortFeet(entry.runway.length, false)).arg(Unit::distShortFeet(entry.runway.width)));
  item->setTextAlignment(Qt::AlignRight);
  runwayTableWidget->setItem(index, col++, item);

  // Heading
  item = new QTableWidgetItem(tr("%1°M").arg(QLocale().toString(heading, 'f', 0)));
  item->setTextAlignment(Qt::AlignRight);
  runwayTableWidget->setItem(index, col++, item);

  // Surface
  item = new QTableWidgetItem(map::surfaceName(entry.runway.surface));
  runwayTableWidget->setItem(index, col++, item);

  // Attributes
  item = new QTableWidgetItem(atts.join(", "));
  runwayTableWidget->setItem(index, col++, item);
}

bool RunwaySelection::includeRunway(const QString& runwayName)
{
  if(runwayNameFilter.isEmpty())
    // Include always
    return true;
  else
  {
    for(const QString& filter : runwayNameFilter)
    {
      if(atools::fs::util::runwayEqual(runwayName, filter))
        return true;
    }
  }
  return false;
}
