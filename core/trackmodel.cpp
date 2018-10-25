/* Copyright (C) 2018 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <QDebug>
#include <QHash>
#include <QUrl>
#include <QFileInfo>

#include "tracker.h"
#include "trackmodel.h"
#include "trackercursor.h"
#include "utils.h"

const QString TrackModel::queryByAlbumTemplate =
      "SELECT ?song " \
      "nie:title(?song) AS title " \
      "nmm:artistName(nmm:performer(?song)) AS artist " \
      "nie:title(nmm:musicAlbum(?song)) AS album "
      "nie:url(?song) AS url " \
      "nmm:trackNumber(?song) AS number " \
      "nfo:duration(?song) AS length " \
      "nie:mimeType(?song) AS mime " \
      "WHERE { " \
      "?song a nmm:MusicPiece; " \
      "nmm:musicAlbum \"%1\" . " \
      "FILTER regex(nie:title(?song), \"%2\", \"i\") " \
      "} " \
      "ORDER BY nmm:trackNumber(?song) " \
      "LIMIT 50";

const QString TrackModel::queryByArtistTemplate =
      "SELECT ?song " \
      "nie:title(?song) AS title " \
      "nmm:artistName(nmm:performer(?song)) AS artist " \
      "nie:title(nmm:musicAlbum(?song)) AS album "
      "nie:url(?song) AS url " \
      "nmm:trackNumber(?song) AS number " \
      "nfo:duration(?song) AS length " \
      "nie:mimeType(?song) AS mime " \
      "WHERE { " \
      "?song a nmm:MusicPiece; " \
      "nmm:performer \"%1\" . " \
      "FILTER regex(nie:title(?song), \"%2\", \"i\") " \
      "} " \
      "ORDER BY nie:title(nmm:musicAlbum(?song)) nmm:trackNumber(?song) " \
      "LIMIT 50";

const QString TrackModel::queryByPlaylistTemplate =
        "SELECT nfo:hasMediaFileListEntry(?list) " \
        "WHERE { ?list a nmm:Playlist . " \
        "FILTER ( ?list in ( <%1> ) ) " \
        "} " \
        "LIMIT 1";

const QString TrackModel::queryByEntriesTemplate =
        "SELECT nfo:entryUrl(?song) " \
        "WHERE { ?song a nfo:MediaFileListEntry . " \
        "FILTER ( ?song in ( %1 )) } " \
        "LIMIT 50";

TrackModel::TrackModel(QObject *parent) :
    SelectableItemModel(new TrackItem, parent)
{
}

QList<ListItem*> TrackModel::makeItems()
{
    auto tracker = Tracker::instance();

    QString query;
    TrackerTasks task;

    if (!m_albumId.isEmpty()) {
        query = queryByAlbumTemplate.arg(m_albumId, getFilter());
        task = TaskAlbum;
    } else if (!m_artistId.isEmpty()) {
        query = queryByArtistTemplate.arg(m_artistId, getFilter());
        task = TaskArtist;
    } else if (!m_playlistId.isEmpty()) {
        query = queryByPlaylistTemplate.arg(m_playlistId);
        task = TaskPlaylist;
    } else {
        qWarning() << "Id not defined";
    }

    if (!query.isEmpty()) {
        if (tracker->query(query, false)) {
            auto result = tracker->getResult();
            return processTrackerReply(task, result.first, result.second);
        } else {
            qWarning() << "Tracker query error";
        }
    }

    return QList<ListItem*>();
}

QList<ListItem*> TrackModel::processTrackerReply(
        TrackerTasks task,
        const QStringList& varNames,
        const QByteArray& data)
{
    QList<ListItem*> items;

    TrackerCursor cursor(varNames, data);
    int n = cursor.columnCount();

    if (task == TaskPlaylist && n == 1) {
        QString entries;
        while(cursor.next()) {
            QStringList list = cursor.value(0).toString().split(',');
            list = list.mid(0, 50);
            list.replaceInStrings(QRegExp("^(.*)$"), "<\\1>");
            entries = list.join(',');
            break;
        }

        auto tracker = Tracker::instance();
        auto query = queryByEntriesTemplate.arg(entries);
        if (tracker->query(query, false)) {
            auto result = tracker->getResult();
            return processTrackerReply(TaskEntries, result.first, result.second);
        } else {
            qWarning() << "Tracker query error";
        }
    } else if (task == TaskEntries && n == 1) {
        while(cursor.next()) {
            auto id = QUrl(cursor.value(0).toString());

            QString path, name, author; QUrl icon; int t = 0;
            Utils::pathTypeNameCookieIconFromId(
                        id,
                        &path,
                        &t,
                        &name,
                        nullptr,
                        &icon,
                        nullptr,
                        &author);

            auto type = t == 0 ?
                        ContentServer::getContentTypeByExtension(id) :
                        static_cast<ContentServer::Type>(t);

            auto title = name.isEmpty() ?
                        path.isEmpty() ?
                            id.toString() :
                            QFileInfo(path).fileName() :
                        name;

            auto filter = getFilter();
            if (filter.isEmpty() ||
                    title.contains(filter, Qt::CaseInsensitive) ||
                    author.contains(filter, Qt::CaseInsensitive)) {
                items << new TrackItem(
                             id.toString(), // id
                             title, // title
                             author, // artist
                             QString(), // album
                             id, // url
                             icon, // icon
                             type, // type
                             0, // number
                             0 // length
                             );
            }
        }
    } else if ((task == TaskAlbum || task == TaskArtist) && n == 8) {
        while(cursor.next()) {
            auto type = ContentServer::typeFromMime(cursor.value(7).toString());
            items << new TrackItem(
                         cursor.value(0).toString(), // id
                         cursor.value(1).toString(),
                         cursor.value(2).toString(),
                         cursor.value(3).toString(),
                         QUrl(cursor.value(4).toString()), // url
                         QUrl(), // icon
                         type,
                         cursor.value(5).toInt(),
                         cursor.value(6).toInt()
                         );
        }
    } else {
        qWarning() << "Tracker reply is incorrect";
    }

    return items;
}

QVariantList TrackModel::selectedItems()
{
    QVariantList list;

    for (auto item : m_list) {
        auto track = dynamic_cast<TrackItem*>(item);
        if (track->selected()) {
            QVariantMap map;
            map.insert("url", QVariant(track->url()));
            list << map;
        }
    }

    return list;
}

void TrackModel::setAlbumId(const QString &id)
{
    if (!m_artistId.isEmpty()) {
        m_artistId.clear();
        emit artistIdChanged();
    }

    if (m_albumId != id) {
        m_albumId = id;
        emit albumIdChanged();

        updateModel();
    }
}

void TrackModel::setArtistId(const QString &id)
{
    if (!m_albumId.isEmpty()) {
        m_albumId.clear();
        emit albumIdChanged();
    }

    if (m_artistId != id) {
        m_artistId = id;
        emit artistIdChanged();

        updateModel();
    }
}

void TrackModel::setPlaylistId(const QString &id)
{
    if (!m_playlistId.isEmpty()) {
        m_playlistId.clear();
        emit playlistIdChanged();
    }

    if (m_playlistId != id) {
        m_playlistId = id;
        emit playlistIdChanged();

        updateModel();
    }
}

QString TrackModel::getAlbumId()
{
    return m_albumId;
}

QString TrackModel::getArtistId()
{
    return m_artistId;
}

QString TrackModel::getPlaylistId()
{
    return m_playlistId;
}

TrackItem::TrackItem(const QString &id,
                   const QString &title,
                   const QString &artist,
                   const QString &album,
                   const QUrl &url,
                   const QUrl &icon,
                   ContentServer::Type type,
                   int number,
                   int length,
                   QObject *parent) :
    SelectableItem(parent),
    m_id(id),
    m_title(title),
    m_artist(artist),
    m_album(album),
    m_url(url),
    m_icon(icon),
    m_type(type),
    m_number(number),
    m_length(length)
{
}

QHash<int, QByteArray> TrackItem::roleNames() const
{
    QHash<int, QByteArray> names;
    names[IdRole] = "id";
    names[TitleRole] = "title";
    names[ArtistRole] = "artist";
    names[AlbumRole] = "album";
    names[UrlRole] = "url";
    names[IconRole] = "icon";
    names[NumberRole] = "number";
    names[LengthRole] = "length";
    names[TypeRole] = "type";
    names[SelectedRole] = "selected";
    return names;
}

QVariant TrackItem::data(int role) const
{
    switch(role) {
    case IdRole:
        return id();
    case TitleRole:
        return title();
    case ArtistRole:
        return artist();
    case AlbumRole:
        return album();
    case UrlRole:
        return url();
    case IconRole:
        return icon();
    case NumberRole:
        return number();
    case LengthRole:
        return length();
    case TypeRole:
        return type();
    case SelectedRole:
        return selected();
    default:
        return QVariant();
    }
}

