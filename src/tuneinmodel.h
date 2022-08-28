/* Copyright (C) 2020-2022 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef TUNEINMODEL_H
#define TUNEINMODEL_H

#include <QByteArray>
#include <QDebug>
#include <QDomNodeList>
#include <QHash>
#include <QString>
#include <QUrl>
#include <QVariant>

#include "itemmodel.h"
#include "listmodel.h"

class TuneinItem : public SelectableItem {
    Q_OBJECT
   public:
    enum Roles {
        NameRole = Qt::DisplayRole,
        IconRole = Qt::DecorationRole,
        IdRole = Qt::UserRole,
        UrlRole,
        DescriptionRole,
        SelectedRole
    };

   public:
    TuneinItem(QObject *parent = nullptr) : SelectableItem(parent) {}
    explicit TuneinItem(const QString &id, const QString &name,
                        const QString &description, const QUrl &url,
                        const QUrl &icon, QObject *parent = nullptr);
    QVariant data(int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    inline QString id() const override { return m_id; }
    inline auto name() const { return m_name; }
    inline auto description() const { return m_description; }
    inline auto url() const { return m_url; }
    inline auto icon() const { return m_icon; }

   private:
    QString m_id;
    QString m_name;
    QString m_description;
    QUrl m_url;
    QUrl m_icon;
};

class TuneinModel : public SelectableItemModel {
    Q_OBJECT
   public:
    explicit TuneinModel(QObject *parent = nullptr);
    Q_INVOKABLE QVariantList selectedItems() override;

   signals:
    void error();

   private:
    QList<ListItem *> makeItems() override;
    static QUrl makeSearchUrl(const QString &phrase);
    static QDomNodeList parseData(const QByteArray &data);
};

#endif  // TUNEINMODEL_H
