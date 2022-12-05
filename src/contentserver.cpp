/* Copyright (C) 2017-2022 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "contentserver.h"

#include <QBuffer>
#include <QDebug>
#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QImageReader>
#include <QMatrix>
#include <QNetworkCookie>
#include <QNetworkCookieJar>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegExp>
#include <QSize>
#include <QTimer>
#include <algorithm>
#include <numeric>
#include <optional>

#include "bcapi.h"
#include "connectivitydetector.h"
#include "contentdirectory.h"
#include "contentserverworker.h"
#include "log.h"
#include "miccaster.h"
#include "playlistmodel.h"
#include "playlistparser.h"
#include "services.h"
#include "settings.h"
#include "soundcloudapi.h"
#include "thumb.h"
#include "tracker.h"
#include "trackercursor.h"
#include "transcoder.h"
#include "utils.h"
#ifndef HARBOUR
#include "ytdlapi.h"
#endif

extern "C" {
#include "libavdevice/avdevice.h"
#include "libavformat/avformat.h"
#include "libavutil/dict.h"
#include "libavutil/log.h"
#include "libavutil/timestamp.h"
}

// TagLib
#include "attachedpictureframe.h"
#include "fileref.h"
#include "id3v2frame.h"
#include "id3v2tag.h"
#include "mp4file.h"
#include "mp4item.h"
#include "mp4tag.h"
#include "mpegfile.h"
#include "tag.h"
#include "tpropertymap.h"

#ifdef SAILFISH
#include <sailfishapp.h>
#endif

#if defined(SAILFISH) || defined(KIRIGAMI)
#include "iconprovider.h"
#endif

const QString ContentServer::queryTemplate = QStringLiteral(
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
    "WHERE { ?item nie:isStoredAs ?url . ?url nie:url \"%1\". }");

const QHash<QString, QString> ContentServer::m_imgExtMap{
    {"jpg", "image/jpeg"},
    {"jpeg", "image/jpeg"},
    {"png", "image/png"},
    {"gif", "image/gif"},
    {"webp", "image/webp"}};

const QHash<QString, QString> ContentServer::m_musicExtMap{
    {"mp3", "audio/mpeg"},       {"m4a", "audio/mp4"},
    {"m4b", "audio/mp4"},        {"aac", "audio/aac"},
    {"mpc", "audio/x-musepack"}, {"flac", "audio/flac"},
    {"wav", "audio/vnd.wav"},    {"ape", "audio/x-monkeys-audio"},
    {"ogg", "audio/ogg"},        {"oga", "audio/ogg"},
    {"wma", "audio/x-ms-wma"},   {"tsa", "audio/MP2T"},
    {"aiff", "audio/x-aiff"}};

const QHash<QString, QString> ContentServer::m_musicMimeToExtMap{
    {"audio/mpeg", "mp3"},
    {"audio/mp4", "m4a"},
    // Disabling AAC because TagLib doesn't support ADTS
    // https://github.com/taglib/taglib/issues/508
    //{"audio/aac", "aac"},
    //{"audio/aacp", "aac"},
    {"audio/ogg", "ogg"},
    {"audio/vorbis", "ogg"},
    {"application/ogg", "ogg"},
    {"audio/wav", "wav"},
    {"audio/vnd.wav", "wav"},
    {"audio/x-wav", "wav"},
    {"audio/x-aiff", "aiff"}};

const QHash<QString, QString> ContentServer::m_imgMimeToExtMap{
    {"image/jpeg", "jpg"},
    {"image/png", "png"},
    {"image/gif", "gif"},
    {"image/webp", "webp"}};

const QHash<QString, QString> ContentServer::m_videoMimeToExtMap{
    {"video/x-matroska", "mkv"}, {"video/webm", "webm"},
    {"video/x-flv", "flv"},      {"video/ogg", "ogv"},
    {"video/x-msvideo", "avi"},  {"video/quicktime", "mov"},
    {"video/quicktime", "qt"},   {"video/x-ms-wmv", "wmv"},
    {"video/mp4", "mp4"},        {"video/mpeg", "mpg"},
    {"video/MP2T", "ts"}};

const QHash<QString, QString> ContentServer::m_videoExtMap{
    {"mkv", "video/x-matroska"}, {"webm", "video/webm"},
    {"flv", "video/x-flv"},      {"ogv", "video/ogg"},
    {"avi", "video/x-msvideo"},  {"mov", "video/quicktime"},
    {"qt", "video/quicktime"},   {"wmv", "video/x-ms-wmv"},
    {"mp4", "video/mp4"},        {"m4v", "video/mp4"},
    {"mpg", "video/mpeg"},       {"mpeg", "video/mpeg"},
    {"m2v", "video/mpeg"},       {"ts", "video/MP2T"},
    {"tsv", "video/MP2T"}
};

const QHash<QString, QString> ContentServer::m_playlistExtMap{
    {"m3u", "audio/x-mpegurl"},
    {"pls", "audio/x-scpls"},
    {"xspf", "application/xspf+xml"}};

const QStringList ContentServer::m_m3u_mimes{
    "application/vnd.apple.mpegurl", "application/mpegurl",
    "application/x-mpegurl", "audio/mpegurl", "audio/x-mpegurl"};

const QStringList ContentServer::m_pls_mimes{"audio/x-scpls"};

const QStringList ContentServer::m_xspf_mimes{"application/xspf+xml"};

const QString ContentServer::genericAudioItemClass =
    QStringLiteral("object.item.audioItem");
const QString ContentServer::genericVideoItemClass =
    QStringLiteral("object.item.videoItem");
const QString ContentServer::genericImageItemClass =
    QStringLiteral("object.item.imageItem");
const QString ContentServer::audioItemClass =
    QStringLiteral("object.item.audioItem.musicTrack");
const QString ContentServer::videoItemClass =
    QStringLiteral("object.item.videoItem.movie");
const QString ContentServer::imageItemClass =
    QStringLiteral("object.item.imageItem.photo");
const QString ContentServer::playlistItemClass =
    QStringLiteral("object.item.playlistItem");
const QString ContentServer::broadcastItemClass =
    QStringLiteral("object.item.audioItem.audioBroadcast");
const QString ContentServer::defaultItemClass = QStringLiteral("object.item");

const QString ContentServer::artCookie = QStringLiteral("jupii_art");

const QByteArray ContentServer::userAgent = QByteArrayLiteral(
    "Mozilla/5.0 (X11; Linux x86_64; rv:88.0) Gecko/20100101 Firefox/88.0");

/* DLNA.ORG_OP flags:
 * 00 - no seeking allowed
 * 01 - seek by byte
 * 10 - seek by time
 * 11 - seek by both*/
const QString ContentServer::dlnaOrgOpFlagsSeekBytes =
    QStringLiteral("DLNA.ORG_OP=01");
const QString ContentServer::dlnaOrgOpFlagsNoSeek =
    QStringLiteral("DLNA.ORG_OP=00");
const QString ContentServer::dlnaOrgCiFlags = QStringLiteral("DLNA.ORG_CI=0");

// Rec
const QString ContentServer::recDateTagName = QStringLiteral("Recording Date");
const QString ContentServer::recUrlTagName = QStringLiteral("Recording URL");
const QString ContentServer::recUrlTagName2 = QStringLiteral("Station URL");
const QString ContentServer::recAlbumName =
    QStringLiteral("Recordings by Jupii");
const QString ContentServer::typeTagName = QStringLiteral("otype");
const char *const ContentServer::recMp4TagPrefix = "----:org.mkiol.jupii:";

ContentServer::ContentServer(QObject *parent) : QThread{parent} {
#ifdef QT_DEBUG
    av_log_set_level(AV_LOG_DEBUG);
#else
    av_log_set_level(AV_LOG_ERROR);
#endif
    if (Settings::instance()->getLogToFile()) av_log_set_callback(ffmpegLog);
    avdevice_register_all();
    // starting worker
    start(QThread::NormalPriority);
    /*
    #ifdef SAILFISH
        // screen on/off status
        auto bus = QDBusConnection::systemBus();
        bus.connect("com.nokia.mce", "/com/nokia/mce/signal",
                    "com.nokia.mce.signal", "display_status_ind",
                    this, SLOT(displayStatusChangeHandler(QString)));
    #endif
    */
}

void ContentServer::displayStatusChangeHandler(const QString &state) {
    qDebug() << "display status:" << state;
    emit displayStatusChanged(state == "on");
}

QString ContentServer::dlnaOrgFlagsForFile() {
    char flags[448];
    sprintf(flags, "%s=%.8x%.24x", "DLNA.ORG_FLAGS",
            DLNA_ORG_FLAG_BYTE_BASED_SEEK |
                DLNA_ORG_FLAG_STREAMING_TRANSFER_MODE |
                DLNA_ORG_FLAG_BACKGROUND_TRANSFER_MODE |
                DLNA_ORG_FLAG_CONNECTION_STALL | DLNA_ORG_FLAG_DLNA_V15,
            0);
    return flags;
}

QString ContentServer::dlnaOrgFlagsForStreaming() {
    char flags[448];
    sprintf(flags, "%s=%.8x%.24x", "DLNA.ORG_FLAGS",
            DLNA_ORG_FLAG_S0_INCREASE | DLNA_ORG_FLAG_SN_INCREASE |
                DLNA_ORG_FLAG_STREAMING_TRANSFER_MODE |
                DLNA_ORG_FLAG_CONNECTION_STALL | DLNA_ORG_FLAG_DLNA_V15,
            0);
    return flags;
}

QString ContentServer::dlnaOrgPnFlags(const QString &mime) {
    if (mime.contains(QStringLiteral("video/x-msvideo"), Qt::CaseInsensitive))
        return QStringLiteral("DLNA.ORG_PN=AVI");
    /*if (mime.contains(QStringLiteral("image/jpeg"))
        return QStringLiteral("DLNA.ORG_PN=JPEG_LRG");*/
    if (mime.contains(QStringLiteral("audio/aac"), Qt::CaseInsensitive) ||
        mime.contains(QStringLiteral("audio/aacp"), Qt::CaseInsensitive))
        return QStringLiteral("DLNA.ORG_PN=AAC");
    if (mime.contains(QStringLiteral("audio/mpeg"), Qt::CaseInsensitive))
        return QStringLiteral("DLNA.ORG_PN=MP3");
    if (mime.contains(QStringLiteral("audio/vnd.wav"), Qt::CaseInsensitive))
        return QStringLiteral("DLNA.ORG_PN=LPCM");
    if (mime.contains(QStringLiteral("audio/L16"), Qt::CaseInsensitive))
        return QStringLiteral("DLNA.ORG_PN=LPCM");
    if (mime.contains(QStringLiteral("video/x-matroska"), Qt::CaseInsensitive))
        return QStringLiteral("DLNA.ORG_PN=MKV");
    return {};
}

QString ContentServer::dlnaContentFeaturesHeader(const QString &mime, bool seek,
                                                 bool flags) {
    auto pnFlags = dlnaOrgPnFlags(mime);
    if (pnFlags.isEmpty()) {
        if (flags)
            return QStringLiteral("%1;%2;%3")
                .arg(seek ? dlnaOrgOpFlagsSeekBytes : dlnaOrgOpFlagsNoSeek,
                     dlnaOrgCiFlags,
                     seek ? dlnaOrgFlagsForFile() : dlnaOrgFlagsForStreaming());
        return QStringLiteral("%1;%2").arg(
            seek ? dlnaOrgOpFlagsSeekBytes : dlnaOrgOpFlagsNoSeek,
            dlnaOrgCiFlags);
    }
    if (flags)
        return QStringLiteral("%1;%2;%3;%4")
            .arg(pnFlags, seek ? dlnaOrgOpFlagsSeekBytes : dlnaOrgOpFlagsNoSeek,
                 dlnaOrgCiFlags,
                 seek ? dlnaOrgFlagsForFile() : dlnaOrgFlagsForStreaming());
    return QStringLiteral("%1;%2;%3")
        .arg(pnFlags, seek ? dlnaOrgOpFlagsSeekBytes : dlnaOrgOpFlagsNoSeek,
             dlnaOrgCiFlags);
}

inline static std::optional<QString> extFromPath(const QString &path) {
    auto l = path.split(QStringLiteral("."));
    if (l.isEmpty()) return std::nullopt;
    return std::make_optional(l.last().toLower());
}

ContentServer::Type ContentServer::getContentTypeByExtension(
    const QString &path) {
    auto ext = extFromPath(path);
    if (!ext) return Type::Type_Unknown;

    if (m_imgExtMap.contains(*ext)) return Type::Type_Image;
    if (m_musicExtMap.contains(*ext)) return Type::Type_Music;
    if (m_videoExtMap.contains(*ext)) return Type::Type_Video;
    if (m_playlistExtMap.contains(*ext)) return Type::Type_Playlist;
    return Type::Type_Unknown;
}

ContentServer::Type ContentServer::getContentTypeByExtension(const QUrl &url) {
    return getContentTypeByExtension(url.fileName());
}

ContentServer::PlaylistType ContentServer::playlistTypeFromMime(
    const QString &mime) {
    if (m_pls_mimes.contains(mime)) return PlaylistType::PLS;
    if (m_m3u_mimes.contains(mime)) return PlaylistType::M3U;
    if (m_xspf_mimes.contains(mime)) return PlaylistType::XSPF;
    return PlaylistType::Unknown;
}

ContentServer::PlaylistType ContentServer::playlistTypeFromExtension(
    const QString &path) {
    auto ext = extFromPath(path);
    if (!ext) return PlaylistType::Unknown;
    return playlistTypeFromMime(m_playlistExtMap.value(*ext));
}

ContentServer::Type ContentServer::typeFromMime(const QString &mime) {
    // check for playlist first
    if (m_pls_mimes.contains(mime) || m_xspf_mimes.contains(mime) ||
        m_m3u_mimes.contains(mime))
        return Type::Type_Playlist;

    // hack for application/ogg
    if (mime.contains(QStringLiteral("/ogg"), Qt::CaseInsensitive))
        return Type::Type_Music;
    if (mime.contains(QStringLiteral("/ogv"), Qt::CaseInsensitive))
        return Type::Type_Video;

    auto l = mime.split(QStringLiteral("/"));
    if (l.isEmpty()) return Type::Type_Unknown;

    auto name = l.first().toLower();

    if (name == QStringLiteral("audio")) return Type::Type_Music;
    if (name == QStringLiteral("video")) return Type::Type_Video;
    if (name == QStringLiteral("image")) return Type::Type_Image;

    return Type::Type_Unknown;
}

