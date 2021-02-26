/* Copyright (C) 2020 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "bcapi.h"

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

#include "gumbotools.h"
#include "utils.h"

BcApi::BcApi(std::shared_ptr<QNetworkAccessManager> nam, QObject *parent) :
    QObject(parent), nam(nam)
{
}

bool BcApi::validUrl(const QUrl &url)
{
    auto str = url.host();

    if (str.contains("bandcamp", Qt::CaseInsensitive)) {
        return true;
    }
    if (str.contains("bcbits", Qt::CaseInsensitive)) {
        return true;
    }

    return false;
}

QByteArray BcApi::downloadData(const QUrl &url)
{
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
            qWarning() << "Error:" << err;
            loop.exit(1);
        }
    });

    if (loop.exec() == 1) {
        qWarning() << "Cannot download data";
    }

    reply->deleteLater();

    return data;
}

BcApi::Type BcApi::textToType(const QString &text)
{
    if (text.size() > 0) {
        if (text.at(0) == 'b')
            return BcApi::Type_Artist;
        if (text.at(0) == 'a')
            return BcApi::Type_Album;
        if (text.at(0) == 't')
            return BcApi::Type_Track;
        if (text.at(0) == 'l')
            return BcApi::Type_Label;
        if (text.at(0) == 'f')
            return BcApi::Type_Fan;
    }

    return BcApi::Type_Unknown;
}

/*BcApi::Type BcApi::textToType(const QString &text)
{
    if (text.size() > 2) {
        if (text.at(0) == 'A') {
            if (text.at(1) == 'R')
                return BcApi::Type_Artist;
            if (text.at(1) == 'L')
                return BcApi::Type_Album;
        } else {
            if (text.at(0) == 'T')
                return BcApi::Type_Track;
            if (text.at(0) == 'L')
                return BcApi::Type_Label;
            if (text.at(0) == 'F')
                return BcApi::Type_Fan;
        }
    }

    return BcApi::Type_Unknown;
}*/

BcApi::Track BcApi::track(const QUrl &url)
{
    Track track;

    auto data = downloadData(url);
    if (data.isEmpty()) {
        qWarning() << "No data received";
        return track;
    }

    if (QThread::currentThread()->isInterruptionRequested())
        return track;

    auto output = gumbo::parseHtmlData(data);
    if (!output) {
        qWarning() << "Cannot parse HTML data";
        return track;
    }

    auto json = parseJsonData(gumbo::attr_data(gumbo::search_for_attr_one(output->root, "data-tralbum"), "data-tralbum"));
    if (json.isNull() || !json.isObject()) {
        qWarning() << "Cannot parse JSON data";
        return track;
    }

    auto track_info = json.object().value("trackinfo").toArray();
    if (track_info.isEmpty()) {
        qWarning() << "Track info is missing";
        return track;
    }

    track.webUrl = QUrl{json.object().value("url").toString()};
    if (track.webUrl.isRelative()) {
        track.webUrl.setScheme(url.scheme());
        track.webUrl.setHost(url.host());
        track.webUrl.setPort(url.port());
    }

    track.artist = json.object().value("artist").toString();
    track.title = track_info.at(0).toObject().value("title").toString();
    track.streamUrl = QUrl{track_info.at(0).toObject().value("file").toObject().value("mp3-128").toString()};
    track.duration = int(track_info.at(0).toObject().value("duration").toDouble());
    track.imageUrl = QUrl{gumbo::attr_data(gumbo::search_for_attr_one(output->root, "property", "og:image"), "content")};

    if (track.streamUrl.isEmpty()) {
        qWarning() << "Stream URL is missing";
    }
    if (track.title.isEmpty()) {
        qWarning() << "Track title is missing";
    }
    if (track.webUrl.isEmpty()) {
        qWarning() << "Web URL is missing";
    }

    return track;
}

