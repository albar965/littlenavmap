/*****************************************************************************
* Copyright 2015-2020 Alexander Barthel alex@littlenavmap.org
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

  /* Update on new, changed or deleted log entries in database */
  void logDataChanged();

  /* Update display units */
  void optionsChanged();

private:
  /* Use events to update data and disconnect from database if not visible */
  virtual void showEvent(QShowEvent *event) override;
  virtual void hideEvent(QHideEvent *event) override;

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

  /* Contains information about selected query in combo box */
  struct Query
  {
    QString label; /* Combo box label */
    QStringList header; /* Result table headers - must be equal to query columns*/
    QVector<Qt::Alignment> align; /* Column alignment - must be equal to query columns*/
    QString query; /* SQL query */
  };
  QVector<Query> QUERIES;

  Ui::LogStatisticsDialog *ui;
  LogdataController *logdataController;

  /* Used to remove margins in table */
  atools::gui::ItemViewZoomHandler *zoomHandler;

  /* using own SQL query model to override data method */
  LogStatsSqlModel *model = nullptr;

  /* Item delegate needed to change alignment */
  LogStatsDelegate *delegate = nullptr;
};

/* Delegate to change table column data alignment */
class LogStatsDelegate
  : public QStyledItemDelegate
{
public:
  LogStatsDelegate();

  QVector<Qt::Alignment> align;

private:
  virtual void paint(QPainter *painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;

};

/* Overrides data method for local sensitive number formatting */
class LogStatsSqlModel :
  public QSqlQueryModel
{
public:
  LogStatsSqlModel();

private:
  virtual QVariant data(const QModelIndex& index, int role) const override;

  QLocale locale;
};

#endif // LNM_LOGSTATISTICSDIALOG_H