QString ContentServer::extFromMime(const QString &mime) {
    if (m_imgMimeToExtMap.contains(mime)) return m_imgMimeToExtMap.value(mime);
    if (m_musicMimeToExtMap.contains(mime))
        return m_musicMimeToExtMap.value(mime);
    if (m_videoMimeToExtMap.contains(mime))
        return m_videoMimeToExtMap.value(mime);
    return {};
}

ContentServer::Type ContentServer::typeFromUpnpClass(const QString &upnpClass) {
    if (upnpClass.startsWith(ContentServer::genericAudioItemClass,
                             Qt::CaseInsensitive))
        return Type::Type_Music;
    if (upnpClass.startsWith(ContentServer::genericVideoItemClass,
                             Qt::CaseInsensitive))
        return Type::Type_Video;
    if (upnpClass.startsWith(ContentServer::genericImageItemClass,
                             Qt::CaseInsensitive))
        return Type::Type_Image;

    return Type::Type_Unknown;
}

QStringList ContentServer::getExtensions(int type) const {
    QStringList exts;

    if (type & static_cast<int>(Type::Type_Image)) exts << m_imgExtMap.keys();
    if (type & static_cast<int>(Type::Type_Music)) exts << m_musicExtMap.keys();
    if (type & static_cast<int>(Type::Type_Video)) exts << m_videoExtMap.keys();
    if (type & static_cast<int>(Type::Type_Playlist))
        exts << m_playlistExtMap.keys();

    for (auto &ext : exts) {
        ext.prepend(QStringLiteral("*."));
    }

    return exts;
}

QStringList ContentServer::extensionsForType(Type type) {
    if (type == Type::Type_Image) return m_imgExtMap.keys();
    if (type == Type::Type_Music) return m_musicExtMap.keys();
    if (type == Type::Type_Video) return m_videoExtMap.keys();
    if (type == Type::Type_Playlist) return m_playlistExtMap.keys();
    return {};
}

QString ContentServer::getContentMimeByExtension(const QString &path) {
    auto ext = extFromPath(path);
    if (ext) {
        if (m_imgExtMap.contains(*ext)) return m_imgExtMap.value(*ext);
        if (m_musicExtMap.contains(*ext)) return m_musicExtMap.value(*ext);
        if (m_videoExtMap.contains(*ext)) return m_videoExtMap.value(*ext);
        if (m_playlistExtMap.contains(*ext))
            return m_playlistExtMap.value(*ext);
    }

    return QStringLiteral("application/octet-stream");
}

QString ContentServer::getContentMimeByExtension(const QUrl &url) {
    return getContentMimeByExtension(url.path());
}

QString ContentServer::getExtensionFromAudioContentType(const QString &mime) {
    return m_musicMimeToExtMap.value(mime);
}

QString ContentServer::extractItemFromDidl(const QString &didl) {
    static QRegExp rx(QStringLiteral("<DIDL-Lite[^>]*>(.*)</DIDL-Lite>"));
    if (rx.indexIn(didl) != -1) {
        return rx.cap(1);
    }
    return QString();
}

bool ContentServer::getContentMetaItem(const QString &id, QString &meta,
                                       bool includeDummy) {
    auto *item = getMetaForId(id, false);
    if (!item) {
        qWarning() << "no meta item found";
        return false;
    }

    if (!includeDummy && item->dummy()) {
        return false;
    }

    if (item->itemType == ItemType_Upnp) {
        qDebug() << "item is upnp and didl item for upnp is not supported";
        return false;
    }

    QUrl url;

    if (const auto relay = Settings::instance()->getRemoteContentMode();
        item->itemType == ItemType_Url &&
        (relay == Settings::RemoteContentMode::RemoteContentMode_None_All ||
         (relay == Settings::RemoteContentMode::
                       RemoteContentMode_Proxy_Shoutcast_None &&
          !item->flagSet(MetaFlag::Seek)))) {
        url = item->url;
        if (!makeUrl(id, url, false)) {
            qWarning() << "cannot make url from id";
            return false;
        }
    } else {
        if (!makeUrl(id, url)) {
            qWarning() << "cannot make url from id";
            return false;
        }
    }

    return getContentMetaItem(id, url, meta, item);
}

bool ContentServer::getContentMetaItemByDidlId(const QString &didlId,
                                               QString &meta) {
    if (m_metaIdx.contains(didlId)) {
        getContentMetaItem(m_metaIdx[didlId], meta, false);
        return true;
    }
    return false;
}

QString ContentServer::durationStringFromSec(int duration) {
    int seconds = duration % 60;
    int minutes = ((duration - seconds) / 60) % 60;
    int hours = (duration - (minutes * 60) - seconds) / 3600;
    return QString::number(hours) + ":" + (minutes < 10 ? "0" : "") +
           QString::number(minutes) + ":" + (seconds < 10 ? "0" : "") +
           QString::number(seconds) + ".000";
}

bool ContentServer::getContentMetaItem(const QString &id, const QUrl &url,
                                       QString &meta, ItemMeta *item) {
    QString path, name, desc, author, album;
    int t = 0;
    int duration = 0;
    QUrl icon;
    if (!Utils::pathTypeNameCookieIconFromId(
            QUrl{id}, &path, &t, &name, nullptr, &icon, &desc, &author, &album,
            nullptr, nullptr, nullptr, /*app=*/nullptr, &duration))
        return false;

    bool audioType = static_cast<Type>(t) ==
                     Type::Type_Music;  // extract audio stream from video

    if (audioType && item->flagSet(MetaFlag::Local)) {
        if (!item->audioAvMeta || !QFile::exists(item->audioAvMeta->path)) {
            item->audioAvMeta = Transcoder::extractAudio(item->path);
            if (!item->audioAvMeta) {
                qWarning() << "cannot extract audio stream";
                return false;
            }
        }
    }

    auto didlId = Utils::hash(id);
    m_metaIdx[didlId] = id;

    QTextStream m{&meta};
    m << "<item id=\"" << didlId << "\" parentID=\"0\" restricted=\"1\">";

    switch (item->type) {
        case Type::Type_Image:
            m << "<upnp:albumArtURI>" << url.toString()
              << "</upnp:albumArtURI>";
            m << "<upnp:class>" << imageItemClass << "</upnp:class>";
            break;
        case Type::Type_Music:
            m << "<upnp:class>" << audioItemClass << "</upnp:class>";
            if (!icon.isEmpty() || !item->albumArt.isEmpty()) {
                auto artUrl = QFileInfo::exists(item->albumArt)
                                  ? QUrl::fromLocalFile(item->albumArt)
                                  : QUrl{item->albumArt};
                if (makeUrl(Utils::idFromUrl(icon.isEmpty() ? artUrl : icon,
                                             artCookie),
                            artUrl))
                    m << "<upnp:albumArtURI>" << artUrl.toString()
                      << "</upnp:albumArtURI>";
                else
                    qWarning() << "Cannot make Url form art path";
            }
            break;
        case Type::Type_Video:
            if (audioType)
                m << "<upnp:class>" << audioItemClass << "</upnp:class>";
            else
                m << "<upnp:class>" << videoItemClass << "</upnp:class>";
            break;
        case Type::Type_Playlist:
            m << "<upnp:class>" << playlistItemClass << "</upnp:class>";
            break;
        default:
            m << "<upnp:class>" << defaultItemClass << "</upnp:class>";
    }

    if (!name.isEmpty())
        m << "<dc:title>" << name.toHtmlEscaped() << "</dc:title>";
    else if (item->title.isEmpty())
        m << "<dc:title>" << ContentServer::bestName(*item).toHtmlEscaped()
          << "</dc:title>";
    else
        m << "<dc:title>" << item->title.toHtmlEscaped() << "</dc:title>";
    if (!author.isEmpty())
        m << "<upnp:artist>" << author.toHtmlEscaped() << "</upnp:artist>";
    else if (!item->artist.isEmpty())
        m << "<upnp:artist>" << item->artist.toHtmlEscaped()
          << "</upnp:artist>";
    if (!album.isEmpty())
        m << "<upnp:album>" << album.toHtmlEscaped() << "</upnp:album>";
    else if (!item->album.isEmpty())
        m << "<upnp:album>" << item->album.toHtmlEscaped() << "</upnp:album>";
    if (!desc.isEmpty())
        m << "<upnp:longDescription>" << desc << "</upnp:longDescription>";
    else if (!item->comment.isEmpty())
        m << "<upnp:longDescription>" << item->comment.toHtmlEscaped()
          << "</upnp:longDescription>";

    m << "<res ";

    if (audioType && item->audioAvMeta) {  // extract audio from video
        // puting audio stream info instead video file
        if (item->audioAvMeta->size > 0)
            m << "size=\"" << QString::number(item->audioAvMeta->size) << "\" ";
        m << "protocolInfo=\"http-get:*:" << item->audioAvMeta->mime << ":"
          << dlnaContentFeaturesHeader(item->audioAvMeta->mime, true, true)
          << "\" ";
    } else {
        if (item->size > 0)
            m << "size=\"" << QString::number(item->size) << "\" ";
        m << "protocolInfo=\"http-get:*:" << item->mime << ":"
          << dlnaContentFeaturesHeader(item->mime,
                                       item->flagSet(MetaFlag::Seek), true)
          << "\" ";
    }

    if (duration == 0) {
        duration = item->duration;
    }
    if (duration > 0) {
        m << "duration=\"" << durationStringFromSec(duration) << "\" ";
    }

    if (audioType && item->audioAvMeta) {
        if (item->audioAvMeta->bitrate > 0)
            m << "bitrate=\"" << QString::number(item->audioAvMeta->bitrate)
              << "\" ";
        if (item->sampleRate > 0)
            m << "sampleFrequency=\"" << QString::number(item->sampleRate)
              << "\" ";
        if (item->audioAvMeta->channels > 0)
            m << "nrAudioChannels=\""
              << QString::number(item->audioAvMeta->channels) << "\" ";
    } else {
        if (item->bitrate > 0)
            m << "bitrate=\"" << QString::number(item->bitrate, 'f', 0)
              << "\" ";
        if (item->sampleRate > 0)
            m << "sampleFrequency=\""
              << QString::number(item->sampleRate, 'f', 0) << "\" ";
        if (item->channels > 0)
            m << "nrAudioChannels=\"" << QString::number(item->channels)
              << "\" ";
    }

    m << ">" << url.toString() << "</res>";
    m << "</item>\n";

    return true;
}

bool ContentServer::getContentMeta(const QString &id, const QUrl &url,
                                   QString &meta, ItemMeta *item) {
    QTextStream m{&meta};
    m << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
    m << "<DIDL-Lite xmlns=\"urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/\" ";
    m << "xmlns:dc=\"http://purl.org/dc/elements/1.1/\" ";
    m << "xmlns:upnp=\"urn:schemas-upnp-org:metadata-1-0/upnp/\" ";
    m << "xmlns:dlna=\"urn:schemas-dlna-org:metadata-1-0/\">";

    if (getContentMetaItem(id, url, meta, item)) {
        m << "</DIDL-Lite>";
        qDebug() << "DIDL:" << meta;
        return true;
    }

    qWarning() << "meta creation error";
    return false;
}

void ContentServer::registerExternalConnections() const {
    connect(PlaylistModel::instance(), &PlaylistModel::activeItemChanged, this,
            &ContentServer::cleanTmpRec, Qt::QueuedConnection);
    connect(
        PlaylistModel::instance(), &PlaylistModel::itemsLoaded,
        ContentServerWorker::instance(),
        [] { ContentServer::cleanCacheFiles(); }, Qt::QueuedConnection);
}

bool ContentServer::getContentUrl(const QString &id, QUrl &url, QString &meta,
                                  const QString &cUrl) {
    if (!Utils::isIdValid(id)) {
        return false;
    }

    auto *item = getMetaForId(id, false);
    if (!item) {
        qWarning() << "no meta item found";
        return false;
    }

    if (item->itemType == ItemType_Upnp) {
        qDebug() << "item is upnp and relaying is disabled";
        if (item->didl.isEmpty()) {
            qWarning() << "didl is empty";
            return false;
        }
        url = item->url;
        if (!makeUrl(id, url, false)) {  // do not relay for upnp
            qWarning() << "cannot make url from id";
            return false;
        }
        qDebug() << "url for upnp item:" << id << url.toString();

        // no need to generate new DIDL, copying DIDL received from Media Server
        meta = item->didl;
        meta.replace(QStringLiteral("%URL%"), url.toString());
        return true;
    }

    if (const auto relay = Settings::instance()->getRemoteContentMode();
        item->itemType == ItemType_Url &&
        (relay == Settings::RemoteContentMode::RemoteContentMode_None_All ||
         (relay == Settings::RemoteContentMode::
                       RemoteContentMode_Proxy_Shoutcast_None &&
          !item->flagSet(MetaFlag::Seek)))) {
        qDebug() << "item is url and relaying is disabled";
        url = item->url;
        if (!makeUrl(id, url, false)) {
            qWarning() << "cannot make url from id";
            return false;
        }
    } else if (!makeUrl(id, url)) {
        qWarning() << "cannot make url from id";
        return false;
    }

    if (!cUrl.isEmpty() && cUrl == url.toString()) {
        // Optimization: Url is the same as current -> skipping getContentMeta
        return true;
    }

    if (!getContentMeta(id, url, meta, item)) {
        qWarning() << "cannot get content meta data";
        return false;
    }

    return true;
}

