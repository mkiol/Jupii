/* Copyright (C) 2022 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "ytdlapi.h"

#ifdef USE_SFOS
#include <archive.h>
#include <archive_entry.h>
#include <lzma.h>
#include <zlib.h>
#endif
#undef slots
#include <pybind11/embed.h>
#include <pybind11/pytypes.h>
#include <pybind11/stl.h>
#define slots Q_SLOTS
#include <stdlib.h>

#include <QDebug>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QIODevice>
#include <QStandardPaths>
#include <QThread>
#include <QTimer>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

#ifdef USE_SFOS
#include "config.h"
#include "downloader.h"
#include "settings.h"
#include "utils.h"
#endif

#include "utils.h"

const int YtdlApi::maxHome = 50;
const int YtdlApi::maxHomeFirstPage = 2;
std::vector<home::Section> YtdlApi::m_homeSections{};

YtdlApi::State YtdlApi::state = YtdlApi::State::Unknown;

#ifdef USE_SFOS
const QString YtdlApi::pythonArchivePath{
    QStringLiteral("/usr/share/%1/lib/python.tar.xz").arg(APP_BINARY_ID)};

QString YtdlApi::pythonSitePath() {
    return QStandardPaths::writableLocation(QStandardPaths::DataLocation) +
           "/python3.8/site-packages";
}

QString YtdlApi::pythonUnpackPath() {
    return QStandardPaths::writableLocation(QStandardPaths::DataLocation);
}

static QString make_checksum(const QString& file) {
    QString hex;

    if (std::ifstream input{file.toStdString(),
                            std::ios::in | std::ifstream::binary}) {
        auto checksum = crc32(0L, Z_NULL, 0);
        char buff[std::numeric_limits<unsigned short>::max()];
        while (input) {
            input.read(buff, sizeof buff);
            checksum = crc32(checksum, reinterpret_cast<unsigned char*>(buff),
                             static_cast<unsigned int>(input.gcount()));
        }
        std::stringstream ss;
        ss << std::hex << checksum;
        hex = QString::fromStdString(ss.str());
        qDebug() << "crc checksum:" << hex << file;
        return hex;
    }
    qWarning() << "cannot open file:" << file;

    return hex;
}

static bool xz_decode(const QString& file_in, const QString& file_out) {
    qDebug() << "extracting xz archive:" << file_in << file_out;

    if (std::ifstream input{file_in.toStdString(),
                            std::ios::in | std::ifstream::binary}) {
        if (std::ofstream output{file_out.toStdString(),
                                 std::ios::out | std::ifstream::binary}) {
            lzma_stream strm = LZMA_STREAM_INIT;
            if (lzma_ret ret = lzma_stream_decoder(&strm, UINT64_MAX, 0);
                ret != LZMA_OK) {
                qWarning() << "error initializing the xz decoder:" << ret;
                return false;
            }

            lzma_action action = LZMA_RUN;

            char buff_out[std::numeric_limits<unsigned short>::max()];
            char buff_in[std::numeric_limits<unsigned short>::max()];

            strm.next_in = reinterpret_cast<uint8_t*>(buff_in);
            strm.next_out = reinterpret_cast<uint8_t*>(buff_out);
            strm.avail_out = sizeof buff_out;
            strm.avail_in = 0;

            while (true) {
                if (strm.avail_in == 0 && input) {
                    strm.next_in = reinterpret_cast<uint8_t*>(buff_in);
                    input.read(buff_in, sizeof buff_in);
                    strm.avail_in = input.gcount();
                }

                if (!input) action = LZMA_FINISH;

                auto ret = lzma_code(&strm, action);

                if (strm.avail_out == 0 || ret == LZMA_STREAM_END) {
                    output.write(buff_out, sizeof buff_out - strm.avail_out);

                    strm.next_out = reinterpret_cast<uint8_t*>(buff_out);
                    strm.avail_out = sizeof buff_out;
                }

                if (ret == LZMA_STREAM_END) break;

                if (ret != LZMA_OK) {
                    qWarning() << "xz decoder error:" << ret;
                    lzma_end(&strm);
                    return false;
                }
            }

            lzma_end(&strm);

            return true;
        }
        qWarning() << "error opening out-file:" << file_out;
    } else {
        qWarning() << "error opening in-file:" << file_in;
    }

    return false;
}

// source: https://github.com/libarchive/libarchive/blob/master/examples/untar.c
static int copy_data(struct archive* ar, struct archive* aw) {
    int r;
    const void* buff;
    size_t size;
#if ARCHIVE_VERSION_NUMBER >= 3000000
    int64_t offset;
#else
    off_t offset;
#endif

    for (;;) {
        r = archive_read_data_block(ar, &buff, &size, &offset);
        if (r == ARCHIVE_EOF) return (ARCHIVE_OK);
        if (r != ARCHIVE_OK) return (r);
        r = archive_write_data_block(aw, buff, size, offset);
        if (r != ARCHIVE_OK) {
            return (r);
        }
    }
}

static bool tar_decode(const QString& file_in, const QString& dir_out) {
    qDebug() << "extracting tar archive:" << file_in << dir_out;

    struct archive* a = archive_read_new();
    struct archive* ext = archive_write_disk_new();
    archive_read_support_format_tar(a);

    bool ok = true;

    if (archive_read_open_filename(a, file_in.toStdString().c_str(), 10240)) {
        qWarning() << "error opening in-file:" << file_in
                   << archive_error_string(a);
        ok = false;
    } else {
        struct archive_entry* entry;
        while (true) {
            int ret = archive_read_next_header(a, &entry);
            if (ret == ARCHIVE_EOF) break;
            if (ret != ARCHIVE_OK) {
                qWarning() << "error archive_read_next_header:" << file_in
                           << archive_error_string(a);
                ok = false;
                break;
            }

            const QString entry_path{archive_entry_pathname_utf8(entry)};
            const QString out_path = dir_out + "/" + entry_path;

            const auto std_file_out = out_path.toStdString();
            archive_entry_set_pathname(entry, std_file_out.c_str());

            ret = archive_write_header(ext, entry);
            if (ret != ARCHIVE_OK) {
                qWarning() << "error archive_write_header:" << file_in
                           << archive_error_string(ext);
                ok = false;
                break;
            }

            copy_data(a, ext);
            ret = archive_write_finish_entry(ext);
            if (ret != ARCHIVE_OK) {
                qWarning() << "error archive_write_finish_entry:" << file_in
                           << archive_error_string(ext);
                ok = false;
                break;
            }
        }
    }

    archive_read_close(a);
    archive_read_free(a);
    archive_write_close(ext);
    archive_write_free(ext);

    return ok;
}
#endif

QDebug operator<<(QDebug debug, const std::string& s) {
    QDebugStateSaver saver{debug};
    debug.nospace() << s.c_str();
    return debug;
}

YtdlApi::YtdlApi(QObject* parent) : QObject{parent} {
#ifdef USE_SFOS
    ::setenv("PYTHONPATH", pythonSitePath().toStdString().c_str(), true);
#endif
    init();
}

bool YtdlApi::unpack() {
#ifdef USE_SFOS
    qDebug() << "unpacking ytdl modules";

    QDir{pythonSitePath()}.removeRecursively();

    if (!QFileInfo::exists(pythonArchivePath)) {
        qWarning() << "python archive does not exist";
        return false;
    }

    auto unpackPath = pythonUnpackPath() + "/python.tar";
    if (QFileInfo::exists(unpackPath)) QFile::remove(unpackPath);

    if (!xz_decode(pythonArchivePath, unpackPath)) {
        qWarning() << "cannot extract python archive";
        Settings::instance()->setPythonChecksum({});
        return false;
    }

    if (!tar_decode(unpackPath, pythonUnpackPath())) {
        qWarning() << "cannot extract python tar archive";
        QFile::remove(unpackPath);
        QDir{pythonSitePath()}.removeRecursively();
        Settings::instance()->setPythonChecksum({});
        return false;
    }

    Settings::instance()->setPythonChecksum(make_checksum(pythonArchivePath));

    QFile::remove(unpackPath);

    qDebug() << "ytdl modules successfully unpacked";
    return true;
#else
    return true;
#endif
}

bool YtdlApi::check() {
#ifdef USE_SFOS
    auto oldChecksum = Settings::instance()->pythonChecksum();
    if (oldChecksum.isEmpty()) {
        qDebug() << "python modules checksum missing => need to unpack";
        return false;
    }

    if (oldChecksum != make_checksum(pythonArchivePath)) {
        qDebug() << "python modules checksum is invalid => need to unpack";
        return false;
    }

    if (!QFile::exists(pythonSitePath())) {
        qDebug() << "no python site dir";
        return false;
    }
#endif
    using namespace pybind11;
    scoped_interpreter guard{};

    try {
        auto ytmusic_mod = module::import("ytmusicapi");
        qDebug()
            << "ytmusicapi version:"
            << ytmusic_mod.attr("version")("ytmusicapi").cast<std::string>();

        auto ytdlp_mod = module::import("yt_dlp").attr("version");
        qDebug() << "yt_dlp version:"
                 << ytdlp_mod.attr("__version__").cast<std::string>();
    } catch (const std::exception& err) {
        qDebug() << "ytdl check failed:" << err.what();
        return false;
    }

    qDebug() << "ytdl modules work fine";
    return true;
}

bool YtdlApi::init() {
    if (state == State::Enabled) return true;
    if (state == State::Disabled) return false;

    if (check()) {
        state = State::Enabled;
        return true;
    }

    if (unpack() && check()) {
        state = State::Enabled;
        return true;
    }

    state = State::Disabled;

    return false;
}

static void logVideoInfo(const video_info::VideoInfo& info) {
    qDebug() << "extract_video_info:";
    qDebug() << "id:" << info.id;
    qDebug() << "title:" << info.title;
    qDebug() << "thumbnail:" << info.thumbnail;
    for (const auto& f : info.formats) {
        qDebug() << " -> format:";
        qDebug() << "    quality:" << f.quality.value_or(0);
        qDebug() << "    acodec:" << f.acodec;
        qDebug() << "    vcodec:" << f.vcodec;
        // #ifdef QT_DEBUG
        qDebug() << "    url:" << f.url;
        // #endif
    }
}

// static bool hlsUrl(std::string_view url) {
//     static const auto* m3u8 = ".m3u8";

//    if (url.empty() || url.size() < strlen(m3u8)) return false;

//    auto data = url.substr(url.size() - strlen(m3u8));
//    std::transform(data.begin(), data.end(), data.begin(),
//                   [](char c) { return std::tolower(c); });

//    return data == m3u8;
//}

QUrl YtdlApi::makeYtUrl(std::string_view id) {
    return QUrl{
        QStringLiteral("https://www.youtube.com/watch?v=%1").arg(id.data())};
}

QUrl YtdlApi::bestAudioUrl(const std::vector<video_info::Format>& formats) {
    std::vector<video_info::Format> audioFormats{};
    std::copy_if(formats.cbegin(), formats.cend(),
                 std::back_inserter(audioFormats), [](const auto& format) {
                     return !format.url.empty() && format.vcodec == "none" &&
                            format.acodec.find("mp4a") != std::string::npos;
                 });

    auto it = std::max_element(audioFormats.cbegin(), audioFormats.cend(),
                               [](const auto& f1, const auto& f2) {
                                   return f1.quality.value_or(0) <
                                          f2.quality.value_or(0);
                               });

    if (it == audioFormats.cend()) {
        qWarning() << "best audio format not found";
        return {};
    }

    return QUrl{QString::fromStdString(it->url)};
}

QUrl YtdlApi::bestVideoUrl(const std::vector<video_info::Format>& formats) {
    std::vector<video_info::Format> videoFormats{};

    std::copy_if(formats.cbegin(), formats.cend(),
                 std::back_inserter(videoFormats), [](const auto& format) {
                     return !format.url.empty() &&
                            format.vcodec.find("avc1") != std::string::npos &&
                            format.acodec.find("mp4a") != std::string::npos;
                 });

    auto it = std::max_element(videoFormats.cbegin(), videoFormats.cend(),
                               [](const auto& f1, const auto& f2) {
                                   return f1.quality.value_or(0) <
                                          f2.quality.value_or(0);
                               });

    if (it == videoFormats.cend()) {
        qWarning() << "best video format not found";
        return {};
    }

    return QUrl{QString::fromStdString(it->url)};
}

QUrl YtdlApi::bestThumbUrl(const std::vector<meta::Thumbnail>& thumbs) {
    if (thumbs.empty()) return {};

    std::vector<meta::Thumbnail> squareThumbs;
    std::copy_if(thumbs.cbegin(), thumbs.cend(),
                 std::back_inserter(squareThumbs),
                 [](const auto& thumb) { return thumb.width == thumb.height; });

    const auto& ts = squareThumbs.empty() ? thumbs : squareThumbs;

    auto it = std::max_element(
        ts.cbegin(), ts.cend(), [](const auto& t1, const auto& t2) {
            return t1.width + t1.height < t2.width + t2.height;
        });

    if (it == ts.cend()) {
        qWarning() << "best thumb not found";
        return {};
    }

    return QUrl{QString::fromStdString(it->url)};
}

QString YtdlApi::bestArtistName(const std::vector<meta::Artist>& artists,
                                const QString& defaultName) {
    if (artists.empty() || artists[0].name.empty()) return defaultName;
    return QString::fromStdString(artists[0].name);
}

std::optional<YtdlApi::Track> YtdlApi::track(QUrl url) const {
    if (state != State::Enabled) return std::nullopt;

    YTMusic ytm{};

    try {
        auto info = ytm.extract_video_info(url.toString().toStdString());

        logVideoInfo(info);

        Track t{/*title=*/QString::fromStdString(info.title),
                /*webUrl=*/std::move(url),
                /*streamUrl=*/bestVideoUrl(info.formats),
                /*streamAudioUrl=*/bestAudioUrl(info.formats),
                /*imageUrl=*/QUrl{QString::fromStdString(info.thumbnail)}};

        if (t.streamUrl.isEmpty() && t.streamAudioUrl.isEmpty())
            return std::nullopt;
        return t;
    } catch (const std::exception& err) {
        qWarning() << "ytdl error:" << err.what();
    }

    return std::nullopt;
}

