/* Copyright (C) 2018 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <QDebug>
#include <QHash>
#include <QFileInfo>
#include <utility>
#include <algorithm>

#include "tracker.h"
#include "albummodel.h"
#include "trackercursor.h"
#include "settings.h"

const QString AlbumModel::albumsQueryTemplate =
        "SELECT ?album " \
        "nie:title(?album) AS ?title " \
        "nmm:artistName(?artist) " \
        "COUNT(?song) " \
        "SUM(?length) " \
        "WHERE { ?album a nmm:MusicAlbum . " \
        "FILTER (regex(nie:title(?album), \"%1\", \"i\") || " \
        "regex(nmm:artistName(?artist), \"%1\", \"i\")) " \
        "?song nmm:musicAlbum ?album; " \
        "nfo:duration ?length; " \
        "nmm:performer ?artist . " \
        "} GROUP BY ?album " \
        "ORDER BY ?title " \
        "LIMIT 1000";

AlbumModel::AlbumModel(QObject *parent) :
    SelectableItemModel(new AlbumItem, parent)
{
    auto s = Settings::instance();
    m_queryType = s->getAlbumQueryType();
}

QList<ListItem*> AlbumModel::makeItems()
{
    const auto query = albumsQueryTemplate.arg(getFilter());
    auto tracker = Tracker::instance();
    if (tracker->query(query, false)) {
        auto result = tracker->getResult();
        return processTrackerReply(result.first, result.second);
    } else {
        qWarning() << "Tracker query error";
    }

    return {};
}

QList<ListItem*> AlbumModel::processTrackerReply(
        const QStringList& varNames,
        const QByteArray& data)
{
    QList<ListItem*> items;

    TrackerCursor cursor(varNames, data);
    int n = cursor.columnCount();

    if (n > 4) {
        QHash<QString, AlbumData> albums; // album id => album data

        while(cursor.next()) {
            auto id = cursor.value(0).toString();

            if (albums.contains(id)) {
                qDebug() << "Duplicate album id, updating count, length and skiping";

                auto& album = albums[id];
                album.count += cursor.value(3).toInt();
                album.length += cursor.value(4).toInt();

                continue;
            }

            auto imgFilePath = Tracker::genAlbumArtFile(cursor.value(1).toString(),
                                                        cursor.value(2).toString());
            QFileInfo imgFile(imgFilePath);
            auto& album = albums[id];
            album.id = id;
            album.title = cursor.value(1).toString();
            album.artist = cursor.value(2).toString();
            album.icon = imgFile.exists() ? QUrl(imgFilePath) : QUrl();
            album.count = cursor.value(3).toInt();
            album.length = cursor.value(4).toInt();
        }

        auto end = albums.cend();
        for (auto it = albums.cbegin(); it != end; ++it) {
            const auto& album = it.value();
            items << new AlbumItem(
                        album.id,
                        album.title,
                        album.artist,
                        album.icon,
                        album.count,
                        album.length);
        }

        // Sorting
        if (m_queryType == 0) { // by album title
            std::sort(items.begin(), items.end(), [](ListItem *a, ListItem *b) {
                auto aa = qobject_cast<AlbumItem*>(a);
                auto bb = qobject_cast<AlbumItem*>(b);
                return aa->title().compare(bb->title(), Qt::CaseInsensitive) < 0;
            });
        } else { // by artist
            std::sort(items.begin(), items.end(), [](ListItem *a, ListItem *b) {
                auto aa = qobject_cast<AlbumItem*>(a);
                auto bb = qobject_cast<AlbumItem*>(b);
                return aa->artist().compare(bb->artist(), Qt::CaseInsensitive) < 0;
            });
        }
    } else {
        qWarning() << "Tracker reply for albums is incorrect";
    }

    return items;
}

int AlbumModel::getQueryType()
{
    return m_queryType;
}

void AlbumModel::setQueryType(int value)
{
    if (value != m_queryType) {
        m_queryType = value;
        emit queryTypeChanged();
        auto s = Settings::instance();
        s->setAlbumQueryType(m_queryType);
        updateModel();
    }
}

AlbumItem::AlbumItem(const QString &id,
                   const QString &title,
                   const QString &artist,
                   const QUrl &icon,
                   int count,
                   int length,
                   QObject *parent) :
    SelectableItem(parent),
    m_id(id),
    m_title(title),
    m_artist(artist),
    m_icon(icon),
    m_count(count),
    m_length(length)
{
}

QHash<int, QByteArray> AlbumItem::roleNames() const
{
    QHash<int, QByteArray> names;
    names[IdRole] = "id";
    names[TitleRole] = "title";
    names[ArtistRole] = "artist";
    names[IconRole] = "icon";
    names[CountRole] = "count";
    names[LengthRole] = "length";
    return names;
}

QVariant AlbumItem::data(int role) const
{
    switch(role) {
    case IdRole:
        return id();
    case TitleRole:
        return title();
    case ArtistRole:
        return artist();
    case IconRole:
        return icon();
    case CountRole:
        return count();
    case LengthRole:
        return length();
    default:
        return {};
    }
}
