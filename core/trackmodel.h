/* Copyright (C) 2018 Michal Kosciesza <michal@mkiol.net>
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

#include "listmodel.h"

class TrackItem : public ListItem
{
    Q_OBJECT
public:
    enum Roles {
        IdRole = Qt::UserRole,
        TitleRole = Qt::DisplayRole,
        ArtistRole,
        AlbumRole,
        PathRole,
        ImageRole,
        LengthRole,
        NumberRole,
        SelectedRole
    };

public:
    TrackItem(QObject *parent = 0): ListItem(parent) {}
    explicit TrackItem(const QString &id,
                      const QString &title,
                      const QString &artist,
                      const QString &album,
                      const QString &path,
                      const QUrl &image,
                      int number,
                      int length,
                      QObject *parent = 0);
    QVariant data(int role) const;
    QHash<int, QByteArray> roleNames() const;
    inline QString id() const { return m_id; }
    inline QString title() const { return m_title; }
    inline QString artist() const { return m_artist; }
    inline QString album() const { return m_album; }
    inline QString path() const { return m_path; }
    inline QUrl image() const { return m_image; }
    inline int number() const { return m_number; }
    inline int length() const { return m_length; }
    inline bool selected() const { return m_selected; }
    void setSelected(bool value);

private:
    QString m_id;
    QString m_title;
    QString m_artist;
    QString m_album;
    QString m_path;
    QUrl m_image;
    int m_number;
    int m_length;
    bool m_selected;
};

class TrackModel : public ListModel
{
    Q_OBJECT
    Q_PROPERTY (int count READ getCount NOTIFY countChanged)
    Q_PROPERTY (QString filter READ getFilter WRITE setFilter NOTIFY filterChanged)
    Q_PROPERTY (QString albumId READ getAlbumId WRITE setAlbumId NOTIFY albumIdChanged)
    Q_PROPERTY (QString artistId READ getArtistId WRITE setArtistId NOTIFY artistIdChanged)
    Q_PROPERTY (int selectedCount READ selectedCount NOTIFY selectedCountChanged)
public:
    explicit TrackModel(QObject *parent = 0);
    ~TrackModel();

    void clear();

    int getCount();

    void setFilter(const QString& filter);
    QString getFilter();

    void setAlbumId(const QString& id);
    QString getAlbumId();

    void setArtistId(const QString& id);
    QString getArtistId();

    int selectedCount();

    Q_INVOKABLE void setSelected(int index, bool value);
    Q_INVOKABLE void setAllSelected(bool value);
    Q_INVOKABLE QStringList selectedPaths();

signals:
    void countChanged();
    void filterChanged();
    void albumIdChanged();
    void artistIdChanged();
    void selectedCountChanged();

private slots:
    void processTrackerReply(const QStringList& varNames,
                             const QByteArray& data);
    void processTrackerError();

private:
    QString m_filter;
    QString m_albumId;
    QString m_artistId;
    QString m_queryByAlbumTemplate;
    QString m_queryByArtistTemplate;
    int m_selectedCount = 0;

    void updateModel();
};

#endif // TRACKMODEL_H