QString ContentServer::bestName(const ContentServer::ItemMeta &meta) {
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

bool ContentServer::makeUrl(const QString &id, QUrl &url, bool relay) {
    QString ifname, addr;
    if (!ConnectivityDetector::instance()->selectNetworkIf(ifname, addr)) {
        qWarning() << "cannot find valid network interface";
        return false;
    }

    auto hash = QString::fromUtf8(encrypt(id.toUtf8()));

    const int safe_hash_size = 800;
    if (hash.size() > safe_hash_size) {
        qWarning()
            << "hash size exceeds safe limit, some clients might have problems";
        auto truncatedHash = hash.left(safe_hash_size);
        if (!m_fullHashes.contains(truncatedHash)) {
            m_fullHashes.insert(truncatedHash, hash);
            emit fullHashesUpdated();
        } else {
            m_fullHashes.insert(truncatedHash, hash);
        }
        hash = truncatedHash;
    }

    if (relay) {
        url.setScheme(QStringLiteral("http"));
        url.setHost(addr);
        url.setPort(Settings::instance()->getPort());
        url.setPath("/" + hash);
    } else {
        QUrlQuery q(url);
        if (q.hasQueryItem(Utils::idKey)) q.removeAllQueryItems(Utils::idKey);
        q.addQueryItem(Utils::idKey, hash);
        url.setQuery(q);
    }

    return true;
}

QByteArray ContentServer::encrypt(const QByteArray &data) {
    QByteArray _data(data);

    auto key = Settings::instance()->getKey();
    QByteArray tmp{key};
    while (key.size() < _data.size()) key += tmp;

    for (int i = 0; i < _data.size(); ++i) _data[i] = _data.at(i) ^ key.at(i);

    _data = _data.toBase64(QByteArray::Base64UrlEncoding |
                           QByteArray::OmitTrailingEquals);
    return _data;
}

QByteArray ContentServer::decrypt(const QByteArray &data) {
    auto _data = QByteArray::fromBase64(
        data, QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
    auto key = Settings::instance()->getKey();
    QByteArray tmp{key};
    while (key.size() < _data.size()) key += tmp;

    for (int i = 0; i < _data.size(); ++i) _data[i] = _data.at(i) ^ key.at(i);

    return _data;
}

QString ContentServer::pathFromUrl(const QUrl &url) const {
    bool isFile;
    if (auto id = idUrlFromUrl(url, &isFile); id && isFile)
        return id->toLocalFile();
    return {};
}

std::optional<QUrl> ContentServer::idUrlFromUrl(const QUrl &url,
                                                bool *isFile) const {
    const auto hash = [&] {
        QString hash;
        if (QUrlQuery q{url}; q.hasQueryItem(Utils::idKey)) {
            hash = q.queryItemValue(Utils::idKey);
        } else {
            hash = url.path();
            hash = hash.right(hash.length() - 1);
        }
        if (m_fullHashes.contains(hash)) hash = m_fullHashes.value(hash);
        return hash;
    }();

    QUrl id{QString::fromUtf8(decrypt(hash.toUtf8()))};
    if (!id.isValid()) return std::nullopt;

    if (QUrlQuery q{id}; !q.hasQueryItem(Utils::cookieKey) ||
                         q.queryItemValue(Utils::cookieKey).isEmpty()) {
        return std::nullopt;
    }

    if (id.isLocalFile()) {
        QFileInfo file{id.toLocalFile()};
        if (!file.exists() || !file.isFile()) return std::nullopt;
        if (isFile) *isFile = true;
        return id;
    }

    if (isFile) *isFile = false;

    return id;
}

QString ContentServer::idFromUrl(const QUrl &url) const {
    if (auto id = idUrlFromUrl(url)) return id->toString();

    return {};
}

QString ContentServer::urlFromUrl(const QUrl &url) const {
    if (auto id = idUrlFromUrl(url))
        return Utils::urlFromId(id.value()).toString();

    return {};
}

void ContentServer::removeMeta(const QUrl &url) { m_metaCache.remove(url); }

ContentServer::ItemMeta *ContentServer::getMetaForImg(const QUrl &url,
                                                      bool createNew) {
    return getMeta(url, createNew, {}, {}, false, true, false,
                   Type::Type_Image);
}

ContentServer::ItemMeta *ContentServer::getMeta(const QUrl &url, bool createNew,
                                                const QUrl &origUrl,
                                                const QString &app, bool ytdl,
                                                bool img, bool refresh,
                                                Type type) {
#ifdef QT_DEBUG
    qDebug() << "get meta:" << Utils::anonymizedUrl(url)
             << ", create new:" << createNew << ", ytdl:" << ytdl
             << ", img:" << img << ", refresh:" << refresh
             << ", type:" << static_cast<int>(type);
#endif
    auto it = getMetaCacheIterator(url, createNew, origUrl, app, ytdl, img,
                                   refresh, /*type=*/type);
    if (it == m_metaCache.end()) {
        return nullptr;
    }
    return &it.value();
}

bool ContentServer::metaExists(const QUrl &url) const {
    auto i = m_metaCache.find(url);
    if (i == m_metaCache.end()) return false;
    if (i->flagSet(MetaFlag::MadeFromCache) && !QFileInfo::exists(i->path)) {
        qWarning() << "meta made from cache exists but cached file does not";
        return false;
    }
    return true;
}

ContentServer::ItemMeta *ContentServer::getMetaForId(const QUrl &id,
                                                     bool createNew) {
    auto [url, origUrl] = Utils::urlsFromId(id);
    return getMeta(url, createNew, /*origUrl=*/origUrl, {}, false, false, false,
                   static_cast<Type>(Utils::typeFromId(id)));
}

ContentServer::ItemMeta *ContentServer::getMetaForId(const QString &id,
                                                     bool createNew) {
    return getMetaForId(QUrl{id}, createNew);
}

QHash<QUrl, ContentServer::ItemMeta>::iterator
ContentServer::getMetaCacheIterator(const QUrl &url, bool createNew,
                                    const QUrl &origUrl, const QString &app,
                                    bool ytdl, bool img, bool refresh,
                                    Type type) {
    if (url.isEmpty()) return m_metaCache.end();

    auto i = m_metaCache.find(url);

    QString albumArt;
    if (refresh && i != m_metaCache.end()) {
        if (i->flagSet(MetaFlag::NiceAlbumArt))
            albumArt = std::move(i->albumArt);
        removeMeta(url);
        i = m_metaCache.end();
    }

    if (i == m_metaCache.end()) {
        if (createNew) {
            auto newIt =
                makeItemMeta(url, origUrl, app, ytdl, img, refresh, type);
            if (!albumArt.isEmpty() && newIt != m_metaCache.end()) {
                newIt->albumArt = std::move(albumArt);
                newIt->setFlags(MetaFlag::NiceAlbumArt);
            }
            return newIt;
        }
#ifdef QT_DEBUG
        qDebug() << "meta data does not exist";
#endif
        return m_metaCache.end();
    }

    if (i->flagSet(MetaFlag::MadeFromCache) && QFileInfo::exists(i->path) &&
        i->type == Type::Type_Music && type != Type::Type_Music) {
        qWarning() << "meta made from cache exists but type does not match";
        if (createNew)
            return makeItemMeta(url, origUrl, app, ytdl, img, refresh, type);
        return m_metaCache.end();
    }

    if (i->flagSet(MetaFlag::MadeFromCache) && !QFileInfo::exists(i->path)) {
        qWarning() << "meta made from cache exists but cached file does not";
        removeMeta(url);
        if (createNew)
            return makeItemMeta(url, origUrl, app, ytdl, img, refresh, type);
        return m_metaCache.end();
    }

    return i;
}

QHash<QUrl, ContentServer::ItemMeta>::iterator
ContentServer::getMetaCacheIteratorForId(const QUrl &id, bool createNew) {
    return getMetaCacheIterator(Utils::urlFromId(id), createNew);
}

QHash<QUrl, ContentServer::ItemMeta>::iterator
ContentServer::metaCacheIteratorEnd() {
    return m_metaCache.end();
}

QHash<QUrl, ContentServer::ItemMeta>::iterator
ContentServer::makeItemMetaUsingTracker(const QUrl &url) {
    auto fileUrl = url.toString(QUrl::EncodeUnicode | QUrl::EncodeSpaces);
    auto path = url.toLocalFile();

    if (getContentTypeByExtension(path) == Type::Type_Image) {
        return m_metaCache.end();  // fallback to taglib
    }

    auto *tracker = Tracker::instance();
    auto query = queryTemplate.arg(
        fileUrl, tracker->tracker3() ? "nmm:artist" : "nmm:performer");

    if (!tracker->query(query, false)) {
        qWarning() << "cannot get tracker data for url:" << fileUrl;
        return m_metaCache.end();
    }

    auto res = tracker->getResult();
    TrackerCursor cursor(res.first, res.second);

    int n = cursor.columnCount();

    if (n == 10) {
        while (cursor.next()) {
            QFileInfo file{path};

            auto &meta = m_metaCache[url];
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
            meta.setFlags(static_cast<int>(MetaFlag::Valid) |
                          static_cast<int>(MetaFlag::Local) |
                          static_cast<int>(MetaFlag::Seek));

            // Recording meta
            if (meta.album == recAlbumName) {
                if (!readMetaUsingTaglib(path, &meta.title, &meta.artist,
                                         &meta.album, &meta.comment,
                                         &meta.recUrl, &meta.recDate,
                                         &meta.albumArt)) {
                    qWarning() << "cannot read meta with taglib";
                }
            }

            return m_metaCache.find(url);
        }
    }

    return m_metaCache.end();
}

QString ContentServer::readTitleUsingTaglib(const QString &path) {
    TagLib::FileRef f(path.toUtf8().constData(), false);

    if (!f.isNull()) {
        auto *tag = f.tag();
        if (tag) {
            return QString::fromWCharArray(tag->title().toCWString());
        }
    }

    return {};
}

inline static auto wStr2qStr(const TagLib::String &str) {
    return QString::fromWCharArray(str.toCWString());
}

bool ContentServer::readMP4MetaUsingTaglib(const TagLib::FileRef &file,
                                           QString *recUrl, QDateTime *recDate,
                                           Type *otype,
                                           const QString &albumArtPrefix,
                                           QString *artPath) {
    using namespace TagLib;

    auto *mf = dynamic_cast<MP4::File *>(file.file());
    if (!mf) return false;

    auto *tag = mf->tag();
    if (!tag) return false;

#ifdef QT_DEBUG
    qDebug() << "reading MP4 tag";
#endif

    if (recUrl || recDate || otype) {
        auto tags = file.file()->properties();
        if (recDate) {
            auto item = tag->item(recMp4TagPrefix +
                                  ContentServer::recDateTagName.toStdString());
            if (item.isValid()) {
                *recDate = QDateTime::fromString(
                    wStr2qStr(item.toStringList().toString()), Qt::ISODate);
            }
        }
        if (recUrl) {
            auto item = tag->item(recMp4TagPrefix +
                                  ContentServer::recUrlTagName.toStdString());
            if (item.isValid()) {
                *recUrl = wStr2qStr(item.toStringList().toString());
            }
        }
        if (otype) {
            auto item = tag->item(recMp4TagPrefix +
                                  ContentServer::typeTagName.toStdString());
            if (item.isValid()) {
                *otype =
                    static_cast<Type>(item.toStringList().toString().toInt());
            } else {
                *otype = Type::Type_Invalid;
            }
        }
    }

    if (artPath && !albumArtPrefix.isEmpty()) {
        auto item = tag->item("covr");
        if (item.isValid()) {
            auto covers = item.toCoverArtList();
            if (!covers.isEmpty()) {
                const auto &cover = covers.front();
                auto ext = [](MP4::CoverArt::Format format) -> QString {
                    switch (format) {
                        case MP4::CoverArt::Format::PNG:
                            return "png";
                        case MP4::CoverArt::Format::JPEG:
                            return "jpg";
                        case MP4::CoverArt::Format::GIF:
                            return "gif";
                        case MP4::CoverArt::Format::BMP:
                            return "bmp";
                        case MP4::CoverArt::Unknown:
                            break;
                    }
                    return {};
                }(cover.format());

                if (!ext.isEmpty()) {
                    auto path = Utils::writeToCacheFile(
                        albumArtPrefix + ext,
                        QByteArray::fromRawData(
                            cover.data().data(),
                            static_cast<int>(cover.data().size())));
                    if (path) {
                        *artPath = *path;
                    } else {
                        qWarning() << "cannot read art file";
                    }
                } else {
                    qWarning() << "art unknown format";
                }
            }
        }
    }

    return true;
}

bool ContentServer::readID3MetaUsingTaglib(const TagLib::FileRef &file,
                                           QString *recUrl, QDateTime *recDate,
                                           Type *otype,
                                           const QString &albumArtPrefix,
                                           QString *artPath) {
    using namespace TagLib;

    auto *mf = dynamic_cast<MPEG::File *>(file.file());
    if (!mf) return false;

#ifdef QT_DEBUG
    qDebug() << "reading ID3 tag";
#endif

    if (recUrl || recDate || otype) {
        auto tags = mf->properties();
        if (recDate) {
            auto it = tags.find(ContentServer::recDateTagName.toStdString());
            if (it != tags.end())
                *recDate = QDateTime::fromString(
                    wStr2qStr(it->second.toString()), Qt::ISODate);
        }
        if (recUrl) {
            auto it = tags.find(ContentServer::recUrlTagName.toStdString());
            if (it != tags.end()) {
                *recUrl = wStr2qStr(it->second.toString());
            } else {  // old rec url tag
                it = tags.find(ContentServer::recUrlTagName2.toStdString());
                if (it != tags.end()) {
                    *recUrl = wStr2qStr(it->second.toString());
                }
            }
        }
        if (otype) {
            auto it = tags.find(ContentServer::typeTagName.toStdString());
            if (it != tags.end()) {
                *otype = static_cast<Type>(it->second.toString().toInt());
            } else {
                *otype = Type::Type_Invalid;
            }
        }
    }

    if (artPath && !albumArtPrefix.isEmpty()) {
        auto *tag = mf->ID3v2Tag(true);
        auto fl = tag->frameList("APIC");

        if (!fl.isEmpty()) {
            auto *frame =
                dynamic_cast<ID3v2::AttachedPictureFrame *>(fl.front());
            if (frame) {
                auto ext = m_imgMimeToExtMap.value(
                    QString::fromStdString(frame->mimeType().to8Bit()));
                if (!ext.isEmpty()) {
                    auto path = Utils::writeToCacheFile(
                        albumArtPrefix + ext,
                        QByteArray::fromRawData(
                            frame->picture().data(),
                            static_cast<int>(frame->picture().size())));
                    if (path) {
                        *artPath = *path;
                    } else {
                        qWarning() << "cannot write art file:" << *path;
                    }
                } else {
                    qWarning() << "art unknown format";
                }
            }
        }
    }

    return true;
}

bool ContentServer::readMetaUsingTaglib(const QString &path, QString *title,
                                        QString *artist, QString *album,
                                        QString *comment, QString *recUrl,
                                        QDateTime *recDate, QString *artPath,
                                        Type *otype, int *duration,
                                        double *bitrate, double *sampleRate,
                                        int *channels) {
    using namespace TagLib;

    bool readAudioProperties = duration || bitrate || sampleRate || channels;

    FileRef file{path.toUtf8().constData(), readAudioProperties};

    if (file.isNull()) {
        qWarning() << "cannot read meta from file:" << path;
        return false;
    }

    auto *tag = file.tag();
    if (!tag) {
        qWarning() << "cannot read tag from file:" << path;
        return false;
    }

    if (title) *title = wStr2qStr(tag->title());
    if (artist) *artist = wStr2qStr(tag->artist());
    if (album) *album = wStr2qStr(tag->album());
    if (comment) *comment = wStr2qStr(tag->comment());

    if (readAudioProperties) {
        auto *prop = file.audioProperties();
        if (prop) {
            if (duration) *duration = prop->length();
            if (bitrate) *bitrate = prop->bitrate();
            if (sampleRate) *sampleRate = prop->sampleRate();
            if (channels) *channels = prop->channels();
        }
    }

    auto albumArtPrefix = albumArtCacheName(path, {});
    bool extractArt = artPath != nullptr;
    if (extractArt) {
        if (auto p = Utils::cachePathForFile(albumArtPrefix + "jpg")) {
            *artPath = *p;
            extractArt = false;
        } else if (auto p = Utils::cachePathForFile(albumArtPrefix + "png")) {
            *artPath = *p;
            extractArt = false;
        } else if (auto p = Utils::cachePathForFile(albumArtPrefix + "bmp")) {
            *artPath = *p;
            extractArt = false;
        }
    }

    if (!readID3MetaUsingTaglib(file, recUrl, recDate, otype, albumArtPrefix,
                                extractArt ? artPath : nullptr)) {
        readMP4MetaUsingTaglib(file, recUrl, recDate, otype, albumArtPrefix,
                               extractArt ? artPath : nullptr);
    }

    return true;
}

bool ContentServer::writeID3MetaUsingTaglib(
    TagLib::FileRef &file, bool removeExistingTags, const QString &title,
    const QString &artist, const QString &album, const QString &comment,
    const QString &recUrl, const QDateTime &recDate, const QString &artPath,
    Type otype) {
    using namespace TagLib;

    auto *mf = dynamic_cast<MPEG::File *>(file.file());
    if (!mf) return false;

    if (removeExistingTags) {
        mf->strip();
        mf->save();
    }

    auto *tag = mf->ID3v2Tag(true);

#ifdef QT_DEBUG
    qDebug() << "writing ID3 tag";
#endif

    if (!title.isEmpty()) tag->setTitle(title.toStdWString());
    if (!artist.isEmpty()) tag->setArtist(artist.toStdWString());
    if (!album.isEmpty()) tag->setAlbum(album.toStdWString());
    if (!comment.isEmpty()) tag->setComment(comment.toStdWString());

    auto txxxFrame = [tag](const QString &text) -> ID3v2::Frame * {
        auto frames = tag->frameList("TXXX");
        auto it = std::find_if(frames.begin(), frames.end(), [&text](auto *f) {
            return QString::fromLatin1(f->toString().toCString())
                .contains(text);
        });
        if (it == frames.end()) return nullptr;
        return *it;
    };

    if (!recUrl.isEmpty()) {
        auto *newFrame = ID3v2::Frame::createTextualFrame(
            recUrlTagName.toStdString(), {recUrl.toStdString()});
        if (removeExistingTags) {
            tag->addFrame(newFrame);
        } else {
            auto *oldFrame = txxxFrame(recUrlTagName);
            if (oldFrame) {
                oldFrame->setData(newFrame->render());
            } else {
                tag->addFrame(newFrame);
            }
        }
    }
    if (!recDate.isNull()) {
        auto *newFrame = ID3v2::Frame::createTextualFrame(
            recDateTagName.toStdString(),
            {recDate.toString(Qt::ISODate).toStdString()});
        if (removeExistingTags) {
            tag->addFrame(newFrame);
        } else {
            auto *oldFrame = txxxFrame(recDateTagName);
            if (oldFrame) {
                oldFrame->setData(newFrame->render());
            } else {
                tag->addFrame(newFrame);
            }
        }
    }

    if (!artPath.isEmpty() && artPath.contains('.')) {
        if (auto mime = m_imgExtMap.value(artPath.split('.').last());
            !mime.isEmpty()) {
            if (QFile art{artPath}; art.open(QIODevice::ReadOnly)) {
                if (auto bytes = art.readAll(); !bytes.isEmpty()) {
                    auto *apf = new ID3v2::AttachedPictureFrame();
                    apf->setPicture(
                        ByteVector(bytes.constData(), uint32_t(bytes.size())));
                    apf->setType(ID3v2::AttachedPictureFrame::FrontCover);
                    apf->setMimeType(mime.toStdString());
                    apf->setDescription("Cover");
                    tag->addFrame(apf);
                }
            } else {
                qWarning() << "cannot open:" << artPath;
            }
        } else {
            qWarning() << "cover type is unknown";
        }
    }

    if (otype != Type::Type_Unknown) {
        auto *newFrame = ID3v2::Frame::createTextualFrame(
            typeTagName.toStdString(),
            {String::number(static_cast<int>(otype))});
        if (removeExistingTags) {
            tag->addFrame(newFrame);
        } else {
            auto *oldFrame = txxxFrame(typeTagName);
            if (oldFrame) {
                oldFrame->setData(newFrame->render());
            } else {
                tag->addFrame(newFrame);
            }
        }
    }

    mf->save(MPEG::File::ID3v2);

    return true;
}

bool ContentServer::writeMP4MetaUsingTaglib(
    TagLib::FileRef &file, [[maybe_unused]] bool removeExistingTags,
    const QString &title, const QString &artist, const QString &album,
    const QString &comment, const QString &recUrl, const QDateTime &recDate,
    const QString &artPath, Type otype) {
    using namespace TagLib;

    auto *mf = dynamic_cast<MP4::File *>(file.file());
    if (!mf) return false;

    auto *tag = mf->tag();
    if (!tag) return false;

#ifdef QT_DEBUG
    qDebug() << "writing mp4 tag";
#endif

    if (!title.isEmpty()) tag->setTitle(title.toStdWString());
    if (!artist.isEmpty()) tag->setArtist(artist.toStdWString());
    if (!album.isEmpty()) tag->setAlbum(album.toStdWString());
    if (!comment.isEmpty()) tag->setComment(comment.toStdWString());

    if (!recUrl.isEmpty()) {
        auto key = recMp4TagPrefix + ContentServer::recUrlTagName.toStdString();
        tag->removeItem(key);
        tag->setItem(key, {String{recUrl.toStdString(), String::Type::UTF8}});
    }
    if (!recDate.isNull()) {
        auto key =
            recMp4TagPrefix + ContentServer::recDateTagName.toStdString();
        tag->removeItem(key);
        tag->setItem(key, {String{recDate.toString(Qt::ISODate).toStdString(),
                                  String::Type::UTF8}});
    }

    if (!artPath.isEmpty() && artPath.contains('.')) {
        auto mapPathToFormat = [](const QString &path) {
            auto ext = path.split('.').last();
            if (ext.compare(QStringLiteral("png"), Qt::CaseInsensitive) == 0)
                return MP4::CoverArt::Format::PNG;
            if (ext.compare(QStringLiteral("jpg"), Qt::CaseInsensitive) == 0 ||
                ext.compare(QStringLiteral("jpeg"), Qt::CaseInsensitive))
                return MP4::CoverArt::Format::JPEG;
            if (ext.compare(QStringLiteral("gif"), Qt::CaseInsensitive) == 0)
                return MP4::CoverArt::Format::GIF;
            if (ext.compare(QStringLiteral("bmp"), Qt::CaseInsensitive) == 0)
                return MP4::CoverArt::Format::BMP;
            return MP4::CoverArt::Format::Unknown;
        };

        if (QFile art{artPath}; art.open(QIODevice::ReadOnly)) {
            auto bytes = art.readAll();
            if (!bytes.isEmpty()) {
                auto type = mapPathToFormat(artPath);
                if (type != MP4::CoverArt::Format::Unknown) {
                    List<MP4::CoverArt> covers{};
                    covers.append({type,
                                   {bytes.constData(),
                                    static_cast<unsigned int>(bytes.size())}});
                    tag->setItem("covr", {covers});
                } else {
                    qWarning() << "cover type is unknown";
                }
            }
        } else {
            qWarning() << "cannot open:" << artPath;
        }
    }

    if (otype != Type::Type_Unknown) {
        auto key = recMp4TagPrefix + ContentServer::typeTagName.toStdString();
        tag->removeItem(key);
        tag->setItem(key, {String::number(static_cast<int>(otype))});
    }

    mf->save();

    return true;
}

bool ContentServer::writeGenericMetaUsingTaglib(
    TagLib::FileRef &file, const QString &title, const QString &artist,
    const QString &album, const QString &comment, const QString &recUrl,
    const QDateTime &recDate, Type otype) {
    using namespace TagLib;

    auto *tag = file.tag();

    if (!tag) return false;

#ifdef QT_DEBUG
    qDebug() << "writing generic tag";
#endif

    if (!title.isEmpty()) tag->setTitle(title.toStdWString());
    if (!artist.isEmpty()) tag->setArtist(artist.toStdWString());
    if (!album.isEmpty()) tag->setAlbum(album.toStdWString());
    if (!comment.isEmpty()) tag->setComment(comment.toStdWString());

    if (!recUrl.isEmpty() || !recDate.isNull() || otype != Type::Type_Unknown) {
        auto *f = file.file();
        auto tags = f->properties();

        if (!recUrl.isEmpty()) {
            auto key = ContentServer::recUrlTagName.toStdString();
            auto val = StringList(recUrl.toStdString());
            auto it = tags.find(key);
            if (it != tags.end())
                it->second = val;
            else
                tags.insert(key, val);
        }
        if (!recDate.isNull()) {
            auto key = ContentServer::recDateTagName.toStdString();
            auto val = StringList(recDate.toString(Qt::ISODate).toStdString());
            auto it = tags.find(key);
            if (it != tags.end())
                it->second = val;
            else
                tags.insert(key, val);
        }
        if (otype != Type::Type_Unknown) {
            auto key = ContentServer::typeTagName.toStdString();
            auto val = StringList(String::number(static_cast<int>(otype)));
            auto it = tags.find(key);
            if (it != tags.end())
                it->second = val;
            else
                tags.insert(key, val);
        }

        f->setProperties(tags);
    }

    file.save();

    return true;
}

bool ContentServer::writeMetaUsingTaglib(
    const QString &path, bool removeExistingTags, const QString &title,
    const QString &artist, const QString &album, const QString &comment,
    const QString &recUrl, const QDateTime &recDate, QString artPath,
    Type otype) {
    TagLib::FileRef file(path.toUtf8().constData(), false);

    if (file.isNull()) return false;

    artPath = localArtPathIfExists(artPath);

    if (writeID3MetaUsingTaglib(file, removeExistingTags, title, artist, album,
                                comment, recUrl, recDate, artPath, otype)) {
        return true;
    }

    if (writeMP4MetaUsingTaglib(file, removeExistingTags, title, artist, album,
                                comment, recUrl, recDate, artPath, otype)) {
        return true;
    }

    if (writeGenericMetaUsingTaglib(file, title, artist, album, comment, recUrl,
                                    recDate, otype)) {
        return true;
    }

    return false;
}

ContentServer::Type ContentServer::readOtypeFromCachedFile(
    const QString &filename) {
    auto path = QDir{Settings::instance()->getCacheDir()}.filePath(filename);

    Type otype;
    if (!readMetaUsingTaglib(path, nullptr, nullptr, nullptr, nullptr, nullptr,
                             nullptr, nullptr, /*otype=*/&otype)) {
        return Type::Type_Invalid;
    }
    return otype;
}

QString ContentServer::minResUrl(const UPnPClient::UPnPDirObject &item) {
    uint64_t l = item.m_resources.size();

    if (l == 0) return {};

    uint64_t min_i = 0;
    int min_width = std::numeric_limits<int>::max();

    for (uint64_t i = 0; i < l; ++i) {
        std::string val;
        QSize();
        if (item.m_resources[i].m_uri.empty()) continue;
        if (item.getrprop(uint32_t(i), "resolution", val)) {
            uint64_t pos = val.find('x');
            if (pos == std::string::npos) {
                pos = val.find('X');
                if (pos == std::string::npos) continue;
            }
            if (pos < 1) continue;
            int width = std::stoi(val.substr(0, pos));
            if (i == 0 || width < min_width) {
                min_i = i;
                min_width = width;
            }
        }
    }

    return QString::fromStdString(item.m_resources[min_i].m_uri);
}

ContentServer::ItemType ContentServer::itemTypeFromUrl(const QUrl &url) {
    if (url.scheme() == QStringLiteral("jupii")) {
        if (url.host() == QStringLiteral("upnp")) return ItemType_Upnp;
        if (url.host() == QStringLiteral("screen"))
            return ItemType_ScreenCapture;
        if (url.host() == QStringLiteral("pulse")) return ItemType_AudioCapture;
        if (url.host() == QStringLiteral("mic")) return ItemType_Mic;
    } else if (url.scheme() == QStringLiteral("http") ||
               url.scheme() == QStringLiteral("https")) {
        return ItemType_Url;
    } else if (url.isLocalFile() || url.scheme() == QStringLiteral("qrc")) {
        return ItemType_LocalFile;
    }
    return ItemType_Unknown;
}

QHash<QUrl, ContentServer::ItemMeta>::iterator
ContentServer::makeItemMetaUsingTaglib(const QUrl &url) {
    auto path = url.toLocalFile();
    QFileInfo file{path};

    ItemMeta meta;
    meta.url = url;
    meta.mime = getContentMimeByExtension(path);
    meta.type = getContentTypeByExtension(path);
    meta.path = std::move(path);
    meta.size = file.size();
    meta.filename = file.fileName();
    meta.itemType = ItemType_LocalFile;
    meta.setFlags(static_cast<int>(MetaFlag::Valid) |
                  static_cast<int>(MetaFlag::Local));

    if (meta.type == Type::Type_Image) {
        meta.setFlags(MetaFlag::Seek, false);
        if (QFile f{meta.path}; f.open(QIODevice::ReadOnly)) {
            if (auto thumbPath =
                    Thumb::save(f.readAll(), meta.url,
                                     m_imgMimeToExtMap.value(meta.mime))) {
                meta.albumArt = std::move(*thumbPath);
            }
        }
    } else {
        meta.setFlags(MetaFlag::Seek);

        if (!readMetaUsingTaglib(
                meta.path, &meta.title, &meta.artist, &meta.album,
                &meta.comment, &meta.recUrl, &meta.recDate, &meta.albumArt,
                /*otype=*/nullptr, &meta.duration, &meta.bitrate,
                &meta.sampleRate, &meta.channels)) {
            qWarning() << "cannot read meta with taglib";
        }
    }

    return m_metaCache.insert(url, meta);
}

QHash<QUrl, ContentServer::ItemMeta>::iterator ContentServer::makeUpnpItemMeta(
    const QUrl &url) {
    qDebug() << "make upnp meta:" << Utils::anonymizedUrl(url);

    if (QThread::currentThread()->isInterruptionRequested()) {
        qWarning() << "thread interruption was requested";
        return m_metaCache.end();
    }

    auto spl = url.path(QUrl::FullyEncoded).split('/');
    if (spl.size() < 3) {
        qWarning() << "path is too short";
        return m_metaCache.end();
    }

    auto did = QUrl::fromPercentEncoding(spl.at(1).toLatin1());  // cdir dev id
    auto id = QUrl::fromPercentEncoding(spl.at(2).toLatin1());   // cdir item id
#ifdef QT_DEBUG
    qDebug() << "did:" << did;
    qDebug() << "id:" << id;
#endif

    auto cd = Services::instance()->contentDir;
    if (!cd->init(did)) {
        qWarning() << "cannot init cdir service";
        return m_metaCache.end();
    }

    if (!cd->getInited()) {
        qDebug() << "cdir is not inited, so waiting for init";

        QEventLoop loop;
        connect(cd.get(), &Service::initedChanged, &loop, &QEventLoop::quit);
        QTimer::singleShot(httpTimeout, &loop, &QEventLoop::quit);  // timeout
        loop.exec();  // waiting for init...

        if (!cd->getInited()) {
            qDebug() << "cannot init cdir service";
            return m_metaCache.end();
        }
    }

    UPnPClient::UPnPDirContent content;
    if (!cd->readItem(id, content)) {
        qDebug() << "cannot read from cdir service";
        return m_metaCache.end();
    }

    if (content.m_items.empty()) {
        qDebug() << "item doesn't exist on cdir";
        return m_metaCache.end();
    }

    UPnPClient::UPnPDirObject &item = content.m_items[0];

    if (item.m_resources.empty()) {
        qDebug() << "item doesn't have resources";
        return m_metaCache.end();
    }

    if (item.m_resources[0].m_uri.empty()) {
        qDebug() << "item uri is empty";
        return m_metaCache.end();
    }

    auto surl = QString::fromStdString(item.m_resources[0].m_uri);

    ItemMeta meta;
    meta.url = QUrl(surl);
    meta.type =
        typeFromUpnpClass(QString::fromStdString(item.getprop("upnp:class")));
    UPnPP::ProtocolinfoEntry proto;
    if (item.m_resources[0].protoInfo(proto)) {
        meta.mime = QString::fromStdString(proto.contentFormat);
        qDebug() << "proto info mime:" << meta.mime;
    } else {
        qWarning() << "cannot parse proto info";
        meta.mime = QStringLiteral("audio/mpeg");
    }
    meta.size = 0;
    meta.album = QString::fromStdString(item.getprop("upnp:album"));
    meta.artist =
        Utils::parseArtist(QString::fromStdString(item.getprop("upnp:artist")));
    meta.didl = QString::fromStdString(item.getdidl());
    meta.didl = meta.didl.replace(surl, QStringLiteral("%URL%"));
    meta.albumArt =
        meta.type == Type::Type_Image
            ? minResUrl(item)
            : QString::fromStdString(item.getprop("upnp:albumArtURI"));
    meta.filename = url.fileName();
    meta.title = QString::fromStdString(item.m_title);
    if (meta.title.isEmpty()) meta.title = meta.filename;
    meta.upnpDevId = std::move(did);
    meta.itemType = ItemType_Upnp;
    meta.setFlags(MetaFlag::Valid);
    meta.setFlags(
        static_cast<int>(MetaFlag::Local) | static_cast<int>(MetaFlag::Seek),
        false);
#ifdef QT_DEBUG
    qDebug() << "DIDL:" << meta.didl;
#endif
    return m_metaCache.insert(url, meta);
}

QHash<QUrl, ContentServer::ItemMeta>::iterator ContentServer::makeMicItemMeta(
    const QUrl &url) {
    ItemMeta meta;
    meta.url = url;
    meta.channels = MicCaster::channelCount;
    meta.sampleRate = MicCaster::sampleRate;
    meta.mime = m_musicExtMap.value(QStringLiteral("mp3"));
    meta.type = Type::Type_Music;
    meta.size = 0;
    meta.title = tr("Microphone");
    meta.itemType = ItemType_Mic;
#if defined(SAILFISH) || defined(KIRIGAMI)
    meta.albumArt = IconProvider::pathToNoResId(QStringLiteral("icon-mic"));
#endif
    meta.setFlags(static_cast<int>(MetaFlag::Valid) |
                  static_cast<int>(MetaFlag::Local));
    meta.setFlags(MetaFlag::Seek, false);

    return m_metaCache.insert(url, meta);
}

QHash<QUrl, ContentServer::ItemMeta>::iterator
ContentServer::makeAudioCaptureItemMeta(const QUrl &url) {
    ItemMeta meta;
    meta.url = url;
    meta.mime = m_musicExtMap.value(QStringLiteral("mp3"));
    meta.type = Type::Type_Music;
    meta.size = 0;
    meta.title = tr("Audio capture");
    meta.itemType = ItemType_AudioCapture;
#if defined(SAILFISH) || defined(KIRIGAMI)
    meta.albumArt = IconProvider::pathToNoResId(QStringLiteral("icon-pulse"));
#endif
    meta.setFlags(static_cast<int>(MetaFlag::Valid) |
                  static_cast<int>(MetaFlag::Local));
    meta.setFlags(MetaFlag::Seek, false);

    return m_metaCache.insert(url, meta);
}

QHash<QUrl, ContentServer::ItemMeta>::iterator
ContentServer::makeScreenCaptureItemMeta(const QUrl &url) {
    ItemMeta meta;
    meta.url = url;
    meta.mime = m_videoExtMap.value(QStringLiteral("tsv"));
    meta.type = Type::Type_Video;
    meta.size = 0;
    meta.title = tr("Screen capture");
    meta.itemType = ItemType_ScreenCapture;
#if defined(SAILFISH) || defined(KIRIGAMI)
    meta.albumArt = IconProvider::pathToNoResId(QStringLiteral("icon-screen"));
#endif
    meta.setFlags(static_cast<int>(MetaFlag::Valid) |
                  static_cast<int>(MetaFlag::Local));
    meta.setFlags(MetaFlag::Seek, false);

    return m_metaCache.insert(url, meta);
}

QString ContentServer::mimeFromDisposition(const QString &disposition) {
    QString mime;

    if (disposition.contains(QStringLiteral("attachment"))) {
        qDebug() << "content as a attachment detected";
        const static QRegExp rx{QStringLiteral("filename=\"?([^\";]*)\"?"),
                                Qt::CaseInsensitive};
        int pos = 0;
        while ((pos = rx.indexIn(disposition, pos)) != -1) {
            QString filename = rx.cap(1);
            if (!filename.isEmpty()) {
                mime = getContentMimeByExtension(filename);
                break;
            }
            pos += rx.matchedLength();
        }
    }

    return mime;
}

bool ContentServer::hlsPlaylist(const QByteArray &data) {
    return data.contains("#EXT-X-");
}

QHash<QUrl, ContentServer::ItemMeta>::iterator
ContentServer::makeItemMetaUsingYtdlApi(
    QUrl url, ItemMeta &meta, std::shared_ptr<QNetworkAccessManager> nam,
    int counter) {
    qDebug() << "trying to find url with ytdl:" << url;

#ifdef HARBOUR
    Q_UNUSED(meta)
    Q_UNUSED(nam)
    Q_UNUSED(counter)
    qDebug() << "ytdl is disabled in harbour build";
    return m_metaCache.end();
#else
    if (QThread::currentThread()->isInterruptionRequested()) {
        qWarning() << "thread interruption was requested";
        return m_metaCache.end();
    }

    if (url.host().contains(QLatin1String{"youtube"}, Qt::CaseInsensitive)) {
        QUrlQuery q{url};
        if (q.hasQueryItem(QStringLiteral("v"))) {
            q.setQueryItems(
                {{QStringLiteral("v"), q.queryItemValue(QStringLiteral("v"))}});
            url.setQuery(q);
            qDebug() << "new ytdl url:" << url.toString();
        }
    }

    auto track = YtdlApi{}.track(url);

    if (!track) {
        qWarning() << "ytdl did not returned any track";
        return m_metaCache.end();
    }

    meta.origUrl = std::move(url);
    meta.title = std::move(track->title);
    meta.app = QStringLiteral("ytdl");
    meta.setFlags(MetaFlag::YtDl);

    if (!track->imageUrl.isEmpty()) {
        meta.albumArt = track->imageUrl.toString();
        meta.setFlags(MetaFlag::NiceAlbumArt);
    }

    auto newUrl = meta.type == Type::Type_Music
                      ? std::move(track->streamAudioUrl)
                      : std::move(track->streamUrl);

    Utils::fixUrl(newUrl);

    return makeItemMetaUsingHTTPRequest2(newUrl, meta, std::move(nam),
                                         counter + 1);
#endif
}

QHash<QUrl, ContentServer::ItemMeta>::iterator
ContentServer::makeItemMetaUsingBcApi(
    const QUrl &url, ItemMeta &meta, std::shared_ptr<QNetworkAccessManager> nam,
    int counter) {
    qDebug() << "trying to find url with bc:" << url;

    if (QThread::currentThread()->isInterruptionRequested()) {
        qWarning() << "thread interruption was requested";
        return m_metaCache.end();
    }

    auto track = BcApi{nam}.track(url);

    if (track.streamUrl.isEmpty()) {
        qWarning() << "bc returned empty url";
        return m_metaCache.end();
    }

    meta.title = std::move(track.title);
    meta.album = std::move(track.album);
    meta.artist = std::move(track.artist);
    meta.duration = track.duration;
    meta.app = QStringLiteral("bc");
    meta.setFlags(MetaFlag::YtDl);

    if (!track.imageUrl.isEmpty()) {
        meta.albumArt = track.imageUrl.toString();
        meta.setFlags(MetaFlag::NiceAlbumArt);
    }

    auto newUrl = std::move(track.streamUrl);
    Utils::fixUrl(newUrl);

    return makeItemMetaUsingHTTPRequest2(newUrl, meta, std::move(nam),
                                         counter + 1);
}

QHash<QUrl, ContentServer::ItemMeta>::iterator
ContentServer::makeItemMetaUsingSoundcloudApi(
    const QUrl &url, ItemMeta &meta, std::shared_ptr<QNetworkAccessManager> nam,
    int counter) {
    qDebug() << "trying to find url with soundcloud:" << url;

    if (QThread::currentThread()->isInterruptionRequested()) {
        qWarning() << "thread interruption was requested";
        return m_metaCache.end();
    }

    auto track = SoundcloudApi{nam}.track(url);

    if (track.streamUrl.isEmpty()) {
        qWarning() << "soundcloud returned empty url";
        return m_metaCache.end();
    }

    meta.title = std::move(track.title);
    meta.album = std::move(track.album);
    meta.artist = std::move(track.artist);
    meta.duration = track.duration;
    meta.app = QStringLiteral("soundcloud");
    meta.setFlags(MetaFlag::YtDl);

    if (!track.imageUrl.isEmpty()) {
        meta.albumArt = track.imageUrl.toString();
        meta.setFlags(MetaFlag::NiceAlbumArt);
    }

    auto newUrl = std::move(track.streamUrl);
    Utils::fixUrl(newUrl);

    return makeItemMetaUsingHTTPRequest2(newUrl, meta, std::move(nam),
                                         counter + 1);
}

QHash<QUrl, ContentServer::ItemMeta>::iterator
ContentServer::makeItemMetaUsingHTTPRequest(const QUrl &url,
                                            const QUrl &origUrl,
                                            const QString &app, bool ytdl,
                                            bool refresh, bool art, Type type) {
    ItemMeta meta;
    meta.setFlags(MetaFlag::YtDl, ytdl);
    meta.setFlags(MetaFlag::Refresh, refresh);  // meta refreshing
    meta.setFlags(MetaFlag::Art, art);
    meta.app = app;
    meta.type = type;

    QUrl fixed_url{url};
    QUrl fixed_origUrl{origUrl};
    Utils::fixUrl(fixed_url);
    Utils::fixUrl(fixed_origUrl);

    if (!origUrl.isEmpty() && fixed_origUrl != fixed_url) {
        meta.setFlags(MetaFlag::OrigUrl);
        meta.origUrl = fixed_origUrl;
    } else {
        meta.setFlags(MetaFlag::OrigUrl, false);
        meta.origUrl = fixed_url;
    }

    meta.url = fixed_url;

    if (!art) {
        if (auto cachedFile = pathToCachedContent(meta, true, meta.type)) {
            auto it = makeItemMetaUsingCachedFile(fixed_url, *cachedFile, meta);
            if (it != m_metaCache.end()) return it;
        }
    }

    return makeItemMetaUsingHTTPRequest2(fixed_url, meta);
}

void ContentServer::makeItemMetaCopy(const QUrl &newUrl,
                                     const ItemMeta &origMeta) {
    qDebug() << "making meta copy:" << origMeta.url << "->" << newUrl;
    if (m_metaCache.contains(newUrl)) {
        qWarning() << "url already in meta cache";
        return;
    }

    auto newMeta{origMeta};
    newMeta.url = newUrl;

    m_metaCache.insert(newUrl, newMeta);
}

QHash<QUrl, ContentServer::ItemMeta>::iterator
ContentServer::makeItemMetaUsingCachedFile(const QUrl &url,
                                           const QString &cachedFile,
                                           ItemMeta &meta) {
    qDebug() << "making meta using cached file:" << cachedFile;

    meta.url = url;
    meta.mime = getContentMimeByExtension(cachedFile);
    meta.type = getContentTypeByExtension(cachedFile);
    meta.size = QFileInfo{cachedFile}.size();
    meta.filename = url.fileName();
    meta.itemType = ItemType_Url;
    meta.setFlags(MetaFlag::Valid);
    meta.setFlags(MetaFlag::Local, false);
    meta.setFlags(MetaFlag::Seek);
    meta.setFlags(MetaFlag::MadeFromCache);
    meta.path = cachedFile;

    if (!readMetaUsingTaglib(cachedFile, &meta.title, &meta.artist, &meta.album,
                             &meta.comment, /*recurl=*/nullptr,
                             /*recDate=*/nullptr, &meta.albumArt,
                             /*otype=*/nullptr, &meta.duration, &meta.bitrate,
                             &meta.sampleRate, &meta.channels)) {
        qWarning() << "cannot read meta with taglib";
        return m_metaCache.end();
    }

    return m_metaCache.insert(url, meta);
}

QString ContentServer::mimeFromReply(const QNetworkReply *reply) {
    auto mime = mimeFromDisposition(
        QString{reply->rawHeader(QByteArrayLiteral("Content-Disposition"))}
            .toLower());

    if (mime.isEmpty()) {
        mime = reply->header(QNetworkRequest::ContentTypeHeader)
                   .toString()
                   .toLower();
    }

    auto list = mime.split(',');
    if (!list.isEmpty()) mime = list.last().trimmed();

    // qDebug() << "mime:" << mime;
    return mime;
}

QString ContentServer::albumArtCacheName(const QString &path,
                                         const QString &ext) {
    return QStringLiteral("art_image_%1.%2").arg(Utils::hash(path), ext);
}

QString ContentServer::albumArtCachePath(const QString &path,
                                         const QString &ext) {
    return QDir{Settings::instance()->getCacheDir()}.filePath(
        albumArtCacheName(path, ext));
}

QString ContentServer::extractedAudioCachePath(const QString &path,
                                               const QString &ext) {
    return QDir{Settings::instance()->getCacheDir()}.filePath(
        QStringLiteral("extracted_audio_%1.%2").arg(Utils::hash(path), ext));
}

QString ContentServer::contentCachePath(const QString &path,
                                        const QString &ext) {
    return QDir{Settings::instance()->getCacheDir()}.filePath(
        QStringLiteral("cache_%1.%2").arg(Utils::hash(path), ext));
}

static void setCookies(const QList<QNetworkCookie> &cookies,
                       QNetworkRequest &request) {
    if (cookies.isEmpty()) return;
#ifdef QT_DEBUG
    for (const auto &c : cookies) {
        qDebug() << "cookie to set:" << c.toRawForm();
    }
#endif
    auto cookieString =
        std::accumulate(std::next(cookies.cbegin()), cookies.cend(),
                        cookies.constFirst().toRawForm(),
                        [](const auto &str, const auto &cookie) {
                            return str + "; " + cookie.toRawForm();
                        });

    request.setRawHeader(QByteArrayLiteral("Cookie"), cookieString);
}

QHash<QUrl, ContentServer::ItemMeta>::iterator
ContentServer::makeItemMetaUsingHTTPRequest2(
    const QUrl &url, ItemMeta &meta, std::shared_ptr<QNetworkAccessManager> nam,
    int counter) {
    qDebug() << ">> making meta using http request";

    if (QThread::currentThread()->isInterruptionRequested()) {
        qWarning() << "thread interruption was requested";
        return m_metaCache.end();
    }

    if (counter >= maxRedirections) {
        qWarning() << "max redirections reached";
        return m_metaCache.end();
    }

    qDebug() << "sending http request for url:" << Utils::anonymizedUrl(url);
    QNetworkRequest request;
    request.setMaximumRedirectsAllowed(maxRedirections);
    request.setUrl(url);
    request.setRawHeader(QByteArrayLiteral("Icy-MetaData"),
                         QByteArrayLiteral("1"));
    request.setRawHeader(QByteArrayLiteral("Range"),
                         QByteArrayLiteral("bytes=0-"));
    request.setRawHeader(QByteArrayLiteral("Connection"),
                         QByteArrayLiteral("close"));
    request.setRawHeader(QByteArrayLiteral("User-Agent"), userAgent);
#if QT_VERSION >= QT_VERSION_CHECK(5, 9, 0)
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
#else
    request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
#endif

    if (nam) {
        // maybe it's not needed
        setCookies(nam->cookieJar()->cookiesForUrl(url), request);
    } else {
        nam = std::make_shared<QNetworkAccessManager>();
    }

    auto *reply = nam->get(request);
    bool art = meta.flagSet(MetaFlag::Art);

    QString mime;
    int code = 0;

    {
        QTimer timer;
        timer.setSingleShot(true);
        timer.setInterval(httpTimeout);
        QEventLoop loop;

        auto bytesHandler = [this, &meta] {  // only for type = audio/mp4
            auto *reply = qobject_cast<QNetworkReply *>(sender());
            if (!reply) return;

            if (!reply->isOpen()) return;

            if (QThread::currentThread()->isInterruptionRequested()) {
                qWarning()
                    << "thread interruption was requested => aborting request";
                reply->abort();
                return;
            }

            qDebug() << ">> received bytes for:"
                     << Utils::anonymizedUrl(reply->url())
                     << reply->bytesAvailable();

            if (reply->bytesAvailable() < 100) {
                qDebug() << "no enough bytes yet, ignoring";
                return;
            }

            if (!meta.flagSet(MetaFlag::Mp4AudioNotIsom) &&
                !Transcoder::isMp4Isom(*reply)) {
                qDebug() << "audio/mp4 stream is not isom";
                meta.setFlags(MetaFlag::Mp4AudioNotIsom);
            }

            reply->abort();
        };

        auto metaDataHandler = [art, &mime, &code, this, &bytesHandler] {
            auto *reply = qobject_cast<QNetworkReply *>(sender());
            if (!reply) return;

            if (QThread::currentThread()->isInterruptionRequested()) {
                qWarning()
                    << "thread interruption was requested => aborting request";
                reply->abort();
                return;
            }

            if (reply->rawHeaderPairs().isEmpty()) return;

            code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute)
                       .toInt();
            qDebug() << ">> received http meta data for:" << code
                     << Utils::anonymizedUrl(reply->url());
            qDebug() << "headers:";
            for (const auto &p : reply->rawHeaderPairs()) {
                qDebug() << "  " << p.first << p.second;
            }

            mime = mimeFromReply(reply);

            if (code < 200 || code >= 300) return;

            auto type = typeFromMime(mime);
            if (type == Type::Type_Playlist) {
                qDebug() << "content is a playlist";
                // content is needed, so not aborting
            } else if (type == Type::Type_Image && art) {
                qDebug() << "content is album art";
                // content is needed, so not aborting
            } else if (type == Type::Type_Music &&
                       mime == QStringLiteral("audio/mp4")) {
                // first bytes of content is needed, so connecting byteHandler
                connect(reply, &QNetworkReply::readyRead, this, bytesHandler,
                        Qt::QueuedConnection);
            } else {
                // content is no needed, so aborting
                if (!reply->isFinished()) reply->abort();
            }
        };

        auto con1 = connect(reply, &QNetworkReply::metaDataChanged, this,
                            metaDataHandler, Qt::QueuedConnection);
        auto con2 = connect(
            reply, &QNetworkReply::finished, this, [&loop] { loop.quit(); },
            Qt::QueuedConnection);
        auto con3 = connect(
            &timer, &QTimer::timeout, this,
            [this, &timer] {
                auto *reply = qobject_cast<QNetworkReply *>(sender());
                if (!reply) return;
                qWarning() << "request timeout";
                timer.stop();
                reply->abort();
            },
            Qt::QueuedConnection);

        loop.exec();
        timer.stop();
        disconnect(con1);
        disconnect(con2);
        disconnect(con3);
    }

    if (QThread::currentThread()->isInterruptionRequested()) {
        qWarning() << "thread interruption was requested";
        reply->deleteLater();
        return m_metaCache.end();
    }

    qDebug() << "http request finished:" << code
             << reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute)
                    .toString()
             << Utils::anonymizedUrl(url);

    if (code < 200) {
        qWarning() << "propably http timeout";
        reply->deleteLater();
        return m_metaCache.end();
    }

    if (code >= 300 && code < 400) {
        auto newUrl =
            QUrl{reply->header(QNetworkRequest::LocationHeader).toString()};
        qWarning() << "redirection received:" << code << newUrl;
        if (newUrl.isRelative()) {
            newUrl = url.resolved(newUrl);
            Utils::fixUrl(newUrl);
        }
        reply->deleteLater();
        if (newUrl.isValid())
            return makeItemMetaUsingHTTPRequest2(newUrl, meta, nam,
                                                 counter + 1);

        return m_metaCache.end();
    }

    bool ytdl_broken = false;

    if (code >= 400) {
        if (!meta.flagSet(MetaFlag::Refresh) && meta.flagSet(MetaFlag::YtDl)) {
            qDebug() << "ytdl broken url";
            ytdl_broken = true;
            meta.metaUpdateTime = {};  // null time to set meta as dummy
        } else if (counter == 0 && meta.flagSet(MetaFlag::OrigUrl)) {
            qWarning() << "url is invalid but origurl provided => checking "
                          "origurl instead";
            reply->deleteLater();
            return makeItemMetaUsingHTTPRequest2(meta.origUrl, meta, nam,
                                                 counter + 1);
        } else {
            qWarning() << "unsupported response code:" << reply->error();
            reply->deleteLater();
            return m_metaCache.end();
        }
    }

    if (mime.isEmpty()) {
        qWarning() << "mime is invalid";
        reply->abort();
        reply->deleteLater();
        return m_metaCache.end();
    }

    auto type = typeFromMime(mime);

    if (!meta.flagSet(MetaFlag::YtDl) && type == Type::Type_Playlist) {
        qDebug() << "content is a playlist";

        auto size = reply->bytesAvailable();
        if (size > 0) {
            auto ptype = playlistTypeFromMime(mime);
            auto data = reply->readAll();

            if (hlsPlaylist(data)) {
                qDebug() << "hls playlist";
                meta.url = url;
                meta.mime = mime;
                meta.type = Type::Type_Playlist;
                meta.filename = url.fileName();
                meta.itemType = ItemType_Url;
                meta.setFlags(static_cast<int>(MetaFlag::Valid) |
                              static_cast<int>(MetaFlag::PlaylistProxy));
                meta.setFlags(static_cast<int>(MetaFlag::Local) |
                                  static_cast<int>(MetaFlag::Seek),
                              false);
                reply->deleteLater();
                return m_metaCache.insert(url, meta);
            }

            auto items = ptype == PlaylistType::PLS
                             ? parsePls(data, reply->url().toString())
                         : ptype == PlaylistType::XSPF
                             ? parseXspf(data, reply->url().toString())
                             : parseM3u(data, reply->url().toString());
            if (!items.isEmpty()) {
                QUrl url = items.first().url;
                Utils::fixUrl(url);
                qDebug()
                    << "trying get meta data for first item in the playlist:"
                    << Utils::anonymizedUrl(url);
                reply->deleteLater();
                return makeItemMetaUsingHTTPRequest2(url, meta, nam,
                                                     counter + 1);
            }
        }

        qWarning() << "playlist content is empty";
        reply->deleteLater();
        return m_metaCache.end();
    }

    if (!ytdl_broken && type != Type::Type_Music && type != Type::Type_Video &&
        type != Type::Type_Image) {
        return makeItemMetaUsingApi(mime, reply, meta, nam, counter);
    }

    bool ranges = QString{reply->rawHeader("Accept-Ranges")}.toLower().contains(
        QStringLiteral("bytes"));
    int size = reply->header(QNetworkRequest::ContentLengthHeader).toInt();

    meta.url = reply->url();
    if (meta.origUrl.isEmpty() && url != meta.url) meta.origUrl = url;
#ifdef QT_DEBUG
    if (url != meta.url) {
        qDebug() << "url change:" << url.toString() << "->"
                 << meta.url.toString() << "(orig:" << meta.origUrl.toString()
                 << ")";
    }
#endif
    meta.mime = ytdl_broken ? QString{} : mime;
    meta.type = type;
    meta.size = ranges ? size : 0;
    meta.filename = url.fileName();
    meta.itemType = ItemType_Url;
    meta.setFlags(MetaFlag::Valid);
    meta.setFlags(MetaFlag::Local, false);
    meta.setFlags(MetaFlag::Seek, size > 0 && ranges);

    if (type == Type::Type_Image && meta.flagSet(MetaFlag::Art)) {
        saveAlbumArt(*reply, meta);
    } else {
        updateMetaAlbumArt(meta);
        updateMetaWithNotStandardHeaders(meta, *reply);
        updateMetaIfCached(meta);
    }

    if (meta.title.isEmpty()) {
        meta.title = url.fileName();
    }

    reply->deleteLater();

    return m_metaCache.insert(meta.url, meta);
}

