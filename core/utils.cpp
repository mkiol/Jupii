/* Copyright (C) 2017 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "utils.h"

#include <QtGlobal>
#include <QNetworkAddressEntry>
#include <QHostAddress>
#include <QList>
#include <QAbstractSocket>
#include <QDebug>
#include <QStringList>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QTextStream>
#include <QDir>
#include <QUrlQuery>
#include <QTime>
#include <QStandardPaths>
#include <QGuiApplication>
#include <QClipboard>
#ifdef SAILFISH
#include <QMetaObject>
#endif

#include "settings.h"
#include "gpoddermodel.h"
#include "services.h"
#include "renderingcontrol.h"
#include "directory.h"

const QString Utils::typeKey = "jupii_type";
const QString Utils::cookieKey = "jupii_cookie";
const QString Utils::nameKey = "jupii_name";
const QString Utils::authorKey = "jupii_author";
const QString Utils::origUrlKey = "jupii_origurl";
const QString Utils::iconKey = "jupii_icon";
const QString Utils::descKey = "jupii_desc";
const QString Utils::playKey = "jupii_play";
const QString Utils::idKey = "jupii_id";
const QString Utils::ytdlKey = "jupii_ytdl";
const QString Utils::appKey = "jupii_app";
const QString Utils::durKey = "jupii_dur";

Utils* Utils::m_instance = nullptr;

Utils::Utils(QObject *parent) : QObject(parent),
    nam(new QNetworkAccessManager())
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

QString Utils::homeDirPath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
}

QString Utils::secToStr(int value) const
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

bool Utils::ethNetworkInf(const QNetworkInterface& interface)
{
#if QT_VERSION < QT_VERSION_CHECK(5, 11, 0)
    return interface.name().startsWith('e') || interface.name().startsWith("rndis");
#else
    return interface.type() == QNetworkInterface::Ethernet;
#endif
}

bool Utils::wlanNetworkInf(const QNetworkInterface& interface)
{
#if QT_VERSION < QT_VERSION_CHECK(5, 11, 0)
    return interface.name().startsWith('w');
#else
    return interface.type() == QNetworkInterface::Wifi;
#endif
}

QStringList Utils::networkInfs(bool onlyUp)
{
    lastNetIfs.clear();
    lastNetIfs.append(tr("Auto"));
    for (auto& interface : QNetworkInterface::allInterfaces()) {
        if (interface.isValid() && (ethNetworkInf(interface) || wlanNetworkInf(interface))) {
            if (!onlyUp || interface.flags().testFlag(QNetworkInterface::IsRunning))
                lastNetIfs.append(interface.name());
        }
    }
    return lastNetIfs;
}

int Utils::prefNetworkInfIndex()
{
    const auto pref_ifname = Settings::instance()->getPrefNetInf();
    if (pref_ifname.isEmpty()) {
        return 0;
    }

    if (lastNetIfs.isEmpty()) {
        networkInfs();
    }

    int idx = lastNetIfs.indexOf(pref_ifname, 1);

    return idx > -1 ? idx : 0;
}

void Utils::setPrefNetworkInfIndex(int idx) const
{
    if (lastNetIfs.size() <= idx) {
        qWarning() << "Invalid networtk interface index";
        return;
    }

    if (idx == 0) {
        Settings::instance()->setPrefNetInf({});
    } else {
        Settings::instance()->setPrefNetInf(lastNetIfs.at(idx));
    }
}

void Utils::removeFromCacheDir(const QStringList &filter)
{
    QDir dir{Settings::instance()->getCacheDir()};
    dir.setNameFilters(filter);
    dir.setFilter(QDir::Files);

    foreach (const auto& f, dir.entryList()) {
        qDebug() << "Removing file from cache:" << f;
        dir.remove(f);
    }
}

bool Utils::writeToCacheFile(const QString &filename, const QByteArray &data, bool del)
{
    QDir dir{Settings::instance()->getCacheDir()};
    return writeToFile(dir.absoluteFilePath(filename), data, del);
}

bool Utils::writeToFile(const QString &path, const QByteArray &data, bool del)
{
    QFile f{path};

    if (del && f.exists())
        f.remove();

    if (!f.exists()) {
        if (f.open(QIODevice::WriteOnly)) {
            f.write(data);
            f.close();
        } else {
            qWarning() << "Cannot open file for writing:" << f.fileName() << f.errorString();
            return false;
        }
    } else {
        qDebug() << "File already exists:" << f.fileName();
        return false;
    }

    return true;
}

bool Utils::createCacheDir()
{
    const auto dir = Settings::instance()->getCacheDir();

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
    const auto dir = Settings::instance()->getPlaylistDir();

    if (!QFile::exists(dir)) {
        if (!QDir::root().mkpath(dir)) {
            qWarning() << "Unable to create playlist dir!";
            return false;
        }
    }

    return true;
}

QString Utils::pathToCacheFile(const QString &filename)
{
    QDir dir(Settings::instance()->getCacheDir());
    return dir.absoluteFilePath(filename);
}

bool Utils::readFromCacheFile(const QString &filename, QByteArray &data)
{
    QDir dir{Settings::instance()->getCacheDir()};
    QFile f{dir.absoluteFilePath(filename)};

    if (f.exists()) {
        if (f.open(QIODevice::ReadOnly)) {
            data = f.readAll();
            f.close();
            return true;
        } else {
            qWarning() << "Cannot open file for reading:" << f.fileName() << f.errorString();
        }
    }

    return false;
}

QString Utils::readLicenseFile() const
{
    QByteArray data;

    const auto dirs = QStandardPaths::standardLocations(QStandardPaths::DataLocation);

    foreach (const auto& dir, dirs) {
        QFile file{QDir{dir}.absoluteFilePath("LICENSE")};
        if (file.exists()) {
            if (file.open(QIODevice::ReadOnly)) {
                data = file.readAll();
                file.close();
            } else {
                qWarning() << "Cannot open license file" << file.fileName();
            }
            break;
        }
    }

    return QString::fromUtf8(data);
}

bool Utils::cacheFileExists(const QString &filename)
{
    QDir dir{Settings::instance()->getCacheDir()};
    return QFileInfo::exists(dir.absoluteFilePath(filename));
}

QString Utils::cachePathForFile(const QString &filename)
{
    QDir dir{Settings::instance()->getCacheDir()};
    auto path = dir.absoluteFilePath(filename);

    if (QFileInfo::exists(path))
        return path;

    return {};
}

QString Utils::hash(const QString &value)
{
    return QString::number(qHash(value));
}

QUrl Utils::iconFromId(const QUrl &id)
{
    QUrl icon;
    Utils::pathTypeNameCookieIconFromId(id, nullptr, nullptr, nullptr, nullptr, &icon);
    return icon;
}

bool Utils::pathTypeNameCookieIconFromId(const QUrl& id,
                                     QString* path,
                                     int* type,
                                     QString* name,
                                     QString* cookie,
                                     QUrl* icon,
                                     QString* desc,
                                     QString* author,
                                     QUrl* origUrl,
                                     bool* ytdl,
                                     bool* play,
                                     QString* app,
                                     int *duration)
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

    if (type || cookie || name || desc || author || icon || origUrl || play || ytdl || app || duration) {
        QUrlQuery q{id};
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
        if (origUrl && q.hasQueryItem(Utils::origUrlKey))
            *origUrl = QUrl(q.queryItemValue(Utils::origUrlKey));
        if (play && q.hasQueryItem(Utils::playKey))
            *play = (q.queryItemValue(Utils::playKey) == "true");
        if (ytdl && q.hasQueryItem(Utils::ytdlKey))
            *ytdl = (q.queryItemValue(Utils::ytdlKey) == "true");
        if (app && q.hasQueryItem(Utils::appKey))
            *app = q.queryItemValue(Utils::appKey);
        if (duration && q.hasQueryItem(Utils::durKey))
            *duration = q.queryItemValue(Utils::durKey).toInt();
    }

    return true;
}

bool Utils::isIdValid(const QString &id)
{
    return isIdValid(QUrl{id});
}

bool Utils::isUrlOk(const QUrl &url) const
{
    return Utils::isUrlValid(url);
}

bool Utils::isGpodderAvailable() const
{
    return Gpodder::enabled();
}

bool Utils::isIdMic(const QUrl &id) const
{
    return Utils::isUrlMic(id);
}

bool Utils::isIdPulse(const QUrl &id) const
{
    return Utils::isUrlPulse(id);
}

bool Utils::isIdScreen(const QUrl &id) const
{
    return Utils::isUrlScreen(id);
}

bool Utils::isIdUpnp(const QUrl &id) const
{
    return Utils::isUrlUpnp(id);
}

bool Utils::isIdRec(const QUrl &id)
{
    auto meta = ContentServer::instance()->getMetaForId(id, false);
    if (meta)
        return meta->album == ContentServer::recAlbumName;
    return false;
}

QString Utils::devNameFromUpnpId(const QUrl &id)
{
    auto meta = ContentServer::instance()->getMetaForId(id, false);
    if (meta && meta->itemType == ContentServer::ItemType_Upnp && !meta->upnpDevId.isEmpty())
        return Directory::instance()->deviceNameFromId(meta->upnpDevId);
    return {};
}

QString Utils::urlToPath(const QUrl &url) const
{
    return url.toLocalFile();
}

QUrl Utils::pathToUrl(const QString &path) const
{
    return QUrl::fromLocalFile(path);
}

void Utils::setClipboard(const QString &data) const
{
    QGuiApplication::clipboard()->setText(data);
}

int Utils::itemTypeFromUrl(const QUrl &url) const
{
    return static_cast<int>(ContentServer::itemTypeFromUrl(url));
}

QString Utils::recUrlFromId(const QUrl &id)
{
    if (!id.isEmpty()) {
        if (const auto meta = ContentServer::instance()->getMetaForId(id, false);
                meta) {
            return meta->recUrl;
        }
    }
    return {};
}

QString Utils::recDateFromId(const QUrl &id)
{
    if (!id.isEmpty()) {
        if (const auto meta = ContentServer::instance()->getMetaForId(id, false);
                meta && !meta->recDate.isNull())
            return friendlyDate(meta->recDate);
    }
    return {};
}

QString Utils::friendlyDate(const QDateTime &date)
{
    if (date.isValid()) {
        QString date_s;
        auto cdate = QDate::currentDate();
        if (date.date() == cdate)
            date_s = tr("Today");
        else if (date.date() == cdate.addDays(-1))
            date_s = tr("Yesterday");

        auto time_f = QLocale::system().timeFormat(QLocale::ShortFormat);
        if (date_s.isEmpty()) {
            return date.toString("d MMM yy, " + time_f);
        } else {
            return date_s + ", " + date.toString(time_f);
        }
    }

    return {};
}

QString Utils::parseArtist(const QString &artist)
{
    const auto artist_parsed = artist.split(',').first();

    int pos = artist_parsed.indexOf('(');
    if (pos < 2)
        return artist_parsed;

    return artist_parsed.left(pos-1).trimmed();
}

QDateTime Utils::parseDate(const QString &date)
{
    return QDateTime::fromString(date, Qt::ISODate);
}

bool Utils::isUrlValid(const QUrl &url)
{
    if (!url.isValid())
        return false;

    if (url.isLocalFile())
        return true;

    if ((url.scheme() == "http" ||
         url.scheme() == "https" ||
         url.scheme() == "jupii") &&
         !url.host().isEmpty())
        return true;

    return false;
}

bool Utils::isIdValid(const QUrl &id)
{
    if (!id.isValid()) {
        //qWarning() << "Utils: Id is invalid:" << id.toString();
        return false;
    }

    QUrlQuery q{id};
    QString cookie;
    if (q.hasQueryItem(Utils::cookieKey))
        cookie = q.queryItemValue(Utils::cookieKey);

    return !cookie.isEmpty();
}

bool Utils::isUrlMic(const QUrl &url)
{
    return url.scheme() == "jupii" && url.host() == "mic";
}

bool Utils::isUrlPulse(const QUrl &url)
{
    return url.scheme() == "jupii" && url.host() == "pulse";
}

bool Utils::isUrlScreen(const QUrl &url)
{
    return url.scheme() == "jupii" && url.host() == "screen";
}

bool Utils::isUrlUpnp(const QUrl &url)
{
    return url.scheme() == "jupii" && url.host() == "upnp";
}

QUrl Utils::urlFromText(const QString &text, const QString &context)
{
    if (!context.isEmpty()) {
        // check if text is a relative file path
        QDir dir{context};
        if (dir.exists(text))
            return QUrl::fromLocalFile(dir.absoluteFilePath(text));

        // check if text is a ralative URL
        QUrl url{text};
        if (url.isRelative()) {
            url = QUrl{context}.resolved(url);
            if (Utils::isUrlValid(url))
                return url;
        }
    }

    // check if text is a absolute file path
    const auto file = QUrl::fromLocalFile(text);
    if (QFileInfo::exists(file.toLocalFile()))
        return file;

    // check if text is valid URL
    QUrl url(text);
    if (Utils::isUrlValid(url))
        return url;

    return {};
}

QString Utils::pathFromId(const QString &id)
{
    return pathFromId(QUrl{id});
}

QString Utils::pathFromId(const QUrl &id)
{
    if (QString path; Utils::pathTypeNameCookieIconFromId(id, &path)) {
        return path;
    }

    return {};
}

int Utils::typeFromId(const QString& id)
{
    return typeFromId(QUrl{id});
}

int Utils::typeFromId(const QUrl& id)
{
    if (int type = 0; Utils::pathTypeNameCookieIconFromId(id, nullptr, &type)) {
        return type;
    }

    return 0;
}

QString Utils::cookieFromId(const QString &id)
{
    return cookieFromId(QUrl{id});
}

QString Utils::cookieFromId(const QUrl &id)
{
    if (QString cookie;
            Utils::pathTypeNameCookieIconFromId(id, nullptr, nullptr, nullptr, &cookie)) {
        return cookie;
    }

    return {};
}

QString Utils::nameFromId(const QString &id)
{
    return nameFromId(QUrl{id});
}

QString Utils::nameFromId(const QUrl &id)
{
    if (QString name;
            Utils::pathTypeNameCookieIconFromId(id, nullptr, nullptr, &name)) {
        return name;
    }

    return {};
}

void Utils::fixUrl(QUrl &url)
{
    url.setQuery(QUrlQuery{url});
}

QUrl Utils::swapUrlInId(const QUrl &url, const QUrl &id,
                        bool swapCookie,
                        bool swapOrigUrl, bool swapYtdl)
{
    QUrlQuery uq{url};
    if (uq.hasQueryItem(Utils::cookieKey))
        uq.removeAllQueryItems(Utils::cookieKey);
    if (uq.hasQueryItem(Utils::typeKey))
        uq.removeAllQueryItems(Utils::typeKey);

    QString cookie, type, name, icon, desc, author, origUrl, ytdl, dur, app;
    QUrlQuery iq{id};
    if (swapCookie && iq.hasQueryItem(Utils::cookieKey))
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
    if (swapOrigUrl && iq.hasQueryItem(Utils::origUrlKey))
        origUrl = iq.queryItemValue(Utils::origUrlKey);
    if (swapYtdl && iq.hasQueryItem(Utils::ytdlKey))
        ytdl = iq.queryItemValue(Utils::ytdlKey);
    if (iq.hasQueryItem(Utils::durKey))
        dur = iq.queryItemValue(Utils::durKey);
    if (iq.hasQueryItem(Utils::appKey))
        app = iq.queryItemValue(Utils::appKey);

    if (!cookie.isEmpty())
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
    if (!origUrl.isEmpty())
        uq.addQueryItem(Utils::origUrlKey, origUrl);
    if (!ytdl.isEmpty())
        uq.addQueryItem(Utils::ytdlKey, ytdl);
    if (!dur.isEmpty())
        uq.addQueryItem(Utils::durKey, dur);
    if (!app.isEmpty())
        uq.addQueryItem(Utils::appKey, app);

    QUrl newUrl{url};
    newUrl.setQuery(uq);

    return newUrl;
}

QString Utils::idFromUrl(const QUrl &url, const QString &cookie)
{
    QUrlQuery q{url};
    if (q.hasQueryItem(Utils::cookieKey))
        q.removeAllQueryItems(Utils::cookieKey);
    q.addQueryItem(Utils::cookieKey, cookie);
    QUrl id{url};
    id.setQuery(q);
    return id.toString();
}

QUrl Utils::cleanId(const QUrl &id)
{
    QUrlQuery q{id};
    if (q.hasQueryItem(Utils::playKey)) {
        q.removeAllQueryItems(Utils::playKey);
        QUrl url{id};
        url.setQuery(q);
        return url;
    }
    return id;
}

QUrl Utils::urlFromId(const QUrl &id)
{
    QUrlQuery q{id};
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
    if (q.hasQueryItem(Utils::playKey))
        q.removeAllQueryItems(Utils::playKey);
    if (q.hasQueryItem(Utils::origUrlKey))
        q.removeAllQueryItems(Utils::origUrlKey);
    if (q.hasQueryItem(Utils::ytdlKey))
        q.removeAllQueryItems(Utils::ytdlKey);
    if (q.hasQueryItem(Utils::appKey))
        q.removeAllQueryItems(Utils::appKey);
    if (q.hasQueryItem(Utils::durKey))
        q.removeAllQueryItems(Utils::durKey);
    QUrl url{id};
    url.setQuery(q);
    return url;
}

QUrl Utils::bestUrlFromId(const QUrl &id)
{
    QUrl url{id};
    QUrlQuery q{id};
    /*if (q.hasQueryItem(Utils::origUrlKey)) {
        auto origUrl = q.queryItemValue(Utils::origUrlKey);
        if (origUrl.isEmpty()) {
            q.removeAllQueryItems(Utils::origUrlKey);
        } else {
            return swapUrlInId(origUrl, id, false, false, false);
        }
    }*/
    if (q.hasQueryItem(Utils::cookieKey))
        q.removeAllQueryItems(Utils::cookieKey);
    if (q.hasQueryItem(Utils::playKey))
        q.removeAllQueryItems(Utils::playKey);
    /*if (q.hasQueryItem(Utils::ytdlKey))
        q.removeAllQueryItems(Utils::ytdlKey);*/
    url.setQuery(q);
    return url;
}

