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
#include "gpoddermodel.h"

const QString Utils::typeKey = "jupii_type";
const QString Utils::cookieKey = "jupii_cookie";
const QString Utils::nameKey = "jupii_name";
const QString Utils::authorKey = "jupii_author";
const QString Utils::iconKey = "jupii_icon";
const QString Utils::descKey = "jupii_desc";

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
    int m = (value - s)/60 % 60;
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

QStringList Utils::getNetworkIfs(bool onlyUp)
{
    auto ifList = QNetworkInterface::allInterfaces();

    QStringList infs;
    for (auto interface : ifList) {
        if (interface.isValid()) {
            if (!onlyUp ||
                    (interface.flags().testFlag(QNetworkInterface::IsUp) &&
                    interface.flags().testFlag(QNetworkInterface::IsRunning)))
                infs << interface.name();
        }
    }

    return infs;
}

bool Utils::getNetworkIf(QString& ifname, QString& address)
{
    auto ifList = QNetworkInterface::allInterfaces();

    // -- debug --
    /*qDebug() << "Net interfaces:";
    for (const auto &interface : ifList) {
        qDebug() << "interface:" << interface.index();
        qDebug() << "  name:" << interface.name();
        qDebug() << "  human name:" << interface.humanReadableName();
        qDebug() << "  MAC:" << interface.hardwareAddress();
        qDebug() << "  point-to-pont:" << interface.flags().testFlag(QNetworkInterface::IsPointToPoint);
        qDebug() << "  loopback:" << interface.flags().testFlag(QNetworkInterface::IsLoopBack);
        qDebug() << "  up:" << interface.flags().testFlag(QNetworkInterface::IsUp);
        qDebug() << "  running:" << interface.flags().testFlag(QNetworkInterface::IsRunning);
    }*/
    // -- debug --

    auto prefNetInf = Settings::instance()->getPrefNetInf();

    if (!prefNetInf.isEmpty()) {
        // Searching prefered network interface

        for (const auto &interface : ifList) {
            if (interface.isValid() &&
                !interface.flags().testFlag(QNetworkInterface::IsLoopBack) &&
                interface.flags().testFlag(QNetworkInterface::IsUp) &&
                interface.flags().testFlag(QNetworkInterface::IsRunning) &&
                interface.name() == prefNetInf) {

                ifname = interface.name();

                auto addra = interface.addressEntries();
                for (auto a : addra) {
                    auto ha = a.ip();
                    if (ha.protocol() == QAbstractSocket::IPv4Protocol ||
                        ha.protocol() == QAbstractSocket::IPv6Protocol) {
                        address = ha.toString();
                        qDebug() << "Net interface:" << ifname << address;
                        return true;
                    }
                }
            }
        }

        qWarning() << "Prefered net interface not found";
    }

    for (const auto &interface : ifList) {
        if (interface.isValid() &&
            !interface.flags().testFlag(QNetworkInterface::IsLoopBack) &&
            interface.flags().testFlag(QNetworkInterface::IsUp) &&
            interface.flags().testFlag(QNetworkInterface::IsRunning)
#ifdef SAILFISH
            && (interface.name() == "eth2" ||
                interface.name() == "wlan0" ||
                interface.name() == "tether")
#endif
            ) {

            ifname = interface.name();

            auto addra = interface.addressEntries();
            for (const auto &a : addra) {
                auto ha = a.ip();
                if (ha.protocol() == QAbstractSocket::IPv4Protocol ||
                    ha.protocol() == QAbstractSocket::IPv6Protocol) {
                    address = ha.toString();
                    qDebug() << "Net interface:" << ifname << address;
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

QUrl Utils::iconFromId(const QUrl &id)
{
    QUrl icon;
    Utils::pathTypeNameCookieIconFromId(id,nullptr,nullptr,nullptr,nullptr,&icon);
    return icon;
}

bool Utils::pathTypeNameCookieIconFromId(const QUrl& id,
                                     QString* path,
                                     int* type,
                                     QString* name,
                                     QString* cookie,
                                     QUrl* icon,
                                     QString* desc,
                                     QString* author)
{
    if (!id.isValid()) {
        //qWarning() << "FromId: Id is invalid:" << id.toString();
        return false;
    }

    if (path) {
        if (id.isLocalFile())
            *path = id.toLocalFile();
        else
            path->clear();
    }

    if (type || cookie || name || desc || author || icon) {
        QUrlQuery q(id);
        if (type && q.hasQueryItem(Utils::typeKey))
            *type = q.queryItemValue(Utils::typeKey).toInt();
        if (cookie && q.hasQueryItem(Utils::cookieKey))
            *cookie = q.queryItemValue(Utils::cookieKey);
        if (name && q.hasQueryItem(Utils::nameKey))
            *name = q.queryItemValue(Utils::nameKey);
        if (icon && q.hasQueryItem(Utils::iconKey))
            *icon = QUrl(q.queryItemValue(Utils::iconKey));
        if (desc && q.hasQueryItem(Utils::descKey))
            *desc = q.queryItemValue(Utils::descKey);
        if (author && q.hasQueryItem(Utils::authorKey))
            *author = q.queryItemValue(Utils::authorKey);
    }

    return true;
}

bool Utils::isIdValid(const QString &id)
{
    return isIdValid(QUrl(id));
}

bool Utils::isUrlOk(const QUrl &url)
{
    return Utils::isUrlValid(url);
}

bool Utils::isGpodderAvailable()
{
    return Gpodder::enabled();
}

bool Utils::isUrlValid(const QUrl &url)
{
    if (!url.isValid())
        return false;

    if (url.isLocalFile())
        return true;

    if ((url.scheme() == "http" || url.scheme() == "https") &&
         !url.host().isEmpty())
        return true;

    return false;
}

bool Utils::isIdValid(const QUrl &id)
{
    if (!id.isValid()) {
        qWarning() << "Utils: Id is invalid:" << id.toString();
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
    bool valid = Utils::pathTypeNameCookieIconFromId(id, &path);

    if (!valid) {
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
    bool valid = Utils::pathTypeNameCookieIconFromId(id, nullptr, &type);

    if (!valid) {
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
    bool valid = Utils::pathTypeNameCookieIconFromId(id, nullptr, nullptr,
                                                 nullptr, &cookie);

    if (!valid) {
        return QString();
    }

    return cookie;
}

QString Utils::nameFromId(const QString &id)
{
    return nameFromId(QUrl(id));
}

QString Utils::nameFromId(const QUrl &id)
{
    QString name;
    bool valid = Utils::pathTypeNameCookieIconFromId(id, nullptr, nullptr, &name);

    if (!valid) {
        return QString();
    }

    return name;
}

QUrl Utils::swapUrlInId(const QUrl &url, const QUrl &id)
{
    QUrlQuery uq(url);
    if (uq.hasQueryItem(Utils::cookieKey))
        uq.removeAllQueryItems(Utils::cookieKey);
    if (uq.hasQueryItem(Utils::typeKey))
        uq.removeAllQueryItems(Utils::typeKey);

    QString cookie, type, name, icon, desc, author;
    QUrlQuery iq(id);
    if (iq.hasQueryItem(Utils::cookieKey))
        cookie = iq.queryItemValue(Utils::cookieKey);
    if (iq.hasQueryItem(Utils::typeKey))
        type = iq.queryItemValue(Utils::typeKey);
    if (iq.hasQueryItem(Utils::nameKey))
        name = iq.queryItemValue(Utils::nameKey);
    if (iq.hasQueryItem(Utils::iconKey))
        icon = iq.queryItemValue(Utils::iconKey);
    if (iq.hasQueryItem(Utils::descKey))
        desc = iq.queryItemValue(Utils::descKey);
    if (iq.hasQueryItem(Utils::authorKey))
        author = iq.queryItemValue(Utils::authorKey);

    uq.addQueryItem(Utils::cookieKey, cookie);

    if (!type.isEmpty())
        uq.addQueryItem(Utils::typeKey, type);
    if (!name.isEmpty())
        uq.addQueryItem(Utils::nameKey, name);
    if (!icon.isEmpty())
        uq.addQueryItem(Utils::iconKey, icon);
    if (!desc.isEmpty())
        uq.addQueryItem(Utils::descKey, desc);
    if (!author.isEmpty())
        uq.addQueryItem(Utils::authorKey, author);

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
    if (q.hasQueryItem(Utils::nameKey))
        q.removeAllQueryItems(Utils::nameKey);
    if (q.hasQueryItem(Utils::iconKey))
        q.removeAllQueryItems(Utils::iconKey);
    if (q.hasQueryItem(Utils::descKey))
        q.removeAllQueryItems(Utils::descKey);
    if (q.hasQueryItem(Utils::authorKey))
        q.removeAllQueryItems(Utils::authorKey);
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
