/* Copyright (C) 2021-2022 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "contentserverworker.h"

#include <QDebug>
#include <QDir>
#include <QStandardPaths>

#include "settings.h"
#include "utils.h"

const int ContentServerWorker::CacheLimit::INF_TIME = -1;
const int ContentServerWorker::CacheLimit::INF_SIZE = -1;
const int ContentServerWorker::CacheLimit::DEFAULT_DELTA = 300'000;

ContentServerWorker::ContentServerWorker(QObject *parent)
    : QObject{parent},
      nam{new QNetworkAccessManager{parent}},
      server{new QHttpServer{parent}},
      volumeBoost{Settings::instance()->getAudioBoost()} {
    qRegisterMetaType<ContentServerWorker::CacheLimit>("CacheLimit");

    connect(server, &QHttpServer::newRequest, this,
            &ContentServerWorker::requestHandler);
    connect(this, &ContentServerWorker::contSeqWriteData, this,
            &ContentServerWorker::seqWriteData, Qt::QueuedConnection);
    connect(this, &ContentServerWorker::proxyRequested, this,
            &ContentServerWorker::startProxyInternal, Qt::QueuedConnection);
    connect(
        Settings::instance(), &Settings::audioBoostChanged, this,
        [this] { volumeBoost = Settings::instance()->getAudioBoost(); },
        Qt::QueuedConnection);

    if (!server->listen(
            static_cast<quint16>(Settings::instance()->getPort()))) {
        throw std::runtime_error("Unable to start HTTP server");
    }

    proxiesCleanupTimer.setTimerType(Qt::TimerType::VeryCoarseTimer);
    proxiesCleanupTimer.setInterval(10000);
    proxiesCleanupTimer.setSingleShot(true);
    connect(&proxiesCleanupTimer, &QTimer::timeout, this,
            &ContentServerWorker::removeDeadProxies, Qt::QueuedConnection);

    pulseSource = std::make_unique<PulseAudioSource>();
    cleanCacheFiles();
}

void ContentServerWorker::closeRecFile(Proxy &proxy) {
    if (proxy.closeRecFile()) {
        proxy.saveRec = false;
        emit streamToRecordChanged(proxy.id, proxy.saveRec);
        emit streamRecordableChanged(proxy.id, false, {});
    }
}

void ContentServerWorker::cleanCacheFiles() {
    Utils::removeFromCacheDir(
        {"rec-*.*", "passlogfile-*.*", "art-*.*", "art_image_*.*"});
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
        for (auto &s : proxy.sources) file = s.endRecFile(proxy);
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
    for (auto &source : proxy.sources) {
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
    qDebug() << "  URL:" << req->url().path();
    qDebug() << "  headers:" << req->url().path();
    for (auto it = req->headers().cbegin(); it != req->headers().cend(); ++it) {
        qDebug() << "    " << it.key() << ":" << it.value();
    }

    if (req->method() != QHttpRequest::HTTP_GET &&
        req->method() != QHttpRequest::HTTP_HEAD) {
        qWarning() << "Request method is unsupported";
        resp->setHeader(QStringLiteral("Allow"), QStringLiteral("HEAD, GET"));
        sendEmptyResponse(resp, 405);
        return;
    }

    const auto id = ContentServer::instance()->idUrlFromUrl(req->url());
    if (!id) {
        qWarning() << "Unknown content requested!";
        sendEmptyResponse(resp, 404);
        return;
    }

    qDebug() << "Id:" << id->toString();

    const auto meta = ContentServer::instance()->getMetaForId(*id, false);
    if (!meta) {
        qWarning() << "No meta item found";
        sendEmptyResponse(resp, 404);
        return;
    }

    removeDeadProxies();

    if (req->method() == QHttpRequest::HTTP_HEAD) {
        handleHeadRequest(meta, req, resp);
        return;
    }

    if (meta->itemType == ContentServer::ItemType_LocalFile) {
        requestForFileHandler(*id, meta, req, resp);
    } else if (meta->itemType == ContentServer::ItemType_ScreenCapture) {
        requestForScreenCaptureHandler(*id, meta, req, resp);
    } else if (meta->itemType == ContentServer::ItemType_Mic) {
        requestForMicHandler(*id, meta, req, resp);
    } else if (meta->itemType == ContentServer::ItemType_AudioCapture) {
        requestForAudioCaptureHandler(*id, meta, req, resp);
    } else if (meta->itemType == ContentServer::ItemType_Url) {
        if (!meta->path.isEmpty()) {
            requestForFileHandler(*id, meta, req, resp);
        } else {
            requestForUrlHandler(*id, req, resp);
        }
    } else {
        qWarning() << "Unknown item type";
        sendEmptyResponse(resp, 404);
    }
}

void ContentServerWorker::responseForAudioCaptureDone() {
    qDebug() << "Audio capture HTTP response done";
    auto resp = sender();
    for (int i = 0; i < audioCaptureItems.size(); ++i) {
        if (resp == audioCaptureItems[i].resp) {
            qDebug() << "Removing finished audio capture item";
            auto id = audioCaptureItems.at(i).id;
            audioCaptureItems.removeAt(i);
            emit itemRemoved(id);
            break;
        }
    }

    if (audioCaptureItems.isEmpty()) {
        qDebug() << "No clients for audio capture connected, "
                    "so ending audio capturing";
        audioCaster.reset();
    }
}

void ContentServerWorker::responseForScreenCaptureDone() {
    qDebug() << "Screen capture HTTP response done";
    auto resp = sender();
    for (int i = 0; i < screenCaptureItems.size(); ++i) {
        if (resp == screenCaptureItems[i].resp) {
            qDebug() << "Removing finished screen capture item";
            auto id = screenCaptureItems.at(i).id;
            screenCaptureItems.removeAt(i);
            emit itemRemoved(id);
            break;
        }
    }

    if (screenCaptureItems.isEmpty()) {
        qDebug() << "No clients for screen capture connected, "
                    "so ending screen capturing";
        screenCaster.reset();
    }
}

void ContentServerWorker::screenErrorHandler() {
    qWarning() << "Error in screen casting, "
                  "so disconnecting clients and ending casting";
    foreach (auto &item, screenCaptureItems)
        item.resp->end();
    foreach (const auto &item, screenCaptureItems)
        emit itemRemoved(item.id);

    screenCaptureItems.clear();
    screenCaster.reset(nullptr);
}

void ContentServerWorker::audioErrorHandler() {
    qWarning() << "Error in audio casting, so disconnecting clients and ending "
                  "casting";
    foreach (auto &item, audioCaptureItems)
        item.resp->end();
    foreach (const auto &item, audioCaptureItems)
        emit itemRemoved(item.id);

    audioCaptureItems.clear();
    audioCaster.reset();
}

void ContentServerWorker::micErrorHandler() {
    qWarning()
        << "Error in mic casting, so disconnecting clients and ending casting";
    foreach (auto &item, micItems)
        item.resp->end();
    foreach (const auto &item, micItems)
        emit itemRemoved(item.id);

    micItems.clear();
    micCaster.reset();
}

void ContentServerWorker::requestForFileHandler(
    const QUrl &id, const ContentServer::ItemMeta *meta, QHttpRequest *req,
    QHttpResponse *resp) {
    const auto type = static_cast<ContentServer::Type>(Utils::typeFromId(id));
    if (meta->type == ContentServer::Type::Video &&
        type == ContentServer::Type::Music) {
        if (id.scheme() == QStringLiteral("qrc")) {
            qWarning() << "Unable to extract audio stream from qrc";
            sendEmptyResponse(resp, 500);
            return;
        }
        qDebug()
            << "Video content and type is audio => extracting audio stream";
        ContentServer::AvData data;
        if (!ContentServer::extractAudio(meta->path, data)) {
            qWarning() << "Unable to extract audio stream";
            sendEmptyResponse(resp, 404);
            return;
        }
        streamFile(data.path, data.mime, req, resp);
    } else {
        streamFile(meta->path, meta->mime, req, resp);
    }

    qDebug() << "requestForFileHandler done";
}

void ContentServerWorker::requestForUrlHandlerFallback(const QUrl &id,
                                                       const QUrl &origUrl,
                                                       QHttpRequest *req,
                                                       QHttpResponse *resp) {
    qDebug() << "Request fallback from" << id << "to" << origUrl;

    const auto meta =
        ContentServer::instance()->getMeta(origUrl, true);  // create new meta
    if (!meta) {
        qWarning() << "No meta item";
        sendEmptyResponse(resp, 404);
        return;
    }

    const auto finalId = Utils::swapUrlInId(meta->url, id);

    qDebug() << "finalId:" << finalId;

    requestForUrlHandler(finalId, req, resp);
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
    request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
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

QByteArray ContentServerWorker::makeRangeHeader(int start, int end) {
    if (end > 0)
        return QStringLiteral("bytes=%1-%2").arg(start, end).toLatin1();
    return QStringLiteral("bytes=%1-").arg(start).toLatin1();
}

void ContentServerWorker::makeProxy(const QUrl &id, const bool first,
                                    const CacheLimit cacheLimit,
                                    const QByteArray &range, QHttpRequest *req,
                                    QHttpResponse *resp) {
    const auto meta = ContentServer::instance()->getMetaForId(id, false);
    if (!meta) qWarning() << "No meta item found";

    if (proxies.contains(id))
        qDebug() << "Proxy already exists for the id";
    else
        qDebug() << "Making new proxy";

    Proxy &proxy = proxies[id];

    auto reply =
        range.isEmpty() ? makeRequest(id, req) : makeRequest(id, range);

    proxy.id = id;
    if (meta) {
        proxy.seek = meta->flagSet(ContentServer::MetaFlag::Seek);
        proxy.mode = meta->flagSet(ContentServer::MetaFlag::PlaylistProxy)
                         ? Proxy::Mode::Playlist
                         : Proxy::Mode::Streaming;
        if (reply->url() != meta->origUrl) proxy.origUrl = meta->origUrl;
    }

    proxy.addSource(reply, first, cacheLimit);

    QString name;
    QUrl icon;
    Utils::pathTypeNameCookieIconFromId(proxy.id, nullptr, nullptr, &name,
                                        nullptr, &icon, nullptr, &proxy.author);
    if (name.isEmpty() && meta)
        proxy.title = ContentServer::bestName(*meta);
    else
        proxy.title = std::move(name);

    if (!icon.isEmpty()) {
        auto artMeta = ContentServer::instance()->getMeta(icon, false);
        if (artMeta && !artMeta->path.isEmpty()) {  // art icon cache found
            proxy.artPath = artMeta->path;
        } else if (QFileInfo::exists(icon.toLocalFile())) {
            proxy.artPath = icon.toLocalFile();
        }
    } else if (meta && !meta->albumArt.isEmpty()) {
        if (QFileInfo::exists(meta->albumArt)) {
            proxy.artPath = meta->albumArt;
        } else {
            auto artMeta =
                ContentServer::instance()->getMeta(QUrl{meta->albumArt}, false);
            if (artMeta && !artMeta->path.isEmpty())  // art icon cache found
                proxy.artPath = artMeta->path;
        }
    }

    if (req && resp) {
        resp->disconnect(this);
        connect(resp, &QHttpResponse::done, this,
                &ContentServerWorker::responseForUrlDone);

        proxy.addSink(req, resp);
        auto source = proxy.replyMatched(resp);
        if (!source) throw std::runtime_error("No source for sink");

        if (!proxy.connected) {
            proxy.connected = true;
            emit proxyConnected(id);
        }

        if (proxy.sources[source.value()].hasHeaders()) {
            handleRespWithProxyMetaData(proxy, req, resp, source.value());
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
        qWarning() << "No meta item found";
        emit proxyError(id);
        return;
    }

    removeDeadProxies();
    logProxies();

    if (proxies.contains(id)) {
        qWarning() << "Proxy already exists";
        const auto proxy = proxies[id];
        if (proxy.connected && !proxy.sinks.isEmpty()) {
            emit proxyConnected(id);
            return;
        }
        removeProxy(id);
        emit proxyError(id);
        return;
    }

    if (meta->flagSet(ContentServer::MetaFlag::Seek)) {
        requestAdditionalSource(id, static_cast<int>(meta->size), meta->type);
    }
    requestFullSource(id, static_cast<int>(meta->size), meta->type);
}

void ContentServerWorker::logProxies() const {
    qDebug() << "Proxies:";
    int i = 1;
    for (auto it = proxies.cbegin(); it != proxies.cend(); ++it, ++i) {
        const auto &proxy = it.value();
        qDebug() << i << "|"
                 << "sources:" << proxy.sources.size()
                 << ", sinks:" << proxy.sinks.size();
    }
}

void ContentServerWorker::logProxySinks(const Proxy &proxy) {
    qDebug() << "Sinks:";
    int i = 1;
    for (auto it = proxy.sinks.cbegin(); it != proxy.sinks.cend(); ++it, ++i) {
        const auto &sink = it.value();
        qDebug() << i << "|"
                 << "sink:" << sink.resp;
    }
}

void ContentServerWorker::logProxySources(const Proxy &proxy) {
    qDebug() << "Sources:";
    int i = 1;
    for (auto it = proxy.sources.cbegin(); it != proxy.sources.cend();
         ++it, ++i) {
        const auto &source = it.value();
        if (source.range)
            qDebug() << i << "|"
                     << "sources:" << source.reply << source.range.value()
                     << static_cast<int>(source.cacheDone)
                     << ", cache:" << source.cache.size() << "("
                     << source.cacheLimit.minSize << "/"
                     << source.cacheLimit.maxSize << ")";
        else
            qDebug() << i << "|"
                     << "sources:" << source.reply << "no-range"
                     << static_cast<int>(source.cacheDone)
                     << ", cache:" << source.cache.size() << "("
                     << source.cacheLimit.minSize << "/"
                     << source.cacheLimit.maxSize << ")";
    }
}

void ContentServerWorker::logProxySourceToSinks(const Proxy &proxy) {
    qDebug() << "Source to Sink:";
    int i = 1;
    for (auto it = proxy.sourceToSinkMap.cbegin();
         it != proxy.sourceToSinkMap.cend(); ++it, ++i) {
        qDebug() << i << "|" << it.key() << "->" << it.value();
    }
}

void ContentServerWorker::startProxyInternal(const QUrl &id, const bool first,
                                             const CacheLimit cacheLimit,
                                             const QByteArray &range,
                                             QHttpRequest *req,
                                             QHttpResponse *resp) {
    const bool cachedRequest = !resp;

    if (cachedRequest && matchedSourceExists(id, range)) {
        qDebug() << "Matched source already exists";
        return;
    }

    if (!cachedRequest && proxies.contains(id)) {
        auto &proxy = proxies[id];

        proxy.addSink(req, resp);

        if (auto reply = proxy.replyMatched(resp)) {
            qDebug() << "Matching source found";

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

        qDebug() << "Matching source not found";
    }

    makeProxy(id, first, cacheLimit, range, req, resp);
}

void ContentServerWorker::requestForUrlHandler(const QUrl &id,
                                               QHttpRequest *req,
                                               QHttpResponse *resp) {
    // 0 - proxy for all
    // 1 - redirection for all
    // 2 - none for all
    // 3 - proxy for shoutcast, redirection for others
    // 4 - proxy for shoutcast, none for others
    const auto relay = Settings::instance()->getRemoteContentMode();

    if (relay == 1 || relay == 3) {
        qDebug() << "Redirection mode";
        sendRedirection(resp, Utils::urlFromId(id).toString());
        return;
    }

    if (relay == 2) {
        qWarning() << "Relaying is disabled";
        sendEmptyResponse(resp, 500);
        return;
    }

    qDebug() << "Proxy mode:" << resp;
    logProxies();
    startProxyInternal(id, false, CacheLimit{}, {}, req, resp);
}

void ContentServerWorker::handleHeadRequest(const ContentServer::ItemMeta *meta,
                                            QHttpRequest *req,
                                            QHttpResponse *resp) {
    qDebug() << "HEAD request handling:" << resp;
    if (req->method() != QHttpRequest::HTTP_HEAD)
        throw std::runtime_error("Request is not HEAD");

    const auto seekSupported = meta->flagSet(ContentServer::MetaFlag::Seek);

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
            qWarning() << "Unable to read or invalid Range header";
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

void ContentServerWorker::requestForMicHandler(
    const QUrl &id, const ContentServer::ItemMeta *meta, QHttpRequest *req,
    QHttpResponse *resp) {
    qDebug() << "Sending 200 response and starting streaming";
    resp->writeHead(200);

    if (!micCaster) {
        micCaster = std::make_unique<MicCaster>();
        if (!micCaster->init()) {
            qWarning() << "Cannot init mic caster";
            micCaster.reset();
            sendEmptyResponse(resp, 500);
            return;
        }
    }

    resp->setHeader(QStringLiteral("Content-Type"), meta->mime);
    resp->setHeader(QStringLiteral("Connection"), QStringLiteral("close"));
    resp->setHeader(QStringLiteral("transferMode.dlna.org"),
                    QStringLiteral("Streaming"));
    resp->setHeader(
        QStringLiteral("contentFeatures.dlna.org"),
        ContentServer::dlnaContentFeaturesHeader(
            meta->mime, meta->flagSet(ContentServer::MetaFlag::Seek)));
    resp->setHeader(QStringLiteral("Accept-Ranges"), QStringLiteral("none"));

    ConnectionItem item;
    item.id = id;
    item.req = req;
    item.resp = resp;
    micItems.append(item);
    emit itemAdded(item.id);

    connect(resp, &QHttpResponse::done, this,
            &ContentServerWorker::responseForMicDone);

    micCaster->start();
}

void ContentServerWorker::requestForAudioCaptureHandler(
    const QUrl &id, const ContentServer::ItemMeta *meta, QHttpRequest *req,
    QHttpResponse *resp) {
    qDebug() << "Audio capture request handler";

    if (!audioCaster) {
        audioCaster = std::make_unique<AudioCaster>();
        if (!audioCaster->init()) {
            qWarning() << "Cannot init audio caster";
            audioCaster.reset();
            sendEmptyResponse(resp, 500);
            return;
        }
    }

    if (!pulseSource->start()) {
        qWarning() << "Cannot init pulse audio";
        audioCaster.reset();
        sendEmptyResponse(resp, 500);
        return;
    }

    qDebug() << "Sending 200 response and starting streaming";
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
    audioCaptureItems.append(item);
    emit itemAdded(item.id);

    connect(resp, &QHttpResponse::done, this,
            &ContentServerWorker::responseForAudioCaptureDone);

    PulseAudioSource::discoverStream();
}

void ContentServerWorker::requestForScreenCaptureHandler(
    const QUrl &id, const ContentServer::ItemMeta *meta, QHttpRequest *req,
    QHttpResponse *resp) {
    qDebug() << "Screen capture request handler";

    if (!Settings::instance()->getScreenSupported()) {
        qWarning() << "Screen capturing is not supported";
        sendEmptyResponse(resp, 500);
        return;
    }

    qDebug() << "Sending 200 response and starting streaming";

    bool startNeeded = false;
    if (!screenCaster) {
        screenCaster = std::make_unique<ScreenCaster>();
        if (!screenCaster->init()) {
            qWarning() << "Cannot init screen capture";
            screenCaster.reset();
            sendEmptyResponse(resp, 500);
            return;
        }
        startNeeded = true;
    }

    ConnectionItem item;
    item.id = id;
    item.req = req;
    item.resp = resp;
    screenCaptureItems.append(item);
    emit itemAdded(item.id);
    connect(resp, &QHttpResponse::done, this,
            &ContentServerWorker::responseForScreenCaptureDone);

    if (startNeeded) {
        screenCaster->start();
        if (Settings::instance()->getScreenAudio()) {
            if (!pulseSource->start()) {
                qWarning() << "Pulse cannot be started";
                sendEmptyResponse(resp, 500);
                return;
            }
        }
    }

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

    PulseAudioSource::discoverStream();
}

void ContentServerWorker::seqWriteData(std::shared_ptr<QFile> file, qint64 size,
                                       QHttpResponse *resp) {
    if (resp->isFinished()) {
        qWarning() << "Connection closed by server, so skiping data sending";
    } else {
        qint64 rlen = size;
        const qint64 len =
            rlen < ContentServer::qlen ? rlen : ContentServer::qlen;
        // qDebug() << "Sending" << len << "of data";
        QByteArray data;
        data.resize(static_cast<int>(len));
        auto cdata = data.data();
        const auto count = static_cast<int>(file->read(cdata, len));
        rlen = rlen - len;

        if (count > 0) {
            resp->write(data);
            if (rlen > 0) {
                emit contSeqWriteData(file, rlen, resp);
                return;
            }
        } else {
            qWarning() << "No more data to read";
        }

        qDebug() << "All data sent, so ending connection";
    }

    resp->end();
}

void ContentServerWorker::sendEmptyResponse(QHttpResponse *resp, int code) {
    qDebug() << "sendEmptyResponse:" << resp << code;
    resp->setHeader(QStringLiteral("Content-Length"), QStringLiteral("0"));
    resp->writeHead(code);
    resp->end();
}

void ContentServerWorker::sendResponse(QHttpResponse *resp, int code,
                                       const QByteArray &data) {
    qDebug() << "sendResponse:" << resp << code;
    resp->writeHead(code);
    resp->end(data);
}

void ContentServerWorker::sendRedirection(QHttpResponse *resp,
                                          const QString &location) {
    qDebug() << "sendRedirection:" << resp << location;
    resp->setHeader(QStringLiteral("Location"), location);
    resp->setHeader(QStringLiteral("Content-Length"), QStringLiteral("0"));
    resp->setHeader(QStringLiteral("Connection"), QStringLiteral("close"));
    resp->writeHead(302);
    resp->end();
}

void ContentServerWorker::responseForMicDone() {
    qDebug() << "Mic HTTP response done";
    auto resp = sender();
    for (int i = 0; i < micItems.size(); ++i) {
        if (resp == micItems[i].resp) {
            qDebug() << "Removing finished mic item";
            auto id = micItems.at(i).id;
            micItems.removeAt(i);
            emit itemRemoved(id);
            break;
        }
    }

    if (micItems.isEmpty()) {
        qDebug() << "No clients for mic connected, "
                    "so ending mic casting";
        micCaster.reset();
    }
}

void ContentServerWorker::responseForUrlDone() {
    auto resp = qobject_cast<QHttpResponse *>(sender());

    qDebug() << "Response done:" << resp;

    if (!resp->property("proxy").isValid()) {
        qWarning() << "No proxy id for resp";
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
    qDebug() << "Removing proxy source:" << reply;
    if (!proxy.sources.contains(reply))
        throw std::runtime_error("No source for reply");

    finishRecFile(proxy, proxy.sources[reply]);
    proxy.removeSource(reply);
    if (proxy.shouldBeRemoved()) removeProxy(proxy.id);
}

void ContentServerWorker::removeProxy(const QUrl &id) {
    qDebug() << "Removing proxy";

    if (!proxies.contains(id)) {
        qWarning() << "No proxy for id";
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
    auto resps = proxy.sourceToSinkMap.values(reply);
    for (auto resp : resps) {
        if (!proxy.sinks.contains(resp))
            throw std::runtime_error("No sink for resp");
        qDebug() << "calling handleRespWithProxyMetaData for sink:" << resp
                 << resp->isFinished();
        if (!resp->isFinished()) {
            auto &sink = proxy.sinks[resp];
            handleRespWithProxyMetaData(proxy, sink.req, sink.resp, reply);
        }
    }
}

void ContentServerWorker::handleRespWithProxyMetaData(Proxy &proxy,
                                                      QHttpRequest *,
                                                      QHttpResponse *resp,
                                                      QNetworkReply *reply) {
    qDebug() << "Handle resp with proxy meta data:" << reply << resp;

    if (!proxy.sources.contains(reply))
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

        const int length = source.hasLength()
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

    qDebug() << "Sending resp:" << code;

    resp->writeHead(code);
    if (source.state == Proxy::State::Streaming) {
        proxy.writeNotSent(reply, resp);
    }
    if (reply->isFinished()) {
        qDebug() << "Ending resp because reply is finished";
        resp->end();
    }
}

void ContentServerWorker::proxyMetaDataChanged() {
    const auto reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply->property("proxy").isValid()) {
        qWarning() << "No proxy id for reply";
        return;
    }

    qDebug() << "Proxy meta data received:" << reply;

    const auto id = reply->property("proxy").toUrl();
    if (!proxies.contains(id)) {
        qWarning() << "No proxy for id";
        return;
    }

    auto &proxy = proxies[id];

    if (!proxy.sources.contains(reply))
        throw std::runtime_error("No source for reply");
    auto &source = proxy.sources[reply];

    const auto reason =
        reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();
    const auto error = reply->error();
    const auto code =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    qDebug() << "Source state:" << static_cast<int>(source.state);
    qDebug() << "Reply status:" << code << reason;
    if (error != QNetworkReply::NoError) qDebug() << "Error code:" << error;
    qDebug() << "Headers:";
    for (const auto &p : reply->rawHeaderPairs()) {
        qDebug() << p.first << p.second;
    }

    if (error != QNetworkReply::NoError || code > 299) {
        qWarning() << "Error response from network server";
        proxy.sendEmptyResponseAll(reply, code < 400 ? 404 : code);
    } else if (reply->header(QNetworkRequest::ContentTypeHeader)
                   .toString()
                   .isEmpty()) {
        qWarning() << "No content type header receive from network server";
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

void ContentServerWorker::requestAdditionalSource(
    const QUrl &id, const int length, const ContentServer::Type type) {
    if (length <= 2 * CacheLimit::DEFAULT_DELTA) {
        qDebug() << "Skipping additional source because length is to short:"
                 << length;
        return;
    }

    const int delta =
        std::max(static_cast<int>(length / 60), CacheLimit::DEFAULT_DELTA);
    qDebug() << "Requesting proxy additional source:" << length << delta;

    emit proxyRequested(id, false, CacheLimit::fromType(type, delta),
                        makeRangeHeader(length - delta, -1), nullptr, nullptr);
}

void ContentServerWorker::requestFullSource(const QUrl &id, const int length,
                                            const ContentServer::Type type) {
    const int delta =
        std::max(static_cast<int>(length / 60), CacheLimit::DEFAULT_DELTA);
    qDebug() << "Requesting proxy full source:" << length << delta;

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

    qDebug() << "Content full length:" << source.length;

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
    if (source.state == Proxy::State::Streaming &&
        Settings::instance()->getRec() && source.full()) {
        const auto mime =
            source.reply->header(QNetworkRequest::ContentTypeHeader).toString();
        if (ContentServer::getExtensionFromAudioContentType(mime).isEmpty()) {
            qDebug() << "Stream should not be recorded";
        } else {
            qDebug() << "Stream should be recorded";
            source.recFile = std::make_shared<QFile>();
            if (!source.hasMeta()) openRecFile(proxy, source);
        }
    }
}

void ContentServerWorker::proxyRedirected(const QUrl &url) {
    qDebug() << "Proxy redirected to:" << url;
}

void ContentServerWorker::proxyFinished() {
    auto reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply->property("proxy").isValid()) {
        qWarning() << "No proxy id for reply";
        reply->deleteLater();
        return;
    }

    qDebug() << "Proxy finished:" << reply;

    const auto id = reply->property("proxy").toUrl();
    if (!proxies.contains(id)) {
        qWarning() << "No proxy for id";
        reply->deleteLater();
        return;
    }

    auto &proxy = proxies[id];

    if (!proxy.sources.contains(reply))
        throw std::runtime_error("No source for reply");
    auto &source = proxy.sources[reply];

    const auto code =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const auto error = reply->error();

    if (error != QNetworkReply::NoError) qDebug() << "Error code:" << error;

    if (source.state == Proxy::State::Initial) {
        proxy.sendEmptyResponseAll(reply, code < 200 ? 404 : code);
    } else if (source.state == Proxy::State::Buffering) {
        qDebug() << "Playlist proxy mode, so sending all data";
        auto data = reply->readAll();
        if (!data.isEmpty()) {
            // Resolving relative URLs in a playlist
            resolveM3u(data, reply->url().toString());
            if (proxy.matchedSourceExists(reply))
                proxy.writeAll(reply, data);
            else
                source.cache = data;
        } else {
            qWarning() << "Data is empty";
        }
        proxy.connected = true;
        emit proxyConnected(proxy.id);
    } else if (proxy.sourceShouldBeRemoved(source)) {
        qDebug() << "Source should be removed";
    } else {
        qDebug() << "Source finished but before cache limit";
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
                qDebug() << "Setting stream to record:" << id << value;
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
    if (!proxy.sources.contains(reply))
        throw std::runtime_error("No sourece for reply");
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
    QList<QPair<int, int>> rpoints;  // (start,size) to remove from data

    int mcount = 0;
    for (int i = 0; i < nmeta; ++i) {
        const int offset = i * source.metaint + totalsize + i;
        const int start = source.metaint - source.metacounter;

        if ((start + offset) < count) {
            const int size = 16 * static_cast<uchar>(data.at(start + offset));
            const int maxsize = count - (start + offset);

            if (size > maxsize) {  // partial metadata received
                // qDebug() << "Partial metadata received";
                const auto metadata = data.mid(start + offset, maxsize);
                data.remove(start + offset, maxsize);
                source.prevData = metadata;
                source.metacounter = bytes - metadata.size();
                return data;
            } else {  // full metadata received
                source.metasize = size + 1;
                if (size > 0) {
                    const auto metadata = data.mid(start + offset + 1, size);
                    if (metadata != proxy.metadata) {
                        const auto newTitle =
                            ContentServer::streamTitleFromShoutcastMetadata(
                                metadata);
#ifdef QT_DEBUG
                        const auto oldTitle =
                            ContentServer::streamTitleFromShoutcastMetadata(
                                proxy.metadata);
                        qDebug() << "metadata:" << proxy.metadata << oldTitle
                                 << "->" << metadata << newTitle;
#endif
                        if (source.recFile) {
                            if (source.recFile->isOpen())
                                saveRecFile(proxy, source);
                            if (!newTitle.isEmpty()) openRecFile(proxy, source);
                        }

                        proxy.metadata = metadata;
                        source.metadata = metadata;
                        emit shoutcastMetadataUpdated(proxy.id, proxy.metadata);
                    }
                    totalsize += size;
                }

                rpoints.append({start + offset, size + 1});
            }
            mcount += source.metaint + 1;
        } else {
            mcount += source.metaint;
            break;
        }
    }

    source.metacounter = bytes - mcount - totalsize;

    if (!rpoints.isEmpty()) removePoints(rpoints, data);
    return data;
}

void ContentServerWorker::removePoints(const QList<QPair<int, int>> &rpoints,
                                       QByteArray &data) {
    int offset = 0;
    for (auto &p : rpoints) {
        data.remove(offset + p.first, p.second);
        offset = p.second;
    }
}

void ContentServerWorker::updatePulseStreamName(const QString &name) {
    foreach (const auto &item, audioCaptureItems) {
#ifdef QT_DEBUG
        qDebug() << "pulseStreamUpdated:" << name;
#endif
        emit pulseStreamUpdated(item.id, name);
    }
    foreach (const auto &item, screenCaptureItems) {
#ifdef QT_DEBUG
        qDebug() << "pulseStreamUpdated:" << name;
#endif
        emit pulseStreamUpdated(item.id, name);
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
            qDebug() << "source matched: Exact" << source.reply;
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
            qDebug() << "source matched: Delta (" << source.cache.size() << ")"
                     << source.reply;
            return MatchType::Delta;
        }
    }

    qDebug() << "source matched: Not" << source.reply;
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
    auto reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply->property("proxy").isValid()) {
        qWarning() << "No proxy id for reply";
        reply->abort();
        return;
    }

    const auto id = reply->property("proxy").toUrl();
    if (!proxies.contains(id)) {
        qWarning() << "No proxy for id";
        reply->abort();
        return;
    }

    auto &proxy = proxies[id];
    if (!proxy.sources.contains(reply))
        throw std::runtime_error("No source for reply");
    auto &source = proxy.sources[reply];

    // qDebug() << "Ready read:" << reply;

    if (source.state == Proxy::State::Buffering) {
        // ignoring, all will be handled in proxyFinished
        return;
    }

    if (proxy.sourceShouldBeRemoved(source)) {
        qDebug() << "Source should be removed";
        removeProxySource(proxy, source.reply);
        return;
    }

    // qDebug() << "proxyReadyRead:" << reply << reply->bytesAvailable();

    dispatchProxyData(proxy, reply, reply->readAll());
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

    if (!proxy.sinks.isEmpty()) {
        if (!proxy.connected) {
            proxy.connected = true;
            emit proxyConnected(proxy.id);
        }
        return;
    }

    if (proxy.minCacheReached()) {
        qDebug() << "Min cache size reached";
        proxy.connected = true;
        proxy.resetCacheTimer();
        emit proxyConnected(proxy.id);
    } else if (proxy.maxCacheReached()) {
        qDebug() << "Max cache reached";
        proxy.connected = true;
        proxy.resetCacheTimer();
        emit proxyConnected(proxy.id);
    }
}

void ContentServerWorker::dispatchProxyData(Proxy &proxy, QNetworkReply *reply,
                                            const QByteArray &data) {
    auto &source = proxy.sources[reply];

    const auto cacheState = source.maxCacheReached();

    if (source.hasMeta()) {
        const auto dataWithoutMeta =
            processShoutcastMetadata(proxy, reply, data);
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
    const QString &rangeHeader, int length) {
    static const QRegExp rx{
        QStringLiteral("bytes[\\s]*=[\\s]*([\\d]+)-([\\d]*)")};

    if (rx.indexIn(rangeHeader) >= 0) {
        Range range{rx.cap(1).toInt(), rx.cap(2).toInt(), length};
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

    qWarning() << "Invalid Range:" << rangeHeader;
    return std::nullopt;
}

std::optional<ContentServerWorker::Range>
ContentServerWorker::Range::fromContentRange(
    const QString &contentRangeHeader) {
    static const QRegExp rx{
        QStringLiteral("bytes[\\s]*([\\d]+)-([\\d]+)/([\\d]+)")};
    if (rx.indexIn(contentRangeHeader) >= 0) {
        Range range{rx.cap(1).toInt(), rx.cap(2).toInt(), rx.cap(3).toInt()};
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
        qWarning() << "Unable to read on invalid Range header";
        sendEmptyResponse(resp, 416);
    }

    resp->setHeader(QStringLiteral("Content-Length"),
                    QString::number(range->rangeLength()));
    resp->setHeader(QStringLiteral("Content-Range"),
                    QStringLiteral("bytes ") + QString::number(range->start) +
                        "-" + QString::number(range->end) + "/" +
                        QString::number(length));

    resp->writeHead(206);
    file->seek(range->start);
    seqWriteData(file, range->rangeLength(), resp);
}

void ContentServerWorker::streamFileNoRange(std::shared_ptr<QFile> file,
                                            QHttpRequest *,
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
        qWarning() << "Unable to open file:" << file->fileName();
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

void ContentServerWorker::adjustVolume(QByteArray *data, float factor,
                                       bool le) {
    QDataStream sr(data, QIODevice::ReadOnly);
    sr.setByteOrder(le ? QDataStream::LittleEndian : QDataStream::BigEndian);
    QDataStream sw(data, QIODevice::WriteOnly);
    sw.setByteOrder(le ? QDataStream::LittleEndian : QDataStream::BigEndian);
    int16_t sample;  // assuming 16-bit LPCM sample

    while (!sr.atEnd()) {
        sr >> sample;
        int32_t s = int32_t(factor * int32_t(sample));
        if (s > std::numeric_limits<int16_t>::max()) {
            sample = std::numeric_limits<int16_t>::max();
        } else if (s < std::numeric_limits<int16_t>::min()) {
            sample = std::numeric_limits<int16_t>::min();
        } else {
            sample = static_cast<int16_t>(s);
        }
        sw << sample;
    }
}

void ContentServerWorker::dispatchPulseData(const void *data, int size) {
    const bool audioCaptureEnabled =
        audioCaster && !audioCaptureItems.isEmpty();
    const bool screenCaptureAudioEnabled = screenCaster &&
                                           screenCaster->audioEnabled() &&
                                           !screenCaptureItems.isEmpty();

    if (audioCaptureEnabled || screenCaptureAudioEnabled) {
        QByteArray d;
        if (data) {
            // qDebug() << "writing data:" << size << volumeBoost;
            if (volumeBoost > 1.0f) {  // increasing volume level
                d = QByteArray(static_cast<const char *>(data),
                               size);  // deep copy
                adjustVolume(&d, volumeBoost, true);
            } else {
                d = QByteArray::fromRawData(static_cast<const char *>(data),
                                            size);
            }
        } else {
            // writing null data
            // qDebug() << "writing null data:" << size;
            d = QByteArray(size, '\0');
        }
        if (audioCaptureEnabled) audioCaster->writeAudioData(d);
        if (screenCaptureAudioEnabled) screenCaster->writeAudioData(d);
    }
}

void ContentServerWorker::sendMicData(const void *data, int size) {
    if (micItems.isEmpty()) {
        qDebug() << "No mic items";
        return;
    }

    const auto d =
        QByteArray::fromRawData(static_cast<const char *>(data), size);
    auto i = micItems.begin();
    while (i != micItems.end()) {
        if (!i->resp->isHeaderWritten()) {
            qWarning() << "Head not written";
            i->resp->end();
        }
        if (i->resp->isFinished()) {
            qWarning() << "Server request already finished, "
                          "so removing mic item";
            const auto id = i->id;
            i = micItems.erase(i);
            emit itemRemoved(id);
        } else {
            i->resp->write(d);
            ++i;
        }
    }
}

void ContentServerWorker::sendAudioCaptureData(const void *data, int size) {
    if (audioCaptureItems.isEmpty()) {
        qDebug() << "No audio capture items";
        return;
    }

    const auto d =
        QByteArray::fromRawData(static_cast<const char *>(data), size);
    auto i = audioCaptureItems.begin();
    while (i != audioCaptureItems.end()) {
        if (!i->resp->isHeaderWritten()) {
            qWarning() << "Head not written";
            i->resp->end();
        }
        if (i->resp->isFinished()) {
            qWarning() << "Server request already finished, "
                          "so removing audio capture item";
            const auto id = i->id;
            i = audioCaptureItems.erase(i);
            emit itemRemoved(id);
        } else {
            i->resp->write(d);
            ++i;
        }
    }
}

void ContentServerWorker::sendScreenCaptureData(const void *data, int size) {
    if (screenCaptureItems.isEmpty()) {
        qDebug() << "No screen items";
        return;
    }

    const auto d =
        QByteArray::fromRawData(static_cast<const char *>(data), size);
    auto i = screenCaptureItems.begin();
    while (i != screenCaptureItems.end()) {
        if (!i->resp->isHeaderWritten()) {
            qWarning() << "Head not written";
            i->resp->end();
        }
        if (i->resp->isFinished()) {
            qWarning() << "Server request already finished, "
                          "so removing screen item";
            const auto id = i->id;
            i = screenCaptureItems.erase(i);
            emit itemRemoved(id);
        } else {
            i->resp->write(d);
            ++i;
        }
    }
}

std::optional<QNetworkReply *> ContentServerWorker::Proxy::matchSource(
    QHttpResponse *resp) {
    if (matchedSinkExists(resp)) {
        qWarning() << "Sink already has matched source";
        return std::nullopt;
    }

    if (!sinks.contains(resp)) throw std::runtime_error("No sink for resp");

    auto &sink = sinks[resp];

    for (auto it = sources.begin(); it != sources.end(); ++it) {
        auto &source = it.value();
        if (sourceMatchesSink(source, sink)) {
            if (sink.range) sink.range->updateLength(source.length);
            if (!sourceToSinkMap.contains(source.reply, sink.resp)) {
                sourceToSinkMap.insert(source.reply, sink.resp);
            }
            return source.reply;
        }
    }

    return std::nullopt;
}

bool ContentServerWorker::Proxy::matchedSourceExists(const QByteArray &range) {
    const auto r = Range::fromRange(QString::fromLatin1(range));
    for (auto &source : sources) {
        if (sourceMatchesRange(source, r) != MatchType::Not) {
            return true;
        }
    }

    return false;
}

bool ContentServerWorker::Proxy::matchedSinkExists(
    const QHttpResponse *resp) const {
    for (auto it = sourceToSinkMap.cbegin(); it != sourceToSinkMap.cend();
         ++it) {
        if (it.value() == resp) return true;
    }
    return false;
}

bool ContentServerWorker::matchedSourceExists(const QUrl &id,
                                              const QByteArray &range) {
    if (!proxies.contains(id)) return false;
    return proxies[id].matchedSourceExists(range);
}

std::optional<QNetworkReply *> ContentServerWorker::Proxy::unmatchSink(
    QHttpResponse *resp) {
    logProxySourceToSinks(*this);
    auto it = sourceToSinkMap.begin();
    while (it != sourceToSinkMap.end()) {
        if (it.value() == resp) {
            auto reply = it.key();
            sourceToSinkMap.remove(reply, resp);
            logProxySourceToSinks(*this);
            return reply;
        }
        ++it;
    }

    return std::nullopt;
}

std::optional<QNetworkReply *> ContentServerWorker::Proxy::replyMatched(
    const QHttpResponse *resp) const {
    auto it = sourceToSinkMap.cbegin();
    while (it != sourceToSinkMap.cend()) {
        if (it.value() == resp) return it.key();
        ++it;
    }
    return std::nullopt;
}

QList<QHttpResponse *> ContentServerWorker::Proxy::matchSinks(
    QNetworkReply *reply) {
    if (!sources.contains(reply))
        throw std::runtime_error("No source for reply");

    QList<QHttpResponse *> resps;

    auto &source = sources[reply];

    for (auto it = sinks.begin(); it != sinks.end(); ++it) {
        auto &sink = it.value();
        if (!matchedSinkExists(sink.resp) && sourceMatchesSink(source, sink)) {
            if (!sourceToSinkMap.contains(source.reply, sink.resp)) {
                sourceToSinkMap.insert(source.reply, sink.resp);
            }
            resps.push_back(sink.resp);
        }
    }

    qDebug() << "Matched sinks:" << reply << resps.size();

    return resps;
}

void ContentServerWorker::Proxy::unmatchSource(QNetworkReply *reply) {
    sourceToSinkMap.remove(reply);
}

bool ContentServerWorker::Proxy::matchedSourceExists(
    QNetworkReply *reply) const {
    return sourceToSinkMap.contains(reply);
}

void ContentServerWorker::Proxy::removeSinks() {
    while (!sinks.isEmpty()) {
        removeSink(sinks.begin()->resp);
    }
}

void ContentServerWorker::Proxy::addSink(QHttpRequest *req,
                                         QHttpResponse *resp) {
    if (sinks.contains(resp)) {
        return;
    }

    resp->setProperty("proxy", id);

    Sink s;
    s.req = req;
    s.resp = resp;
    s.range = Range::fromRange(req->header(QStringLiteral("range")));

    sinks.insert(resp, s);

    matchSource(resp);

    qDebug() << "Sink added:" << resp << sinks.size();
    ContentServerWorker::logProxySinks(*this);
    ContentServerWorker::logProxySourceToSinks(*this);
}

void ContentServerWorker::Proxy::removeSink(QHttpResponse *resp) {
    resp->setProperty("proxy", {});

    sinks.remove(resp);

    unmatchSink(resp);

    qDebug() << "Sink removed:" << resp << sinks.size();
    ContentServerWorker::logProxySinks(*this);
    ContentServerWorker::logProxySourceToSinks(*this);

    if (!resp->isFinished()) {
        if (resp->isHeaderWritten())
            resp->end();
        else
            sendEmptyResponse(resp, 500);
    }
}

void ContentServerWorker::Proxy::addSource(QNetworkReply *reply,
                                           const bool first,
                                           const CacheLimit cacheLimit) {
    if (sources.contains(reply)) return;

    reply->setProperty("proxy", id);

    Source s;
    s.reply = reply;
    s.range = Range::fromRange(
        reply->request().rawHeader(QByteArrayLiteral("range")));
    s.cacheLimit = cacheLimit;
    s.first = first;

    sources.insert(reply, s);

    matchSinks(reply);

    qDebug() << "Source added:" << reply << sources.size();
    ContentServerWorker::logProxySources(*this);
    ContentServerWorker::logProxySourceToSinks(*this);
}

void ContentServerWorker::Proxy::removeSource(QNetworkReply *reply) {
    reply->setProperty("proxy", {});

    sources.remove(reply);
    unmatchSource(reply);

    qDebug() << "Source removed:" << reply << sources.size();
    ContentServerWorker::logProxySources(*this);
    ContentServerWorker::logProxySourceToSinks(*this);

    if (reply->isFinished()) {
        reply->deleteLater();
    } else {
        reply->abort();
    }
}

void ContentServerWorker::Proxy::removeSources() {
    while (!sources.isEmpty()) {
        removeSource(sources.begin()->reply);
    }
}

void ContentServerWorker::Proxy::writeAll(QNetworkReply *reply,
                                          const QByteArray &data,
                                          const QByteArray &dataWithoutMeta) {
    if (sources[reply].hasMeta()) {
        for (auto it = sourceToSinkMap.cbegin(); it != sourceToSinkMap.cend();
             ++it) {
            if (it.key() == reply && !it.value()->isFinished()) {
                auto &sink = sinks[it.value()];
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
        if (it.key() == reply && !it.value()->isFinished())
            it.value()->write(data);
    }
}

void ContentServerWorker::Proxy::writeNotSentAll(QNetworkReply *reply) {
    for (auto it = sourceToSinkMap.cbegin(); it != sourceToSinkMap.cend();
         ++it) {
        if (it.key() == reply && !it.value()->isFinished())
            writeNotSent(reply, it.value());
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

    const auto time = cacheStart.secsTo(QTime::currentTime());

    const CacheReachedType type = [time, this] {
        if (cacheDone == CacheReachedType::BuffFull ||
            (cacheLimit.maxSize != CacheLimit::INF_SIZE &&
             cache.size() > cacheLimit.maxSize)) {
            if (cacheDone == CacheReachedType::Timeout ||
                (cacheLimit.maxTime != CacheLimit::INF_TIME &&
                 time > cacheLimit.maxTime))
                return CacheReachedType::All;
            else
                return CacheReachedType::BuffFull;
        }
        if (cacheDone == CacheReachedType::Timeout ||
            (cacheLimit.maxTime != CacheLimit::INF_TIME &&
             time > cacheLimit.maxTime))
            return CacheReachedType::Timeout;
        else
            return CacheReachedType::NotReached;
    }();

    if (type == CacheReachedType::All) {
        qDebug() << "Proxy cache reached (All):" << reply;
        qDebug() << "Cache limits:" << reply << ", size:" << cache.size() << "("
                 << cacheLimit.maxSize << "), time:" << time << "("
                 << cacheLimit.maxTime << ")";
        cache.clear();
        cacheWithoutMeta.clear();
    } else if (cacheDone != type && type == CacheReachedType::Timeout) {
        qDebug() << "Proxy cache reached (Timeout):" << reply;
        qDebug() << "Cache limits:" << reply << ", size:" << cache.size() << "("
                 << cacheLimit.maxSize << "), time:" << time << "("
                 << cacheLimit.maxTime << ")";
    } else if (cacheDone != type && type == CacheReachedType::BuffFull) {
        qDebug() << "Proxy cache reached (BuffFull):" << reply;
        qDebug() << "Cache limits:" << reply << ", size:" << cache.size() << "("
                 << cacheLimit.maxSize << "), time:" << time << "("
                 << cacheLimit.maxTime << ")";
        cache.clear();
        cacheWithoutMeta.clear();
    }

    if (cacheDone != type) {
        qDebug() << "Cache state change:" << static_cast<int>(cacheDone) << "->"
                 << static_cast<int>(type);
        cacheDone = type;
    }

    return cacheDone;
}

void ContentServerWorker::Proxy::Source::resetCacheTimer() {
    const auto cacheState = maxCacheReached();

    if (cacheState != CacheReachedType::NotReached) {
        cacheDone = CacheReachedType::NotReached;
    }

    cacheStart = QTime::currentTime();
    maxCacheReached();
}

void ContentServerWorker::Proxy::endAll() {
    for (auto &r : sinks) r.resp->end();
}

void ContentServerWorker::Proxy::endSinks(QNetworkReply *reply) {
    const auto matchedSinks = sourceToSinkMap.values(reply);
    for (auto &r : matchedSinks) {
        if (!r->isFinished()) r->end();
    }
}

void ContentServerWorker::Proxy::sendEmptyResponseAll(QNetworkReply *reply,
                                                      int code) {
    for (auto &resp : sourceToSinkMap.values(reply)) {
        ContentServerWorker::sendEmptyResponse(resp, code);
    }
}

void ContentServerWorker::Proxy::sendResponseAll(QNetworkReply *reply, int code,
                                                 const QByteArray &data) {
    for (auto &resp : sourceToSinkMap.values(reply)) {
        ContentServerWorker::sendResponse(resp, code, data);
    }
}

void ContentServerWorker::Proxy::sendRedirectionAll(QNetworkReply *reply,
                                                    const QString &location) {
    for (auto &resp : sourceToSinkMap.values(reply)) {
        ContentServerWorker::sendRedirection(resp, location);
    }
}

bool ContentServerWorker::Proxy::shouldBeRemoved() {
    return sinks.isEmpty() &&
           (sources.isEmpty() ||
            std::all_of(sources.begin(), sources.end(),
                        [this](auto &s) { return sourceShouldBeRemoved(s); }));
}

bool ContentServerWorker::Proxy::sourceShouldBeRemoved(Source &source) {
    const auto cacheState = source.maxCacheReached();
    return connected && cacheState != CacheReachedType::NotReached &&
           !sourceToSinkMap.contains(source.reply);
}

void ContentServerWorker::Proxy::removeDeadSources() {
    const auto replies = sources.keys();
    for (auto r : replies) {
        auto &source = sources[r];
        if (sourceShouldBeRemoved(source)) removeSource(source.reply);
    }
}

void ContentServerWorker::removeDeadProxies() {
    qDebug() << "Checking for dead proxies and sources";
    proxiesCleanupTimer.stop();

    const auto ids = proxies.keys();
    for (auto &id : ids) {
        auto &proxy = proxies[id];

        const auto replies = proxy.sources.keys();
        for (auto r : replies) {
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
    foreach (const auto &source, sources) {
        if (source.recordable()) return true;
    }

    return false;
}

bool ContentServerWorker::Proxy::closeRecFile() {
    bool ret = false;
    foreach (const Source &s, sources) {
        if (s.recFile) {
            s.recFile->remove();
            ret = true;
        }
    }
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
        qDebug() << "Opening file for recording:" << recFilePath;
        recFile->setFileName(recFilePath);
        if (recFile->open(QIODevice::WriteOnly)) return true;
        qWarning() << "File for recording cannot be open";
    }

    return false;
}

std::optional<QString> ContentServerWorker::Proxy::Source::endRecFile(
    const Proxy &proxy) {
    if (!recFile) return std::nullopt;

    recFile->close();

    const auto title =
        hasMeta()
            ? ContentServer::streamTitleFromShoutcastMetadata(proxy.metadata)
            : proxy.title;

    if (title.isEmpty()) {
        recFile->remove();
        return std::nullopt;
    }

    qDebug() << "Saving tmp recorded file:" << title;

    if (!recFile->exists() || recFile->size() <= ContentServer::recMinSize) {
        qDebug() << "Recorded file doesn't exist or tiny size";
        recFile->remove();
        return std::nullopt;
    }

    const auto mime =
        reply->header(QNetworkRequest::ContentTypeHeader).toString();
    const auto tmpFile = QString{"%1.tmp.%2"}.arg(
        recFile->fileName(),
        ContentServer::getExtensionFromAudioContentType(mime));

    if (!recFile->copy(tmpFile)) {
        qWarning() << "Cannot copy file:" << recFile->fileName() << tmpFile;
        recFile->remove();
        return std::nullopt;
    }

    recFile->remove();

    const auto url = proxy.origUrl.isEmpty()
                         ? Utils::urlFromId(proxy.id).toString()
                         : proxy.origUrl.toString();

    ContentServer::writeMetaUsingTaglib(
        tmpFile,                                   // path
        title,                                     // title
        metaint > 0 ? proxy.title : proxy.author,  // author
        ContentServer::recAlbumName,               // album
        {},                                        // comment
        url,                                       // URL
        QDateTime::currentDateTime(),              // time of recording
        proxy.artPath);

    return tmpFile;
}

std::optional<std::pair<QString, QString>>
ContentServerWorker::Proxy::Source::saveRecFile(const Proxy &proxy) {
    if (!recFile) return std::nullopt;

    recFile->close();

    if (!proxy.saveRec) {
        recFile->remove();
        return std::nullopt;
    }

    const auto title =
        hasMeta()
            ? ContentServer::streamTitleFromShoutcastMetadata(proxy.metadata)
            : proxy.title;

    if (title.isEmpty()) {
        recFile->remove();
        return std::nullopt;
    }

    qDebug() << "Saving recorded file for title:" << title;

    if (!recFile->exists() || recFile->size() <= ContentServer::recMinSize) {
        qDebug() << "Recorded file doesn't exist or tiny size";
        recFile->remove();
        return std::nullopt;
    }

    const auto mime =
        reply->header(QNetworkRequest::ContentTypeHeader).toString();
    const auto recFilePath = QDir{Settings::instance()->getRecDir()}.filePath(
        QString{"%1.%2.%3.%4"}.arg(
            Utils::escapeName(title), Utils::instance()->randString(),
            "jupii_rec",
            ContentServer::getExtensionFromAudioContentType(mime)));

    if (!recFile->copy(recFilePath)) {
        qWarning() << "Cannot copy file:" << recFile->fileName() << recFilePath;
        recFile->remove();
        return std::nullopt;
    }

    recFile->remove();

    const auto url = proxy.origUrl.isEmpty()
                         ? Utils::urlFromId(proxy.id).toString()
                         : proxy.origUrl.toString();

    ContentServer::writeMetaUsingTaglib(
        recFilePath,                               // path
        title,                                     // title
        metaint > 0 ? proxy.title : proxy.author,  // author
        ContentServer::recAlbumName,               // album
        {},                                        // comment
        url,                                       // URL
        QDateTime::currentDateTime(),              // time of recording
        proxy.artPath);

    return std::pair{title, recFilePath};
}

void ContentServerWorker::Range::updateLength(const int length) {
    this->length = length;
    if (end == -1 && length > -1) end = length - 1;
}

bool ContentServerWorker::Proxy::minCacheReached() const {
    if (!sinks.isEmpty()) return true;

    bool firstReached = false;
    for (auto &source : sources) {
        if (source.minCacheReached()) {
            if (source.first) firstReached = true;
        } else {
            return false;
        }
    }

    return firstReached;
}

bool ContentServerWorker::Proxy::maxCacheReached() {
    for (auto &source : sources) {
        if (source.maxCacheReached() != CacheReachedType::NotReached) {
            return true;
        }
    };
    return false;
}

void ContentServerWorker::Proxy::resetCacheTimer() {
    for (auto &source : sources) {
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
    debug.nospace()
        << "Range("
        << QString{"%1-%2/%3"}.arg(range.start).arg(range.end).arg(range.length)
        << ")";
    return debug;
}

void ContentServerWorker::Proxy::updateRageLength(const Source &source) {
    if (source.length < 0) return;
    if (!sourceToSinkMap.contains(source.reply)) return;

    for (auto it = sourceToSinkMap.cbegin(); it != sourceToSinkMap.cend();
         ++it) {
        if (it.key() == source.reply && sinks.contains(it.value())) {
            auto &sink = sinks[it.value()];
            if (sink.range) sink.range->updateLength(source.length);
        }
    }
}
