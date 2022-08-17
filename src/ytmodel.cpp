/* Copyright (C) 2022 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "ytmodel.h"

#include <QDebug>
#include <QList>
#include <vector>

#include "utils.h"
#include "ytdlapi.h"

const QString YtModel::mHomeId{QStringLiteral("jupii://yt-home")};

YtModel::YtModel(QObject *parent) : SelectableItemModel(new YtItem, parent) {}

void YtModel::setAlbumId(const QString &albumId) {
    if (this->mAlbumId != albumId) {
        this->mAlbumId = albumId;
        emit albumIdChanged();
    }
}

void YtModel::setAlbumType(Type albumType) {
    if (this->mAlbumType != albumType) {
        this->mAlbumType = albumType;
        emit albumTypeChanged();
    }
}

void YtModel::setAlbumTitle(const QString &albumTitle) {
    if (this->mAlbumTitle != albumTitle) {
        this->mAlbumTitle = albumTitle;
        emit albumTitleChanged();
    }
}

void YtModel::setArtistId(const QString &artistId) {
    if (this->mArtistId != artistId) {
        this->mArtistId = artistId;
        emit artistIdChanged();
    }
}

void YtModel::setArtistName(const QString &artistName) {
    if (this->mArtistName != artistName) {
        this->mArtistName = artistName;
        emit artistNameChanged();
    }
}

QVariantList YtModel::selectedItems() {
    QVariantList list;

    foreach (const auto item, m_list) {
        auto *ytitem = qobject_cast<YtItem *>(item);
        if (ytitem->selected()) {
            QVariantMap map;
            map.insert(QStringLiteral("url"), ytitem->url());
            map.insert(QStringLiteral("name"), ytitem->name());
            map.insert(QStringLiteral("author"), ytitem->artist());
            map.insert(QStringLiteral("icon"), ytitem->icon());
            map.insert(QStringLiteral("origUrl"), ytitem->origUrl());
            map.insert(QStringLiteral("app"), QStringLiteral("yt"));
            map.insert(QStringLiteral("duration"), ytitem->duration());
            map.insert(QStringLiteral("album"), ytitem->album());
            list << map;
        }
    }

    return list;
}

QList<ListItem *> YtModel::makeItems() {
    if (mAlbumId.isEmpty() && mArtistId.isEmpty()) return makeSearchItems();
    if (mArtistId == mHomeId) return makeHomeItems();
    if (mArtistId.isEmpty()) return makeAlbumItems();
    return makeArtistItems();
}

QList<ListItem *> YtModel::makeHomeItems() {
    QList<ListItem *> items;

    auto results = YtdlApi{}.home();

    if (QThread::currentThread()->isInterruptionRequested()) return items;

    std::transform(std::make_move_iterator(results.begin()),
                   std::make_move_iterator(results.end()),
                   std::back_inserter(items), [](auto result) {
                       return new YtItem{std::move(result.id),
                                         std::move(result.title),
                                         std::move(result.artist),
                                         std::move(result.album),
                                         /*url=*/std::move(result.webUrl),
                                         /*origUrl=*/{},
                                         std::move(result.imageUrl),
                                         /*section=*/std::move(result.section),
                                         result.duration,
                                         static_cast<Type>(result.type)};
                   });
    return items;
}

QList<ListItem *> YtModel::makeSearchItems() {
    QList<ListItem *> items;

    auto phrase = getFilter().simplified();

    auto results =
        phrase.isEmpty() ? YtdlApi{}.homeFirstPage() : YtdlApi{}.search(phrase);

    if (QThread::currentThread()->isInterruptionRequested()) return items;

    std::transform(std::make_move_iterator(results.begin()),
                   std::make_move_iterator(results.end()),
                   std::back_inserter(items), [](auto result) {
                       return new YtItem{std::move(result.id),
                                         std::move(result.title),
                                         std::move(result.artist),
                                         std::move(result.album),
                                         /*url=*/std::move(result.webUrl),
                                         /*origUrl=*/{},
                                         std::move(result.imageUrl),
                                         /*section=*/std::move(result.section),
                                         result.duration,
                                         static_cast<Type>(result.type)};
                   });

    if (!phrase.isEmpty()) {  // don't sort home items
        std::sort(items.begin(), items.end(), [](ListItem *a, ListItem *b) {
            return qobject_cast<YtItem *>(a)->type() <
                   qobject_cast<YtItem *>(b)->type();
        });
    }

    return items;
}

