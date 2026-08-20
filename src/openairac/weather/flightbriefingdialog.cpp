/*****************************************************************************
* OpenAIRAC Map — Integrated Preflight Flight Briefing Dialog Implementation
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

#include "openairac/weather/flightbriefingdialog.h"
#include "openairac/weather/weatherclient.h"
#include "openairac/charts/chartclient.h"
#include "openairac/online/onlineclient.h"
#include "openairac/navigationprovider.h"
#include <QHBoxLayout>
#include <QClipboard>
#include <QGuiApplication>
#include <QPrinter>
#include <QPrintDialog>
#include <QMessageBox>

namespace openairac {

FlightBriefingDialog::FlightBriefingDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("OpenAIRAC Preflight Briefing"));
    resize(850, 700);
    setupUi();

    connect(&WeatherClient::instance(), &WeatherClient::airportWeatherReady, this, &FlightBriefingDialog::onAirportWeatherReady);
    connect(&WeatherClient::instance(), &WeatherClient::sigmetsReady, this, &FlightBriefingDialog::onSigmetsReady);
}

void FlightBriefingDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(6);

    // Top action bar
    QHBoxLayout *topLayout = new QHBoxLayout();
    m_refreshBtn = new QPushButton(tr("🔄 Refresh Weather"), this);
    topLayout->addWidget(m_refreshBtn);

    m_copyBtn = new QPushButton(tr("📋 Copy Plain Text"), this);
    topLayout->addWidget(m_copyBtn);

    m_printBtn = new QPushButton(tr("🖨️ Print Briefing"), this);
    topLayout->addWidget(m_printBtn);

    topLayout->addStretch();
    mainLayout->addLayout(topLayout);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setVisible(false);
    mainLayout->addWidget(m_progressBar);

    // Rich Text Browser
    m_browser = new QTextBrowser(this);
    m_browser->setOpenExternalLinks(true);
    mainLayout->addWidget(m_browser);

    connect(m_refreshBtn, &QPushButton::clicked, this, &FlightBriefingDialog::refreshBriefing);
    connect(m_copyBtn, &QPushButton::clicked, this, &FlightBriefingDialog::copyPlainText);
    connect(m_printBtn, &QPushButton::clicked, this, &FlightBriefingDialog::printBriefing);
}

void FlightBriefingDialog::setRoute(
    const QString& departureIcao,
    const QString& destinationIcao,
    const QStringList& alternateIcaos,
    const QList<QPair<double, double>>& routeCoordinates,
    double flightHours
)
{
    m_depIcao = departureIcao.trimmed().toUpper();
    m_destIcao = destinationIcao.trimmed().toUpper();
    m_alts = alternateIcaos;
    m_routeCoords = routeCoordinates;
    m_flightHours = flightHours;

    refreshBriefing();
}

void FlightBriefingDialog::refreshBriefing()
{
    if (m_depIcao.isEmpty()) return;

    m_progressBar->setVisible(true);
    m_progressBar->setRange(0, 0);

    WeatherClient::instance().requestAirportWeather(m_depIcao, true /* force */);
    if (!m_destIcao.isEmpty()) {
        WeatherClient::instance().requestAirportWeather(m_destIcao, true);
    }
    WeatherClient::instance().requestActiveSigmets(true);

    updateBriefingDisplay();
}

void FlightBriefingDialog::onAirportWeatherReady(const QString& stationId, const openairac::MetarInfo& metar, const openairac::TafInfo& taf)
{
    if (stationId == m_depIcao) {
        m_depMetar = metar;
        m_depTaf = taf;
    } else if (stationId == m_destIcao) {
        m_destMetar = metar;
        m_destTaf = taf;
    }
    updateBriefingDisplay();
}

void FlightBriefingDialog::onSigmetsReady(const QList<openairac::SigmetAdvisory>& sigmets)
{
    m_sigmets = sigmets;
    m_progressBar->setVisible(false);
    updateBriefingDisplay();
}

QString FlightBriefingDialog::escapeHtml(const QString& str)
{
    QString res = str;
    return res.toHtmlEscaped();
}

void FlightBriefingDialog::updateBriefingDisplay()
{
    m_browser->setHtml(generateBriefingHtml());
}

