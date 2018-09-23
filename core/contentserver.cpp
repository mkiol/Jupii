/* Copyright (C) 2017 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <QDir>
#include <QFileInfo>
#include <QRegExp>
#include <QImage>
#include <QThread>
#include <QNetworkRequest>
#include <QEventLoop>
#include <QTextStream>
#include <QRegExp>
#include <QTimer>

#include <iomanip>
#include <memory>

#include "contentserver.h"
#include "utils.h"
#include "settings.h"
#include "tracker.h"
#include "trackercursor.h"
#include "info.h"

// TagLib
#include "fileref.h"
#include "tag.h"
#include "tpropertymap.h"
#include "mpegfile.h"
#include "id3v2frame.h"
#include "id3v2tag.h"
#include "attachedpictureframe.h"

// Libav
#ifdef FFMPEG
extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/dict.h>
#include <libavutil/mathematics.h>
}
#endif

ContentServer* ContentServer::m_instance = nullptr;

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
    {"wma", "audio/x-ms-wma"}
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
    {"mpg", "video/mpeg"}, {"mpeg", "video/mpeg"}, {"m2v", "video/mpeg"}
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

ContentServerWorker::ContentServerWorker(QObject *parent) :
    QObject(parent),
    server(new QHttpServer(parent)),
    nam(new QNetworkAccessManager(parent))
{
    QObject::connect(server, &QHttpServer::newRequest,
                     this, &ContentServerWorker::requestHandler);

    if (!server->listen(static_cast<quint16>(Settings::instance()->getPort()))) {
        qWarning() << "Unable to start HTTP server!";
        //TODO: Handle: Unable to start HTTP server
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

    bool valid, isFile, isArt;
    auto id = ContentServer::idUrlFromUrl(req->url(), &valid, &isFile, &isArt);

    if (!valid) {
        qWarning() << "Unknown content requested!";
        sendEmptyResponse(resp, 404);
        return;
    }

    auto cs = ContentServer::instance();

    /*ContentServer::ItemMeta meta;

    if (isArt) {
        // Album Cover Art
        meta = cs->makeItemMetaUsingExtension(id) ;
    } else {
        const auto it = cs->getMetaCacheIteratorForId(id);
        if (it == cs->metaCache.end()) {
            qWarning() << "No meta item found";
            sendEmptyResponse(resp, 404);
            return;
        }
        meta = it.value();
    }*/

    const ContentServer::ItemMeta *meta;

    if (isArt) {
        // Album Cover Art
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

    if (isFile) {
        requestForFileHandler(id, meta, req, resp);
    } else {
        requestForUrlHandler(id, meta, req, resp);
    }
}