BcApi::Album BcApi::album(const QUrl &url)
{
    Album album;

    auto data = downloadData(url);
    if (data.isEmpty()) {
        qWarning() << "No data received";
        return album;
    }

    if (QThread::currentThread()->isInterruptionRequested())
        return album;

    auto output = gumbo::parseHtmlData(data);
    if (!output) {
        qWarning() << "Cannot parse HTML data";
        return album;
    }

    auto json = parseJsonData(gumbo::attr_data(gumbo::search_for_attr_one(output->root, "data-tralbum"), "data-tralbum"));
    if (json.isNull() || !json.isObject()) {
        qWarning() << "Cannot parse JSON data";
        return album;
    }

    album.title = json.object().value("current").toObject().value("title").toString();
    album.artist = json.object().value("artist").toString();
    album.imageUrl = QUrl{gumbo::attr_data(gumbo::search_for_attr_one(output->root, "property", "og:image"), "content")};

    auto track_info = json.object().value("trackinfo").toArray();

    for (int i = 0; i < track_info.size(); ++i) {
        AlbumTrack track;

        track.streamUrl = QUrl{track_info.at(i).toObject().value("file").toObject().value("mp3-128").toString()};
        if (track.streamUrl.isEmpty()) {
            continue;
        }

        track.title = track_info.at(i).toObject().value("title").toString();
        if (track.title.isEmpty()) {
            continue;
        }

        track.duration = int(track_info.at(i).toObject().value("duration").toDouble());

        track.webUrl = QUrl{track_info.at(i).toObject().value("title_link").toString()};
        if (track.webUrl.isRelative()) {
            track.webUrl.setScheme(url.scheme());
            track.webUrl.setHost(url.host());
            track.webUrl.setPort(url.port());
        }

        album.tracks.push_back(std::move(track));
    }

    return album;
}

BcApi::Artist BcApi::artist(const QUrl &url)
{
    Artist artist;

    QUrl newUrl = url;
    if (newUrl.path().isEmpty()) {
        newUrl.setPath("/music");
    }

    auto data = downloadData(newUrl);
    if (data.isEmpty()) {
        qWarning() << "No data received";
        return artist;
    }

    if (QThread::currentThread()->isInterruptionRequested())
        return artist;

    auto output = gumbo::parseHtmlData(data);
    if (!output) {
        qWarning() << "Cannot parse HTML data";
        return artist;
    }

    auto head = gumbo::search_for_tag_one(output->root, GUMBO_TAG_HEAD);
    artist.name = QString::fromUtf8(gumbo::attr_data(gumbo::search_for_attr_one(head, "property", "og:title"), "content"));
    artist.imageUrl = QUrl{gumbo::attr_data(gumbo::search_for_attr_one(head, "property", "og:image"), "content")};

    auto lis = gumbo::search_for_tag(gumbo::search_for_id(output->root, "music-grid"), GUMBO_TAG_LI);

    for (auto li : lis) {
        ArtistAlbum album;
        auto a = gumbo::search_for_tag_one(li, GUMBO_TAG_A);

        album.webUrl = QUrl{gumbo::attr_data(a, "href")};
        if (album.webUrl.isEmpty()) {
            continue;
        }
        if (album.webUrl.isRelative()) {
            album.webUrl.setScheme(url.scheme());
            album.webUrl.setHost(url.host());
            album.webUrl.setPort(url.port());
        }

        auto img = gumbo::search_for_tag_one(a, GUMBO_TAG_IMG);
        album.imageUrl = QUrl{gumbo::attr_data(img, "data-original")};
        if (album.imageUrl.isEmpty())
            album.imageUrl = QUrl{gumbo::attr_data(img, "src")};

        album.title = gumbo::node_text(gumbo::search_for_class_one(a, "title"));
        artist.albums.push_back(std::move(album));
    }

    return artist;
}

QJsonDocument BcApi::parseJsonData(const QByteArray &data)
{
    QJsonParseError err;

    auto json = QJsonDocument::fromJson(data, &err);

    if (err.error != QJsonParseError::NoError) {
        qWarning() << "Error parsing json:" << err.errorString();
    }

    return json;
}

