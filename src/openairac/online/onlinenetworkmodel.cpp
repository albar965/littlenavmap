/*****************************************************************************
* OpenAIRAC Map — Online Flight Network Domain Models Implementation
*
* Copyright 2026 OpenAIRAC Contributors
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
*****************************************************************************/

#include "openairac/online/onlinenetworkmodel.h"
#include <QtMath>

namespace openairac {

QPair<double, double> OnlinePilotItem::interpolatedPosition(const QDateTime& currentTime) const {
    if (!prevUpdated.isValid() || !lastUpdated.isValid() || qFuzzyIsNull(prevLatitude)) {
        return QPair<double, double>(longitude, latitude);
    }

    qint64 totalSpanMs = lastUpdated.toMSecsSinceEpoch() - prevUpdated.toMSecsSinceEpoch();
    if (totalSpanMs <= 0 || totalSpanMs > 120000) {
        return QPair<double, double>(longitude, latitude);
    }

    qint64 elapsedMs = currentTime.toMSecsSinceEpoch() - prevUpdated.toMSecsSinceEpoch();
    double t = static_cast<double>(elapsedMs) / static_cast<double>(totalSpanMs);

    // Bounded extrapolation (max 1.25x span to avoid runaway jumps on delayed feed)
    t = qBound(0.0, t, 1.25);

    // Large jump reset (e.g. teleport / wrap around meridian)
    if (qAbs(latitude - prevLatitude) > 5.0 || qAbs(longitude - prevLongitude) > 5.0) {
        return QPair<double, double>(longitude, latitude);
    }

    double interpLat = prevLatitude + t * (latitude - prevLatitude);
    double interpLon = prevLongitude + t * (longitude - prevLongitude);

    return QPair<double, double>(interpLon, interpLat);
}

} // namespace openairac
