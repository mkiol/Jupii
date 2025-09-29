/* Copyright (C) 2022-2024 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "ytdlapi.h"

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

#include "py_executor.hpp"
#include "utils.h"

std::vector<home::Section> YtdlApi::m_homeSections{};

QDebug operator<<(QDebug debug, const std::string& s) {
    QDebugStateSaver saver{debug};
    debug.nospace() << s.c_str();
    return debug;
}

YtdlApi::YtdlApi(QObject* parent) : QObject{parent} {}

YtdlApi::~YtdlApi() {
    if (m_ytmusic) {
        auto task = py_executor::instance()->execute([&]() {
            try {
                m_ytmusic.reset();
                return true;
            } catch (const std::exception& err) {
                qCritical() << "error:" << err.what();
                return false;
            }
        });
        if (task) task->get();
    }
}

bool YtdlApi::ready() const {
    return py_executor::instance()->libs_availability.has_value() &&
           py_executor::instance()->libs_availability->ytdlp &&
           py_executor::instance()->libs_availability->ytmusicapi;
}

bool YtdlApi::makeYtMusic() {
    if (m_ytmusic) return true;

    if (!ready()) return false;

    m_ytmusic.emplace();

    return true;
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
#ifdef DEBUG
        qDebug() << "    url:" << f.url;
#endif
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

std::optional<YtdlApi::Track> YtdlApi::track(QUrl url) {
    if (url.isEmpty()) return std::nullopt;

    if (QThread::currentThread()->isInterruptionRequested())
        return std::nullopt;

    auto* pe = py_executor::instance();

    auto task = pe->execute([&]() {
        try {
            if (makeYtMusic())
                return m_ytmusic->extract_video_info(
                    url.toString().toStdString());
        } catch (const std::exception& err) {
            qWarning() << "ytdl error:" << err.what();
        }
        return video_info::VideoInfo{};
    });

    if (!task) return std::nullopt;

    auto info = std::any_cast<video_info::VideoInfo>(task->get());

    logVideoInfo(info);

    Track t{/*title=*/QString::fromStdString(info.title),
            /*webUrl=*/std::move(url),
            /*streamUrl=*/bestVideoUrl(info.formats),
            /*streamAudioUrl=*/bestAudioUrl(info.formats),
            /*imageUrl=*/QUrl{QString::fromStdString(info.thumbnail)}};

    if (t.streamUrl.isEmpty() && t.streamAudioUrl.isEmpty())
        return std::nullopt;
    return t;

    return std::nullopt;
}

std::optional<YtdlApi::Album> YtdlApi::album(const QString& id) {
    if (id.isEmpty()) return std::nullopt;

    if (QThread::currentThread()->isInterruptionRequested())
        return std::nullopt;

    auto* pe = py_executor::instance();

    auto task = pe->execute([&]() {
        try {
            if (makeYtMusic()) return m_ytmusic->get_album(id.toStdString());
        } catch (const std::exception& err) {
            qWarning() << "ytdl error:" << err.what();
        }
        return album::Album{};
    });

    if (!task) return std::nullopt;

    auto result = std::any_cast<album::Album>(task->get());

    if (result.title.empty()) return std::nullopt;

    Album a{/*title=*/QString::fromStdString(result.title),
            /*artist=*/bestArtistName(result.artists),
            /*imageUrl=*/bestThumbUrl(result.thumbnails),
            /*tracks=*/{}};

    result.tracks.erase(
        std::remove_if(
            result.tracks.begin(), result.tracks.end(),
            [](const auto& t) { return !static_cast<bool>(t.video_id); }),
        result.tracks.end());

    std::transform(result.tracks.cbegin(), result.tracks.cend(),
                   std::back_inserter(a.tracks), [&a](const auto& t) {
                       return AlbumTrack{
                           QString::fromStdString(*t.video_id),
                           QString::fromStdString(t.title),
                           bestArtistName(t.artists, a.artist),
                           QUrl{},
                           makeYtUrl(*t.video_id),
                           t.duration ? Utils::strToSecStatic(
                                            QString::fromStdString(*t.duration))
                                      : 0};
                   });

    return a;
}

