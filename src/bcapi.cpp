/* Copyright (C) 2020-2026 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "bcapi.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLatin1String>
#include <QLocale>
#include <QTextStream>
#include <QThread>
#include <QTime>
#include <QTimeZone>
#include <QTimer>
#include <QUrlQuery>

#include "downloader.h"
#include "gumbotools.h"

std::vector<double> BcApi::m_notableIds{};
std::vector<BcApi::SearchResultItem> BcApi::m_notableItems{};
std::vector<BcApi::Show> BcApi::m_shows{};
bool BcApi::m_notableItemsDone = false;

BcApi::BcApi(std::shared_ptr<QNetworkAccessManager> nam, QObject *parent)
    : QObject{parent}, nam{std::move(nam)} {}

bool BcApi::validUrl(const QUrl &url) {
    auto str = url.host();
    if (str.contains(QLatin1String{"bandcamp"}, Qt::CaseInsensitive))
        return true;
    if (str.contains(QLatin1String{"bcbits"}, Qt::CaseInsensitive)) return true;
    return false;
}

BcApi::Type BcApi::textToType(const QString &text) {
    if (text.size() > 0) {
        if (text.at(0) == 'b') return BcApi::Type::Artist;
        if (text.at(0) == 'a') return BcApi::Type::Album;
        if (text.at(0) == 't') return BcApi::Type::Track;
        if (text.at(0) == 'l') return BcApi::Type::Label;
        if (text.at(0) == 'f') return BcApi::Type::Fan;
    }

    return BcApi::Type::Unknown;
}

BcApi::Track BcApi::track(const QUrl &url) const {
    Track track;

    switch (guessUrlType(url)) {
        case UrlType::Unknown:
        case UrlType::Track: {
            auto data = Downloader{nam}.downloadData(url);
            if (data.bytes.isEmpty()) {
                qWarning() << "no data received";
                return track;
            }

            if (QThread::currentThread()->isInterruptionRequested()) {
                return track;
            }

            return trackFromBytes(url, data.bytes);
        }
        case UrlType::RadioShowTrack: {
            return radioTrack(
                QUrlQuery{url}.queryItemValue(QStringLiteral("show")).toInt());
        }
        case UrlType::RadioShow:
        case UrlType::Album:
        case UrlType::RadioMain:
        case UrlType::Artist:
            break;
    }

    qWarning() << "invalid bc url type";
    return track;
}

BcApi::Track BcApi::trackFromBytes(const QUrl &url,
                                   const QByteArray &bytes) const {
    Track track;

    auto output = gumbo::parseHtmlData(bytes);
    if (!output) {
        qWarning() << "cannot parse html data";
        return track;
    }

    auto json = parseJsonData(gumbo::attr_data(
        gumbo::search_for_attr_one(output->root, "data-tralbum"),
        "data-tralbum"));
    if (json.isNull() || !json.isObject()) {
        qWarning() << "cannot parse json data";
        return track;
    }

    auto track_info = json.object().value(QLatin1String{"trackinfo"}).toArray();
    if (track_info.isEmpty()) {
        qWarning() << "track info is missing";
        return track;
    }

    track.webUrl = QUrl{json.object().value(QLatin1String{"url"}).toString()};
    if (track.webUrl.isRelative()) {
        track.webUrl.setScheme(url.scheme());
        track.webUrl.setHost(url.host());
        track.webUrl.setPort(url.port());
    }

    track.artist = json.object().value(QLatin1String{"artist"}).toString();
    track.title =
        track_info.at(0).toObject().value(QLatin1String{"title"}).toString();
    track.streamUrl = QUrl{track_info.at(0)
                               .toObject()
                               .value(QLatin1String{"file"})
                               .toObject()
                               .value(QLatin1String{"mp3-128"})
                               .toString()};
    track.duration = int(track_info.at(0)
                             .toObject()
                             .value(QLatin1String{"duration"})
                             .toDouble());
    track.imageUrl = QUrl{gumbo::attr_data(
        gumbo::search_for_attr_one(output->root, "property", "og:image"),
        "content")};

    if (track.streamUrl.isEmpty()) {
        qWarning() << "stream url is missing";
    }
    if (track.title.isEmpty()) {
        qWarning() << "track title is missing";
    }
    if (track.webUrl.isEmpty()) {
        qWarning() << "web url is missing";
    }

    return track;
}

BcApi::Album BcApi::album(const QUrl &url) const {
    Album album;

    auto data = Downloader{nam}.downloadData(url);
    if (data.bytes.isEmpty()) {
        qWarning() << "no data received";
        return album;
    }

    if (QThread::currentThread()->isInterruptionRequested()) return album;

    return albumFromBytes(url, data.bytes);
}

BcApi::Album BcApi::albumFromBytes(const QUrl &url,
                                   const QByteArray &bytes) const {
    Album album;

    auto output = gumbo::parseHtmlData(bytes);
    if (!output) {
        qWarning() << "cannot parse html data";
        return album;
    }

    auto json = parseJsonData(gumbo::attr_data(
        gumbo::search_for_attr_one(output->root, "data-tralbum"),
        "data-tralbum"));
    if (json.isNull() || !json.isObject()) {
        qWarning() << "cannot parse json data";
        return album;
    }

    album.title = json.object()
                      .value(QLatin1String{"current"})
                      .toObject()
                      .value(QLatin1String{"title"})
                      .toString();
    album.artist = json.object().value(QLatin1String{"artist"}).toString();
    album.imageUrl = QUrl{gumbo::attr_data(
        gumbo::search_for_attr_one(output->root, "property", "og:image"),
        "content")};

    auto track_info = json.object().value(QLatin1String{"trackinfo"}).toArray();

    for (int i = 0; i < track_info.size(); ++i) {
        AlbumTrack track;

        track.streamUrl = QUrl{track_info.at(i)
                                   .toObject()
                                   .value(QLatin1String{"file"})
                                   .toObject()
                                   .value(QLatin1String{"mp3-128"})
                                   .toString()};
        if (track.streamUrl.isEmpty()) continue;

        track.title = track_info.at(i)
                          .toObject()
                          .value(QLatin1String{"title"})
                          .toString();
        if (track.title.isEmpty()) continue;

        track.duration = int(track_info.at(i)
                                 .toObject()
                                 .value(QLatin1String{"duration"})
                                 .toDouble());

        track.webUrl = QUrl{track_info.at(i)
                                .toObject()
                                .value(QLatin1String{"title_link"})
                                .toString()};
        if (track.webUrl.isRelative()) {
            track.webUrl.setScheme(url.scheme());
            track.webUrl.setHost(url.host());
            track.webUrl.setPort(url.port());
        }

        album.tracks.push_back(std::move(track));
    }

    return album;
}

BcApi::ArtistVariant BcApi::artist(const QUrl &url) {
    QUrl newUrl{url};
    if (newUrl.path().isEmpty()) {
        newUrl.setPath(QStringLiteral("/music"));
    }

    auto data = Downloader{nam}.downloadData(newUrl);
    if (data.bytes.isEmpty()) {
        qWarning() << "no data received";
        return {};
    }

    if (QThread::currentThread()->isInterruptionRequested()) {
        return {};
    }

    qDebug() << "final url:" << data.finalUrl;
    switch (guessUrlType(data.finalUrl)) {
        case UrlType::Album:
            return albumFromBytes(data.finalUrl, data.bytes);
        case UrlType::Track:
            return trackFromBytes(data.finalUrl, data.bytes);
        case UrlType::RadioShow: {
            auto showId = QUrlQuery{data.finalUrl}
                              .queryItemValue(QStringLiteral("show"))
                              .toInt();
            if (showId <= 0) {
                qWarning() << "invalid show id";
                break;
            }
            return radioTracks(showId);
        }
        case UrlType::RadioShowTrack:
            return shows(ShowType::Radio);
        case UrlType::RadioMain:
            return shows(ShowType::Radio);
        case UrlType::Artist:
        case UrlType::Unknown:
            break;
    }

    auto output = gumbo::parseHtmlData(data.bytes);
    if (!output) {
        qWarning() << "cannot parse html data";
        return {};
    }

    Artist artist;

    auto *head = gumbo::search_for_tag_one(output->root, GUMBO_TAG_HEAD);
    artist.name = QString::fromUtf8(gumbo::attr_data(
        gumbo::search_for_attr_one(head, "property", "og:title"), "content"));
    artist.imageUrl = QUrl{gumbo::attr_data(
        gumbo::search_for_attr_one(head, "property", "og:image"), "content")};

    auto lis = gumbo::search_for_tag(
        gumbo::search_for_id(output->root, "music-grid"), GUMBO_TAG_LI);

    for (auto *li : lis) {
        ArtistAlbum album;
        auto *a = gumbo::search_for_tag_one(li, GUMBO_TAG_A);

        album.webUrl = QUrl{gumbo::attr_data(a, "href")};
        if (album.webUrl.isEmpty()) {
            continue;
        }
        if (album.webUrl.isRelative()) {
            album.webUrl.setScheme(url.scheme());
            album.webUrl.setHost(url.host());
            album.webUrl.setPort(url.port());
        }

        auto *img = gumbo::search_for_tag_one(a, GUMBO_TAG_IMG);
        album.imageUrl = QUrl{gumbo::attr_data(img, "data-original")};
        if (album.imageUrl.isEmpty())
            album.imageUrl = QUrl{gumbo::attr_data(img, "src")};

        album.title = gumbo::node_text(gumbo::search_for_class_one(a, "title"));
        artist.albums.push_back(std::move(album));
    }

    return artist;
}

QJsonDocument BcApi::parseJsonData(const QByteArray &data) {
    QJsonParseError err{};

    auto json = QJsonDocument::fromJson(data, &err);

    if (err.error != QJsonParseError::NoError) {
        qWarning() << "error parsing json:" << err.errorString();
    }

    return json;
}

std::vector<BcApi::SearchResultItem> BcApi::search(const QString &query) const {
    std::vector<SearchResultItem> items;

    auto [url, postData] = makeSearchUrl(query);
    auto data = Downloader{nam}.downloadData(
        url, {"application/json", std::move(postData), {}});
    if (data.bytes.isEmpty()) {
        qWarning() << "no data received";
        return items;
    }

    if (QThread::currentThread()->isInterruptionRequested()) return items;

    auto json = parseJsonData(data.bytes);
    if (json.isNull() || !json.isObject()) {
        qWarning() << "cannot parse json data";
        return items;
    }

    auto res = json.object()
                   .value(QLatin1String{"auto"})
                   .toObject()
                   .value(QLatin1String{"results"})
                   .toArray();

    auto extra_img_fix = [](QUrl &url) {
        // extra fix: add 'a' at the begining of a filename in img url
        auto fn = url.fileName();
        url.setPath(url.path().replace(fn, 'a' + fn));
    };

    for (int i = 0; i < res.size(); ++i) {
        auto elm = res.at(i).toObject();

        auto type = textToType(elm.value(QLatin1String{"type"}).toString());
        if (type == Type::Unknown || type == Type::Fan || type == Type::Label) {
            continue;
        }

        SearchResultItem item;
        item.type = type;

        item.imageUrl = QUrl{elm.value(QLatin1String{"img"}).toString()};

        if (type == Type::Track) {
            item.webUrl =
                QUrl{elm.value(QLatin1String{"item_url_root"}).toString()};
            if (item.webUrl.isEmpty()) continue;

            item.title = elm.value(QLatin1String{"name"}).toString();
            item.artist = elm.value(QLatin1String{"band_name"}).toString();
            item.album = elm.value(QLatin1String{"album_name"}).toString();

            extra_img_fix(item.imageUrl);
        } else if (type == Type::Artist) {
            item.webUrl =
                QUrl{elm.value(QLatin1String{"item_url_root"}).toString()};
            if (item.webUrl.isEmpty()) continue;

            item.artist = elm.value(QLatin1String{"name"}).toString();
        } else if (type == Type::Album) {
            item.webUrl =
                QUrl{elm.value(QLatin1String{"item_url_path"}).toString()};
            if (item.webUrl.isEmpty()) continue;

            item.album = elm.value(QLatin1String{"name"}).toString();
            item.artist = elm.value(QLatin1String{"band_name"}).toString();

            extra_img_fix(item.imageUrl);
        }

        items.push_back(std::move(item));
    }

    return items;
}

std::pair<QUrl, QByteArray> BcApi::makeSearchUrl(const QString &phrase) {
    return std::make_pair(
        QUrl{"https://bandcamp.com/api/bcsearch_public_api/1/"
             "autocomplete_elastic"},
        QStringLiteral("{\"search_text\":\"%1\",\"search_filter\":\"\",\"full_"
                       "page\":false,\"fan_id\":null}")
            .arg(phrase)
            .toLocal8Bit());
}

QUrl BcApi::artUrl(const QString &id) {
    return QUrl{QStringLiteral("https://f4.bcbits.com/img/a%1_2.jpg").arg(id)};
}

std::optional<BcApi::SearchResultItem> BcApi::notableItem(
    const QByteArray &bytes) const {
    if (bytes.isEmpty()) {
        qWarning() << "bytes is empty";
        return std::nullopt;
    }

    if (QThread::currentThread()->isInterruptionRequested())
        return std::nullopt;

    auto json = parseJsonData(bytes);
    if (json.isNull() || !json.isObject()) {
        qWarning() << "cannot parse json data";
        return std::nullopt;
    }

    return notableItem(json.object());
}

BcApi::SearchResultItem BcApi::notableItem(const QJsonObject &obj) {
    SearchResultItem item;
    item.album = obj.value(QLatin1String{"title"}).toString();
    item.artist = obj.value(QLatin1String{"artist"}).toString();
    item.imageUrl = artUrl(
        QString::number(obj.value(QLatin1String{"art_id"}).toDouble(), 'd', 0));
    item.webUrl = QUrl{obj.value(QLatin1String{"tralbum_url"}).toString()};
    item.genre = obj.value(QLatin1String{"genre"}).toString();
    item.type = Type::Album;

    return item;
}

void BcApi::storeNotableIds(const QJsonArray &array) {
    auto size = array.size();
    for (int i = 0; i < size; ++i) {
        if (array.at(i).isArray()) {
            storeNotableIds(array.at(i).toArray());
        } else if (array.at(i).isDouble()) {
            m_notableIds.push_back(array.at(i).toDouble());
        }
    }
}

void BcApi::storeNotableIds(const QJsonObject &obj) {
    auto data = obj.value(QLatin1String{"data"}).toArray();
    m_notableIds.clear();
    storeNotableIds(data);
}

bool BcApi::prepareNotableIds() const {
    if (m_notableIds.empty()) {
        const auto json = parseNotableBlob();
        if (!json) return false;
        storeNotableIds(json.value().object());
    }

    return !m_notableIds.empty();
}

void BcApi::makeMoreNotableItems() {
    if (!prepareNotableIds()) {
        qWarning() << "no notable ids";
        return;
    }

    if (!canMakeMoreNotableItems()) return;

    auto size = std::min<decltype(maxNotable)>(
        m_notableIds.size(), m_notableItems.size() + maxNotable);

    std::vector<QUrl> urls;
    urls.reserve(size);
    for (decltype(size) i = m_notableItems.size(); i < size; ++i) {
        urls.push_back(
            QUrl{"https://bandcamp.com/api/notabletralbum/2/get?id=" +
                 QString::number(m_notableIds[i], 'd', 0)});
    }

    auto data = Downloader{nam}.downloadData(urls);

    m_notableItems.reserve(size);

    for (const auto &d : data) {
        auto item = notableItem(d.bytes);
        if (!item) break;
        m_notableItems.push_back(item.value());
    }

    emit progressChanged(size, size);

    if (!QThread::currentThread()->isInterruptionRequested())
        m_notableItemsDone = true;
}

std::vector<BcApi::SearchResultItem> BcApi::notableItems() {
    std::vector<SearchResultItem> items;

    if (!m_notableItemsDone || m_notableItems.empty()) {
        makeMoreNotableItems();
    }

    items = m_notableItems;

    return items;
}

std::optional<QJsonDocument> BcApi::parseNotableBlob() const {
    Downloader::Data postData;
    postData.bytes = "{}";
    postData.mime = "application/json";
    auto data = Downloader{nam}.downloadData(
        QUrl{QStringLiteral(
            "https://bandcamp.com/api/homepage_api/1/notable_tralbums_list")},
        postData);
    if (data.bytes.isEmpty()) {
        qWarning() << "no data received";
        return std::nullopt;
    }

    if (QThread::currentThread()->isInterruptionRequested())
        return std::nullopt;

    auto json = parseJsonData("{\"data\":" + data.bytes + "}");
    if (json.isNull() || !json.isObject()) {
        qWarning() << "cannot parse json data";
        return std::nullopt;
    }

    return json;
}

QUrl BcApi::makeRadioShowUrl(int showId) {
    // https://bandcamp.com/radio?show=883
    return QUrl{QStringLiteral("%1?show=%2").arg(radioUrlStr).arg(showId)};
}

QUrl BcApi::makeRadioShowTrackUrl(int showId) {
    // https://bandcamp.com/radio?show=883&jupii_urltype=track
    auto url = makeRadioShowUrl(showId);
    QUrlQuery query{url};
    query.removeAllQueryItems(urlTypeQueryKey);
    query.addQueryItem(urlTypeQueryKey, trackUrlTypeValue);
    url.setQuery(query);

    return url;
}

std::optional<QJsonDocument> BcApi::parseRadioBlob() const {
    auto data = Downloader{nam}.downloadData(QUrl{radioUrlStr});
    if (data.bytes.isEmpty()) {
        qWarning() << "no data received";
        return std::nullopt;
    }

    if (QThread::currentThread()->isInterruptionRequested()) {
        return std::nullopt;
    }

    auto output = gumbo::parseHtmlData(data.bytes);
    if (!output) {
        qWarning() << "cannot parse html data";
        return std::nullopt;
    }

    auto blobData = QString::fromUtf8(gumbo::attr_data(
        gumbo::search_for_id(output->root, "ArchiveApp"), "data-blob"));

    auto json = parseJsonData(blobData.toUtf8());
    if (json.isNull() || !json.isObject()) {
        qWarning() << "cannot parse json data";
        return std::nullopt;
    }

    return json;
}

std::vector<BcApi::SearchResultItem> BcApi::notableItemsFirstPage() {
    std::vector<SearchResultItem> items;

    if (!m_notableItemsDone || m_notableItems.empty()) {
        auto json = parseNotableBlob();
        if (!json) return items;
        storeNotableIds(json.value().object());
        makeMoreNotableItems();
    }

    auto size = std::min<decltype(maxNotableFirstPage)>(m_notableItems.size(),
                                                        maxNotableFirstPage);
    items.resize(size);
    std::copy_n(m_notableItems.cbegin(), size, items.begin());

    return items;
}

BcApi::UrlType BcApi::guessUrlType(const QUrl &url) {
    auto path = url.path();
    if (path.contains(QStringLiteral("/album/"), Qt::CaseInsensitive))
        return UrlType::Album;
    if (path.contains(QStringLiteral("/track/"), Qt::CaseInsensitive))
        return UrlType::Track;
    if (path.compare(QStringLiteral("/radio"), Qt::CaseInsensitive) == 0) {
        QUrlQuery query{url};
        if (query.hasQueryItem(QStringLiteral("show"))) {
            // if ?jupii_urltype=track exists, url should points to radio stream
            // track
            if (query.queryItemValue(urlTypeQueryKey)
                    .compare(QLatin1String{trackUrlTypeValue},
                             Qt::CaseInsensitive) == 0) {
                return UrlType::RadioShowTrack;
            }
            return UrlType::RadioShow;
        }
        return UrlType::RadioMain;
    }
    if (path.isEmpty() || path == "/" ||
        path.contains(QStringLiteral("/music/"), Qt::CaseInsensitive) ||
        path.contains(QStringLiteral("/merch/"), Qt::CaseInsensitive) ||
        path.contains(QStringLiteral("/community/"), Qt::CaseInsensitive) ||
        path.contains(QStringLiteral("/concerts/"), Qt::CaseInsensitive)) {
        return UrlType::Artist;
    }
    return UrlType::Unknown;
}

QUrl BcApi::artUrl(long imageId) {
    return QUrl{QStringLiteral("https://f4.bcbits.com/img/%1_11.jpg")
                    .arg(imageId, 10, 10, QLatin1Char('0'))};
}

std::optional<QJsonObject> BcApi::radioShowObj(int showId) const {
    Downloader::Data postData;
    postData.bytes = QStringLiteral("{\"id\":%1}").arg(showId).toUtf8();
    postData.mime = "application/json";
    auto data = Downloader{nam}.downloadData(
        QUrl{QStringLiteral("https://bandcamp.com/api/bcradio_api/1/get_show")},
        postData);
    if (data.bytes.isEmpty()) {
        qWarning() << "no data received";
        return std::nullopt;
    }

    if (QThread::currentThread()->isInterruptionRequested()) {
        return std::nullopt;
    }

    auto json = parseJsonData(data.bytes);
    if (json.isNull() || !json.isObject()) {
        qWarning() << "cannot parse json data";
        return std::nullopt;
    }

    return json.object();
}

BcApi::Track BcApi::radioTrack(int showId) const {
    Track t;

    auto obj = radioShowObj(showId);
    if (!obj) {
        return t;
    }

    auto radioShowAudio =
        obj->value(QLatin1String{"radioShowAudio"}).toObject();

    t.title = radioShowAudio.value(QLatin1String{"title"}).toString();
    if (t.title.isEmpty()) {
        qWarning() << "show title is empty";
        return t;
    }
    t.imageUrl =
        artUrl(radioShowAudio.value(QLatin1String{"artId"}).toDouble());
    t.streamUrl =
        QUrl{radioShowAudio.value(QLatin1String{"streamUrl"}).toString()};
    t.webUrl = makeRadioShowUrl(showId);
    t.duration = radioShowAudio.value(QLatin1String{"duration"}).toDouble();
    t.artist = QStringLiteral("Bandcamp");

    // add date to titile
    auto date = QLocale::c().toDateTime(
        radioShowAudio.value(QLatin1String{"date"}).toString().remove(" GMT"),
        "dd MMM yyyy hh:mm:ss");
    if (date.isValid()) {
        t.title += (" · " + date.toString("d MMM yyyy"));
    }

    return t;
}

BcApi::ShowTracks BcApi::radioTracks(int showId) const {
    ShowTracks tracks;

    auto obj = radioShowObj(showId);
    if (!obj) {
        return tracks;
    }

    tracks.id = showId;

    auto radioShowAudio =
        obj->value(QLatin1String{"radioShowAudio"}).toObject();

    tracks.title = radioShowAudio.value(QLatin1String{"title"}).toString();
    if (tracks.title.isEmpty()) {
        qWarning() << "show title is empty";
        return tracks;
    }
    tracks.imageUrl =
        artUrl(radioShowAudio.value(QLatin1String{"artId"}).toDouble());
    tracks.date = QLocale::c().toDateTime(
        radioShowAudio.value(QLatin1String{"date"}).toString().remove(" GMT"),
        "dd MMM yyyy hh:mm:ss");
    tracks.webUrl = makeRadioShowTrackUrl(showId);

    // add date to titile
    if (tracks.date.isValid()) {
        tracks.title += (" · " + tracks.date.toString("d MMM yyyy"));
    }

    auto tracklist = obj->value(QLatin1String{"tracklist"}).toArray();

    for (int i = 0; i < tracklist.size(); ++i) {
        auto tobj = tracklist.at(i).toObject();

        Track item;

        item.webUrl = QUrl{tobj.value(QLatin1String{"url"}).toString()};
        if (item.webUrl.isEmpty()) {
            continue;
        }
        item.title = tobj.value(QLatin1String{"title"}).toString();
        if (item.title.isEmpty()) {
            continue;
        }
        item.artist = tobj.value(QLatin1String{"artistName"}).toString();
        item.album = tobj.value(QLatin1String{"album"})
                         .toObject()
                         .value(QLatin1String{"title"})
                         .toString();
        item.imageUrl = artUrl(QString::number(
            tobj.value(QLatin1String{"artId"}).toDouble(), 'd', 0));

        tracks.featuredTracks.push_back(std::move(item));
    }

    return tracks;
}

std::vector<BcApi::Show> BcApi::makeShows(ShowType type) const {
    std::vector<Show> shows;

    if (type != ShowType::Radio) {
        qWarning() << "only radio shows are supported";
        return shows;
    }

    auto json = parseRadioBlob();
    if (!json) {
        qWarning() << "failed to parse shows";
        return shows;
    }

    auto appData = json->object().value(QLatin1String{"appData"}).toObject();
    auto showsArray = appData.value(QLatin1String{"shows"}).toArray();
    shows.reserve(showsArray.size());
    for (int i = 0; i < showsArray.size(); ++i) {
        auto ele = showsArray.at(i).toObject();
        Show show;
        show.id = ele.value(QLatin1String{"showId"}).toInt();
        if (show.id <= 0) {
            continue;
        }
        if (!ele.value(QLatin1String{"isPublished"}).toBool()) {
            continue;
        }
        show.title = ele.value(QLatin1String{"title"}).toString();
        if (show.title.isEmpty()) {
            continue;
        }
        long imageId = ele.value(QLatin1String{"imageId"}).toDouble();
        if (imageId > 0) {
            show.imageUrl = artUrl(imageId);
        }
        show.description = ele.value(QLatin1String{"shortDesc"}).toString();
        // "date": "16 Jan 2026 00:00:00 GMT",
        show.date = QLocale::c().toDateTime(
            ele.value(QLatin1String{"date"}).toString().remove(" GMT"),
            "dd MMM yyyy hh:mm:ss");
        auto url = ele.value(QLatin1String{"url"}).toString();
        if (!url.isEmpty()) {
            show.webUrl = QUrl{QStringLiteral("https://bandcamp.com") + url};
        }

        // add date to titile
        if (show.date.isValid()) {
            show.title += (" · " + show.date.toString("d MMM yyyy"));
        }

        shows.push_back(std::move(show));
    }

    return shows;
}

std::vector<BcApi::Show> BcApi::latestShows(ShowType type) {
    if (m_shows.empty()) {
        m_shows = makeShows(type);
    }

    std::vector<BcApi::Show> newShows;
    std::copy(m_shows.cbegin(), std::next(m_shows.cbegin(), 10),
              std::back_inserter(newShows));

    return newShows;
}

std::vector<BcApi::Show> BcApi::shows(ShowType type) {
    if (m_shows.empty()) {
        m_shows = makeShows(type);
    }

    return m_shows;
}
