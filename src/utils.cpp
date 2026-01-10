/* Copyright (C) 2017-2026 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "utils.h"

#include <QtCore/qmath.h>

#include <QAbstractSocket>
#include <QClipboard>
#include <QDate>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHostAddress>
#include <QIODevice>
#include <QList>
#include <QNetworkAddressEntry>
#include <QStandardPaths>
#include <QStringList>
#include <QTextStream>
#include <QTime>
#include <QUrlQuery>
#include <QtGlobal>
#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
#include <QRandomGenerator>
#endif
#ifdef USE_SFOS
#include <QMetaObject>
#endif

#include "contentserver.h"
#include "directory.h"
#include "gpoddermodel.h"
#include "iconprovider.h"
#include "settings.h"
#include "tracker.h"
#include "trackercursor.h"

const QString Utils::typeKey = QStringLiteral("jupii_type");
const QString Utils::cookieKey = QStringLiteral("jupii_cookie");
const QString Utils::nameKey = QStringLiteral("jupii_name");
const QString Utils::authorKey = QStringLiteral("jupii_author");
const QString Utils::albumKey = QStringLiteral("jupii_album");
const QString Utils::origUrlKey = QStringLiteral("jupii_origurl");
const QString Utils::iconKey = QStringLiteral("jupii_icon");
const QString Utils::descKey = QStringLiteral("jupii_desc");
const QString Utils::playKey = QStringLiteral("jupii_play");
const QString Utils::idKey = QStringLiteral("jupii_id");
const QString Utils::ytdlKey = QStringLiteral("jupii_ytdl");
const QString Utils::appKey = QStringLiteral("jupii_app");
const QString Utils::durKey = QStringLiteral("jupii_dur");

Utils::Utils(QObject *parent)
    : QObject{parent}, nam{std::make_unique<QNetworkAccessManager>()} {}

QString Utils::secToStrStatic(int value) {
    int s = value % 60;
    int m = (value - s) / 60 % 60;
    int h = (value - s - m * 60) / 3600;

    QString str;
    QTextStream sstr{&str};

    if (h > 0) sstr << h << ':';

    sstr << m << ':';

    if (s < 10) sstr << '0';

    sstr << s;

    return str;
}

int Utils::strToSecStatic(const QString &value) {
    auto list = value.split(':');

    if (list.size() > 2)
        return list.at(list.size() - 1).toInt() +
               60 * list.at(list.size() - 2).toInt() +
               3600 * list.at(list.size() - 3).toInt();
    if (list.size() > 1)
        return list.at(list.size() - 1).toInt() +
               60 * list.at(list.size() - 2).toInt();
    if (!list.empty()) return list.at(list.size() - 1).toInt();
    return 0;
}

bool Utils::ethNetworkInf(const QNetworkInterface &interface) {
#if QT_VERSION < QT_VERSION_CHECK(5, 11, 0)
    return interface.name().startsWith('e') ||
           interface.name().startsWith("rndis");
#else
    return interface.type() == QNetworkInterface::Ethernet;
#endif
}

bool Utils::localNetworkInf(const QNetworkInterface &interface) {
#if QT_VERSION < QT_VERSION_CHECK(5, 11, 0)
    return interface.name().startsWith("lo");
#else
    return interface.type() == QNetworkInterface::Loopback;
#endif
}

bool Utils::wlanNetworkInf(const QNetworkInterface &interface) {
#if QT_VERSION < QT_VERSION_CHECK(5, 11, 0)
    return interface.name().startsWith('w') ||
           interface.name().startsWith("tether");
#else
    return interface.type() == QNetworkInterface::Wifi;
#endif
}

QStringList Utils::networkInfs(bool onlyUp) {
    m_lastNetIfs.clear();
    m_lastNetIfs.append(tr("Auto"));
    for (auto &interface : QNetworkInterface::allInterfaces()) {
        if (interface.isValid()) {
            if (((!onlyUp ||
                  interface.flags().testFlag(QNetworkInterface::IsRunning)) &&
                 (ethNetworkInf(interface) || wlanNetworkInf(interface))) ||
                localNetworkInf(interface))
                m_lastNetIfs.append(interface.name());
        }
    }
    return m_lastNetIfs;
}

int Utils::prefNetworkInfIndex() {
    const auto pref_ifname = Settings::instance()->getPrefNetInf();
    if (pref_ifname.isEmpty()) {
        return 0;
    }

    if (m_lastNetIfs.isEmpty()) {
        networkInfs();
    }

    int idx = m_lastNetIfs.indexOf(pref_ifname, 1);

    return idx > -1 ? idx : 0;
}

void Utils::setPrefNetworkInfIndex(int idx) const {
    if (m_lastNetIfs.size() <= idx) {
        qWarning() << "Invalid networtk interface index";
        return;
    }

    if (idx == 0) {
        Settings::instance()->setPrefNetInf({});
    } else {
        Settings::instance()->setPrefNetInf(m_lastNetIfs.at(idx));
    }
}

void Utils::removeFromCacheDir(const QStringList &filter) {
    QDir dir{Settings::instance()->getCacheDir()};
    dir.setNameFilters(filter);
    dir.setFilter(QDir::Files);

    foreach (const auto &f, dir.entryList()) {
        qDebug() << "Removing file from cache:" << f;
        dir.remove(f);
    }
}

std::optional<QString> Utils::writeToCacheFile(const QString &filename,
                                               const QByteArray &data,
                                               bool del) {
    if (filename.isEmpty()) return std::nullopt;

    auto path =
        QDir{Settings::instance()->getCacheDir()}.absoluteFilePath(filename);

    if (!writeToFile(path, data, del)) {
        return std::nullopt;
    }

    return path;
}

bool Utils::writeToFile(const QString &path, const QByteArray &data, bool del) {
    QFile f{path};

    if (del && f.exists()) f.remove();

    if (f.exists()) {
        qDebug() << "file already exists:" << f.fileName();
        return false;
    }

    if (!f.open(QIODevice::WriteOnly)) {
        qWarning() << "cannot open file for writing:" << f.fileName()
                   << f.errorString();
        return false;
    }

    f.write(data);

    return true;
}

bool Utils::createPlaylistDir() {
    const auto dir = Settings::instance()->getPlaylistDir();

    if (!QFile::exists(dir)) {
        if (!QDir::root().mkpath(dir)) {
            qWarning() << "Unable to create playlist dir!";
            return false;
        }
    }

    return true;
}

QString Utils::pathToCacheFile(const QString &filename) {
    QDir dir(Settings::instance()->getCacheDir());
    return dir.absoluteFilePath(filename);
}

bool Utils::readFromCacheFile(const QString &filename, QByteArray &data) {
    QDir dir{Settings::instance()->getCacheDir()};
    QFile f{dir.absoluteFilePath(filename)};

    if (!f.exists()) return false;

    if (f.open(QIODevice::ReadOnly)) {
        data = f.readAll();
        return true;
    }

    qWarning() << "Cannot open file for reading:" << f.fileName()
               << f.errorString();

    return false;
}

QString Utils::readLicenseFile() const {
    QByteArray data;

    const auto dirs =
        QStandardPaths::standardLocations(QStandardPaths::DataLocation);

    foreach (const auto &dir, dirs) {
        QFile file{QDir{dir}.absoluteFilePath(QStringLiteral("LICENSE"))};
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

bool Utils::cacheFileExists(const QString &filename) {
    return static_cast<bool>(cachePathForFile(filename));
}

std::optional<QString> Utils::cachePathForFile(const QString &filename) {
    auto path =
        QDir{Settings::instance()->getCacheDir()}.absoluteFilePath(filename);
    if (QFileInfo::exists(path)) return path;
    return std::nullopt;
}

QString Utils::hash(const QString &value) {
    return QString::number(qHash(value));
}

QUrl Utils::iconFromId(const QUrl &id) {
    QUrl icon;
    Utils::pathTypeNameCookieIconFromId(id, nullptr, nullptr, nullptr, nullptr,
                                        &icon);
    return icon;
}

bool Utils::pathTypeNameCookieIconFromId(
    const QUrl &id, QString *path, int *type, QString *name, QString *cookie,
    QUrl *icon, QString *desc, QString *author, QString *album, QUrl *origUrl,
    bool *ytdl, bool *play, QString *app, int *duration) {
    if (!id.isValid()) {
        qWarning() << "Id is invalid:" << id.toString();
        return false;
    }

    if (path) {
        if (id.isLocalFile())
            *path = id.toLocalFile();
        else
            path->clear();
    }

    if (type || cookie || name || desc || author || album || icon || origUrl ||
        play || ytdl || app || duration) {
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
        if (album && q.hasQueryItem(Utils::albumKey))
            *album = q.queryItemValue(Utils::albumKey);
        if (origUrl && q.hasQueryItem(Utils::origUrlKey))
            *origUrl = QUrl(q.queryItemValue(Utils::origUrlKey));
        if (play && q.hasQueryItem(Utils::playKey))
            *play =
                (q.queryItemValue(Utils::playKey) == QStringLiteral("true"));
        if (ytdl && q.hasQueryItem(Utils::ytdlKey))
            *ytdl =
                (q.queryItemValue(Utils::ytdlKey) == QStringLiteral("true"));
        if (app && q.hasQueryItem(Utils::appKey))
            *app = q.queryItemValue(Utils::appKey);
        if (duration && q.hasQueryItem(Utils::durKey))
            *duration = q.queryItemValue(Utils::durKey).toInt();
    }

    return true;
}

bool Utils::isIdValid(const QString &id) { return isIdValid(QUrl{id}); }

bool Utils::isUrlOk(const QUrl &url) const { return Utils::isUrlValid(url); }

bool Utils::isGpodderAvailable() const { return Gpodder::enabled(); }

bool Utils::isIdMic(const QUrl &id) const { return Utils::isUrlMic(id); }

bool Utils::isIdPulse(const QUrl &id) const { return Utils::isUrlPulse(id); }

bool Utils::isIdScreen(const QUrl &id) const { return Utils::isUrlScreen(id); }

bool Utils::isIdUpnp(const QUrl &id) const { return Utils::isUrlUpnp(id); }

bool Utils::isIdRec(const QUrl &id) {
    auto meta = ContentServer::instance()->getMetaForId(id, false);
    if (meta) return meta->album == ContentServer::recAlbumName;
    return false;
}

QString Utils::devNameFromUpnpId(const QUrl &id) {
    auto meta = ContentServer::instance()->getMetaForId(id, false);
    if (meta && meta->itemType == ContentServer::ItemType_Upnp &&
        !meta->upnpDevId.isEmpty())
        return Directory::instance()->deviceNameFromId(meta->upnpDevId);
    return {};
}

QString Utils::urlToPath(const QUrl &url) const { return url.toLocalFile(); }

QUrl Utils::pathToUrl(const QString &path) const {
    return QUrl::fromLocalFile(path);
}

void Utils::setClipboard(const QString &data) const {
    QGuiApplication::clipboard()->setText(data);
}

QString Utils::clipboard() const {
    return QGuiApplication::clipboard()->text();
}

bool Utils::clipboardContainsUrl() const {
    auto text = QGuiApplication::clipboard()->text();
    return Utils::isUrlOk(QUrl{text});
}

QUrl Utils::noresIcon(const QString &id) const {
    return QUrl::fromLocalFile(IconProvider::pathToNoResId(id));
}

int Utils::itemTypeFromUrl(const QUrl &url) const {
    return static_cast<int>(ContentServer::itemTypeFromUrl(url));
}

QString Utils::recUrlFromId(const QUrl &id) {
    if (!id.isEmpty()) {
        if (const auto meta =
                ContentServer::instance()->getMetaForId(id, false);
            meta) {
            return meta->recUrl;
        }
    }
    return {};
}

QString Utils::recDateFromId(const QUrl &id) {
    if (!id.isEmpty()) {
        if (const auto meta =
                ContentServer::instance()->getMetaForId(id, false);
            meta && !meta->recDate.isNull())
            return friendlyDate(meta->recDate);
    }
    return {};
}

QString Utils::friendlyDate(const QDateTime &date) {
    if (date.isValid()) {
        QString date_s;
        auto cdate = QDate::currentDate();
        if (date.date() == cdate)
            date_s = tr("Today");
        else if (date.date() == cdate.addDays(-1))
            date_s = tr("Yesterday");

        auto time_f = QLocale::system().timeFormat(QLocale::NarrowFormat);
        if (date_s.isEmpty()) return date.toString("d MMM yyyy, " + time_f);
        return date_s + ", " + date.toString(time_f);
    }

    return {};
}

QString Utils::parseArtist(const QString &artist) {
    const auto artist_parsed = artist.split(',').first();

    int pos = artist_parsed.indexOf('(');
    if (pos < 2) return artist_parsed;

    return artist_parsed.left(pos - 1).trimmed();
}

QDateTime Utils::parseDate(const QString &date) {
    return QDateTime::fromString(date, Qt::ISODate);
}

bool Utils::isUrlValid(const QUrl &url) {
    if (!url.isValid()) return false;

    if (url.isLocalFile()) return true;

    if ((url.scheme() == QStringLiteral("http") ||
         url.scheme() == QStringLiteral("https") ||
         url.scheme() == QStringLiteral("jupii")) &&
        !url.host().isEmpty())
        return true;

    return false;
}

bool Utils::isIdValid(const QUrl &id) {
    if (!id.isValid()) {
        // qWarning() << "Utils: Id is invalid:" << id.toString();
        return false;
    }

    QUrlQuery q{id};
    QString cookie;
    if (q.hasQueryItem(Utils::cookieKey))
        cookie = q.queryItemValue(Utils::cookieKey);

    return !cookie.isEmpty();
}

bool Utils::isUrlMic(const QUrl &url) {
    return url.scheme() == QStringLiteral("jupii") &&
           url.host() == QStringLiteral("mic");
}

bool Utils::isUrlPulse(const QUrl &url) {
    return url.scheme() == QStringLiteral("jupii") &&
           url.host() == QStringLiteral("pulse");
}

bool Utils::isUrlScreen(const QUrl &url) {
    return url.scheme() == QStringLiteral("jupii") &&
           url.host() == QStringLiteral("screen");
}

bool Utils::isUrlUpnp(const QUrl &url) {
    return url.scheme() == QStringLiteral("jupii") &&
           url.host() == QStringLiteral("upnp");
}

QUrl Utils::urlFromText(const QString &text, const QString &context) {
    if (!context.isEmpty()) {
        // check if text is a relative file path
        QDir dir{context};
        if (dir.exists(text))
            return QUrl::fromLocalFile(dir.absoluteFilePath(text));

        // check if text is a ralative URL
        QUrl url{text};
        if (url.isRelative()) {
            url = QUrl{context}.resolved(url);
            fixUrl(url);
            if (Utils::isUrlValid(url)) return url;
        }
    }

    // check if text is a absolute file path
    auto file = QUrl::fromLocalFile(text);
    if (QFileInfo::exists(file.toLocalFile())) return file;

    // check if text is valid URL
    QUrl url(text);
    fixUrl(url);
    if (Utils::isUrlValid(url)) return url;

    return {};
}

QString Utils::pathFromId(const QString &id) { return pathFromId(QUrl{id}); }

QString Utils::pathFromId(const QUrl &id) {
    if (QString path; Utils::pathTypeNameCookieIconFromId(id, &path)) {
        return path;
    }

    return {};
}

int Utils::typeFromId(const QString &id) { return typeFromId(QUrl{id}); }

int Utils::typeFromId(const QUrl &id) {
    if (int type = 0; Utils::pathTypeNameCookieIconFromId(id, nullptr, &type)) {
        return type;
    }

    return 0;
}

QString Utils::cookieFromId(const QString &id) {
    return cookieFromId(QUrl{id});
}

QString Utils::cookieFromId(const QUrl &id) {
    if (QString cookie; Utils::pathTypeNameCookieIconFromId(
            id, nullptr, nullptr, nullptr, &cookie)) {
        return cookie;
    }

    return {};
}

QString Utils::nameFromId(const QString &id) { return nameFromId(QUrl{id}); }

QString Utils::nameFromId(const QUrl &id) {
    if (QString name;
        Utils::pathTypeNameCookieIconFromId(id, nullptr, nullptr, &name)) {
        return name;
    }

    return {};
}

void Utils::fixUrl(QUrl &url) {
    url.setQuery(QUrlQuery(url));  // don't use QUrlQuery{}
}

QUrl Utils::swapUrlInId(const QUrl &url, const QUrl &id, bool swapCookie,
                        bool swapOrigUrl, bool swapYtdl) {
    QUrlQuery uq{url};
    if (uq.hasQueryItem(Utils::cookieKey))
        uq.removeAllQueryItems(Utils::cookieKey);
    if (uq.hasQueryItem(Utils::typeKey)) uq.removeAllQueryItems(Utils::typeKey);

    QString cookie, type, name, icon, desc, author, album, origUrl, ytdl, dur,
        app;
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
    if (iq.hasQueryItem(Utils::albumKey))
        album = iq.queryItemValue(Utils::albumKey);
    if (swapOrigUrl && iq.hasQueryItem(Utils::origUrlKey))
        origUrl = iq.queryItemValue(Utils::origUrlKey);
    if (swapYtdl && iq.hasQueryItem(Utils::ytdlKey))
        ytdl = iq.queryItemValue(Utils::ytdlKey);
    if (iq.hasQueryItem(Utils::durKey)) dur = iq.queryItemValue(Utils::durKey);
    if (iq.hasQueryItem(Utils::appKey)) app = iq.queryItemValue(Utils::appKey);

    if (!cookie.isEmpty()) uq.addQueryItem(Utils::cookieKey, cookie);
    if (!type.isEmpty()) uq.addQueryItem(Utils::typeKey, type);
    if (!name.isEmpty()) uq.addQueryItem(Utils::nameKey, name);
    if (!icon.isEmpty()) uq.addQueryItem(Utils::iconKey, icon);
    if (!desc.isEmpty()) uq.addQueryItem(Utils::descKey, desc);
    if (!author.isEmpty()) uq.addQueryItem(Utils::authorKey, author);
    if (!album.isEmpty()) uq.addQueryItem(Utils::albumKey, album);
    if (!origUrl.isEmpty()) uq.addQueryItem(Utils::origUrlKey, origUrl);
    if (!ytdl.isEmpty()) uq.addQueryItem(Utils::ytdlKey, ytdl);
    if (!dur.isEmpty()) uq.addQueryItem(Utils::durKey, dur);
    if (!app.isEmpty()) uq.addQueryItem(Utils::appKey, app);

    QUrl newUrl{url};
    newUrl.setQuery(uq);

    return newUrl;
}

QString Utils::idFromUrl(const QUrl &url, const QString &cookie) {
    QUrlQuery q{url};
    if (q.hasQueryItem(Utils::cookieKey))
        q.removeAllQueryItems(Utils::cookieKey);
    q.addQueryItem(Utils::cookieKey, cookie);
    QUrl id{url};
    id.setQuery(q);
    return id.toString();
}

QUrl Utils::cleanId(const QUrl &id) {
    QUrlQuery q{id};
    if (q.hasQueryItem(Utils::playKey)) {
        q.removeAllQueryItems(Utils::playKey);
        QUrl url{id};
        url.setQuery(q);
        return url;
    }
    return id;
}

QUrl Utils::urlFromId(const QUrl &id) { return urlsFromId(id).first; }

QUrl Utils::urlFromId(const QString &id) { return urlFromId(QUrl{id}); }

std::pair<QUrl, QUrl> Utils::urlsFromId(const QUrl &id) {
    std::pair<QUrl, QUrl> urls{id, {}};
    QUrlQuery q{id};
    if (q.hasQueryItem(Utils::cookieKey))
        q.removeAllQueryItems(Utils::cookieKey);
    if (q.hasQueryItem(Utils::typeKey)) q.removeAllQueryItems(Utils::typeKey);
    if (q.hasQueryItem(Utils::nameKey)) q.removeAllQueryItems(Utils::nameKey);
    if (q.hasQueryItem(Utils::iconKey)) q.removeAllQueryItems(Utils::iconKey);
    if (q.hasQueryItem(Utils::descKey)) q.removeAllQueryItems(Utils::descKey);
    if (q.hasQueryItem(Utils::authorKey))
        q.removeAllQueryItems(Utils::authorKey);
    if (q.hasQueryItem(Utils::albumKey)) q.removeAllQueryItems(Utils::albumKey);
    if (q.hasQueryItem(Utils::playKey)) q.removeAllQueryItems(Utils::playKey);
    if (q.hasQueryItem(Utils::origUrlKey)) {
        urls.second = QUrl{q.queryItemValue(Utils::origUrlKey)};
        q.removeAllQueryItems(Utils::origUrlKey);
    }
    if (q.hasQueryItem(Utils::ytdlKey)) q.removeAllQueryItems(Utils::ytdlKey);
    if (q.hasQueryItem(Utils::appKey)) q.removeAllQueryItems(Utils::appKey);
    if (q.hasQueryItem(Utils::durKey)) q.removeAllQueryItems(Utils::durKey);

    urls.first.setQuery(q);

    return urls;
}

std::pair<QUrl, QUrl> Utils::urlsFromId(const QString &id) {
    return urlsFromId(QUrl{id});
}

QString Utils::anonymizedUrl(const QUrl &url) {
    auto path = url.path();
    auto query = url.query();
    return url.scheme() + "://" + url.authority() +
           (path.isEmpty()     ? QString{}
            : path.size() < 14 ? '/' + path
                               : "/..." + (path.right(10))) +
           (query.isEmpty()     ? QString{}
            : query.size() < 14 ? query
                                : "?..." + query.right(10));
}

QString Utils::anonymizedId(const QUrl &id) {
    return swapUrlInId(QUrl{anonymizedUrl(urlFromId(id))}, id).toString();
}

QString Utils::anonymizedId(const QString &id) {
    return anonymizedId(QUrl{id});
}

QUrl Utils::bestUrlFromId(const QUrl &id) {
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
    if (q.hasQueryItem(Utils::playKey)) q.removeAllQueryItems(Utils::playKey);
    /*if (q.hasQueryItem(Utils::ytdlKey))
        q.removeAllQueryItems(Utils::ytdlKey);*/
    url.setQuery(q);
    return url;
}

