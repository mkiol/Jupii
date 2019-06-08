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
#include <QEventLoop>
#include <memory>
#include <cstddef>

#include <qhttpserver.h>
#include <qhttprequest.h>
#include <qhttpresponse.h>

#include "taskexecutor.h"

#ifdef FFMPEG
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
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
class MicSource;
#ifdef PULSE
class PulseAudioSource;
class AudioCaster;
#endif
#ifdef SCREEN
class ScreenSource;
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
        // modes:
        // 0 - stream proxy (default)
        // 1 - playlist proxy (e.g. for HLS playlists)
        bool mode = 0;
    };

    struct PlaylistItemMeta {
        QUrl url;
        QString title;
        int length = 0;
    };

    const static int micSampleRate = 22050;
    const static int micChannelCount = 1;
    const static int micSampleSize = 16;
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
    static void resolveM3u(QByteArray &data, const QString context);
    static QString streamTitleFromShoutcastMetadata(const QByteArray &metadata);

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
    Q_INVOKABLE void setStreamToRecord(const QUrl &id, bool value);
    Q_INVOKABLE bool isStreamToRecord(const QUrl &id);
    Q_INVOKABLE bool isStreamRecordable(const QUrl &id);

signals:
    void streamRecordError(const QString& title);
    void streamRecorded(const QString& title, const QString& filename);
    void streamTitleChanged(const QUrl &id, const QString &title);
    void requestStreamToRecord(const QUrl &id, bool value);
    void streamToRecordChanged(const QUrl &id, bool value);
    void streamRecordableChanged(const QUrl &id, bool value);

private slots:
    void streamRecordedHandler(const QString& title, const QString& path);
    void streamToRecordChangedHandler(const QUrl &id, bool value);
    void streamRecordableChangedHandler(const QUrl &id, bool value);
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
    static const QHash<QString,QString> m_musicMimeToExtMap;
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
    static const qint64 recMaxSize = 500000000;
    static const qint64 recMinSize = 100000;

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
    static QString dlnaContentFeaturesHeader(const QString& mime, bool seek = true,
                                             bool flags = true);
    static QString getContentMimeByExtension(const QString &path);
    static QString getContentMimeByExtension(const QUrl &url);
    static QString getExtensionFromAudioContentType(const QString &mime);
    static QString mimeFromDisposition(const QString &disposition);
    static bool hlsPlaylist(const QByteArray &data);
    static void updateMetaUsingTaglib(const QString& path, const QString& title,
                                      const QString& artist = QString(),
                                      const QString& album = QString(),
                                      const QString& comment = QString());
    ContentServer(QObject *parent = nullptr);
    bool getContentMeta(const QString &id, const QUrl &url, QString &meta, const ItemMeta* item);
    void requestHandler(QHttpRequest *req, QHttpResponse *resp);
    const QHash<QUrl, ItemMeta>::const_iterator makeItemMeta(const QUrl &url);
    const QHash<QUrl, ItemMeta>::const_iterator makeMicItemMeta(const QUrl &url);
#ifdef PULSE
    const QHash<QUrl, ItemMeta>::const_iterator makeAudioCaptureItemMeta(const QUrl &url);
#endif
#ifdef SCREEN
    const QHash<QUrl, ItemMeta>::const_iterator makeScreenCaptureItemMeta(const QUrl &url);
#endif
    const QHash<QUrl, ItemMeta>::const_iterator makeItemMetaUsingTracker(const QUrl &url);
    const QHash<QUrl, ItemMeta>::const_iterator makeItemMetaUsingTaglib(const QUrl &url);
    const QHash<QUrl, ItemMeta>::const_iterator makeItemMetaUsingHTTPRequest(const QUrl &url,
            std::shared_ptr<QNetworkAccessManager> nam = std::shared_ptr<QNetworkAccessManager>(),
            int counter = 0);
    ItemMeta *makeMetaUsingExtension(const QUrl &url);
    void fillCoverArt(ItemMeta& item);
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
    friend MicSource;
#ifdef PULSE
    friend PulseAudioSource;
    friend AudioCaster;
#endif
#ifdef SCREEN
    friend ScreenSource;
#endif
public:
    static ContentServerWorker* instance(QObject *parent = nullptr);
    QHttpServer* server;
    QNetworkAccessManager* nam;
    static void adjustVolume(QByteArray *data, float factor, bool le = true);
    bool isStreamToRecord(const QUrl &id);
    bool isStreamRecordable(const QUrl &id);

