/* Copyright (C) 2020 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <QDebug>
#include <QList>
#include <vector>

#include "bcmodel.h"
#include "bcapi.h"
#include "utils.h"
#include "iconprovider.h"

BcModel::BcModel(QObject *parent) :
    SelectableItemModel(new BcItem, parent)
{
}

QUrl BcModel::getAlbumUrl() const
{
    return albumUrl;
}

void BcModel::setAlbumUrl(const QUrl& albumUrl)
{
    if (this->albumUrl != albumUrl) {
        this->albumUrl = albumUrl;
        emit albumUrlChanged();
    }
}

QString BcModel::getAlbumTitle() const
{
    return albumTitle;
}

void BcModel::setAlbumTitle(const QString& albumTitle)
{
    if (this->albumTitle != albumTitle) {
        this->albumTitle = albumTitle;
        emit albumTitleChanged();
    }
}

QUrl BcModel::getArtistUrl() const
{
    return artistUrl;
}

void BcModel::setArtistUrl(const QUrl& artistUrl)
{
    if (this->artistUrl != artistUrl) {
        this->artistUrl = artistUrl;
        emit artistUrlChanged();
    }
}

QString BcModel::getArtistName() const
{
    return artistName;
}

void BcModel::setArtistName(const QString& artistName)
{
    if (this->artistName != artistName) {
        this->artistName = artistName;
        emit artistNameChanged();
    }
}

QVariantList BcModel::selectedItems()
{
    QVariantList list;

    foreach (const auto item, m_list) {
        const auto bcitem = qobject_cast<BcItem*>(item);
        if (bcitem->selected()) {
            QVariantMap map;
            map.insert("url", bcitem->url());
            map.insert("name", bcitem->name());
            map.insert("author", bcitem->artist());
            map.insert("icon", bcitem->icon());
            map.insert("origUrl", bcitem->origUrl());
            map.insert("app", "bc");
            map.insert("duration", bcitem->duration());
            list << map;
        }
    }

    return list;
}

QList<ListItem*> BcModel::makeItems()
{
    return albumUrl.isEmpty() && artistUrl.isEmpty() ? makeSearchItems() :
           artistUrl.isEmpty() ? makeAlbumItems() : makeArtistItems();
}

QList<ListItem*> BcModel::makeSearchItems()
{
    QList<ListItem*> items;

    const auto phrase = getFilter().simplified();

    if (!phrase.isEmpty()) {
        BcApi api;

        const auto results = api.search(phrase);

        for (const auto& result : results) {
            items << new BcItem(result.webUrl.toString(),
                                result.title,
                                result.artist,
                                result.album,
                                result.webUrl,
                                {},
                                result.imageUrl,
                                0,
                                BcModel::Type(result.type));
        }

        // Sorting
        std::sort(items.begin(), items.end(), [](ListItem *a, ListItem *b) {
            auto aa = qobject_cast<BcItem*>(a);
            auto bb = qobject_cast<BcItem*>(b);
            // Artist = 1, Album = 2, Track = 3
            return aa->type() < bb->type();
        });
    }

    return items;
}

QList<ListItem*> BcModel::makeAlbumItems()
{
    QList<ListItem*> items;

    BcApi api;

    const auto album = api.album(albumUrl);

    setAlbumTitle(album.title);

    for (const auto& track : album.tracks) {
        items << new BcItem(track.webUrl.toString(),
                            track.title,
                            album.artist,
                            album.title,
                            track.streamUrl,
                            track.webUrl, // origUrl
                            album.imageUrl,
                            track.duration,
                            Type_Track);
    }

    return items;
}

QList<ListItem*> BcModel::makeArtistItems()
{
    QList<ListItem*> items;

    BcApi api;

    const auto artist = api.artist(artistUrl);

    setArtistName(artist.name);

    for (const auto& album : artist.albums) {
        items << new BcItem(album.webUrl.toString(),
                            album.title,
                            artist.name,
                            album.title,
                            album.webUrl,
                            {},
                            album.imageUrl,
                            0,
                            Type_Album);
    }

    return items;
}

BcItem::BcItem(const QString &id,
                   const QString &name,
                   const QString &artist,
                   const QString &album,
                   const QUrl &url,
                   const QUrl &origUrl,
                   const QUrl &icon,
                   int duration,
                   BcModel::Type type,
                   QObject *parent) :
    SelectableItem(parent),
    m_id(id),
    m_name(name),
    m_artist(artist),
    m_album(album),
    m_url(url),
    m_origUrl(origUrl),
    m_icon(icon),
    m_duration(duration),
    m_type(type)
{
    m_selectable = type == BcModel::Type_Track;
}

QHash<int, QByteArray> BcItem::roleNames() const
{
    QHash<int, QByteArray> names;
    names[IdRole] = "id";
    names[NameRole] = "name";
    names[ArtistRole] = "artist";
    names[AlbumRole] = "album";
    names[UrlRole] = "url";
    names[OrigUrlRole] = "origurl";
    names[IconRole] = "icon";
    names[TypeRole] = "type";
    names[SelectedRole] = "selected";
    return names;
}

QVariant BcItem::data(int role) const
{
    switch(role) {
    case IdRole:
        return id();
    case NameRole:
        return name();
    case ArtistRole:
        return artist();
    case AlbumRole:
        return album();
    case UrlRole:
        return url();
    case OrigUrlRole:
        return origUrl();
    case IconRole:
        return icon();
    case TypeRole:
        return type();
    case SelectedRole:
        return selected();
    default:
        return QVariant();
    }
}
