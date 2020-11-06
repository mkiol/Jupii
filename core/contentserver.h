/* Copyright (C) 2017 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef CONTENTSERVER_H
#define CONTENTSERVER_H

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QUrl>
#include <QHash>
#include <QMutex>
#include <QThread>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QFile>
#include <QIODevice>
#include <QTime>
#include <QDateTime>
#include <memory>
#include <qhttpserver.h>
#include <qhttprequest.h>
#include <qhttpresponse.h>
#include "libupnpp/control/cdirectory.hxx"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/dict.h>
#include <libavutil/mathematics.h>
#include <libavdevice/avdevice.h>
#include <libavutil/imgutils.h>
#include <libavutil/timestamp.h>
#include <libavutil/time.h>
#include <libavutil/log.h>
}

#include "taskexecutor.h"
#include "pulseaudiosource.h"
#include "audiocaster.h"
#include "miccaster.h"
#ifdef SCREENCAST
#include "screencaster.h"
#endif

class ContentServerWorker;

class ContentServer :
        public QThread
{
friend class ContentServerWorker;
    Q_OBJECT
public:
    enum Type {
        TypeUnknown = 0,
        TypeImage = 1,
        TypeMusic = 2,
        TypeVideo = 4,
        TypeDir = 128,
        TypePlaylist = 256
    };

    enum PlaylistType {
        PlaylistUnknown,
        PlaylistPLS,
        PlaylistM3U,
        PlaylistXSPF
    };

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

    enum MetaFlag {
        MetaFlag_Unknown        = 0,
        MetaFlag_Valid          = 1 << 0,
        MetaFlag_IceCast        = 1 << 1,
        MetaFlag_Local          = 1 << 2,
        MetaFlag_YtDl           = 1 << 3,
        MetaFlag_OrigUrl        = 1 << 4,
        MetaFlag_Seek           = 1 << 5,
        MetaFlag_PlaylistProxy  = 1 << 6,
        MetaFlag_Refresh        = 1 << 7,
        MetaFlag_Art            = 1 << 8
    };

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
        Type type = TypeUnknown;
        ItemType itemType = ItemType_Unknown;
        int duration = 0;
        double bitrate = 0.0;
        double sampleRate = 0.0;
        int channels = 0;
        int64_t size = 0;
        QString recUrl;
        QDateTime recDate;
        QTime metaUpdateTime = QTime::currentTime();
        QString app;
        int flags = 0;

        inline bool flagSet(MetaFlag flag) const {
            return (this->flags & flag) != 0;
        }

        inline void setFlags(int flags, bool set = true) {
            if (set)
                this->flags |= flags;
            else
                this->flags &= ~flags;
        }

        inline bool expired() const {
            if (metaUpdateTime.isNull())
                return true;
            if (flagSet(MetaFlag_YtDl)) // ytdl meta is expired after 60s
                return metaUpdateTime.addSecs(60) < QTime::currentTime();
            return metaUpdateTime.addSecs(180) < QTime::currentTime();
        }

        inline bool dummy() const {
            return flagSet(MetaFlag_YtDl) ? metaUpdateTime.isNull() : false;
        }

        ItemMeta(const ItemMeta *meta);
        ItemMeta();
    };

    struct PlaylistItemMeta {
        QUrl url;
        QString title;
        int length = 0;
    };

    static const QString artCookie;

    static const QString recDateTagName;
    static const QString recUrlTagName;
    static const QString recUrlTagName2;
    static const QString recAlbumName;

    static ContentServer* instance(QObject *parent = nullptr);
    static Type typeFromMime(const QString &mime);
    static QString extFromMime(const QString &mime);
    static Type typeFromUpnpClass(const QString &upnpClass);
    static QString bestName(const ItemMeta &meta);
    static Type getContentTypeByExtension(const QString &path);
    static Type getContentTypeByExtension(const QUrl &url);
    static PlaylistType playlistTypeFromMime(const QString &mime);
    static PlaylistType playlistTypeFromExtension(const QString &path);
    static QList<PlaylistItemMeta> parsePls(const QByteArray &data, const QString context = QString());
    static QList<PlaylistItemMeta> parseM3u(const QByteArray &data, const QString context = QString());
    static QList<PlaylistItemMeta> parseXspf(const QByteArray &data, const QString context = QString());
    static void resolveM3u(QByteArray &data, const QString context);
    static QString streamTitleFromShoutcastMetadata(const QByteArray &metadata);
    static bool writeMetaUsingTaglib(const QString& path, const QString& title,
                                      const QString& artist = QString(),
                                      const QString& album = QString(),
                                      const QString& comment = QString(),
                                      const QString& recUrl = QString(),
                                      const QDateTime& recDate = QDateTime(),
                                      const QString& artPath = QString());
    static bool readMetaUsingTaglib(const QString& path,
                                    QString& title,
                                    QString& artist,
                                    QString& album,
                                    QString& comment,
                                    QString& recUrl,
                                    QDateTime& recDate,
                                    QString& artPath,
                                    int *duration = nullptr,
                                    double *bitrate = nullptr,
                                    double *sampleRate = nullptr,
                                    int *channels = nullptr);
    static QString minResUrl(const UPnPClient::UPnPDirObject &item);
    static ItemType itemTypeFromUrl(const QUrl &url);

    void registerExternalConnections();
    bool getContentUrl(const QString &id, QUrl &url, QString &meta, QString cUrl = "");
    Q_INVOKABLE QStringList getExtensions(int type) const;
    Q_INVOKABLE QString idFromUrl(const QUrl &url) const;
    Q_INVOKABLE QString pathFromUrl(const QUrl &url) const;
    Q_INVOKABLE QString urlFromUrl(const QUrl &url) const;
    const QHash<QUrl, ItemMeta>::const_iterator getMetaCacheIterator(const QUrl &url,
                                                                     bool createNew = true,
                                                                     const QUrl &origUrl = QUrl(),
                                                                     const QString &app = QString(),
                                                                     bool ytdl = false, bool img = false,
                                                                     bool refresh = false);
    const QHash<QUrl, ItemMeta>::const_iterator getMetaCacheIteratorForId(const QUrl &id,
                                                                          bool createNew = true);
    const QHash<QUrl, ItemMeta>::const_iterator metaCacheIteratorEnd();
    const ItemMeta* getMeta(const QUrl &url, bool createNew,
                            const QUrl &origUrl = QUrl(), const QString &app = QString(),
                            bool ytdl = false, bool img = false, bool refresh = false);
    const ContentServer::ItemMeta* getMetaForImg(const QUrl &url, bool createNew);
    void removeMeta(const QUrl &url);
    const ItemMeta* getMetaForId(const QUrl &id, bool createNew);
    Q_INVOKABLE QString streamTitle(const QUrl &id) const;
    Q_INVOKABLE QStringList streamTitleHistory(const QUrl &id) const;
    Q_INVOKABLE void setStreamToRecord(const QUrl &id, bool value);
    Q_INVOKABLE bool isStreamToRecord(const QUrl &id);
    Q_INVOKABLE bool isStreamRecordable(const QUrl &id);
    bool getContentMetaItem(const QString &id, QString &meta, bool includeDummy = true);
    bool getContentMetaItemByDidlId(const QString &didlId, QString &meta);
    QUrl idUrlFromUrl(const QUrl &url, bool* ok = nullptr,
                             bool* isFile = nullptr) const;
    bool makeUrl(const QString& id, QUrl& url, bool relay = true);

signals:
    void streamRecordError(const QString& title);
    void streamRecorded(const QString& title, const QString& filename);
    void streamTitleChanged(const QUrl &id, const QString &title);
    void requestStreamToRecord(const QUrl &id, bool value);
    void streamToRecordChanged(const QUrl &id, bool value);
    void streamRecordableChanged(const QUrl &id, bool value);
    void displayStatusChanged(bool status);
    void fullHashesUpdated();

public slots:
    void displayStatusChangeHandler(QString state);

private slots:
    void streamRecordedHandler(const QString& title, const QString& path);
    void streamToRecordChangedHandler(const QUrl &id, bool value);
    void streamRecordableChangedHandler(const QUrl &id, bool value, const QString& tmpFile);
    void shoutcastMetadataHandler(const QUrl &id, const QByteArray &metadata);
    void pulseStreamNameHandler(const QUrl &id, const QString &name);
    void itemAddedHandler(const QUrl &id);
    void itemRemovedHandler(const QUrl &id);
    void cleanTmpRec();

private:
    enum DLNA_ORG_FLAGS {
      DLNA_ORG_FLAG_NONE                       = 0,
      DLNA_ORG_FLAG_SENDER_PACED               = (1 << 31),
      DLNA_ORG_FLAG_TIME_BASED_SEEK            = (1 << 30),
      DLNA_ORG_FLAG_BYTE_BASED_SEEK            = (1 << 29),
      DLNA_ORG_FLAG_PLAY_CONTAINER             = (1 << 28),
      DLNA_ORG_FLAG_S0_INCREASE                = (1 << 27),
      DLNA_ORG_FLAG_SN_INCREASE                = (1 << 26),
      DLNA_ORG_FLAG_RTSP_PAUSE                 = (1 << 25),
      DLNA_ORG_FLAG_STREAMING_TRANSFER_MODE    = (1 << 24),
      DLNA_ORG_FLAG_INTERACTIVE_TRANSFERT_MODE = (1 << 23),
      DLNA_ORG_FLAG_BACKGROUND_TRANSFER_MODE   = (1 << 22),
      DLNA_ORG_FLAG_CONNECTION_STALL           = (1 << 21),
      DLNA_ORG_FLAG_DLNA_V15                   = (1 << 20)
    };

    struct AvData {
        QString path;
        QString mime;
        QString type;
        QString extension;
        int bitrate;
        int channels;
        int64_t size;
    };

    struct StreamData {
        QUrl id;
        QString title; // current title
        QStringList titleHistory; // current and previous titles
        int count = 0;
    };

    static ContentServer* m_instance;

    static const QHash<QString,QString> m_imgExtMap;
    static const QHash<QString,QString> m_musicExtMap;
    static const QHash<QString,QString> m_musicMimeToExtMap;
    static const QHash<QString,QString> m_imgMimeToExtMap;
    static const QHash<QString,QString> m_videoExtMap;
    static const QHash<QString,QString> m_playlistExtMap;
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
    static const qint64 qlen = 100000;
    static const int threadWait = 1;
    static const int maxRedirections = 5;
    static const int httpTimeout = 20000;
    static const qint64 recMaxSize = 500000000;
    static const qint64 recMinSize = 100000;

    QHash<QUrl, ItemMeta> metaCache; // url => ItemMeta
    QHash<QString, QString> metaIdx; // DIDL-lite id => id
    QHash<QUrl, StreamData> streams; // id => StreamData
    QHash<QString, QString> fullHashes; // truncated hash => full hash
    QHash<QUrl, QString> tmpRecs; // id => tmp rec file
    QMutex metaCacheMutex;
    QString pulseStreamName;

    static QByteArray encrypt(const QByteArray& data);
    static QByteArray decrypt(const QByteArray& data);
    static QString dlnaOrgFlagsForFile();
    static QString dlnaOrgFlagsForStreaming();
    static QString dlnaOrgPnFlags(const QString& mime);
    static QString dlnaContentFeaturesHeader(const QString& mime, bool seek = true,
                                             bool flags = true);
    static QString getContentMimeByExtension(const QString &path);
    static QString getContentMimeByExtension(const QUrl &url);
    static QString getExtensionFromAudioContentType(const QString &mime);
    static QString mimeFromDisposition(const QString &disposition);
    static bool hlsPlaylist(const QByteArray &data);
    ContentServer(QObject *parent = nullptr);
    bool getContentMetaItem(const QString &id, const QUrl &url, QString &meta, const ItemMeta* item);
    bool getContentMeta(const QString &id, const QUrl &url, QString &meta, const ItemMeta* item);
    void requestHandler(QHttpRequest *req, QHttpResponse *resp);
    const QHash<QUrl, ItemMeta>::const_iterator makeItemMeta(const QUrl &url,
                                                             const QUrl &origUrl = QUrl(),
                                                             const QString &app = QString(),
                                                             bool ytdl = false,
                                                             bool art = false,
                                                             bool refresh = false);
    const QHash<QUrl, ItemMeta>::const_iterator makeMicItemMeta(const QUrl &url);
    const QHash<QUrl, ItemMeta>::const_iterator makeAudioCaptureItemMeta(const QUrl &url);
    const QHash<QUrl, ItemMeta>::const_iterator makeScreenCaptureItemMeta(const QUrl &url);
    const QHash<QUrl, ItemMeta>::const_iterator makeItemMetaUsingTracker(const QUrl &url);
    const QHash<QUrl, ItemMeta>::const_iterator makeItemMetaUsingTaglib(const QUrl &url);
    const QHash<QUrl, ItemMeta>::const_iterator makeItemMetaUsingHTTPRequest(const QUrl &url,
                                                                             const QUrl &origUrl = QUrl(),
                                                                             const QString &app = QString(),
                                                                             bool ytdl = false,
                                                                             bool refresh = false,
                                                                             bool art = false);
    const QHash<QUrl, ItemMeta>::const_iterator makeItemMetaUsingHTTPRequest2(const QUrl &url,
            ItemMeta &meta,
            std::shared_ptr<QNetworkAccessManager> nam = std::shared_ptr<QNetworkAccessManager>(),
            int counter = 0);
    const QHash<QUrl, ItemMeta>::const_iterator makeItemMetaUsingYoutubeDl(const QUrl &url,
            ItemMeta &meta,
            std::shared_ptr<QNetworkAccessManager> nam = std::shared_ptr<QNetworkAccessManager>(),
            int counter = 0);
    const QHash<QUrl, ItemMeta>::const_iterator makeItemMetaUsingBcApi(const QUrl &url,
            ItemMeta &meta,
            std::shared_ptr<QNetworkAccessManager> nam = std::shared_ptr<QNetworkAccessManager>(),
            int counter = 0);
    const QHash<QUrl, ItemMeta>::const_iterator makeUpnpItemMeta(const QUrl &url);
    const QHash<QUrl, ItemMeta>::const_iterator makeMetaUsingExtension(const QUrl &url);
    void run();
    static bool extractAudio(const QString& path, ContentServer::AvData& data);
    static bool fillAvDataFromCodec(const AVCodecParameters* codec, const QString &videoPath, AvData &data);
    static QString extractItemFromDidl(const QString &didl);
    bool saveTmpRec(const QString &path);
    static QString readTitleUsingTaglib(const QString &path);
    void updateMetaAlbumArt(ItemMeta &meta) const;
};

class ContentServerWorker :
        public QObject
{
    Q_OBJECT
    friend MicCaster;
    friend PulseAudioSource;
    friend AudioCaster;
#ifdef SCREENCAST
    friend ScreenCaster;
#endif
public:
    static ContentServerWorker* instance(QObject *parent = nullptr);
    QNetworkAccessManager* nam;
    QHttpServer* server;
    static void adjustVolume(QByteArray *data, float factor, bool le = true);
    bool isStreamToRecord(const QUrl &id);
    bool isStreamRecordable(const QUrl &id);

signals:
    void streamRecorded(const QString& title, const QString& path);
    void streamToRecordChanged(const QUrl &id, bool value);
    void streamRecordableChanged(const QUrl &id, bool value, const QString& tmpFile);
    void shoutcastMetadataUpdated(const QUrl &id, const QByteArray &metadata);
    void pulseStreamUpdated(const QUrl &id, const QString& name);
    void itemAdded(const QUrl &id);
    void itemRemoved(const QUrl &id);
    void contSeqWriteData(QFile* file, qint64 size, QHttpResponse *resp);

public slots:
    void setStreamToRecord(const QUrl &id, bool value);
    void setDisplayStatus(bool status);

private slots:
    void proxyMetaDataChanged();
    void proxyRedirected(const QUrl &url);
    void proxyFinished();
    void proxyReadyRead();
    //void startMic();
    //void stopMic();
    void responseForMicDone();
    void responseForAudioCaptureDone();
#ifdef SCREENCAST
    void responseForScreenCaptureDone();
    void screenErrorHandler();
#endif
    void audioErrorHandler();
    void micErrorHandler();
    void responseForUrlDone();
    void seqWriteData(QFile* file, qint64 size, QHttpResponse *resp);

private:
    struct ProxyItem {
        QHttpRequest* req = nullptr;
        QHttpResponse* resp = nullptr;
        QNetworkReply* reply = nullptr;
        QUrl id;
        bool seek = false;
        int state = 0;
        bool meta = false; // shoutcast metadata requested by client
        int metaint = 0; // shoutcast metadata interval received from server
        int metacounter = 0; // bytes couter reseted every metaint
        QByteArray data;
        QByteArray metadata;
        // modes:
        // 0 - stream proxy (default)
        // 1 - playlist proxy (e.g. for HLS playlists)
        int mode = 0;
        bool head = false;
        std::shared_ptr<QFile> recFile;
        bool saveRec = false;
        QString title;
        QString author;
        QString recExt;
        QString artPath;
        bool finished = false;
        QUrl origUrl;
        ~ProxyItem();
    };

    struct ConnectionItem {
        QUrl id;
        QHttpRequest* req = nullptr;
        QHttpResponse* resp = nullptr;
    };

    static ContentServerWorker* m_instance;

    std::unique_ptr<MicCaster> micCaster;
    std::unique_ptr<PulseAudioSource> pulseSource;
    std::unique_ptr<AudioCaster> audioCaster;
#ifdef SCREENCAST
    std::unique_ptr<ScreenCaster> screenCaster;
#endif

    QHash<QNetworkReply*, ProxyItem> proxyItems;
    QHash<QHttpResponse*, QNetworkReply*> responseToReplyMap;
    QList<ConnectionItem> micItems;
    QList<ConnectionItem> audioCaptureItems;
    QList<ConnectionItem> screenCaptureItems;
    QMutex proxyItemsMutex;
    bool displayStatus = true;
    float volumeBoost = 1.0f;

    ContentServerWorker(QObject *parent = nullptr);
    void streamFile(const QString& path, const QString &mime, QHttpRequest *req, QHttpResponse *resp);
    void streamFileRange(QFile *file, QHttpRequest *req, QHttpResponse *resp);
    void streamFileNoRange(QFile *file, QHttpRequest *req, QHttpResponse *resp);
    void requestHandler(QHttpRequest *req, QHttpResponse *resp);
    void requestForFileHandler(const QUrl &id, const ContentServer::ItemMeta *meta, QHttpRequest *req, QHttpResponse *resp);
    void requestForUrlHandler(const QUrl &id, const ContentServer::ItemMeta *meta, QHttpRequest *req, QHttpResponse *resp);
    void requestForUrlHandlerFallback(const QUrl &id, const QUrl &origUrl, QHttpRequest *req, QHttpResponse *resp);
    void requestForMicHandler(const QUrl &id, const ContentServer::ItemMeta *meta, QHttpRequest *req, QHttpResponse *resp);
    void requestForAudioCaptureHandler(const QUrl &id, const ContentServer::ItemMeta *meta, QHttpRequest *req, QHttpResponse *resp);
    void requestForScreenCaptureHandler(const QUrl &id, const ContentServer::ItemMeta *meta, QHttpRequest *req, QHttpResponse *resp);
    void sendEmptyResponse(QHttpResponse *resp, int code);
    void sendResponse(QHttpResponse *resp, int code, const QByteArray &data = QByteArray());
    void sendRedirection(QHttpResponse *resp, const QString &location);
    void processShoutcastMetadata(QByteArray &data, ProxyItem &item);
    void updatePulseStreamName(const QString& name);
    void dispatchPulseData(const void *data, int size);
    void sendScreenCaptureData(const void *data, int size);
    void sendAudioCaptureData(const void *data, int size);
    void sendMicData(const void *data, int size);
    static void removePoints(const QList<QPair<int,int>> &rpoints, QByteArray &data);
    void openRecFile(ProxyItem &item);
    void saveRecFile(ProxyItem &item);
    void endRecFile(ProxyItem &item);
    void closeRecFile(ProxyItem &item);
    static void cleanCacheFiles();
};

#endif // CONTENTSERVER_H
