/* Copyright (C) 2018 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef SOMAFMMODEL_H
#define SOMAFMMODEL_H

#include <QString>
#include <QHash>
#include <QDebug>
#include <QByteArray>
#include <QVariant>
#include <QUrl>
#include <QJsonArray>
#include <QDir>

#ifdef DESKTOP
#include <QIcon>
#endif

#include "listmodel.h"
#include "itemmodel.h"

class SomafmItem : public SelectableItem
{
    Q_OBJECT
public:
    enum Roles {
        NameRole = Qt::DisplayRole,
        IconRole = Qt::DecorationRole,
        IdRole = Qt::UserRole,
        UrlRole,
        DescriptionRole
    };

public:
    SomafmItem(QObject *parent = nullptr): SelectableItem(parent) {}
    explicit SomafmItem(const QString &id,
                      const QString &name,
                      const QString &description,
                      const QUrl &url,
#ifdef SAILFISH
                      const QUrl &icon,
#else
                      const QIcon &icon,
#endif
                      QObject *parent = nullptr);
    QVariant data(int role) const;
    QHash<int, QByteArray> roleNames() const;
    inline QString id() const { return m_id; }
    inline QString name() const { return m_name; }
    inline QString description() const { return m_description; }
    inline QUrl url() const { return m_url; }
#ifdef SAILFISH
    inline QUrl icon() const { return m_icon; }
#else
    inline QIcon icon() const { return m_icon; }
#endif
private:
    QString m_id;
    QString m_name;
    QString m_description;
    QUrl m_url;
#ifdef SAILFISH
    QUrl m_icon;
#else
    QIcon m_icon;
#endif
};

class SomafmModel : public SelectableItemModel
{
    Q_OBJECT
public:
    explicit SomafmModel(QObject *parent = nullptr);

private:
    QList<ListItem*> makeItems();
    QJsonArray m_channels;
    QDir m_dir;
};

#endif // SOMAFMMODEL_H
