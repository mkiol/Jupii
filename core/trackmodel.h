/* Copyright (C) 2018-2020 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef TRACKMODEL_H
#define TRACKMODEL_H

#include <QString>
#include <QStringList>
#include <QHash>
#include <QDebug>
#include <QByteArray>
#include <QModelIndex>
#include <QVariant>
#include <QUrl>

#include "contentserver.h"
#include "listmodel.h"
#include "itemmodel.h"
#include "trackercursor.h"

class TrackItem : public SelectableItem
{
    Q_OBJECT
public:
    enum Roles {
        IdRole = Qt::UserRole,
        TitleRole = Qt::DisplayRole,
        ArtistRole,
        AlbumRole,
        UrlRole,
        IconRole,
        LengthRole,
        NumberRole,
        TypeRole,
        ItemTypeRole,
        SelectedRole
    };

public:
    TrackItem(QObject *parent = nullptr): SelectableItem(parent) {}
    explicit TrackItem(const QString &id,
                      const QString &title,
                      const QString &artist,
                      const QString &album,
                      const QUrl &url,
                      const QUrl &icon,
                      ContentServer::Type type,
                      int number,
                      int length,
                      ContentServer::ItemType itemType = ContentServer::ItemType_LocalFile,
                      QObject *parent = nullptr);
    QVariant data(int role) const;
    QHash<int, QByteArray> roleNames() const;
    inline QString id() const { return m_id; }
    inline QString title() const { return m_title; }
    inline QString artist() const { return m_artist; }
    inline QString album() const { return m_album; }
    inline QUrl url() const { return m_url; }
    inline QUrl icon() const { return m_icon; }
    inline ContentServer::Type type() const { return m_type; }
    inline int number() const { return m_number; }
    inline int length() const { return m_length; }
    inline ContentServer::ItemType itemType() const { return m_item_type; }

private:
    QString m_id;
    QString m_title;
    QString m_artist;
    QString m_album;
    QUrl m_url;
    QUrl m_icon;
    ContentServer::Type m_type = ContentServer::TypeUnknown;
    int m_number = 0;
    int m_length = 0;
    ContentServer::ItemType m_item_type = ContentServer::ItemType_Unknown;
};

class TrackModel : public SelectableItemModel
{
    Q_OBJECT

    Q_PROPERTY (QString albumId READ getAlbumId WRITE setAlbumId NOTIFY albumIdChanged)
    Q_PROPERTY (QString artistId READ getArtistId WRITE setArtistId NOTIFY artistIdChanged)
    Q_PROPERTY (QString playlistId READ getPlaylistId WRITE setPlaylistId NOTIFY playlistIdChanged)

public:
    explicit TrackModel(QObject *parent = nullptr);
    void setAlbumId(const QString& id);
    QString getAlbumId();
    void setArtistId(const QString& id);
    QString getArtistId();
    void setPlaylistId(const QString& id);
    QString getPlaylistId();
    Q_INVOKABLE QVariantList selectedItems() override;

signals:
    void albumIdChanged();
    void artistIdChanged();
    void playlistIdChanged();

private:
    enum TrackerTasks {
        TaskUnknown,
        TaskAlbum,
        TaskArtist,
        TaskPlaylist,
        TaskEntries,
        TaskUrls
    };

    struct TrackData {
        QString title;
        QString author;
        QUrl icon;
        ContentServer::Type type = ContentServer::TypeUnknown;
    };

    static const QString queryByAlbumTemplate;
    static const QString queryByArtistTemplate;
    static const QString queryByPlaylistTemplate;
    static const QString queryByEntriesTemplate;
    static const QString queryByUrlsTemplate;

    QString m_albumId;
    QString m_artistId;
    QString m_playlistId;
    QHash<QUrl,TrackData> m_trackdata_by_id;
    QList<QUrl> m_ids;

    QList<ListItem*> makeItems() override;
    TrackData makeTrackDataFromId(const QUrl& id) const;
    QList<ListItem*> makeTrackItemsFromTrackData();
    QList<ListItem*> processTrackerReplyForAlbumArtist(TrackerCursor& cursor);
    QList<ListItem*> processTrackerReplyForPlaylist(TrackerCursor& cursor);
    QList<ListItem*> processTrackerReplyForEntries(TrackerCursor& cursor);
    QList<ListItem*> processTrackerReplyForUrls(TrackerCursor& cursor);
    QList<ListItem*> processTrackerReply(
            TrackerTasks task,
            const QStringList& varNames,
            const QByteArray& data);
};

#endif // TRACKMODEL_H
