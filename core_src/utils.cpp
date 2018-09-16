/* Copyright (C) 2017 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "utils.h"

#include <QNetworkInterface>
#include <QNetworkAddressEntry>
#include <QHostAddress>
#include <QList>
#include <QAbstractSocket>
#include <QDebug>
#include <QStringList>
#include <QFile>
#include <QIODevice>
#include <QTextStream>
#include <QDir>
#include <QUrlQuery>
#include <QTime>

#include "settings.h"


const QString Utils::typeKey = "jupii_type";
const QString Utils::cookieKey = "jupii_cookie";

Utils* Utils::m_instance = nullptr;

Utils::Utils(QObject *parent) : QObject(parent)
{
    createCacheDir();
}

Utils* Utils::instance(QObject *parent)
{
    if (Utils::m_instance == nullptr) {
        Utils::m_instance = new Utils(parent);
    }

    return Utils::m_instance;
}

QString Utils::friendlyDeviceType(const QString &deviceType)
{
    auto list = deviceType.split(':');
    return list.mid(list.length()-2).join(':');
    //return list.at(list.length()-2);
}

QString Utils::friendlyServiceType(const QString &serviceType)
{
    auto list = serviceType.split(':');
    return list.mid(list.length()-2).join(':');
    //return list.at(list.length()-2);
}

QString Utils::secToStr(int value)
{
    int s = value % 60;
    int m = (value - s)/60 % 3600;
    int h = (value - s - m * 60)/3600;

    QString str;
    QTextStream sstr(&str);

    if (h > 0)
        sstr << h << ":";

    sstr << m << ":";

    if (s < 10)
        sstr << "0";

    sstr << s;

    return str;
}

bool Utils::checkNetworkIf()
{
    QString ifname, address;
    return getNetworkIf(ifname, address);
}

bool Utils::getNetworkIf(QString& ifname, QString& address)
{
    auto if_list = QNetworkInterface::allInterfaces();

    for (auto interface : if_list) {

        if (interface.isValid() &&
            !interface.flags().testFlag(QNetworkInterface::IsLoopBack) &&
            interface.flags().testFlag(QNetworkInterface::IsUp) &&
            interface.flags().testFlag(QNetworkInterface::IsRunning)
#ifdef SAILFISH
            && (interface.name() == "eth2" ||
                interface.name() == "wlan0" ||
                interface.name() == "tether")
#endif
            ){
            qDebug() << "interface:" << interface.name();

            ifname = interface.name();

            auto addra = interface.addressEntries();
            for (auto a : addra) {
                auto ha = a.ip();
                if (ha.protocol() == QAbstractSocket::IPv4Protocol ||
                    ha.protocol() == QAbstractSocket::IPv6Protocol) {
                    qDebug() << " address:" << ha.toString();
                    address = ha.toString();
                    return true;
                }
            }
        }
    }

    return false;
}

bool Utils::writeToCacheFile(const QString &filename, const QByteArray &data)
{
    const QString cacheDir = Settings::instance()->getCacheDir();
    return writeToFile(cacheDir + "/" + filename, data);
}

bool Utils::writeToFile(const QString &path, const QByteArray &data)
{
    const QString cacheDir = Settings::instance()->getCacheDir();

    QFile f(path);
    if (!f.exists()) {
        if (f.open(QIODevice::WriteOnly)) {
            f.write(data);
            f.close();
        } else {
            qWarning() << "Can't open file" << f.fileName() <<
                          "for writing (" + f.errorString() + ")";
            return false;
        }
    } else {
        qDebug() << "File" << f.fileName() << "already exists";
    }

    return true;
}

bool Utils::createCacheDir()
{
    const QString dir = Settings::instance()->getCacheDir();

    if (!QFile::exists(dir)) {
        if (!QDir::root().mkpath(dir)) {
            qWarning() << "Unable to create cache dir!";
            return false;
        }
    }

    return true;
}

bool Utils::createPlaylistDir()
{
    const QString dir = Settings::instance()->getPlaylistDir();

    if (!QFile::exists(dir)) {
        if (!QDir::root().mkpath(dir)) {
            qWarning() << "Unable to create playlist dir!";
            return false;
        }
    }

    return true;
}

bool Utils::readFromFile(const QString &filename, QByteArray &data)
{
    const QString cacheDir = Settings::instance()->getCacheDir();

    QFile f(cacheDir + "/" + filename);
    if (f.exists()) {
        if (f.open(QIODevice::ReadOnly)) {
            data = f.readAll();
            f.close();
            return true;
        } else {
            qWarning() << "Can't open file" << f.fileName() <<
                          "for reading (" + f.errorString() + ")";
        }
    } else {
        qDebug() << "File" << f.fileName() << "doesn't exist";
    }

    return false;
}

QString Utils::hash(const QString &value)
{
    return QString::number(qHash(value));
}

bool Utils::pathTypeCookieFromId(const QUrl& id, QString* path, int* type, QString* cookie)
{
    if (!id.isValid()) {
        qWarning() << "Id is invalid";
        return false;
    }

    if (path) {
        if (id.isLocalFile())
            *path = id.toLocalFile();
        else
            path->clear();
    }

    if (type || cookie) {
        QUrlQuery q(id);
        if (type && q.hasQueryItem(Utils::typeKey))
            *type = q.queryItemValue(Utils::typeKey).toInt();
        if (cookie && q.hasQueryItem(Utils::cookieKey))
            *cookie = q.queryItemValue(Utils::cookieKey);
    }

    return true;
}

bool Utils::isIdValid(const QString &id)
{
    return isIdValid(QUrl(id));
}

bool Utils::isIdValid(const QUrl &id)
{
    if (!id.isValid()) {
        qWarning() << "Id is invalid";
        return false;
    }

    QUrlQuery q(id);
    QString cookie;
    if (q.hasQueryItem(Utils::cookieKey))
        cookie = q.queryItemValue(Utils::cookieKey);

    return !cookie.isEmpty();
}

QString Utils::pathFromId(const QString &id)
{
    return pathFromId(QUrl(id));
}

QString Utils::pathFromId(const QUrl &id)
{
    QString path;
    bool valid = Utils::pathTypeCookieFromId(id, &path);

    if (!valid) {
        qWarning() << "Id is invalid";
        return QString();
    }

    return path;
}

int Utils::typeFromId(const QString& id)
{
    return typeFromId(QUrl(id));
}

int Utils::typeFromId(const QUrl& id)
{
    int type;
    bool valid = Utils::pathTypeCookieFromId(id, nullptr, &type);

    if (!valid) {
        qWarning() << "Id is invalid";
        return 0;
    }

    return type;
}

QString Utils::cookieFromId(const QString &id)
{
    return cookieFromId(QUrl(id));
}

QString Utils::cookieFromId(const QUrl &id)
{
    QString cookie;
    bool valid = Utils::pathTypeCookieFromId(id, nullptr, nullptr, &cookie);

    if (!valid) {
        qWarning() << "Id is invalid";
        return QString();
    }

    return cookie;
}

QUrl Utils::swapUrlInId(const QUrl &url, const QUrl &id)
{
    QUrlQuery uq(url);
    if (uq.hasQueryItem(Utils::cookieKey))
        uq.removeAllQueryItems(Utils::cookieKey);
    if (uq.hasQueryItem(Utils::typeKey))
        uq.removeAllQueryItems(Utils::typeKey);

    QString cookie, type;
    QUrlQuery iq(id);
    if (iq.hasQueryItem(Utils::cookieKey))
        cookie = iq.queryItemValue(Utils::cookieKey);
    if (iq.hasQueryItem(Utils::typeKey))
        type = iq.queryItemValue(Utils::typeKey);

    uq.addQueryItem(Utils::cookieKey, cookie);
    if (!type.isEmpty())
        uq.addQueryItem(Utils::typeKey, type);

    QUrl newUrl(url);
    newUrl.setQuery(uq);

    return newUrl;
}

QString Utils::idFromUrl(const QUrl &url, const QString &cookie)
{
    QUrlQuery q(url);
    if (q.hasQueryItem(Utils::cookieKey))
        q.removeAllQueryItems(Utils::cookieKey);
    q.addQueryItem(Utils::cookieKey, cookie);
    QUrl id(url);
    id.setQuery(q);
    return id.toString();
}

QUrl Utils::urlFromId(const QUrl &id)
{
    QUrlQuery q(id);
    if (q.hasQueryItem(Utils::cookieKey))
        q.removeAllQueryItems(Utils::cookieKey);
    if (q.hasQueryItem(Utils::typeKey))
        q.removeAllQueryItems(Utils::typeKey);
    QUrl url(id);
    url.setQuery(q);
    return url;
}

QUrl Utils::urlWithTypeFromId(const QUrl &id)
{
    QUrlQuery q(id);
    if (q.hasQueryItem(Utils::cookieKey))
        q.removeAllQueryItems(Utils::cookieKey);
    QUrl url(id);
    url.setQuery(q);
    return url;
}

QUrl Utils::urlWithTypeFromId(const QString &id)
{
    return urlWithTypeFromId(QUrl(id));
}

QUrl Utils::urlFromId(const QString &id)
{
    return urlFromId(QUrl(id));
}

QString Utils::randString(int len)
{
   // Seed init, needed for key generation
   qsrand(QTime::currentTime().msec());

   const QString pc("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789");

   QString rs;

   for(int i = 0; i < len; ++i) {
       int in = qrand() % pc.length();
       QChar nc = pc.at(in);
       rs.append(nc);
   }

   return rs;
}

void Utils::removeFile(const QString &path)
{
    QFile file(path);
    if (file.exists())
        file.remove();
}
