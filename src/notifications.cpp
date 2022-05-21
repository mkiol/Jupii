/* Copyright (C) 2020-2022 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "notifications.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusPendingReply>
#include <QVariantMap>

#include "info.h"

Notifications::Notifications(QObject* parent) : QObject{parent} {
    createDbusInf();
}

bool Notifications::createDbusInf() {
    if (!m_dbus_inf) {
        m_dbus_inf = std::make_unique<OrgFreedesktopNotificationsInterface>(
            "org.freedesktop.Notifications", "/org/freedesktop/Notifications",
            QDBusConnection::sessionBus());
    }

    if (!m_dbus_inf->isValid()) {
        qWarning() << "Dbus interface cannot be created";
        m_dbus_inf.reset();
        return false;
    }

#ifdef QT_DEBUG
    {
        auto reply = m_dbus_inf->GetCapabilities();
        reply.waitForFinished();
        if (reply.isError()) {
            qWarning() << "Cannot get notification server caps";
        } else {
            for (const auto& cap : reply.value()) {
                qDebug() << "cap:" << cap;
            }
        }
    }

    {
        auto reply = m_dbus_inf->GetServerInformation();
        reply.waitForFinished();
        if (reply.isError()) {
            qWarning() << "Cannot get notification server info";
        } else {
            auto name = reply.argumentAt<0>();
            auto vendor = reply.argumentAt<1>();
            auto version = reply.argumentAt<2>();
            auto spec = reply.argumentAt<3>();
            qDebug() << "Notification server info:" << name << vendor << version
                     << spec;
        }
    }
#endif

    return true;
}

bool Notifications::show(const QString& summary, const QString& body,
                         const QString& iconPath) {
    if (!m_dbus_inf || !m_dbus_inf->isValid()) return false;

    QVariantMap hints;
    int timeout = -1;

#ifdef SAILFISH
    timeout = 5;
    hints.insert("urgency", 1);
    hints.insert("x-nemo-owner", Jupii::APP_BINARY_ID);
    hints.insert("x-nemo-max-content-lines", 10);
    if (!body.isEmpty()) hints.insert("x-nemo-preview-body", body);
    if (!summary.isEmpty()) hints.insert("x-nemo-preview-summary", summary);
    if (!iconPath.isEmpty()) hints.insert("x-nemo-icon", iconPath);
#endif

    auto reply =
        m_dbus_inf->Notify(Jupii::APP_NAME, 0,
                           iconPath.isEmpty() ? Jupii::APP_BINARY_ID : iconPath,
                           summary, body, {}, hints, timeout);

    return !reply.isError();
}