QList<ListItem *> YtModel::makeAlbumItems() {
    QList<ListItem *> items;

    auto album = mAlbumType == Type::Type_Playlist
                     ? YtdlApi{}.playlist(mAlbumId)
                     : YtdlApi{}.album(mAlbumId);

    if (!album) return items;

    if (QThread::currentThread()->isInterruptionRequested()) return items;

    setAlbumTitle(album->title);

    std::transform(std::make_move_iterator(album->tracks.begin()),
                   std::make_move_iterator(album->tracks.end()),
                   std::back_inserter(items),
                   [&album, icon = mAlbumType == Type::Type_Playlist
                                       ? QUrl{}
                                       : album->imageUrl](auto result) {
                       return new YtItem{std::move(result.id),
                                         std::move(result.title),
                                         std::move(result.artist),
                                         album->title,
                                         /*url=*/std::move(result.webUrl),
                                         /*origUrl=*/{},
                                         /*icon=*/icon,
                                         /*section=*/{},
                                         result.duration,
                                         Type::Type_Video};
                   });

    return items;
}

QList<ListItem *> YtModel::makeArtistItems() {
    QList<ListItem *> items;

    auto artist = YtdlApi{}.artist(mArtistId);

    if (!artist) return items;

    if (QThread::currentThread()->isInterruptionRequested()) return items;

    setArtistName(artist->name);

    std::transform(std::make_move_iterator(artist->albums.begin()),
                   std::make_move_iterator(artist->albums.end()),
                   std::back_inserter(items), [&artist](auto result) {
                       return new YtItem{std::move(result.id),
                                         result.title,
                                         artist->name,
                                         /*album=*/std::move(result.title),
                                         /*url=*/{},
                                         /*origUrl=*/{},
                                         std::move(result.imageUrl),
                                         /*section=*/{},
                                         0,
                                         Type::Type_Album};
                   });
    std::transform(std::make_move_iterator(artist->tracks.begin()),
                   std::make_move_iterator(artist->tracks.end()),
                   std::back_inserter(items), [&artist](auto result) {
                       return new YtItem{std::move(result.id),
                                         std::move(result.title),
                                         artist->name,
                                         std::move(result.album),
                                         /*url=*/std::move(result.webUrl),
                                         /*origUrl=*/{},
                                         std::move(result.imageUrl),
                                         /*section=*/{},
                                         result.duration,
                                         Type::Type_Video};
                   });
    return items;
}

YtItem::YtItem(QString id, QString name, QString artist, QString album,
               QUrl url, QUrl origUrl, QUrl icon, QString section, int duration,
               YtModel::Type type, QObject *parent)
    : SelectableItem(parent), m_id{std::move(id)}, m_name{std::move(name)},
      m_artist{std::move(artist)}, m_album{std::move(album)},
      m_url{std::move(url)}, m_origUrl{std::move(origUrl)}, m_icon{std::move(
                                                                icon)},
      m_section{std::move(section)}, m_duration{duration}, m_type{type} {
    m_selectable = type == YtModel::Type_Video;
}

QHash<int, QByteArray> YtItem::roleNames() const {
    QHash<int, QByteArray> names;
    names[IdRole] = "id";
    names[NameRole] = "name";
    names[ArtistRole] = "artist";
    names[AlbumRole] = "album";
    names[UrlRole] = "url";
    names[OrigUrlRole] = "origurl";
    names[IconRole] = "icon";
    names[DurationRole] = "duration";
    names[TypeRole] = "type";
    names[SectionRole] = "section";
    names[SelectedRole] = "selected";
    return names;
}

QVariant YtItem::data(int role) const {
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
        case DurationRole:
            return durationStr();
        case TypeRole:
            return type();
        case SectionRole:
            return section();
        case SelectedRole:
            return selected();
        default:
            return {};
    }
}

QString YtItem::durationStr() const {
    if (duration() > 0) return Utils::secToStrStatic(duration());
    return {};
}
