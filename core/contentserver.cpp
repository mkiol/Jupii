/* Copyright (C) 2017-2021 Michal Kosciesza <michal@mkiol.net>
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
#include <algorithm>
#include <utility>

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
#include "directory.h"
#include "ytdlapi.h"
#include "playlistmodel.h"
#include "libupnpp/control/cdirectory.hxx"
#include "bcapi.h"
#include "connectivitydetector.h"
#include "soundcloudapi.h"
#include "playlistparser.h"

// TagLib
#include "fileref.h"
#include "tag.h"
#include "tpropertymap.h"
#include "mpegfile.h"
#include "id3v2frame.h"
#include "id3v2tag.h"
#include "attachedpictureframe.h"
#include "mp4file.h"
#include "mp4tag.h"
#include "mp4properties.h"
#include "oggflacfile.h"
#include "vorbisfile.h"
#include "vorbisproperties.h"
#include "flacfile.h"
#include "flacproperties.h"

#ifdef SAILFISH
#include <sailfishapp.h>
#endif

#if defined(SAILFISH) || defined(KIRIGAMI)
#include "iconprovider.h"
#endif

ContentServer* ContentServer::m_instance = nullptr;
ContentServerWorker* ContentServerWorker::m_instance = nullptr;

const QString ContentServer::queryTemplate {
    "SELECT ?item "
    "nie:mimeType(?item) "
    "nie:title(?item) "
    "nie:comment(?item) "
    "nfo:duration(?item) "
    "nie:title(nmm:musicAlbum(?item)) "
    "nmm:artistName(%2(?item)) "
    "nfo:averageBitrate(?item) "
    "nfo:channels(?item) "
    "nfo:sampleRate(?item) "
    "WHERE { ?item nie:isStoredAs ?url . ?url nie:url \"%1\". }"};

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
    {"tsa", "audio/MP2T"},
    {"aiff", "audio/x-aiff"}
};

const QHash<QString,QString> ContentServer::m_musicMimeToExtMap {
    {"audio/mpeg", "mp3"},
    {"audio/mp4", "m4a"},
    // Disabling AAC because TagLib doesn't support ADTS https://github.com/taglib/taglib/issues/508
    //{"audio/aac", "aac"},
    //{"audio/aacp", "aac"},
    {"audio/ogg", "ogg"},
    {"audio/vorbis", "ogg"},
    {"application/ogg", "ogg"},
    {"audio/wav", "wav"},
    {"audio/vnd.wav", "wav"},
    {"audio/x-wav", "wav"},
    {"audio/x-aiff", "aiff"}
};

const QHash<QString,QString> ContentServer::m_imgMimeToExtMap {
    {"image/jpeg", "jpg"},
    {"image/png", "png"},
    {"image/gif", "gif"}
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

const QByteArray ContentServer::userAgent{"Mozilla/5.0 (X11; Linux x86_64; rv:88.0) Gecko/20100101 Firefox/88.0"};

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
const QString ContentServer::recUrlTagName = "Recording URL";
const QString ContentServer::recUrlTagName2 = "Station URL";
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
    nam(new QNetworkAccessManager(parent)),
    server(new QHttpServer(parent)),
    volumeBoost(Settings::instance()->getAudioBoost())
{
    connect(server, &QHttpServer::newRequest,
                     this, &ContentServerWorker::requestHandler);
    connect(this, &ContentServerWorker::contSeqWriteData,
                     this, &ContentServerWorker::seqWriteData,
                     Qt::QueuedConnection);
    connect(Settings::instance(), &Settings::audioBoostChanged,
                     this, [this]{volumeBoost = Settings::instance()->getAudioBoost();},
                     Qt::QueuedConnection);

    if (!server->listen(static_cast<quint16>(Settings::instance()->getPort()))) {
        qWarning() << "Unable to start HTTP server!";
        //TODO: Handle: Unable to start HTTP server
    }
    pulseSource = std::unique_ptr<PulseAudioSource>(new PulseAudioSource());
    cleanCacheFiles();
}

bool ContentServerWorker::streamToRecord(const QUrl &id)
{
    proxyItemsMutex.lock();
    foreach (const ProxyItem& item, proxyItems) {
        if (item.id == id) {
            proxyItemsMutex.unlock();
            return item.saveRec;
        }
    }
    proxyItemsMutex.unlock();

    return false;
}

bool ContentServerWorker::streamRecordable(const QUrl &id)
{
    proxyItemsMutex.lock();
    foreach (const ProxyItem& item, proxyItems) {
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
        emit streamRecordableChanged(item.id, false, QString());
    }
}

void ContentServerWorker::cleanCacheFiles()
{
    Utils::removeFromCacheDir(QStringList() << "rec-*.*"
                              << "passlogfile-*.*"
                              << "art-*.*"
                              << "art_image_*.*");
}

void ContentServerWorker::openRecFile(ProxyItem &item)
{
    if (item.recFile) {
        item.recFile->remove();
        auto recDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
        auto recFilePath = QDir(recDir).filePath(QString("rec-%1.%2").arg(
                                        Utils::instance()->randString(), item.recExt));
        qDebug() << "Opening file for recording:" << recFilePath;
        item.recFile->setFileName(recFilePath);
        item.recFile->remove();
        if (item.recFile->open(QIODevice::WriteOnly)) {
            item.saveRec = false;
            emit streamToRecordChanged(item.id, item.saveRec);
            emit streamRecordableChanged(item.id, true, QString());
        } else {
            qWarning() << "File for recording cannot be open";
        }
    }
}

void ContentServerWorker::endRecFile(ProxyItem &item)
{
    QString tmpFile;

    if (item.recFile) {
        if (const auto title = item.metaint > 0 ?
                ContentServer::streamTitleFromShoutcastMetadata(item.metadata) : item.title;
                !title.isEmpty()) {
            qDebug() << "Saving tmp recorded file:" << title;
            item.recFile->close();
            if (item.recFile->exists() && item.recFile->size() > ContentServer::recMinSize) {
                tmpFile = QString("%1.tmp.%2").arg(item.recFile->fileName(), item.recExt);
                //qDebug() << "tmp file:" << tmpFile;
                if (item.recFile->copy(tmpFile)) {
                    auto url = item.origUrl.isEmpty() ?
                                Utils::urlFromId(item.id).toString() :
                                item.origUrl.toString();
                    ContentServer::writeMetaUsingTaglib(
                        tmpFile, // path
                        title, // title
                        item.metaint > 0 ? item.title : item.author, // author
                        ContentServer::recAlbumName, // album
                        //QString("Recorded by %1").arg(Jupii::APP_NAME),
                        QString(), // comment
                        url, // URL
                        QDateTime::currentDateTime(), // time of recording
                        item.artPath
                    );
                } else {
                    qWarning() << "Cannot copy file:"
                               << item.recFile->fileName() << tmpFile;
                    tmpFile.clear();
                }
            } else {
                qWarning() << "Recorded file doesn't exist or tiny size:"
                           << item.recFile->fileName();
            }
        } else {
            qWarning() << "Title is null so not saving recorded file";
        }

        item.recFile->remove();
    }

    item.saveRec = false;
    emit streamToRecordChanged(item.id, item.saveRec);
    emit streamRecordableChanged(item.id, false, tmpFile);
}

void ContentServerWorker::saveRecFile(ProxyItem &item)
{
    if (item.recFile) {
        if (item.saveRec) {
            if (const auto title = item.metaint > 0 ?
                    ContentServer::streamTitleFromShoutcastMetadata(item.metadata) : item.title;
                    !title.isEmpty()) {
                qDebug() << "Saving recorded file for title:" << title;
                item.recFile->close();
                auto recFilePath = QDir{Settings::instance()->getRecDir()}.filePath(
                            QString{"%1.%2.%3.%4"}.arg(Utils::escapeName(title), Utils::instance()->randString(),
                                                       "jupii_rec", item.recExt));
                if (item.recFile->exists() && item.recFile->size() > ContentServer::recMinSize) {
                    if (item.recFile->copy(recFilePath)) {
                        const auto url = item.origUrl.isEmpty() ?
                                    Utils::urlFromId(item.id).toString() :
                                    item.origUrl.toString();
                        ContentServer::writeMetaUsingTaglib(
                            recFilePath, // path
                            title, // title
                            item.metaint > 0 ? item.title : item.author, // author
                            ContentServer::recAlbumName, // album
                            //QString("Recorded by %1").arg(Jupii::APP_NAME),
                            {}, // comment
                            url, // URL
                            QDateTime::currentDateTime(), // time of recording
                            item.artPath
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
    emit streamRecordableChanged(item.id, false, {});
}

ContentServerWorker::ProxyItem::~ProxyItem()
{
    if (this->id.isValid()) {
        auto worker = ContentServerWorker::instance();
        if (this->metaint > 0 || this->saveRec) {
            worker->saveRecFile(*this);
        } else {
            worker->endRecFile(*this);
        }
    }
}

void ContentServerWorker::requestHandler(QHttpRequest *req, QHttpResponse *resp)
{
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
        resp->setHeader("Allow", "HEAD, GET");
        sendEmptyResponse(resp, 405);
        return;
    }

    auto cs = ContentServer::instance();

    const auto id = cs->idUrlFromUrl(req->url());

    if (!id) {
        qWarning() << "Unknown content requested!";
        sendEmptyResponse(resp, 404);
        return;
    }

    qDebug() << "Id:" << id->toString();

    const auto meta = cs->getMetaForId(*id, false);
    if (!meta) {
        qWarning() << "No meta item found";
        sendEmptyResponse(resp, 404);
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
            requestForUrlHandler(*id, meta, req, resp);
        }
    } else {
        qWarning() << "Unknown item type";
        sendEmptyResponse(resp, 404);
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
    foreach (auto& item, screenCaptureItems)
        item.resp->end();
    foreach (const auto& item, screenCaptureItems)
        emit itemRemoved(item.id);

    screenCaptureItems.clear();
    screenCaster.reset(nullptr);
}
#endif

void ContentServerWorker::audioErrorHandler()
{
    qWarning() << "Error in audio casting, "
                  "so disconnecting clients and ending casting";
    foreach (auto& item, audioCaptureItems)
        item.resp->end();
    foreach (const auto& item, audioCaptureItems)
        emit itemRemoved(item.id);

    audioCaptureItems.clear();
    audioCaster.reset(nullptr);
}

void ContentServerWorker::micErrorHandler()
{
    qWarning() << "Error in mic casting, "
                  "so disconnecting clients and ending casting";
    foreach (auto& item, micItems)
        item.resp->end();
    foreach (const auto& item, micItems)
        emit itemRemoved(item.id);

    micItems.clear();
    micCaster.reset(nullptr);
}

void ContentServerWorker::requestForFileHandler(const QUrl &id,
                                                const ContentServer::ItemMeta* meta,
                                                QHttpRequest *req, QHttpResponse *resp)
{
    const auto type = static_cast<ContentServer::Type>(Utils::typeFromId(id));
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

void ContentServerWorker::requestForUrlHandlerFallback(const QUrl &id,
                                                       const QUrl &origUrl,
                                                       QHttpRequest *req, QHttpResponse *resp)
{
    qDebug() << "Request fallback from" << id << "to" << origUrl;

    auto meta = ContentServer::instance()->getMeta(origUrl, true); // create new meta
    if (!meta) {
        qWarning() << "No meta item";
        sendEmptyResponse(resp, 404);
        return;
    }

    const auto finalId = Utils::swapUrlInId(meta->url, id);

    qDebug() << "finalId:" << finalId;

    requestForUrlHandler(finalId, meta, req, resp);
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
        qDebug() << "Proxy mode enabled => creating new proxy, count:" << proxyItems.size();

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
        item.seek = meta->flagSet(ContentServer::MetaFlag_Seek);
        item.mode = meta->flagSet(ContentServer::MetaFlag_PlaylistProxy);
        item.head = head; // orig request is HEAD
        item.origUrl = (url == meta->origUrl ? QUrl() : meta->origUrl);

        QString name;
        QUrl icon;
        Utils::pathTypeNameCookieIconFromId(item.id, nullptr, nullptr, &name,
                                            nullptr, &icon, nullptr, &item.author);
        item.title = name.isEmpty() ? ContentServer::bestName(*meta) : name;

        auto cs = ContentServer::instance();
        if (!icon.isEmpty()) {
            auto artMeta = cs->getMeta(icon, false);
            if (artMeta && !artMeta->path.isEmpty()) { // art icon cache found
                item.artPath = artMeta->path;
            } else if (QFileInfo::exists(icon.toLocalFile())) {
                item.artPath = icon.toLocalFile();
            }
        } else if (!meta->albumArt.isEmpty()) {
            if (QFileInfo::exists(meta->albumArt)) {
                item.artPath = meta->albumArt;
            } else {
                auto artMeta = cs->getMeta(QUrl(meta->albumArt), false);
                if (artMeta && !artMeta->path.isEmpty()) // art icon cache found
                    item.artPath = artMeta->path;
            }
        }

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
                        meta->flagSet(ContentServer::MetaFlag_Seek)));
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
                        meta->flagSet(ContentServer::MetaFlag_Seek)));
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
                        meta->flagSet(ContentServer::MetaFlag_Seek)));
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
                        meta->flagSet(ContentServer::MetaFlag_Seek)));
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

    if (!Settings::instance()->getScreenSupported()) {
        qWarning() << "Screen capturing is not supported";
        sendEmptyResponse(resp, 500);
        return;
    }

    if (req->method() == QHttpRequest::HTTP_HEAD) {
        qDebug() << "Sending 200 response without content";
        resp->setHeader("Content-Type", meta->mime);
        resp->setHeader("Connection", "close");
        resp->setHeader("transferMode.dlna.org", "Streaming");
        resp->setHeader("contentFeatures.dlna.org",
                        ContentServer::dlnaContentFeaturesHeader(meta->mime,
                        meta->flagSet(ContentServer::MetaFlag_Seek)));
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
            if (Settings::instance()->getScreenAudio()) {
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
                        meta->flagSet(ContentServer::MetaFlag_Seek)));
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

    qDebug() << "Request:" << (head ? "HEAD" : "GET")
             << item.id;
    qDebug() << "Reply status:" << code << reason;
    if (error != QNetworkReply::NoError)
        qDebug() << "Error code:" << error;
    qDebug() << "Headers:";
    for (const auto &p : reply->rawHeaderPairs()) {
        qDebug() << p.first << p.second;
    }

    if (error != QNetworkReply::NoError || code > 299) {
        qWarning() << "Error response from network server";
        /*if (!item.origUrl.isEmpty()) {
            qDebug() << "Orig URL exists";
            // will be handled in proxyFinished
        } else {
            if (code < 400) {
                code = 404;
            }
            qDebug() << "Ending request with code:" << code;
            sendEmptyResponse(item.resp, code);
        }*/
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
            qDebug() << "item.state:" << item.state;

            // recording only when: stream proxy mode && shoutcast && valid audio extension
            //if (item.state == 1 && item.metaint > 0 &&
            if (item.state == 1 &&
                Settings::instance()->getRec()) {
                auto ext = ContentServer::getExtensionFromAudioContentType(mime);
                if (!ext.isEmpty()) {
                    qDebug() << "Stream should be recorded";
                    item.recExt = ext;
                    item.recFile = std::shared_ptr<QFile>(new QFile());

                    if (item.metaint == 0) { // non-shoutcast stream
                        openRecFile(item);
                    }
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
    auto reply = qobject_cast<QNetworkReply*>(sender());

    if (!proxyItems.contains(reply)) {
        qWarning() << "Deleting reply because cannot find proxy item";
        reply->deleteLater();
        return;
    }

    auto &item = proxyItems[reply];

    if (!item.finished) {
        auto code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        //auto reason = reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();
        auto error = reply->error();
        qDebug() << "Request:" << (item.req->method() == QHttpRequest::HTTP_GET ? "GET" : "HEAD")
                 << item.id;
        if (error != QNetworkReply::NoError)
            qDebug() << "Error code:" << error;

        if (item.state == 0) {
            /*if (!item.origUrl.isEmpty()) {
                qDebug() << "Request ended with error but orig URL exists, so trying new request";
                requestForUrlHandlerFallback(item.id, item.origUrl, item.req, item.resp);
            } else {
                if (code < 200)
                    code = 404;
                qDebug() << "Ending request with code:" << code;
                sendEmptyResponse(item.resp, code);
            }*/
            if (code < 200)
                code = 404;
            qDebug() << "Ending request with code:" << code;
            sendEmptyResponse(item.resp, code);
        } else if (item.state == 2) {
            qDebug() << "Playlist proxy mode, so sending all data";
            auto data = reply->readAll();
            if (!data.isEmpty()) {
                // Resolving relative URLs in a playlist
                resolveM3u(data, reply->url().toString());
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
    qDebug() << "bytes:" << bytes;*/

    if (bytes > item.metaint) {
        Q_ASSERT(item.metaint >= item.metacounter);

        int nmeta = bytes / item.metaint;
        int totalsize = 0;
        QList<QPair<int,int>> rpoints; // (start,size) to remove from data

        //qDebug() << "nmeta:" << nmeta;

        int mcount = 0;
        for (int i = 0; i < nmeta; ++i) {
            int offset = i * item.metaint + totalsize + i;
            int start = item.metaint - item.metacounter;

            /*qDebug() << "------- soutcast metadata scan:" << i;
            qDebug() << "offset:" << offset;
            qDebug() << "start:" << start;*/

            if ((start + offset) < count) {
                int size = 16 * static_cast<uchar>(data.at(start + offset));
                int maxsize = count - (start + offset);

                /*qDebug() << "------- metadata detected ---------";
                qDebug() << "totalsize:" << totalsize;
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
#ifdef QT_DEBUG
                            auto oldTitle = ContentServer::streamTitleFromShoutcastMetadata(item.metadata);
                            qDebug() << "metadata:" << item.metadata << oldTitle
                                     << "->" << metadata << newTitle;
#endif
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
                mcount += item.metaint + 1;
            } else {
                mcount += item.metaint;
                break;
            }
        }

        item.metacounter = bytes - mcount - totalsize;

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
    foreach (const auto& item, audioCaptureItems) {
#ifdef QT_DEBUG
        qDebug() << "pulseStreamUpdated:" << item.id << name;
#endif
        emit pulseStreamUpdated(item.id, name);
    }
    foreach (const auto& item, screenCaptureItems) {
#ifdef QT_DEBUG
        qDebug() << "pulseStreamUpdated:" << item.id << name;
#endif
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
            if (item.metaint > 0) {
                processShoutcastMetadata(data, item);
            } else {
                // recording for non-shoutcast streams
                if (item.recFile && item.recFile->isOpen()) {
                    if (item.recFile->size() < ContentServer::recMaxSize)
                        item.recFile->write(data);
                }
            }

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

ContentServer::ItemMeta::ItemMeta() :
    flags(MetaFlag_Unknown|MetaFlag_Local|MetaFlag_Seek)
{
}

ContentServer::ItemMeta::ItemMeta(const ItemMeta *meta) :
    trackerId(meta->trackerId),
    url(meta->url),
    origUrl(meta->origUrl),
    path(meta->path),
    filename(meta->filename),
    title(meta->title),
    mime(meta->mime),
    comment(meta->comment),
    album(meta->album),
    albumArt(meta->albumArt),
    artist(meta->artist),
    didl(meta->didl),
    upnpDevId(meta->upnpDevId),
    type(meta->type),
    itemType(meta->itemType),
    duration(meta->duration),
    bitrate(meta->bitrate),
    sampleRate(meta->sampleRate),
    channels(meta->channels),
    size(meta->size),
    recUrl(meta->recUrl),
    recDate(meta->recDate),
    metaUpdateTime(meta->metaUpdateTime),
    flags(meta->flags)
{
}

ContentServer::ContentServer(QObject *parent) :
    QThread(parent)
{
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
    qDebug() << "Display status:" << state;
    emit displayStatusChanged(state == "on");
}

QString ContentServer::dlnaOrgFlagsForFile()
{
    char flags[448];
    sprintf(flags, "%s=%.8x%.24x", "DLNA.ORG_FLAGS",
            DLNA_ORG_FLAG_BYTE_BASED_SEEK |
            DLNA_ORG_FLAG_STREAMING_TRANSFER_MODE |
            DLNA_ORG_FLAG_BACKGROUND_TRANSFER_MODE |
            DLNA_ORG_FLAG_CONNECTION_STALL |
            DLNA_ORG_FLAG_DLNA_V15, 0);
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
            DLNA_ORG_FLAG_STREAMING_TRANSFER_MODE |
            DLNA_ORG_FLAG_CONNECTION_STALL |
            DLNA_ORG_FLAG_DLNA_V15, 0);
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

QString ContentServer::extFromMime(const QString &mime)
{
    if (m_imgMimeToExtMap.contains(mime))
        return m_imgMimeToExtMap.value(mime);
    if (m_musicMimeToExtMap.contains(mime))
        return m_musicMimeToExtMap.value(mime);

    return QString();
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

QString ContentServer::extractItemFromDidl(const QString &didl)
{
    QRegExp rx("<DIDL-Lite[^>]*>(.*)</DIDL-Lite>");
    if (rx.indexIn(didl) != -1) {
        return rx.cap(1);
    }
    return QString();
}

bool ContentServer::getContentMetaItem(const QString &id, QString &meta, bool includeDummy)
{   
    const auto item = getMeta(Utils::urlFromId(id), false);
    if (!item) {
        qWarning() << "No meta item found";
        return false;
    }

    if (!includeDummy && item->dummy()) {
        return false;
    }

    int relay = Settings::instance()->getRemoteContentMode();
    QUrl url;

    if (item->itemType == ItemType_Upnp) {
        qDebug() << "Item is upnp and didl item for upnp is not supported";
        return false;
    } else if (item->itemType == ItemType_Url &&
                   (relay == 2 || (relay == 4 && !item->flagSet(MetaFlag_Seek)))) {
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
        getContentMetaItem(metaIdx[didlId], meta, false);
        return true;
    }
    return false;
}

bool ContentServer::getContentMetaItem(const QString &id, const QUrl &url,
                                   QString &meta, const ItemMeta *item)
{
    QString path, name, desc, author;
    int t = 0;
    int duration = 0;
    QUrl icon;
    if (!Utils::pathTypeNameCookieIconFromId(QUrl(id), &path, &t, &name, nullptr,
          &icon, &desc, &author, nullptr, nullptr, nullptr, nullptr, &duration))
        return false;

    bool audioType = static_cast<Type>(t) == TypeMusic; // extract audio stream from video

    AvData data;
    if (audioType && item->flagSet(MetaFlag_Local)) {
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
            auto artUrl = QFileInfo::exists(item->albumArt) ?
                                QUrl::fromLocalFile(item->albumArt) :
                                QUrl(item->albumArt);
            auto id = Utils::idFromUrl(icon.isEmpty() ? artUrl : icon, artCookie);
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

    if (audioType && item->flagSet(MetaFlag_Local)) { // extract audio from video
        // puting audio stream info instead video file
        if (data.size > 0)
            m << "size=\"" << QString::number(data.size) << "\" ";
        m << "protocolInfo=\"http-get:*:" << data.mime << ":"
          << dlnaContentFeaturesHeader(data.mime, true, true)
          << "\" ";
    } else {
        if (item->size > 0)
            m << "size=\"" << QString::number(item->size) << "\" ";
        m << "protocolInfo=\"http-get:*:" << item->mime << ":"
          << dlnaContentFeaturesHeader(item->mime, item->flagSet(MetaFlag_Seek), true)
          << "\" ";
    }

    if (duration == 0) {
        duration = item->duration;
    }
    if (duration > 0) {
        int seconds = duration % 60;
        int minutes = ((duration - seconds) / 60) % 60;
        int hours = (duration - (minutes * 60) - seconds) / 3600;
        QString durationStr = QString::number(hours) + ":" + (minutes < 10 ? "0" : "") +
                           QString::number(minutes) + ":" + (seconds < 10 ? "0" : "") +
                           QString::number(seconds) + ".000";
        m << "duration=\"" << durationStr << "\" ";
    }

    if (audioType && item->flagSet(MetaFlag_Local)) {
        if (data.bitrate > 0)
            m << "bitrate=\"" << QString::number(data.bitrate) << "\" ";
        if (item->sampleRate > 0)
            m << "sampleFrequency=\"" << QString::number(item->sampleRate) << "\" ";
        if (data.channels > 0)
            m << "nrAudioChannels=\"" << QString::number(data.channels) << "\" ";
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

void ContentServer::registerExternalConnections()
{
    connect(PlaylistModel::instance(), &PlaylistModel::activeItemChanged,
            this, &ContentServer::cleanTmpRec, Qt::QueuedConnection);
}

bool ContentServer::getContentUrl(const QString &id, QUrl &url, QString &meta,
                                  QString cUrl)
{
    if (!Utils::isIdValid(id)) {
        return false;
    }

    const auto item = getMeta(Utils::urlFromId(id), false);
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
               (relay == 2 || (relay == 4 && !item->flagSet(MetaFlag_Seek)))) {
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
    if (!ConnectivityDetector::instance()->selectNetworkIf(ifname, addr)) {
        qWarning() << "Cannot find valid network interface";
        return false;
    }

    auto hash = QString::fromUtf8(encrypt(id.toUtf8()));

    const int safe_hash_size = 800;
    if (hash.size() > safe_hash_size) {
        qWarning() << "Hash size exceeds safe limit. Some clients might have problems.";
        auto truncatedHash = hash.left(safe_hash_size);
        if (!fullHashes.contains(truncatedHash)) {
            fullHashes.insert(truncatedHash, hash);
            emit fullHashesUpdated();
        } else {
            fullHashes.insert(truncatedHash, hash);
        }
        hash = truncatedHash;
    }

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

    auto key = Settings::instance()->getKey();
    QByteArray tmp{key};
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
    auto _data = QByteArray::fromBase64(data, QByteArray::Base64UrlEncoding |
                                              QByteArray::OmitTrailingEquals);
    auto key = Settings::instance()->getKey();
    QByteArray tmp{key};
    while (key.size() < _data.size())
        key += tmp;

    for (int i = 0; i < _data.size(); ++i)
        _data[i] = _data.at(i) ^ key.at(i);

    return _data;
}

QString ContentServer::pathFromUrl(const QUrl &url) const
{
    bool isFile;
    if (auto id = idUrlFromUrl(url, &isFile); id && isFile)
        return id->toLocalFile();
    return {};
}

std::optional<QUrl> ContentServer::idUrlFromUrl(const QUrl &url, bool* isFile) const
{
    QUrlQuery q1{url};
    QString hash;
    if (q1.hasQueryItem(Utils::idKey)) {
        hash = q1.queryItemValue(Utils::idKey);
    } else {
        hash = url.path();
        hash = hash.right(hash.length()-1);
    }

    if (fullHashes.contains(hash)) {
        //qDebug() << "converting truncated hash to full hash";
        hash = fullHashes.value(hash);
    }

    QUrl id{QString::fromUtf8(decrypt(hash.toUtf8()))};
    if (!id.isValid())
        return std::nullopt;

    QUrlQuery q2{id};
    if (!q2.hasQueryItem(Utils::cookieKey) || q2.queryItemValue(Utils::cookieKey).isEmpty())
        return std::nullopt;

    if (id.isLocalFile()) {
        QFileInfo file{id.toLocalFile()};
        if (!file.exists() || !file.isFile())
            return std::nullopt;
        if (isFile)
            *isFile = true;
        return id;
    }

    if (isFile)
        *isFile = false;

    return id;
}

QString ContentServer::idFromUrl(const QUrl &url) const
{
    if (auto id = idUrlFromUrl(url))
        return id->toString();

    return {};
}

QString ContentServer::urlFromUrl(const QUrl &url) const
{
    if (auto id = idUrlFromUrl(url))
        return Utils::urlFromId(id.value()).toString();

    return {};
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
    data.bitrate = int(codec->bit_rate);
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

    int aidx = av_find_best_stream(ic, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (aidx < 0) {
        qWarning() << "No audio stream found";
        avformat_close_input(&ic);
        return false;
    }

#ifdef QT_DEBUG
    // Debug: audio stream side data
    qDebug() << "nb_streams:" << ic->nb_streams;
    qDebug() << "audio stream index is:" << aidx;
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
#endif

    if (!fillAvDataFromCodec(ic->streams[aidx]->codecpar, path, data)) {
        qWarning() << "Unable to find correct mime for the codec:"
                   << ic->streams[aidx]->codecpar->codec_id;
        avformat_close_input(&ic);
        return false;
    }

    qDebug() << "Audio stream content type" << data.mime;
    qDebug() << "Audio stream bitrate" << data.bitrate;
    qDebug() << "Audio stream channels" << data.channels;

    AVOutputFormat *of = nullptr;
    auto t = data.type.toLatin1();
    of = av_guess_format(t.data(), nullptr, nullptr);
    if (!of) {
        qWarning() << "av_guess_format error";
        avformat_close_input(&ic);
        return false;
    }

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

    /*qDebug() << "ast->codec->sample_fmt:" << ast->codec->sample_fmt;
    qDebug() << "ast->codec->bit_rate:" << ast->codec->bit_rate;
    qDebug() << "ast->codec->sample_rate:" << ast->codec->sample_rate;
    qDebug() << "ast->codec->channels:" << ast->codec->channels;
    qDebug() << "ast->codecpar->codec_tag:" << ast->codecpar->codec_tag;
    qDebug() << "ic->streams[aidx]->codecpar->codec_id:" << ic->streams[aidx]->codecpar->codec_id;
    qDebug() << "ast->codecpar->codec_id:" << ast->codecpar->codec_id;*/

    qDebug() << "Extracted audio file:" << data.path;

    QFile audioFile(data.path);
    if (audioFile.exists()) {
        qDebug() << "Extracted audio stream already exists";
        data.size = QFileInfo(data.path).size();
        avformat_close_input(&ic);
        avformat_free_context(oc);
        return true;
    }

    auto bapath = data.path.toUtf8();
    if (avio_open(&oc->pb, bapath.data(), AVIO_FLAG_WRITE) < 0) {
        qWarning() << "avio_open error";
        avformat_close_input(&ic);
        avformat_free_context(oc);
        return false;
    }

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
#ifdef QT_DEBUG
            // Debug: audio stream packet side data
            for (int i = 0; i < pkt.side_data_elems; ++i) {
                qDebug() << "Audio stream packet side data:";
                qDebug() << "type:" << pkt.side_data[i].type;
                qDebug() << "size:" << pkt.side_data[i].size;
                QByteArray data(reinterpret_cast<const char*>(pkt.side_data[i].data),
                                pkt.side_data[i].size);
                qDebug() << "data:" << data;
            }
#endif

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

    if (av_write_trailer(oc) < 0) {
        qWarning() << "av_write_trailer error";
        avformat_close_input(&ic);
        avio_close(oc->pb);
        avformat_free_context(oc);
        audioFile.remove();
        return false;
    }

    avformat_close_input(&ic);

    if (avio_close(oc->pb) < 0) {
        qWarning() << "avio_close error";
    }

    avformat_free_context(oc);

    data.size = QFileInfo{data.path}.size();

    return true;
}

void ContentServer::removeMeta(const QUrl &url)
{
    metaCache.remove(url);
}

const ContentServer::ItemMeta* ContentServer::getMetaForImg(const QUrl &url,
                                                      bool createNew)
{
    return getMeta(url, createNew, {}, {}, false, true);
}

const ContentServer::ItemMeta* ContentServer::getMeta(const QUrl &url,
                                                      bool createNew,
                                                      const QUrl &origUrl,
                                                      const QString &app,
                                                      bool ytdl, bool img,
                                                      bool refresh)
{
#ifdef QT_DEBUG
    qDebug() << "getMeta:" << url << createNew << ytdl << img << refresh;
#endif
    auto it = getMetaCacheIterator(url, createNew, origUrl, app, ytdl, img, refresh);
    auto meta = it == metaCache.cend() ? nullptr : &it.value();
    return meta;
}

bool ContentServer::metaExists(const QUrl &url) const
{
    return std::as_const(metaCache).find(url) != metaCache.cend();
}

const ContentServer::ItemMeta* ContentServer::getMetaForId(const QUrl &id, bool createNew)
{
    auto url = Utils::urlFromId(id);
    return getMeta(url, createNew);
}

const QHash<QUrl, ContentServer::ItemMeta>::const_iterator
ContentServer::getMetaCacheIterator(const QUrl &url, bool createNew,
                                    const QUrl &origUrl, const QString &app, bool ytdl, bool img,
                                    bool refresh)
{
    if (url.isEmpty())
        return metaCache.cend();

    if (refresh)
        removeMeta(url);

    auto i = std::as_const(metaCache).find(url);

    if (i == metaCache.cend()) {
        qDebug() << "Meta data for" << url << "not cached";
        if (createNew)
            return makeItemMeta(url, origUrl, app, ytdl, img, refresh);
        else
            return metaCache.cend();
    }

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
    return metaCache.cend();
}

const QHash<QUrl, ContentServer::ItemMeta>::const_iterator
ContentServer::makeItemMetaUsingTracker(const QUrl &url)
{
    const QString fileUrl = url.toString(QUrl::EncodeUnicode|QUrl::EncodeSpaces);
    const QString path = url.toLocalFile();

    auto tracker = Tracker::instance();
    const QString query = queryTemplate.arg(fileUrl, tracker->tracker3() ? "nmm:artist" : "nmm:performer");

    if (!tracker->query(query, false)) {
        qWarning() << "Cannot get tracker data for url:" << fileUrl;
        return metaCache.cend();
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
            meta.albumArt = Tracker::genAlbumArtFile(meta.album, meta.artist);
            meta.type = typeFromMime(meta.mime);
            meta.size = file.size();
            meta.itemType = ItemType_LocalFile;
            meta.setFlags(MetaFlag_Valid|MetaFlag_Local|MetaFlag_Seek);

            // Recording meta
            if (meta.album == recAlbumName) {
                if (!readMetaUsingTaglib(path,
                           meta.title, meta.artist, meta.album,
                           meta.comment, meta.recUrl, meta.recDate,
                           meta.albumArt)) {
                    qWarning() << "Cannot read meta with TagLib";
                }
            }

            return std::as_const(metaCache).find(url);
        }
    }

    return metaCache.cend();
}

QString ContentServer::readTitleUsingTaglib(const QString &path)
{
    TagLib::FileRef f(path.toUtf8().constData(), false);

    if (!f.isNull())  {
        auto tag = f.tag();
        if (tag) {
            return QString::fromWCharArray(tag->title().toCWString());
        }
    }

    return {};
}

bool ContentServer::readMetaUsingTaglib(const QString &path,
                                        QString &title,
                                        QString &artist,
                                        QString &album,
                                        QString &comment,
                                        QString &recUrl,
                                        QDateTime &recDate,
                                        QString &artPath,
                                        int *duration,
                                        double *bitrate,
                                        double *sampleRate,
                                        int *channels)
{
    bool readAudioProperties = duration || bitrate || sampleRate || channels;

    TagLib::FileRef f(path.toUtf8().constData(), readAudioProperties);
    if (!f.isNull())  {        
        auto tag = f.tag();
        if (tag) {
            title = QString::fromWCharArray(tag->title().toCWString());
            artist = QString::fromWCharArray(tag->artist().toCWString());
            album = QString::fromWCharArray(tag->album().toCWString());
            comment = QString::fromWCharArray(tag->comment().toCWString());

            if (album == ContentServer::recAlbumName) { // Rec additional tags
                auto tags = f.file()->properties();
                auto it = tags.find(ContentServer::recDateTagName.toStdString());
                if (it != tags.end())
                    recDate = QDateTime::fromString(
                                QString::fromWCharArray(it->second.toString()
                                         .toCWString()), Qt::ISODate);
                it = tags.find(ContentServer::recUrlTagName.toStdString());
                if (it != tags.end()) {
                    recUrl = QString::fromWCharArray(it->second.toString().toCWString());
                } else { // old rec url tag
                    it = tags.find(ContentServer::recUrlTagName2.toStdString());
                    if (it != tags.end()) {
                        recUrl = QString::fromWCharArray(it->second.toString().toCWString());
                    }
                }
            }

            if (readAudioProperties) {
                auto prop = f.audioProperties();
                if (prop) {
                    if (duration)
                        *duration = prop->length();
                    if (bitrate)
                        *bitrate = prop->bitrate();
                    if (sampleRate)
                        *sampleRate = prop->sampleRate();
                    if (channels)
                        *channels = prop->channels();
                }
            }

            const auto albumArt = QString("%1/art_image_%2.")
                    .arg(Settings::instance()->getCacheDir(),
                         Utils::instance()->hash(path));
            if (QFileInfo::exists(albumArt + "jpg")) {
                artPath = albumArt + "jpg";
            } else if (QFileInfo::exists(albumArt + "png")) {
                artPath = albumArt + "png";
            } else {
                auto mf = dynamic_cast<TagLib::MPEG::File*>(f.file());
                if (mf) {
                    TagLib::ID3v2::Tag *tag = mf->ID3v2Tag(true);
                    TagLib::ID3v2::FrameList fl = tag->frameList("APIC");

                    if (!fl.isEmpty()) {
                        TagLib::ID3v2::AttachedPictureFrame *frame =
                                dynamic_cast<TagLib::ID3v2::AttachedPictureFrame*>(fl.front());
                        if (frame) {
                            auto ext = m_imgMimeToExtMap.value(QString::fromStdString(frame->mimeType().to8Bit()));
                            if (!ext.isEmpty()) {
                                if (Utils::writeToFile(albumArt + ext,
                                       QByteArray::fromRawData(frame->picture().data(),
                                            static_cast<int>(frame->picture().size())))) {
                                    artPath = albumArt + ext;
                                } else {
                                    qWarning() <<  "Cannot write art file:" << (albumArt + ext);
                                }
                            }
                        }
                    }
                } else {
                    qWarning() << "Cannot extract art because file is not MPEG";
                }
            }

            /*qDebug() << "-- TAG (properties) --";
            for(TagLib::PropertyMap::ConstIterator it = tags.begin(); it != tags.end(); ++it) {
                qDebug() << QString::fromWCharArray(it->first.toCWString());
            }*/

            return true;
        }
    }

    return false;
}

bool ContentServer::writeMetaUsingTaglib(const QString &path, const QString &title,
                                          const QString &artist, const QString &album,
                                          const QString &comment,
                                          const QString &recUrl,
                                          const QDateTime &recDate,
                                          const QString &artPath)
{
    TagLib::FileRef f(path.toUtf8().constData(), false);
    if (!f.isNull())  {
        auto mf = dynamic_cast<TagLib::MPEG::File*>(f.file());
        if (mf) {
            //qDebug() << "MPEG tag";
            mf->strip(); // remove old tags
            mf->save();

            auto tag = mf->ID3v2Tag(true);

            if (!title.isEmpty())
                tag->setTitle(title.toStdWString());
            if (!artist.isEmpty())
                tag->setArtist(artist.toStdWString());
            if (!album.isEmpty())
                tag->setAlbum(album.toStdWString());
            if (!comment.isEmpty())
                tag->setComment(comment.toStdWString());

            // Rec additional tags
            if (!recUrl.isEmpty()) {
                auto frame = TagLib::ID3v2::Frame::createTextualFrame(
                            ContentServer::recUrlTagName.toStdString(),
                                {recUrl.toStdString()});
                tag->addFrame(frame);
            }
            if (!recDate.isNull()) {
                auto frame = TagLib::ID3v2::Frame::createTextualFrame(
                            ContentServer::recDateTagName.toStdString(),
                            {recDate.toString(Qt::ISODate)
                             .toStdString()});
                tag->addFrame(frame);
            }

            if (!artPath.isEmpty()) {
                auto mime = m_imgExtMap.value(artPath.split('.').last());
                if (!mime.isEmpty()) {
                    QFile art(artPath);
                    if (art.open(QIODevice::ReadOnly)) {
                        auto bytes = art.readAll();
                        art.close();
                        if (!bytes.isEmpty()) {
                            auto apf = new TagLib::ID3v2::AttachedPictureFrame();
                            apf->setPicture(TagLib::ByteVector(bytes.constData(), uint32_t(bytes.size())));
                            apf->setType(TagLib::ID3v2::AttachedPictureFrame::FrontCover);
                            apf->setMimeType(mime.toStdString());
                            apf->setDescription("Cover");
                            tag->addFrame(apf);
                        }
                    }
                }
            }

            mf->save(TagLib::MPEG::File::ID3v2);
            return true;
        } else {
            auto tag = f.tag();
            if (tag) {
                //qDebug() << "Generic tag";
                if (!title.isEmpty())
                    tag->setTitle(title.toStdWString());
                if (!artist.isEmpty())
                    tag->setArtist(artist.toStdWString());
                if (!album.isEmpty())
                    tag->setAlbum(album.toStdWString());
                if (!comment.isEmpty())
                    tag->setComment(comment.toStdWString());

                if (!recUrl.isEmpty() || !recDate.isNull()) { // Rec additional tags
                    auto file = f.file();
                    auto tags = file->properties();

                    if (!recUrl.isEmpty()) {
                        auto url_key = ContentServer::recUrlTagName.toStdString();
                        auto url_val = TagLib::StringList(recUrl.toStdString());
                        auto it = tags.find(url_key);
                        if (it != tags.end())
                            it->second = url_val;
                        else
                            tags.insert(url_key, url_val);
                    }
                    if (!recDate.isNull()) {
                        auto date_key = ContentServer::recDateTagName.toStdString();
                        auto date_val = TagLib::StringList(recDate.toString(Qt::ISODate).toStdString());
                        auto it = tags.find(date_key);
                        if (it != tags.end())
                            it->second = date_val;
                        else
                            tags.insert(date_key, date_val);
                    }

                    file->setProperties(tags);
                }

                f.save();
                return true;
            }
        }
    }

    return false;
}

QString ContentServer::minResUrl(const UPnPClient::UPnPDirObject &item)
{
    uint64_t l = item.m_resources.size();

    if (l == 0) {
        return QString();
    }

    uint64_t min_i = 0;
    int min_width = std::numeric_limits<int>::max();

    for (uint64_t i = 0; i < l; ++i) {
        std::string val; QSize();
        if (item.m_resources[i].m_uri.empty())
            continue;
        if (item.getrprop(uint32_t(i), "resolution", val)) {
            uint64_t pos = val.find('x');
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
    auto path = url.toLocalFile();
    QFileInfo file(path);

    ItemMeta meta;
    meta.url = url;
    meta.mime = getContentMimeByExtension(path);
    meta.type = getContentTypeByExtension(path);
    meta.path = std::move(path);
    meta.size = file.size();
    meta.filename = file.fileName();
    meta.itemType = ItemType_LocalFile;
    meta.setFlags(MetaFlag_Valid|MetaFlag_Local);

    if (meta.type == TypeImage) {
        meta.setFlags(MetaFlag_Seek, false);
    } else {
        meta.setFlags(MetaFlag_Seek);

        if (!readMetaUsingTaglib(meta.path,
                           meta.title, meta.artist, meta.album,
                           meta.comment, meta.recUrl, meta.recDate,
                           meta.albumArt, &meta.duration, &meta.bitrate,
                           &meta.sampleRate, &meta.channels)) {
            qWarning() << "Cannot read meta with TagLib";
        }

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

    if (QThread::currentThread()->isInterruptionRequested()) {
        qWarning() << "Thread interruption was requested";
        return metaCache.cend();
    }

    auto spl = url.path(QUrl::FullyEncoded).split('/');
    if (spl.size() < 3) {
        qWarning() << "Path is too short";
        return metaCache.cend();
    }

    auto did = QUrl::fromPercentEncoding(spl.at(1).toLatin1()); // cdir dev id
    auto id = QUrl::fromPercentEncoding(spl.at(2).toLatin1()); // cdir item id
#ifdef QT_DEBUG
    qDebug() << "did:" << did;
    qDebug() << "id:" << id;
#endif

    auto cd = Services::instance()->contentDir;
    if (!cd->init(did)) {
        qWarning() << "Cannot init CDir service";
        return metaCache.cend();
    }

    if (!cd->getInited()) {
        qDebug() << "CDir is not inited, so waiting for init";

        QEventLoop loop;
        connect(cd.get(), &Service::initedChanged, &loop, &QEventLoop::quit);
        QTimer::singleShot(httpTimeout, &loop, &QEventLoop::quit); // timeout
        loop.exec(); // waiting for init...

        if (!cd->getInited()) {
            qDebug() << "Cannot init CDir service";
            return metaCache.cend();
        }
    }

    UPnPClient::UPnPDirContent content;
    if (!cd->readItem(id, content)) {
        qDebug() << "Cannot read from CDir service";
        return metaCache.cend();
    }

    if (content.m_items.empty()) {
        qDebug() << "Item doesn't exist on CDir";
        return metaCache.cend();
    }

    UPnPClient::UPnPDirObject &item = content.m_items[0];

    if (item.m_resources.empty()) {
        qDebug() << "Item doesn't have resources";
        return metaCache.cend();
    }

    if (item.m_resources[0].m_uri.empty()) {
        qDebug() << "Item uri is empty";
        return metaCache.cend();
    }

    auto surl = QString::fromStdString(item.m_resources[0].m_uri);

    ItemMeta meta;
    meta.url = QUrl(surl);
    meta.type = typeFromUpnpClass(QString::fromStdString(item.getprop("upnp:class")));
    UPnPP::ProtocolinfoEntry proto;
    if (item.m_resources[0].protoInfo(proto)) {
        meta.mime = QString::fromStdString(proto.contentFormat);
        qDebug() << "proto info mime:" << meta.mime;
    } else {
        qWarning() << "Cannot parse proto info";
        meta.mime = "audio/mpeg";
    }
    meta.size = 0;
    meta.album = QString::fromStdString(item.getprop("upnp:album"));
    meta.artist = Utils::parseArtist(QString::fromStdString(item.getprop("upnp:artist")));
    meta.didl = QString::fromStdString(item.getdidl());
    meta.didl = meta.didl.replace(surl, "%URL%");
    meta.albumArt = meta.type == TypeImage ? minResUrl(item) : QString::fromStdString(item.getprop("upnp:albumArtURI"));
    meta.filename = url.fileName();
    meta.title = QString::fromStdString(item.m_title);
    if (meta.title.isEmpty())
        meta.title = meta.filename;
    meta.upnpDevId = std::move(did);
    meta.itemType = ItemType_Upnp;
    meta.setFlags(MetaFlag_Valid);
    meta.setFlags(MetaFlag_Local|MetaFlag_Seek, false);
#ifdef QT_DEBUG
    qDebug() << "DIDL:" << meta.didl;
#endif
    return metaCache.insert(url, meta);
}

const QHash<QUrl, ContentServer::ItemMeta>::const_iterator
ContentServer::makeMicItemMeta(const QUrl &url)
{
    ItemMeta meta;
    meta.url = url;
    meta.channels = MicCaster::channelCount;
    meta.sampleRate = MicCaster::sampleRate;
    meta.mime = m_musicExtMap.value("mp3");
    meta.type = TypeMusic;
    meta.size = 0;
    meta.title = tr("Microphone");
    meta.itemType = ItemType_Mic;
#if defined(SAILFISH) || defined(KIRIGAMI)
    meta.albumArt = IconProvider::pathToNoResId("icon-mic");
#endif
    meta.setFlags(MetaFlag_Valid|MetaFlag_Local);
    meta.setFlags(MetaFlag_Seek, false);

    return metaCache.insert(url, meta);
}

const QHash<QUrl, ContentServer::ItemMeta>::const_iterator
ContentServer::makeAudioCaptureItemMeta(const QUrl &url)
{
    ItemMeta meta;
    meta.url = url;
    meta.mime = m_musicExtMap.value("mp3");
    meta.type = TypeMusic;
    meta.size = 0;
    meta.title = tr("Audio capture");
    meta.itemType = ItemType_AudioCapture;
#if defined(SAILFISH) || defined(KIRIGAMI)
    meta.albumArt = IconProvider::pathToNoResId("icon-pulse");
#endif
    meta.setFlags(MetaFlag_Valid|MetaFlag_Local);
    meta.setFlags(MetaFlag_Seek, false);

    return metaCache.insert(url, meta);
}

const QHash<QUrl, ContentServer::ItemMeta>::const_iterator
ContentServer::makeScreenCaptureItemMeta(const QUrl &url)
{
    ItemMeta meta;
    meta.url = url;
    meta.mime = m_videoExtMap.value("tsv");
    meta.type = TypeVideo;
    meta.size = 0;
    meta.title = tr("Screen capture");
    meta.itemType = ItemType_ScreenCapture;
#if defined(SAILFISH) || defined(KIRIGAMI)
    meta.albumArt = IconProvider::pathToNoResId("icon-screen");
#endif
    meta.setFlags(MetaFlag_Valid|MetaFlag_Local);
    meta.setFlags(MetaFlag_Seek, false);

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
ContentServer::makeItemMetaUsingYtdlApi(const QUrl &url, ItemMeta &meta,
                                          std::shared_ptr<QNetworkAccessManager> nam,
                                          int counter)
{
    qDebug() << "Trying to find url with Ytdl API:" << url;

    if (QThread::currentThread()->isInterruptionRequested()) {
        qWarning() << "Thread interruption was requested";
        return metaCache.cend();
    }

    auto track = YtdlApi{nam}.track(url);

    if (track.streamUrl.isEmpty()) {
        qWarning() << "Ytdl API returned empty url";
        return metaCache.cend();
    }

    meta.title = std::move(track.title);
    meta.app = "ytdl";
    meta.setFlags(MetaFlag_YtDl);

    auto newUrl = std::move(track.streamUrl);
    Utils::fixUrl(newUrl);

    return makeItemMetaUsingHTTPRequest2(newUrl, meta, nam, counter + 1);
}

const QHash<QUrl, ContentServer::ItemMeta>::const_iterator
ContentServer::makeItemMetaUsingBcApi(const QUrl &url, ItemMeta &meta,
                                          std::shared_ptr<QNetworkAccessManager> nam,
                                          int counter)
{
    qDebug() << "Trying to find url with Bc API:" << url;

    if (QThread::currentThread()->isInterruptionRequested()) {
        qWarning() << "Thread interruption was requested";
        return metaCache.cend();
    }

    auto track = BcApi{nam}.track(url);

    if (track.streamUrl.isEmpty()) {
        qWarning() << "Bc API returned empty url";
        return metaCache.cend();
    }

    meta.title = std::move(track.title);
    meta.album = std::move(track.album);
    meta.artist = std::move(track.artist);
    meta.duration = track.duration;
    meta.albumArt = track.imageUrl.toString();
    meta.app = "bc";
    meta.setFlags(MetaFlag_YtDl);

    auto newUrl = std::move(track.streamUrl);
    Utils::fixUrl(newUrl);

    return makeItemMetaUsingHTTPRequest2(newUrl, meta, nam, counter + 1);
}

const QHash<QUrl, ContentServer::ItemMeta>::const_iterator
ContentServer::makeItemMetaUsingSoundcloudApi(const QUrl &url, ItemMeta &meta,
                                          std::shared_ptr<QNetworkAccessManager> nam,
                                          int counter)
{
    qDebug() << "Trying to find url with Soundcloud API:" << url;

    if (QThread::currentThread()->isInterruptionRequested()) {
        qWarning() << "Thread interruption was requested";
        return metaCache.cend();
    }

    auto track = SoundcloudApi{nam}.track(url);

    if (track.streamUrl.isEmpty()) {
        qWarning() << "Soundcloud API returned empty url";
        return metaCache.cend();
    }

    meta.title = std::move(track.title);
    meta.album = std::move(track.album);
    meta.artist = std::move(track.artist);
    meta.duration = track.duration;
    meta.albumArt = track.imageUrl.toString();
    meta.app = "soundcloud";
    meta.setFlags(MetaFlag_YtDl);

    auto newUrl = std::move(track.streamUrl);
    Utils::fixUrl(newUrl);

    return makeItemMetaUsingHTTPRequest2(newUrl, meta, nam, counter + 1);
}

const QHash<QUrl, ContentServer::ItemMeta>::const_iterator
ContentServer::makeItemMetaUsingHTTPRequest(const QUrl &url,
                                            const QUrl &origUrl,
                                            const QString &app,
                                            bool ytdl, bool refresh,
                                            bool art)
{
    ItemMeta meta;
    meta.setFlags(MetaFlag_YtDl, ytdl);
    meta.setFlags(MetaFlag_Refresh, refresh); // meta refreshing
    meta.setFlags(MetaFlag_Art, art);
    meta.app = app;

    QUrl fixed_url(url);
    QUrl fixed_origUrl(origUrl);
    Utils::fixUrl(fixed_url);
    Utils::fixUrl(fixed_origUrl);

    if (!origUrl.isEmpty() && fixed_origUrl != fixed_url) {
        meta.setFlags(MetaFlag_OrigUrl);
        meta.origUrl = fixed_origUrl;
    } else {
        meta.setFlags(MetaFlag_OrigUrl, false);
        meta.origUrl = fixed_url;
    }
    return makeItemMetaUsingHTTPRequest2(fixed_url, meta);
}

const QHash<QUrl, ContentServer::ItemMeta>::const_iterator
ContentServer::makeItemMetaUsingHTTPRequest2(const QUrl &url, ItemMeta &meta,
                                             std::shared_ptr<QNetworkAccessManager> nam,
                                             int counter)
{
    qDebug() << ">> makeItemMetaUsingHTTPRequest";

    if (QThread::currentThread()->isInterruptionRequested()) {
        qWarning() << "Thread interruption was requested";
        return metaCache.cend();
    }

    if (counter >= maxRedirections) {
        qWarning() << "Max redirections reached";
        return metaCache.cend();
    }

    qDebug() << "Sending HTTP request for url:" << url;
    QNetworkRequest request;
    request.setUrl(url);
    request.setRawHeader("Icy-MetaData", "1"); // needed for SHOUTcast streams discovery
    request.setRawHeader("User-Agent", userAgent);
    request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);

    if (!nam)
        nam = std::make_shared<QNetworkAccessManager>();

    auto reply = nam->get(request);

    bool art = meta.flagSet(MetaFlag_Art);
    QEventLoop loop;
    connect(reply, &QNetworkReply::metaDataChanged, [reply, art]{
        qDebug() << ">> metaDataChanged";
        qDebug() << "Received meta data of HTTP reply for url:" << reply->url();

        // Bug in Qt? "Content-Disposition" cannot be retrived with QNetworkRequest::ContentDispositionHeader
        //auto disposition = reply->header(QNetworkRequest::ContentDispositionHeader).toString().toLower();
        auto disposition = QString(reply->rawHeader("Content-Disposition")).toLower();

        auto mime = mimeFromDisposition(disposition);
        if (mime.isEmpty())
            mime = reply->header(QNetworkRequest::ContentTypeHeader).toString().toLower();
        auto type = typeFromMime(mime);

        if (type == TypePlaylist) {
            qDebug() << "Content is a playlist";
            // Content is needed, so not aborting
        } else if (type == TypeImage && art) {
            qDebug() << "Content is album art";
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
        disconnect(reply, nullptr, &loop, nullptr);
        return metaCache.cend();
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
        return metaCache.cend();
    }

    if (code > 299 && code < 399) {
        qWarning() << "Redirection received:" << reply->error() << code << reason;
        auto newUrl = reply->header(QNetworkRequest::LocationHeader).toUrl();
        if (newUrl.isRelative()) {
            newUrl = url.resolved(newUrl);
            Utils::fixUrl(newUrl);
        }
        reply->deleteLater();
        if (newUrl.isValid())
            return makeItemMetaUsingHTTPRequest2(newUrl, meta, nam, counter + 1);
        else
            return metaCache.cend();
    }

    bool ytdl_broken = false;

    if (code > 299) {
        if (!meta.flagSet(MetaFlag_Refresh) && meta.flagSet(MetaFlag_YtDl)) {
            qDebug() << "ytdl broken URL";
            ytdl_broken = true;
            meta.metaUpdateTime = {}; // null time to set meta as dummy
        } else if (counter == 0 && meta.flagSet(MetaFlag_OrigUrl)) {
            qWarning() << "URL is invalid but origURL is provided => checking origURL instead";
            reply->deleteLater();
            return makeItemMetaUsingHTTPRequest2(meta.origUrl, meta, nam, counter + 1);
        } else {
            qWarning() << "Unsupported response code:" << reply->error() << code << reason;
            reply->deleteLater();
            return metaCache.cend();
        }
    }

    // Bug in Qt? "Content-Disposition" cannot be retrived with QNetworkRequest::ContentDispositionHeader
    //auto disposition = reply->header(QNetworkRequest::ContentDispositionHeader).toString().toLower();
    auto disposition = QString(reply->rawHeader("Content-Disposition")).toLower();
    auto mime = mimeFromDisposition(disposition);
    if (mime.isEmpty())
        mime = reply->header(QNetworkRequest::ContentTypeHeader).toString().toLower();
    auto type = typeFromMime(mime);

    if (!meta.flagSet(MetaFlag_YtDl) && type == TypePlaylist) {
        qDebug() << "Content is a playlist";

        auto size = reply->bytesAvailable();
        if (size > 0) {
            auto ptype = playlistTypeFromMime(mime);
            auto data = reply->readAll();

            if (hlsPlaylist(data)) {
                qDebug() <<  "HLS playlist";
                meta.url = url;
                meta.mime = mime;
                meta.type = TypePlaylist;
                meta.filename = url.fileName();
                meta.itemType = ItemType_Url;
                meta.setFlags(MetaFlag_Valid|MetaFlag_PlaylistProxy);
                meta.setFlags(MetaFlag_Local|MetaFlag_Seek, false);
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
                    Utils::fixUrl(url);
                    qDebug() << "Trying get meta data for first item in the playlist:" << url;
                    reply->deleteLater();
                    return makeItemMetaUsingHTTPRequest2(url, meta, nam, counter + 1);
                }
            }
        }

        qWarning() << "Playlist content is empty";
        reply->deleteLater();
        return metaCache.cend();
    }

    if (!ytdl_broken && type != TypeMusic && type != TypeVideo && type != TypeImage) {
        if (mime.contains("text/html")) {
            reply->deleteLater();
            const auto url = reply->url();
            if (meta.app == "bc" || BcApi::validUrl(url)) {
                return makeItemMetaUsingBcApi(url, meta, nam, counter);
            } else if (meta.app == "soundcloud" || SoundcloudApi::validUrl(url)) {
                return makeItemMetaUsingSoundcloudApi(url, meta, nam, counter);
            } else {
                return makeItemMetaUsingYtdlApi(url, meta, nam, counter);
            }
        }

        qWarning() << "Unsupported type:" << mime;
        reply->deleteLater();
        return metaCache.cend();
    }

    bool ranges = QString{reply->rawHeader("Accept-Ranges")}.toLower().contains("bytes");
    int size = reply->header(QNetworkRequest::ContentLengthHeader).toInt();

    meta.url = url;
    meta.mime = ytdl_broken ? QString{} : mime;
    meta.type = type;
    meta.size = ranges ? size : 0;
    meta.filename = url.fileName();
    meta.itemType = ItemType_Url;
    meta.setFlags(MetaFlag_Valid);
    meta.setFlags(MetaFlag_Local, false);
    meta.setFlags(MetaFlag_Seek, size > 0 && ranges);

    if (type == TypeImage && meta.flagSet(MetaFlag_Art)) {
        qDebug() << "Saving album art to file";
        auto ext = m_imgMimeToExtMap.value(meta.mime);
        auto data = reply->readAll();
        if (!ext.isEmpty() && !data.isEmpty()) {
            auto albumArt = QString{"art_image_%1.%2"}
                    .arg(Utils::instance()->hash(meta.url.toString()), ext);
            Utils::writeToCacheFile(albumArt, data, true);
            meta.path = QString("%1/%2").arg(Settings::instance()->getCacheDir(), albumArt);
        } else {
            qWarning() << "Cannot save album art for:" << meta.url.toString();
        }
    } else {
        updateMetaAlbumArt(meta);

        if (reply->hasRawHeader("icy-metaint")) {
            int metaint = reply->rawHeader("icy-metaint").toInt();
            if (metaint > 0) {
                qDebug() << "Shoutcast stream detected";
                meta.setFlags(MetaFlag_IceCast);
            }
        }

        if (meta.title.isEmpty()) {
            if (reply->hasRawHeader("icy-name"))
                meta.title = QString{reply->rawHeader("icy-name")};
            else
                meta.title = url.fileName();
        }

        if (reply->hasRawHeader("x-amz-meta-bitrate")) {
            meta.bitrate = reply->rawHeader("x-amz-meta-bitrate").toInt() * 1000;
        }
        if (meta.duration == 0 && reply->hasRawHeader("x-amz-meta-duration")) {
            meta.duration = reply->rawHeader("x-amz-meta-duration").toInt() / 1000;
        }
    }

    reply->deleteLater();
    return metaCache.insert(url, meta);
}

void ContentServer::updateMetaAlbumArt(ItemMeta &meta) const
{
    if (meta.albumArt.isEmpty()) {
        if (meta.app == "bc")
            meta.albumArt = IconProvider::pathToNoResId("icon-bandcamp");
        else if (meta.app == "somafm")
            meta.albumArt = IconProvider::pathToNoResId("icon-somafm");
        else if (meta.app == "icecast")
            meta.albumArt = IconProvider::pathToNoResId("icon-icecast");
        else if (meta.app == "fosdem")
            meta.albumArt = IconProvider::pathToNoResId("icon-fosdem");
        else if (meta.app == "gpodder")
            meta.albumArt = IconProvider::pathToNoResId("icon-gpodder");
        else if (meta.app == "tunein")
            meta.albumArt = IconProvider::pathToNoResId("icon-tunein");
        else if (BcApi::validUrl(meta.origUrl) || BcApi::validUrl(meta.url))
            meta.albumArt = IconProvider::pathToNoResId("icon-bandcamp");
        else if (meta.origUrl.host().contains("youtube.") || meta.origUrl.host().contains("yout.be"))
            meta.albumArt = IconProvider::pathToNoResId("icon-youtube");
        else if (meta.origUrl.host().contains("soundcloud.com"))
            meta.albumArt = IconProvider::pathToNoResId("icon-soundcloud");
        else if (meta.origUrl.host().contains("somafm.com") || meta.url.host().contains("somafm.com"))
            meta.albumArt = IconProvider::pathToNoResId("icon-somafm");
    }
}

const QHash<QUrl, ContentServer::ItemMeta>::const_iterator
ContentServer::makeMetaUsingExtension(const QUrl &url)
{
    bool isFile = url.isLocalFile();
    bool isQrc = url.scheme() == "qrc";

    ItemMeta meta;
    meta.path = isFile ? url.toLocalFile() :
                  isQrc ? (":" + Utils::urlFromId(url).path()) : "";
    meta.url = url;
    meta.mime = getContentMimeByExtension(url);
    meta.type = getContentTypeByExtension(url);
    meta.size = 0;
    meta.filename = url.fileName();
    meta.itemType = itemTypeFromUrl(url);
    meta.setFlags(MetaFlag_Valid);
    meta.setFlags(MetaFlag_Local|MetaFlag_Seek, isFile || isQrc);

    return metaCache.insert(url, meta);
}

const QHash<QUrl, ContentServer::ItemMeta>::const_iterator
ContentServer::makeItemMeta(const QUrl &url, const QUrl &origUrl,
                            const QString &app, bool ytdl,
                            bool art, bool refresh)
{
    metaCacheMutex.lock();

    QHash<QUrl, ContentServer::ItemMeta>::const_iterator it;
    auto itemType = itemTypeFromUrl(url);
    if (itemType == ItemType_LocalFile) {
        if (art) {
            it = makeMetaUsingExtension(url);
        } else {
#ifdef SAILFISH
            it = makeItemMetaUsingTracker(url);
            if (it == metaCache.cend()) {
                qWarning() << "Cannot get meta using Tacker, so fallbacking to Taglib";
                it = makeItemMetaUsingTaglib(url);
            }
#else
            it = makeItemMetaUsingTaglib(url);
#endif
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
            it = metaCache.cend();
        }
    } else if (itemType == ItemType_Upnp) {
        qDebug() << "Upnp URL detected";
        it = makeUpnpItemMeta(url);
    } else if (itemType == ItemType_Url){
        qDebug() << "Geting meta using HTTP request";
        it = makeItemMetaUsingHTTPRequest(url, origUrl, app, ytdl, refresh, art);
    } else if (url.scheme() == "jupii") {
        qDebug() << "Unsupported Jupii URL detected";
        it = metaCache.cend();
    } else {
        qDebug() << "Unsupported URL type";
        it = metaCache.cend();
    }

    metaCacheMutex.unlock();
    return it;
}

void ContentServer::run()
{
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
    return worker->streamToRecord(id);
}

bool ContentServer::isStreamRecordable(const QUrl &id)
{
    if (tmpRecs.contains(id)) {
        return true;
    } else {
        auto worker = ContentServerWorker::instance();
        return worker->streamRecordable(id);
    }
}

bool ContentServer::saveTmpRec(const QString &path)
{
    const auto title = readTitleUsingTaglib(path);
    const auto ext = path.split('.').last();

    if (!title.isEmpty() && !ext.isEmpty()) {
        auto recFilePath = QDir{Settings::instance()->getRecDir()}.filePath(
                    QString{"%1.%2.%3.%4"}.arg(Utils::escapeName(title), Utils::instance()->randString(),
                                               "jupii_rec", ext));
        qDebug() << "Saving rec file:" << path << recFilePath;
        if (QFile::copy(path, recFilePath)) {
            QFile::remove(path);
            emit streamRecorded(title, recFilePath);
            return true;
        } else {
            qWarning() << "Cannot copy:" << path << recFilePath;
        }
    }

    QFile::remove(path);
    emit streamRecordError(title);
    return false;
}

void ContentServer::cleanTmpRec()
{
    const auto id = QUrl(PlaylistModel::instance()->activeId());

    auto i = tmpRecs.begin();
    while (i != tmpRecs.end()) {
        if (i.key() != id) {
#ifdef QT_DEBUG
            qDebug() << "Deleting tmp rec file:" << i.value();
#endif
            QFile::remove(i.value());
            i = tmpRecs.erase(i);
        } else {
            ++i;
        }
    }
}

void ContentServer::setStreamToRecord(const QUrl &id, bool value)
{
    if (tmpRecs.contains(id)) {
        qDebug() << "Stream already recorded:" << id << value;
        if (value) {
            saveTmpRec(tmpRecs.take(id));
            emit streamRecordableChanged(id, false);
        } else {
            QFile::remove(tmpRecs.take(id));
        }
        emit streamRecordableChanged(id, false);
    } else {
        emit requestStreamToRecord(id, value);
    }
}

void ContentServer::streamToRecordChangedHandler(const QUrl &id, bool value)
{
    emit streamToRecordChanged(id, value);
}

void ContentServer::streamRecordableChangedHandler(const QUrl &id, bool value,
                                                   const QString& tmpFile)
{
    qDebug() << id << value << tmpFile;

    if (tmpFile.isEmpty()) {
        if (tmpRecs.contains(id))
            QFile::remove(tmpRecs.value(id));
        tmpRecs.remove(id);
        emit streamRecordableChanged(id, value);
    } else {
        if (tmpRecs.contains(id))
            QFile::remove(tmpRecs.value(id));
        tmpRecs.insert(id, tmpFile);
        emit streamRecordableChanged(id, true);
    }
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

    if (new_title.isEmpty()) {
        stream.titleHistory.clear();
    } else if (stream.titleHistory.isEmpty() ||
               stream.titleHistory.first() != new_title) {
            stream.titleHistory.push_front(new_title);
            if (stream.titleHistory.length() > 20) // max number of titiles in history list
                stream.titleHistory.removeLast();
    }

    emit streamTitleChanged(id, stream.title);
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
            if (volumeBoost > 1.0f) { // increasing volume level
                d = QByteArray(static_cast<const char*>(data), size); // deep copy
                adjustVolume(&d, volumeBoost, true);
            } else {
                d = QByteArray::fromRawData(static_cast<const char*>(data), size);
            }
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
