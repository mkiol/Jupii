/* Copyright (C) 2022 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef YTDLAPI_H
#define YTDLAPI_H

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QUrl>
#include <memory>
#include <optional>
#include <string_view>

#include "ytmusic.h"

class YtdlApi : public QObject {
    Q_OBJECT
   public:
    enum class Type { Unknown = 0, Artist, Album, Playlist, Video };

    struct SearchResultItem {
        Type type = Type::Unknown;
        QString id;
        QString title;
        QString album;
        QString artist;
        QUrl imageUrl;
        QUrl webUrl;
        QString section;
        int duration = 0;
        int count = 0;
    };

    struct Track {
        QString title;
        QUrl webUrl;
        QUrl streamUrl;
        QUrl streamAudioUrl;
        QUrl imageUrl;
    };

    struct AlbumTrack {
        QString id;
        QString title;
        QString artist;
        QUrl webUrl;
        int duration = 0;
    };

    struct Album {
        QString title;
        QString artist;
        QUrl imageUrl;
        std::vector<AlbumTrack> tracks;
    };

    struct ArtistAlbum {
        QString id;
        QString title;
        QUrl imageUrl;
    };

    struct ArtistTrack {
        QString id;
        QString title;
        QString album;
        QUrl webUrl;
        QUrl imageUrl;
        int duration = 0;
    };

    struct Artist {
        QString name;
        QUrl imageUrl;
        std::vector<ArtistAlbum> albums;
        std::vector<ArtistTrack> tracks;
    };

    YtdlApi(QObject* parent = nullptr);
    std::vector<SearchResultItem> search(const QString& query) const;
    std::optional<Track> track(QUrl url) const;
    std::optional<Album> album(const QString& id) const;
    std::optional<Album> playlist(const QString& id) const;
    std::optional<Artist> artist(const QString& id) const;
    std::vector<SearchResultItem> home(int limit = maxHome);
    std::vector<SearchResultItem> homeFirstPage();
    static bool init();

   private:
    enum class State { Unknown, Disabled, Enabled };
    static const int maxHome;
    static const int maxHomeFirstPage;
    static std::vector<home::Section> m_homeSections;
    static State state;
#ifdef USE_SFOS
    const static QString pythonArchivePath;
    static QString pythonSitePath();
    static QString pythonUnpackPath();
#endif
    static bool unpack();
    static bool check();
    static QUrl bestAudioUrl(const std::vector<video_info::Format>& formats);
    static QUrl bestVideoUrl(const std::vector<video_info::Format>& formats);
    static QUrl bestThumbUrl(const std::vector<meta::Thumbnail>& thumbs);
    static QString bestArtistName(const std::vector<meta::Artist>& artists,
                                  const QString& defaultName = {});
    static bool urlOk(std::string_view url);
    static QUrl makeYtUrl(std::string_view id);
};

#endif  // YTDLAPI_H
