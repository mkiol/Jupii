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
#include "playlistfilemodel.h"
#include "trackercursor.h"

PlaylistFileModel::PlaylistFileModel(QObject *parent) :
    ListModel(new PlaylistFileItem, parent)
{
    m_playlistsQueryTemplate =  "SELECT ?list nie:title(?list) AS title " \
                                "nfo:entryCounter(?list) AS counter " \
                                "nfo:hasMediaFileListEntry(?list) AS list" \
                                "WHERE { ?list a nmm:Playlist . " \
                                "FILTER regex(nie:title(?list), \"%1\", \"i\") " \
                                "} " \
                                "ORDER BY nie:title(?list) " \
                                "LIMIT 500";

    m_tracksQueryTemplate = "SELECT nfo:entryUrl(?song) " \
                            "WHERE { ?song a nfo:MediaFileListEntry . " \
                            "FILTER ( ?song in ( %1 )) } " \
                            "LIMIT 500";

    updateModel();
}

PlaylistFileModel::~PlaylistFileModel()
{
}

int PlaylistFileModel::getCount()
{
    return m_list.length();
}

void PlaylistFileModel::updateModel()
{
    const QString query = m_playlistsQueryTemplate.arg(m_filter);

    auto tracker = Tracker::instance();
    QObject::connect(tracker, &Tracker::queryFinished,
                     this, &PlaylistFileModel::processTrackerReply);
    QObject::connect(tracker, &Tracker::queryError,
                     this, &PlaylistFileModel::processTrackerError);

    tracker->queryAsync(query);
}

void PlaylistFileModel::querySongs(const QString &playlistId)
{
    const auto item = static_cast<PlaylistFileItem*>(find(playlistId));
    QStringList list = item->list().split(',');
    list.replaceInStrings(QRegExp("^(.*)$"), "<\\1>");

    const QString query = m_tracksQueryTemplate.arg(list.join(','));

    auto tracker = Tracker::instance();
    QObject::connect(tracker, &Tracker::queryFinished,
                     this, &PlaylistFileModel::processTrackerReply);
    QObject::connect(tracker, &Tracker::queryError,
                     this, &PlaylistFileModel::processTrackerError);

    tracker->queryAsync(query);
}

void PlaylistFileModel::processTrackerError()
{
    auto tracker = Tracker::instance();
    QObject::disconnect(tracker, 0, 0, 0);

    clear();
}

void PlaylistFileModel::processTrackerReply(const QStringList& varNames,
                                     const QByteArray& data)
{
    auto tracker = Tracker::instance();
    QObject::disconnect(tracker, 0, 0, 0);

    TrackerCursor cursor(varNames, data);

    int n = cursor.columnCount();

    if (n > 3) {
        // Result of PlaylistFile query

        clear();

        while(cursor.next()) {
            appendRow(new PlaylistFileItem(
                                    cursor.value(0).toString(),
                                    cursor.value(1).toString(),
                                    cursor.value(3).toString(),
                                    QUrl(),
                                    cursor.value(2).toInt(),
                                    0));
        }

        if (m_list.length() > 0)
            emit countChanged();

        return;
    }

    if (n > 0) {
        // Result of Song query

        QStringList songs;

        while(cursor.next()) {
            QUrl file(cursor.value(0).toString());
            songs << file.toLocalFile();
        }

        emit songsQueryResult(songs);

        return;
    }
}

void PlaylistFileModel::clear()
{
    if (rowCount() == 0)
        return;

    removeRows(0,rowCount());

    emit countChanged();
}

void PlaylistFileModel::setFilter(const QString &filter)
{
    if (m_filter != filter) {
        m_filter = filter;
        emit filterChanged();

        updateModel();
    }
}

QString PlaylistFileModel::getFilter()
{
    return m_filter;
}

PlaylistFileItem::PlaylistFileItem(const QString &id,
                   const QString &title,
                   const QString &list,
                   const QUrl &image,
                   int count,
                   int length,
                   QObject *parent) :
    ListItem(parent),
    m_id(id),
    m_title(title),
    m_list(list),
    m_image(image),
    m_count(count),
    m_length(length)
{
}

QHash<int, QByteArray> PlaylistFileItem::roleNames() const
{
    QHash<int, QByteArray> names;
    names[IdRole] = "id";
    names[TitleRole] = "title";
    names[ListRole] = "list";
    names[ImageRole] = "image";
    names[CountRole] = "count";
    names[LengthRole] = "length";
    return names;
}

QVariant PlaylistFileItem::data(int role) const
{
    switch(role) {
    case IdRole:
        return id();
    case TitleRole:
        return title();
    case ListRole:
        return list();
    case ImageRole:
        return image();
    case CountRole:
        return count();
    case LengthRole:
        return length();
    default:
        return QVariant();
    }
}
