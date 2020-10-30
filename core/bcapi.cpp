/* Copyright (C) 2020 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

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
#include <memory>
#include <string>

#include "bcapi.h"
#include "utils.h"

BcApi::BcApi(std::shared_ptr<QNetworkAccessManager> nam, QObject *parent) :
    QObject(parent), nam(nam)
{
}

bool BcApi::bcUrl(const QUrl &url)
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
    if (!nam)
        nam = std::make_shared<QNetworkAccessManager>();
    auto reply = nam->get(request);
    QTimer::singleShot(httpTimeout, reply, &QNetworkReply::abort);
    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, this, [&loop, reply, &data] {
        auto err = reply->error();
        if (err == QNetworkReply::NoError) {
            data = reply->readAll();
        } else {
            qWarning() << "Error:" << err;
            reply->deleteLater();
            loop.exit(1);
        }
        reply->deleteLater();
        loop.quit();
    });

    if (loop.exec() == 1) {
        qWarning() << "Cannot download data";
    }

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

    auto output = parseHtmlData(data);

    if (!output) {
        qWarning() << "Cannot parse HTML data";
        return track;
    }

    auto json = parseJsonData(attr_data(search_for_attr_one(output->root, "data-tralbum"), "data-tralbum"));
    if (json.isNull() || !json.isObject()) {
        qWarning() << "Cannot parse JSON data";
        return track;
    }

    auto track_info = json.object().value("trackinfo").toArray();
    if (track_info.isEmpty()) {
        qWarning() << "Track info is missing";
        return track;
    }

    track.artist = json.object().value("artist").toString();
    track.title = track_info.at(0).toObject().value("title").toString();
    track.streamUrl = QUrl(track_info.at(0).toObject().value("file").toObject().value("mp3-128").toString());
    track.webUrl = QUrl(json.object().value("url").toString());
    track.duration = int(track_info.at(0).toObject().value("duration").toDouble());
    track.imageUrl = QUrl(attr_data(search_for_attr_one(output->root, "property", "og:image"), "content"));

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

    auto output = parseHtmlData(data);

    if (!output) {
        qWarning() << "Cannot parse HTML data";
        return album;
    }

    auto json = parseJsonData(attr_data(search_for_attr_one(output->root, "data-tralbum"), "data-tralbum"));
    if (json.isNull() || !json.isObject()) {
        qWarning() << "Cannot parse JSON data";
        return album;
    }

    album.title = json.object().value("current").toObject().value("title").toString();
    album.artist = json.object().value("artist").toString();
    album.imageUrl = QUrl(attr_data(search_for_attr_one(output->root, "property", "og:image"), "content"));

    auto track_info = json.object().value("trackinfo").toArray();

    for (int i = 0; i < track_info.size(); ++i) {
        AlbumTrack track;
        track.title = track_info.at(i).toObject().value("title").toString();
        track.webUrl.setScheme(url.scheme());
        track.webUrl.setAuthority(url.authority());
        track.webUrl.setPath(track_info.at(i).toObject().value("title_link").toString());
        track.duration = int(track_info.at(i).toObject().value("duration").toDouble());
        track.streamUrl = QUrl(track_info.at(i).toObject().value("file").toObject().value("mp3-128").toString());

        album.tracks.push_back(std::move(track));
    }

    return album;
}

BcApi::Artist BcApi::artist(const QUrl &url)
{
    Artist artist;

    auto data = downloadData(url);

    if (data.isEmpty()) {
        qWarning() << "No data received";
        return artist;
    }

    auto output = parseHtmlData(data);

    if (!output) {
        qWarning() << "Cannot parse HTML data";
        return artist;
    }

    auto head = search_for_tag_one(output->root, GUMBO_TAG_HEAD);
    artist.name = QString::fromUtf8(attr_data(search_for_attr_one(head, "property", "og:title"), "content"));
    artist.imageUrl = QUrl(attr_data(search_for_attr_one(head, "property", "og:image"), "content"));

    auto lis = search_for_tag(search_for_id(output->root, "music-grid"), GUMBO_TAG_LI);

    for (auto li : lis) {
        ArtistAlbum album;
        auto a = search_for_tag_one(li, GUMBO_TAG_A);
        album.webUrl = url;
        album.webUrl.setPath(attr_data(a, "href"));

        auto img = search_for_tag_one(a, GUMBO_TAG_IMG);
        album.imageUrl = QUrl(attr_data(img, "data-original"));
        if (album.imageUrl.isEmpty())
            album.imageUrl = QUrl(attr_data(img, "src"));

        album.title = node_text(search_for_class_one(a, "title"));
        artist.albums.push_back(std::move(album));
    }

    return artist;
}

QJsonDocument BcApi::parseJsonData(const QByteArray &data) const
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

        item.webUrl = QUrl(elm.value("url").toString());
        if (item.webUrl.isEmpty()) {
            continue;
        }

        item.imageUrl = QUrl(elm.value("img").toString());

        if (type == Type_Track) {
            item.title = elm.value("name").toString();
            item.artist = elm.value("band_name").toString();
            item.album = elm.value("album_name").toString();
        } else if (type == Type_Artist) {
            item.artist = elm.value("name").toString();
            item.webUrl.setPath("/music");
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

    auto output = parseHtmlData(data);

    if (!output) {
        qWarning() << "Cannot parse data";
        return items;
    }

    auto srs = search_for_class(output->root, "searchresult");

    for (auto sr : srs) {
        auto ri = search_for_class_one(sr, "result-info");
        if (!ri) {
            break;
        }

        auto type = textToType(node_text(search_for_class_one(ri, "itemtype")));
        if (type == Type_Unknown) {
            break;
        }

        SearchResultItem item;
        item.type = type;

        auto img = search_for_tag_one(search_for_class_one(sr, "art"), GUMBO_TAG_IMG);
        if (img) {
            if (auto src = gumbo_get_attribute(&img->v.element.attributes, "src")) {
                item.imageUrl = QUrl(QString::fromUtf8(src->value));
            }
        }

        item.name = node_text(search_for_tag_one(search_for_class_one(ri, "heading"), GUMBO_TAG_A));
        if (item.name.isEmpty()) {
            break;
        }

        auto subhead = search_for_class_one(ri, "subhead");
        if (!subhead) {
            break;
        }

        item.webUrl = QUrl(node_text(search_for_tag_one(search_for_class_one(ri, "itemurl"), GUMBO_TAG_A)));
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
    QUrl url("https://bandcamp.com/api/fuzzysearch/1/mobile_autocomplete");
    url.setQuery(qurl);
    return url;
}

BcApi::GumboOutput_ptr BcApi::parseHtmlData(const QByteArray &data)
{
    return GumboOutput_ptr(
                gumbo_parse_with_options(&kGumboDefaultOptions, data.data(),
                size_t(data.length())), [](auto output){
        gumbo_destroy_output(&kGumboDefaultOptions, output);
    });
}

void BcApi::search_for_class(GumboNode* node, const char* cls_name, std::vector<GumboNode*> *nodes)
{
    if (!node || node->type != GUMBO_NODE_ELEMENT) {
        return;
    }

    GumboAttribute* cls_attr;
    if ((cls_attr = gumbo_get_attribute(&node->v.element.attributes, "class")) &&
            strstr(cls_attr->value, cls_name) != nullptr) {
        nodes->push_back(node);
    }

    GumboVector* children = &node->v.element.children;
    for (size_t i = 0; i < children->length; ++i) {
        search_for_class(static_cast<GumboNode*>(children->data[i]), cls_name, nodes);
    }
}

std::vector<GumboNode*> BcApi::search_for_class(GumboNode* node, const char* cls_name)
{
    std::vector<GumboNode*> nodes;
    BcApi::search_for_class(node, cls_name, &nodes);
    return nodes;
}

GumboNode* BcApi::search_for_attr_one(GumboNode* node, const char* attr_name, const char* attr_value)
{
    if (!node || node->type != GUMBO_NODE_ELEMENT) {
        return nullptr;
    }

    GumboAttribute* attr;
    if ((attr = gumbo_get_attribute(&node->v.element.attributes, attr_name)) &&
            strstr(attr->value, attr_value) != nullptr) {
        return node;
    }

    GumboVector* children = &node->v.element.children;
    for (size_t i = 0; i < children->length; ++i) {
        GumboNode* result = search_for_attr_one(static_cast<GumboNode*>(children->data[i]), attr_name, attr_value);
        if (result) {
            return result;
        }
    }

    return nullptr;
}

GumboNode* BcApi::search_for_class_one(GumboNode* node, const char* cls_name)
{
    return search_for_attr_one(node, "class", cls_name);
}

GumboNode* BcApi::search_for_id(GumboNode* node, const char* id)
{
    return search_for_attr_one(node, "id", id);
}

GumboNode* BcApi::search_for_attr_one(GumboNode* node, const char* attr_name)
{
    if (!node || node->type != GUMBO_NODE_ELEMENT) {
        return nullptr;
    }

    if (gumbo_get_attribute(&node->v.element.attributes, attr_name) != nullptr) {
        return node;
    }

    GumboVector* children = &node->v.element.children;
    for (size_t i = 0; i < children->length; ++i) {
        GumboNode* result = search_for_attr_one(static_cast<GumboNode*>(children->data[i]), attr_name);
        if (result) {
            return result;
        }
    }

    return nullptr;
}

void BcApi::search_for_tag(GumboNode* node, GumboTag tag, std::vector<GumboNode*> *nodes)
{
    if (!node || node->type != GUMBO_NODE_ELEMENT) {
        return;
    }

    if (node->v.element.tag == tag) {
        nodes->push_back(node);
    }

    GumboVector* children = &node->v.element.children;
    for (size_t i = 0; i < children->length; ++i) {
        search_for_tag(static_cast<GumboNode*>(children->data[i]), tag, nodes);
    }
}

std::vector<GumboNode*> BcApi::search_for_tag(GumboNode* node, GumboTag tag)
{
    std::vector<GumboNode*> nodes;
    BcApi::search_for_tag(node, tag, &nodes);
    return nodes;
}

GumboNode* BcApi::search_for_tag_one(GumboNode* node, GumboTag tag)
{
    if (!node || node->type != GUMBO_NODE_ELEMENT) {
        return nullptr;
    }

    if (node->v.element.tag == tag) {
        return node;
    }

    GumboVector* children = &node->v.element.children;
    for (size_t i = 0; i < children->length; ++i) {
        GumboNode* result = search_for_tag_one(static_cast<GumboNode*>(children->data[i]), tag);
        if (result) {
            return result;
        }
    }

    return nullptr;
}

QString BcApi::node_text(GumboNode* node)
{
    QString text;
    QTextStream ts(&text);

    if (!node || node->type != GUMBO_NODE_ELEMENT) {
        return {};
    }

    for (size_t i = 0; i < node->v.element.children.length; ++i) {
        auto text = static_cast<GumboNode*>(node->v.element.children.data[0]);
        if (text->type == GUMBO_NODE_TEXT) {
            ts << text->v.text.text;
        }
    }

    return text.simplified();
}

QByteArray BcApi::attr_data(GumboNode* node, const char* attr_name)
{
    if (!node || node->type != GUMBO_NODE_ELEMENT) {
        return {};
    }

    GumboAttribute* attr_value;
    if ((attr_value = gumbo_get_attribute(&node->v.element.attributes, attr_name)) != nullptr) {
        return QByteArray(attr_value->value);
    }

    return {};
}
