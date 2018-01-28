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
#include "albummodel.h"
#include "trackercursor.h"

AlbumModel::AlbumModel(QObject *parent) :
    ListModel(new AlbumItem, parent)
{
    m_albumsQueryTemplate = "SELECT ?album nie:title(?album) AS title " \
                            "nmm:artistName(?artist) AS artist " \
                            "COUNT(?song) AS songs " \
                            "SUM(?length) AS totallength " \
                            "WHERE { ?album a nmm:MusicAlbum . " \
                            "FILTER regex(nie:title(?album), \"%1\", \"i\") " \
                            "?song nmm:musicAlbum ?album ; " \
                            "nfo:duration ?length ; " \
                            "nmm:performer ?artist . " \
                            "} GROUP BY ?album ?artist " \
                            "ORDER BY ?album " \
                            "LIMIT 500";

    m_tracksQueryTemplate = "SELECT nie:url(?song) " \
                            "WHERE { ?song nmm:musicAlbum \"%1\" } " \
                            "ORDER BY nmm:trackNumber(?song) " \
                            "LIMIT 500";

    updateModel();
}

AlbumModel::~AlbumModel()
{
}

int AlbumModel::getCount()
{
    return m_list.length();
}

void AlbumModel::updateModel()
{
    const QString query = m_albumsQueryTemplate.arg(m_filter);

    auto tracker = Tracker::instance();
    QObject::connect(tracker, &Tracker::queryFinished,
                     this, &AlbumModel::processTrackerReply);
    QObject::connect(tracker, &Tracker::queryError,
                     this, &AlbumModel::processTrackerError);

    tracker->queryAsync(query);
}

void AlbumModel::querySongs(const QString &albumId)
{
    const QString query = m_tracksQueryTemplate.arg(albumId);

    auto tracker = Tracker::instance();
    QObject::connect(tracker, &Tracker::queryFinished,
                     this, &AlbumModel::processTrackerReply);
    QObject::connect(tracker, &Tracker::queryError,
                     this, &AlbumModel::processTrackerError);

    tracker->queryAsync(query);
}

void AlbumModel::processTrackerError()
{
    auto tracker = Tracker::instance();
    QObject::disconnect(tracker, 0, 0, 0);

    clear();
}

void AlbumModel::processTrackerReply(const QStringList& varNames,
                                     const QByteArray& data)
{
    auto tracker = Tracker::instance();
    QObject::disconnect(tracker, 0, 0, 0);

    TrackerCursor cursor(varNames, data);

    int n = cursor.columnCount();

    if (n > 4) {
        // Result of Album query

        clear();

        QStringList albums;

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

            QString id = cursor.value(0).toString();

            if (albums.contains(id)) {
                qDebug() << "Duplicate album, skiping";
                continue;
            }

            auto tracker = Tracker::instance();
            QString imgFilePath = tracker->genAlbumArtFile(cursor.value(1).toString(),
                                                            cursor.value(2).toString());

            QFileInfo imgFile(imgFilePath);

            appendRow(new AlbumItem(id,
                                    cursor.value(1).toString(),
                                    cursor.value(2).toString(),
                                    imgFile.exists() ?
                                        QUrl(imgFilePath) :
                                        QUrl("image://theme/graphic-grid-playlist"),
                                    cursor.value(4).toInt(),
                                    cursor.value(5).toInt()));

            albums << id;
        }

        if (m_list.length() > 0)
            emit countChanged();

        return;
    }

    if (n > 0) {
        // Result of Song query

        QStringList songs;

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

            QUrl file(cursor.value(0).toString());
            songs << file.toLocalFile();
        }

        emit songsQueryResult(songs);

        return;
    }
}

void AlbumModel::clear()
{
    if (rowCount() == 0)
        return;

    removeRows(0,rowCount());

    emit countChanged();
}

void AlbumModel::setFilter(const QString &filter)
{
    if (m_filter != filter) {
        m_filter = filter;
        emit filterChanged();

        updateModel();
    }
}

QString AlbumModel::getFilter()
{
    return m_filter;
}

AlbumItem::AlbumItem(const QString &id,
                   const QString &title,
                   const QString &artist,
                   const QUrl &image,
                   int count,
                   int length,
                   QObject *parent) :
    ListItem(parent),
    m_id(id),
    m_title(title),
    m_artist(artist),
    m_image(image),
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
    names[ImageRole] = "image";
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
