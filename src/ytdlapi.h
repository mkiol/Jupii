/* Copyright (C) 2022-2024 Michal Kosciesza <michal@mkiol.net>
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
    ~YtdlApi();

    bool ready() const;
    std::vector<SearchResultItem> search(const QString& query);
    std::optional<Track> track(QUrl url);
    std::optional<Album> album(const QString& id);
    std::optional<Album> playlist(const QString& id);
    std::optional<Artist> artist(const QString& id);
    std::vector<SearchResultItem> home(int limit = maxHome);
    std::vector<SearchResultItem> homeFirstPage();

   private:
    static const int maxHome = 50;
    static const int maxHomeFirstPage = 5;
    static std::vector<home::Section> m_homeSections;
    std::optional<YTMusic> m_ytmusic;

    static QUrl bestAudioUrl(const std::vector<video_info::Format>& formats);
    static QUrl bestVideoUrl(const std::vector<video_info::Format>& formats);
    static QUrl bestThumbUrl(const std::vector<meta::Thumbnail>& thumbs);
    static QString bestArtistName(const std::vector<meta::Artist>& artists,
                                  const QString& defaultName = {});
    static QUrl makeYtUrl(std::string_view id);
    bool makeYtMusic();
};

#endif  // YTDLAPI_H
