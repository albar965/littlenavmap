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

#ifndef LITTLENAVMAP_DATABASEDIALOG_H
#define LITTLENAVMAP_DATABASEDIALOG_H

#include "db/dbtypes.h"

#include <QDialog>
#include <QWidget>

namespace Ui {
class DatabaseDialog;
}

/*
 * Load scenery database dialog
 */
class DatabaseDialog
  : public QDialog
{
  Q_OBJECT

public:
  /*
   * @param pathMap which might be changed by the dialog
   */
  DatabaseDialog(QWidget *parent, const SimulatorTypeMap& pathMap);
  virtual ~DatabaseDialog();

  /* Get the base path of the currently selected simulator in the combo box */
  QString getBasePath() const;

  /* Load all scenery, even inactive */
  bool isReadInactive() const;
  void setReadInactive(bool value);

  /* Read P3D add-on.xml packages */
  bool isReadAddOnXml() const;
  void setReadAddOnXml(bool value);

  /* Get the path and filename of the scenery.cfg file of the currently selected simulator in the combo box */
  QString getSceneryConfigFile() const;

  /* Get the paths which might be modified (changed paths and more) */
  const SimulatorTypeMap& getPaths() const
  {
    return simulators;
  }

  /* Set the databast information into the header */
  void setHeader(const QString& header);

  /* Get the simulator currently selected in the combo box */
  atools::fs::FsPaths::SimulatorType getCurrentFsType() const
  {
    return currentFsType;
  }

  /* Set simulator */
  void setCurrentFsType(atools::fs::FsPaths::SimulatorType value);

signals:
  /* Emitted if the user changed the fligh simulator in the combo box */
  void simulatorChanged(atools::fs::FsPaths::SimulatorType value);

private:
  void basePathEdited(const QString& text);
  void resetPathsClicked();
  void helpClicked();
  void sceneryConfigFileEdited(const QString& text);
  void selectBasePathClicked();
  void selectSceneryConfigClicked();
  void simComboChanged(int index);
  void updateComboBox();
  void updateWidgets();

  Ui::DatabaseDialog *ui;
  atools::fs::FsPaths::SimulatorType currentFsType = atools::fs::FsPaths::UNKNOWN;

  // Copy of the FS path map which can be used or not (in case of cancel)
  SimulatorTypeMap simulators;

};

#endif // LITTLENAVMAP_DATABASEDIALOG_H
