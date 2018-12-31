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
#include <QUrl>
#include <QHash>
#include <QVector>
#include <QStringList>
#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QThread>
#include <QMutex>
#include <QIODevice>
#include <QAudioInput>
#include <memory>

#include <qhttpserver.h>
#include <qhttprequest.h>
#include <qhttpresponse.h>

#include "taskexecutor.h"

#ifdef FFMPEG
extern "C" {
#include <libavcodec/avcodec.h>
}
#endif

#ifdef PULSE
extern "C" {
#include <pulse/mainloop.h>
#include <pulse/mainloop-signal.h>
#include <pulse/mainloop-api.h>
#include <pulse/context.h>
#include <pulse/error.h>
#include <pulse/introspect.h>
#include <pulse/subscribe.h>
#include <pulse/stream.h>
#include <pulse/xmalloc.h>
}
#endif

class ContentServerWorker;
class MicDevice;
#ifdef PULSE
class PulseDevice;
#endif

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

    struct ItemMeta {
        bool valid = false;
        QString trackerId;
        QUrl url;
        QString path;
        QString filename;
        QString title;
        QString mime;
        QString comment;
        QString album;
        QString albumArt;
        QString artist;
        Type type = TypeUnknown;
        bool local = true;
        bool seekSupported = true;
        int duration = 0;
        double bitrate = 0.0;
        double sampleRate = 0.0;
        int channels = 0;
        int64_t size = 0;
    };

    struct PlaylistItemMeta {
        QUrl url;
        QString title;
        int length = 0;
    };

    const static int micSampleRate = 22050;
    const static int micChannelCount = 1;
    const static int micSampleSize = 16;

    //const static int pulseSampleRate = 44100;
    //const static int pulseSampleRate = 22050;
    //const static int pulseChannelCount = 2;
    const static int pulseSampleSize = 16;

    static ContentServer* instance(QObject *parent = nullptr);
    static Type typeFromMime(const QString &mime);
    static QUrl idUrlFromUrl(const QUrl &url, bool* ok = nullptr, bool* isFile = nullptr, bool *isArt = nullptr);
    static QString bestName(const ItemMeta &meta);
    static Type getContentTypeByExtension(const QString &path);
    static Type getContentTypeByExtension(const QUrl &url);
    static PlaylistType playlistTypeFromMime(const QString &mime);
    static PlaylistType playlistTypeFromExtension(const QString &path);
    static QList<PlaylistItemMeta> parsePls(const QByteArray &data, const QString context = QString());
    static QList<PlaylistItemMeta> parseM3u(const QByteArray &data, const QString context = QString());
    static QList<PlaylistItemMeta> parseXspf(const QByteArray &data, const QString context = QString());

    bool getContentUrl(const QString &id, QUrl &url, QString &meta, QString cUrl = "");
    Type getContentType(const QString &path);
    Type getContentType(const QUrl &url);
    QString getContentMime(const QString &path);
    QString getContentMime(const QUrl &url);
    Q_INVOKABLE QStringList getExtensions(int type) const;
    Q_INVOKABLE QString idFromUrl(const QUrl &url) const;
    Q_INVOKABLE QString pathFromUrl(const QUrl &url) const;
    Q_INVOKABLE QString urlFromUrl(const QUrl &url) const;
    const QHash<QUrl, ItemMeta>::const_iterator getMetaCacheIterator(const QUrl &url, bool createNew = true);
    const QHash<QUrl, ItemMeta>::const_iterator getMetaCacheIteratorForId(const QUrl &id, bool createNew = true);
    const QHash<QUrl, ItemMeta>::const_iterator metaCacheIteratorEnd();
    const ItemMeta* getMeta(const QUrl &url, bool createNew = true);
    const ItemMeta* getMetaForId(const QUrl &id, bool createNew = true);
    Q_INVOKABLE QString streamTitle(const QUrl &id) const;

signals:
    void streamTitleChanged(const QUrl &id, const QString &title);

