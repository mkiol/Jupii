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

#include <iomanip>

#include "contentserver.h"
#include "utils.h"
#include "settings.h"
#include "tracker.h"
#include "trackercursor.h"

using namespace std;

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
    {"avi", "video/msvideo"},
    {"mov", "video/quicktime"}, {"qt", "video/quicktime"},
    {"wmv", "video/x-ms-wmv"},
    {"mp4", "video/mp4"}, {"m4v", "video/mp4"},
    {"mpg", "video/mpeg"}, {"mpeg", "video/mpeg"}, {"m2v", "video/mpeg"}
};

ContentServer::ContentServer(QObject *parent) :
    QObject(parent),
    TaskExecutor(parent, 3)
{
    m_server = new QHttpServer;
    QObject::connect(m_server, &QHttpServer::newRequest,
                     this, &ContentServer::asyncRequestHandler);
    QObject::connect(this, &ContentServer::do_sendEmptyResponse,
                     this, &ContentServer::sendEmptyResponse);
    QObject::connect(this, &ContentServer::do_sendResponse,
                     this, &ContentServer::sendResponse);
    QObject::connect(this, &ContentServer::do_sendHead,
                     this, &ContentServer::sendHead);
    QObject::connect(this, &ContentServer::do_sendData,
                     this, &ContentServer::sendData);
    QObject::connect(this, &ContentServer::do_sendEnd,
                     this, &ContentServer::sendEnd);

    if (!m_server->listen(Settings::instance()->getPort())) {
        qWarning() << "Unable to start HTTP server!";
        //TODO: Handle: Unable to start HTTP server
    }
}

ContentServer* ContentServer::instance(QObject *parent)
{
    if (ContentServer::m_instance == nullptr) {
        ContentServer::m_instance = new ContentServer(parent);
    }

    return ContentServer::m_instance;
}

ContentServer::Type ContentServer::getContentType(const QString &path)
{
    const auto it = getMetaCacheIterator(path);
    if (it == m_metaCache.end()) {
        qWarning() << "No cache item found, so guessing based on file extension";

        auto ext = path.split(".").last();
        ext = ext.toLower();

        if (m_imgExtMap.contains(ext)) {
            return ContentServer::TypeImage;
        } else if (m_musicExtMap.contains(ext)) {
            return ContentServer::TypeMusic;
        } else if (m_videoExtMap.contains(ext)) {
            return ContentServer::TypeVideo;
        }

        // Default type
        return ContentServer::TypeUnknown;
    }

    const ItemMeta& item = it.value();
    return typeFromMime(item.mime);
}

ContentServer::Type ContentServer::typeFromMime(const QString &mime)
{
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

    for (auto& ext : exts) {
        ext.prepend("*.");
    }

    return exts;
}

QString ContentServer::getContentMime(const QString &path)
{
    const auto it = getMetaCacheIterator(path);
    if (it == m_metaCache.end()) {
        qWarning() << "No cache item found, so guessing based on file extension";

        auto ext = path.split(".").last();
        ext = ext.toLower();

        if (m_imgExtMap.contains(ext)) {
            return m_imgExtMap.value(ext);
        } else if (m_musicExtMap.contains(ext)) {
            return m_musicExtMap.value(ext);
        } else if (m_videoExtMap.contains(ext)) {
            return m_videoExtMap.value(ext);
        }

        // Default mime
        return "application/octet-stream";
    }

    const ItemMeta& item = it.value();
    return item.mime;
}

