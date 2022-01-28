/* Copyright (C) 2020-2022 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "bcapi.h"

#include <QDebug>
#include <QTimer>
#include <QUrlQuery>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonArray>
#include <QJsonObject>

#include "gumbotools.h"
#include "utils.h"
#include "downloader.h"

const int BcApi::maxNotable = 30;
const int BcApi::maxNotableFirstPage = 5;

std::vector<double> BcApi::m_notableIds{};
std::vector<BcApi::SearchResultItem> BcApi::m_notableItems{};
bool BcApi::m_notableItemsDone = false;

BcApi::BcApi(std::shared_ptr<QNetworkAccessManager> nam, QObject *parent) :
    QObject{parent}, nam{nam}
{
}

bool BcApi::validUrl(const QUrl &url)
{
    auto str = url.host();
    if (str.contains("bandcamp", Qt::CaseInsensitive)) return true;
    if (str.contains("bcbits", Qt::CaseInsensitive)) return true;
    return false;
}

BcApi::Type BcApi::textToType(const QString &text)
{
    if (text.size() > 0) {
        if (text.at(0) == 'b')
            return BcApi::Type::Artist;
        if (text.at(0) == 'a')
            return BcApi::Type::Album;
        if (text.at(0) == 't')
            return BcApi::Type::Track;
        if (text.at(0) == 'l')
            return BcApi::Type::Label;
        if (text.at(0) == 'f')
            return BcApi::Type::Fan;
    }

    return BcApi::Type::Unknown;
}

BcApi::Track BcApi::track(const QUrl &url) const
{
    Track track;

    auto data = Downloader{nam}.downloadData(url);
    if (data.isEmpty()) {
        qWarning() << "No data received";
        return track;
    }

    if (QThread::currentThread()->isInterruptionRequested()) return track;

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

BcApi::Album BcApi::album(const QUrl &url) const
{
    Album album;

    auto data = Downloader{nam}.downloadData(url);
    if (data.isEmpty()) {
        qWarning() << "No data received";
        return album;
    }

    if (QThread::currentThread()->isInterruptionRequested()) return album;

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
        if (track.streamUrl.isEmpty()) continue;

        track.title = track_info.at(i).toObject().value("title").toString();
        if (track.title.isEmpty()) continue;

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

BcApi::Artist BcApi::artist(const QUrl &url) const
{
    Artist artist;

    QUrl newUrl{url};
    if (newUrl.path().isEmpty()) {
        newUrl.setPath("/music");
    }

    auto data = Downloader{nam}.downloadData(newUrl);
    if (data.isEmpty()) {
        qWarning() << "No data received";
        return artist;
    }

    if (QThread::currentThread()->isInterruptionRequested()) return artist;

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

std::vector<BcApi::SearchResultItem> BcApi::search(const QString &query) const
{
    std::vector<SearchResultItem> items;

    auto data = Downloader{nam}.downloadData(makeSearchUrl(query));
    if (data.isEmpty()) {
        qWarning() << "No data received";
        return items;
    }

    if (QThread::currentThread()->isInterruptionRequested()) return items;

    auto json = parseJsonData(data);
    if (json.isNull() || !json.isObject()) {
        qWarning() << "Cannot parse JSON data";
        return items;
    }

    auto res = json.object().value("auto").toObject().value("results").toArray();

    for (int i = 0; i < res.size(); ++i) {
        auto elm = res.at(i).toObject();

        auto type = textToType(elm.value("type").toString());
        if (type == Type::Unknown || type == Type::Fan || type == Type::Label) {
            continue;
        }

        SearchResultItem item;
        item.type = type;

        item.webUrl = QUrl{elm.value("url").toString()};
        if (item.webUrl.isEmpty()) continue;

        item.imageUrl = QUrl{elm.value("img").toString()};

        if (type == Type::Track) {
            item.title = elm.value("name").toString();
            item.artist = elm.value("band_name").toString();
            item.album = elm.value("album_name").toString();
        } else if (type == Type::Artist) {
            item.artist = elm.value("name").toString();
        } else if (type == Type::Album) {
            item.album = elm.value("name").toString();
            item.artist = elm.value("band_name").toString();
        }

        items.push_back(std::move(item));
    }

    return items;
}

QUrl BcApi::makeSearchUrl(const QString &phrase)
{
    QUrlQuery qurl;
    qurl.addQueryItem("q", phrase);
    QUrl url{"https://bandcamp.com/api/fuzzysearch/1/autocomplete"};
    url.setQuery(qurl);
    return url;
}

QUrl BcApi::artUrl(const QString& id)
{
    return QUrl{QString{"https://f4.bcbits.com/img/a%1_2.jpg"}.arg(id)};
}

std::optional<BcApi::SearchResultItem> BcApi::notableItem(double id) const
{
    auto data = Downloader{nam}.downloadData(
        QUrl{"https://bandcamp.com/api/notabletralbum/2/get?id=" + QString::number(id, 'd', 0)});
    if (data.isEmpty()) {
        qWarning() << "No data received";
        return std::nullopt;
    }

    if (QThread::currentThread()->isInterruptionRequested()) return std::nullopt;

    auto json = parseJsonData(data);
    if (json.isNull() || !json.isObject()) {
        qWarning() << "Cannot parse JSON data";
        return std::nullopt;
    }

    return notableItem(json.object());
}

BcApi::SearchResultItem BcApi::notableItem(const QJsonObject &obj) const
{
    SearchResultItem item;
    item.album = obj.value("title").toString();
    item.artist = obj.value("artist").toString();
    item.imageUrl = artUrl(QString::number(obj.value("art_id").toDouble(), 'd', 0));
    item.webUrl = QUrl{obj.value("tralbum_url").toString()};
    item.type = Type::Album;

    return item;
}

void BcApi::storeNotableIds(const QJsonObject &obj)
{
    const auto bcnt_seq = obj.value("bcnt_seq").toArray();
    if (bcnt_seq.isEmpty()) {
        qWarning() << "bcnt_seq is missing";
        return;
    }

    m_notableIds.clear();
    m_notableIds.reserve(bcnt_seq.size());
    for (int i = 0; i < bcnt_seq.size(); ++i) {
        m_notableIds.push_back(bcnt_seq.at(i).toDouble());
    }
}

std::vector<BcApi::SearchResultItem> BcApi::notableItems()
{
    std::vector<SearchResultItem> items;

    if (!m_notableItemsDone || m_notableItems.empty()) {
        if (m_notableIds.empty()) {
            const auto json = parseDataBlob();
            if (!json) return items;
            storeNotableIds(json.value().object());
        }

        const auto size = std::min<int>(m_notableIds.size(), maxNotable);

        m_notableItems.clear();
        m_notableItems.reserve(size);
        for (auto i = 0; i < size; ++i) {
            emit progressChanged(i, size);
            const auto item = notableItem(m_notableIds.at(i));
            if (!item) break;
            m_notableItems.push_back(item.value());
        }
        emit progressChanged(size, size);

        if (!QThread::currentThread()->isInterruptionRequested()) m_notableItemsDone = true;
    }

    items = m_notableItems;

    return items;
}

std::optional<QJsonDocument> BcApi::parseDataBlob() const
{
    auto data = Downloader{nam}.downloadData(QUrl{"https://bandcamp.com"});
    if (data.isEmpty()) {
        qWarning() << "No data received";
        return std::nullopt;
    }

    if (QThread::currentThread()->isInterruptionRequested()) return std::nullopt;

    auto output = gumbo::parseHtmlData(data);
    if (!output) {
        qWarning() << "Cannot parse HTML data";
        return std::nullopt;
    }

    auto json = parseJsonData(gumbo::attr_data(gumbo::search_for_id(output->root, "pagedata"), "data-blob"));
    if (json.isNull() || !json.isObject()) {
        qWarning() << "Cannot parse JSON data";
        return std::nullopt;
    }

    return json;
}

std::vector<BcApi::SearchResultItem> BcApi::notableItemsFirstPage()
{
    std::vector<SearchResultItem> items;

    if (!m_notableItemsDone || m_notableItems.empty()) {
        auto json = parseDataBlob();
        if (!json) return items;

        storeNotableIds(json.value().object());

        auto bcnt_firstpage = json.value().object().value("bcnt_firstpage").toArray();
        if (bcnt_firstpage.isEmpty()) {
            qWarning() << "bcnt_firstpage is missing";
            return items;
        }

        auto size = std::min(bcnt_firstpage.size(), maxNotableFirstPage);
        items.reserve(size);
        for (int i = 0; i < size; ++i) {
            items.push_back(notableItem(bcnt_firstpage.at(i).toObject()));
        }
    } else {
        auto size = std::min<int>(m_notableItems.size(), maxNotableFirstPage);
        items.resize(size);
        std::copy_n(m_notableItems.cbegin(), size, items.begin());
    }

    return items;
}