private slots:
    void shoutcastMetadataHandler(const QUrl &id, const QByteArray &metadata);
    void pulseStreamNameHandler(const QUrl &id, const QString &name);
    void itemAddedHandler(const QUrl &id);
    void itemRemovedHandler(const QUrl &id);

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
        QString title;
        int count = 0;
    };

    static ContentServer* m_instance;

    static const QHash<QString,QString> m_imgExtMap;
    static const QHash<QString,QString> m_musicExtMap;
    static const QHash<QString,QString> m_videoExtMap;
    static const QHash<QString,QString> m_playlistExtMap;
    static const QStringList m_m3u_mimes;
    static const QStringList m_pls_mimes;
    static const QStringList m_xspf_mimes;
    static const QString queryTemplate;
    static const QString dlnaOrgOpFlagsSeekBytes;
    static const QString dlnaOrgOpFlagsNoSeek;
    static const QString dlnaOrgCiFlags;
    static const QString audioItemClass;
    static const QString videoItemClass;
    static const QString imageItemClass;
    static const QString playlistItemClass;
    static const QString broadcastItemClass;
    static const QString defaultItemClass;
    static const QByteArray userAgent;
    static const QString artCookie;
    static const qint64 qlen = 100000;
    static const int threadWait = 1;
    static const int maxRedirections = 5;
    static const int httpTimeout = 10000;

    QHash<QUrl, ItemMeta> metaCache; // url => ItemMeta
    QHash<QUrl, StreamData> streams; // id => StreamData
    QMutex metaCacheMutex;
    QString pulseStreamName;

    static QByteArray encrypt(const QByteArray& data);
    static QByteArray decrypt(const QByteArray& data);
    static bool makeUrl(const QString& id, QUrl& url);
    static QString dlnaOrgFlagsForFile();
    static QString dlnaOrgFlagsForStreaming();
    static QString dlnaOrgPnFlags(const QString& mime);
    static QString dlnaContentFeaturesHeader(const QString& mime, bool seek = true, bool flags = true);
    static QString getContentMimeByExtension(const QString &path);
    static QString getContentMimeByExtension(const QUrl &url);
    static QString mimeFromDisposition(const QString &disposition);

    ContentServer(QObject *parent = nullptr);
    bool getContentMeta(const QString &id, const QUrl &url, QString &meta);
    void requestHandler(QHttpRequest *req, QHttpResponse *resp);
    const QHash<QUrl, ItemMeta>::const_iterator makeItemMeta(const QUrl &url);
    const QHash<QUrl, ItemMeta>::const_iterator makeMicItemMeta(const QUrl &url);
#ifdef PULSE
    const QHash<QUrl, ItemMeta>::const_iterator makePulseItemMeta(const QUrl &url);
#endif
    const QHash<QUrl, ItemMeta>::const_iterator makeItemMetaUsingTracker(const QUrl &url);
    const QHash<QUrl, ItemMeta>::const_iterator makeItemMetaUsingTaglib(const QUrl &url);
    const QHash<QUrl, ItemMeta>::const_iterator makeItemMetaUsingHTTPRequest(const QUrl &url,
            std::shared_ptr<QNetworkAccessManager> nam = std::shared_ptr<QNetworkAccessManager>(),
            int counter = 0);
    //const QHash<QUrl, ItemMeta>::const_iterator makeItemMetaUsingExtension(const QUrl &url);
    ItemMeta *makeMetaUsingExtension(const QUrl &url);
    void fillCoverArt(ItemMeta& item);
    //QString makePlaylistForUrl(const QUrl &url);
    void run();
#ifdef FFMPEG
    static bool extractAudio(const QString& path, ContentServer::AvData& data);
    static bool fillAvDataFromCodec(const AVCodecParameters* codec, const QString &videoPath, AvData &data);
#endif
};

class ContentServerWorker :
        public QObject
{
    Q_OBJECT
    friend MicDevice;
#ifdef PULSE
    friend PulseDevice;
#endif
public:
    static ContentServerWorker* instance(QObject *parent = nullptr);
    QHttpServer* server;
    QNetworkAccessManager* nam;

signals:
    void shoutcastMetadataUpdated(const QUrl &id, const QByteArray &metadata);
    void pulseStreamUpdated(const QUrl &id, const QString& name);
    void itemAdded(const QUrl &id);
    void itemRemoved(const QUrl &id);

private slots:
    void proxyMetaDataChanged();
    void proxyRedirected(const QUrl &url);
    void proxyFinished();
    void proxyReadyRead();
    void startMic();
    void stopMic();
    void responseForMicDone();
#ifdef PULSE
    void startPulse();
    void stopPulse();
    void responseForPulseDone();
#endif
    void responseForUrlDone();

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
        //int metasize = 0; // shoutcast metadata size detected
        //QByteArray metadata; // shoutcast metadata readed so far
        QByteArray data;
    };

    struct SimpleProxyItem {
        QUrl id;
        QHttpRequest* req = nullptr;
        QHttpResponse* resp = nullptr;
    };

    static ContentServerWorker* m_instance;

    std::unique_ptr<QAudioInput> micInput;
    std::unique_ptr<MicDevice> micDev;
