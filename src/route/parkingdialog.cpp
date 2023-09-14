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

#include "route/parkingdialog.h"

#include "atools.h"
#include "common/constants.h"
#include "common/mapcolors.h"
#include "common/symbolpainter.h"
#include "common/unit.h"
#include "geo/calculations.h"
#include "gui/helphandler.h"
#include "gui/itemviewzoomhandler.h"
#include "gui/tools.h"
#include "gui/widgetstate.h"
#include "app/navapp.h"
#include "query/airportquery.h"
#include "query/mapquery.h"
#include "route/route.h"
#include "ui_parkingdialog.h"

#include <QPushButton>

namespace internal {

enum Column
{
  NAME,
  TYPE,
  SIZE,
  CODES,
  FACILITIES,
  COUNT = FACILITIES + 1
};

/* Object for one table entry. Either start, parking or single airport */
class StartPosition
{
public:
  StartPosition()
  {

  }

  explicit StartPosition(const map::MapAirport& airportParam)
    : airport(airportParam)
  {
  }

  explicit StartPosition(const map::MapParking& parkingParam)
    : parking(parkingParam)
  {
  }

  explicit StartPosition(const map::MapStart& startParam)
    : start(startParam)
  {
  }

  map::MapAirport airport;
  map::MapParking parking;
  map::MapStart start;
};

}

ParkingDialog::ParkingDialog(QWidget *parent, const map::MapAirport& departureAirportParam)
  : QDialog(parent), ui(new Ui::ParkingDialog), departureAirport(departureAirportParam)
{
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
  setWindowModality(Qt::ApplicationModal);

  ui->setupUi(this);

  zoomHandler = new atools::gui::ItemViewZoomHandler(ui->tableWidgetSelectParking);
  atools::gui::adjustSelectionColors(ui->tableWidgetSelectParking);

  restoreState();
  updateTable();
  updateTableSelection();
  updateButtonsAndHeader();

  connect(ui->tableWidgetSelectParking, &QTableWidget::itemSelectionChanged, this, &ParkingDialog::updateButtonsAndHeader);
  connect(ui->buttonBoxSelectParking, &QDialogButtonBox::clicked, this, &ParkingDialog::buttonBoxClicked);
  connect(ui->tableWidgetSelectParking, &QTableWidget::doubleClicked, this, &ParkingDialog::doubleClicked);
  connect(ui->lineEditSelectParkingFilter, &QLineEdit::textEdited, this, &ParkingDialog::filterTextEdited);
}

ParkingDialog::~ParkingDialog()
{
  saveState();

  delete zoomHandler;
  delete ui;
}

void ParkingDialog::buttonBoxClicked(QAbstractButton *button)
{
  saveState();

  if(button == ui->buttonBoxSelectParking->button(QDialogButtonBox::Ok))
    QDialog::accept();
  else if(button == ui->buttonBoxSelectParking->button(QDialogButtonBox::Cancel))
    QDialog::reject();
  else if(button == ui->buttonBoxSelectParking->button(QDialogButtonBox::Help))
    atools::gui::HelpHandler::openHelpUrlWeb(parentWidget(), lnm::helpOnlineUrl + "PARKINGPOSITION.html", lnm::helpLanguageOnline());
}

void ParkingDialog::doubleClicked()
{
  saveState();
  QDialog::accept();
}

void ParkingDialog::filterTextEdited()
{
  updateTable();
  updateTableSelection();
}

