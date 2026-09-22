/*****************************************************************************
* Generic VOR/DME RNAV reference calculation for Little Navmap.
*****************************************************************************/
#ifndef LNM_RNAVREFERENCE_H
#define LNM_RNAVREFERENCE_H

#include "common/maptypes.h"
#include "geo/pos.h"

#include <QList>

namespace rnav {

struct Reference
{
  map::MapVor vor;
  atools::geo::Pos target;
  float radialMagDeg = 0.f;
  float distanceNm = 0.f;
  bool withinPublishedRange = false;
};

struct SearchOptions
{
  int maxResults = 3;
  bool requirePublishedRange = true;
};

bool isEligibleVorDme(const map::MapVor& vor);
QList<Reference> calculateReferences(const atools::geo::Pos& target, const QList<map::MapVor>& vors,
                                     const SearchOptions& options = SearchOptions());

} // namespace rnav

#endif // LNM_RNAVREFERENCE_H