QUrl Utils::urlFromId(const QString &id)
{
    return urlFromId(QUrl{id});
}

QString Utils::randString(int len)
{
    auto t = QThread::currentThread();
    if (!seedDone.contains(t)) {
       // Seed init, needed for key generation
       qsrand(QTime::currentTime().msec());
       seedDone[t] = true;
    }

    const QString pc("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789");

    QString rs;
    for(int i = 0; i < len; ++i) {
        rs.append(pc.at(qrand() % pc.length()));
    }

    return rs;
}

void Utils::removeFile(const QString &path)
{
    QFile file{path};
    if (file.exists())
        file.remove();
}

QString Utils::dirNameFromPath(const QString &path) const
{
    QDir dir{path};
    dir.makeAbsolute();
    return dir.dirName();
}


QString Utils::deviceIconFileName(QString id, const QString& ext)
{
    return "device_image_" + id.replace(':', '-') + "." + ext;
}

QString Utils::deviceIconFilePath(const QString& id)
{
    auto jpgPath = cachePathForFile(deviceIconFileName(id, "jpg"));
    return jpgPath.isEmpty() ?
                Utils::cachePathForFile(deviceIconFileName(id, "png")) : jpgPath;
}

QString Utils::escapeName(const QString& filename)
{
    QString escapedName{filename};
    escapedName.remove(QRegExp(QStringLiteral("\\[[^\\]]*\\]|\\([^\\)]*\\)|\\{[^\\}]*\\}|<[^>}]*>|" \
                                          "[_!@#\\$\\^&\\*\\+=\\|\\\\/\"'\\?~`]+"), Qt::CaseInsensitive));
    escapedName.replace(QRegExp(QStringLiteral("\\s\\s"), Qt::CaseInsensitive)," ");
    escapedName = escapedName.trimmed().normalized(QString::NormalizationForm_KD).toLower();
    return escapedName;
}

#ifdef SAILFISH
void Utils::setQmlRootItem(QQuickItem *rootItem)
{
    qmlRootItem = rootItem;
}

void Utils::activateWindow()
{
    if (qmlRootItem) {
        QMetaObject::invokeMethod(qmlRootItem, "activate");
    }
}
#endif