std::optional<YtdlApi::Album> YtdlApi::album(const QString& id) const {
    if (state != State::Enabled) return std::nullopt;
    if (id.isEmpty()) return std::nullopt;
    if (QThread::currentThread()->isInterruptionRequested())
        return std::nullopt;

    YTMusic ytm{};

    try {
        auto result = ytm.get_album(id.toStdString());

        Album a{/*title=*/QString::fromStdString(result.title),
                /*artist=*/bestArtistName(result.artists),
                /*imageUrl=*/bestThumbUrl(result.thumbnails),
                /*tracks=*/{}};

        result.tracks.erase(
            std::remove_if(
                result.tracks.begin(), result.tracks.end(),
                [](const auto& t) { return !static_cast<bool>(t.video_id); }),
            result.tracks.end());

        std::transform(
            result.tracks.cbegin(), result.tracks.cend(),
            std::back_inserter(a.tracks), [&a](const auto& t) {
                return AlbumTrack{
                    QString::fromStdString(*t.video_id),
                    QString::fromStdString(t.title),
                    bestArtistName(t.artists, a.artist), makeYtUrl(*t.video_id),
                    t.duration ? Utils::strToSecStatic(
                                     QString::fromStdString(*t.duration))
                               : 0};
            });

        return a;
    } catch (const std::exception& err) {
        qWarning() << "ytdl error:" << err.what();
    }

    return std::nullopt;
}

