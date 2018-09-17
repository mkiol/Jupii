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
    static const QString typeKey;
    static const QString cookieKey;

    static Utils* instance(QObject *parent = nullptr);

    bool getNetworkIf(QString &ifname, QString &address);
    bool checkNetworkIf();
    Q_INVOKABLE QString friendlyDeviceType(const QString &deviceType);
    Q_INVOKABLE QString friendlyServiceType(const QString &serviceType);
    Q_INVOKABLE QString secToStr(int value);

    QString hash(const QString &value);
    static bool isIdValid(const QString &id);
    static bool isIdValid(const QUrl &id);
    static QString pathFromId(const QString &id);
    static QString pathFromId(const QUrl &id);
    static int typeFromId(const QString &id);
    static int typeFromId(const QUrl &id);
    static QString cookieFromId(const QString &id);
    static QString cookieFromId(const QUrl &id);
    static QString idFromUrl(const QUrl &url, const QString &cookie);
    static QUrl swapUrlInId(const QUrl &url, const QUrl &id);
    static QString swapUrlInId(const QUrl &url, const QString &id);
    static QUrl urlFromId(const QUrl &id);
    static QUrl urlFromId(const QString &id);
    static QUrl urlWithTypeFromId(const QUrl &id);
    static QUrl urlWithTypeFromId(const QString &id);
    static bool pathTypeCookieFromId(const QUrl &id,
                                        QString* path = nullptr,
                                        int* type = nullptr,
                                        QString* cookie = nullptr);

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
