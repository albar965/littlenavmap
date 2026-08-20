/*****************************************************************************
* OpenAIRAC Map — Navigation Provider Abstraction
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

#ifndef OPENAIRAC_NAVIGATIONPROVIDER_H
#define OPENAIRAC_NAVIGATIONPROVIDER_H

#include <QString>
#include <QStringList>
#include <QDateTime>
#include <QHash>
#include <QList>
#include <QSharedPointer>

namespace openairac {

/**
 * License category of a navigation data provider.
 */
enum class ProviderLicenseType {
    PublicDomain,     // e.g. US FAA CIFP
    OpenLicense,      // e.g. France SIA Licence Ouverte, OpenFlightmaps
    PersonalUseOnly,  // e.g. DFS Germany AIP
    CommercialProprietary, // e.g. Navigraph, Jeppesen (Optional third-party)
    SimulatorScenery  // Compiled simulator BGL / apt.dat
};

/**
 * Coverage capabilities supported by a navigation provider.
 */
struct ProviderCapabilities {
    bool airports = true;
    bool runways = true;
    bool navaids = true;
    bool airways = true;
    bool sids = false;
    bool stars = false;
    bool approaches = false;
    bool lpvFas = false;
    bool msa = false;
    bool mora = false;
    bool holding = false;
    bool sceneryGeometry = false; // 3D taxiways, aprons, parking
};

/**
 * Abstract Navigation Provider.
 *
 * Encapsulates navigation database identity, provenance metadata,
 * capability matrix, and blending policies.
 */
class NavigationProvider {
public:
    virtual ~NavigationProvider() = default;

    virtual QString id() const = 0;
    virtual QString displayName() const = 0;
    virtual QString shortName() const = 0;
    virtual QString badge() const = 0; // [OA], [SIM], [N]
    virtual ProviderLicenseType licenseType() const = 0;
    virtual QString licenseNotice() const = 0;

    virtual QString databaseFilename() const = 0;
    virtual QString databasePath() const = 0;
    virtual void setDatabasePath(const QString& path) = 0;

    virtual bool isAvailable() const = 0;
    virtual bool isEnabled() const = 0;
    virtual void setEnabled(bool enabled) = 0;

    virtual int priority() const = 0; // Higher = preferred
    virtual bool supportsSceneryBlending() const = 0;

    virtual QString airacCycle() const = 0;
    virtual QDateTime effectiveDate() const = 0;
    virtual QString dataSourceName() const = 0;
    virtual QString compilerVersion() const = 0;

    virtual ProviderCapabilities capabilities() const = 0;
    virtual QString authority() const = 0;
    virtual QString jurisdiction() const = 0;

    // Refresh metadata from database file
    virtual bool refreshMetadata() = 0;
};

using NavigationProviderPtr = QSharedPointer<NavigationProvider>;

/**
 * OpenAIRAC First-Class Navigation Provider (Default)
 */
class OpenAiracProvider : public NavigationProvider {
public:
    OpenAiracProvider();
    virtual ~OpenAiracProvider() override = default;

    virtual QString id() const override { return QStringLiteral("openairac"); }
    virtual QString displayName() const override { return QStringLiteral("OpenAIRAC"); }
    virtual QString shortName() const override { return QStringLiteral("OpenAIRAC"); }
    virtual QString badge() const override { return QStringLiteral("OA"); }
    virtual ProviderLicenseType licenseType() const override { return ProviderLicenseType::OpenLicense; }
    virtual QString licenseNotice() const override { return QStringLiteral("Public Domain & Open Aviation Data"); }

    virtual QString databaseFilename() const override { return QStringLiteral("openairac.sqlite"); }
    virtual QString databasePath() const override { return m_databasePath; }
    virtual void setDatabasePath(const QString& path) override { m_databasePath = path; }

    virtual bool isAvailable() const override;
    virtual bool isEnabled() const override { return m_enabled; }
    virtual void setEnabled(bool enabled) override { m_enabled = enabled; }

    virtual int priority() const override { return 100; } // Top priority
    virtual bool supportsSceneryBlending() const override { return true; }

    virtual QString airacCycle() const override { return m_cycle; }
    virtual QDateTime effectiveDate() const override { return m_effectiveDate; }
    virtual QString dataSourceName() const override { return m_dataSource; }
    virtual QString compilerVersion() const override { return m_compilerVersion; }

    virtual ProviderCapabilities capabilities() const override { return m_caps; }
    virtual QString authority() const override { return m_authority; }
    virtual QString jurisdiction() const override { return m_jurisdiction; }

    virtual bool refreshMetadata() override;

private:
    QString m_databasePath;
    bool m_enabled = true;
    QString m_cycle = QStringLiteral("----");
    QDateTime m_effectiveDate;
    QString m_dataSource = QStringLiteral("OPENAIRAC");
    QString m_compilerVersion;
    QString m_authority = QStringLiteral("OpenAIRAC Federation");
    QString m_jurisdiction = QStringLiteral("Worldwide / Government");
    ProviderCapabilities m_caps;
};

/**
 * Simulator Scenery Navigation Provider
 */