std::optional<YtdlApi::Album> YtdlApi::playlist(const QString& id) const {
    if (state != State::Enabled) return std::nullopt;
    if (id.isEmpty()) return std::nullopt;
    if (QThread::currentThread()->isInterruptionRequested())
        return std::nullopt;

    YTMusic ytm{};

    try {
        auto result = ytm.get_playlist(id.toStdString());

        Album a;
        a.artist = QString::fromStdString(result.author.name);
        a.title = QString::fromStdString(result.title);
        a.imageUrl = bestThumbUrl(result.thumbnails);

        result.tracks.erase(
            std::remove_if(
                result.tracks.begin(), result.tracks.end(),
                [](const auto& t) { return !static_cast<bool>(t.video_id); }),
            result.tracks.end());

        std::transform(
            result.tracks.cbegin(), result.tracks.cend(),
            std::back_inserter(a.tracks), [](const auto& t) {
                return AlbumTrack{
                    QString::fromStdString(*t.video_id),
                    QString::fromStdString(t.title), bestArtistName(t.artists),
                    makeYtUrl(*t.video_id),
                    Utils::strToSecStatic(QString::fromStdString(t.duration))};
            });

        return a;
    } catch (const std::exception& err) {
        qWarning() << "ytdl error:" << err.what();
    }

    return std::nullopt;
}

