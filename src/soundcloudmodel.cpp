/* Copyright (C) 2021-2022 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "soundcloudmodel.h"

#include <QDebug>
#include <QList>
#include <vector>

#include "contentserver.h"
#include "settings.h"
#include "soundcloudapi.h"

const QUrl SoundcloudModel::mFeaturedUrl{
    QStringLiteral("jupii://soundcloud-featured")};

SoundcloudModel::SoundcloudModel(QObject *parent)
    : SelectableItemModel(new SoundcloudItem, parent) {}

SoundcloudModel::~SoundcloudModel() { m_worker.reset(); }

QUrl SoundcloudModel::getAlbumUrl() const { return mAlbumUrl; }

void SoundcloudModel::setAlbumUrl(const QUrl &albumUrl) {
    if (this->mAlbumUrl != albumUrl) {
        this->mAlbumUrl = albumUrl;
        emit albumUrlChanged();
    }
}

QString SoundcloudModel::getAlbumTitle() const { return mAlbumTitle; }

void SoundcloudModel::setAlbumTitle(const QString &albumTitle) {
    if (this->mAlbumTitle != albumTitle) {
        this->mAlbumTitle = albumTitle;
        emit albumTitleChanged();
    }
}

QUrl SoundcloudModel::getArtistUrl() const { return mArtistUrl; }

void SoundcloudModel::setArtistUrl(const QUrl &artistUrl) {
    if (this->mArtistUrl != artistUrl) {
        this->mArtistUrl = artistUrl;
        this->mLastArtist.reset();
        emit artistUrlChanged();
    }
}

QString SoundcloudModel::getArtistName() const { return mArtistName; }

void SoundcloudModel::setArtistName(const QString &artistName) {
    if (this->mArtistName != artistName) {
        this->mArtistName = artistName;
        emit artistNameChanged();
    }
}

QVariantList SoundcloudModel::selectedItems() {
    QVariantList list;

    foreach (const auto *item, m_list) {
        const auto *scitem = qobject_cast<const SoundcloudItem *>(item);
        if (scitem->selected()) {
            QVariantMap map;
            map.insert(QStringLiteral("url"), scitem->url());
            map.insert(QStringLiteral("name"), scitem->name());
            map.insert(QStringLiteral("author"), scitem->artist());
            map.insert(QStringLiteral("album"), scitem->album());
            map.insert(QStringLiteral("icon"), scitem->icon());
            map.insert(QStringLiteral("app"), QStringLiteral("soundcloud"));
            list << map;
        }
    }

    return list;
}

QList<ListItem *> SoundcloudModel::makeItems() {
    const auto phrase = getFilter().simplified();
    if (mAlbumUrl.isEmpty() && mArtistUrl.isEmpty() && phrase.isEmpty())
        return makeFeaturedItems(false);
    if (mAlbumUrl.isEmpty() && mArtistUrl.isEmpty()) return makeSearchItems();
    if (mArtistUrl.isEmpty()) return makeAlbumItems();
    if (mArtistUrl == mFeaturedUrl) {
        if (mShowMoreRequested) {
            mShowMoreRequested = false;
            return makeFeaturedItems(true);
        }
        return makeFeaturedItems(false);
    }
    return makeArtistItems();
}

QList<ListItem *> SoundcloudModel::makeFeaturedItems(bool more) {
    QList<ListItem *> items;

    SoundcloudApi api{};

    if (more) api.makeMoreFeaturedItems();

    auto results = api.featuredItems();

    if (mCanShowMore != SoundcloudApi::canMakeMoreFeaturedItems()) {
        mCanShowMore = SoundcloudApi::canMakeMoreFeaturedItems();
        emit canShowMoreChanged();
    }

    if (QThread::currentThread()->isInterruptionRequested()) return items;

    setArtistName({});
    setAlbumTitle({});

    items.reserve(static_cast<int>(results.size()));
    for (const auto &result : results) {
        if (result.title.isEmpty() || result.items.empty()) continue;
        for (const auto &item : result.items) {
            items << new SoundcloudItem{item.webUrl.toString(),
                                        item.title,
                                        /*artist=*/{},
                                        item.album,
                                        item.webUrl,
                                        item.imageUrl,
                                        /*sections=*/result.title,
                                        item.type == SoundcloudApi::Type::Album
                                            ? SoundcloudModel::Type_Album
                                            : SoundcloudModel::Type_Playlist};
        }
    }

    return items;
}