QString FlightBriefingDialog::generateBriefingHtml() const
{
    QDateTime now = QDateTime::currentDateTimeUtc();
    QDateTime eta = now.addSecs(static_cast<qint64>(m_flightHours * 3600.0));

    QString html;
    html += QStringLiteral("<div style='font-family: sans-serif; padding: 10px; color: #222;'>");

    // Header
    html += QStringLiteral("<h2 style='margin-bottom: 2px; color: #0a4b78;'>OpenAIRAC Flight Briefing</h2>");
    html += QStringLiteral("<p style='font-size: 16px; margin-top: 0;'><b>") + escapeHtml(m_depIcao) + QStringLiteral(" &rarr; ") + escapeHtml(m_destIcao) + QStringLiteral("</b></p>");
    html += QStringLiteral("<div style='font-size: 11px; color: #666; margin-bottom: 12px;'>Generated: ") + now.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss 'UTC'")) + QStringLiteral(" | Est. Enroute: ") + QString::number(m_flightHours, 'f', 1) + QStringLiteral(" hrs | ETA: ") + eta.toString(QStringLiteral("HH:mm'Z'")) + QStringLiteral("</div>");

    // 1. Departure Block
    html += QStringLiteral("<div style='border: 1px solid #ccc; border-radius: 4px; padding: 8px; margin-bottom: 10px; background: #fafafa;'>");
    html += QStringLiteral("<h3 style='margin-top: 0; color: #0a4b78;'>1. Departure: ") + escapeHtml(m_depIcao) + QStringLiteral("</h3>");
    if (m_depMetar.isValid()) {
        html += QStringLiteral("<p><b>METAR</b> <span style='background: ") + m_depMetar.flightCategoryColorHex() + QStringLiteral("; color: white; padding: 2px 6px; border-radius: 3px; font-weight: bold;'>") + m_depMetar.flightCategoryString() + QStringLiteral("</span> (Age: ") + QString::number(m_depMetar.ageMinutes(now)) + QStringLiteral(" min):<br/><code>") + escapeHtml(m_depMetar.rawText) + QStringLiteral("</code></p>");
        html += QStringLiteral("<p style='font-size: 12px;'><b>Conditions:</b> Wind ") + QString::number(m_depMetar.windDirDeg) + QStringLiteral("/") + QString::number(m_depMetar.windSpeedKts) + QStringLiteral(" kt, Temp ") + QString::number(m_depMetar.temperatureC, 'f', 1) + QStringLiteral("°C, Dewp ") + QString::number(m_depMetar.dewpointC, 'f', 1) + QStringLiteral("°C, Vis ") + QString::number(m_depMetar.visibilitySm, 'f', 1) + QStringLiteral(" SM, QNH ") + QString::number(m_depMetar.altimeterHpa, 'f', 0) + QStringLiteral(" hPa</p>");
    } else {
        html += QStringLiteral("<p><i>METAR observation not available.</i></p>");
    }
    if (m_depTaf.isValid()) {
        html += QStringLiteral("<p><b>TAF</b> (Valid ") + m_depTaf.validFrom.toString(QStringLiteral("HH:mm'Z'")) + QStringLiteral(" to ") + m_depTaf.validTo.toString(QStringLiteral("HH:mm'Z'")) + QStringLiteral("):<br/><code>") + escapeHtml(m_depTaf.rawText) + QStringLiteral("</code></p>");
    }
    int depChartsCount = ChartClient::instance().getChartsForAirport(m_depIcao).size();
    html += QStringLiteral("<p style='font-size: 12px; color: #555;'><b>Charts:</b> ") + QString::number(depChartsCount) + QStringLiteral(" published plates available | <b>Navdata:</b> ") + (m_depIcao.startsWith(QLatin1Char('K')) ? QStringLiteral("FAA CIFP procedures active") : QStringLiteral("OpenAIRAC navdata active")) + QStringLiteral("</p>");
    html += QStringLiteral("</div>");

    // 2. Destination Block
    html += QStringLiteral("<div style='border: 1px solid #ccc; border-radius: 4px; padding: 8px; margin-bottom: 10px; background: #fafafa;'>");
    html += QStringLiteral("<h3 style='margin-top: 0; color: #0a4b78;'>2. Destination: ") + escapeHtml(m_destIcao) + QStringLiteral(" (ETA ") + eta.toString(QStringLiteral("HH:mm'Z'")) + QStringLiteral(")</h3>");
    if (m_destMetar.isValid()) {
        html += QStringLiteral("<p><b>Current METAR</b> <span style='background: ") + m_destMetar.flightCategoryColorHex() + QStringLiteral("; color: white; padding: 2px 6px; border-radius: 3px; font-weight: bold;'>") + m_destMetar.flightCategoryString() + QStringLiteral("</span>:<br/><code>") + escapeHtml(m_destMetar.rawText) + QStringLiteral("</code></p>");
    }
    if (m_destTaf.isValid()) {
        const TafPeriodInfo *etaFcst = m_destTaf.forecastAtEta(eta);
        if (etaFcst) {
            html += QStringLiteral("<p><b>Forecast at ETA (") + eta.toString(QStringLiteral("HH:mm'Z'")) + QStringLiteral("):</b> <span style='background: ") + (etaFcst->flightCategory == FlightCategory::Vfr ? QStringLiteral("#28a745") : QStringLiteral("#dc3545")) + QStringLiteral("; color: white; padding: 2px 6px; border-radius: 3px; font-weight: bold;'>") + (etaFcst->flightCategory == FlightCategory::Vfr ? QStringLiteral("VFR") : QStringLiteral("IFR/MVFR")) + QStringLiteral("</span><br/><code>") + escapeHtml(etaFcst->rawPeriod) + QStringLiteral("</code></p>");
        }
        html += QStringLiteral("<p><b>Full TAF:</b><br/><code>") + escapeHtml(m_destTaf.rawText) + QStringLiteral("</code></p>");
    }
    int destChartsCount = ChartClient::instance().getChartsForAirport(m_destIcao).size();
    QString destNavNote = (m_destIcao == QStringLiteral("LFPG") || m_destIcao.startsWith(QLatin1String("LF")))
        ? QStringLiteral("Public SIA dataset contains 0 procedures; official eAIP charts active")
        : QStringLiteral("OpenAIRAC navdata active");
    html += QStringLiteral("<p style='font-size: 12px; color: #555;'><b>Charts:</b> ") + QString::number(destChartsCount) + QStringLiteral(" published plates available | <b>Navdata:</b> ") + destNavNote + QStringLiteral("</p>");
    html += QStringLiteral("</div>");

    // 3. Enroute Hazards & SIGMET Block
    html += QStringLiteral("<div style='border: 1px solid #e0a800; border-radius: 4px; padding: 8px; margin-bottom: 10px; background: #fffdf5;'>");
    html += QStringLiteral("<h3 style='margin-top: 0; color: #a07000;'>3. Enroute Hazards & Weather Advisories (Route Corridor: 50 NM)</h3>");

    if (m_sigmets.isEmpty()) {
        html += QStringLiteral("<p><i>No active SIGMET advisories reported along the route corridor.</i></p>");
    } else {
        html += QStringLiteral("<p><b>Active SIGMETs in Airspace (") + QString::number(m_sigmets.size()) + QStringLiteral("):</b></p>");
        for (const SigmetAdvisory& s : m_sigmets.mid(0, 5)) {
            html += QStringLiteral("<div style='margin-bottom: 6px;'><b>[") + escapeHtml(s.hazard) + QStringLiteral("]</b> FIR: <b>") + escapeHtml(s.firId) + QStringLiteral("</b> (Valid ") + s.validFrom.toString(QStringLiteral("HH:mm'Z'")) + QStringLiteral(" to ") + s.validTo.toString(QStringLiteral("HH:mm'Z'")) + QStringLiteral("):<br/><code>") + escapeHtml(s.rawText) + QStringLiteral("</code></div>");
        }
    }
    html += QStringLiteral("</div>");

    // 4. Online Network Awareness Block [VATSIM]
    RouteOnlineItem onlineAwareness = OnlineClient::instance().getRouteOnlineAwareness(m_depIcao, m_destIcao, m_routeCoords, 50.0);
    html += QStringLiteral("<div style='border: 1px solid #17a2b8; border-radius: 4px; padding: 8px; margin-bottom: 10px; background: #f4faff;'>");
    html += QStringLiteral("<h3 style='margin-top: 0; color: #117a8b;'>4. Online Network Awareness [VATSIM]</h3>");

    // Departure ATC
    html += QStringLiteral("<p style='margin-bottom: 4px;'><b>Departure ATC (") + escapeHtml(m_depIcao) + QStringLiteral("):</b> ");
    if (onlineAwareness.departureAtc.isEmpty()) {
        html += QStringLiteral("<i>No active ATC stations online.</i>");
    } else {
        QStringList atcStrs;
        for (const OnlineControllerItem& c : onlineAwareness.departureAtc) {
            atcStrs.append(QStringLiteral("<b>") + escapeHtml(c.callsign) + QStringLiteral("</b> (") + escapeHtml(c.frequency) + QStringLiteral(" - ") + escapeHtml(c.facilityTypeName) + QStringLiteral(")"));
        }
        html += atcStrs.join(QStringLiteral(", "));
    }
    if (onlineAwareness.hasDepartureAtis) {
        html += QStringLiteral("<br/><b>Departure ATIS:</b> ") + escapeHtml(onlineAwareness.departureAtis.callsign) + QStringLiteral(" (") + QString(onlineAwareness.departureAtis.atisCode) + QStringLiteral(" - ") + escapeHtml(onlineAwareness.departureAtis.frequency) + QStringLiteral(")");
    }
    html += QStringLiteral("</p>");

    // Enroute ATC
    html += QStringLiteral("<p style='margin-bottom: 4px;'><b>Enroute ATC Sectors:</b> ");
    if (onlineAwareness.enrouteAtc.isEmpty()) {
        html += QStringLiteral("<i>No relevant enroute centers online.</i>");
    } else {
        QStringList ctrStrs;
        for (const OnlineControllerItem& c : onlineAwareness.enrouteAtc) {
            ctrStrs.append(QStringLiteral("<b>") + escapeHtml(c.callsign) + QStringLiteral("</b> (") + escapeHtml(c.frequency) + QStringLiteral(")"));
        }
        html += ctrStrs.join(QStringLiteral(", "));
    }
    html += QStringLiteral("</p>");

    // Arrival ATC
    html += QStringLiteral("<p style='margin-bottom: 4px;'><b>Arrival ATC (") + escapeHtml(m_destIcao) + QStringLiteral("):</b> ");
    if (onlineAwareness.arrivalAtc.isEmpty()) {
        html += QStringLiteral("<i>No active ATC stations online.</i>");
    } else {
        QStringList atcStrs;
        for (const OnlineControllerItem& c : onlineAwareness.arrivalAtc) {
            atcStrs.append(QStringLiteral("<b>") + escapeHtml(c.callsign) + QStringLiteral("</b> (") + escapeHtml(c.frequency) + QStringLiteral(" - ") + escapeHtml(c.facilityTypeName) + QStringLiteral(")"));
        }
        html += atcStrs.join(QStringLiteral(", "));
    }
    if (onlineAwareness.hasArrivalAtis) {
        html += QStringLiteral("<br/><b>Arrival ATIS:</b> ") + escapeHtml(onlineAwareness.arrivalAtis.callsign) + QStringLiteral(" (") + QString(onlineAwareness.arrivalAtis.atisCode) + QStringLiteral(" - ") + escapeHtml(onlineAwareness.arrivalAtis.frequency) + QStringLiteral(")");
    }
    html += QStringLiteral("</p>");

    // Traffic
    html += QStringLiteral("<p style='margin-bottom: 4px;'><b>Online Traffic:</b> ") + QString::number(onlineAwareness.trafficInCorridor.size()) + QStringLiteral(" aircraft in route corridor (50 NM) | ") + QString::number(onlineAwareness.trafficNearDestination.size()) + QStringLiteral(" aircraft near destination</p>");

    // Matching Events
    if (!onlineAwareness.matchingEvents.isEmpty()) {
        html += QStringLiteral("<p style='margin-bottom: 4px; color: #d9534f;'><b>Matching Online Events:</b></p><ul>");
        for (const OnlineEventItem& ev : onlineAwareness.matchingEvents) {
            html += QStringLiteral("<li><b>") + escapeHtml(ev.name) + QStringLiteral("</b> (") + ev.startTime.toString(QStringLiteral("yyyy-MM-dd hh:mmZ")) + QStringLiteral(" to ") + ev.endTime.toString(QStringLiteral("yyyy-MM-dd hh:mmZ")) + QStringLiteral(")</li>");
        }
        html += QStringLiteral("</ul>");
    }
    html += QStringLiteral("</div>");

    // Footer & Provenance
    html += QStringLiteral("<div style='font-size: 11px; color: #777; border-top: 1px solid #ddd; padding-top: 6px;'>");
    html += QStringLiteral("<b>Data Provenance:</b> Navdata: OpenAIRAC | Charts: FAA d-TPP / France SIA through OpenAIRAC | Weather: NOAA AviationWeather.gov | Online: VATSIM Data API v3<br/>");
    html += QStringLiteral("<i>OpenAIRAC first: transparent open aviation data without proprietary subscriptions.</i>");
    html += QStringLiteral("</div>");
    return html;
}

