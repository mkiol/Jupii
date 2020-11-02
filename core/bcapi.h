/* Copyright (C) 2020 Michal Kosciesza <michal@mkiol.net>
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
#include <functional>

#include "gumbo.h"

class BcApi : QObject
{
    Q_OBJECT
public:
    enum Type {
        Type_Unknown = 0,
        Type_Artist,
        Type_Album,
        Type_Track,
        Type_Label,
        Type_Fan,
    };

    struct SearchResultItem {
        Type type = Type_Unknown;
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

    BcApi(std::shared_ptr<QNetworkAccessManager> nam = std::shared_ptr<QNetworkAccessManager>(), QObject *parent = nullptr);
    std::vector<SearchResultItem> search(const QString &query);
    Track track(const QUrl &url);
    Album album(const QUrl &url);
    Artist artist(const QUrl &url);
    static bool validUrl(const QUrl &url);

private:
    typedef std::unique_ptr<GumboOutput, std::function<void (GumboOutput*)>> GumboOutput_ptr;

    static const int httpTimeout = 10000;
    std::shared_ptr<QNetworkAccessManager> nam;

    QByteArray downloadData(const QUrl &url);
    QUrl makeSearchUrl(const QString &phrase);
    GumboOutput_ptr parseHtmlData(const QByteArray &data);
    QJsonDocument parseJsonData(const QByteArray &data) const;

    static void search_for_class(GumboNode* node, const char* cls_name, std::vector<GumboNode*> *nodes);
    static std::vector<GumboNode*> search_for_class(GumboNode* node, const char* cls_name);
    static GumboNode* search_for_class_one(GumboNode* node, const char* cls_name);
    static GumboNode* search_for_id(GumboNode* node, const char* id);
    static GumboNode* search_for_attr_one(GumboNode* node, const char* attr_name);
    static GumboNode* search_for_attr_one(GumboNode* node, const char* attr_name, const char* attr_value);
    static GumboNode* search_for_tag_one(GumboNode* node, GumboTag tag);
    static void search_for_tag(GumboNode* node, GumboTag tag, std::vector<GumboNode*> *nodes);
    static std::vector<GumboNode*> search_for_tag(GumboNode* node, GumboTag tag);
    static QString node_text(GumboNode* node);
    static QByteArray attr_data(GumboNode* node, const char *attr_name);
    static Type textToType(const QString &text);
};

#endif // BCAPI_H
