/* Copyright (C) 2017-2019 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "contentserver.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QRegExp>
#include <QImage>
#include <QTextStream>
#include <QRegExp>
#include <QTimer>
#include <QAudioFormat>
#include <QAudioDeviceInfo>
#include <QAudioInput>
#include <QDomDocument>
#include <QDomElement>
#include <QDomNode>
#include <QDomNodeList>
#include <QDomText>
#include <QSslConfiguration>
#include <QTextStream>
#include <QStandardPaths>
#include <QEventLoop>
#include <iomanip>
#include <limits>

#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusConnection>

#include "utils.h"
#include "settings.h"
#include "tracker.h"
#include "trackercursor.h"
#include "info.h"
#include "services.h"
#include "contentdirectory.h"
#include "log.h"
#include "libupnpp/control/cdirectory.hxx"

// TagLib
#include "fileref.h"
#include "tag.h"
#include "tpropertymap.h"
#include "mpegfile.h"
#include "id3v2frame.h"
#include "id3v2tag.h"
#include "attachedpictureframe.h"

#ifdef SAILFISH
#include <sailfishapp.h>
#include "iconprovider.h"
#endif

ContentServer* ContentServer::m_instance = nullptr;
ContentServerWorker* ContentServerWorker::m_instance = nullptr;

const QString ContentServer::queryTemplate =
        "SELECT ?item " \
        "nie:mimeType(?item) as mime " \
        "nie:title(?item) as title " \
        "nie:comment(?item) as comment " \
        "nfo:duration(?item) as duration " \
        "nie:title(nmm:musicAlbum(?item)) as album " \
        "nmm:artistName(nmm:performer(?item)) as artist " \
        "nfo:averageBitrate(?item) as bitrate " \
        "nfo:channels(?item) as channels " \
        "nfo:sampleRate(?item) as sampleRate " \
        "WHERE { ?item nie:url \"%1\". }";

const QHash<QString,QString> ContentServer::m_imgExtMap {
    {"jpg", "image/jpeg"},{"jpeg", "image/jpeg"},
    {"png", "image/png"},
    {"gif", "image/gif"}
};

const QHash<QString,QString> ContentServer::m_musicExtMap {
    {"mp3", "audio/mpeg"},
    {"m4a", "audio/mp4"}, {"m4b","audio/mp4"},
    {"aac", "audio/aac"},
    {"mpc", "audio/x-musepack"},
    {"flac", "audio/flac"},
    {"wav", "audio/vnd.wav"},
    {"ape", "audio/x-monkeys-audio"},
    {"ogg", "audio/ogg"}, {"oga", "audio/ogg"},
    {"wma", "audio/x-ms-wma"},
    {"tsa", "audio/MP2T"}
};

const QHash<QString,QString> ContentServer::m_musicMimeToExtMap {
    {"audio/mpeg", "mp3"},
    {"audio/mp4", "m4a"},
    // Disabling AAC because TagLib doesn't support ADTS https://github.com/taglib/taglib/issues/508
    //{"audio/aac", "aac"},
    //{"audio/aacp", "aac"},
    {"audio/ogg", "ogg"},
    {"application/ogg", "ogg"}
};

const QHash<QString,QString> ContentServer::m_videoExtMap {
    {"mkv", "video/x-matroska"},
    {"webm", "video/webm"},
    {"flv", "video/x-flv"},
    {"ogv", "video/ogg"},
    {"avi", "video/x-msvideo"},
    {"mov", "video/quicktime"}, {"qt", "video/quicktime"},
    {"wmv", "video/x-ms-wmv"},
    {"mp4", "video/mp4"}, {"m4v", "video/mp4"},
    {"mpg", "video/mpeg"}, {"mpeg", "video/mpeg"}, {"m2v", "video/mpeg"},
    {"ts", "video/MP2T"}, {"tsv", "video/MP2T"}
    //{"ts", "video/vnd.dlna.mpeg-tts"}
};

const QHash<QString,QString> ContentServer::m_playlistExtMap {
    {"m3u", "audio/x-mpegurl"},
    {"pls", "audio/x-scpls"},
    {"xspf", "application/xspf+xml"}
};

const QStringList ContentServer::m_m3u_mimes {
    "application/vnd.apple.mpegurl",
    "application/mpegurl",
    "application/x-mpegurl",
    "audio/mpegurl",
    "audio/x-mpegurl"
};

const QStringList ContentServer::m_pls_mimes {
    "audio/x-scpls"
};

const QStringList ContentServer::m_xspf_mimes {
    "application/xspf+xml"
};

const QString ContentServer::genericAudioItemClass = "object.item.audioItem";
const QString ContentServer::genericVideoItemClass = "object.item.videoItem";
const QString ContentServer::genericImageItemClass = "object.item.imageItem";
const QString ContentServer::audioItemClass = "object.item.audioItem.musicTrack";
const QString ContentServer::videoItemClass = "object.item.videoItem.movie";
const QString ContentServer::imageItemClass = "object.item.imageItem.photo";
const QString ContentServer::playlistItemClass = "object.item.playlistItem";
const QString ContentServer::broadcastItemClass = "object.item.audioItem.audioBroadcast";
const QString ContentServer::defaultItemClass = "object.item";

const QString ContentServer::artCookie = "jupii_art";

const QByteArray ContentServer::userAgent = QString("%1 %2")
        .arg(Jupii::APP_NAME, Jupii::APP_VERSION).toLatin1();

/* DLNA.ORG_OP flags:
 * 00 - no seeking allowed
 * 01 - seek by byte
 * 10 - seek by time
 * 11 - seek by both*/
const QString ContentServer::dlnaOrgOpFlagsSeekBytes = "DLNA.ORG_OP=01";
const QString ContentServer::dlnaOrgOpFlagsNoSeek = "DLNA.ORG_OP=00";
const QString ContentServer::dlnaOrgCiFlags = "DLNA.ORG_CI=0";

// Rec
const QString ContentServer::recDateTagName = "Recording Date";
const QString ContentServer::recUrlTagName = "Station URL";
const QString ContentServer::recStationTagName = "Station Name";
const QString ContentServer::recAlbumName = "Recordings by Jupii";

ContentServerWorker* ContentServerWorker::instance(QObject *parent)
{
    if (ContentServerWorker::m_instance == nullptr) {
        ContentServerWorker::m_instance = new ContentServerWorker(parent);
    }

    return ContentServerWorker::m_instance;
}

ContentServerWorker::ContentServerWorker(QObject *parent) :
    QObject(parent),
    server(new QHttpServer(parent)),
    nam(new QNetworkAccessManager(parent))
{
    connect(server, &QHttpServer::newRequest,
                     this, &ContentServerWorker::requestHandler);
    connect(this, &ContentServerWorker::contSeqWriteData,
                     this, &ContentServerWorker::seqWriteData,
                     Qt::QueuedConnection);

    if (!server->listen(static_cast<quint16>(Settings::instance()->getPort()))) {
        qWarning() << "Unable to start HTTP server!";
        //TODO: Handle: Unable to start HTTP server
    }
    pulseSource = std::unique_ptr<PulseAudioSource>(new PulseAudioSource());
    cleanCacheFiles();
}

bool ContentServerWorker::isStreamToRecord(const QUrl &id)
{
    proxyItemsMutex.lock();
    for (ProxyItem& item : proxyItems) {
        if (item.id == id) {
            proxyItemsMutex.unlock();
            return item.saveRec;
        }
    }
    proxyItemsMutex.unlock();

    return false;
}

bool ContentServerWorker::isStreamRecordable(const QUrl &id)
{
    proxyItemsMutex.lock();
    for (ProxyItem& item : proxyItems) {
        if (item.id == id) {
            proxyItemsMutex.unlock();
            return item.recFile && item.recFile->isOpen();
        }
    }
    proxyItemsMutex.unlock();

    return false;
}

void ContentServerWorker::closeRecFile(ProxyItem &item)
{
    if (item.recFile) {
        item.recFile->remove();
        item.saveRec = false;
        emit streamToRecordChanged(item.id, item.saveRec);
        emit streamRecordableChanged(item.id, false);
    }
}

void ContentServerWorker::cleanCacheFiles()
{
    auto recDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QDir dir(recDir);
    dir.setNameFilters(QStringList() << "rec-*.*" << "passlogfile-*.*");
    dir.setFilter(QDir::Files);
    for (const QString& f : dir.entryList()) {
        qDebug() << "Removing old cache file:" << f;
        dir.remove(f);
    }
}

void ContentServerWorker::openRecFile(ProxyItem &item)
{
    if (item.recFile) {
        item.recFile->remove();
        auto recDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
        auto recFilePath = QDir(recDir).filePath(QString("rec-%1.%2").arg(
                                        Utils::randString(), item.recExt));
        qDebug() << "Opening file for recording:" << recFilePath;
        item.recFile->setFileName(recFilePath);
        item.recFile->remove();
        if (item.recFile->open(QIODevice::WriteOnly)) {
            item.saveRec = false;
            emit streamToRecordChanged(item.id, item.saveRec);
            emit streamRecordableChanged(item.id, true);
        } else {
            qWarning() << "File for recording cannot be open";
        }
    }
}

void ContentServerWorker::saveRecFile(ProxyItem &item)
{
    if (item.recFile) {
        if (item.saveRec) {
            auto title = ContentServer::streamTitleFromShoutcastMetadata(item.metadata);
            if (!title.isEmpty()) {
                qDebug() << "Saving recorded file for title:" << title;
                item.recFile->close();
                auto recFilePath = QDir(Settings::instance()->getRecDir()).filePath(
                            QString("%1.%2.%3.%4").arg(title, Utils::randString(),
                                                       "jupii_rec", item.recExt));
                if (item.recFile->exists() && item.recFile->size() > ContentServer::recMinSize) {
                    if (item.recFile->copy(recFilePath)) {
                        auto url = Utils::urlFromId(item.id).toString();
                        ContentServer::writeMetaUsingTaglib(
                            recFilePath, // path
                            title, // title
                            item.title, // author (radio station name)
                            ContentServer::recAlbumName, // album
                            tr("Recorded from %1").arg(item.title), // comment
                            item.title, // radio station name
                            url, // radio station URL
                            QDateTime::currentDateTime() // time of recording
                        );
                        emit streamRecorded(title, recFilePath);
                    } else {
                        qWarning() << "Cannot copy file:"
                                   << item.recFile->fileName() << recFilePath;
                    }
                } else {
                    qWarning() << "Recorded file doesn't exist or tiny size:"
                               << item.recFile->fileName();
                }
            } else {
                qWarning() << "Title is null so not saving recorded file";
            }
        }
        item.recFile->remove();
    }

    item.saveRec = false;
    emit streamToRecordChanged(item.id, item.saveRec);
    emit streamRecordableChanged(item.id, false);
}

ContentServerWorker::ProxyItem::~ProxyItem()
{
    if (this->id.isValid()) {
        auto worker = ContentServerWorker::instance();
        worker->saveRecFile(*this);
    }
}

void ContentServerWorker::requestHandler(QHttpRequest *req, QHttpResponse *resp)
{
    qDebug() << ">>> requestHandler thread:" << QThread::currentThreadId();
    qDebug() << "  method:" << req->methodString();
    qDebug() << "  URL:" << req->url().path();
    qDebug() << "  headers:" << req->url().path();

    const auto& headers = req->headers();
    for (const auto& h : headers.keys()) {
        qDebug() << "    " << h << ":" << headers.value(h);
    }

    if (req->method() != QHttpRequest::HTTP_GET &&
        req->method() != QHttpRequest::HTTP_HEAD) {
        qWarning() << "Request method is unsupported";
        resp->setHeader("Allow", "HEAD, GET");
        sendEmptyResponse(resp, 405);
        return;
    }

    bool valid, isArt;
    auto id = ContentServer::idUrlFromUrl(req->url(), &valid, nullptr, &isArt);

    qDebug() << "Id:" << id.toString();

    if (!valid) {
        qWarning() << "Unknown content requested!";
        sendEmptyResponse(resp, 404);
        return;
    }

    auto cs = ContentServer::instance();

    const ContentServer::ItemMeta *meta;

    if (isArt) {
        qWarning() << "Requested content is art";
        meta = cs->makeMetaUsingExtension(id);
        requestForFileHandler(id, meta, req, resp);
        delete meta;
        return;
    } else {
        meta = cs->getMetaForId(id);
        if (!meta) {
            qWarning() << "No meta item found";
            sendEmptyResponse(resp, 404);
            return;
        }
    }

    if (meta->itemType == ContentServer::ItemType_LocalFile) {
        requestForFileHandler(id, meta, req, resp);
    } else if (meta->itemType == ContentServer::ItemType_ScreenCapture) {
        requestForScreenCaptureHandler(id, meta, req, resp);
    } else if (meta->itemType == ContentServer::ItemType_Mic) {
        requestForMicHandler(id, meta, req, resp);
    } else if (meta->itemType == ContentServer::ItemType_AudioCapture) {
        requestForAudioCaptureHandler(id, meta, req, resp);
    } else if (meta->itemType == ContentServer::ItemType_Url) {
        requestForUrlHandler(id, meta, req, resp);
    } else {
        qWarning() << "Unknown item type";
        sendEmptyResponse(resp, 404);
        return;
    }
}

void ContentServerWorker::responseForAudioCaptureDone()
{
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
        audioCaster.reset(nullptr);
    }
}

#ifdef SCREENCAST
void ContentServerWorker::responseForScreenCaptureDone()
{
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
        screenCaster.reset(nullptr);
    }
}

void ContentServerWorker::screenErrorHandler()
{
    qWarning() << "Error in screen casting, "
                  "so disconnecting clients and ending casting";
    for (auto& item : screenCaptureItems) {
        item.resp->end();
    }
    for (auto& item : screenCaptureItems) {
        emit itemRemoved(item.id);
    }

    screenCaptureItems.clear();
    screenCaster.reset(nullptr);
}
#endif

void ContentServerWorker::audioErrorHandler()
{
    qWarning() << "Error in audio casting, "
                  "so disconnecting clients and ending casting";
    for (auto& item : audioCaptureItems) {
        item.resp->end();
    }
    for (auto& item : audioCaptureItems) {
        emit itemRemoved(item.id);
    }

    audioCaptureItems.clear();
    audioCaster.reset(nullptr);
}

