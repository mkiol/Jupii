/* Copyright (C) 2021-2025 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef CONTENTSERVERWORKER_H
#define CONTENTSERVERWORKER_H

#include <qhttprequest.h>
#include <qhttpresponse.h>
#include <qhttpserver.h>

#include <QByteArray>
#include <QFile>
#include <QHash>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QPair>
#include <QString>
#include <QTime>
#include <QTimer>
#include <QUrl>
#include <cstdint>
#include <memory>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "caster.hpp"
#include "contentserver.h"
#include "singleton.h"
#include "thumb.h"

class ContentServerWorker : public QObject,
                            public Singleton<ContentServerWorker> {
    Q_OBJECT
    friend class MicCaster;
    friend class PulseAudioSource;
    friend class AudioCaster;
    friend class ScreenCaster;

   public:
    struct CacheLimit {
        static const int INF_TIME;
        static const int64_t INF_SIZE;
        static const int64_t DEFAULT_DELTA;
        static inline CacheLimit fromType(const ContentServer::Type type,
                                          int delta = DEFAULT_DELTA) {
            if (type == ContentServer::Type::Type_Video)
                return {INF_SIZE, INF_SIZE, 5, delta};
            if (type == ContentServer::Type::Type_Music)
                return {INF_SIZE, INF_SIZE, 2, delta};
            return {INF_SIZE, INF_SIZE, 0, delta};
        }

        int64_t minSize = INF_SIZE;
        int64_t maxSize = INF_SIZE;
        int maxTime = 0;
        int64_t delta = DEFAULT_DELTA;
    };
    struct Range {
        int64_t start;
        int64_t end;
        int64_t length;
        inline bool full() const {
            return start == 0 && (end == -1 || end == length - 1);
        }
        inline auto rangeLength() const { return end - start + 1; }
        inline bool operator==(const Range &rv) const {
            return start == rv.start && end == rv.end;
        }
        void updateLength(int64_t length);
        static std::optional<Range> fromRange(const QString &rangeHeader,
                                              int64_t length = -1);
        static std::optional<Range> fromContentRange(
            const QString &contentRangeHeader);
        friend QDebug operator<<(QDebug debug,
                                 const ContentServerWorker::Range &range);
    };

    static void sendEmptyResponse(QHttpResponse *resp, int code);
    static void sendResponse(QHttpResponse *resp, int code,
                             const QByteArray &data = {});
    static void sendRedirection(QHttpResponse *resp, const QString &location);
    std::shared_ptr<QNetworkAccessManager> nam;
    QHttpServer *server;
    ContentServerWorker(QObject *parent = nullptr);
    void startProxy(const QUrl &id);
    void requestToPreStartCaster(ContentServer::CasterType type,
                                 const QUrl &id);

   signals:
    void streamRecorded(const QString &title, const QString &path);
    void streamToRecordChanged(const QUrl &id, bool value);
    void streamRecordableChanged(const QUrl &id, bool value,
                                 const QString &tmpFile);
    void shoutcastMetadataUpdated(const QUrl &id, const QByteArray &metadata);
    void playbackCaptureNameUpdated(const QUrl &id, const QString &name);
    void casterAudioSourceNameChanged(const QString &name);
    void casterFileStreamStartedInternal(int idx, const QStringList &files);
    void casterFileStreamStarted(const QUrl &id, int idx,
                                 const QStringList &files);
    void itemAdded(const QUrl &id);
    void itemRemoved(const QUrl &id);
    void proxyConnected(const QUrl &id);
    void proxyError(const QUrl &id);
    void proxyRequested(const QUrl &id, bool first,
                        ContentServerWorker::CacheLimit cacheLimit,
                        const QByteArray &range, QHttpRequest *req,
                        QHttpResponse *resp);
    void casterData(ContentServer::CasterType type, QHttpResponse *resp,
                    const QByteArray &data);
    void casterError(ContentServer::CasterError casterErr);
    void casterErrorInternal();
    void preStartCaster(ContentServer::CasterType type, const QUrl &url);
    void hlsTimeout();

   public slots:
    void setStreamToRecord(const QUrl &id, bool value);
    void setDisplayStatus(bool status);
    void setSlidesFiles(const QStringList &paths);
    void slidesSwitch(bool backward);
    void slidesSwitchToIdx(int idx);

   private:
    static const int maxCamBufSize = 10000000;
    static const int casterTimeout = 3000;

    struct Proxy {
        friend class QDebug;
        enum class State { Initial, Streaming, Buffering };
        enum class Mode { Streaming, Playlist };
        enum class CacheReachedType { NotReached, BuffFull, Timeout, All };
        enum class MatchType { Not, Exact, Delta };

        struct Source {
            QNetworkReply *reply = nullptr;
            std::optional<Range> range;
            int metaint = 0;
            int metacounter = 0;
            int metasize = 0;
            int64_t length = -1;
            CacheLimit cacheLimit;
            bool first = false;
            QByteArray metadata;
            CacheReachedType cacheDone = CacheReachedType::NotReached;
            State state = State::Initial;
            QByteArray cache;
            QByteArray cacheWithoutMeta;
            QTime cacheStart = QTime::currentTime();
            QByteArray prevData;
            std::shared_ptr<QFile> recFile;
            inline bool operator==(const Source &s) const {
                return reply == s.reply;
            }
            inline bool operator==(const QNetworkReply *r) const {
                return reply == r;
            }
            inline bool recordable() const {
                return recFile && recFile->isOpen();
            }
            inline bool hasMeta() const { return metaint > 0; }
            inline bool hasHeaders() const {
                return !reply->rawHeaderList().isEmpty();
            }
            inline bool full() const { return !range || range->full(); }
            inline bool hasLength() const { return length > 0; }
            inline bool minCacheReached() const;
            void trimProxyCache();
            void resetCacheTimer();
            CacheReachedType maxCacheReached();
            bool openRecFile();
            std::optional<QString> endRecFile(const Proxy &proxy);
            std::optional<std::pair<QString, QString>> saveRecFile(
                const Proxy &proxy);
        };
        struct Sink {
            QHttpRequest *req = nullptr;
            QHttpResponse *resp = nullptr;
            std::optional<Range> range;
            int64_t sentBytes = 0;
            inline bool operator==(const Sink &s) const {
                return resp == s.resp;
            }
            inline bool operator==(const QHttpResponse *r) const {
                return resp == r;
            }
            inline bool head() const {
                return req->method() == QHttpRequest::HTTP_HEAD;
            }
            inline bool meta() const {
                return req->headers().contains(QStringLiteral("icy-metadata"));
            }
            inline bool fromBegin() const { return !range || range->full(); }
            void write(const QByteArray &data);
            void writeNotSent(const QByteArray &data);
        };

        QUrl id;
        bool connected = false;
        bool seek = false;
        QByteArray metadata;
        bool saveRec = false;
        Mode mode = Mode::Streaming;
        QString title;
        QString author;
        QString album;
        QString recExt;
        QString artPath;
        QUrl origUrl;
        ContentServer::Type otype = ContentServer::Type::Type_Unknown;
        std::unordered_map<QHttpResponse *, Sink> sinks;
        std::unordered_map<QNetworkReply *, Source> sources;
        std::unordered_multimap<QNetworkReply *, QHttpResponse *>
            sourceToSinkMap;
        Proxy &operator=(const Proxy &) = delete;
        ~Proxy();
        void addSink(QHttpRequest *req, QHttpResponse *resp);
        void removeSink(QHttpResponse *resp);
        void removeSinks();
        ContentServerWorker::Proxy::Source &source(const QNetworkReply *reply);
        void addSource(QNetworkReply *reply, bool first, CacheLimit cacheLimit);
        void removeSource(QNetworkReply *reply);
        void removeSources();
        void writeAll(const QNetworkReply *reply, const QByteArray &data);
        void writeAll(QNetworkReply *reply, const QByteArray &data,
                      const QByteArray &dataWithoutMeta);
        void writeNotSent(QNetworkReply *reply, QHttpResponse *resp);
        void writeNotSentAll(QNetworkReply *reply);
        void endAll();
        void endSinks(QNetworkReply *reply);
        void sendEmptyResponseAll(QNetworkReply *reply, int code);
        void sendResponseAll(QNetworkReply *reply, int code,
                             const QByteArray &data = {});
        void sendRedirectionAll(QNetworkReply *reply, const QString &location);
        bool shouldBeRemoved();
        bool sourceShouldBeRemoved(Source &source);
        void removeDeadSources();
        inline bool hasMeta() const {
            return !sources.empty() && sources.cbegin()->second.metaint > 0;
        }
        bool recordable() const;
        bool closeRecFile();
        std::optional<QNetworkReply *> matchSource(QHttpResponse *resp);
        std::optional<QNetworkReply *> unmatchSink(const QHttpResponse *resp);
        std::optional<QNetworkReply *> replyMatched(
            const QHttpResponse *resp) const;
        QList<QHttpResponse *> matchSinks(QNetworkReply *reply);
        void unmatchSource(QNetworkReply *reply);
        bool matchedSourceExists(QNetworkReply *reply) const;
        bool matchedSourceExists(const QByteArray &range);
        bool matchedSinkExists(const QHttpResponse *resp) const;
        static bool sourceMatchesSink(Source &source, Sink &sink);
        static MatchType sourceMatchesRange(Source &source,
                                            std::optional<Range> range);
        bool minCacheReached() const;
        bool maxCacheReached();
        void resetCacheTimer();
        void recordData(QNetworkReply *reply, const QByteArray &data);
        void updateRageLength(const Source &source);
    };
    struct ConnectionItem {
        QUrl id;
        QHttpRequest *req = nullptr;
        QHttpResponse *resp = nullptr;
        size_t ctx = 0;
    };

    struct CasterCtx {
        QUrl url;
        std::unique_ptr<Caster> caster;
        CasterCtx(QUrl url, std::unique_ptr<Caster> &&caster)
            : url{std::move(url)}, caster{std::move(caster)} {}
    };
    std::optional<CasterCtx> m_casterCtx;

    int m_hlsLastSeq = -1;
    QUrl m_hlsUrl;

    QHash<QUrl, Proxy> proxies;
    QList<ConnectionItem> casterItems;
    bool displayStatus = true;
    QTimer proxiesCleanupTimer;
    QTimer casterTimer;
    ThumbDownloadQueue m_thumb_queue;
    QHash<QUrl, std::unordered_set<QHttpResponse *>> m_http_active_resps;

    void streamFile(const QString &path, const QString &mime, QHttpRequest *req,
                    QHttpResponse *resp);
    void streamFileRange(std::shared_ptr<QFile> file, QHttpRequest *req,
                         QHttpResponse *resp);
    void streamFileNoRange(std::shared_ptr<QFile> file, QHttpRequest *req,
                           QHttpResponse *resp);
    void requestHandler(QHttpRequest *req, QHttpResponse *resp);
    void requestForFileHandler(const QUrl &id, ContentServer::ItemMeta *meta,
                               QHttpRequest *req, QHttpResponse *resp);
    void requestForUrlHandler(const QUrl &id,
                              const ContentServer::ItemMeta *meta,
                              QHttpRequest *req, QHttpResponse *resp);
    void requestForCachedContent(const QUrl &id,
                                 const ContentServer::ItemMeta *meta,
                                 const QString &path, ContentServer::Type type,
                                 QHttpRequest *req, QHttpResponse *resp);
    void requestForUrlHandlerFallback(const QUrl &id, const QUrl &origUrl,
                                      QHttpRequest *req, QHttpResponse *resp);
    void requestForCasterHandler(const QUrl &id,
                                 const ContentServer::ItemMeta *meta,
                                 QHttpRequest *req, QHttpResponse *resp);
    void handleHeadRequest(const QUrl &id, const ContentServer::ItemMeta *meta,
                           QHttpRequest *req, QHttpResponse *resp);
    void handleThumbRequest(const QUrl &id, QHttpResponse *resp);
    void handleThumbRequestFile(const QUrl &id, const QString &path,
                                QHttpResponse *resp);
    void handleThumbRequestDownload(QUrl url, QString mime, QByteArray bytes);
    void makeProxy(const QUrl &id, bool first, CacheLimit cacheLimit,
                   const QByteArray &range = {}, QHttpRequest *req = nullptr,
                   QHttpResponse *resp = nullptr);
    QByteArray processShoutcastMetadata(Proxy &proxy, QNetworkReply *reply,
                                        QByteArray data);
    void casterAudioSourceNameChangedHandler(const QString &name);
    void casterDataHandler(ContentServer::CasterType type, QHttpResponse *resp,
                           const QByteArray &data);
    void casterFileStreamStartedHandler(int idx, const QStringList &files);
    static void removePoints(std::vector<std::pair<int, int>> rpoints,
                             QByteArray &data);
    void initRecFile(Proxy &proxy, Proxy::Source &source);
    void openRecFile(Proxy &proxy, Proxy::Source &source);
    void saveRecFile(Proxy &proxy, Proxy::Source &source);
    void saveAllRecFile(Proxy &proxy);
    void endRecFile(Proxy &proxy, Proxy::Source &source);
    void endAllRecFile(Proxy &proxy);
    void closeRecFile(Proxy &proxy);
    void finishRecFile(Proxy &proxy, Proxy::Source &source);
    void finishAllRecFile(Proxy &proxy);
    void handleRespWithProxyMetaData(Proxy &proxy, QHttpRequest *,
                                     QHttpResponse *resp, QNetworkReply *reply);
    void handleRespWithProxyMetaData(Proxy &proxy, QNetworkReply *reply);
    void startProxyInternal(const QUrl &id, bool first, CacheLimit cacheLimit,
                            const QByteArray &range = {},
                            QHttpRequest *req = nullptr,
                            QHttpResponse *resp = nullptr);
    void removeProxy(const QUrl &id);
    void removeProxySource(Proxy &proxy, QNetworkReply *reply);
    void proxyMetaDataChanged();
    void proxyRedirected(const QUrl &url);
    void proxyFinished();
    void proxyReadyRead();
    void responseForCasterDone();
    void casterErrorHandler();
    void responseForUrlDone();
    void seqWriteData(std::shared_ptr<QFile> file, int64_t size,
                      QHttpResponse *resp);
    QNetworkReply *makeRequest(const QUrl &id, const QHttpRequest *req);
    QNetworkReply *makeRequest(const QUrl &id, const QByteArray &range = {});
    void dispatchProxyData(Proxy &proxy, QNetworkReply *reply,
                           QByteArray &data);
    void removeDeadProxies();
    void logProxies() const;
    static void logProxySinks(const Proxy &proxy);
    static void logProxySources(const Proxy &proxy);
    static void logProxySourceToSinks(const Proxy &proxy);
    static QByteArray makeRangeHeader(int64_t start, int64_t end = -1);
    void processNewSource(Proxy &proxy, Proxy::Source &source);
    void checkCachedCondition(Proxy &proxy);
    void requestFullSource(const QUrl &id, int64_t length,
                           ContentServer::Type type);
    void requestAdditionalSource(const QUrl &id, int64_t length,
                                 ContentServer::Type type);
    bool matchedSourceExists(const QUrl &id, const QByteArray &range);
    bool castingForType(ContentServer::CasterType type) const;
    std::optional<Caster::Config> configForCaster(
        ContentServer::CasterType type, const ContentServer::ItemMeta *meta);
    bool initCaster(ContentServer::CasterType type,
                    const ContentServer::ItemMeta *meta);
    std::optional<std::vector<std::string>> cacheHlsSegmentsForCaster(
        const QUrl &url, bool init);
    void casterTimeoutHandler();
    bool preStartCasterHandler(ContentServer::CasterType type, const QUrl &id);
    void hlsTimeoutHandler();
    void handleHttpResponseDone();
    static std::pair<ContentServer::CasterType, QString>
    casterTypeAndMimeFromMeta(const ContentServer::ItemMeta *meta);
};

#endif  // CONTENTSERVERWORKER_H