QString FlightBriefingDialog::generateBriefingPlainText() const
{
    QDateTime now = QDateTime::currentDateTimeUtc();
    QDateTime eta = now.addSecs(static_cast<qint64>(m_flightHours * 3600.0));

    QString txt;
    txt += QStringLiteral("================================================================================\n");
    txt += QStringLiteral("OPENAIRAC FLIGHT BRIEFING — ") + m_depIcao + QStringLiteral(" -> ") + m_destIcao + QStringLiteral("\n");
    txt += QStringLiteral("Generated: ") + now.toString(Qt::ISODate) + QStringLiteral(" | ETA: ") + eta.toString(QStringLiteral("HH:mm'Z'")) + QStringLiteral("\n");
    txt += QStringLiteral("================================================================================\n\n");

    txt += QStringLiteral("1. DEPARTURE: ") + m_depIcao + QStringLiteral("\n");
    if (m_depMetar.isValid()) {
        txt += QStringLiteral("   METAR: ") + m_depMetar.rawText + QStringLiteral("\n");
    }
    if (m_depTaf.isValid()) {
        txt += QStringLiteral("   TAF:   ") + m_depTaf.rawText + QStringLiteral("\n");
    }
    txt += QStringLiteral("\n2. DESTINATION: ") + m_destIcao + QStringLiteral(" (ETA ") + eta.toString(QStringLiteral("HH:mm'Z'")) + QStringLiteral(")\n");
    if (m_destMetar.isValid()) {
        txt += QStringLiteral("   Current METAR: ") + m_destMetar.rawText + QStringLiteral("\n");
    }
    if (m_destTaf.isValid()) {
        txt += QStringLiteral("   TAF:           ") + m_destTaf.rawText + QStringLiteral("\n");
    }
    txt += QStringLiteral("\n3. ENROUTE HAZARDS: ") + QString::number(m_sigmets.size()) + QStringLiteral(" SIGMETs active.\n");
    for (const SigmetAdvisory& s : m_sigmets.mid(0, 5)) {
        txt += QStringLiteral("   - [") + s.hazard + QStringLiteral("] FIR: ") + s.firId + QStringLiteral(" (") + s.validFrom.toString(QStringLiteral("HH:mm'Z'")) + QStringLiteral(" to ") + s.validTo.toString(QStringLiteral("HH:mm'Z'")) + QStringLiteral(")\n");
        txt += QStringLiteral("     ") + s.rawText + QStringLiteral("\n");
    }

    txt += QStringLiteral("\n4. ONLINE NETWORK AWARENESS [VATSIM]:\n");
    RouteOnlineItem onlineAwareness = OnlineClient::instance().getRouteOnlineAwareness(m_depIcao, m_destIcao, m_routeCoords, 50.0);
    txt += QStringLiteral("   Departure ATC (") + m_depIcao + QStringLiteral("): ") + QString::number(onlineAwareness.departureAtc.size()) + QStringLiteral(" online\n");
    for (const OnlineControllerItem& c : onlineAwareness.departureAtc) {
        txt += QStringLiteral("     - ") + c.callsign + QStringLiteral(" (") + c.frequency + QStringLiteral(" - ") + c.facilityTypeName + QStringLiteral(")\n");
    }
    txt += QStringLiteral("   Arrival ATC   (") + m_destIcao + QStringLiteral("): ") + QString::number(onlineAwareness.arrivalAtc.size()) + QStringLiteral(" online\n");
    for (const OnlineControllerItem& c : onlineAwareness.arrivalAtc) {
        txt += QStringLiteral("     - ") + c.callsign + QStringLiteral(" (") + c.frequency + QStringLiteral(" - ") + c.facilityTypeName + QStringLiteral(")\n");
    }
    txt += QStringLiteral("   Corridor Traffic: ") + QString::number(onlineAwareness.trafficInCorridor.size()) + QStringLiteral(" aircraft\n");
    txt += QStringLiteral("\n================================================================================\n");
    txt += QStringLiteral("End of OpenAIRAC Flight Briefing\n");
    return txt;
}

void FlightBriefingDialog::copyPlainText()
{
    QClipboard *cb = QGuiApplication::clipboard();
    if (cb) {
        cb->setText(generateBriefingPlainText());
        QMessageBox::information(this, tr("Briefing Copied"), tr("Plain text flight briefing copied to clipboard."));
    }
}

void FlightBriefingDialog::printBriefing()
{
    QPrinter printer(QPrinter::HighResolution);
    QPrintDialog dialog(&printer, this);
    if (dialog.exec() == QDialog::Accepted) {
        m_browser->print(&printer);
    }
}

} // namespace openairac
