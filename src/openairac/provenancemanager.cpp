/*****************************************************************************
* OpenAIRAC Map — Navigation Provenance Manager Implementation
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

#include "openairac/provenancemanager.h"
#include "openairac/navigationprovider.h"

namespace openairac {

ProvenanceManager& ProvenanceManager::instance()
{
    static ProvenanceManager s_instance;
    return s_instance;
}

EntityProvenance ProvenanceManager::getAirportProvenance(const QString& airportIdent, const QString& countryIso, int procedureCount) const
{
    EntityProvenance prov;
    NavigationProviderPtr active = ProviderRegistry::instance().activeProvider();
    if (active) {
        prov.sourceEngine = active->displayName();
        prov.airacCycle = active->airacCycle();
        prov.effectiveDate = active->effectiveDate();
        prov.sourceBadge = active->badge();
    }

    QString iso = countryIso.toUpper().trimmed();
    if (iso == QStringLiteral("US") || airportIdent.startsWith(QLatin1Char('K')) || airportIdent.startsWith(QLatin1String("PA")) || airportIdent.startsWith(QLatin1String("PH"))) {
        prov.providerName = QStringLiteral("FAA_CIFP");
        prov.authorityName = QStringLiteral("Federal Aviation Administration (FAA)");
        prov.jurisdiction = QStringLiteral("United States");
        prov.licenseName = QStringLiteral("US Government Work (Public Domain)");
        prov.proceduresAvailable = (procedureCount > 0);
        if (procedureCount > 0) {
            prov.procedureStatusNote = QStringLiteral("Full ARINC 424 Terminal Flight Procedures");
        } else {
            prov.procedureStatusNote = QStringLiteral("No terminal procedures published in CIFP");
        }
    } else if (iso == QStringLiteral("FR") || iso == QStringLiteral("FRA") || airportIdent.startsWith(QLatin1String("LF"))) {
        prov.providerName = QStringLiteral("FR_SIA");
        prov.authorityName = QStringLiteral("SIA DGAC France");
        prov.jurisdiction = QStringLiteral("France");
        prov.licenseName = QStringLiteral("Licence Ouverte v2.0 (Etalab)");
        prov.proceduresAvailable = (procedureCount > 0);
        if (procedureCount == 0) {
            prov.procedureStatusNote = QStringLiteral("Official public provider does not contain terminal procedures.");
        } else {
            prov.procedureStatusNote = QStringLiteral("Procedures available");
        }
    } else if (iso == QStringLiteral("DE") || iso == QStringLiteral("DEU") || airportIdent.startsWith(QLatin1String("ED"))) {
        prov.providerName = QStringLiteral("DFS_Germany");
        prov.authorityName = QStringLiteral("Deutsche Flugsicherung (DFS)");
        prov.jurisdiction = QStringLiteral("Germany");
        prov.licenseName = QStringLiteral("Open Data / Personal Use");
        prov.proceduresAvailable = (procedureCount > 0);
        prov.procedureStatusNote = (procedureCount > 0) ? QStringLiteral("Procedures available") : QStringLiteral("No procedures in provider dataset");
    } else {
        prov.providerName = QStringLiteral("OurAirports / OpenFlightmaps");
        prov.authorityName = QStringLiteral("Open Aviation Community");
        prov.jurisdiction = QStringLiteral("Worldwide");
        prov.licenseName = QStringLiteral("Public Domain / ODbL");
        prov.proceduresAvailable = (procedureCount > 0);
        prov.procedureStatusNote = (procedureCount > 0) ? QStringLiteral("Procedures available") : QStringLiteral("Procedures not provided by open source dataset.");
    }

    return prov;
}

EntityProvenance ProvenanceManager::getNavaidProvenance(const QString& navaidIdent, const QString& region) const
{
    EntityProvenance prov;
    NavigationProviderPtr active = ProviderRegistry::instance().activeProvider();
    if (active) {
        prov.sourceEngine = active->displayName();
        prov.airacCycle = active->airacCycle();
        prov.effectiveDate = active->effectiveDate();
        prov.sourceBadge = active->badge();
    }

    QString reg = region.toUpper().trimmed();
    if (reg.startsWith(QLatin1Char('K')) || reg == QStringLiteral("US")) {
        prov.providerName = QStringLiteral("FAA_CIFP");
        prov.authorityName = QStringLiteral("Federal Aviation Administration (FAA)");
        prov.jurisdiction = QStringLiteral("United States");
        prov.licenseName = QStringLiteral("US Government Work (Public Domain)");
    } else if (reg.startsWith(QLatin1String("LF")) || reg == QStringLiteral("FR")) {
        prov.providerName = QStringLiteral("FR_SIA");
        prov.authorityName = QStringLiteral("SIA DGAC France");
        prov.jurisdiction = QStringLiteral("France");
        prov.licenseName = QStringLiteral("Licence Ouverte v2.0 (Etalab)");
    } else {
        prov.providerName = QStringLiteral("OpenAIRAC Federation");
        prov.authorityName = QStringLiteral("Open Aviation Community");
        prov.jurisdiction = QStringLiteral("Worldwide");
        prov.licenseName = QStringLiteral("Open Aviation Data");
    }

    return prov;
}

QString ProvenanceManager::formatProvenanceHtml(const EntityProvenance& prov) const
{
    QString html;
    html += QStringLiteral("<hr/>");
    html += QStringLiteral("<b>Navigation Source:</b> ") + prov.sourceEngine + QStringLiteral(" [") + prov.sourceBadge + QStringLiteral("]<br/>");
    html += QStringLiteral("<b>Provider:</b> ") + prov.providerName + QStringLiteral("<br/>");
    html += QStringLiteral("<b>Authority:</b> ") + prov.authorityName + QStringLiteral("<br/>");
    html += QStringLiteral("<b>Jurisdiction:</b> ") + prov.jurisdiction + QStringLiteral("<br/>");
    html += QStringLiteral("<b>AIRAC Cycle:</b> ") + prov.airacCycle + QStringLiteral("<br/>");
    html += QStringLiteral("<b>License:</b> ") + prov.licenseName + QStringLiteral("<br/>");
    if (!prov.procedureStatusNote.isEmpty()) {
        html += QStringLiteral("<b>Procedures:</b> ") + (prov.proceduresAvailable ? QStringLiteral("<font color='green'>Available</font>") : QStringLiteral("<font color='#cc6600'>Not provided by source</font>")) + QStringLiteral(" — <i>") + prov.procedureStatusNote + QStringLiteral("</i><br/>");
    }
    return html;
}

QString ProvenanceManager::formatProvenancePlain(const EntityProvenance& prov) const
{
    QString txt;
    txt += QStringLiteral("Navigation Source: ") + prov.sourceEngine + QStringLiteral(" [") + prov.sourceBadge + QStringLiteral("]\n");
    txt += QStringLiteral("Provider: ") + prov.providerName + QStringLiteral("\n");
    txt += QStringLiteral("Authority: ") + prov.authorityName + QStringLiteral("\n");
    txt += QStringLiteral("AIRAC Cycle: ") + prov.airacCycle + QStringLiteral("\n");
    txt += QStringLiteral("License: ") + prov.licenseName + QStringLiteral("\n");
    if (!prov.procedureStatusNote.isEmpty()) {
        txt += QStringLiteral("Procedures: ") + (prov.proceduresAvailable ? QStringLiteral("YES") : QStringLiteral("NO")) + QStringLiteral(" (") + prov.procedureStatusNote + QStringLiteral(")\n");
    }
    return txt;
}

} // namespace openairac
