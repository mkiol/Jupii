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
#include "itemmodel.h"

class AlbumItem : public SelectableItem
{
    Q_OBJECT

public:
    enum Roles {
        IdRole = Qt::UserRole,
        TitleRole = Qt::DisplayRole,
        ArtistRole,
        CountRole,
        LengthRole,
        IconRole
    };

public:
    AlbumItem(QObject *parent = nullptr): SelectableItem(parent) {}
    explicit AlbumItem(const QString &id,
                      const QString &title,
                      const QString &artist,
                      const QUrl &icon,
                      int count,
                      int length,
                      QObject *parent = nullptr);
    QVariant data(int role) const;
    QHash<int, QByteArray> roleNames() const;
    inline QString id() const { return m_id; }
    inline QString title() const { return m_title; }
    inline QString artist() const { return m_artist; }
    inline QUrl icon() const { return m_icon; }
    inline int count() const { return m_count; }
    inline int length() const { return m_length; }

private:
    QString m_id;
    QString m_title;
    QString m_artist;
    QUrl m_icon;
    int m_count = 0;
    int m_length = 0;
};

class AlbumModel : public SelectableItemModel
{
    Q_OBJECT
    // 0 - query by album title, 1 - query by artist
    Q_PROPERTY (int queryType READ getQueryType WRITE setQueryType NOTIFY queryTypeChanged)
public:
    explicit AlbumModel(QObject *parent = nullptr);

    int getQueryType();
    void setQueryType(int value);

signals:
    void queryTypeChanged();

private:
    struct AlbumData {
        QString id;
        QString title;
        QString artist;
        QUrl icon;
        int count = 0;
        int length = 0;
    };

    static const QString albumsQueryTemplate;
    int m_queryType = 0;
    QList<ListItem*> makeItems();
    QList<ListItem*> processTrackerReply(
            const QStringList& varNames,
            const QByteArray& data);
};

#endif // ALBUMMODEL_H
