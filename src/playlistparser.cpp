/* Copyright (C) 2021-2023 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "playlistparser.h"

#include <QDebug>
#include <QDomDocument>
#include <QRegExp>
#include <QTextStream>
#include <map>
#include <string_view>

#include "utils.h"

namespace PlaylistParser {

QDebug operator<<(QDebug dbg, HlsType type) {
    switch (type) {
        case HlsType::Unknown:
            dbg << "unknown";
            break;
        case HlsType::MasterPlaylist:
            dbg << "master-playlist";
            break;
        case HlsType::MediaPlaylist:
            dbg << "media-playlist";
            break;
    }
    return dbg;
}

QDebug operator<<(QDebug dbg, const Item &item) {
    dbg << "url=" << item.url << ", length=" << item.length
        << ", title=" << item.title;
    return dbg;
}

QDebug operator<<(QDebug dbg, const Playlist &playlist) {
    dbg << "title=" << playlist.title << ", items=";
    for (const auto &item : playlist.items) dbg << "[" << item << "], ";
    return dbg;
}

QDebug operator<<(QDebug dbg, const HlsMasterItem &item) {
    dbg << "url=" << item.url << ", bandwidth=" << item.bandwidth
        << ", codecs=" << item.codecs;
    return dbg;
}

QDebug operator<<(QDebug dbg, const HlsMediaItem &item) {
    dbg << "url=" << item.url << ", length=" << item.length
        << ", title=" << item.title << ", seq=" << item.seq;
    return dbg;
}

QDebug operator<<(QDebug dbg, const HlsMasterPlaylist &playlist) {
    dbg << "title=" << playlist.title << ", items=";
    for (const auto &item : playlist.items) dbg << "[" << item << "], ";
    return dbg;
}

QDebug operator<<(QDebug dbg, const HlsMediaPlaylist &playlist) {
    dbg << "title=" << playlist.title
        << ", targetDuration=" << playlist.targetDuration
        << ", live=" << playlist.live << ", items=";
    for (const auto &item : playlist.items) dbg << "[" << item << "], ";
    return dbg;
}

HlsType hlsPlaylistType(const QByteArray &data) {
    if (data.contains("#EXT-X-")) {
        if (data.contains(m3utag_extxstreaminf)) return HlsType::MasterPlaylist;
        if (data.contains(m3utag_extinf)) return HlsType::MediaPlaylist;
    }
    return HlsType::Unknown;
}

std::optional<std::variant<HlsMasterPlaylist, HlsMediaPlaylist>> parseHls(
    const QByteArray &data, const QString &context) {
    if (!data.startsWith(m3utag_extm3u)) {
        qWarning() << "not m3u playlist:" << data;
        return std::nullopt;
    }

    auto type = hlsPlaylistType(data);

    if (type == HlsType::Unknown) {
        qWarning() << "not hls playlist:" << data;
        return std::nullopt;
    }

    QTextStream s(data, QIODevice::ReadOnly);
    s.setAutoDetectUnicode(true);

    if (type == HlsType::MasterPlaylist) {
        HlsMasterPlaylist playlist;

        HlsMasterItem item;

        while (!s.atEnd()) {
            const auto line = s.readLine();
            if (line.startsWith(m3utag_playlist)) {
                auto s = line.right(line.size() - strlen(m3utag_playlist))
                             .split(',');
                if (!s.empty()) playlist.title = std::move(s.front());
            } else if (line.startsWith(m3utag_extxstreaminf)) {
                auto s = line.right(line.size() - strlen(m3utag_extxstreaminf))
                             .split(',');
                if (s.empty()) continue;
                for (auto &p : s) {
                    auto sp = p.split('=');
                    if (sp.size() < 2) continue;
                    if (sp.at(0) == QStringLiteral("BANDWIDTH"))
                        item.bandwidth = sp.at(1).toInt();
                    else if (sp.at(0) == QStringLiteral("CODECS"))
                        item.codecs = sp.at(1);
                }
            } else if (item.bandwidth > 0) {
                auto url = Utils::urlFromText(line, context);
                if (!url.isEmpty()) {
                    item.url = std::move(url);
                    playlist.items.push_back(std::move(item));
                }
                item = HlsMasterItem{};
            }
        }

        if (playlist.items.empty()) {
            qWarning() << "playlist does not contain any items";
            return std::nullopt;
        }

        return playlist;
    }

    HlsMediaPlaylist playlist;

    HlsMediaItem item;
    int seq = 0;
    bool haveExtinf = false;

    while (!s.atEnd()) {
        const auto line = s.readLine();
        if (line.startsWith(m3utag_playlist)) {
            auto s =
                line.right(line.size() - strlen(m3utag_playlist)).split(',');
            if (!s.empty()) playlist.title = std::move(s.front());
        } else if (line.startsWith(m3utag_extxtargetduration)) {
            auto s = line.right(line.size() - strlen(m3utag_extxtargetduration))
                         .split(',');
            if (!s.empty())
                playlist.targetDuration =
                    static_cast<int>(s.front().toDouble());
        } else if (line.startsWith(m3utag_extxmediasequence)) {
            auto s = line.right(line.size() - strlen(m3utag_extxmediasequence))
                         .split(',');
            if (!s.empty()) seq = s.front().toInt();
        } else if (line.startsWith(m3utag_extxendlist)) {
            playlist.live = false;
        } else if (line.startsWith(m3utag_extinf)) {
            auto s = line.right(line.size() - strlen(m3utag_extinf)).split(',');
            if (s.empty()) continue;
            item.length = static_cast<int>(s.front().toDouble());
            if (s.size() > 1) item.title = s.back();
            haveExtinf = true;
        } else if (haveExtinf) {
            auto url = Utils::urlFromText(line, context);
            if (!url.isEmpty()) {
                item.url = std::move(url);
                item.seq = seq;
                ++seq;
                playlist.items.push_back(std::move(item));
                item = HlsMediaItem{};
            }
            haveExtinf = false;
        }
    }

    if (playlist.items.empty()) {
        qWarning() << "playlist does not contain any items";
        return std::nullopt;
    }

    return playlist;
}

std::optional<Playlist> parseM3u(const QByteArray &data,
                                 const QString &context) {
    if (!data.startsWith(m3utag_extm3u)) {
        qWarning() << "extm3u tag is missing";
    }

    Playlist playlist;

    QTextStream s(data, QIODevice::ReadOnly);
    s.setAutoDetectUnicode(true);

    Item item;

    while (!s.atEnd()) {
        const auto line = s.readLine();
        if (line.startsWith(m3utag_playlist)) {
            auto s =
                line.right(line.size() - strlen(m3utag_playlist)).split(',');
            if (!s.empty()) playlist.title = std::move(s.front());
        } else if (line.startsWith(m3utag_extinf)) {
            auto s = line.right(line.size() - strlen(m3utag_extinf)).split(',');
            if (!s.empty()) {
                item.length = static_cast<int>(s.front().toDouble());
                if (s.size() > 1) item.title = s.back();
            }
        } else {
            auto url = Utils::urlFromText(line, context);
            if (!url.isEmpty()) {
                item.url = std::move(url);
                playlist.items.push_back(std::move(item));
            }
        }
    }

    if (playlist.items.empty()) {
        qWarning() << "playlist does not contain any items";
        return std::nullopt;
    }

    return playlist;
}

std::optional<Playlist> parseXspf(const QByteArray &data,
                                  const QString &context) {
    QDomDocument doc;

    QString error;
    if (!doc.setContent(data, false, &error)) {
        qWarning() << "xspf playlist parse error:" << error;
        return std::nullopt;
    }

    Playlist playlist;

    auto tracks = doc.elementsByTagName(QStringLiteral("track"));

    playlist.items.reserve(tracks.size());

    for (int i = 0; i < tracks.size(); ++i) {
        auto track = tracks.at(i).toElement();
        if (track.isNull()) continue;

        auto ls = track.elementsByTagName(QStringLiteral("location"));
        if (ls.isEmpty()) continue;

        auto l = ls.at(0).toElement();
        if (l.isNull()) continue;

        auto url = Utils::urlFromText(l.text(), context);
        if (url.isEmpty()) continue;

        Item item;
        item.url = std::move(url);

        auto ts = track.elementsByTagName(QStringLiteral("title"));
        if (!ts.isEmpty()) {
            auto t = ts.at(0).toElement();
            if (!t.isNull()) item.title = t.text();
        }

        auto ds = track.elementsByTagName(QStringLiteral("duration"));
        if (!ds.isEmpty()) {
            auto d = ds.at(0).toElement();
            if (!d.isNull())
                item.length = static_cast<int>(d.text().toDouble());
        }

        playlist.items.push_back(item);
    }

    if (playlist.items.empty()) {
        qWarning() << "playlist does not contain any items";
        return std::nullopt;
    }

    return playlist;
}

std::optional<Playlist> parsePls(const QByteArray &data,
                                 const QString &context) {
    if (!data.startsWith("[playlist]")) {
        qWarning() << "not pls playlist:" << data;
        return std::nullopt;
    }

    std::map<int, Item> map;

    // urls
    QRegExp rxFile{"\\nFile(\\d\\d?)=([^\\n]*)", Qt::CaseInsensitive};
    int pos = 0;
    while ((pos = rxFile.indexIn(data, pos)) != -1) {
        auto cap1 = rxFile.cap(1);
        auto cap2 = rxFile.cap(2);

        bool ok;
        int n = cap1.toInt(&ok);
        if (ok) {
            auto url = Utils::urlFromText(cap2, context);
            if (!url.isEmpty()) map[n].url = std::move(url);
        }

        pos += rxFile.matchedLength();
    }

    if (map.empty()) {
        qWarning() << "playlist does not contain any items";
        return std::nullopt;
    }

    // titles
    QRegExp rxTitle{"\\nTitle(\\d\\d?)=([^\\n]*)", Qt::CaseInsensitive};
    pos = 0;
    while ((pos = rxTitle.indexIn(data, pos)) != -1) {
        auto cap1 = rxTitle.cap(1);
        auto cap2 = rxTitle.cap(2);

        bool ok;
        int n = cap1.toInt(&ok);
        if (ok && map.count(n) > 0) map[n].title = cap2;

        pos += rxTitle.matchedLength();
    }

    // length
    QRegExp rxLength{"\\nLength(\\d\\d?)=([^\\n]*)", Qt::CaseInsensitive};
    pos = 0;
    while ((pos = rxLength.indexIn(data, pos)) != -1) {
        auto cap1 = rxLength.cap(1);
        auto cap2 = rxLength.cap(2);

        bool ok;
        int n = cap1.toInt(&ok);
        if (ok && map.count(n) > 0) {
            int length = static_cast<int>(cap2.toDouble());
            map[n].length = length < 0 ? 0 : length;
        }

        pos += rxLength.matchedLength();
    }

    Playlist playlist;

    playlist.items.reserve(map.size());
    for (auto &p : map) playlist.items.push_back(std::move(p.second));

    return playlist;
}

void resolveM3u(QByteArray &data, const QString &context) {
    QTextStream s(data, QIODevice::ReadOnly);
    s.setAutoDetectUnicode(true);

    std::vector<QString> lines;

    while (!s.atEnd()) {
        auto line = s.readLine();
        if (!line.startsWith('#')) lines.push_back(std::move(line));
    }

    for (const auto &line : lines) {
        auto url = Utils::urlFromText(line, context);
        if (!url.isEmpty())
            data.replace(line.toUtf8(), url.toString().toUtf8());
    }
}
}  // namespace PlaylistParser
