/* Copyright (C) 2017 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDebug>
#include <QFile>

#include "dbusapp.h"
#include "dbus_jupii_adaptor.h"

DbusProxy::DbusProxy(QObject *parent) :
    QObject(parent),
    TaskExecutor(parent, 2)
{
    new PlayerAdaptor(this);

    QDBusConnection con = QDBusConnection::sessionBus();

    if (!con.registerService("org.jupii")) {
        qWarning() << "D-bus service registration failed";
        return;
    }

    if (!con.registerObject("/", this)) {
        qWarning() << "D-bus object registration failed";
        return;
    }
}

bool DbusProxy::canControl()
{
    qDebug() << "canControl, value:" << m_canControl;

    return m_canControl;
}

void DbusProxy::setCanControl(bool value)
{
    qDebug() << "setCanControl, value:" << value;

    if (m_canControl != value) {
        m_canControl = value;
        emit canControlChanged();
        emit CanControlPropertyChanged(m_canControl);
    }
}

void DbusProxy::appendPath(const QString& path)
{
    qDebug() << "appendPath, path:" << path;

    emit requestAppendPath(path);
}

void DbusProxy::playPath(const QString& path)
{
    qDebug() << "playPath, path:" << path;

    emit requestPlayPath(path);
}

void DbusProxy::clearPlaylist()
{
    emit requestClearPlaylist();
}
