/* Copyright (C) 2020-2026 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "bcmodel.h"

#include <QDebug>
#include <QList>
#include <variant>
#include <vector>

#include "bcapi.h"
#include "contentserver.h"
#include "settings.h"

const QUrl BcModel::mNotableUrl{QStringLiteral("jupii://bc-notable")};
const QUrl BcModel::mRadioUrl{QStringLiteral("jupii://bc-radio")};

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
    if (mAlbumUrl.isEmpty() && mArtistUrl.isEmpty()) {
        if (getFilter().simplified().isEmpty()) {
            if (mShowMoreRequested) {
                mShowMoreRequested = false;
                return makeFeatureItems(FeatureType::All, true);
            }
            return makeFeatureItems(FeatureType::All, false);
        } else {
            mLastIndex = 0;
            return makeSearchItems();
        }
    }
    mLastIndex = 0;
    if (mArtistUrl.isEmpty()) {
        return makeAlbumItems();
    }
    if (mArtistUrl == mNotableUrl) {
        if (mShowMoreRequested) {
            mShowMoreRequested = false;
            return makeFeatureItems(FeatureType::Notable, true);
        }
        return makeFeatureItems(FeatureType::Notable, false);
    }
    if (mArtistUrl == mRadioUrl) {
        return makeFeatureItems(FeatureType::Radio, false);
    }
    return makeArtistItems();
}

QList<ListItem *> BcModel::makeFeatureItems(FeatureType type, bool more) {
    QList<ListItem *> items;

    BcApi api;
    connect(&api, &BcApi::progressChanged, this,
            [this](int n, int total) { emit progressChanged(n, total); });

    if (type == FeatureType::All || type == FeatureType::Radio) {
        auto results = type == FeatureType::All
                           ? api.latestShows(BcApi::ShowType::Radio)
                           : api.shows(BcApi::ShowType::Radio);
        if (QThread::currentThread()->isInterruptionRequested()) return items;
        items.reserve(static_cast<int>(results.size()));
        for (const auto &result : results) {
            items.push_back(
                new BcItem{result.webUrl.toString(), std::move(result.title),
                           /*artist=*/{},
                           /*album=*/{}, result.webUrl,
                           /*origUrl=*/{}, result.imageUrl,
                           /*description=*/result.description,
                           /*section=*/tr("Bandcamp Radio"), 0, Type::Type_Show,
                           /*genre=*/{}});
        }
    }

    if (type == FeatureType::All || type == FeatureType::Notable) {
        if (more) api.makeMoreNotableItems();

        auto results = api.notableItems();

        if (mCanShowMore != BcApi::canMakeMoreNotableItems()) {
            mCanShowMore = BcApi::canMakeMoreNotableItems();
            emit canShowMoreChanged();
        }

        if (QThread::currentThread()->isInterruptionRequested()) return items;

        items.reserve(static_cast<int>(results.size()));
        for (const auto &result : results) {
            items.push_back(new BcItem{
                result.webUrl.toString(), result.title, result.artist,
                result.album, result.webUrl,
                /*origUrl=*/{}, result.imageUrl,
                /*description=*/{},
                /*section=*/tr("New and Notable"), 0,
                static_cast<BcModel::Type>(result.type), result.genre});
        }
    }

    setArtistName({});
    setAlbumTitle({});

    emit progressChanged(0, 0);

    return items;
}

QList<ListItem *> BcModel::makeSearchItems() {
    QList<ListItem *> items;

    auto phrase = getFilter().simplified();

    auto results = BcApi{}.search(phrase);

    if (mCanShowMore != BcApi::canMakeMoreNotableItems()) {
        mCanShowMore = BcApi::canMakeMoreNotableItems();
        emit canShowMoreChanged();
    }

    if (QThread::currentThread()->isInterruptionRequested()) return items;

    items.reserve(static_cast<int>(results.size()));
    for (const auto &result : results) {
        items.push_back(new BcItem{result.webUrl.toString(), result.title,
                                   result.artist, result.album, result.webUrl,
                                   /*origUrl=*/{}, result.imageUrl,
                                   /*description=*/{},
                                   /*section=*/tr("Search results"), 0,
                                   static_cast<BcModel::Type>(result.type),
                                   result.genre});
    }

    //    if (!phrase.isEmpty()) {
    //        std::sort(items.begin(), items.end(), [](ListItem *a, ListItem *b)
    //        {
    //            // Artist = 1, Album = 2, Track = 3
    //            return qobject_cast<BcItem *>(a)->type() <
    //                   qobject_cast<BcItem *>(b)->type();
    //        });
    //    }

    return items;
}

QList<ListItem *> BcModel::makeTrackItemsFromBcTrack(BcApi::Track &&track) {
    return QList<ListItem *>{} << new BcItem{track.webUrl.toString(),
                                             track.title,
                                             track.artist,
                                             track.album,
                                             track.webUrl,
                                             {},
                                             track.imageUrl,
                                             /*description=*/{},
                                             /*section=*/{},
                                             track.duration,
                                             BcModel::Type::Type_Track};
}

