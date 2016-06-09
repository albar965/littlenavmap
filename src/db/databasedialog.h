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

#ifndef DATABASEDIALOG_H
#define DATABASEDIALOG_H

#include <QDialog>
#include <QWidget>

namespace atools {
namespace gui {
class Dialog;
}
}

namespace Ui {
class DatabaseDialog;
}

class DatabaseDialog
  : public QDialog
{
  Q_OBJECT

public:
  DatabaseDialog(QWidget *parent);
  virtual ~DatabaseDialog();

  QString getBasePath() const;
  QString getSceneryConfigFile() const;

  void setBasePath(const QString& path);
  void setSceneryConfigFile(const QString& path);
  void setHeader(const QString& header);

private:
  Ui::DatabaseDialog *ui;
  void selectBasePath();
  void selectSceneryConfig();

  atools::gui::Dialog *dialog;
  void menuTriggered(QAction *action);

};

#endif // DATABASEDIALOG_H