std::optional<YtdlApi::Artist> YtdlApi::artist(const QString& id) const {
    if (state != State::Enabled) return std::nullopt;
    if (id.isEmpty()) return std::nullopt;
    if (QThread::currentThread()->isInterruptionRequested())
        return std::nullopt;

    YTMusic ytm{};

    try {
        auto result = ytm.get_artist(id.toStdString());

        Artist a;

        a.name = QString::fromStdString(result.name);
        a.imageUrl = bestThumbUrl(result.thumbnails);

        if (result.albums) {
            std::transform(
                result.albums->results.cbegin(), result.albums->results.cend(),
                std::back_inserter(a.albums), [](const auto& t) {
                    return ArtistAlbum{QString::fromStdString(t.browse_id),
                                       QString::fromStdString(t.title),
                                       bestThumbUrl(t.thumbnails)};
                });
        }
        if (result.songs) {
            std::transform(
                result.songs->results.cbegin(), result.songs->results.cend(),
                std::back_inserter(a.tracks), [](const auto& t) {
                    return ArtistTrack{QString::fromStdString(t.video_id),
                                       QString::fromStdString(t.title),
                                       QString::fromStdString(t.album.name),
                                       makeYtUrl(t.video_id),
                                       bestThumbUrl(t.thumbnails),
                                       /*duration=*/0};
                });
        }
        if (result.videos) {
            std::transform(
                result.videos->results.cbegin(), result.videos->results.cend(),
                std::back_inserter(a.tracks), [](const auto& t) {
                    return ArtistTrack{QString::fromStdString(t.video_id),
                                       QString::fromStdString(t.title),
                                       /*album=*/{},
                                       makeYtUrl(t.video_id),
                                       bestThumbUrl(t.thumbnails),
                                       /*duration=*/0};
                });
        }

        return a;
    } catch (const std::exception& err) {
        qWarning() << "ytdl error:" << err.what();
    }

    return std::nullopt;
}