void ParkingDialog::updateTable()
{
  startPositions.clear();

  // Create a copy from the cached start objects to allow sorting ====================================
  AirportQuery *airportQuerySim = NavApp::getAirportQuerySim();
  for(const map::MapStart& start : *airportQuerySim->getStartPositionsForAirport(departureAirport.id))
    startPositions.append(internal::StartPosition(start));

  // Create a copy from the cached parking objects including fuel ====================================
  for(const map::MapParking& parking : *airportQuerySim->getParkingsForAirport(departureAirport.id))
    // Vehicle parking spots are already omitted in database creation
    startPositions.append(internal::StartPosition(parking));

  // Sort copied objects by type (order: runway, helipad, parking), name and numbers ======================
  std::sort(startPositions.begin(), startPositions.end(), [](const internal::StartPosition& p1, const internal::StartPosition& p2) -> bool
  {
    if(p1.parking.isValid() == p2.parking.isValid() && p1.start.isValid() == p2.start.isValid())
    {
      // Same type
      if(p1.parking.isValid())
        // Compare parking
        return std::make_tuple(p1.parking.name, p1.parking.number, p1.parking.suffix) <
        std::make_tuple(p2.parking.name, p2.parking.number, p2.parking.suffix);
      else if(p1.start.isValid())
      {
        // Compare start - first runways by name and then helipads by name
        if(p1.start.type == p2.start.type)
          return p1.start.runwayName < p2.start.runwayName;
        else
          return p1.start.type > p2.start.type;
      }
    }
    // Sort runway before parking
    return p1.parking.isValid() < p2.parking.isValid();
  });

  // Put airport entry always on top independent of sorting
  startPositions.prepend(internal::StartPosition(departureAirport));

  // Text from filter
  QString searchText(ui->lineEditSelectParkingFilter->text());

  // Fill table ========================================================
  ui->tableWidgetSelectParking->setEnabled(true);
  ui->tableWidgetSelectParking->setColumnCount(internal::COUNT);
  ui->tableWidgetSelectParking->setRowCount(startPositions.size()); // Rows will be reset later if smaller due to filter
  ui->tableWidgetSelectParking->setHorizontalHeaderLabels({tr(" Name "), tr(" Type or\nSurface "),
                                                           tr(" Size or\nLength, Width and Heading"),
                                                           tr(" Airline Codes "), tr(" Facilities ")});

  // Add to list widget
  int rowsInserted = 0;
  for(int i = 0; i < startPositions.size(); i++)
  {
    const internal::StartPosition& startPos = startPositions.at(i);
    QVector<QTableWidgetItem *> items(5, nullptr);
    if(startPos.airport.isValid())
    {
      // First airport entry =======================================================================
      QIcon icon = SymbolPainter::createAirportIcon(startPos.airport, ui->tableWidgetSelectParking->verticalHeader()->defaultSectionSize());
      items[internal::NAME] = new QTableWidgetItem(icon, tr("Airport %1").arg(map::airportTextShort(startPos.airport, 20 /* elideName */)));
    }
    else if(startPos.parking.isValid())
    {
      // Parking position =======================================================================
      items[internal::NAME] =
        new QTableWidgetItem(mapcolors::iconForParkingType(startPos.parking.type), map::parkingNameOrNumber(startPos.parking));
      items[internal::TYPE] = new QTableWidgetItem(map::parkingTypeName(startPos.parking.type));
      items[internal::SIZE] = new QTableWidgetItem(Unit::distShortFeet(startPos.parking.getRadius() * 2));
      items[internal::CODES] = new QTableWidgetItem(startPos.parking.airlineCodes.split(",").join(tr(", ")));
      items[internal::FACILITIES] = new QTableWidgetItem(startPos.parking.jetway ? tr("Has Jetway") : QString());
    }
    else if(startPos.start.isValid())
    {
      // Other start position - runway or helipad ===============================================
      items[internal::NAME] = new QTableWidgetItem(mapcolors::iconForStart(startPos.start),
                                                   tr("%1 %2").arg(map::startType(startPos.start)).arg(startPos.start.runwayName));

      if(!startPos.start.runwayName.isEmpty())
      {
        // Runway start ==========================
        map::MapRunwayEnd end = airportQuerySim->getRunwayEndByName(startPos.start.airportId, startPos.start.runwayName);
        if(end.isValid())
        {
          map::MapRunway runway = airportQuerySim->getRunwayByEndId(startPos.start.airportId, end.id);
          if(runway.isValid())
          {
            if(departureAirport.isValid())
            {
              // Add runway information ===============================
              float heading = atools::geo::normalizeCourse(end.heading - departureAirport.magvar);
              items[internal::TYPE] = new QTableWidgetItem(map::surfaceName(runway.surface));
              items[internal::SIZE] = new QTableWidgetItem(tr("%1 x %2, %3°M").
                                                           arg(Unit::distShortFeet(runway.length, false)).
                                                           arg(Unit::distShortFeet(runway.width)).
                                                           arg(QLocale().toString(heading, 'f', 0)));
            }

            // Fill runway attribute list
            QStringList atts;
            if(runway.isLighted())
              atts.append(tr("Lighted"));
            if(!end.secondary && runway.primaryClosed)
              atts.append(tr("Closed"));
            if(end.secondary && runway.secondaryClosed)
              atts.append(tr("Closed"));

            // Add ILS and similar approach aids
            if(departureAirport.isValid())
            {
              for(map::MapIls ils : NavApp::getMapQueryGui()->getIlsByAirportAndRunway(departureAirport.ident, end.name))
                atts.append(map::ilsTypeShort(ils));
            }
            atts.removeAll(QString());
            atts.removeDuplicates();
            items[internal::FACILITIES] = new QTableWidgetItem(atts.join(tr(",")));
          }
        }
      }
    }
    else
      items[internal::NAME] = new QTableWidgetItem(tr("Invalid position"));

    // First item contains index
    items[internal::NAME]->setData(Qt::UserRole, i);

    // Check if any column in an entry has a text match ==============================
    bool match = true;
    if(!searchText.isEmpty())
    {
      // User entered text
      match = false;
      for(int col : {internal::NAME, internal::TYPE, internal::CODES, internal::FACILITIES})
      {
        if(items.at(col) != nullptr && items.at(col)->text().contains(searchText, Qt::CaseInsensitive))
        {
          match = true;
          break;
        }
      }
    }

    // Format and insert all matching items ==============================================
    if(match)
    {
      // First column in bold font
      if(items.at(internal::NAME) != nullptr)
      {
        QFont font = items.at(internal::NAME)->font();
        font.setBold(true);
        items.at(internal::NAME)->setFont(font);
      }

      // Align size right
      if(items.at(internal::SIZE) != nullptr)
        items.at(internal::SIZE)->setTextAlignment(Qt::AlignRight);

      // Add items to table
      for(int col = 0; col < items.size(); col++)
      {
        if(items.at(col) != nullptr)
          ui->tableWidgetSelectParking->setItem(rowsInserted, col, items.at(col));
        else
          ui->tableWidgetSelectParking->setItem(rowsInserted, col, new QTableWidgetItem());
      }
      rowsInserted++;
    }
  } // for(int i = 0; i < entries.size(); i++)

  // Re-adjust / lower row count
  ui->tableWidgetSelectParking->setRowCount(rowsInserted);

  // Restore header or adjust column widths
  atools::gui::WidgetState state(lnm::ROUTE_PARKING_DIALOG);
  if(state.contains(ui->tableWidgetSelectParking))
    state.restore(ui->tableWidgetSelectParking);
  else
    ui->tableWidgetSelectParking->resizeColumnsToContents();
}

