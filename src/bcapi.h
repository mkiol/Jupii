/* Copyright (C) 2020-2026 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef BCAPI_H
#define BCAPI_H

#include <QByteArray>
#include <QDateTime>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QUrl>
#include <memory>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

class BcApi : public QObject {
    Q_OBJECT
   public:
    enum class Type { Unknown = 0, Artist, Album, Track, Label, Fan };
    enum class ShowType { Radio };
    enum class UrlType {
        Unknown,
        Artist,
        Album,
        Track,
        RadioMain /* List of all radio shows */,
        RadioShow /* List of radio track + all featured tracks */,
        RadioShowTrack /* Radio track */
    };

    struct SearchResultItem {
        Type type = Type::Unknown;
        QString title;
        QString album;
        QString artist;
        QUrl imageUrl;
        QUrl webUrl;
        QString genre;
    };

    struct Track {
        QString title;
        QString album;
        QString artist;
        QUrl imageUrl;
        QUrl webUrl;
        QUrl streamUrl;
        int duration = 0;
    };

    struct AlbumTrack {
        QString title;
        QUrl webUrl;
        QUrl streamUrl;
        int duration = 0;
    };

    struct Album {
        QString title;
        QString artist;
        QUrl imageUrl;
        std::vector<AlbumTrack> tracks;
    };

    struct ArtistAlbum {
        QString title;
        QUrl imageUrl;
        QUrl webUrl;
    };

    struct Artist {
        QString name;
        QUrl imageUrl;
        std::vector<ArtistAlbum> albums;
    };

    struct Show {
        int id;
        ShowType type = ShowType::Radio;
        QString title;
        QString description;
        QDateTime date;
        QUrl imageUrl;
        QUrl webUrl /* web URL to list of tracks */;
    };

    struct ShowTracks {
        int id;
        ShowType type = ShowType::Radio;
        QString title;
        QDateTime date;
        QUrl imageUrl;
        QUrl webUrl /* web URL to radio track */;
        int duration = 0;
        std::vector<Track> featuredTracks;
    };

    using ArtistVariant = std::variant<std::monostate, Artist, Album, Track,
                                       ShowTracks, std::vector<Show>>;

    BcApi(std::shared_ptr<QNetworkAccessManager> nam = {},
          QObject *parent = nullptr);
    std::vector<SearchResultItem> search(const QString &query) const;
    std::vector<SearchResultItem> notableItems();
    void makeMoreNotableItems();
    inline static bool canMakeMoreNotableItems() {
        return m_notableIds.size() > m_notableItems.size();
    }
    std::vector<SearchResultItem> notableItemsFirstPage();
    Track track(const QUrl &url) const;
    Album album(const QUrl &url) const;
    ArtistVariant artist(const QUrl &url);
    std::vector<Show> latestShows(ShowType type);
    std::vector<Show> shows(ShowType type);
    ShowTracks radioTracks(int showId) const;
    Track radioTrack(int showId) const;
    static bool validUrl(const QUrl &url);
    static UrlType guessUrlType(const QUrl &url);

   signals:
    void progressChanged(int n, int total);

   private:
    inline static const int maxNotable = 30;
    inline static const int maxNotableFirstPage = 30;
    inline static const int maxShows = 30;
    inline static const char *radioUrlStr = "https://bandcamp.com/radio";
    inline static const char *urlTypeQueryKey = "url_type";
    inline static const char *trackUrlTypeValue = "track";

    static std::vector<SearchResultItem> m_notableItems;
    static bool m_notableItemsDone;
    static std::vector<double> m_notableIds;
    static std::vector<Show> m_shows;
    std::shared_ptr<QNetworkAccessManager> nam;

    std::optional<QJsonDocument> parseNotableBlob() const;
    std::optional<QJsonDocument> parseRadioBlob() const;
    std::optional<SearchResultItem> notableItem(const QByteArray &bytes) const;
    static SearchResultItem notableItem(const QJsonObject &obj);
    static QUrl artUrl(const QString &id);
    static std::pair<QUrl, QByteArray> makeSearchUrl(const QString &phrase);
    static QUrl makeRadioShowUrl(int showId);
    static QUrl makeRadioShowTrackUrl(int showId);
    static QJsonDocument parseJsonData(const QByteArray &data);
    static Type textToType(const QString &text);
    static void storeNotableIds(const QJsonObject &obj);
    static void storeNotableIds(const QJsonArray &array);
    std::vector<Show> makeShows(ShowType type) const;
    bool prepareNotableIds() const;
    Track trackFromBytes(const QUrl &url, const QByteArray &bytes) const;
    Album albumFromBytes(const QUrl &url, const QByteArray &bytes) const;
    static QUrl artUrl(long imageId);
    std::optional<QJsonObject> radioShowObj(int showId) const;
};

#endif  // BCAPI_H
