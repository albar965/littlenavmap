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

#include "route/parkingdialog.h"

#include "ui_parkingdialog.h"
#include "mapgui/mapquery.h"
#include "common/mapcolors.h"
#include "atools.h"
#include "common/unit.h"

#include <QPushButton>


ParkingDialog::ParkingDialog(QWidget *parent, MapQuery *mapQuery,
                             const maptypes::MapAirport& departureAirport)
  : QDialog(parent), ui(new Ui::ParkingDialog)
{
  ui->setupUi(this);

  // Update label with airport name/ident
  ui->labelSelectParking->setText(ui->labelSelectParking->text().arg(maptypes::airportText(departureAirport)));

  const QList<maptypes::MapStart> *startCache = mapQuery->getStartPositionsForAirport(departureAirport.id);
  // Create a copy from the cached start objects to allow sorting
  for(const maptypes::MapStart& start : *startCache)
    entries.append({maptypes::MapParking(), start});

  const QList<maptypes::MapParking> *parkingCache = mapQuery->getParkingsForAirport(departureAirport.id);
  // Create a copy from the cached parking objects and exclude fuel
  for(const maptypes::MapParking& parking : *parkingCache)
    // Vehicles are already omitted in database creation
    entries.append({parking, maptypes::MapStart()});

  // Sort by type (order: runway, helipad, parking), name and numbers
  std::sort(entries.begin(), entries.end(),
            [ = ](const StartPosition &p1, const StartPosition &p2)->bool
            {
              if(p1.parking.position.isValid() == p2.parking.position.isValid() &&
                 p1.start.position.isValid() == p2.start.position.isValid())
              {
                // Same type
                if(p1.parking.position.isValid())
                {
                  // Compare parking
                  if(p1.parking.name == p2.parking.name)
                    return p1.parking.number < p2.parking.number;
                  else
                    return p1.parking.name < p2.parking.name;

                }
                else if(p1.start.position.isValid())
                {
                  // Compare start
                  if(p1.start.type == p2.start.type)
                  {
                    if(p1.start.runwayName == p2.start.runwayName)
                      return p1.start.helipadNumber < p2.start.helipadNumber;
                    else
                      return p1.start.runwayName < p2.start.runwayName;
                  }
                  else
                    return p1.start.type > p2.start.type;
                }
              }
              // Sort runway before parking
              return p1.parking.position.isValid() < p2.parking.position.isValid();
            });

  if(entries.isEmpty())
  {
    new QListWidgetItem(tr("No start positions found."), ui->listWidgetSelectParking);
    ui->listWidgetSelectParking->setEnabled(false);
  }
  else
  {
    // Add to list widget
    for(const StartPosition& startPos : entries)
    {
      if(startPos.parking.position.isValid())
      {
        QString text = tr("%1, %2%3").arg(maptypes::parkingNameNumberType(startPos.parking)).
                       arg(Unit::distShortFeet(startPos.parking.radius * 2)).
                       arg((startPos.parking.jetway ? tr(", Has Jetway") : QString()));

        // Item will insert itself in list widget
        new QListWidgetItem(
          mapcolors::iconForParkingType(startPos.parking.type), text, ui->listWidgetSelectParking);
      }
      else if(startPos.start.position.isValid())
      {
        QString number;
        if(startPos.start.helipadNumber > 0)
          number = QString::number(startPos.start.helipadNumber);

        QString text = tr("%1 %2 %3").
                       arg(atools::capString(startPos.start.type)).
                       arg(startPos.start.runwayName).
                       arg(number);

        new QListWidgetItem(mapcolors::iconForStartType(
                              startPos.start.type), text, ui->listWidgetSelectParking);
      }
    }
    ui->listWidgetSelectParking->setEnabled(true);
  }

  updateButtons();
  connect(ui->listWidgetSelectParking, &QListWidget::itemSelectionChanged, this,
          &ParkingDialog::updateButtons);

  // Activated on double click or return
  connect(ui->listWidgetSelectParking, &QListWidget::itemActivated, this, &QDialog::accept);

  connect(ui->buttonBoxSelectParking, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(ui->buttonBoxSelectParking, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

ParkingDialog::~ParkingDialog()
{
  delete ui;
}

bool ParkingDialog::getSelectedParking(maptypes::MapParking& parking)
{
  if(ui->listWidgetSelectParking->currentItem() != nullptr)
  {
    parking = entries.at(ui->listWidgetSelectParking->currentRow()).parking;
    if(parking.position.isValid())
      return true;
  }
  return false;
}

bool ParkingDialog::getSelectedStartPosition(maptypes::MapStart& start)
{
  if(ui->listWidgetSelectParking->currentItem() != nullptr)
  {
    start = entries.at(ui->listWidgetSelectParking->currentRow()).start;
    if(start.position.isValid())
      return true;
  }
  return false;
}

void ParkingDialog::updateButtons()
{
  ui->buttonBoxSelectParking->button(QDialogButtonBox::Ok)->setEnabled(
    ui->listWidgetSelectParking->currentItem() != nullptr);
}