signals:
    void streamRecorded(const QString& title, const QString& path);
    void streamToRecordChanged(const QUrl &id, bool value);
    void streamRecordableChanged(const QUrl &id, bool value);
    void shoutcastMetadataUpdated(const QUrl &id, const QByteArray &metadata);
    void pulseStreamUpdated(const QUrl &id, const QString& name);
    void itemAdded(const QUrl &id);
    void itemRemoved(const QUrl &id);
    void contSeqWriteData(QFile* file, qint64 size, QHttpResponse *resp);

public slots:
    void setStreamToRecord(const QUrl &id, bool value);

private slots:
    void proxyMetaDataChanged();
    void proxyRedirected(const QUrl &url);
    void proxyFinished();
    void proxyReadyRead();
    void startMic();
    void stopMic();
    void responseForMicDone();
#ifdef PULSE
    void responseForAudioCaptureDone();
#endif
#ifdef SCREEN
    void responseForScreenCaptureDone();
    void screenErrorHandler();
#endif
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
        QString recExt;
        bool finished = false;
        ~ProxyItem();
    };

    struct ConnectionItem {
        QUrl id;
        QHttpRequest* req = nullptr;
        QHttpResponse* resp = nullptr;
    };

    static ContentServerWorker* m_instance;

    std::unique_ptr<QAudioInput> micInput;
    std::unique_ptr<MicSource> micSource;
#ifdef SCREEN
    std::unique_ptr<ScreenSource> screenSource;
#endif
#ifdef PULSE
    std::unique_ptr<PulseAudioSource> pulseSource;
    std::unique_ptr<AudioCaster> audioCaster;
#endif

    QHash<QNetworkReply*, ProxyItem> proxyItems;
    QHash<QHttpResponse*, QNetworkReply*> responseToReplyMap;
    QList<ConnectionItem> micItems;
    QList<ConnectionItem> audioCaptureItems;
    QList<ConnectionItem> screenCaptureItems;
    QMutex proxyItemsMutex;

    ContentServerWorker(QObject *parent = nullptr);
    void streamFile(const QString& path, const QString &mime, QHttpRequest *req, QHttpResponse *resp);
    void streamFileRange(QFile *file, QHttpRequest *req, QHttpResponse *resp);
    void streamFileNoRange(QFile *file, QHttpRequest *req, QHttpResponse *resp);
    void requestHandler(QHttpRequest *req, QHttpResponse *resp);
    void requestForFileHandler(const QUrl &id, const ContentServer::ItemMeta *meta, QHttpRequest *req, QHttpResponse *resp);
    void requestForUrlHandler(const QUrl &id, const ContentServer::ItemMeta *meta, QHttpRequest *req, QHttpResponse *resp);
    void requestForMicHandler(const QUrl &id, const ContentServer::ItemMeta *meta, QHttpRequest *req, QHttpResponse *resp);
    void requestForAudioCaptureHandler(const QUrl &id, const ContentServer::ItemMeta *meta, QHttpRequest *req, QHttpResponse *resp);
    void requestForScreenCaptureHandler(const QUrl &id, const ContentServer::ItemMeta *meta, QHttpRequest *req, QHttpResponse *resp);
    void sendEmptyResponse(QHttpResponse *resp, int code);
    void sendResponse(QHttpResponse *resp, int code, const QByteArray &data = QByteArray());
    void sendRedirection(QHttpResponse *resp, const QString &location);
    void processShoutcastMetadata(QByteArray &data, ProxyItem &item);
    void updatePulseStreamName(const QString& name);
    void dispatchPulseData(const void *data, int size, uint64_t latency = 0);
    void sendScreenCaptureData(const void *data, int size);
    void sendAudioCaptureData(const void *data, int size);
    static void removePoints(const QList<QPair<int,int>> &rpoints, QByteArray &data);
    void openRecFile(ProxyItem &item);
    void saveRecFile(ProxyItem &item);
    void closeRecFile(ProxyItem &item);
    static void cleanRecFiles();
};

class MicSource : public QIODevice
{
    Q_OBJECT
public:
    MicSource(QObject *parent = nullptr);
    void setActive(bool value);
    bool isActive();

protected:
    qint64 readData(char *data, qint64 maxSize);
    qint64 writeData(const char *data, qint64 maxSize);

private:
    bool active = false;
};

