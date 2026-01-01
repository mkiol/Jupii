/* Copyright (C) 2021-2025 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef PLAYLISTPARSER_H
#define PLAYLISTPARSER_H

#include <QByteArray>
#include <QString>
#include <QUrl>
#include <optional>
#include <variant>
#include <vector>

#include "playlistmodel.h"

namespace PlaylistParser {
static const auto m3utag_extm3u = "#EXTM3U";
static const auto m3utag_playlist = "#PLAYLIST:";
static const auto m3utag_extinf = "#EXTINF:";
static const auto m3utag_extxmediasequence = "#EXT-X-MEDIA-SEQUENCE:";
static const auto m3utag_extxtargetduration = "#EXT-X-TARGETDURATION:";
static const auto m3utag_extxendlist = "#EXT-X-ENDLIST";
static const auto m3utag_extxstreaminf = "#EXT-X-STREAM-INF:";

enum class HlsType { Unknown, MasterPlaylist, MediaPlaylist };
QDebug operator<<(QDebug dbg, HlsType type);

struct Item {
    QUrl url;
    int length = 0;
    QString title;
    friend QDebug operator<<(QDebug dbg, const Item &item);
};

struct Playlist {
    QString title;
    std::vector<Item> items;
    friend QDebug operator<<(QDebug dbg, const Playlist &playlist);
};

struct HlsMasterItem {
    QUrl url;
    int bandwidth = 0;
    QString codecs;
    friend QDebug operator<<(QDebug dbg, const HlsMasterItem &item);
};

struct HlsMediaItem {
    QUrl url;
    int length = 0;
    QString title;
    int seq = 0;
    friend QDebug operator<<(QDebug dbg, const HlsMediaItem &item);
};

struct HlsMasterPlaylist {
    QString title;
    std::vector<HlsMasterItem> items;
    friend QDebug operator<<(QDebug dbg, const HlsMasterPlaylist &playlist);
};

struct HlsMediaPlaylist {
    QString title;
    int targetDuration = 0;
    bool live = true;
    std::vector<HlsMediaItem> items;
    friend QDebug operator<<(QDebug dbg, const HlsMediaPlaylist &playlist);
};

HlsType hlsPlaylistType(const QByteArray &data);
std::optional<std::variant<HlsMasterPlaylist, HlsMediaPlaylist>> parseHls(
    const QByteArray &data, const QString &context = {});
std::optional<Playlist> parsePls(const QByteArray &data,
                                 const QString &context = {});
std::optional<Playlist> parseM3u(const QByteArray &data,
                                 const QString &context = {});
std::optional<Playlist> parseXspf(const QByteArray &data,
                                  const QString &context = {});
std::optional<Playlist> parsePlaylistFile(const QString &path);
void resolveM3u(QByteArray &data, const QString &context);
bool slidesItem(const PlaylistItem &item);
bool slidesPlaylist(const Playlist &playlist);
void toSlidesPlaylist(Playlist &playlist);
bool saveAsXspf(const Playlist &playlist, const QString &filePath);
}  // namespace PlaylistParser

#endif  // PLAYLISTPARSER_H
