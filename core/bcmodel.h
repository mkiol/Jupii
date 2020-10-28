/* Copyright (C) 2020 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef BCMODEL_H
#define BCMODEL_H

#include <QString>
#include <QHash>
#include <QDebug>
#include <QByteArray>
#include <QVariant>
#include <QVariantList>
#include <QUrl>

#include "listmodel.h"
#include "itemmodel.h"

class BcItem : public SelectableItem
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
        SelectedRole
    };

public:
    BcItem(QObject *parent = nullptr): SelectableItem(parent) {}
    explicit BcItem(const QString &id,
                      const QString &name,
                      const QString &artist,
                      const QString &album,
                      const QUrl &url,
                      const QUrl &icon,
                      QObject *parent = nullptr);
    QVariant data(int role) const;
    QHash<int, QByteArray> roleNames() const;
    inline QString id() const { return m_id; }
    inline QString name() const { return m_name; }
    inline QString artist() const { return m_artist; }
    inline QString album() const { return m_album; }
    inline QUrl url() const { return m_url; }
    inline QUrl icon() const { return m_icon; }
    void refresh();
private:
    QString m_id;
    QString m_name;
    QString m_artist;
    QString m_album;
    QUrl m_url;
    QUrl m_icon;
};

class BcModel : public SelectableItemModel
{
    Q_OBJECT
public:
    explicit BcModel(QObject *parent = nullptr);
    Q_INVOKABLE QVariantList selectedItems();

signals:
    void error();

private:
    QList<ListItem*> makeItems();
};

#endif // BCMODEL_H