std::vector<BcApi::SearchResultItem> BcApi::search(const QString &query)
{
    std::vector<SearchResultItem> items;

    auto data = downloadData(makeSearchUrl(query));
    if (data.isEmpty()) {
        qWarning() << "No data received";
        return items;
    }

    if (QThread::currentThread()->isInterruptionRequested())
        return items;

    auto json = parseJsonData(data);
    if (json.isNull() || !json.isObject()) {
        qWarning() << "Cannot parse JSON data";
        return items;
    }

    auto res = json.object().value("auto").toObject().value("results").toArray();

    for (int i = 0; i < res.size(); ++i) {
        auto elm = res.at(i).toObject();

        auto type = textToType(elm.value("type").toString());
        if (type == Type_Unknown || type == Type_Fan || type == Type_Label) {
            continue;
        }

        SearchResultItem item;
        item.type = type;

        item.webUrl = QUrl{elm.value("url").toString()};
        if (item.webUrl.isEmpty()) {
            continue;
        }

        item.imageUrl = QUrl{elm.value("img").toString()};

        if (type == Type_Track) {
            item.title = elm.value("name").toString();
            item.artist = elm.value("band_name").toString();
            item.album = elm.value("album_name").toString();
        } else if (type == Type_Artist) {
            item.artist = elm.value("name").toString();
        } else if (type == Type_Album) {
            item.album = elm.value("name").toString();
            item.artist = elm.value("band_name").toString();
        }

        items.push_back(std::move(item));
    }

    return items;
}

/*std::vector<BcApi::SearchResultItem> BcApi::search(const QString &query)
{
    std::vector<SearchResultItem> items;

    auto data = downloadHtmlData(makeSearchUrl(query));

    if (data.isEmpty()) {
        qWarning() << "No data received";
        return items;
    }

    auto output = gumbo::parseHtmlData(data);

    if (!output) {
        qWarning() << "Cannot parse data";
        return items;
    }

    auto srs = gumbo::search_for_class(output->root, "searchresult");

    for (auto sr : srs) {
        auto ri = search_for_class_one(sr, "result-info");
        if (!ri) {
            break;
        }

        auto type = textToType(gumbo::node_text(gumbo::search_for_class_one(ri, "itemtype")));
        if (type == Type_Unknown) {
            break;
        }

        SearchResultItem item;
        item.type = type;

        auto img = gumbo::search_for_tag_one(gumbo::search_for_class_one(sr, "art"), GUMBO_TAG_IMG);
        if (img) {
            if (auto src = gumbo::gumbo_get_attribute(&img->v.element.attributes, "src")) {
                item.imageUrl = QUrl(QString::fromUtf8(src->value));
            }
        }

        item.name = gumbo::node_text(gumbo::search_for_tag_one(search_for_class_one(ri, "heading"), GUMBO_TAG_A));
        if (item.name.isEmpty()) {
            break;
        }

        auto subhead = gumbo::search_for_class_one(ri, "subhead");
        if (!subhead) {
            break;
        }

        item.webUrl = QUrl(gumbo::node_text(gumbo::search_for_tag_one(search_for_class_one(ri, "itemurl"), GUMBO_TAG_A)));
        if (item.webUrl.isEmpty()) {
            break;
        }

        if (type == Type_Track) {
            auto sh = node_text(subhead);

            int fi = sh.indexOf("from ");
            int bi = sh.indexOf("by ");

            item.album = sh.mid(fi + 5, bi - fi - 6);
            item.artist = sh.mid(bi + 3, sh.size() - bi - 3);
        } else if (type == Type_Album) {
            auto sh = node_text(subhead);

            int bi = sh.indexOf("by ");

            item.artist = sh.mid(bi + 3, sh.size() - bi - 3);
            item.album = item.name;
        }

        items.push_back(std::move(item));
    }

    return items;
}*/

QUrl BcApi::makeSearchUrl(const QString &phrase)
{
    QUrlQuery qurl;
    qurl.addQueryItem("q", phrase);
    //QUrl url("https://bandcamp.com/search");
    QUrl url{"https://bandcamp.com/api/fuzzysearch/1/mobile_autocomplete"};
    url.setQuery(qurl);
    return url;
}
