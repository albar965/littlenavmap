/*****************************************************************************
* Copyright 2015-2022 Alexander Barthel alex@littlenavmap.org
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

#ifndef LNM_SQLMODELTYPES_H
#define LNM_SQLMODELTYPES_H

#include <QVariant>
#include <functional>

class Column;

namespace sqlmodeltypes {

/* Search direction. This is not the precise direction but an approximation where the ranges overlap.
 * E.g. EAST is 22.5f <= heading && heading <= 157.5f */
enum SearchDirection
{
  /* Numbers have to match index in the combo box */
  ALL = 0,
  NORTH = 1,
  EAST = 2,
  SOUTH = 3,
  WEST = 4
};

/*
 * Callback function/method type defintion.
 * @param colIndex Column index
 * @param rowIndex Row index
 * @param col column Descriptor
 * @param roleValue Role value depending on role like color, font, etc.
 * @param displayRoleValue actual column data
 * @param role Data role
 * @return a variant. Mostly string for display role.
 */
typedef std::function<QVariant(int colIndex, int rowIndex, const Column *col, const QVariant& roleValue,
                               const QVariant& displayRoleValue, Qt::ItemDataRole role)> DataFunctionType;

}

#endif // LNM_SQLMODELTYPES_H
