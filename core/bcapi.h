/* Copyright (C) 2020-2021 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef BCAPI_H
#define BCAPI_H

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QNetworkAccessManager>
#include <QUrl>
#include <QJsonDocument>
#include <memory>
#include <vector>
#include <optional>

class BcApi : public QObject
{
    Q_OBJECT
public:
    enum class Type {
        Unknown = 0,
        Artist,
        Album,
        Track,
        Label,
        Fan,
    };

    struct SearchResultItem {
        Type type = Type::Unknown;
        QString title;
        QString album;
        QString artist;
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

    BcApi(std::shared_ptr<QNetworkAccessManager> nam = {}, QObject *parent = nullptr);
    std::vector<SearchResultItem> search(const QString &query) const;
    std::vector<SearchResultItem> notableItems();
    std::vector<SearchResultItem> notableItemsFirstPage();
    Track track(const QUrl &url) const;
    Album album(const QUrl &url) const;
    Artist artist(const QUrl &url) const;
    static bool validUrl(const QUrl &url);

signals:
    void progressChanged(int n, int total);

private:
    static const int maxNotable;
    static const int maxNotableFirstPage;
    static std::vector<BcApi::SearchResultItem> m_notableItems;
    static bool m_notableItemsDone;
    static std::vector<double> m_notableIds;
    std::shared_ptr<QNetworkAccessManager> nam;

    std::optional<QJsonDocument> parseDataBlob() const;
    std::optional<SearchResultItem> notableItem(double id) const;
    SearchResultItem notableItem(const QJsonObject &obj) const;
    inline static QUrl artUrl(const QString &id);
    static QUrl makeSearchUrl(const QString &phrase);
    static QJsonDocument parseJsonData(const QByteArray &data);
    static Type textToType(const QString &text);
    void storeNotableIds(const QJsonObject &obj);
};

#endif // BCAPI_H