void ContentServerWorker::micErrorHandler()
{
    qWarning() << "Error in mic casting, "
                  "so disconnecting clients and ending casting";
    for (auto& item : micItems) {
        item.resp->end();
    }
    for (auto& item : micItems) {
        emit itemRemoved(item.id);
    }

    micItems.clear();
    micCaster.reset(nullptr);
}

void ContentServerWorker::requestForFileHandler(const QUrl &id,
                                                const ContentServer::ItemMeta* meta,
                                                QHttpRequest *req, QHttpResponse *resp)
{
    auto type = static_cast<ContentServer::Type>(Utils::typeFromId(id));
    if (meta->type == ContentServer::TypeVideo &&
        type == ContentServer::TypeMusic) {
        if (id.scheme() == "qrc") {
            qWarning() << "Unable to extract audio stream from qrc";
            sendEmptyResponse(resp, 500);
            return;
        }
        qDebug() << "Video content and type is audio => extracting audio stream";
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

void ContentServerWorker::requestForUrlHandler(const QUrl &id,
                                                const ContentServer::ItemMeta *meta,
                                                QHttpRequest *req, QHttpResponse *resp)
{
    auto url = Utils::urlFromId(id);
    auto s = Settings::instance();
    // 0 - proxy for all
    // 1 - redirection for all
    // 2 - none for all
    // 3 - proxy for shoutcast, redirection for others
    // 4 - proxy for shoutcast, none for others
    int relay = s->getRemoteContentMode();

    if (relay == 1 || relay == 3) {
        // Redirection mode
        qDebug() << "Redirection mode enabled => sending HTTP redirection";
        sendRedirection(resp, url.toString());
    } else if (relay == 2) {
        // None
        qWarning() << "Relaying is disabled";
        sendEmptyResponse(resp, 500);
    } else {
        // Proxy mode
        qDebug() << "Proxy mode enabled => creating proxy";
        qDebug() << "Proxy items count:" << proxyItems.size();

        QNetworkRequest request;
        request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
        request.setUrl(url);

        // Add headers
        const auto& headers = req->headers();
        if (headers.contains("range"))
            request.setRawHeader("Range", headers.value("range").toLatin1());
        request.setRawHeader("Icy-MetaData", "1");
        request.setRawHeader("Connection", "close");
        request.setRawHeader("User-Agent", ContentServer::userAgent);

        QNetworkReply *reply;
        bool head = req->method() == QHttpRequest::HTTP_HEAD;
        qDebug() << (head ? "HEAD" : "GET") << "request for url:" << url;

        // Always sending GET request to remote server because some
        // DLNA renderers are confused when they receives error for HEAD
        reply = nam->get(request);

        ProxyItem &item = proxyItems[reply];
        item.req = req;
        item.resp = resp;
        item.reply = reply;
        item.id = id;
        item.meta = headers.contains("icy-metadata");
        item.seek = meta->seekSupported;
        item.mode = meta->mode;
        item.head = head; // orig request is HEAD

        QString name;
        Utils::pathTypeNameCookieIconFromId(item.id, nullptr, nullptr, &name);
        item.title = name.isEmpty() ? ContentServer::bestName(*meta) : name;

        responseToReplyMap.insert(resp, reply);

        connect(reply, &QNetworkReply::metaDataChanged,
                this, &ContentServerWorker::proxyMetaDataChanged);
        connect(reply, &QNetworkReply::redirected,
                this, &ContentServerWorker::proxyRedirected);
        connect(reply, &QNetworkReply::finished,
                this, &ContentServerWorker::proxyFinished);
        connect(reply, &QNetworkReply::readyRead,
                this, &ContentServerWorker::proxyReadyRead);
        connect(resp, &QHttpResponse::done,
                this, &ContentServerWorker::responseForUrlDone);

        emit itemAdded(item.id);
    }
}

void ContentServerWorker::requestForMicHandler(const QUrl &id,
                                                const ContentServer::ItemMeta *meta,
                                                QHttpRequest *req, QHttpResponse *resp)
{
    bool isHead = req->method() == QHttpRequest::HTTP_HEAD;

    if (isHead) {
        qDebug() << "Sending 200 response without content";
        resp->setHeader("Content-Type", meta->mime);
        resp->setHeader("Connection", "close");
        resp->setHeader("transferMode.dlna.org", "Streaming");
        resp->setHeader("contentFeatures.dlna.org",
                        ContentServer::dlnaContentFeaturesHeader(meta->mime,
                                                  meta->seekSupported));
        //resp->setHeader("Transfer-Encoding", "chunked");
        resp->setHeader("Accept-Ranges", "none");
        sendResponse(resp, 200, "");
    } else {
        qDebug() << "Sending 200 response and starting streaming";
        resp->writeHead(200);

        if (!micCaster) {
            micCaster = std::unique_ptr<MicCaster>(new MicCaster());
            if (!micCaster->init()) {
                qWarning() << "Cannot init mic caster";
                micCaster.reset(nullptr);
                sendEmptyResponse(resp, 500);
                return;
            }
        }

        resp->setHeader("Content-Type", meta->mime);
        resp->setHeader("Connection", "close");
        resp->setHeader("transferMode.dlna.org", "Streaming");
        resp->setHeader("contentFeatures.dlna.org",
                        ContentServer::dlnaContentFeaturesHeader(meta->mime,
                                                  meta->seekSupported));
        //resp->setHeader("Transfer-Encoding", "chunked");
        resp->setHeader("Accept-Ranges", "none");

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
}

void ContentServerWorker::requestForAudioCaptureHandler(const QUrl &id,
                                                const ContentServer::ItemMeta *meta,
                                                QHttpRequest *req, QHttpResponse *resp)
{
    qDebug() << "Audio capture request handler";
    bool isHead = req->method() == QHttpRequest::HTTP_HEAD;

    if (isHead) {
        qDebug() << "Sending 200 response without content";
        resp->setHeader("Content-Type", meta->mime);
        resp->setHeader("Connection", "close");
        resp->setHeader("transferMode.dlna.org", "Streaming");
        resp->setHeader("contentFeatures.dlna.org",
                        ContentServer::dlnaContentFeaturesHeader(meta->mime,
                                                  meta->seekSupported));
        //resp->setHeader("Transfer-Encoding", "chunked");
        resp->setHeader("Accept-Ranges", "none");
        sendResponse(resp, 200, "");
    } else {
        if (!audioCaster) {
            audioCaster = std::unique_ptr<AudioCaster>(new AudioCaster());
            if (!audioCaster->init()) {
                qWarning() << "Cannot init audio caster";
                audioCaster.reset(nullptr);
                sendEmptyResponse(resp, 500);
                return;
            }
        }

        if (!pulseSource->start()) {
            qWarning() << "Cannot init pulse audio";
            audioCaster.reset(nullptr);
            sendEmptyResponse(resp, 500);
            return;
        }

        qDebug() << "Sending 200 response and starting streaming";
        resp->setHeader("Content-Type", meta->mime);
        resp->setHeader("Connection", "close");
        resp->setHeader("transferMode.dlna.org", "Streaming");
        resp->setHeader("contentFeatures.dlna.org",
                        ContentServer::dlnaContentFeaturesHeader(meta->mime,
                                                  meta->seekSupported));
        //resp->setHeader("Transfer-Encoding", "chunked");
        resp->setHeader("Accept-Ranges", "none");
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
}

void ContentServerWorker::requestForScreenCaptureHandler(const QUrl &id,
                                                const ContentServer::ItemMeta *meta,
                                                QHttpRequest *req, QHttpResponse *resp)
{
    qDebug() << "Screen capture request handler";
#ifndef SCREENCAST
    qWarning() << "Screen capturing is disabled";
    sendEmptyResponse(resp, 500);
#else
    auto s = Settings::instance();

    if (!s->getScreenSupported()) {
        qWarning() << "Screen capturing is not supported";
        sendEmptyResponse(resp, 500);
        return;
    }


    bool isHead = req->method() == QHttpRequest::HTTP_HEAD;
    if (isHead) {
        qDebug() << "Sending 200 response without content";
        resp->setHeader("Content-Type", meta->mime);
        resp->setHeader("Connection", "close");
        resp->setHeader("transferMode.dlna.org", "Streaming");
        resp->setHeader("contentFeatures.dlna.org",
                        ContentServer::dlnaContentFeaturesHeader(meta->mime,
                                                  meta->seekSupported));
        //resp->setHeader("Transfer-Encoding", "chunked");
        resp->setHeader("Accept-Ranges", "none");
        sendResponse(resp, 200, "");
    } else {
        qDebug() << "Sending 200 response and starting streaming";

        bool startNeeded = false;
        if (!screenCaster) {
            screenCaster = std::unique_ptr<ScreenCaster>(new ScreenCaster());
            if (!screenCaster->init()) {
                qWarning() << "Cannot init screen capture";
                screenCaster.reset(nullptr);
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
            if (s->getScreenAudio()) {
                if (!pulseSource->start()) {
                    qWarning() << "Pulse cannot be started";
                    sendEmptyResponse(resp, 500);
                    return;
                }
            }
        }

        resp->setHeader("Content-Type", meta->mime);
        resp->setHeader("Connection", "close");
        resp->setHeader("transferMode.dlna.org", "Streaming");
        resp->setHeader("contentFeatures.dlna.org",
                        ContentServer::dlnaContentFeaturesHeader(meta->mime,
                                                  meta->seekSupported));
        //resp->setHeader("Transfer-Encoding", "chunked");
        resp->setHeader("Accept-Ranges", "none");
        resp->writeHead(200);

        PulseAudioSource::discoverStream();
    }
#endif
}

void ContentServerWorker::seqWriteData(QFile *file, qint64 size, QHttpResponse *resp)
{
    if (resp->isFinished()) {
        qWarning() << "Connection closed by server, so skiping data sending";
    } else {
        qint64 rlen = size;
        const qint64 len = rlen < ContentServer::qlen ? rlen : ContentServer::qlen;
        //qDebug() << "Sending" << len << "of data";
        QByteArray data; data.resize(static_cast<int>(len));
        auto cdata = data.data();
        auto count = static_cast<int>(file->read(cdata, len));
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
    file->close();
    delete file;
}

void ContentServerWorker::sendEmptyResponse(QHttpResponse *resp, int code)
{
    resp->setHeader("Content-Length", "0");
    resp->writeHead(code);
    resp->end();
}

void ContentServerWorker::sendResponse(QHttpResponse *resp, int code, const QByteArray &data)
{
    resp->writeHead(code);
    resp->end(data);
}

void ContentServerWorker::sendRedirection(QHttpResponse *resp, const QString &location)
{
    resp->setHeader("Location", location);
    resp->setHeader("Content-Length", "0");
    resp->setHeader("Connection", "close");
    resp->writeHead(302);
    resp->end();
}

void ContentServerWorker::responseForMicDone()
{
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
        micCaster.reset(nullptr);
    }
}

void ContentServerWorker::responseForUrlDone()
{
    qDebug() << "Response done";
    auto resp = dynamic_cast<QHttpResponse*>(sender());
    if (responseToReplyMap.contains(resp)) {
        auto reply = responseToReplyMap.take(resp);
        if (proxyItems.contains(reply)) {
            auto &item = proxyItems[reply];
            item.finished = true;
            qDebug() << "Setting proxy item as finished:" << item.id;
        }
        if (reply->isFinished()) {
            qDebug() << "Reply already finished";
        } else {
            qDebug() << "Aborting reply";
            reply->abort();
        }
    } else {
        qWarning() << "Unknown response done";
    }
}

void ContentServerWorker::proxyMetaDataChanged()
{
    qDebug() << "Request meta data received";
    auto reply = dynamic_cast<QNetworkReply*>(sender());

    if (!proxyItems.contains(reply)) {
        qWarning() << "Cannot find proxy item";
        return;
    }

    auto &item = proxyItems[reply];

    auto code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    auto reason = reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();
    auto mime = reply->header(QNetworkRequest::ContentTypeHeader).toString();
    auto error = reply->error();
    bool head = item.req->method() == QHttpRequest::HTTP_HEAD;

    // -- debug --
    qDebug() << "Request:" << (head ? "HEAD" : "GET")
             << item.id;
    qDebug() << "Reply status:" << code << reason;
    qDebug() << "Error code:" << error;
    qDebug() << "Headers:";
    for (const auto &p : reply->rawHeaderPairs()) {
        qDebug() << p.first << p.second;
    }
    // -- debug --

    if (error != QNetworkReply::NoError || code > 299) {
        qWarning() << "Error response from network server";
        if (code < 400) {
            code = 404;
        }
        qDebug() << "Ending request with code:" << code;
        sendEmptyResponse(item.resp, code);
    } else if (mime.isEmpty()) {
        qWarning() << "No content type header receive from network server";
        qDebug() << "Ending request with code:" << 404;
        sendEmptyResponse(item.resp, 404);
    } else if (item.state == 0) {
        item.resp->setHeader("transferMode.dlna.org", "Streaming");
        item.resp->setHeader("contentFeatures.dlna.org",
                             ContentServer::dlnaContentFeaturesHeader(mime, item.seek));
        item.resp->setHeader("Content-Type", mime);
        item.resp->setHeader("Connection", "close");
        if (reply->header(QNetworkRequest::ContentLengthHeader).isValid()) {
            item.resp->setHeader("Content-Length",
                                 reply->header(QNetworkRequest::ContentLengthHeader).toString());
        } /*else {
            item.resp->setHeader("Transfer-Encoding", "chunked");
        }*/
        if (reply->hasRawHeader("Accept-Ranges")) {
            item.resp->setHeader("Accept-Ranges", reply->rawHeader("Accept-Ranges"));
        }
        if (reply->hasRawHeader("Content-Range")) {
            item.resp->setHeader("Content-Range", reply->rawHeader("Content-Range"));
        } else {
            if (!reply->hasRawHeader("Accept-Ranges"))
                item.resp->setHeader("Accept-Ranges", "none");
        }

        if (reply->hasRawHeader("icy-metaint")) {
            item.metaint = reply->rawHeader("icy-metaint").toInt();
            qDebug() << "Shoutcast stream has metadata. Interval is"
                     << item.metaint;
        }

        // copying icy-* headers
        const auto &headers = reply->rawHeaderPairs();
        for (const auto& h : headers) {
            if (h.first.toLower().startsWith("icy-"))
                item.resp->setHeader(h.first, h.second);
        }

        if (item.head) {
            qDebug() << "Orig reqest is HEAD, so ending proxy request with code:" << code;
            item.resp->writeHead(code);
            item.resp->end();
        } else {
            // state change
            // stream proxy (0) => sending partial data every ready read signal (1)
            // playlist proxy (1) => sending all data when request finished (2)
            item.state = item.mode == 1 ? 2 : 1;

            // recording only when: stream proxy mode && shoutcast && valid audio extension
            if (item.state == 1 && item.metaint > 0 &&
                Settings::instance()->getRec()) {
                auto ext = ContentServer::getExtensionFromAudioContentType(mime);
                if (!ext.isEmpty()) {
                    qDebug() << "Stream should be recorded";
                    item.recExt = ext;
                    item.recFile = std::make_shared<QFile>();
                }
            }

            qDebug() << "Sending head for request with code:" << code;
            item.resp->writeHead(code);
            return;
        }
    }

    if (!reply->isFinished()) {
        qDebug() << "Aborting reply";
        reply->abort();
    }
}

void ContentServerWorker::proxyRedirected(const QUrl &url)
{
    qDebug() << "Request redirected to:" << url;
}

void ContentServerWorker::proxyFinished()
{
    qDebug() << "Request finished";
    auto reply = dynamic_cast<QNetworkReply*>(sender());

    if (!proxyItems.contains(reply)) {
        qWarning() << "Deleting reply because cannot find proxy item";
        reply->deleteLater();
        return;
    }

    auto &item = proxyItems[reply];

    if (!item.finished) {
        auto code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        auto reason = reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();
        auto error = reply->error();
        qDebug() << "Request:" << (item.req->method() == QHttpRequest::HTTP_GET ? "GET" : "HEAD")
                 << item.id;
        qDebug() << "Error code:" << error;

        if (item.state == 0) {
            if (code < 200)
                code = 404;
            qDebug() << "Ending request with code:" << code;
            sendEmptyResponse(item.resp, code);
        } else if (item.state == 2) {
            qDebug() << "Playlist proxy mode, so sending all data";
            auto data = reply->readAll();
            if (!data.isEmpty()) {
                // Resolving relative URLs in a playlist
                ContentServer::resolveM3u(data, reply->url().toString());
                item.resp->write(data);
            } else {
                qWarning() << "Data is empty";
            }
        }

        qDebug() << "Ending request";
        item.resp->end();
    } else {
        qDebug() << "Proxy item is finished";
    }

    qDebug() << "Removing proxy item";
    emit itemRemoved(item.id);
    proxyItemsMutex.lock(); proxyItems.remove(reply); proxyItemsMutex.unlock();
    responseToReplyMap.remove(item.resp);
    qDebug() << "Deleting reply";
    reply->deleteLater();
}

void ContentServerWorker::setDisplayStatus(bool status)
{
    displayStatus = status;
}

void ContentServerWorker::setStreamToRecord(const QUrl &id, bool value)
{
    for (ProxyItem& item : proxyItems) {
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

void ContentServerWorker::processShoutcastMetadata(QByteArray &data,
                                                   ProxyItem &item)
{
    auto count = data.length();
    int bytes = item.metacounter + count;
    QByteArray dataWithoutMeta;

    /*qDebug() << "========== processShoutcastMetadata ==========";
    qDebug() << "metacounter:" << item.metacounter;
    qDebug() << "metaint:" << item.metaint;
    qDebug() << "count:" << count;
    qDebug() << "bytes:" << bytes;
    qDebug() << "data size:" << data.size();*/

    if (bytes > item.metaint) {
        Q_ASSERT(item.metaint >= item.metacounter);

        int nmeta = bytes / item.metaint;
        int totalsize = 0;
        QList<QPair<int,int>> rpoints; // (start,size) to remove from data

        for (int i = 0; i < nmeta; ++i) {
            int offset = i * item.metaint + totalsize + i;
            int start = item.metaint - item.metacounter;
            int size = 16 * static_cast<uchar>(data.at(start + offset));
            int maxsize = count - (start + offset);

            /*qDebug() << "------- Soutcast metadata detected ---------";
            qDebug() << "totalsize:" << totalsize;
            qDebug() << "offset:" << offset;
            qDebug() << "start:" << start;
            qDebug() << "start+offset:" << start + offset;
            qDebug() << "metadata size:" << size;
            qDebug() << "metadata max size:" << maxsize;
            qDebug() << "data size:" << data.size();
            qDebug() << "metacounter:" << item.metacounter;
            qDebug() << "metaint:" << item.metaint;*/

            if (size > maxsize) {
                // partial metadata received
                qDebug() << "Partial metadata received";
                auto metadata = data.mid(start + offset, maxsize);
                data.remove(start + offset, maxsize);
                item.data = metadata;
                item.metacounter = bytes - metadata.size();
                return;
            } else {
                // full metadata received
                if (size > 0) {
                    auto metadata = data.mid(start + offset + 1, size);
                    if (metadata != item.metadata) {
                        // recorder
                        auto newTitle = ContentServer::streamTitleFromShoutcastMetadata(metadata);
                        auto oldTitle = ContentServer::streamTitleFromShoutcastMetadata(item.metadata);
                        qDebug() << "old metadata:" << item.metadata << oldTitle;
                        qDebug() << "new metadata:" << metadata << newTitle;
                        if (item.recFile) {
                            if (item.recFile->isOpen())
                                saveRecFile(item);
                            if (!newTitle.isEmpty())
                                openRecFile(item);
                        }
                        //--
                        item.metadata = metadata;
                        emit shoutcastMetadataUpdated(item.id, item.metadata);
                    }
                    totalsize += size;
                }

                // Metadata point to remove from stream
                rpoints.append({start + offset, size +1});
            }
        }

        item.metacounter = bytes - nmeta * (item.metaint + 1) - totalsize;

        if (!rpoints.isEmpty()) {
            if (!item.meta) {
                // Shoutcast meta data wasn't requested by client, so removing
                removePoints(rpoints, data);
            } else if (item.recFile && item.recFile->isOpen()) {
                // Removing metadata from stream for recording
                dataWithoutMeta = data; // deep copy
                removePoints(rpoints, dataWithoutMeta);
            }
        }
    } else {
        item.metacounter = bytes;
    }

    // recording
    if (item.recFile && item.recFile->isOpen()) {
        if (item.recFile->size() < ContentServer::recMaxSize)
            item.recFile->write(dataWithoutMeta.isEmpty() ? data : dataWithoutMeta);
    }
}

void ContentServerWorker::removePoints(const QList<QPair<int,int>> &rpoints,
                                       QByteArray &data)
{
    int offset = 0;
    for (auto& p : rpoints) {
        data.remove(offset + p.first, p.second);
        offset = p.second;
    }
}

void ContentServerWorker::updatePulseStreamName(const QString &name)
{
    for (const auto& item : audioCaptureItems) {
        qDebug() << "pulseStreamUpdated:" << item.id << name;
        emit pulseStreamUpdated(item.id, name);
    }
    for (const auto& item : screenCaptureItems) {
        qDebug() << "pulseStreamUpdated:" << item.id << name;
        emit pulseStreamUpdated(item.id, name);
    }
}

void ContentServerWorker::proxyReadyRead()
{
    auto reply = dynamic_cast<QNetworkReply*>(sender());

    if (!proxyItems.contains(reply)) {
        qWarning() << "Proxy ready read: Cannot find proxy item";
        return;
    }

    if (reply->isFinished()) {
        qWarning() << "Proxy ready read: Reply is finished";
    }

    auto &item = proxyItems[reply];

    if (item.state == 2) {
        // ignoring, all will be handled in proxyFinished
        return;
    }

    if (item.finished) {
        qWarning() << "Server request already finished, so ignoring";
        return;
    }

    if (item.state == 1) {
        if (!item.resp->isHeaderWritten()) {
            qWarning() << "Head not written but state=1 => this should not happen";
            qDebug() << "Aborting reply";
            reply->abort();
            return;
        }

        auto data = reply->readAll();
        if (!item.data.isEmpty()) {
            // adding cached data from previous packet
            data.prepend(item.data);
            item.data.clear();
        }

        if (data.length() > 0) {
            if (item.metaint > 0)
                processShoutcastMetadata(data, item);

            item.resp->write(data);
        }
    }
}

void ContentServerWorker::streamFileRange(QFile *file,
                                          QHttpRequest *req,
                                          QHttpResponse *resp)
{
    auto length = file->bytesAvailable();
    const auto& headers = req->headers();
    bool head = req->method() == QHttpRequest::HTTP_HEAD;

    QRegExp rx("bytes[\\s]*=[\\s]*([\\d]+)-([\\d]*)");
    if (rx.indexIn(headers.value("range")) >= 0) {
        qint64 startByte = rx.cap(1).toInt();
        qint64 endByte = (rx.cap(3) == "" ? length-1 : rx.cap(3).toInt());
        qint64 rangeLength = endByte-startByte+1;

        /*qDebug() << "Range start:" << startByte;
        qDebug() << "Range end:" << endByte;
        qDebug() << "Range length:" << rangeLength;*/

        if (endByte > length-1) {
            qWarning() << "Range end byte is higher than content lenght";
            sendEmptyResponse(resp, 416);
            file->close();
            delete file;
        } else {
            resp->setHeader("Content-Length", QString::number(rangeLength));
            resp->setHeader("Content-Range", "bytes " +
                            QString::number(startByte) + "-" +
                            QString::number(endByte) + "/" +
                            QString::number(length-1));

            const int code = startByte == 0 && endByte == length-1 ? 200 : 206;
            qDebug() << "Sending" << code << "response";
            if (head) {
                sendResponse(resp, code, "");
                file->close();
                delete file;
            } else {
                resp->writeHead(code);
                // Sending data
                file->seek(startByte);
                seqWriteData(file, rangeLength, resp);
            }
        }
    } else {
        qWarning() << "Unable to read Range header";
        sendEmptyResponse(resp, 416);
        file->close();
        delete file;
    }
}

void ContentServerWorker::streamFileNoRange(QFile *file,
                                            QHttpRequest *req,
                                            QHttpResponse *resp)
{
    Q_UNUSED(resp)
    qint64 length = file->bytesAvailable();
    bool head = req->method() == QHttpRequest::HTTP_HEAD;

    resp->setHeader("Content-Length", QString::number(length));

    if (head) {
        qDebug() << "Sending 200 response without content";
        sendResponse(resp, 200, "");
        file->close();
        delete file;
        return;
    } else {
        qDebug() << "Sending 200 response";
        resp->writeHead(200);
        qDebug() << "Sending data";
        seqWriteData(file, length, resp);
    }
}

void ContentServerWorker::streamFile(const QString& path, const QString& mime,
                           QHttpRequest *req, QHttpResponse *resp)
{
    auto file = new QFile(path);

    if (file->open(QFile::ReadOnly)) {
        const auto& headers = req->headers();
        qint64 length = file->bytesAvailable();
        bool range = headers.contains("range");
        bool head = req->method() == QHttpRequest::HTTP_HEAD;

        qDebug() << "Content file name:" << file->fileName();
        qDebug() << "Content size:" << length;
        qDebug() << "Content type:" << mime;
        qDebug() << "Content request contains Range header:" << range;
        qDebug() << "Content request is HEAD:" << head;

        resp->setHeader("Content-Type", mime);
        resp->setHeader("Accept-Ranges", "bytes");
        resp->setHeader("Connection", "close");
        resp->setHeader("Cache-Control", "no-cache");
        resp->setHeader("TransferMode.DLNA.ORG", "Streaming");
        resp->setHeader("contentFeatures.DLNA.ORG", ContentServer::dlnaContentFeaturesHeader(mime));

        if (range) {
            streamFileRange(file, req, resp);
        } else {
            streamFileNoRange(file, req, resp);
        }
    } else {
        qWarning() << "Unable to open file" << file->fileName() << "to read!";
        sendEmptyResponse(resp, 500);
        delete file;
    }
}

ContentServer::ContentServer(QObject *parent) :
    QThread(parent)
{
    qDebug() << "Creating Content Server in thread:" << QThread::currentThreadId();

    // FFMPEG stuff
#ifdef QT_DEBUG
    av_log_set_level(AV_LOG_DEBUG);
#else
    av_log_set_level(AV_LOG_ERROR);
#endif
    if (Settings::instance()->getLogToFile())
        av_log_set_callback(ffmpegLog);
    av_register_all();
    avcodec_register_all();
    avdevice_register_all();

    // starting worker
    start(QThread::NormalPriority);
/*
#ifdef SAILFISH
#ifdef SCREENCAST
    // screen on/off status
    auto bus = QDBusConnection::systemBus();
    bus.connect("com.nokia.mce", "/com/nokia/mce/signal",
                "com.nokia.mce.signal", "display_status_ind",
                this, SLOT(displayStatusChangeHandler(QString)));
#endif
#endif
*/
}

ContentServer* ContentServer::instance(QObject *parent)
{
    if (ContentServer::m_instance == nullptr) {
        ContentServer::m_instance = new ContentServer(parent);
    }

    return ContentServer::m_instance;
}

void ContentServer::displayStatusChangeHandler(QString state)
{
    qDebug() << "display status:" << state;
    emit displayStatusChanged(state == "on");
}

QString ContentServer::dlnaOrgFlagsForFile()
{
    char flags[448];
    sprintf(flags, "%s=%.8x%.24x", "DLNA.ORG_FLAGS",
            DLNA_ORG_FLAG_BYTE_BASED_SEEK |
            DLNA_ORG_FLAG_INTERACTIVE_TRANSFERT_MODE |
            DLNA_ORG_FLAG_STREAMING_TRANSFER_MODE |
            DLNA_ORG_FLAG_BACKGROUND_TRANSFER_MODE, 0);
    QString f(flags);
    qDebug() << f;
    return f;
}

QString ContentServer::dlnaOrgFlagsForStreaming()
{
    char flags[448];
    sprintf(flags, "%s=%.8x%.24x", "DLNA.ORG_FLAGS",
            DLNA_ORG_FLAG_S0_INCREASE |
            DLNA_ORG_FLAG_SN_INCREASE |
            DLNA_ORG_FLAG_CONNECTION_STALL |
            DLNA_ORG_FLAG_STREAMING_TRANSFER_MODE, 0);
    QString f(flags);
    qDebug() << f;
    return f;
}

QString ContentServer::dlnaOrgPnFlags(const QString &mime)
{
    if (mime.contains("video/x-msvideo", Qt::CaseInsensitive))
        return "DLNA.ORG_PN=AVI";
    /*if (mime.contains(image/jpeg"))
        return "DLNA.ORG_PN=JPEG_LRG";*/
    if (mime.contains("audio/aac", Qt::CaseInsensitive) ||
            mime.contains("audio/aacp", Qt::CaseInsensitive))
        return "DLNA.ORG_PN=AAC";
    if (mime.contains("audio/mpeg", Qt::CaseInsensitive))
        return "DLNA.ORG_PN=MP3";
    if (mime.contains("audio/vnd.wav", Qt::CaseInsensitive))
        return "DLNA.ORG_PN=LPCM";
    if (mime.contains("audio/L16", Qt::CaseInsensitive))
        return "DLNA.ORG_PN=LPCM";
    if (mime.contains("video/x-matroska", Qt::CaseInsensitive))
        return "DLNA.ORG_PN=MKV";
    return QString();
}

QString ContentServer::dlnaContentFeaturesHeader(const QString& mime, bool seek, bool flags)
{
    QString pnFlags = dlnaOrgPnFlags(mime);
    if (pnFlags.isEmpty()) {
        if (flags)
            return QString("%1;%2;%3").arg(
                        seek ? dlnaOrgOpFlagsSeekBytes : dlnaOrgOpFlagsNoSeek,
                        dlnaOrgCiFlags,
                        seek ? dlnaOrgFlagsForFile() : dlnaOrgFlagsForStreaming());
        else
            return QString("%1;%2").arg(seek ?
                                            dlnaOrgOpFlagsSeekBytes : dlnaOrgOpFlagsNoSeek,
                                            dlnaOrgCiFlags);
    } else {
        if (flags)
            return QString("%1;%2;%3;%4").arg(
                        pnFlags, seek ? dlnaOrgOpFlagsSeekBytes : dlnaOrgOpFlagsNoSeek,
                        dlnaOrgCiFlags,
                        seek ? dlnaOrgFlagsForFile() : dlnaOrgFlagsForStreaming());
        else
            return QString("%1;%2;%3").arg(
                        pnFlags, seek ? dlnaOrgOpFlagsSeekBytes : dlnaOrgOpFlagsNoSeek,
                        dlnaOrgCiFlags);
    }
}

ContentServer::Type ContentServer::getContentTypeByExtension(const QString &path)
{
    auto ext = path.split(".").last();
    ext = ext.toLower();

    if (m_imgExtMap.contains(ext)) {
        return ContentServer::TypeImage;
    } else if (m_musicExtMap.contains(ext)) {
        return ContentServer::TypeMusic;
    } else if (m_videoExtMap.contains(ext)) {
        return ContentServer::TypeVideo;
    } else if (m_playlistExtMap.contains(ext)) {
        return ContentServer::TypePlaylist;
    }

    // Default type
    return ContentServer::TypeUnknown;
}

ContentServer::Type ContentServer::getContentTypeByExtension(const QUrl &url)
{
    return getContentTypeByExtension(url.fileName());
}

ContentServer::Type ContentServer::getContentType(const QString &path)
{
    return getContentType(QUrl::fromLocalFile(path));
}

ContentServer::Type ContentServer::getContentType(const QUrl &url)
{
    const auto meta = getMeta(url);
    if (!meta) {
        qWarning() << "No cache item found, so guessing based on file extension";
        return getContentTypeByExtension(url);
    }

    return typeFromMime(meta->mime);
}

ContentServer::PlaylistType ContentServer::playlistTypeFromMime(const QString &mime)
{
    if (m_pls_mimes.contains(mime))
        return PlaylistPLS;
    if (m_m3u_mimes.contains(mime))
        return PlaylistM3U;
    if (m_xspf_mimes.contains(mime))
        return PlaylistXSPF;
    return PlaylistUnknown;
}

ContentServer::PlaylistType ContentServer::playlistTypeFromExtension(const QString &path)
{
    auto ext = path.split(".").last();
    ext = ext.toLower();
    return playlistTypeFromMime(m_playlistExtMap.value(ext));
}

ContentServer::Type ContentServer::typeFromMime(const QString &mime)
{
    // check for playlist first
    if (m_pls_mimes.contains(mime) ||
        m_xspf_mimes.contains(mime) ||
        m_m3u_mimes.contains(mime))
        return ContentServer::TypePlaylist;

    // hack for application/ogg
    if (mime.contains("/ogg", Qt::CaseInsensitive))
        return ContentServer::TypeMusic;
    if (mime.contains("/ogv", Qt::CaseInsensitive))
        return ContentServer::TypeVideo;

    auto name = mime.split("/").first().toLower();
    if (name == "audio")
        return ContentServer::TypeMusic;
    if (name == "video")
        return ContentServer::TypeVideo;
    if (name == "image")
        return ContentServer::TypeImage;

    // Default type
    return ContentServer::TypeUnknown;
}

ContentServer::Type ContentServer::typeFromUpnpClass(const QString &upnpClass)
{
    if (upnpClass.startsWith(ContentServer::genericAudioItemClass, Qt::CaseInsensitive))
        return ContentServer::TypeMusic;
    if (upnpClass.startsWith(ContentServer::genericVideoItemClass, Qt::CaseInsensitive))
        return ContentServer::TypeVideo;
    if (upnpClass.startsWith(ContentServer::genericImageItemClass, Qt::CaseInsensitive))
        return ContentServer::TypeImage;

    // Default type
    return ContentServer::TypeUnknown;
}

QStringList ContentServer::getExtensions(int type) const
{
    QStringList exts;

    if (type & TypeImage)
        exts << m_imgExtMap.keys();
    if (type & TypeMusic)
        exts << m_musicExtMap.keys();
    if (type & TypeVideo)
        exts << m_videoExtMap.keys();
    if (type & TypePlaylist)
        exts << m_playlistExtMap.keys();

    for (auto& ext : exts) {
        ext.prepend("*.");
    }

    return exts;
}

QString ContentServer::getContentMimeByExtension(const QString &path)
{
    auto ext = path.split(".").last();
    ext = ext.toLower();

    if (m_imgExtMap.contains(ext)) {
        return m_imgExtMap.value(ext);
    } else if (m_musicExtMap.contains(ext)) {
        return m_musicExtMap.value(ext);
    } else if (m_videoExtMap.contains(ext)) {
        return m_videoExtMap.value(ext);
    } else if (m_playlistExtMap.contains(ext)) {
        return m_playlistExtMap.value(ext);
    }

    // Default mime
    return "application/octet-stream";
}

QString ContentServer::getContentMimeByExtension(const QUrl &url)
{
    return getContentMimeByExtension(url.path());
}

QString ContentServer::getExtensionFromAudioContentType(const QString &mime)
{
   return m_musicMimeToExtMap.value(mime);
}

QString ContentServer::getContentMime(const QString &path)
{
    return getContentMime(QUrl::fromLocalFile(path));
}

QString ContentServer::getContentMime(const QUrl &url)
{
    const auto meta = getMeta(url);
    if (!meta) {
        qWarning() << "No cache item found, so guessing based on file extension";
        return getContentMimeByExtension(url);
    }

    return meta->mime;
}

void ContentServer::fillCoverArt(ItemMeta& item)
{
    item.albumArt = QString("%1/art-%2.jpg")
            .arg(Settings::instance()->getCacheDir(),
                 Utils::instance()->hash(item.path));

    if (QFileInfo::exists(item.albumArt)) {
        qDebug() << "Cover Art exists";
        return; // OK
    }

    // TODO: Support for extracting image from other than mp3 file formats
    TagLib::MPEG::File af(item.path.toUtf8().constData());

    if (af.isOpen()) {
        TagLib::ID3v2::Tag *tag = af.ID3v2Tag(true);
        TagLib::ID3v2::FrameList fl = tag->frameList("APIC");

        if(!fl.isEmpty()) {
            TagLib::ID3v2::AttachedPictureFrame *frame =
                    static_cast<TagLib::ID3v2::AttachedPictureFrame*>(fl.front());

            QImage img;
            img.loadFromData(reinterpret_cast<const uchar*>(frame->picture().data()),
                             static_cast<int>(frame->picture().size()));
            if (img.save(item.albumArt))
                return; // OK
            else
                qWarning() << "Unable to write album art image:" << item.albumArt;
        } else {
            qDebug() << "No cover art in" << item.path;
        }
    } else {
        qWarning() << "Cannot open file" << item.path << "with TagLib";
    }

    item.albumArt.clear();
}

QString ContentServer::extractItemFromDidl(const QString &didl)
{
    QRegExp rx("<DIDL-Lite[^>]*>(.*)</DIDL-Lite>");
    if (rx.indexIn(didl) != -1) {
        return rx.cap(1);
    }
    return QString();
}

bool ContentServer::getContentMetaItem(const QString &id, QString &meta)
{
    const auto item = getMeta(Utils::urlFromId(id));
    if (!item) {
        qWarning() << "No meta item found";
        return false;
    }

    auto s = Settings::instance();
    int relay = s->getRemoteContentMode();
    QUrl url;

    if (item->itemType == ItemType_Upnp) {
        qDebug() << "Item is upnp and didl item for upnp is not supported";
        return false;
    } else if (item->itemType == ItemType_Url &&
                   (relay == 2 || (relay == 4 && !item->shoutCast))) {
        url = item->url;
        if (!makeUrl(id, url, false)) {
            qWarning() << "Cannot make Url from id";
            return false;
        }
    } else {
        if (!makeUrl(id, url)) {
            qWarning() << "Cannot make Url from id";
            return false;
        }
    }

    return getContentMetaItem(id, url, meta, item);
}

bool ContentServer::getContentMetaItemByDidlId(const QString &didlId, QString &meta)
{
    if (metaIdx.contains(didlId)) {
        getContentMetaItem(metaIdx[didlId], meta);
        return true;
    }
    return false;
}

bool ContentServer::getContentMetaItem(const QString &id, const QUrl &url,
                                   QString &meta, const ItemMeta *item)
{
    QString path, name, desc, author; int t = 0; QUrl icon;
    if (!Utils::pathTypeNameCookieIconFromId(id, &path, &t, &name, nullptr,
                                             &icon, &desc, &author))
        return false;

    bool audioType = static_cast<Type>(t) == TypeMusic; // extract audio stream from video

    AvData data;
    if (audioType && item->local) {
        if (!extractAudio(path, data)) {
            qWarning() << "Cannot extract audio stream";
            return false;
        }
        qDebug() << "Audio stream extracted to" << data.path;
    }

    auto u = Utils::instance();
    QString didlId = u->hash(id);
    metaIdx[didlId] = id;
    QTextStream m(&meta);
    m << "<item id=\"" << didlId << "\" parentID=\"0\" restricted=\"1\">";

    switch (item->type) {
    case TypeImage:
        m << "<upnp:albumArtURI>" << url.toString() << "</upnp:albumArtURI>";
        m << "<upnp:class>" << imageItemClass << "</upnp:class>";
        break;
    case TypeMusic:
        m << "<upnp:class>" << audioItemClass << "</upnp:class>";
        if (!icon.isEmpty() || !item->albumArt.isEmpty()) {
            auto id = Utils::idFromUrl(icon.isEmpty() ?
                                           QUrl::fromLocalFile(item->albumArt) :
                                           icon,
                                       artCookie);
            QUrl artUrl;
            if (makeUrl(id, artUrl))
                m << "<upnp:albumArtURI>" << artUrl.toString() << "</upnp:albumArtURI>";
            else
                qWarning() << "Cannot make Url form art path";
        }
        break;
    case TypeVideo:
        if (audioType)
            m << "<upnp:class>" << audioItemClass << "</upnp:class>";
        else
            m << "<upnp:class>" << videoItemClass << "</upnp:class>";
        break;
    case TypePlaylist:
        m << "<upnp:class>" << playlistItemClass << "</upnp:class>";
        break;
    default:
        m << "<upnp:class>" << defaultItemClass << "</upnp:class>";
    }

    if (name.isEmpty()) {
        if (item->title.isEmpty())
            m << "<dc:title>" << ContentServer::bestName(*item).toHtmlEscaped() << "</dc:title>";
        else
            m << "<dc:title>" << item->title.toHtmlEscaped() << "</dc:title>";
        if (!item->artist.isEmpty())
            m << "<upnp:artist>" << item->artist.toHtmlEscaped() << "</upnp:artist>";
        if (!item->album.isEmpty())
            m << "<upnp:album>" << item->album.toHtmlEscaped() << "</upnp:album>";
    } else {
        m << "<dc:title>" << name.toHtmlEscaped() << "</dc:title>";
        if (!author.isEmpty())
            m << "<upnp:artist>" << author.toHtmlEscaped() << "</upnp:artist>";
    }

    if (desc.isEmpty()) {
        if (!item->comment.isEmpty())
            m << "<upnp:longDescription>" << item->comment.toHtmlEscaped() << "</upnp:longDescription>";
    } else {
        m << "<upnp:longDescription>" << desc << "</upnp:longDescription>";
    }

    m << "<res ";

    if (audioType) {
        // puting audio stream info instead video file
        if (data.size > 0)
            m << "size=\"" << QString::number(data.size) << "\" ";
        m << "protocolInfo=\"http-get:*:" << data.mime << ":*\" ";
    } else {
        if (item->size > 0)
            m << "size=\"" << QString::number(item->size) << "\" ";
        //m << "protocolInfo=\"http-get:*:" << item->mime << ":*\" ";
        m << "protocolInfo=\"http-get:*:" << item->mime << ":"
          << dlnaContentFeaturesHeader(item->mime, item->seekSupported, false)
          << "\" ";
    }

    if (item->duration > 0) {
        int seconds = item->duration % 60;
        int minutes = ((item->duration - seconds) / 60) % 60;
        int hours = (item->duration - (minutes * 60) - seconds) / 3600;
        QString duration = QString::number(hours) + ":" + (minutes < 10 ? "0" : "") +
                           QString::number(minutes) + ":" + (seconds < 10 ? "0" : "") +
                           QString::number(seconds) + ".000";
        m << "duration=\"" << duration << "\" ";
    }

    if (audioType) {
        if (item->bitrate > 0)
            m << "bitrate=\"" << QString::number(data.bitrate) << "\" ";
        if (item->sampleRate > 0)
            m << "sampleFrequency=\"" << QString::number(item->sampleRate) << "\" ";
        if (item->channels > 0)
            m << "nrAudioChannels=\"" << QString::number(item->channels) << "\" ";
    } else {
        if (item->bitrate > 0)
            m << "bitrate=\"" << QString::number(item->bitrate, 'f', 0) << "\" ";
        if (item->sampleRate > 0)
            m << "sampleFrequency=\"" << QString::number(item->sampleRate, 'f', 0) << "\" ";
        if (item->channels > 0)
            m << "nrAudioChannels=\"" << QString::number(item->channels) << "\" ";
    }

    m << ">" << url.toString() << "</res>";
    m << "</item>\n";
    return true;
}

bool ContentServer::getContentMeta(const QString &id, const QUrl &url,
                                   QString &meta, const ItemMeta *item)
{
    QTextStream m(&meta);
    m << "<?xml version=\"1.0\" encoding=\"utf-8\"?>" << endl;
    m << "<DIDL-Lite xmlns=\"urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/\" ";
    m << "xmlns:dc=\"http://purl.org/dc/elements/1.1/\" ";
    m << "xmlns:upnp=\"urn:schemas-upnp-org:metadata-1-0/upnp/\" ";
    m << "xmlns:dlna=\"urn:schemas-dlna-org:metadata-1-0/\">";

    if (getContentMetaItem(id, url, meta, item)) {
        m << "</DIDL-Lite>";
        qDebug() << "DIDL:" << meta;
        return true;
    } else {
        qWarning() << "Meta creation error";
        return false;
    }
}

bool ContentServer::getContentUrl(const QString &id, QUrl &url, QString &meta,
                                  QString cUrl)
{
    if (!Utils::isIdValid(id)) {
        return false;
    }

    const auto item = getMeta(Utils::urlFromId(id));
    if (!item) {
        qWarning() << "No meta item found";
        return false;
    }

    auto s = Settings::instance();
    int relay = s->getRemoteContentMode();

    if (item->itemType == ItemType_Upnp) {
        qDebug() << "Item is upnp and relaying is disabled";
        if (item->didl.isEmpty()) {
            qWarning() << "Didl is empty";
            return false;
        }
        url = item->url;
        if (!makeUrl(id, url, false)) { // do not relay for upnp
            qWarning() << "Cannot make Url from id";
            return false;
        }
        qDebug() << "Url for upnp item:" << id << url.toString();

        // no need to generate new DIDL, copying DIDL received from Media Server
        meta = item->didl;
        meta.replace("%URL%", url.toString());
        return true;
    } else if (item->itemType == ItemType_Url &&
               (relay == 2 || (relay == 4 && !item->shoutCast))) {
        qDebug() << "Item is url and relaying is disabled";
        url = item->url;
        if (!makeUrl(id, url, false)) {
            qWarning() << "Cannot make Url from id";
            return false;
        }
    } else {
        if (!makeUrl(id, url)) {
            qWarning() << "Cannot make Url from id";
            return false;
        }
    }

    if (!cUrl.isEmpty() && cUrl == url.toString()) {
        // Optimization: Url is the same as current -> skipping getContentMeta
        return true;
    }

    if (!getContentMeta(id, url, meta, item)) {
        qWarning() << "Cannot get content meta data";
        return false;
    }

    return true;
}

QString ContentServer::bestName(const ContentServer::ItemMeta &meta)
{
    QString name;

    if (!meta.title.isEmpty())
        name = meta.title;
    else if (!meta.filename.isEmpty() && meta.filename.length() > 1)
        name = meta.filename;
    else if (!meta.url.isEmpty())
        name = meta.url.toString();
    else
        name = tr("Unknown");

    return name;
}

bool ContentServer::makeUrl(const QString& id, QUrl& url, bool relay)
{
    QString ifname, addr;
    if (!Utils::instance()->getNetworkIf(ifname, addr)) {
        qWarning() << "Cannot find valid network interface";
        return false;
    }

    QString hash = QString::fromUtf8(encrypt(id.toUtf8()));

    if (relay) {
        url.setScheme("http");
        url.setHost(addr);
        url.setPort(Settings::instance()->getPort());
        url.setPath("/" + hash);
    } else {
        QUrlQuery q(url);
        if (q.hasQueryItem(Utils::idKey))
            q.removeAllQueryItems(Utils::idKey);
        q.addQueryItem(Utils::idKey, hash);
        url.setQuery(q);
    }

    return true;
}

QByteArray ContentServer::encrypt(const QByteArray &data)
{
    QByteArray _data(data);

    QByteArray key = Settings::instance()->getKey();
    QByteArray tmp(key);
    while (key.size() < _data.size())
        key += tmp;

    for (int i = 0; i < _data.size(); ++i)
        _data[i] = _data.at(i) ^ key.at(i);

    _data = _data.toBase64(QByteArray::Base64UrlEncoding |
                         QByteArray::OmitTrailingEquals);
    return _data;
}

QByteArray ContentServer::decrypt(const QByteArray& data)
{
    QByteArray _data = QByteArray::fromBase64(data, QByteArray::Base64UrlEncoding |
                                                   QByteArray::OmitTrailingEquals);
    QByteArray key = Settings::instance()->getKey();
    QByteArray tmp(key);
    while (key.size() < _data.size())
        key += tmp;

    for (int i = 0; i < _data.size(); ++i)
        _data[i] = _data.at(i) ^ key.at(i);

    return _data;
}

QString ContentServer::pathFromUrl(const QUrl &url) const
{
    bool valid, isFile;
    auto id = idUrlFromUrl(url, &valid, &isFile);
    if (valid && isFile)
        return id.toLocalFile();
    return QString();
}

QUrl ContentServer::idUrlFromUrl(const QUrl &url, bool* ok, bool* isFile,
                                 bool* isArt)
{
    QUrlQuery q1(url);
    QString hash;
    if (q1.hasQueryItem(Utils::idKey)) {
        hash = q1.queryItemValue(Utils::idKey);
    } else {
        hash = url.path();
        hash = hash.right(hash.length()-1);
    }

    auto id = QUrl(QString::fromUtf8(decrypt(hash.toUtf8())));
    if (!id.isValid()) {
        if (ok)
            *ok = false;
        return QUrl();
    }

    QUrlQuery q2(id);
    if (!q2.hasQueryItem(Utils::cookieKey) ||
            q2.queryItemValue(Utils::cookieKey).isEmpty()) {
        if (ok)
            *ok = false;
        return QUrl();
    } else {
        if (isArt)
            *isArt = q2.queryItemValue(Utils::cookieKey) == artCookie;
    }

    if (id.isLocalFile()) {
        QString path = id.toLocalFile();
        QFileInfo file(path);
        if (!file.exists() || !file.isFile()) {
            qWarning() << "Content path doesn't exist";
            if (ok)
                *ok = false;
            if (isFile)
                *isFile = true;
            return QUrl();
        }

        if (ok)
            *ok = true;
        if (isFile)
            *isFile = true;
        return id;
    }

    if (ok)
        *ok = true;
    if (isFile)
        *isFile = false;
    return id;
}

QString ContentServer::idFromUrl(const QUrl &url) const
{
    bool valid;
    auto id = idUrlFromUrl(url, &valid);
    return valid ? id.toString() : QString();
}

QString ContentServer::urlFromUrl(const QUrl &url) const
{
    bool valid;
    auto id = idUrlFromUrl(url, &valid);
    return valid ? Utils::urlFromId(id).toString() : QString();
}

bool ContentServer::fillAvDataFromCodec(const AVCodecParameters *codec,
                                        const QString& videoPath,
                                        AvData& data) {
    switch (codec->codec_id) {
    case AV_CODEC_ID_MP2:
    case AV_CODEC_ID_MP3:
        data.mime = "audio/mpeg";
        data.type = "mp3";
        data.extension = "mp3";
        break;
    case AV_CODEC_ID_VORBIS:
        data.mime = "audio/ogg";
        data.type = "oga";
        data.extension = "oga";
        break;
    default:
        data.mime = "audio/mp4";
        data.type = "mp4";
        data.extension = "m4a";
    }

    data.path = videoPath + ".audio-extracted." + data.extension;
    data.bitrate = codec->bit_rate;
    data.channels = codec->channels;

    return true;
}

bool ContentServer::extractAudio(const QString& path,
                                 ContentServer::AvData& data)
{
    auto f = path.toUtf8();
    const char* file = f.data();

    qDebug() << "Extracting audio from file:" << file;

    AVFormatContext *ic = nullptr;
    if (avformat_open_input(&ic, file, nullptr, nullptr) < 0) {
        qWarning() << "avformat_open_input error";
        return false;
    }

    if ((avformat_find_stream_info(ic, nullptr)) < 0) {
        qWarning() << "Could not find stream info";
        avformat_close_input(&ic);
        return false;
    }

    qDebug() << "nb_streams:" << ic->nb_streams;

    int aidx = av_find_best_stream(ic, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    qDebug() << "audio stream index is:" << aidx;

    if (aidx < 0) {
        qWarning() << "No audio stream found";
        avformat_close_input(&ic);
        return false;
    }

    // Debug: audio stream side data
    qDebug() << "audio stream nb_side_data:" << ic->streams[aidx]->nb_side_data;
    for (int i = 0; i < ic->streams[aidx]->nb_side_data; ++i) {
        qDebug() << "-- audio stream side data --";
        qDebug() << "type:" << ic->streams[aidx]->side_data[i].type;
        qDebug() << "size:" << ic->streams[aidx]->side_data[i].size;
        QByteArray data(reinterpret_cast<const char*>(ic->streams[aidx]->side_data[i].data),
                        ic->streams[aidx]->side_data[i].size);
        qDebug() << "data:" << data;
    }
    // --

    qDebug() << "Audio codec:";
    qDebug() << "codec_id:" << ic->streams[aidx]->codecpar->codec_id;
    qDebug() << "codec_channels:" << ic->streams[aidx]->codecpar->channels;
    qDebug() << "codec_tag:" << ic->streams[aidx]->codecpar->codec_tag;
    // --

    if (!fillAvDataFromCodec(ic->streams[aidx]->codecpar, path, data)) {
        qWarning() << "Unable to find correct mime for the codec:"
                   << ic->streams[aidx]->codecpar->codec_id;
        avformat_close_input(&ic);
        return false;
    }

    qDebug() << "Audio stream content type" << data.mime;
    qDebug() << "Audio stream bitrate" << data.bitrate;
    qDebug() << "Audio stream channels" << data.channels;

    qDebug() << "av_guess_format";
    AVOutputFormat *of = nullptr;
    auto t = data.type.toLatin1();
    of = av_guess_format(t.data(), nullptr, nullptr);
    if (!of) {
        qWarning() << "av_guess_format error";
        avformat_close_input(&ic);
        return false;
    }

    qDebug() << "avformat_alloc_context";
    AVFormatContext *oc = nullptr;
    oc = avformat_alloc_context();
    if (!oc) {
        qWarning() << "avformat_alloc_context error";
        avformat_close_input(&ic);
        return false;
    }

    if (ic->metadata) {
        // Debug: metadata
        AVDictionaryEntry *tag = nullptr;
        while ((tag = av_dict_get(ic->metadata, "", tag, AV_DICT_IGNORE_SUFFIX)))
            qDebug() << tag->key << "=" << tag->value;

        if (av_dict_copy(&oc->metadata, ic->metadata, 0) < 0) {
            qWarning() << "oc->metadata av_dict_copy error";
            avformat_close_input(&ic);
            avformat_free_context(oc);
            return false;
        }
    } else {
        qDebug() << "No metadata found";
    }

    oc->oformat = of;

    qDebug() << "avformat_new_stream";
    AVStream* ast = nullptr;
    ast = avformat_new_stream(oc, ic->streams[aidx]->codec->codec);
    if (!ast) {
        qWarning() << "avformat_new_stream error";
        avformat_close_input(&ic);
        avformat_free_context(oc);
        return false;
    }

    ast->id = 0;

    if (ic->streams[aidx]->metadata) {
        // Debug: audio stream metadata, codec
        AVDictionaryEntry *tag = nullptr;
        while ((tag = av_dict_get(ic->streams[aidx]->metadata, "", tag, AV_DICT_IGNORE_SUFFIX)))
            qDebug() << tag->key << "=" << tag->value;

        if (av_dict_copy(&ast->metadata, ic->streams[aidx]->metadata, 0) < 0) {
            qWarning() << "av_dict_copy error";
            avformat_close_input(&ic);
            avformat_free_context(oc);
            return false;
        }
    } else {
        qDebug() << "No metadata in audio stream";
    }

    // Copy codec params
    AVCodecParameters* t_cpara = avcodec_parameters_alloc();
    if (avcodec_parameters_from_context(t_cpara, ic->streams[aidx]->codec) < 0) {
        qWarning() << "avcodec_parameters_from_context error";
        avformat_close_input(&ic);
        avformat_free_context(oc);
        return false;
    }
    if (avcodec_parameters_copy(ast->codecpar, t_cpara ) < 0) {
        qWarning() << "avcodec_parameters_copy error";
        avformat_close_input(&ic);
        avformat_free_context(oc);
        return false;
    }
    if (avcodec_parameters_to_context(ast->codec, t_cpara) < 0) {
        qWarning() << "avcodec_parameters_to_context error";
        avformat_close_input(&ic);
        avformat_free_context(oc);
        return false;
    }
    avcodec_parameters_free(&t_cpara);

    ast->codecpar->codec_tag =
            av_codec_get_tag(oc->oformat->codec_tag,
                             ic->streams[aidx]->codecpar->codec_id);

    if (oc->oformat->flags & AVFMT_GLOBALHEADER)
        ast->codec->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    qDebug() << "ast->codec->sample_fmt:" << ast->codec->sample_fmt;
    qDebug() << "ast->codec->bit_rate:" << ast->codec->bit_rate;
    qDebug() << "ast->codec->sample_rate:" << ast->codec->sample_rate;
    qDebug() << "ast->codec->channels:" << ast->codec->channels;
    qDebug() << "ast->codecpar->codec_tag:" << ast->codecpar->codec_tag;
    qDebug() << "ic->streams[aidx]->codecpar->codec_id:" << ic->streams[aidx]->codecpar->codec_id;
    qDebug() << "ast->codecpar->codec_id:" << ast->codecpar->codec_id;

    qDebug() << "Extracted audio file will be:" << data.path;

    QFile audioFile(data.path);
    if (audioFile.exists()) {
        qDebug() << "Extracted audio stream exists";
        data.size = QFileInfo(data.path).size();
        avformat_close_input(&ic);
        avformat_free_context(oc);
        return true;
    }

    qDebug() << "avio_open";
    auto bapath = data.path.toUtf8();
    if (avio_open(&oc->pb, bapath.data(), AVIO_FLAG_WRITE) < 0) {
        qWarning() << "avio_open error";
        avformat_close_input(&ic);
        avformat_free_context(oc);
        return false;
    }

    qDebug() << "avformat_write_header";
    if (avformat_write_header(oc, nullptr) < 0) {
        qWarning() << "avformat_write_header error";
        avformat_close_input(&ic);
        avio_close(oc->pb);
        avformat_free_context(oc);
        audioFile.remove();
        return false;
    }

    AVPacket pkt = {};
    av_init_packet(&pkt);

    while (!av_read_frame(ic, &pkt)) {
        // Only processing audio stream packets
        if (pkt.stream_index == aidx) {
            // Debug: audio stream packet side data
            for (int i = 0; i < pkt.side_data_elems; ++i) {
                qDebug() << "Audio stream packet side data:";
                qDebug() << "type:" << pkt.side_data[i].type;
                qDebug() << "size:" << pkt.side_data[i].size;
                QByteArray data(reinterpret_cast<const char*>(pkt.side_data[i].data),
                                pkt.side_data[i].size);
                qDebug() << "data:" << data;
            }

            /*qDebug() << "------ orig -----";
            qDebug() << "duration:" << pkt.duration;
            qDebug() << "dts:" << pkt.dts;
            qDebug() << "pts:" << pkt.pts;
            qDebug() << "pos:" << pkt.pos;

            qDebug() << "------ time base -----";
            qDebug() << "ast->codec->time_base:" << ast->codec->time_base.num << ast->codec->time_base.den;
            qDebug() << "ast->time_base:" << ast->time_base.num << ast->time_base.den;
            qDebug() << "ic->streams[aidx]->codec->time_base:" << ic->streams[aidx]->codec->time_base.num << ic->streams[aidx]->codec->time_base.den;
            qDebug() << "ic->streams[aidx]->time_base:" << ic->streams[aidx]->time_base.num << ic->streams[aidx]->time_base.den;*/

            av_packet_rescale_ts(&pkt, ic->streams[aidx]->time_base, ast->time_base);

            /*qDebug() << "------ after rescale -----";
            qDebug() << "duration:" << pkt.duration;
            qDebug() << "dts:" << pkt.dts;
            qDebug() << "pts:" << pkt.pts;
            qDebug() << "pos:" << pkt.pos;*/

            pkt.stream_index = ast->index;

            if (av_write_frame(oc, &pkt) != 0) {
                qWarning() << "Error while writing audio frame";
                av_packet_unref(&pkt);
                avformat_close_input(&ic);
                avio_close(oc->pb);
                avformat_free_context(oc);
                audioFile.remove();
                return false;
            }
        }

        av_packet_unref(&pkt);
    }

    qDebug() << "av_write_trailer";
    if (av_write_trailer(oc) < 0) {
        qWarning() << "av_write_trailer error";
        avformat_close_input(&ic);
        avio_close(oc->pb);
        avformat_free_context(oc);
        audioFile.remove();
        return false;
    }

    qDebug() << "avformat_close_input";
    avformat_close_input(&ic);

    qDebug() << "avio_close";
    if (avio_close(oc->pb) < 0) {
        qDebug() << "avio_close error";
    }

    qDebug() << "avformat_free_context";
    avformat_free_context(oc);

    data.size = QFileInfo(data.path).size();

    return true;
}

const ContentServer::ItemMeta* ContentServer::getMeta(const QUrl &url, bool createNew)
{
    metaCacheMutex.lock();
    auto it = getMetaCacheIterator(url, createNew);
    auto meta = it == metaCache.end() ? nullptr : &it.value();
    metaCacheMutex.unlock();

    return meta;
}

const ContentServer::ItemMeta* ContentServer::getMetaForId(const QUrl &id, bool createNew)
{
    auto url = Utils::urlFromId(id);
    return getMeta(url, createNew);
}

const QHash<QUrl, ContentServer::ItemMeta>::const_iterator
ContentServer::getMetaCacheIterator(const QUrl &url, bool createNew)
{    
    const auto i = metaCache.find(url);
    if (i == metaCache.end()) {
        qDebug() << "Meta data for" << url << "not cached";
        if (createNew)
            return makeItemMeta(url);
        else
            return metaCache.end();
    }

    //qDebug() << "Meta data for" << url << "found in cache";
    return i;
}

const QHash<QUrl, ContentServer::ItemMeta>::const_iterator
ContentServer::getMetaCacheIteratorForId(const QUrl &id, bool createNew)
{
    auto url = Utils::urlFromId(id);
    return getMetaCacheIterator(url, createNew);
}

const QHash<QUrl, ContentServer::ItemMeta>::const_iterator
ContentServer::metaCacheIteratorEnd()
{
    return metaCache.end();
}

const QHash<QUrl, ContentServer::ItemMeta>::const_iterator
ContentServer::makeItemMetaUsingTracker(const QUrl &url)
{
    const QString fileUrl = url.toString(QUrl::EncodeUnicode|QUrl::EncodeSpaces);
    const QString path = url.toLocalFile();
    const QString query = queryTemplate.arg(fileUrl);

    auto tracker = Tracker::instance();
    if (!tracker->query(query, false)) {
        qWarning() << "Cannot get tracker data for url:" << fileUrl;
        return metaCache.end();
    }

    auto res = tracker->getResult();
    TrackerCursor cursor(res.first, res.second);

    int n = cursor.columnCount();

    if (n == 10) {
        while(cursor.next()) {
            /*for (int i = 0; i < n; ++i) {
                auto name = cursor.name(i);
                auto type = cursor.type(i);
                auto value = cursor.value(i);
                qDebug() << "column:" << i;
                qDebug() << " name:" << name;
                qDebug() << " type:" << type;
                qDebug() << " value:" << value;
            }*/

            QFileInfo file(path);

            auto& meta = metaCache[url];
            meta.valid = true;
            meta.trackerId = cursor.value(0).toString();
            meta.url = url;
            meta.mime = cursor.value(1).toString();
            meta.title = cursor.value(2).toString();
            meta.comment = cursor.value(3).toString();
            meta.duration = cursor.value(4).toInt();
            meta.album = cursor.value(5).toString();
            meta.artist = cursor.value(6).toString();
            meta.bitrate = cursor.value(7).toDouble();
            meta.channels = cursor.value(8).toInt();
            meta.sampleRate = cursor.value(9).toDouble();
            meta.path = path;
            meta.filename = file.fileName();
            meta.albumArt = tracker->genAlbumArtFile(meta.album, meta.artist);
            meta.type = typeFromMime(meta.mime);
            meta.size = file.size();
            meta.local = true;
            meta.seekSupported = true;
            meta.itemType = ItemType_LocalFile;

            // Recording meta
            if (meta.album == ContentServer::recAlbumName) {
                if (!ContentServer::readMetaUsingTaglib(path, meta.title, meta.artist, meta.album,
                                       meta.comment, meta.recStation, meta.recUrl, meta.recDate)) {
                    qWarning() << "Cannot read meta with TagLib";
                }
            }

            return metaCache.find(url);
        }
    }

    return metaCache.end();
}

bool ContentServer::readMetaUsingTaglib(const QString &path, QString &title,
                                          QString &artist, QString &album,
                                          QString &comment, QString &recStation,
                                          QString &recUrl, QDateTime &recDate)
{
    auto ff = TagLib::ID3v2::FrameFactory::instance();
    TagLib::MPEG::File f(path.toUtf8().constData(), ff, false);
    if (f.isValid()) {
        auto tag = f.ID3v2Tag(false);
        if (tag) {
            title = QString::fromWCharArray(tag->title().toCWString());
            artist = QString::fromWCharArray(tag->artist().toCWString());
            album = QString::fromWCharArray(tag->album().toCWString());
            comment = QString::fromWCharArray(tag->comment().toCWString());

            if (album == ContentServer::recAlbumName) {
                // Rec additional tags
                auto props = tag->properties();
                auto station_key = ContentServer::recStationTagName.toStdString();
                if (props.contains(station_key)) {
                    recStation = QString::fromWCharArray(props[station_key].front().toCWString());
                }
                auto url_key = ContentServer::recUrlTagName.toStdString();
                if (props.contains(url_key)) {
                    recUrl = QString::fromLatin1(props[url_key].front().toCString());
                }
                auto date_key = ContentServer::recDateTagName.toStdString();
                if (props.contains(date_key)) {
                    auto date = QString::fromLatin1(props[date_key].front().toCString());
                    recDate = QDateTime::fromString(date, Qt::ISODate);
                }
            }
        } else {
            qWarning() << "Cannot read ID3v2:" << path;
            return false;
        }
    } else {
        qWarning() << "Cannot open file with TagLib:" << path;
        return false;
    }

    return true;
}

void ContentServer::writeMetaUsingTaglib(const QString &path, const QString &title,
                                          const QString &artist, const QString &album,
                                          const QString &comment,
                                          const QString &recStation,
                                          const QString &recUrl,
                                          const QDateTime &recDate)
{
    auto ff = TagLib::ID3v2::FrameFactory::instance();
    TagLib::MPEG::File f(path.toUtf8().constData(), ff, false);
    if (f.isValid()) {
        auto tag = f.ID3v2Tag(true);
        if (!title.isEmpty())
            tag->setTitle(title.toStdWString());
        if (!artist.isEmpty())
            tag->setArtist(artist.toStdWString());
        if (!album.isEmpty())
            tag->setAlbum(album.toStdWString());
        if (!comment.isEmpty())
            tag->setComment(comment.toStdWString());

        // Jupii additional tags
        if (!recUrl.isEmpty()) {
            auto frame = TagLib::ID3v2::Frame::createTextualFrame(
                        ContentServer::recUrlTagName.toStdString(),
                            {recUrl.toStdString()});
            tag->addFrame(frame);
        }
        if (!recStation.isEmpty()) {
            auto frame = TagLib::ID3v2::Frame::createTextualFrame(
                        ContentServer::recStationTagName.toStdString(),
                            {recStation.toStdString()});
            tag->addFrame(frame);
        }
        if (!recDate.isNull()) {
            auto frame = TagLib::ID3v2::Frame::createTextualFrame(
                        ContentServer::recDateTagName.toStdString(),
                        {recDate.toString(Qt::ISODate)
                         .toStdString()});
            tag->addFrame(frame);
        }
        f.save();
    } else {
        qWarning() << "Cannot open file with TagLib:" << path;
    }
}

QString ContentServer::minResUrl(const UPnPClient::UPnPDirObject &item)
{
    int min_i;
    int min_width;
    int l = item.m_resources.size();

    for (int i = 0; i < l; ++i) {
        std::string val; QSize();
        if (item.m_resources[i].m_uri.empty())
            continue;
        if (item.getrprop(i, "resolution", val)) {
            int pos = val.find('x');
            if (pos == std::string::npos) {
                pos = val.find('X');
                if (pos == std::string::npos)
                    continue;
            }
            if (pos < 1)
                continue;
            int width = std::stoi(val.substr(0,pos));
            if (i == 0 || width < min_width) {
                min_i = i;
                min_width = width;
            }
        }
    }

    return QString::fromStdString(item.m_resources[min_i].m_uri);
}

ContentServer::ItemType ContentServer::itemTypeFromUrl(const QUrl &url)
{
    if (url.scheme() == "jupii") {
        if (url.host() == "upnp")
            return ItemType_Upnp;
        if (url.host() == "screen")
            return ItemType_ScreenCapture;
        if (url.host() == "pulse")
            return ItemType_AudioCapture;
        if (url.host() == "mic")
            return ItemType_Mic;
    } else if (url.scheme() == "http" || url.scheme() == "https") {
        return ItemType_Url;
    } else if (url.isLocalFile() || url.scheme() == "qrc") {
        return ItemType_LocalFile;
    }
    return ItemType_Unknown;
}

const QHash<QUrl, ContentServer::ItemMeta>::const_iterator
ContentServer::makeItemMetaUsingTaglib(const QUrl &url)
{
    QString path = url.toLocalFile();
    QFileInfo file(path);

    ContentServer::ItemMeta meta;
    meta.valid = true;
    meta.url = url;
    meta.path = path;
    meta.mime = getContentMimeByExtension(path);
    meta.type = getContentTypeByExtension(path);
    meta.size = file.size();
    meta.filename = file.fileName();
    meta.local = true;
    meta.itemType = ItemType_LocalFile;

    if (meta.type == ContentServer::TypeImage) {
        meta.seekSupported = false;
    } else {
        meta.seekSupported = true;

        if (!ContentServer::readMetaUsingTaglib(path, meta.title, meta.artist, meta.album,
                               meta.comment, meta.recStation, meta.recUrl, meta.recDate)) {
            qWarning() << "Cannot read meta with TagLib";
        }

        TagLib::FileRef f(path.toUtf8().constData());
        if(f.isNull()) {
            qWarning() << "Cannot read audio meta with TagLib";
        } else {
            if(f.audioProperties()) {
                TagLib::AudioProperties *properties = f.audioProperties();
                meta.duration = properties->length();
                meta.bitrate = properties->bitrate();
                meta.sampleRate = properties->sampleRate();
                meta.channels = properties->channels();
            }
        }

        if (meta.mime == "audio/mpeg")
            fillCoverArt(meta);

        // defaults
        /*if (meta.title.isEmpty())
            meta.title = file.fileName();
        if (meta.artist.isEmpty())
            meta.artist = tr("Unknown");
        if (meta.album.isEmpty())
            meta.album = tr("Unknown");*/
    }

    return metaCache.insert(url, meta);
}

const QHash<QUrl, ContentServer::ItemMeta>::const_iterator
ContentServer::makeUpnpItemMeta(const QUrl &url)
{
    qDebug() << "makeUpnpItemMeta:" << url;
    auto spl = url.path(QUrl::FullyEncoded).split('/');
    if (spl.size() < 3) {
        qDebug() << "Path is too short";
        return metaCache.end();
    }

    auto did = QUrl::fromPercentEncoding(spl.at(1).toLatin1()); // cdir dev id
    auto id = QUrl::fromPercentEncoding(spl.at(2).toLatin1()); // cdir item id

    qDebug() << "did:" << did;
    qDebug() << "id:" << id;

    auto cd = Services::instance()->contentDir;
    if (!cd->init(did)) {
        qWarning() << "Cannot init CDir service";
        return metaCache.end();
    }

    if (!cd->getInited()) {
        qDebug() << "CDir is not inited, so waiting for init";

        QEventLoop loop;
        connect(cd.get(), &Service::initedChanged, &loop, &QEventLoop::quit);
        QTimer::singleShot(httpTimeout, &loop, &QEventLoop::quit); // timeout
        loop.exec(); // waiting for init...

        if (!cd->getInited()) {
            qDebug() << "Cannot init CDir service";
            return metaCache.end();
        }
    }

    UPnPClient::UPnPDirContent content;
    if (!cd->readItem(id, content)) {
        qDebug() << "Cannot read from CDir service";
        return metaCache.end();
    }

    if (content.m_items.empty()) {
        qDebug() << "Item doesn't exist on CDir";
        return metaCache.end();
    }

    UPnPClient::UPnPDirObject &item = content.m_items[0];

    if (item.m_resources.empty()) {
        qDebug() << "Item doesn't have resources";
        return metaCache.end();
    }

    if (item.m_resources[0].m_uri.empty()) {
        qDebug() << "Item uri is empty";
        return metaCache.end();
    }

    QString surl = QString::fromStdString(item.m_resources[0].m_uri);

    ContentServer::ItemMeta meta;
    meta.valid = true;
    meta.url = QUrl(surl);
    meta.type = ContentServer::typeFromUpnpClass(QString::fromStdString(item.getprop("upnp:class")));
    UPnPP::ProtocolinfoEntry proto;
    if (item.m_resources[0].protoInfo(proto)) {
        meta.mime = QString::fromStdString(proto.contentFormat);
        qDebug() << "proto info mime:" << meta.mime;
    } else {
        qWarning() << "Cannot parse proto info";
        meta.mime = "audio/mpeg";
    }
    meta.size = 0;
    meta.local = false;
    meta.album = QString::fromStdString(item.getprop("upnp:album"));
    meta.artist = Utils::parseArtist(QString::fromStdString(item.getprop("upnp:artist")));
    meta.didl = QString::fromStdString(item.getdidl());
    meta.didl = meta.didl.replace(surl, "%URL%");
    meta.albumArt = meta.type == TypeImage ? minResUrl(item) : QString::fromStdString(item.getprop("upnp:albumArtURI"));
    meta.filename = url.fileName();
    meta.seekSupported = false;
    meta.title = QString::fromStdString(item.m_title);
    if (meta.title.isEmpty())
        meta.title = meta.filename;
    meta.upnpDevId = did;
    meta.itemType = ItemType_Upnp;

    qDebug() << "DIDL:" << meta.didl;

    return metaCache.insert(url, meta);
}

const QHash<QUrl, ContentServer::ItemMeta>::const_iterator
ContentServer::makeMicItemMeta(const QUrl &url)
{
    ContentServer::ItemMeta meta;
    meta.valid = true;
    meta.url = url;
    meta.channels = MicCaster::channelCount;
    meta.sampleRate = MicCaster::sampleRate;
    meta.mime = m_musicExtMap.value("mp3");
    meta.type = ContentServer::TypeMusic;
    meta.size = 0;
    meta.local = true;
    meta.seekSupported = false;
    meta.title = tr("Microphone");
    meta.itemType = ItemType_Mic;

#ifdef SAILFISH
    meta.albumArt = IconProvider::pathToNoResId("icon-mic");
#endif

    return metaCache.insert(url, meta);
}

const QHash<QUrl, ContentServer::ItemMeta>::const_iterator
ContentServer::makeAudioCaptureItemMeta(const QUrl &url)
{
    ContentServer::ItemMeta meta;
    meta.valid = true;
    meta.url = url;
    meta.mime = m_musicExtMap.value("mp3");
    meta.type = ContentServer::TypeMusic;
    meta.size = 0;
    meta.local = true;
    meta.seekSupported = false;
    meta.title = tr("Audio capture");
    meta.itemType = ItemType_AudioCapture;
#ifdef SAILFISH
    meta.albumArt = IconProvider::pathToNoResId("icon-pulse");
#endif

    return metaCache.insert(url, meta);
}

const QHash<QUrl, ContentServer::ItemMeta>::const_iterator
ContentServer::makeScreenCaptureItemMeta(const QUrl &url)
{
    ContentServer::ItemMeta meta;
    meta.valid = true;
    meta.url = url;
    meta.mime = m_videoExtMap.value("tsv");
    meta.type = ContentServer::TypeVideo;
    meta.size = 0;
    meta.local = true;
    meta.seekSupported = false;
    meta.title = tr("Screen capture");
    meta.itemType = ItemType_ScreenCapture;
#ifdef SAILFISH
    meta.albumArt = IconProvider::pathToNoResId("icon-screen");
#endif

    return metaCache.insert(url, meta);
}

QString ContentServer::mimeFromDisposition(const QString &disposition)
{
    QString mime;

    if (disposition.contains("attachment")) {
        qDebug() << "Content as a attachment detected";
        QRegExp rx("filename=\"?([^\";]*)\"?", Qt::CaseInsensitive);
        int pos = 0;
        while ((pos = rx.indexIn(disposition, pos)) != -1) {
            QString filename = rx.cap(1);
            if (!filename.isEmpty()) {
                qDebug() << "filename:" << filename;
                mime = getContentMimeByExtension(filename);
                break;
            }
            pos += rx.matchedLength();
        }
    }

    return mime;
}

bool ContentServer::hlsPlaylist(const QByteArray &data)
{
    return data.contains("#EXT-X-");
}

const QHash<QUrl, ContentServer::ItemMeta>::const_iterator
ContentServer::makeItemMetaUsingHTTPRequest(const QUrl &url,
                                            std::shared_ptr<QNetworkAccessManager> nam,
                                            int counter)
{
    qDebug() << ">> makeItemMetaUsingHTTPRequest in thread:" << QThread::currentThreadId();
    if (counter >= maxRedirections) {
        qWarning() << "Max redirections reached";
        return metaCache.end();
    }

    qDebug() << "Sending HTTP request for url:" << url;
    QNetworkRequest request;
    request.setUrl(url);
    request.setRawHeader("Icy-MetaData", "1"); // needed for SHOUTcast streams discovery
    request.setRawHeader("User-Agent", userAgent);
    request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);

    if (!nam) {
        nam = std::shared_ptr<QNetworkAccessManager>(new QNetworkAccessManager());
    }

    auto reply = nam->get(request);

    QEventLoop loop;
    connect(reply, &QNetworkReply::metaDataChanged, [reply]{
        qDebug() << ">> metaDataChanged in thread:" << QThread::currentThreadId();
        qDebug() << "Received meta data of HTTP reply for url:" << reply->url();

        // Bug in Qt? "Content-Disposition" cannot be retrived with QNetworkRequest::ContentDispositionHeader
        //auto disposition = reply->header(QNetworkRequest::ContentDispositionHeader).toString().toLower();
        auto disposition = QString(reply->rawHeader("Content-Disposition")).toLower();

        auto mime = mimeFromDisposition(disposition);
        if (mime.isEmpty())
            mime = reply->header(QNetworkRequest::ContentTypeHeader).toString().toLower();
        auto type = typeFromMime(mime);

        if (type == ContentServer::TypePlaylist) {
            qDebug() << "Content is a playlist";
            // Content is needed, so not aborting
        } else {
            // Content is no needed, so aborting
            if (!reply->isFinished())
                reply->abort();
        }
    });
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QTimer::singleShot(httpTimeout, &loop, &QEventLoop::quit); // timeout
    loop.exec(); // waiting for HTTP reply...

    if (!reply->isFinished()) {
        qWarning() << "Timeout occured";
        reply->abort();
        reply->deleteLater();
        return metaCache.end();
    }

    qDebug() << "Received HTTP reply for url:" << url;

    qDebug() << "Headers:";
    for (const auto &p : reply->rawHeaderPairs()) {
        qDebug() << p.first << p.second;
    }

    auto code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    auto reason = reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();
    auto error = reply->error();
    qDebug() << "Response code:" << code << reason;

    if (error != QNetworkReply::NoError &&
        error != QNetworkReply::OperationCanceledError) {
        qWarning() << "Error:" << error;
        reply->deleteLater();
        return metaCache.end();
    }

    if (code > 299 && code < 399) {
        qWarning() << "Redirection received:" << reply->error() << code << reason;
        QUrl newUrl = reply->header(QNetworkRequest::LocationHeader).toUrl();
        if (newUrl.isRelative())
            newUrl = url.resolved(newUrl);
        reply->deleteLater();
        if (newUrl.isValid())
            return makeItemMetaUsingHTTPRequest(newUrl, nam, counter + 1);
        else
            return metaCache.end();
    }

    if (code > 299) {
        qWarning() << "Unsupported response code:" << reply->error() << code << reason;
        reply->deleteLater();
        return metaCache.end();
    }

    // Bug in Qt? "Content-Disposition" cannot be retrived with QNetworkRequest::ContentDispositionHeader
    //auto disposition = reply->header(QNetworkRequest::ContentDispositionHeader).toString().toLower();
    auto disposition = QString(reply->rawHeader("Content-Disposition")).toLower();
    auto mime = mimeFromDisposition(disposition);
    if (mime.isEmpty())
        mime = reply->header(QNetworkRequest::ContentTypeHeader).toString().toLower();
    auto type = typeFromMime(mime);

    if (type == ContentServer::TypePlaylist) {
        qDebug() << "Content is a playlist";

        auto size = reply->bytesAvailable();
        if (size > 0) {
            auto ptype = playlistTypeFromMime(mime);
            auto data = reply->readAll();

            if (hlsPlaylist(data)) {
                qDebug() <<  "HLS playlist";
                ContentServer::ItemMeta meta;
                meta.valid = true;
                meta.url = url;
                meta.mime = mime;
                meta.type = ContentServer::TypePlaylist;
                meta.filename = url.fileName();
                meta.local = false;
                meta.seekSupported = false;
                meta.mode = 2; // playlist proxy
                meta.itemType = ItemType_Url;
                reply->deleteLater();
                return metaCache.insert(url, meta);
            } else {
                auto items = ptype == PlaylistPLS ?
                             parsePls(data, reply->url().toString()) :
                                ptype == PlaylistXSPF ?
                                parseXspf(data, reply->url().toString()) :
                                    parseM3u(data, reply->url().toString());
                if (!items.isEmpty()) {
                    QUrl url = items.first().url;
                    qDebug() << "Trying get meta data for first item in the playlist:" << url;
                    reply->deleteLater();
                    return makeItemMetaUsingHTTPRequest(url, nam, counter + 1);
                }
            }
        }

        qWarning() << "Playlist content is empty";
        reply->deleteLater();
        return metaCache.end();
    }

    if (type != TypeMusic && type != TypeVideo && type != TypeImage) {
        qWarning() << "Unsupported type";
        reply->deleteLater();
        return metaCache.end();
    }

    auto ranges = QString(reply->rawHeader("Accept-Ranges")).toLower().contains("bytes");
    int size = reply->header(QNetworkRequest::ContentLengthHeader).toInt();

    const QByteArray icy_name_h = "icy-name";
    const QByteArray icy_br_h = "icy-br";
    const QByteArray icy_sr_h = "icy-sr";

    ContentServer::ItemMeta meta;
    meta.valid = true;
    meta.url = url;
    meta.mime = mime;
    meta.type = type;
    meta.size = ranges ? size : 0;
    meta.filename = url.fileName();
    meta.local = false;
    meta.seekSupported = size > 0 ? ranges : false;
    meta.itemType = ItemType_Url;

    if (reply->hasRawHeader("icy-metaint")) {
        int metaint = reply->rawHeader("icy-metaint").toInt();
        if (metaint > 0) {
            qDebug() << "Shoutcast stream detected";
            meta.shoutCast = true;
        }
    }

    if (reply->hasRawHeader(icy_name_h))
        meta.title = QString(reply->rawHeader(icy_name_h));
    else
        meta.title = url.fileName();
    /*if (reply->hasRawHeader(icy_br_h))
        meta.bitrate = reply->rawHeader(icy_br_h).toDouble();
    if (reply->hasRawHeader(icy_sr_h))
        meta.sampleRate = reply->rawHeader(icy_sr_h).toDouble();*/

    reply->deleteLater();
    return metaCache.insert(url, meta);
}

ContentServer::ItemMeta*
ContentServer::makeMetaUsingExtension(const QUrl &url)
{
    bool isFile = url.isLocalFile();
    bool isQrc = url.scheme() == "qrc";

    auto item = new ContentServer::ItemMeta;
    item->valid = true;
    item->path = isFile ? url.toLocalFile() :
                          isQrc ? (":" + Utils::urlFromId(url).toString().mid(6)) :
                                  "";
    item->url = url;
    item->mime = getContentMimeByExtension(url);
    item->type = getContentTypeByExtension(url);
    item->size = 0;
    item->filename = url.fileName();
    item->local = isFile || isQrc;
    item->seekSupported = isFile || isQrc;
    item->itemType = itemTypeFromUrl(url);
    return item;
}

const QHash<QUrl, ContentServer::ItemMeta>::const_iterator
ContentServer::makeItemMeta(const QUrl &url)
{
    QHash<QUrl, ContentServer::ItemMeta>::const_iterator it;
    auto itemType = itemTypeFromUrl(url);
    if (itemType == ItemType_LocalFile) {
        if (QFile::exists(url.toLocalFile())) {
            it = makeItemMetaUsingTracker(url);
            if (it == metaCache.end()) {
                qWarning() << "Cannot get meta using Tacker, so fallbacking to Taglib";
                it = makeItemMetaUsingTaglib(url);
            }
        } else {
            qWarning() << "File doesn't exist, cannot create meta item";
            it = metaCache.end();
        }
    } else if (itemType == ItemType_Mic) {
        qDebug() << "Mic URL detected";
        it = makeMicItemMeta(url);
    } else if (itemType == ItemType_AudioCapture) {
        qDebug() << "Pulse URL detected";
        it = makeAudioCaptureItemMeta(url);
    } else if (itemType == ItemType_ScreenCapture) {
        qDebug() << "Screen url detected";
        auto s = Settings::instance();
        if (s->getScreenSupported()) {
            it = makeScreenCaptureItemMeta(url);
        } else {
            qWarning() << "Screen capturing is not supported";
            it = metaCache.end();
        }
    } else if (itemType == ItemType_Upnp) {
        qDebug() << "Upnp URL detected";
        it = makeUpnpItemMeta(url);
    } else if (itemType == ItemType_Url){
        qDebug() << "Geting meta using HTTP request";
        it = makeItemMetaUsingHTTPRequest(url);
    } else if (url.scheme() == "jupii") {
        qDebug() << "Unsupported Jupii URL detected";
        it = metaCache.end();
    } else {
        qDebug() << "Unsupported URL type";
        it = metaCache.end();
    }

    return it;
}

void ContentServer::run()
{
    qDebug() << "Creating content server worker in thread:"
             << QThread::currentThreadId();

    auto worker = ContentServerWorker::instance();
    connect(worker, &ContentServerWorker::shoutcastMetadataUpdated, this,
            &ContentServer::shoutcastMetadataHandler);
    connect(worker, &ContentServerWorker::pulseStreamUpdated, this,
            &ContentServer::pulseStreamNameHandler);
    connect(worker, &ContentServerWorker::itemAdded, this,
            &ContentServer::itemAddedHandler);
    connect(worker, &ContentServerWorker::itemRemoved, this,
            &ContentServer::itemRemovedHandler);
    connect(this, &ContentServer::requestStreamToRecord, worker,
            &ContentServerWorker::setStreamToRecord);
    connect(worker, &ContentServerWorker::streamToRecordChanged, this,
            &ContentServer::streamToRecordChangedHandler);
    connect(worker, &ContentServerWorker::streamRecordableChanged, this,
            &ContentServer::streamRecordableChangedHandler);
    connect(worker, &ContentServerWorker::streamRecorded, this,
            &ContentServer::streamRecordedHandler);
    connect(this, &ContentServer::displayStatusChanged, worker,
            &ContentServerWorker::setDisplayStatus);
    QThread::exec();
    qDebug() << "Ending worker event loop";
}

QString ContentServer::streamTitle(const QUrl &id) const
{
    if (streams.contains(id)) {
        return streams.value(id).title;
    }

    return QString();
}

QStringList ContentServer::streamTitleHistory(const QUrl &id) const
{
    if (streams.contains(id)) {
        return streams.value(id).titleHistory;
    }

    return QStringList();
}

bool ContentServer::isStreamToRecord(const QUrl &id)
{
    auto worker = ContentServerWorker::instance();
    return worker->isStreamToRecord(id);
}

bool ContentServer::isStreamRecordable(const QUrl &id)
{
    auto worker = ContentServerWorker::instance();
    return worker->isStreamRecordable(id);
}

void ContentServer::setStreamToRecord(const QUrl &id, bool value)
{
    emit requestStreamToRecord(id, value);
}

void ContentServer::streamToRecordChangedHandler(const QUrl &id, bool value)
{
    emit streamToRecordChanged(id, value);
}

void ContentServer::streamRecordableChangedHandler(const QUrl &id, bool value)
{
    emit streamRecordableChanged(id, value);
}

void ContentServer::streamRecordedHandler(const QString& title, const QString& path)
{
    emit streamRecorded(title, path);
}

void ContentServer::itemAddedHandler(const QUrl &id)
{
    qDebug() << "New item for id:" << id;
    auto &stream = streams[id];
    stream.count++;
    stream.id = id;
}

void ContentServer::itemRemovedHandler(const QUrl &id)
{
    qDebug() << "Item removed for id:" << id;
    auto &stream = streams[id];
    stream.count--;
    if (stream.count < 1) {
        streams.remove(id);
        emit streamTitleChanged(id, QString());
    }
}

void ContentServer::pulseStreamNameHandler(const QUrl &id,
                                           const QString &name)
{
    qDebug() << "Pulse-audio stream name updated:" << id << name;

    auto &stream = streams[id];
    stream.id = id;
    stream.title = name;
    emit streamTitleChanged(id, name);
}

QString ContentServer::streamTitleFromShoutcastMetadata(const QByteArray &metadata)
{
    auto data = QString::fromUtf8(metadata);
    data.remove('\'').remove('\"');
    QRegExp rx("StreamTitle=([^;]*)", Qt::CaseInsensitive);
    int pos = 0;
    QString title;
    while ((pos = rx.indexIn(data, pos)) != -1) {
        title = rx.cap(1).remove('$').remove('%').remove('*').remove('&')
                .remove('?').remove('.').remove('/').remove('\\').remove(',')
                .remove('+').trimmed();
        if (!title.isEmpty()) {
            qDebug() << "Stream title:" << title;
            break;
        }
        pos += rx.matchedLength();
    }

    return title;
}

void ContentServer::shoutcastMetadataHandler(const QUrl &id,
                                             const QByteArray &metadata)
{
    qDebug() << "Shoutcast Metadata:" << metadata;

    auto new_title = streamTitleFromShoutcastMetadata(metadata);
    auto &stream = streams[id];
    stream.id = id;
    stream.title = new_title;

    if (stream.titleHistory.isEmpty() || stream.titleHistory.first() != new_title)
        stream.titleHistory.push_front(new_title);

    if (stream.titleHistory.length() > 20) // max number of titiles in history list
        stream.titleHistory.removeLast();

    emit streamTitleChanged(id, stream.title);
}

QList<ContentServer::PlaylistItemMeta>
ContentServer::parsePls(const QByteArray &data, const QString context)
{
    qDebug() << "Parsing PLS playlist";
    QMap<int,ContentServer::PlaylistItemMeta> map;
    int pos;

    // urls
    QRegExp rxFile("\\nFile(\\d\\d?)=([^\\n]*)", Qt::CaseInsensitive);
    pos = 0;
    while ((pos = rxFile.indexIn(data, pos)) != -1) {
        QString cap1 = rxFile.cap(1);
        QString cap2 = rxFile.cap(2);
        qDebug() << "cap:" << cap1 << cap2;

        bool ok;
        int n = cap1.toInt(&ok);
        if (ok) {
            auto url = Utils::urlFromText(cap2, context);
            if (!url.isEmpty()) {
                auto &item = map[n];
                item.url = url;
            } else {
                qWarning() << "Playlist item url is invalid";
            }
        } else {
            qWarning() << "Playlist item no is invalid";
        }

        pos += rxFile.matchedLength();
    }

    if (!map.isEmpty()) {
        // titles
        QRegExp rxTitle("\\nTitle(\\d\\d?)=([^\\n]*)", Qt::CaseInsensitive);
        pos = 0;
        while ((pos = rxTitle.indexIn(data, pos)) != -1) {
            QString cap1 = rxTitle.cap(1);
            QString cap2 = rxTitle.cap(2);
            qDebug() << "cap:" << cap1 << cap2;

            bool ok;
            int n = cap1.toInt(&ok);
            if (ok && map.contains(n)) {
                auto &item = map[n];
                item.title = cap2;
            }

            pos += rxTitle.matchedLength();
        }

        // length
        QRegExp rxLength("\\nLength(\\d\\d?)=([^\\n]*)", Qt::CaseInsensitive);
        pos = 0;
        while ((pos = rxLength.indexIn(data, pos)) != -1) {
            QString cap1 = rxLength.cap(1);
            QString cap2 = rxLength.cap(2);
            qDebug() << "cap:" << cap1 << cap2;

            bool ok;
            int n = cap1.toInt(&ok);
            if (ok && map.contains(n)) {
                bool ok;
                int length = cap2.toInt(&ok);
                if (ok) {
                    auto &item = map[n];
                    item.length = length < 0 ? 0 : length;
                }
            }

            pos += rxLength.matchedLength();
        }
    } else {
        qWarning() << "Playlist doesn't contain any URLs";
    }

    return map.values();
}

void ContentServer::resolveM3u(QByteArray &data, const QString context)
{
    QStringList lines;

    QTextStream s(data, QIODevice::ReadOnly);
    s.setAutoDetectUnicode(true);

    while (!s.atEnd()) {
        auto line = s.readLine();
        if (!line.startsWith("#"))
            lines << line;
    }

    for (const auto& line : lines) {
        auto url = Utils::urlFromText(line, context);
        if (!url.isEmpty())
            data.replace(line, url.toString().toUtf8());
    }
}

QList<ContentServer::PlaylistItemMeta>
ContentServer::parseM3u(const QByteArray &data, const QString context)
{
    qDebug() << "Parsing M3U playlist";

    QList<ContentServer::PlaylistItemMeta> list;

    QTextStream s(data, QIODevice::ReadOnly);
    s.setAutoDetectUnicode(true);

    while (!s.atEnd()) {
        auto line = s.readLine();
        if (line.startsWith("#")) {
            // TODO: read title from M3U playlist
        } else {
            auto url = Utils::urlFromText(line, context);
            if (!url.isEmpty()) {
                PlaylistItemMeta item;
                item.url = url;
                qDebug() << url;
                list.append(item);
            }
        }
    }

    return list;
}

QList<ContentServer::PlaylistItemMeta>
ContentServer::parseXspf(const QByteArray &data, const QString context)
{
    qDebug() << "Parsing XSPF playlist";
    QList<ContentServer::PlaylistItemMeta> list;

    QDomDocument doc; QString error;
    if (doc.setContent(data, false, &error)) {
        auto tracks = doc.elementsByTagName("track");
        int l = tracks.length();
        for (int i = 0; i < l; ++i) {
            auto track = tracks.at(i).toElement();
            if (!track.isNull()) {
                auto ls = track.elementsByTagName("location");
                if (!ls.isEmpty()) {
                    auto l = ls.at(0).toElement();
                    if (!l.isNull()) {
                        qDebug() << "location:" << l.text();

                        auto url = Utils::urlFromText(l.text(), context);
                        if (!url.isEmpty()) {
                            PlaylistItemMeta item;
                            item.url = url;

                            auto ts = track.elementsByTagName("title");
                            if (!ts.isEmpty()) {
                                auto t = ts.at(0).toElement();
                                if (!t.isNull()) {
                                    qDebug() << "title:" << t.text();
                                    item.title = t.text();
                                }
                            }

                            auto ds = track.elementsByTagName("duration");
                            if (!ds.isEmpty()) {
                                auto d = ds.at(0).toElement();
                                if (!d.isNull()) {
                                    qDebug() << "duration:" << d.text();
                                    item.length = d.text().toInt();
                                }
                            }

                            list.append(item);
                        }
                    }
                }
            }
        }
    } else {
        qWarning() << "Playlist parse error:" << error;
    }

    return list;
}

void ContentServerWorker::adjustVolume(QByteArray* data, float factor, bool le)
{
    QDataStream sr(data, QIODevice::ReadOnly);
    sr.setByteOrder(le ? QDataStream::LittleEndian : QDataStream::BigEndian);
    QDataStream sw(data, QIODevice::WriteOnly);
    sw.setByteOrder(le ? QDataStream::LittleEndian : QDataStream::BigEndian);
    int16_t sample; // assuming 16-bit LPCM sample

    while (!sr.atEnd()) {
        sr >> sample;
        int32_t s = factor * static_cast<int32_t>(sample);
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

void ContentServerWorker::dispatchPulseData(const void *data, int size)
{
    bool audioCaptureEnabled = audioCaster && !audioCaptureItems.isEmpty();
#ifdef SCREENCAST
    bool screenCaptureAudioEnabled = screenCaster &&
            screenCaster->audioEnabled() && !screenCaptureItems.isEmpty();
#else
    bool screenCaptureAudioEnabled = false;
#endif

    if (audioCaptureEnabled || screenCaptureAudioEnabled) {
        QByteArray d;
        if (data) {
#ifdef SAILFISH
            // increasing volume level only in SFOS
            d = QByteArray(static_cast<const char*>(data), size); // deep copy
            adjustVolume(&d, 2.3f, true);
#else
            d = QByteArray::fromRawData(static_cast<const char*>(data), size);
#endif
        } else {
            // writing null data
            //qDebug() << "writing null data:" << size;
            d = QByteArray(size,'\0');
        }
        if (audioCaptureEnabled)
            audioCaster->writeAudioData(d);
#ifdef SCREENCAST
        if (screenCaptureAudioEnabled)
            screenCaster->writeAudioData(d);
#endif
    } else {
        qDebug() << "No audio capture";
    }
}

void ContentServerWorker::sendMicData(const void *data, int size)
{
    if (!micItems.isEmpty()) {
        QByteArray d = QByteArray::fromRawData(static_cast<const char*>(data),
                                               size);
        auto i = micItems.begin();
        while (i != micItems.end()) {
            if (!i->resp->isHeaderWritten()) {
                qWarning() << "Head not written";
                i->resp->end();
            }
            if (i->resp->isFinished()) {
                qWarning() << "Server request already finished, "
                              "so removing mic item";
                auto id = i->id;
                i = micItems.erase(i);
                emit itemRemoved(id);
            } else {
                i->resp->write(d);
                ++i;
            }
        }
    } else {
        qDebug() << "No mic items";
    }
}

void ContentServerWorker::sendAudioCaptureData(const void *data, int size)
{
    if (!audioCaptureItems.isEmpty()) {
        QByteArray d = QByteArray::fromRawData(static_cast<const char*>(data),
                                               size);
        auto i = audioCaptureItems.begin();
        while (i != audioCaptureItems.end()) {
            if (!i->resp->isHeaderWritten()) {
                qWarning() << "Head not written";
                i->resp->end();
            }
            if (i->resp->isFinished()) {
                qWarning() << "Server request already finished, "
                              "so removing audio capture item";
                auto id = i->id;
                i = audioCaptureItems.erase(i);
                emit itemRemoved(id);
            } else {
                i->resp->write(d);
                ++i;
            }
        }
    } else {
        qDebug() << "No audio capture items";
    }
}

void ContentServerWorker::sendScreenCaptureData(const void *data, int size)
{
    if (!screenCaptureItems.isEmpty()) {
        QByteArray d = QByteArray::fromRawData(static_cast<const char*>(data),
                                               size);
        auto i = screenCaptureItems.begin();
        while (i != screenCaptureItems.end()) {
            if (!i->resp->isHeaderWritten()) {
                qWarning() << "Head not written";
                i->resp->end();
            }
            if (i->resp->isFinished()) {
                qWarning() << "Server request already finished, "
                              "so removing screen item";
                auto id = i->id;
                i = screenCaptureItems.erase(i);
                emit itemRemoved(id);
            } else {
                i->resp->write(d);
                ++i;
            }
        }
    } else {
        qDebug() << "No screen items";
    }
}