QString Utils::randString(int len) {
#if QT_VERSION < QT_VERSION_CHECK(5, 10, 0)
    auto *t = QThread::currentThread();
    if (!m_seedDone.contains(t)) {
        // Seed init, needed for key generation
        qsrand(QTime::currentTime().msec());
        m_seedDone[t] = true;
    }
#endif

    static const auto pc = QStringLiteral(
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789");

    QString rs;
    for (int i = 0; i < len; ++i) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
        rs.push_back(
            pc.at(QRandomGenerator::global()->bounded(pc.length() - 1)));
#else
        rs.push_back(pc.at(qrand() % pc.length()));
#endif
    }

    return rs;
}

void Utils::removeFile(const QString &path) {
    QFile file{path};
    if (file.exists()) file.remove();
}

QString Utils::dirNameFromPath(const QString &path) const {
    QDir dir{path};
    dir.makeAbsolute();
    return dir.dirName();
}

QString Utils::deviceIconFileName(QString id, const QString &ext) {
    return "device_image_" + id.replace(':', '-') + "." + ext;
}

QString Utils::deviceIconFilePath(const QString &id) {
    if (auto path =
            cachePathForFile(deviceIconFileName(id, QStringLiteral("jpg")))) {
        return *path;
    }
    if (auto path =
            cachePathForFile(deviceIconFileName(id, QStringLiteral("png")))) {
        return *path;
    }
    return {};
}

