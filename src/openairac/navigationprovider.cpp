/*****************************************************************************
* OpenAIRAC Map — Navigation Provider Abstraction Implementation
*
* Copyright 2026 OpenAIRAC Contributors
* Copyright 2015-2026 Alexander Barthel alex@littlenavmap.org
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

#include "openairac/navigationprovider.h"
#include "fs/db/databasemeta.h"
#include "sql/sqldatabase.h"
#include "sql/sqlquery.h"
#include "db/dbtools.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDebug>

namespace openairac {

// ============================================================================
// OpenAiracProvider
// ============================================================================

OpenAiracProvider::OpenAiracProvider()
{
    m_caps.airports = true;
    m_caps.runways = true;
    m_caps.navaids = true;
    m_caps.airways = true;
    m_caps.sids = true;
    m_caps.stars = true;
    m_caps.approaches = true;
    m_caps.lpvFas = true;
    m_caps.msa = true;
    m_caps.mora = true;
    m_caps.holding = true;
    m_caps.sceneryGeometry = false;
}

bool OpenAiracProvider::isAvailable() const
{
    return !m_databasePath.isEmpty() && QFile::exists(m_databasePath);
}

bool OpenAiracProvider::refreshMetadata()
{
    if (!isAvailable()) {
        return false;
    }

    try {
        atools::sql::SqlDatabase db(QStringLiteral("OPENAIRAC_META_TEMP"));
        dbtools::openDatabaseFile(&db, m_databasePath, true /* readonly */, false /* createSchema */);

        atools::fs::db::DatabaseMeta meta(&db);
        if (meta.isValid()) {
            m_cycle = meta.getAiracCycle();
            m_effectiveDate = meta.getLastLoadTime();
            m_dataSource = meta.getDataSource();
            m_compilerVersion = meta.getCompilerVersion();
            m_caps.sids = meta.hasSidStar();
            m_caps.stars = meta.hasSidStar();
            m_caps.approaches = meta.hasSidStar();
        }

        dbtools::closeDatabaseFile(&db);
        return true;
    } catch (const std::exception& e) {
        qWarning() << "Failed to refresh OpenAIRAC metadata:" << e.what();
        return false;
    }
}

// ============================================================================
// SimulatorProvider
// ============================================================================

SimulatorProvider::SimulatorProvider(const QString& simName, const QString& simShortName)
    : m_simName(simName), m_shortName(simShortName)
{
}

QString SimulatorProvider::databaseFilename() const
{
    return QStringLiteral("little_navmap_") + m_shortName.toLower() + QStringLiteral(".sqlite");
}

bool SimulatorProvider::isAvailable() const
{
    return !m_databasePath.isEmpty() && QFile::exists(m_databasePath);
}

ProviderCapabilities SimulatorProvider::capabilities() const
{
    ProviderCapabilities caps;
    caps.airports = true;
    caps.runways = true;
    caps.navaids = true;
    caps.airways = true;
    caps.sceneryGeometry = true; // Provides 3D taxiways, aprons, parking
    return caps;
}

bool SimulatorProvider::refreshMetadata()
{
    if (!isAvailable()) {
        return false;
    }

    try {
        atools::sql::SqlDatabase db(QStringLiteral("SIM_META_TEMP"));
        dbtools::openDatabaseFile(&db, m_databasePath, true /* readonly */, false /* createSchema */);

        atools::fs::db::DatabaseMeta meta(&db);
        if (meta.isValid()) {
            m_cycle = meta.getAiracCycle();
            m_effectiveDate = meta.getLastLoadTime();
            m_compilerVersion = meta.getCompilerVersion();
        }

        dbtools::closeDatabaseFile(&db);
        return true;
    } catch (const std::exception& e) {
        qWarning() << "Failed to refresh Simulator metadata:" << e.what();
        return false;
    }
}

// ============================================================================
// NavigraphProvider
// ============================================================================

NavigraphProvider::NavigraphProvider()
{
}

bool NavigraphProvider::isAvailable() const
{
    return !m_databasePath.isEmpty() && QFile::exists(m_databasePath);
}

ProviderCapabilities NavigraphProvider::capabilities() const
{
    ProviderCapabilities caps;
    caps.airports = true;
    caps.runways = true;
    caps.navaids = true;
    caps.airways = true;
    caps.sids = true;
    caps.stars = true;
    caps.approaches = true;
    caps.holding = true;
    caps.sceneryGeometry = false;
    return caps;
}

bool NavigraphProvider::refreshMetadata()
{
    if (!isAvailable()) {
        return false;
    }

    try {
        atools::sql::SqlDatabase db(QStringLiteral("NAVIGRAPH_META_TEMP"));
        dbtools::openDatabaseFile(&db, m_databasePath, true /* readonly */, false /* createSchema */);

        atools::fs::db::DatabaseMeta meta(&db);
        if (meta.isValid()) {
            m_cycle = meta.getAiracCycle();
            m_effectiveDate = meta.getLastLoadTime();
            m_compilerVersion = meta.getCompilerVersion();
        }

        dbtools::closeDatabaseFile(&db);
        return true;
    } catch (const std::exception& e) {
        qWarning() << "Failed to refresh Navigraph metadata:" << e.what();
        return false;
    }
}

// ============================================================================
// ProviderRegistry
// ============================================================================

ProviderRegistry& ProviderRegistry::instance()
{
    static ProviderRegistry s_instance;
    return s_instance;
}

ProviderRegistry::ProviderRegistry()
{
    // Register standard first-class OpenAIRAC provider
    NavigationProviderPtr oa(new OpenAiracProvider());
    m_providers.insert(oa->id(), oa);

    // Register optional Navigraph provider
    NavigationProviderPtr navi(new NavigraphProvider());
    m_providers.insert(navi->id(), navi);
}

void ProviderRegistry::registerProvider(NavigationProviderPtr provider)
{
    if (provider) {
        m_providers.insert(provider->id(), provider);
    }
}

NavigationProviderPtr ProviderRegistry::provider(const QString& id) const
{
    return m_providers.value(id);
}

QList<NavigationProviderPtr> ProviderRegistry::allProviders() const
{
    return m_providers.values();
}

NavigationProviderPtr ProviderRegistry::activeProvider() const
{
    NavigationProviderPtr act = m_providers.value(m_activeProviderId);
    if (!act || !act->isAvailable()) {
        // Default to OpenAIRAC if available
        NavigationProviderPtr oa = openAiracProvider();
        if (oa && oa->isAvailable()) {
            return oa;
        }
    }
    return act;
}

void ProviderRegistry::setActiveProvider(const QString& id)
{
    if (m_providers.contains(id)) {
        m_activeProviderId = id;
    }
}

NavigationProviderPtr ProviderRegistry::openAiracProvider() const
{
    return m_providers.value(QStringLiteral("openairac"));
}

NavigationProviderPtr ProviderRegistry::navigraphProvider() const
{
    return m_providers.value(QStringLiteral("navigraph"));
}

} // namespace openairac
