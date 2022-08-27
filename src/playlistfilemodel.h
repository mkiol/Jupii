/* Copyright (C) 2018-2022 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef PLAYLISTFILEMODEL_H
#define PLAYLISTFILEMODEL_H

#include <QByteArray>
#include <QDebug>
#include <QHash>
#include <QList>
#include <QModelIndex>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariant>

#include "itemmodel.h"
#include "listmodel.h"

class PlaylistFileItem : public SelectableItem {
    Q_OBJECT
   public:
    enum Roles {
        TitleRole = Qt::DisplayRole,
        IdRole = Qt::UserRole,
        CountRole,
        LengthRole,
        IconRole,
        PathRole
    };

   public:
    PlaylistFileItem(QObject *parent = nullptr) : SelectableItem(parent) {}
    explicit PlaylistFileItem(const QString &id, const QString &title,
                              const QString &path, const QUrl &icon, int count,
                              int length, QObject *parent = nullptr);
    QVariant data(int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    inline QString id() const override { return m_id; }
    inline auto title() const { return m_title; }
    inline auto path() const { return m_path; }
    inline auto icon() const { return m_icon; }
    inline auto count() const { return m_count; }
    inline auto length() const { return m_length; }

   private:
    QString m_id;
    QString m_title;
    QString m_path;
    QUrl m_icon;
    int m_count = 0;
    int m_length = 0;
};

class PlaylistFileModel : public SelectableItemModel {
    Q_OBJECT

   public:
    explicit PlaylistFileModel(QObject *parent = nullptr);
    Q_INVOKABLE bool deleteFile(const QString &playlistId);

   private:
    static const QString playlistsQueryTemplate;
    static const QString playlistsQueryTemplateEx;

    QString m_exId;

    void updateModelEx(const QString &exId);
    QList<ListItem *> makeItems() override;
    static QList<ListItem *> processTrackerReply(const QStringList &varNames,
                                                 const QByteArray &data);

    void disconnectAll();
};

#endif  // PLAYLISTFILEMODEL_H
