/* Copyright (C) 2017 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef UTILS_H
#define UTILS_H

#include <QString>
#include <QPair>
#include <QObject>
#include <QByteArray>

class Utils : public QObject
{
    Q_OBJECT
public:
    static Utils* instance(QObject *parent = nullptr);

    bool getNetworkIf(QString &ifname, QString &address);
    bool checkNetworkIf();
    Q_INVOKABLE QString friendlyDeviceType(const QString &deviceType);
    Q_INVOKABLE QString friendlyServiceType(const QString &serviceType);
    Q_INVOKABLE QString secToStr(int value);

    QString hash(const QString &value);
    static QString pathFromId(const QString &id);
    static QString cookieFromId(const QString &id);
    static QString randString(int len = 5);
    static void removeFile(const QString &path);
    bool createCacheDir();
    bool createPlaylistDir();
    bool writeToCacheFile(const QString &filename, const QByteArray &data);
    bool writeToFile(const QString &path, const QByteArray &data);
    bool readFromFile(const QString &filename, QByteArray &data);

private:
    static Utils* m_instance;

    explicit Utils(QObject *parent = nullptr);
};

#endif // UTILS_H