bool ContentServer::getContentMeta(const QString &path, const QUrl &url, QString &meta)
{
    const auto it = getMetaCacheIterator(path);
    if (it == m_metaCache.end()) {
        qWarning() << "No cache item found!";
        return false;
    }
    const ItemMeta& item = it.value();

    auto u = Utils::instance();
    QString hash = u->hash(path);
    QFileInfo file(path);
    QString hash_dir = u->hash(file.dir().path());
    qint64 size = file.size();

    QTextStream m(&meta);

    m << "<?xml version=\"1.0\" encoding=\"utf-8\"?>" << endl;
    m << "<DIDL-Lite xmlns=\"urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/\" ";
    m << "xmlns:dc=\"http://purl.org/dc/elements/1.1/\" ";
    m << "xmlns:upnp=\"urn:schemas-upnp-org:metadata-1-0/upnp/\" ";
    m << "xmlns:dlna=\"urn:schemas-dlna-org:metadata-1-0/\">";
    m << "<item id=\"" << hash << "\" parentID=\"" << hash_dir << "\" restricted=\"true\">";

    switch (item.type) {
    case TypeImage:
        m << "<upnp:albumArtURI>" << url.toString() << "</upnp:albumArtURI>";
        m << "<upnp:class>object.item.imageItem.photo</upnp:class>";
        break;
    case TypeMusic:
        m << "<upnp:class>object.item.audioItem.musicTrack</upnp:class>";
        if (!item.albumArt.isEmpty()) {
            QUrl artUrl;
            if (makeUrl(item.albumArt + "/jupii", artUrl))
                m << "<upnp:albumArtURI>" << artUrl.toString() << "</upnp:albumArtURI>";
            else
                qWarning() << "Can't make Url form art path";
            break;
        }
    case TypeVideo:
        m << "<upnp:class>object.item.videoItem.movie</upnp:class>";
        break;
    default:
        m << "<upnp:class>object.item</upnp:class>";
    }

    m << "<dc:title>" << (item.title.isEmpty() ? file.fileName().toHtmlEscaped() : item.title.toHtmlEscaped()) << "</dc:title>";

    if (item.type != TypeImage) {
        m << "<upnp:artist>" << (item.artist.isEmpty() ? "Unknown" : item.artist.toHtmlEscaped()) << "</upnp:artist>";
        m << "<upnp:album>" << (item.album.isEmpty() ? "Unknown" : item.album.toHtmlEscaped()) << "</upnp:album>";
    }

    if (!item.comment.isEmpty())
        m << "<upnp:longDescription>" << item.comment.toHtmlEscaped() << "</upnp:longDescription>";

    m << "<res size=\"" << QString::number(size) << "\" ";
    m << "protocolInfo=\"http-get:*:" << item.mime << ":*\" ";

    if (item.duration > 0) {
        int seconds = item.duration % 60;
        int minutes = (item.duration - seconds) / 60;
        int hours = (item.duration - (minutes * 60) - seconds) / 3600;
        QString duration = QString::number(hours) + ":" + (minutes < 10 ? "0" : "") +
                           QString::number(minutes) + ":" + (seconds < 10 ? "0" : "") +
                           QString::number(seconds) + ".000";
        m << "duration=\"" << duration << "\" ";
    }
    if (item.bitrate > 0)
        m << "bitrate=\"" << QString::number(item.bitrate) << "\" ";
    if (item.sampleRate > 0)
        m << "sampleFrequency=\"" << QString::number(item.sampleRate) << "\" ";
    if (item.channels > 0)
        m << "nrAudioChannels=\"" << QString::number(item.channels) << "\" ";

    m << ">" << url.toString() << "</res>";
    m << "</item>\n";
    m << "</DIDL-Lite>";

    qDebug() << "DIDL:" << meta;

    return true;
}

bool ContentServer::getContentUrl(const QString &id, QUrl &url, QString &meta,
                                  QString cUrl)
{
    const QString path = Utils::pathFromId(id);

    QFileInfo file(path);

    if (file.exists()) {

        if (!makeUrl(id, url)) {
            qWarning() << "Can't make Url form path";
            return false;
        }

        if (!cUrl.isEmpty() && cUrl == url.toString()) {
            // Optimization: Url is the same as current -> skipping getContentMeta
            return true;
        }

        //qDebug() << "Content URL:" << url.toString();

        if (!getContentMeta(path, url, meta)) {
            qWarning() << "Can't get content meta data";
            return false;
        }

        return true;
    }

    qWarning() << "File" << file.fileName() << "doesn't exist!";

    return false;
}