void ParkingDialog::updateTableSelection()
{
  // Get current values from route
  map::MapParking parking = NavApp::getRouteConst().getDepartureParking();
  map::MapStart start = NavApp::getRouteConst().getDepartureStart();

  ui->tableWidgetSelectParking->clearSelection();

  for(int row = 0; row < ui->tableWidgetSelectParking->rowCount(); row++)
  {
    QTableWidgetItem *item = ui->tableWidgetSelectParking->item(row, internal::NAME);
    if(item != nullptr)
    {
      // Check if position matches current route values
      const internal::StartPosition pos = startPositions.value(item->data(Qt::UserRole).toInt());

      if( // Parking matches
        (pos.parking.isValid() && parking.name == pos.parking.name && parking.number == pos.parking.number &&
         parking.suffix == pos.parking.suffix) ||
        // Start matches
        (pos.start.isValid() && start.type == pos.start.type && start.runwayName == pos.start.runwayName) ||
        // Neither from route is valid and airport entry
        (!parking.isValid() && !start.isValid() && pos.airport.isValid()))
        ui->tableWidgetSelectParking->selectRow(row);
    }
  }
}

const internal::StartPosition& ParkingDialog::currentPos() const
{
  const static internal::StartPosition EMPTY;

  QTableWidgetItem *item = ui->tableWidgetSelectParking->item(ui->tableWidgetSelectParking->currentRow(), internal::NAME);
  if(item != nullptr)
  {
    int index = item->data(Qt::UserRole).toInt();
    if(atools::inRange(startPositions, index))
      return startPositions.at(index);
  }

  return EMPTY;
}

map::MapParking ParkingDialog::getSelectedParking() const
{
  return currentPos().parking;
}

map::MapStart ParkingDialog::getSelectedStartPosition() const
{
  return currentPos().start;
}

bool ParkingDialog::isAirportSelected() const
{
  return currentPos().airport.isValid();
}

void ParkingDialog::saveState()
{
  atools::gui::WidgetState(lnm::ROUTE_PARKING_DIALOG).save({this, ui->tableWidgetSelectParking});
}

void ParkingDialog::restoreState()
{
  atools::gui::WidgetState(lnm::ROUTE_PARKING_DIALOG).restore({this, ui->tableWidgetSelectParking});
}

void ParkingDialog::updateButtonsAndHeader()
{
  ui->buttonBoxSelectParking->button(QDialogButtonBox::Ok)->setEnabled(ui->tableWidgetSelectParking->currentItem() != nullptr);

  map::MapParking parking = NavApp::getRouteConst().getDepartureParking();
  map::MapStart start = NavApp::getRouteConst().getDepartureStart();

  QString currentSelectedText;
  if(parking.isValid())
    currentSelectedText = tr("<br/>Currently selected in flight plan: %1.").arg(map::parkingNameOrNumber(parking));
  else if(start.isValid())
    currentSelectedText = tr("<br/>Currently selected in flight plan: %1 %2.").arg(map::startType(start)).arg(start.runwayName);
  else
    currentSelectedText = tr("<br/>Airport selected as start position.");

  ui->labelSelectParking->setText(tr("<b>%1, elevation %2</b>%3").
                                  arg(map::airportTextShort(departureAirport)).
                                  arg(Unit::altFeet(departureAirport.position.getAltitude())).arg(currentSelectedText));
}
