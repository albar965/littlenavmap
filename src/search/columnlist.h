/*****************************************************************************
* Copyright 2015-2018 Alexander Barthel albar965@mailbox.org
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

#ifndef LITTLENAVMAP_COLUMNLIST_H
#define LITTLENAVMAP_COLUMNLIST_H

#include <QHash>
#include <QObject>
#include <QVector>
#include <QStringList>

class QWidget;
class Column;
class QSpinBox;
class QComboBox;
class QCheckBox;
class QPushButton;

/*
 * A list of column descriptors that define behavior and display in the table
 * view for one database table.
 */
class ColumnList
{
public:
  ColumnList(const QString& tableName, const QString& idColumnName);
  virtual ~ColumnList();

  /* Get column descriptor for the given query column name or alias */
  const Column *getColumn(int index) const;
  const Column *getColumn(const QString& field) const;
  bool hasColumn(const QString& field) const;
  const Column *getIdColumn() const;

  /* Get the default sort that is used after reset view */
  const Column *getDefaultSortColumn() const;

  /* Get all column descriptors */
  const QVector<Column *>& getColumns() const
  {
    return columns;
  }

  /* Assign a widget to the column descriptor with the given name */
  void assignWidget(const QString& field, QWidget *widget);
  void assignMinMaxWidget(const QString& field, QWidget *minWidget, QWidget *maxWidget);

  /* Clear all LineEdit widgets and reset other back to default state */
  void resetWidgets(const QStringList& exceptColNames = QStringList());

  /* Enable or disable widgets except the ones with the give column names */
  void enableWidgets(bool enabled = true, const QStringList& exceptColNames = QStringList());

  ColumnList& append(const Column& col);

  QString getTablename() const
  {
    return table;
  }

  /* Assign widgets for distance search */
  void assignDistanceSearchWidgets(QCheckBox *checkBox, QComboBox *directionWidget,
                                   QSpinBox *minWidget, QSpinBox *maxWidget);
  void clear();

  QSpinBox *getMinDistanceWidget() const
  {
    return minDistanceWidget;
  }

  QSpinBox *getMaxDistanceWidget() const
  {
    return maxDistanceWidget;
  }

  QComboBox *getDistanceDirectionWidget() const
  {
    return distanceDirectionWidget;
  }

  QCheckBox *getDistanceCheckBox() const
  {
    return distanceCheckBox;
  }

  bool isDistanceCheckBoxChecked() const;

  const QString& getIdColumnName() const
  {
    return idColumn;
  }

  void updateUnits();

private:
  QSpinBox *minDistanceWidget = nullptr, *maxDistanceWidget = nullptr;
  QString minDistanceWidgetSuffix, maxDistanceWidgetSuffix;
  QCheckBox *distanceCheckBox = nullptr;
  QComboBox *distanceDirectionWidget = nullptr;
  QString table, idColumn;
  QVector<Column *> columns;
  QHash<QString, Column *> nameColumnMap;
};

#endif // LITTLENAVMAP_COLUMNLIST_H
