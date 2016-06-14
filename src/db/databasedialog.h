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

#ifndef LITTLENAVMAP_DATABASEDIALOG_H
#define LITTLENAVMAP_DATABASEDIALOG_H

#include "fs/fspaths.h"
#include "db/dbtypes.h"

#include <QDialog>
#include <QWidget>

namespace Ui {
class DatabaseDialog;
}

class DatabaseDialog
  : public QDialog
{
  Q_OBJECT

public:
  DatabaseDialog(QWidget *parent, const FsPathTypeMap& value);
  virtual ~DatabaseDialog();

  void setHeader(const QString& header);

  atools::fs::FsPaths::SimulatorType getCurrentFsType() const
  {
    return currentFsType;
  }

  QString getBasePath() const;
  QString getSceneryConfigFile() const;

  void setCurrentFsType(atools::fs::FsPaths::SimulatorType value);

  const FsPathTypeMap& getPaths() const
  {
    return paths;
  }

signals:
  void simulatorChanged(atools::fs::FsPaths::SimulatorType value);

private:
  Ui::DatabaseDialog *ui;
  atools::fs::FsPaths::SimulatorType currentFsType = atools::fs::FsPaths::UNKNOWN;
  FsPathTypeMap paths;

  void basePathEdited(const QString& text);
  void sceneryConfigFileEdited(const QString& text);
  void selectBasePath();
  void selectSceneryConfig();
  void simComboChanged(int index);
  void updateLineEdits();
  void resetPaths();

  void updateComboBox();

};

#endif // LITTLENAVMAP_DATABASEDIALOG_H
