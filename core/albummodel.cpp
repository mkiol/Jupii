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

const QString AlbumModel::albumsQueryTemplate =
        "SELECT ?album nie:title(?album) AS title " \
        "nmm:artistName(?artist) AS artist " \
        "COUNT(?song) AS songs " \
        "SUM(?length) AS totallength " \
        "WHERE { ?album a nmm:MusicAlbum . " \
        "FILTER regex(nie:title(?album), \"%1\", \"i\") " \
        "?song nmm:musicAlbum ?album; " \
        "nfo:duration ?length; " \
        "nmm:performer ?artist . " \
        "} GROUP BY ?album ?artist " \
        "ORDER BY nie:title(?album) " \
        "LIMIT 100";

AlbumModel::AlbumModel(QObject *parent) :
    SelectableItemModel(new AlbumItem, parent)
{
}

QList<ListItem*> AlbumModel::makeItems()
{
    const QString query = albumsQueryTemplate.arg(getFilter());
    auto tracker = Tracker::instance();

    if (tracker->query(query, false)) {
        auto result = tracker->getResult();
        return processTrackerReply(result.first, result.second);
    } else {
        qWarning() << "Tracker query error";
    }

    return QList<ListItem*>();
}

QList<ListItem*> AlbumModel::processTrackerReply(
        const QStringList& varNames,
        const QByteArray& data)
{
    auto tracker = Tracker::instance();

    QList<ListItem*> items;

    TrackerCursor cursor(varNames, data);
    int n = cursor.columnCount();

    if (n > 4) {
        QHash<QString, AlbumData> albums; // album id => album data
        //QHash<QString, QString> titles; // album title => album id

        while(cursor.next()) {
            /*for (int i = 0; i < n; ++i) {
                auto name = cursor.name(i);
                auto type = cursor.type(i);
                auto value = cursor.value(i);
                qDebug() << "column:" << i;
                qDebug() << " name:" << name;
                qDebug() << " type:" << type;
                qDebug() << " value:" << value;
            }*/

            auto id = cursor.value(0).toString();

            if (albums.contains(id)) {
                qDebug() << "Duplicate album id, updating count, length and skiping";

                AlbumData& album = albums[id];
                album.count += cursor.value(3).toInt();
                album.length += cursor.value(4).toInt();

                continue;
            }

            auto title = cursor.value(1).toString();

            /*if (titles.contains(title)) {
                qDebug() << "Duplicate album title, updating count, length and skiping";

                AlbumData& album = albums[titles.value(title)];
                album.count += cursor.value(3).toInt();
                album.length += cursor.value(4).toInt();

                continue;
            }*/

            if (title == "Another Late Night") {
                qDebug() << id << title << cursor.value(2).toString();
            }

            auto imgFilePath = tracker->genAlbumArtFile(cursor.value(1).toString(),
                                                           cursor.value(2).toString());
            QFileInfo imgFile(imgFilePath);

            AlbumData& album = albums[id];
            album.id = id;
            album.title = title;
            album.artist = cursor.value(2).toString();
            album.icon = imgFile.exists() ? QUrl(imgFilePath) : QUrl();
            album.count = cursor.value(3).toInt();
            album.length = cursor.value(4).toInt();

            //titles.insert(title, id);
        }

        auto end = albums.cend();
        for (auto it = albums.cbegin(); it != end; ++it) {
            const AlbumData& album = it.value();
            items << new AlbumItem(
                        album.id,
                        album.title,
                        album.artist,
                        album.icon,
                        album.count,
                        album.length);
        }

        // Sorting
        std::sort(items.begin(), items.end(), [](ListItem *a, ListItem *b) {
            auto aa = dynamic_cast<AlbumItem*>(a);
            auto bb = dynamic_cast<AlbumItem*>(b);
            return aa->title() < bb->title();
        });

    } else {
        qWarning() << "Tracker reply for albums is incorrect";
    }

    return items;
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
        return QVariant();
    }
}
