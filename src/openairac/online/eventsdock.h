/*****************************************************************************
* OpenAIRAC Map — Online Events Dock Panel
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

#ifndef OPENAIRAC_EVENTSDOCK_H
#define OPENAIRAC_EVENTSDOCK_H

#include "openairac/online/onlinenetworkmodel.h"
#include <QDockWidget>
#include <QLineEdit>
#include <QTreeWidget>
#include <QPushButton>
#include <QTextBrowser>
#include <QLabel>

namespace openairac {

class EventsDock : public QDockWidget {
    Q_OBJECT

public:
    explicit EventsDock(QWidget *parent = nullptr);
    virtual ~EventsDock() override = default;

public slots:
    void onEventsReady(const QList<openairac::OnlineEventItem>& events);
    void onRefreshClicked();
    void onFilterChanged(const QString& text);
    void onEventSelected(QTreeWidgetItem *current, QTreeWidgetItem *previous);

private:
    QLineEdit *m_filterEdit = nullptr;
    QPushButton *m_refreshButton = nullptr;
    QTreeWidget *m_eventsTree = nullptr;
    QTextBrowser *m_detailsBrowser = nullptr;
    QLabel *m_statusLabel = nullptr;

    QList<OnlineEventItem> m_allEvents;

    void updateTreeDisplay();
};

} // namespace openairac

#endif // OPENAIRAC_EVENTSDOCK_H
