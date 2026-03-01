/*****************************************************************************
* Copyright 2015-2025 Alexander Barthel alex@littlenavmap.org
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
class WidgetZoomHandler;
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

  void fontChanged(const QFont& font);

  /* Save and restore dialog size and tab selection */
  void saveState() const;
  void restoreState();

  /* Reset saved settings and position of dialog */
  void resetWindowLayout();

private:
  /* Use events to update data and disconnect from database if not visible. */
  /* Also enable or disable toolbar/menu action. */
  virtual void showEvent(QShowEvent *) override;
  virtual void hideEvent(QHideEvent *) override;

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

  QList<Query> queries;

  Ui::LogStatisticsDialog *ui;
  LogdataController *logdataController;

  /* Used to remove margins in table */
  atools::gui::WidgetZoomHandler *zoomHandler;

  /* using own SQL query model to override data method */
  LogStatsSqlModel *model = nullptr;

  /* Item delegate needed to change alignment */
  LogStatsDelegate *delegate = nullptr;

  /* Size as given in UI */
  QSize defaultSize;
};

/* Internal. Delegate to change table column data alignment */
class LogStatsDelegate
  : public QStyledItemDelegate
{
  Q_OBJECT

public:
  QList<Qt::Alignment> align;

private:
  virtual void paint(QPainter *painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;

};

/* Internal. Overrides data method for local sensitive number formatting and sorting by SQL query */
class LogStatsSqlModel :
  public QSqlQueryModel
{
  Q_OBJECT

public:
  LogStatsSqlModel(QObject *parent, const QSqlDatabase *db)
    : QSqlQueryModel(parent), database(db)
  {

  }

  void setLogStatQuery(const Query *queryParam);

private:
  virtual QVariant data(const QModelIndex& index, int role) const override;

  virtual void sort(int column, Qt::SortOrder order) override;

  QString buildQuery();

  QLocale locale;
  float nmToUnitFactor = 1.f;
  const QSqlDatabase *database = nullptr;
  const Query *query = nullptr;
  int sortColumn = 0;
  Qt::SortOrder sortOrder = Qt::DescendingOrder;
};

#endif // LNM_LOGSTATISTICSDIALOG_H
