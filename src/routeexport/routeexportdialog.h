/*****************************************************************************
* Copyright 2015-2024 Alexander Barthel alex@littlenavmap.org
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

#ifndef LNM_ROUTEEXPORTDIALOG_H
#define LNM_ROUTEEXPORTDIALOG_H

#include "routeexport/routeexportflags.h"

#include <QDialog>

namespace Ui {
class RouteExportDialog;
}

class UnitStringTool;
class RouteExportData;
class QAbstractButton;

/*
 * Allows user to enter additional information for online network flight plan export like pilot name and more.
 */
class RouteExportDialog :
  public QDialog
{
  Q_OBJECT

public:
  explicit RouteExportDialog(QWidget *parent, re::RouteExportType routeType);
  virtual ~RouteExportDialog() override;

  RouteExportDialog(const RouteExportDialog& other) = delete;
  RouteExportDialog& operator=(const RouteExportDialog& other) = delete;

  void restoreState();
  void saveState() const;

  RouteExportData getExportData() const;
  void setExportData(const RouteExportData& value);

  static QString getRouteTypeAsDisplayString(re::RouteExportType routeType);
  static QString getRouteTypeAsString(re::RouteExportType routeType);

private:
  void buttonBoxClicked(QAbstractButton *button);
  void dataToDialog(const RouteExportData& data);
  void dialogToData(RouteExportData& data);
  void clearDialog();

  Ui::RouteExportDialog *ui;
  re::RouteExportType type = re::UNKNOWN;
  RouteExportData *exportData = nullptr;
  RouteExportData *exportDataSaved = nullptr;
  UnitStringTool *units = nullptr;
  QList<QObject *> widgets;
};

#endif // LNM_ROUTEEXPORTDIALOG_H
