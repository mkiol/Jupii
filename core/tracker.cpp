/* Copyright (C) 2018 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <QDebug>
#include <QDBusError>
#include <QDBusUnixFileDescriptor>
#include <QCryptographicHash>
#include <QStandardPaths>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QFileInfo>
#include <QEventLoop>
#include <QTimer>

#include "tracker.h"
#include "trackercursor.h"
#include "settings.h"
#include "utils.h"

#include <fcntl.h>
#include <unistd.h>

Tracker* Tracker::m_instance = nullptr;

struct Query
{
    const QString &m_query;
    QDBusUnixFileDescriptor &m_qfd;

    Query(const QString &query, QDBusUnixFileDescriptor &qfd) : m_query{query}, m_qfd{qfd} {}

    auto operator()(OrgFreedesktopTracker1SteroidsInterface &dbus_inf) const {
        return dbus_inf.Query(m_query, m_qfd);
    }

    auto operator()(OrgFreedesktopTracker3EndpointInterface &dbus_inf) const {
        return dbus_inf.Query(m_query, m_qfd, QVariantMap{});
    }

    auto operator()(std::monostate) {
        return QDBusPendingReply<QStringList>{};
    }
};

Tracker::Tracker(QObject *parent) : QObject{parent},
    m_cacheDir{QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation)}
{
   if (!createTrackerInf()) {
       qWarning() << "Cannot create Tracker Dbus interface";
   }
}

Tracker* Tracker::instance(QObject *parent)
{
    if (Tracker::m_instance == nullptr) {
        Tracker::m_instance = new Tracker(parent);
    }

    return Tracker::m_instance;
}

bool Tracker::createUnixPipe(int &readFd, int &writeFd)
{
    int fds[2];

    if (::pipe(fds) < 0) {
        qWarning() << "Cannot create Unix pipe";
        return false;
    }

    readFd = fds[0];
    writeFd = fds[1];

    return true;
}

bool Tracker::query(const QString &query, bool emitSignal)
{
    if (!valid()) {
        qWarning() << "Tracker dbus interface is invalid";
        emit queryError();
        return false;
    }

    m_dbusReplyData.clear();
    m_pipeData.clear();

    int readFd, writeFd;

    if (!createUnixPipe(readFd, writeFd)) {
        qWarning() << "Cannot create Unix pipe";
        emit queryError();
        return false;
    }

    QDBusUnixFileDescriptor qfd(writeFd);
    close(writeFd);

    auto reply = std::visit(Query{query, qfd}, m_dbus_inf);

    reply.waitForFinished();

    close(qfd.fileDescriptor());

    if (reply.isError() || !reply.isValid()) {
        auto error = reply.error();
        qWarning() << "Dbus query failed:" << error.name() << error.message();
        close(readFd);
        emit queryError();
        return false;
    }

    m_dbusReplyData << reply.value();

    const int pipe_buf_len = 1024;
    char pipe_buf[pipe_buf_len];

    ssize_t bytes_read;

    while ((bytes_read = read(readFd, &pipe_buf, pipe_buf_len)) > 0) {
        m_pipeData.append(pipe_buf, int(bytes_read));
    }

    close(readFd);

    if (m_pipeData.isEmpty()) {
        qWarning() << "No data received from pipe";
    }

    if (emitSignal) {
        emit queryFinished(m_dbusReplyData, m_pipeData);
    }

    return true;
}

QString Tracker::genTrackerId(const QString& name)
{    
    // Strip invalid entities and normalize as defined in
    // https://wiki.gnome.org/action/show/DraftSpecs/MediaArtStorageSpec

    auto striped = Utils::escapeName(name);
    return QString{QCryptographicHash::hash(striped.isEmpty() ? " " : striped.toUtf8(),
                                            QCryptographicHash::Md5).toHex()};
}

QString Tracker::genAlbumArtFile(const QString &albumName,
                                 const QString &artistName)
{
    const auto filepath = QString{"%1/media-art/album-%2-%3.jpeg"}
            .arg(QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation),
                 genTrackerId(artistName), genTrackerId(albumName));
    if (!QFileInfo::exists(filepath))
        return {};
    return filepath;
}

bool Tracker::pingTracker1() const
{
    OrgFreedesktopDBusPeerInterface dbus_inf{
        "org.freedesktop.Tracker1", "/org/freedesktop/Tracker1/Steroids",
        QDBusConnection::sessionBus()};

    auto resp = dbus_inf.Ping();
    resp.waitForFinished();
    if (resp.isError()) {
        qWarning() << "Tracker1 ping error:" << resp.error().name() << resp.error().message();
        return false;
    }

    return true;
}

bool Tracker::pingTracker3() const
{
    OrgFreedesktopDBusPeerInterface dbus_inf{
        "org.freedesktop.Tracker3.Miner.Files", "/org/freedesktop/Tracker3/Endpoint",
        QDBusConnection::sessionBus()};

    auto resp = dbus_inf.Ping();
    resp.waitForFinished();
    if (resp.isError()) {
        qWarning() << "Tracker3 ping error:" << resp.error().name() << resp.error().message();
        return false;
    }

    return true;
}

bool Tracker::createTrackerInf()
{
    if (pingTracker3()) {
        m_dbus_inf.emplace<OrgFreedesktopTracker3EndpointInterface>(
            "org.freedesktop.Tracker3.Miner.Files", "/org/freedesktop/Tracker3/Endpoint",
            QDBusConnection::sessionBus());
    } else if (pingTracker1()) {
        m_dbus_inf.emplace<OrgFreedesktopTracker1SteroidsInterface>(
            "org.freedesktop.Tracker1", "/org/freedesktop/Tracker1/Steroids",
            QDBusConnection::sessionBus());
    } else {
        qWarning() << "Cannot create Tracker interface";
        return false;
    }

    return true;
}

std::pair<const QStringList&, const QByteArray&> Tracker::getResult()
{
    return {m_dbusReplyData, m_pipeData};
}
