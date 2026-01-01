/* Copyright (C) 2021-2025 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "playlistparser.h"

#include <qfileinfo.h>
#include <qnamespace.h>

#include <QDebug>
#include <QDir>
#include <QDomDocument>
#include <QFile>
#include <QFileInfo>
#include <QRegExp>
#include <QTextStream>
#include <map>
#include <optional>

#include "playlistmodel.h"
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
                    else if (sp.at(0) == QStringLiteral("CODECS")) {
                        item.codecs = sp.at(1);
                        if (item.codecs.size() > 1 &&
                            item.codecs.startsWith('"') &&
                            item.codecs.endsWith('"')) {
                            item.codecs =
                                item.codecs.mid(1, item.codecs.size() - 2);
                        }
                    }
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

    auto titles = doc.elementsByTagName(QStringLiteral("title"));
    for (int i = 0; i < titles.size(); ++i) {
        if (titles.at(i).parentNode() == doc.documentElement()) {
            playlist.title = titles.at(i).toElement().text();
            break;
        }
    }

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

std::optional<Playlist> parsePlaylistFile(const QString &path) {
    QFile file{path};
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "failed to open playlist file:" << file.fileName();
        return std::nullopt;
    }

    QFileInfo fi{file};

    auto ext = fi.suffix();
    if (ext.compare(QLatin1String{"pls"}, Qt::CaseInsensitive) == 0) {
        return parsePls(file.readAll(), fi.dir().absolutePath());
    }
    if (ext.compare(QLatin1String{"m3u"}, Qt::CaseInsensitive) == 0) {
        return parseM3u(file.readAll(), fi.dir().absolutePath());
    }
    if (ext.compare(QLatin1String{"xspf"}, Qt::CaseInsensitive) == 0) {
        return parseXspf(file.readAll(), fi.dir().absolutePath());
    }

    return std::nullopt;
}

bool slidesItem(const Item &item) {
    if (!item.url.isLocalFile()) return false;
    QFileInfo fileInfo{item.url.toLocalFile()};
    auto ext = fileInfo.suffix();
    if (ext.compare(QLatin1String{"jpg"}, Qt::CaseInsensitive) != 0 &&
        ext.compare(QLatin1String{"jpeg"}, Qt::CaseInsensitive) != 0 &&
        ext.compare(QLatin1String{"png"}, Qt::CaseInsensitive) != 0) {
        return false;
    }
    return true;
}

bool slidesPlaylist(const Playlist &playlist) {
    return std::all_of(playlist.items.cbegin(), playlist.items.cend(),
                       [](const Item &item) { return slidesItem(item); });
}

void toSlidesPlaylist(Playlist &playlist) {
    std::vector<Item> items;
    std::copy_if(std::make_move_iterator(playlist.items.begin()),
                 std::make_move_iterator(playlist.items.end()),
                 std::back_inserter(items),
                 [](const auto &item) { return slidesItem(item); });
    playlist.items = std::move(items);
}

bool saveAsXspf(const Playlist &playlist, const QString &filePath) {
    QFile file{filePath};
    if (!file.open(QFile::WriteOnly | QFile::Truncate)) {
        qWarning() << "failed to open file for writing:" << filePath;
        return false;
    }

    QTextStream out{&file};
    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        << "<playlist version=\"1\" xmlns=\"http://xspf.org/ns/0/\">\n";

    if (!playlist.title.isEmpty()) {
        out << "  <title>" << playlist.title << "</title>\n";
    }

    if (!playlist.items.empty()) {
        out << "  <trackList>\n";
        for (const auto &item : playlist.items) {
            out << "    <track>\n";
            if (!item.url.isEmpty()) {
                out << "      <location>" << item.url.toString()
                    << "</location>\n";
            }
            if (!item.title.isEmpty()) {
                out << "      <title>" << item.title << "</title>\n";
            }
            out << "    </track>\n";
        }
        out << "  </trackList>\n";
    }

    out << "</playlist>\n";

    return true;
}

}  // namespace PlaylistParser