std::vector<YtdlApi::SearchResultItem> YtdlApi::homeFirstPage() {
    return home(maxHomeFirstPage);
}

std::vector<YtdlApi::SearchResultItem> YtdlApi::home(int limit) {
    std::vector<SearchResultItem> items;

    if (state != State::Enabled) return items;
    if (QThread::currentThread()->isInterruptionRequested()) return items;

    YTMusic ytm{};

    try {
        if (m_homeSections.empty()) {
            m_homeSections = ytm.get_home();
        }

        for (const auto& section : m_homeSections) {
            for (int i = 0; i < std::min<int>(limit, section.content.size());
                 ++i) {
                const auto& r = section.content[i];

                if (QThread::currentThread()->isInterruptionRequested())
                    return items;

                SearchResultItem item;

                std::visit(
                    [&](auto&& arg) {
                        using T = std::decay_t<decltype(arg)>;
                        if constexpr (std::is_same_v<T, home::Video>) {
                            item.type = Type::Video;
                            item.id = QString::fromStdString(arg.video_id);
                            item.title = QString::fromStdString(arg.title);
                            item.webUrl = makeYtUrl(arg.video_id);
                            if (arg.album)
                                item.album =
                                    QString::fromStdString(arg.album->name);
                            item.imageUrl = bestThumbUrl(arg.thumbnail);
                            item.artist = bestArtistName(arg.artists);
                        } else if constexpr (std::is_same_v<T,
                                                            home::Playlist>) {
                            item.type = Type::Playlist;
                            item.id = QString::fromStdString(arg.id);
                            item.title = QString::fromStdString(arg.title);
                            item.imageUrl = bestThumbUrl(arg.thumbnail);
                        } else {
#ifdef QT_DEBUG
                            qWarning() << "unknown type";
#endif
                        }
                    },
                    r);

                if (!item.id.isEmpty()) {
                    item.section = QString::fromStdString(section.title);
                    items.push_back(std::move(item));
                }
            }
        }
    } catch (const std::exception& err) {
        qWarning() << "ytdl error:" << err.what();
    }

    return items;
}