bool ContentServer::makeUrl(const QString& id, QUrl& url) const
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

QByteArray ContentServer::encrypt(const QByteArray &data) const
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

QByteArray ContentServer::decrypt(const QByteArray& data) const
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

void ContentServer::sendEmptyResponse(QHttpResponse *resp, int code)
{
    resp->writeHead(code);
    resp->setHeader("Content-Length", "0");
    resp->end();
}

void ContentServer::sendData(QHttpResponse *resp, QByteArray* data)
{
    resp->write(*data);
    delete data;
}

void ContentServer::sendHead(QHttpResponse *resp, int code)
{
    resp->writeHead(code);
}

void ContentServer::sendResponse(QHttpResponse *resp, int code, const QByteArray &data)
{
    resp->writeHead(code);
    resp->end(data);
}

void ContentServer::sendEnd(QHttpResponse *resp)
{
    resp->end();
}

QString ContentServer::pathFromUrl(const QUrl &url) const
{
    QString hash = url.path();
    hash = hash.right(hash.length()-1);

    QString id = QString::fromUtf8(decrypt(hash.toUtf8()));
    QString path = Utils::pathFromId(id);

    if (QFileInfo::exists(path)) {
        return path;
    } else {
        qWarning() << "Content path doesn't exist";
        return QString();
    }
}

QString ContentServer::idFromUrl(const QUrl &url) const
{
    QString hash = url.path();
    hash = hash.right(hash.length()-1);

    QString id = QString::fromUtf8(decrypt(hash.toUtf8()));
    QString path = Utils::pathFromId(id);

    if (QFileInfo::exists(path)) {
        return id;
    } else {
        qWarning() << "Content path doesn't exist";
        return QString();
    }
}

void ContentServer::asyncRequestHandler(QHttpRequest *req, QHttpResponse *resp)
{
    startTask([this, req, resp](){
        requestHandler(req, resp);
    });
}

void ContentServer::requestHandler(QHttpRequest *req, QHttpResponse *resp)
{

    qDebug() << ">>> requestHandler thread:" << QThread::currentThreadId();
    qDebug() << "  method:" << req->methodString();
    qDebug() << "  URL:" << req->url().path();
    qDebug() << "  headers:" << req->url().path();

    const auto& headers = req->headers();
    for (const auto& h : headers.keys()) {
        qDebug() << "    " << h << ":" << headers.value(h);
    }

    QString path = pathFromUrl(req->url());

    if (path.isEmpty()) {
        qWarning() << "Unknown content requested!";
        emit do_sendEmptyResponse(resp, 404);
        return;
    }

    QFile file(path);

    if (!file.exists()) {
        qWarning() << "Reqested file" << file.fileName() << "doesn't exist!";
        emit do_sendEmptyResponse(resp, 404);
        return;
    }

    if (!file.open(QFile::ReadOnly)) {
        qWarning() << "Unable to open file" << file.fileName() << "to read!";
        emit do_sendEmptyResponse(resp, 500);
        return;
    }

    qint64 length = file.bytesAvailable();
    QString ctype = getContentMime(file.fileName());
    qDebug() << "Content-Type of" << file.fileName() << "is" << ctype;

    bool isRange = headers.contains("range");
    bool isHead = req->method() == QHttpRequest::HTTP_HEAD;

    resp->setHeader("Content-Type", ctype);
    resp->setHeader("Accept-Ranges", "bytes");
    resp->setHeader("Connection", "close");

    if (isRange) {
        qDebug() << "Reqest contains Range header";
        QRegExp rx("bytes[\\s]*=[\\s]*([\\d]+)-([\\d]*)");
        if (rx.indexIn(headers.value("range")) >= 0) {
            //qDebug() << "cap0:" << rx.cap(0);
            //qDebug() << "cap1:" << rx.cap(1);
            //qDebug() << "cap2:" << rx.cap(2);

            qint64 startByte = rx.cap(1).toInt();
            qint64 endByte = (rx.cap(3) == "" ? length-1 : rx.cap(3).toInt());
            qint64 rangeLength = endByte-startByte+1;

            qDebug() << "start:" << startByte << "end:" << endByte << "length:" << rangeLength;

            if (endByte > length-1) {
                qWarning() << "Range end byte is higher than content lenght";
                emit do_sendEmptyResponse(resp, 416);
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
                emit do_sendResponse(resp, 206, "");
                file.close();
                return;
            }

            file.seek(startByte);
            emit do_sendHead(resp, 206);

            qDebug() << "rangeLength:" << rangeLength;

            if (!seqWriteData(file, rangeLength, resp)) {
                file.close();
                return;
            }

            emit do_sendEnd(resp);
            file.close();
            return;
        }

        qWarning() << "Unable to read Range header - regexp doesn't match.";
        emit do_sendEmptyResponse(resp, 416);
        file.close();
        return;
    }

    qDebug() << "Reqest doesn't contain Range header";

    resp->setHeader("Content-Length", QString::number(length));

    if (isHead) {
        qDebug() << "Sending 200 response without content";
        emit do_sendResponse(resp, 200, "");
        file.close();
        return;
    }

    qDebug() << "Sending 200 response";

    emit do_sendHead(resp, 200);

    if (!seqWriteData(file, length, resp)) {
        file.close();
        return;
    }

    emit do_sendEnd(resp);
    file.close();
}

