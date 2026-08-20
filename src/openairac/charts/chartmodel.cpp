/*****************************************************************************
* OpenAIRAC Map — Chart Model Implementation
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

#include "openairac/charts/chartmodel.h"
#include <QFileInfo>

namespace openairac {

ChartEntry ChartEntry::fromJson(const QJsonObject& obj)
{
    ChartEntry entry;
    entry.id = obj.value(QStringLiteral("id")).toObject().value(QStringLiteral("0")).toString();
    if (entry.id.isEmpty()) {
        entry.id = obj.value(QStringLiteral("id")).toString();
    }
    entry.providerId = obj.value(QStringLiteral("provider_id")).toString();
    entry.airportIcao = obj.value(QStringLiteral("airport_icao")).toString();
    entry.airportIata = obj.value(QStringLiteral("airport_iata")).toString();
    entry.normalizedTypeStr = obj.value(QStringLiteral("chart_type")).toString();
    entry.providerType = obj.value(QStringLiteral("provider_chart_type")).toString();
    entry.title = obj.value(QStringLiteral("title")).toString();
    entry.procedureName = obj.value(QStringLiteral("procedure_name")).toString();
    entry.runway = obj.value(QStringLiteral("runway")).toString();

    QString effFromStr = obj.value(QStringLiteral("effective_from")).toString();
    if (!effFromStr.isEmpty()) entry.effectiveFrom = QDateTime::fromString(effFromStr, Qt::ISODate);

    QString effToStr = obj.value(QStringLiteral("effective_to")).toString();
    if (!effToStr.isEmpty()) entry.effectiveTo = QDateTime::fromString(effToStr, Qt::ISODate);

    entry.airacCycle = obj.value(QStringLiteral("airac_cycle")).toString();
    entry.language = obj.value(QStringLiteral("language")).toString();
    entry.sourceUrl = obj.value(QStringLiteral("source_url")).toString();
    entry.sourceDocumentId = obj.value(QStringLiteral("source_document_id")).toString();
    entry.licensePolicy = obj.value(QStringLiteral("license_policy")).toString();
    entry.attribution = obj.value(QStringLiteral("attribution")).toString();
    entry.mimeType = obj.value(QStringLiteral("mime_type")).toString();
    entry.georeferenceStatus = obj.value(QStringLiteral("georeference_status")).toString();

    // Map category
    QString t = entry.normalizedTypeStr.toLower();
    if (t.contains(QStringLiteral("airport_diagram")) || t.contains(QStringLiteral("parking")) || t.contains(QStringLiteral("ground"))) {
        entry.category = ChartCategory::AirportDiagram;
    } else if (t == QStringLiteral("sid")) {
        entry.category = ChartCategory::Departure;
    } else if (t == QStringLiteral("star")) {
        entry.category = ChartCategory::Arrival;
    } else if (t.contains(QStringLiteral("approach"))) {
        entry.category = ChartCategory::Approach;
    } else if (t.contains(QStringLiteral("minima")) || t.contains(QStringLiteral("hot_spot"))) {
        entry.category = ChartCategory::Minima;
    } else if (t.contains(QStringLiteral("general"))) {
        entry.category = ChartCategory::General;
    } else {
        entry.category = ChartCategory::Other;
    }

    return entry;
}

QJsonObject ChartEntry::toJson() const
{
    QJsonObject obj;
    obj[QStringLiteral("id")] = id;
    obj[QStringLiteral("provider_id")] = providerId;
    obj[QStringLiteral("airport_icao")] = airportIcao;
    obj[QStringLiteral("airport_iata")] = airportIata;
    obj[QStringLiteral("chart_type")] = normalizedTypeStr;
    obj[QStringLiteral("provider_chart_type")] = providerType;
    obj[QStringLiteral("title")] = title;
    obj[QStringLiteral("procedure_name")] = procedureName;
    obj[QStringLiteral("runway")] = runway;
    obj[QStringLiteral("airac_cycle")] = airacCycle;
    obj[QStringLiteral("source_url")] = sourceUrl;
    obj[QStringLiteral("source_document_id")] = sourceDocumentId;
    obj[QStringLiteral("attribution")] = attribution;
    return obj;
}

QString ChartEntry::categoryName() const
{
    switch (category) {
        case ChartCategory::AirportDiagram: return QStringLiteral("Airport Diagram & Layout");
        case ChartCategory::Departure: return QStringLiteral("Departure (SID)");
        case ChartCategory::Arrival: return QStringLiteral("Arrival (STAR)");
        case ChartCategory::Approach: return QStringLiteral("Approach (IAP / IAC)");
        case ChartCategory::Minima: return QStringLiteral("Minima & Hotspots");
        case ChartCategory::General: return QStringLiteral("General & Legends");
        case ChartCategory::Other: return QStringLiteral("Other Charts");
    }
    return QStringLiteral("Other Charts");
}

QString ChartEntry::displayTitle() const
{
    if (!runway.isEmpty()) {
        return title + QStringLiteral(" [RWY ") + runway + QStringLiteral("]");
    }
    return title;
}

QString ChartEntry::providerBadge() const
{
    if (providerId.contains(QStringLiteral("FAA"))) {
        return QStringLiteral("FAA");
    } else if (providerId.contains(QStringLiteral("SIA"))) {
        return QStringLiteral("SIA");
    }
    return QStringLiteral("OA");
}

ProcedureChartMatch ProcedureChartMatch::fromJson(const QJsonObject& obj)
{
    ProcedureChartMatch m;
    m.procedureIdent = obj.value(QStringLiteral("procedure_ident")).toString();
    QString kindStr = obj.value(QStringLiteral("procedure_kind")).toString();
    m.procedureKind = kindStr.isEmpty() ? QChar('F') : kindStr.at(0);
    m.airportIcao = obj.value(QStringLiteral("airport_icao")).toString();
    m.runway = obj.value(QStringLiteral("runway")).toString();
    m.chartId = obj.value(QStringLiteral("chart_id")).toObject().value(QStringLiteral("0")).toString();
    if (m.chartId.isEmpty()) m.chartId = obj.value(QStringLiteral("chart_id")).toString();
    m.confidence = obj.value(QStringLiteral("confidence")).toString();
    m.matchReason = obj.value(QStringLiteral("match_reason")).toString();
    return m;
}

} // namespace openairac
