/*****************************************************************************
* OpenAIRAC Map — Chart Model
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

#ifndef OPENAIRAC_CHARTMODEL_H
#define OPENAIRAC_CHARTMODEL_H

#include <QString>
#include <QDateTime>
#include <QList>
#include <QJsonObject>
#include <QJsonArray>

namespace openairac {

enum class ChartCategory {
    AirportDiagram,
    Departure,
    Arrival,
    Approach,
    Minima,
    General,
    Other,
};

struct ChartEntry {
    QString id;
    QString providerId;
    QString airportIcao;
    QString airportIata;
    ChartCategory category = ChartCategory::Other;
    QString normalizedTypeStr;
    QString providerType;
    QString title;
    QString procedureName;
    QString runway;
    QDateTime effectiveFrom;
    QDateTime effectiveTo;
    QDateTime revisionDate;
    QString airacCycle;
    QString language;
    QString sourceUrl;
    QString sourceDocumentId;
    QString licensePolicy;
    QString attribution;
    QString mimeType;
    QString localCachePath;
    bool isCached = false;
    bool isOutdated = false;
    QString georeferenceStatus;

    static ChartEntry fromJson(const QJsonObject& obj);
    QJsonObject toJson() const;

    QString categoryName() const;
    QString displayTitle() const;
    QString providerBadge() const;
};

struct ProcedureChartMatch {
    QString procedureIdent;
    QChar procedureKind; // 'D', 'E', 'F'
    QString airportIcao;
    QString runway;
    QString chartId;
    QString confidence; // "Exact", "Likely", "Ambiguous", "Unmatched"
    QString matchReason;
    ChartEntry chart;

    static ProcedureChartMatch fromJson(const QJsonObject& obj);
};

} // namespace openairac

#endif // OPENAIRAC_CHARTMODEL_H
