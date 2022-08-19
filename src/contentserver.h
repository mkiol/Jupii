/* Copyright (C) 2017-2022 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef CONTENTSERVER_H
#define CONTENTSERVER_H

#include <qhttprequest.h>
#include <qhttpresponse.h>

#include <QByteArray>
#include <QDateTime>
#include <QHash>
#include <QMutex>
#include <QNetworkAccessManager>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QThread>
#include <QTime>
#include <QUrl>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>

#include "downloader.h"
#include "singleton.h"

extern "C" {
#include <libavcodec/avcodec.h>
}

namespace UPnPClient {
class UPnPDirObject;
}

namespace TagLib {
class FileRef;
}

class ContentServerWorker;

struct AvMeta {
    QString path;
    QString mime;
    QString type;
    QString extension;
    int64_t bitrate = 0;
    int channels = 0;
    int64_t size = 0;
};

class ContentServer : public QThread, public Singleton<ContentServer> {
    Q_OBJECT
    friend class ContentServerWorker;

    Q_PROPERTY(bool caching READ caching NOTIFY cachingChanged)
    Q_PROPERTY(QString cacheProgressString READ cacheProgressString NOTIFY
                   cacheProgressChanged)

   public:
    enum class Type : unsigned int {
        Type_Unknown = 0,
        Type_Image = 1,
        Type_Music = 2,
        Type_Video = 4,
        Type_Dir = 128,
        Type_Playlist = 256,
        Type_Invalid = std::numeric_limits<unsigned int>::max(),
    };
    Q_ENUM(Type)

    enum class PlaylistType { Unknown, PLS, M3U, XSPF };

    enum ItemType {
        ItemType_Unknown = 0,
        ItemType_LocalFile,
        ItemType_QrcFile,
        ItemType_Url,
        ItemType_AudioCapture,
        ItemType_ScreenCapture,
        ItemType_Mic,
        ItemType_Upnp
    };
    Q_ENUM(ItemType)

    enum class MetaFlag {
        Unknown = 0,
        Valid = 1 << 0,
        IceCast = 1 << 1,
        Local = 1 << 2,
        YtDl = 1 << 3,
        OrigUrl = 1 << 4,
        Seek = 1 << 5,
        PlaylistProxy = 1 << 6,
        Refresh = 1 << 7,
        Art = 1 << 8,
        Mp4AudioNotIsom = 1 << 9,
        MadeFromCache = 1 << 10,
        NiceAlbumArt = 1 << 11
    };

    enum class CachingResult {
        Invalid,
        Cached,
        NotCached,
        NotCachedError,
        NotCachedCanceled
    };
    enum class ProxyError { NoError, Canceled, Error };

    struct ItemMeta {
        QString trackerId;
        QUrl url;
        QUrl origUrl;
        QString path;
        QString filename;
        QString title;
        QString mime;
        QString comment;
        QString album;
        QString albumArt;
        QString artist;
        QString didl;
        QString upnpDevId;
        std::optional<AvMeta> audioAvMeta;
        Type type{Type::Type_Unknown};
        ItemType itemType{ItemType_Unknown};
        int duration = 0;
        double bitrate = 0.0;
        double sampleRate = 0.0;
        int channels = 0;
        int64_t size = 0;
        QString recUrl;
        QDateTime recDate;
        QTime metaUpdateTime{QTime::currentTime()};
        QString app;
        int flags = 0;

        inline bool flagSet(MetaFlag flag) const {
            return (this->flags & static_cast<int>(flag)) != 0;
        }

        inline void setFlags(int flags, bool set = true) {
            if (set)
                this->flags |= flags;
            else
                this->flags &= ~flags;
        }

        inline void setFlags(MetaFlag flag, bool set = true) {
            if (set)
                this->flags |= static_cast<int>(flag);
            else
                this->flags &= ~static_cast<int>(flag);
        }

        inline bool expired() const {
            if (metaUpdateTime.isNull()) return true;
            if (flagSet(MetaFlag::YtDl)) {  // ytdl meta is expired after 60s
                return metaUpdateTime.addSecs(60) < QTime::currentTime();
            }
            return metaUpdateTime.addSecs(180) < QTime::currentTime();
        }

        inline bool dummy() const {
            return flagSet(MetaFlag::YtDl) ? metaUpdateTime.isNull() : false;
        }

        explicit ItemMeta(const ItemMeta *meta);
        ItemMeta();
    };

    static const QString artCookie;

    static const QString recDateTagName;
    static const QString recUrlTagName;
    static const QString recUrlTagName2;
    static const QString recAlbumName;
    static const QString typeTagName;
    static const char *const recMp4TagPrefix;

    static Type typeFromMime(const QString &mime);
    static QString extFromMime(const QString &mime);
    static Type typeFromUpnpClass(const QString &upnpClass);
    static QString bestName(const ItemMeta &meta);
    static Type getContentTypeByExtension(const QString &path);
    static Type getContentTypeByExtension(const QUrl &url);
    static PlaylistType playlistTypeFromMime(const QString &mime);
    static PlaylistType playlistTypeFromExtension(const QString &path);
    static QString streamTitleFromShoutcastMetadata(const QByteArray &metadata);
    static bool writeID3MetaUsingTaglib(
        TagLib::FileRef &file, bool removeExistingTags, const QString &title,
        const QString &artist, const QString &album, const QString &comment,
        const QString &recUrl, const QDateTime &recDate, const QString &artPath,
        Type otype = Type::Type_Unknown);
    static bool writeMP4MetaUsingTaglib(
        TagLib::FileRef &file, bool removeExistingTags, const QString &title,
        const QString &artist, const QString &album, const QString &comment,
        const QString &recUrl, const QDateTime &recDate, const QString &artPath,
        Type otype = Type::Type_Unknown);
    static bool writeGenericMetaUsingTaglib(
        TagLib::FileRef &file, const QString &title, const QString &artist,
        const QString &album, const QString &comment, const QString &recUrl,
        const QDateTime &recDate, Type otype = Type::Type_Unknown);
    static bool writeMetaUsingTaglib(
        const QString &path, bool removeExistingTags, const QString &title,
        const QString &artist = {}, const QString &album = {},
        const QString &comment = {}, const QString &recUrl = {},
        const QDateTime &recDate = {}, QString artPath = {},
        Type otype = Type::Type_Unknown);
    static bool readID3MetaUsingTaglib(const TagLib::FileRef &file,
                                       QString *recUrl, QDateTime *recDate,
                                       Type *otype,
                                       const QString &albumArtPrefix,
                                       QString *artPath);
    static bool readMP4MetaUsingTaglib(const TagLib::FileRef &file,
                                       QString *recUrl, QDateTime *recDate,
                                       Type *otype,
                                       const QString &albumArtPrefix,
                                       QString *artPath);
    static bool readMetaUsingTaglib(
        const QString &path, QString *title = nullptr,
        QString *artist = nullptr, QString *album = nullptr,
        QString *comment = nullptr, QString *recUrl = nullptr,
        QDateTime *recDate = nullptr, QString *artPath = nullptr,
        Type *otype = nullptr, int *duration = nullptr,
        double *bitrate = nullptr, double *sampleRate = nullptr,
        int *channels = nullptr);
    static Type readOtypeFromCachedFile(const QString &filename);
    static QString minResUrl(const UPnPClient::UPnPDirObject &item);
    static ItemType itemTypeFromUrl(const QUrl &url);
    static QString albumArtCacheName(const QString &path, const QString &ext);
    static QString albumArtCachePath(const QString &path, const QString &ext);
    static QString extractedAudioCachePath(const QString &path,
                                           const QString &ext);
    static QString contentCachePath(const QString &path, const QString &ext);
    static bool pathIsCachedFile(const QString &path);
    static std::optional<QString> pathToCachedContent(
        const ItemMeta &meta, bool mustExists, Type = Type::Type_Unknown);
    static QString localArtPathIfExists(const QString &artPath);
    static QUrl artUrl(const QString &artPath);

    ContentServer(QObject *parent = nullptr);
    void registerExternalConnections() const;
    bool getContentUrl(const QString &id, QUrl &url, QString &meta,
                       const QString &cUrl = {});
    Q_INVOKABLE QStringList getExtensions(int type) const;
    Q_INVOKABLE QString idFromUrl(const QUrl &url) const;
    Q_INVOKABLE QString pathFromUrl(const QUrl &url) const;
    Q_INVOKABLE QString urlFromUrl(const QUrl &url) const;
    static QStringList extensionsForType(Type type);
    QHash<QUrl, ItemMeta>::iterator getMetaCacheIterator(
        const QUrl &url, bool createNew = true, const QUrl &origUrl = {},
        const QString &app = {}, bool ytdl = false, bool img = false,
        bool refresh = false, Type type = Type::Type_Unknown);
    QHash<QUrl, ItemMeta>::iterator getMetaCacheIteratorForId(
        const QUrl &id, bool createNew = true);
    QHash<QUrl, ItemMeta>::iterator metaCacheIteratorEnd();
    ItemMeta *getMeta(const QUrl &url, bool createNew, const QUrl &origUrl,
                      const QString &app, bool ytdl, bool img, bool refresh,
                      Type type);
    ItemMeta *getMetaForImg(const QUrl &url, bool createNew);
    void removeMeta(const QUrl &url);
    bool metaExists(const QUrl &url) const;
    ItemMeta *getMetaForId(const QUrl &id, bool createNew);
    ItemMeta *getMetaForId(const QString &id, bool createNew);
    Q_INVOKABLE QString streamTitle(const QUrl &id) const;
    Q_INVOKABLE QStringList streamTitleHistory(const QUrl &id) const;
    Q_INVOKABLE void setStreamToRecord(const QUrl &id, bool value);
    Q_INVOKABLE bool isStreamToRecord(const QUrl &id) const;
    Q_INVOKABLE bool isStreamRecordable(const QUrl &id) const;
    bool getContentMetaItem(const QString &id, QString &meta,
                            bool includeDummy = true);
    bool getContentMetaItemByDidlId(const QString &didlId, QString &meta);
    std::optional<QUrl> idUrlFromUrl(const QUrl &url,
                                     bool *isFile = nullptr) const;
    bool makeUrl(const QString &id, QUrl &url, bool relay = true);
    ProxyError startProxy(const QUrl &id);
    Q_INVOKABLE void cleanCache();
    Q_INVOKABLE QString cacheSizeString() const;
    void cancelCaching();
    inline auto caching() const { return m_caching; }
    double cacheProgress() const;
    inline auto cacheProgressString() const { return m_cacheProgressString; };
    Q_INVOKABLE bool idCached(const QUrl &id);
    std::pair<CachingResult, CachingResult> makeCache(const QUrl &id1,
                                                      const QUrl &id2);
    static QString mimeFromReply(const QNetworkReply *reply);

   signals:
    void streamRecordError(const QString &title);
    void streamRecorded(const QString &title, const QString &filename);
    void streamTitleChanged(const QUrl &id, const QString &title);
    void requestStreamToRecord(const QUrl &id, bool value);
    void streamToRecordChanged(const QUrl &id, bool value);
    void streamRecordableChanged(const QUrl &id, bool value);
    void streamToRecordRequested(const QUrl &id);
    void streamRecordableRequested(const QUrl &id);
    void displayStatusChanged(bool status);
    void fullHashesUpdated();
    void startProxyRequested(const QUrl &id);
    void cacheSizeChanged();
    void cachingChanged();
    void cacheProgressChanged();

   public slots:
    void displayStatusChangeHandler(const QString &state);

   private:
    enum DLNA_ORG_FLAGS {
        DLNA_ORG_FLAG_NONE = 0U,
        DLNA_ORG_FLAG_SENDER_PACED = (1U << 31),
        DLNA_ORG_FLAG_TIME_BASED_SEEK = (1U << 30),
        DLNA_ORG_FLAG_BYTE_BASED_SEEK = (1U << 29),
        DLNA_ORG_FLAG_PLAY_CONTAINER = (1U << 28),
        DLNA_ORG_FLAG_S0_INCREASE = (1U << 27),
        DLNA_ORG_FLAG_SN_INCREASE = (1U << 26),
        DLNA_ORG_FLAG_RTSP_PAUSE = (1U << 25),
        DLNA_ORG_FLAG_STREAMING_TRANSFER_MODE = (1U << 24),
        DLNA_ORG_FLAG_INTERACTIVE_TRANSFERT_MODE = (1U << 23),
        DLNA_ORG_FLAG_BACKGROUND_TRANSFER_MODE = (1U << 22),
        DLNA_ORG_FLAG_CONNECTION_STALL = (1U << 21),
        DLNA_ORG_FLAG_DLNA_V15 = (1U << 20)
    };

    struct StreamData {
        QUrl id;
        QString title;             // current title
        QStringList titleHistory;  // current and previous titles
        int count = 0;
    };

    static const QHash<QString, QString> m_imgExtMap;
    static const QHash<QString, QString> m_musicExtMap;
    static const QHash<QString, QString> m_musicMimeToExtMap;
    static const QHash<QString, QString> m_imgMimeToExtMap;
    static const QHash<QString, QString> m_videoMimeToExtMap;
    static const QHash<QString, QString> m_videoExtMap;
    static const QHash<QString, QString> m_playlistExtMap;
    static const QStringList m_m3u_mimes;
    static const QStringList m_pls_mimes;
    static const QStringList m_xspf_mimes;
    static const QString queryTemplate;
    static const QString dlnaOrgOpFlagsSeekBytes;
    static const QString dlnaOrgOpFlagsNoSeek;
    static const QString dlnaOrgCiFlags;
    static const QString genericAudioItemClass;
    static const QString genericVideoItemClass;
    static const QString genericImageItemClass;
    static const QString audioItemClass;
    static const QString videoItemClass;
    static const QString imageItemClass;
    static const QString playlistItemClass;
    static const QString broadcastItemClass;
    static const QString defaultItemClass;
    static const QByteArray userAgent;
    static const int threadWait = 1;
    static const int maxRedirections = 6;
    static const int httpTimeout = 20000;
    static const int64_t recMaxSize = 500000000;
    static const int64_t recMinSize = 100000;
    static const int64_t maxSizeForCaching = 50000000;

    QHash<QUrl, ItemMeta> m_metaCache;     // url => ItemMeta
    QHash<QString, QString> m_metaIdx;     // DIDL-lite id => id
    QHash<QUrl, StreamData> m_streams;     // id => StreamData
    QHash<QString, QString> m_fullHashes;  // truncated hash => full hash
    QHash<QUrl, QString> m_tmpRecs;        // id => tmp rec file
    QSet<QUrl> m_streamToRecord;           // id => stream should be recorded
    QSet<QUrl> m_streamRecordable;         // id => stream recordable
    QString m_pulseStreamName;
    QMutex m_metaCacheMutex;
    std::optional<Downloader> m_cacheDownloader;
    QString m_cacheProgressString;
    bool m_caching = false;
    int64_t m_cachePendingSize = 0;
    int64_t m_cacheDoneSize = 0;

    void streamRecordedHandler(const QString &title, const QString &path);
    void streamToRecordChangedHandler(const QUrl &id, bool value);
    void streamRecordableChangedHandler(const QUrl &id, bool value,
                                        const QString &tmpFile);
    void shoutcastMetadataHandler(const QUrl &id, const QByteArray &metadata);
    void pulseStreamNameHandler(const QUrl &id, const QString &name);
    void itemAddedHandler(const QUrl &id);
    void itemRemovedHandler(const QUrl &id);
    void cleanTmpRec();

    static QByteArray encrypt(const QByteArray &data);
    static QByteArray decrypt(const QByteArray &data);
    static QString dlnaOrgFlagsForFile();
    static QString dlnaOrgFlagsForStreaming();
    static QString dlnaOrgPnFlags(const QString &mime);
    static QString dlnaContentFeaturesHeader(const QString &mime,
                                             bool seek = true,
                                             bool flags = true);
    static QString getContentMimeByExtension(const QString &path);
    static QString getContentMimeByExtension(const QUrl &url);
    static QString getExtensionFromAudioContentType(const QString &mime);
    static QString mimeFromDisposition(const QString &disposition);
    static bool hlsPlaylist(const QByteArray &data);
    static QString durationStringFromSec(int duration);
    bool getContentMetaItem(const QString &id, const QUrl &url, QString &meta,
                            ItemMeta *item);
    bool getContentMeta(const QString &id, const QUrl &url, QString &meta,
                        ItemMeta *item);
    void requestHandler(QHttpRequest *req, QHttpResponse *resp);
    QHash<QUrl, ItemMeta>::iterator makeItemMeta(
        const QUrl &url, const QUrl &origUrl = {}, const QString &app = {},
        bool ytdl = false, bool art = false, bool refresh = false,
        Type type = Type::Type_Unknown);
    QHash<QUrl, ItemMeta>::iterator makeMicItemMeta(const QUrl &url);
    QHash<QUrl, ItemMeta>::iterator makeAudioCaptureItemMeta(const QUrl &url);
    QHash<QUrl, ItemMeta>::iterator makeScreenCaptureItemMeta(const QUrl &url);
    QHash<QUrl, ItemMeta>::iterator makeItemMetaUsingTracker(const QUrl &url);
    QHash<QUrl, ItemMeta>::iterator makeItemMetaUsingTaglib(const QUrl &url);
    QHash<QUrl, ItemMeta>::iterator makeItemMetaUsingHTTPRequest(
        const QUrl &url, const QUrl &origUrl = {}, const QString &app = {},
        bool ytdl = false, bool refresh = false, bool art = false,
        Type type = Type::Type_Unknown);
    QHash<QUrl, ItemMeta>::iterator makeItemMetaUsingHTTPRequest2(
        const QUrl &url, ItemMeta &meta,
        std::shared_ptr<QNetworkAccessManager> nam =
            std::shared_ptr<QNetworkAccessManager>(),
        int counter = 0);
    QHash<QUrl, ContentServer::ItemMeta>::iterator makeItemMetaUsingCachedFile(
        const QUrl &url, const QString &cachedFile, ItemMeta &meta);
    QHash<QUrl, ItemMeta>::iterator makeItemMetaUsingYtdlApi(
        QUrl url, ItemMeta &meta,
        std::shared_ptr<QNetworkAccessManager> nam =
            std::shared_ptr<QNetworkAccessManager>(),
        int counter = 0);
    QHash<QUrl, ItemMeta>::iterator makeItemMetaUsingBcApi(
        const QUrl &url, ItemMeta &meta,
        std::shared_ptr<QNetworkAccessManager> nam =
            std::shared_ptr<QNetworkAccessManager>(),
        int counter = 0);
    QHash<QUrl, ItemMeta>::iterator makeItemMetaUsingSoundcloudApi(
        const QUrl &url, ItemMeta &meta,
        std::shared_ptr<QNetworkAccessManager> nam =
            std::shared_ptr<QNetworkAccessManager>(),
        int counter = 0);
    QHash<QUrl, ItemMeta>::iterator makeUpnpItemMeta(const QUrl &url);
    QHash<QUrl, ItemMeta>::iterator makeMetaUsingExtension(const QUrl &url);
    QHash<QUrl, ContentServer::ItemMeta>::iterator makeItemMetaUsingApi(
        const QString &mime, QNetworkReply *reply, ItemMeta &meta,
        std::shared_ptr<QNetworkAccessManager> nam, int counter);
    void run() override;
    static QString extractItemFromDidl(const QString &didl);
    bool saveTmpRec(const QString &path, bool deletePath);
    static QString readTitleUsingTaglib(const QString &path);
    void updateMetaAlbumArt(ItemMeta &meta) const;
    static void updateMetaWithNotStandardHeaders(ItemMeta &meta,
                                                 const QNetworkReply &reply);
    static void updateMetaIfCached(ItemMeta &meta,
                                   Type type = Type::Type_Unknown,
                                   const QString &cachedPath = {});
    std::optional<QString> cacheContent(const ItemMeta &meta);
    static std::optional<AvMeta> transcodeToAudioFile(const QString &path);
    static bool writeTagsWithMeta(const QString &path, const ItemMeta &meta,
                                  const QUrl &id);
    static void writeRecTags(const QString &path);
    void updateCacheProgressString(bool toZero = false);
    void setCachingState(bool state);
    int64_t cachePendingSizeForId(const QUrl &id);
    CachingResult makeCache(const QUrl &id);
    static void saveAlbumArt(QNetworkReply &reply, ItemMeta &meta);
};
#endif  // CONTENTSERVER_H
