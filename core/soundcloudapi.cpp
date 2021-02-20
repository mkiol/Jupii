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
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QEventLoop>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonArray>
#include <QJsonObject>
#include <QLocale>

QString SoundcloudApi::clientId;

SoundcloudApi::SoundcloudApi(std::shared_ptr<QNetworkAccessManager> nam, QObject *parent) :
    QObject(parent), nam(nam),
    locale(QLocale::system().bcp47Name().split('-').first())
{
    discoverClientId();
}

bool SoundcloudApi::discoverClientId()
{
    if (!clientId.isEmpty())
        return true;

    auto output = downloadHtmlData(QUrl{"https://soundcloud.com"});
    if (!output) {
        qWarning() << "Cannot parse HTML data";
        return false;
    }

    auto scripts = gumbo::search_for_tag(output->root, GUMBO_TAG_SCRIPT);
    for (auto script : scripts) {
        if (QUrl url{gumbo::attr_data(script, "src")}; !url.isEmpty()) {
            if (auto data = downloadData(url); !data.isEmpty()) {
                if (auto id = extractClientId(data); !id.isEmpty()) {
                    qWarning() << "client id: " << id;
                    clientId = id;
                    return true;
                }
            }
        }
    }

    return false;
}

bool SoundcloudApi::validUrl(const QUrl &url)
{
    auto str = url.host();
    if (str.contains("soundcloud.com", Qt::CaseInsensitive)) {
        return true;
    }
    return false;
}

QByteArray SoundcloudApi::downloadData(const QUrl &url)
{
    qDebug() << url;
    QByteArray data;

    QNetworkRequest request;
    request.setUrl(url);
    request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);

    QNetworkReply* reply;
    std::unique_ptr<QNetworkAccessManager> priv_nam;

    if (nam) {
        reply = nam->get(request);
    } else {
        priv_nam = std::make_unique<QNetworkAccessManager>();
        reply = priv_nam->get(request);
    }

    QTimer::singleShot(httpTimeout, reply, &QNetworkReply::abort);
    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, this, [&loop, reply, &data] {
        auto err = reply->error();
        if (err == QNetworkReply::NoError) {
            data = reply->readAll();
            loop.quit();
        } else {
            qWarning() << "Error:" << err << reply->url();
            loop.exit(1);
        }
    });

    if (loop.exec() == 1) {
        qWarning() << "Cannot download data:" << reply->url();
    }

    reply->deleteLater();

    return data;
}

QJsonDocument SoundcloudApi::parseJsonData(const QByteArray &data)
{
    QJsonParseError err;

    auto json = QJsonDocument::fromJson(data, &err);

    if (err.error != QJsonParseError::NoError) {
        qWarning() << "Error parsing json:" << err.errorString();
    }

    return json;
}

gumbo::GumboOutput_ptr SoundcloudApi::downloadHtmlData(const QUrl &url)
{
    auto data = downloadData(url);
    if (data.isEmpty()) {
        qWarning() << "No data received";
        return {};
    }

    return gumbo::parseHtmlData(data);
}

QJsonDocument SoundcloudApi::downloadJsonData(const QUrl &url)
{
    auto data = downloadData(url);
    if (data.isEmpty()) {
        qWarning() << "No data received";
        return {};
    }

    return parseJsonData(data);
}

SoundcloudApi::Type SoundcloudApi::textToType(const QString &text)
{
    if (text.size() > 0) {
        if (text.at(0) == 'u')
            return SoundcloudApi::Type::Type_User;
        if (text.at(0) == 't')
            return SoundcloudApi::Type::Type_Track;
        if (text.at(0) == 'p')
            return SoundcloudApi::Type::Type_Playlist;
    }

    return SoundcloudApi::Type::Type_Unknown;
}

QString SoundcloudApi::extractData(const QString &text)
{
    QRegExp rx("e\\.data\\.forEach\\(function\\(e\\)\\{n\\(e\\)\\}\\)\\}catch\\(e\\)\\{\\}\\}\\)\\}\\,(\\[.*\\])\\);$", Qt::CaseSensitive);
    if (rx.indexIn(text) != -1)
        return rx.cap(1);
    return {};
}

QString SoundcloudApi::extractClientId(const QString &text)
{
    QRegExp rx(",client_id:\"([\\0-9a-zA-Z]+)\",", Qt::CaseSensitive);
    if (rx.indexIn(text) != -1)
        return rx.cap(1);
    return {};
}

QString SoundcloudApi::extractUserId(const QString &text)
{
    QRegExp rx("soundcloud://users:([0-9]+)$", Qt::CaseSensitive);
    if (rx.indexIn(text) != -1)
        return rx.cap(1);
    return {};
}

