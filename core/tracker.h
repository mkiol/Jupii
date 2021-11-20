/* Copyright (C) 2018-2021 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef TRACKER_H
#define TRACKER_H

#include <QObject>
#include <QByteArray>
#include <QStringList>
#include <QRegExp>
#include <utility>
#include <variant>

#include "dbus_tracker_inf.h"

class Tracker :
        public QObject
{
    Q_OBJECT
public:
    static Tracker* instance(QObject *parent = nullptr);
    static QString genAlbumArtFile(const QString& albumName,
                                   const QString& artistName);
    bool query(const QString& query, bool emitSignal = true);
    std::pair<const QStringList&, const QByteArray&> getResult();
    inline bool tracker1() const { return std::holds_alternative<OrgFreedesktopTracker1SteroidsInterface>(m_dbus_inf); }
    inline bool tracker3() const { return std::holds_alternative<OrgFreedesktopTracker3EndpointInterface>(m_dbus_inf); }
    inline bool valid() const { return !std::holds_alternative<std::monostate>(m_dbus_inf); }
signals:
    void queryFinished(const QStringList& varNames, const QByteArray& data);
    void queryError();

private:
    static Tracker* m_instance;
    const QString m_cacheDir;
    std::variant<std::monostate,
                 OrgFreedesktopTracker3EndpointInterface,
                 OrgFreedesktopTracker1SteroidsInterface> m_dbus_inf;
    QByteArray m_pipeData;
    QStringList m_dbusReplyData;

    Tracker(QObject *parent = nullptr);
    static QString genTrackerId(const QString& name);
    bool createTrackerInf();
    bool createUnixPipe(int& readFd, int& writeFd);
    bool pingTracker1() const;
    bool pingTracker3() const;
};

#endif // TRACKER_H
