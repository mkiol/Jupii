/* Copyright (C) 2018-2022 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef ICECASTMODEL_H
#define ICECASTMODEL_H

#include <QByteArray>
#include <QDebug>
#include <QDomNodeList>
#include <QHash>
#include <QString>
#include <QUrl>
#include <QVariant>

#include "contentserver.h"
#include "itemmodel.h"
#include "listmodel.h"

class IcecastItem : public SelectableItem {
    Q_OBJECT
   public:
    enum Roles {
        NameRole = Qt::DisplayRole,
        IdRole = Qt::UserRole,
        TypeRole,
        UrlRole,
        DescriptionRole,
        SelectedRole
    };

   public:
    IcecastItem(QObject *parent = nullptr) : SelectableItem(parent) {}
    explicit IcecastItem(const QString &id, const QString &name,
                         const QString &description, const QUrl &url,
                         ContentServer::Type type, QObject *parent = nullptr);
    QVariant data(int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    inline QString id() const override { return m_id; }
    inline auto name() const { return m_name; }
    inline auto description() const { return m_description; }
    inline auto url() const { return m_url; }
    inline auto type() const { return m_type; }

   private:
    QString m_id;
    QString m_name;
    QString m_description;
    QUrl m_url;
    ContentServer::Type m_type = ContentServer::Type::Type_Unknown;
};

class IcecastModel : public SelectableItemModel {
    Q_OBJECT
    Q_PROPERTY(bool refreshing READ refreshing NOTIFY refreshingChanged)
   public:
    explicit IcecastModel(QObject *parent = nullptr);
    ~IcecastModel() override;
    Q_INVOKABLE QVariantList selectedItems() override;
    Q_INVOKABLE void refresh();
    inline auto refreshing() const { return m_refreshing; }

   signals:
    void error();
    void refreshingChanged();

   private:
    static const QUrl m_dirUrl;
    static const QString m_dirFilename;
    QDomNodeList m_entries;
    bool m_refreshing = false;

    QList<ListItem *> makeItems() override;
    bool parseData();
    void downloadDir();
    void setRefreshing(bool value);
};

#endif  // ICECASTMODEL_H
