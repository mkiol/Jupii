/* Copyright (C) 2018-2022 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "playlistfilemodel.h"

#include <QDebug>
#include <QFile>
#include <QFileInfo>

#include "tracker.h"
#include "trackercursor.h"

const QString PlaylistFileModel::playlistsQueryTemplate{
    QStringLiteral("SELECT ?list nie:title(?list) "
                   "nie:isStoredAs(?list) "
                   "nfo:entryCounter(?list) "
                   "nfo:hasMediaFileListEntry(?list) "
                   "WHERE { ?list a nmm:Playlist . "
                   "FILTER (regex(nie:title(?list), \"%1\", \"i\")) . "
                   "} "
                   "ORDER BY nie:title(?list) LIMIT 1000")};

const QString PlaylistFileModel::playlistsQueryTemplateEx{QStringLiteral(
    "SELECT ?list nie:title(?list) "
    "nie:isStoredAs(?list) "
    "nfo:entryCounter(?list) "
    "WHERE { ?list a nmm:Playlist . "
    "FILTER (regex(nie:title(?list), \"%1\", \"i\") && ?list != <%2>) . "
    "} "
    "ORDER BY nie:title(?list) LIMIT 1000")};

PlaylistFileModel::PlaylistFileModel(QObject *parent)
    : SelectableItemModel(new PlaylistFileItem, parent) {}

bool PlaylistFileModel::deleteFile(const QString &playlistId) {
    auto *item = qobject_cast<PlaylistFileItem *>(find(playlistId));
    if (item) {
        qDebug() << item->id() << item->path() << item->icon();
        if (auto path = item->path(); !QFile::remove(path)) {
            qWarning() << "cannot remove playlist file:" << path;
        } else {
            updateModelEx(playlistId);
            return true;
        }
    }

    return false;
}

QList<ListItem *> PlaylistFileModel::makeItems() {
    const QString query =
        m_exId.isEmpty() ? playlistsQueryTemplate.arg(getFilter())
                         : playlistsQueryTemplateEx.arg(getFilter(), m_exId);

    m_exId.clear();

    if (auto *tracker = Tracker::instance(); tracker->query(query, false)) {
        auto result = tracker->getResult();
        return processTrackerReply(result.first, result.second);
    }

    qWarning() << "tracker query error";
    return {};
}

void PlaylistFileModel::updateModelEx(const QString &exId) {
    m_exId = exId;
    updateModel();
}

QList<ListItem *> PlaylistFileModel::processTrackerReply(
    const QStringList &varNames, const QByteArray &data) {
    QList<ListItem *> items;

    TrackerCursor cursor{varNames, data};
    int n = cursor.columnCount();

    if (n > 3) {
        while (cursor.next()) {
            items << new PlaylistFileItem{
                /*id=*/cursor.value(0).toString(),
                /*title=*/cursor.value(1).toString(),
                /*path=*/QUrl{cursor.value(2).toString()}.toLocalFile(),
                /*icon=*/{},
                /*count=*/cursor.value(3).toInt(),
                /*length=*/0};
        }
    } else {
        qWarning() << "tracker reply for artists is incorrect";
    }

    return items;
}

PlaylistFileItem::PlaylistFileItem(const QString &id, const QString &title,
                                   const QString &path, const QUrl &icon,
                                   int count, int length, QObject *parent)
    : SelectableItem(parent),
      m_id{id},
      m_title{title},
      m_path{path},
      m_icon{icon},
      m_count{count},
      m_length{length} {}

QHash<int, QByteArray> PlaylistFileItem::roleNames() const {
    QHash<int, QByteArray> names;
    names[IdRole] = "id";
    names[TitleRole] = "title";
    names[PathRole] = "path";
    names[IconRole] = "icon";
    names[CountRole] = "count";
    names[LengthRole] = "length";
    return names;
}

QVariant PlaylistFileItem::data(int role) const {
    switch (role) {
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
            return {};
    }
}
