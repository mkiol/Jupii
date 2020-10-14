/* Copyright (C) 2020 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef NOTIFICATIONS_H
#define NOTIFICATIONS_H

#include <QObject>
#include <QString>
#include <memory>

#include "dbus_notifications_inf.h"

class Notifications : public QObject
{
    Q_OBJECT
public:
    static Notifications* instance(QObject *parent = nullptr);

    Q_INVOKABLE bool show(const QString& body, const QString& summary = QString(), const QString &iconPath = QString());

private:
    static Notifications* m_instance;

    std::unique_ptr<OrgFreedesktopNotificationsInterface> m_dbus_inf;

    explicit Notifications(QObject *parent = nullptr);
    bool createDbusInf();
};

#endif // NOTIFICATIONS_H
