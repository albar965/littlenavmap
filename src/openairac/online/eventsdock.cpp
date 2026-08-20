/*****************************************************************************
* OpenAIRAC Map — Online Events Dock Panel Implementation
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

#include "openairac/online/eventsdock.h"
#include "openairac/online/onlineclient.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QDesktopServices>
#include <QUrl>

namespace openairac {

EventsDock::EventsDock(QWidget *parent)
    : QDockWidget(tr("Online Events [VATSIM]"), parent) {
    setObjectName(QStringLiteral("OpenAIRACEventsDock"));

    QWidget *container = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(container);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);

    // Top control bar
    QHBoxLayout *topBar = new QHBoxLayout();
    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setPlaceholderText(tr("Filter by event name, airport ICAO..."));
    m_filterEdit->setClearButtonEnabled(true);
    connect(m_filterEdit, &QLineEdit::textChanged, this, &EventsDock::onFilterChanged);

    m_refreshButton = new QPushButton(tr("Refresh"), this);
    connect(m_refreshButton, &QPushButton::clicked, this, &EventsDock::onRefreshClicked);

    topBar->addWidget(m_filterEdit);
    topBar->addWidget(m_refreshButton);
    mainLayout->addLayout(topBar);

    // Events Tree
    m_eventsTree = new QTreeWidget(this);
    m_eventsTree->setColumnCount(4);
    m_eventsTree->setHeaderLabels({tr("Event Name"), tr("Type"), tr("Start (UTC)"), tr("Airports")});
    m_eventsTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_eventsTree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_eventsTree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_eventsTree->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_eventsTree->setAlternatingRowColors(true);
    m_eventsTree->setRootIsDecorated(false);
    connect(m_eventsTree, &QTreeWidget::currentItemChanged, this, &EventsDock::onEventSelected);
    mainLayout->addWidget(m_eventsTree, 2);

    // Details browser
    m_detailsBrowser = new QTextBrowser(this);
    m_detailsBrowser->setOpenExternalLinks(true);
    m_detailsBrowser->setPlaceholderText(tr("Select an event to view full description and routes."));
    mainLayout->addWidget(m_detailsBrowser, 1);

    // Status label
    m_statusLabel = new QLabel(tr("Status: Live Events API"), this);
    m_statusLabel->setStyleSheet(QStringLiteral("color: #666; font-size: 11px;"));
    mainLayout->addWidget(m_statusLabel);

    setWidget(container);

    // Connect to OnlineClient
    connect(&OnlineClient::instance(), &OnlineClient::eventsReady, this, &EventsDock::onEventsReady);
    onEventsReady(OnlineClient::instance().latestEvents());
}

void EventsDock::onEventsReady(const QList<openairac::OnlineEventItem>& events) {
    m_allEvents = events;
    updateTreeDisplay();
    m_statusLabel->setText(tr("Events loaded: %1 total events").arg(events.size()));
}

void EventsDock::onRefreshClicked() {
    OnlineClient::instance().requestEvents(true);
}

void EventsDock::onFilterChanged(const QString& text) {
    Q_UNUSED(text);
    updateTreeDisplay();
}

void EventsDock::updateTreeDisplay() {
    m_eventsTree->clear();
    QString query = m_filterEdit->text().trimmed().toUpper();

    for (const OnlineEventItem& ev : m_allEvents) {
        if (!query.isEmpty()) {
            bool matchesName = ev.name.toUpper().contains(query);
            bool matchesApt = ev.matchesAirport(query);
            if (!matchesName && !matchesApt) {
                continue;
            }
        }

        QTreeWidgetItem *item = new QTreeWidgetItem(m_eventsTree);
        item->setText(0, ev.name);
        item->setText(1, ev.eventType.isEmpty() ? tr("Event") : ev.eventType);
        item->setText(2, ev.startTime.isValid() ? ev.startTime.toString(QStringLiteral("yyyy-MM-dd hh:mmZ")) : tr("---"));
        item->setText(3, ev.airports.join(QStringLiteral(", ")));
        item->setData(0, Qt::UserRole, QVariant::fromValue(ev.id));
    }
}

void EventsDock::onEventSelected(QTreeWidgetItem *current, QTreeWidgetItem *previous) {
    Q_UNUSED(previous);
    if (!current) {
        m_detailsBrowser->clear();
        return;
    }

    quint64 id = current->data(0, Qt::UserRole).toULongLong();
    for (const OnlineEventItem& ev : m_allEvents) {
        if (ev.id == id) {
            QString html;
            html += QStringLiteral("<h3>") + ev.name.toHtmlEscaped() + QStringLiteral("</h3>");
            if (!ev.eventType.isEmpty()) {
                html += QStringLiteral("<p><b>Type:</b> ") + ev.eventType.toHtmlEscaped() + QStringLiteral("</p>");
            }
            html += QStringLiteral("<p><b>Window:</b> ") +
                    (ev.startTime.isValid() ? ev.startTime.toString(QStringLiteral("yyyy-MM-dd hh:mmZ")) : QStringLiteral("---")) +
                    QStringLiteral(" &mdash; ") +
                    (ev.endTime.isValid() ? ev.endTime.toString(QStringLiteral("yyyy-MM-dd hh:mmZ")) : QStringLiteral("---")) +
                    QStringLiteral("</p>");

            if (!ev.airports.isEmpty()) {
                html += QStringLiteral("<p><b>Airports:</b> ") + ev.airports.join(QStringLiteral(", ")).toHtmlEscaped() + QStringLiteral("</p>");
            }
            if (!ev.routes.isEmpty()) {
                html += QStringLiteral("<p><b>Routes:</b><br>") + ev.routes.join(QStringLiteral("<br>")).toHtmlEscaped() + QStringLiteral("</p>");
            }
            if (!ev.description.isEmpty()) {
                html += QStringLiteral("<p>") + ev.description.toHtmlEscaped() + QStringLiteral("</p>");
            }
            if (!ev.link.isEmpty()) {
                html += QStringLiteral("<p><a href=\"") + ev.link.toHtmlEscaped() + QStringLiteral("\">Open Official Event Page</a></p>");
            }

            m_detailsBrowser->setHtml(html);
            return;
        }
    }
}

} // namespace openairac
