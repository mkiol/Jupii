/* Copyright (C) 2017-2022 Michal Kosciesza <michal@mkiol.net>
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

#include "avtransport.h"
#include "contentserver.h"
#include "info.h"
#include "playlistmodel.h"
#include "services.h"
#include "utils.h"

DbusProxy::DbusProxy(QObject* parent)
    : QObject{parent}, player{this}, playlist{this} {
    connect(
        PlaylistModel::instance(), &PlaylistModel::itemsLoaded, this,
        [this] { setEnabled(true); }, Qt::QueuedConnection);

    auto con = QDBusConnection::sessionBus();

    if (!con.registerObject(QStringLiteral("/"), this)) {
        qWarning() << "dbus object registration failed:"
                   << con.lastError().message();
        if (con.lastError().type() == QDBusError::AddressInUse)
            throw std::runtime_error("dbus address in use");
        return;
    }
    if (!con.registerService(Jupii::DBUS_SERVICE)) {
        qWarning() << "dbus service registration failed:"
                   << con.lastError().message();
        if (con.lastError().type() == QDBusError::AddressInUse)
            throw std::runtime_error("dbus address in use");
        return;
    }

    auto av = Services::instance()->avTransport;
    if (av) {
        connect(av.get(), &Service::initedChanged, this,
                &DbusProxy::updateCanControl, Qt::QueuedConnection);
        connect(av.get(), &AVTransport::transportStateChanged, this,
                &DbusProxy::updatePlaying, Qt::QueuedConnection);
    } else {
        qWarning() << "avt doesn't exist so cannot connect to dbus proxy";
    }

    qDebug() << "dbus service successfully registered";
}

DbusProxy::~DbusProxy() {
    setCanControl(false);
    setPlaying(false);
}

void DbusProxy::updatePlaying() {
    auto av = Services::instance()->avTransport;
    if (av) setPlaying(av->getTransportState() == AVTransport::Playing);
}

void DbusProxy::updateCanControl() {
    auto av = Services::instance()->avTransport;
    if (av) {
        setCanControl(av->getInited());
        emit deviceNameChanged();
        emit DeviceNamePropertyChanged(av->getDeviceFriendlyName());
    }
}

QString DbusProxy::deviceName() {
    auto av = Services::instance()->avTransport;
    if (av) av->getDeviceFriendlyName();
    return {};
}

bool DbusProxy::canControl() { return m_canControl; }

void DbusProxy::setCanControl(bool value) {
    if (m_canControl != value) {
        m_canControl = value;
        emit canControlChanged();
        emit CanControlPropertyChanged(m_canControl);
    }
}

bool DbusProxy::playing() { return m_playing; }

void DbusProxy::setPlaying(bool value) {
    if (m_playing != value) {
        m_playing = value;
        emit playingChanged();
        emit PlayingPropertyChanged(m_playing);
    }
}

void DbusProxy::appendPath(const QString& path) {
    if (!enabled()) {
        qWarning() << "dbus not yet enabled";
        return;
    }
    PlaylistModel::instance()->addItemPath(path,
                                           ContentServer::Type::Type_Unknown);
}

void DbusProxy::addPath(const QString& path, const QString& name) {
    if (!enabled()) {
        qWarning() << "dbus not yet enabled";
        return;
    }
    PlaylistModel::instance()->addItemPath(
        path, ContentServer::Type::Type_Unknown, name);
}

void DbusProxy::addUrl(const QString& url, const QString& name) {
    if (!enabled()) {
        qWarning() << "dbus not yet enabled";
        return;
    }
    PlaylistModel::instance()->addItemUrl(
        QUrl{url}, ContentServer::Type::Type_Unknown, name);
}

void DbusProxy::openUrl(const QStringList& arguments) {
    m_pendingOpenUrlArguments << arguments;
    if (enabled()) {
        openPendingUrl();
    } else {
        emit focusRequested();
    }
}

void DbusProxy::openPendingUrl() {
    if (!m_pendingOpenUrlArguments.isEmpty()) {
        qDebug() << "dbus pending open url arguments:"
                 << m_pendingOpenUrlArguments;
        foreach (const auto url, m_pendingOpenUrlArguments)
            addUrl(url, {});
        m_pendingOpenUrlArguments.clear();
        focus();
    }
}

void DbusProxy::addPathOnce(const QString& path, const QString& name) {
    if (!enabled()) {
        qWarning() << "dbus not yet enabled";
        return;
    }

    auto pl = PlaylistModel::instance();
    if (pl->pathExists(path)) {
        qDebug() << "path already exists";
    } else {
        pl->addItemPath(path, ContentServer::Type::Type_Unknown, name);
    }
}

void DbusProxy::addPathOnceAndPlay(const QString& path, const QString& name) {
    if (!enabled()) {
        qWarning() << "dbus not yet enabled";
        return;
    }

    auto pl = PlaylistModel::instance();
    if (pl->playPath(path)) {
        qDebug() << "path already exists";
    } else {
        pl->addItemPath(path, ContentServer::Type::Type_Unknown, name, true);
    }

    focus();
}

void DbusProxy::addUrlOnce(const QString& url, const QString& name) {
    if (!enabled()) {
        qWarning() << "dbus not yet enabled";
        return;
    }

    QUrl u{url};

    auto pl = PlaylistModel::instance();
    if (pl->urlExists(u)) {
        qDebug() << "url already exists";
    } else {
        pl->addItemUrl(u, ContentServer::Type::Type_Unknown, name);
    }
}

void DbusProxy::addUrlOnceAndPlay(const QString& url, const QString& name) {
    if (!enabled()) {
        qWarning() << "dbus not yet enabled";
        return;
    }

    QUrl u{url};

    auto pl = PlaylistModel::instance();
    if (pl->playUrl(u)) {
        qDebug() << "url already exists";
    } else {
        pl->addItemUrl(u, ContentServer::Type::Type_Unknown, name, {}, {}, {},
                       {}, {}, true);
    }

    focus();
}

void DbusProxy::add(const QString& url, const QString& origUrl,
                    const QString& name, const QString& author,
                    const QString& description, int type, const QString& app,
                    const QString& icon, bool once, bool play) {
    if (!enabled()) {
        qWarning() << "dbus not yet enabled";
        return;
    }

    auto pl = PlaylistModel::instance();

    QUrl u{url};
    QUrl ou{origUrl};

    if (once &&
        (pl->urlExists(u) || (!origUrl.isEmpty() && pl->urlExists(ou)))) {
        if (play) {
            qDebug() << "url already exists, so only playing it";
            pl->playUrl(u);
        } else {
            qDebug() << "url already exists";
        }
    } else {
        pl->addItemUrl(u, static_cast<ContentServer::Type>(type), name, ou,
                       author, QUrl{icon}, description, app, play);
    }
}

void DbusProxy::pause() {
    if (m_canControl) {
        PlaylistModel::instance()->pause();
    }
}

void DbusProxy::play() {
    if (m_canControl) {
        PlaylistModel::instance()->play();
    }
}

void DbusProxy::togglePlay() {
    if (m_canControl) {
        PlaylistModel::instance()->togglePlay();
    }
}

void DbusProxy::focus() {
#ifdef SAILFISH
    // bringing app to foreground
    emit focusRequested();
    auto utils = Utils::instance();
    utils->activateWindow();
#endif
}

void DbusProxy::clearPlaylist() {
    if (!enabled()) {
        qWarning() << "dbus not yet enabled";
        return;
    }
    PlaylistModel::instance()->clear();
}

void DbusProxy::setEnabled(bool value) {
    if (m_enabled != value) {
        m_enabled = value;
        emit enabledChanged();
        if (m_enabled) openPendingUrl();
    }
}
