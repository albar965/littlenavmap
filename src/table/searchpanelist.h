#ifndef SEARCHPANELIST_H
#define SEARCHPANELIST_H

#include <QObject>

class MainWindow;
class SearchPane;
class ColumnList;
class QLayout;

namespace atools {
namespace sql {
class SqlDatabase;
}
}

class SearchPaneList :
  public QObject
{
  Q_OBJECT

public:
  SearchPaneList(MainWindow *parent, atools::sql::SqlDatabase *sqlDb);
  virtual ~SearchPaneList();
  void createAirportSearch();

  void preDatabaseLoad();
  void postDatabaseLoad();

  void saveState();
  void restoreState();

private:
  atools::sql::SqlDatabase *db;
  ColumnList *airportColumns = nullptr;
  SearchPane *airportSearchPane = nullptr;

  MainWindow *parentWidget;
  void showHideLayoutElements(const QList<QLayout *> layouts, bool visible,
                              const QList<QWidget *>& otherWidgets);

};

#endif // SEARCHPANELIST_H