QList<ListItem *> BcModel::makeTrackItemsFromBcShowTracks(
    BcApi::ShowTracks &&showTracks) {
    setAlbumTitle(showTracks.title);
    setArtistName({});

    QList<ListItem *> items;

    items.reserve(showTracks.featuredTracks.size() + 1);
    items.push_back(new BcItem{QString::number(showTracks.id),
                               std::move(showTracks.title),
                               /*artist=*/{},
                               /*album*/ {},
                               /*url=*/showTracks.webUrl,
                               /*origUrl=*/{}, showTracks.imageUrl,
                               /*description=*/{},
                               /*section=*/tr("Radio track"),
                               showTracks.duration, BcModel::Type::Type_Track});
    for (auto &track : showTracks.featuredTracks) {
        items.push_back(new BcItem{track.webUrl.toString(),
                                   track.title,
                                   track.artist,
                                   track.album,
                                   track.webUrl,
                                   {},
                                   track.imageUrl,
                                   /*description=*/{},
                                   /*section=*/tr("Featured tracks"),
                                   track.duration,
                                   BcModel::Type::Type_Track});
    }

    return items;
}

QList<ListItem *> BcModel::makeAlbumItems() {
    QList<ListItem *> items;

    auto album = BcApi{}.album(mAlbumUrl);

    if (QThread::currentThread()->isInterruptionRequested()) return items;

    return makeAlbumItemsFromBcAlbum(std::move(album));
}

QList<ListItem *> BcModel::makeAlbumItemsFromBcAlbum(BcApi::Album &&album) {
    QList<ListItem *> items;

    setAlbumTitle(album.title);
    setArtistName(album.artist);

    auto isShowIcon = Settings::instance()->getShowPlaylistItemIcon();

    items.reserve(static_cast<int>(album.tracks.size()));
    for (const auto &track : album.tracks) {
        items.push_back(new BcItem{track.webUrl.toString(), track.title,
                                   album.artist, album.title, track.streamUrl,
                                   track.webUrl,  // origUrl
                                   isShowIcon ? album.imageUrl : QUrl{},
                                   /*description=*/{},
                                   /*section=*/{}, track.duration,
                                   Type::Type_Track});
    }

    return items;
}

QList<ListItem *> BcModel::makeArtistItems() {
    QList<ListItem *> items;

    auto artistVariant = BcApi{}.artist(mArtistUrl);

    if (QThread::currentThread()->isInterruptionRequested()) return items;

    std::visit(
        [&](auto &&arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, BcApi::Artist>) {
                auto &artist = arg;
                setArtistName(artist.name);
                setAlbumTitle({});
                items.reserve(static_cast<int>(artist.albums.size()));
                for (const auto &album : artist.albums) {
                    items.push_back(
                        new BcItem{album.webUrl.toString(), album.title,
                                   artist.name, album.title, album.webUrl,
                                   /*origUrl=*/{}, album.imageUrl,
                                   /*description=*/{},
                                   /*section=*/{}, 0, Type::Type_Album});
                }
            } else if constexpr (std::is_same_v<T, BcApi::Album>) {
                items = makeAlbumItemsFromBcAlbum(std::move(arg));
            } else if constexpr (std::is_same_v<T, BcApi::Track>) {
                items = makeTrackItemsFromBcTrack(std::move(arg));
            } else if constexpr (std::is_same_v<T, BcApi::ShowTracks>) {
                items = makeTrackItemsFromBcShowTracks(std::move(arg));
            }
        },
        artistVariant);

    return items;
}

BcItem::BcItem(const QString &id, const QString &name, const QString &artist,
               const QString &album, const QUrl &url, const QUrl &origUrl,
               const QUrl &icon, const QString &description,
               const QString &section, int duration, BcModel::Type type,
               const QString &genre, QObject *parent)
    : SelectableItem(parent),
      m_id(id),
      m_name(name),
      m_artist(artist),
      m_album(album),
      m_url(url),
      m_origUrl(origUrl),
      m_icon(icon),
      m_description(description),
      m_section(section),
      m_duration(duration),
      m_type(type),
      m_genre(genre) {
    m_selectable = type == BcModel::Type::Type_Track;
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
    names[DescriptionRole] = "description";
    names[SectionRole] = "section";
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
            return iconThumb();
        case TypeRole:
            return type();
        case GenreRole:
            return genre();
        case DescriptionRole:
            return description();
        case SectionRole:
            return section();
        case SelectedRole:
            return selected();
        default:
            return {};
    }
}

QUrl BcItem::iconThumb() const {
    auto url = ContentServer::instance()->thumbUrl(m_icon);
    if (url.isEmpty()) return m_icon;
    return url;
}
