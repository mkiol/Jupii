/* Copyright (C) 2017 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <QDBusConnection>
#include <QDebug>

#include "dbusapp.h"

#include "app_adaptor.h"
#include "app_interface.h"

DbusApp::DbusApp(QObject *parent) :
    QObject(parent)
{
    new PlayerAdaptor(this);

    QDBusConnection con = QDBusConnection::sessionBus();

    bool ret = con.registerService("org.jupii");
    Q_ASSERT(ret);
    ret = con.registerObject("/", this);
    Q_ASSERT(ret);
    Q_UNUSED(ret);
}

bool DbusApp::canControl()
{
    qDebug() << "canControl, value:" << m_canControl;

    return m_canControl;
}

void DbusApp::setCanControl(bool value)
{
    qDebug() << "setCanControl, value:" << value;

    if (m_canControl != value) {
        m_canControl = value;
        emit canControlChanged();
        emit CanControlPropertyChanged(m_canControl);
    }
}

void DbusApp::appendPath(const QString& path)
{
    qDebug() << "appendPath, path:" << path;

    emit requestAppendPath(path);
}

void DbusApp::clearPlaylist()
{
    emit requestClearPlaylist();
}
