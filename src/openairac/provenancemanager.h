/*****************************************************************************
* OpenAIRAC Map — Navigation Provenance Manager
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

#ifndef OPENAIRAC_PROVENANCEMANAGER_H
#define OPENAIRAC_PROVENANCEMANAGER_H

#include <QString>
#include <QDateTime>

namespace openairac {

struct EntityProvenance {
    QString sourceEngine = QStringLiteral("OpenAIRAC");
    QString providerName;       // e.g. "FAA_CIFP", "FR_SIA", "DFS_Germany"
    QString authorityName;      // e.g. "Federal Aviation Administration", "DGAC France / SIA"
    QString jurisdiction;       // e.g. "United States", "France"
    QString airacCycle;         // e.g. "2608"
    QDateTime effectiveDate;
    QString licenseName;        // e.g. "US Public Domain", "Licence Ouverte v2.0"
    bool proceduresAvailable = false;
    QString procedureStatusNote; // e.g. "Official public provider does not contain terminal procedures."
    QString sourceBadge = QStringLiteral("OA");
};

class ProvenanceManager {
public:
    static ProvenanceManager& instance();

    EntityProvenance getAirportProvenance(const QString& airportIdent, const QString& countryIso, int procedureCount) const;
    EntityProvenance getNavaidProvenance(const QString& navaidIdent, const QString& region) const;

    QString formatProvenanceHtml(const EntityProvenance& prov) const;
    QString formatProvenancePlain(const EntityProvenance& prov) const;

private:
    ProvenanceManager() = default;
};

} // namespace openairac

#endif // OPENAIRAC_PROVENANCEMANAGER_H
