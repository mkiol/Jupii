/* Copyright (C) 2018 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef GPODDERMODEL_H
#define GPODDERMODEL_H

#include <QHash>
#include <QByteArray>
#include <QVariant>
#include <QUrl>
#include <QDir>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QList>

#ifdef DESKTOP
#include <QIcon>
#endif

#include "contentserver.h"
#include "listmodel.h"
#include "itemmodel.h"

class Gpodder
{
public:
    static QSqlDatabase db();
    static QDir dataDir();
    static bool enabled();

private:
    static QString conName;
    static QSqlDatabase m_db;
};

class GpodderEpisodeItem : public SelectableItem
{
    Q_OBJECT
public:
    enum Roles {
        TitleRole = Qt::DisplayRole,
        IconRole = Qt::DecorationRole,
        IdRole = Qt::UserRole,
        DescriptionRole,
        UrlRole,
        DurationRole,
        PositionRole,
        SelectedRole,
        TypeRole,
        PublishedRole,
        PodcastTitleRole
    };

public:
    GpodderEpisodeItem(QObject *parent = nullptr): SelectableItem(parent) {}
    explicit GpodderEpisodeItem(const QString &id,
                      const QString &title,
                      const QString &description,
                      const QString &podcastTitle,
                      const QUrl &url,
                      int duration,
                      int position,
                      ContentServer::Type type,
                      uint published,
#ifdef SAILFISH
                      const QUrl &icon,
#else
                      const QIcon &icon,
#endif
                      QObject *parent = nullptr);
    QVariant data(int role) const;
    QHash<int, QByteArray> roleNames() const;
    inline QString id() const { return m_id; }
    inline QString title() const { return m_title; }
    inline QString description() const { return m_description; }
    inline QString podcastTitle() const { return m_ptitle; }
    inline QUrl url() const { return m_url; }
    inline int duration() const { return m_duration; }
    inline int position() const { return m_position; }
    inline ContentServer::Type type() const { return m_type; }
    inline uint published() const { return m_published; }
#ifdef SAILFISH
    inline QUrl icon() const { return m_icon; }
#else
    inline QIcon icon() const { return m_icon; }
#endif
private:
    QString m_id;
    QString m_title;
    QString m_description;
    QString m_ptitle;
    QUrl m_url;
    int m_duration;
    int m_position;
    ContentServer::Type m_type = ContentServer::TypeUnknown;
    uint m_published;
#ifdef SAILFISH
    QUrl m_icon;
#else
    QIcon m_icon;
#endif
};

class GpodderEpisodeModel : public SelectableItemModel
{
    Q_OBJECT

public:
    explicit GpodderEpisodeModel(QObject *parent = nullptr);
    Q_INVOKABLE QVariantList selectedItems();

private:
    QDir m_dir;
#ifdef SAILFISH
    QUrl m_icon;
#else
    QIcon m_icon;
#endif

private:
    QList<ListItem*> makeItems();
};

#endif // GPODDERMODEL_H
