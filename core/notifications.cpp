/* Copyright (C) 2020 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusPendingReply>
#include <QVariantMap>

#include "notifications.h"
#include "info.h"

Notifications* Notifications::m_instance = nullptr;

Notifications::Notifications(QObject *parent) : QObject(parent)
{
    createDbusInf();
}

Notifications* Notifications::instance(QObject *parent)
{
    if (Notifications::m_instance == nullptr) {
        Notifications::m_instance = new Notifications(parent);
    }

    return Notifications::m_instance;
}

bool Notifications::createDbusInf()
{
    if (!m_dbus_inf) {
        QDBusConnection bus = QDBusConnection::sessionBus();

        m_dbus_inf = std::make_unique<OrgFreedesktopNotificationsInterface>(
                    "org.freedesktop.Notifications",
                    "/org/freedesktop/Notifications",
                    bus);

        if (!m_dbus_inf->isValid()) {
            qWarning() << "Dbus interface cannot be created";
            m_dbus_inf.reset();
            return false;
        }
    }

    return true;
}

bool Notifications::show(const QString& body, const QString& summary, const QString& iconPath)
{
    if (!m_dbus_inf || !m_dbus_inf->isValid())
        return false;

    QVariantMap hints;

    if (!iconPath.isEmpty()) {
        hints.insert("image-path", iconPath);
    }

    auto reply = m_dbus_inf->Notify(
                Jupii::APP_NAME,
                0,
                Jupii::APP_ID,
                summary,
                body,
                {},
                hints,
                -1
                );

    return !reply.isError();
}

