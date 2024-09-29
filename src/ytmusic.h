// SPDX-FileCopyrightText: 2021 Jonah Brüchert <jbb@kaidan.im>
//
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#ifndef YTMUSIC_H
#define YTMUSIC_H

#include <string>
#include <optional>
#include <map>
#include <variant>
#include <vector>
#include <memory>

constexpr auto TESTED_YTMUSICAPI_VERSION = "1.8.0";

struct YTMusicPrivate;

namespace meta {
struct Thumbnail {
    std::string url;
    int width;
    int height;

    bool operator<(const Thumbnail &other) const {
        return height < other.height;
    }
};
struct Artist {
    std::string name;
    std::optional<std::string> id;
};
struct Album {
    std::string name;
    std::optional<std::string> id;
};
}


namespace search {
///
/// Base class for Video and Song, which share most attributes.
///
struct Media {
    std::string video_id;
    std::string title;
    std::vector<meta::Artist> artists;
    std::optional<std::string> duration;
    std::vector<meta::Thumbnail> thumbnails;
};

struct Video : public Media {
    std::optional<std::string> views;
};

struct Playlist {
    std::string browse_id;
    std::string title;
    std::optional<std::string> author;
    std::string item_count;
    std::vector<meta::Thumbnail> thumbnails;
};

struct Song : public Media {
    std::optional<meta::Album> album;
    std::optional<bool> is_explicit;
};

struct Album {
    std::optional<std::string> browse_id;
    std::string title;
    std::string type;
    std::vector<meta::Artist> artists;
    std::optional<std::string> year;
    bool is_explicit;
    std::vector<meta::Thumbnail> thumbnails;
};

struct Artist {
    std::string browse_id;
    std::string artist;
    std::optional<std::string> shuffle_id;
    std::optional<std::string> radio_id;
    std::vector<meta::Thumbnail> thumbnails;
};

struct TopResult {
    std::string category;
    std::string result_type;
    std::optional<std::string> video_id;
    std::optional<std::string> title;
    std::vector<meta::Artist> artists;
    std::vector<meta::Thumbnail> thumbnails;
};

using SearchResultItem = std::variant<Video, Playlist, Song, Album, Artist, TopResult>;
};


namespace artist {
struct Artist {
    template<typename T>
    struct Section {
        std::optional<std::string> browse_id;
        std::vector<T> results;
        std::optional<std::string> params;
    };

    struct Song {
        struct Album {
            std::string name;
            std::string id;
        };

        std::string video_id;
        std::string title;
        std::vector<meta::Thumbnail> thumbnails;
        std::vector<meta::Artist> artist;
        Album album;
    };

    struct Album {
        std::string title;
        std::vector<meta::Thumbnail> thumbnails;
        std::optional<std::string> year;
        std::string browse_id;
        std::optional<std::string> type;
    };

    struct Video {
        std::string title;
        std::vector<meta::Thumbnail> thumbnails;
        std::optional<std::string> views;
        std::string video_id;
        std::string playlist_id;
    };

    struct Single {
        std::string title;
        std::vector<meta::Thumbnail> thumbnails;
        std::string year;
        std::string browse_id;
    };

    std::optional<std::string> description;
    std::optional<std::string> views;
    std::string name;
    std::string channel_id;
    std::optional<std::string> subscribers;
    bool subscribed;
    std::vector<meta::Thumbnail> thumbnails;
    std::optional<Section<Song>> songs;
    std::optional<Section<Album>> albums;
    std::optional<Section<Single>> singles;
    std::optional<Section<Video>> videos;
};
}

namespace album {
    struct Track {
        std::optional<bool> is_explicit;
        std::string title;
        std::vector<meta::Artist> artists;
        std::optional<std::string> album;
        std::optional<std::string> video_id;
        std::optional<std::string> duration;
        std::optional<std::string> like_status;
    };

    struct Album {
        struct ReleaseDate {
            int year;
            int month;
            int day;
        };

        std::string title;
        int track_count;
        std::string duration;
        std::string audio_playlist_id;
        std::optional<std::string> year;
        std::optional<std::string> description;
        std::vector<meta::Thumbnail> thumbnails;
        std::vector<Track> tracks;
        std::vector<meta::Artist> artists;
    };
}

namespace song {
struct Song {
    struct Thumbnail {
        std::vector<meta::Thumbnail> thumbnails;
    };

    std::string video_id;
    std::string title;
    std::string length;
    std::string channel_id;
    bool is_owner_viewer;
    bool is_crawlable;
    Thumbnail thumbnail;
    std::string view_count;
    std::string author;
    bool is_private;
    bool is_unplugged_corpus;
    bool is_live_content;
    std::vector<std::string> artists;
};
}