void ContentServerWorker::requestForFileHandler(const QUrl &id,
                                                const ContentServer::ItemMeta* meta,
                                                QHttpRequest *req, QHttpResponse *resp)
{
    auto type = static_cast<ContentServer::Type>(Utils::typeFromId(id));

    if (meta->type == ContentServer::TypeVideo &&
        type == ContentServer::TypeMusic) {
#ifdef FFMPEG
        qDebug() << "Video content and type is audio => extracting audio stream";

        ContentServer::AvData data;
        if (!ContentServer::extractAudio(meta->path, data)) {
            qWarning() << "Unable to extract audio stream";
            sendEmptyResponse(resp, 404);
            return;
        }

        streamFile(data.path, data.mime, req, resp);
#else
        qWarning() << "Video content and type is audio => can't extract because ffmpeg is disabled";
#endif
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

    QNetworkRequest request;
    request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
    request.setUrl(url);

    // Add headers
    const auto& headers = req->headers();
    if (headers.contains("range"))
        request.setRawHeader("Range", headers.value("range").toLatin1());
    request.setRawHeader("Connection", "close");
    request.setRawHeader("User-Agent", ContentServer::userAgent);

    QNetworkReply *reply;
    bool isHead = req->method() == QHttpRequest::HTTP_HEAD;
    if (isHead) {
        qDebug() << "HEAD request for url:" << url;
        reply = nam->head(request);
    } else {
        qDebug() << "GET request for url:" << url;
        reply = nam->get(request);
    }

    ProxyItem &item = proxyItems[reply];
    item.req = req;
    item.resp = resp;
    item.reply = reply;
    item.id = id;
    item.seek = meta->seekSupported;

    connect(reply, &QNetworkReply::metaDataChanged,
            this, &ContentServerWorker::proxyMetaDataChanged);
    connect(reply, &QNetworkReply::redirected,
            this, &ContentServerWorker::proxyRedirected);
    connect(reply, &QNetworkReply::finished,
            this, &ContentServerWorker::proxyFinished);
    connect(reply, &QNetworkReply::readyRead,
            this, &ContentServerWorker::proxyReadyRead);

    qDebug() << "requestForUrlHandler done";
}

bool ContentServerWorker::seqWriteData(QFile& file, qint64 size, QHttpResponse *resp)
{
    qint64 rlen = size;

    qDebug() << "Start of writting" << rlen << "of data";

    do {
        if (resp->isFinished()) {
            qWarning() << "Connection closed by server";
            return false;
        }

        const qint64 len = rlen < ContentServer::qlen ? rlen : ContentServer::qlen;
        QByteArray data; data.resize(static_cast<int>(len));
        auto cdata = data.data();
        const int count = static_cast<int>(file.read(cdata, len));
        rlen = rlen - len;

        if (count > 0) {
            resp->write(data);
            //QThread::currentThread()->msleep(ContentServer::threadWait);
        } else {
            break;
        }
    } while (rlen > 0);

    qDebug() << "End of writting all data";

    return true;
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
    resp->writeHead(302);
    resp->end();
}

void ContentServerWorker::proxyMetaDataChanged()
{
    qDebug() << "Request meta data received";
    auto reply = dynamic_cast<QNetworkReply*>(sender());

    if (!proxyItems.contains(reply)) {
        qWarning() << "Proxy meta data: Can't find proxy item";
        return;
    }

    auto &item = proxyItems[reply];

    auto code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    auto reason = reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();
    auto mime = reply->header(QNetworkRequest::ContentTypeHeader).toString();
    auto error = reply->error();

    // -- debug --
    qDebug() << "Request:" << (item.req->method() == QHttpRequest::HTTP_GET ? "GET" : "HEAD")
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
        if (code < 400)
            code = 404;
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
        if (reply->header(QNetworkRequest::ContentLengthHeader).isValid())
            item.resp->setHeader("Content-Length",
                                 reply->header(QNetworkRequest::ContentLengthHeader).toString());
        if (reply->hasRawHeader("Accept-Ranges"))
            item.resp->setHeader("Accept-Ranges", reply->rawHeader("Accept-Ranges"));
        if (reply->hasRawHeader("Content-Range"))
            item.resp->setHeader("Content-Range", reply->rawHeader("Content-Range"));

        // copying icy-* headers
        const auto &headers = reply->rawHeaderPairs();
        for (const auto& h : headers) {
            if (h.first.toLower().startsWith("icy-"))
                item.resp->setHeader(h.first, h.second);
        }

        item.state = 1;

        qDebug() << "Sending head for request with code:" << code;
        item.resp->writeHead(code);
        return;
    }

    proxyItems.remove(reply);
    reply->abort();
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
        //qWarning() << "Can't find proxy item";
        reply->deleteLater();
        return;
    }

    auto &item = proxyItems[reply];

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
    } else {
        qDebug() << "Ending request";
        // TODO: Do not end if resp doesn't exists!
        item.resp->end();
    }

    proxyItems.remove(reply);
    reply->deleteLater();
}

void ContentServerWorker::proxyReadyRead()
{
    auto reply = dynamic_cast<QNetworkReply*>(sender());

    if (!proxyItems.contains(reply)) {
        qWarning() << "Proxy ready read: Can't find proxy item";
        return;
    }

    if (reply->isFinished()) {
        qWarning() << "Proxy ready read: Reply is finished";
    }

    const auto &item = proxyItems.value(reply);

    if (item.resp->isFinished()) {
        qWarning() << "Server request already finished, so ending client side";
        reply->abort();
        reply->deleteLater();
        proxyItems.remove(reply);
        return;
    }

    if (item.state == 1) {
        auto len = reply->bytesAvailable();
        QByteArray data; data.resize(static_cast<int>(len));
        auto cdata = data.data();
        const int count = static_cast<int>(item.reply->read(cdata, len));

        if (count > 0)
            item.resp->write(data);
    }
}