QString Utils::escapeName(const QString &filename) {
    QString escapedName{filename};
    escapedName.remove(QRegExp(
        QStringLiteral("\\[[^\\]]*\\]|\\([^\\)]*\\)|\\{[^\\}]*\\}|<[^>}]*>|"
                       "[_!@#\\$\\^&\\*\\+=\\|\\\\/\"'\\?~`]+"),
        Qt::CaseInsensitive));
    escapedName.replace(QRegExp(QStringLiteral("\\s\\s"), Qt::CaseInsensitive),
                        QStringLiteral(" "));
    escapedName = escapedName.trimmed()
                      .normalized(QString::NormalizationForm_KD)
                      .toLower();
    return escapedName;
}

#ifdef USE_SFOS
void Utils::setQmlRootItem(QQuickItem *rootItem) { m_qmlRootItem = rootItem; }

void Utils::activateWindow() {
    if (m_qmlRootItem) {
        QMetaObject::invokeMethod(m_qmlRootItem, "activate");
    }
}
#endif

QString Utils::humanFriendlySizeString(int64_t size) {
    if (size == 0) return QStringLiteral("0");
    if (size < 1024) return QStringLiteral("%1 B").arg(size);
    if (size < 1048576) return QStringLiteral("%1 kB").arg(qFloor(size / 1024));
    if (size < 1073741824)
        return QStringLiteral("%1 MB").arg(qFloor(size / 1048576));
    return QStringLiteral("%1 GB").arg(size / 1073741824);
}