QHash<QUrl, ContentServer::ItemMeta>::iterator
ContentServer::makeItemMetaUsingApi(const QString &mime, QNetworkReply *reply,
                                    ItemMeta &meta,
                                    std::shared_ptr<QNetworkAccessManager> nam,
                                    int counter) {
    reply->deleteLater();

    if (mime.contains(QStringLiteral("text/html"))) {
        auto url = reply->url();
        if (meta.app == QStringLiteral("bc") || BcApi::validUrl(url)) {
            return makeItemMetaUsingBcApi(url, meta, nam, counter);
        }
        if (meta.app == QStringLiteral("soundcloud") ||
            SoundcloudApi::validUrl(url)) {
            return makeItemMetaUsingSoundcloudApi(url, meta, nam, counter);
        }
        return makeItemMetaUsingYtdlApi(url, meta, nam, counter);
    }

    qWarning() << "unsupported type:" << mime;
    return m_metaCache.end();
}

void ContentServer::saveAlbumArt(QNetworkReply &reply, ItemMeta &meta) {
    qDebug() << "saving album art";

    auto path = Thumb::save(reply.readAll(), meta.url,
                                 m_imgMimeToExtMap.value(meta.mime));
    if (!path) {
        qWarning() << "cannot save album art";
        return;
    }

    meta.path = std::move(*path);

    qDebug() << "album art saved:" << meta.path;
}

