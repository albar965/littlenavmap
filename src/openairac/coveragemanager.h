/*****************************************************************************
* OpenAIRAC Map — Coverage Diagnostics Manager
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

#ifndef OPENAIRAC_COVERAGEMANAGER_H
#define OPENAIRAC_COVERAGEMANAGER_H

#include <QString>

namespace openairac {

struct AirportCoverageSummary {
    bool hasAirport = true;
    bool hasRunways = true;
    bool hasNavaids = true;
    bool hasAirways = true;
    bool hasSid = false;
    bool hasStar = false;
    bool hasApproach = false;
    int sidCount = 0;
    int starCount = 0;
    int approachCount = 0;
    QString coverageReason;
};

class CoverageManager {
public:
    static CoverageManager& instance();

    AirportCoverageSummary evaluateAirportCoverage(
        const QString& airportIdent,
        const QString& countryIso,
        int sidCount,
        int starCount,
        int approachCount
    ) const;

    QString formatCoverageHtml(const AirportCoverageSummary& cov) const;
    QString formatCoveragePlain(const AirportCoverageSummary& cov) const;

private:
    CoverageManager() = default;
};

} // namespace openairac

#endif // OPENAIRAC_COVERAGEMANAGER_H
