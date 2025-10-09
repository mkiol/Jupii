/* Copyright (C) 2020-2022 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef BCMODEL_H
#define BCMODEL_H

#include <QByteArray>
#include <QDebug>
#include <QHash>
#include <QString>
#include <QUrl>
#include <QVariant>
#include <QVariantList>

#include "itemmodel.h"
#include "listmodel.h"

class BcModel : public SelectableItemModel {
    Q_OBJECT
    Q_PROPERTY(
        QUrl albumUrl READ getAlbumUrl WRITE setAlbumUrl NOTIFY albumUrlChanged)
    Q_PROPERTY(QString albumTitle READ getAlbumTitle NOTIFY albumTitleChanged)
    Q_PROPERTY(QUrl artistUrl READ getArtistUrl WRITE setArtistUrl NOTIFY
                   artistUrlChanged)
    Q_PROPERTY(QString artistName READ getArtistName NOTIFY artistNameChanged)
    Q_PROPERTY(bool canShowMore READ canShowMore NOTIFY canShowMoreChanged)
    Q_PROPERTY(QUrl notableUrl READ notableUrl CONSTANT)
   public:
    enum Type {
        Type_Unknown = 0,
        Type_Artist,
        Type_Album,
        Type_Track,
        Type_Label,
        Type_Fan,
    };
    Q_ENUM(Type)

    explicit BcModel(QObject *parent = nullptr);
    Q_INVOKABLE QVariantList selectedItems() override;
    Q_INVOKABLE inline void requestMoreItems() {
        mLastIndex = getCount();
        mShowMoreRequested = true;
    }
    Q_INVOKABLE inline int lastIndex() const { return mLastIndex; }

    QUrl getAlbumUrl() const;
    void setAlbumUrl(const QUrl &albumUrl);
    QUrl getArtistUrl() const;
    void setArtistUrl(const QUrl &artistUrl);
    QString getAlbumTitle() const;
    QString getArtistName() const;

   signals:
    void error();
    void albumUrlChanged();
    void albumTitleChanged();
    void artistUrlChanged();
    void artistNameChanged();
    void progressChanged(int n, int total);
    void canShowMoreChanged();

   private:
    static const QUrl mNotableUrl;
    QUrl mAlbumUrl;
    QString mAlbumTitle;
    QUrl mArtistUrl;
    QString mArtistName;
    bool mCanShowMore = false;
    bool mShowMoreRequested = false;
    int mLastIndex = 0;

    QList<ListItem *> makeItems() override;
    QList<ListItem *> makeSearchItems();
    QList<ListItem *> makeAlbumItems();
    QList<ListItem *> makeArtistItems();
    QList<ListItem *> makeNotableItems(bool more);
    void setAlbumTitle(const QString &albumTitle);
    void setArtistName(const QString &artistName);
    inline auto canShowMore() const { return mCanShowMore; }
    static inline auto notableUrl() { return mNotableUrl; }
};

class BcItem : public SelectableItem {
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
        TypeRole,
        GenreRole,
        SelectedRole
    };

   public:
    BcItem(QObject *parent = nullptr) : SelectableItem(parent) {}
    explicit BcItem(const QString &id, const QString &name,
                    const QString &artist, const QString &album,
                    const QUrl &url, const QUrl &origUrl, const QUrl &icon,
                    int duration, BcModel::Type type, const QString &genre = {},
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
    inline auto iconThumb() const {
        return QUrl{ICON_PROVIDER_PREFIX + m_icon.toString()};
    }
    inline auto duration() const { return m_duration; }
    inline auto type() const { return m_type; }
    inline auto genre() const { return m_genre; }
    void refresh();

   private:
    QString m_id;
    QString m_name;
    QString m_artist;
    QString m_album;
    QUrl m_url;
    QUrl m_origUrl;
    QUrl m_icon;
    int m_duration = 0;
    BcModel::Type m_type = BcModel::Type::Type_Unknown;
    QString m_genre;
};

#endif  // BCMODEL_H