void ContentServer::updateMetaWithNotStandardHeaders(
    ItemMeta &meta, const QNetworkReply &reply) {
    if (reply.hasRawHeader(QByteArrayLiteral("icy-metaint")) &&
        reply.rawHeader(QByteArrayLiteral("icy-metaint")).toInt() > 0) {
        qDebug() << "shoutcast stream detected";
        meta.setFlags(MetaFlag::IceCast);
    }

    if (meta.title.isEmpty() &&
        reply.hasRawHeader(QByteArrayLiteral("icy-name"))) {
        meta.title = QString{reply.rawHeader(QByteArrayLiteral("icy-name"))};
    }

    if (reply.hasRawHeader(QByteArrayLiteral("x-amz-meta-bitrate"))) {
        meta.bitrate =
            reply.rawHeader(QByteArrayLiteral("x-amz-meta-bitrate")).toInt() *
            1000;
    }

    if (meta.duration == 0 &&
        reply.hasRawHeader(QByteArrayLiteral("x-amz-meta-duration"))) {
        meta.duration =
            reply.rawHeader(QByteArrayLiteral("x-amz-meta-duration")).toInt() /
            1000;
    }
}

void ContentServer::updateMetaAlbumArt(ItemMeta &meta) const {
    if (meta.albumArt.isEmpty()) {
        if (meta.app == QStringLiteral("bc"))
            meta.albumArt =
                IconProvider::pathToNoResId(QStringLiteral("icon-bandcamp"));
        else if (meta.app == QStringLiteral("somafm"))
            meta.albumArt =
                IconProvider::pathToNoResId(QStringLiteral("icon-somafm"));
        else if (meta.app == QStringLiteral("icecast"))
            meta.albumArt =
                IconProvider::pathToNoResId(QStringLiteral("icon-icecast"));
        else if (meta.app == QStringLiteral("fosdem"))
            meta.albumArt =
                IconProvider::pathToNoResId(QStringLiteral("icon-fosdem"));
        else if (meta.app == QStringLiteral("gpodder"))
            meta.albumArt =
                IconProvider::pathToNoResId(QStringLiteral("icon-gpodder"));
        else if (meta.app == QStringLiteral("tunein"))
            meta.albumArt =
                IconProvider::pathToNoResId(QStringLiteral("icon-tunein"));
        else if (BcApi::validUrl(meta.origUrl) || BcApi::validUrl(meta.url))
            meta.albumArt =
                IconProvider::pathToNoResId(QStringLiteral("icon-bandcamp"));
        else if (meta.origUrl.host().contains(QStringLiteral("youtube.")) ||
                 meta.origUrl.host().contains(QStringLiteral("yout.be")))
            meta.albumArt =
                IconProvider::pathToNoResId(QStringLiteral("icon-youtube"));
        else if (meta.origUrl.host().contains(QStringLiteral("soundcloud.com")))
            meta.albumArt =
                IconProvider::pathToNoResId(QStringLiteral("icon-soundcloud"));
        else if (meta.origUrl.host().contains(QStringLiteral("somafm.com")) ||
                 meta.url.host().contains(QStringLiteral("somafm.com")))
            meta.albumArt =
                IconProvider::pathToNoResId(QStringLiteral("icon-somafm"));
    }
}

