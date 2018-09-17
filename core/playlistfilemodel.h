/* Copyright (C) 2018 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef PLAYLISTFILEMODEL_H
#define PLAYLISTFILEMODEL_H

#include <QString>
#include <QStringList>
#include <QHash>
#include <QDebug>
#include <QByteArray>
#include <QModelIndex>
#include <QVariant>
#include <QUrl>
#include <QList>

#include "listmodel.h"

class PlaylistFileItem : public ListItem
{
    Q_OBJECT
public:
    enum Roles {
        IdRole = Qt::UserRole,
        TitleRole = Qt::DisplayRole,
        ListRole,
        CountRole,
        LengthRole,
        ImageRole,
        PathRole
    };

public:
    PlaylistFileItem(QObject *parent = 0): ListItem(parent) {}
    explicit PlaylistFileItem(const QString &id,
                      const QString &title,
                      const QString &list,
                      const QString &path,
                      const QUrl &image,
                      int count,
                      int length,
                      QObject *parent = 0);
    QVariant data(int role) const;
    QHash<int, QByteArray> roleNames() const;
    inline QString id() const { return m_id; }
    inline QString title() const { return m_title; }
    inline QString list() const { return m_list; }
    inline QString path() const { return m_path; }
    inline QUrl image() const { return m_image; }
    inline int count() const { return m_count; }
    inline int length() const { return m_length; }

private:
    QString m_id;
    QString m_title;
    QString m_list;
    QString m_path;
    QUrl m_image;
    int m_count;
    int m_length;
};

class PlaylistFileModel : public ListModel
{
    Q_OBJECT
    Q_PROPERTY (int count READ getCount NOTIFY countChanged)
    Q_PROPERTY (QString filter READ getFilter WRITE setFilter NOTIFY filterChanged)
public:
    explicit PlaylistFileModel(QObject *parent = 0);
    ~PlaylistFileModel();

    void clear();

    int getCount();

    void setFilter(const QString& filter);
    QString getFilter();

    Q_INVOKABLE void querySongs(const QString& playlistId);
    Q_INVOKABLE bool deleteFile(const QString& playlistId);

signals:
    void songsQueryResult(const QList<QUrl>& songs);
    void countChanged();
    void filterChanged();

private slots:
    void processTrackerReply(const QStringList& varNames,
                             const QByteArray& data);
    void processTrackerError();

private:
    QString m_filter;
    QString m_playlistsQueryTemplate;
    QString m_playlistsQueryTemplateEx;
    QString m_tracksQueryTemplate;

    void updateModel(const QString& excludedId = "");
    void disconnectAll();
};

#endif // PLAYLISTFILEMODEL_H
