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

#include "route/parkingdialog.h"

#include "ui_parkingdialog.h"
#include "atools.h"
#include "geo/calculations.h"
#include "common/constants.h"
#include "common/mapcolors.h"
#include "common/unit.h"
#include "gui/helphandler.h"
#include "gui/itemviewzoomhandler.h"
#include "gui/widgetstate.h"
#include "navapp.h"
#include "query/airportquery.h"
#include "query/mapquery.h"

#include <QPushButton>

namespace internal {
struct StartPosition
{
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

  // Update label with airport name/ident
  ui->labelSelectParking->setText(tr("<b>%1, elevation %2:</b>").
                                  arg(map::airportTextShort(departureAirport)).
                                  arg(Unit::altFeet(departureAirport.position.getAltitude())));

  zoomHandler = new atools::gui::ItemViewZoomHandler(ui->tableWidgetSelectParking);

  restoreState();
  updateTable();
  updateButtons();

  connect(ui->tableWidgetSelectParking, &QTableWidget::itemSelectionChanged, this, &ParkingDialog::updateButtons);
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
}

void ParkingDialog::updateTable()
{
  enum
  {
    NAME,
    TYPE,
    SIZE,
    CODES,
    FACILITIES
  };

  entries.clear();

  // Create a copy from the cached start objects to allow sorting ====================================
  AirportQuery *airportQuerySim = NavApp::getAirportQuerySim();
  for(const map::MapStart& start : *airportQuerySim->getStartPositionsForAirport(departureAirport.id))
    entries.append({map::MapParking(), start});

  // Create a copy from the cached parking objects and exclude fuel ====================================
  for(const map::MapParking& parking : *airportQuerySim->getParkingsForAirport(departureAirport.id))
    // Vehicles are already omitted in database creation
    entries.append({parking, map::MapStart()});

  // Sort by type (order: runway, helipad, parking), name and numbers ======================
  std::sort(entries.begin(), entries.end(), [](const internal::StartPosition& p1, const internal::StartPosition& p2) -> bool
  {
    if(p1.parking.isValid() == p2.parking.isValid() && p1.start.isValid() == p2.start.isValid())
    {
      // Same type
      if(p1.parking.isValid())
      {
        // Compare parking
        if(p1.parking.name == p2.parking.name)
          return p1.parking.number < p2.parking.number;
        else
          return p1.parking.name < p2.parking.name;

      }
      else if(p1.start.isValid())
      {
        // Compare start
        if(p1.start.type == p2.start.type)
          return p1.start.runwayName < p2.start.runwayName;
        else
          return p1.start.type > p2.start.type;
      }
    }
    // Sort runway before parking
    return p1.parking.isValid() < p2.parking.isValid();
  });

  QString searchText(ui->lineEditSelectParkingFilter->text());

  if(entries.isEmpty())
  {
    // Empty
    ui->tableWidgetSelectParking->setRowCount(1);
    ui->tableWidgetSelectParking->setColumnCount(1);
    ui->tableWidgetSelectParking->setItem(0, 0, new QTableWidgetItem(tr("No start positions found.")));
    ui->tableWidgetSelectParking->setEnabled(false);
  }
  else
  {
    // Fill table ========================================================
    ui->tableWidgetSelectParking->setEnabled(true);
    ui->tableWidgetSelectParking->setColumnCount(5);
    ui->tableWidgetSelectParking->setRowCount(entries.size()); // Rows will be reset later if smaller due to filter
    ui->tableWidgetSelectParking->setHorizontalHeaderLabels({tr(" Name "), tr(" Type or\nSurface "),
                                                             tr(" Size or\nLength, Width and Heading"),
                                                             tr(" Airline Codes "), tr(" Facilities ")});

    // Add to list widget
    int rowsInserted = 0;
    for(int i = 0; i < entries.size(); i++)
    {
      const internal::StartPosition& startPos = entries.at(i);
      QVector<QTableWidgetItem *> items(5, nullptr);
      if(startPos.parking.isValid())
      {
        // Parking position =======================================================================
        items[NAME] = new QTableWidgetItem(mapcolors::iconForParkingType(startPos.parking.type),
                                           map::parkingNameOrNumber(startPos.parking));
        items[TYPE] = new QTableWidgetItem(map::parkingTypeName(startPos.parking.type));
        items[SIZE] = new QTableWidgetItem(Unit::distShortFeet(startPos.parking.getRadius() * 2));
        items[CODES] = new QTableWidgetItem(startPos.parking.airlineCodes.split(",").join(tr(", ")));
        items[FACILITIES] = new QTableWidgetItem(startPos.parking.jetway ? tr("Has Jetway") : QString());
      }
      else if(startPos.start.isValid())
      {
        // Other start position - runway or helipad ===============================================
        items[NAME] = new QTableWidgetItem(mapcolors::iconForStart(startPos.start),
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
                items[TYPE] = new QTableWidgetItem(map::surfaceName(runway.surface));
                items[SIZE] = new QTableWidgetItem(tr("%1 x %2, %3°M").
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
              items[FACILITIES] = new QTableWidgetItem(atts.join(tr(",")));
            }
          }
        }
      }
      else
        items[0] = new QTableWidgetItem(tr("Invalid position"));

      bool match = true;
      if(!searchText.isEmpty())
      {
        // Check if any column in an entry has a text match
        match = false;
        for(int c : {NAME, TYPE, CODES, FACILITIES})
        {
          if(items.at(c) != nullptr && items.at(c)->text().contains(searchText, Qt::CaseInsensitive))
          {
            match = true;
            break;
          }
        }
      }

      if(match)
      {
        // First column in bold font
        if(items.at(NAME) != nullptr)
        {
          QFont font = items.at(NAME)->font();
          font.setBold(true);
          items.at(NAME)->setFont(font);
        }

        // Align size right
        if(items.at(SIZE) != nullptr)
          items.at(SIZE)->setTextAlignment(Qt::AlignRight);

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

    // Re-adjust row count
    ui->tableWidgetSelectParking->setRowCount(rowsInserted);

    // Restore header or adjust column widths
    atools::gui::WidgetState state(lnm::ROUTE_PARKING_DIALOG);
    if(state.contains(ui->tableWidgetSelectParking))
      state.restore(ui->tableWidgetSelectParking);
    else
      ui->tableWidgetSelectParking->resizeColumnsToContents();
  } // if(entries.isEmpty()) {} else ...
}

bool ParkingDialog::getSelectedParking(map::MapParking& parking) const
{
  if(ui->tableWidgetSelectParking->currentItem() != nullptr)
  {
    parking = entries.at(ui->tableWidgetSelectParking->currentRow()).parking;
    if(parking.isValid())
      return true;
  }
  return false;
}

bool ParkingDialog::getSelectedStartPosition(map::MapStart& start) const
{
  if(ui->tableWidgetSelectParking->currentItem() != nullptr)
  {
    start = entries.at(ui->tableWidgetSelectParking->currentRow()).start;
    if(start.isValid())
      return true;
  }
  return false;
}

void ParkingDialog::saveState()
{
  atools::gui::WidgetState(lnm::ROUTE_PARKING_DIALOG).save({this, ui->tableWidgetSelectParking});
}

void ParkingDialog::restoreState()
{
  atools::gui::WidgetState(lnm::ROUTE_PARKING_DIALOG).restore({this, ui->tableWidgetSelectParking});
}

void ParkingDialog::updateButtons()
{
  ui->buttonBoxSelectParking->button(QDialogButtonBox::Ok)->setEnabled(
    ui->tableWidgetSelectParking->currentItem() != nullptr);
}
