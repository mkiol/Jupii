/* Copyright (C) 2021-2023 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "contentserverworker.h"

#include <QDebug>
#include <QDir>
#include <QStandardPaths>
#include <QUrlQuery>

#include "config.h"
#include "logger.hpp"
#include "playlistparser.h"
#include "settings.h"
#include "transcoder.h"
#include "utils.h"

const int ContentServerWorker::CacheLimit::INF_TIME = -1;
const int64_t ContentServerWorker::CacheLimit::INF_SIZE = -1;
const int64_t ContentServerWorker::CacheLimit::DEFAULT_DELTA = 300000;

inline static std::optional<QString> extFromPath(const QString &path) {
    auto l = path.split(QStringLiteral("."));
    if (l.isEmpty()) return std::nullopt;
    return std::make_optional(l.last().toLower());
}

static Caster::VideoEncoder convertVideoEncoder(
    Settings::CasterVideoEncoder videoEncoder) {
    switch (videoEncoder) {
        case Settings::CasterVideoEncoder::CasterVideoEncoder_Auto:
            return Caster::VideoEncoder::Auto;
        case Settings::CasterVideoEncoder::CasterVideoEncoder_Nvenc:
            return Caster::VideoEncoder::Nvenc;
        case Settings::CasterVideoEncoder::CasterVideoEncoder_V4l2:
            return Caster::VideoEncoder::V4l2;
        case Settings::CasterVideoEncoder::CasterVideoEncoder_X264:
            return Caster::VideoEncoder::X264;
    }
    return Caster::VideoEncoder::Auto;
}

static Caster::StreamFormat convertStreamFormat(
    Settings::CasterStreamFormat streamFormat) {
    switch (streamFormat) {
        case Settings::CasterStreamFormat::CasterStreamFormat_Mp4:
            return Caster::StreamFormat::Mp4;
        case Settings::CasterStreamFormat::CasterStreamFormat_MpegTs:
            return Caster::StreamFormat::MpegTs;
        case Settings::CasterStreamFormat::CasterStreamFormat_Mp3:
            return Caster::StreamFormat::Mp3;
    }
    return Caster::StreamFormat::Mp4;
}

static Caster::VideoOrientation convertVideoOrientation(
    Settings::CasterVideoOrientation videoOrientation) {
    switch (videoOrientation) {
        case Settings::CasterVideoOrientation::CasterVideoOrientation_Auto:
            return Caster::VideoOrientation::Auto;
        case Settings::CasterVideoOrientation::CasterVideoOrientation_Landscape:
            return Caster::VideoOrientation::Landscape;
        case Settings::CasterVideoOrientation::
            CasterVideoOrientation_InvertedLandscape:
            return Caster::VideoOrientation::InvertedLandscape;
        case Settings::CasterVideoOrientation::CasterVideoOrientation_Portrait:
            return Caster::VideoOrientation::Portrait;
        case Settings::CasterVideoOrientation::
            CasterVideoOrientation_InvertedPortrait:
            return Caster::VideoOrientation::InvertedPortrait;
    }
    return Caster::VideoOrientation::Auto;
}

ContentServerWorker::ContentServerWorker(QObject *parent)
    : QObject{parent}, nam{new QNetworkAccessManager{parent}},
      server{new QHttpServer{parent}} {
    qRegisterMetaType<ContentServerWorker::CacheLimit>("CacheLimit");
    qRegisterMetaType<ContentServerWorker::CacheLimit>(
        "ContentServerWorker::CacheLimit");

    connect(server, &QHttpServer::newRequest, this,
            &ContentServerWorker::requestHandler);

    connect(this, &ContentServerWorker::proxyRequested, this,
            &ContentServerWorker::startProxyInternal, Qt::QueuedConnection);

    connect(this, &ContentServerWorker::casterData, this,
            &ContentServerWorker::casterDataHandler, Qt::QueuedConnection);
    connect(this, &ContentServerWorker::casterError, this,
            &ContentServerWorker::casterErrorHandler, Qt::QueuedConnection);
    connect(this, &ContentServerWorker::casterAudioSourceNameChanged, this,
            &ContentServerWorker::casterAudioSourceNameChangedHandler,
            Qt::QueuedConnection);
    connect(this, &ContentServerWorker::preStartCaster, this,
            &ContentServerWorker::preStartCasterHandler, Qt::QueuedConnection);
    connect(this, &ContentServerWorker::preStartCaster, this,
            &ContentServerWorker::preStartCasterHandler, Qt::QueuedConnection);

    connect(this, &ContentServerWorker::hlsTimeout, this,
            &ContentServerWorker::hlsTimeoutHandler, Qt::QueuedConnection);

    connect(
        Settings::instance(), &Settings::casterPlaybackVolumeChanged, this,
        [this] {
            if (caster)
                caster->setAudioVolume(
                    Settings::instance()->getCasterPlaybackVolume());
        },
        Qt::QueuedConnection);
    connect(
        Settings::instance(), &Settings::casterMicVolumeChanged, this,
        [this] {
            if (caster)
                caster->setAudioVolume(
                    Settings::instance()->getCasterMicVolume());
        },
        Qt::QueuedConnection);

    qDebug() << "starting http server on port:"
             << Settings::instance()->getPort();
    if (!server->listen(
            static_cast<quint16>(Settings::instance()->getPort()))) {
        throw std::runtime_error(
            "unable to start http server - maybe another instance is running?");
    }

    proxiesCleanupTimer.setTimerType(Qt::TimerType::VeryCoarseTimer);
    proxiesCleanupTimer.setInterval(10000);
    proxiesCleanupTimer.setSingleShot(true);
    connect(&proxiesCleanupTimer, &QTimer::timeout, this,
            &ContentServerWorker::removeDeadProxies, Qt::QueuedConnection);

    casterTimer.setTimerType(Qt::TimerType::VeryCoarseTimer);
    casterTimer.setInterval(casterTimeout);
    casterTimer.setSingleShot(true);
    connect(&casterTimer, &QTimer::timeout, this,
            &ContentServerWorker::casterTimeoutHandler, Qt::QueuedConnection);

    if (Settings::instance()->cacheCleaningType() ==
        Settings::CacheCleaningType::CacheCleaning_Always) {
        ContentServer::cleanCacheFiles(true);
    }
}

void ContentServerWorker::closeRecFile(Proxy &proxy) {
    if (proxy.closeRecFile()) {
        proxy.saveRec = false;
        emit streamToRecordChanged(proxy.id, proxy.saveRec);
        emit streamRecordableChanged(proxy.id, false, {});
    }
}

void ContentServerWorker::openRecFile(Proxy &proxy, Proxy::Source &source) {
    if (source.openRecFile()) {
        proxy.saveRec = false;
        emit streamToRecordChanged(proxy.id, false);
        emit streamRecordableChanged(proxy.id, true, {});
    }
}

void ContentServerWorker::endRecFile(Proxy &proxy, Proxy::Source &source) {
    if (auto file = source.endRecFile(proxy)) {
        proxy.saveRec = false;
        emit streamToRecordChanged(proxy.id, false);
        emit streamRecordableChanged(proxy.id, false, file.value());
    }
}

void ContentServerWorker::endAllRecFile(Proxy &proxy) {
    const auto file = [&] {
        std::optional<QString> file;
        for (auto &[_, s] : proxy.sources) file = s.endRecFile(proxy);
        return file;
    }();

    proxy.saveRec = false;
    emit streamToRecordChanged(proxy.id, false);
    if (file) emit streamRecordableChanged(proxy.id, false, file.value());
}

void ContentServerWorker::saveRecFile(Proxy &proxy, Proxy::Source &source) {
    if (!source.recordable()) return;

    if (const auto r = source.saveRecFile(proxy)) {
        emit streamRecorded(r->first, r->second);
    }

    proxy.saveRec = false;
    emit streamToRecordChanged(proxy.id, false);
    emit streamRecordableChanged(proxy.id, false, {});
}

void ContentServerWorker::saveAllRecFile(Proxy &proxy) {
    for (auto &[_, source] : proxy.sources) {
        if (auto r = source.saveRecFile(proxy)) {
            emit streamRecorded(r->first, r->second);
        }
    }

    proxy.saveRec = false;
    emit streamToRecordChanged(proxy.id, false);
    emit streamRecordableChanged(proxy.id, false, {});
}

void ContentServerWorker::finishRecFile(Proxy &proxy, Proxy::Source &source) {
    if (!source.recordable()) return;

    if (proxy.hasMeta() || proxy.saveRec) {
        saveRecFile(proxy, source);
    } else {
        endRecFile(proxy, source);
    }
}

void ContentServerWorker::finishAllRecFile(Proxy &proxy) {
    if (proxies.isEmpty()) return;

    if (proxy.hasMeta() || proxy.saveRec) {
        saveAllRecFile(proxy);
    } else {
        endAllRecFile(proxy);
    }
}

ContentServerWorker::Proxy::~Proxy() {
    if (this->id.isValid()) {
        ContentServerWorker::instance()->finishAllRecFile(*this);
    }
}

void ContentServerWorker::requestHandler(QHttpRequest *req,
                                         QHttpResponse *resp) {
    qDebug() << ">>> requestHandler";
    qDebug() << "  method:" << req->methodString();
    qDebug() << "  path:" << req->url().path();
    qDebug() << "  headers:";
    for (auto it = req->headers().cbegin(); it != req->headers().cend(); ++it) {
        qDebug() << "    " << it.key() << ":" << it.value();
    }

    if (req->method() != QHttpRequest::HTTP_GET &&
        req->method() != QHttpRequest::HTTP_HEAD) {
        qWarning() << "request method is unsupported";
        resp->setHeader(QStringLiteral("Allow"), QStringLiteral("HEAD, GET"));
        sendEmptyResponse(resp, 405);
        return;
    }

    auto id = ContentServer::instance()->idUrlFromUrl(req->url());
    if (!id) {
        qWarning() << "unknown content requested";
        sendEmptyResponse(resp, 404);
        return;
    }

    qDebug() << "id:" << Utils::anonymizedId(*id);

    auto *meta = ContentServer::instance()->getMetaForId(*id, false);
    if (!meta) {
        qWarning() << "no meta item found";
        sendEmptyResponse(resp, 404);
        return;
    }

    removeDeadProxies();

    if (req->method() == QHttpRequest::HTTP_HEAD) {
        handleHeadRequest(meta, req, resp);
        return;
    }

    switch (meta->itemType) {
        case ContentServer::ItemType_LocalFile:
            requestForFileHandler(*id, meta, req, resp);
            break;
        case ContentServer::ItemType::ItemType_Cam:
        case ContentServer::ItemType::ItemType_Mic:
        case ContentServer::ItemType::ItemType_ScreenCapture:
        case ContentServer::ItemType::ItemType_PlaybackCapture:
            requestForCasterHandler(*id, meta, req, resp);
            break;
        case ContentServer::ItemType::ItemType_Url:
            if (!meta->path.isEmpty() &&
                !meta->flagSet(ContentServer::MetaFlag::MadeFromCache)) {
                requestForFileHandler(*id, meta, req, resp);
            } else if (meta->flagSet(ContentServer::MetaFlag::Hls)) {
                requestForCasterHandler(*id, meta, req, resp);
            } else {
                requestForUrlHandler(*id, meta, req, resp);
            }
            break;
        default:
            qWarning() << "unknown item type";
            sendEmptyResponse(resp, 404);
    }
}

void ContentServerWorker::casterErrorHandler() {
    qWarning() << "error in casting, ending casting";

    foreach (auto &item, casterItems)
        item.resp->end();
    foreach (const auto &item, casterItems)
        emit itemRemoved(item.id);

    casterItems.clear();
    casterTimer.stop();
    caster.reset();
}

void ContentServerWorker::requestForFileHandler(const QUrl &id,
                                                ContentServer::ItemMeta *meta,
                                                QHttpRequest *req,
                                                QHttpResponse *resp) {
    auto type = static_cast<ContentServer::Type>(Utils::typeFromId(id));

    if (meta->type == ContentServer::Type::Type_Video &&
        type == ContentServer::Type::Type_Music) {
        if (id.scheme() == QStringLiteral("qrc")) {
            qWarning() << "unable to extract audio stream from qrc";
            sendEmptyResponse(resp, 500);
            return;
        }

        qDebug() << "video content but audio type requested => using extracted "
                    "audio stream";

        if (!meta->audioAvMeta || !QFile::exists(meta->audioAvMeta->path)) {
            meta->audioAvMeta = Transcoder::extractAudio(meta->path);
            if (!meta->audioAvMeta) {
                qWarning() << "unable to extract audio stream";
                sendEmptyResponse(resp, 404);
                return;
            }
        }

        streamFile(meta->audioAvMeta->path, meta->audioAvMeta->mime, req, resp);
    } else {
        streamFile(meta->path, meta->mime, req, resp);
    }

    qDebug() << "requestForFileHandler done";
}

void ContentServerWorker::requestForUrlHandlerFallback(const QUrl &id,
                                                       const QUrl &origUrl,
                                                       QHttpRequest *req,
                                                       QHttpResponse *resp) {
    qDebug() << "request fallback from" << Utils::anonymizedId(id) << "to"
             << origUrl;

    auto *meta = ContentServer::instance()->getMeta(
        origUrl, true, {}, {}, false, false, false,
        static_cast<ContentServer::Type>(
            Utils::typeFromId(id)));  // create new meta
    if (!meta) {
        qWarning() << "no meta item";
        sendEmptyResponse(resp, 404);
        return;
    }

    const auto finalId = Utils::swapUrlInId(meta->url, id);

    qDebug() << "final id:" << Utils::anonymizedId(finalId);

    requestForUrlHandler(finalId, meta, req, resp);
}

QNetworkReply *ContentServerWorker::makeRequest(const QUrl &id,
                                                const QHttpRequest *req) {
    if (req && req->headers().contains(QStringLiteral("range"))) {
        return makeRequest(
            id, req->headers().value(QStringLiteral("range")).toLatin1());
    }

    return makeRequest(id);
}

QNetworkReply *ContentServerWorker::makeRequest(const QUrl &id,
                                                const QByteArray &range) {
    QNetworkRequest request;
#if QT_VERSION >= QT_VERSION_CHECK(5, 9, 0)
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
#else
    request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
#endif
    request.setUrl(Utils::urlFromId(id));
    request.setRawHeader(QByteArrayLiteral("Icy-MetaData"),
                         QByteArrayLiteral("1"));
    request.setRawHeader(QByteArrayLiteral("Connection"),
                         QByteArrayLiteral("close"));
    request.setRawHeader(QByteArrayLiteral("User-Agent"),
                         ContentServer::userAgent);

    if (range.isEmpty()) {
        request.setRawHeader(QByteArrayLiteral("Range"), makeRangeHeader(0));
    } else {
        request.setRawHeader(QByteArrayLiteral("Range"), range);
    }

    return nam->get(request);
}

QByteArray ContentServerWorker::makeRangeHeader(int64_t start, int64_t end) {
    if (end > 0)
        return QStringLiteral("bytes=%1-%2").arg(start, end).toLatin1();
    return QStringLiteral("bytes=%1-").arg(start).toLatin1();
}

void ContentServerWorker::makeProxy(const QUrl &id, bool first,
                                    CacheLimit cacheLimit,
                                    const QByteArray &range, QHttpRequest *req,
                                    QHttpResponse *resp) {
    const auto meta = ContentServer::instance()->getMetaForId(id, false);
    if (!meta) qWarning() << "no meta item found";

    if (proxies.contains(id))
        qDebug() << "proxy already exists for the id";
    else
        qDebug() << "making new proxy";

    Proxy &proxy = proxies[id];

    auto *reply =
        range.isEmpty() ? makeRequest(id, req) : makeRequest(id, range);

    proxy.id = id;
    if (meta) {
        proxy.seek = meta->flagSet(ContentServer::MetaFlag::Seek);
        proxy.mode = meta->flagSet(ContentServer::MetaFlag::PlaylistProxy)
                         ? Proxy::Mode::Playlist
                         : Proxy::Mode::Streaming;
        proxy.otype = meta->type;
        if (reply->url() != meta->origUrl) proxy.origUrl = meta->origUrl;
    }

    proxy.addSource(reply, first, cacheLimit);

    QString name;
    QUrl icon;
    Utils::pathTypeNameCookieIconFromId(proxy.id, nullptr, nullptr, &name,
                                        nullptr, &icon, nullptr, &proxy.author,
                                        &proxy.album);
    if (name.isEmpty() && meta)
        proxy.title = ContentServer::bestName(*meta);
    else
        proxy.title = std::move(name);

    if (!icon.isEmpty()) {
        proxy.artPath = ContentServer::localArtPathIfExists(icon.toString());
    } else if (meta) {
        proxy.artPath = ContentServer::localArtPathIfExists(meta->albumArt);
    }

    if (req && resp) {
        resp->disconnect(this);
        connect(resp, &QHttpResponse::done, this,
                &ContentServerWorker::responseForUrlDone, Qt::QueuedConnection);

        proxy.addSink(req, resp);
        auto source = proxy.replyMatched(resp);
        if (!source) throw std::runtime_error("no source for sink");

        if (!proxy.connected) {
            proxy.connected = true;
            emit proxyConnected(id);
        }

        if (proxy.sources[*source].hasHeaders()) {
            handleRespWithProxyMetaData(proxy, req, resp, *source);
        }
    }

    reply->disconnect(this);
    connect(reply, &QNetworkReply::metaDataChanged, this,
            &ContentServerWorker::proxyMetaDataChanged, Qt::QueuedConnection);
    connect(reply, &QNetworkReply::redirected, this,
            &ContentServerWorker::proxyRedirected, Qt::QueuedConnection);
    connect(reply, &QNetworkReply::finished, this,
            &ContentServerWorker::proxyFinished, Qt::QueuedConnection);
    connect(reply, &QNetworkReply::readyRead, this,
            &ContentServerWorker::proxyReadyRead, Qt::QueuedConnection);

    emit itemAdded(proxy.id);
}

void ContentServerWorker::startProxy(const QUrl &id) {
    const auto meta = ContentServer::instance()->getMetaForId(id, false);
    if (!meta) {
        qWarning() << "no meta item found";
        emit proxyError(id);
        return;
    }

    removeDeadProxies();
    logProxies();

    if (proxies.contains(id)) {
        qWarning() << "proxy already exists";
        const auto proxy = proxies[id];
        if (proxy.connected && !proxy.sinks.empty()) {
            emit proxyConnected(id);
            return;
        }
        removeProxy(id);
        emit proxyError(id);
        return;
    }

    if (meta->flagSet(ContentServer::MetaFlag::Seek)) {
        requestAdditionalSource(id, meta->size, meta->type);
    }
    requestFullSource(id, meta->size, meta->type);
}

void ContentServerWorker::logProxies() const {
    qDebug() << "proxies:";
    int i = 1;
    for (auto it = proxies.cbegin(); it != proxies.cend(); ++it, ++i) {
        const auto &proxy = it.value();
        qDebug() << "  " << i << "|"
                 << "sources:" << proxy.sources.size()
                 << ", sinks:" << proxy.sinks.size();
    }
}

void ContentServerWorker::logProxySinks(const Proxy &proxy) {
    qDebug() << "sinks:";
    int i = 1;
    for (auto it = proxy.sinks.cbegin(); it != proxy.sinks.cend(); ++it, ++i) {
        qDebug() << i << "|"
                 << "sink:" << it->second.resp;
    }
}

void ContentServerWorker::logProxySources(const Proxy &proxy) {
    qDebug() << "sources:";
    int i = 1;
    for (auto it = proxy.sources.cbegin(); it != proxy.sources.cend();
         ++it, ++i) {
        const auto &source = it->second;
        if (source.range)
            qDebug() << "  " << i << "|"
                     << "sources:" << source.reply << source.range.value()
                     << static_cast<int>(source.cacheDone)
                     << ", cache:" << source.cache.size() << "("
                     << source.cacheLimit.minSize << "/"
                     << source.cacheLimit.maxSize << ")";
        else
            qDebug() << "  " << i << "|"
                     << "sources:" << source.reply << "no-range"
                     << static_cast<int>(source.cacheDone)
                     << ", cache:" << source.cache.size() << "("
                     << source.cacheLimit.minSize << "/"
                     << source.cacheLimit.maxSize << ")";
    }
}

void ContentServerWorker::logProxySourceToSinks(const Proxy &proxy) {
    qDebug() << "source to sink:";
    int i = 1;
    for (auto it = proxy.sourceToSinkMap.cbegin();
         it != proxy.sourceToSinkMap.cend(); ++it, ++i) {
        qDebug() << "  " << i << "|" << it->first << "->" << it->second;
    }
}

void ContentServerWorker::startProxyInternal(const QUrl &id, bool first,
                                             CacheLimit cacheLimit,
                                             const QByteArray &range,
                                             QHttpRequest *req,
                                             QHttpResponse *resp) {
    const bool cachedRequest = !resp;

    if (cachedRequest && matchedSourceExists(id, range)) {
        qDebug() << "matched source already exists";
        return;
    }

    if (!cachedRequest && proxies.contains(id)) {
        auto &proxy = proxies[id];

        proxy.addSink(req, resp);

        if (auto reply = proxy.replyMatched(resp)) {
            qDebug() << "matching source found";

            resp->disconnect(this);
            connect(resp, &QHttpResponse::done, this,
                    &ContentServerWorker::responseForUrlDone);

            if (!proxy.connected) {
                proxy.connected = true;
                emit proxyConnected(id);
            }

            if (proxy.sources[reply.value()].hasHeaders()) {
                handleRespWithProxyMetaData(proxy, req, resp, reply.value());
            }

            return;
        }

        qDebug() << "matching source not found";
    }

    makeProxy(id, first, cacheLimit, range, req, resp);
}

void ContentServerWorker::requestForCachedContent(
    const QUrl &id, const ContentServer::ItemMeta *meta, const QString &path,
    ContentServer::Type type, QHttpRequest *req, QHttpResponse *resp) {
    // recording is possible only for audio canched files
    if (meta->type == ContentServer::Type::Type_Music ||
        type == ContentServer::Type::Type_Music) {
        emit streamRecordableChanged(id, true, path);
    }

    streamFile(path, meta->mime, req, resp);
}

void ContentServerWorker::requestForUrlHandler(
    const QUrl &id, const ContentServer::ItemMeta *meta, QHttpRequest *req,
    QHttpResponse *resp) {
    const auto type = static_cast<ContentServer::Type>(Utils::typeFromId(id));

    if (auto path = ContentServer::pathToCachedContent(*meta, true, type)) {
        qDebug() << "cached content exists:" << *path;
        requestForCachedContent(id, meta, *path, type, req, resp);
        return;
    }

    qDebug() << "proxy mode:" << resp;
    logProxies();
    startProxyInternal(id, false, CacheLimit{}, {}, req, resp);
}

void ContentServerWorker::handleHeadRequest(const ContentServer::ItemMeta *meta,
                                            QHttpRequest *req,
                                            QHttpResponse *resp) {
    qDebug() << "head request handling:" << resp;
    if (req->method() != QHttpRequest::HTTP_HEAD)
        throw std::runtime_error("request is not head");

    auto seekSupported = meta->flagSet(ContentServer::MetaFlag::Seek);

    resp->setHeader(QStringLiteral("Content-Type"), meta->mime);
    resp->setHeader(QStringLiteral("Connection"), QStringLiteral("close"));
    resp->setHeader(QStringLiteral("Cache-Control"),
                    QStringLiteral("no-cache, no-store"));
    resp->setHeader(QStringLiteral("transferMode.dlna.org"),
                    QStringLiteral("Streaming"));
    resp->setHeader(
        QStringLiteral("contentFeatures.dlna.org"),
        ContentServer::dlnaContentFeaturesHeader(meta->mime, seekSupported));

    if (seekSupported) {
        resp->setHeader(QStringLiteral("Accept-Ranges"),
                        QStringLiteral("bytes"));
    } else {
        resp->setHeader(QStringLiteral("Accept-Ranges"),
                        QStringLiteral("none"));
    }

    if (req->headers().contains(QStringLiteral("range")) && meta->size > 0 &&
        seekSupported) {
        const auto range = Range::fromRange(
            req->headers().value(QStringLiteral("range")), meta->size);
        if (!range) {
            qWarning() << "unable to read or invalid range header";
            sendEmptyResponse(resp, 416);
            return;
        }

        resp->setHeader(QStringLiteral("Content-Length"),
                        QString::number(range->rangeLength()));
        resp->setHeader(QStringLiteral("Content-Range"),
                        QStringLiteral("bytes ") +
                            QString::number(range->start) + "-" +
                            QString::number(range->end) + "/" +
                            QString::number(meta->size));
        sendResponse(resp, 206);
        return;
    }

    if (meta->size > 0) {
        resp->setHeader(QStringLiteral("Content-Length"),
                        QString::number(meta->size));
    }

    sendResponse(resp, 200);
}

std::optional<Caster::Config> ContentServerWorker::configForCaster(
    ContentServer::CasterType type, const ContentServer::ItemMeta *meta) {
    const auto *s = Settings::instance();

    Caster::Config config;
    config.streamAuthor = APP_NAME;

    switch (type) {
        case ContentServer::CasterType::Cam: {
            auto params = ContentServer::parseCasterUrl(meta->url);

            config.videoSource = params.videoSource
                                     ? params.videoSource->toStdString()
                                     : s->getCasterCam().toStdString();
            config.audioSource =
                params.audioSource
                    ? params.audioSource->toStdString()
                    : (s->getCasterCamAudio() ? s->getCasterMic().toStdString()
                                              : "");
            config.videoOrientation =
                params.videoOrientation
                    ? convertVideoOrientation(
                          ContentServer::videoOrientationFromStr(
                              *params.videoOrientation)
                              .value_or(s->getCasterVideoOrientation()))
                    : convertVideoOrientation(s->getCasterVideoOrientation());
            config.audioVolume = s->getCasterMicVolume();
            config.videoEncoder =
                convertVideoEncoder(s->getCasterVideoEncoder());
            config.streamFormat =
                convertStreamFormat(s->getCasterVideoStreamFormat());

            config.options = Caster::OptionsFlags::DroidCamRawVideoSources |
                             Caster::OptionsFlags::V4l2VideoSources;
            if (!config.audioSource.empty())
                config.options |= Caster::OptionsFlags::PaMicAudioSources;

            break;
        }
        case ContentServer::CasterType::Mic: {
            auto params = ContentServer::parseCasterUrl(meta->url);

            config.audioSource = params.audioSource
                                     ? params.audioSource->toStdString()
                                     : s->getCasterMic().toStdString();
            config.audioVolume = s->getCasterMicVolume();
            config.streamFormat =
                convertStreamFormat(s->getCasterAudioStreamFormat());

            config.options = Caster::OptionsFlags::PaMicAudioSources;

            break;
        }
        case ContentServer::CasterType::Playback: {
            auto params = ContentServer::parseCasterUrl(meta->url);

            config.audioSource = params.audioSource
                                     ? params.audioSource->toStdString()
                                     : s->getCasterPlayback().toStdString();
            config.audioVolume = s->getCasterPlaybackVolume();
            config.streamFormat =
                convertStreamFormat(s->getCasterAudioStreamFormat());

            config.options = Caster::OptionsFlags::PaMonitorAudioSources |
                             Caster::OptionsFlags::PaPlaybackAudioSources;
            if (params.audioSourceMuted.value_or(false))
                config.options |= Caster::OptionsFlags::MuteAudioSource;

            break;
        }
        case ContentServer::CasterType::Screen: {
            auto params = ContentServer::parseCasterUrl(meta->url);

            config.videoSource = params.videoSource
                                     ? params.videoSource->toStdString()
                                     : s->getCasterScreen().toStdString();
            config.audioSource =
                params.audioSource ? params.audioSource->toStdString()
                                   : (s->getCasterScreenAudio()
                                          ? s->getCasterPlayback().toStdString()
                                          : "");
            config.streamFormat =
                convertStreamFormat(s->getCasterVideoStreamFormat());

            config.videoOrientation =
                params.videoOrientation
                    ? convertVideoOrientation(
                          ContentServer::videoOrientationFromStr(
                              *params.videoOrientation)
                              .value_or(s->getCasterVideoOrientation()))
                    : convertVideoOrientation(s->getCasterVideoOrientation());
            config.audioVolume = s->getCasterMicVolume();
            config.videoEncoder =
                convertVideoEncoder(s->getCasterVideoEncoder());

            config.options = Caster::OptionsFlags::LipstickCaptureVideoSources |
                             Caster::OptionsFlags::X11CaptureVideoSources;
            if (!config.audioSource.empty()) {
                config.options |= Caster::OptionsFlags::PaMonitorAudioSources |
                                  Caster::OptionsFlags::PaPlaybackAudioSources;
                if (params.audioSourceMuted.value_or(false))
                    config.options |= Caster::OptionsFlags::MuteAudioSource;
            }

            break;
        }
        case ContentServer::CasterType::AudioFile: {
            auto files = cacheHlsSegmentsForCaster(meta->url, true);
            if (!files) return std::nullopt;

            config.audioSource = "file";
            config.streamFormat =
                convertStreamFormat(s->getCasterAudioStreamFormat());

            if (meta->flagSet(ContentServer::MetaFlag::Live)) {
                auto flags = Caster::FileSourceFlags::SameFormatForAllFiles;
                config.fileSourceConfig = Caster::FileSourceConfig{
                    std::move(*files), flags,
                    [this](const std::string &fileDone, size_t remainingFiles) {
                        qDebug() << "hls remaining files:" << remainingFiles;
                        Utils::removeFile(QString::fromStdString(fileDone));
                        if (remainingFiles < 2) emit hlsTimeout();
                    }};
            } else {
                config.fileSourceConfig =
                    Caster::FileSourceConfig{std::move(*files), 0, {}};
            }

            config.options = Caster::OptionsFlags::FileAudioSources;

            break;
        }
    }

#ifdef USE_SFOS
    if (config.videoOrientation == Caster::VideoOrientation::Auto &&
        !config.videoSource.empty())
        config.videoSource += "-rotate";
#endif

    return config;
}

bool ContentServerWorker::castingForType(ContentServer::CasterType type) const {
    if (!caster) return false;

    switch (type) {
        case ContentServer::CasterType::Cam:
            return QString::fromStdString(caster->config().videoSource)
                .startsWith(QStringLiteral("cam"));
        case ContentServer::CasterType::Mic:
            return caster->config().videoSource.empty() &&
                   QString::fromStdString(caster->config().audioSource)
                       .startsWith(QStringLiteral("mic"));
        case ContentServer::CasterType::Screen:
            return QString::fromStdString(caster->config().videoSource)
                .startsWith(QStringLiteral("screen"));
        case ContentServer::CasterType::Playback: {
            if (!caster->config().videoSource.empty()) return false;
            auto n = QString::fromStdString(caster->config().audioSource);
            return n.startsWith(QStringLiteral("playback")) ||
                   n.startsWith(QStringLiteral("monitor"));
        }
        case ContentServer::CasterType::AudioFile:
            if (!caster->config().videoSource.empty()) return false;
            return QString::fromStdString(caster->config().audioSource)
                .startsWith(QStringLiteral("file"));
    }

    return false;
}

void ContentServerWorker::requestForCasterHandler(
    const QUrl &id, const ContentServer::ItemMeta *meta, QHttpRequest *req,
    QHttpResponse *resp) {
    qDebug() << "caster request handler:" << id;

    auto type = [type = meta->itemType]() {
        switch (type) {
            case ContentServer::ItemType::ItemType_Cam:
                return ContentServer::CasterType::Cam;
            case ContentServer::ItemType::ItemType_Mic:
                return ContentServer::CasterType::Mic;
            case ContentServer::ItemType::ItemType_PlaybackCapture:
                return ContentServer::CasterType::Playback;
            case ContentServer::ItemType::ItemType_ScreenCapture:
                return ContentServer::CasterType::Screen;
            case ContentServer::ItemType::ItemType_Url:
                return ContentServer::CasterType::AudioFile;
            default:
                throw std::runtime_error("item type is not for caster");
        }
    }();

    if (!castingForType(type) && !initCaster(type, meta)) {
        sendEmptyResponse(resp, 500);
        return;
    }

    qDebug() << "sending 200 response and starting streaming";

    resp->setHeader(QStringLiteral("Content-Type"), meta->mime);
    resp->setHeader(QStringLiteral("Connection"), QStringLiteral("close"));
    resp->setHeader(QStringLiteral("transferMode.dlna.org"),
                    QStringLiteral("Streaming"));
    resp->setHeader(
        QStringLiteral("contentFeatures.dlna.org"),
        ContentServer::dlnaContentFeaturesHeader(
            meta->mime, meta->flagSet(ContentServer::MetaFlag::Seek)));
    resp->setHeader(QStringLiteral("Accept-Ranges"), QStringLiteral("none"));
    resp->writeHead(200);

    ConnectionItem item;
    item.id = id;
    item.req = req;
    item.resp = resp;
    casterItems.append(item);
    emit itemAdded(item.id);

    connect(resp, &QHttpResponse::done, this,
            &ContentServerWorker::responseForCasterDone);

    caster->start();
    casterTimer.start();
}

void ContentServerWorker::hlsTimeoutHandler() {
    qDebug() << "hls timeout";
    if (!castingForType(ContentServer::CasterType::AudioFile)) return;

    cacheHlsSegmentsForCaster(m_hlsUrl, false);
}

std::optional<std::vector<std::string>>
ContentServerWorker::cacheHlsSegmentsForCaster(const QUrl &url, bool init) {
    std::vector<std::string> files;

    auto nam = std::make_shared<QNetworkAccessManager>();

    auto ddata = Downloader{nam}.downloadData(url);

    if (ddata.bytes.isEmpty()) {
        qWarning() << "failed to download hls media playlist";
        return std::nullopt;
    }

    auto playlist = PlaylistParser::parseHls(ddata.bytes, url.toString());
    if (!playlist) {
        qWarning() << "hls playlist is invalid";
        return std::nullopt;
    }

    if (!std::holds_alternative<PlaylistParser::HlsMediaPlaylist>(*playlist)) {
        qWarning() << "hls media playlist was expected";
        return std::nullopt;
    }

    auto &mp = std::get<PlaylistParser::HlsMediaPlaylist>(*playlist);

    qDebug() << "caching hls segments";

    files.reserve(mp.items.size());

    if (init) m_hlsLastSeq = -1;

    for (const auto &item : mp.items) {
        if (item.seq <= m_hlsLastSeq) {
            qDebug() << "skip seq:" << item.seq;
            continue;
        }

        auto file = ContentServer::contentCachePath(
            item.url.toString(),
            extFromPath(item.url.toString()).value_or("ts"));

        if (!QFile::exists(file) &&
            !Downloader{nam}.downloadToFile(item.url, file, true)) {
            qWarning() << "failed to download:" << file;
        }

        if (init) {
            files.push_back(file.toStdString());
        } else {
            if (!caster) {
                qWarning() << "failed to add hls sement to caster";
                return std::nullopt;
            }
            caster->addFile(file.toStdString());
        }

        m_hlsLastSeq = item.seq;
    }

    if (files.empty() && init) {
        qWarning() << "empty playlist";
        return std::nullopt;
    }

    m_hlsUrl = url;

    return files;
}

bool ContentServerWorker::initCaster(ContentServer::CasterType type,
                                     const ContentServer::ItemMeta *meta) {
    qDebug() << "init caster:" << type;

    casterTimer.stop();

    foreach (auto &item, casterItems)
        item.resp->end();
    foreach (const auto &item, casterItems)
        emit itemRemoved(item.id);

    casterItems.clear();

    caster.reset();

    try {
        auto config = configForCaster(type, meta);
        if (!config) return false;

        switch (type) {
            case ContentServer::CasterType::Cam:
            case ContentServer::CasterType::Mic:
            case ContentServer::CasterType::AudioFile:
                caster.emplace(
                    std::move(*config),
                    [this, type](const uint8_t *data, size_t size) {
                        emit casterData(
                            type,
                            QByteArray(reinterpret_cast<const char *>(data),
                                       size));
                        return size;
                    },
                    [this](Caster::State state) {
                        if (state == Caster::State::Terminating)
                            emit casterError();
                    });
                break;
            case ContentServer::CasterType::Screen:
            case ContentServer::CasterType::Playback:
                caster.emplace(
                    std::move(*config),
                    [this, type](const uint8_t *data, size_t size) {
                        emit casterData(
                            type,
                            QByteArray(reinterpret_cast<const char *>(data),
                                       size));
                        return size;
                    },
                    [this](Caster::State state) {
                        if (state == Caster::State::Terminating)
                            emit casterError();
                    },
                    [this](const auto &name) {
                        emit casterAudioSourceNameChanged(
                            QString::fromStdString(name));
                    });
                break;
        }
    } catch (const std::runtime_error &e) {
        LOGW("failed to init caster: " << e.what());
        return false;
    }

    return true;
}

bool ContentServerWorker::preStartCasterHandler(ContentServer::CasterType type,
                                                const QUrl &id) {
    const auto *meta = ContentServer::instance()->getMetaForId(id, false);
    if (meta == nullptr) return false;

    if (!initCaster(type, meta)) {
        qWarning() << "failed to pre-start caster";
        return false;
    }

    caster->start(true);
    casterTimer.start();

    qDebug() << "caster pre-started";

    return true;
}

void ContentServerWorker::requestToPreStartCaster(
    ContentServer::CasterType type, const QUrl &id) {
    emit preStartCaster(type, id);
}

void ContentServerWorker::seqWriteData(std::shared_ptr<QFile> file,
                                       int64_t size, QHttpResponse *resp) {
    if (resp->isFinished()) {
        qWarning() << "connection closed by server, so skiping data sending";
        resp->end();
    }

    auto rlen = size;
    auto len = std::min<int64_t>(rlen, 1000000);
    QByteArray data;
    data.resize(static_cast<int>(len));
    auto *cdata = data.data();
    const auto count = static_cast<int>(file->read(cdata, len));
    rlen = rlen - len;

    qDebug() << file->fileName() << data.size();

    if (count > 0) {
        if (rlen > 0 && !resp->property("seq").toBool()) {
            resp->setProperty("seq", true);
            connect(
                resp, &QHttpResponse::allBytesWritten, this,
                [this, rlen, file] {
                    auto *resp = qobject_cast<QHttpResponse *>(sender());
                    if (!resp) return;
                    seqWriteData(file, rlen, resp);
                },
                Qt::QueuedConnection);
        }
        resp->write(data);
        return;
    }

    qDebug() << "all data sent, so ending connection";
    resp->end();
}

void ContentServerWorker::sendEmptyResponse(QHttpResponse *resp, int code) {
    qDebug() << "send empty response:" << resp << code;
    resp->setHeader(QStringLiteral("Content-Length"), QStringLiteral("0"));
    resp->writeHead(code);
    resp->end();
}

void ContentServerWorker::sendResponse(QHttpResponse *resp, int code,
                                       const QByteArray &data) {
    qDebug() << "send response:" << resp << code;
    resp->writeHead(code);
    resp->end(data);
}

void ContentServerWorker::sendRedirection(QHttpResponse *resp,
                                          const QString &location) {
    qDebug() << "send redirection:" << resp << location;
    resp->setHeader(QStringLiteral("Location"), location);
    resp->setHeader(QStringLiteral("Content-Length"), QStringLiteral("0"));
    resp->setHeader(QStringLiteral("Connection"), QStringLiteral("close"));
    resp->writeHead(302);
    resp->end();
}

void ContentServerWorker::responseForCasterDone() {
    qDebug() << "caster http response done";
    auto resp = sender();
    for (int i = 0; i < casterItems.size(); ++i) {
        if (resp == casterItems[i].resp) {
            qDebug() << "removing finished caster item";
            auto id = casterItems.at(i).id;
            casterItems.removeAt(i);
            emit itemRemoved(id);
            break;
        }
    }

    if (casterItems.isEmpty()) {
        qDebug() << "no clients for caster connected, so ending casting";

        if (casterTimer.isActive()) {
            caster->pause();
        } else {
            caster.reset();
        }
    }
}

void ContentServerWorker::responseForUrlDone() {
    auto *resp = qobject_cast<QHttpResponse *>(sender());

    qDebug() << "response done:" << resp;

    if (!resp->property("proxy").isValid()) {
        qWarning() << "no proxy id for resp";
        return;
    }

    const auto id = resp->property("proxy").toUrl();
    if (proxies.contains(id)) {
        auto &proxy = proxies[id];
        proxy.removeSink(resp);
        removeDeadProxies();
    }
}

void ContentServerWorker::removeProxySource(Proxy &proxy,
                                            QNetworkReply *reply) {
    qDebug() << "removing proxy source:" << reply;
    if (proxy.sources.count(reply) == 0)
        throw std::runtime_error("no source for reply");

    finishRecFile(proxy, proxy.sources[reply]);
    proxy.removeSource(reply);
    if (proxy.shouldBeRemoved()) removeProxy(proxy.id);
}

void ContentServerWorker::removeProxy(const QUrl &id) {
    qDebug() << "removing proxy";

    if (!proxies.contains(id)) {
        qWarning() << "no proxy for id";
        return;
    }

    auto &proxy = proxies[id];
    if (!proxy.connected) emit proxyError(proxy.id);

    finishAllRecFile(proxy);

    proxy.removeSinks();
    proxy.removeSources();

    proxies.remove(id);

    emit itemRemoved(id);
}

void ContentServerWorker::handleRespWithProxyMetaData(Proxy &proxy,
                                                      QNetworkReply *reply) {
    for (auto [key, val] : proxy.sourceToSinkMap) {
        if (key != reply) continue;
        if (proxy.sinks.count(val) == 0)
            throw std::runtime_error("no sink for resp");
        qDebug() << "calling handleRespWithProxyMetaData for sink:" << val
                 << val->isFinished();
        if (!val->isFinished()) {
            auto &sink = proxy.sinks[val];
            handleRespWithProxyMetaData(proxy, sink.req, sink.resp, reply);
        }
    }
}

void ContentServerWorker::handleRespWithProxyMetaData(Proxy &proxy,
                                                      QHttpRequest *,
                                                      QHttpResponse *resp,
                                                      QNetworkReply *reply) {
    qDebug() << "handle resp with proxy meta data:" << reply << resp;

    if (proxy.sources.count(reply) == 0)
        throw std::runtime_error("No source for reply");

    auto &source = proxy.sources[reply];

    const auto mime =
        source.reply->header(QNetworkRequest::ContentTypeHeader).toString();
    auto code =
        source.reply->attribute(QNetworkRequest::HttpStatusCodeAttribute)
            .toInt();

    resp->setHeader(QStringLiteral("Content-Type"), mime);
    resp->setHeader(QStringLiteral("Connection"), QStringLiteral("close"));
    resp->setHeader(QStringLiteral("Cache-Control"),
                    QStringLiteral("no-cache, no-store"));
    resp->setHeader(QStringLiteral("transferMode.dlna.org"),
                    QStringLiteral("Streaming"));
    resp->setHeader(QStringLiteral("contentFeatures.dlna.org"),
                    ContentServer::dlnaContentFeaturesHeader(mime, proxy.seek));

    if (source.length > 0) {
        resp->setHeader(QStringLiteral("Accept-Ranges"),
                        QStringLiteral("bytes"));
    }

    if (code > 199 && code < 299) {
        auto &sink = proxy.sinks[resp];

        const auto length = source.hasLength()
                                ? sink.range
                                      ? sink.range->end - sink.range->start + 1
                                      : source.length
                                : -1;

        qDebug() << "lengths:" << source.hasLength() << source.length << length;
        if (sink.range) {
            qDebug() << "length range:" << sink.range->start << sink.range->end;
        }

        if (length > -1) {
            resp->setHeader(QStringLiteral("Content-Length"),
                            QString::number(length));
        }

        if (sink.range && length > 0) {
            resp->setHeader(QStringLiteral("Content-Range"),
                            QStringLiteral("bytes ") +
                                QString::number(sink.range->start) + "-" +
                                QString::number(sink.range->end) + "/" +
                                QString::number(source.length));
            code = 206;
        } else {
            code = 200;
        }
    }

    const auto &headers = source.reply->rawHeaderPairs();
    for (const auto &h : headers) {
        if (h.first.toLower().startsWith("icy-"))
            resp->setHeader(QString::fromLatin1(h.first),
                            QString::fromLatin1(h.second));
    }

    qDebug() << "sending resp:" << code;

    resp->writeHead(code);
    if (source.state == Proxy::State::Streaming) {
        proxy.writeNotSent(reply, resp);
    }
    if (reply->isFinished()) {
        qDebug() << "ending resp because reply is finished";
        resp->end();
    }
}

void ContentServerWorker::proxyMetaDataChanged() {
    const auto reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply->property("proxy").isValid()) {
        qWarning() << "no proxy id for reply";
        return;
    }

    qDebug() << "proxy meta data received:" << reply;

    const auto id = reply->property("proxy").toUrl();
    if (!proxies.contains(id)) {
        qWarning() << "no proxy for id";
        return;
    }

    auto &proxy = proxies[id];

    if (proxy.sources.count(reply) == 0)
        throw std::runtime_error("No source for reply");
    auto &source = proxy.sources[reply];

    const auto reason =
        reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();
    const auto error = reply->error();
    const auto code =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    qDebug() << "source state:" << static_cast<int>(source.state);
    qDebug() << "reply status:" << code << reason;
    if (error != QNetworkReply::NoError) qDebug() << "error code:" << error;
    qDebug() << "headers:";
    for (const auto &p : reply->rawHeaderPairs()) {
        qDebug() << "  " << p.first << p.second;
    }

    if (error != QNetworkReply::NoError || code > 299) {
        qWarning() << "error response from network server";
        proxy.sendEmptyResponseAll(reply, code < 400 ? 404 : code);
    } else if (reply->header(QNetworkRequest::ContentTypeHeader)
                   .toString()
                   .isEmpty()) {
        qWarning() << "no content type header receive from network server";
        proxy.sendEmptyResponseAll(reply, 404);
    } else if (source.state == Proxy::State::Initial) {
        processNewSource(proxy, source);
        handleRespWithProxyMetaData(proxy, reply);
        checkCachedCondition(proxy);
        return;
    }

    // error handling
    emit proxyError(proxy.id);
    removeProxySource(proxy, reply);
}

void ContentServerWorker::requestAdditionalSource(const QUrl &id,
                                                  int64_t length,
                                                  ContentServer::Type type) {
    if (length <= 2 * CacheLimit::DEFAULT_DELTA) {
        qDebug() << "skipping additional source because length is to short:"
                 << length;
        return;
    }

    const int64_t delta =
        std::max(static_cast<int64_t>(length / 60), CacheLimit::DEFAULT_DELTA);
    qDebug() << "requesting proxy additional source:" << length << delta;

    emit proxyRequested(id, false, CacheLimit::fromType(type, delta),
                        makeRangeHeader(length - delta, -1), nullptr, nullptr);
}

void ContentServerWorker::requestFullSource(const QUrl &id, int64_t length,
                                            const ContentServer::Type type) {
    const int64_t delta =
        std::max(static_cast<int64_t>(length / 60), CacheLimit::DEFAULT_DELTA);
    qDebug() << "requesting proxy full source:" << length << delta;

    emit proxyRequested(id, true, CacheLimit::fromType(type, delta), {},
                        nullptr, nullptr);
}

void ContentServerWorker::processNewSource(Proxy &proxy,
                                           Proxy::Source &source) {
    source.state = proxy.mode == Proxy::Mode::Playlist
                       ? Proxy::State::Buffering
                       : Proxy::State::Streaming;

    const auto length =
        source.reply->header(QNetworkRequest::ContentLengthHeader)
            .toString()
            .toInt();
    if (!source.range || source.range->full()) {
        source.length = length;
    } else if (source.reply->hasRawHeader(QByteArrayLiteral("Content-Range"))) {
        const auto range = Range::fromContentRange(
            source.reply->rawHeader(QByteArrayLiteral("Content-Range")));
        if (range) source.length = range->length;
        source.range = range;
    }

    qDebug() << "content full length:" << source.length;

    proxy.updateRageLength(source);

    logProxySources(proxy);

    if (source.state == Proxy::State::Streaming) {
        if (source.reply->hasRawHeader(QByteArrayLiteral("icy-metaint"))) {
            source.metaint =
                source.reply->rawHeader(QByteArrayLiteral("icy-metaint"))
                    .toInt();
        }
    }

    initRecFile(proxy, source);
}

void ContentServerWorker::initRecFile(Proxy &proxy, Proxy::Source &source) {
    if (source.state == Proxy::State::Streaming && source.full()) {
        const auto mime =
            source.reply->header(QNetworkRequest::ContentTypeHeader).toString();
        if (ContentServer::getExtensionFromAudioContentType(mime).isEmpty()) {
            qDebug() << "stream should not be recorded";
        } else {
            qDebug() << "stream should be recorded";
            source.recFile = std::make_shared<QFile>();
            if (!source.hasMeta()) openRecFile(proxy, source);
        }
    }
}

void ContentServerWorker::proxyRedirected(const QUrl &url) {
    qDebug() << "proxy redirected to:" << url;
}

void ContentServerWorker::proxyFinished() {
    auto reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply->property("proxy").isValid()) {
        qWarning() << "no proxy id for reply";
        reply->deleteLater();
        return;
    }

    qDebug() << "proxy finished:" << reply;

    const auto id = reply->property("proxy").toUrl();
    if (!proxies.contains(id)) {
        qWarning() << "no proxy for id";
        reply->deleteLater();
        return;
    }

    auto &proxy = proxies[id];

    if (proxy.sources.count(reply) == 0)
        throw std::runtime_error("no source for reply");
    auto &source = proxy.sources[reply];

    const auto code =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const auto error = reply->error();

    if (error != QNetworkReply::NoError) qDebug() << "Error code:" << error;

    if (source.state == Proxy::State::Initial) {
        proxy.sendEmptyResponseAll(reply, code < 200 ? 404 : code);
    } else if (source.state == Proxy::State::Buffering) {
        qDebug() << "playlist proxy mode, so sending all data";
        auto data = reply->readAll();
        if (!data.isEmpty()) {
            // Resolving relative URLs in a playlist
            PlaylistParser::resolveM3u(data, reply->url().toString());
            if (proxy.matchedSourceExists(reply))
                proxy.writeAll(reply, data);
            else
                source.cache = data;
        } else {
            qWarning() << "data is empty";
        }
        proxy.connected = true;
        emit proxyConnected(proxy.id);
    } else if (proxy.sourceShouldBeRemoved(source)) {
        qDebug() << "source should be removed";
    } else {
        qDebug() << "source finished but before cache limit";
        checkCachedCondition(proxy);
        proxy.writeNotSentAll(reply);
        proxy.endSinks(reply);
        return;
    }

    removeProxySource(proxy, reply);
    removeDeadProxies();
}

void ContentServerWorker::setDisplayStatus(bool status) {
    displayStatus = status;
}

void ContentServerWorker::setStreamToRecord(const QUrl &id, bool value) {
    for (Proxy &item : proxies) {
        if (item.id == id) {
            if (item.saveRec != value) {
                qDebug() << "setting stream to record:" << id << value;
                item.saveRec = value;
                emit streamToRecordChanged(id, value);
            } else {
                break;
            }
        }
    }
}

QByteArray ContentServerWorker::processShoutcastMetadata(Proxy &proxy,
                                                         QNetworkReply *reply,
                                                         QByteArray data) {
    if (proxy.sources.count(reply) == 0)
        throw std::runtime_error("no sourece for reply");
    auto &source = proxy.sources[reply];

    if (!source.prevData.isEmpty()) {
        data.prepend(source.prevData);
        source.prevData.clear();
    }

    const auto count = data.length();
    const int bytes = source.metacounter + count;

    if (bytes <= source.metaint) {
        source.metacounter = bytes;
        return data;
    }

    if (source.metaint < source.metacounter)
        throw std::runtime_error("source.metaint < source.metacounter");

    const int nmeta = bytes / (source.metaint + 1);

    int totalsize = 0;

    std::vector<std::pair<int, int>> rpoints;
    rpoints.reserve(nmeta);

    int mcount = 0;
    for (int i = 0; i < nmeta; ++i) {
        const int offset = i * source.metaint + totalsize + i;
        const int start = source.metaint - source.metacounter;

        if ((start + offset) < count) {
            const int size = 16 * static_cast<uchar>(data.at(start + offset));
            const int maxsize = count - (start + offset);

            if (size > maxsize) {  // partial metadata received
                const auto metadata = data.mid(start + offset, maxsize);
                qDebug() << "partial metadata received:" << metadata;
                data.remove(start + offset, maxsize);
                source.prevData = metadata;
                source.metacounter = bytes - metadata.size();
                return data;
            }

            // full metadata received
            source.metasize = size + 1;
            if (size > 0) {
                const auto metadata = data.mid(start + offset + 1, size);
                if (metadata != proxy.metadata) {
                    if (source.recFile) {
                        if (source.recFile->isOpen())
                            saveRecFile(proxy, source);
                        auto newTitle =
                            ContentServer::streamTitleFromShoutcastMetadata(
                                metadata);
                        if (!newTitle.isEmpty()) openRecFile(proxy, source);
                    }

                    proxy.metadata = metadata;
                    source.metadata = metadata;
                    emit shoutcastMetadataUpdated(proxy.id, proxy.metadata);
                }
                totalsize += size;
            }

            rpoints.push_back({start + offset, size + 1});
            mcount += source.metaint + 1;
        } else {
            mcount += source.metaint;
            break;
        }
    }

    source.metacounter = bytes - mcount - totalsize;

    if (!rpoints.empty()) removePoints(rpoints, data);
    return data;
}

void ContentServerWorker::removePoints(std::vector<std::pair<int, int>> rpoints,
                                       QByteArray &data) {
    int offset = 0;

    for (auto &p : rpoints) {
        data.remove(p.first - offset, p.second);
        offset += p.second;
    }
}

void ContentServerWorker::casterAudioSourceNameChangedHandler(
    const QString &name) {
    foreach (const auto &item, casterItems) {
        emit playbackCaptureNameUpdated(item.id, name);
    }
}

ContentServerWorker::Proxy::MatchType
ContentServerWorker::Proxy::sourceMatchesRange(Source &source,
                                               std::optional<Range> range) {
    ContentServerWorker::Proxy::MatchType type = MatchType::Not;

    const auto cacheState = source.maxCacheReached();
    if (cacheState == CacheReachedType::NotReached) {
        if ((!source.range && !range) || (!source.range && range->full()) ||
            (!range && source.range->full()) ||
            (source.range && range && source.range->start == range->start &&
             (source.range->end == range->end ||
              (source.range->end == -1 &&
               range->end == source.range->length - 1) ||
              (range->end == -1 &&
               source.range->end == source.range->length - 1)))) {
            qDebug() << "source matched: exact" << source.reply;
            return MatchType::Exact;
        }

        const bool sinkToEnd =
            range->end == -1 ||
            (source.hasLength() && range->end == source.length - 1);
        const bool sourceToEnd =
            !source.range ||
            (source.range->end == -1 ||
             (source.hasLength() && source.range->end == source.length - 1));

        if (sinkToEnd && sourceToEnd &&
            ((!source.range && range->start <= source.cacheLimit.delta) ||
             (range->start <= (source.range->start + source.cacheLimit.delta) &&
              range->start >= source.range->start))) {
            qDebug() << "source matched: delta (" << source.cache.size() << ")"
                     << source.reply;
            return MatchType::Delta;
        }
    }

    qDebug() << "source matched: not" << source.reply;
    return type;
}

bool ContentServerWorker::Proxy::sourceMatchesSink(Source &source, Sink &sink) {
    const auto matchType = sourceMatchesRange(source, sink.range);

    if (matchType == MatchType::Not) return false;

    if (matchType == MatchType::Delta) {
        sink.sentBytes = source.range ? sink.range->start - source.range->start
                                      : sink.range->start;
    }

    return true;
}

void ContentServerWorker::Proxy::recordData(QNetworkReply *reply,
                                            const QByteArray &data) {
    auto &source = sources[reply];

    if (source.recFile && source.recFile->isOpen() &&
        source.recFile->size() < ContentServer::recMaxSize) {
        source.recFile->write(data);
    }
}

void ContentServerWorker::proxyReadyRead() {
    auto *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply->property("proxy").isValid()) {
        qWarning() << "no proxy id for reply";
        reply->abort();
        return;
    }

    const auto id = reply->property("proxy").toUrl();
    if (!proxies.contains(id)) {
        qWarning() << "no proxy for id";
        reply->abort();
        return;
    }

    auto &proxy = proxies[id];
    if (proxy.sources.count(reply) == 0)
        throw std::runtime_error("no source for reply");
    auto &source = proxy.sources[reply];

    // qDebug() << "Ready read:" << reply;

    if (source.state == Proxy::State::Buffering) {
        // ignoring, all will be handled in proxyFinished
        return;
    }

    if (proxy.sourceShouldBeRemoved(source)) {
        qDebug() << "source should be removed";
        removeProxySource(proxy, source.reply);
        return;
    }

    // qDebug() << "proxyReadyRead:" << reply << reply->bytesAvailable();

    auto data = reply->readAll();
    dispatchProxyData(proxy, reply, data);
}

void ContentServerWorker::Proxy::Source::trimProxyCache() {
    const auto meta =
        QByteArray(metaint, '\0')
            .append(QByteArray(1, static_cast<char>(metadata.size() / 16)))
            .append(metadata);
    const auto sizeWithoutMeta = metaint + metacounter;
    const auto size = sizeWithoutMeta + metasize;
    if (cache.size() > size) cache = cache.right(size).prepend(meta);
    if (cacheWithoutMeta.size() > sizeWithoutMeta)
        cacheWithoutMeta = cacheWithoutMeta.right(sizeWithoutMeta);
}

void ContentServerWorker::checkCachedCondition(Proxy &proxy) {
    if (proxy.connected) return;

    if (!proxy.sinks.empty()) {
        if (!proxy.connected) {
            proxy.connected = true;
            emit proxyConnected(proxy.id);
        }
        return;
    }

    if (proxy.minCacheReached()) {
        qDebug() << "min cache size reached";
        proxy.connected = true;
        proxy.resetCacheTimer();
        emit proxyConnected(proxy.id);
    } else if (proxy.maxCacheReached()) {
        qDebug() << "max cache reached";
        proxy.connected = true;
        proxy.resetCacheTimer();
        emit proxyConnected(proxy.id);
    }
}

void ContentServerWorker::dispatchProxyData(Proxy &proxy, QNetworkReply *reply,
                                            QByteArray &data) {
    auto &source = proxy.sources[reply];

    auto cacheState = source.maxCacheReached();

    if (source.hasMeta()) {
        auto dataWithoutMeta = processShoutcastMetadata(proxy, reply, data);
        proxy.recordData(reply, dataWithoutMeta);
        source.cache.append(data);
        source.cacheWithoutMeta.append(dataWithoutMeta);
        if (cacheState != Proxy::CacheReachedType::NotReached) {
            proxy.writeAll(reply, data, dataWithoutMeta);
        } else {
            proxy.writeNotSentAll(reply);
        }
    } else {
        proxy.recordData(reply, data);
        if (cacheState != Proxy::CacheReachedType::NotReached) {
            proxy.writeAll(reply, data);
        } else {
            source.cache.append(data);
            proxy.writeNotSentAll(reply);
        }
    }

    checkCachedCondition(proxy);
}

std::optional<ContentServerWorker::Range> ContentServerWorker::Range::fromRange(
    const QString &rangeHeader, int64_t length) {
    static const QRegExp rx{
        QStringLiteral("bytes[\\s]*=[\\s]*([\\d]+)-([\\d]*)")};

    if (rx.indexIn(rangeHeader) >= 0) {
        Range range{rx.cap(1).toLongLong(), rx.cap(2).toLongLong(), length};
        if (range.length <= 0) range.length = -1;
        if (range.end <= 0) range.end = -1;
        if (length > 0) {
            if (range.end < 0) range.end = length - 1;
            if (range.start < length - 1 && range.end < length &&
                range.end > range.start && range.end > 0 && range.start >= 0) {
                return range;
            }
        } else {
            if ((range.end == -1 ||
                 (range.end > range.start && range.end > 0)) &&
                range.start >= 0) {
                return range;
            }
        }
    }

    qWarning() << "invalid range:" << rangeHeader;
    return std::nullopt;
}

std::optional<ContentServerWorker::Range>
ContentServerWorker::Range::fromContentRange(
    const QString &contentRangeHeader) {
    static const QRegExp rx{
        QStringLiteral("bytes[\\s]*([\\d]+)-([\\d]+)/([\\d]+)")};
    if (rx.indexIn(contentRangeHeader) >= 0) {
        Range range{rx.cap(1).toLongLong(), rx.cap(2).toLongLong(),
                    rx.cap(3).toLongLong()};
        if (range.length <= 0) range.length = -1;
        if (range.start >= 0 && range.start < range.end && range.end > 0 &&
            (range.length == -1 || range.end < range.length))
            return range;
    }

    qWarning() << "Invalid Content-Range:" << contentRangeHeader;
    return std::nullopt;
}

void ContentServerWorker::streamFileRange(std::shared_ptr<QFile> file,
                                          QHttpRequest *req,
                                          QHttpResponse *resp) {
    const auto length = file->bytesAvailable();
    const auto range =
        Range::fromRange(req->headers().value(QStringLiteral("range")), length);
    if (!range) {
        qWarning() << "unable to read on invalid range header";
        sendEmptyResponse(resp, 416);
    }

    resp->setHeader(QStringLiteral("Content-Length"),
                    QString::number(range->rangeLength()));
    resp->setHeader(QStringLiteral("Content-Range"),
                    QStringLiteral("bytes ") + QString::number(range->start) +
                        '-' + QString::number(range->end) + '/' +
                        QString::number(length));

    resp->writeHead(206);
    file->seek(range->start);
    qDebug() << "range->rangeLength()" << range->rangeLength()
             << file->bytesAvailable() << *range;
    seqWriteData(file, range->rangeLength(), resp);
}

void ContentServerWorker::streamFileNoRange(std::shared_ptr<QFile> file,
                                            [[maybe_unused]] QHttpRequest *req,
                                            QHttpResponse *resp) {
    const auto length = file->bytesAvailable();

    resp->setHeader(QStringLiteral("Content-Length"), QString::number(length));

    resp->writeHead(200);
    seqWriteData(file, length, resp);
}

void ContentServerWorker::streamFile(const QString &path, const QString &mime,
                                     QHttpRequest *req, QHttpResponse *resp) {
    auto file = std::make_shared<QFile>(path);

    if (!file->open(QFile::ReadOnly)) {
        qWarning() << "unable to open file:" << file->fileName();
        sendEmptyResponse(resp, 500);
        return;
    }

    const auto &headers = req->headers();

    resp->setHeader(QStringLiteral("Content-Type"), mime);
    resp->setHeader(QStringLiteral("Accept-Ranges"), QStringLiteral("bytes"));
    resp->setHeader(QStringLiteral("Connection"), QStringLiteral("close"));
    resp->setHeader(QStringLiteral("Cache-Control"),
                    QStringLiteral("no-cache"));
    resp->setHeader(QStringLiteral("TransferMode.DLNA.ORG"),
                    QStringLiteral("Streaming"));
    resp->setHeader(QStringLiteral("contentFeatures.DLNA.ORG"),
                    ContentServer::dlnaContentFeaturesHeader(mime));

    if (headers.contains(QStringLiteral("range"))) {
        streamFileRange(file, req, resp);
    } else {
        streamFileNoRange(file, req, resp);
    }
}

void ContentServerWorker::casterDataHandler(
    [[maybe_unused]] ContentServer::CasterType type, const QByteArray &data) {
    if (casterItems.isEmpty()) {
        qDebug() << "no caster items";
        return;
    }

    auto i = casterItems.begin();
    while (i != casterItems.end()) {
        if (!i->resp->isHeaderWritten()) {
            qWarning() << "head not written for cater item";
            i->resp->end();
        }
        if (i->resp->isFinished()) {
            qWarning()
                << "server request already finished, so removing caster item";
            const auto id = i->id;
            i = casterItems.erase(i);
            emit itemRemoved(id);
        } else {
            i->resp->write(data);
            ++i;
        }
    }
}

std::optional<QNetworkReply *> ContentServerWorker::Proxy::matchSource(
    QHttpResponse *resp) {
    if (matchedSinkExists(resp)) {
        qWarning() << "sink already has matched source";
        return std::nullopt;
    }

    if (sinks.count(resp) == 0) throw std::runtime_error("no sink for resp");

    auto &sink = sinks[resp];

    auto it = std::find_if(sources.begin(), sources.end(), [&sink](auto &n) {
        return sourceMatchesSink(n.second, sink);
    });

    if (it == sources.end()) return std::nullopt;

    const auto &source = it->second;

    if (sink.range) sink.range->updateLength(source.length);

    if (std::find_if(sourceToSinkMap.cbegin(), sourceToSinkMap.cend(),
                     [reply = source.reply, resp = sink.resp](const auto &n) {
                         return n.first == reply && n.second == resp;
                     }) == sourceToSinkMap.cend()) {
        sourceToSinkMap.emplace(source.reply, sink.resp);
    }

    return source.reply;
}

bool ContentServerWorker::Proxy::matchedSourceExists(const QByteArray &range) {
    return std::find_if(
               sources.begin(), sources.end(),
               [r = Range::fromRange(QString::fromLatin1(range))](auto &n) {
                   return sourceMatchesRange(n.second, r) != MatchType::Not;
               }) != sources.end();
}

bool ContentServerWorker::Proxy::matchedSinkExists(
    const QHttpResponse *resp) const {
    return std::find_if(sourceToSinkMap.cbegin(), sourceToSinkMap.cend(),
                        [resp](const auto &n) { return n.second == resp; }) !=
           sourceToSinkMap.cend();
}

bool ContentServerWorker::matchedSourceExists(const QUrl &id,
                                              const QByteArray &range) {
    if (!proxies.contains(id)) return false;
    return proxies[id].matchedSourceExists(range);
}

std::optional<QNetworkReply *> ContentServerWorker::Proxy::unmatchSink(
    const QHttpResponse *resp) {
    logProxySourceToSinks(*this);

    for (auto it = sourceToSinkMap.begin(); it != sourceToSinkMap.end();) {
        if (it->second == resp) {
            auto reply = it->first;
            sourceToSinkMap.erase(it);
            logProxySourceToSinks(*this);
            return reply;
        }
        ++it;
    }

    return std::nullopt;
}

std::optional<QNetworkReply *> ContentServerWorker::Proxy::replyMatched(
    const QHttpResponse *resp) const {
    auto it = std::find_if(sourceToSinkMap.cbegin(), sourceToSinkMap.cend(),
                           [resp](const auto &n) { return n.second == resp; });

    if (it == sourceToSinkMap.cend()) return std::nullopt;

    return it->first;
}

QList<QHttpResponse *> ContentServerWorker::Proxy::matchSinks(
    QNetworkReply *reply) {
    if (sources.count(reply) == 0)
        throw std::runtime_error("no source for reply");

    QList<QHttpResponse *> resps;

    auto &source = sources[reply];

    for (auto it = sinks.begin(); it != sinks.end(); ++it) {
        auto &sink = it->second;
        if (!matchedSinkExists(sink.resp) && sourceMatchesSink(source, sink)) {
            auto it = std::find_if(
                sourceToSinkMap.cbegin(), sourceToSinkMap.cend(),
                [reply = source.reply, resp = sink.resp](const auto &n) {
                    return n.first == reply && n.second == resp;
                });

            if (it == sourceToSinkMap.cend()) {
                sourceToSinkMap.emplace(source.reply, sink.resp);
            }

            resps.push_back(sink.resp);
        }
    }

    qDebug() << "matched sinks:" << reply << resps.size();

    return resps;
}

void ContentServerWorker::Proxy::unmatchSource(QNetworkReply *reply) {
    for (auto it = sourceToSinkMap.begin(); it != sourceToSinkMap.end();) {
        if (it->first == reply)
            it = sourceToSinkMap.erase(it);
        else
            ++it;
    }
}

bool ContentServerWorker::Proxy::matchedSourceExists(
    QNetworkReply *reply) const {
    return sourceToSinkMap.count(reply) > 0;
}

void ContentServerWorker::Proxy::removeSinks() {
    while (!sinks.empty()) {
        removeSink(sinks.begin()->second.resp);
    }
}

void ContentServerWorker::Proxy::addSink(QHttpRequest *req,
                                         QHttpResponse *resp) {
    if (sinks.count(resp) > 0) {
        return;
    }

    resp->setProperty("proxy", id);

    Sink s;
    s.req = req;
    s.resp = resp;
    s.range = Range::fromRange(req->header(QStringLiteral("range")));

    sinks.emplace(resp, s);

    matchSource(resp);

    qDebug() << "sink added:" << resp << sinks.size();
    ContentServerWorker::logProxySinks(*this);
    ContentServerWorker::logProxySourceToSinks(*this);
}

void ContentServerWorker::Proxy::removeSink(QHttpResponse *resp) {
    resp->setProperty("proxy", {});

    sinks.erase(resp);

    unmatchSink(resp);

    qDebug() << "sink removed:" << resp << sinks.size();
    ContentServerWorker::logProxySinks(*this);
    ContentServerWorker::logProxySourceToSinks(*this);

    if (!resp->isFinished()) {
        if (resp->isHeaderWritten())
            resp->end();
        else
            sendEmptyResponse(resp, 500);
    }
}

void ContentServerWorker::Proxy::addSource(QNetworkReply *reply, bool first,
                                           CacheLimit cacheLimit) {
    if (sources.count(reply) > 0) return;

    reply->setProperty("proxy", id);

    Source s;
    s.reply = reply;
    s.range = Range::fromRange(
        reply->request().rawHeader(QByteArrayLiteral("range")));
    s.cacheLimit = cacheLimit;
    s.first = first;

    sources.emplace(reply, s);

    matchSinks(reply);

    qDebug() << "source added:" << reply << sources.size();
    ContentServerWorker::logProxySources(*this);
    ContentServerWorker::logProxySourceToSinks(*this);
}

void ContentServerWorker::Proxy::removeSource(QNetworkReply *reply) {
    reply->setProperty("proxy", {});

    sources.erase(reply);
    unmatchSource(reply);

    qDebug() << "source removed:" << reply << sources.size();
    ContentServerWorker::logProxySources(*this);
    ContentServerWorker::logProxySourceToSinks(*this);

    if (reply->isFinished()) {
        reply->deleteLater();
    } else {
        reply->abort();
    }
}

void ContentServerWorker::Proxy::removeSources() {
    while (!sources.empty()) {
        removeSource(sources.begin()->second.reply);
    }
}

void ContentServerWorker::Proxy::writeAll(QNetworkReply *reply,
                                          const QByteArray &data,
                                          const QByteArray &dataWithoutMeta) {
    if (sources[reply].hasMeta()) {
        for (auto it = sourceToSinkMap.cbegin(); it != sourceToSinkMap.cend();
             ++it) {
            if (it->first == reply && !it->second->isFinished()) {
                auto &sink = sinks[it->second];
                if (sink.meta())
                    sink.resp->write(data);
                else
                    sink.resp->write(dataWithoutMeta);
            }
        }

    } else {
        writeAll(reply, data);
    }
}

void ContentServerWorker::Proxy::writeAll(const QNetworkReply *reply,
                                          const QByteArray &data) {
    for (auto it = sourceToSinkMap.cbegin(); it != sourceToSinkMap.cend();
         ++it) {
        if (it->first == reply && !it->second->isFinished()) {
            it->second->write(data);
        }
    }
}

void ContentServerWorker::Proxy::writeNotSentAll(QNetworkReply *reply) {
    for (auto it = sourceToSinkMap.cbegin(); it != sourceToSinkMap.cend();
         ++it) {
        if (it->first == reply && !it->second->isFinished())
            writeNotSent(reply, it->second);
    }
}

void ContentServerWorker::Proxy::writeNotSent(QNetworkReply *reply,
                                              QHttpResponse *resp) {
    auto &sink = sinks[resp];
    auto &source = sources[reply];
    if (source.hasMeta()) {
        if (sink.meta())
            sink.writeNotSent(source.cache);
        else
            sink.writeNotSent(source.cacheWithoutMeta);
    } else {
        sink.writeNotSent(source.cache);
    }
}

ContentServerWorker::Proxy::CacheReachedType
ContentServerWorker::Proxy::Source::maxCacheReached() {
    if (cacheDone == CacheReachedType::All) return CacheReachedType::All;

    auto time = cacheStart.secsTo(QTime::currentTime());

    CacheReachedType type = [time, this] {
        if (cacheDone == CacheReachedType::BuffFull ||
            (cacheLimit.maxSize != CacheLimit::INF_SIZE &&
             cache.size() > cacheLimit.maxSize)) {
            if (cacheDone == CacheReachedType::Timeout ||
                (cacheLimit.maxTime != CacheLimit::INF_TIME &&
                 time > cacheLimit.maxTime)) {
                return CacheReachedType::All;
            }
            return CacheReachedType::BuffFull;
        }
        if (cacheDone == CacheReachedType::Timeout ||
            (cacheLimit.maxTime != CacheLimit::INF_TIME &&
             time > cacheLimit.maxTime)) {
            return CacheReachedType::Timeout;
        }
        return CacheReachedType::NotReached;
    }();

    if (type == CacheReachedType::All) {
        qDebug() << "proxy cache reached (all):" << reply;
        qDebug() << "cache limits:" << reply << ", size:" << cache.size() << "("
                 << cacheLimit.maxSize << "), time:" << time << "("
                 << cacheLimit.maxTime << ")";
        cache.clear();
        cacheWithoutMeta.clear();
    } else if (cacheDone != type && type == CacheReachedType::Timeout) {
        qDebug() << "proxy cache reached (timeout):" << reply;
        qDebug() << "cache limits:" << reply << ", size:" << cache.size() << "("
                 << cacheLimit.maxSize << "), time:" << time << "("
                 << cacheLimit.maxTime << ")";
    } else if (cacheDone != type && type == CacheReachedType::BuffFull) {
        qDebug() << "proxy cache reached (buff full):" << reply;
        qDebug() << "cache limits:" << reply << ", size:" << cache.size() << "("
                 << cacheLimit.maxSize << "), time:" << time << "("
                 << cacheLimit.maxTime << ")";
        cache.clear();
        cacheWithoutMeta.clear();
    }

    if (cacheDone != type) {
        qDebug() << "cache state change:" << static_cast<int>(cacheDone) << "->"
                 << static_cast<int>(type);
        cacheDone = type;
    }

    return cacheDone;
}

void ContentServerWorker::Proxy::Source::resetCacheTimer() {
    auto cacheState = maxCacheReached();

    if (cacheState != CacheReachedType::NotReached) {
        cacheDone = CacheReachedType::NotReached;
    }

    cacheStart = QTime::currentTime();
    maxCacheReached();
}

void ContentServerWorker::Proxy::endAll() {
    for (auto &r : sinks) r.second.resp->end();
}

void ContentServerWorker::Proxy::endSinks(QNetworkReply *reply) {
    for (auto [source, sink] : sourceToSinkMap) {
        if (source == reply && sink->isFinished()) sink->end();
    }
}

void ContentServerWorker::Proxy::sendEmptyResponseAll(QNetworkReply *reply,
                                                      int code) {
    for (auto [source, sink] : sourceToSinkMap) {
        if (source == reply) ContentServerWorker::sendEmptyResponse(sink, code);
    }
}

void ContentServerWorker::Proxy::sendResponseAll(QNetworkReply *reply, int code,
                                                 const QByteArray &data) {
    for (auto [source, sink] : sourceToSinkMap) {
        if (source == reply)
            ContentServerWorker::sendResponse(sink, code, data);
    }
}

void ContentServerWorker::Proxy::sendRedirectionAll(QNetworkReply *reply,
                                                    const QString &location) {
    for (auto [source, sink] : sourceToSinkMap) {
        if (source == reply)
            ContentServerWorker::sendRedirection(sink, location);
    }
}

bool ContentServerWorker::Proxy::shouldBeRemoved() {
    return sinks.empty() &&
           (sources.empty() ||
            std::all_of(sources.begin(), sources.end(), [this](auto &n) {
                return sourceShouldBeRemoved(n.second);
            }));
}

bool ContentServerWorker::Proxy::sourceShouldBeRemoved(Source &source) {
    const auto cacheState = source.maxCacheReached();
    return connected && cacheState != CacheReachedType::NotReached &&
           sourceToSinkMap.count(source.reply) == 0;
}

template <typename K, typename V>
static inline std::vector<K> mapsKeys(const std::unordered_map<K, V> &map) {
    std::vector<K> keys;
    keys.reserve(map.size());
    for (const auto &n : map) keys.push_back(n.first);
    return keys;
}

void ContentServerWorker::Proxy::removeDeadSources() {
    for (auto *r : mapsKeys(sources)) {
        auto &source = sources[r];
        if (sourceShouldBeRemoved(source)) removeSource(source.reply);
    }
}

void ContentServerWorker::removeDeadProxies() {
    qDebug() << "checking for dead proxies and sources";
    proxiesCleanupTimer.stop();

    const auto ids = proxies.keys();
    for (const auto &id : ids) {
        auto &proxy = proxies[id];
        for (auto *r : mapsKeys(proxy.sources)) {
            auto &source = proxy.sources[r];
            if (proxy.sourceShouldBeRemoved(source)) {
                finishRecFile(proxy, source);
                proxy.removeSource(source.reply);
            }
        }

        if (proxy.shouldBeRemoved()) removeProxy(id);
    }

    if (!proxies.isEmpty()) proxiesCleanupTimer.start();
}

bool ContentServerWorker::Proxy::recordable() const {
    return std::find_if(sources.cbegin(), sources.cend(), [](const auto &n) {
               return n.second.recordable();
           }) != sources.cend();
}

bool ContentServerWorker::Proxy::closeRecFile() {
    bool ret = false;

    std::for_each(sources.cbegin(), sources.cend(), [&ret](const auto &n) {
        if (n.second.recFile) {
            n.second.recFile->remove();
            ret = true;
        }
    });

    return ret;
}

void ContentServerWorker::Proxy::Sink::write(const QByteArray &data) {
    resp->write(data);
    sentBytes += data.size();
}

void ContentServerWorker::Proxy::Sink::writeNotSent(const QByteArray &data) {
    if (data.size() <= sentBytes) return;

    if (sentBytes == 0) {
        write(data);
        return;
    }

    write(QByteArray::fromRawData(data.data() + sentBytes,
                                  data.size() - sentBytes));
}

bool ContentServerWorker::Proxy::Source::openRecFile() {
    if (recFile) {
        if (recFile->isOpen()) recFile->close();
        if (recFile->exists()) recFile->remove();
        const auto mime =
            reply->header(QNetworkRequest::ContentTypeHeader).toString();
        const auto recDir =
            QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
        const auto recFilePath = QDir{recDir}.filePath(
            QStringLiteral("rec-%1.%2")
                .arg(Utils::instance()->randString(),
                     ContentServer::getExtensionFromAudioContentType(mime)));
        qDebug() << "opening file for recording:" << recFilePath;
        recFile->setFileName(recFilePath);
        if (recFile->open(QIODevice::WriteOnly)) return true;
        qWarning() << "file for recording cannot be open";
    }

    return false;
}

std::optional<QString> ContentServerWorker::Proxy::Source::endRecFile(
    const Proxy &proxy) {
    if (!recFile) return std::nullopt;

    recFile->close();

    auto title =
        hasMeta()
            ? ContentServer::streamTitleFromShoutcastMetadata(proxy.metadata)
            : proxy.title;

    if (title.isEmpty()) {
        recFile->remove();
        return std::nullopt;
    }

    qDebug() << "saving tmp recorded file:" << title;

    if (!recFile->exists() || recFile->size() <= ContentServer::recMinSize) {
        qDebug() << "recorded file doesn't exist or tiny size";
        recFile->remove();
        return std::nullopt;
    }

    auto tmpFile =
        QStringLiteral("%1.tmp.%2")
            .arg(recFile->fileName(),
                 ContentServer::getExtensionFromAudioContentType(
                     reply->header(QNetworkRequest::ContentTypeHeader)
                         .toString()));

    if (!recFile->copy(tmpFile)) {
        qWarning() << "cannot copy file:" << recFile->fileName() << tmpFile;
        recFile->remove();
        return std::nullopt;
    }

    recFile->remove();

    auto url = proxy.origUrl.isEmpty() ? Utils::urlFromId(proxy.id).toString()
                                       : proxy.origUrl.toString();

    if (!ContentServer::writeMetaUsingTaglib(
            tmpFile, true, title, metaint > 0 ? proxy.title : proxy.author,
            ContentServer::recAlbumName, {}, url, QDateTime::currentDateTime(),
            proxy.artPath, proxy.otype)) {
        qWarning() << "cannot write meta using taglib";
    };

    return std::make_optional(std::move(tmpFile));
}

std::optional<std::pair<QString, QString>>
ContentServerWorker::Proxy::Source::saveRecFile(const Proxy &proxy) {
    if (!recFile) return std::nullopt;

    recFile->close();

    if (!proxy.saveRec) {
        recFile->remove();
        return std::nullopt;
    }

    auto title =
        hasMeta()
            ? ContentServer::streamTitleFromShoutcastMetadata(proxy.metadata)
            : proxy.title;

    if (title.isEmpty()) {
        recFile->remove();
        return std::nullopt;
    }

    qDebug() << "saving recorded file for title:" << title;

    if (!recFile->exists() || recFile->size() <= ContentServer::recMinSize) {
        qDebug() << "recorded file doesn't exist or tiny size";
        recFile->remove();
        return std::nullopt;
    }

    auto mime = reply->header(QNetworkRequest::ContentTypeHeader).toString();

    auto recFilePath = QDir{Settings::instance()->getRecDir()}.filePath(
        QStringLiteral("%1.%2.%3.%4")
            .arg(Utils::escapeName(title), Utils::instance()->randString(),
                 "jupii_rec",
                 ContentServer::getExtensionFromAudioContentType(mime)));

    if (mime == QStringLiteral("audio/mp4") &&
        !Transcoder::isMp4Isom(recFile->fileName())) {
        auto avMeta =
            Transcoder::extractAudio(recFile->fileName(), recFilePath);
        if (!avMeta) {
            qWarning() << "cannot transcode file:" << recFile->fileName()
                       << recFilePath;
            recFile->remove();
            return std::nullopt;
        }
        recFilePath = std::move(avMeta->path);
    } else {
        if (!recFile->copy(recFilePath)) {
            qWarning() << "cannot copy file:" << recFile->fileName()
                       << recFilePath;
            recFile->remove();
            return std::nullopt;
        }
    }

    recFile->remove();

    auto url = proxy.origUrl.isEmpty() ? Utils::urlFromId(proxy.id).toString()
                                       : proxy.origUrl.toString();

    if (!ContentServer::writeMetaUsingTaglib(
            recFilePath, true, title, metaint > 0 ? proxy.title : proxy.author,
            ContentServer::recAlbumName, {}, url, QDateTime::currentDateTime(),
            proxy.artPath, proxy.otype)) {
        qWarning() << "cannot write meta using taglib";
    };

    return std::pair{title, recFilePath};
}

void ContentServerWorker::Range::updateLength(int64_t length) {
    this->length = length;
    if (end == -1 && length > -1) end = length - 1;
}

bool ContentServerWorker::Proxy::minCacheReached() const {
    if (!sinks.empty()) return true;

    bool firstReached = false;
    for (const auto &[_, source] : sources) {
        if (source.minCacheReached()) {
            if (source.first) firstReached = true;
        } else {
            return false;
        }
    }

    return firstReached;
}

bool ContentServerWorker::Proxy::maxCacheReached() {
    for (auto &[_, source] : sources) {
        if (source.maxCacheReached() != CacheReachedType::NotReached) {
            return true;
        }
    };
    return false;
}

void ContentServerWorker::Proxy::resetCacheTimer() {
    for (auto &[_, source] : sources) {
        source.resetCacheTimer();
    }
}

bool ContentServerWorker::Proxy::Source::minCacheReached() const {
    return cacheLimit.minSize == 0 ||
           (cacheLimit.minSize != CacheLimit::INF_SIZE &&
            cache.size() > cacheLimit.minSize) ||
           reply->isFinished();
}

QDebug operator<<(QDebug debug, const ContentServerWorker::Range &range) {
    QDebugStateSaver saver{debug};
    debug.nospace() << "Range("
                    << QStringLiteral("%1-%2/%3")
                           .arg(range.start)
                           .arg(range.end)
                           .arg(range.length)
                    << ")";
    return debug;
}

void ContentServerWorker::Proxy::updateRageLength(const Source &source) {
    if (source.length < 0) return;
    if (sourceToSinkMap.count(source.reply) == 0) return;

    for (auto it = sourceToSinkMap.cbegin(); it != sourceToSinkMap.cend();
         ++it) {
        if (it->first == source.reply && sinks.count(it->second) > 0) {
            auto &sink = sinks[it->second];
            if (sink.range) sink.range->updateLength(source.length);
        }
    }
}

void ContentServerWorker::casterTimeoutHandler() {
    qDebug() << "caster timeouted";
    if (caster && caster->state() == Caster::State::Paused) {
        qDebug() << "caster timeouted => error";
        emit casterError();
    }
}
