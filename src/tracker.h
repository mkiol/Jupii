/* Copyright (C) 2018-2022 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef TRACKER_H
#define TRACKER_H

#include <QByteArray>
#include <QObject>
#include <QRegExp>
#include <QStringList>
#include <utility>
#include <variant>

#include "dbus_tracker_inf.h"
#include "singleton.h"

class Tracker : public QObject, public Singleton<Tracker> {
    Q_OBJECT
   public:
    Tracker(QObject* parent = nullptr);
    static QString genAlbumArtFile(const QString& albumName,
                                   const QString& artistName);
    bool query(const QString& query, bool emitSignal = true);
    std::pair<const QStringList&, const QByteArray&> getResult();
    inline auto tracker1() const {
        return std::holds_alternative<OrgFreedesktopTracker1SteroidsInterface>(
            m_dbus_inf);
    }
    inline auto tracker3() const {
        return std::holds_alternative<OrgFreedesktopTracker3EndpointInterface>(
            m_dbus_inf);
    }
    inline auto valid() const {
        return !std::holds_alternative<std::monostate>(m_dbus_inf);
    }

   signals:
    void queryFinished(const QStringList& varNames, const QByteArray& data);
    void queryError();

   private:
    QString m_cacheDir;
    std::variant<std::monostate, OrgFreedesktopTracker3EndpointInterface,
                 OrgFreedesktopTracker1SteroidsInterface>
        m_dbus_inf;
    QByteArray m_pipeData;
    QStringList m_dbusReplyData;

    static QString genTrackerId(const QString& name);
    bool createTrackerInf();
    static bool createUnixPipe(int& readFd, int& writeFd);
    static bool pingTracker1();
    static bool pingTracker3();
};

#endif  // TRACKER_H
