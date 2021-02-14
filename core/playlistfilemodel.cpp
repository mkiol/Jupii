/* Copyright (C) 2018 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <QDebug>
#include <QFileInfo>
#include <QFile>

#include "tracker.h"
#include "playlistfilemodel.h"
#include "trackercursor.h"

const QString PlaylistFileModel::playlistsQueryTemplate =
        "SELECT ?list nie:title(?list) " \
        "nie:url(?list) " \
        "nfo:entryCounter(?list) " \
        "nfo:hasMediaFileListEntry(?list) " \
        "WHERE { ?list a nmm:Playlist . " \
        "FILTER (regex(nie:title(?list), \"%1\", \"i\")) . " \
        "} " \
        "ORDER BY nie:title(?list) " \
        "LIMIT 1000";

const QString PlaylistFileModel::playlistsQueryTemplateEx =
        "SELECT ?list nie:title(?list) " \
        "nie:url(?list) " \
        "nfo:entryCounter(?list) " \
        "WHERE { ?list a nmm:Playlist . " \
        "FILTER (regex(nie:title(?list), \"%1\", \"i\") && " \
        "?list != <%2>) . " \
        "} " \
        "ORDER BY nie:title(?list) " \
        "LIMIT 1000";

PlaylistFileModel::PlaylistFileModel(QObject *parent) :
    SelectableItemModel(new PlaylistFileItem, parent)
{
}

bool PlaylistFileModel::deleteFile(const QString &playlistId)
{
    const auto item = qobject_cast<PlaylistFileItem*>(find(playlistId));
    if (item) {
        if (const auto path = item->path(); !QFile::remove(path)) {
            qWarning() << "Cannot remove playlist file:" << path;
        } else {
            updateModelEx(playlistId);
            return true;
        }
    }

    return false;
}

QList<ListItem*> PlaylistFileModel::makeItems()
{
    const QString query = m_exId.isEmpty() ?
                playlistsQueryTemplate.arg(getFilter()) :
                playlistsQueryTemplateEx.arg(getFilter(), m_exId);

    m_exId.clear();

    if (auto tracker = Tracker::instance(); tracker->query(query, false)) {
        auto result = tracker->getResult();
        return processTrackerReply(result.first, result.second);
    } else {
        qWarning() << "Tracker query error";
    }

    return {};
}

void PlaylistFileModel::updateModelEx(const QString &exId)
{
    m_exId = exId;
    updateModel();
}

QList<ListItem*> PlaylistFileModel::processTrackerReply(
        const QStringList& varNames,
        const QByteArray& data)
{
    QList<ListItem*> items;

    TrackerCursor cursor{varNames, data};
    int n = cursor.columnCount();

    if (n > 3) {
        while(cursor.next()) {
            items << new PlaylistFileItem(
                        cursor.value(0).toString(), // id
                        cursor.value(1).toString(), // title
                        QUrl{cursor.value(2).toString()}.toLocalFile(), // path
                        {}, // icon
                        cursor.value(3).toInt(), // count
                        0 // length
             );
        }
    } else {
        qWarning() << "Tracker reply for artists is incorrect";
    }

    return items;
}

PlaylistFileItem::PlaylistFileItem(const QString &id,
                   const QString &title,
                   const QString &path,
                   const QUrl &url,
                   int count,
                   int length,
                   QObject *parent) :
    SelectableItem(parent),
    m_id(id),
    m_title(title),
    m_path(path),
    m_icon(url),
    m_count(count),
    m_length(length)
{
}

QHash<int, QByteArray> PlaylistFileItem::roleNames() const
{
    QHash<int, QByteArray> names;
    names[IdRole] = "id";
    names[TitleRole] = "title";
    names[PathRole] = "path";
    names[IconRole] = "icon";
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
    case PathRole:
        return path();
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