#ifdef PULSE
class PulseAudioSource : public QObject
{
    Q_OBJECT
public:
    PulseAudioSource(QObject *parent = nullptr);
    ~PulseAudioSource();
    bool start();
    void stop();
    void delayedStop();
    void resetDelayedStop();
    static pa_sample_spec sampleSpec;
    static bool inited();
    static void discoverStream();
    static bool encode(void *in_data, int in_size,
                       void **out_data, int *out_size);

signals:
    void doNextPulseIteration();
    void pulseIterationError();

private slots:
    void doPulseIteration();

private:
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
    int delayedStopCouter = 0;
    static int nullDataSize;
    static bool started;
    static const long timerDelta; // micro seconds
    static bool timerActive;
    static bool muted;
    static pa_stream *stream;
    static uint32_t connectedSinkInput;
    static pa_mainloop *ml;
    static pa_mainloop_api *mla;
    static pa_context *ctx;
    static QHash<uint32_t, PulseAudioSource::Client> clients;
    static QHash<uint32_t, PulseAudioSource::SinkInput> sinkInputs;
    static bool init();
    static void subscriptionCallback(pa_context *ctx, pa_subscription_event_type_t t, uint32_t idx, void *userdata);
    static void stateCallback(pa_context *ctx, void *userdata);
    static void successSubscribeCallback(pa_context *ctx, int success, void *userdata);
    static void streamRequestCallback(pa_stream *stream, size_t nbytes, void *userdata);
    static bool startRecordStream(pa_context *ctx, uint32_t si);
    static void stopRecordStream();
    //static void exitSignalCallback(pa_mainloop_api *mla, pa_signal_event *e, int sig, void *userdata);
    static void sinkInputInfoCallback(pa_context *ctx, const pa_sink_input_info *i, int eol, void *userdata);
    static void clientInfoCallback(pa_context *ctx, const pa_client_info *i, int eol, void *userdata);
    static void timeEventCallback(pa_mainloop_api *mla, pa_time_event *e, const struct timeval *tv, void *userdata);
    static bool checkIfShouldBeEnabled();
    static bool startTimer();
    static void stopTimer();
    static void restartTimer(pa_time_event *e, const struct timeval *tv);
    static void muteConnectedSinkInput();
    static void unmuteConnectedSinkInput();
    static bool isBlacklisted(const char* name);
    static void correctClientName(Client &client);
    static QString subscriptionEventToStr(pa_subscription_event_type_t t);
    static QList<PulseAudioSource::Client> activeClients();

    static void deinit();
};

class AudioCaster : public QObject
{
    Q_OBJECT
public:
    AudioCaster(QObject *parent = nullptr);
    ~AudioCaster();
    bool init();
    bool writeAudioData(const QByteArray& data, uint64_t latency = 0);

signals:
    void frameError();

private:
    AVPacket in_pkt;
    AVPacket out_pkt;
    AVCodecContext* out_video_codec_ctx = nullptr;
    AVFormatContext* out_format_ctx = nullptr;
    AVFrame* in_frame = nullptr;
    static int write_packet_callback(void *opaque, uint8_t *buf, int buf_size);
    int audio_frame_size = 0;
    AVCodecContext* in_audio_codec_ctx = nullptr;
    AVCodecContext* out_audio_codec_ctx = nullptr;
    SwrContext* audio_swr_ctx = nullptr;
    QByteArray audio_outbuf; // pulse audio data buf
};

#endif // PULSE

#ifdef SCREEN
class ScreenSource : public QObject
{
    Q_OBJECT
public:
    ScreenSource(QObject *parent = nullptr);
    ~ScreenSource();
    bool init();
    void start();
    bool audioEnabled();
#ifdef PULSE
    bool writeAudioData(const QByteArray& data, uint64_t latency = 0);
#endif

signals:
    void readNextVideoFrame();
    void frameError();

private slots:
    void readVideoFrame();

private:
    AVPacket in_pkt;
    AVPacket out_pkt;
    AVFormatContext* in_video_format_ctx = nullptr;
    AVCodecContext* in_video_codec_ctx = nullptr;
    AVCodecContext* out_video_codec_ctx = nullptr;
    AVFormatContext* out_format_ctx = nullptr;
    AVFrame* in_frame = nullptr;
    AVFrame* in_frame_s = nullptr;
    SwsContext* video_sws_ctx = nullptr;
    uint8_t* video_outbuf = nullptr;
    //static int read_packet_callback(void *opaque, uint8_t *buf, int buf_size);
    static int write_packet_callback(void *opaque, uint8_t *buf, int buf_size);
    int audio_frame_size = 0; // 0 => audio disabled for screen casting
#ifdef PULSE
    AVCodecContext* in_audio_codec_ctx = nullptr;
    AVCodecContext* out_audio_codec_ctx = nullptr;
    SwrContext* audio_swr_ctx = nullptr;
    QByteArray audio_outbuf; // pulse audio data buf
#endif
};
#endif // SCREEN

#endif // CONTENTSERVER_H
