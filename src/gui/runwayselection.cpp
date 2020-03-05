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
#include "gui/itemviewzoomhandler.h"

#include "atools.h"

#include <QLabel>
#include <QTableWidget>

RunwaySelection::RunwaySelection(QObject *parent, const map::MapAirport& mapAirport, QTableWidget *runwayTableParam)
  : QObject(parent), airport(mapAirport), runwayTable(runwayTableParam)
{
  connect(runwayTable, &QTableWidget::itemSelectionChanged, this, &RunwaySelection::itemSelectionChanged);
  connect(runwayTable, &QTableWidget::doubleClicked, this, &RunwaySelection::doubleClicked);

  // Resize widget to get rid of the too large default margins
  zoomHandler = new atools::gui::ItemViewZoomHandler(runwayTable);
}

RunwaySelection::~RunwaySelection()
{
  delete zoomHandler;
}

void RunwaySelection::restoreState()
{
  fillRunwayList();
  fillAirportLabel();

  QItemSelectionModel *selection = runwayTable->selectionModel();

  if(selection != nullptr)
  {
    // Select first row
    selection->clearSelection();
    selection->select(runwayTable->model()->index(0, 0), QItemSelectionModel::Select | QItemSelectionModel::Rows);
  }
}

void RunwaySelection::getCurrentSelected(map::MapRunway& runway, map::MapRunwayEnd& end) const
{
  QItemSelectionModel *selection = runwayTable->selectionModel();

  if(selection != nullptr)
  {
    QModelIndexList rows = selection->selectedRows();
    if(!rows.isEmpty())
    {
      QTableWidgetItem *item = runwayTable->item(rows.first().row(), 0);
      if(item != nullptr)
      {
        // Get index and primary from user role data
        QVariantList data = item->data(Qt::UserRole).toList();
        int rwIndex = data.at(0).toInt();
        bool primary = data.at(1).toBool();

        // Get runway
        runway = runways.at(rwIndex);

        // Get runway end
        end = NavApp::getAirportQuerySim()->getRunwayEndById(primary ? runway.primaryEndId : runway.secondaryEndId);
      }
    }
  }
}

void RunwaySelection::fillAirportLabel()
{
  if(airportLabel != nullptr)
    airportLabel->setText(tr("<b>%1, elevation %2</b>").
                          arg(map::airportTextShort(airport)).
                          arg(Unit::altFeet(airport.position.getAltitude())));
}

void RunwaySelection::fillRunwayList()
{
  // Get all runways from airport
  const QList<map::MapRunway> *rw = NavApp::getAirportQuerySim()->getRunways(airport.id);
  if(rw != nullptr)
    runways = *rw;

  // Sort by lenght and heading
  std::sort(runways.begin(), runways.end(), [](const map::MapRunway& rw1, const map::MapRunway& rw2) -> bool {
    if(rw1.length == rw2.length)
      return rw1.heading < rw2.heading;
    else
      return rw1.length > rw2.length;
  });

  // Set table size
  runwayTable->setRowCount(runways.size() * 2);
  runwayTable->setColumnCount(5);

  // Index in runway table
  int index = 0;
  for(const map::MapRunway& runway : runways)
  {
    // Primary end
    addItem(runway, index, true);

    // Secondary end
    addItem(runway, index, false);
    index++;
  }
}

void RunwaySelection::addItem(const map::MapRunway& rw, int index, bool primary)
{
  // Collect attributes
  QStringList atts;
  if(rw.isLighted())
    atts.append(tr("Lighted"));

  if(primary && rw.primaryClosed)
    atts.append(tr("Closed"));
  if(!primary && rw.secondaryClosed)
    atts.append(tr("Closed"));
  if(showPattern && rw.patternAlt > 100.f)
    atts.append(tr("Pattern at %1").arg(Unit::altFeet(rw.patternAlt)));

  float heading = atools::geo::normalizeCourse((primary ? rw.heading : atools::geo::opposedCourseDeg(
                                                  rw.heading)) - airport.magvar);

  // Row index in table
  int row = index * 2 + !primary;

  // Column index in table
  int col = 0;

  // First item with index data attached
  QTableWidgetItem *item = new QTableWidgetItem(primary ? rw.primaryName : rw.secondaryName);
  item->setData(Qt::UserRole, QVariantList({index, primary}));
  QFont font = item->font();
  font.setBold(true);
  item->setFont(font);
  runwayTable->setItem(row, col++, item);

  // Dimensions
  item = new QTableWidgetItem(tr("%1 x %2").arg(Unit::distShortFeet(rw.length, false)).
                              arg(Unit::distShortFeet(rw.width)));
  item->setTextAlignment(Qt::AlignRight);
  runwayTable->setItem(row, col++, item);

  // Heading
  item = new QTableWidgetItem(tr("%1°M").arg(QLocale().toString(heading, 'f', 0)));
  item->setTextAlignment(Qt::AlignRight);
  runwayTable->setItem(row, col++, item);

  // Surface
  item = new QTableWidgetItem(map::surfaceName(rw.surface));
  runwayTable->setItem(row, col++, item);

  // Attributes
  item = new QTableWidgetItem(atts.join(", "));
  runwayTable->setItem(row, col++, item);
}
