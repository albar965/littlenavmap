#ifndef MAPQUERY_H
#define MAPQUERY_H

namespace atools {
namespace sql {
class SqlDatabase;
}
}


class MapQuery
{
public:
  MapQuery(atools::sql::SqlDatabase *sqlDb);

//void getAirports(const atools::geo::Rect)
private:
  atools::sql::SqlDatabase *db;

};

#endif // MAPQUERY_H
