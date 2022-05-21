/* Copyright (C) 2020-2022 Michal Kosciesza <michal@mkiol.net>
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
#include "singleton.h"

class Notifications : public QObject, public Singleton<Notifications> {
    Q_OBJECT
   public:
    Notifications(QObject* parent = nullptr);
    Q_INVOKABLE bool show(const QString& summary, const QString& body = {},
                          const QString& iconPath = {});

   private:
    std::unique_ptr<OrgFreedesktopNotificationsInterface> m_dbus_inf;
    bool createDbusInf();
};

#endif  // NOTIFICATIONS_H