void ContentServerWorker::streamFile(const QString& path, const QString& mime,
                           QHttpRequest *req, QHttpResponse *resp)
{
    QFile file(path);

    if (!file.open(QFile::ReadOnly)) {
        qWarning() << "Unable to open file" << file.fileName() << "to read!";
        sendEmptyResponse(resp, 500);
        return;
    }

    const auto& headers = req->headers();
    qint64 length = file.bytesAvailable();
    bool isRange = headers.contains("range");
    bool isHead = req->method() == QHttpRequest::HTTP_HEAD;

    qDebug() << "Content file name:" << file.fileName();
    qDebug() << "Content size:" << length;
    qDebug() << "Content type:" << mime;
    qDebug() << "Content request contains Range header:" << isRange;
    qDebug() << "Content request is HEAD:" << isHead;

    resp->setHeader("Content-Type", mime);
    resp->setHeader("Accept-Ranges", "bytes");
    resp->setHeader("Connection", "close");
    resp->setHeader("transferMode.dlna.org", "Streaming");
    resp->setHeader("contentFeatures.dlna.org", ContentServer::dlnaContentFeaturesHeader(mime));

    if (isRange) {
        QRegExp rx("bytes[\\s]*=[\\s]*([\\d]+)-([\\d]*)");
        if (rx.indexIn(headers.value("range")) >= 0) {
            qint64 startByte = rx.cap(1).toInt();
            qint64 endByte = (rx.cap(3) == "" ? length-1 : rx.cap(3).toInt());
            qint64 rangeLength = endByte-startByte+1;

            qDebug() << "Range start:" << startByte;
            qDebug() << "Range end:" << endByte;
            qDebug() << "Range length:" << rangeLength;

            if (endByte > length-1) {
                qWarning() << "Range end byte is higher than content lenght";
                sendEmptyResponse(resp, 416);
                file.close();
                return;
            }

            resp->setHeader("Content-Length", QString::number(rangeLength));
            resp->setHeader("Content-Range", "bytes " +
                            QString::number(startByte) + "-" +
                            QString::number(endByte) + "/" +
                            QString::number(length-1));

            qDebug() << "Sending 206 response";
            if (isHead) {
                sendResponse(resp, 206, "");
                file.close();
                return;
            }
            resp->writeHead(206);

            // Sending data
            file.seek(startByte);
            if (!seqWriteData(file, rangeLength, resp)) {
                file.close();
                return;
            }

            resp->end();
            file.close();
            return;
        }

        qWarning() << "Unable to read Range header - regexp doesn't match.";
        sendEmptyResponse(resp, 416);
        file.close();
        return;
    }

    qDebug() << "Reqest doesn't contain Range header";

    resp->setHeader("Content-Length", QString::number(length));

    if (isHead) {
        qDebug() << "Sending 200 response without content";
        sendResponse(resp, 200, "");
        file.close();
        return;
    }

    qDebug() << "Sending 200 response";

    resp->writeHead(200);

    if (!seqWriteData(file, length, resp)) {
        file.close();
        return;
    }

    resp->end();
    file.close();
}

ContentServer::ContentServer(QObject *parent) :
    QThread(parent)
{
    qDebug() << "Creating Content Server in thread:" << QThread::currentThreadId();
#ifdef FFMPEG
    // Libav stuff
    av_log_set_level(AV_LOG_DEBUG);
    av_register_all();
    avcodec_register_all();
#endif

    // starting worker
    start(QThread::NormalPriority);
}

ContentServer* ContentServer::instance(QObject *parent)
{
    if (ContentServer::m_instance == nullptr) {
        ContentServer::m_instance = new ContentServer(parent);
    }

    return ContentServer::m_instance;
}