class SimulatorProvider : public NavigationProvider {
public:
    explicit SimulatorProvider(const QString& simName, const QString& simShortName);
    virtual ~SimulatorProvider() override = default;

    virtual QString id() const override { return QStringLiteral("simulator_") + m_shortName.toLower(); }
    virtual QString displayName() const override { return m_simName + QStringLiteral(" Scenery"); }
    virtual QString shortName() const override { return m_shortName; }
    virtual QString badge() const override { return QStringLiteral("SIM"); }
    virtual ProviderLicenseType licenseType() const override { return ProviderLicenseType::SimulatorScenery; }
    virtual QString licenseNotice() const override { return QStringLiteral("Simulator Scenery Data"); }

    virtual QString databaseFilename() const override;
    virtual QString databasePath() const override { return m_databasePath; }
    virtual void setDatabasePath(const QString& path) override { m_databasePath = path; }

    virtual bool isAvailable() const override;
    virtual bool isEnabled() const override { return m_enabled; }
    virtual void setEnabled(bool enabled) override { m_enabled = enabled; }

    virtual int priority() const override { return 50; }
    virtual bool supportsSceneryBlending() const override { return false; }

    virtual QString airacCycle() const override { return m_cycle; }
    virtual QDateTime effectiveDate() const override { return m_effectiveDate; }
    virtual QString dataSourceName() const override { return m_simName; }
    virtual QString compilerVersion() const override { return m_compilerVersion; }

    virtual ProviderCapabilities capabilities() const override;
    virtual QString authority() const override { return m_simName; }
    virtual QString jurisdiction() const override { return QStringLiteral("Local Scenery"); }

    virtual bool refreshMetadata() override;

private:
    QString m_simName;
    QString m_shortName;
    QString m_databasePath;
    bool m_enabled = true;
    QString m_cycle;
    QDateTime m_effectiveDate;
    QString m_compilerVersion;
};

/**
 * Navigraph Optional Third-Party Navigation Provider
 */
class NavigraphProvider : public NavigationProvider {
public:
    NavigraphProvider();
    virtual ~NavigraphProvider() override = default;

    virtual QString id() const override { return QStringLiteral("navigraph"); }
    virtual QString displayName() const override { return QStringLiteral("Navigraph (Optional)"); }
    virtual QString shortName() const override { return QStringLiteral("Navigraph"); }
    virtual QString badge() const override { return QStringLiteral("N"); }
    virtual ProviderLicenseType licenseType() const override { return ProviderLicenseType::CommercialProprietary; }
    virtual QString licenseNotice() const override { return QStringLiteral("Commercial Navdata (Separate / Optional)"); }

    virtual QString databaseFilename() const override { return QStringLiteral("little_navmap_navigraph.sqlite"); }
    virtual QString databasePath() const override { return m_databasePath; }
    virtual void setDatabasePath(const QString& path) override { m_databasePath = path; }

    virtual bool isAvailable() const override;
    virtual bool isEnabled() const override { return m_enabled; }
    virtual void setEnabled(bool enabled) override { m_enabled = enabled; }

    virtual int priority() const override { return 10; } // Optional fallback only
    virtual bool supportsSceneryBlending() const override { return true; }

    virtual QString airacCycle() const override { return m_cycle; }
    virtual QDateTime effectiveDate() const override { return m_effectiveDate; }
    virtual QString dataSourceName() const override { return QStringLiteral("NAVIGRAPH"); }
    virtual QString compilerVersion() const override { return m_compilerVersion; }

    virtual ProviderCapabilities capabilities() const override;
    virtual QString authority() const override { return QStringLiteral("Jeppesen / Navigraph"); }
    virtual QString jurisdiction() const override { return QStringLiteral("Commercial Worldwide"); }

    virtual bool refreshMetadata() override;

    bool fallbackWhenOpenAiracMissing() const { return m_fallbackEnabled; }
    void setFallbackWhenOpenAiracMissing(bool fallback) { m_fallbackEnabled = fallback; }

private:
    QString m_databasePath;
    bool m_enabled = false; // OFF by default
    bool m_fallbackEnabled = false; // OFF by default
    QString m_cycle = QStringLiteral("----");
    QDateTime m_effectiveDate;
    QString m_compilerVersion;
};

/**
 * Provider Registry and Blending Controller
 */
class ProviderRegistry {
public:
    static ProviderRegistry& instance();

    void registerProvider(NavigationProviderPtr provider);
    NavigationProviderPtr provider(const QString& id) const;
    QList<NavigationProviderPtr> allProviders() const;

    NavigationProviderPtr activeProvider() const;
    void setActiveProvider(const QString& id);

    NavigationProviderPtr openAiracProvider() const;
    NavigationProviderPtr navigraphProvider() const;

    bool isSceneryBlendingActive() const { return m_sceneryBlending; }
    void setSceneryBlendingActive(bool active) { m_sceneryBlending = active; }

private:
    ProviderRegistry();
    QHash<QString, NavigationProviderPtr> m_providers;
    QString m_activeProviderId = QStringLiteral("openairac");
    bool m_sceneryBlending = true;
};

} // namespace openairac

#endif // OPENAIRAC_NAVIGATIONPROVIDER_H
