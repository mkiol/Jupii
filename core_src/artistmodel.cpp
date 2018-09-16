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

ArtistModel::ArtistModel(QObject *parent) :
    ListModel(new ArtistItem, parent)
{
    m_artistsQueryTemplate = "SELECT ?artist nmm:artistName(?artist) AS artist " \
                            "COUNT(?song) AS songs " \
                            "SUM(?length) AS totallength " \
                            "WHERE { " \
                            "FILTER regex(nmm:artistName(?artist), \"%1\", \"i\") " \
                            "?song a nmm:MusicPiece; " \
                            "nfo:duration ?length; " \
                            "nmm:performer ?artist . } " \
                            "GROUP BY ?artist " \
                            "ORDER BY nmm:artistName(?artist) " \
                            "LIMIT 500";

    m_tracksQueryTemplate = "SELECT nie:url(?song) " \
                            "WHERE { ?song nmm:performer \"%1\" } " \
                            "ORDER BY nmm:musicAlbum(?song) nmm:trackNumber(?song) " \
                            "LIMIT 500";

    updateModel();
}

ArtistModel::~ArtistModel()
{
}

int ArtistModel::getCount()
{
    return m_list.length();
}

void ArtistModel::updateModel()
{
    const QString query = m_artistsQueryTemplate.arg(m_filter);

    auto tracker = Tracker::instance();
    QObject::connect(tracker, &Tracker::queryFinished,
                     this, &ArtistModel::processTrackerReply);
    QObject::connect(tracker, &Tracker::queryError,
                     this, &ArtistModel::processTrackerError);

    tracker->queryAsync(query);
}

void ArtistModel::querySongs(const QString &artistId)
{
    const QString query = m_tracksQueryTemplate.arg(artistId);

    auto tracker = Tracker::instance();
    QObject::connect(tracker, &Tracker::queryFinished,
                     this, &ArtistModel::processTrackerReply);
    QObject::connect(tracker, &Tracker::queryError,
                     this, &ArtistModel::processTrackerError);

    tracker->queryAsync(query);
}

void ArtistModel::processTrackerError()
{
    auto tracker = Tracker::instance();
    QObject::disconnect(tracker, 0, 0, 0);

    clear();
}

void ArtistModel::processTrackerReply(const QStringList& varNames,
                                     const QByteArray& data)
{
    auto tracker = Tracker::instance();
    QObject::disconnect(tracker, 0, 0, 0);

    TrackerCursor cursor(varNames, data);

    int n = cursor.columnCount();

    if (n > 3) {
        // Result of Artist query

        clear();

        QStringList artists;

        while(cursor.next()) {
            QString id = cursor.value(0).toString();

            if (artists.contains(id)) {
                qDebug() << "Duplicate artist, skiping";
                continue;
            }

            appendRow(new ArtistItem(id,
                                    cursor.value(1).toString(),
                                    QUrl(), // image
                                    cursor.value(2).toInt(),
                                    cursor.value(3).toInt()));

            artists << id;
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

void ArtistModel::clear()
{
    if (rowCount() == 0)
        return;

    removeRows(0,rowCount());

    emit countChanged();
}

void ArtistModel::setFilter(const QString &filter)
{
    if (m_filter != filter) {
        m_filter = filter;
        emit filterChanged();

        updateModel();
    }
}

QString ArtistModel::getFilter()
{
    return m_filter;
}

ArtistItem::ArtistItem(const QString &id,
                   const QString &name,
                   const QUrl &image,
                   int count,
                   int length,
                   QObject *parent) :
    ListItem(parent),
    m_id(id),
    m_name(name),
    m_image(image),
    m_count(count),
    m_length(length)
{
}

QHash<int, QByteArray> ArtistItem::roleNames() const
{
    QHash<int, QByteArray> names;
    names[IdRole] = "id";
    names[NameRole] = "name";
    names[ImageRole] = "image";
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
