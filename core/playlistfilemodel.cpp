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
        "SELECT ?list nie:title(?list) AS title " \
        "nie:url(?list) AS url " \
        "nfo:entryCounter(?list) AS counter " \
        "nfo:hasMediaFileListEntry(?list) AS list" \
        "WHERE { ?list a nmm:Playlist . " \
        "FILTER (regex(nie:title(?list), \"%1\", \"i\")) . " \
        "} " \
        "ORDER BY nie:title(?list) " \
        "LIMIT 500";

const QString PlaylistFileModel::playlistsQueryTemplateEx =
        "SELECT ?list nie:title(?list) AS title " \
        "nie:url(?list) AS url " \
        "nfo:entryCounter(?list) AS counter " \
        "nfo:hasMediaFileListEntry(?list) AS list" \
        "WHERE { ?list a nmm:Playlist . " \
        "FILTER (regex(nie:title(?list), \"%1\", \"i\") && " \
        "?list != <%2>) . " \
        "} " \
        "ORDER BY nie:title(?list) " \
        "LIMIT 500";

PlaylistFileModel::PlaylistFileModel(QObject *parent) :
    SelectableItemModel(new PlaylistFileItem, parent)
{
}

bool PlaylistFileModel::deleteFile(const QString &playlistId)
{
    const auto item = dynamic_cast<PlaylistFileItem*>(find(playlistId));
    if (item) {
        const QString path = item->path();

        if (QFile::remove(path)) {
            updateModelEx(playlistId);
            return true;
        } else {
            qWarning() << "Cannot remove playlist file:" << path;
        }
    }

    return false;
}

QList<ListItem*> PlaylistFileModel::makeItems()
{
    const QString query = m_exId.isEmpty() ?
                playlistsQueryTemplate.arg(getFilter()) :
                playlistsQueryTemplateEx.arg(getFilter()).arg(m_exId);

    m_exId.clear();

    auto tracker = Tracker::instance();

    if (tracker->query(query, false)) {
        auto result = tracker->getResult();
        return processTrackerReply(result.first, result.second);
    } else {
        qWarning() << "Tracker query error";
    }

    return QList<ListItem*>();
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

    TrackerCursor cursor(varNames, data);
    int n = cursor.columnCount();

    if (n > 4) {
        while(cursor.next()) {
            items << new PlaylistFileItem(
                        cursor.value(0).toString(), // id
                        cursor.value(1).toString(), // title
                        cursor.value(4).toString(), // list
                        QUrl(cursor.value(2).toString()).toLocalFile(), // path
                        QUrl(), // icon
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
                   const QString &list,
                   const QString &path,
                   const QUrl &url,
                   int count,
                   int length,
                   QObject *parent) :
    SelectableItem(parent),
    m_id(id),
    m_title(title),
    m_list(list),
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
    names[ListRole] = "list";
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
    case ListRole:
        return list();
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
