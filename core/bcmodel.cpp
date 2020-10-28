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

QVariantList BcModel::selectedItems()
{
    QVariantList list;

    for (auto item : m_list) {
        auto bcitem = dynamic_cast<BcItem*>(item);
        if (bcitem->selected()) {
            QVariantMap map;
            map.insert("url", QVariant(bcitem->url()));
            map.insert("name", QVariant(bcitem->name()));
            map.insert("author", QVariant(bcitem->artist()));
            map.insert("icon", QVariant(bcitem->icon()));
            list << map;
        }
    }

    return list;
}

QList<ListItem*> BcModel::makeItems()
{
    QList<ListItem*> items;

    auto phrase = getFilter().simplified();

    if (!phrase.isEmpty()) {
        BcApi api;

        auto results = api.search(phrase);

        for (const auto& result : results) {
            if (result.type == BcApi::Type_Track)
            items << new BcItem(result.webUrl.toString(),
                                result.name,
                                result.artist,
                                result.album,
                                result.webUrl,
                                result.imageUrl);
        }
    }

    return items;
}

BcItem::BcItem(const QString &id,
                   const QString &name,
                   const QString &artist,
                   const QString &album,
                   const QUrl &url,
                   const QUrl &icon,
                   QObject *parent) :
    SelectableItem(parent),
    m_id(id),
    m_name(name),
    m_artist(artist),
    m_album(album),
    m_url(url),
    m_icon(icon)
{
}

QHash<int, QByteArray> BcItem::roleNames() const
{
    QHash<int, QByteArray> names;
    names[IdRole] = "id";
    names[NameRole] = "name";
    names[ArtistRole] = "artist";
    names[AlbumRole] = "album";
    names[UrlRole] = "url";
    names[IconRole] = "icon";
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
    case IconRole:
        return icon();
    case SelectedRole:
        return selected();
    default:
        return QVariant();
    }
}
