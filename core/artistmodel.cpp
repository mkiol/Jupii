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
#include "artistmodel.h"
#include "trackercursor.h"

const QString ArtistModel::artistsQueryTemplate =
        "SELECT ?artist nmm:artistName(?artist) AS ?artistName " \
        "COUNT(?song) " \
        "SUM(?length) " \
        "WHERE { " \
        "FILTER regex(nmm:artistName(?artist), \"%1\", \"i\") " \
        "?song a nmm:MusicPiece; " \
        "nfo:duration ?length; " \
        "nmm:performer ?artist . } " \
        "GROUP BY ?artist " \
        "ORDER BY ?artistName " \
        "LIMIT 1000";

ArtistModel::ArtistModel(QObject *parent) :
    SelectableItemModel(new ArtistItem, parent)
{
}

QList<ListItem*> ArtistModel::makeItems()
{
    const auto query = artistsQueryTemplate.arg(getFilter());
    auto tracker = Tracker::instance();

    if (tracker->query(query, false)) {
        const auto result = tracker->getResult();
        return processTrackerReply(result.first, result.second);
    } else {
        qWarning() << "Tracker query error:" << query;
    }

    return {};
}

QList<ListItem*> ArtistModel::processTrackerReply(
        const QStringList& varNames,
        const QByteArray& data)
{
    QList<ListItem*> items;

    TrackerCursor cursor(varNames, data);
    int n = cursor.columnCount();

    if (n > 3) {
        QStringList artists;

        while(cursor.next()) {
            const auto id = cursor.value(0).toString();

            if (artists.contains(id)) {
#ifdef QT_DEBUG
                qDebug() << "Duplicate artist, skiping";
#endif
                continue;
            }

            items << new ArtistItem{
                         id,
                         cursor.value(1).toString(),
                         QUrl(), // icon
                         cursor.value(2).toInt(),
                         cursor.value(3).toInt()
                         };

            artists << id;
        }
    } else {
        qWarning() << "Tracker reply for artists is incorrect";
    }

    return items;
}

ArtistItem::ArtistItem(const QString &id,
                   const QString &name,
                   const QUrl &icon,
                   int count,
                   int length,
                   QObject *parent) :
    SelectableItem(parent),
    m_id(id),
    m_name(name),
    m_icon(icon),
    m_count(count),
    m_length(length)
{
}

QHash<int, QByteArray> ArtistItem::roleNames() const
{
    QHash<int, QByteArray> names;
    names[IdRole] = "id";
    names[NameRole] = "name";
    names[IconRole] = "icon";
    names[CountRole] = "count";
    names[LengthRole] = "length";
    return names;
}

QVariant ArtistItem::data(int role) const
{
    switch(role) {
    case IdRole:
        return id();
    case NameRole:
        return name();
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
