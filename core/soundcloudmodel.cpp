/* Copyright (C) 2021 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "soundcloudmodel.h"

#include <QDebug>
#include <QList>
#include <vector>

#include "soundcloudapi.h"
#include "utils.h"
#include "iconprovider.h"

SoundcloudModel::SoundcloudModel(QObject *parent) :
    SelectableItemModel(new SoundcloudItem, parent)
{
}

SoundcloudModel::~SoundcloudModel()
{
    m_worker.reset();
}

QUrl SoundcloudModel::getAlbumUrl() const
{
    return albumUrl;
}

void SoundcloudModel::setAlbumUrl(const QUrl& albumUrl)
{
    if (this->albumUrl != albumUrl) {
        this->albumUrl = albumUrl;
        emit albumUrlChanged();
    }
}

QString SoundcloudModel::getAlbumTitle() const
{
    return albumTitle;
}

void SoundcloudModel::setAlbumTitle(const QString& albumTitle)
{
    if (this->albumTitle != albumTitle) {
        this->albumTitle = albumTitle;
        emit albumTitleChanged();
    }
}

QUrl SoundcloudModel::getArtistUrl() const
{
    return artistUrl;
}

void SoundcloudModel::setArtistUrl(const QUrl& artistUrl)
{
    if (this->artistUrl != artistUrl) {
        this->artistUrl = artistUrl;
        this->lastArtist.reset();
        emit artistUrlChanged();
    }
}

QString SoundcloudModel::getArtistName() const
{
    return artistName;
}

void SoundcloudModel::setArtistName(const QString& artistName)
{
    if (this->artistName != artistName) {
        this->artistName = artistName;
        emit artistNameChanged();
    }
}

QVariantList SoundcloudModel::selectedItems()
{
    QVariantList list;

    foreach (const auto item, m_list) {
        const auto scitem = qobject_cast<SoundcloudItem*>(item);
        if (scitem->selected()) {
            QVariantMap map;
            map.insert("url", scitem->url());
            map.insert("name", scitem->name());
            map.insert("author", scitem->artist());
            map.insert("icon", scitem->icon());
            map.insert("app", "soundcloud");
            list << map;
        }
    }

    return list;
}

QList<ListItem*> SoundcloudModel::makeItems()
{
    return albumUrl.isEmpty() && artistUrl.isEmpty() ? makeSearchItems() :
           artistUrl.isEmpty() ? makeAlbumItems() : makeArtistItems();
}

QList<ListItem*> SoundcloudModel::makeSearchItems()
{
    QList<ListItem*> items;

    const auto phrase = getFilter().simplified();

    if (!phrase.isEmpty()) {
        const auto results = SoundcloudApi{}.search(phrase);

        if (QThread::currentThread()->isInterruptionRequested())
            return items;

        for (const auto& result : results) {
            items << new SoundcloudItem{result.webUrl.toString(),
                                result.title,
                                result.artist,
                                result.album,
                                result.webUrl,
                                result.imageUrl,
                                SoundcloudModel::Type(result.type)};
        }

        // Sorting
        std::sort(items.begin(), items.end(), [](ListItem *a, ListItem *b) {
            const auto aa = qobject_cast<SoundcloudItem*>(a);
            const auto bb = qobject_cast<SoundcloudItem*>(b);
            // Artist = 1, Album = 2, Track = 3
            return aa->type() < bb->type();
        });
    }

    return items;
}

QList<ListItem*> SoundcloudModel::makeAlbumItems()
{
    QList<ListItem*> items;

    const auto album = SoundcloudApi{}.playlist(albumUrl);

    if (QThread::currentThread()->isInterruptionRequested())
        return items;

    setAlbumTitle(album.title);

    for (const auto& track : album.tracks) {
        items << new SoundcloudItem{track.webUrl.toString(),
                            track.title,
                            track.artist,
                            track.album,
                            track.webUrl,
                            track.imageUrl,
                            Type_Track};
    }

    return items;
}

QList<ListItem*> SoundcloudModel::makeArtistItems()
{
    QList<ListItem*> items;

    if (!lastArtist || lastArtist->webUrl != artistUrl) {
        lastArtist = SoundcloudApi{}.user(artistUrl);
        setArtistName(lastArtist->name);
    }

    if (QThread::currentThread()->isInterruptionRequested())
        return items;

    const auto phrase = getFilter().simplified();

    for (const auto& album : lastArtist->playlists) {
        if (!phrase.isEmpty() &&
            !album.title.contains(phrase, Qt::CaseInsensitive) &&
            !album.artist.contains(phrase, Qt::CaseInsensitive)) {
            continue;
        }

        items << new SoundcloudItem{album.webUrl.toString(),
                            album.title,
                            album.artist,
                            album.title,
                            album.webUrl,
                            album.imageUrl,
                            Type_Album};
    }

    for (const auto& track : lastArtist->tracks) {
        if (!phrase.isEmpty() &&
            !track.title.contains(phrase, Qt::CaseInsensitive) &&
            !track.artist.contains(phrase, Qt::CaseInsensitive) &&
            !track.album.contains(phrase, Qt::CaseInsensitive)) {
            continue;
        }

        items << new SoundcloudItem{track.webUrl.toString(),
                            track.title,
                            track.artist,
                            track.album,
                            track.webUrl,
                            track.imageUrl,
                            Type_Track};
    }

    return items;
}

SoundcloudItem::SoundcloudItem(const QString &id,
                   const QString &name,
                   const QString &artist,
                   const QString &album,
                   const QUrl &url,
                   const QUrl &icon,
                   SoundcloudModel::Type type,
                   QObject *parent) :
    SelectableItem(parent),
    m_id(id),
    m_name(name),
    m_artist(artist),
    m_album(album),
    m_url(url),
    m_icon(icon),
    m_type(type)
{
    m_selectable = type == SoundcloudModel::Type_Track;
}

QHash<int, QByteArray> SoundcloudItem::roleNames() const
{
    QHash<int, QByteArray> names;
    names[IdRole] = "id";
    names[NameRole] = "name";
    names[ArtistRole] = "artist";
    names[AlbumRole] = "album";
    names[UrlRole] = "url";
    names[IconRole] = "icon";
    names[TypeRole] = "type";
    names[SelectedRole] = "selected";
    return names;
}

QVariant SoundcloudItem::data(int role) const
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
    case IconRole:
        return icon();
    case TypeRole:
        return type();
    case SelectedRole:
        return selected();
    default:
        return {};
    }
}