#ifdef PULSE
    std::unique_ptr<PulseDevice> pulseDev;
#endif

    QHash<QNetworkReply*, ProxyItem> proxyItems;
    QHash<QHttpResponse*, QNetworkReply*> responseToReplyMap;
    QList<SimpleProxyItem> micItems;
    QList<SimpleProxyItem> pulseItems;

    ContentServerWorker(QObject *parent = nullptr);
    void streamFile(const QString& path, const QString &mime, QHttpRequest *req, QHttpResponse *resp);
    bool seqWriteData(QFile &file, qint64 size, QHttpResponse *resp);
    void requestHandler(QHttpRequest *req, QHttpResponse *resp);
    void requestForFileHandler(const QUrl &id, const ContentServer::ItemMeta *meta, QHttpRequest *req, QHttpResponse *resp);
    void requestForUrlHandler(const QUrl &id, const ContentServer::ItemMeta *meta, QHttpRequest *req, QHttpResponse *resp);
    void requestForMicHandler(const QUrl &id, const ContentServer::ItemMeta *meta, QHttpRequest *req, QHttpResponse *resp);
    void requestForPulseHandler(const QUrl &id, const ContentServer::ItemMeta *meta, QHttpRequest *req, QHttpResponse *resp);
    void sendEmptyResponse(QHttpResponse *resp, int code);
    void sendResponse(QHttpResponse *resp, int code, const QByteArray &data = QByteArray());
    void sendRedirection(QHttpResponse *resp, const QString &location);
    void processShoutcastMetadata(QByteArray &data, ProxyItem &item);
    void updatePulseStreamName(const QString& name);
    void writePulseData(const char* data, size_t maxSize);
};

class MicDevice : public QIODevice
{
    Q_OBJECT
public:
    MicDevice(QObject *parent = nullptr);
    void setActive(bool value);
    bool isActive();

protected:
    qint64 readData(char *data, qint64 maxSize);
    qint64 writeData(const char *data, qint64 maxSize);

private:
    bool active = false;
};

#ifdef PULSE
class PulseDevice : public QObject
{
    Q_OBJECT
public:
    struct SinkInput {
        uint32_t idx = PA_INVALID_INDEX;
        uint32_t clientIdx = PA_INVALID_INDEX;
        QString name;
        bool corked = false;
    };
    struct Client {
        uint32_t idx = PA_INVALID_INDEX;
        QString name;
        QString binary;
        QString icon;
    };

    const static int timerDelta;

    static pa_sample_spec sampleSpec;
    static bool timerActive;
    static bool muted;
    static pa_stream* stream;
    static uint32_t connectedSinkInput;
    static pa_mainloop* ml;
    static pa_mainloop_api* mla;
    static pa_context *ctx;
    static QHash<uint32_t, PulseDevice::Client> clients;
    static QHash<uint32_t, PulseDevice::SinkInput> sinkInputs;
    //static void sinkInfoCallback(pa_context *ctx, const pa_sink_info *i, int eol, void *userdata);
    static void subscriptionCallback(pa_context *ctx, pa_subscription_event_type_t t, uint32_t idx, void *userdata);
    static void stateCallback(pa_context *ctx, void *userdata);
    static void successSubscribeCallback(pa_context *ctx, int success, void *userdata);
    static void streamRequestCallback(pa_stream *stream, size_t nbytes, void *userdata);
    static bool startRecordStream(pa_context *ctx, uint32_t si, const Client& client);
    static void stopRecordStream();
    static void exitSignalCallback(pa_mainloop_api *mla, pa_signal_event *e, int sig, void *userdata);
    static void sinkInputInfoCallback(pa_context *ctx, const pa_sink_input_info *i, int eol, void *userdata);
    static void clientInfoCallback(pa_context *ctx, const pa_client_info *i, int eol, void *userdata);
    static void timeEventCallback(pa_mainloop_api *mla, pa_time_event *e, const struct timeval *tv, void *userdata);
    static void discoverStream();
    static bool setupContext();
    static bool startTimer();
    static void stopTimer();
    static void restartTimer(pa_time_event *e, const struct timeval *tv);
    static void muteConnectedSinkInput();
    static void unmuteConnectedSinkInput();
    static bool isBlacklisted(const char* name);
    static void correctClientName(Client &client);
    static QString subscriptionEventToStr(pa_subscription_event_type_t t);
    static QList<PulseDevice::Client> activeClients();
    static bool isInited();

    PulseDevice(QObject *parent = nullptr);
};
#endif

#endif // CONTENTSERVER_H
