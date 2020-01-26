/* Copyright (C) 2020 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef CDIRMODEL_H
#define CDIRMODEL_H

#include <QVariant>
#include <QHash>
#include <QString>
#include <QUrl>
#include <QDateTime>
#include "listmodel.h"
#include "itemmodel.h"

class CDirItem;

class CDirModel : public SelectableItemModel
{
    Q_OBJECT
    Q_PROPERTY (QString currentId READ getCurrentId WRITE setCurrentId NOTIFY currentDirChanged)
    Q_PROPERTY (QString currentTitle READ getCurrentTitle NOTIFY currentDirChanged)
    Q_PROPERTY (Types currentType READ getCurrentType NOTIFY currentDirChanged)
    Q_PROPERTY (int queryType READ getQueryType WRITE setQueryType NOTIFY queryTypeChanged)
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
    Q_INVOKABLE QVariantList selectedItems();
    void setCurrentId(const QString& id);
    QString getCurrentId();
    QString getCurrentTitle();
    Types getCurrentType();
    int getQueryType();
    void setQueryType(int value);
    Q_INVOKABLE void switchQueryType();

signals:
    void currentDirChanged();
    void queryTypeChanged();

private slots:
    void handleContentDirectoryInited();

private:
    static const QHash<QString,Types> containerClasses;
    QString m_id;
    QString m_pid;
    QString m_title;
    Types m_type;
    int m_queryType = 0;
    int m_musicQueryType = 0;
    int m_imageQueryType = 0;
    QList<ListItem*> makeItems();
    QHash<QString, QString> idToTitle;
    QHash<QString, QString> idToPid;
    QHash<QString, Types> idToType;
    QString findTitle(const QString &id);
    QString findPid(const QString &id);
    Types findType(const QString &id);
    Types typeFromItemClass(const QString &upnpClass);
    Types typeFromContainerClass(const QString &upnpClass);
};

class CDirItem: public SelectableItem
{
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
        SelectedRole,
        SelectableRole,
        IconRole,
        DateRole
    };

    CDirItem(QObject* parent = nullptr) : SelectableItem(parent) {}
    explicit CDirItem(const QString &id,
                      const QString &pid,
                      const QString &title,
                      const QString &artist,
                      const QString &album,
                      const QUrl &url,
                      const QUrl &icon,
                      int number,
                      QDateTime date,
                      CDirModel::Types type,
                      QObject *parent = nullptr);
    explicit CDirItem(const QString &id,
                      const QString &pid,
                      const QString &title,
                      CDirModel::Types type,
                      QObject *parent = nullptr);
    QVariant data(int role) const;
    QHash<int, QByteArray> roleNames() const;
    inline QString id() const { return m_id; }
    inline QString pid() const { return m_pid; }
    inline QString title() const { return m_title; }
    inline QString artist() const { return m_artist; }
    inline QString album() const { return m_album; }
    inline QUrl url() const { return m_url; }
    inline QUrl icon() const { return m_icon; }
    inline int number() const { return m_number; }
    inline QDateTime date() const { return m_date; }
    inline CDirModel::Types type() const { return m_type; }

private:
    QString m_id;
    QString m_pid;
    QString m_title;
    QString m_artist;
    QString m_album;
    QUrl m_icon;
    QUrl m_url;
    int m_number;
    QDateTime m_date;
    CDirModel::Types m_type;
};

#endif // CDIRMODEL_H
