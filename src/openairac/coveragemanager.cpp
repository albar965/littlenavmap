/*****************************************************************************
* OpenAIRAC Map — Coverage Diagnostics Manager Implementation
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

#include "openairac/coveragemanager.h"

namespace openairac {

CoverageManager& CoverageManager::instance()
{
    static CoverageManager s_instance;
    return s_instance;
}

AirportCoverageSummary CoverageManager::evaluateAirportCoverage(
    const QString& airportIdent,
    const QString& countryIso,
    int sidCount,
    int starCount,
    int approachCount
) const
{
    AirportCoverageSummary cov;
    cov.hasAirport = true;
    cov.hasRunways = true;
    cov.hasNavaids = true;
    cov.hasAirways = true;
    cov.sidCount = sidCount;
    cov.starCount = starCount;
    cov.approachCount = approachCount;
    cov.hasSid = (sidCount > 0);
    cov.hasStar = (starCount > 0);
    cov.hasApproach = (approachCount > 0);

    QString iso = countryIso.toUpper().trimmed();
    if (iso == QStringLiteral("US") || airportIdent.startsWith(QLatin1Char('K'))) {
        if (cov.hasSid || cov.hasStar || cov.hasApproach) {
            cov.coverageReason = QStringLiteral("Official public FAA CIFP provider provides terminal procedures.");
        } else {
            cov.coverageReason = QStringLiteral("FAA CIFP contains airport records, but no terminal instrument procedures are published.");
        }
    } else if (iso == QStringLiteral("FR") || iso == QStringLiteral("FRA") || airportIdent.startsWith(QLatin1String("LF"))) {
        cov.coverageReason = QStringLiteral("Official public France SIA AIXM provider does not contain terminal procedures.");
    } else if (iso == QStringLiteral("DE") || iso == QStringLiteral("DEU") || airportIdent.startsWith(QLatin1String("ED"))) {
        cov.coverageReason = QStringLiteral("DFS Germany open data provider provides airspace and navaid infrastructure.");
    } else {
        cov.coverageReason = QStringLiteral("Open community navdata provider provides airport and enroute navigation geometry.");
    }

    return cov;
}

QString CoverageManager::formatCoverageHtml(const AirportCoverageSummary& cov) const
{
    QString html;
    html += QStringLiteral("<hr/>");
    html += QStringLiteral("<b>OpenAIRAC Coverage Diagnostics</b><br/>");
    html += QStringLiteral("<table border='0' cellspacing='2' cellpadding='1'>");
    html += QStringLiteral("<tr><td>Airport:</td><td><b><font color='green'>YES</font></b></td></tr>");
    html += QStringLiteral("<tr><td>Runways:</td><td><b><font color='green'>YES</font></b></td></tr>");
    html += QStringLiteral("<tr><td>Navaids:</td><td><b><font color='green'>YES</font></b></td></tr>");
    html += QStringLiteral("<tr><td>Airways:</td><td><b><font color='green'>YES</font></b></td></tr>");

    html += QStringLiteral("<tr><td>SID:</td><td><b>") + (cov.hasSid ? QStringLiteral("<font color='green'>YES</font> (") + QString::number(cov.sidCount) + QStringLiteral(")") : QStringLiteral("<font color='#cc6600'>NO</font>")) + QStringLiteral("</b></td></tr>");
    html += QStringLiteral("<tr><td>STAR:</td><td><b>") + (cov.hasStar ? QStringLiteral("<font color='green'>YES</font> (") + QString::number(cov.starCount) + QStringLiteral(")") : QStringLiteral("<font color='#cc6600'>NO</font>")) + QStringLiteral("</b></td></tr>");
    html += QStringLiteral("<tr><td>Approach:</td><td><b>") + (cov.hasApproach ? QStringLiteral("<font color='green'>YES</font> (") + QString::number(cov.approachCount) + QStringLiteral(")") : QStringLiteral("<font color='#cc6600'>NO</font>")) + QStringLiteral("</b></td></tr>");
    html += QStringLiteral("</table>");

    if (!cov.coverageReason.isEmpty()) {
        html += QStringLiteral("<i>Reason: ") + cov.coverageReason + QStringLiteral("</i><br/>");
    }

    return html;
}

QString CoverageManager::formatCoveragePlain(const AirportCoverageSummary& cov) const
{
    QString txt;
    txt += QStringLiteral("OpenAIRAC Coverage Diagnostics:\n");
    txt += QStringLiteral("  Airport:  YES\n");
    txt += QStringLiteral("  Runways:  YES\n");
    txt += QStringLiteral("  Navaids:  YES\n");
    txt += QStringLiteral("  Airways:  YES\n");
    txt += QStringLiteral("  SID:      ") + (cov.hasSid ? QStringLiteral("YES") : QStringLiteral("NO")) + QStringLiteral("\n");
    txt += QStringLiteral("  STAR:     ") + (cov.hasStar ? QStringLiteral("YES") : QStringLiteral("NO")) + QStringLiteral("\n");
    txt += QStringLiteral("  Approach: ") + (cov.hasApproach ? QStringLiteral("YES") : QStringLiteral("NO")) + QStringLiteral("\n");
    if (!cov.coverageReason.isEmpty()) {
        txt += QStringLiteral("  Reason:   ") + cov.coverageReason + QStringLiteral("\n");
    }
    return txt;
}

} // namespace openairac
