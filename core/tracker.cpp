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
#include "dbus_systemd_inf.h"
#include "utils.h"

#include <fcntl.h>
#include <unistd.h>

Tracker* Tracker::m_instance = nullptr;

Tracker::Tracker(QObject *parent) :
    QObject(parent),
    m_cacheDir(QStandardPaths::writableLocation(
                   QStandardPaths::GenericCacheLocation))
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
    m_dbusReplyData.clear();
    m_pipeData.clear();

    if (!m_dbus_inf || !m_dbus_inf->isValid()) {
        qWarning() << "Tracker Dbus interface is not created";
        emit queryError();
        return false;
    }

    int readFd, writeFd;

    if (!createUnixPipe(readFd, writeFd)) {
        qWarning() << "Cannot create Unix pipe";
        emit queryError();
        return false;
    }

    QDBusUnixFileDescriptor qfd(writeFd);
    close(writeFd);

    auto reply = m_dbus_inf->Query(query, qfd);

    reply.waitForFinished();

    close(qfd.fileDescriptor());

    if (reply.isError()) {
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

bool Tracker::startTracker()
{
    OrgFreedesktopSystemd1ManagerInterface dbus_inf(
                QStringLiteral("org.freedesktop.systemd1"),
                QStringLiteral("/org/freedesktop/systemd1"),
                QDBusConnection::sessionBus());

    if (!dbus_inf.isValid()) {
        qWarning() << "Systemd interface cannot be created";
        return false;
    }

    auto reply = dbus_inf.StartUnit(
                QStringLiteral("tracker-store.service"), QStringLiteral("replace"));

    reply.waitForFinished();

    if (reply.isError()) {
        auto error = reply.error();
        qWarning() << "Systemd error reply:" << error.name() << error.message();
        return false;
    }

    const auto path = reply.value().path();

    if (!dbus_inf.isValid() || path.isEmpty()) {
        qWarning() << "Systemd job is invalid";
        return false;
    }

    QEventLoop loop;
    QTimer::singleShot(1000, &loop, &QEventLoop::quit); // timeout 1s
    connect(&dbus_inf, &OrgFreedesktopSystemd1ManagerInterface::JobRemoved,
            this, [&loop, &path](uint,
                          const QDBusObjectPath &job,
                          const QString&,
                          const QString &result){
        qDebug() << "JobRemoved:" << job.path() << result;
        if (path == job.path()) {
            loop.exit(1);
        }
    });

    return loop.exec() == 1;
}

bool Tracker::createTrackerInf(bool startTrackerOnError)
{
    if (!m_dbus_inf) {
        auto bus = QDBusConnection::sessionBus();
        auto cap = bus.connectionCapabilities();

        if (!cap.testFlag(QDBusConnection::UnixFileDescriptorPassing)) {
            qWarning() << "Dbus doesn't support Unix File Descriptor";
            return false;
        }

        m_dbus_inf = std::make_unique<OrgFreedesktopTracker1SteroidsInterface>(
                    QStringLiteral("org.freedesktop.Tracker1"),
                    QStringLiteral("/org/freedesktop/Tracker1/Steroids"),
                    bus);

        if (!m_dbus_inf->isValid() && startTrackerOnError) {
            qWarning() << "Tracker interface unavailable but trying to start tracker-store";
            m_dbus_inf.reset();
            return startTracker() ? createTrackerInf(false) : false;
        }
    }

    if (!m_dbus_inf->isValid()) {
        qWarning() << "Tracker interface cannot be created";
        m_dbus_inf.reset();
        return false;
    }

    return true;
}

std::pair<const QStringList&, const QByteArray&> Tracker::getResult()
{
    return {m_dbusReplyData, m_pipeData};
}
