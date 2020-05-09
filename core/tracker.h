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

#include "dbus_tracker_inf.h"

class Tracker :
        public QObject
{
    Q_OBJECT

public:
    static Tracker* instance(QObject *parent = nullptr);
    QString genAlbumArtFile(const QString& albumName,
                                   const QString& artistName);
    bool query(const QString& query, bool emitSignal = true);
    std::pair<const QStringList&, const QByteArray&> getResult();

signals:
    void queryFinished(const QStringList& varNames, const QByteArray& data);
    void queryError();

private:
    static Tracker* m_instance;

    const QRegExp m_re1;
    const QRegExp m_re2;
    const QString m_cacheDir;

    OrgFreedesktopTracker1SteroidsInterface* m_tracker_inf = nullptr;

    QByteArray m_pipeData;
    QStringList m_dbusReplyData;

    Tracker(QObject *parent = nullptr);
    QString genTrackerId(const QString& name);
    bool createTrackerInf();
    bool createUnixPipe(int& readFd, int& writeFd);
};

#endif // TRACKER_H
