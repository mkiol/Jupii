/* Copyright (C) 2021-2025 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "soundcloudapi.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLocale>
#include <QTextStream>
#include <QThread>
#include <QTimer>
#include <QUrlQuery>
#include <utility>

#include "downloader.h"

const int SoundcloudApi::maxFeatured = 10;
const int SoundcloudApi::maxFeaturedFirstPage = 10;
bool SoundcloudApi::m_canMakeMore = false;
std::vector<SoundcloudApi::Selection> SoundcloudApi::m_featuresItems{};
QString SoundcloudApi::clientId;

SoundcloudApi::SoundcloudApi(std::shared_ptr<QNetworkAccessManager> nam,
                             QObject *parent)
    : QObject{parent},
      m_nam{std::move(nam)},
      m_locale{QLocale::system().bcp47Name().split('-').first()} {
    discoverClientId();
}

void SoundcloudApi::discoverClientId() {
    if (!clientId.isEmpty()) return;

    auto output =
        downloadHtmlData(QUrl{QStringLiteral("https://soundcloud.com")});
    if (!output) {
        qWarning() << "cannot parse html data";
        return;
    }

    if (QThread::currentThread()->isInterruptionRequested()) return;

    auto scripts = gumbo::search_for_tag(output->root, GUMBO_TAG_SCRIPT);
    for (auto *script : scripts) {
        if (QUrl url{gumbo::attr_data(script, "src")}; !url.isEmpty()) {
            if (auto data = Downloader{m_nam}.downloadData(url);
                !data.bytes.isEmpty()) {
                if (auto id = extractClientId(data.bytes); !id.isEmpty()) {
#ifdef QT_DEBUG
                    qDebug() << "client id: " << id;
#endif
                    clientId = id;
                    return;
                }
            }
        }
        if (QThread::currentThread()->isInterruptionRequested()) return;
    }

    qWarning() << "cannot find client id";
}

bool SoundcloudApi::validUrl(const QUrl &url) {
    return url.host().contains(QLatin1String{"soundcloud.com"},
                               Qt::CaseInsensitive);
}

QJsonDocument SoundcloudApi::parseJsonData(const QByteArray &data) {
    QJsonParseError err{};

    auto json = QJsonDocument::fromJson(data, &err);

    if (err.error != QJsonParseError::NoError) {
        qWarning() << "error parsing json:" << err.errorString();
    }

    return json;
}

gumbo::GumboOutput_ptr SoundcloudApi::downloadHtmlData(const QUrl &url) const {
    auto data = Downloader{m_nam}.downloadData(url);
    if (data.bytes.isEmpty()) {
        qWarning() << "no data received";
        return {};
    }

    return gumbo::parseHtmlData(data.bytes);
}

QJsonDocument SoundcloudApi::downloadJsonData(const QUrl &url) const {
    auto data = Downloader{m_nam}.downloadData(url);
    if (data.bytes.isEmpty()) {
        qWarning() << "no data received";
        return {};
    }

    return parseJsonData(data.bytes);
}

SoundcloudApi::Type SoundcloudApi::textToType(const QString &text) {
    if (text.size() > 0) {
        if (text.at(0) == 'u') return SoundcloudApi::Type::User;
        if (text.at(0) == 't') return SoundcloudApi::Type::Track;
        if (text.at(0) == 'p') return SoundcloudApi::Type::Playlist;
        if (text.at(0) == 's') return SoundcloudApi::Type::Selection;
    }

    return SoundcloudApi::Type::Unknown;
}

QString SoundcloudApi::extractData(const QString &text) {
    // QRegExp
    // rx("e\\.data\\.forEach\\(function\\(e\\)\\{n\\(e\\)\\}\\)\\}catch\\(e\\)\\{\\}\\}\\)\\}\\,(\\[.*\\])\\);$",
    // Qt::CaseSensitive);
    static QRegExp rx("window\\.__sc_hydration\\s=\\s(.*);$",
                      Qt::CaseSensitive);
    if (rx.indexIn(text) != -1) return rx.cap(1);
    return {};
}

QString SoundcloudApi::extractClientId(const QString &text) {
    static QRegExp rx(",client_id:\"([\\0-9a-zA-Z]+)\",", Qt::CaseSensitive);
    if (rx.indexIn(text) != -1) return rx.cap(1);
    return {};
}

QString SoundcloudApi::extractUserId(const QString &text) {
    static QRegExp rx("soundcloud://users:([0-9]+)$", Qt::CaseSensitive);
    if (rx.indexIn(text) != -1) return rx.cap(1);
    return {};
}

QString SoundcloudApi::extractPlaylistId(const QString &text) {
    static QRegExp rx("soundcloud://playlists:([0-9]+)$", Qt::CaseSensitive);
    if (rx.indexIn(text) != -1) return rx.cap(1);
    return {};
}

QString SoundcloudApi::extractUsernameFromTitle(const QString &text) {
    static QRegExp rx("^(.+) \\| .+$", Qt::CaseSensitive);
    if (rx.indexIn(text) != -1) {
        return rx.cap(1);
    }
    return {};
}

QString SoundcloudApi::extractUsernameFromPage(const QString &text) {
    static QRegExp rx("^.+\"username\":\"([^\"]+)\",.+$", Qt::CaseSensitive);
    if (rx.indexIn(text) != -1) {
        return rx.cap(1);
    }
    return {};
}

void SoundcloudApi::addClientId(QUrl *url) {
    QUrlQuery query{*url};
    if (query.hasQueryItem(QStringLiteral("client_id")))
        query.removeAllQueryItems(QStringLiteral("client_id"));
    query.addQueryItem(QStringLiteral("client_id"), clientId);
    url->setQuery(query);
}

QJsonArray SoundcloudApi::extractItems(const QUrl &url) const {
    QJsonArray items;

    auto output = downloadHtmlData(url);
    if (!output) {
        qWarning() << "cannot parse html data";
        return items;
    }

    if (QThread::currentThread()->isInterruptionRequested()) return items;

    QJsonArray chunks;
    auto scripts = gumbo::search_for_tag(output->root, GUMBO_TAG_SCRIPT);
    for (auto *script : scripts) {
        if (auto data = extractData(gumbo::node_text(script));
            !data.isEmpty()) {
            auto json = parseJsonData(data.toUtf8());
            if (json.isNull() || !json.isArray()) {
                qWarning() << "cannot parse json data";
            } else {
                chunks = json.array();
            }
            break;
        }
        if (QThread::currentThread()->isInterruptionRequested()) return items;
    }

    if (chunks.isEmpty()) {
        qWarning() << "empty chunks data";
        return items;
    }

    for (int i = 0; i < chunks.size(); ++i) {
        auto elm =
            chunks.at(i).toObject().value(QLatin1String{"data"}).toObject();
        if (!elm.value(QLatin1String{"kind"}).toString().isEmpty())
            items.append(elm);
    }

    return items;
}

SoundcloudApi::Track SoundcloudApi::track(const QUrl &url) {
    Track track;

    auto items = extractItems(url);
    if (items.isEmpty()) {
        qWarning() << "no items";
        return track;
    }

    if (QThread::currentThread()->isInterruptionRequested()) return track;

    for (int i = 0; i < items.size(); ++i) {
        auto elm = items.at(i).toObject();
        auto type = textToType(elm.value(QLatin1String{"kind"}).toString());
        if (type != Type::Track) continue;

        auto media = elm.value(QLatin1String{"media"})
                         .toObject()
                         .value(QLatin1String{"transcodings"})
                         .toArray();
        if (media.isEmpty()) return track;

        for (int i = 0; i < media.size(); ++i) {
            auto m = media.at(i).toObject();
            // TODO: Support for HLS urls
            if (m.value(QLatin1String{"format"})
                        .toObject()
                        .value(QLatin1String{"protocol"})
                        .toString() != "progressive" ||
                m.value(QLatin1String{"format"})
                        .toObject()
                        .value(QLatin1String{"mime_type"})
                        .toString() != "audio/mpeg") {
                continue;
            }

            QUrl url{m.value(QLatin1String{"url"}).toString()};
            if (url.isEmpty()) {
                qWarning() << "empty url";
                return track;
            }

            addClientId(&url);

            auto json = downloadJsonData(url);
            if (json.isNull() || !json.isObject()) {
                qWarning() << "cannot parse JSON data";
                return track;
            }

            track.streamUrl =
                QUrl{json.object().value(QLatin1String{"url"}).toString()};
            track.duration =
                int(m.value(QLatin1String{"duration"}).toDouble() / 1000);

            if (QThread::currentThread()->isInterruptionRequested())
                return track;

            break;
        }

        if (track.streamUrl.isEmpty()) {
            qWarning() << "stream url is empty";
            return track;
        }

        track.webUrl =
            QUrl{elm.value(QLatin1String{"permalink_url"}).toString()};
        track.imageUrl =
            QUrl{elm.value(QLatin1String{"artwork_url"}).toString()};
        track.title = elm.value(QLatin1String{"title"}).toString();
        track.artist = elm.value(QLatin1String{"publisher_metadata"})
                           .toObject()
                           .value(QLatin1String{"artist"})
                           .toString();
        if (track.artist.isEmpty()) {
            track.artist = elm.value(QLatin1String{"user"})
                               .toObject()
                               .value(QLatin1String{"username"})
                               .toString();
        }
        track.album = elm.value(QLatin1String{"publisher_metadata"})
                          .toObject()
                          .value(QLatin1String{"album_title"})
                          .toString();

        return track;
    }

    qWarning() << "no valid chunk";
    return track;
}

bool SoundcloudApi::mediaOk(const QJsonArray &media) {
    for (int i = 0; i < media.size(); ++i) {
        auto m = media.at(i).toObject();
        // TODO: Support for HLS urls
        if (m.value(QLatin1String{"format"})
                    .toObject()
                    .value(QLatin1String{"protocol"})
                    .toString() != "progressive" ||
            m.value(QLatin1String{"format"})
                    .toObject()
                    .value(QLatin1String{"mime_type"})
                    .toString() != "audio/mpeg") {
            continue;
        }

        if (m.value(QLatin1String{"url"}).toString().isEmpty()) continue;

        return true;
    }

    return false;
}

SoundcloudApi::Playlist SoundcloudApi::playlist(const QUrl &url) {
    Playlist playlist;

    auto output = downloadHtmlData(url);
    if (!output) {
        qWarning() << "cannot parse HTML data";
        return playlist;
    }

    if (QThread::currentThread()->isInterruptionRequested()) return playlist;

    QString playlistId;

    auto metas = gumbo::search_for_tag(output->root, GUMBO_TAG_META);
    for (auto *meta : metas) {
        if (auto content = gumbo::attr_data(meta, "content");
            !content.isEmpty()) {
            playlistId = extractPlaylistId(content);
            if (!playlistId.isEmpty()) break;
        }
    }

    if (playlistId.isEmpty()) {
        qWarning() << "empty playlist id";
        return playlist;
    }

    auto json = downloadJsonData(makePlaylistUrl(playlistId));
    if (json.isNull() || !json.isObject()) {
        qWarning() << "cannot parse JSON data";
        return playlist;
    }

    if (QThread::currentThread()->isInterruptionRequested()) return playlist;

    auto pobj = json.object();
    playlist.title = pobj.value(QLatin1String{"title"}).toString();
    playlist.imageUrl =
        QUrl{pobj.value(QLatin1String{"artwork_url"}).toString()};
    playlist.artist = pobj.value(QLatin1String{"user"})
                          .toObject()
                          .value(QLatin1String{"username"})
                          .toString();

    auto tracks = pobj.value(QLatin1String{"tracks"}).toArray();
    if (tracks.isEmpty()) {
        qWarning() << "no tracks for playlist:" << playlistId;
        return playlist;
    }

    for (int i = 0; i < tracks.size(); ++i) {
        auto elm = tracks.at(i).toObject();
        auto type = textToType(elm.value(QLatin1String{"kind"}).toString());
        if (type != Type::Track) continue;
        if (!mediaOk(elm.value(QLatin1String{"media"})
                         .toObject()
                         .value(QLatin1String{"transcodings"})
                         .toArray()))
            continue;

        QUrl webUrl{elm.value(QLatin1String{"permalink_url"}).toString()};
        if (webUrl.isEmpty()) continue;

        PlaylistTrack track;
        track.imageUrl =
            QUrl{elm.value(QLatin1String{"artwork_url"}).toString()};
        track.title = elm.value(QLatin1String{"title"}).toString();
        track.webUrl = std::move(webUrl);
        track.artist = elm.value(QLatin1String{"publisher_metadata"})
                           .toObject()
                           .value(QLatin1String{"artist"})
                           .toString();
        if (track.artist.isEmpty())
            track.artist = elm.value(QLatin1String{"user"})
                               .toObject()
                               .value(QLatin1String{"username"})
                               .toString();
        track.album = elm.value(QLatin1String{"publisher_metadata"})
                          .toObject()
                          .value(QLatin1String{"album_title"})
                          .toString();
        if (track.album.isEmpty()) track.album = playlist.title;

        playlist.tracks.push_back(std::move(track));
    }

    return playlist;
}

void SoundcloudApi::user(const QUrl &url, User *user, int count) const {
    if (count == 5) return;

    auto json = downloadJsonData(url);
    if (json.isNull() || !json.isObject()) {
        qWarning() << "cannot parse json data";
        return;
    }

    if (QThread::currentThread()->isInterruptionRequested()) return;

    auto items = json.object().value(QLatin1String{"collection"}).toArray();
    if (items.isEmpty()) {
        qWarning() << "no items";
        return;
    }

    for (int i = 0; i < items.size(); ++i) {
        auto elm = items.at(i).toObject();
        auto type = textToType(elm.value(QLatin1String{"type"}).toString());
        if (type == Type::Playlist) {
            elm = elm.value(QLatin1String{"playlist"}).toObject();

            QUrl webUrl{elm.value(QLatin1String{"permalink_url"}).toString()};
            if (webUrl.isEmpty()) continue;

            UserPlaylist playlist;
            playlist.imageUrl =
                QUrl{elm.value(QLatin1String{"artwork_url"}).toString()};
            playlist.title = elm.value(QLatin1String{"title"}).toString();
            playlist.webUrl = std::move(webUrl);
            playlist.artist = elm.value(QLatin1String{"user"})
                                  .toObject()
                                  .value(QLatin1String{"username"})
                                  .toString();

            user->playlists.push_back(std::move(playlist));
        } else if (type == Type::Track) {
            elm = elm.value(QLatin1String{"track"}).toObject();

            if (!mediaOk(elm.value(QLatin1String{"media"})
                             .toObject()
                             .value(QLatin1String{"transcodings"})
                             .toArray()))
                continue;

            QUrl webUrl{elm.value(QLatin1String{"permalink_url"}).toString()};
            if (webUrl.isEmpty()) continue;

            UserTrack track;
            track.imageUrl =
                QUrl{elm.value(QLatin1String{"artwork_url"}).toString()};
            track.title = elm.value(QLatin1String{"title"}).toString();
            track.webUrl = std::move(webUrl);
            track.artist = elm.value(QLatin1String{"publisher_metadata"})
                               .toObject()
                               .value(QLatin1String{"artist"})
                               .toString();
            if (track.artist.isEmpty())
                track.artist = elm.value(QLatin1String{"user"})
                                   .toObject()
                                   .value(QLatin1String{"username"})
                                   .toString();

            user->tracks.push_back(std::move(track));
        }
    }

    QUrl nextUrl{json.object().value(QLatin1String{"next_href"}).toString()};
    if (nextUrl.isEmpty()) return;

    json = {};
    items = {};

    if (QThread::currentThread()->isInterruptionRequested()) return;

    addClientId(&nextUrl);
    this->user(nextUrl, user, count + 1);
}

SoundcloudApi::User SoundcloudApi::user(const QUrl &url) {
    User user;

    auto output = downloadHtmlData(url);
    if (!output) {
        qWarning() << "cannot parse html data";
        return user;
    }

    if (QThread::currentThread()->isInterruptionRequested()) return user;

    const auto userId = [&output] {
        auto metas = gumbo::search_for_tag(output->root, GUMBO_TAG_META);
        for (const auto &meta : metas) {
            auto content = gumbo::attr_data(meta, "content");
            if (!content.isEmpty()) {
                auto id = extractUserId(content);
                if (!id.isEmpty()) return id;
            }
        }
        return QString{};
    }();

    if (userId.isEmpty()) {
        qWarning() << "empty user id";
        return user;
    }

    this->user(makeUserStreamUrl(userId), &user);

    user.name = [&output] {
        auto tags = gumbo::search_for_tag(output->root, GUMBO_TAG_SCRIPT);
        for (const auto &tag : tags) {
            auto content = gumbo::node_text(tag);
            if (!content.isEmpty()) {
                auto id = extractUsernameFromPage(content);
                if (!id.isEmpty()) return id;
            }
        }
        return QString{};
    }();

    user.webUrl = url;

    return user;
}

std::optional<SoundcloudApi::SearchResultItem> SoundcloudApi::searchItem(
    const QJsonObject &obj) {
    auto type = textToType(obj.value(QLatin1String{"kind"}).toString());
    if (type == Type::Unknown) return std::nullopt;

    QUrl webUrl{obj.value(QLatin1String{"permalink_url"}).toString()};
    if (webUrl.isEmpty()) return std::nullopt;

    SearchResultItem item;
    item.type = type;
    item.webUrl = std::move(webUrl);
    item.genre = obj.value(QLatin1String{"genre"}).toString();

    if (type == Type::Track) {
        item.imageUrl =
            QUrl{obj.value(QLatin1String{"artwork_url"}).toString()};
        item.title = obj.value(QLatin1String{"title"}).toString();
        item.artist = obj.value(QLatin1String{"publisher_metadata"})
                          .toObject()
                          .value(QLatin1String{"artist"})
                          .toString();
        if (item.artist.isEmpty())
            item.artist = obj.value(QLatin1String{"user"})
                              .toObject()
                              .value(QLatin1String{"username"})
                              .toString();
    } else if (type == Type::User) {
        item.imageUrl = QUrl{obj.value(QLatin1String{"avatar_url"}).toString()};
        item.artist = obj.value(QLatin1String{"username"}).toString();
    } else if (type == Type::Playlist) {
        item.imageUrl =
            QUrl{obj.value(QLatin1String{"artwork_url"}).toString()};
        if (item.imageUrl.isEmpty()) {
            item.imageUrl =
                QUrl{obj.value(QLatin1String{"avatar_url"}).toString()};
        }
        item.album = obj.value(QLatin1String{"title"}).toString();
        item.title = obj.value(QLatin1String{"title"}).toString();
        item.artist = obj.value(QLatin1String{"user"})
                          .toObject()
                          .value(QLatin1String{"username"})
                          .toString();
    }

    return item;
}

std::optional<SoundcloudApi::Selection> SoundcloudApi::selectionItem(
    const QJsonObject &obj) {
    if (textToType(obj.value(QLatin1String{"kind"}).toString()) !=
        Type::Selection) {
        return std::nullopt;
    }
    auto title = obj.value(QLatin1String{"title"}).toString();
    if (title.isEmpty()) {
        return std::nullopt;
    }

    auto colArr = obj.value(QLatin1String{"items"})
                      .toObject()
                      .value(QLatin1String{"collection"})
                      .toArray();
    if (colArr.isEmpty()) {
        return std::nullopt;
    }

    Selection selection;
    selection.title = std::move(title);

    for (const auto &item : std::as_const(colArr)) {
        auto sitem = searchItem(item.toObject());
        if (!sitem) continue;
        selection.items.push_back(std::move(*sitem));
    }

    if (selection.items.empty()) {
        return std::nullopt;
    }

    return selection;
}

std::vector<SoundcloudApi::SearchResultItem> SoundcloudApi::search(
    const QString &query) const {
    std::vector<SearchResultItem> items;

    auto json = downloadJsonData(makeSearchUrl(query));
    if (json.isNull() || !json.isObject()) {
        qWarning() << "cannot parse json data";
        return items;
    }

    if (QThread::currentThread()->isInterruptionRequested()) return items;

    auto res = json.object().value(QLatin1String{"collection"}).toArray();

    for (int i = 0; i < res.size(); ++i) {
        auto item = searchItem(res.at(i).toObject());
        if (item) items.push_back(std::move(item.value()));
    }

    return items;
}

std::vector<SoundcloudApi::Selection> SoundcloudApi::featuredItems() const {
    if (m_featuresItems.size() < maxFeatured)
        makeFeaturedItems(maxFeatured - m_featuresItems.size());

    return m_featuresItems;
}

void SoundcloudApi::makeFeaturedItems(int max) const {
    auto json = downloadJsonData(makeFeaturedTracksUrl(max));
    if (json.isNull() || !json.isObject()) {
        qWarning() << "cannot parse json data";
        return;
    }

    if (QThread::currentThread()->isInterruptionRequested()) return;

    auto res = json.object().value(QLatin1String{"collection"}).toArray();

    // m_canMakeMore = res.size() >= max;

    auto size = std::min(res.size(), max);

    for (int i = 0; i < size; ++i) {
        auto item = selectionItem(res.at(i).toObject());
        if (item) m_featuresItems.push_back(std::move(item.value()));
    }
}

std::vector<SoundcloudApi::Selection> SoundcloudApi::featuredItemsFirstPage()
    const {
    if (m_featuresItems.size() < maxFeaturedFirstPage)
        makeFeaturedItems(maxFeaturedFirstPage);

    auto size = std::min<int>(m_featuresItems.size(), maxFeaturedFirstPage);
    std::vector<Selection> selections;
    std::copy_n(m_featuresItems.cbegin(), size, std::back_inserter(selections));
    return selections;
}

QUrl SoundcloudApi::makeSearchUrl(const QString &phrase) const {
    QUrlQuery qurl;
    qurl.addQueryItem(QStringLiteral("q"), phrase);
    qurl.addQueryItem(QStringLiteral("app_locale"), m_locale);
    qurl.addQueryItem(QStringLiteral("client_id"), clientId);
    qurl.addQueryItem(QStringLiteral("limit"), QStringLiteral("50"));
    qurl.addQueryItem(QStringLiteral("offset"), QStringLiteral("0"));
    QUrl url{QStringLiteral("https://api-v2.soundcloud.com/search")};
    url.setQuery(qurl);
    return url;
}

QUrl SoundcloudApi::makePlaylistUrl(const QString &id) const {
    QUrlQuery qurl;
    qurl.addQueryItem(QStringLiteral("app_locale"), m_locale);
    qurl.addQueryItem(QStringLiteral("client_id"), clientId);
    QUrl url{"https://api-v2.soundcloud.com/playlists/" + id};
    url.setQuery(qurl);
    return url;
}

QUrl SoundcloudApi::makeTrackUrl(const QString &id) const {
    QUrlQuery qurl;
    qurl.addQueryItem(QStringLiteral("app_locale"), m_locale);
    qurl.addQueryItem(QStringLiteral("client_id"), clientId);
    QUrl url{"https://api-v2.soundcloud.com/tracks/" + id};
    url.setQuery(qurl);
    return url;
}

QUrl SoundcloudApi::makeUserPlaylistsUrl(const QString &id) const {
    QUrlQuery qurl;
    qurl.addQueryItem(QStringLiteral("app_locale"), m_locale);
    qurl.addQueryItem(QStringLiteral("client_id"), clientId);
    qurl.addQueryItem(QStringLiteral("limit"), QStringLiteral("50"));
    qurl.addQueryItem(QStringLiteral("offset"), QStringLiteral("0"));
    qurl.addQueryItem(QStringLiteral("linked_partitioning"),
                      QStringLiteral("1"));
    QUrl url{"https://api-v2.soundcloud.com/users/" + id + "/playlists"};
    url.setQuery(qurl);
    return url;
}

QUrl SoundcloudApi::makeUserStreamUrl(const QString &id) const {
    QUrlQuery qurl;
    qurl.addQueryItem(QStringLiteral("app_locale"), m_locale);
    qurl.addQueryItem(QStringLiteral("client_id"), clientId);
    qurl.addQueryItem(QStringLiteral("limit"), QStringLiteral("50"));
    qurl.addQueryItem(QStringLiteral("offset"), QStringLiteral("0"));
    qurl.addQueryItem(QStringLiteral("linked_partitioning"),
                      QStringLiteral("1"));
    QUrl url{"https://api-v2.soundcloud.com/stream/users/" + id};
    url.setQuery(qurl);
    return url;
}

QUrl SoundcloudApi::makeFeaturedTracksUrl(int limit) const {
    QUrlQuery qurl;
    qurl.addQueryItem(QStringLiteral("app_locale"), m_locale);
    qurl.addQueryItem(QStringLiteral("client_id"), clientId);
    qurl.addQueryItem(QStringLiteral("limit"), QString::number(limit));
    qurl.addQueryItem(QStringLiteral("offset"),
                      QString::number(m_featuresItems.size()));
    qurl.addQueryItem(QStringLiteral("linked_partitioning"),
                      QStringLiteral("1"));
    QUrl url{QStringLiteral("https://api-v2.soundcloud.com/mixed-selections")};
    url.setQuery(qurl);
    return url;
}

void SoundcloudApi::makeMoreFeaturedItems() {
    if (!canMakeMoreFeaturedItems()) return;
    makeFeaturedItems();
}
