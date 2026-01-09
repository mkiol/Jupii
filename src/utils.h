/* Copyright (C) 2017-2026 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef UTILS_H
#define UTILS_H

#include <QByteArray>
#include <QNetworkAccessManager>
#include <QNetworkInterface>
#include <QObject>
#include <QPair>
#include <QString>
#include <QStringList>
#if QT_VERSION < QT_VERSION_CHECK(5, 10, 0)
#include <QThread>
#endif
#include <memory>
#include <optional>
#include <string>
#include <variant>
#ifdef USE_SFOS
#include <QQuickItem>
#endif

#include "singleton.h"

class Utils : public QObject, public Singleton<Utils> {
    Q_OBJECT
   public:
    enum class SlidesTime { Today, Last7Days, Last30Days };

    static const QString typeKey;
    static const QString cookieKey;
    static const QString nameKey;
    static const QString authorKey;
    static const QString albumKey;
    static const QString origUrlKey;
    static const QString iconKey;
    static const QString descKey;
    static const QString playKey;
    static const QString idKey;
    static const QString ytdlKey;
    static const QString appKey;
    static const QString durKey;

    std::unique_ptr<QNetworkAccessManager> nam;

    Utils(QObject *parent = nullptr);
    static bool ethNetworkInf(const QNetworkInterface &interface);
    static bool localNetworkInf(const QNetworkInterface &interface);
    static bool wlanNetworkInf(const QNetworkInterface &interface);
    Q_INVOKABLE QStringList networkInfs(bool onlyUp = true);
    Q_INVOKABLE int prefNetworkInfIndex();
    Q_INVOKABLE void setPrefNetworkInfIndex(int idx) const;
    static QString escapeName(const QString &filename);
    static QString anonymizedUrl(const QUrl &url);
    static QString anonymizedId(const QUrl &id);
    static QString anonymizedId(const QString &id);

#ifdef USE_SFOS
    void setQmlRootItem(QQuickItem *rootItem);
    void activateWindow();
#endif
    static QString secToStrStatic(int value);
    static int strToSecStatic(const QString &value);
    Q_INVOKABLE inline QString secToStr(int value) const {
        return secToStrStatic(value);
    }
    Q_INVOKABLE bool isUrlOk(const QUrl &url) const;
    Q_INVOKABLE bool isGpodderAvailable() const;
    Q_INVOKABLE bool isIdMic(const QUrl &id) const;
    Q_INVOKABLE bool isIdPulse(const QUrl &id) const;
    Q_INVOKABLE bool isIdScreen(const QUrl &id) const;
    Q_INVOKABLE bool isIdUpnp(const QUrl &id) const;
    Q_INVOKABLE bool isIdRec(const QUrl &id);
    Q_INVOKABLE int itemTypeFromUrl(const QUrl &url) const;
    Q_INVOKABLE QString devNameFromUpnpId(const QUrl &id);
    Q_INVOKABLE QString readLicenseFile() const;
    Q_INVOKABLE QString recUrlFromId(const QUrl &id);
    Q_INVOKABLE QString recDateFromId(const QUrl &id);
    Q_INVOKABLE QString dirNameFromPath(const QString &path) const;
    Q_INVOKABLE QString urlToPath(const QUrl &url) const;
    Q_INVOKABLE QUrl pathToUrl(const QString &path) const;
    Q_INVOKABLE void setClipboard(const QString &data) const;
    Q_INVOKABLE QString clipboard() const;
    Q_INVOKABLE bool clipboardContainsUrl() const;
    Q_INVOKABLE QUrl noresIcon(const QString &id) const;

    static QString hash(const QString &value);
    static bool isIdValid(const QString &id);
    static bool isUrlValid(const QUrl &url);
    static bool isIdValid(const QUrl &id);
    static bool isUrlMic(const QUrl &url);
    static bool isUrlPulse(const QUrl &url);
    static bool isUrlScreen(const QUrl &url);
    static bool isUrlUpnp(const QUrl &url);
    static QUrl urlFromText(const QString &text,
                            const QString &context = QString());
    static QString pathFromId(const QString &id);
    static QString pathFromId(const QUrl &id);
    static int typeFromId(const QString &id);
    static int typeFromId(const QUrl &id);
    static QString cookieFromId(const QString &id);
    static QString cookieFromId(const QUrl &id);
    static QString nameFromId(const QString &id);
    static QString nameFromId(const QUrl &id);
    static QString idFromUrl(const QUrl &url, const QString &cookie);
    static QUrl swapUrlInId(const QUrl &url, const QUrl &id,
                            bool swapCookie = true, bool swapOrigUrl = true,
                            bool swapYtdl = true);
    static void fixUrl(QUrl &url);
    static QUrl urlFromId(const QUrl &id);
    static QUrl urlFromId(const QString &id);
    static std::pair<QUrl, QUrl> urlsFromId(const QUrl &id);
    static std::pair<QUrl, QUrl> urlsFromId(const QString &id);
    static QUrl bestUrlFromId(const QUrl &id);
    static QUrl cleanId(const QUrl &id);
    static QUrl iconFromId(const QUrl &id);
    static bool pathTypeNameCookieIconFromId(
        const QUrl &id, QString *path = nullptr, int *type = nullptr,
        QString *name = nullptr, QString *cookie = nullptr,
        QUrl *icon = nullptr, QString *desc = nullptr,
        QString *author = nullptr, QString *album = nullptr,
        QUrl *origUrl = nullptr, bool *ytdl = nullptr, bool *play = nullptr,
        QString *app = nullptr, int *duration = nullptr);
    QString randString(int len = 5);
    static void removeFile(const QString &path);
    static std::optional<QString> writeToCacheFile(const QString &filename,
                                                   const QByteArray &data,
                                                   bool del = false);
    static bool writeToFile(const QString &path, const QByteArray &data,
                            bool del = false);
    static void removeFromCacheDir(const QStringList &filter);
    static bool readFromCacheFile(const QString &filename, QByteArray &data);
    static QString pathToCacheFile(const QString &filename);
    static bool cacheFileExists(const QString &filename);
    static std::optional<QString> cachePathForFile(const QString &filename);
    static QString friendlyDate(const QDateTime &date);
    static QString parseArtist(const QString &artist);
    static QDateTime parseDate(const QString &date);
    static QString deviceIconFileName(QString id, const QString &ext);
    static QString deviceIconFilePath(const QString &id);
    static bool createPlaylistDir();
    static QString humanFriendlySizeString(int64_t size);
    static QUrl slidesUrl(const std::variant<QString, SlidesTime> &var);
    static QString slidesTimeStr(SlidesTime time);
    static QString slidesTimeName(SlidesTime time);
    static std::optional<SlidesTime> strToSlidesTime(const QString &str);
    static std::vector<std::string> imagePathsForSlidesTime(SlidesTime time);
    static bool imageSupportedInSlides(const QString &path);

   private:
#if QT_VERSION < QT_VERSION_CHECK(5, 10, 0)
    QHash<QThread *, bool> m_seedDone;
#endif
    QStringList m_lastNetIfs;
#ifdef USE_SFOS
    qint32 m_notifId = 0;
    // Notification notif;
    QQuickItem *m_qmlRootItem = nullptr;
#endif
};

#endif  // UTILS_H