QHash<QUrl, ContentServer::ItemMeta>::iterator
ContentServer::makeMetaUsingExtension(const QUrl &url) {
    bool isFile = url.isLocalFile();
    bool isQrc = url.scheme() == QStringLiteral("qrc");

    ItemMeta meta;
    meta.path = isFile  ? url.toLocalFile()
                : isQrc ? (":" + Utils::urlFromId(url).path())
                        : QString{};
    meta.url = url;
    meta.mime = getContentMimeByExtension(url);
    meta.type = getContentTypeByExtension(url);
    meta.size = 0;
    meta.filename = url.fileName();
    meta.itemType = itemTypeFromUrl(url);
    meta.setFlags(MetaFlag::Valid);
    meta.setFlags(
        static_cast<int>(MetaFlag::Local) | static_cast<int>(MetaFlag::Seek),
        isFile || isQrc);

    return m_metaCache.insert(url, meta);
}

QHash<QUrl, ContentServer::ItemMeta>::iterator ContentServer::makeItemMeta(
    const QUrl &url, const QUrl &origUrl, const QString &app, bool ytdl,
    bool art, bool refresh, Type type) {
    m_metaCacheMutex.lock();

    QHash<QUrl, ContentServer::ItemMeta>::iterator it;
    auto itemType = itemTypeFromUrl(url);
    if (itemType == ItemType_LocalFile) {
        if (art) {
            it = makeMetaUsingExtension(url);
        } else {
#ifdef SAILFISH
            it = makeItemMetaUsingTracker(url);
            if (it == m_metaCache.end()) {
                qWarning()
                    << "cannot get meta using tacker, so fallbacking to taglib";
                it = makeItemMetaUsingTaglib(url);
            }
#else
            it = makeItemMetaUsingTaglib(url);
#endif
        }
    } else if (itemType == ItemType_Mic) {
        qDebug() << "mic url detected";
        it = makeMicItemMeta(url);
    } else if (itemType == ItemType_AudioCapture) {
        qDebug() << "pulse url detected";
        it = makeAudioCaptureItemMeta(url);
    } else if (itemType == ItemType_ScreenCapture) {
        qDebug() << "screen url detected";
        if (Settings::instance()->getScreenSupported()) {
            it = makeScreenCaptureItemMeta(url);
        } else {
            qWarning() << "screen capturing is not supported";
            it = m_metaCache.end();
        }
    } else if (itemType == ItemType_Upnp) {
        qDebug() << "upnp url detected";
        it = makeUpnpItemMeta(url);
    } else if (itemType == ItemType_Url) {
        qDebug() << "geting meta using http request";
        it = makeItemMetaUsingHTTPRequest(url, origUrl, app, ytdl, refresh, art,
                                          type);
    } else if (url.scheme() == QStringLiteral("jupii")) {
        qDebug() << "unsupported jupii url detected";
        it = m_metaCache.end();
    } else {
        qDebug() << "unsupported url type";
        it = m_metaCache.end();
    }

    m_metaCacheMutex.unlock();
    return it;
}

