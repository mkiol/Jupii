/* Copyright (C) 2021 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef SOUNDCLOUDMODEL_H
#define SOUNDCLOUDMODEL_H

#include <QObject>
#include <QString>
#include <QHash>
#include <QDebug>
#include <QByteArray>
#include <QVariant>
#include <QVariantList>
#include <QUrl>
#include <optional>

#include "listmodel.h"
#include "itemmodel.h"
#include "soundcloudapi.h"

class SoundcloudModel : public SelectableItemModel
{
    Q_OBJECT
    Q_PROPERTY (QUrl albumUrl READ getAlbumUrl WRITE setAlbumUrl NOTIFY albumUrlChanged)
    Q_PROPERTY (QString albumTitle READ getAlbumTitle NOTIFY albumTitleChanged)
    Q_PROPERTY (QUrl artistUrl READ getArtistUrl WRITE setArtistUrl NOTIFY artistUrlChanged)
    Q_PROPERTY (QString artistName READ getArtistName NOTIFY artistNameChanged)
public:
    enum Type {
        Type_Unknown = 0,
        Type_Artist,
        Type_Album,
        Type_Track
    };
    Q_ENUM(Type)

    explicit SoundcloudModel(QObject *parent = nullptr);
    ~SoundcloudModel();
    Q_INVOKABLE QVariantList selectedItems();

    QUrl getAlbumUrl() const;
    void setAlbumUrl(const QUrl& albumUrl);

    QUrl getArtistUrl() const;
    void setArtistUrl(const QUrl& artistUrl);

    QString getAlbumTitle() const;
    QString getArtistName() const;

signals:
    void error();
    void albumUrlChanged();
    void albumTitleChanged();
    void artistUrlChanged();
    void artistNameChanged();

private:
    QList<ListItem*> makeItems();
    QList<ListItem*> makeSearchItems();
    QList<ListItem*> makeAlbumItems();
    QList<ListItem*> makeArtistItems();
    void setAlbumTitle(const QString& albumTitle);
    void setArtistName(const QString& artistName);
    QUrl albumUrl;
    QString albumTitle;
    QUrl artistUrl;
    QString artistName;
    std::optional<SoundcloudApi::User> lastArtist;
};

class SoundcloudItem : public SelectableItem
{
    Q_OBJECT
public:
    enum Roles {
        NameRole = Qt::DisplayRole,
        IdRole = Qt::UserRole,
        ArtistRole,
        AlbumRole,
        UrlRole,
        IconRole,
        TypeRole,
        SelectedRole
    };

public:
    SoundcloudItem(QObject *parent = nullptr): SelectableItem(parent) {}
    explicit SoundcloudItem(const QString &id,
                      const QString &name,
                      const QString &artist,
                      const QString &album,
                      const QUrl &url,
                      const QUrl &icon,
                      SoundcloudModel::Type type,
                      QObject *parent = nullptr);
    QVariant data(int role) const;
    QHash<int, QByteArray> roleNames() const;
    inline QString id() const { return m_id; }
    inline QString name() const { return m_name; }
    inline QString artist() const { return m_artist; }
    inline QString album() const { return m_album; }
    inline QUrl url() const { return m_url; }
    inline QUrl icon() const { return m_icon; }
    inline SoundcloudModel::Type type() const { return m_type; }
    void refresh();
private:
    QString m_id;
    QString m_name;
    QString m_artist;
    QString m_album;
    QUrl m_url;
    QUrl m_icon;
    SoundcloudModel::Type m_type;
};

#endif // SOUNDCLOUDMODEL_H
