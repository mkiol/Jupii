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

TrackModel::TrackModel(QObject *parent) :
    ListModel(new TrackItem, parent)
{
    m_queryByAlbumTemplate =  "SELECT ?song " \
                              "nie:title(?song) AS title " \
                              "nmm:artistName(nmm:performer(?song)) AS artist " \
                              "nie:title(nmm:musicAlbum(?song)) AS album "
                              "nie:url(?song) AS url " \
                              "nmm:trackNumber(?song) AS number " \
                              "nfo:duration(?song) AS length " \
                              "WHERE { " \
                              "?song a nmm:MusicPiece; " \
                              "nmm:musicAlbum \"%1\" . " \
                              "FILTER regex(nie:title(?song), \"%2\", \"i\") " \
                              "} " \
                              "ORDER BY nmm:trackNumber(?song) " \
                              "LIMIT 500";

    m_queryByArtistTemplate = "SELECT ?song " \
                              "nie:title(?song) AS title " \
                              "nmm:artistName(nmm:performer(?song)) AS artist " \
                              "nie:title(nmm:musicAlbum(?song)) AS album "
                              "nie:url(?song) AS url " \
                              "nmm:trackNumber(?song) AS number " \
                              "nfo:duration(?song) AS length " \
                              "WHERE { " \
                              "?song a nmm:MusicPiece; " \
                              "nmm:performer \"%1\" . " \
                              "FILTER regex(nie:title(?song), \"%2\", \"i\") " \
                              "} " \
                              "ORDER BY nie:title(nmm:musicAlbum(?song)) nmm:trackNumber(?song) " \
                              "LIMIT 500";
}

TrackModel::~TrackModel()
{
}

int TrackModel::getCount()
{
    return m_list.length();
}

void TrackModel::updateModel()
{
    QString query;

    if (!m_albumId.isEmpty()) {
        query = m_queryByAlbumTemplate.arg(m_albumId, m_filter);
    } else if (!m_artistId.isEmpty()) {
        query = m_queryByArtistTemplate.arg(m_artistId, m_filter);
    } else {
        qWarning() << "AlbumId neither ArtistId defined";
        return;
    }

    auto tracker = Tracker::instance();
    QObject::connect(tracker, &Tracker::queryFinished,
                     this, &TrackModel::processTrackerReply);
    QObject::connect(tracker, &Tracker::queryError,
                     this, &TrackModel::processTrackerError);
    tracker->queryAsync(query);
}

void TrackModel::processTrackerError()
{
    auto tracker = Tracker::instance();
    QObject::disconnect(tracker, 0, 0, 0);

    clear();
}

/*int TrackModel::selectedCount()
{
    int l = m_list.length();

    if (l == 0)
        return 0;

    int count = 0;

    for (auto item : m_list) {
        auto track = static_cast<TrackItem*>(item);
        if (track->selected())
            ++count;
    }

    return count;
}*/

QStringList TrackModel::selectedPaths()
{
    QStringList list;

    for (auto item : m_list) {
        auto track = static_cast<TrackItem*>(item);
        if (track->selected())
            list << track->path();
    }

    return list;
}

int TrackModel::selectedCount()
{
    return m_selectedCount;
}

void TrackModel::processTrackerReply(const QStringList& varNames,
                                     const QByteArray& data)
{
    auto tracker = Tracker::instance();
    QObject::disconnect(tracker, 0, 0, 0);

    TrackerCursor cursor(varNames, data);

    int n = cursor.columnCount();

    if (n > 6) {

        clear();

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

            appendRow(new TrackItem(cursor.value(0).toString(),
                                    cursor.value(1).toString(),
                                    cursor.value(2).toString(),
                                    cursor.value(3).toString(),
                                    QUrl(cursor.value(4).toString()).toLocalFile(),
                                    QUrl("image://theme/icon-m-file-audio"),
                                    cursor.value(5).toInt(),
                                    cursor.value(6).toInt()));
        }

        if (m_list.length() > 0)
            emit countChanged();
    }
}

void TrackModel::clear()
{
    if (rowCount() == 0)
        return;

    removeRows(0,rowCount());

    m_selectedCount = 0;
    emit selectedCountChanged();
    emit countChanged();
}

void TrackModel::setFilter(const QString &filter)
{
    if (m_filter != filter) {
        m_filter = filter;
        emit filterChanged();

        updateModel();
    }
}

QString TrackModel::getFilter()
{
    return m_filter;
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

void TrackModel::setSelected(int index, bool value)
{
    int l = m_list.length();

    if (index >= l) {
        qWarning() << "Index is invalid";
        return;
    }

    auto track = static_cast<TrackItem*>(m_list.at(index));

    bool cvalue = track->selected();

    if (cvalue != value) {
        track->setSelected(value);

        if (value)
            m_selectedCount++;
        else
            m_selectedCount--;

        emit selectedCountChanged();
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

TrackItem::TrackItem(const QString &id,
                   const QString &title,
                   const QString &artist,
                   const QString &album,
                   const QString &path,
                   const QUrl &image,
                   int number,
                   int length,
                   QObject *parent) :
    ListItem(parent),
    m_id(id),
    m_title(title),
    m_artist(artist),
    m_album(album),
    m_path(path),
    m_image(image),
    m_number(number),
    m_length(length),
    m_selected(false)
{
}

QHash<int, QByteArray> TrackItem::roleNames() const
{
    QHash<int, QByteArray> names;
    names[IdRole] = "id";
    names[TitleRole] = "title";
    names[ArtistRole] = "artist";
    names[AlbumRole] = "album";
    names[PathRole] = "path";
    names[ImageRole] = "image";
    names[NumberRole] = "number";
    names[LengthRole] = "length";
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
    case PathRole:
        return path();
    case ImageRole:
        return image();
    case NumberRole:
        return number();
    case LengthRole:
        return length();
    case SelectedRole:
        return selected();
    default:
        return QVariant();
    }
}

void TrackItem::setSelected(bool value)
{
    if (m_selected != value) {
        m_selected = value;
        emit dataChanged();
    }
}