void ContentServer::run() {
    auto *worker = ContentServerWorker::instance();
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
    connect(this, &ContentServer::startProxyRequested, worker,
            &ContentServerWorker::startProxy);
    QThread::exec();
}

static inline QString removeExtension(const QString &fileName) {
    auto idx = fileName.lastIndexOf('.');
    if (idx == -1) return fileName;
    return fileName.left(idx);
}

bool ContentServer::writeTagsWithMeta(const QString &path, const ItemMeta &meta,
                                      const QUrl &id) {
    auto url = meta.origUrl.isEmpty() ? meta.url : meta.origUrl;
    auto icon = localArtPathIfExists(Utils::iconFromId(id).toString());

    if (!writeMetaUsingTaglib(path, true, meta.title, meta.artist, meta.album,
                              meta.comment, url.toString(),
                              QDateTime::currentDateTime(),
                              icon.isEmpty() ? meta.albumArt : icon,
                              /*otype=*/meta.type)) {
        qWarning() << "cannot write meta using taglib";
        return false;
    }

    return true;
}

void ContentServer::setCachingState(bool state) {
    if (state != m_caching) {
        m_caching = state;
        emit cachingChanged();
    }
}

std::optional<QString> ContentServer::cacheContent(const ItemMeta &meta) {
    qDebug() << "downloading content:" << meta.size << "bytes";

    auto path = pathToCachedContent(meta, false);

    if (m_cacheDownloader)
        m_cacheDownloader->reset();
    else
        m_cacheDownloader.emplace();

    if (path) {
        updateCacheProgressString();
        auto conn = connect(
            &m_cacheDownloader.value(), &Downloader::progressChanged, this,
            [this]() { updateCacheProgressString(); }, Qt::QueuedConnection);

        if (m_cacheDownloader->downloadToFile(meta.url, *path, true)) {
            qDebug() << "content successfully cached";
            disconnect(conn);
            updateCacheProgressString();
            return path;
        }

        disconnect(conn);
        updateCacheProgressString();
    }

    qWarning() << "cannot cache content";
    return std::nullopt;
}

std::optional<AvMeta> ContentServer::transcodeToAudioFile(const QString &path) {
    auto avMeta = Transcoder::extractAudio(path, path + ".tmp");
    if (!avMeta) {
        qWarning() << "cannot transcode file:" << path;
        return std::nullopt;
    }

    auto outFile = removeExtension(path) + '.' + avMeta->extension;
    QFile::remove(outFile);

    if (!QFile::copy(avMeta->path, outFile)) {
        QFile::remove(avMeta->path);
        return std::nullopt;
    }

    QFile::remove(avMeta->path);

    qDebug() << "content successfully transcoded:" << avMeta->type << path
             << "->" << outFile;

    avMeta->path = std::move(outFile);

    return avMeta;
}

void ContentServer::updateMetaIfCached(ItemMeta &meta, Type type,
                                       const QString &cachedPath) {
    QString path;
    if (cachedPath.isEmpty()) {
        if (auto p = pathToCachedContent(meta, true, type)) {
            path = *p;
        } else {
            return;  // file not cached
        }
    }

    meta.setFlags(MetaFlag::Seek);
    if (!readMetaUsingTaglib(path.isEmpty() ? cachedPath : path, nullptr,
                             nullptr, nullptr, nullptr, nullptr, nullptr,
                             nullptr, /*otype=*/nullptr, &meta.duration,
                             &meta.bitrate, &meta.sampleRate, &meta.channels)) {
        qWarning() << "cannot read meta with taglib";
    }
}

int64_t ContentServer::cachePendingSizeForId(const QUrl &id) {
    if (id.isEmpty()) return 0;
    auto *meta = getMetaForId(id, false);
    if (!meta) return 0;
    if (meta->size == 0) return 0;
    if (pathToCachedContent(*meta, true,
                            static_cast<Type>(Utils::typeFromId(id)))) {
        return 0;
    }
    return meta->size;
}

ContentServer::CachingResult ContentServer::makeCache(const QUrl &id) {
    qDebug() << "make cache:" << Utils::anonymizedId(id);

    auto *meta = getMetaForId(id, false);
    if (!meta) return CachingResult::NotCachedError;

    if (meta->size == 0) return CachingResult::NotCached;

    auto type = static_cast<Type>(Utils::typeFromId(id));

    if (pathToCachedContent(*meta, true, type)) {
        qDebug() << "content already cached";
        return CachingResult::Cached;
    }

    if (meta->flagSet(MetaFlag::Mp4AudioNotIsom)) {
        qDebug() << "audio/mp4 not isom content => pre-caching forced";

        auto cachePath = cacheContent(*meta);
        if (!cachePath) {
            if (m_cacheDownloader && m_cacheDownloader->canceled())
                return CachingResult::NotCachedCanceled;
            return Settings::instance()->allowNotIsomMp4()
                       ? CachingResult::NotCached
                       : CachingResult::NotCachedError;
        }

        auto avMeta = transcodeToAudioFile(*cachePath);
        if (!avMeta) {
            QFile::remove(*cachePath);
            return Settings::instance()->allowNotIsomMp4()
                       ? CachingResult::NotCached
                       : CachingResult::NotCachedError;
        }

        if (*cachePath != avMeta->path) QFile::remove(*cachePath);

        if (!writeTagsWithMeta(avMeta->path, *meta, id)) {
            QFile::remove(avMeta->path);
            return Settings::instance()->allowNotIsomMp4()
                       ? CachingResult::NotCached
                       : CachingResult::NotCachedError;
        }

        updateMetaIfCached(*meta, type, avMeta->path);

        return CachingResult::Cached;
    }

    if (meta->type == Type::Type_Video && type == Type::Type_Music) {
        qDebug() << "video content but audio requested => pre-caching forced";

        auto cachePath = pathToCachedContent(*meta, true, Type::Type_Video);
        if (!cachePath) cachePath = cacheContent(*meta);
        if (!cachePath) {
            if (m_cacheDownloader && m_cacheDownloader->canceled())
                return CachingResult::NotCachedCanceled;
            return CachingResult::NotCachedError;
        }

        auto avMeta = transcodeToAudioFile(*cachePath);
        if (!avMeta) {
            qWarning() << "cannot extract audio";
            QFile::remove(*cachePath);
            return CachingResult::NotCachedError;
        }

        if (!writeTagsWithMeta(avMeta->path, *meta, id)) {
            QFile::remove(*cachePath);
            QFile::remove(avMeta->path);
            return CachingResult::NotCachedError;
        }

        meta->audioAvMeta = std::move(avMeta);

        writeTagsWithMeta(*cachePath, *meta, id);

        updateMetaIfCached(*meta, type, *cachePath);

        return CachingResult::Cached;
    }

    bool doCaching = false;

    if (meta->size <= maxSizeForCaching) {
        doCaching = true;
        qDebug() << "file is small => pre-caching forced:" << meta->size
                 << "bytes";
    } else if (Settings::instance()->cacheType() ==
               Settings::CacheType::Cache_Always) {
        doCaching = true;
        qDebug() << "pre-caching enabled for all content";
    }

    if (doCaching) {
        auto cachePath = cacheContent(*meta);
        if (!cachePath) {
            if (m_cacheDownloader && m_cacheDownloader->canceled())
                return CachingResult::NotCachedCanceled;
            return CachingResult::NotCached;
        }

        if (!writeTagsWithMeta(*cachePath, *meta, id) &&
            type == Type::Type_Music) {
            QFile::remove(*cachePath);
            return CachingResult::NotCached;
        }

        updateMetaIfCached(*meta, type, *cachePath);

        return CachingResult::Cached;
    }

    return CachingResult::NotCached;
}

std::pair<ContentServer::CachingResult, ContentServer::CachingResult>
ContentServer::makeCache(const QUrl &id1, const QUrl &id2) {
    auto cacheType = Settings::instance()->cacheType();

    qDebug() << "make cache requested: cache type=" << cacheType
             << "id1=" << Utils::anonymizedId(id1)
             << "id2=" << Utils::anonymizedId(id2);

    if (cacheType == Settings::CacheType::Cache_Never)
        return {CachingResult::NotCached, CachingResult::NotCached};

    std::pair<CachingResult, CachingResult> result{CachingResult::Invalid,
                                                   CachingResult::Invalid};

    setCachingState(true);
    updateCacheProgressString(true);

    if (!id1.isEmpty()) {
        m_cacheDoneSize = 0;
        m_cachePendingSize = cachePendingSizeForId(id2);
        auto sizeId1 = cachePendingSizeForId(id1);
        result.first = makeCache(id1);
        if (result.first == CachingResult::NotCachedCanceled ||
            result.first == CachingResult::NotCachedError) {
            setCachingState(false);
            m_cacheDoneSize = 0;
            m_cachePendingSize = 0;
            return result;
        }
        m_cacheDoneSize = sizeId1;
        m_cachePendingSize = sizeId1;
    }
    if (!id2.isEmpty()) {
        result.second = makeCache(id2);
    }

    setCachingState(false);
    m_cacheDoneSize = 0;
    m_cachePendingSize = 0;

    return result;
}

ContentServer::ProxyError ContentServer::startProxy(const QUrl &id) {
    qDebug() << "start proxy requested";

    QEventLoop loop;
    QMetaObject::Connection connOk, connErr;
    const auto ok = [&](const QUrl &pid) {
        if (pid == id) {
            QObject::disconnect(connErr);
            QObject::disconnect(connOk);
            loop.quit();
        }
    };
    const auto err = [&](const QUrl &pid) {
        if (pid == id) {
            QObject::disconnect(connErr);
            QObject::disconnect(connOk);
            loop.exit(1);
        }
    };
    connOk = connect(ContentServerWorker::instance(),
                     &ContentServerWorker::proxyConnected, this, ok,
                     Qt::QueuedConnection);
    connErr = connect(ContentServerWorker::instance(),
                      &ContentServerWorker::proxyError, this, err,
                      Qt::QueuedConnection);
    emit startProxyRequested(id);
    const auto ret = loop.exec();

    if (ret == 0)
        qDebug() << "proxy started";
    else
        qDebug() << "proxy error";

    return ret == 0 ? ProxyError::NoError : ProxyError::Error;
}

QString ContentServer::streamTitle(const QUrl &id) const {
    if (m_streams.contains(id)) {
        return m_streams.value(id).title;
    }

    return {};
}

QStringList ContentServer::streamTitleHistory(const QUrl &id) const {
    if (m_streams.contains(id)) return m_streams.value(id).titleHistory;
    return {};
}

bool ContentServer::isStreamToRecord(const QUrl &id) const {
    if (id.isEmpty()) return false;
    return m_streamToRecord.contains(id);
}

