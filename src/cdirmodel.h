/* Copyright (C) 2020-2025 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef CDIRMODEL_H
#define CDIRMODEL_H

#include <QDateTime>
#include <QHash>
#include <QString>
#include <QUrl>
#include <QVariant>

#include "itemmodel.h"
#include "listmodel.h"

class CDirItem;

class CDirModel : public SelectableItemModel {
    Q_OBJECT
    Q_PROPERTY(QString currentId READ getCurrentId WRITE setCurrentId NOTIFY
                   currentDirChanged)
    Q_PROPERTY(
        QString currentTitle READ getCurrentTitle NOTIFY currentDirChanged)
    Q_PROPERTY(Types currentType READ getCurrentType NOTIFY currentDirChanged)
    Q_PROPERTY(int queryType READ getQueryType WRITE setQueryType NOTIFY
                   queryTypeChanged)
   public:
    enum Types {
        UnknownType,
        AudioType,
        VideoType,
        ImageType,
        DirType,
        MusicAlbumType,
        PhotoAlbumType,
        ArtistType,
        BackType
    };
    Q_ENUM(Types)

    explicit CDirModel(QObject *parent = nullptr);
    Q_INVOKABLE QVariantList selectedItems() override;
    void setCurrentId(const QString &id);
    QString getCurrentId() const;
    QString getCurrentTitle() const;
    Types getCurrentType() const;
    int getQueryType() const;
    void setQueryType(int value);
    Q_INVOKABLE void switchQueryType();

   signals:
    void currentDirChanged();
    void queryTypeChanged();

   private slots:
    void handleContentDirectoryInited();

   private:
    static const QHash<QString, Types> containerClasses;
    QString m_id{QStringLiteral("0")};
    QString m_pid{QStringLiteral("0")};
    QString m_title;
    Types m_type = Types::DirType;
    int m_queryType = 0;
    int m_musicQueryType = 0;
    int m_imageQueryType = 0;
    QList<ListItem *> makeItems() override;
    QHash<QString, QString> idToTitle;
    QHash<QString, QString> idToPid;
    QHash<QString, Types> idToType;
    QString findTitle(const QString &id);
    QString findPid(const QString &id);
    Types findType(const QString &id);
    static Types typeFromItemClass(const QString &upnpClass);
    static Types typeFromContainerClass(const QString &upnpClass);
};

class CDirItem : public SelectableItem {
    Q_OBJECT
   public:
    enum Roles {
        TitleRole = Qt::DisplayRole,
        IdRole = Qt::UserRole,
        PidRole,
        ArtistRole,
        AlbumRole,
        UrlRole,
        NumberRole,
        TypeRole,
        DurationRole,
        SelectedRole,
        SelectableRole,
        IconRole,
        DateRole
    };

    CDirItem(QObject *parent = nullptr) : SelectableItem(parent) {}
    explicit CDirItem(QString id, QString pid, QString title, QString artist,
                      QString album, QUrl url, QUrl icon, int number,
                      QDateTime date, int duration, CDirModel::Types type,
                      QObject *parent = nullptr);
    explicit CDirItem(QString id, QString pid, QString title,
                      CDirModel::Types type, QObject *parent = nullptr);
    QVariant data(int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    inline QString id() const override { return m_id; }
    inline QString pid() const { return m_pid; }
    inline QString title() const { return m_title; }
    inline QString artist() const { return m_artist; }
    inline QString album() const { return m_album; }
    inline QUrl url() const { return m_url; }
    inline QUrl icon() const { return m_icon; }
    inline int number() const { return m_number; }
    inline QDateTime date() const { return m_date; }
    inline int duration() const { return m_duration; }
    inline CDirModel::Types type() const { return m_type; }
    QUrl iconThumb() const;

   private:
    QString m_id;
    QString m_pid;
    QString m_title;
    QString m_artist;
    QString m_album;
    QUrl m_icon;
    QUrl m_url;
    int m_number = 0;
    QDateTime m_date;
    int m_duration = 0;
    CDirModel::Types m_type = CDirModel::Types::UnknownType;
};

#endif  // CDIRMODEL_H