QString SoundcloudApi::extractPlaylistId(const QString &text)
{
    QRegExp rx("soundcloud://playlists:([0-9]+)$", Qt::CaseSensitive);
    if (rx.indexIn(text) != -1)
        return rx.cap(1);
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
    auto output = downloadHtmlData(url);
    if (!output) {
        qWarning() << "Cannot parse HTML data";
        return {};
    }

    QJsonArray chunks;
    auto scripts = gumbo::search_for_tag(output->root, GUMBO_TAG_SCRIPT);
    for (auto script : scripts) {
        if (auto data = extractData(gumbo::node_text(script)); !data.isEmpty()) {
            auto json = parseJsonData(data.toUtf8());
            if (json.isNull() || !json.isArray())
                qWarning() << "Cannot parse JSON data";
            else
                chunks = json.array();
            break;
        }
    }

    if (chunks.isEmpty()) {
        qWarning() << "Empty chunks data";
        return {};
    }

    QJsonArray items;

    for (int i = 0; i < chunks.size(); ++i) {
        auto res = chunks.at(i).toObject().value("data").toArray();
        if (res.isEmpty())
            continue;

        for (int i = 0; i < res.size(); ++i) {
            auto elm = res.at(i).toObject();
            if (!elm.value("kind").toString().isEmpty())
                items.append(elm);
        }
    }

    return items;
}

SoundcloudApi::Track SoundcloudApi::track(const QUrl &url)
{
    Track track;

    auto items = extractItems(url);
    if (items.isEmpty()) {
        qWarning() << "No items";
        return track;
    }

    for (int i = 0; i < items.size(); ++i) {
        auto elm = items.at(i).toObject();
        auto type = textToType(elm.value("kind").toString());
        if (type != Type_Track) {
            continue;
        }

        auto media = elm.value("media").toObject().value("transcodings").toArray();
        if (media.isEmpty())
            return track;

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
            track.duration = int(m.value("duration").toDouble());

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

        if (m.value("url").toString().isEmpty())
            continue;

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

    QString playlistId;

    auto metas = gumbo::search_for_tag(output->root, GUMBO_TAG_META);
    for (auto meta : metas) {
        if (auto content = gumbo::attr_data(meta, "content"); !content.isEmpty()) {
            playlistId = extractPlaylistId(content);
            if (!playlistId.isEmpty())
                break;
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
        if (type != Type_Track)
            continue;
        if (!mediaOk(elm.value("media").toObject().value("transcodings").toArray()))
            continue;

        QUrl webUrl{elm.value("permalink_url").toString()};
        if (webUrl.isEmpty())
            continue;

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
    if (count == 5)
        return;

    auto json = downloadJsonData(url);
    if (json.isNull() || !json.isObject()) {
        qWarning() << "Cannot parse JSON data";
        return;
    }

    auto items = json.object().value("collection").toArray();
    if (items.isEmpty()) {
        qWarning() << "No items";
        return;
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
    }

    QUrl nextUrl{json.object().value("next_href").toString()};
    if (nextUrl.isEmpty())
        return;

    json = QJsonDocument{};
    items = QJsonArray{};

    addClientId(nextUrl);
    this->user(nextUrl, user, count + 1);
    return;
}

SoundcloudApi::User SoundcloudApi::user(const QUrl &url)
{
    User user;

    auto output = downloadHtmlData(url);
    if (!output) {
        qWarning() << "Cannot parse HTML data";
        return user;
    }

    QString userId;

    auto metas = gumbo::search_for_tag(output->root, GUMBO_TAG_META);
    for (auto meta : metas) {
        if (auto content = gumbo::attr_data(meta, "content"); !content.isEmpty()) {
            userId = extractUserId(content);
            if (!userId.isEmpty())
                break;
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

    auto res = json.object().value("collection").toArray();

    for (int i = 0; i < res.size(); ++i) {
        auto elm = res.at(i).toObject();
        auto type = textToType(elm.value("kind").toString());
        if (type == Type_Unknown) {
            continue;
        }

        QUrl webUrl{elm.value("permalink_url").toString()};
        if (webUrl.isEmpty())
            continue;

        SearchResultItem item;
        item.type = type;
        item.webUrl = std::move(webUrl);

        if (type == Type_Track) {
            item.imageUrl = QUrl{elm.value("artwork_url").toString()};
            item.title = elm.value("title").toString();
            item.artist = elm.value("publisher_metadata").toObject().value("artist").toString();
            if (item.artist.isEmpty())
                item.artist = elm.value("user").toObject().value("username").toString();
        } else if (type == Type_User) {
            item.imageUrl = QUrl{elm.value("avatar_url").toString()};
            item.artist = elm.value("username").toString();
        } else if (type == Type_Playlist) {
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
    QUrl url("https://api-v2.soundcloud.com/search");
    url.setQuery(qurl);
    return url;
}

QUrl SoundcloudApi::makePlaylistUrl(const QString &id) const
{
    QUrlQuery qurl;
    qurl.addQueryItem("app_locale", locale);
    qurl.addQueryItem("client_id", clientId);
    QUrl url("https://api-v2.soundcloud.com/playlists/" + id);
    url.setQuery(qurl);
    return url;
}

QUrl SoundcloudApi::makeTrackUrl(const QString &id) const
{
    QUrlQuery qurl;
    qurl.addQueryItem("app_locale", locale);
    qurl.addQueryItem("client_id", clientId);
    QUrl url("https://api-v2.soundcloud.com/tracks/" + id);
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
    QUrl url("https://api-v2.soundcloud.com/users/" + id + "/playlists");
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
    QUrl url("https://api-v2.soundcloud.com/stream/users/" + id);
    url.setQuery(qurl);
    return url;
}

