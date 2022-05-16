/* Copyright (C) 2021-2022 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef SOUNDCLOUDAPI_H
#define SOUNDCLOUDAPI_H

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QNetworkAccessManager>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonArray>
#include <memory>
#include <functional>
#include <optional>

#include "gumbotools.h"

class SoundcloudApi : public QObject
{
    Q_OBJECT
public:
    enum class Type {
        Unknown = 0,
        User,
        Playlist,
        Track,
    };

    struct SearchResultItem {
        Type type = Type::Unknown;
        QString title;
        QString artist; // or username
        QString album;
        QUrl imageUrl;
        QUrl webUrl;
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

    struct PlaylistTrack {
        QString title;
        QString artist;
        QString album;
        QUrl imageUrl;
        QUrl webUrl;
    };

    struct Playlist {
        QString title;
        QString artist;
        QUrl imageUrl;
        std::vector<PlaylistTrack> tracks;
    };

    struct UserPlaylist {
        QString title;
        QString artist;
        QUrl imageUrl;
        QUrl webUrl;
    };

    struct UserTrack {
        QString title;
        QString artist; // or username
        QString album;
        QUrl imageUrl;
        QUrl webUrl;
    };

    struct User {
        QString name;
        QUrl imageUrl;
        QUrl webUrl;
        std::vector<UserPlaylist> playlists;
        std::vector<UserTrack> tracks;
    };

    SoundcloudApi(std::shared_ptr<QNetworkAccessManager> nam = {}, QObject *parent = nullptr);
    std::vector<SearchResultItem> search(const QString &query) const;
    std::vector<SearchResultItem> featuredItems(int max = maxFeatured) const;
    std::vector<SearchResultItem> featuredItemsFirstPage() const;
    Track track(const QUrl &url);
    User user(const QUrl &url);
    Playlist playlist(const QUrl &url);
    static bool validUrl(const QUrl &url);

private:
    static QString clientId;
    static const int maxFeatured;
    static const int maxFeaturedFirstPage;
    std::shared_ptr<QNetworkAccessManager> nam;
    QString locale;

    void discoverClientId();
    std::optional<SearchResultItem> searchItem(const QJsonObject &obj) const;
    QJsonDocument downloadJsonData(const QUrl &url) const;
    gumbo::GumboOutput_ptr downloadHtmlData(const QUrl &url) const;
    static QJsonDocument parseJsonData(const QByteArray &data);
    static Type textToType(const QString &text);
    QUrl makeSearchUrl(const QString &phrase) const;
    QUrl makeUserPlaylistsUrl(const QString &id) const;
    QUrl makePlaylistUrl(const QString &id) const;
    QUrl makeUserStreamUrl(const QString &id) const;
    QUrl makeTrackUrl(const QString &id) const;
    QUrl makeFeaturedTracksUrl(int limit = maxFeatured) const;
    static QString extractData(const QString &text);
    static QString extractClientId(const QString &text);
    static QString extractUserId(const QString &text);
    static QString extractPlaylistId(const QString &text);
    static QString extractUsernameFromTitle(const QString &text);
    static bool mediaOk(const QJsonArray &media);
    QJsonArray extractItems(const QUrl &url) const;
    void addClientId(QUrl *url) const;
    void user(const QUrl &url, User *user, int count = 0) const;
};

#endif // SOUNDCLOUDAPI_H
