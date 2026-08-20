/*****************************************************************************
* OpenAIRAC Map — Integrated Preflight Flight Briefing Dialog
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

#ifndef OPENAIRAC_FLIGHTBRIEFINGDIALOG_H
#define OPENAIRAC_FLIGHTBRIEFINGDIALOG_H

#include "openairac/weather/weathermodel.h"
#include <QDialog>
#include <QTextBrowser>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>

namespace openairac {

class FlightBriefingDialog : public QDialog {
    Q_OBJECT

public:
    explicit FlightBriefingDialog(QWidget *parent = nullptr);
    virtual ~FlightBriefingDialog() override = default;

    void setRoute(
        const QString& departureIcao,
        const QString& destinationIcao,
        const QStringList& alternateIcaos,
        const QList<QPair<double, double>>& routeCoordinates,
        double flightHours = 7.0
    );

public slots:
    void refreshBriefing();
    void copyPlainText();
    void printBriefing();

private slots:
    void onAirportWeatherReady(const QString& stationId, const openairac::MetarInfo& metar, const openairac::TafInfo& taf);
    void onSigmetsReady(const QList<openairac::SigmetAdvisory>& sigmets);

private:
    QString m_depIcao;
    QString m_destIcao;
    QStringList m_alts;
    QList<QPair<double, double>> m_routeCoords;
    double m_flightHours = 7.0;

    MetarInfo m_depMetar;
    TafInfo m_depTaf;
    MetarInfo m_destMetar;
    TafInfo m_destTaf;
    QList<SigmetAdvisory> m_sigmets;

    QTextBrowser *m_browser = nullptr;
    QPushButton *m_refreshBtn = nullptr;
    QPushButton *m_copyBtn = nullptr;
    QPushButton *m_printBtn = nullptr;
    QProgressBar *m_progressBar = nullptr;

    void setupUi();
    void updateBriefingDisplay();
    QString generateBriefingHtml() const;
    QString generateBriefingPlainText() const;
    static QString escapeHtml(const QString& str);
};

} // namespace openairac

#endif // OPENAIRAC_FLIGHTBRIEFINGDIALOG_H
