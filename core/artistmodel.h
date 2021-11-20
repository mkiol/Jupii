/* Copyright (C) 2018 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef ARTISTMODEL_H
#define ARTISTMODEL_H

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

class ArtistItem : public SelectableItem
{
    Q_OBJECT

public:
    enum Roles {
        IdRole = Qt::UserRole,
        NameRole = Qt::DisplayRole,
        CountRole,
        LengthRole,
        IconRole
    };

public:
    ArtistItem(QObject *parent = nullptr): SelectableItem(parent) {}
    explicit ArtistItem(const QString &id,
                      const QString &name,
                      const QUrl &icon,
                      int count,
                      int length,
                      QObject *parent = nullptr);
    QVariant data(int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    inline QString id() const override { return m_id; }
    inline QString name() const { return m_name; }
    inline QUrl icon() const { return m_icon; }
    inline int count() const { return m_count; }
    inline int length() const { return m_length; }

private:
    QString m_id;
    QString m_name;
    QUrl m_icon;
    int m_count = 0;
    int m_length = 0;
};

class ArtistModel : public SelectableItemModel
{
    Q_OBJECT

public:
    explicit ArtistModel(QObject *parent = nullptr);

private:
    static const QString artistsQueryTemplate;

    QList<ListItem*> makeItems() override;
    QList<ListItem*> processTrackerReply(
            const QStringList& varNames,
            const QByteArray& data);
};

#endif // ARTISTMODEL_H