bool ContentServer::isStreamRecordable(const QUrl &id) const {
    if (id.isEmpty()) return false;
    if (m_tmpRecs.contains(id)) return true;
    return m_streamRecordable.contains(id);
}

bool ContentServer::saveTmpRec(const QString &path, bool deletePath) {
    const auto title = readTitleUsingTaglib(path);
    const auto ext = path.split('.').last();

    if (!title.isEmpty() && !ext.isEmpty()) {
        auto recFilePath = QDir{Settings::instance()->getRecDir()}.filePath(
            QStringLiteral("%1.%2.%3.%4")
                .arg(Utils::escapeName(title), Utils::instance()->randString(),
                     "jupii_rec", ext));  // without extension

        if (ext == QStringLiteral("m4a") && !Transcoder::isMp4Isom(path)) {
            auto avMeta = Transcoder::extractAudio(path, recFilePath);
            if (avMeta) {
                writeRecTags(avMeta->path);
                qDebug() << "saving rec file:" << path << avMeta->path;
            } else {
                qWarning() << "cannot transcode file:" << path;
            }
        } else {
            qDebug() << "saving rec file:" << path << recFilePath;
            if (QFile::copy(path, recFilePath)) {
                writeRecTags(recFilePath);
                if (deletePath) QFile::remove(path);
                emit streamRecorded(title, recFilePath);
                return true;
            }
            qWarning() << "cannot copy:" << path << recFilePath;
        }
    }

    if (deletePath) QFile::remove(path);
    emit streamRecordError(title);
    return false;
}

void ContentServer::cleanTmpRec() {
    const auto id = QUrl{PlaylistModel::instance()->activeId()};

    auto i = m_tmpRecs.begin();
    while (i != m_tmpRecs.end()) {
        if (i.key() != id && !pathIsCachedFile(i.value())) {
#ifdef QT_DEBUG
            qDebug() << "deleting tmp rec file:" << i.value();
#endif
            QFile::remove(i.value());
            i = m_tmpRecs.erase(i);
        } else {
            ++i;
        }
    }
}

void ContentServer::setStreamToRecord(const QUrl &id, bool value) {
    if (m_tmpRecs.contains(id)) {
        auto file = m_tmpRecs.take(id);
        bool cached = pathIsCachedFile(file);
        qDebug() << "stream already recorded:" << Utils::anonymizedUrl(id)
                 << value << file << cached;
        if (value) {
            saveTmpRec(file, !cached);
        } else if (!cached) {
            QFile::remove(file);
        }
        streamToRecordChangedHandler(id, false);
        emit streamRecordableChanged(id, false);
    } else {
        emit requestStreamToRecord(id, value);
    }
}

void ContentServer::streamToRecordChangedHandler(const QUrl &id, bool value) {
    if (m_streamToRecord.contains(id)) {
        if (!value) {
            m_streamToRecord.remove(id);
            emit streamToRecordChanged(id, false);
        }
    } else {
        if (value) {
            m_streamToRecord.insert(id);
            emit streamToRecordChanged(id, true);
        }
    }
}

void ContentServer::streamRecordableChangedHandler(const QUrl &id, bool value,
                                                   const QString &tmpFile) {
    if (pathIsCachedFile(tmpFile)) {
        if (value)
            m_tmpRecs.insert(id, tmpFile);
        else
            m_tmpRecs.remove(id);
        emit streamRecordableChanged(id, value);
        return;
    }

    if (tmpFile.isEmpty()) {
        if (m_tmpRecs.contains(id)) QFile::remove(m_tmpRecs.value(id));
        m_tmpRecs.remove(id);
        if (value)
            m_streamRecordable.insert(id);
        else
            m_streamRecordable.remove(id);
        emit streamRecordableChanged(id, value);
    } else {
        if (m_tmpRecs.contains(id)) QFile::remove(m_tmpRecs.value(id));
        m_tmpRecs.insert(id, tmpFile);
        m_streamRecordable.remove(id);
        emit streamRecordableChanged(id, true);
    }
}

void ContentServer::streamRecordedHandler(const QString &title,
                                          const QString &path) {
    emit streamRecorded(title, path);
}

void ContentServer::itemAddedHandler(const QUrl &id) {
    qDebug() << "new item for id:" << Utils::anonymizedUrl(id);
    auto &stream = m_streams[id];
    stream.count++;
    stream.id = id;
}

void ContentServer::itemRemovedHandler(const QUrl &id) {
    qDebug() << "item removed for id:" << Utils::anonymizedUrl(id);
    auto &stream = m_streams[id];
    stream.count--;
    if (stream.count < 1) {
        m_streams.remove(id);
        emit streamTitleChanged(id, {});
    }
}

void ContentServer::pulseStreamNameHandler(const QUrl &id,
                                           const QString &name) {
    qDebug() << "pulse-audio stream name updated:" << Utils::anonymizedUrl(id)
             << name;

    auto &stream = m_streams[id];
    stream.id = id;
    stream.title = name;
    emit streamTitleChanged(id, name);
}

QString ContentServer::streamTitleFromShoutcastMetadata(
    const QByteArray &metadata) {
    auto data = QString::fromUtf8(metadata);
    data.remove('\'').remove('\"');
    static QRegExp rx(QStringLiteral("StreamTitle=([^;]*)"),
                      Qt::CaseInsensitive);
    int pos = 0;
    QString title;
    while ((pos = rx.indexIn(data, pos)) != -1) {
        title = rx.cap(1)
                    .remove('$')
                    .remove('%')
                    .remove('*')
                    .remove('&')
                    .remove('?')
                    .remove('.')
                    .remove('/')
                    .remove('\\')
                    .remove(',')
                    .remove('+')
                    .trimmed();
        if (!title.isEmpty()) {
            qDebug() << "stream title:" << title;
            break;
        }
        pos += rx.matchedLength();
    }

    return title;
}

void ContentServer::shoutcastMetadataHandler(const QUrl &id,
                                             const QByteArray &metadata) {
    qDebug() << "shoutcast metadata:" << metadata;

    auto new_title = streamTitleFromShoutcastMetadata(metadata);
    auto &stream = m_streams[id];
    stream.id = id;
    stream.title = new_title;

    if (new_title.isEmpty()) {
        stream.titleHistory.clear();
    } else if (stream.titleHistory.isEmpty() ||
               stream.titleHistory.first() != new_title) {
        stream.titleHistory.push_front(new_title);
        if (stream.titleHistory.length() >
            20)  // max number of titiles in history list
            stream.titleHistory.removeLast();
    }

    emit streamTitleChanged(id, stream.title);
}

void ContentServer::cleanCache() {
    Utils::removeFromCacheDir(QStringList{"cache_*.*"});
    emit cacheSizeChanged();
}

void ContentServer::cancelCaching() {
    if (m_cacheDownloader) m_cacheDownloader->cancel();
}

double ContentServer::cacheProgress() const {
    if (m_cacheDownloader) {
        auto [bytesReceived, bytesTotal] = m_cacheDownloader->progress();
        if (bytesTotal > 0) {
            bytesReceived += m_cacheDoneSize;
            bytesTotal += m_cachePendingSize;
            return static_cast<double>(bytesReceived) /
                   static_cast<double>(bytesTotal);
        }
    }

    return 0.0;
}

void ContentServer::updateCacheProgressString(bool toZero) {
    QString newStr;
    if (toZero) {
        newStr = QStringLiteral("0%");
    } else if (auto p = cacheProgress(); p > 0.0) {
        newStr = QString::number(static_cast<int>(p * 100)) + '%';
    }

    if (newStr.isEmpty()) return;

    if (newStr != m_cacheProgressString) {
        m_cacheProgressString = std::move(newStr);
        // qDebug() << "cache progress:" << m_cacheProgressString;
        emit cacheProgressChanged();
    }
}

QString ContentServer::cacheSizeString() const {
    auto files = QDir{Settings::instance()->getCacheDir()}.entryInfoList(
        QStringList{"cache_*.*"}, QDir::Files);
    return Utils::humanFriendlySizeString(std::accumulate(
        files.cbegin(), files.cend(), 0,
        [](int64_t size, const QFileInfo &f) { return size + f.size(); }));
}

bool ContentServer::pathIsCachedFile(const QString &path) {
    return QFileInfo{path}.fileName().startsWith(QStringLiteral("cache_"));
}

std::optional<QString> ContentServer::pathToCachedContent(const ItemMeta &meta,
                                                          bool mustExists,
                                                          Type type) {
    auto url = meta.origUrl.isEmpty() ? meta.url : meta.origUrl;
    auto path =
        QFileInfo{contentCachePath(url.toString(), QStringLiteral("*"))};
    auto dir = path.absoluteDir();
    auto allFiles = dir.entryList(QStringList{path.fileName()}, QDir::Files);

    if (type == Type::Type_Unknown) type = meta.type;

    auto filesOfType = [&allFiles](Type type) {
        QStringList list;
        auto exts = extensionsForType(type);
        for (const auto &f : allFiles) {
            if (auto ext = extFromPath(f); exts.contains(*ext)) {
                list.push_back(f);
            }
        }
        return list;
    };

    QStringList files;
    if (type == Type::Type_Unknown)
        files << filesOfType(Type::Type_Video) << filesOfType(Type::Type_Music)
              << filesOfType(Type::Type_Image);
    else if (type == Type::Type_Music || type == Type::Type_Video ||
             type == Type::Type_Image)
        files << filesOfType(type);
    else
        return std::nullopt;

#ifdef QT_DEBUG
    qDebug() << "type:" << type;
    qDebug() << "cached files:" << files;
#endif

    if (!files.isEmpty()) {
        files.erase(
            std::remove_if(files.begin(), files.end(),
                           [](const auto &file) {
                               auto otype = readOtypeFromCachedFile(file);
                               /*qDebug() << "otype:" << file << otype
                                        << getContentTypeByExtension(file);*/
                               return otype == Type::Type_Invalid ||
                                      otype != getContentTypeByExtension(file);
                           }),
            files.end());

#ifdef QT_DEBUG
        qDebug() << "cached files after filtering:" << files;
#endif
    }

    if (files.isEmpty()) {
        if (mustExists) return std::nullopt;
        auto ext = extFromMime(meta.mime);
        if (ext.isEmpty()) {
            qWarning() << "unsupported mime for caching" << meta.mime;
            return std::nullopt;
        }
        return contentCachePath(url.toString(), ext);
    }

    qDebug() << "found cached file:" << files.first();
    return dir.absoluteFilePath(files.first());
}

QString ContentServer::localArtPathIfExists(const QString &artPath) {
    if (artPath.isEmpty()) return {};
    if (QFileInfo::exists(artPath)) return artPath;
    if (artPath.startsWith(QStringLiteral("qrc:")))
        return artPath.right(artPath.size() - 3);
    if (auto file = QUrl{artPath}.toLocalFile(); QFileInfo::exists(file))
        return file;
    if (auto *artMeta = ContentServer::instance()->getMeta(
            QUrl{artPath}, false, {}, {}, false, false, false,
            Type::Type_Image);
        artMeta != nullptr) {
        return artMeta->path;
    }
    return {};
}

QUrl ContentServer::artUrl(const QString &artPath) {
    if (artPath.isEmpty()) return {};
    if (artPath.startsWith(QStringLiteral(":"))) return QUrl{"qrc" + artPath};
    if (artPath.startsWith(QStringLiteral("qrc:"))) return QUrl{artPath};
    if (QFileInfo::exists(artPath)) return QUrl::fromLocalFile(artPath);
    return QUrl{artPath};
}

void ContentServer::writeRecTags(const QString &path) {
    writeMetaUsingTaglib(path, false, {}, {}, recAlbumName, {}, {},
                         QDateTime::currentDateTime(), {});
}

bool ContentServer::idCached(const QUrl &id) {
    auto *meta = getMetaForId(id, false);
    if (!meta) return false;
    if (meta->size == 0) return false;
    if (meta->itemType != ItemType::ItemType_Url) return false;

    auto type = static_cast<Type>(Utils::typeFromId(id));
    return static_cast<bool>(pathToCachedContent(*meta, true, type));
}

QStringList ContentServer::unusedCachedFiles() const {
    auto allFiles = QDir{Settings::instance()->getCacheDir()}.entryList(
        QStringList{"cache_*.*"}, QDir::Files);

    if (allFiles.isEmpty()) return allFiles;

    QStringList usedFiles{};
    usedFiles.reserve(allFiles.size());
    for (const auto &meta : m_metaCache) {
        if (auto path = pathToCachedContent(meta, true, meta.type)) {
            usedFiles.push_back(QFileInfo{*path}.fileName());
        }
    }

    allFiles.erase(std::remove_if(allFiles.begin(), allFiles.end(),
                                  [&](const auto &path) {
                                      return usedFiles.contains(path);
                                  }),
                   allFiles.end());

    qDebug() << "unused cached files:" << allFiles;

    return allFiles;
}

QStringList ContentServer::unusedArtFiles() const {
    auto allFiles = QDir{Settings::instance()->getCacheDir()}.entryList(
        QStringList{"art_image_*.*"}, QDir::Files);

    if (allFiles.isEmpty()) return allFiles;

    QStringList usedFiles{};
    for (const auto &meta : m_metaCache) {
        if (!meta.albumArt.isEmpty()) {
            auto fn = QFileInfo{meta.albumArt}.fileName();
            if (allFiles.contains(fn)) {
                usedFiles.push_back(fn);
                continue;
            }
        }
        if (!meta.path.isEmpty()) {
            auto fn = QFileInfo{meta.path}.fileName();
            if (allFiles.contains(fn)) usedFiles.push_back(fn);
        }
    }

    allFiles.erase(std::remove_if(allFiles.begin(), allFiles.end(),
                                  [&](const auto &path) {
                                      return usedFiles.contains(path);
                                  }),
                   allFiles.end());

    qDebug() << "unused art files:" << allFiles;

    return allFiles;
}

void ContentServer::cleanCacheFiles(bool force) {
    if (!force && Settings::instance()->cacheCleaningType() ==
                      Settings::CacheCleaningType::CacheCleaning_Always)
        return;

    QStringList filters{"rec-*.*", "passlogfile-*.*", "tmp_*.*"};

    if (force) {
        filters.append({"extracted_audio_*.*", "cache_*.*", "art_image_*.*"});
    } else if (Settings::instance()->cacheCleaningType() ==
               Settings::CacheCleaningType::CacheCleaning_Auto) {
        filters.append(QStringLiteral("extracted_audio_*.*"));
        filters.append(ContentServer::instance()->unusedCachedFiles());
        filters.append(ContentServer::instance()->unusedArtFiles());
    }

    Utils::removeFromCacheDir(filters);
}