std::optional<YtdlApi::Album> YtdlApi::playlist(const QString& id) {
    if (id.isEmpty()) return std::nullopt;

    if (QThread::currentThread()->isInterruptionRequested())
        return std::nullopt;

    auto* pe = py_executor::instance();

    auto task = pe->execute([&]() {
        try {
            if (makeYtMusic()) return m_ytmusic->get_playlist(id.toStdString());
        } catch (const std::exception& err) {
            qWarning() << "ytdl error:" << err.what();
        }
        return playlist::Playlist{};
    });

    if (!task) return std::nullopt;

    auto result = std::any_cast<playlist::Playlist>(task->get());

    if (result.title.empty()) return std::nullopt;

    Album a;
    a.artist = result.author ? QString::fromStdString(result.author->name) : "";
    a.title = QString::fromStdString(result.title);
    a.imageUrl = bestThumbUrl(result.thumbnails);

    result.tracks.erase(
        std::remove_if(
            result.tracks.begin(), result.tracks.end(),
            [](const auto& t) { return !static_cast<bool>(t.video_id); }),
        result.tracks.end());

    std::transform(result.tracks.cbegin(), result.tracks.cend(),
                   std::back_inserter(a.tracks), [&a](const auto& t) {
                       auto thumb = bestThumbUrl(t.thumbnails);
                       return AlbumTrack{
                           QString::fromStdString(*t.video_id),
                           QString::fromStdString(t.title),
                           bestArtistName(t.artists),
                           thumb.isEmpty() ? a.imageUrl : std::move(thumb),
                           makeYtUrl(*t.video_id),
                           Utils::strToSecStatic(QString::fromStdString(
                               t.duration.value_or("")))};
                   });

    return a;
}

