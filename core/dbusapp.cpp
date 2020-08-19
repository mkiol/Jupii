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
#include "playlistmodel.h"
#include "avtransport.h"
#include "services.h"
#include "utils.h"

DbusProxy::DbusProxy(QObject *parent) :
    QObject(parent)
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

    auto av = Services::instance()->avTransport;
    if (av) {
        connect(av.get(), &Service::initedChanged, [this]{
            auto av = Services::instance()->avTransport;
            if (av)
                setCanControl(av->getInited());
        });
    } else {
        qWarning() << "AVTransport doesn't exist so cannot connect to dbus proxy";
    }
}

DbusProxy::~DbusProxy()
{
    setCanControl(false);
}

bool DbusProxy::canControl()
{
    qDebug() << "Dbus canControl, value:" << m_canControl;

    return m_canControl;
}

void DbusProxy::setCanControl(bool value)
{
    if (m_canControl != value) {
        m_canControl = value;
        emit canControlChanged();
        emit CanControlPropertyChanged(m_canControl);
    }
}

void DbusProxy::appendPath(const QString& path)
{
    qDebug() << "Dbus appendPath, path:" << path;

    auto pl = PlaylistModel::instance();
    pl->addItemPath(path);
}

void DbusProxy::addPath(const QString& path, const QString& name)
{
    qDebug() << "Dbus addPath, path:" << path << name;

    auto pl = PlaylistModel::instance();
    pl->addItemPath(path, name);
}

void DbusProxy::addUrl(const QString& url, const QString& name)
{
    qDebug() << "Dbus addUrl, url:" << url << name;

    auto pl = PlaylistModel::instance();
    pl->addItemUrl(QUrl(url), name);
}

void DbusProxy::addPathOnce(const QString& path, const QString& name)
{
    qDebug() << "Dbus addPathOnce, path:" << path << name;

    auto pl = PlaylistModel::instance();
    if (pl->pathExists(path)) {
        qDebug() << "Path already exists";
    } else {
        pl->addItemPath(path, name);
    }
}

void DbusProxy::addPathOnceAndPlay(const QString& path, const QString& name)
{
    qDebug() << "Dbus addPathOnceAndPlay, path:" << path << name;

    auto pl = PlaylistModel::instance();
    if (pl->playPath(path)) {
        qDebug() << "Path already exists";
    } else {
        pl->addItemPath(path, name, true);
    }

#ifdef SAILFISH
    // bringing app to foreground
    auto utils = Utils::instance();
    utils->activateWindow();
#endif
}

void DbusProxy::addUrlOnce(const QString& url, const QString& name)
{
    qDebug() << "Dbus addUrlOnce, url:" << url << name;

    QUrl u(url);

    auto pl = PlaylistModel::instance();
    if (pl->urlExists(u)) {
        qDebug() << "Url already exists";
    } else {
        pl->addItemUrl(u, name);
    }
}

void DbusProxy::addUrlOnceAndPlay(const QString& url, const QString& name)
{
    qDebug() << "Dbus addUrlOnceAndPlay, url:" << url << name;

    QUrl u(url);

    auto pl = PlaylistModel::instance();
    if (pl->playUrl(u)) {
        qDebug() << "Url already exists";
    } else {
        pl->addItemUrl(u, name, {}, {}, {}, true);
    }

#ifdef SAILFISH
    // bringing app to foreground
    auto utils = Utils::instance();
    utils->activateWindow();
#endif
}

void DbusProxy::add(const QString &url,
                    const QString &name,
                    const QString &author,
                    const QString &description,
                    uint type,
                    const QString &app,
                    const QString &icon,
                    bool once,
                    bool play) {
    Q_UNUSED(type)
    Q_UNUSED(app)
    qDebug() << "Dbus add, url:" << url << name;

    auto pl = PlaylistModel::instance();

    QUrl u(url);

    if (once && pl->urlExists(u)) {
        if (play) {
            qDebug() << "Url already exists, so only playing it";
            pl->playUrl(u);
        } else {
            qDebug() << "Url already exists";
        }
    } else {
        pl->addItemUrl(u, name, author, icon, description, play);
    }

#ifdef SAILFISH
    if (play) {
        // bringing app to foreground
        auto utils = Utils::instance();
        utils->activateWindow();
    }
#endif
}

void DbusProxy::clearPlaylist()
{
    qDebug() << "Dbus clearPlaylist";

    auto pl = PlaylistModel::instance();
    pl->clear();
}
