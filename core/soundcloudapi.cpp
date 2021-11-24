/* Copyright (C) 2021 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "soundcloudapi.h"

#include <QDebug>
#include <QTimer>
#include <QUrlQuery>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonArray>
#include <QJsonObject>
#include <QLocale>
#include <QThread>

#include "downloader.h"

QString SoundcloudApi::clientId;

SoundcloudApi::SoundcloudApi(std::shared_ptr<QNetworkAccessManager> nam, QObject *parent) :
    QObject{parent}, nam{nam},
    locale{QLocale::system().bcp47Name().split('-').first()}
{
    discoverClientId();
}

void SoundcloudApi::discoverClientId()
{
    if (!clientId.isEmpty()) return;

    auto output = downloadHtmlData(QUrl{"https://soundcloud.com"});
    if (!output) {
        qWarning() << "Cannot parse HTML data";
        return;
    }

    if (QThread::currentThread()->isInterruptionRequested()) return;

    auto scripts = gumbo::search_for_tag(output->root, GUMBO_TAG_SCRIPT);
    for (auto script : scripts) {
        if (QUrl url{gumbo::attr_data(script, "src")}; !url.isEmpty()) {
            if (auto data = Downloader{nam}.downloadData(url); !data.isEmpty()) {
                if (auto id = extractClientId(data); !id.isEmpty()) {
#ifdef QT_DEBUG
                    qDebug() << "Client id: " << id;
#endif
                    clientId = id;
                    return;
                }
            }
        }
        if (QThread::currentThread()->isInterruptionRequested()) return;
    }

    qWarning() << "Cannot find clientId";
}

bool SoundcloudApi::validUrl(const QUrl &url)
{
    if (url.host().contains("soundcloud.com", Qt::CaseInsensitive)) {
        return true;
    }
    return false;
}

QJsonDocument SoundcloudApi::parseJsonData(const QByteArray &data)
{
    QJsonParseError err;

    const auto json = QJsonDocument::fromJson(data, &err);

    if (err.error != QJsonParseError::NoError) {
        qWarning() << "Error parsing json:" << err.errorString();
    }

    return json;
}

gumbo::GumboOutput_ptr SoundcloudApi::downloadHtmlData(const QUrl &url)
{
    auto data = Downloader{nam}.downloadData(url);
    if (data.isEmpty()) {
        qWarning() << "No data received";
        return {};
    }

    return gumbo::parseHtmlData(data);
}

QJsonDocument SoundcloudApi::downloadJsonData(const QUrl &url)
{
    auto data = Downloader{nam}.downloadData(url);
    if (data.isEmpty()) {
        qWarning() << "No data received";
        return {};
    }

    return parseJsonData(data);
}

SoundcloudApi::Type SoundcloudApi::textToType(const QString &text)
{
    if (text.size() > 0) {
        if (text.at(0) == 'u') return SoundcloudApi::Type::User;
        if (text.at(0) == 't') return SoundcloudApi::Type::Track;
        if (text.at(0) == 'p') return SoundcloudApi::Type::Playlist;
    }

    return SoundcloudApi::Type::Unknown;
}

QString SoundcloudApi::extractData(const QString &text)
{
    //QRegExp rx("e\\.data\\.forEach\\(function\\(e\\)\\{n\\(e\\)\\}\\)\\}catch\\(e\\)\\{\\}\\}\\)\\}\\,(\\[.*\\])\\);$", Qt::CaseSensitive);
    QRegExp rx("window\\.__sc_hydration\\s=\\s(.*);$", Qt::CaseSensitive);
    if (rx.indexIn(text) != -1) return rx.cap(1);
    return {};
}

QString SoundcloudApi::extractClientId(const QString &text)
{
    QRegExp rx(",client_id:\"([\\0-9a-zA-Z]+)\",", Qt::CaseSensitive);
    if (rx.indexIn(text) != -1) return rx.cap(1);
    return {};
}

QString SoundcloudApi::extractUserId(const QString &text)
{
    QRegExp rx("soundcloud://users:([0-9]+)$", Qt::CaseSensitive);
    if (rx.indexIn(text) != -1) return rx.cap(1);
    return {};
}

QString SoundcloudApi::extractPlaylistId(const QString &text)
{
    QRegExp rx("soundcloud://playlists:([0-9]+)$", Qt::CaseSensitive);
    if (rx.indexIn(text) != -1) return rx.cap(1);
    return {};
}

QString SoundcloudApi::extractUsernameFromTitle(const QString &text)
{
    QRegExp rx("^(.+) \\| .+$", Qt::CaseSensitive);
    if (rx.indexIn(text) != -1) {
        return rx.cap(1);
    }
    return {};
}

void SoundcloudApi::addClientId(QUrl& url) const
{
    QUrlQuery query{url};
    if (query.hasQueryItem("client_id"))
        query.removeAllQueryItems("client_id");
    query.addQueryItem("client_id", clientId);
    url.setQuery(query);
}

QJsonArray SoundcloudApi::extractItems(const QUrl &url)
{
    QJsonArray items;

    auto output = downloadHtmlData(url);
    if (!output) {
        qWarning() << "Cannot parse HTML data";
        return items;
    }

    if (QThread::currentThread()->isInterruptionRequested()) return items;

    QJsonArray chunks;
    auto scripts = gumbo::search_for_tag(output->root, GUMBO_TAG_SCRIPT);
    for (auto script : scripts) {
        if (auto data = extractData(gumbo::node_text(script)); !data.isEmpty()) {
            const auto json = parseJsonData(data.toUtf8());
            if (json.isNull() || !json.isArray()) {
                qWarning() << "Cannot parse JSON data";
            } else {
                chunks = json.array();
            }
            break;
        }
        if (QThread::currentThread()->isInterruptionRequested()) return items;
    }

    if (chunks.isEmpty()) {
        qWarning() << "Empty chunks data";
        return items;
    }

    for (int i = 0; i < chunks.size(); ++i) {
        const auto elm = chunks.at(i).toObject().value("data").toObject();
        if (!elm.value("kind").toString().isEmpty()) items.append(elm);
    }

    return items;
}

SoundcloudApi::Track SoundcloudApi::track(const QUrl &url)
{
    Track track;

    const auto items = extractItems(url);
    if (items.isEmpty()) {
        qWarning() << "No items";
        return track;
    }

    if (QThread::currentThread()->isInterruptionRequested()) return track;

    for (int i = 0; i < items.size(); ++i) {
        auto elm = items.at(i).toObject();
        auto type = textToType(elm.value("kind").toString());
        if (type != Type::Track) {
            continue;
        }

        const auto media = elm.value("media").toObject().value("transcodings").toArray();
        if (media.isEmpty()) return track;

        for (int i = 0; i < media.size(); ++i) {
            auto m = media.at(i).toObject();
            // TODO: Support for HLS urls
            if (m.value("format").toObject().value("protocol").toString() != "progressive" ||
                m.value("format").toObject().value("mime_type").toString() != "audio/mpeg") {
                continue;
            }

            QUrl url{m.value("url").toString()};
            if (url.isEmpty()) {
                qWarning() << "Empty url";
                return track;
            }

            addClientId(url);

            auto json = downloadJsonData(url);
            if (json.isNull() || !json.isObject()) {
                qWarning() << "Cannot parse JSON data";
                return track;
            }

            track.streamUrl = QUrl{json.object().value("url").toString()};
            track.duration = int(m.value("duration").toDouble() / 1000);

            if (QThread::currentThread()->isInterruptionRequested()) return track;

            break;
        }

        if (track.streamUrl.isEmpty()) {
            qWarning() << "Stream Url is empty";
            return track;
        }

        track.webUrl = QUrl{elm.value("permalink_url").toString()};
        track.imageUrl = QUrl{elm.value("artwork_url").toString()};
        track.title = elm.value("title").toString();
        track.artist = elm.value("publisher_metadata").toObject().value("artist").toString();
        if (track.artist.isEmpty())
            track.artist = elm.value("user").toObject().value("username").toString();
        track.album = elm.value("publisher_metadata").toObject().value("album_title").toString();

        return track;
    }

    qWarning() << "No valid chunk";
    return track;
}

bool SoundcloudApi::mediaOk(const QJsonArray &media)
{
    for (int i = 0; i < media.size(); ++i) {
        auto m = media.at(i).toObject();
        // TODO: Support for HLS urls
        if (m.value("format").toObject().value("protocol").toString() != "progressive" ||
            m.value("format").toObject().value("mime_type").toString() != "audio/mpeg") {
            continue;
        }

        if (m.value("url").toString().isEmpty()) continue;

        return true;
    }

    return false;
}

SoundcloudApi::Playlist SoundcloudApi::playlist(const QUrl &url)
{
    Playlist playlist;

    auto output = downloadHtmlData(url);
    if (!output) {
        qWarning() << "Cannot parse HTML data";
        return playlist;
    }

    if (QThread::currentThread()->isInterruptionRequested()) return playlist;

    QString playlistId;

    auto metas = gumbo::search_for_tag(output->root, GUMBO_TAG_META);
    for (auto meta : metas) {
        if (auto content = gumbo::attr_data(meta, "content"); !content.isEmpty()) {
            playlistId = extractPlaylistId(content);
            if (!playlistId.isEmpty()) break;
        }
    }

    if (playlistId.isEmpty()) {
        qWarning() << "Empty playlist id";
        return playlist;
    }

    auto json = downloadJsonData(makePlaylistUrl(playlistId));
    if (json.isNull() || !json.isObject()) {
        qWarning() << "Cannot parse JSON data";
        return playlist;
    }

    if (QThread::currentThread()->isInterruptionRequested()) return playlist;

    auto pobj = json.object();
    playlist.title = pobj.value("title").toString();
    playlist.imageUrl = QUrl{pobj.value("artwork_url").toString()};
    playlist.artist = pobj.value("user").toObject().value("username").toString();

    auto tracks = pobj.value("tracks").toArray();
    if (tracks.isEmpty()) {
        qWarning() << "No tracks for playlist:" << playlistId;
        return playlist;
    }

    for (int i = 0; i < tracks.size(); ++i) {
        auto elm = tracks.at(i).toObject();
        auto type = textToType(elm.value("kind").toString());
        if (type != Type::Track) continue;
        if (!mediaOk(elm.value("media").toObject().value("transcodings").toArray())) continue;

        QUrl webUrl{elm.value("permalink_url").toString()};
        if (webUrl.isEmpty()) continue;

        PlaylistTrack track;
        track.imageUrl = QUrl{elm.value("artwork_url").toString()};
        track.title = elm.value("title").toString();
        track.webUrl = std::move(webUrl);
        track.artist = elm.value("publisher_metadata").toObject().value("artist").toString();
        if (track.artist.isEmpty())
            track.artist = elm.value("user").toObject().value("username").toString();
        track.album = elm.value("publisher_metadata").toObject().value("album_title").toString();
        if (track.album.isEmpty())
            track.artist = playlist.title;

        playlist.tracks.push_back(std::move(track));
    }

    return playlist;
}

void SoundcloudApi::user(const QUrl &url, User &user, int count)
{
    if (count == 5) return;

    auto json = downloadJsonData(url);
    if (json.isNull() || !json.isObject()) {
        qWarning() << "Cannot parse JSON data";
        return;
    }

    if (QThread::currentThread()->isInterruptionRequested())
        return;

    auto items = json.object().value("collection").toArray();
    if (items.isEmpty()) {
        qWarning() << "No items";
        return;
    }

    for (int i = 0; i < items.size(); ++i) {
        auto elm = items.at(i).toObject();
        auto type = textToType(elm.value("type").toString());
        if (type == Type::Playlist) {
            elm = elm.value("playlist").toObject();

            QUrl webUrl{elm.value("permalink_url").toString()};
            if (webUrl.isEmpty()) continue;

            UserPlaylist playlist;
            playlist.imageUrl = QUrl{elm.value("artwork_url").toString()};
            playlist.title = elm.value("title").toString();
            playlist.webUrl = std::move(webUrl);
            playlist.artist = elm.value("user").toObject().value("username").toString();

            user.playlists.push_back(std::move(playlist));
        } else if (type == Type::Track) {
            elm = elm.value("track").toObject();

            if (!mediaOk(elm.value("media").toObject().value("transcodings").toArray())) continue;

            QUrl webUrl{elm.value("permalink_url").toString()};
            if (webUrl.isEmpty()) continue;

            UserTrack track;
            track.imageUrl = QUrl{elm.value("artwork_url").toString()};
            track.title = elm.value("title").toString();
            track.webUrl = std::move(webUrl);
            track.artist = elm.value("publisher_metadata").toObject().value("artist").toString();
            if (track.artist.isEmpty()) track.artist = elm.value("user").toObject().value("username").toString();

            user.tracks.push_back(std::move(track));
        }
    }

    QUrl nextUrl{json.object().value("next_href").toString()};
    if (nextUrl.isEmpty()) return;

    json = {};
    items = {};

    if (QThread::currentThread()->isInterruptionRequested()) return;

    addClientId(nextUrl);
    this->user(nextUrl, user, count + 1);
}

SoundcloudApi::User SoundcloudApi::user(const QUrl &url)
{
    User user;

    auto output = downloadHtmlData(url);
    if (!output) {
        qWarning() << "Cannot parse HTML data";
        return user;
    }

    if (QThread::currentThread()->isInterruptionRequested()) return user;

    QString userId;

    auto metas = gumbo::search_for_tag(output->root, GUMBO_TAG_META);
    for (auto meta : metas) {
        if (auto content = gumbo::attr_data(meta, "content"); !content.isEmpty()) {
            userId = extractUserId(content);
            if (!userId.isEmpty()) break;
        }
    }

    if (userId.isEmpty()) {
        qWarning() << "Empty user id";
        return user;
    }

    this->user(makeUserStreamUrl(userId), user);

    user.name = extractUsernameFromTitle(
                gumbo::node_text(gumbo::search_for_tag_one(output->root, GUMBO_TAG_TITLE)));
    user.webUrl = url;

    return user;

    /*auto json = downloadJsonData(makeUserPlaylistsUrl(userId));
    if (json.isNull() || !json.isObject()) {
        qWarning() << "Cannot parse JSON data";
        return user;
    }

    auto playlists = json.object().value("collection").toArray();
    if (playlists.isEmpty()) {
        qWarning() << "No playlists for user:" << userId;
        return user;
    }

    for (int i = 0; i < playlists.size(); ++i) {
        auto elm = playlists.at(i).toObject();
        auto type = textToType(elm.value("kind").toString());
        if (type != Type_Playlist)
            continue;

        QUrl webUrl{elm.value("permalink_url").toString()};
        if (webUrl.isEmpty())
            continue;

        UserPlaylist playlist;
        playlist.imageUrl = QUrl{elm.value("artwork_url").toString()};
        playlist.title = elm.value("title").toString();
        playlist.webUrl = std::move(webUrl);
        playlist.artist = elm.value("user").toObject().value("username").toString();

        user.playlists.push_back(std::move(playlist));
    }*/

    /*auto json = downloadJsonData(makeUserStreamUrl(userId));
    if (json.isNull() || !json.isObject()) {
        qWarning() << "Cannot parse JSON data";
        return user;
    }

    auto items = json.object().value("collection").toArray();
    if (items.isEmpty()) {
        qWarning() << "No items for user:" << userId;
        return user;
    }

    for (int i = 0; i < items.size(); ++i) {
        auto elm = items.at(i).toObject();
        auto type = textToType(elm.value("type").toString());
        if (type == Type_Playlist) {
            elm = elm.value("playlist").toObject();

            QUrl webUrl{elm.value("permalink_url").toString()};
            if (webUrl.isEmpty())
                continue;

            UserPlaylist playlist;
            playlist.imageUrl = QUrl{elm.value("artwork_url").toString()};
            playlist.title = elm.value("title").toString();
            playlist.webUrl = std::move(webUrl);
            playlist.artist = elm.value("user").toObject().value("username").toString();

            user.playlists.push_back(std::move(playlist));
        } else if (type == Type_Track) {
            elm = elm.value("track").toObject();

            if (!mediaOk(elm.value("media").toObject().value("transcodings").toArray()))
                continue;

            QUrl webUrl{elm.value("permalink_url").toString()};
            if (webUrl.isEmpty())
                continue;

            UserTrack track;
            track.imageUrl = QUrl{elm.value("artwork_url").toString()};
            track.title = elm.value("title").toString();
            track.webUrl = std::move(webUrl);
            track.artist = elm.value("publisher_metadata").toObject().value("artist").toString();
            if (track.artist.isEmpty())
                track.artist = elm.value("user").toObject().value("username").toString();

            user.tracks.push_back(std::move(track));
        }
    }*/
}