QUrl Utils::slidesUrl(const std::variant<QString, SlidesTime> &var) {
    QUrlQuery query;

    std::visit(
        [&](auto &&arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, QString>) {
                query.addQueryItem(QStringLiteral("playlist"), arg);
            } else if constexpr (std::is_same_v<T, SlidesTime>) {
                query.addQueryItem(QStringLiteral("time"), slidesTimeStr(arg));
            }
        },
        var);

    QUrl url{"jupii://slides"};
    url.setQuery(query);

    return url;
}

QString Utils::slidesTimeStr(SlidesTime time) {
    switch (time) {
        case SlidesTime::Today:
            return QStringLiteral("today");
        case SlidesTime::Last7Days:
            return QStringLiteral("last7days");
        case SlidesTime::Last30Days:
            return QStringLiteral("last30days");
    }

    qWarning() << "invalid slides time";
    return {};
}

QString Utils::slidesTimeName(SlidesTime time) {
    switch (time) {
        case SlidesTime::Today:
            return tr("Today's images");
        case SlidesTime::Last7Days:
            return tr("Images from last 7 days");
        case SlidesTime::Last30Days:
            return tr("Images from last 30 days");
    }

    qWarning() << "invalid slides time";
    return {};
}

std::optional<Utils::SlidesTime> Utils::strToSlidesTime(const QString &str) {
    if (str.compare(QLatin1String{"today"}, Qt::CaseInsensitive) == 0) {
        return SlidesTime::Today;
    }
    if (str.compare(QLatin1String{"last7days"}, Qt::CaseInsensitive) == 0) {
        return SlidesTime::Last7Days;
    }
    if (str.compare(QLatin1String{"last30days"}, Qt::CaseInsensitive) == 0) {
        return SlidesTime::Last30Days;
    }

    qWarning() << "invalid slides time str";
    return std::nullopt;
}

