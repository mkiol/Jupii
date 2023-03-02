/* Copyright (C) 2023 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "playlistparser.h"

#include <QByteArray>
#include <QString>
#include <QUrl>
#include <catch2/catch_test_macros.hpp>
#include <optional>

TEST_CASE("Playlist parser", "[playlist-parser]") {
    auto m3uData = QByteArrayLiteral(
        "#EXTM3U\n"
        "#PLAYLIST:Test Playlist\n"
        "#EXTINF:419,Track 01\n"
        "https://localhost/01_Track.mp3\n"
        "#EXTINF:260,Track 02\n"
        "https://localhost/02_Track.mp3\n"
        "#EXTINF:255,Track 03\n"
        "https://localhost/03_Track.mp3");

    auto plsData = QByteArrayLiteral(
        "[playlist]\n"
        "NumberOfEntries=3\n"
        "File1=https://localhost/01_Track.mp3\n"
        "File2=https://localhost/02_Track.mp3\n"
        "File3=https://localhost/03_Track.mp3\n"
        "Title1=Track 01\n"
        "Title2=Track 02\n"
        "Title3=Track 03\n"
        "Length1=419\n"
        "Length2=260\n"
        "Length3=255");

    auto hlsMasterData = QByteArrayLiteral(
        "#EXTM3U\n"
        "#EXT-X-VERSION:3\n"
        "#EXT-X-STREAM-INF:BANDWIDTH=71763,CODECS=\"mp4a.40.2\"\n"
        "chunklist_w176572341.m3u8\n");

    auto hlsMediaData = QByteArrayLiteral(
        "#EXTM3U\n"
        "#EXT-X-VERSION:3\n"
        "#EXT-X-TARGETDURATION:11\n"
        "#EXT-X-MEDIA-SEQUENCE:6562\n"
        "#EXT-X-DISCONTINUITY-SEQUENCE:0\n"
        "#EXTINF:9.984\n"
        "media_w1136423284_6562.aac\n"
        "#EXTINF:10.048,\n"
        "media_w1136423284_6563.aac\n"
        "#EXTINF:9.984,\n"
        "media_w1136423284_6564.aac\n");

    auto xspfData = QByteArrayLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<playlist version=\"1\" xmlns=\"http://xspf.org/ns/0/\">"
        "<trackList>"
        "<track><title>Track "
        "01</title><duration>419</duration><location>https://localhost/"
        "01_Track.mp3</location></track>"
        "<track><title>Track "
        "02</title><duration>260</duration><location>https://localhost/"
        "02_Track.mp3</location></track>"
        "<track><title>Track "
        "03</title><duration>255</duration><location>https://localhost/"
        "03_Track.mp3</location></track>"
        "</trackList>"
        "</playlist>");

    SECTION("M3U parse fails with PLS data") {
        auto result = PlaylistParser::parseM3u(plsData);

        REQUIRE_FALSE(result.has_value());
    }

    SECTION("M3U parse is successful with M3U data") {
        auto result = PlaylistParser::parseM3u(m3uData);

        REQUIRE(result.has_value());
        REQUIRE(result->items.size() == 3);
        REQUIRE(result->title == QStringLiteral("Test Playlist"));
        REQUIRE(result->items.front().length == 419);
        REQUIRE(result->items.front().title == QStringLiteral("Track 01"));
        REQUIRE(result->items.front().url ==
                QUrl{QStringLiteral("https://localhost/01_Track.mp3")});
    }

    SECTION("PLS parse fails with M3U data") {
        auto result = PlaylistParser::parsePls(m3uData);

        REQUIRE_FALSE(result.has_value());
    }

    SECTION("PLS parse is successful with PLS data") {
        auto result = PlaylistParser::parsePls(plsData);

        REQUIRE(result.has_value());
        REQUIRE(result->items.size() == 3);
        REQUIRE(result->items.front().length == 419);
        REQUIRE(result->items.front().title == QStringLiteral("Track 01"));
        REQUIRE(result->items.front().url ==
                QUrl{QStringLiteral("https://localhost/01_Track.mp3")});
    }

    SECTION("HLS master playlist type detected with HLS master data") {
        auto result = PlaylistParser::hlsPlaylistType(hlsMasterData);

        REQUIRE(result == PlaylistParser::HlsType::MasterPlaylist);
    }

    SECTION("HLS playlist parse is successful with HLS master data") {
        auto result = PlaylistParser::parseHls(
            hlsMasterData, QStringLiteral("https://localhost"));

        REQUIRE(result.has_value());
        REQUIRE(std::holds_alternative<PlaylistParser::HlsMasterPlaylist>(
            result.value()));

        const auto& playlist =
            std::get<PlaylistParser::HlsMasterPlaylist>(result.value());

        REQUIRE(playlist.items.size() == 1);
        REQUIRE(playlist.items.front().bandwidth == 71763);
        REQUIRE(playlist.items.front().codecs == QStringLiteral("mp4a.40.2"));
        REQUIRE(playlist.items.front().url ==
                QUrl{QStringLiteral(
                    "https://localhost/chunklist_w176572341.m3u8")});
    }

    SECTION("HLS media playlist type detected with HLS media data") {
        auto result = PlaylistParser::hlsPlaylistType(hlsMediaData);

        REQUIRE(result == PlaylistParser::HlsType::MediaPlaylist);
    }

    SECTION("HLS playlist parse is successful with HLS media data") {
        auto result = PlaylistParser::parseHls(
            hlsMediaData, QStringLiteral("https://localhost"));

        REQUIRE(result.has_value());
        REQUIRE(std::holds_alternative<PlaylistParser::HlsMediaPlaylist>(
            result.value()));

        const auto& playlist =
            std::get<PlaylistParser::HlsMediaPlaylist>(result.value());

        REQUIRE(playlist.items.size() == 3);
        REQUIRE(playlist.live);
        REQUIRE(playlist.targetDuration == 11);
        REQUIRE(playlist.items.front().length == 9);
        REQUIRE(playlist.items.front().seq == 6562);
        REQUIRE(playlist.items.front().url ==
                QUrl{QStringLiteral(
                    "https://localhost/media_w1136423284_6562.aac")});
    }

    SECTION("XSPF parse fails with PLS data") {
        auto result = PlaylistParser::parseXspf(plsData);

        REQUIRE_FALSE(result.has_value());
    }

    SECTION("XSPF parse is successful with XSPF data") {
        auto result = PlaylistParser::parseXspf(xspfData);

        REQUIRE(result.has_value());
        REQUIRE(result->items.size() == 3);
        REQUIRE(result->items.front().length == 419);
        REQUIRE(result->items.front().title == QStringLiteral("Track 01"));
        REQUIRE(result->items.front().url ==
                QUrl{QStringLiteral("https://localhost/01_Track.mp3")});
    }
}
