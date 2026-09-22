/*****************************************************************************
* RNAV Web API controller.
*****************************************************************************/
#include "webapi/rnavactionscontroller.h"

#include "common/mapresult.h"
#include "query/mapquery.h"
#include "query/querymanager.h"
#include "rnav/rnavreference.h"
#include "webapi/webapirequest.h"
#include "webapi/webapiresponse.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

RnavActionsController::RnavActionsController(QObject *parent, bool verboseParam, AbstractInfoBuilder *infoBuilderParam)
  : AbstractLnmActionsController(parent, verboseParam, infoBuilderParam)
{
}

WebApiResponse RnavActionsController::referenceAction(WebApiRequest request)
{
  WebApiResponse response = getResponse();
  response.headers.replace("Content-Type", "application/json; charset=utf-8");

  bool latOk = false, lonOk = false;
  const double lat = request.parameters.value("lat").toDouble(&latOk);
  const double lon = request.parameters.value("lon").toDouble(&lonOk);
  if(!latOk || !lonOk || lat < -90. || lat > 90. || lon < -180. || lon > 180.)
  {
    response.status = 400;
    response.body = QJsonDocument(QJsonObject{{"error", "lat and lon must be valid decimal coordinates"}}).toJson(QJsonDocument::Compact);
    return response;
  }

  const atools::geo::Pos target(lon, lat);
  QList<map::MapVor> vors;
  {
    Queries *queries = getQueries();
    QueryLocker locker(queries);
    map::MapResultIndex *nearby = queries->getMapQuery()->getNearestNavaids(target, 200.f, map::VOR, 0, 0.f);
    if(nearby != nullptr)
      for(const map::MapBase *base : *nearby)
        if(base != nullptr && base->type == map::VOR)
          vors.append(base->asObj<map::MapVor>());
  }

  const QList<rnav::Reference> references = rnav::calculateReferences(target, vors);
  QJsonArray candidates;
  for(int index = 0; index < references.size(); ++index)
  {
    const rnav::Reference& reference = references.at(index);
    QJsonObject candidate;
    candidate.insert("recommended", index == 0);
    candidate.insert("ident", reference.vor.ident);
    candidate.insert("name", reference.vor.name);
    candidate.insert("frequency_mhz", reference.vor.frequency / 1000.0);
    candidate.insert("has_dme", reference.vor.hasDme);
    candidate.insert("published_range_nm", reference.vor.range);
    candidate.insert("radial_magnetic_deg", qRound(reference.radialMagDeg));
    candidate.insert("reference_distance_nm", qRound(reference.distanceNm * 10.f) / 10.0);
    candidates.append(candidate);
  }

  QJsonObject targetJson;
  targetJson.insert("lat", lat);
  targetJson.insert("lon", lon);
  response.status = 200;
  response.body = QJsonDocument(QJsonObject{{"target", targetJson}, {"candidates", candidates}}).toJson(QJsonDocument::Compact);
  return response;
}