std::vector<std::string> Utils::imagePathsForSlidesTime(SlidesTime time) {
    std::vector<std::string> paths;

    auto timeArg = [time] {
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        auto now = QDate::currentDate().startOfDay();
#else
        auto now = QDateTime{QDate::currentDate()};
#endif
        switch (time) {
            case SlidesTime::Today:
                return now.toUTC().toString(Qt::ISODate);
            case SlidesTime::Last7Days:
                return now.addDays(-7).toUTC().toString(Qt::ISODate);
            case SlidesTime::Last30Days:
                return now.addDays(-30).toUTC().toString(Qt::ISODate);
        }
        return now.toUTC().toString(Qt::ISODate);
    }();

    // get files from tracker and take into account only up to 1000 images
    // larger than 600x600
    const static auto tmpl = QStringLiteral(
        "SELECT nie:url(?file) "
        "WHERE { ?item a nfo:Image; nie:isStoredAs ?file; nfo:width ?width; "
        "nfo:height ?height. ?file nfo:fileLastModified ?date. "
        "FILTER ( ?width > 600 && ?height > 600 && ?date >= "
        "\"%1\"^^xsd:dateTime ). } GROUP BY ?file ORDER BY ASC( ?date ) LIMIT "
        "1000");

    auto *tracker = Tracker::instance();

    if (!tracker->query(tmpl.arg(timeArg), false)) {
        qWarning() << "cannot get tracker data for slides";
        return paths;
    }

    auto res = tracker->getResult();
    TrackerCursor cursor(res.first, res.second);

    if (cursor.columnCount() == 0) {
        qWarning() << "invalid tracker data for slides";
        return paths;
    }

    while (cursor.next()) {
        paths.push_back(
            QUrl{cursor.value(0).toString()}.toLocalFile().toStdString());
    }

    return paths;
}

bool Utils::imageSupportedInSlides(const QString &path) {
    auto ext = QFileInfo{path}.suffix();
    return ext.compare(QLatin1String{"png"}, Qt::CaseInsensitive) == 0 ||
           ext.compare(QLatin1String{"jpg"}, Qt::CaseInsensitive) == 0 ||
           ext.compare(QLatin1String{"jpeg"}, Qt::CaseInsensitive) == 0;
}