QString ContentServer::dlnaOrgFlagsForFile()
{
    char flags[448];
    sprintf(flags, "%s=%.8x%.24x", "DLNA.ORG_FLAGS",
            DLNA_ORG_FLAG_BYTE_BASED_SEEK |
            DLNA_ORG_FLAG_INTERACTIVE_TRANSFERT_MODE |
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
    if (mime == "video/x-msvideo")
        return "DLNA.ORG_PN=AVI";
    /*if (mime == "image/jpeg")
        return "DLNA.ORG_PN=JPEG_LRG";*/
    if (mime == "audio/aac" || mime == "audio/aacp")
        return "DLNA.ORG_PN=AAC";
    if (mime == "audio/mpeg")
        return "DLNA.ORG_PN=MP3";
    if (mime == "audio/vnd.wav")
        return "DLNA.ORG_PN=LPCM";
    if (mime == "video/x-matroska")
        return "DLNA.ORG_PN=MKV";
    return QString();
}

QString ContentServer::dlnaContentFeaturesHeader(const QString& mime, bool seek)
{
    QString pnFlags = dlnaOrgPnFlags(mime);
    if (pnFlags.isEmpty())
        return QString("%1;%2;%3").arg(
                    seek ? dlnaOrgOpFlagsSeekBytes : dlnaOrgOpFlagsNoSeek,
                    dlnaOrgCiFlags,
                    seek ? dlnaOrgFlagsForFile() : dlnaOrgFlagsForStreaming());
    else
        return QString("%1;%2;%3;%4").arg(
                    pnFlags, seek ? dlnaOrgOpFlagsSeekBytes : dlnaOrgOpFlagsNoSeek,
                    dlnaOrgCiFlags,
                    seek ? dlnaOrgFlagsForFile() : dlnaOrgFlagsForStreaming());
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

ContentServer::Type ContentServer::typeFromMime(const QString &mime)
{
    // check for playlist first
    if (m_m3u_mimes.contains(mime) ||
        m_pls_mimes.contains(mime))
        return ContentServer::TypePlaylist;
    /*if (m_playlistExtMap.values().contains(mime))
        return ContentServer::TypePlaylist;*/

    // hack for application/ogg
    if (mime.contains("/ogg"))
        return ContentServer::TypeMusic;
    if (mime.contains("/ogv"))
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
        qWarning() << "Can't open file" << item.path << "with TagLib";
    }

    item.albumArt.clear();
}

bool ContentServer::getContentMeta(const QString &id, const QUrl &url, QString &meta)
{
    QString path, name; int t;
    bool valid = Utils::pathTypeNameCookieFromId(id, &path, &t, &name);
    if (!valid)
        return false;

    bool audioType = static_cast<Type>(t) == TypeMusic; // extract audio stream from video
    QUrl urlFromId = Utils::urlFromId(id);

    const auto item = getMeta(urlFromId);
    if (!item) {
        qWarning() << "No meta item found";
        return false;
    }

    AvData data;
    if (audioType && item->local) {
#ifdef FFMPEG
        if (!extractAudio(path, data)) {
            qWarning() << "Can't extract audio stream";
            return false;
        }
        qDebug() << "Audio stream extracted to" << data.path;
#else
        qWarning() << "Audio stream can't be extracted because ffmpeg is disabled";
        return false;
#endif
    }

    auto u = Utils::instance();
    QString hash = u->hash(id);
    QString hash_dir = u->hash(id+"/parent");
    QTextStream m(&meta);

    m << "<?xml version=\"1.0\" encoding=\"utf-8\"?>" << endl;
    m << "<DIDL-Lite xmlns=\"urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/\" ";
    m << "xmlns:dc=\"http://purl.org/dc/elements/1.1/\" ";
    m << "xmlns:upnp=\"urn:schemas-upnp-org:metadata-1-0/upnp/\" ";
    m << "xmlns:dlna=\"urn:schemas-dlna-org:metadata-1-0/\">";
    m << "<item id=\"" << hash << "\" parentID=\"" << hash_dir << "\" restricted=\"true\">";

    switch (item->type) {
    case TypeImage:
        m << "<upnp:albumArtURI>" << url.toString() << "</upnp:albumArtURI>";
        m << "<upnp:class>" << imageItemClass << "</upnp:class>";
        break;
    case TypeMusic:
        m << "<upnp:class>" << audioItemClass << "</upnp:class>";
        if (!item->albumArt.isEmpty()) {
            auto id = Utils::idFromUrl(QUrl::fromLocalFile(item->albumArt), artCookie);
            QUrl artUrl;
            if (makeUrl(id, artUrl))
                m << "<upnp:albumArtURI>" << artUrl.toString() << "</upnp:albumArtURI>";
            else
                qWarning() << "Can't make Url form art path";
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
        if (!item->title.isEmpty())
            m << "<dc:title>" << item->title.toHtmlEscaped() << "</dc:title>";
        if (!item->artist.isEmpty())
            m << "<upnp:artist>" << item->artist.toHtmlEscaped() << "</upnp:artist>";
        if (!item->album.isEmpty())
            m << "<upnp:album>" << item->album.toHtmlEscaped() << "</upnp:album>";
        if (!item->comment.isEmpty())
            m << "<upnp:longDescription>" << item->comment.toHtmlEscaped() << "</upnp:longDescription>";
    } else {
        m << "<dc:title>" << name.toHtmlEscaped() << "</dc:title>";
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
          << (item->seekSupported ? dlnaOrgOpFlagsSeekBytes : dlnaOrgOpFlagsNoSeek)
          << "\" ";
    }

    if (item->duration > 0) {
        int seconds = item->duration % 60;
        int minutes = (item->duration - seconds) / 60;
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
            m << "bitrate=\"" << QString::number(item->bitrate) << "\" ";
        if (item->sampleRate > 0)
            m << "sampleFrequency=\"" << QString::number(item->sampleRate) << "\" ";
        if (item->channels > 0)
            m << "nrAudioChannels=\"" << QString::number(item->channels) << "\" ";
    }

    m << ">" << url.toString() << "</res>";
    m << "</item>\n";
    m << "</DIDL-Lite>";

    qDebug() << "DIDL:" << meta;

    return true;
}

bool ContentServer::getContentUrl(const QString &id, QUrl &url, QString &meta,
                                  QString cUrl)
{
    if (!Utils::isIdValid(id)) {
        qWarning() << "Id is invalid!";
        return false;
    }

    if (!makeUrl(id, url)) {
        qWarning() << "Can't make Url form id";
        return false;
    }

    if (!cUrl.isEmpty() && cUrl == url.toString()) {
        // Optimization: Url is the same as current -> skipping getContentMeta
        return true;
    }

    if (!getContentMeta(id, url, meta)) {
        qWarning() << "Can't get content meta data";
        return false;
    }

    return true;
}

bool ContentServer::makeUrl(const QString& id, QUrl& url)
{
    QString hash = QString::fromUtf8(encrypt(id.toUtf8()));

    QString ifname, addr;
    if (!Utils::instance()->getNetworkIf(ifname, addr)) {
        qWarning() << "Can't find valid network interface";
        return false;
    }

    url.setScheme("http");
    url.setHost(addr);
    url.setPort(Settings::instance()->getPort());
    url.setPath("/" + hash);

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

    qWarning() << "Can't get path from URL";
    return QString();
}

QUrl ContentServer::idUrlFromUrl(const QUrl &url, bool* ok, bool* isFile, bool* isArt)
{
    QString hash = url.path();
    hash = hash.right(hash.length()-1);

    auto id = QUrl(QString::fromUtf8(decrypt(hash.toUtf8())));

    if (!id.isValid()) {
        qWarning() << "Id is invalid";
        if (ok)
            *ok = false;
        return QUrl();
    }

    QUrlQuery q(id);
    if (!q.hasQueryItem(Utils::cookieKey) ||
            q.queryItemValue(Utils::cookieKey).isEmpty()) {
        qWarning() << "Id has no cookie";
        if (ok)
            *ok = false;
        return QUrl();
    } else {
        if (isArt)
            *isArt = q.queryItemValue(Utils::cookieKey) == artCookie;
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
        return id.toString();
    }

    qDebug() << "Id is an valid URL";

    if (ok)
        *ok = true;
    if (isFile)
        *isFile = false;
    return id;
}

QString ContentServer::idFromUrl(const QUrl &url) const
{
    bool valid, isFile;
    auto id = idUrlFromUrl(url, &valid, &isFile);

    if (valid)
        return id.toString();

    qWarning() << "Can't get id from URL";
    return QString();
}

#ifdef FFMPEG
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

    AVFormatContext *ic = NULL;
    if (avformat_open_input(&ic, file, NULL, NULL) < 0) {
        qWarning() << "avformat_open_input error";
        return false;
    }

    if ((avformat_find_stream_info(ic, NULL)) < 0) {
        qWarning() << "Could not find stream info";
        avformat_close_input(&ic);
        return false;
    }

    qDebug() << "nb_streams:" << ic->nb_streams;

    int aidx = av_find_best_stream(ic, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
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
    AVOutputFormat *of = NULL;
    auto t = data.type.toLatin1();
    of = av_guess_format(t.data(), NULL, NULL);
    if (!of) {
        qWarning() << "av_guess_format error";
        avformat_close_input(&ic);
        return false;
    }

    qDebug() << "avformat_alloc_context";
    AVFormatContext *oc = NULL;
    oc = avformat_alloc_context();
    if (!oc) {
        qWarning() << "avformat_alloc_context error";
        avformat_close_input(&ic);
        return false;
    }

    if (ic->metadata) {
        // Debug: metadata
        AVDictionaryEntry *tag = NULL;
        while ((tag = av_dict_get(ic->metadata, "", tag, AV_DICT_IGNORE_SUFFIX)))
            qDebug() << tag->key << "=" << tag->value;

        if (!av_dict_copy(&oc->metadata, ic->metadata, 0) < 0) {
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
    AVStream* ast = NULL;
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
        AVDictionaryEntry *tag = NULL;
        while ((tag = av_dict_get(ic->streams[aidx]->metadata, "", tag, AV_DICT_IGNORE_SUFFIX)))
            qDebug() << tag->key << "=" << tag->value;

        if (!av_dict_copy(&ast->metadata, ic->streams[aidx]->metadata, 0) < 0) {
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
    if (avformat_write_header(oc, NULL) < 0) {
        qWarning() << "avformat_write_header error";
        avformat_close_input(&ic);
        avio_close(oc->pb);
        avformat_free_context(oc);
        audioFile.remove();
        return false;
    }

    AVPacket pkt = { 0 };
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
#endif

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

    qDebug() << "Meta data for" << url << "found in cache";
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
        qWarning() << "Can't get tracker data for url:" << fileUrl;
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

            // defauls
            /*if (meta.title.isEmpty())
                meta.title = file.fileName();
            if (meta.artist.isEmpty())
                meta.artist = tr("Unknown");
            if (meta.album.isEmpty())
                meta.album = tr("Unknown");*/

            return metaCache.find(url);
        }
    }

    return metaCache.end();
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
    meta.seekSupported = true;

    TagLib::FileRef f(path.toUtf8().constData());
    if(f.isNull()) {
        qWarning() << "Can't extract meta data with TagLib";
    } else {
        if(f.tag()) {
            TagLib::Tag *tag = f.tag();
            meta.title = QString::fromWCharArray(tag->title().toCWString());
            meta.artist = QString::fromWCharArray(tag->artist().toCWString());
            meta.album = QString::fromWCharArray(tag->album().toCWString());
        }

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

    // defauls
    /*if (meta.title.isEmpty())
        meta.title = file.fileName();
    if (meta.artist.isEmpty())
        meta.artist = tr("Unknown");
    if (meta.album.isEmpty())
        meta.album = tr("Unknown");*/

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
    request.setRawHeader("User-Agent", userAgent);
    request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);

    if (!nam)
        nam = std::shared_ptr<QNetworkAccessManager>(new QNetworkAccessManager());

    auto reply = nam->get(request);

    QEventLoop loop;
    connect(reply, &QNetworkReply::metaDataChanged, [reply]{
        qDebug() << ">> metaDataChanged in thread:" << QThread::currentThreadId();
        qDebug() << "Received meta data of HTTP reply for url:" << reply->url();

        // Bug in Qt? "Content-Disposition" can't be retrived with QNetworkRequest::ContentDispositionHeader
        //auto disposition = reply->header(QNetworkRequest::ContentDispositionHeader).toString().toLower();
        auto disposition = QString(reply->rawHeader("Content-Disposition")).toLower();
        auto mime = mimeFromDisposition(disposition);
        if (mime.isEmpty())
            mime = reply->header(QNetworkRequest::ContentTypeHeader).toString().toLower();

        if (m_pls_mimes.contains(mime)) {
            //TODO: Support M3U
            qWarning() << "Content is a playlist";
            // Content is needed, so not aborting
            return;
        }

        // Content is no needed, so aborting
        if (!reply->isFinished())
            reply->abort();
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

    if (code > 299) {
        qWarning() << "Unsupported response code:" << reply->error() << code << reason;
        reply->deleteLater();
        return metaCache.end();
    }

    // Bug in Qt? "Content-Disposition" can't be retrived with QNetworkRequest::ContentDispositionHeader
    //auto disposition = reply->header(QNetworkRequest::ContentDispositionHeader).toString().toLower();
    auto disposition = QString(reply->rawHeader("Content-Disposition")).toLower();
    auto mime = mimeFromDisposition(disposition);
    if (mime.isEmpty())
        mime = reply->header(QNetworkRequest::ContentTypeHeader).toString().toLower();
    auto type = typeFromMime(mime);

    if (m_pls_mimes.contains(mime)) {
        //TODO: M3U playlist support
        qWarning() << "Content is a playlist";
        auto size = reply->bytesAvailable();
        if (size > 0) {
            auto items = parsePls(reply->readAll());
            if (!items.isEmpty()) {
                QUrl url = items.first().url;
                qDebug() << "Trying get meta data for first item in the playlist:" << url;
                reply->deleteLater();
                return makeItemMetaUsingHTTPRequest(url, nam, counter + 1);
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

/*const QHash<QUrl, ContentServer::ItemMeta>::const_iterator
ContentServer::makeItemMetaUsingExtension(const QUrl &url)
{
    return metaCache.insert(url, makeItemMetaUsingExtension2(url));
}*/

ContentServer::ItemMeta*
ContentServer::makeMetaUsingExtension(const QUrl &url)
{
    bool isFile = url.isLocalFile();
    auto item = new ContentServer::ItemMeta;
    item->valid = true;
    item->path = isFile ? url.toLocalFile() : "";
    item->url = url;
    item->mime = getContentMimeByExtension(url);
    item->type = getContentTypeByExtension(url);
    item->size = 0;
    item->filename = url.fileName();
    item->local = isFile;
    item->seekSupported = isFile;
    return item;
}

const QHash<QUrl, ContentServer::ItemMeta>::const_iterator
ContentServer::makeItemMeta(const QUrl &url)
{
    QHash<QUrl, ContentServer::ItemMeta>::const_iterator it;
    if (url.isLocalFile()) {
        it = makeItemMetaUsingTracker(url);
        if (it == metaCache.end()) {
            qWarning() << "Can't get meta using Tacker, so fallbacking to Taglib";
            it = makeItemMetaUsingTaglib(url);
        }
    } else {
        qDebug() << "Geting meta using HTTP request";
        it = makeItemMetaUsingHTTPRequest(url);
    }

    /*if (it == metaCache.end()) {
        qWarning() << "Fallbacking to extension";
        it = makeItemMetaUsingExtension(url);
    }*/

    return it;
}

/*QString ContentServer::makePlaylistForUrl(const QUrl &url)
{
    return QString("[playlist]\nnumberofentries=1\nFile1=%1\nTitle1=Test\nLength1=-1")
            .arg(url.toString());
}*/

void ContentServer::run()
{
    qDebug() << "Creating content server worker in thread:"
             << QThread::currentThreadId();
    worker = new ContentServerWorker();

    // TODO: Loop exit
    QThread::exec();
    qDebug() << "Content server worker event loop exit in thread:"
             << QThread::currentThreadId();
}

QList<ContentServer::PlaylistItemMeta>
ContentServer::parsePls(const QByteArray &data)
{
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
            QUrl url(cap2);
            if (Utils::isUrlValid(url)) {
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

QList<ContentServer::PlaylistItemMeta>
ContentServer::parseM3u(const QByteArray &data, bool* ok)
{
    Q_UNUSED(data)
    QList<ContentServer::PlaylistItemMeta> list;

    if (ok)
        *ok = false;

    return list;
}
