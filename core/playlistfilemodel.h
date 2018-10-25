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
#include "itemmodel.h"

class PlaylistFileItem : public SelectableItem
{
    Q_OBJECT
public:
    enum Roles {
        TitleRole = Qt::DisplayRole,
        IdRole = Qt::UserRole,
        ListRole,
        CountRole,
        LengthRole,
        IconRole,
        PathRole
    };

public:
    PlaylistFileItem(QObject *parent = nullptr): SelectableItem(parent) {}
    explicit PlaylistFileItem(const QString &id,
                      const QString &title,
                      const QString &list,
                      const QString &path,
                      const QUrl &url,
                      int count,
                      int length,
                      QObject *parent = 0);
    QVariant data(int role) const;
    QHash<int, QByteArray> roleNames() const;
    inline QString id() const { return m_id; }
    inline QString title() const { return m_title; }
    inline QString list() const { return m_list; }
    inline QString path() const { return m_path; }
    inline QUrl icon() const { return m_icon; }
    inline int count() const { return m_count; }
    inline int length() const { return m_length; }

private:
    QString m_id;
    QString m_title;
    QString m_list;
    QString m_path;
    QUrl m_icon;
    int m_count;
    int m_length;
};

class PlaylistFileModel : public SelectableItemModel
{
    Q_OBJECT

public:
    explicit PlaylistFileModel(QObject *parent = nullptr);
    Q_INVOKABLE bool deleteFile(const QString& playlistId);

private:
    static const QString playlistsQueryTemplate;
    static const QString playlistsQueryTemplateEx;

    QString m_exId;

    void updateModelEx(const QString &exId);
    QList<ListItem*> makeItems();
    QList<ListItem*> processTrackerReply(
            const QStringList& varNames,
            const QByteArray& data);

    void disconnectAll();
};

#endif // PLAYLISTFILEMODEL_H