bool ContentServer::seqWriteData(QFile& file, qint64 size, QHttpResponse *resp)
{
    // This function should not be called in the main thread

    qint64 rlen = size;

    qDebug() << "Start of writting" << rlen << "of data";

    do {
        if (resp->isFinished()) {
            qWarning() << "Connection has been closed by server";
            return false;
        }

        const qint64 len = rlen < qlen ? rlen : qlen;
        char cdata[len];

        rlen = rlen - len;
        const qint64 count = file.read(cdata, len);

        if (count > 0) {
            QByteArray* data = new QByteArray(cdata, count);
            emit do_sendData(resp, data);
            QThread::currentThread()->msleep(threadWait);
        } else {
            break;
        }
    } while (rlen > 0);

    qDebug() << "End of writting all data";

    return true;
}

const QHash<QString, ContentServer::ItemMeta>::const_iterator ContentServer::getMetaCacheIterator(const QString &path)
{
    const auto i = m_metaCache.find(path);
    if (i == m_metaCache.end()) {
        qDebug() << "Meta data for" << path << "not cached. Getting new from tracker";
        return makeItemMeta(path);
    }

    qDebug() << "Meta data for" << path << "found in cache";
    return i;
}

const QHash<QString, ContentServer::ItemMeta>::const_iterator ContentServer::makeItemMeta(const QString &path)
{
    const QString fileUrl = QUrl::fromLocalFile(path).toString(
                QUrl::EncodeUnicode|QUrl::EncodeSpaces);
    const QString query = queryTemplate.arg(fileUrl);

    auto tracker = Tracker::instance();
    if (!tracker->query(query)) {
        qWarning() << "Can't get tracker data for url:" << fileUrl;
        return m_metaCache.end();
    }

    auto res = tracker->getResult();
    TrackerCursor cursor(res.first, res.second);

    int n = cursor.columnCount();

    if (n == 10) {
        while(cursor.next()) {
            for (int i = 0; i < n; ++i) {
                auto name = cursor.name(i);
                auto type = cursor.type(i);
                auto value = cursor.value(i);
                qDebug() << "column:" << i;
                qDebug() << " name:" << name;
                qDebug() << " type:" << type;
                qDebug() << " value:" << value;
            }

            auto& meta = m_metaCache[path];
            meta.trackerId = cursor.value(0).toString();
            meta.url = fileUrl;
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
            meta.albumArt = tracker->genAlbumArtFile(meta.album, meta.artist);
            meta.type = typeFromMime(meta.mime);

            return m_metaCache.find(path);
        }
    }

    return m_metaCache.end();
}