std::vector<SoundcloudApi::SearchResultItem> SoundcloudApi::search(const QString &query)
{
    std::vector<SearchResultItem> items;

    auto json = downloadJsonData(makeSearchUrl(query));
    if (json.isNull() || !json.isObject()) {
        qWarning() << "Cannot parse JSON data";
        return items;
    }

    if (QThread::currentThread()->isInterruptionRequested()) return items;

    auto res = json.object().value("collection").toArray();

    for (int i = 0; i < res.size(); ++i) {
        auto elm = res.at(i).toObject();
        auto type = textToType(elm.value("kind").toString());
        if (type == Type::Unknown) {
            continue;
        }

        QUrl webUrl{elm.value("permalink_url").toString()};
        if (webUrl.isEmpty()) continue;

        SearchResultItem item;
        item.type = type;
        item.webUrl = std::move(webUrl);

        if (type == Type::Track) {
            item.imageUrl = QUrl{elm.value("artwork_url").toString()};
            item.title = elm.value("title").toString();
            item.artist = elm.value("publisher_metadata").toObject().value("artist").toString();
            if (item.artist.isEmpty()) item.artist = elm.value("user").toObject().value("username").toString();
        } else if (type == Type::User) {
            item.imageUrl = QUrl{elm.value("avatar_url").toString()};
            item.artist = elm.value("username").toString();
        } else if (type == Type::Playlist) {
            item.imageUrl = QUrl{elm.value("avatar_url").toString()};
            item.album = elm.value("title").toString();
            item.artist = elm.value("user").toObject().value("username").toString();
        }

        items.push_back(std::move(item));
    }

    return items;
}

