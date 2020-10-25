/* Copyright (C) 2018 Michal Kosciesza <michal@mkiol.net>
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
#include <memory>

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

signals:
    void queryFinished(const QStringList& varNames, const QByteArray& data);
    void queryError();

private:
    static Tracker* m_instance;
    const QString m_cacheDir;
    std::unique_ptr<OrgFreedesktopTracker1SteroidsInterface> m_dbus_inf;
    QByteArray m_pipeData;
    QStringList m_dbusReplyData;

    Tracker(QObject *parent = nullptr);
    static QString genTrackerId(const QString& name);
    bool createTrackerInf(bool startTrackerOnError = true);
    bool startTracker();
    bool createUnixPipe(int& readFd, int& writeFd);
};

#endif // TRACKER_H
