/* Copyright (C) 2018 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef ALBUMMODEL_H
#define ALBUMMODEL_H

#include <QString>
#include <QStringList>
#include <QHash>
#include <QDebug>
#include <QByteArray>
#include <QModelIndex>
#include <QVariant>
#include <QUrl>

#include "listmodel.h"

class AlbumItem : public ListItem
{
    Q_OBJECT
public:
    enum Roles {
        IdRole = Qt::UserRole,
        TitleRole = Qt::DisplayRole,
        ArtistRole,
        CountRole,
        LengthRole,
        ImageRole
    };

public:
    AlbumItem(QObject *parent = 0): ListItem(parent) {}
    explicit AlbumItem(const QString &id,
                      const QString &title,
                      const QString &artist,
                      const QUrl &image,
                      int count,
                      int length,
                      QObject *parent = 0);
    QVariant data(int role) const;
    QHash<int, QByteArray> roleNames() const;
    inline QString id() const { return m_id; }
    inline QString title() const { return m_title; }
    inline QString artist() const { return m_artist; }
    inline QUrl image() const { return m_image; }
    inline int count() const { return m_count; }
    inline int length() const { return m_length; }

private:
    QString m_id;
    QString m_title;
    QString m_artist;
    QUrl m_image;
    int m_count;
    int m_length;
};

class AlbumModel : public ListModel
{
    Q_OBJECT
    Q_PROPERTY (int count READ getCount NOTIFY countChanged)
    Q_PROPERTY (QString filter READ getFilter WRITE setFilter NOTIFY filterChanged)
public:
    explicit AlbumModel(QObject *parent = 0);
    ~AlbumModel();

    void clear();

    int getCount();

    void setFilter(const QString& filter);
    QString getFilter();

    Q_INVOKABLE void querySongs(const QString& albumId);

signals:
    void songsQueryResult(const QStringList& songs);
    void countChanged();
    void filterChanged();

private slots:
    void processTrackerReply(const QStringList& varNames,
                             const QByteArray& data);
    void processTrackerError();

private:
    struct AlbumData {
        QString id;
        QString title;
        QString artist;
        QUrl image;
        int count;
        int length;
    };

    QString m_filter;
    QString m_albumsQueryTemplate;
    QString m_tracksQueryTemplate;

    void updateModel();
};

#endif // ALBUMMODEL_H
