/*****************************************************************************
* OpenAIRAC Map — Flight Plan Chart Suggestions & Shortcuts
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

#ifndef OPENAIRAC_FLIGHTSUGGESTIONS_H
#define OPENAIRAC_FLIGHTSUGGESTIONS_H

#include "openairac/charts/chartmodel.h"
#include <QObject>
#include <QString>
#include <QList>

namespace openairac {

struct FlightPlanChartShortcut {
    QString label;
    QString airportIcao;
    QChar procedureKind; // 'D', 'E', 'F' or 'A' (Airport)
    QString procedureIdent;
    QString runway;
    QString chartId;
    QString confidence;
};

class FlightSuggestions : public QObject {
    Q_OBJECT

public:
    static FlightSuggestions& instance();

    QList<FlightPlanChartShortcut> evaluateShortcuts(
        const QString& departureIcao,
        const QString& departureSid,
        const QString& destinationIcao,
        const QString& arrivalStar,
        const QString& arrivalApproach,
        const QString& arrivalRunway
    ) const;

signals:
    void openChartRequested(const QString& airportIcao, const QString& chartId);
    void openProcedureChartRequested(const QString& airportIcao, QChar kind, const QString& procIdent, const QString& runway);

private:
    FlightSuggestions() = default;
};

} // namespace openairac

#endif // OPENAIRAC_FLIGHTSUGGESTIONS_H
