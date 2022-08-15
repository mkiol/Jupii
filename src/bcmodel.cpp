/* Copyright (C) 2020-2022 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "bcmodel.h"

#include <QDebug>
#include <QList>
#include <vector>

#include "bcapi.h"

const QUrl BcModel::mNotableUrl{QStringLiteral("jupii://bc-notable")};

BcModel::BcModel(QObject *parent) : SelectableItemModel(new BcItem, parent) {}

QUrl BcModel::getAlbumUrl() const { return mAlbumUrl; }

void BcModel::setAlbumUrl(const QUrl &albumUrl) {
    if (this->mAlbumUrl != albumUrl) {
        this->mAlbumUrl = albumUrl;
        emit albumUrlChanged();
    }
}

QString BcModel::getAlbumTitle() const { return mAlbumTitle; }

void BcModel::setAlbumTitle(const QString &albumTitle) {
    if (this->mAlbumTitle != albumTitle) {
        this->mAlbumTitle = albumTitle;
        emit albumTitleChanged();
    }
}

QUrl BcModel::getArtistUrl() const { return mArtistUrl; }

void BcModel::setArtistUrl(const QUrl &artistUrl) {
    if (this->mArtistUrl != artistUrl) {
        this->mArtistUrl = artistUrl;
        emit artistUrlChanged();
    }
}

QString BcModel::getArtistName() const { return mArtistName; }

void BcModel::setArtistName(const QString &artistName) {
    if (this->mArtistName != artistName) {
        this->mArtistName = artistName;
        emit artistNameChanged();
    }
}

QVariantList BcModel::selectedItems() {
    QVariantList list;

    foreach (const auto item, m_list) {
        auto *bcitem = qobject_cast<BcItem *>(item);
        if (bcitem->selected()) {
            QVariantMap map;
            map.insert(QStringLiteral("url"), bcitem->url());
            map.insert(QStringLiteral("name"), bcitem->name());
            map.insert(QStringLiteral("author"), bcitem->artist());
            map.insert(QStringLiteral("icon"), bcitem->icon());
            map.insert(QStringLiteral("origUrl"), bcitem->origUrl());
            map.insert(QStringLiteral("app"), QStringLiteral("bc"));
            map.insert(QStringLiteral("duration"), bcitem->duration());
            map.insert(QStringLiteral("album"), bcitem->album());
            list << map;
        }
    }

    return list;
}

QList<ListItem *> BcModel::makeItems() {
    if (mAlbumUrl.isEmpty() && mArtistUrl.isEmpty()) return makeSearchItems();
    if (mArtistUrl.isEmpty()) return makeAlbumItems();
    if (mArtistUrl == mNotableUrl) {
        if (mShowMoreRequested) {
            mShowMoreRequested = false;
            return makeNotableItems(true);
        }
        return makeNotableItems(false);
    }
    return makeArtistItems();
}

QList<ListItem *> BcModel::makeNotableItems(bool more) {
    QList<ListItem *> items;

    BcApi api;
    connect(&api, &BcApi::progressChanged, this,
            [this](int n, int total) { emit progressChanged(n, total); });

    if (more) api.makeMoreNotableItems();

    auto results = api.notableItems();

    if (mCanShowMore != BcApi::canMakeMoreNotableItems()) {
        mCanShowMore = BcApi::canMakeMoreNotableItems();
        emit canShowMoreChanged();
    }

    if (QThread::currentThread()->isInterruptionRequested()) return items;

    setArtistName({});

    items.reserve(static_cast<int>(results.size()));
    for (const auto &result : results) {
        items << new BcItem{result.webUrl.toString(),
                            result.title,
                            result.artist,
                            result.album,
                            result.webUrl,
                            {},
                            result.imageUrl,
                            0,
                            static_cast<BcModel::Type>(result.type),
                            result.genre};
    }

    emit progressChanged(0, 0);

    return items;
}

QList<ListItem *> BcModel::makeSearchItems() {
    QList<ListItem *> items;

    auto phrase = getFilter().simplified();

    auto results = phrase.isEmpty() ? BcApi{}.notableItemsFirstPage()
                                    : BcApi{}.search(phrase);

    if (mCanShowMore != BcApi::canMakeMoreNotableItems()) {
        mCanShowMore = BcApi::canMakeMoreNotableItems();
        emit canShowMoreChanged();
    }

    if (QThread::currentThread()->isInterruptionRequested()) return items;

    items.reserve(static_cast<int>(results.size()));
    for (const auto &result : results) {
        items << new BcItem{result.webUrl.toString(),
                            result.title,
                            result.artist,
                            result.album,
                            result.webUrl,
                            {},
                            result.imageUrl,
                            0,
                            static_cast<BcModel::Type>(result.type),
                            result.genre};
    }

    if (!phrase.isEmpty()) {
        std::sort(items.begin(), items.end(), [](ListItem *a, ListItem *b) {
            // Artist = 1, Album = 2, Track = 3
            return qobject_cast<BcItem *>(a)->type() <
                   qobject_cast<BcItem *>(b)->type();
        });
    }

    return items;
}

QList<ListItem *> BcModel::makeAlbumItems() {
    QList<ListItem *> items;

    auto album = BcApi{}.album(mAlbumUrl);

    if (QThread::currentThread()->isInterruptionRequested()) return items;

    setAlbumTitle(album.title);

    items.reserve(static_cast<int>(album.tracks.size()));
    for (const auto &track : album.tracks) {
        items << new BcItem{track.webUrl.toString(),
                            track.title,
                            album.artist,
                            album.title,
                            track.streamUrl,
                            track.webUrl,  // origUrl
                            album.imageUrl,
                            track.duration,
                            Type_Track};
    }

    return items;
}

QList<ListItem *> BcModel::makeArtistItems() {
    QList<ListItem *> items;

    auto artist = BcApi{}.artist(mArtistUrl);

    if (QThread::currentThread()->isInterruptionRequested()) return items;

    setArtistName(artist.name);

    items.reserve(static_cast<int>(artist.albums.size()));
    for (const auto &album : artist.albums) {
        items << new BcItem{album.webUrl.toString(),
                            album.title,
                            artist.name,
                            album.title,
                            album.webUrl,
                            {},
                            album.imageUrl,
                            0,
                            Type_Album};
    }

    return items;
}

BcItem::BcItem(const QString &id, const QString &name, const QString &artist,
               const QString &album, const QUrl &url, const QUrl &origUrl,
               const QUrl &icon, int duration, BcModel::Type type,
               const QString &genre, QObject *parent)
    : SelectableItem(parent), m_id(id), m_name(name), m_artist(artist),
      m_album(album), m_url(url), m_origUrl(origUrl), m_icon(icon),
      m_duration(duration), m_type(type), m_genre(genre) {
    m_selectable = type == BcModel::Type_Track;
}

QHash<int, QByteArray> BcItem::roleNames() const {
    QHash<int, QByteArray> names;
    names[IdRole] = "id";
    names[NameRole] = "name";
    names[ArtistRole] = "artist";
    names[AlbumRole] = "album";
    names[UrlRole] = "url";
    names[OrigUrlRole] = "origurl";
    names[IconRole] = "icon";
    names[TypeRole] = "type";
    names[GenreRole] = "genre";
    names[SelectedRole] = "selected";
    return names;
}

QVariant BcItem::data(int role) const {
    switch (role) {
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
        case GenreRole:
            return genre();
        case SelectedRole:
            return selected();
        default:
            return {};
    }
}
