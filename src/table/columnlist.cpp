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
  // Use sort function to put nulls at end of the list
  QString nullAtEndsortFunc = "ifnull(%1,'~')";
  QString nullAtEndSortFuncDesc = "ifnull(%1,' ')";

  // Default view column descriptors
  columns.append(Column("logbook_id", tr("Logbook-\nEntry")).
                 canSort().defaultCol().defaultSort().defaultSortOrder(Qt::SortOrder::DescendingOrder));

  columns.append(Column("simulator_id",
                        tr("Simulator")).canGroup().canSort().defaultCol().alwaysAnd());

  columns.append(Column("startdate", tr("Start Time")).
                 canSort().defaultCol());

  columns.append(Column("airport_from_icao",
                        tr("From\nICAO")).canFilter().canGroup().canSort().defaultCol().
                 sortFunc(nullAtEndsortFunc, nullAtEndSortFuncDesc));

//  if(hasAirports)
  {
    columns.append(Column("airport_from_name",
                          tr("From Airport")).canFilter().canGroup().canSort().defaultCol().
                   sortFunc(nullAtEndsortFunc, nullAtEndSortFuncDesc));

    columns.append(Column("airport_from_city",
                          tr("From City")).canFilter().canGroup().canSort().defaultCol().
                   sortFunc(nullAtEndsortFunc, nullAtEndSortFuncDesc));

    columns.append(Column("airport_from_state",
                          tr("From State")).canFilter().canGroup().canSort().defaultCol().
                   sortFunc(nullAtEndsortFunc, nullAtEndSortFuncDesc));

    columns.append(Column("airport_from_country",
                          tr("From Country")).canFilter().canGroup().canSort().defaultCol().
                   sortFunc(nullAtEndsortFunc, nullAtEndSortFuncDesc));

  }

  columns.append(Column("airport_to_icao",
                        tr("To\nICAO")).canFilter().canGroup().canSort().defaultCol().
                 sortFunc(nullAtEndsortFunc, nullAtEndSortFuncDesc));

//  if(hasAirports)
  {
    columns.append(Column("airport_to_name",
                          tr("To Airport")).canFilter().canGroup().canSort().defaultCol().
                   sortFunc(nullAtEndsortFunc, nullAtEndSortFuncDesc));

    columns.append(Column("airport_to_city",
                          tr("To City")).canFilter().canGroup().canSort().defaultCol().
                   sortFunc(nullAtEndsortFunc, nullAtEndSortFuncDesc));

    columns.append(Column("airport_to_state",
                          tr("To State")).canFilter().canGroup().canSort().defaultCol().
                   sortFunc(nullAtEndsortFunc, nullAtEndSortFuncDesc));

    columns.append(Column("airport_to_country",
                          tr("To Country")).canFilter().canGroup().canSort().defaultCol().
                   sortFunc(nullAtEndsortFunc, nullAtEndSortFuncDesc));

    columns.append(Column("distance",
                          tr("Distance\nNM")).canSum().canSort().defaultCol());
  }

  columns.append(Column("description",
                        tr("Flight\nDescription")).canFilter().canGroup().canSort().defaultCol().
                 sortFunc(nullAtEndsortFunc, nullAtEndSortFuncDesc));

  columns.append(Column("total_time",
                        tr("Total Time\nh:mm")).canSum().canSort().defaultCol());

  columns.append(Column("night_time",
                        tr("Night Time\nh:mm")).canSum().canSort().defaultCol());

  columns.append(Column("instrument_time",
                        tr("Instrument\nTime h:mm")).canSum().canSort().defaultCol());

  columns.append(Column("aircraft_reg",
                        tr("Aircraft\nRegistration")).canFilter().canGroup().canSort().defaultCol().
                 sortFunc(nullAtEndsortFunc, nullAtEndSortFuncDesc));

  columns.append(Column("aircraft_descr",
                        tr("Aircraft\nDescription")).canFilter().canGroup().canSort().defaultCol());

  // Do aircraft type sorting by localized type name
  QString aircraftTypeSort = QString(
    "CASE WHEN aircraft_type = 0 THEN '%1' "
    "WHEN aircraft_type = 1 THEN '%2' "
    "WHEN aircraft_type = 2 THEN '%3' "
    "WHEN aircraft_type = 3 THEN '%4' "
    "WHEN aircraft_type = 4 THEN '%5'"
    "ELSE '%6' END").arg(tr("Unknown")).arg(tr("Glider")).
                             arg(tr("Fixed Wing")).arg(tr("Amphibious")).
                             arg(tr("Rotor")).arg(tr("Unknown"));

  columns.append(Column("aircraft_type",
                        tr("Aircraft\nType")).canGroup().canSort().defaultCol().
                 sortFunc(aircraftTypeSort, aircraftTypeSort));

  columns.append(Column("aircraft_flags",
                        tr("Aircraft\nInformation")).canGroup().canSort().defaultCol());

  columns.append(Column("visits", tr("Visits\nAirport/Landings, ...")).defaultCol());

  // Column descriptors for grouped views
  columns.append(Column("total_time_sum", tr("Total Time all\nh:mm")).canSort());
  columns.append(Column("night_time_sum", tr("Night Time all\nh:mm")).canSort());
  columns.append(Column("instrument_time_sum", tr("Instrument\nTime all h:mm")).canSort());
  columns.append(Column("distance_sum", tr("Total Distance\nNM")).canSort());
  columns.append(Column("num_flights", tr("Number of\nFlights")).canSort());

  // Add names to the index
  for(Column& cd : columns)
    nameColumnMap.insert(cd.getColumnName(), &cd);
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