QUrl SoundcloudApi::makeSearchUrl(const QString &phrase) const
{
    QUrlQuery qurl;
    qurl.addQueryItem("q", phrase);
    qurl.addQueryItem("app_locale", locale);
    qurl.addQueryItem("client_id", clientId);
    qurl.addQueryItem("limit", "50");
    qurl.addQueryItem("offset", "0");
    QUrl url{"https://api-v2.soundcloud.com/search"};
    url.setQuery(qurl);
    return url;
}

QUrl SoundcloudApi::makePlaylistUrl(const QString &id) const
{
    QUrlQuery qurl;
    qurl.addQueryItem("app_locale", locale);
    qurl.addQueryItem("client_id", clientId);
    QUrl url{"https://api-v2.soundcloud.com/playlists/" + id};
    url.setQuery(qurl);
    return url;
}

QUrl SoundcloudApi::makeTrackUrl(const QString &id) const
{
    QUrlQuery qurl;
    qurl.addQueryItem("app_locale", locale);
    qurl.addQueryItem("client_id", clientId);
    QUrl url{"https://api-v2.soundcloud.com/tracks/" + id};
    url.setQuery(qurl);
    return url;
}

QUrl SoundcloudApi::makeUserPlaylistsUrl(const QString &id) const
{
    QUrlQuery qurl;
    qurl.addQueryItem("app_locale", locale);
    qurl.addQueryItem("client_id", clientId);
    qurl.addQueryItem("limit", "50");
    qurl.addQueryItem("offset", "0");
    qurl.addQueryItem("linked_partitioning", "1");
    QUrl url{"https://api-v2.soundcloud.com/users/" + id + "/playlists"};
    url.setQuery(qurl);
    return url;
}

QUrl SoundcloudApi::makeUserStreamUrl(const QString &id) const
{
    QUrlQuery qurl;
    qurl.addQueryItem("app_locale", locale);
    qurl.addQueryItem("client_id", clientId);
    qurl.addQueryItem("limit", "50");
    qurl.addQueryItem("offset", "0");
    qurl.addQueryItem("linked_partitioning", "1");
    QUrl url{"https://api-v2.soundcloud.com/stream/users/" + id};
    url.setQuery(qurl);
    return url;
}