std::optional<YtdlApi::Artist> YtdlApi::artist(const QString& id) {
    if (id.isEmpty()) return std::nullopt;

    if (QThread::currentThread()->isInterruptionRequested())
        return std::nullopt;

    auto* pe = py_executor::instance();

    auto task = pe->execute([&]() {
        try {
            if (makeYtMusic()) return m_ytmusic->get_artist(id.toStdString());
        } catch (const std::exception& err) {
            qWarning() << "ytdl error:" << err.what();
        }
        return artist::Artist{};
    });

    if (!task) return std::nullopt;

    auto result = std::any_cast<artist::Artist>(task->get());

    if (result.name.empty()) return std::nullopt;

    Artist a;

    a.name = QString::fromStdString(result.name);
    a.imageUrl = bestThumbUrl(result.thumbnails);

    if (result.albums) {
        std::
            transform(
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
}

std::vector<YtdlApi::SearchResultItem> YtdlApi::homeFirstPage() {
    return home(maxHome);
}

std::vector<YtdlApi::SearchResultItem> YtdlApi::home(int limit) {
    std::vector<SearchResultItem> items;

    if (QThread::currentThread()->isInterruptionRequested()) return items;

    auto* pe = py_executor::instance();

    if (m_homeSections.empty()) {
        auto task = pe->execute([&]() {
            try {
                if (makeYtMusic()) return m_ytmusic->get_home();
            } catch (const std::exception& err) {
                qWarning() << "ytdl error:" << err.what();
            }
            return std::vector<home::Section>{};
        });

        if (task)
            m_homeSections =
                std::any_cast<std::vector<home::Section>>(task->get());
    }

    for (const auto& section : m_homeSections) {
        for (int i = 0; i < std::min<int>(limit, section.content.size()); ++i) {
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
                    } else if constexpr (std::is_same_v<T, home::Playlist>) {
                        item.type = Type::Playlist;
                        item.id = QString::fromStdString(arg.id);
                        item.title = QString::fromStdString(arg.title);
                        item.imageUrl = bestThumbUrl(arg.thumbnail);
                    } else {
#ifdef DEBUG
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

    return items;
}

std::vector<YtdlApi::SearchResultItem> YtdlApi::search(const QString& query) {
    std::vector<SearchResultItem> items;

    if (QThread::currentThread()->isInterruptionRequested()) return items;

    auto task = py_executor::instance()->execute([&]() {
        try {
            if (!makeYtMusic()) return std::vector<SearchResultItem>{};

            auto results = m_ytmusic->search(query.toStdString());

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
                            item.id = QString::fromStdString(
                                arg.browse_id.value_or(""));
                            item.album = QString::fromStdString(arg.title);
                            item.artist = bestArtistName(arg.artists);
                            auto album = m_ytmusic->get_album(
                                arg.browse_id.value_or(""));
                            item.imageUrl = bestThumbUrl(album.thumbnails);
                        } else if constexpr (std::is_same_v<T,
                                                            search::Artist>) {
                            try {
                                auto artist =
                                    m_ytmusic->get_artist(arg.browse_id);
                                item.imageUrl = bestThumbUrl(artist.thumbnails);
                                item.type = Type::Artist;
                                item.id = QString::fromStdString(arg.browse_id);
                                item.artist =
                                    QString::fromStdString(arg.artist);
                            } catch (
                                [[maybe_unused]] const std::exception& err) {
                                // KeyError: 'musicImmersiveHeaderRenderer'
                            }
                        } else if constexpr (std::is_same_v<T,
                                                            search::Playlist>) {
                            item.type = Type::Playlist;
                            item.id = QString::fromStdString(arg.browse_id);
                            item.count =
                                QString::fromStdString(arg.item_count).toInt();
                            item.title = QString::fromStdString(arg.title);
                            // item.artist =
                            // QString::fromStdString(arg.author); auto
                            // playlist = ytm.get_playlist(arg.browse_id);
                            // item.imageUrl =
                            // bestThumbUrl(playlist.thumbnails);
                        } else if constexpr (std::is_same_v<T, search::Song> ||
                                             std::is_same_v<T, search::Video>) {
                            item.type = Type::Video;
                            item.id = QString::fromStdString(arg.video_id);
                            item.title = QString::fromStdString(arg.title);
                            item.webUrl = makeYtUrl(arg.video_id);
                            item.artist = bestArtistName(arg.artists);

                            if constexpr (std::is_same_v<T, search::Song>) {
                                if (arg.album)
                                    item.album =
                                        QString::fromStdString(arg.album->name);
                                item.duration = Utils::strToSecStatic(
                                    QString::fromStdString(
                                        arg.duration.value_or("")));
                            }

                            /*if (auto song =
                            m_ytmusic->get_song(arg.video_id)) { if
                            (item.artist.isEmpty() &&
                                    !song->author.empty()) {
                                    item.artist =
                                        QString::fromStdString(song->author);
                                }
                                item.imageUrl =
                                    bestThumbUrl(song->thumbnail.thumbnails);
                                item.duration =
                                    QString::fromStdString(song->length).toInt();
                            }*/
                        } else if constexpr (std::is_same_v<
                                                 T, search::TopResult>) {
                            if (arg.video_id && arg.title &&
                                !arg.title->empty()) {
                                item.type = Type::Video;
                                item.id = QString::fromStdString(*arg.video_id);
                                item.title = QString::fromStdString(*arg.title);
                                item.webUrl = makeYtUrl(*arg.video_id);
                                item.artist = bestArtistName(arg.artists);
                            }
                        } else {
#ifdef DEBUG
                            qWarning() << "unknown type";
#endif
                        }
                    },
                    r);

                if (!item.id.isEmpty()) items.push_back(std::move(item));
            }

        } catch (const std::exception& err) {
            qWarning() << "ytdl error:" << err.what();
            return std::vector<SearchResultItem>{};
        }

        return items;
    });

    if (task) items = std::any_cast<std::vector<SearchResultItem>>(task->get());

    return items;
}
