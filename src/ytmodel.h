/* Copyright (C) 2022 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef YTMODEL_H
#define YTMODEL_H

#include <QByteArray>
#include <QDebug>
#include <QHash>
#include <QString>
#include <QUrl>
#include <QVariant>
#include <QVariantList>
#include <utility>

#include "itemmodel.h"
#include "listmodel.h"

class YtModel : public SelectableItemModel {
    Q_OBJECT
    Q_PROPERTY(
        QString albumId READ getAlbumId WRITE setAlbumId NOTIFY albumIdChanged)
    Q_PROPERTY(QString albumTitle READ getAlbumTitle NOTIFY albumTitleChanged)
    Q_PROPERTY(YtModel::Type albumType READ getAlbumType WRITE setAlbumType
                   NOTIFY albumTypeChanged)
    Q_PROPERTY(QString artistId READ getArtistId WRITE setArtistId NOTIFY
                   artistIdChanged)
    Q_PROPERTY(QString artistName READ getArtistName NOTIFY artistNameChanged)
    Q_PROPERTY(QString homeId READ homeId CONSTANT)
   public:
    enum Type {
        Type_Unknown = 0,
        Type_Artist,
        Type_Album,
        Type_Playlist,
        Type_Video
    };
    Q_ENUM(Type)

    explicit YtModel(QObject *parent = nullptr);
    Q_INVOKABLE QVariantList selectedItems() override;

    inline auto getAlbumId() const { return mAlbumId; }
    void setAlbumId(const QString &albumId);
    inline auto getArtistId() const { return mArtistId; }
    void setAlbumType(Type albumType);
    inline auto getAlbumType() const { return mAlbumType; }
    void setArtistId(const QString &artistId);
    inline auto getAlbumTitle() const { return mAlbumTitle; }
    inline auto getArtistName() const { return mArtistName; }

   signals:
    void error();
    void albumIdChanged();
    void albumTitleChanged();
    void artistIdChanged();
    void artistNameChanged();
    void progressChanged(int n, int total);
    void albumTypeChanged();

   private:
    static const QString mHomeId;
    QString mAlbumId;
    QString mAlbumTitle;
    Type mAlbumType = Type::Type_Album;
    QString mArtistId;
    QString mArtistName;

    QList<ListItem *> makeItems() override;
    QList<ListItem *> makeSearchItems();
    QList<ListItem *> makeAlbumItems();
    QList<ListItem *> makeArtistItems();
    static QList<ListItem *> makeHomeItems();
    void setAlbumTitle(const QString &albumTitle);
    void setArtistName(const QString &artistName);
    static inline auto homeId() { return mHomeId; }
};

class YtItem : public SelectableItem {
    Q_OBJECT
   public:
    enum Roles {
        NameRole = Qt::DisplayRole,
        IdRole = Qt::UserRole,
        ArtistRole,
        AlbumRole,
        UrlRole,
        OrigUrlRole,
        IconRole,
        DurationRole,
        TypeRole,
        SectionRole,
        SelectedRole
    };

   public:
    YtItem(QObject *parent = nullptr) : SelectableItem(parent) {}
    explicit YtItem(QString id, QString name, QString artist, QString album,
                    QUrl url, QUrl origUrl, QUrl icon, QString section,
                    int duration, YtModel::Type type,
                    QObject *parent = nullptr);
    QVariant data(int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    inline QString id() const override { return m_id; }
    inline auto name() const { return m_name; }
    inline auto artist() const { return m_artist; }
    inline auto album() const { return m_album; }
    inline auto url() const { return m_url; }
    inline auto origUrl() const { return m_origUrl; }
    inline auto icon() const { return m_icon; }
    QUrl iconCached() const;
    inline auto section() const { return m_section; }
    inline auto duration() const { return m_duration; }
    inline auto type() const { return m_type; }
    QString durationStr() const;
    void refresh();

   private:
    QString m_id;
    QString m_name;
    QString m_artist;
    QString m_album;
    QUrl m_url;
    QUrl m_origUrl;
    QUrl m_icon;
    QString m_section;
    int m_duration = 0;
    YtModel::Type m_type = YtModel::Type::Type_Unknown;
};

#endif  // YTMODEL_H