namespace playlist {
struct Track {
    std::optional<std::string> video_id;
    std::string title;
    std::vector<meta::Artist> artists;
    std::optional<meta::Album> album;
    std::optional<std::string> duration;
    std::optional<std::string> like_status;
    std::vector<meta::Thumbnail> thumbnails;
    bool is_available;
    std::optional<bool> is_explicit;
};

struct Playlist {
    std::string id;
    std::string privacy;
    std::string title;
    std::vector<meta::Thumbnail> thumbnails;
    std::optional<meta::Artist> author;
    std::optional<std::string> year;
    std::string duration;
    int track_count;
    std::vector<Track> tracks;
};
}

namespace video_info {
struct Format {
    std::optional<float> quality;
    std::string url;
    std::string vcodec;
    std::string acodec;

    // More, but not interesting for us right now
};

struct VideoInfo {
    std::string id;
    std::string title;
    std::string artist;
    std::string channel;
    std::vector<Format> formats;
    std::string thumbnail;

    // More, but not interesting for us right now
};
}

namespace watch {
struct Playlist {
    struct Track {
        std::string title;
        std::optional<std::string> length;
        std::string video_id;
        std::optional<std::string> playlistId;
        std::vector<meta::Thumbnail> thumbnail;
        std::optional<std::string> like_status;
        std::vector<meta::Artist> artists;
        std::optional<meta::Album> album;
    };

    std::vector<Track> tracks;
    std::optional<std::string> lyrics;
};
}

namespace home {
struct Video {
    std::string video_id;
    std::string title;
    std::vector<meta::Artist> artists;
    std::vector<meta::Thumbnail> thumbnail;
    std::optional<meta::Album> album;
};

struct Playlist {
    std::string id;
    std::string title;
    std::vector<meta::Thumbnail> thumbnail;
};

using Content = std::variant<std::monostate, Video, Playlist>;

struct Section {
    std::string title;
    std::vector<Content> content;
};
}  // namespace home

struct Lyrics {
    std::optional<std::string> source;
    std::string lyrics;
};

class YTMusic
{
public:
    YTMusic(const std::optional<std::string> &auth = std::nullopt,
            const std::optional<std::string> &user = std::nullopt,
            const std::optional<bool> requests_session = std::nullopt,
            const std::optional<std::map<std::string, std::string>> &proxies = std::nullopt,
            const std::string &language = "en");

    ~YTMusic();

    std::vector<search::SearchResultItem> search(const std::string &query,
                                                 const std::optional<std::string> &filter = std::nullopt,
                                                 const std::optional<std::string> &scope = std::nullopt,
                                                 const int limit = 100,
                                                 const bool ignore_spelling = false) const;

    /// https://ytmusicapi.readthedocs.io/en/latest/reference.html#ytmusicapi.YTMusic.get_artist
    artist::Artist get_artist(const std::string &channel_id) const;

    /// https://ytmusicapi.readthedocs.io/en/latest/reference.html#ytmusicapi.YTMusic.get_album
    album::Album get_album(const std::string &browse_id) const;

    /// https://ytmusicapi.readthedocs.io/en/latest/reference.html#ytmusicapi.YTMusic.get_song
    std::optional<song::Song> get_song(const std::string &video_id) const;

    /// https://ytmusicapi.readthedocs.io/en/latest/reference.html#ytmusicapi.YTMusic.get_playlist
    playlist::Playlist get_playlist(const std::string &playlist_id, int limit = 1024) const;

    /// https://ytmusicapi.readthedocs.io/en/latest/reference.html#ytmusicapi.YTMusic.get_artist_albums
    std::vector<artist::Artist::Album> get_artist_albums(const std::string &channel_id, const std::string &params) const;

    /// youtube-dl's extract_info function
    video_info::VideoInfo extract_video_info(const std::string &video_id) const;

    /// https://ytmusicapi.readthedocs.io/en/latest/reference.html#ytmusicapi.YTMusic.get_watch_playlist
    watch::Playlist get_watch_playlist(const std::optional<std::string> &videoId = std::nullopt,
                                      const std::optional<std::string> &playlistId = std::nullopt,
                                      int limit = 25) const;

    Lyrics get_lyrics(const std::string &browse_id) const;

    std::string get_version() const;

    std::vector<home::Section> get_home(int limit = 3) const;

    // TODO wrap more methods

private:
    std::unique_ptr<YTMusicPrivate> d;
};

#endif // YTMUSIC_H