QList<ListItem *> SoundcloudModel::makeSearchItems() {
    QList<ListItem *> items;

    auto results = SoundcloudApi{}.search(getFilter().simplified());

    if (mCanShowMore != SoundcloudApi::canMakeMoreFeaturedItems()) {
        mCanShowMore = SoundcloudApi::canMakeMoreFeaturedItems();
        emit canShowMoreChanged();
    }

    if (QThread::currentThread()->isInterruptionRequested()) return items;

    items.reserve(static_cast<int>(results.size()));
    for (const auto &result : results) {
        items << new SoundcloudItem{
            result.webUrl.toString(),
            result.title,
            result.artist,
            result.album,
            result.webUrl,
            result.imageUrl,
            /*section=*/{},
            static_cast<SoundcloudModel::Type>(result.type),
            result.genre};
    }

    // if (!phrase.isEmpty()) {
    //     std::sort(items.begin(), items.end(), [](ListItem *a, ListItem *b) {
    //         auto *aa = qobject_cast<SoundcloudItem *>(a);
    //         auto *bb = qobject_cast<SoundcloudItem *>(b);
    //         return aa->type() < bb->type();
    //     });
    // }

    return items;
}

QList<ListItem *> SoundcloudModel::makeAlbumItems() {
    QList<ListItem *> items;

    const auto album = SoundcloudApi{}.playlist(mAlbumUrl);

    if (QThread::currentThread()->isInterruptionRequested()) return items;

    setAlbumTitle(album.title);
    setArtistName(album.artist);

    auto isShowIcon = Settings::instance()->getShowPlaylistItemIcon();

    items.reserve(static_cast<int>(album.tracks.size()));
    for (const auto &track : album.tracks) {
        items << new SoundcloudItem{track.webUrl.toString(),
                                    track.title,
                                    track.artist,
                                    track.album,
                                    track.webUrl,
                                    isShowIcon ? track.imageUrl : QUrl{},
                                    /*section=*/{},
                                    Type_Track};
    }

    return items;
}

QList<ListItem *> SoundcloudModel::makeArtistItems() {
    QList<ListItem *> items;

    if (!mLastArtist || mLastArtist->webUrl != mArtistUrl) {
        mLastArtist = SoundcloudApi{}.user(mArtistUrl);
        setArtistName(mLastArtist->name);
        setAlbumTitle({});
    }

    if (QThread::currentThread()->isInterruptionRequested()) return items;

    const auto phrase = getFilter().simplified();

    for (const auto &album : mLastArtist->playlists) {
        if (!phrase.isEmpty() &&
            !album.title.contains(phrase, Qt::CaseInsensitive) &&
            !album.artist.contains(phrase, Qt::CaseInsensitive)) {
            continue;
        }

        items << new SoundcloudItem{
            album.webUrl.toString(),
            album.title,
            album.artist,
            album.title,
            album.webUrl,
            album.imageUrl,
            /*section=*/{},
            album.type == SoundcloudApi::PlaylistType::Album
                ? SoundcloudModel::Type_Album
                : SoundcloudModel::Type_Playlist};
    }

    for (const auto &track : mLastArtist->tracks) {
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
                                    /*section=*/{},
                                    Type_Track};
    }

    return items;
}

SoundcloudItem::SoundcloudItem(const QString &id, const QString &name,
                               const QString &artist, const QString &album,
                               const QUrl &url, const QUrl &icon,
                               const QString &section,
                               SoundcloudModel::Type type, const QString &genre,
                               QObject *parent)
    : SelectableItem(parent),
      m_id(id),
      m_name(name),
      m_artist(artist),
      m_album(album),
      m_url(url),
      m_icon(icon),
      m_section(section),
      m_type(type),
      m_genre(genre) {
    m_selectable = type == SoundcloudModel::Type_Track;
}

QHash<int, QByteArray> SoundcloudItem::roleNames() const {
    QHash<int, QByteArray> names;
    names[IdRole] = "id";
    names[NameRole] = "name";
    names[ArtistRole] = "artist";
    names[AlbumRole] = "album";
    names[UrlRole] = "url";
    names[IconRole] = "icon";
    names[TypeRole] = "type";
    names[GenreRole] = "genre";
    names[SectionRole] = "section";
    names[SelectedRole] = "selected";
    return names;
}

QVariant SoundcloudItem::data(int role) const {
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
        case IconRole:
            return iconThumb();
        case TypeRole:
            return type();
        case GenreRole:
            return genre();
        case SectionRole:
            return section();
        case SelectedRole:
            return selected();
        default:
            return {};
    }
}

QUrl SoundcloudItem::iconThumb() const {
    auto url = ContentServer::instance()->thumbUrl(m_icon);
    if (url.isEmpty()) return m_icon;
    return url;
}
