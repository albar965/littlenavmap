/*****************************************************************************
* Copyright 2015-2023 Alexander Barthel alex@littlenavmap.org
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

#ifndef LNM_LOGSTATISTICSDIALOG_H
#define LNM_LOGSTATISTICSDIALOG_H

#include <QDialog>
#include <QSqlQueryModel>
#include <QStyledItemDelegate>

class LogdataController;
class LogStatsSqlModel;
class LogStatsDelegate;
class QAbstractButton;

namespace atools {
namespace gui {
class ItemViewZoomHandler;
}
namespace util {
class HtmlBuilder;
}
}

namespace Ui {
class LogStatisticsDialog;
}

class LogStatsSqlModel;
class Query;

/*
 * Shows logbook statistics in a text browser as well as for various queries in a table
 */
class LogStatisticsDialog :
  public QDialog
{
  Q_OBJECT

public:
  explicit LogStatisticsDialog(QWidget *parent, LogdataController *logdataControllerParam);
  virtual ~LogStatisticsDialog() override;

  LogStatisticsDialog(const LogStatisticsDialog& other) = delete;
  LogStatisticsDialog& operator=(const LogStatisticsDialog& other) = delete;

  /* Update on new, changed or deleted log entries in database */
  void logDataChanged();

  /* Update display units */
  void optionsChanged();

  void styleChanged();

private:
  /* Use events to update data and disconnect from database if not visible. */
  /* Also enable or disable toolbar/menu action. */
  virtual void showEvent(QShowEvent *) override;
  virtual void hideEvent(QHideEvent *) override;

  /* Save and restore dialog size and tab selection */
  void saveState();
  void restoreState();

  /* Update tables and text browser */
  void updateWidgets();

  /* Update text browser */
  void updateStatisticsText();

  /* Query has changed in combo box */
  void groupChanged(int index);

  /* Initialize queries (translation) */
  void initQueries();

  void buttonBoxClicked(QAbstractButton *button);

  /* Create and set models in view */
  void setModel();
  void clearModel();

  QVector<Query> queries;

  Ui::LogStatisticsDialog *ui;
  LogdataController *logdataController;

  /* Used to remove margins in table */
  atools::gui::ItemViewZoomHandler *zoomHandler;

  /* using own SQL query model to override data method */
  LogStatsSqlModel *model = nullptr;

  /* Item delegate needed to change alignment */
  LogStatsDelegate *delegate = nullptr;

  /* Remember dilalog position when reopening */
  QPoint position;

};

#endif // LNM_LOGSTATISTICSDIALOG_H
