/*****************************************************************************
* Generic VOR/DME RNAV reference calculation for Little Navmap.
*****************************************************************************/
#include "rnav/rnavreference.h"

#include "geo/calculations.h"

#include <algorithm>

namespace rnav {

bool isEligibleVorDme(const map::MapVor& vor)
{
  return vor.isCalibratedVor() && vor.hasDme && vor.frequency > 0 && vor.position.isValid();
}

QList<Reference> calculateReferences(const atools::geo::Pos& target, const QList<map::MapVor>& vors,
                                     const SearchOptions& options)
{
  QList<Reference> result;
  if(!target.isValid())
    return result;

  for(const map::MapVor& vor : vors)
  {
    if(!isEligibleVorDme(vor))
      continue;

    Reference reference;
    reference.vor = vor;
    reference.target = target;
    reference.distanceNm = atools::geo::meterToNm(vor.position.distanceMeterTo(target));
    reference.withinPublishedRange = vor.range > 0 && reference.distanceNm <= vor.range;

    if(options.requirePublishedRange && !reference.withinPublishedRange)
      continue;

    // Same magnetic convention used by LNM's VOR measurement-line renderer.
    reference.radialMagDeg = atools::geo::normalizeCourse(vor.position.initialBearing(target) - vor.magvar);
    result.append(reference);
  }

  std::sort(result.begin(), result.end(), [](const Reference& left, const Reference& right) {
    if(left.withinPublishedRange != right.withinPublishedRange)
      return left.withinPublishedRange;
    return left.distanceNm < right.distanceNm;
  });

  if(options.maxResults >= 0)
    result = result.mid(0, options.maxResults);
  return result;
}

} // namespace rnav
