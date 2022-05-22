/* Copyright (C) 2017-2021 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "dbusapp.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDebug>
#include <QFile>

#include "playlistmodel.h"
#include "avtransport.h"
#include "services.h"
#include "utils.h"
#include "info.h"

DbusProxy::DbusProxy(QObject *parent) : QObject(parent),
    player{this},
    playlist{this}
{
    connect(PlaylistModel::instance(), &PlaylistModel::itemsLoaded, this, [this](){ setEnabled(true); }, Qt::QueuedConnection);

    auto con = QDBusConnection::sessionBus();

    if (!con.registerObject("/", this)) {
        qWarning() << "Dbus object registration failed:" << con.lastError().message();
        if (con.lastError().type() == QDBusError::AddressInUse) throw std::runtime_error("dbus address in use");
        return;
    }
    if (!con.registerService(Jupii::DBUS_SERVICE)) {
        qWarning() << "Dbus service registration failed:" << con.lastError().message();
        if (con.lastError().type() == QDBusError::AddressInUse) throw std::runtime_error("dbus address in use");
        return;
    }

    auto av = Services::instance()->avTransport;
    if (av) {
        connect(av.get(), &Service::initedChanged, this,
                &DbusProxy::updateCanControl, Qt::QueuedConnection);
        connect(av.get(), &AVTransport::transportStateChanged, this,
                &DbusProxy::updatePlaying, Qt::QueuedConnection);
    } else {
        qWarning() << "AVTransport doesn't exist so cannot connect to dbus proxy";
    }

    qDebug() << "Dbus service successfully registered";
}

DbusProxy::~DbusProxy()
{
    setCanControl(false);
    setPlaying(false);
}

void DbusProxy::updatePlaying()
{
    auto av = Services::instance()->avTransport;
    if (av)
        setPlaying(av->getTransportState() == AVTransport::Playing);
}

void DbusProxy::updateCanControl()
{
    auto av = Services::instance()->avTransport;
    if (av) {
        setCanControl(av->getInited());
        emit deviceNameChanged();
        emit DeviceNamePropertyChanged(av->getDeviceFriendlyName());
    }
}

QString DbusProxy::deviceName()
{
    auto av = Services::instance()->avTransport;
    if (av)
        av->getDeviceFriendlyName();
    return {};
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

bool DbusProxy::playing()
{
    qDebug() << "Dbus playing, value:" << m_playing;
    return m_playing;
}

void DbusProxy::setPlaying(bool value)
{
    if (m_playing != value) {
        m_playing = value;
        emit playingChanged();
        emit PlayingPropertyChanged(m_playing);
    }
}

void DbusProxy::appendPath(const QString& path)
{
    qDebug() << "Dbus appendPath, path:" << path;

    if (!enabled()) {
        qWarning() << "Dbus not yet enabled";
        return;
    }

    auto pl = PlaylistModel::instance();
    pl->addItemPath(path);
}

void DbusProxy::addPath(const QString& path, const QString& name)
{
    qDebug() << "Dbus addPath, path:" << path << name;

    if (!enabled()) {
        qWarning() << "Dbus not yet enabled";
        return;
    }

    auto pl = PlaylistModel::instance();
    pl->addItemPath(path, name);
}

void DbusProxy::addUrl(const QString& url, const QString& name)
{
    qDebug() << "Dbus addUrl, url:" << url << name;

    if (!enabled()) {
        qWarning() << "Dbus not yet enabled";
        return;
    }

    auto pl = PlaylistModel::instance();
    pl->addItemUrl(QUrl(url), name);
}

void DbusProxy::openUrl(const QStringList& arguments)
{
    qDebug() << "Dbus openUrl, arguments:" << arguments;

    m_pendingOpenUrlArguments << arguments;
    if (enabled()) {
        openPendingUrl();
    } else {
        emit focusRequested();
    }
}

void DbusProxy::openPendingUrl()
{
    if (!m_pendingOpenUrlArguments.isEmpty()) {
        qDebug() << "Dbus openPendingUrl, arguments:" << m_pendingOpenUrlArguments;
        foreach (const auto url, m_pendingOpenUrlArguments) addUrl(url, {});
        m_pendingOpenUrlArguments.clear();
        focus();
    }
}

void DbusProxy::addPathOnce(const QString& path, const QString& name)
{
    qDebug() << "Dbus addPathOnce, path:" << path << name;

    if (!enabled()) {
        qWarning() << "D-Bus not yet enabled";
        return;
    }

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

    if (!enabled()) {
        qWarning() << "Dbus not yet enabled";
        return;
    }

    auto pl = PlaylistModel::instance();
    if (pl->playPath(path)) {
        qDebug() << "Path already exists";
    } else {
        pl->addItemPath(path, name, true);
    }

    focus();
}

void DbusProxy::addUrlOnce(const QString& url, const QString& name)
{
    qDebug() << "Dbus addUrlOnce, url:" << url << name;

    if (!enabled()) {
        qWarning() << "Dbus not yet enabled";
        return;
    }

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

    if (!enabled()) {
        qWarning() << "Dbus not yet enabled";
        return;
    }

    QUrl u(url);

    auto pl = PlaylistModel::instance();
    if (pl->playUrl(u)) {
        qDebug() << "Url already exists";
    } else {
        pl->addItemUrl(u, name, {}, {}, {}, {}, {}, true);
    }

    focus();
}

void DbusProxy::add(const QString &url,
                    const QString &origUrl,
                    const QString &name,
                    const QString &author,
                    const QString &description,
                    int type,
                    const QString &app,
                    const QString &icon,
                    bool once,
                    bool play) {
    Q_UNUSED(type)
    qDebug() << "Dbus add, url:" << url << name;

    if (!enabled()) {
        qWarning() << "Dbus not yet enabled";
        return;
    }

    auto pl = PlaylistModel::instance();

    QUrl u(url);
    QUrl ou(origUrl);

    if (once && (pl->urlExists(u) || (!origUrl.isEmpty() && pl->urlExists(ou)))) {
        if (play) {
            qDebug() << "Url already exists, so only playing it";
            pl->playUrl(u);
        } else {
            qDebug() << "Url already exists";
        }
    } else {
        pl->addItemUrl(u, name, ou, author, QUrl(icon), description, app, play);
    }
}

void DbusProxy::pause()
{
    if (m_canControl) {
        auto pl = PlaylistModel::instance();
        pl->pause();
    }
}

void DbusProxy::play()
{
    if (m_canControl) {
        auto pl = PlaylistModel::instance();
        pl->play();
    }
}

void DbusProxy::togglePlay()
{
    if (m_canControl) {
        auto pl = PlaylistModel::instance();
        pl->togglePlay();
    }
}

void DbusProxy::focus()
{
#ifdef SAILFISH
    // bringing app to foreground
    emit focusRequested();
    auto utils = Utils::instance();
    utils->activateWindow();
#endif
}

void DbusProxy::clearPlaylist()
{
    qDebug() << "Dbus clearPlaylist";

    if (!enabled()) {
        qWarning() << "Dbus not yet enabled";
        return;
    }

    auto pl = PlaylistModel::instance();
    pl->clear();
}

void DbusProxy::setEnabled(bool value)
{
    if (m_enabled != value) {
        m_enabled = value;
        emit enabledChanged();
        if (m_enabled) openPendingUrl();
    }
}