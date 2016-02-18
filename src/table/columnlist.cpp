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

#include "table/columnlist.h"
#include "logging/loggingdefs.h"

#include <QLineEdit>
#include <QComboBox>

ColumnList::ColumnList()
{
  tablename = "airport";
  // Default view column descriptors
  columns.append(Column("airport_id", tr("ID")).
                 canSort().defaultCol().defaultSort().defaultSortOrder(Qt::SortOrder::DescendingOrder));

  columns.append(Column("ident",
                        tr("ICAO")).canSort().canFilter().defaultCol());

  columns.append(Column("name",
                        tr("Name")).canSort().canFilter().defaultCol());

  columns.append(Column("city",
                        tr("City")).canSort().canFilter().defaultCol());
  columns.append(Column("state",
                        tr("State")).canSort().canFilter().defaultCol());
  columns.append(Column("country",
                        tr("Country")).canSort().canFilter().defaultCol());

  // Add names to the index
  for(Column& cd : columns)
    nameColumnMap.insert(cd.getColumnName(), &cd);

  // airport_id
  // file_id
  // ident
  // region
  // name
  // country
  // state
  // city
  // fuel_flags
  // has_avgas
  // has_jetfuel
  // has_tower_object
  // has_tower
  // is_closed
  // is_military
  // is_addon
  // num_boundary_fence
  // num_com
  // num_parking_gate
  // num_parking_ga_ramp
  // num_approach
  // num_runway_hard
  // num_runway_soft
  // num_runway_water
  // num_runway_light
  // num_runway_end_closed
  // num_runway_end_vasi
  // num_runway_end_als
  // num_runway_end_ils
  // num_apron
  // num_taxi_path
  // num_helipad
  // num_jetway
  // longest_runway_length
  // longest_runway_width
  // longest_runway_heading
  // longest_runway_surface
  // num_runways
  // largest_parking_ramp
  // largest_parking_gate
  // rating
  // scenery_local_path
  // bgl_filename
  // left_lonx
  // top_laty
  // right_lonx
  // bottom_laty
  // mag_var
  // tower_altitude
  // tower_lonx
  // tower_laty
  // altitude
  // lonx
  // laty

}

ColumnList::~ColumnList()
{

}

const Column *ColumnList::getColumn(const QString& field) const
{
  if(nameColumnMap.contains(field))
    return nameColumnMap.value(field);
  else
    return nullptr;
}

void ColumnList::assignLineEdit(const QString& field, QLineEdit *edit)
{
  if(nameColumnMap.contains(field))
    nameColumnMap[field]->lineEdit(edit);
  else
    qWarning() << "Cannot assign line edit to" << field;
}

void ColumnList::assignComboBox(const QString& field, QComboBox *combo)
{
  if(nameColumnMap.contains(field))
    nameColumnMap[field]->comboBox(combo);
  else
    qWarning() << "Cannot assign combo box to" << field;
}

void ColumnList::clearWidgets(const QStringList& exceptColNames)
{
  for(Column& cd : columns)
    if(!exceptColNames.contains(cd.getColumnName()))
    {
      if(cd.getLineEditWidget() != nullptr)
        cd.getLineEditWidget()->setText("");
      else if(cd.getComboBoxWidget() != nullptr)
        cd.getComboBoxWidget()->setCurrentIndex(0);
    }
}

void ColumnList::enableWidgets(bool enabled, const QStringList& exceptColNames)
{
  for(Column& cd : columns)
    if(!exceptColNames.contains(cd.getColumnName()))
    {
      if(cd.getLineEditWidget() != nullptr)
        cd.getLineEditWidget()->setEnabled(enabled);
      else if(cd.getComboBoxWidget() != nullptr)
        cd.getComboBoxWidget()->setEnabled(enabled);
    }
}


