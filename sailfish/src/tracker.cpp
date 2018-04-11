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

#include "tracker.h"
#include "trackercursor.h"
#include "settings.h"

#include <fcntl.h>
#include <unistd.h>

Tracker* Tracker::m_instance = nullptr;

Tracker::Tracker(QObject *parent) :
    QObject(parent),
    TaskExecutor(parent, 1),
    m_re1("\\[[^\\]]*\\]|\\([^\\)]*\\)|\\{[^\\}]*\\}|<[^>}]*>|" \
          "[_!@#\\$\\^&\\*\\+=\\|\\\/\"'\\?~`]+", Qt::CaseInsensitive),
    m_re2("\\s\\s", Qt::CaseInsensitive),
    m_cacheDir(QStandardPaths::writableLocation(
                   QStandardPaths::GenericCacheLocation))
{
    if (!createTrackerInf())
        qWarning() << "Can't create Tracker Dbus interface";
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
        qWarning() << "Can't create Unix pipe";
        return false;
    }

    readFd = fds[0];
    writeFd = fds[1];

    return true;
}

void Tracker::queryAsync(const QString &query)
{
    startTask([this, query](){
        this->query(query);
    });
}

bool Tracker::query(const QString &query)
{
    m_dbusReplyData.clear();
    m_pipeData.clear();

    if (m_tracker_inf == nullptr) {
        qWarning() << "Tracker Dbus interface is not created";
        emit queryError();
        return false;
    }

    if (!m_tracker_inf->isValid()) {
        qWarning() << "Tracker Dbus interface is invalid";
        emit queryError();
        return false;
    }

    int readFd, writeFd;

    if (!createUnixPipe(readFd, writeFd)) {
        qWarning() << "Can't create Unix pipe";
        emit queryError();
        return false;
    }

    QDBusUnixFileDescriptor qfd(writeFd);
    close(writeFd);

    auto reply = m_tracker_inf->Query(query, qfd);

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
        //qDebug() << "Pipe bytes received:" << bytes_read;
        m_pipeData.append(pipe_buf, bytes_read);
    }

    //qDebug() << "Pipe last bytes_read: " << bytes_read;

    close(readFd);

    if (m_pipeData.isEmpty())
        qWarning() << "No data received from pipe";

    emit queryFinished(m_dbusReplyData, m_pipeData);

    return true;
}

QString Tracker::genTrackerId(const QString& name)
{    
    // Strip invalid entities and normalize as defined in
    // https://wiki.gnome.org/action/show/DraftSpecs/MediaArtStorageSpec

    QString striped(name);
    striped.remove(m_re1);
    striped.replace(m_re2," ");
    striped = striped.trimmed().normalized(
                QString::NormalizationForm_KD).toLower();

    QString hash = QString(QCryptographicHash::hash(
                               striped.isEmpty() ? " " : striped.toUtf8(),
                               QCryptographicHash::Md5).toHex()
                           );
    return hash;
}

QString Tracker::genAlbumArtFile(const QString &albumName,
                                 const QString &artistName)
{
    QString albumId = genTrackerId(albumName);
    QString artistId = genTrackerId(artistName);

    QString filepath = m_cacheDir + "/media-art/" + "album-" +
            artistId + "-" + albumId + ".jpeg";

    return filepath;
}

bool Tracker::createTrackerInf()
{
    if (m_tracker_inf == nullptr) {
        QDBusConnection bus = QDBusConnection::sessionBus();
        QDBusConnection::ConnectionCapabilities cap =
                bus.connectionCapabilities();

        if (!cap.testFlag(QDBusConnection::UnixFileDescriptorPassing)) {
            qWarning() << "Dbus doesn't support Unix File Descriptor";
            return false;
        }

        m_tracker_inf = new OrgFreedesktopTracker1SteroidsInterface(
                    "org.freedesktop.Tracker1",
                    "/org/freedesktop/Tracker1/Steroids",
                    bus);

        if (!m_tracker_inf->isValid()) {
            qWarning() << "Tracker interface can't be created";
            delete m_tracker_inf;
            m_tracker_inf = nullptr;
            return false;
        }
    }

    return true;
}

std::pair<const QStringList&, const QByteArray&> Tracker::getResult()
{
    return std::pair<const QStringList&, const QByteArray&>(m_dbusReplyData, m_pipeData);
}