std::vector<YtdlApi::SearchResultItem> YtdlApi::search(
    const QString& query) const {
    std::vector<SearchResultItem> items;

    if (state != State::Enabled) return items;
    if (QThread::currentThread()->isInterruptionRequested()) return items;

    YTMusic ytm{};

    try {
        auto results = ytm.search(query.toStdString());
        items.reserve(results.size());

        for (const auto& r : results) {
            if (QThread::currentThread()->isInterruptionRequested())
                return items;

            SearchResultItem item;

            std::visit(
                [&](auto&& arg) {
                    using T = std::decay_t<decltype(arg)>;
                    if constexpr (std::is_same_v<T, search::Album>) {
                        item.type = Type::Album;
                        item.id = QString::fromStdString(arg.browse_id);
                        item.album = QString::fromStdString(arg.title);
                        item.artist = bestArtistName(arg.artists);
                        auto album = ytm.get_album(arg.browse_id);
                        item.imageUrl = bestThumbUrl(album.thumbnails);
                    } else if constexpr (std::is_same_v<T, search::Artist>) {
                        item.type = Type::Artist;
                        item.id = QString::fromStdString(arg.browse_id);
                        item.artist = QString::fromStdString(arg.artist);
                        auto artist = ytm.get_artist(arg.browse_id);
                        item.imageUrl = bestThumbUrl(artist.thumbnails);
                    } else if constexpr (std::is_same_v<T, search::Playlist>) {
                        item.type = Type::Playlist;
                        item.id = QString::fromStdString(arg.browse_id);
                        item.count =
                            QString::fromStdString(arg.item_count).toInt();
                        item.title = QString::fromStdString(arg.title);
                        // item.artist = QString::fromStdString(arg.author);
                        // auto playlist = ytm.get_playlist(arg.browse_id);
                        // item.imageUrl = bestThumbUrl(playlist.thumbnails);
                    } else if constexpr (std::is_same_v<T, search::Song> ||
                                         std::is_same_v<T, search::Video>) {
                        item.type = Type::Video;
                        item.id = QString::fromStdString(arg.video_id);
                        item.title = QString::fromStdString(arg.title);
                        item.webUrl = makeYtUrl(arg.video_id);
                        item.artist = bestArtistName(arg.artists);

                        if constexpr (std::is_same_v<T, search::Song>) {
                            item.album = QString::fromStdString(arg.album.name);
                            item.duration = Utils::strToSecStatic(
                                QString::fromStdString(arg.duration));
                        }

                        /*if (auto song = ytm.get_song(arg.video_id)) {
                            if (item.artist.isEmpty() &&
                                !song->author.empty()) {
                                item.artist =
                                    QString::fromStdString(song->author);
                            }
                            item.imageUrl =
                                bestThumbUrl(song->thumbnail.thumbnails);
                            item.duration =
                                QString::fromStdString(song->length).toInt();
                        }*/
                    } else {
#ifdef QT_DEBUG
                        qWarning() << "unknown type";
#endif
                    }
                },
                r);

            if (!item.id.isEmpty()) items.push_back(std::move(item));
        }
    } catch (const std::exception& err) {
        qWarning() << "ytdl error:" << err.what();
    }

    return items;
}
