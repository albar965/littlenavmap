/*****************************************************************************
* OpenAIRAC Map — Flight Plan Chart Suggestions & Shortcuts Implementation
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

#include "openairac/charts/flightsuggestions.h"
#include "openairac/charts/chartclient.h"

namespace openairac {

FlightSuggestions& FlightSuggestions::instance()
{
    static FlightSuggestions s_instance;
    return s_instance;
}

QList<FlightPlanChartShortcut> FlightSuggestions::evaluateShortcuts(
    const QString& departureIcao,
    const QString& departureSid,
    const QString& destinationIcao,
    const QString& arrivalStar,
    const QString& arrivalApproach,
    const QString& arrivalRunway
) const
{
    QList<FlightPlanChartShortcut> list;

    // 1. Departure Airport Diagram
    if (!departureIcao.isEmpty()) {
        FlightPlanChartShortcut sc;
        sc.label = QStringLiteral("Departure Airport Diagram (") + departureIcao + QStringLiteral(")");
        sc.airportIcao = departureIcao;
        sc.procedureKind = QChar('A');
        list.append(sc);
    }

    // 2. Departure SID Chart
    if (!departureIcao.isEmpty() && !departureSid.isEmpty()) {
        FlightPlanChartShortcut sc;
        sc.label = QStringLiteral("SID Chart: ") + departureSid;
        sc.airportIcao = departureIcao;
        sc.procedureKind = QChar('D');
        sc.procedureIdent = departureSid;

        QList<ProcedureChartMatch> matches = ChartClient::instance().matchProcedureCharts(departureIcao, QChar('D'), departureSid);
        if (!matches.isEmpty()) {
            sc.chartId = matches.first().chartId;
            sc.confidence = matches.first().confidence;
        }
        list.append(sc);
    }

    // 3. Arrival STAR Chart
    if (!destinationIcao.isEmpty() && !arrivalStar.isEmpty()) {
        FlightPlanChartShortcut sc;
        sc.label = QStringLiteral("STAR Chart: ") + arrivalStar;
        sc.airportIcao = destinationIcao;
        sc.procedureKind = QChar('E');
        sc.procedureIdent = arrivalStar;

        QList<ProcedureChartMatch> matches = ChartClient::instance().matchProcedureCharts(destinationIcao, QChar('E'), arrivalStar);
        if (!matches.isEmpty()) {
            sc.chartId = matches.first().chartId;
            sc.confidence = matches.first().confidence;
        }
        list.append(sc);
    }

    // 4. Arrival Approach Chart
    if (!destinationIcao.isEmpty() && !arrivalApproach.isEmpty()) {
        FlightPlanChartShortcut sc;
        sc.label = QStringLiteral("Approach Chart: ") + arrivalApproach;
        sc.airportIcao = destinationIcao;
        sc.procedureKind = QChar('F');
        sc.procedureIdent = arrivalApproach;
        sc.runway = arrivalRunway;

        QList<ProcedureChartMatch> matches = ChartClient::instance().matchProcedureCharts(destinationIcao, QChar('F'), arrivalApproach, arrivalRunway);
        if (!matches.isEmpty()) {
            sc.chartId = matches.first().chartId;
            sc.confidence = matches.first().confidence;
        }
        list.append(sc);
    }

    // 5. Destination Airport Diagram
    if (!destinationIcao.isEmpty()) {
        FlightPlanChartShortcut sc;
        sc.label = QStringLiteral("Destination Airport Diagram (") + destinationIcao + QStringLiteral(")");
        sc.airportIcao = destinationIcao;
        sc.procedureKind = QChar('A');
        list.append(sc);
    }

    return list;
}

} // namespace openairac
