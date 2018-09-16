/* Copyright (C) 2017 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef DBUSAPP_H
#define DBUSAPP_H

#include <QObject>

#include "taskexecutor.h"

class DbusProxy :
        public QObject,
        public TaskExecutor
{
    Q_OBJECT
    Q_PROPERTY(bool canControl READ canControl WRITE setCanControl NOTIFY canControlChanged)

public:
    explicit DbusProxy(QObject *parent = nullptr);
    bool canControl();
    void setCanControl(bool value);

signals:
    void requestAppendPath(const QString& path);
    void requestPlayPath(const QString& path);
    void requestClearPlaylist();
    void canControlChanged();

    // internal
    void CanControlPropertyChanged(bool canControl);

public slots:
    void appendPath(const QString& path);
    void playPath(const QString& path);
    void clearPlaylist();

private:
    bool m_canControl = false;
};

#endif // DBUSAPP_H
